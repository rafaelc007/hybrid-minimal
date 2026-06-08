#pragma once
#include <pebble.h>

typedef enum {
  WIDGET_STEPS = 0,
  WIDGET_BATTERY,
  WIDGET_TIME,
  WIDGET_DATE,
  WIDGET_WEATHER,
  WIDGET_COUNT
} WidgetId;

typedef enum {
  DATE_FMT_MMM_DD = 0,        // Mar-05
  DATE_FMT_MM_DD,             // 03-05
  DATE_FMT_DD_MM,             // 05-03
  DATE_FMT_DD_MM_YY,          // 05-03-26
  DATE_FMT_DD_WEEKDAY,        // 05-Mon
  DATE_FMT_WEEKDAY_DD_MM,     // Mon | 05-03
  DATE_FMT_WEEKDAY_MM_DD,     // Mon | 03-05
  DATE_FMT_COUNT
} DateFormat;

typedef struct {
  uint32_t steps;
  uint32_t step_goal;
  uint8_t battery_pct;
  struct tm *current_time;
  int weather_temp;
  const char *weather_cond;
  GDrawCommandImage *icon_steps;
  GDrawCommandImage *icon_weather;
  GDrawCommandImage *icon_disconnect;
  bool connected;
  bool is_24h;
  char weather_units;  // 'C' or 'F'
  uint8_t date_format; // DateFormat enum
  // Pre-measured icon sizes (avoid gdraw_command_image_get_bounds_size per draw).
  GSize icon_steps_size;
  GSize icon_weather_size;
  GSize icon_disconnect_size;
} WidgetState;

// Render the widget identified by `id` into `rect` using `state`.
void widget_render(GContext *ctx, GRect rect, WidgetId id, const WidgetState *state);

// Minimum vertical pixels the widget needs to render legibly on this platform.
int16_t widget_natural_height(WidgetId id);
