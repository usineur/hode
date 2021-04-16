
// https://forums.libretro.com/t/xbr-algorithm-tutorial/123
// https://git.ffmpeg.org/gitweb/ffmpeg.git/blob_plain/HEAD:/libavfilter/vf_xbr.c

#include "scaler.h"

static uint8_t _yuv[256][3];
static int16_t _diffYuv[256][256];

template <int M, int S>
static uint32_t interpolate(uint32_t a, uint32_t b) {
	static const uint32_t kMask = 0xFF00FF;

	const uint32_t a_rb =  a       & kMask;
	const uint32_t a_ag = (a >> 8) & kMask;

	const uint32_t b_rb =  b       & kMask;
	const uint32_t b_ag = (b >> 8) & kMask;

	const uint32_t m1 = ((((b_rb - a_rb) * M) >> S) + a_rb) & kMask;
	const uint32_t m2 = ((((b_ag - a_ag) * M) >> S) + a_ag) & kMask;

	return m1 | (m2 << 8);
}

#define COLOR(x) palette[x]

#define ALPHA_BLEND_32_W(a, b)  interpolate<1,3>(a, b)
#define ALPHA_BLEND_64_W(a, b)  interpolate<1,2>(a, b)
#define ALPHA_BLEND_128_W(a, b) interpolate<1,1>(a, b)
#define ALPHA_BLEND_192_W(a, b) interpolate<3,2>(a, b)
#define ALPHA_BLEND_224_W(a, b) interpolate<7,3>(a, b)

#define df(A, B) _diffYuv[A][B]
#define eq(A, B) (df(A, B) < 155)

#define filt2b(PE, PI, PH, PF, PG, PC, PD, PB, PA, G5, C4, G0, D0, C1, B1, F4, I4, H5, I5, A0, A1, N0, N1, N2, N3) do { \
  if (COLOR(PE) != COLOR(PH) && COLOR(PE) != COLOR(PF)) { \
    const unsigned e = df(PE,PC) + df(PE,PG) + df(PI,H5) + df(PI,F4) + (df(PH,PF)<<2); \
    const unsigned i = df(PH,PD) + df(PH,I5) + df(PF,I4) + df(PF,PB) + (df(PE,PI)<<2); \
    if (e <= i) { \
      const unsigned px = df(PE,PF) <= df(PE,PH) ? COLOR(PF) : COLOR(PH); \
      if (e < i && ( (!eq(PF,PB) && !eq(PH,PD)) || (eq(PE,PI) && (!eq(PF,I4) && !eq(PH,I5))) || eq(PE,PG) || eq(PE,PC)) ) { \
        const unsigned ke = df(PF,PG); \
        const unsigned ki = df(PH,PC); \
        const bool left   = ke<<1 <= ki && COLOR(PE) != COLOR(PG) && COLOR(PD) != COLOR(PG); \
        const bool up     = ke >= ki<<1 && COLOR(PE) != COLOR(PC) && COLOR(PB) != COLOR(PC); \
        if (left && up) { \
          E[N3] = ALPHA_BLEND_224_W(E[N3], px); \
          E[N2] = ALPHA_BLEND_64_W( E[N2], px); \
          E[N1] = E[N2]; \
        } else if (left) { \
          E[N3] = ALPHA_BLEND_192_W(E[N3], px); \
          E[N2] = ALPHA_BLEND_64_W( E[N2], px); \
        } else if (up) { \
          E[N3] = ALPHA_BLEND_192_W(E[N3], px); \
          E[N1] = ALPHA_BLEND_64_W( E[N1], px); \
        } else { \
          E[N3] = ALPHA_BLEND_128_W(E[N3], px); \
        } \
      } else { \
        E[N3] = ALPHA_BLEND_128_W(E[N3], px); \
      } \
    } \
  } \
} while (0)

#define filt3a(PE, PI, PH, PF, PG, PC, PD, PB, PA, G5, C4, G0, D0, C1, B1, F4, I4, H5, I5, A0, A1, N0, N1, N2, N3, N4, N5, N6, N7, N8) do { \
  if (COLOR(PE) != COLOR(PH) && COLOR(PE) != COLOR(PF)) {                                                                     \
    const unsigned e = df(PE,PC) + df(PE,PG) + df(PI,H5) + df(PI,F4) + (df(PH,PF)<<2); \
    const unsigned i = df(PH,PD) + df(PH,I5) + df(PF,I4) + df(PF,PB) + (df(PE,PI)<<2); \
    if (e <= i) { \
      const unsigned px = df(PE,PF) <= df(PE,PH) ? COLOR(PF) : COLOR(PH); \
      if (e < i && ( (!eq(PF,PB) && !eq(PF,PC)) || (!eq(PH,PD) && !eq(PH,PG)) || ((eq(PE,PI) && (!eq(PF,F4) && !eq(PF,I4))) || (!eq(PH,H5) && !eq(PH,I5))) || eq(PE,PG) || eq(PE,PC)) ) { \
        const unsigned ke = df(PF,PG); \
        const unsigned ki = df(PH,PC); \
        const bool left   = ke<<1 <= ki && COLOR(PE) != COLOR(PG) && COLOR(PD) != COLOR(PG); \
        const bool up     = ke >= ki<<1 && COLOR(PE) != COLOR(PC) && COLOR(PB) != COLOR(PC); \
        if (left && up) { \
          E[N7] = ALPHA_BLEND_192_W(E[N7], px); \
          E[N6] = ALPHA_BLEND_64_W( E[N6], px); \
          E[N5] = E[N7]; \
          E[N2] = E[N6]; \
          E[N8] = px; \
        } else if (left) { \
          E[N7] = ALPHA_BLEND_192_W(E[N7], px); \
          E[N5] = ALPHA_BLEND_64_W( E[N5], px); \
          E[N6] = ALPHA_BLEND_64_W( E[N6], px); \
          E[N8] = px; \
        } else if (up) { \
          E[N5] = ALPHA_BLEND_192_W(E[N5], px); \
          E[N7] = ALPHA_BLEND_64_W( E[N7], px); \
          E[N2] = ALPHA_BLEND_64_W( E[N2], px); \
          E[N8] = px; \
        } else { \
          E[N8] = ALPHA_BLEND_224_W(E[N8], px); \
          E[N5] = ALPHA_BLEND_32_W( E[N5], px); \
          E[N7] = ALPHA_BLEND_32_W( E[N7], px); \
        } \
      } else { \
        E[N8] = ALPHA_BLEND_128_W(E[N8], px); \
      } \
    } \
  } \
} while (0)

#define filt4b(PE, PI, PH, PF, PG, PC, PD, PB, PA, G5, C4, G0, D0, C1, B1, F4, I4, H5, I5, A0, A1, N15, N14, N11, N3, N7, N10, N13, N12, N9, N6, N2, N1, N5, N8, N4, N0) do { \
  if (COLOR(PE) != COLOR(PH) && COLOR(PE) != COLOR(PF)) { \
    const unsigned e = df(PE,PC) + df(PE,PG) + df(PI,H5) + df(PI,F4) + (df(PH,PF)<<2); \
    const unsigned i = df(PH,PD) + df(PH,I5) + df(PF,I4) + df(PF,PB) + (df(PE,PI)<<2); \
    if (e <= i) { \
      const unsigned px = df(PE,PF) <= df(PE,PH) ? COLOR(PF) : COLOR(PH); \
      if (e < i && ( (!eq(PF,PB) && !eq(PH,PD)) || (eq(PE,PI) && (!eq(PF,I4) && !eq(PH,I5))) || eq(PE,PG) || eq(PE,PC)) ) { \
        const unsigned ke = df(PF,PG); \
        const unsigned ki = df(PH,PC); \
        const bool left   = ke<<1 <= ki && COLOR(PE) != COLOR(PG) && COLOR(PD) != COLOR(PG); \
        const bool up     = ke >= ki<<1 && COLOR(PE) != COLOR(PC) && COLOR(PB) != COLOR(PC); \
        if (left && up) { \
          E[N13] = ALPHA_BLEND_192_W(E[N13], px); \
          E[N12] = ALPHA_BLEND_64_W( E[N12], px); \
          E[N15] = E[N14] = E[N11] = px; \
          E[N10] = E[N3]  = E[N12]; \
          E[N7]  = E[N13]; \
        } else if (left) { \
          E[N11] = ALPHA_BLEND_192_W(E[N11], px); \
          E[N13] = ALPHA_BLEND_192_W(E[N13], px); \
          E[N10] = ALPHA_BLEND_64_W( E[N10], px); \
          E[N12] = ALPHA_BLEND_64_W( E[N12], px); \
          E[N14] = px; \
          E[N15] = px; \
        } else if (up) { \
          E[N14] = ALPHA_BLEND_192_W(E[N14], px); \
          E[N7 ] = ALPHA_BLEND_192_W(E[N7 ], px); \
          E[N10] = ALPHA_BLEND_64_W( E[N10], px); \
          E[N3 ] = ALPHA_BLEND_64_W( E[N3 ], px); \
          E[N11] = px; \
          E[N15] = px; \
        } else { \
          E[N11] = ALPHA_BLEND_128_W(E[N11], px); \
          E[N14] = ALPHA_BLEND_128_W(E[N14], px); \
          E[N15] = px; \
        } \
      } else { \
        E[N15] = ALPHA_BLEND_128_W(E[N15], px); \
      } \
    } \
  } \
} while (0)

template <int N>
static void scale_xbr(uint32_t *dst, int dstPitch, const uint8_t *src, int srcPitch, int w, int h, const uint32_t *palette) {
	const int nl = dstPitch;
	const int nl1 = dstPitch * 2;
	const int nl2 = dstPitch * 3;

	for (int y = 0; y < h; ++y) {

		uint32_t *E = dst + y * dstPitch * N;

		const uint8_t *sa2 = src + y * srcPitch - 2;
		const uint8_t *sa1 = sa2 - srcPitch;
		const uint8_t *sa0 = sa1 - srcPitch;
		const uint8_t *sa3 = sa2 + srcPitch;
		const uint8_t *sa4 = sa3 + srcPitch;

		if (y <= 1) {
			sa0 = sa1;
			if (y == 0) {
				sa0 = sa1 = sa2;
			}
		}
		if (y >= h - 2) {
			sa4 = sa3;
			if (y == h - 1) {
				sa4 = sa3 = sa2;
			}
		}

		for (int x = 0; x < w; ++x) {

			//    A1 B1 C1
			// A0 PA PB PC C4
			// D0 PD PE PF F4
			// G0 PG PH PI I4
			//    G5 H5 I5

			const uint32_t B1 = sa0[2];
			const uint32_t PB = sa1[2];
			const uint32_t PE = sa2[2];
			const uint32_t PH = sa3[2];
			const uint32_t H5 = sa4[2];

			const int pprev = 2 - (x > 0);
			const uint32_t A1 = sa0[pprev];
			const uint32_t PA = sa1[pprev];
			const uint32_t PD = sa2[pprev];
			const uint32_t PG = sa3[pprev];
			const uint32_t G5 = sa4[pprev];

			const int pprev2 = pprev - (x > 1);
			const uint32_t A0 = sa1[pprev2];
			const uint32_t D0 = sa2[pprev2];
			const uint32_t G0 = sa3[pprev2];

			const int pnext = 3 - (x == w - 1);
			const uint32_t C1 = sa0[pnext];
			const uint32_t PC = sa1[pnext];
			const uint32_t PF = sa2[pnext];
			const uint32_t PI = sa3[pnext];
			const uint32_t I5 = sa4[pnext];

			const int pnext2 = pnext + 1 - (x >= w - 2);
			const uint32_t C4 = sa1[pnext2];
			const uint32_t F4 = sa2[pnext2];
			const uint32_t I4 = sa3[pnext2];

			for (int j = 0; j < N; ++j) {
				for (int i = 0; i < N; ++i) {
					*(E + j * dstPitch + i) = COLOR(PE);
				}
			}


			if (N == 2) {
				filt2b(PE, PI, PH, PF, PG, PC, PD, PB, PA, G5, C4, G0, D0, C1, B1, F4, I4, H5, I5, A0, A1, 0, 1, nl, nl+1);
				filt2b(PE, PC, PF, PB, PI, PA, PH, PD, PG, I4, A1, I5, H5, A0, D0, B1, C1, F4, C4, G5, G0, nl, 0, nl+1, 1);
				filt2b(PE, PA, PB, PD, PC, PG, PF, PH, PI, C1, G0, C4, F4, G5, H5, D0, A0, B1, A1, I4, I5, nl+1, nl, 1, 0);
				filt2b(PE, PG, PD, PH, PA, PI, PB, PF, PC, A0, I5, A1, B1, I4, F4, H5, G5, D0, G0, C1, C4, 1, nl+1, 0, nl);
			} else if (N == 3) {
				filt3a(PE, PI, PH, PF, PG, PC, PD, PB, PA, G5, C4, G0, D0, C1, B1, F4, I4, H5, I5, A0, A1, 0, 1, 2, nl, nl+1, nl+2, nl1, nl1+1, nl1+2);
				filt3a(PE, PC, PF, PB, PI, PA, PH, PD, PG, I4, A1, I5, H5, A0, D0, B1, C1, F4, C4, G5, G0, nl1, nl, 0, nl1+1, nl+1, 1, nl1+2, nl+2, 2);
				filt3a(PE, PA, PB, PD, PC, PG, PF, PH, PI, C1, G0, C4, F4, G5, H5, D0, A0, B1, A1, I4, I5, nl1+2, nl1+1, nl1, nl+2, nl+1, nl, 2, 1, 0);
				filt3a(PE, PG, PD, PH, PA, PI, PB, PF, PC, A0, I5, A1, B1, I4, F4, H5, G5, D0, G0, C1, C4, 2, nl+2, nl1+2, 1, nl+1, nl1+1, 0, nl, nl1);
			} else if (N == 4) {
				filt4b(PE, PI, PH, PF, PG, PC, PD, PB, PA, G5, C4, G0, D0, C1, B1, F4, I4, H5, I5, A0, A1, nl2+3, nl2+2, nl1+3, 3, nl+3, nl1+2, nl2+1, nl2, nl1+1, nl+2, 2, 1, nl+1, nl1, nl, 0);
				filt4b(PE, PC, PF, PB, PI, PA, PH, PD, PG, I4, A1, I5, H5, A0, D0, B1, C1, F4, C4, G5, G0, 3, nl+3, 2, 0, 1, nl+2, nl1+3, nl2+3, nl1+2, nl+1, nl, nl1, nl1+1, nl2+2, nl2+1, nl2);
				filt4b(PE, PA, PB, PD, PC, PG, PF, PH, PI, C1, G0, C4, F4, G5, H5, D0, A0, B1, A1, I4, I5, 0, 1, nl, nl2, nl1, nl+1, 2, 3, nl+2, nl1+1, nl2+1, nl2+2, nl1+2, nl+3, nl1+3, nl2+3);
				filt4b(PE, PG, PD, PH, PA, PI, PB, PF, PC, A0, I5, A1, B1, I4, F4, H5, G5, D0, G0, C1, C4, nl2, nl1, nl2+1, nl2+3, nl2+2, nl1+1, nl, 0, nl+1, nl1+2, nl1+3, nl+3, nl+2, 1, 2, 3);
			}

			++sa0;
			++sa1;
			++sa2;
			++sa3;
			++sa4;

			E += N;
		}
	}
}

static void palette_xbr(const uint32_t *palette) {
	for (int i = 0; i < 256; ++i) {
		const int r = (palette[i] >> 16) & 255;
		const int g = (palette[i] >>  8) & 255;
		const int b =  palette[i]        & 255;
		_yuv[i][0] = ( 299 * r + 587 * g + 114 * b) / 1000;
		_yuv[i][1] = (-169 * r - 331 * g + 500 * b) / 1000 + 128;
		_yuv[i][2] = ( 500 * r - 419 * g -  81 * b) / 1000 + 128;
	}
	for (int j = 0; j < 256; ++j) {
		for (int i = 0; i < j; ++i) {
			if (i != j) {
				const int dy = _yuv[i][0] - _yuv[j][0];
				const int du = _yuv[i][1] - _yuv[j][1];
				const int dv = _yuv[i][2] - _yuv[j][2];
				_diffYuv[j][i] = ABS(dy) + ABS(du) + ABS(dv);
			}
		}
	}
	for (int j = 0; j < 256; ++j) {
		for (int i = j; i < 256; ++i) {
			_diffYuv[j][i] = _diffYuv[i][j];
		}
	}
}

const Scaler scaler_xbr = {
	"xbr",
	2, 4,
	palette_xbr,
	{ scale_xbr<2>, scale_xbr<3>, scale_xbr<4> }
};
