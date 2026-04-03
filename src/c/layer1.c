#include "layer1.h"
#include "utils.h"

// ============================================================================
// Rectangular perimeter helpers (square/rect screens only)
// ============================================================================
#ifndef PBL_ROUND

// Get a point on a rectangle's perimeter at a given tick position.
// tick 0 = top-center (12 o'clock), going clockwise.
// Each quadrant of 15 ticks maps to one quarter of the perimeter,
// ensuring 12/3/6/9 always land at edge midpoints.
static GPoint rect_perimeter_point(GRect rect, int tick, int total_ticks) {
  int x = rect.origin.x;
  int y = rect.origin.y;
  int w = rect.size.w;
  int h = rect.size.h;
  int hw = w / 2;
  int hh = h / 2;

  int ticks_per_q = total_ticks / 4;
  int quadrant = tick / ticks_per_q;
  int pos = tick % ticks_per_q;

  // Distance along this quadrant's perimeter (hw + hh total per quadrant)
  int q_perim = hw + hh;
  int d = pos * q_perim / ticks_per_q;

  switch (quadrant) {
    case 0: // 12→3: top-center → top-right corner → right-center
      if (d <= hw) return GPoint(x + hw + d, y);
      else         return GPoint(x + w, y + (d - hw));
    case 1: // 3→6: right-center → bottom-right corner → bottom-center
      if (d <= hh) return GPoint(x + w, y + hh + d);
      else         return GPoint(x + w - (d - hh), y + h);
    case 2: // 6→9: bottom-center → bottom-left corner → left-center
      if (d <= hw) return GPoint(x + hw - d, y + h);
      else         return GPoint(x, y + h - (d - hw));
    case 3: // 9→12: left-center → top-left corner → top-center
      if (d <= hh) return GPoint(x, y + hh - d);
      else         return GPoint(x + (d - hh), y);
    default:
      return GPoint(x + hw, y);
  }
}

// Draw a rectangular progress bar that fills the band along the screen edges,
// clockwise from 12 o'clock. progress/total = fraction filled (0..1).
static void rect_draw_progress(GContext *ctx, GRect outer, int16_t band,
                                int progress, int total) {
  int x = outer.origin.x;
  int y = outer.origin.y;
  int w = outer.size.w;
  int h = outer.size.h;
  int hw = w / 2;

  int perim = 2 * w + 2 * h;
  int fill_dist = progress * perim / total;

  // 5 perimeter segments starting from top-center clockwise:
  //   top-right half (hw), right edge (h), bottom edge (w), left edge (h), top-left half
  int seg_lengths[] = { hw, h, w, h, w - hw };
  int remaining = fill_dist;

  for (int seg = 0; seg < 5 && remaining > 0; seg++) {
    int slen = seg_lengths[seg];
    int flen = (remaining > slen) ? slen : remaining;
    remaining -= flen;

    switch (seg) {
      case 0: // Top edge, center → right
        graphics_fill_rect(ctx, GRect(x + hw, y, flen, band), 0, GCornerNone);
        break;
      case 1: // Right edge, top → bottom
        graphics_fill_rect(ctx, GRect(x + w - band, y, band, flen), 0, GCornerNone);
        break;
      case 2: // Bottom edge, right → left
        graphics_fill_rect(ctx, GRect(x + w - flen, y + h - band, flen, band), 0, GCornerNone);
        break;
      case 3: // Left edge, bottom → top
        graphics_fill_rect(ctx, GRect(x, y + h - flen, band, flen), 0, GCornerNone);
        break;
      case 4: // Top edge, left → center
        graphics_fill_rect(ctx, GRect(x, y, flen, band), 0, GCornerNone);
        break;
    }
  }
}

#endif // !PBL_ROUND

// ============================================================================
// Layer 1 update: tick marks, hour numbers, 12h progress bar
// ============================================================================
void layer1_update(Layer *layer, GContext *ctx, struct tm *current_time) {
  GRect bounds = layer_get_bounds(layer);

  // Background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // -- Minute progress bar (fills clockwise from 12, resets each hour) --
  int32_t minutes = current_time->tm_min; // 0..59

#ifdef PBL_ROUND
  {
    int32_t progress_angle = (int32_t)(minutes * TRIG_MAX_ANGLE / 60);
    if (progress_angle > 0) {
      int16_t outer_inset = 2;
      int16_t band = 10;
      GRect arc_rect = grect_inset(bounds, GEdgeInsets(outer_inset));

      graphics_context_set_fill_color(ctx, GColorJazzberryJam);
      graphics_fill_radial(ctx, arc_rect, GOvalScaleModeFitCircle,
                           band, DEG_TO_TRIGANGLE(0), progress_angle);
    }
  }
#else
  {
    if (minutes > 0) {
    int16_t outer_inset = 2;
    int16_t band = 8;
    GRect prog_rect = grect_inset(bounds, GEdgeInsets(outer_inset));

    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorJazzberryJam, GColorLightGray));
    rect_draw_progress(ctx, prog_rect, band, minutes, 60);
    }
  }
#endif

  // -- Tick marks: 60 minute ticks, larger for hours --
  int16_t tick_outer_inset = 2;

#ifdef PBL_ROUND
  {
    int16_t tick_inner_minute = 8;
    int16_t tick_inner_hour   = 14;
    GRect outer_rect = grect_inset(bounds, GEdgeInsets(tick_outer_inset));

    for (int i = 0; i < 60; i++) {
      int32_t angle = TRIG_MAX_ANGLE * i / 60;
      bool is_hour = (i % 5 == 0);
      int16_t inner_inset = is_hour ? tick_inner_hour : tick_inner_minute;
      GRect inner_rect = grect_inset(bounds, GEdgeInsets(inner_inset));

      GPoint p_outer = gpoint_from_polar(outer_rect, GOvalScaleModeFitCircle, angle);
      GPoint p_inner = gpoint_from_polar(inner_rect, GOvalScaleModeFitCircle, angle);

      graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite));
      graphics_context_set_stroke_width(ctx, is_hour ? 3 : 1);
      graphics_context_set_stroke_color(ctx, GColorWhite);
      graphics_draw_line(ctx, p_outer, p_inner);
    }
  }
#else
  {
    int16_t tick_inner_minute = 7;
    int16_t tick_inner_hour   = 12;
    GRect outer_rect = grect_inset(bounds, GEdgeInsets(tick_outer_inset));

    for (int i = 0; i < 60; i++) {
      bool is_hour = (i % 5 == 0);
      int16_t inner_inset = is_hour ? tick_inner_hour : tick_inner_minute;
      GRect inner_rect = grect_inset(bounds, GEdgeInsets(inner_inset));

      GPoint p_outer = rect_perimeter_point(outer_rect, i, 60);
      GPoint p_inner = rect_perimeter_point(inner_rect, i, 60);

      graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite));
      graphics_context_set_stroke_width(ctx, is_hour ? 3 : 1);
      graphics_context_set_stroke_color(ctx, GColorWhite);
      graphics_draw_line(ctx, p_outer, p_inner);
    }
  }
#endif

  // -- Hour numbers (1–12): current hour white, others gray --
  int current_hour12 = current_time->tm_hour % 12;
  if (current_hour12 == 0) current_hour12 = 12;

#ifdef PBL_ROUND
  {
    int16_t number_inset = 28;
    GRect number_rect = grect_inset(bounds, GEdgeInsets(number_inset));

    char hour_str[3];
    for (int h = 1; h <= 12; h++) {
      int32_t angle = TRIG_MAX_ANGLE * h / 12;
      GPoint pos = gpoint_from_polar(number_rect, GOvalScaleModeFitCircle, angle);

      bool is_current = (h == current_hour12);
      graphics_context_set_text_color(ctx, is_current ? GColorWhite : GColorDarkGray);

      snprintf(hour_str, sizeof(hour_str), "%d", h);
      GRect text_box = GRect(pos.x - 10, pos.y - 9, 20, 18);
      graphics_draw_text(ctx, hour_str,
                         fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS),
                         text_box, GTextOverflowModeTrailingEllipsis,
                         GTextAlignmentCenter, NULL);
    }
  }
#else
  {
    int16_t number_inset = 24;
    GRect number_rect = grect_inset(bounds, GEdgeInsets(number_inset));

    char hour_str[3];
    for (int h = 1; h <= 12; h++) {
      int tick = h * 5;
      if (tick >= 60) tick -= 60;  // hour 12 → tick 0
      GPoint pos = rect_perimeter_point(number_rect, tick, 60);

      bool is_current = (h == current_hour12);
#ifdef PBL_COLOR
      graphics_context_set_text_color(ctx, is_current ? GColorWhite : GColorDarkGray);
#else
      (void)is_current;
      graphics_context_set_text_color(ctx, GColorWhite);
#endif

      snprintf(hour_str, sizeof(hour_str), "%d", h);
      GRect text_box = GRect(pos.x - 12, pos.y - 10, 24, 20);
      graphics_draw_text(ctx, hour_str,
                         fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS),
                         text_box, GTextOverflowModeTrailingEllipsis,
                         GTextAlignmentCenter, NULL);
    }
  }
#endif
}
