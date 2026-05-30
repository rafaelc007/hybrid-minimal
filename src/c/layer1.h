#pragma once
#include <pebble.h>

// Layer 1 is split in two so we can avoid redrawing static chrome every minute:
//
//   layer1_bg_update     — black background + minute progress band.
//                          Marked dirty once per minute.
//   layer1_chrome_update — tick marks + hour numbers (transparent areas).
//                          Marked dirty only when the displayed hour changes.
//
// `rounded` selects between a rounded-square perimeter (true) and a sharp
// rectangle (false) on rectangular displays. Ignored on round displays.
void layer1_bg_update(Layer *layer, GContext *ctx, int minute,
                      GColor progress_color, bool rounded);
void layer1_chrome_update(Layer *layer, GContext *ctx, int current_hour12,
                          bool rounded);
