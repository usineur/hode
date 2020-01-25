
#ifndef SCREENSHOT_H__
#define SCREENSHOT_H__

#include <stdio.h>
#include <stdint.h>

void saveBMP(FILE *fp, const uint8_t *bits, const uint8_t *pal, int w, int h);

#endif
