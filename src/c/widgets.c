#include "widgets.h"

// Large-screen platforms (emery, gabbro) get bigger fonts.
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  #define IS_LARGE_SCREEN 1
#else
  #define IS_LARGE_SCREEN 0
#endif

#define FONT_SMALL_TEXT     (IS_LARGE_SCREEN ? FONT_KEY_GOTHIC_18      : FONT_KEY_GOTHIC_14)
#define FONT_SMALL_BOLD     (IS_LARGE_SCREEN ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD)
#define FONT_MEDIUM_TEXT    (IS_LARGE_SCREEN ? FONT_KEY_GOTHIC_24      : FONT_KEY_GOTHIC_18)
#define FONT_TIME_LARGE     (IS_LARGE_SCREEN ? FONT_KEY_LECO_32_BOLD_NUMBERS : FONT_KEY_LECO_20_BOLD_NUMBERS)
#define SMALL_TEXT_H        (IS_LARGE_SCREEN ? 22 : 16)

// ============================================================================
// Per-widget renderers. Each draws into the supplied rect.
// ============================================================================

static void prv_render_steps(GContext *ctx, GRect rect, const WidgetState *s) {
  // Manual itoa: avoid snprintf format-parser overhead on every redraw.
  char buf[12];
  uint32_t n = s->steps;
  int len;
  if (n == 0) {
    buf[0] = '0'; buf[1] = '\0'; len = 1;
  } else {
    char tmp[12];
    int t = 0;
    while (n > 0 && t < 11) { tmp[t++] = '0' + (n % 10); n /= 10; }
    len = t;
    for (int i = 0; i < t; i++) buf[i] = tmp[t - 1 - i];
    buf[len] = '\0';
  }

  GSize icon_size = s->icon_steps ? s->icon_steps_size : GSize(0, 0);
  int16_t gap = s->icon_steps ? 4 : 0;
  int16_t text_h = SMALL_TEXT_H;
  int16_t max_text_w = IS_LARGE_SCREEN ? 70 : 50;

  GFont font = fonts_get_system_font(FONT_SMALL_TEXT);
  GSize text_size = graphics_text_layout_get_content_size(
    buf, font,
    GRect(0, 0, max_text_w, text_h),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);

  int16_t group_w = text_size.w + gap + icon_size.w;
  int16_t group_x = rect.origin.x + (rect.size.w - group_w) / 2;
  int16_t center_y = rect.origin.y + rect.size.h / 2;

  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, buf, font,
                     GRect(group_x, center_y - text_h / 2, text_size.w, text_h),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);

  if (s->icon_steps) {
    gdraw_command_image_draw(ctx, s->icon_steps,
      GPoint(group_x + text_size.w + gap, center_y - icon_size.h / 2));
  }
}

static void prv_render_battery(GContext *ctx, GRect rect, const WidgetState *s) {
  GColor batt_fill = PBL_IF_COLOR_ELSE(GColorCyan, GColorWhite);

  int16_t border = 1;
  int16_t pad    = 1;
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  int16_t fill_h = 6;
#else
  // Small screens: ~2px thinner.
  int16_t fill_h = 4;
#endif
  int16_t body_h = fill_h + 2 * (border + pad);
  int16_t nub_w  = 3;
  int16_t nub_h  = fill_h;
#if defined(PBL_ROUND)
  // Round platforms (chalk, gabbro): 20% shorter than emery.
  int16_t body_w = rect.size.w * 4 / 6 * 4 / 5 * 4 / 5;
#elif defined(PBL_PLATFORM_EMERY)
  int16_t body_w = rect.size.w * 4 / 6 * 4 / 5;
#else
  int16_t body_w = rect.size.w * 4 / 6;
#endif
  int16_t total_w = body_w + nub_w;

  int16_t body_x = rect.origin.x + (rect.size.w - total_w) / 2;
  int16_t body_y = rect.origin.y + (rect.size.h - body_h) / 2;
  int16_t max_fill_w = body_w - 2 * (border + pad);
  int16_t fill_x = body_x + border + pad;
  int16_t fill_y = body_y + border + pad;
  int16_t nub_x  = body_x + body_w;
  int16_t nub_y  = body_y + (body_h - nub_h) / 2;

  graphics_context_set_fill_color(ctx, batt_fill);
  graphics_fill_rect(ctx, GRect(nub_x, nub_y, nub_w, nub_h), 0, GCornerNone);

  graphics_context_set_stroke_color(ctx, batt_fill);
  graphics_draw_rect(ctx, GRect(body_x, body_y, body_w, body_h));

  if (s->battery_pct > 0 && max_fill_w > 0) {
    int16_t fill_w = (int16_t)(max_fill_w * s->battery_pct / 100);
    if (fill_w > 0) {
      graphics_fill_rect(ctx, GRect(fill_x, fill_y, fill_w, fill_h), 0, GCornerNone);
    }
  }
}

static void prv_render_time(GContext *ctx, GRect rect, const WidgetState *s) {
  char buf[8];
  strftime(buf, sizeof(buf),
           s->is_24h ? "%H:%M" : "%I:%M", s->current_time);

#if IS_LARGE_SCREEN
  GFont font = fonts_get_system_font(FONT_TIME_LARGE);
#else
  GFont font = (rect.size.w >= 70 && rect.size.h >= 22)
    ? fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS)
    : fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
#endif

  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, buf, font, rect,
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}

static void prv_render_date(GContext *ctx, GRect rect, const WidgetState *s) {
  static const char *months[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  char buf[12];
  snprintf(buf, sizeof(buf), "%s-%02d",
           months[s->current_time->tm_mon], s->current_time->tm_mday);

  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, buf,
                     fonts_get_system_font(FONT_SMALL_BOLD),
                     rect, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}

static void prv_render_weather(GContext *ctx, GRect rect, const WidgetState *s) {
  if (!s->connected && s->icon_disconnect) {
    GSize icon_size = s->icon_disconnect_size;
    int16_t icon_x = rect.origin.x + (rect.size.w - icon_size.w) / 2;
    int16_t icon_y = rect.origin.y + (rect.size.h - icon_size.h) / 2;
    gdraw_command_image_draw(ctx, s->icon_disconnect, GPoint(icon_x, icon_y));
    return;
  }

  char buf[12];
  if (s->weather_temp == -999) {
    snprintf(buf, sizeof(buf), "--");
  } else {
    char unit = (s->weather_units == 'F') ? 'F' : 'C';
    snprintf(buf, sizeof(buf), "%d\xc2\xb0 %c", s->weather_temp, unit);
  }

  graphics_context_set_text_color(ctx, GColorWhite);

  if (s->icon_weather) {
    GSize icon_size = s->icon_weather_size;
    int16_t gap = 3;
    int16_t text_w = IS_LARGE_SCREEN ? 56 : 40;
    int16_t group_w = icon_size.w + gap + text_w;
    int16_t group_x = rect.origin.x + (rect.size.w - group_w) / 2;
    int16_t icon_y  = rect.origin.y + (rect.size.h - icon_size.h) / 2;
    gdraw_command_image_draw(ctx, s->icon_weather, GPoint(group_x, icon_y));
    graphics_draw_text(ctx, buf,
                       fonts_get_system_font(FONT_MEDIUM_TEXT),
                       GRect(group_x + icon_size.w + gap, rect.origin.y, text_w, rect.size.h),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  } else {
    graphics_draw_text(ctx, buf,
                       fonts_get_system_font(FONT_SMALL_TEXT),
                       rect, GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  }
}

void widget_render(GContext *ctx, GRect rect, WidgetId id, const WidgetState *s) {
  switch (id) {
    case WIDGET_STEPS:   prv_render_steps(ctx, rect, s);   break;
    case WIDGET_BATTERY: prv_render_battery(ctx, rect, s); break;
    case WIDGET_TIME:    prv_render_time(ctx, rect, s);    break;
    case WIDGET_DATE:    prv_render_date(ctx, rect, s);    break;
    case WIDGET_WEATHER: prv_render_weather(ctx, rect, s); break;
    default: break;
  }
}

int16_t widget_natural_height(WidgetId id) {
  switch (id) {
    case WIDGET_STEPS:   return IS_LARGE_SCREEN ? 24 : 16;
    case WIDGET_BATTERY: return 14;
    case WIDGET_TIME:    return IS_LARGE_SCREEN ? 34 : 22;
    case WIDGET_DATE:    return IS_LARGE_SCREEN ? 22 : 16;
    case WIDGET_WEATHER: return IS_LARGE_SCREEN ? 28 : 20;
    default:             return 16;
  }
}
