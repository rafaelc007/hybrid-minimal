#include <pebble.h>
#include "utils.h"
#include "layer1.h"
#include "layer2.h"
#include "widgets.h"

#define PERSIST_KEY_SLOT_ORDER 4
#define PERSIST_KEY_PROGRESS_COLOR 2
#define PERSIST_KEY_BAR_STYLE 3
#define PERSIST_KEY_WEATHER_UNITS 5
#define PERSIST_KEY_DATE_FORMAT 6
#define SLOT_COUNT 5

// Refresh weather every 30 minutes from the watch.
#define WEATHER_REFRESH_MINUTES 30
// Refresh steps once every N minutes (health service polling is not free).
#define STEPS_REFRESH_MINUTES 5

// ============================================================================
// State
// ============================================================================
static Window *s_window;
static Layer *s_layer1_bg;     // black bg + minute progress (minute-rate)
static Layer *s_layer1_chrome; // ticks + hour numbers      (hour-rate)
static Layer *s_layer2;        // outer strips: steps + battery
static Layer *s_layer2_inner;  // inner stack: time, date, weather

static struct tm s_current_time;
static int s_last_drawn_hour12 = -1;

static uint8_t  s_battery_pct = 100;
static uint32_t s_steps = 0;
static uint32_t s_step_goal = 10000;

static int  s_weather_temp = -999;
static char s_weather_cond[16] = "";
static char s_weather_units = 'C';  // 'C' or 'F'

// Cached PDC images
static GDrawCommandImage *s_icon_steps = NULL;
static GDrawCommandImage *s_icon_disconnect = NULL;
static GSize s_icon_steps_size      = {0, 0};
static GSize s_icon_disconnect_size = {0, 0};

static bool s_connected = true;
static bool s_is_24h = false;

#define NUM_WEATHER_ICONS 7
typedef enum {
  WEATHER_IDX_GENERIC = 0,
  WEATHER_IDX_CLEAR,
  WEATHER_IDX_PARTLY_CLOUDY,
  WEATHER_IDX_LIGHT_RAIN,
  WEATHER_IDX_HEAVY_RAIN,
  WEATHER_IDX_LIGHT_SNOW,
  WEATHER_IDX_HEAVY_SNOW,
} WeatherIconIndex;
static GDrawCommandImage *s_weather_icons[NUM_WEATHER_ICONS];
static GSize s_weather_icon_sizes[NUM_WEATHER_ICONS];

// Resolved weather icon, computed only when WeatherIcon changes (not per redraw).
static GDrawCommandImage *s_resolved_weather_icon = NULL;
static GSize s_resolved_weather_icon_size = {0, 0};

// Slot order is the visual stack, top → bottom:
//   [0] outer top strip
//   [1..3] inner stack (top, middle, bottom)
//   [4] outer bottom strip
static uint8_t s_slot_order[SLOT_COUNT] = {
  WIDGET_STEPS, WIDGET_TIME, WIDGET_DATE, WIDGET_WEATHER, WIDGET_BATTERY
};

// Progress band color (GColor8.argb). Defaults differ per display.
#ifdef PBL_COLOR
static uint8_t s_progress_color_argb = 0;  // set in prv_init from GColorJazzberryJam
#else
static uint8_t s_progress_color_argb = 0;  // set in prv_init from GColorLightGray
#endif

// Progress bar shape: 0 = rounded square (default), 1 = sharp square.
// Only affects rectangular displays; round displays always render an arc.
static uint8_t s_bar_style = 0;

// Date format selection — see DateFormat enum in widgets.h.
static uint8_t s_date_format = DATE_FMT_MMM_DD;

// ============================================================================
// PDC color helpers (called once at load)
// ============================================================================
static bool prv_recolor_black_to(GDrawCommand *cmd, uint32_t idx, void *context) {
  GColor target = *(GColor *)context;
  if (gcolor_equal(gdraw_command_get_fill_color(cmd), GColorBlack))
    gdraw_command_set_fill_color(cmd, target);
  if (gcolor_equal(gdraw_command_get_stroke_color(cmd), GColorBlack))
    gdraw_command_set_stroke_color(cmd, target);
  return true;
}

static bool prv_invert_cmd(GDrawCommand *cmd, uint32_t idx, void *context) {
  GColor fc = gdraw_command_get_fill_color(cmd);
  if (gcolor_equal(fc, GColorBlack)) gdraw_command_set_fill_color(cmd, GColorWhite);
  else if (gcolor_equal(fc, GColorWhite)) gdraw_command_set_fill_color(cmd, GColorBlack);
  GColor sc = gdraw_command_get_stroke_color(cmd);
  if (gcolor_equal(sc, GColorBlack)) gdraw_command_set_stroke_color(cmd, GColorWhite);
  else if (gcolor_equal(sc, GColorWhite)) gdraw_command_set_stroke_color(cmd, GColorBlack);
  return true;
}

// ============================================================================
// Weather icon resolution — done once on receipt, then cached.
// ============================================================================
static int prv_resolve_weather_icon_index(void) {
  const char *c = s_weather_cond;
  if (s_weather_temp == -999 || !c || c[0] == '\0') return -1;
  if (strstr(c, "sun")     || strstr(c, "clear"))    return WEATHER_IDX_CLEAR;
  if (strstr(c, "cloud")   || strstr(c, "overcast")) return WEATHER_IDX_PARTLY_CLOUDY;
  if (strstr(c, "drizzle"))                          return WEATHER_IDX_LIGHT_RAIN;
  if (strstr(c, "rain")    || strstr(c, "thunder"))  return WEATHER_IDX_HEAVY_RAIN;
  if (strstr(c, "sleet"))                            return WEATHER_IDX_LIGHT_SNOW;
  if (strstr(c, "snow"))                             return WEATHER_IDX_HEAVY_SNOW;
  return WEATHER_IDX_GENERIC;
}

static void prv_refresh_resolved_weather_icon(void) {
  int idx = prv_resolve_weather_icon_index();
  if (idx < 0) {
    s_resolved_weather_icon = NULL;
    s_resolved_weather_icon_size = GSize(0, 0);
  } else {
    s_resolved_weather_icon = s_weather_icons[idx];
    s_resolved_weather_icon_size = s_weather_icon_sizes[idx];
  }
}

// ============================================================================
// Shared WidgetState — single instance, refreshed by event handlers only.
// ============================================================================
static WidgetState s_state;

static void prv_refresh_widget_state(void) {
  s_state.steps                = s_steps;
  s_state.step_goal            = s_step_goal;
  s_state.battery_pct          = s_battery_pct;
  s_state.current_time         = &s_current_time;
  s_state.weather_temp         = s_weather_temp;
  s_state.weather_cond         = s_weather_cond;
  s_state.icon_steps           = s_icon_steps;
  s_state.icon_weather         = s_resolved_weather_icon;
  s_state.icon_disconnect      = s_icon_disconnect;
  s_state.connected            = s_connected;
  s_state.is_24h               = s_is_24h;
  s_state.weather_units        = s_weather_units;
  s_state.date_format          = s_date_format;
  s_state.icon_steps_size      = s_icon_steps_size;
  s_state.icon_weather_size    = s_resolved_weather_icon_size;
  s_state.icon_disconnect_size = s_icon_disconnect_size;
}

// ============================================================================
// Layer update procs
// ============================================================================
static void prv_layer1_bg_update(Layer *layer, GContext *ctx) {
  GColor c = (GColor){ .argb = s_progress_color_argb };
  layer1_bg_update(layer, ctx, s_current_time.tm_min, c, s_bar_style == 0);
}

static void prv_layer1_chrome_update(Layer *layer, GContext *ctx) {
  int h12 = s_current_time.tm_hour % 12;
  if (h12 == 0) h12 = 12;
  layer1_chrome_update(layer, ctx, h12, s_bar_style == 0);
}

static void prv_layer2_update(Layer *layer, GContext *ctx) {
  layer2_update(layer, ctx, s_slot_order, &s_state);
}

static void prv_layer2_inner_update(Layer *layer, GContext *ctx) {
  layer2_inner_update(layer, ctx, s_slot_order, &s_state);
}

// ============================================================================
// Outbound: request fresh weather from the phone
// ============================================================================
static void prv_request_weather(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) return;
  dict_write_uint8(iter, MESSAGE_KEY_RequestWeather, 1);
  app_message_outbox_send();
}

// ============================================================================
// Tick handler — minute-rate. Mark only the layers whose displayed values
// actually changed.
// ============================================================================
static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_current_time = *tick_time;

  // BG (progress + bg fill) changes every minute.
  layer_mark_dirty(s_layer1_bg);

  // Chrome only changes on the hour (current hour highlight).
  int h12 = s_current_time.tm_hour % 12;
  if (h12 == 0) h12 = 12;
  if (h12 != s_last_drawn_hour12) {
    s_last_drawn_hour12 = h12;
    layer_mark_dirty(s_layer1_chrome);
  }

  // Time/date display lives in the inner stack — redraw every minute.
  // Refresh the cached 24h flag in case the user toggled the system setting
  // while the watchface is running.
  bool now_24h = clock_is_24h_style();
  if (now_24h != s_is_24h) {
    s_is_24h = now_24h;
    s_state.is_24h = now_24h;
  }
  layer_mark_dirty(s_layer2_inner);

  // Steps poll throttled: health_service_sum_today isn't free; querying every
  // minute wakes the health subsystem 1440 times/day. Polling every 5 min
  // gives the same perceived freshness with 5× fewer queries.
  if ((tick_time->tm_min % STEPS_REFRESH_MINUTES) == 0) {
    uint32_t new_steps = (uint32_t)health_service_sum_today(HealthMetricStepCount);
    if (new_steps != s_steps) {
      s_steps = new_steps;
      s_state.steps = new_steps;
      layer_mark_dirty(s_layer2);  // steps live in the outer strips
    }
  }

  // Periodic weather refresh (skip if we know we're disconnected — the outbox
  // would just fail and burn cycles on the BT stack).
  if (s_connected && (tick_time->tm_min % WEATHER_REFRESH_MINUTES) == 0) {
    prv_request_weather();
  }
}

// ============================================================================
// Battery service handler — only repaint when the displayed value changed.
// ============================================================================
static void prv_battery_handler(BatteryChargeState charge) {
  if (charge.charge_percent == s_battery_pct) return;
  s_battery_pct = charge.charge_percent;
  s_state.battery_pct = s_battery_pct;
  layer_mark_dirty(s_layer2);  // battery lives in the outer strips
}

// ============================================================================
// Connection service — swap the weather widget for a disconnect icon when
// the phone link drops.
// ============================================================================
static void prv_connection_handler(bool connected) {
  if (connected == s_connected) return;
  s_connected = connected;
  s_state.connected = connected;
  layer_mark_dirty(s_layer2_inner);
}

// ============================================================================
// Window load / unload
// ============================================================================
static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Layer 1 background + progress (bottom)
  s_layer1_bg = layer_create(bounds);
  layer_set_update_proc(s_layer1_bg, prv_layer1_bg_update);
  layer_add_child(window_layer, s_layer1_bg);

  // Layer 1 chrome (ticks + numbers) — sits on top of bg, transparent elsewhere
  s_layer1_chrome = layer_create(bounds);
  layer_set_update_proc(s_layer1_chrome, prv_layer1_chrome_update);
  layer_add_child(window_layer, s_layer1_chrome);

  // Layer 2 outer: 19/30 of screen, centered
  GRect layer2_rect = utils_get_centered_rect(bounds, 19, 30);
  s_layer2 = layer_create(layer2_rect);
  layer_set_update_proc(s_layer2, prv_layer2_update);
  layer_add_child(window_layer, s_layer2);

  // Layer 2 inner: wider on large screens so big-numeric fonts fit.
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  GRect layer2_inner_rect = utils_get_centered_rect(bounds, 15, 30);
#else
  GRect layer2_inner_rect = utils_get_centered_rect(bounds, 11, 30);
#endif
  s_layer2_inner = layer_create(layer2_inner_rect);
  layer_set_update_proc(s_layer2_inner, prv_layer2_inner_update);
  layer_add_child(window_layer, s_layer2_inner);

  // Seed time + chrome cache
  time_t now = time(NULL);
  s_current_time = *localtime(&now);
  s_last_drawn_hour12 = s_current_time.tm_hour % 12;
  if (s_last_drawn_hour12 == 0) s_last_drawn_hour12 = 12;

  // Seed battery + steps
  s_battery_pct = battery_state_service_peek().charge_percent;
  s_steps = (uint32_t)health_service_sum_today(HealthMetricStepCount);

  // Steps icon (PDC) — recolor black → green once at load
  GColor green = PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite);
  s_icon_steps = gdraw_command_image_create_with_resource(RESOURCE_ID_ICON_STEPS);
  if (s_icon_steps) {
    gdraw_command_list_iterate(
      gdraw_command_image_get_command_list(s_icon_steps),
      prv_recolor_black_to, &green);
    s_icon_steps_size = gdraw_command_image_get_bounds_size(s_icon_steps);
  }

  s_icon_disconnect = gdraw_command_image_create_with_resource(RESOURCE_ID_ICON_DISCONNECT);
  if (s_icon_disconnect) {
    gdraw_command_list_iterate(
      gdraw_command_image_get_command_list(s_icon_disconnect),
      prv_invert_cmd, NULL);
    s_icon_disconnect_size = gdraw_command_image_get_bounds_size(s_icon_disconnect);
  }

  // Pre-scaled weather icons (sized per platform via package.json). Invert
  // black<->white once at load.
  static const uint32_t s_weather_res_ids[NUM_WEATHER_ICONS] = {
    RESOURCE_ID_WEATHER_GENERIC,
    RESOURCE_ID_WEATHER_CLEAR,
    RESOURCE_ID_WEATHER_PARTLY_CLOUDY,
    RESOURCE_ID_WEATHER_LIGHT_RAIN,
    RESOURCE_ID_WEATHER_HEAVY_RAIN,
    RESOURCE_ID_WEATHER_LIGHT_SNOW,
    RESOURCE_ID_WEATHER_HEAVY_SNOW,
  };
  for (int i = 0; i < NUM_WEATHER_ICONS; i++) {
    s_weather_icons[i] = gdraw_command_image_create_with_resource(s_weather_res_ids[i]);
    if (s_weather_icons[i]) {
      gdraw_command_list_iterate(
        gdraw_command_image_get_command_list(s_weather_icons[i]),
        prv_invert_cmd, NULL);
      s_weather_icon_sizes[i] = gdraw_command_image_get_bounds_size(s_weather_icons[i]);
    } else {
      s_weather_icon_sizes[i] = GSize(0, 0);
    }
  }

  // Cache 24h style + initial weather icon + initial widget state.
  s_is_24h = clock_is_24h_style();
  prv_refresh_resolved_weather_icon();
  prv_refresh_widget_state();
}

static void prv_window_unload(Window *window) {
  gdraw_command_image_destroy(s_icon_steps);
  s_icon_steps = NULL;
  gdraw_command_image_destroy(s_icon_disconnect);
  s_icon_disconnect = NULL;
  for (int i = 0; i < NUM_WEATHER_ICONS; i++) {
    gdraw_command_image_destroy(s_weather_icons[i]);
    s_weather_icons[i] = NULL;
  }
  layer_destroy(s_layer2_inner);
  layer_destroy(s_layer2);
  layer_destroy(s_layer1_chrome);
  layer_destroy(s_layer1_bg);
}

// ============================================================================
// AppMessage inbox
// ============================================================================
static void prv_inbox_received_handler(DictionaryIterator *received, void *context) {
  bool weather_changed = false;
  bool order_changed   = false;
  bool color_changed   = false;

  Tuple *temp_t = dict_find(received, MESSAGE_KEY_WeatherTemp);
  if (temp_t) {
    int new_temp = (int)temp_t->value->int32;
    if (new_temp != s_weather_temp) {
      s_weather_temp = new_temp;
      weather_changed = true;
    }
  }

  Tuple *icon_t = dict_find(received, MESSAGE_KEY_WeatherIcon);
  if (icon_t && icon_t->type == TUPLE_CSTRING) {
    if (strncmp(s_weather_cond, icon_t->value->cstring,
                sizeof(s_weather_cond)) != 0) {
      strncpy(s_weather_cond, icon_t->value->cstring, sizeof(s_weather_cond) - 1);
      s_weather_cond[sizeof(s_weather_cond) - 1] = '\0';
      weather_changed = true;
    }
  }

  if (weather_changed) {
    prv_refresh_resolved_weather_icon();
    s_state.icon_weather      = s_resolved_weather_icon;
    s_state.icon_weather_size = s_resolved_weather_icon_size;
    s_state.weather_temp      = s_weather_temp;
  }

  Tuple *order_t = dict_find(received, MESSAGE_KEY_SlotOrder);
  if (order_t && order_t->type == TUPLE_BYTE_ARRAY &&
      order_t->length >= SLOT_COUNT) {
    uint8_t seen[WIDGET_COUNT] = {0};
    bool ok = true;
    for (int i = 0; i < SLOT_COUNT; i++) {
      uint8_t v = order_t->value->data[i];
      if (v >= WIDGET_COUNT || seen[v]) { ok = false; break; }
      seen[v] = 1;
    }
    if (ok && memcmp(s_slot_order, order_t->value->data, SLOT_COUNT) != 0) {
      memcpy(s_slot_order, order_t->value->data, SLOT_COUNT);
      persist_write_data(PERSIST_KEY_SLOT_ORDER, s_slot_order, SLOT_COUNT);
      order_changed = true;
    }
  }

  Tuple *color_t = dict_find(received, MESSAGE_KEY_ProgressColor);
  if (color_t) {
    uint8_t new_argb = (uint8_t)color_t->value->int32;
    if (new_argb != s_progress_color_argb) {
      s_progress_color_argb = new_argb;
      persist_write_int(PERSIST_KEY_PROGRESS_COLOR, (int32_t)new_argb);
      color_changed = true;
    }
  }

  Tuple *style_t = dict_find(received, MESSAGE_KEY_BarStyle);
  bool style_changed = false;
  if (style_t) {
    uint8_t new_style = (uint8_t)style_t->value->int32 ? 1 : 0;
    if (new_style != s_bar_style) {
      s_bar_style = new_style;
      persist_write_int(PERSIST_KEY_BAR_STYLE, (int32_t)new_style);
      style_changed = true;
    }
  }

  Tuple *units_t = dict_find(received, MESSAGE_KEY_WeatherUnits);
  if (units_t && units_t->type == TUPLE_CSTRING && units_t->value->cstring[0]) {
    char new_unit = units_t->value->cstring[0];
    if (new_unit != 'C' && new_unit != 'F') new_unit = 'C';
    if (new_unit != s_weather_units) {
      s_weather_units = new_unit;
      s_state.weather_units = new_unit;
      persist_write_int(PERSIST_KEY_WEATHER_UNITS, (int32_t)new_unit);
      weather_changed = true;
    }
  }

  Tuple *date_fmt_t = dict_find(received, MESSAGE_KEY_DateFormat);
  bool date_format_changed = false;
  if (date_fmt_t) {
    int32_t v = date_fmt_t->value->int32;
    if (v < 0 || v >= DATE_FMT_COUNT) v = DATE_FMT_MMM_DD;
    uint8_t new_fmt = (uint8_t)v;
    if (new_fmt != s_date_format) {
      s_date_format = new_fmt;
      s_state.date_format = new_fmt;
      persist_write_int(PERSIST_KEY_DATE_FORMAT, (int32_t)new_fmt);
      date_format_changed = true;
    }
  }

  if (weather_changed || order_changed || date_format_changed) {
    layer_mark_dirty(s_layer2);
    layer_mark_dirty(s_layer2_inner);
  }
  if (color_changed || style_changed) {
    layer_mark_dirty(s_layer1_bg);
  }
  if (style_changed) {
    layer_mark_dirty(s_layer1_chrome);
  }
}

// ============================================================================
// Init / Deinit
// ============================================================================
static void prv_init(void) {
  // Default progress color depends on display type.
  s_progress_color_argb = PBL_IF_COLOR_ELSE(GColorJazzberryJam, GColorLightGray).argb;

  // Restore persisted slot order if present and valid.
  if (persist_exists(PERSIST_KEY_SLOT_ORDER)) {
    uint8_t buf[SLOT_COUNT];
    int n = persist_read_data(PERSIST_KEY_SLOT_ORDER, buf, SLOT_COUNT);
    if (n == SLOT_COUNT) {
      uint8_t seen[WIDGET_COUNT] = {0};
      bool ok = true;
      for (int i = 0; i < SLOT_COUNT; i++) {
        if (buf[i] >= WIDGET_COUNT || seen[buf[i]]) { ok = false; break; }
        seen[buf[i]] = 1;
      }
      if (ok) memcpy(s_slot_order, buf, SLOT_COUNT);
    }
  }

  if (persist_exists(PERSIST_KEY_PROGRESS_COLOR)) {
    s_progress_color_argb = (uint8_t)persist_read_int(PERSIST_KEY_PROGRESS_COLOR);
  }

  if (persist_exists(PERSIST_KEY_BAR_STYLE)) {
    s_bar_style = (uint8_t)persist_read_int(PERSIST_KEY_BAR_STYLE) ? 1 : 0;
  }

  if (persist_exists(PERSIST_KEY_WEATHER_UNITS)) {
    char u = (char)persist_read_int(PERSIST_KEY_WEATHER_UNITS);
    s_weather_units = (u == 'F') ? 'F' : 'C';
  }

  if (persist_exists(PERSIST_KEY_DATE_FORMAT)) {
    int32_t v = persist_read_int(PERSIST_KEY_DATE_FORMAT);
    if (v >= 0 && v < DATE_FMT_COUNT) s_date_format = (uint8_t)v;
  }

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);

  // Register inbox before opening (Pebble requirement).
  // Buffers sized to actual payloads: temp (8B) + cond (≤24B) + slot order
  // (≤16B) + dict overhead. 96/32 is comfortably above worst case.
  app_message_register_inbox_received(prv_inbox_received_handler);
  app_message_open(96, 32);

  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick_handler);
  battery_state_service_subscribe(prv_battery_handler);

  s_connected = connection_service_peek_pebble_app_connection();
  connection_service_subscribe((ConnectionHandlers){
    .pebble_app_connection_handler = prv_connection_handler,
  });
}

static void prv_deinit(void) {
  // OS reclaims subscriptions on exit — no need to unsubscribe.
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
