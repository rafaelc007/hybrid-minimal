#include <pebble.h>
#include "utils.h"
#include "layer1.h"
#include "layer2.h"
#include "layer3.h"

// ============================================================================
// Hybrid Minimal Watchface — Main Entry Point
// ============================================================================

static Window *s_window;
static Layer *s_layer1;  // base — full screen
static Layer *s_layer2;  // middle — 4/6
static Layer *s_layer3;  // inner — 2/6

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

// Recolor all black strokes/fills in a PDC to green (called once at load)
static bool prv_recolor_to_green(GDrawCommand *cmd, uint32_t idx, void *context) {
  GColor green = PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite);
  if (gcolor_equal(gdraw_command_get_fill_color(cmd), GColorBlack))
    gdraw_command_set_fill_color(cmd, green);
  if (gcolor_equal(gdraw_command_get_stroke_color(cmd), GColorBlack))
    gdraw_command_set_stroke_color(cmd, green);
  return true;
}

// ============================================================================
// Layer update proc wrappers (bridge to per-layer modules)
// ============================================================================
static void prv_layer1_update(Layer *layer, GContext *ctx) {
  layer1_update(layer, ctx, &s_current_time);
}

static void prv_layer2_update(Layer *layer, GContext *ctx) {
  layer2_update(layer, ctx, s_steps, s_step_goal, s_battery_pct, s_icon_steps);
}

static void prv_layer3_update(Layer *layer, GContext *ctx) {
  layer3_update(layer, ctx, &s_current_time, s_weather_temp, s_weather_cond);
}

// ============================================================================
// Tick timer handler
// ============================================================================
static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_current_time = *tick_time;
  layer_mark_dirty(s_layer1);
  layer_mark_dirty(s_layer2);
  layer_mark_dirty(s_layer3);
}

// ============================================================================
// Battery service handler
// ============================================================================
static void prv_battery_handler(BatteryChargeState charge) {
  s_battery_pct = charge.charge_percent;
  layer_mark_dirty(s_layer2);
}

// ============================================================================
// Health service handler
// ============================================================================
static void prv_health_handler(HealthEventType event, void *context) {
  if (event == HealthEventMovementUpdate || event == HealthEventSignificantUpdate) {
    s_steps = (uint32_t)health_service_sum_today(HealthMetricStepCount);
    layer_mark_dirty(s_layer2);
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

  // Layer 3: 11/30 of screen, centered
  GRect layer3_rect = utils_get_centered_rect(bounds, 11, 30);
  s_layer3 = layer_create(layer3_rect);
  layer_set_update_proc(s_layer3, prv_layer3_update);
  layer_add_child(window_layer, s_layer3);

  // Seed initial time
  time_t now = time(NULL);
  s_current_time = *localtime(&now);

  // Seed initial battery state
  BatteryChargeState batt = battery_state_service_peek();
  s_battery_pct = batt.charge_percent;

  // Seed initial step count
  s_steps = (uint32_t)health_service_sum_today(HealthMetricStepCount);

  // Load cached PDC icon and recolor black → green
  s_icon_steps = gdraw_command_image_create_with_resource(RESOURCE_ID_ICON_STEPS);
  if (s_icon_steps) {
    gdraw_command_list_iterate(
      gdraw_command_image_get_command_list(s_icon_steps),
      prv_recolor_to_green, NULL);
  }
}

static void prv_window_unload(Window *window) {
  gdraw_command_image_destroy(s_icon_steps);
  s_icon_steps = NULL;
  layer_destroy(s_layer3);
  layer_destroy(s_layer2);
  layer_destroy(s_layer1);
}

// ============================================================================
// Init / Deinit
// ============================================================================
static void prv_init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);

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
