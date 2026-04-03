#include "layer3.h"
#include "utils.h"

void layer3_update(Layer *layer, GContext *ctx, struct tm *current_time,
                   int weather_temp, const char *weather_cond) {
  GRect bounds = layer_get_bounds(layer);

  // Three stacked slots: date (top), weather (middle), digital time (bottom)
  int16_t padding = 2;
  int16_t slot_w = bounds.size.w - 2 * padding;
  int16_t total_h = bounds.size.h - 2 * padding;
  int16_t slot_h = (total_h - 2 * padding) / 3;

  GRect top_slot = GRect(padding, padding, slot_w, slot_h);
  GRect mid_slot = GRect(padding, padding + slot_h + padding, slot_w, slot_h);
  GRect bot_slot = GRect(padding, padding + 2 * (slot_h + padding), slot_w, slot_h);

  // -- Top: Date (e.g. "Apr-02") --
  static const char *months[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  char date_str[8];
  snprintf(date_str, sizeof(date_str), "%s-%02d",
           months[current_time->tm_mon], current_time->tm_mday);

  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, date_str,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     top_slot, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  // -- Middle: Weather (temp + condition) --
  char weather_str[16];
  if (weather_temp == -999) {
    snprintf(weather_str, sizeof(weather_str), "--");
  } else {
    snprintf(weather_str, sizeof(weather_str), "%d° %s", weather_temp,
             weather_cond ? weather_cond : "");
  }

  graphics_draw_text(ctx, weather_str,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     mid_slot, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  // -- Bottom: Digital time (hh:mm) --
  char time_str[6];
  strftime(time_str, sizeof(time_str),
           clock_is_24h_style() ? "%H:%M" : "%I:%M", current_time);

  GFont time_font = (bounds.size.w >= 60)
    ? fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS)
    : fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  graphics_draw_text(ctx, time_str,
                     time_font,
                     bot_slot, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}
