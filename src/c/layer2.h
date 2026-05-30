#pragma once
#include <pebble.h>

// Layer 2: stacked widgets.
//
// Two render functions share this module because both regions draw a stack of
// widgets on top of layer 1:
//   layer2_update       — outer region (4/6): steps + battery arc/bar
//   layer2_inner_update — inner region (2/6): time, date, weather
void layer2_update(Layer *layer, GContext *ctx, uint32_t steps, uint32_t step_goal,
                   uint8_t battery_pct, GDrawCommandImage *icon_steps);

void layer2_inner_update(Layer *layer, GContext *ctx, struct tm *current_time,
                         int weather_temp, const char *weather_cond,
                         GDrawCommandImage *icon_weather);
