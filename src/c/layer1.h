#pragma once
#include <pebble.h>

// Layer 1 is split in two so we can avoid redrawing static chrome every minute:
//
//   layer1_bg_update     — black background + minute progress band.
//                          Marked dirty once per minute.
//   layer1_chrome_update — tick marks + hour numbers (transparent areas).
//                          Marked dirty only when the displayed hour changes.
void layer1_bg_update(Layer *layer, GContext *ctx, int minute);
void layer1_chrome_update(Layer *layer, GContext *ctx, int current_hour12);
