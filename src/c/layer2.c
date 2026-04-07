#include "layer2.h"
#include "utils.h"

// ============================================================================
// Battery arc outline (round screens)

// ============================================================================
// Layer 2 update
// ============================================================================
void layer2_update(Layer *layer, GContext *ctx, uint32_t steps, uint32_t step_goal,
                   uint8_t battery_pct, GDrawCommandImage *icon_steps) {
  GRect bounds = layer_get_bounds(layer);
  int16_t hw = bounds.size.w / 2;

  GColor steps_color = GColorWhite;
  GColor batt_fill   = PBL_IF_COLOR_ELSE(GColorCyan, GColorWhite);
  int16_t bar_thickness = PBL_IF_ROUND_ELSE(8, 6);

  // -- Steps: count centered + step icon to the right --
  // Layout: [count 40px] [4px gap] [icon 10px], group centered
  int16_t text_w = 40;
  int16_t icon_w = 10;
  int16_t gap    = 4;
  int16_t group_w = text_w + gap + icon_w;
  int16_t group_x = hw - group_w / 2;
  int16_t center_y = bar_thickness + 10;  // near the top of layer 2

  char steps_str[8];
  snprintf(steps_str, sizeof(steps_str), "%lu", (unsigned long)steps);

  GRect text_box = GRect(group_x, center_y - 7, text_w, 14);
  graphics_context_set_text_color(ctx, steps_color);
  graphics_draw_text(ctx, steps_str,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     text_box, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentRight, NULL);

  GPoint icon_origin = GPoint(group_x + text_w + gap, center_y - 5);
  if (icon_steps) {
    GSize icon_size = gdraw_command_image_get_bounds_size(icon_steps);
    GRect icon_rect = GRect(icon_origin.x, icon_origin.y, icon_size.w, icon_size.h);
    gdraw_command_image_draw(ctx, icon_steps, icon_rect.origin);
  }

  // -- Battery widget --
  int16_t mid_inset_w = bounds.size.w / 14;
  int16_t mid_inset_h = bounds.size.h / 14;

#ifdef PBL_ROUND
  {
    // Battery arc outline: 1px outer border + 1px gap + 4px fill + 1px gap + 1px inner border
    int16_t border  = 1;
    int16_t pad     = 1;
    int16_t fill_t  = 4;
    int16_t outer_t = border + pad + fill_t + pad + border;  // 8px total

    GColor track_color = PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray);
    GRect arc_rect = grect_inset(bounds, GEdgeInsets(mid_inset_h, mid_inset_w,
                                                      mid_inset_h, mid_inset_w));
    int32_t start_angle = DEG_TO_TRIGANGLE(135);
    int32_t end_angle   = DEG_TO_TRIGANGLE(225);

    // 1. Outer shell in batt_fill (full range) — forms both borders
    graphics_context_set_fill_color(ctx, batt_fill);
    graphics_fill_radial(ctx, arc_rect, GOvalScaleModeFitCircle,
                         outer_t, start_angle, end_angle);

    // 2. Hollow out interior with track_color, preserving 1px outer + 1px inner borders
    GRect hollow_rect = grect_inset(arc_rect, GEdgeInsets(border));
    graphics_context_set_fill_color(ctx, track_color);
    graphics_fill_radial(ctx, hollow_rect, GOvalScaleModeFitCircle,
                         outer_t - 2 * border, start_angle, end_angle);

    // 3. Fill level bar
    if (battery_pct > 0) {
      GRect fill_rect = grect_inset(arc_rect, GEdgeInsets(border + pad));
      int32_t fill_end = start_angle +
        (int32_t)((int64_t)battery_pct * (end_angle - start_angle) / 100);
      graphics_context_set_fill_color(ctx, batt_fill);
      graphics_fill_radial(ctx, fill_rect, GOvalScaleModeFitCircle,
                           fill_t, start_angle, fill_end);
    }

    // 4. Nub at the end of the arc (positive terminal)
    GRect nub_arc_rect = grect_inset(arc_rect, GEdgeInsets(outer_t / 2));
    GPoint nub_pt = gpoint_from_polar(nub_arc_rect, GOvalScaleModeFitCircle, end_angle);
    GPoint end = gpoint_from_polar(nub_arc_rect, GOvalScaleModeFitCircle, end_angle+DEG_TO_TRIGANGLE(1));
    graphics_context_set_stroke_color(ctx, batt_fill);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_line(ctx, nub_pt, end);
  }
#else
  (void)mid_inset_w; (void)mid_inset_h;
  {
    // Battery contour: 1px border, 1px gap, fill bar, nub on right
    int16_t border     = 1;
    int16_t pad        = 1;
    int16_t fill_h     = 4;
    int16_t body_h     = fill_h + 2 * (border + pad);  // 8px total
    int16_t nub_w      = 3;
    int16_t nub_h      = fill_h;
    int16_t body_w     = bounds.size.w * 4 / 6;
    int16_t total_w    = body_w + nub_w;
    int16_t body_x     = (bounds.size.w - total_w) / 2;
    int16_t body_y     = bounds.size.h - body_h - 5;
    int16_t max_fill_w = body_w - 2 * (border + pad);
    int16_t fill_x     = body_x + border + pad;
    int16_t fill_y     = body_y + border + pad;
    int16_t nub_x      = body_x + body_w;
    int16_t nub_y      = body_y + (body_h - nub_h) / 2;

    // Nub - always filled (positive terminal)
    graphics_context_set_fill_color(ctx, batt_fill);
    graphics_fill_rect(ctx, GRect(nub_x, nub_y, nub_w, nub_h), 0, GCornerNone);

    // Body outline (1px stroke, sharp corners)
    graphics_context_set_stroke_color(ctx, batt_fill);
    graphics_draw_rect(ctx, GRect(body_x, body_y, body_w, body_h));

    // Fill bar (based on battery %)
    if (battery_pct > 0 && max_fill_w > 0) {
      int16_t fill_w = (int16_t)(max_fill_w * battery_pct / 100);
      if (fill_w > 0) {
        graphics_context_set_fill_color(ctx, batt_fill);
        graphics_fill_rect(ctx, GRect(fill_x, fill_y, fill_w, fill_h), 0, GCornerNone);
      }
    }
  }
#endif
}
