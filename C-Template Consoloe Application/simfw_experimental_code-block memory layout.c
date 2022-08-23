

// block and space size
#define B1ZP (16)													// block 1d-size in pixels
#define BOLP (1)													// block overlap in pixels
#define SYZB (32)													// space y-size in blocks
#define SXZB (32)													// space x-size in blocks

#define B2ZP (B1ZP * B1ZP)											// block 2d-size in pixels
#define B1NP (B1ZP - 2 * BOLP)										// block-net-size (without overlap) in pixels
#define SYZP (SYZB * B1NP)											// space y-size in pixels
#define SXZP (SXZB * B1NP)											// space x-size in pixels

/*
syzp - space y-size in pixels
sxzp - space x-size in pixels
lmv - line-memory first valid element - size given by syzp * sxzp
*/
void 
display_linear_memory(SIMFW *simfw, int syzp, int sxzp, UINT32 *lmv) {
	int dyzp = simfw->sim_height;											// display y-size in pixels
	int dxzp = simfw->sim_width;											// display x-size in pixels

	if (dyzp < syzp || dxzp < sxzp) {										// make sure display-size is sufficient
		printf("ERROR display_linear_memory display size to small\n");
		return;
	}

	UINT32 *dbc = simfw->sim_canvas;										// display buffer cursor
	dbc += (dyzp - syzp) / 2 * dxzp + (dxzp - sxzp) / 2;					// move dbc so lmv will be displayed centered

	// draw memory-buffer on display-buffer
	for (int iy = 0; iy < syzp; ++iy) {
		for (int ix = 0; ix < sxzp; ++ix) {
			*dbc = *lmv;
			++dbc;
			++lmv;
		}
		dbc += dxzp - sxzp;
	}
}

/*
syzb - space y-size in blocks
sxzb - space x-size in blocks
b1zp - block 1d-size in pixels
mbc - memory buffer cursor - size given by syzb * sxzb * b1zp * b1zp
*/
void
display_block_memory(SIMFW *simfw, int syzb, int sxzb, int b1zp, UINT32 *mbc) {
	int dyzp = simfw->sim_height;											// display y-size in pixels
	int dxzp = simfw->sim_width;											// display x-size in pixels
	int syzp = syzb * B1NP;													// space y-size in pixels
	int sxzp = sxzb * B1NP;													// space x-size in pixels

	if (dyzp < syzp || dxzp < sxzp) {										// make sure display-size is sufficient
		printf("ERROR display_block_memory display size to small\n");
		return;
	}

	UINT32 *dbv = simfw->sim_canvas;										// display buffer first valid element
	dbv += (dyzp - syzp) / 2 * dxzp + (dxzp - sxzp) / 2;					// move dbv so mbc will be displayed centered

																			// draw memory-buffer on display-buffer
	for (int iby = 0; iby < syzb; ++iby) {									// loop y-blocks (rows)
		for (int ibx = 0; ibx < sxzb; ++ibx) {								// loop x-blocks (columns)
			UINT32 *dbc = dbv + iby * B1NP * dxzp + ibx * B1NP;				// display-buffer-cursor initialized to point on top left corner of current block
			for (int iy = BOLP; iy < b1zp - BOLP; ++iy) {
				for (int ix = BOLP; ix < b1zp - BOLP; ++ix) {
					*dbc = mbc[(iby * SXZB + ibx) * B2ZP + iy * B1ZP + ix];
					++dbc;
				}
				dbc += dxzp - B1NP;
			}
		}
	}
}

/*
syzb - space y-size in blocks
sxzb - space x-size in blocks
b1zp - block 1d-size in pixels
mbc - memory buffer cursor - size given by syzb * sxzb * b1zp * b1zp
*/
void
display_block_memory_with_border(SIMFW *simfw, int syzb, int sxzb, int b1zp, UINT32 *mbc) {
	int dyzp = simfw->sim_height;											// display y-size in pixels
	int dxzp = simfw->sim_width;											// display x-size in pixels
	int syzp = syzb * b1zp;													// space y-size in pixels
	int sxzp = sxzb * b1zp;													// space x-size in pixels

	if (dyzp < syzp || dxzp < sxzp) {										// make sure display-size is sufficient
		printf("ERROR display_block_memory_with_border display size to small\n");
		return;
	}

	UINT32 *dbv = simfw->sim_canvas;										// display buffer first valid element
	dbv += (dyzp - syzp) / 2 * dxzp + (dxzp - sxzp) / 2;					// move dbv so mbc will be displayed centered

																			// draw memory-buffer on display-buffer
	for (int iby = 0; iby < syzb; ++iby) {									// loop y-blocks (rows)
		for (int ibx = 0; ibx < sxzb; ++ibx) {								// loop x-blocks (columns)
			UINT32 *dbc = dbv + iby * b1zp * dxzp + ibx * b1zp;				// display-buffer-cursor initialized to point on top left corner of current block
			for (int iy = 0; iy < b1zp; ++iy) {
				for (int ix = 0; ix < b1zp; ++ix) {
					*dbc = *mbc;
					++dbc;
					++mbc;
				}
				dbc += dxzp - b1zp;
			}
		}
	}
}

void
copy_block_overlap(int *OLCP, UINT32 *bmv) {
	for (int olcc = 0; olcc < 8 * 4; olcc += 4) {
		for (int iby = 1; iby < SYZB - 1; iby += 2) {									// loop y-blocks (rows)
			for (int ibx = 1; ibx < SXZB - 1; ibx += 2) {								// loop x-blocks (columns)
				UINT32 *src = bmv + (iby * SXZB + ibx) * B2ZP + OLCP[olcc + 0];		// source
				UINT32 *dst = bmv + (iby * SXZB + ibx) * B2ZP + OLCP[olcc + 1];		// destination
																					////							if (src < bmv || src >= bmi || (ibx == 0 && olcc < 3) || (ibx == SXZB - 1 && olcc > 4))
																					////								src = &osm;
				int ycz = OLCP[olcc + 2];									// y-copy-size
				int xcz = OLCP[olcc + 3];									// x-copy-size

				for (int icy = 0; icy < ycz; ++icy) {							// loop y-blocks (rows)
					for (int icx = 0; icx < xcz; ++icx) {						// loop x-blocks (columns)
						dst[icy * B1ZP + icx] = src[icy * B1ZP + icx];
					}
				}
			}
		}
	}
}

void
diffuse_block_memory(UINT32 *bmv) {
	static pcg32_random_t pcgr;
	int dn[] = {-B1ZP, -B1ZP + 1, 1 , +B1ZP + 1, +B1ZP, +B1ZP - 1, -1, -B1ZP - 1};	// directions
	for (int iby = 1; iby < SYZB - 1; iby += 2) {									// loop y-blocks (rows)
		for (int ibx = 1; ibx < SXZB - 1; ibx += 2) {								// loop x-blocks (columns)
			UINT32 *bmc = bmv + (iby * SXZB + ibx) * B2ZP;							// block-memory-cursor
			uint32_t rn = 0;														// random number
			for (int iy = 1; iy < B1ZP - 1; ++iy) {
				for (int ix = 1; ix < B1ZP - 1; ++ix) {
					//if (rand() % 8 == 0)
					//uint32_t rn = pcg32_random_r(&pcgr);
					//printf("rn %u\n", rn);
					if (rn < 1 << 7 ) {													// at least 8 random bits needed
						rn = pcg32_random_r(&pcgr);									// > reuse generated random number
						//rn = rand() << 15 | rand();
						//rn = rand();
						//printf("rn %x\n", rn);
					}
					// use 16 bit random number
					if (rn % 1 == 0) {
						rn = rn >> 4;
						int s = dn[rn % 8];
						rn = rn >> 4;
						UINT32 t = bmc[iy * B1ZP + ix];
						bmc[iy * B1ZP + ix] = bmc[iy * B1ZP + ix + s];
						bmc[iy * B1ZP + ix + s] = t;
					}
					else
						rn = rn >> 4;
				}
			}
		}
	}
}

void
diffuse_line_memory(UINT32 *bmv) {
	static pcg32_random_t pcgr;
	//int dn[] = { -SXZP, -SXZP + 1, 1 , +SXZP + 1, +SXZP, +SXZP - 1, -1, -SXZP - 1 };	// directions
	int dn[] = { -SXZP, +1, +SXZP, -1 };	// directions
	UINT32 *bmc = bmv;													// block-memory-cursor
	uint32_t rn = 0;														// random number
	for (int iy = 10; iy < SYZP - 10; ++iy) {
		for (int ix = 10; ix < SXZP - 10; ++ix) {
			//if (rn < 1 << 7) 
			//{													// at least 8 random bits needed
			//	rn = pcg32_random_r(&pcgr);									// > reuse generated random number
			//	//printf("rn %u\n", rn);
			//}
			//// use 16 bit random number
			//if (rn % 1 == 0) {
			//	rn = rn >> 4;
			//	int s = dn[rn % 8];
			//	rn = rn >> 4;
			//	UINT32 t = bmc[iy * SXZP + ix];
			//	bmc[iy * SXZP + ix] = bmc[iy * SXZP + ix + s];
			//	bmc[iy * SXZP + ix + s] = t;
			//}
			//else
			//	rn = rn >> 4;
			static UINT32 rng;
			if (rn < 1 << 5)
			{
				rn = rng;
				rng++;
			}
			////rn = rand() % 2;
			//rn = rn >> 4;


			int s = dn[rn % 4];
			rn = rn >> 3;
			//int s = 1;// dn[4];
			int p = iy * SXZP + ix;
			UINT32 t = bmc[p];
			bmc[p] = bmc[p + s];
			bmc[p + s] = t;
		}
	}
}

/*
demonstrates use of block memory layout which is used to reduce cache misses
*/
void
SIMFW_TestBlockMemoryLayout() {
		/* block-memory layout */
		// basic initialization
		int  OLCPo[] = {														// overlap copy positions
			// source - block  - within source block	destination within current block	y-    x-copy-size
			// west-positions have to be the first elements, east-positions the last elements
			(-SXZB -1) * B2ZP  + B1NP * B1ZP + B1NP,	0,									BOLP, BOLP,		// NW
			(      -1) * B2ZP  + BOLP * B1ZP + B1NP,	BOLP * B1ZP,						B1NP, BOLP,		// WW
			(+SXZB -1) * B2ZP  + BOLP * B1ZP + B1NP,	(BOLP + B1NP) * B1ZP,				BOLP, BOLP,		// SW
			(-SXZB   ) * B2ZP  + B1NP * B1ZP + BOLP,	BOLP,								BOLP, B1NP,		// NN
			(+SXZB   ) * B2ZP  + BOLP * B1ZP + BOLP,	(BOLP + B1NP) * B1ZP + BOLP,		BOLP, B1NP,		// SS
			(-SXZB +1) * B2ZP  + B1NP * B1ZP + BOLP,	BOLP + B1NP,						BOLP, BOLP,		// NE
			(      +1) * B2ZP  + BOLP * B1ZP + BOLP,	BOLP * B1ZP + BOLP + B1NP,			B1NP, BOLP,		// EE
			(+SXZB +1) * B2ZP  + BOLP * B1ZP + BOLP,	(BOLP + B1NP) * B1ZP + BOLP + B1NP,	BOLP, BOLP		// SE
		};
		int  OLCP[] = {														// overlap copy positions
			// source - block  - within source block	destination within current block	y-    x-copy-size
			0,						(-SXZB -1) * B2ZP  + B1NP * B1ZP + B1NP,	BOLP*2, BOLP*2,		// NW
			0,						(-SXZB   ) * B2ZP  + B1NP * B1ZP,			BOLP*2, B1ZP,		// NN
			B1NP,					(-SXZB +1) * B2ZP  + B1NP * B1ZP,			BOLP*2, BOLP*2,		// NE
			0,						(      -1) * B2ZP  + B1NP,					B1ZP,	BOLP*2,		// WW
			B1NP,					(      +1) * B2ZP,							B1ZP,	BOLP*2,		// EE
			B1NP * B1ZP,			(+SXZB -1) * B2ZP  + B1NP,					BOLP*2, BOLP*2,		// SW
			B1NP * B1ZP,			(+SXZB   ) * B2ZP,							BOLP*2, B1ZP,		// SS
			B1NP * B1ZP + B1NP,		(+SXZB +1) * B2ZP,							BOLP*2, BOLP*2		// SE
		};
		// init line-memory-buffer
		UINT32 *lmv;														// line-memory first valid element
		UINT32 *lmi;														// line-memory first invalid element
		UINT32 *lmc;														// line-memory current element
		int lmz = SYZP * SXZP;												// line-memory size in pixels
		lmv = malloc(lmz * sizeof(*lmv));
		lmi = lmv + lmz;
		// fill line-memory-buffer with color-gradient
		for (lmc = lmv; lmc < lmi; ++lmc) {
			*lmc = getColor(1.0 / (lmi - lmv) * (lmc - lmv), 0, 0, 0);
			//if ((int)lmc % 1000 == 0)
			//if (rand() % 100 == 0)
			//	*lmc = 0xFFFFFF;
		}
		// init block-memory-buffer
		UINT32 *bmv;														// block-memory first valid element
		UINT32 *bmi;														// block-memory first invalid element
		UINT32 *bmc;														// block-memory current element
		int bmz = SYZB * B1ZP * SXZB * B1ZP;								// block-memory size in pixels
		bmv = malloc(bmz * sizeof(*bmv));
		memset(bmv, 0, bmz  * sizeof(*bmv));
		bmi = bmv + bmz;
		// fill block-memory-buffer with color-gradient
		///for (bmc = bmv; bmc < bmi; ++bmc)
		///	*bmc = getColor(1.0 / (bmi - bmv) * (bmc - bmv), 2, 0, 0);

		bmc = bmv;
		int cc = 0; // current color
		for (int iby = 0; iby < SYZB; ++iby) {									// loop y-blocks (rows)
			for (int ibx = 0; ibx < SXZB; ++ibx) {								// loop x-blocks (columns)
				for (int iy = BOLP; iy < B1ZP - BOLP; ++iy) {
					for (int ix = BOLP; ix < B1ZP - BOLP; ++ix) {
						UINT32 col;
						if (iby == 0 || iby == SYZB - 1 || ibx == 0 || ibx == SXZB - 1)
							col = 0xFFFFFF;
						else if ((iby * 2 + ibx) % 4 == 0)
							col = 0x0;
						else {
							col = getColor(1.0 * cc / (SYZB - 2) / (SXZB - 2) / B1NP / B1NP, 2, 0, 0);
							++cc;
						}
						bmv[(iby * SXZB + ibx) * B2ZP + iy * B1ZP + ix] = col;
					}
				}
			}
		}
		// copy line-memory-buffer to block-memory-buffer
		lmc = lmv;
		for (int iy = 0; iy < SYZP; ++iy) {
			for (int ix = 0; ix < SXZP; ++ix) {
				int by = iy / B1NP;							// get block y-position
				int bx = ix / B1NP;							// get block x-position
				int ry = iy % B1NP;							// get relative y-position within block
				int rx = ix % B1NP;							// get relative x-position within block
				bmv[(by * SXZB + bx) * B2ZP + (ry + BOLP) * B1ZP + rx + BOLP] = *lmc;
				++lmc;
			}
		}
		// init overlap
		for (int ys = 0; ys < 2; ++ys)
			for (int xs = 0; xs < 2; ++xs)
				copy_block_overlap(&OLCPo, bmv + (ys * SXZB + xs) * B2ZP);

	/* Init */
	SIMFW simfw = { 0 };
	SIMFW_Init(&simfw, SIMFW_TEST_TITLE, SIMFW_TEST_DSP_HEIGHT, SIMFW_TEST_DSP_WIDTH, SIMFW_TEST_DSP_WINDOW_FLAGS, SIMFW_TEST_SIM_HEIGHT, SIMFW_TEST_SIM_WIDTH);

	/* SDL loop */
	while (1) {
		/* SDL event handling */
		SDL_Event e;
		if (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT)
				break;
			else if (e.type == SDL_KEYDOWN) {
				switch (e.key.keysym.sym) {
				case SDLK_PAGEUP:
					SIMFW_SetSimSize(&simfw, simfw.sim_height * 5 / 4, simfw.sim_width * 5 / 4);
					break;
				case SDLK_PAGEDOWN:
					SIMFW_SetSimSize(&simfw, simfw.sim_height * 3 / 4, simfw.sim_width * 3 / 4);
					break;
				case SDLK_SPACE: {
					static int olcc = 0;
///					UINT32 osm[B2ZP];
///					memset(&osm, 0xFF, B2ZP * sizeof(*osm));
					//for (int iby = 1; iby < SYZB - 1; ++iby) {									// loop y-blocks (rows)
					//	for (int ibx = 1; ibx < SXZB - 1; ++ibx) {								// loop x-blocks (columns)
					olcc += 4;
					if (olcc == 8 * 4)
						olcc = 0;
					break;
				}
				}
			}
		}

		int repeats = 16;
		if (1) {
			for (int r = 0; r < repeats; ++r) {
				diffuse_line_memory(lmv);
			}
			display_linear_memory(&simfw, SYZP, SXZP, lmv);
		}
		//else {
		//	int sh[] = { 0, B2ZP, SXZB * B2ZP, SXZB * B2ZP + B2ZP };
		//	for (int rp = 0; rp < repeats; ++rp) {
		//		for (int r = 0; r < 4; ++r) {
		//			///int rsh = sh[rand() % 4];
		//			static int rshp = 0;
		//			if (rshp >= 4)
		//				rshp = 0;
		//			int rsh = sh[rshp];
		//			rshp++;

		//			diffuse_block_memory(bmv + rsh);
		//			copy_block_overlap(&OLCP, bmv + rsh);
		//		}
		//	}
		//	display_block_memory(&simfw, SYZB, SXZB, B1ZP, bmv);
		//}
	
		/* Update status and screen */
		SIMFW_SetStatusText(&simfw, "SIMFW TEST   fps %.2f  #%.4e", 1.0 / simfw.avg_duration, 1.0 / simfw.avg_duration * simfw.sim_height * simfw.sim_width * repeats);
		SIMFW_UpdateDisplay(&simfw);
	}

	/* Cleanup */
	SIMFW_Quit(&simfw);
}