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
    case 0: // 12→3
      if (d <= hw) return GPoint(x + hw + d, y);
      else         return GPoint(x + w, y + (d - hw));
    case 1: // 3→6
      if (d <= hh) return GPoint(x + w, y + hh + d);
      else         return GPoint(x + w - (d - hh), y + h);
    case 2: // 6→9
      if (d <= hw) return GPoint(x + hw - d, y + h);
      else         return GPoint(x, y + h - (d - hw));
    case 3: // 9→12
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

  int seg_lengths[] = { hw, h, w, h, w - hw };
  int remaining = fill_dist;

  for (int seg = 0; seg < 5 && remaining > 0; seg++) {
    int slen = seg_lengths[seg];
    int flen = (remaining > slen) ? slen : remaining;
    remaining -= flen;

    switch (seg) {
      case 0:
        graphics_fill_rect(ctx, GRect(x + hw, y, flen, band), band, GCornerBottomRight);
        break;
      case 1:
        graphics_fill_rect(ctx, GRect(x + w - band, y, band, flen), band, GCornerBottomLeft);
        break;
      case 2:
        graphics_fill_rect(ctx, GRect(x + w - flen, y + h - band, flen, band), band, GCornerTopLeft);
        break;
      case 3:
        graphics_fill_rect(ctx, GRect(x, y + h - flen, band, flen), band, GCornerTopRight);
        break;
      case 4:
        graphics_fill_rect(ctx, GRect(x, y, flen, band), band, GCornerBottomRight);
        break;
    }
  }
}

#endif // !PBL_ROUND

// ============================================================================
// Rounded-square perimeter helpers (rectangular screens only)
// ============================================================================
#ifndef PBL_ROUND

#define ROUNDED_CORNER_R 14

// πr/2 in pixels using integer math: TRIG_MAX_ANGLE / 4 ≈ 16384; arc length
// for a quarter circle is (π/2)·r ≈ 1.5708·r ≈ r * 15708 / 10000.
static inline int arc_quarter_len(int16_t r) {
  return (r * 15708) / 10000;
}

// Total perimeter of a rounded rect with given corner radius.
static int rounded_perim(GRect rect, int16_t r) {
  if (r <= 0) return 2 * rect.size.w + 2 * rect.size.h;
  return 2 * (rect.size.w - 2 * r) + 2 * (rect.size.h - 2 * r) + 4 * arc_quarter_len(r);
}

// Point on a rounded rect perimeter at distance d (clockwise from 12 o'clock).
static GPoint rounded_perim_point_at_d(GRect rect, int16_t r, int d) {
  int x = rect.origin.x, y = rect.origin.y;
  int w = rect.size.w, h = rect.size.h;
  int hw = w / 2;
  int aq = arc_quarter_len(r);
  int s0 = hw - r;          // top-center → TR straight
  int s1 = aq;              // TR arc
  int s2 = h - 2 * r;       // right straight
  int s3 = aq;              // BR arc
  int s4 = w - 2 * r;       // bottom straight
  int s5 = aq;              // BL arc
  int s6 = h - 2 * r;       // left straight
  int s7 = aq;              // TL arc
  // s8 = hw - r (top-left straight back to top-center)

  int b = 0;
  if (d <= (b += s0)) return GPoint(x + hw + d, y);
  int prev = b;
  if (d <= (b += s1)) {
    int32_t a = (int32_t)(TRIG_MAX_ANGLE / 4) * (d - prev) / aq;
    int cx = x + w - r, cy = y + r;
    int sx =  sin_lookup(a) * r / TRIG_MAX_RATIO;
    int sy = -cos_lookup(a) * r / TRIG_MAX_RATIO;
    return GPoint(cx + sx, cy + sy);
  }
  prev = b;
  if (d <= (b += s2)) return GPoint(x + w, y + r + (d - prev));
  prev = b;
  if (d <= (b += s3)) {
    int32_t a = TRIG_MAX_ANGLE / 4 + (int32_t)(TRIG_MAX_ANGLE / 4) * (d - prev) / aq;
    int cx = x + w - r, cy = y + h - r;
    int sx =  sin_lookup(a) * r / TRIG_MAX_RATIO;
    int sy = -cos_lookup(a) * r / TRIG_MAX_RATIO;
    return GPoint(cx + sx, cy + sy);
  }
  prev = b;
  if (d <= (b += s4)) return GPoint(x + w - r - (d - prev), y + h);
  prev = b;
  if (d <= (b += s5)) {
    int32_t a = TRIG_MAX_ANGLE / 2 + (int32_t)(TRIG_MAX_ANGLE / 4) * (d - prev) / aq;
    int cx = x + r, cy = y + h - r;
    int sx =  sin_lookup(a) * r / TRIG_MAX_RATIO;
    int sy = -cos_lookup(a) * r / TRIG_MAX_RATIO;
    return GPoint(cx + sx, cy + sy);
  }
  prev = b;
  if (d <= (b += s6)) return GPoint(x, y + h - r - (d - prev));
  prev = b;
  if (d <= (b += s7)) {
    int32_t a = 3 * TRIG_MAX_ANGLE / 4 + (int32_t)(TRIG_MAX_ANGLE / 4) * (d - prev) / aq;
    int cx = x + r, cy = y + r;
    int sx =  sin_lookup(a) * r / TRIG_MAX_RATIO;
    int sy = -cos_lookup(a) * r / TRIG_MAX_RATIO;
    return GPoint(cx + sx, cy + sy);
  }
  prev = b;
  // top-left straight back to top-center
  return GPoint(x + r + (d - prev), y);
}

// Unified perimeter sampling: rounded when r > 0, sharp rectangle when r == 0.
static GPoint perim_point_at_tick(GRect rect, int16_t r, int tick, int total_ticks) {
  if (r <= 0) return rect_perimeter_point(rect, tick, total_ticks);
  int perim = rounded_perim(rect, r);
  return rounded_perim_point_at_d(rect, r, tick * perim / total_ticks);
}

// Draw the band of a rounded-rect from 12 o'clock, clockwise, for `progress/total`.
static void rounded_draw_progress(GContext *ctx, GRect outer, int16_t r,
                                   int16_t band, int progress, int total) {
  int x = outer.origin.x, y = outer.origin.y;
  int w = outer.size.w, h = outer.size.h;
  int hw = w / 2;
  int aq = arc_quarter_len(r);
  int seg[9] = {
    hw - r, aq, h - 2 * r, aq, w - 2 * r, aq, h - 2 * r, aq, hw - r
  };
  int perim = 0;
  for (int i = 0; i < 9; i++) perim += seg[i];
  int remaining = progress * perim / total;

  for (int i = 0; i < 9 && remaining > 0; i++) {
    int slen = seg[i];
    int flen = (remaining > slen) ? slen : remaining;
    remaining -= flen;
    if (flen <= 0) continue;

    switch (i) {
      case 0:
        graphics_fill_rect(ctx, GRect(x + hw, y, flen, band), 0, GCornerNone);
        break;
      case 1: {
        int cx = x + w - r, cy = y + r;
        int32_t a_end = (int32_t)(TRIG_MAX_ANGLE / 4) * flen / aq;
        graphics_fill_radial(ctx, GRect(cx - r, cy - r, 2 * r, 2 * r),
                             GOvalScaleModeFitCircle, band, 0, a_end);
        break;
      }
      case 2:
        graphics_fill_rect(ctx, GRect(x + w - band, y + r, band, flen), 0, GCornerNone);
        break;
      case 3: {
        int cx = x + w - r, cy = y + h - r;
        int32_t a_end = TRIG_MAX_ANGLE / 4 + (int32_t)(TRIG_MAX_ANGLE / 4) * flen / aq;
        graphics_fill_radial(ctx, GRect(cx - r, cy - r, 2 * r, 2 * r),
                             GOvalScaleModeFitCircle, band,
                             TRIG_MAX_ANGLE / 4, a_end);
        break;
      }
      case 4:
        graphics_fill_rect(ctx, GRect(x + w - r - flen, y + h - band, flen, band), 0, GCornerNone);
        break;
      case 5: {
        int cx = x + r, cy = y + h - r;
        int32_t a_end = TRIG_MAX_ANGLE / 2 + (int32_t)(TRIG_MAX_ANGLE / 4) * flen / aq;
        graphics_fill_radial(ctx, GRect(cx - r, cy - r, 2 * r, 2 * r),
                             GOvalScaleModeFitCircle, band,
                             TRIG_MAX_ANGLE / 2, a_end);
        break;
      }
      case 6:
        graphics_fill_rect(ctx, GRect(x, y + h - r - flen, band, flen), 0, GCornerNone);
        break;
      case 7: {
        int cx = x + r, cy = y + r;
        int32_t a_end = 3 * TRIG_MAX_ANGLE / 4 + (int32_t)(TRIG_MAX_ANGLE / 4) * flen / aq;
        graphics_fill_radial(ctx, GRect(cx - r, cy - r, 2 * r, 2 * r),
                             GOvalScaleModeFitCircle, band,
                             3 * TRIG_MAX_ANGLE / 4, a_end);
        break;
      }
      case 8:
        graphics_fill_rect(ctx, GRect(x + r, y, flen, band), 0, GCornerNone);
        break;
    }
  }
}

#endif // !PBL_ROUND

// ============================================================================
// Background + minute progress band (repainted every minute)
// ============================================================================
void layer1_bg_update(Layer *layer, GContext *ctx, int minute,
                      GColor progress_color, bool rounded) {
  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  if (minute <= 0) return;

#ifdef PBL_ROUND
  (void)rounded;
  {
    int32_t progress_angle = (int32_t)(minute * TRIG_MAX_ANGLE / 60);
    int16_t outer_inset = 2;
    int16_t band = 10;
    GRect arc_rect = grect_inset(bounds, GEdgeInsets(outer_inset));

    graphics_context_set_fill_color(ctx, progress_color);
    graphics_fill_radial(ctx, arc_rect, GOvalScaleModeFitCircle,
                         band, DEG_TO_TRIGANGLE(0), progress_angle);
  }
#else
  {
    int16_t outer_inset = 2;
    int16_t band = 8;
    GRect prog_rect = grect_inset(bounds, GEdgeInsets(outer_inset));

    graphics_context_set_fill_color(ctx, progress_color);
    if (rounded) {
      rounded_draw_progress(ctx, prog_rect, ROUNDED_CORNER_R, band, minute, 60);
    } else {
      rect_draw_progress(ctx, prog_rect, band, minute, 60);
    }
  }
#endif
}

// ============================================================================
// Static chrome: tick marks + hour numbers (repainted once per hour)
// ============================================================================
void layer1_chrome_update(Layer *layer, GContext *ctx, int current_hour12,
                          bool rounded) {
  GRect bounds = layer_get_bounds(layer);
  int16_t tick_outer_inset = 2;

#ifdef PBL_ROUND
  (void)rounded;
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
    int16_t r_outer = rounded ? ROUNDED_CORNER_R : 0;

    for (int i = 0; i < 60; i++) {
      bool is_hour = (i % 5 == 0);
      int16_t inner_inset = is_hour ? tick_inner_hour : tick_inner_minute;
      GRect inner_rect = grect_inset(bounds, GEdgeInsets(inner_inset));
      int16_t r_inner = rounded ? (ROUNDED_CORNER_R - (inner_inset - tick_outer_inset)) : 0;
      if (r_inner < 0) r_inner = 0;

      GPoint p_outer = perim_point_at_tick(outer_rect, r_outer, i, 60);
      GPoint p_inner = perim_point_at_tick(inner_rect, r_inner, i, 60);

      graphics_context_set_stroke_width(ctx, is_hour ? 3 : 1);
      graphics_context_set_stroke_color(ctx, GColorWhite);
      graphics_draw_line(ctx, p_outer, p_inner);
    }
  }
#endif

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
      GRect text_box = GRect(pos.x - 14, pos.y - 9, 28, 20);
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
    int16_t r_num = rounded ? (ROUNDED_CORNER_R - (number_inset - tick_outer_inset)) : 0;
    if (r_num < 0) r_num = 0;

    char hour_str[3];
    for (int h = 1; h <= 12; h++) {
      int tick = h * 5;
      if (tick >= 60) tick -= 60;
      GPoint pos = perim_point_at_tick(number_rect, r_num, tick, 60);

      bool is_current = (h == current_hour12);
#ifdef PBL_COLOR
      graphics_context_set_text_color(ctx, is_current ? GColorWhite : GColorDarkGray);
#else
      if (is_current) {
        graphics_context_set_stroke_color(ctx, GColorWhite);
        graphics_draw_rect(ctx, GRect(pos.x - 13, pos.y - 8, 23, 20));
      }
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
