#include "stdafx.h"		// default includes (precompiled headers)
#include "basetsd.h"
#define MAXUINT16   ((UINT16)~((UINT16)0))

#include "simfw.h"
#include "color.h"
#include "float.h"

#define SIMFW_TEST_TITLE	"ZELLOSKOP"
#define SIMFW_FONTSIZE			12
#define SIMFW_TEST_DSP_HEIGHT	(	1024		/	1)
#define SIMFW_TEST_DSP_WIDTH	(	1024	/	1)
#define SIMFW_TEST_DSP_ZOOM		(8)

#define SIMFW_TEST_DSP_WINDOW_FLAGS 0
//#define SIMFW_TEST_DSP_WINDOW_FLAGS SDL_WINDOW_FULLSCREEN_DESKTOP

int log2t(double d) {
	int result;
	frexp(d, &result);
	return result;
}

void
TWOD_DIFFUSION(void) {

#define CTP	double		// cell type
#define CMX (DBL_MAX / 256.0)		// cell max
#define CMN (DBL_MIN)		// cell min
#define CIP (1.0)		// cell input
	//#define CIT (CMN * 256.0)// cell init
#define CIT 0//(CIP / 256.0 / 256.0)		// cell init

	//#define CTP	UINT16		// cell type
	//#define CMX MAXUINT16	// cell max
	//#define CMN 0			// cell min
	//#define CIT (1)		// cell init

	/* Init */
	//
	srand((unsigned)time(NULL));
	//
	SIMFW simfw = { 0 };
	SIMFW_Init(&simfw, SIMFW_TEST_TITLE, SIMFW_TEST_DSP_HEIGHT, SIMFW_TEST_DSP_WIDTH, SIMFW_TEST_DSP_WINDOW_FLAGS * 1, -4, -4);
	//
	printf("window  height %d  width %d\n", simfw.window_height, simfw.window_width);
	printf("sim  height %d  width %d\n", simfw.sim_height, simfw.sim_width);
	pixel_effect_moving_gradient(&simfw);
	//
	double zoom = 1.0;
	int res = 1;
	int ar = 3; /* auto range */
	int cm = 1; /* color mode */
	int crct = 0; /* color count */
	int pitch = 0;
	int speed = 16;

	double dwnr = 8; // draw - number of ranges
	uint64_t tm = 0; /* time */

					 /* */
	const int w = simfw.sim_width;
	const int h = simfw.sim_height;
	const int sz = w * h;
	CTP* ans = malloc(w * h * sizeof(*ans));	// absolute numbers
	CTP* bans = malloc(w * h * sizeof(*bans));	// buffer for absolute numbers
	for (int i = 0; i < sz; ++i)
		ans[i] = CIT;
	const float rh = (float)simfw.sim_height / simfw.window_height;		// relative height
	const float rw = (float)simfw.sim_width / simfw.window_width;		// relative width
	int ip = 1;		//input enabled/disabled flag

	//ans[h / 2 * w + w / 2] = CMX;


	struct { float y, x, i; } ips[2];
	ips[0].y = h / 5, ips[0].x = w / 5, ips[0].i = 0.01;
	ips[1].y = h / 3, ips[1].x = w / 2, ips[1].i = 0.07;



	/* SDL loop */
	int mouse_y = h / 2, mouse_x = w / 2;
	while (1) {
		/* SDL event handling */
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			SDL_Keymod kbms = SDL_GetModState(); // keyboard mod state
			int ctrl = kbms & KMOD_CTRL;
			int shft = kbms & KMOD_SHIFT;
			if (e.type == SDL_QUIT)						break;
			else if (e.type == SDL_MOUSEMOTION)			mouse_y = e.motion.y * rh + 0.5, mouse_x = e.motion.x * rw + 0.5;
			else if (e.type == SDL_MOUSEBUTTONDOWN)		ip = !ip;
			else if (e.type == SDL_WINDOWEVENT_RESIZED);
			else if (e.type == SDL_MOUSEWHEEL) {
				if (shft)
					dwnr *= pow(1.0001, e.wheel.y);
				else
					dwnr *= pow(1.01, e.wheel.y);
				SIMFW_SetFlushMsg(&simfw, "draw nr. ranges %.4e", dwnr);
			}
			else if (e.type == SDL_KEYDOWN) {
				/* */
				int os = speed;		// old speed
				int keysym = e.key.keysym.sym;
				switch (keysym) {
				case SDLK_F4:			goto behind_sdl_loop;
				case SDLK_END:			speed = 32;			break;
				case SDLK_HOME:			speed = 1;			break;
				case SDLK_PAGEUP:		if (ctrl)	speed += 1;		else	speed *= 2;		break;
				case SDLK_PAGEDOWN:		if (ctrl)	speed -= 1;		else	speed /= 2;		break;
				case SDLK_SPACE:		for (int i = 0; i < w * h; ++i)		ans[i] = CIT;	break;
				}
				if (os != speed)
					SIMFW_SetFlushMsg(&simfw, "speed %d", speed);
			}
		}

		/* Draw */
		UINT32* px = simfw.sim_canvas;
		CTP mn, mx;
		/* get min/max */
		mn = CMX, mx = CMN;
		for (int i = 0; i < w * h; ++i) {
			if (ans[i] < mn && ans[i] > CMN) mn = ans[i];
			if (ans[i] > mx) mx = ans[i];
		}
		//mn = 1.0, mx = 256.0;
		/* draw */
		// log2t to get int log2
		double lgmn = log2(mn + 1.0);
		double sl = 1.0 / (log2(mx + 1.0) - lgmn);	// scale

		for (int i = 0; i < w * h; ++i) {
			unsigned char bw;
			bw = 255 * (dwnr - dwnr * sl * (log2(ans[i] + 1.0) - lgmn));
			bw = 255 - bw;
			*px = bw << 16 | bw << 8 | bw;
			++px;
		}
		for (int rt = 0; rt < speed; ++rt) {
			/* calc next */
			*px = simfw.sim_canvas;
			static int gen = 0;
			if (ip) {
				gen++;
				for (int ipi = 0; ipi < sizeof(ips) - 1; ++ipi) {
					int ap = (int)ips[ipi].y * w + (int)ips[ipi].x;
					if (ap < w || ap >= h * w - w)
						continue;

					ans[ap] = ips[ipi].i;

					if (gen % 10 != 0)
						continue;

					ips[ipi].y += 4 * (ans[ap + w] - ans[ap - w]);
					ips[ipi].x += 4 * (ans[ap + 1] - ans[ap - 1]);
					printf("#%2d  %5d  %.3f  %.4f %.4f\n", ipi, ap, ips[ipi].i, ips[ipi].y, ips[ipi].x);

					simfw.sim_canvas[ap] = 0xFF0000;

					ap = (int)ips[ipi].y * w + (int)ips[ipi].x;
					if (ap < w || ap >= h * w - w)
						continue;


					//ans[mouse_y * w + mouse_x] = CIP;
				}
			}
			// frame
			for (int ih = 0; ih < h; ++ih)
				for (int iw = 0; iw < w; ++iw) {
					int mrp = 0; // mirror cell position
					if (ih == 0)		mrp = (h - 2) * w;
					else if (ih == h - 1)	mrp = w;
					else mrp = ih * w;
					if (iw == 0)		mrp += w - 2;
					else if (iw == w - 1)	mrp += 1;
					else mrp += iw;
					ans[ih * w + iw] = ans[mrp];

					//if (ih == 0 || ih == h - 1 || iw == 0 || iw == w - 1)
					//	ans[ih * w + iw] = 0.0;
					//if (iw == 0)
					//	ans[ih * w + iw] = 1.0;
				}
			for (int i = 0; i < sz; ++i) {
				CTP ds = 2;	// diffusion speed - 1 = max, 2 = half ...
				if (i < sz / 2)
					ds = 8;
				if (i > w && i < sz - w && i % w > 0) { // && i%h < w) {
					// concentration = diffusion rate
					//ds = 1.0 / 1.0;
					//bans[i] =
					//	// remaining
					//	ans[i]
					//	- ans[i] * ds / 4.0 * (1.0 - ans[i - w])
					//	- ans[i] * ds / 4.0 * (1.0 - ans[i + 1])
					//	- ans[i] * ds / 4.0 * (1.0 - ans[i + w])
					//	- ans[i] * ds / 4.0 * (1.0 - ans[i - 1])
					//	// incoming	 * ds 
					//	+ ans[i - w] * ds / 4.0 * (1.0 - ans[i])
					//	+ ans[i + 1] * ds / 4.0 * (1.0 - ans[i])
					//	+ ans[i + w] * ds / 4.0 * (1.0 - ans[i])
					//	+ ans[i - 1] * ds / 4.0 * (1.0 - ans[i]);
					// ds = diffusion rate
					bans[i] =
						// remaining
						ans[i]
						- ans[i] / ds / 4.0
						- ans[i] / ds / 4.0
						- ans[i] / ds / 4.0
						- ans[i] / ds / 4.0
						// incoming	 * ds 
						+ ans[i - w] / 4 / ds
						+ ans[i + 1] / 4 / ds
						+ ans[i + w] / 4 / ds
						+ ans[i - 1] / 4 / ds;
				}
				else
					;// bans[i] = ans[i] = CMN;
			}

			//}
			/* swap current ans with buffered ones */
			void* sp;
			sp = ans, ans = bans, bans = sp;
		}

		/* Update status and screen */
		int mp = mouse_y * w + mouse_x;
		SIMFW_SetStatusText(&simfw, "SIMFW TEST   fps %.2f\nmn %.2e/%.2e  mx %.2e/%.2e  mp %.3e",
			1.0 / simfw.avg_duration,
			(double)mn, log(mn) / log(2), (double)mx, log(mx) / log(2), ans[mp]);
		//SIMFW_SetStatusText(&simfw,
		//	"SIMFW TEST   fps %.2f  sd %.2f\nmn %.2e/%.2e  mx %.2e/%.2e    mouse %.2e/%.2e\n%.4f",
		//	1.0 / simfw.avg_duration,
		//	1.0 * speed,
		//	mn, log(mn), mx, log(mx), ans[mouse_x], log(ans[mouse_x]),
		//	(log(ans[mouse_x]) - log(mn)) / (log(mx) - log(mn)));
		SIMFW_UpdateDisplay(&simfw);

	}
behind_sdl_loop:;
	/* Cleanup */
	SIMFW_Quit(&simfw);
}

__inline
uint32_t YXE(uint32_t y, uint32_t x)
{
	//return (y & 0xF) * 0xF + 0xF + (x & 0xF) + 1;

	return y * 1024 + x;

	// MortonEncode
	//return _pdep_u32(y, 0xAAAAAAAA) | _pdep_u32(x, 0x55555555);
}

__inline
void MortonDecode2(uint32_t code, uint32_t* outX, uint32_t* outY)
{
	*outX = _pext_u32(code, 0x55555555);
	*outY = _pext_u32(code, 0xAAAAAAAA);
}


#define MTTP uint32_t	// mantissa type
#define MTSZ 32			// mantissa size in bits
#define MTBM (MTSZ - 1)	// mantisse bis mask to calculate  x % MTSZ = x & MTBM
#define MTLZCF __lzcnt	// mantissa leading zero count function
#define EXTP uint16_t	// exponent type
#define EXSZ 16			// exponent size in bits
typedef struct sint {	// scaled int
	MTTP mt;			// mantissa
	EXTP ex;			// exponent
	uint8_t ds;			// diffusion speed
} sint;
#define CTP	sint		// cell type
void box_sint(CTP* ans, int h, int w) {
	int bh = 1 << (rand() % 5 + 3);
	int bw = 1 << (rand() % 5 + 3);
	int by = rand() % h;
	int bx = rand() % w;
	int ds = rand() % 7+1;
	for (int ih = max(1, by - bh / 2); ih < min(h - 1, by + bh / 2); ++ih)
		for (int iw = max(1, bx - bw / 2); iw < min(w - 1, bx + bw / 2); ++iw) {
			///int p = 2 * (ih * w + iw);
			ans[YXE(ih,iw)*2+1].ds = ans[YXE(ih,iw)*2].ds = ds;
		}
}
/*
h	height
w	width
b	border-width
a	absorbtion-coefficient
*/
void box_outline_sint(CTP* ans, int h, int w, int b, int ac) {
	if (b == 0)
		b = max(1, (w + h) >> (rand() % 6 + 6));
	int bh = (w + h) >> (rand() % 6 + 2);
	int bw = (w + h) >> (rand() % 6 + 2);
	int by = rand() % h + 1;
	int bx = rand() % w + 1;
	if (ac == 0)
		ac = rand() % 7+1;
	printf("box-outline  bw %d  h %d  w %d  y %d  x %d  ac %d\n", b, bh, bw, by, bx, ac);
	for (int ih = by; ih < min(h - 1, by + bh); ++ih) {
		for (int iw = bx; iw < min(w - 1, bx + bw); ++iw) {
			if ((ih < by + b) || (ih >= by + bh - b) || (iw < bx + b) || (iw >= bx + bw - b)) {
				///int p = 2 * (ih * w + iw);
				ans[YXE(ih,iw)*2+1].ds = ans[YXE(ih,iw)*2].ds = ac;
			}
		}
	}
}
void circle_sint(CTP* ans, int h, int w) {
	int cr = 1 << (rand() % 5 + 3);
	int cy = rand() % h;
	int cx = rand() % w;
	int ds = rand() % 7+1;
	for (int iy = max(1, cy - cr); iy < min(h - 1, cy + cr); ++iy)
		for (int ix = max(1, cx - cr); ix < min(w - 1, cx + cr); ++ix)
			if (sqrt(pow((double)(cy - iy), 2.0) + pow((double)(cx - ix), 2.0)) < (double)cr) {
				///int p = 2 * (iy * w + ix);
				ans[YXE(iy,ix)*2+1].ds = ans[YXE(iy,ix)*2].ds = ds;
			}
}
void box_sparse(CTP* ans, int h, int w) {
	int bh = 1 << (rand() % 5 + 3);
	int bw = 1 << (rand() % 5 + 3);
	int by = rand() % h;
	int bx = rand() % w;
	int dy = rand();
	for (int ih = max(1, by); ih < min(h - 1, by + bh); ++ih)
		for (int iw = max(1, bx); iw < min(w - 1, bx + bw); ++iw) {
			if (rand() <= dy) {
				///int p = 2 * (ih * w + iw);
				int ds = rand() % 7+1;
				ans[YXE(ih,iw)*2+1].ds = ans[YXE(ih,iw)*2].ds = ds;
			}
		}
}
void init_sint(CTP* ans, int h, int w) {
	///int sz = h * w;
	for (int ih = 0; ih < h; ++ih)
		for (int iw = 0; iw < w; ++iw) {
			uint32_t i = YXE(ih, iw) * 2;
			ans[i+1].ex = ans[i].ex = 0x0F00 - 0 - 2;			//correction for ds -ans[i].ds;
			ans[i+1].mt = ans[i].mt = 0x1 << (MTSZ - 1);
			ans[i+1].ds = ans[i].ds = 1;
	}
	// frame
	for (int ih = 0; ih < h; ++ih)
		for (int iw = 0; iw < w; ++iw)
			if (ih == 0 || ih == h - 1 || iw == 0 || iw == w - 1) {
				///int p = 2 * (ih * w + iw);
				uint32_t i = YXE(ih, iw) * 2;
				ans[i+1].ex = ans[i].ex = 0;
				ans[i+1].mt = ans[i].mt = 0x000;
				ans[i+1].ds = ans[i].ds = MTSZ - 1;
			}
}

void
TWOD_DIFFUSION_FP(void) {
	/* Init */
	//
	srand((unsigned)time(NULL));
	//
	SIMFW simfw = { 0 };
	SIMFW_Init(&simfw, SIMFW_TEST_TITLE, SIMFW_TEST_DSP_HEIGHT, SIMFW_TEST_DSP_WIDTH, SIMFW_TEST_DSP_WINDOW_FLAGS * 1, SIMFW_TEST_DSP_HEIGHT / SIMFW_TEST_DSP_ZOOM, SIMFW_TEST_DSP_WIDTH / SIMFW_TEST_DSP_ZOOM);
	//
	printf("window  height %d  width %d\n", simfw.window_height, simfw.window_width);
	printf("sim  height %d  width %d\n", simfw.sim_height, simfw.sim_width);
	pixel_effect_moving_gradient(&simfw);
	//
	double zoom = 1.0 / 1.0;
	int res = 1;
	int ar = 3; /* auto range */
	int cm = 1; /* color mode */
	int crct = 0; /* color count */
	int speed = 8;

	int ipen = 0; // input enabled
	int im = 1; // input mode

	double dwnr = 1; // draw - number of ranges
	uint64_t tm = 0; /* time */

	const int h = simfw.sim_height;										// sim_height
	const int w = simfw.sim_width;										// sim_width
	const float rh = (float)simfw.sim_height / simfw.window_height;		// relative height
	const float rw = (float)simfw.sim_width / simfw.window_width;		// relative width
	const int sz = w * h;												// sim_width * sim_height
	const int esz = YXE(h, w);
	CTP* oans = malloc(2 * esz * sizeof(*oans));							// absolute numbers
	CTP* ans = oans;
printf("sz %d  y %d  x %d\n", esz, h, w);
printf("sz pixel %d  line %d  all %d\n", sizeof(*oans) * 2, sizeof(*oans) * 2 * w, sizeof(*oans) * 2 * w * h);

	init_sint(ans, h, w);

	/* SDL loop */
	int mouse_y = h / 2, mouse_x = w / 2;
	int cn = 0;			// nr of calculation passes
	int ctrl = 0;
	int alt = 0;
	int shft = 0;
	int mspd = 0;	// mouse pressed
	int itfy = 1;	// input frequency
	while (1) {
		/* SDL event handling */
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			SDL_Keymod kbms = SDL_GetModState(); // keyboard mod state
			ctrl = kbms & KMOD_CTRL;
			shft = kbms & KMOD_SHIFT;
			alt = kbms & KMOD_ALT;
			if (e.type == SDL_QUIT)						break;
			else if (e.type == SDL_MOUSEMOTION)			mouse_y = e.motion.y * rh + 0.5, mouse_x = e.motion.x * rw + 0.5;
			else if (e.type == SDL_MOUSEBUTTONUP) {
				mspd = 0;
			}
			else if (e.type == SDL_MOUSEBUTTONDOWN) {
				mspd = 1;
				itfy = rand() % 100 + 1;
				printf("e.button.button %d\n", e.button.button);
				if (e.button.button != 1) {
					im = !im;
					printf("input mode %d\n", im);
				}
				else {
					ipen = !ipen;
					printf("input  %d\n", ipen);
				}

			}
			else if (e.type == SDL_WINDOWEVENT_RESIZED);
			else if (e.type == SDL_MOUSEWHEEL) {
				if (shft)
					dwnr *= pow(1.0001, e.wheel.y);
				else
					dwnr *= pow(1.25, e.wheel.y);
				SIMFW_SetFlushMsg(&simfw, "draw nr. ranges %.4e", dwnr);
			}
			else if (e.type == SDL_KEYDOWN) {
				/* */
				int os = speed;		// old speed
				int keysym = e.key.keysym.sym;
				switch (keysym) {
				case SDLK_F4:			goto behind_sdl_loop;
				case SDLK_END:			speed = min(64, speed * 2);			break;
				case SDLK_HOME:			speed = max(1, speed / 2);			break;
				case SDLK_PAGEUP:		if (ctrl)	speed += 1;		else	speed *= 2;		break;
				case SDLK_PAGEDOWN:		if (ctrl)	speed -= 1;		else	speed /= 2;		break;
				case SDLK_SPACE:		init_sint(ans, h, w);	break;
				case SDLK_b:
					if (shft)
						box_sint(ans, h, w, 0, 0);
					else
						box_outline_sint(ans, h, w, 0, 0);
					break;
				case SDLK_c:			circle_sint(ans, h, w);	break;
				case SDLK_n:			box_sparse(ans, h, w);	break;
				}
				if (os != speed)
					SIMFW_SetFlushMsg(&simfw, "speed %d", speed);
			}
		}

		/* Draw */
		EXTP exmx = 0, exmn = ~exmx;		// exponent max and min
		MTTP mtmx = 0, mtmn = ~mtmx;		// mantissa max and min
		double dmn = 0;
		double dmx = 0;


		if (1) {
			UINT32* px = simfw.sim_canvas;
			/* get min/max */
			for (int ih = 0; ih < h; ++ih)
				for (int iw = 0; iw < w; ++iw) {
					uint32_t i = YXE(ih, iw) * 2;
					if (ans[i].ex == exmn) {
						if (ans[i].mt < mtmn)
							mtmn = ans[i].mt;
					}
					else if (ans[i].ex > 0 && ans[i].ex < exmn) {
						exmn = ans[i].ex;
						mtmn = ans[i].mt;
					}
					if (ans[i].ex == exmx) {
						if (ans[i].mt > mtmx)
							mtmx = ans[i].mt;
					}
					else if (ans[i].ex > exmx) {
						exmx = ans[i].ex;
						mtmx = ans[i].mt;
					}
				}

			dmn = exmn + (1.0 / ((MTTP)1 << (MTSZ - 1)) * mtmn);
			dmx = exmx + (1.0 / ((MTTP)1 << (MTSZ - 1)) * mtmx);
			//dmn = exmn;
			//dmx = exmx;

			double sl;
			sl = 1.0 / (dmx - dmn);	// scale

			for (int ih = 0; ih < h; ++ih)
				for (int iw = 0; iw < w; ++iw) {
					uint32_t i = YXE(ih, iw) * 2;
					if (ans[i].ex == 0) {
						px++;
						continue;
					}
					double v;

					v = sl * (ans[i].ex - dmn + (1.0 / ((MTTP)1 << (MTSZ - 1)) * ans[i].mt));
					//v = 1.0 - sl * (ans[i].ex - exmn);

					// HLV
					*px = getColorEx(1.0 - v, 2, 1, 1, 1.0, 1.0, dwnr);
					//*px = getColorEx(1.0 - v, 2, 1, 0, 1.0, 1.0, dwnr);

					// OKLAB
					/*
					double l = 0.8 * (1.0 - v) * dwnr; l = l - floor(l);
					double c = 0.1; // (1.0 - v)* dwnr; c = c - floor(c); c *= 1.0 / 3.0;
					*px = oklab_lch_to_srgb(l, c, v);
					*/
					//*px = oklab_mix_two_rgb_colors(1.0, 0.0, 0.0, 0.0, 0.0, 1.0, v);
					//*px = oklab_mix_two_rgb_colors(1.0, 1.0, 0.0, 0.0, 1.0, 1.0, v);

					// GRAYSCALE
					v = 1.0 - v;
					v = v * dwnr;
					v = v - floor(v);
					uint32_t gs = v * 255.0 + 0.5;
					*px = gs << 16 | gs << 8 | gs;


					++px;
				}
		}

		if(1)
		for (int rt = 0; rt < speed; ++rt) {
			cn++;
			//mp = h / 2 * w + w / 2;
			// INPUT at mouse pos
			///int mp = 2 * (mouse_y * w + mouse_x);
			int mp = YXE(min(h - 1, mouse_y), min(w - 1, mouse_x))*2;
			if (mspd && shft) {
				ipen = 0;
				if (cn % itfy == 0) {
					printf("Input  fy %d  cn %d\n", itfy, cn);
					ans[mp].mt = 0x1 << (MTSZ - 1);
					ans[mp].ex = 0xA000;
				}
			}
			if (ipen) {
				if (ans[mp].ex != 0 && ans[mp].ex < 0xF << (EXSZ - 4)) {
					if (im == 1) {
						if (ctrl || alt) {
							ans[mp].mt = 0x1 << (MTSZ - 1);
							ans[mp].ex = 0xA000;
							if (alt)
								ipen = 0;
						}
						else if (shft) {
							ans[mp].ex += 1;
						}
						else
							ans[mp].ex += 0xF;
					}
					else {
						if (ctrl)
							ans[mp].mt = 0x1 << (MTSZ - 1),
							ans[mp].ex = 0x00F0;
						else
							ans[mp].ex -= 0xF;
					}
				}
			}
#define D 0
			/* calc next */
			if(1)
			for (int ih = 1; ih < h-1; ++ih)
				for (int iw = 1; iw < w-1; ++iw) {

//printf("y %d  x %d  M %d\n", ih, iw, YXE(ih, iw));

#define Cm (ans[YXE(ih, iw)   * 2].mt)
#define Cx (ans[YXE(ih, iw)   * 2].ex)
#define Cd (ans[YXE(ih, iw)   * 2].ds)
#define Nm (ans[YXE(ih-1, iw) * 2].mt)
#define Nx (ans[YXE(ih-1, iw) * 2].ex)
#define Nd (ans[YXE(ih-1, iw) * 2].ds)
#define Sm (ans[YXE(ih+1, iw) * 2].mt)
#define Sx (ans[YXE(ih+1, iw) * 2].ex)
#define Sd (ans[YXE(ih+1, iw) * 2].ds)
#define Em (ans[YXE(ih, iw-1) * 2].mt)
#define Ex (ans[YXE(ih, iw-1) * 2].ex)
#define Ed (ans[YXE(ih, iw-1) * 2].ds)
#define Wm (ans[YXE(ih, iw+1) * 2].mt)
#define Wx (ans[YXE(ih, iw+1) * 2].ex)
#define Wd (ans[YXE(ih, iw+1) * 2].ds)

					// outgoing
					MTTP Oc = Cm;					// outgoing center (per direction)
					Oc = (Oc >> 2);
					Oc = (Oc >> Cd);
					MTTP Ons = (Oc >> Nd)			// outgoing north and south
						+ (Oc >> Sd);
					MTTP Oew = (Oc >> Ed)			// outgoing east and west
						+ (Oc >> Wd);
					MTTP O = (Ons + Oew);			// outgoing total
					// remaining
					MTTP R = Cm - O;				// remaining total
					EXTP Rx = Cx;					// remaining exponent
					if (R == 0 && Cd != 0) printf("R == 0 && Cd != 0\n");
					if (R == 0)
						Rx = 0;
					// incoming
					EXTP Imx = max(max(Nx, Sx),		// incoming exponent
						max(Ex, Wx));
					Imx = Imx + 2;
					MTTP Ins = (Nm >> (min(MTBM, Imx - Nx)))	// incoming north and south
						+ (Sm >> (min(MTBM, Imx - Sx)));
					MTTP Iew = (Em >> (min(MTBM, Imx - Ex)))	// incoming east and west
						+ (Wm >> (min(MTBM, Imx - Wx)));
					MTTP I = Ins + Iew;			// incoming total
					EXTP Ix = Imx
						- Cd
						- Cd
						- 2
						;
					if (I == 0) {
						printf("I == 0!  Imx %d  Im %x  Ix %d\n", Imx, I, Ix);

						printf("         Nx  %d  Nm %x  Nm >> %x  Imx-Nx %d\n", Nx, Nm, (Nm >> (min(MTBM, Imx - Nx))), (Imx - Nx));
						printf("         Ex  %d  Em %x  Em >> %x  Imx-Ex %d\n", Ex, Em, (Em >> (min(MTBM, Imx - Sx))), (Imx - Ex));
						printf("         Sx  %d  Sm %x  Sm >> %x  Imx-Sx %d\n", Sx, Sm, (Sm >> (min(MTBM, Imx - Ex))), (Imx - Sx));
						printf("         Wx  %d  Wm %x  Wm >> %x  Imx-Wx %d\n", Wx, Wm, (Wm >> (min(MTBM, Imx - Wx))), (Imx - Wx));
					}
					// total
					EXTP Tx = max(Rx, Ix)			// total exponent
						+ 1;
					MTTP T = (R >> (min(MTBM, Tx - Rx)))		// total
						+ (I >> (min(MTBM, Tx - Ix)));

					MTTP lz = MTLZCF(T);


					T = T << lz;
					Tx = Tx
						- lz
						//- 2
						//- Cd
						;
					;
					if (T == 0) printf("T == 0!\n");
					//
					if (ans == oans) {
						ans[YXE(ih, iw) * 2 + 1].ex = Tx;
						ans[YXE(ih, iw) * 2 + 1].mt = T;
					}
					else {
						ans[YXE(ih, iw) * 2 - 1].ex = Tx;
						ans[YXE(ih, iw) * 2 - 1].mt = T;
					}
				}
				
			/* swap current ans with buffered ones */
			if (ans == oans)
				ans = oans + 1;
			else
				ans = oans;
			if (D) getch();
		}

		/* Update status and screen */
		///int mp = 2 * (min(h - 1, mouse_y) * w + min(w - 1, mouse_x));
		int mp = YXE(min(h - 1, mouse_y), min(w - 1, mouse_x))*2;
		double sl = min(1.0, 1.0 / (dmx - dmn));	// scale
		double v = sl * (oans[mp].ex - dmn + (1.0 / ((MTTP)1 << (MTSZ - 1)) * oans[mp].mt));

		SIMFW_SetStatusText(&simfw, "SIMFW TEST   fps %.2f   cs %.4e \nmn %4x/%8x/%.2e  mx %4x/%8x/%.2e  mp %4x/%8x/%3.0f  ds %d",
			1.0 / simfw.avg_duration, w * h / simfw.avg_duration * speed,
			exmn, mtmn, dmn, exmx, mtmx, dmx,
			oans[mp].ex, oans[mp].mt, 100*v, oans[mp].ds);
		SIMFW_UpdateDisplay(&simfw);
		SIMFW_UpdateStatistics(&simfw);

	}
behind_sdl_loop:;
	/* Cleanup */
	SIMFW_Quit(&simfw);
}