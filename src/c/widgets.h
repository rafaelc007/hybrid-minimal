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
} WidgetState;

// Render the widget identified by `id` into `rect` using `state`.
void widget_render(GContext *ctx, GRect rect, WidgetId id, const WidgetState *state);

// Minimum vertical pixels the widget needs to render legibly on this platform.
int16_t widget_natural_height(WidgetId id);
