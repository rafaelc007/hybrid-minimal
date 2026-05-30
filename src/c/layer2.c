#include "layer2.h"
#include "utils.h"

// Stack `n` widgets (from slot_order[start..start+n)) vertically in `bounds`.
// Each widget gets its natural height; remaining space becomes equal gaps
// above/between/below. If natural heights overflow the region, all heights are
// scaled down proportionally and gaps go to zero.
static void prv_render_stack(GContext *ctx, GRect bounds, int start, int n,
                             const uint8_t *slot_order, const WidgetState *state) {
  int16_t total_natural = 0;
  int16_t heights[5] = {0};
  for (int i = 0; i < n; i++) {
    heights[i] = widget_natural_height((WidgetId)slot_order[start + i]);
    total_natural += heights[i];
  }

  int16_t avail = bounds.size.h;
  int16_t gap;
  if (total_natural >= avail) {
    // Scale heights to fit; no gaps.
    for (int i = 0; i < n; i++) {
      heights[i] = (int16_t)((int32_t)heights[i] * avail / total_natural);
    }
    gap = 0;
  } else {
    // Distribute leftover space as (n+1) equal gaps (top, between, bottom).
    gap = (avail - total_natural) / (n + 1);
  }

  int16_t y = bounds.origin.y + gap;
  for (int i = 0; i < n; i++) {
    GRect r = GRect(bounds.origin.x, y, bounds.size.w, heights[i]);
    widget_render(ctx, r, (WidgetId)slot_order[start + i], state);
    y += heights[i] + gap;
  }
}

// ============================================================================
// Outer region: 2 slots — one strip above the inner region, one below.
// The inner layer (11/30 of screen) is centered inside this outer layer
// (19/30). To avoid overlap we render only in the top and bottom margins.
// ============================================================================
void layer2_update(Layer *layer, GContext *ctx,
                   const uint8_t *slot_order, const WidgetState *state) {
  GRect bounds = layer_get_bounds(layer);
  int16_t pad = 2;

  // Inner layer is centered inside this outer layer. On large screens
  // (emery/gabbro) the inner region is 15/30 of the screen (vs outer 19/30),
  // so inner_h = outer_h * 15/19. On small screens it is 11/30 → 11/19.
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  int16_t inner_h = bounds.size.h * 15 / 19;
#else
  int16_t inner_h = bounds.size.h * 11 / 19;
#endif
  int16_t margin  = (bounds.size.h - inner_h) / 2;

  GRect top_strip = GRect(pad, pad,
                          bounds.size.w - 2 * pad,
                          margin - 2 * pad);
  GRect bot_strip = GRect(pad, margin + inner_h + pad,
                          bounds.size.w - 2 * pad,
                          margin - 2 * pad);

  if (top_strip.size.h > 0) {
    widget_render(ctx, top_strip, (WidgetId)slot_order[0], state);
  }
  if (bot_strip.size.h > 0) {
    widget_render(ctx, bot_strip, (WidgetId)slot_order[1], state);
  }
}

// ============================================================================
// Inner region: 3 slots stacked vertically
// ============================================================================
void layer2_inner_update(Layer *layer, GContext *ctx,
                         const uint8_t *slot_order, const WidgetState *state) {
  GRect bounds = layer_get_bounds(layer);
  int16_t pad = 2;
  GRect inset = GRect(pad, pad, bounds.size.w - 2 * pad, bounds.size.h - 2 * pad);
  prv_render_stack(ctx, inset, 2, 3, slot_order, state);
}
