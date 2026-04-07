#include "layer3.h"
#include "utils.h"

void layer3_update(Layer *layer, GContext *ctx, struct tm *current_time,
                   int weather_temp, const char *weather_cond,
                   GDrawCommandImage *icon_weather) {
  GRect bounds = layer_get_bounds(layer);

  // Three stacked slots: date (top), weather (middle), digital time (bottom)
  int16_t padding = 2;
  int16_t slot_w = bounds.size.w - 2 * padding;
  int16_t total_h = bounds.size.h - 2 * padding;
  int16_t slot_h = (total_h - 2 * padding) / 3;

  GRect top_slot = GRect(padding, padding, slot_w, slot_h);
  GRect mid_slot = GRect(padding, padding + slot_h + padding, slot_w, slot_h);
  GRect bot_slot = GRect(padding, padding + 2 * (slot_h + padding), slot_w, slot_h);

  // -- Top: Digital time (hh:mm) --
  char time_str[6];
  strftime(time_str, sizeof(time_str),
           clock_is_24h_style() ? "%H:%M" : "%I:%M", current_time);

  GFont time_font = (bounds.size.w >= 70)
    ? fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS)
    : fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, time_str,
                     time_font,
                     top_slot, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  // -- Middle: Date (e.g. "Apr-02") --
  static const char *months[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  char date_str[8];
  snprintf(date_str, sizeof(date_str), "%s-%02d",
           months[current_time->tm_mon], current_time->tm_mday);

  graphics_draw_text(ctx, date_str,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     mid_slot, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  // -- Bottom: Weather (icon + temp only) --
  char weather_str[8];
  if (weather_temp == -999) {
    snprintf(weather_str, sizeof(weather_str), "--");
  } else {
    snprintf(weather_str, sizeof(weather_str), "%d\xc2\xb0 C", weather_temp);
  }

  if (icon_weather) {
    GSize icon_size = gdraw_command_image_get_bounds_size(icon_weather);
    int16_t gap     = 3;
    int16_t text_w  = 36;  // wide enough for "-10°" / "100°" in GOTHIC_14
    int16_t group_w = icon_size.w + gap + text_w;
    int16_t group_x = bot_slot.origin.x + (slot_w - group_w) / 2;
    int16_t icon_y  = bot_slot.origin.y + (slot_h - icon_size.h) / 2;
    gdraw_command_image_draw(ctx, icon_weather, GPoint(group_x, icon_y));
    graphics_draw_text(ctx, weather_str,
                       fonts_get_system_font(FONT_KEY_GOTHIC_18),
                       GRect(group_x + icon_size.w + gap, bot_slot.origin.y, text_w, slot_h),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  } else {
    graphics_draw_text(ctx, weather_str,
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       bot_slot, GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  }
}
