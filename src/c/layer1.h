#pragma once
#include <pebble.h>

// Layer 1: Full screen — tick marks, hour numbers, 12h progress bar
void layer1_update(Layer *layer, GContext *ctx, struct tm *current_time);
