#pragma once
#include <pebble.h>

// Compute a centered rect at (numerator/denominator) of the given bounds
static inline GRect utils_get_centered_rect(GRect bounds, int numerator, int denominator) {
  int16_t w = bounds.size.w * numerator / denominator;
  int16_t h = bounds.size.h * numerator / denominator;
  int16_t x = (bounds.size.w - w) / 2;
  int16_t y = (bounds.size.h - h) / 2;
  return GRect(x, y, w, h);
}

// Draw a red debug border (circle on round, rounded rect on square)
static inline void utils_draw_debug_border(GContext *ctx, GRect bounds) {
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
  graphics_context_set_stroke_width(ctx, 1);
#ifdef PBL_ROUND
  int16_t radius = bounds.size.w / 2 - 1;
  graphics_draw_circle(ctx, grect_center_point(&bounds), radius);
#else
  graphics_draw_round_rect(ctx, grect_inset(bounds, GEdgeInsets(1)), 4);
#endif
}
