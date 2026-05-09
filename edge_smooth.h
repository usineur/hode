/*
 * Heart of Darkness engine rewrite
 * MLAA edge smoothing for upscaled sprites
 */

#ifndef EDGE_SMOOTH_H__
#define EDGE_SMOOTH_H__

#include "intern.h"

// Morphological Anti-Aliasing (MLAA)
// Applied as post-processing after xBRZ upscale
void mlaa_smooth(uint32_t *pixels, int w, int h);

#endif // EDGE_SMOOTH_H__
