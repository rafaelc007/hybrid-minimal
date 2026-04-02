#include "layer2.h"
#include "utils.h"

void layer2_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Red debug border for layer 2
  utils_draw_debug_border(ctx, bounds);

  // Widget slot sizes — small rectangles in Q2 (upper-left) and Q3 (lower-left)
  int16_t slot_w = bounds.size.w / 3;
  int16_t slot_h = bounds.size.h / 5;

  // Q2: upper-right quadrant (steps placeholder)
  GRect q2_slot = GRect(bounds.size.w - bounds.size.w / 8 - slot_w,
                        bounds.size.h / 5,
                        slot_w, slot_h);
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
  graphics_fill_rect(ctx, q2_slot, 2, GCornersAll);

  // Q3: lower-left quadrant (battery placeholder)
  GRect q3_slot = GRect(bounds.size.w / 8,
                        bounds.size.h - bounds.size.h / 5 - slot_h,
                        slot_w, slot_h);
  graphics_fill_rect(ctx, q3_slot, 2, GCornersAll);
}
