/*
 * Heart of Darkness engine rewrite
 * MLAA (Morphological Anti-Aliasing) edge smoothing
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "edge_smooth.h"

static inline int pixelDiff(uint32_t a, uint32_t b) {
	const int dr = (int)((a >> 16) & 0xFF) - (int)((b >> 16) & 0xFF);
	const int dg = (int)((a >> 8) & 0xFF) - (int)((b >> 8) & 0xFF);
	const int db = (int)(a & 0xFF) - (int)(b & 0xFF);
	return abs(dr) + abs(dg) + abs(db);
}

static inline uint32_t lerpPixel(uint32_t a, uint32_t b, int t256) {
	const int inv = 256 - t256;
	const int r = (((a >> 16) & 0xFF) * inv + ((b >> 16) & 0xFF) * t256) >> 8;
	const int g = (((a >> 8) & 0xFF) * inv + ((b >> 8) & 0xFF) * t256) >> 8;
	const int bl = ((a & 0xFF) * inv + (b & 0xFF) * t256) >> 8;
	const int al = (((a >> 24) & 0xFF) * inv + ((b >> 24) & 0xFF) * t256) >> 8;
	return (al << 24) | (r << 16) | (g << 8) | bl;
}

void mlaa_smooth(uint32_t *pixels, int w, int h) {
	if (w < 3 || h < 3) return;

	// Edge threshold: pixels with difference above this are "edges"
	static const int kEdgeThreshold = 48;

	// Temporary buffer for the smoothed result
	uint32_t *temp = (uint32_t *)malloc(w * h * sizeof(uint32_t));
	if (!temp) return;
	memcpy(temp, pixels, w * h * sizeof(uint32_t));

	// Horizontal edge-aware smoothing
	for (int y = 1; y < h - 1; ++y) {
		for (int x = 1; x < w - 1; ++x) {
			const uint32_t C = pixels[y * w + x];
			const uint32_t L = pixels[y * w + x - 1];
			const uint32_t R = pixels[y * w + x + 1];
			const uint32_t U = pixels[(y - 1) * w + x];
			const uint32_t D = pixels[(y + 1) * w + x];

			// Skip transparent pixels
			if ((C >> 24) == 0) continue;

			// Detect horizontal edge (strong vertical color change)
			const int diffUD = pixelDiff(U, D);
			if (diffUD > kEdgeThreshold) {
				// We're at a horizontal edge. Blend with neighbors
				// based on how similar they are to reduce staircase
				const int diffLR = pixelDiff(L, R);
				if (diffLR < kEdgeThreshold) {
					// L and R are similar, we can smooth
					temp[y * w + x] = lerpPixel(C,
						lerpPixel(L, R, 128), 32);
				}
			}

			// Detect vertical edge (strong horizontal color change)
			const int diffLR2 = pixelDiff(L, R);
			if (diffLR2 > kEdgeThreshold) {
				const int diffUD2 = pixelDiff(U, D);
				if (diffUD2 < kEdgeThreshold) {
					temp[y * w + x] = lerpPixel(C,
						lerpPixel(U, D, 128), 32);
				}
			}
		}
	}

	// Diagonal smoothing pass
	for (int y = 1; y < h - 1; ++y) {
		for (int x = 1; x < w - 1; ++x) {
			const uint32_t C = temp[y * w + x];
			if ((C >> 24) == 0) continue;

			const uint32_t UL = temp[(y-1) * w + x-1];
			const uint32_t UR = temp[(y-1) * w + x+1];
			const uint32_t DL = temp[(y+1) * w + x-1];
			const uint32_t DR = temp[(y+1) * w + x+1];

			// Check for diagonal staircase pattern
			const int d1 = pixelDiff(UL, DR); // main diagonal
			const int d2 = pixelDiff(UR, DL); // anti diagonal

			if (d1 > kEdgeThreshold && d2 < kEdgeThreshold / 2) {
				// Anti-diagonal edge: smooth along it
				pixels[y * w + x] = lerpPixel(C,
					lerpPixel(UR, DL, 128), 24);
			} else if (d2 > kEdgeThreshold && d1 < kEdgeThreshold / 2) {
				// Main diagonal edge: smooth along it
				pixels[y * w + x] = lerpPixel(C,
					lerpPixel(UL, DR, 128), 24);
			} else {
				pixels[y * w + x] = C;
			}
		}
	}

	free(temp);
}
