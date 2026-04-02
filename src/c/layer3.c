#include "layer3.h"
#include "utils.h"

void layer3_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Red debug border for layer 3
  utils_draw_debug_border(ctx, bounds);

  // Three stacked slots: date (top), digital time (middle), weather (bottom)
  int16_t padding = 4;
  int16_t slot_w = bounds.size.w - 2 * padding;
  int16_t total_h = bounds.size.h - 2 * padding;
  int16_t slot_h = (total_h - 2 * padding) / 3;

  // Top slot — date placeholder
  GRect top_slot = GRect(padding, padding, slot_w, slot_h);
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
  graphics_fill_rect(ctx, top_slot, 2, GCornersAll);

  // Middle slot — digital time placeholder
  GRect mid_slot = GRect(padding, padding + slot_h + padding, slot_w, slot_h);
  graphics_fill_rect(ctx, mid_slot, 2, GCornersAll);

  // Bottom slot — weather placeholder
  GRect bot_slot = GRect(padding, padding + 2 * (slot_h + padding), slot_w, slot_h);
  graphics_fill_rect(ctx, bot_slot, 2, GCornersAll);
}
