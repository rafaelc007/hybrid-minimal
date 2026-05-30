#include <pebble.h>
#include "utils.h"
#include "layer1.h"
#include "layer2.h"
#include "widgets.h"

#define PERSIST_KEY_SLOT_ORDER 1
#define SLOT_COUNT 5

// Default order: outer = steps, battery; inner = time, date, weather
static uint8_t s_slot_order[SLOT_COUNT] = {
  WIDGET_STEPS, WIDGET_BATTERY, WIDGET_TIME, WIDGET_DATE, WIDGET_WEATHER
};

// ============================================================================
// Hybrid Minimal Watchface — Main Entry Point
// ============================================================================

static Window *s_window;
static Layer *s_layer1;        // base — full screen
static Layer *s_layer2;        // middle — 4/6 (steps + battery)
static Layer *s_layer2_inner;  // inner — 2/6 (date, time, weather)

// Current time cache
static struct tm s_current_time;

// Battery & steps state
static uint8_t s_battery_pct = 100;
static uint32_t s_steps = 0;
static uint32_t s_step_goal = 10000;

// Weather state
static int s_weather_temp = -999;  // -999 = no data
static char s_weather_cond[16] = "";

// Cached draw command images
static GDrawCommandImage *s_icon_steps = NULL;

// Weather icons (order must match WEATHER_IDX_* enum below)
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

// Recolor all black strokes/fills in a PDC to a target color (called once at load)
static bool prv_recolor_black_to(GDrawCommand *cmd, uint32_t idx, void *context) {
  GColor target = *(GColor *)context;
  if (gcolor_equal(gdraw_command_get_fill_color(cmd), GColorBlack))
    gdraw_command_set_fill_color(cmd, target);
  if (gcolor_equal(gdraw_command_get_stroke_color(cmd), GColorBlack))
    gdraw_command_set_stroke_color(cmd, target);
  return true;
}

// Returns the weather icon matching s_weather_cond (expects lowercase tokens from JS)
static GDrawCommandImage *prv_get_weather_icon(void) {
  const char *c = s_weather_cond;
  if (s_weather_temp == -999 || !c || c[0] == '\0') return NULL;
  if (strstr(c, "sun")     || strstr(c, "clear"))   return s_weather_icons[WEATHER_IDX_CLEAR];
  if (strstr(c, "cloud")   || strstr(c, "overcast")) return s_weather_icons[WEATHER_IDX_PARTLY_CLOUDY];
  if (strstr(c, "drizzle"))                           return s_weather_icons[WEATHER_IDX_LIGHT_RAIN];
  if (strstr(c, "rain")    || strstr(c, "thunder"))  return s_weather_icons[WEATHER_IDX_HEAVY_RAIN];
  if (strstr(c, "sleet"))                             return s_weather_icons[WEATHER_IDX_LIGHT_SNOW];
  if (strstr(c, "snow"))                              return s_weather_icons[WEATHER_IDX_HEAVY_SNOW];
  return s_weather_icons[WEATHER_IDX_GENERIC];
}

// Invert black<->white on a draw command (called once at load for weather icons)
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
// Layer update proc wrappers (bridge to per-layer modules)
// ============================================================================
static void prv_layer1_update(Layer *layer, GContext *ctx) {
  layer1_update(layer, ctx, &s_current_time);
}

static void prv_layer2_update(Layer *layer, GContext *ctx) {
  WidgetState st = {
    .steps = s_steps,
    .step_goal = s_step_goal,
    .battery_pct = s_battery_pct,
    .current_time = &s_current_time,
    .weather_temp = s_weather_temp,
    .weather_cond = s_weather_cond,
    .icon_steps = s_icon_steps,
    .icon_weather = prv_get_weather_icon(),
  };
  layer2_update(layer, ctx, s_slot_order, &st);
}

static void prv_layer2_inner_update(Layer *layer, GContext *ctx) {
  WidgetState st = {
    .steps = s_steps,
    .step_goal = s_step_goal,
    .battery_pct = s_battery_pct,
    .current_time = &s_current_time,
    .weather_temp = s_weather_temp,
    .weather_cond = s_weather_cond,
    .icon_steps = s_icon_steps,
    .icon_weather = prv_get_weather_icon(),
  };
  layer2_inner_update(layer, ctx, s_slot_order, &st);
}

// ============================================================================
// Tick timer handler
// ============================================================================
static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_current_time = *tick_time;
  layer_mark_dirty(s_layer1);
  layer_mark_dirty(s_layer2);
  layer_mark_dirty(s_layer2_inner);
}

// ============================================================================
// Battery service handler
// ============================================================================
static void prv_battery_handler(BatteryChargeState charge) {
  s_battery_pct = charge.charge_percent;
  layer_mark_dirty(s_layer2);
  layer_mark_dirty(s_layer2_inner);
}

// ============================================================================
// Health service handler
// ============================================================================
static void prv_health_handler(HealthEventType event, void *context) {
  if (event == HealthEventMovementUpdate || event == HealthEventSignificantUpdate) {
    s_steps = (uint32_t)health_service_sum_today(HealthMetricStepCount);
    layer_mark_dirty(s_layer2);
    layer_mark_dirty(s_layer2_inner);
  }
}

// ============================================================================
// Window load / unload
// ============================================================================
static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Layer 1: full screen
  s_layer1 = layer_create(bounds);
  layer_set_update_proc(s_layer1, prv_layer1_update);
  layer_add_child(window_layer, s_layer1);

  // Layer 2: 19/30 of screen, centered
  GRect layer2_rect = utils_get_centered_rect(bounds, 19, 30);
  s_layer2 = layer_create(layer2_rect);
  layer_set_update_proc(s_layer2, prv_layer2_update);
  layer_add_child(window_layer, s_layer2);

  // Layer 2 inner: large screens get a wider inner region so big-numeric
  // fonts (LECO_32) fit; small screens stick with 11/30.
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  GRect layer2_inner_rect = utils_get_centered_rect(bounds, 15, 30);
#else
  GRect layer2_inner_rect = utils_get_centered_rect(bounds, 11, 30);
#endif
  s_layer2_inner = layer_create(layer2_inner_rect);
  layer_set_update_proc(s_layer2_inner, prv_layer2_inner_update);
  layer_add_child(window_layer, s_layer2_inner);

  // Seed initial time
  time_t now = time(NULL);
  s_current_time = *localtime(&now);

  // Seed initial battery state
  BatteryChargeState batt = battery_state_service_peek();
  s_battery_pct = batt.charge_percent;

  // Seed initial step count
  s_steps = (uint32_t)health_service_sum_today(HealthMetricStepCount);

  // Load cached PDC icon and recolor black to green
  GColor green = PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite);
  s_icon_steps = gdraw_command_image_create_with_resource(RESOURCE_ID_ICON_STEPS);
  if (s_icon_steps) {
    gdraw_command_list_iterate(
      gdraw_command_image_get_command_list(s_icon_steps),
      prv_recolor_black_to, &green);
  }

  // Load pre-scaled weather icons (already sized for this platform via targetPlatforms
  // in package.json: emery gets _half.pdc at 25x25, small screens get _small.pdc at 16x16).
  // Only invert colors (icons are black-on-transparent; watchface background is dark).
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
      GDrawCommandList *list = gdraw_command_image_get_command_list(s_weather_icons[i]);
      gdraw_command_list_iterate(list, prv_invert_cmd, NULL);
    }
  }
}

static void prv_window_unload(Window *window) {
  gdraw_command_image_destroy(s_icon_steps);
  s_icon_steps = NULL;
  for (int i = 0; i < NUM_WEATHER_ICONS; i++) {
    gdraw_command_image_destroy(s_weather_icons[i]);
    s_weather_icons[i] = NULL;
  }
  layer_destroy(s_layer2_inner);
  layer_destroy(s_layer2);
  layer_destroy(s_layer1);
}

// ============================================================================
// AppMessage handler (receives weather data from JS)
// ============================================================================
static void prv_inbox_received_handler(DictionaryIterator *received, void *context) {
  Tuple *temp_t = dict_find(received, MESSAGE_KEY_WeatherTemp);
  if (temp_t) {
    s_weather_temp = (int)temp_t->value->int32;
  }
  Tuple *icon_t = dict_find(received, MESSAGE_KEY_WeatherIcon);
  if (icon_t && icon_t->type == TUPLE_CSTRING) {
    strncpy(s_weather_cond, icon_t->value->cstring, sizeof(s_weather_cond) - 1);
    s_weather_cond[sizeof(s_weather_cond) - 1] = '\0';
  }
  Tuple *order_t = dict_find(received, MESSAGE_KEY_SlotOrder);
  if (order_t && order_t->type == TUPLE_BYTE_ARRAY &&
      order_t->length >= SLOT_COUNT) {
    // Validate: each byte must be a known widget id and the set must be a
    // permutation of {0..SLOT_COUNT-1}.
    uint8_t seen[WIDGET_COUNT] = {0};
    bool ok = true;
    for (int i = 0; i < SLOT_COUNT; i++) {
      uint8_t v = order_t->value->data[i];
      if (v >= WIDGET_COUNT || seen[v]) { ok = false; break; }
      seen[v] = 1;
    }
    if (ok) {
      memcpy(s_slot_order, order_t->value->data, SLOT_COUNT);
      persist_write_data(PERSIST_KEY_SLOT_ORDER, s_slot_order, SLOT_COUNT);
    }
  }
  layer_mark_dirty(s_layer2);
  layer_mark_dirty(s_layer2_inner);
}

// ============================================================================
// Init / Deinit
// ============================================================================
static void prv_init(void) {
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

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);

  // Open AppMessage channel for weather data
  app_message_register_inbox_received(prv_inbox_received_handler);
  app_message_open(256, 32);

  // Subscribe to minute-level tick updates
  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick_handler);

  // Subscribe to battery updates
  battery_state_service_subscribe(prv_battery_handler);

  // Subscribe to health updates
  health_service_events_subscribe(prv_health_handler, NULL);
}

static void prv_deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  health_service_events_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
