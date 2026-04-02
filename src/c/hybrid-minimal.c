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

// ============================================================================
// Layer update proc wrappers (bridge to per-layer modules)
// ============================================================================
static void prv_layer1_update(Layer *layer, GContext *ctx) {
  layer1_update(layer, ctx, &s_current_time);
}

static void prv_layer2_update(Layer *layer, GContext *ctx) {
  layer2_update(layer, ctx);
}

static void prv_layer3_update(Layer *layer, GContext *ctx) {
  layer3_update(layer, ctx);
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
// Window load / unload
// ============================================================================
static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Layer 1: full screen
  s_layer1 = layer_create(bounds);
  layer_set_update_proc(s_layer1, prv_layer1_update);
  layer_add_child(window_layer, s_layer1);

  // Layer 2: 4/6 of screen, centered
  GRect layer2_rect = utils_get_centered_rect(bounds, 4, 6);
  s_layer2 = layer_create(layer2_rect);
  layer_set_update_proc(s_layer2, prv_layer2_update);
  layer_add_child(window_layer, s_layer2);

  // Layer 3: 2/6 of screen, centered
  GRect layer3_rect = utils_get_centered_rect(bounds, 2, 6);
  s_layer3 = layer_create(layer3_rect);
  layer_set_update_proc(s_layer3, prv_layer3_update);
  layer_add_child(window_layer, s_layer3);

  // Seed initial time
  time_t now = time(NULL);
  s_current_time = *localtime(&now);
}

static void prv_window_unload(Window *window) {
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
}

static void prv_deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
