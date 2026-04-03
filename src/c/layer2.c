#include "layer2.h"
#include "utils.h"

// ============================================================================
// Quarter-arc progress bar drawing
// ============================================================================

#ifdef PBL_ROUND
// Draw a quarter-arc progress on round screens.
// start_angle/end_angle in Pebble trig units. Fills proportional to value/max.
static void draw_quarter_arc(GContext *ctx, GRect rect, int16_t thickness,
                              GColor fill_color, GColor track_color,
                              int32_t start_angle, int32_t end_angle,
                              int value, int max) {
  // Track
  graphics_context_set_fill_color(ctx, track_color);
  graphics_fill_radial(ctx, rect, GOvalScaleModeFitCircle,
                       thickness, start_angle, end_angle);
  // Fill
  if (value > 0 && max > 0) {
    int clamped = value > max ? max : value;
    int32_t fill_end = start_angle +
      (int32_t)((int64_t)clamped * (end_angle - start_angle) / max);
    graphics_context_set_fill_color(ctx, fill_color);
    graphics_fill_radial(ctx, rect, GOvalScaleModeFitCircle,
                         thickness, start_angle, fill_end);
  }
}
#else
// Draw a quarter-perimeter progress bar on rectangular screens.
// Follows the rect edge from one midpoint around one corner to the next midpoint.
// quadrant: 0 = top-center→right-center, 1 = right→bottom, 2 = bottom→left, 3 = left→top
static void draw_quarter_rect(GContext *ctx, GRect rect, int16_t band,
                               GColor fill_color, GColor track_color,
                               int quadrant, int value, int max) {
  int x = rect.origin.x;
  int y = rect.origin.y;
  int w = rect.size.w;
  int h = rect.size.h;
  int hw = w / 2;
  int hh = h / 2;

  // Quarter-perimeter has 2 segments (half-edge + half-edge around one corner)
  int seg1, seg2;
  switch (quadrant) {
    case 0: seg1 = hw; seg2 = hh; break; // top-center→TR corner→right-center
    case 1: seg1 = hh; seg2 = hw; break; // right-center→BR corner→bottom-center
    case 2: seg1 = hw; seg2 = hh; break; // bottom-center→BL corner→left-center
    case 3: seg1 = hh; seg2 = hw; break; // left-center→TL corner→top-center
    default: seg1 = hw; seg2 = hh; break;
  }
  int q_perim = seg1 + seg2;

  // Helper: draw a segment pair with a given fill distance
  #define DRAW_SEGS(color, dist) do { \
    int _r = (dist); \
    int _f1 = (_r > seg1) ? seg1 : _r; \
    int _f2 = (_r > seg1) ? (_r - seg1) : 0; \
    if (_f2 > seg2) _f2 = seg2; \
    graphics_context_set_fill_color(ctx, (color)); \
    switch (quadrant) { \
      case 0: \
        if (_f1 > 0) graphics_fill_rect(ctx, GRect(x+hw, y, _f1, band), 0, GCornerNone); \
        if (_f2 > 0) graphics_fill_rect(ctx, GRect(x+w-band, y, band, _f2), 0, GCornerNone); \
        break; \
      case 1: \
        if (_f1 > 0) graphics_fill_rect(ctx, GRect(x+w-band, y+hh, band, _f1), 0, GCornerNone); \
        if (_f2 > 0) graphics_fill_rect(ctx, GRect(x+w-_f2, y+h-band, _f2, band), 0, GCornerNone); \
        break; \
      case 2: \
        if (_f1 > 0) graphics_fill_rect(ctx, GRect(x+hw-_f1, y+h-band, _f1, band), 0, GCornerNone); \
        if (_f2 > 0) graphics_fill_rect(ctx, GRect(x, y+h-_f2, band, _f2), 0, GCornerNone); \
        break; \
      case 3: \
        if (_f1 > 0) graphics_fill_rect(ctx, GRect(x, y+hh-_f1, band, _f1), 0, GCornerNone); \
        if (_f2 > 0) graphics_fill_rect(ctx, GRect(x, y, _f2, band), 0, GCornerNone); \
        break; \
    } \
  } while(0)

  // Track (full quarter)
  DRAW_SEGS(track_color, q_perim);

  // Fill
  if (value > 0 && max > 0) {
    int clamped = value > max ? max : value;
    int fill_dist = clamped * q_perim / max;
    DRAW_SEGS(fill_color, fill_dist);
  }

  #undef DRAW_SEGS
}
#endif

// ============================================================================
// Layer 2 update
// ============================================================================
void layer2_update(Layer *layer, GContext *ctx, uint32_t steps, uint32_t step_goal,
                   uint8_t battery_pct) {
  GRect bounds = layer_get_bounds(layer);

  // Position bars at the midpoint between layer 1 edge and layer 3 edge.
  // In layer 2 local coords: layer 2 outer = 0, layer 3 inner = bounds/4 inset.
  // Midpoint = bounds/8 inset from layer 2 edge.
  int16_t mid_inset_w = bounds.size.w / 8;
  int16_t mid_inset_h = bounds.size.h / 8;
  GRect bar_rect = grect_inset(bounds, GEdgeInsets(mid_inset_h, mid_inset_w, mid_inset_h, mid_inset_w));

  GColor steps_fill  = PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite);
  GColor batt_fill   = PBL_IF_COLOR_ELSE(GColorCyan, GColorWhite);
  GColor track_color = PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray);

#ifdef PBL_ROUND
  int16_t thickness = 8;
  // Steps: top-center (0°) → right-center (90°)
  draw_quarter_arc(ctx, bar_rect, thickness,
                   steps_fill, track_color,
                   DEG_TO_TRIGANGLE(0), DEG_TO_TRIGANGLE(90),
                   steps, step_goal);
  // Battery: bottom-center (180°) → left-center (270°)
  draw_quarter_arc(ctx, bar_rect, thickness,
                   batt_fill, track_color,
                   DEG_TO_TRIGANGLE(180), DEG_TO_TRIGANGLE(270),
                   battery_pct, 100);
#else
  int16_t band = 6;
  // Steps: quadrant 0 = top-center → right-center
  draw_quarter_rect(ctx, bar_rect, band,
                    steps_fill, track_color,
                    0, steps, step_goal);
  // Battery: quadrant 2 = bottom-center → left-center
  draw_quarter_rect(ctx, bar_rect, band,
                    batt_fill, track_color,
                    2, battery_pct, 100);
#endif

  // -- Labels centered in each quadrant area --
  int16_t hw = bounds.size.w / 2;
  int16_t hh = bounds.size.h / 2;

  // Steps label (upper-right quadrant)
  char steps_str[8];
  snprintf(steps_str, sizeof(steps_str), "%lu", (unsigned long)steps);
  GRect steps_label = GRect(hw - hw / 2, hh / 4 -5, hw / 2, 16);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, steps_str,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     steps_label, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  // Battery label (lower-left quadrant)
  char batt_str[6];
  snprintf(batt_str, sizeof(batt_str), "%d%%", battery_pct);
  GRect batt_label = GRect(hw, hh + hh / 2 + 5, hw / 2 , 16);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, batt_str,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     batt_label, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}
