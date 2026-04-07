#pragma once
#include <pebble.h>

// Layer 3: 2/6 screen — stacked widgets (date, time, weather)
void layer3_update(Layer *layer, GContext *ctx, struct tm *current_time,
                   int weather_temp, const char *weather_cond,
                   GDrawCommandImage *icon_weather);
