#include "layer2.h"
#include "utils.h"

// ============================================================================
// Quarter-arc progress bar (round screens — battery only)
// ============================================================================
#ifdef PBL_ROUND
static void draw_quarter_arc(GContext *ctx, GRect rect, int16_t thickness,
                              GColor fill_color, GColor track_color,
                              int32_t start_angle, int32_t end_angle,
                              int value, int max) {
  graphics_context_set_fill_color(ctx, track_color);
  graphics_fill_radial(ctx, rect, GOvalScaleModeFitCircle,
                       thickness, start_angle, end_angle);
  if (value > 0 && max > 0) {
    int clamped = value > max ? max : value;
    int32_t fill_end = start_angle +
      (int32_t)((int64_t)clamped * (end_angle - start_angle) / max);
    graphics_context_set_fill_color(ctx, fill_color);
    graphics_fill_radial(ctx, rect, GOvalScaleModeFitCircle,
                         thickness, start_angle, fill_end);
  }
}
#endif

// ============================================================================
// Layer 2 update
// ============================================================================
void layer2_update(Layer *layer, GContext *ctx, uint32_t steps, uint32_t step_goal,
                   uint8_t battery_pct, GDrawCommandImage *icon_steps) {
  GRect bounds = layer_get_bounds(layer);
  int16_t hw = bounds.size.w / 2;

  GColor steps_color = GColorWhite;
  GColor batt_fill   = PBL_IF_COLOR_ELSE(GColorCyan, GColorWhite);
  GColor track_color = PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray);
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
    GRect bar_rect = grect_inset(bounds, GEdgeInsets(mid_inset_h, mid_inset_w,
                                                      mid_inset_h, mid_inset_w));
    draw_quarter_arc(ctx, bar_rect, bar_thickness,
                     batt_fill, track_color,
                     DEG_TO_TRIGANGLE(135), DEG_TO_TRIGANGLE(225),
                     battery_pct, 100);
  }
#else
  (void)mid_inset_w; (void)mid_inset_h;
  {
    int16_t bar_w = bounds.size.w * 4 / 6;
    int16_t bar_x = (bounds.size.w - bar_w) / 2;
    int16_t bar_y = bounds.size.h - bar_thickness - 5;
    graphics_context_set_fill_color(ctx, track_color);
    graphics_fill_rect(ctx, GRect(bar_x, bar_y, bar_w, bar_thickness),
                       bar_thickness, GCornersAll);
    if (battery_pct > 0) {
      int fill_w = bar_w * battery_pct / 100;
      graphics_context_set_fill_color(ctx, batt_fill);
      graphics_fill_rect(ctx, GRect(bar_x, bar_y, fill_w, bar_thickness),
                         bar_thickness, GCornersAll);
    }
  }
#endif
}
