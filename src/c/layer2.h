#pragma once
#include <pebble.h>

// Layer 2: 4/6 screen — arc progress widgets (steps, battery)
void layer2_update(Layer *layer, GContext *ctx, uint32_t steps, uint32_t step_goal,
                   uint8_t battery_pct, GBitmap *icon_steps);
