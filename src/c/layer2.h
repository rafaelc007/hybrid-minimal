#pragma once
#include <pebble.h>
#include "widgets.h"

// Layer 2: stacked widgets.
//
// `slot_order` is a 5-element array of WidgetIds. Indices 0–1 map to the
// outer region (top → bottom); indices 2–4 map to the inner region
// (top → middle → bottom).
void layer2_update(Layer *layer, GContext *ctx,
                   const uint8_t *slot_order, const WidgetState *state);

void layer2_inner_update(Layer *layer, GContext *ctx,
                         const uint8_t *slot_order, const WidgetState *state);
