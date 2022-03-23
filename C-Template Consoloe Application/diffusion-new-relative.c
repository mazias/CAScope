//#include "stdafx.h"		// default includes (precompiled headers)
//#include "basetsd.h"
//#define MAXUINT16   ((UINT16)~((UINT16)0))
//
//#include "simfw.h"
//#include "float.h"
//
//#define SIMFW_TEST_TITLE	"ZELLOSKOP"
//#define SIMFW_FONTSIZE			12
//#define SIMFW_TEST_DSP_WIDTH	1200
//#define SIMFW_TEST_DSP_HEIGHT	512
//#define SIMFW_TEST_DSP_WIDTH	512
//#define SIMFW_TEST_DSP_WINDOW_FLAGS	SDL_WINDOW_FULLSCREEN_DESKTOP
//
//
//#define CP 16		// center position in bits from right
//
//typedef struct rc { // scaled int
//	uint8_t ds;		// diffusion speed
//	uint32_t rc:	// relative shifted cell value
//} rc;
//#define CTP	rc		// cell type
//void box_rc(CTP *ans, CTP *bans, int h, int w) {
//	int bh = 1 << (rand() % 5 + 4);
//	int bw = 1 << (rand() % 5 + 4);
//	int by = rand() % h;
//	int bx = rand() % w;
//	int ds = rand() % 3;
//	for (int ih = by; ih < min(h, by + bh); ++ih)
//		for (int iw = bx; iw < min(w, bx + bw); ++iw)
//			ans[ih * h + iw].ds = ds;
//}
//void init_rc(CTP *ans, CTP *bans, int h, int w) {
//	int sz = h * w;
//	for (int i = 0; i < sz; ++i)
//		ans[i].ds = 0x0,
//		ans[i].rc = 0x1 << (CP - 1);
//	//
//	for (int ri = 0; ri < 0x1F; ++ri)
//		box_rc(ans, bans, h, w);
//	//
//	//for (int ih = 0; ih < h / 2; ++ih)
//	//	for (int iw = 0; iw < w; ++iw)
//	//		ans[ih * h + iw].ds = 0x2;
//	// frame
//	for (int ih = 0; ih < h; ++ih)
//		for (int iw = 0; iw < w; ++iw)
//			if (ih == 0 || ih == h - 1 || iw == 0 || iw == w - 1)
//				ans[ih * h + iw].re = 0x0;
//	// copy to buffer
//	memcpy(bans, ans, sz * sizeof(*ans));
//}
//
//void
//TWOD_DIFFUSION_FP_RC(void) {
//
//
//	/* Init */
//	//
//	srand((unsigned)time(NULL));
//	static pcg32_random_t pcgr;
//	//
//	SIMFW simfw = { 0 };
//	SIMFW_Init(&simfw, SIMFW_TEST_TITLE, SIMFW_TEST_DSP_HEIGHT, SIMFW_TEST_DSP_WIDTH, SIMFW_TEST_DSP_WINDOW_FLAGS * 1, 0, 0);
//	//
//	printf("window  height %d  width %d\n", simfw.window_height, simfw.window_width);
//	printf("sim  height %d  width %d\n", simfw.sim_height, simfw.sim_width);
//	pixel_effect_moving_gradient(&simfw);
//	//
//	double zoom = 1.0;
//	int res = 1;
//	int ar = 3; /* auto range */
//	int cm = 1; /* color mode */
//	int crct = 0; /* color count */
//	int speed = 4;
//
//	int ipen = 0; // input enabled
//
//	double dwnr = 1; // draw - number of ranges
//	uint64_t tm = 0; /* time */
//
//					 /* */
//	const int h = simfw.sim_height;
//	const int w = simfw.sim_width;
//	const float rh = (float)simfw.sim_height / simfw.window_height;		// relative height
//	const float rw = (float)simfw.sim_width / simfw.window_width;		// relative width
//	const int sz = w * h;
//	CTP *ans = malloc(sz * sizeof(*ans));	// absolute numbers
//	CTP *bans = malloc(sz * sizeof(*bans));	// buffer for absolute numbers
//	init_rc(ans, bans, h, w);
//
//	/* SDL loop */
//	int mouse_y = h / 2, mouse_x = w / 2;
//	while (1) {
//		/* SDL event handling */
//		SDL_Event e;
//		while (SDL_PollEvent(&e)) {
//			SDL_Keymod kbms = SDL_GetModState(); // keyboard mod state
//			int ctrl = kbms & KMOD_CTRL;
//			int shft = kbms & KMOD_SHIFT;
//			if (e.type == SDL_QUIT)						break;
//			else if (e.type == SDL_MOUSEMOTION)			mouse_y = e.motion.y * rh + 0.5, mouse_x = e.motion.x * rw + 0.5;
//			else if (e.type == SDL_MOUSEBUTTONDOWN) {
//				ipen = !ipen; printf("input  %d", ipen);
//			}
//			else if (e.type == SDL_WINDOWEVENT_RESIZED);
//			else if (e.type == SDL_MOUSEWHEEL) {
//				if (shft)
//					dwnr *= pow(1.0001, e.wheel.y);
//				else
//					dwnr *= pow(1.01, e.wheel.y);
//				SIMFW_SetFlushMsg(&simfw, "draw nr. ranges %.4e", dwnr);
//			}
//			else if (e.type == SDL_KEYDOWN) {
//				/* */
//				int os = speed;		// old speed
//				int keysym = e.key.keysym.sym;
//				switch (keysym) {
//				case SDLK_F4:			goto behind_sdl_loop;
//				case SDLK_END:			speed = 32;			break;
//				case SDLK_HOME:			speed = 1;			break;
//				case SDLK_PAGEUP:		if (ctrl)	speed += 1;		else	speed *= 2;		break;
//				case SDLK_PAGEDOWN:		if (ctrl)	speed -= 1;		else	speed /= 2;		break;
//				case SDLK_SPACE:		init_rc(ans, bans, h, w);	break;
//				}
//				if (os != speed)
//					SIMFW_SetFlushMsg(&simfw, "speed %d", speed);
//			}
//		}
//
//		/* Draw */
//		static int gen = 0;
//		//		gen++;
//
//		if (gen % 2 == 0) {
//			void *sp;
//			sp = ans, ans = bans, bans = sp;
//		}
//
//		UINT32 *px = simfw.sim_canvas;
//		int mn = MAXUINT16, mx = 0;
//		/* get min/max */
//		for (int i = 0; i < sz; ++i) {
//			if (ans[i].rc > 0 && ans[i].rc < mn)		mn = ans[i].rc;
//			if (ans[i].rc > mx)							mx = ans[i].rc;
//		}
//
//		///mn = 0,	mtmn = 1, mtmx = 1, mx = 512;
//
//		double dmn = mn;
//		double dmx = mx;
//
//		double sl = 1.0 / (dmx - dmn);	// scale
//
//		for (int i = 0; i < sz; ++i) {
//			if (ans[i].rc == 0) {
//				*px = 0;
//				++px;
//				continue;
//			}
//
//			double v = 1.0 - sl * (ans[i].rc - dmn);
//
//			*px = getColorEx(v, 2, 1, 3, 1.0, 1.0, dwnr);
//
//			++px;
//		}
//		if (gen % 2 == 0) {
//			void *sp;
//			sp = ans, ans = bans, bans = sp;
//		}
//
//
//		for (int rt = 0; rt < speed; ++rt) {
//			//mp = h / 2 * w + w / 2;
//			if (ipen) {
//				int ps[] = { 0, -w, +1, +w, -1 };
//				//int ps[] = { 0 };
//				for (int ips = 0; ips < sizeof(ips) / sizeof(*ps); ++ips) {
//					int mp = mouse_y * w + mouse_x;
//					if (ans[mp].rc != 0) {
//						ans[mp].rc *= 2;
//					}
//				}
//			}
//			/* calc next */
//			//ans[(h / 2) * w + w / 2].ex = 0xFFF, ans[(h / 2) * w + w / 2].mt = 0xFFF;
//			//ans[mp].ex = 0xFFF, ans[mp].mt = 0xFFF;
//			for (int i = 0; i < sz; ++i) {
//				if (ans[i].rc != 0) {
//
//					uint32_t nv = 0;
//					nv += ans[i].rc << (2 + ans[i].ds);
//					int lzcs = _lzcnt_u32(nv);
//					nv -= ans[i].rc >> ans[i - w].ds;
//					nv -= ans[i].rc >> ans[i + 1].ds;
//					nv -= ans[i].rc >> ans[i + w].ds;
//					nv -= ans[i].rc >> ans[i - 1].ds;
//					nv += ans[i - w].rc;
//					nv += ans[i + 1].rc;
//					nv += ans[i + w].rc;
//					nv += ans[i - 1].rc;
//					int lzcb = _lzcnt_u32(nv);
//
//					uint32_t lzc = _lzcnt_u32(nv);
//					if (lzc < 16)
//						lzc = 16 - lzc;
//					else
//						lzc = 0;
//					bans[i].ex = mnex + lzc - 2;// -ans[i].ds;
//					bans[i].mt = nv >> lzc;
//				}
//			}
//			/* swap current ans with buffered ones */
//			void *sp;
//			sp = ans, ans = bans, bans = sp;
//		}
//
//		/* Update status and screen */
//		int mp = mouse_y * w + mouse_x;
//		double v = (ans[mp].ex - mn - (2.0 / 0xFFFF * mtmn - 1.0) + (2.0 / 0xFFFF * (ans[mp].mt) - 1.0));
//		v = (ans[mp].ex - dmn + (1.0 / 16.0 * (15 - __lzcnt16(ans[mp].mt << 1))));
//
//		SIMFW_SetStatusText(&simfw, "SIMFW TEST   fps %.2f   cs %.4e \nmn %5d/%5d/%.4f  mx %5d/%5d/%.4f  mp %5d/%5d %6e  ds %d",
//			1.0 / simfw.avg_duration, w * h / simfw.avg_duration * speed,
//			mn, mtmn, dmn, mx, mtmx, dmx,
//			bans[mp].ex, bans[mp].mt, v, bans[mp].ds);
//		SIMFW_UpdateDisplay(&simfw);
//	}
//behind_sdl_loop:;
//	/* Cleanup */
//	SIMFW_Quit(&simfw);
//}