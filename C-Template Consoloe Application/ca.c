
// rule 1507 und 1146 in stzm 1 schön

#include "stdafx.h"							// default includes (precompiled headers)
#include "simfw.h"							// simulation framework library (abstracts SDL2)
#include "color.h"							// color functions
#include "float.h"
#include "hp_timer.h"						// high performance timing functions
#include "pcg_basic.h"						// random number generator library

// defines for gcc
#define MAXUINT16 ((UINT16)~ ((UINT16)0))
#define MAXUINT32 ((UINT32)~ ((UINT32)0))
#define MAXUINT64   ((UINT64)~((UINT64)0))
#define MAXUINT     ((UINT)~((UINT)0))
#define MAXINT      ((INT)(MAXUINT >> 1))
#define MININT      ((INT)~MAXINT)
// end defines for gcc

#include "math.h"
#define M_E        2.71828182845904523536   // Euler's number

// Return a random double value in the range [0, 1)
// TODO __inline does not work with gcc high optimization setting
double random01() {
	return ldexp(pcg32_random(), -32);
}

/* OpenCL */
#define ENABLE_CL 0
// to enable OpenCL also select the appropiate build option ("Release OpenCL enabled") include OpenCL headers in Settings > Compiler > Additional Include Directories and the lib file in Settings > Linker > Additional Dependencies / Zusätzliche Abhängigkeiten
#if ENABLE_CL
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS // https://stackoverflow.com/questions/28500496/opencl-function-found-deprecated-by-visual-studio
#include <CL/cl.h>
#endif

#define SIMFW_TEST_TITLE	"ZELLOSKOP"
#define SIMFW_FONTSIZE			12
#define SIMFW_TEST_DSP_WIDTH	1200
#define SIMFW_TEST_DSP_HEIGHT	512+128
#define SIMFW_TEST_DSP_WIDTH	512+128
//#define SIMFW_TEST_DSP_HEIGHT	128
//#define SIMFW_TEST_DSP_WIDTH	256
//#define SIMFW_TEST_DSP_HEIGHT	1024
#define SIMFW_TEST_DSP_WINDOW_FLAGS	0
#define SIMFW_TEST_DSP_WIDTH	1024
#define SIMFW_TEST_DSP_HEIGHT	800
//#define SIMFW_TEST_DSP_WINDOW_FLAGS	SDL_WINDOW_FULLSCREEN_DESKTOP
//#define SIMFW_TEST_SIM_WIDTH	1366
//#define SIMFW_TEST_SIM_HEIGHT	768
#define SIMFW_TEST_FPS_LIMIT	100
#define SIMFW_SDL_PIXELFORMAT		SDL_PIXELFORMAT_ARGB8888

#define BYAL	(64)								// byte alignment


typedef UINT8 CA_CT; /* memory-type of a cell */
//typedef cl_uchar CA_CT; /* memory-type of a cell */
//typedef int CA_CT; /* memory-type of a cell */


SIMFW sfw = { 0 };
CA_CT* ca_space = NULL;								// cell-space
uint64_t ca_space_sz = 1 << 16;

int sdmrpt;											// hash-array - speed-multiplicator-as-power-of-two - 1 means ca runs with speed of half its size (speed=(size/2)*2^(sdmrpt-1))


typedef struct caDisplaySettings {
	int fsme;										// focus-mode - 0: absolute, 1: totalistic, 2: frequency - only in hash computing mode
	int tm;											// test mode - 0: disabled, 1: graphics, 2: computation
	unsigned int ar;								// auto-range - 0: none, 1: auto min, 2: auto max, 3: auto min and max
	double ard;										// auto-range delay
	double arf;										// auto-range force
	unsigned int cm;								// color mode
	unsigned int crct;								// color count
	unsigned int gm;								// gradient mode
	unsigned int lgm;								// log mode
	unsigned int cmm;								// cumulative mode
	double sd;										// spread - range: 0.0 - +2.0; 1.0 = disabled
	double te;										// translate - range: -1.0 - +1.0 / 0.0 = disabled
	UINT32 plw;										// pixel-screen line width
	unsigned int pnp;								// pixel-screen pitch // currently ignored
	int sfcm;										// screen-filling-curve-mode
	int sfcsw;										// screen-filling-curve-step-width
	int sfcmp;										// screen-filling-curve-mode-paramter
	int vlfs;										// focus > nr. of hlfs cells next to eachother are summed... TODO
	int hlfs;										// focus > nr. of hlfs cells next to eachother are summed... TODO
	int vlzm;										// vertical zoom - how many vertical cells will make out one pixel (together with hor. zoom); 0 = disable drawing, < 0 = how many vertical pixels a cell will make out
	int hlzm;										// horizontal zoom - how many horizontal cells will make out one pixel (together with vert. zoom); 0 = disable drawing, < 0 = how many horizontal pixels a cell will make out
	int vlpz;										// vertical pixel zoom
	int hlpz;										// horizontal pixel zoom
	int stzm;										// state zoom - 0 = direct zoom on intensity
	int sm;											// scanline-mode
	int hddh;										// hash-display-depth
	int hdll;										// hash-display-level
	int mlsc;										// manual shift correction
} caDisplaySettings;
caDisplaySettings ds;								// display settings

typedef struct
CA_RULE {
	UINT64 rl;										/* rule */
	int ncs;										/* number of cell-states */
	int ncn;										/* number of cells in a neighborhod */
	int nns;										/* number of neighborhod-states */
	UINT64 nrls;									/* number of rules */
	CA_CT* rltl;									/* rule table - maps neighborhod-state of present generation to cell-state of future generation */
	CA_CT* mntl;									/* multiplication table - all 1 for totalistic, xx for absolute TODO */
	enum { ABS, TOT } tp;							/* type - absolute or totalistic*/
} CA_RULE;

const CA_RULE CA_RULE_EMPTY = { 0, 0, 0, 0, 0, 0, 0, 0 };

/* Bit-array */
#define BBT UINT32									// bit-block-type - datatype used to do operations (esp. bit manipulation) on vertical-bit-array
#define BBTBTCTPT 5									// bit-block-type-bit-count as power of 2
#define BBTBTCTMM ((1<<BBTBTCTPT) - 1)				// bit-block-type-bit-count-mod-mask - is equivalent to X % vertical-bit-block-type-bit-count (must be power of 2)
#define FBPT UINT16									// full-bit-block c-data-type
#define HBPT UINT8									// half-bit-block c-data-type
#define BPB 16										// bits per block, i.e. bits per elemet in bta
#define BPBMM (BPB - 1)								// bits-per-block-mod-mask - X & (BPB - 1) is equivalent to X % BPB when BPB is power of 2
#define BPBS 4										// bits per block shift - i.e. nr of bit shifts needed to divide by BPB
typedef struct caBitArray {
	BBT* v;											// vertical/horizontal-bit-array (vhba) - first valid element / array-pointer to bit array
	int ct;											// vhba-count in bits (including overlap rows)
	int bcpt;										// vhba-block-count as power of two
	int bh;											// vhba-block-height in nr. of rows (including overlap rows)
	int lcpt;										// vhba-lane-count in nr. of lanes (columns) as power of two
	int lwpt;										// vhba-lane-width in bits/cells as power of two
	int rwpt;										// vhba-row-width in bits as power of two; rwpt = lwpt + lcpt
	int oc;											// vhba-overlap-row-count - set to 0 for auto-initialization
	int mwpt;										// vhba-memory-window in rows/lines - must be power of two! - only by convertBetweenCACTandCABitArray; (each rw wide; should allow memory accesses to stay within fastest cache
	int rc;											// vhba-row-count (including overlap rows)		(automatically determined by initCAVerticalBitArray)
	CA_CT* clsc;									// cell-space
	uint64_t scsz;									// cell-space-size in nr of cells
	int brsz;										// cell-space-border-size in nr of cells
	CA_RULE* cr;									// ca-rule configuration
} caBitArray;

__inline
UINT64 ipow(UINT64 base, int exp) {
	UINT64 result = 1;
	while (exp) {
		if (exp & 1)
			result *= base;
		exp >>= 1;
		base *= base;
	}
	return result;
}

// https://github.com/dmoulding/log2fix
__inline
int32_t log2fix(uint32_t x, size_t precision)
{
	int32_t b = 1U << (precision - 1);
	int32_t y = 0;

	if (precision < 1 || precision > 31) {
		return INT32_MAX; // indicates an error
	}

	if (x == 0) {
		return INT32_MIN; // represents negative infinity
	}

	while (x < 1U << precision) {
		x <<= 1;
		y -= 1U << precision;
	}

	while (x >= 2U << precision) {
		x >>= 1;
		y += 1U << precision;
	}

	uint64_t z = x;

	for (size_t i = 0; i < precision; i++) {
		z = (z * z) >> precision;
		if (z >= 2U << (uint64_t)precision) {
			z >>= 1;
			y += b;
		}
		b >>= 1;
	}

	return y;
}

// fnv hash functions: http://www.isthe.com/chongo/tech/comp/fnv/index.html
/*
 * 32 bit magic FNV-0 and FNV-1 prime
 */
#define FNV_32_PRIME ((Fnv32_t)0x01000193)
#define FNV1_32_INIT ((UINT32)2166136261)

 /*
  * fnv_32_buf - perform a 32 bit Fowler/Noll/Vo hash on a buffer
  *
  * input:
  *	buf	- start of buffer to hash
  *	len	- length of buffer in octets
  *	hval	- previous hash value or 0 if first call
  *
  * returns:
  *	32 bit hash as a static hash type
  *
  * NOTE: To use the 32 bit FNV-0 historic hash, use FNV0_32_INIT as the hval
  *	 argument on the first call to either fnv_32_buf() or fnv_32_str().
  *
  * NOTE: To use the recommended 32 bit FNV-1 hash, use FNV1_32_INIT as the hval
  *	 argument on the first call to either fnv_32_buf() or fnv_32_str().
  */
UINT32
fnv_32_buf(void* buf, size_t len, UINT32 hval)
{
	unsigned char* bp = (unsigned char*)buf;	/* start of buffer */
	unsigned char* be = bp + len;		/* beyond end of buffer */

	/*
	 * FNV-1 hash each octet in the buffer
	 */
	while (bp < be) {

		/* multiply by the 32 bit FNV magic prime mod 2^32 */
#if defined(NO_FNV_GCC_OPTIMIZATION)
		hval *= FNV_32_PRIME;
#else
		hval += (hval << 1) + (hval << 4) + (hval << 7) + (hval << 8) + (hval << 24);
#endif

		/* xor the bottom with the current octet */
		hval ^= (UINT32)*bp++;
	}

	/* return our new hash value */
	return hval;
}

UINT16
fnv_16_buf(void* buf, size_t len)
{
#define MASK_16 (((UINT32)1<<16)-1) /* i.e., (u_int32_t)0xffff */
	UINT32 hash;
	hash = fnv_32_buf(buf, len, FNV1_32_INIT);
	hash = (hash >> 16) ^ (hash & MASK_16);
	return (UINT16)hash;
}

// returns 0-terminated string of content of filename
char* read_file(const char* filename) {
	printf("INF read file  %s", filename);
	// Get the current working directory:
	char* cwdbuf[1024];
	if (_getcwd(cwdbuf, sizeof(cwdbuf)) == NULL)
		perror("_getcwd error\n");
	else {
		printf("\tsearch path  %s\n", cwdbuf);
	}

	char* buffer = 0;
	long length;
	FILE* f = NULL;
	fopen_s(&f, filename, "rb");

	if (f) {
		fseek(f, 0L, SEEK_END);
		length = ftell(f);
		fseek(f, 0L, SEEK_SET);
		buffer = malloc(length + 1);		// + 1 to have space for 0-terminator
		if (buffer) {
			fread(buffer, 1, length, f);
		}
		buffer[length] = 0;					// 0-terminate string
		fclose(f);
	}
	else
		printf("\tERR file not found!\n");

	return buffer;
}

void
CA_PrintSpace(char** caption, CA_CT* spc, int sz) {
	printf("print space  %s", caption);
	for (int i = 0; i < sz; i++) {
		if (i && !(i % 8))
			printf(".");
		printf("%d", spc[i]);
	}
	printf("\n");
}

/*
Save cellular-automata-space to file
*/
void
CA_SaveToFile(const char* filename, CA_CT* spc, int ct) {
	FILE* stream;
	errno_t err = fopen_s(&stream, filename, "wb");
	if (err)
		SIMFW_DbgMsg("The file  %s  could not be opened. Err: %d\n", filename, err);
	else {
		fwrite(spc, sizeof(CA_CT), ct, stream);
		fclose(stream);
	}
}

/*
Load cellular-automata-space from file
*/
CA_CT*
CA_LoadFromFile(const char* filename, int* ct) {
	FILE* stream;
	errno_t err = fopen_s(&stream, filename, "rb");
	if (err)
		SIMFW_DbgMsg("The file  %s  could not be opened. Err: %d\n", filename, err);
	else {
		// get file size
		fseek(stream, 0L, SEEK_END);
		long nwsz = ftell(stream);							// new size
		fseek(stream, 0L, SEEK_SET);
		if (ct)
			*ct = nwsz;
		if (!nwsz)
			return NULL;
		// alloc memory
		CA_CT* nwsc = NULL;									// new space
		nwsc = malloc(nwsz * sizeof * nwsc);
		if (!nwsc)
			return NULL;
		// read data and close
		fread(nwsc, sizeof(CA_CT), nwsz / sizeof(CA_CT), stream);
		fclose(stream);
		//
		return nwsc;
	}
	return NULL;
}

/*
	elpr		elements per row
*/
void print_bitblock_bits(FBPT* fbp, size_t num_elements, int elpr) {
	int rn = 0; // row-number
	printf("[");
	for (size_t i = 0; i < num_elements; i++) {
		if (elpr && i && !(i % elpr)) {
			printf(" %3x\n ", rn);
			++rn;
		}
		for (int ib = 0; ib < BPB; ++ib) {
			printf("%c", (1 & (fbp[i] >> ib)) ? '1' : '0');
			if (ib && (ib + 1) % 8 == 0)
				printf(".");
		}
		printf("|");
	}
	printf("]\n");
}

void print_byte_as_bits(char val) {
	for (int i = 0; i < 8; i++) {
		printf("%c", (val & (1 << i)) ? '1' : '0');
	}
}

/*
	bypr		bytes per row
*/
void print_bits(unsigned char* bytes, size_t num_bytes, int bypr) {
	printf("[");
	for (size_t i = 0; i < num_bytes; i++) {
		if (bypr && i && !(i % bypr))
			printf("\n ");
		print_byte_as_bits(bytes[i]);
		printf(" ");
	}
	printf("]\n");
}

/**
* from, to		in bits, must be multiple of size of ba->v type (currently 32)
*/
__inline
void
CABitArrayPrepareOL(caBitArray* ba) {
	int bbrwpt = ba->rwpt - BBTBTCTPT;						// bit-blocks per row as power of two
	int bcmk = (1 << ba->bcpt) - 1;							// block-count-mask (x & bcmk == x % bc)
	int ccmk = (1 << bbrwpt) - 1;							// column-count-mask (x & bcmk == x % bc)
	int ofszbb = ba->oc * (1 << (ba->rwpt - BBTBTCTPT));	// overflow-size in nr of BBTs
	int bkhtbt = ba->bh * (1 << (ba->rwpt - 3));			// block-height (including overflow) in bytes
	int ofszbt = ba->oc * (1 << (ba->rwpt - 3));			// overflow-size in bytes
	int	to = 1 << bbrwpt;
	for (int b = 0; b < 1 << ba->bcpt; ++b) {
		BBT* cb = ba->v + ((((b + 0))			* ba->bh) << bbrwpt);		// current-block
		BBT* nb = ba->v + ((((b + 1) & bcmk)	* ba->bh) << bbrwpt);		// next-block
		BBT* of = cb + ((ba->bh - ba->oc) << bbrwpt);						// overflow - beginning of overflow region
//printf("b %d  cb %4d  nb %4d  of %4d  bh %d/%d\n", b, cb - ba->v, nb - ba->v, of - ba->v, ba->bh, ba->bh<< bbrwpt);
		memcpy(
			(char*)cb + bkhtbt - ofszbt,
			(char*)cb,
			ofszbt);
		for (int r = 0; r < ba->oc; ++r) {
			BBT* cr = of + (r << bbrwpt);		// current-row
			for (int c = 0; c < to; ++c) {
				cr[c] = cr[c] >> 1;
				if (c == to - 1)
					cr[c] |= nb[(r << bbrwpt)] << BBTBTCTMM;
				else
					cr[c] |= cr[(c + 1) & ccmk] << BBTBTCTMM;
			}
		}
	}
}

/*
* See notes of caBitArray struct!
*
* Make sure to free free (_aligned_free) ba.v before reseting / reinitializing
*
* ba	make sure ba is initialized to zero before first use (ba = { 0 };
*/
void
initCAVerticalBitArray(caBitArray* ba, CA_RULE* rl) {
	ba->ct = max(ba->ct, ba->scsz);
	ba->oc = max(ba->oc, rl->ncn - 1);
	ba->lwpt = max(BBTBTCTPT, ba->lwpt);
	ba->lcpt = max(0, ba->lcpt);
	ba->rwpt = ba->lwpt + ba->lcpt;

	{
		int bh;						// block-height in nr. of rows - including overlap rows
		bh = ba->ct;
		bh = (bh + (1 << ba->rwpt) - 1) / (1 << ba->rwpt);
		bh = (bh + (1 << ba->bcpt) - 1) / (1 << ba->bcpt);
		bh = (bh + (1 << ba->mwpt) - 1) / (1 << ba->mwpt) * (1 << ba->mwpt);
		bh = max(1, bh);
		bh += ba->oc;
		ba->bh = bh;
	}

	ba->rc = ba->bh << ba->bcpt;

	ba->ct = ba->bh << (ba->rwpt + ba->bcpt);

	ba->v = _aligned_malloc(ba->ct >> 3, BYAL);
	printf("initCAVerticalBitArray   scsz %d  ct %d  bc 2^%d  bh %d  rc %d  oc %d  rw 2^%d (%d)  lc 2^%d  lw 2^%d  mw 2^%d  v %p\n", ba->scsz, ba->ct, ba->bcpt, ba->bh, ba->rc, ba->oc, ba->rwpt, 1 << ba->rwpt, ba->lcpt, ba->lwpt, ba->mwpt, ba->v);
}

/*
csv		cell-space first valid element
csi		cell-space first invalid element
ba		(vertical-)bit-array
dr		direction - 0: csv -> ba; !0 ba -> csv
*/
void
convertBetweenCACTandCABitArray(caBitArray* ba, int dr) {
#define DG 0
	if (DG) printf("\33[2J\33[0;0fBIT-ARRAY (rc %d  rw 2^%d  ct %d) [\n", ba->rc, ba->rwpt, ba->ct);

	int dcl = 0;											// vertical-bit-array-destination-column
	int drw = 0;											// vertical-bit-array-destination-row (relative to block)
	int dbk = 0;											// vertical-bit-array-destination-block
	unsigned int bacsp = 1 << (ba->rwpt - BBTBTCTPT);		// vertical-bit-array-cursor-step (in bytes)
	int l = ba->scsz;										// cell-space-length / size in nr. of cells
	CA_CT* csv = ba->clsc;									// cell-space first valid element
	CA_CT* csi = ba->clsc + ba->scsz;						// cell-space first invalid element
	CA_CT* csc = csv;										// cell-space-cursor
	int cp = 0;												// check-position-counter

	if (!dr)
		memset((UINT8*)ba->v, 0, ba->ct >> 3);				// vertical-bit-array has to be zero'd before use, as individual bits are 'ored' in

	goto calculate_next_check_position;

check_position:
	//if (DG) getch();
	if (!(drw & ((1 << ba->mwpt) - 1))) {					// vba-destination-row behind memory-window or last row
		drw -= 1 << ba->mwpt;								// > move destination-row back to beginning of memory-window
		++dcl;												// > move to next column
		//		printf("drw %d  dcl %d\n", drw, dcl);
		if (dcl == (1 << ba->rwpt)) {						// if vba-destination-column behind row-width
			dcl = 0;										// > move destination-column to beginning / zero
			drw += 1 << ba->mwpt;							// > move destination-row to beginning of next memory-window
			if (drw >= ba->bh - ba->oc) {					// if vba-destination-row is behind last row in block
				drw = 0;									// > move destination row (relative to block) to zero
				dbk++;										// > move to next destination-block
				if (dbk >= 1 << ba->bcpt)					// if vba-destination-block is behind last block
					goto end_loop;							// > conversion is finished
			}
		}
//		printf("         dbk %d  drw %d  dcl %d\n", dbk, drw, dcl);
	}
calculate_next_check_position:;								// calculate position in (horizontal-)source-byte-array
	int i = ((dbk * (ba->bh - ba->oc)) << ba->rwpt) + drw + dcl * (ba->bh - ba->oc);					// cell-space-index of next cell to convert#
//printf("i %d\n", i);
	while (i >= l)											// avoid using expensive modulo operator, i.e. same as i %= l
		i -= l;
	csc = csv + i;
	cp = min(1 << ba->mwpt, csi - csc);						// next check-position is next memory-window or earlier when the nr. of remaining cells after cursor in source array is smaller than size of memory window
	BBT* bac = ba->v + (((((dbk * ba->bh) + drw) << ba->rwpt) + dcl) >> BBTBTCTPT);	// vertical-bit-array-cursor
	unsigned int bacst = dcl & BBTBTCTMM;						// vertical-bit-array-cursor-shift (in bits)
convert_next_cell:
//printf("bac  %p  dbk %d  drw %d  dcl %d  cp %d  csc %d\n", bac, dbk, drw, dcl, cp, csc - csv);
	///		if (DG) printf("\33[%d;%df%.3x %c ", drw + 2, 8 + dcl * 6, csc - csv, *csc ? '#' : '.');
	if (DG) printf("\tcsc %d  bar %d  bac %d   bax #%x  %c\n", csc - csv, drw, dcl, bac - ba->v, *csc ? '#' : '.');
	if (dr)
		*csc = 1 & ((*bac) >> bacst);						// convert vba -> csv
	else
		*bac |= ((BBT)*csc) << bacst;						// convert csv -> vba
	//
	++drw;
	++csc;
	--cp;
	if (!cp)
		goto check_position;
	bac += bacsp;
	goto convert_next_cell;
end_loop:
	if (DG) printf("]\n");
}

/*
sync-function for vertical-bit-array
*/
void CA_SCFN_VBA(caBitArray* vba) {
	convertBetweenCACTandCABitArray(vba, 1);
}

/*
bav		bit-array first valid element
csv		cell-space first valid element
ct		count / nr. cells to convert - must be larger than 0
---
csv and bav must allow for access of  (ct + (BPB - 1)) / BPB  elements
*/
void
convert_bit_array_to_CA_CT(FBPT* bav, CA_CT* csv, int ct) {
	int bib = 0;						// bit index in individual bit-block
	ct /= BPB;
loop:
	*csv = (FBPT)1 & (*bav >> bib);
	++csv;
	++bib;
	if (bib < BPB)
		goto loop;
	bib = 0;
	++bav;
	--ct;
	if (ct)
		goto loop;

	// Shorter version, but in assembler it is about the same length and seems (not really tested or benchmarked) to do more work
	//for (int bi = 0; bi <= ct; ++bi)
		//csv[bi] = (FBPT)1 & (bav[bi / BPB]>>((bi % BPB)));
}

/*
sync-function for horizontal-bit-array
*/
void CA_SCFN_HBA(caBitArray* vba) {
	convert_bit_array_to_CA_CT((FBPT*)vba->v, vba->clsc, vba->scsz);
}



/*
Plan for vertical-bit-array memory layout that is suitable for parallel usage
> seperate memory blocks of each parallel process to avoid false sharing.
1 lane is handles by 1 parallel process

cell-space-size: 1f elements
vba: 2 lanes, each 4 cells wide

source adr.		dest. adr.		vba
-cell-space		-vba			overlap
-in bytes		-in bits		source

first lane
00 04 08 0c		00 01 02 03
01 05 09 0d		04 05 06 07
02 06 0a 0e		08 09 0a 0b
03 07 0b 0f		0c 0d 0e 0f
--overlap--		--overlap--
04 08 0c 10		10 11 12 13		01 02 03 18
05 09 0d 11		14 15 16 17		05 06 07 1c

second lane
10 14 18 1c		18 19 1a 1b
11 15 19 1d		1c 1d 1e 1f
12 16 1a 1e		20 21 22 23
13 17 1b 1f		24 25 26 27
--overlap--		--overlap--
14 18 1c 00		28 29 2a 2b		19 1a 1b 00
15 19 1d 01		2c 2d 2e 2f		1d 1e 1f 04
*/

/*
vbatst(CA_CT* csv, CA_CT* csi, caBitArray* ba, int dr) {
	//unsigned int rwsz	= 1<<(rwwpt - 3);						// row-size in bytes
	unsigned int rpbwol;										// rows per block without overlap
	rpbwol = (ba->scsz + (1<<(bcpt + rwwpt)) - 1) / (1<<(bcpt + rwwpt));		// i.e.: scsz / (block-count * row-width) rounded up to multiple of (block-count * row-width)
	unsigned int rpbiol = rpbwol + ba->oc;						// rows per block including overlap
	//unsigned int bksz = rpbiol<<(rwwpt - 3);				// block-size in bytes
	unsigned int vbasz = rpbiol<<(bcpt + rwwpt - 3);			// vertical-bit-array-size in bytes

	// convert cell-space-position to vba-position
	CA_CT* csc = csv;										// cell-space-cursor
	int csi = 0;	// cell-space-index
	int vbai = 0;	// vba-index (in bits)

	// convert vba-position to cell-space-position
	unsigned int bi;			// block-iterator
	for (bi = 0; bi < 1<<bcpt; bi++) {

	}

}
*/
/*
csv		cell-space first valid element
bav		bit-array first valid element
ct		count / nr. cells to convert - must be larger than 0
---
csv and bav must allow for access of  (ct + (BPB - 1)) / BPB  elements
*/
void
convert_CA_CT_to_bit_array(CA_CT* csv, FBPT* bav, int ct) {
	*bav = 0;							// must be zero'd, since individual bits are or'ed in
	int bib = 0;						// bit index in individual bit-block
	ct /= BPB;
loop:
	*bav |= (FBPT)*csv << bib;
	++csv;
	++bib;
	if (bib < BPB)
		goto loop;
	bib = 0;
	++bav;
	*bav = 0;							// must be zero'd, since individual bits are or'ed in - as ba.v always has to have an extra element, this also works when count is zero - this should make the if-jump below less complicated in asm
	--ct;
	if (ct)
		goto loop;
}

static UINT8* calt = NULL;									// ca-lookup-table/LUT
void CA_CNITFN_LUT(caBitArray* vba, CA_RULE* cr) {
	_aligned_free(vba->v);
	vba->ct = (vba->scsz + BPB - 1) / BPB * BPB + BPB;		// bit-array-count (rounded up / ceil) // allow one extra-element since when we process the last half block we read one half block past the end
	vba->v = _aligned_malloc(vba->ct / 8, BYAL);

	// build the LUT
	_aligned_free(calt);
	calt = _aligned_malloc(0x40000, 64);
	// Precalculate all states of a space of 16 cells after 4 time steps.
	// Ruleset: 2 states, 3 cells in a neighborhod
	// After 4 time steps 8 cells (1 byte in binary representation) will remain
	for (int dn = 1; dn < 5; ++dn) {				// duration-loop
		UINT16 i = 0;								// cell-space-index
		for (;;) {									// cell-space-loop
            UINT16 cs = i;							// result cell-space
			// get result byte
			for (int tm = 0; tm < dn; ++tm) {		// time-loop
				for (int p = 0; p < 14; ++p) {
					// get result bit
					int sr =
						cr->mntl[0] * (1 & (cs >> (p + 0))) +
						cr->mntl[1] * (1 & (cs >> (p + 1))) +
						cr->mntl[2] * (1 & (cs >> (p + 2)));
					// set result bit in cs
					if (cr->rltl[sr] == 0)
						cs &= ((UINT16)~((UINT16)1 << p));
					else
						cs |= (UINT16)1 << p;
				}
			}
			// store result byte
			calt[((dn - 1) << 16) | i] = (UINT8)cs;
			// inc / finish
			if (i == MAXUINT16)
				break;
			++i;
		}
	}
}

int64_t CA_CNFN_LUT(int64_t pgnc, caBitArray* vba) {
	convert_CA_CT_to_bit_array(vba->clsc, (FBPT*)vba->v, vba->scsz);

	while (pgnc) {
		// copy wrap arround buffer
		uint8_t* wb = vba->v;
		wb[vba->scsz / 8] = wb[0];
		//
		for (int bi = 0, sz = vba->scsz / 8; bi < sz; ++bi) {
			UINT8* bya = vba->v;  // need to adress on byte-basis
			bya += bi;
			*bya = calt[(((UINT32)min(3, pgnc - 1)) << 16) | *(UINT16*)bya];
		}
		pgnc -= min(4, pgnc);
	}

	return 0;
}

void CA_CNITFN_BOOL(caBitArray* vba, CA_RULE* cr) {
	_aligned_free(vba->v);
	vba->ct = (vba->scsz + BPB - 1) / BPB * BPB + BPB;		// bit-array-count - round up count and allow one extra-element since when we process the last half block we read one half block past the end
	vba->v = _aligned_malloc(vba->ct / 8, BYAL);
}

int64_t CA_CNFN_BOOL(int64_t pgnc, caBitArray* vba) {
	convert_CA_CT_to_bit_array(vba->clsc, (FBPT*)vba->v, vba->scsz + 8);

	while (pgnc > 0) {
		// copy wrap arround buffer
		uint8_t* wb = vba->v;
		wb[vba->scsz / 8] = wb[0];
		// BOOLEAN go through half-blocks
		for (int bi = 0; bi < vba->scsz / BPB * 2; ++bi) {
			UINT8* bya = vba->v;  // need to adress on byte-basis
			bya += bi * (BPB / 8 / 2);
			register FBPT l;
			l = *(FBPT*)bya;

			for (int tc = 0; tc < min(4, pgnc); ++tc) {
				// rule 147 - not(b xor (a and c)) - https://www.wolframalpha.com/input/?i=not%28b+xor+%28a+and+c%29%29
				//l = ~((l>>1) ^ (l & (l>>2)));
				// equivalent rule 54 - (p, q, r) -> q xor (p or r) - https://www.wolframalpha.com/input/?i=rule+54
				l = (l >> 1) ^ (l | (l >> 2));
			}
			*(HBPT*)bya = (HBPT)l;
		}
		pgnc -= min(4, pgnc);
	}

	return 0;
}

void CA_CNITFN_DISABLED(caBitArray* vba, CA_RULE* cr) {
}

int64_t CA_CNFN_DISABLED(int64_t pgnc, caBitArray* vba) {
	printf("CM_DISABLED  computation disabled!\n");
	return 0;
}

void CA_CNITFN_VBA_16x256(caBitArray* vba, CA_RULE* cr) {
	vba->lcpt = 0;
	vba->lwpt = 8;
	vba->mwpt = 3;
	vba->oc = 14;
	initCAVerticalBitArray(vba, cr);
}

void CA_CNITFN_OMP_TEST(caBitArray* vba, CA_RULE* cr) {
	vba->lcpt = 0;
	vba->lwpt = 8;
	vba->mwpt = 2;
	vba->bcpt = 3;
	vba->oc = 4;
	initCAVerticalBitArray(vba, cr);
	// temp TODO
	convertBetweenCACTandCABitArray(vba, 0);
	//	getch();
}

int64_t CA_CNFN_VBA_1x256(int64_t gnc, caBitArray* vba) {
	convertBetweenCACTandCABitArray(vba, 0);

	int oc = vba->oc;				// overflow-count
	int rc = vba->rc - oc;			// row-count without overlap

	while (gnc > 0) {
		gnc--;
		CABitArrayPrepareOL(vba);
		__m256i* vbac = (__m256i*)vba->v;
		register __m256i ymm0, ymm1, ymm2;
		for (int ri = 0; ri < rc; ++ri) {
			ymm0 = _mm256_load_si256(vbac + ri + 0);
			ymm1 = _mm256_load_si256(vbac + ri + 1);
			ymm2 = _mm256_load_si256(vbac + ri + 2);
			ymm0 = _mm256_or_si256(ymm0, ymm2);
			ymm0 = _mm256_xor_si256(ymm0, ymm1);
			_mm256_store_si256(vbac + ri, ymm0);
		}
	}

	return gnc;
}

int64_t CA_CNFN_OMP_TEST(int64_t gnc, caBitArray* vba) {
	omp_set_num_threads(1 << vba->bcpt);

	convertBetweenCACTandCABitArray(vba, 0);

	int oc = vba->oc;				// overflow-count
	int bh = vba->bh - oc;			// block-height without overlap

	int gwolp = oc / 2;				// generations that can be calculated without overlap prepare
	int gwolc = 0;					// generations without overlap prepare count

	BBT* v = vba->v;

	#pragma omp parallel default(none) shared(vba) firstprivate(v, bh, oc, gnc)
	//		for (int bkcr = 0; bkcr < 1 << vba->bcpt; bkcr++)
	{
		int bkcr = omp_get_thread_num();
//		printf("thread #%d\n", bkcr);
		while (gnc > 0) {
			gnc -= 2;
			//if (!gwolc || gwolc == gwolp) {
			//	gwolc = 0;
			//	CABitArrayPrepareOL(vba);
			//}
			//else
			//	gwolc += 2;
#pragma omp barrier

#pragma omp master
			CABitArrayPrepareOL(vba);

#pragma omp barrier

#pragma omp parallel
			{
				__m256i* vbac = (__m256i*)v + bkcr * (bh + oc);
				for (int ri = 0; ri < bh; ++ri) {
					//register __m256i ymm0 = _mm256_load_si256(vbac + ri + 0);
					//register __m256i ymm1 = _mm256_load_si256(vbac + ri + 1);
					//register __m256i ymm2 = _mm256_load_si256(vbac + ri + 2);

					//ymm0 = _mm256_or_si256(ymm0, ymm2);
					//ymm0 = _mm256_xor_si256(ymm0, ymm1);

					//_mm256_store_si256(vbac + ri, ymm0);
					// calc two gens
					register __m256i ymm0 = _mm256_load_si256(vbac + ri + 0);
					register __m256i ymm1 = _mm256_load_si256(vbac + ri + 1);
					register __m256i ymm2 = _mm256_load_si256(vbac + ri + 2);
					register __m256i ymm3 = _mm256_load_si256(vbac + ri + 3);
					register __m256i ymm4 = _mm256_load_si256(vbac + ri + 4);

					ymm0 = _mm256_or_si256(	ymm0, ymm2);
					ymm0 = _mm256_xor_si256(ymm0, ymm1);

					ymm1 = _mm256_or_si256(	ymm1, ymm3);
					ymm1 = _mm256_xor_si256(ymm1, ymm2);

					ymm2 = _mm256_or_si256(	ymm2, ymm4);
					ymm2 = _mm256_xor_si256(ymm2, ymm3);

					ymm0 = _mm256_or_si256(	ymm0, ymm2);
					ymm0 = _mm256_xor_si256(ymm0, ymm1);

					_mm256_store_si256(vbac + ri, ymm0);
				}
			}
		}
	}

	return 0;

	// calc two gens
	//register __m256i ymm0 = _mm256_load_si256(vbac + ri + 0);
	//register __m256i ymm1 = _mm256_load_si256(vbac + ri + 1);
	//register __m256i ymm2 = _mm256_load_si256(vbac + ri + 2);
	//register __m256i ymm3 = _mm256_load_si256(vbac + ri + 3);
	//register __m256i ymm4 = _mm256_load_si256(vbac + ri + 4);

	//ymm0 = _mm256_or_si256(	ymm0, ymm2);
	//ymm0 = _mm256_xor_si256(ymm0, ymm1);

	//ymm1 = _mm256_or_si256(	ymm1, ymm3);
	//ymm1 = _mm256_xor_si256(ymm1, ymm2);

	//ymm2 = _mm256_or_si256(	ymm2, ymm4);
	//ymm2 = _mm256_xor_si256(ymm2, ymm3);

	//ymm0 = _mm256_or_si256(	ymm0, ymm2);
	//ymm0 = _mm256_xor_si256(ymm0, ymm1);

	//_mm256_store_si256(vbac + ri, ymm0);

}

int64_t CA_CNFN_VBA_16x256(int64_t pgnc, caBitArray* vba) {
	// WORK IN PROGRESS
	/* Idea is to use all 16 AVX regsiters */
	convertBetweenCACTandCABitArray(vba, 0);
	while (pgnc > 0) {
		pgnc -= 4;
		CABitArrayPrepareOL(vba);

		register __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;
		register __m256i ymm8, ymm9;
		register __m256i ymm10, ymm11, ymm12, ymm13, ymm14, ymm15;

		for (int ri = 0; ri < vba->rc - vba->oc; ri += 8) {
			__m256i* vbac = (__m256i*)vba->v + ri;

			ymm0 = _mm256_load_si256(vbac + 0);
			ymm1 = _mm256_load_si256(vbac + 1);
			ymm2 = _mm256_load_si256(vbac + 2);
			ymm3 = _mm256_load_si256(vbac + 3);
			ymm4 = _mm256_load_si256(vbac + 4);
			ymm5 = _mm256_load_si256(vbac + 5);
			ymm6 = _mm256_load_si256(vbac + 6);
			ymm7 = _mm256_load_si256(vbac + 7);
			ymm8 = _mm256_load_si256(vbac + 8);
			ymm9 = _mm256_load_si256(vbac + 9);
			ymm10 = _mm256_load_si256(vbac + 10);
			ymm11 = _mm256_load_si256(vbac + 11);
			ymm12 = _mm256_load_si256(vbac + 12);
			ymm13 = _mm256_load_si256(vbac + 13);
			ymm14 = _mm256_load_si256(vbac + 14);
			ymm15 = _mm256_load_si256(vbac + 15);

			ymm0 = _mm256_or_si256(ymm0, ymm2);		 ymm0 = _mm256_xor_si256(ymm0, ymm1);
			ymm1 = _mm256_or_si256(ymm1, ymm3);		 ymm1 = _mm256_xor_si256(ymm1, ymm2);
			ymm2 = _mm256_or_si256(ymm2, ymm4);		 ymm2 = _mm256_xor_si256(ymm2, ymm3);
			ymm3 = _mm256_or_si256(ymm3, ymm5);		 ymm3 = _mm256_xor_si256(ymm3, ymm4);
			ymm4 = _mm256_or_si256(ymm4, ymm6);		 ymm4 = _mm256_xor_si256(ymm4, ymm5);
			ymm5 = _mm256_or_si256(ymm5, ymm7);		 ymm5 = _mm256_xor_si256(ymm5, ymm6);
			ymm6 = _mm256_or_si256(ymm6, ymm8);		 ymm6 = _mm256_xor_si256(ymm6, ymm7);
			ymm7 = _mm256_or_si256(ymm7, ymm9);		 ymm7 = _mm256_xor_si256(ymm7, ymm8);
			ymm8 = _mm256_or_si256(ymm8, ymm10);		 ymm8 = _mm256_xor_si256(ymm8, ymm9);
			ymm9 = _mm256_or_si256(ymm9, ymm11);		 ymm9 = _mm256_xor_si256(ymm9, ymm10);
			ymm10 = _mm256_or_si256(ymm10, ymm12);		ymm10 = _mm256_xor_si256(ymm10, ymm11);
			ymm11 = _mm256_or_si256(ymm11, ymm13);		ymm11 = _mm256_xor_si256(ymm11, ymm12);
			ymm12 = _mm256_or_si256(ymm12, ymm14);		ymm12 = _mm256_xor_si256(ymm12, ymm13);
			ymm13 = _mm256_or_si256(ymm13, ymm15);		ymm13 = _mm256_xor_si256(ymm13, ymm14);

			ymm0 = _mm256_or_si256(ymm0, ymm2);		 ymm0 = _mm256_xor_si256(ymm0, ymm1);
			ymm1 = _mm256_or_si256(ymm1, ymm3);		 ymm1 = _mm256_xor_si256(ymm1, ymm2);
			ymm2 = _mm256_or_si256(ymm2, ymm4);		 ymm2 = _mm256_xor_si256(ymm2, ymm3);
			ymm3 = _mm256_or_si256(ymm3, ymm5);		 ymm3 = _mm256_xor_si256(ymm3, ymm4);
			ymm4 = _mm256_or_si256(ymm4, ymm6);		 ymm4 = _mm256_xor_si256(ymm4, ymm5);
			ymm5 = _mm256_or_si256(ymm5, ymm7);		 ymm5 = _mm256_xor_si256(ymm5, ymm6);
			ymm6 = _mm256_or_si256(ymm6, ymm8);		 ymm6 = _mm256_xor_si256(ymm6, ymm7);
			ymm7 = _mm256_or_si256(ymm7, ymm9);		 ymm7 = _mm256_xor_si256(ymm7, ymm8);
			ymm8 = _mm256_or_si256(ymm8, ymm10);		 ymm8 = _mm256_xor_si256(ymm8, ymm9);
			ymm9 = _mm256_or_si256(ymm9, ymm11);		 ymm9 = _mm256_xor_si256(ymm9, ymm10);
			ymm10 = _mm256_or_si256(ymm10, ymm12);		ymm10 = _mm256_xor_si256(ymm10, ymm11);
			ymm11 = _mm256_or_si256(ymm11, ymm13);		ymm11 = _mm256_xor_si256(ymm11, ymm12);

			ymm0 = _mm256_or_si256(ymm0, ymm2);		 ymm0 = _mm256_xor_si256(ymm0, ymm1);
			ymm1 = _mm256_or_si256(ymm1, ymm3);		 ymm1 = _mm256_xor_si256(ymm1, ymm2);
			ymm2 = _mm256_or_si256(ymm2, ymm4);		 ymm2 = _mm256_xor_si256(ymm2, ymm3);
			ymm3 = _mm256_or_si256(ymm3, ymm5);		 ymm3 = _mm256_xor_si256(ymm3, ymm4);
			ymm4 = _mm256_or_si256(ymm4, ymm6);		 ymm4 = _mm256_xor_si256(ymm4, ymm5);
			ymm5 = _mm256_or_si256(ymm5, ymm7);		 ymm5 = _mm256_xor_si256(ymm5, ymm6);
			ymm6 = _mm256_or_si256(ymm6, ymm8);		 ymm6 = _mm256_xor_si256(ymm6, ymm7);
			ymm7 = _mm256_or_si256(ymm7, ymm9);		 ymm7 = _mm256_xor_si256(ymm7, ymm8);
			ymm8 = _mm256_or_si256(ymm8, ymm10);		 ymm8 = _mm256_xor_si256(ymm8, ymm9);
			ymm9 = _mm256_or_si256(ymm9, ymm11);		 ymm9 = _mm256_xor_si256(ymm9, ymm10);

			ymm0 = _mm256_or_si256(ymm0, ymm2);		 ymm0 = _mm256_xor_si256(ymm0, ymm1);
			ymm1 = _mm256_or_si256(ymm1, ymm3);		 ymm1 = _mm256_xor_si256(ymm1, ymm2);
			ymm2 = _mm256_or_si256(ymm2, ymm4);		 ymm2 = _mm256_xor_si256(ymm2, ymm3);
			ymm3 = _mm256_or_si256(ymm3, ymm5);		 ymm3 = _mm256_xor_si256(ymm3, ymm4);
			ymm4 = _mm256_or_si256(ymm4, ymm6);		 ymm4 = _mm256_xor_si256(ymm4, ymm5);
			ymm5 = _mm256_or_si256(ymm5, ymm7);		 ymm5 = _mm256_xor_si256(ymm5, ymm6);
			ymm6 = _mm256_or_si256(ymm6, ymm8);		 ymm6 = _mm256_xor_si256(ymm6, ymm7);
			ymm7 = _mm256_or_si256(ymm7, ymm9);		 ymm7 = _mm256_xor_si256(ymm7, ymm8);

			_mm256_store_si256(vbac + 0, ymm0);
			_mm256_store_si256(vbac + 1, ymm1);
			_mm256_store_si256(vbac + 2, ymm2);
			_mm256_store_si256(vbac + 3, ymm3);
			_mm256_store_si256(vbac + 4, ymm4);
			_mm256_store_si256(vbac + 5, ymm5);
			_mm256_store_si256(vbac + 6, ymm6);
			_mm256_store_si256(vbac + 7, ymm7);
		}
	}

	return 0;
}

void CA_CNITFN_VBA_2x256(caBitArray* vba, CA_RULE* cr) {
	vba->lcpt = 1;
	vba->lwpt = 8;
	vba->mwpt = 3;
	vba->oc = 2;
	initCAVerticalBitArray(vba, cr);
}

void CA_CNITFN_VBA_1x32(caBitArray* vba, CA_RULE* cr) {
	vba->lcpt = 0;
	vba->lwpt = 5;
	vba->mwpt = 6;
	initCAVerticalBitArray(vba, cr);
}

void CA_CNITFN_VBA_1x64(caBitArray* vba, CA_RULE* cr) {
	vba->lcpt = 0;
	vba->lwpt = 6;
	vba->mwpt = 5;
	initCAVerticalBitArray(vba, cr);
}

void CA_CNITFN_VBA_2x32(caBitArray* vba, CA_RULE* cr) {
	vba->lcpt = 1;
	vba->lwpt = 5;
	vba->mwpt = 5;
	initCAVerticalBitArray(vba, cr);
}

void CA_CNITFN_VBA_4x32(caBitArray* vba, CA_RULE* cr) {
	vba->lcpt = 2;
	vba->lwpt = 5;
	vba->mwpt = 4;
	initCAVerticalBitArray(vba, cr);
}

void CA_CNITFN_VBA_1x256(caBitArray* vba, CA_RULE* cr) {
	vba->lcpt = 0;
	vba->lwpt = 8;
	vba->mwpt = 2;
	vba->bcpt = 0;
	initCAVerticalBitArray(vba, cr);
// temp TODO
	convertBetweenCACTandCABitArray(vba, 0);
//	getch();
}

int64_t CA_CNFN_VBA_1x32(int64_t pgnc, caBitArray* vba) {
	convertBetweenCACTandCABitArray(vba, 0);
	while (pgnc > 0) {
		CABitArrayPrepareOL(vba);

		UINT32* vbac = (UINT32*)vba->v;
		for (int ri = 0; ri < vba->rc - vba->oc; ++ri) {
			vbac[ri] = vbac[ri + 1] ^ (vbac[ri] | vbac[ri + 2]);				// equivalent rule 54 - (p, q, r) -> q xor (p or r) - https://www.wolframalpha.com/input/?i=rule+54
		}

		pgnc--;
	}

	return 0;
}

int64_t CA_CNFN_VBA_2x32(int64_t pgnc, caBitArray* vba) {
	convertBetweenCACTandCABitArray(vba, 0);
	while (pgnc > 0) {
		CABitArrayPrepareOL(vba);

		UINT32* vbac = (UINT32*)vba->v;
		for (int ri = 0; ri < (vba->rc - vba->oc) * 2; ri += 2) {
			vbac[ri + 0] = vbac[ri + 2] ^ (vbac[ri + 0] | vbac[ri + 4]);			// equivalent rule 54 - (p, q, r) -> q xor (p or r) - https://www.wolframalpha.com/input/?i=rule+54
			vbac[ri + 1] = vbac[ri + 3] ^ (vbac[ri + 1] | vbac[ri + 5]);			// equivalent rule 54 - (p, q, r) -> q xor (p or r) - https://www.wolframalpha.com/input/?i=rule+54
		}

		pgnc--;
	}

	return 0;
}

int64_t CA_CNFN_VBA_4x32(int64_t pgnc, caBitArray* vba) {
	convertBetweenCACTandCABitArray(vba, 0);
	while (pgnc > 0) {
		CABitArrayPrepareOL(vba);

		UINT32* vbac = (UINT32*)vba->v;
		for (int ri = 0; ri < (vba->rc - vba->oc) * 4; ri += 4) {
			vbac[ri + 0] = vbac[ri + 4] ^ (vbac[ri + 0] | vbac[ri + 8]);			// equivalent rule 54 - (p, q, r) -> q xor (p or r) - https://www.wolframalpha.com/input/?i=rule+54
			vbac[ri + 1] = vbac[ri + 5] ^ (vbac[ri + 1] | vbac[ri + 9]);			// equivalent rule 54 - (p, q, r) -> q xor (p or r) - https://www.wolframalpha.com/input/?i=rule+54
			vbac[ri + 2] = vbac[ri + 6] ^ (vbac[ri + 2] | vbac[ri + 10]);			// equivalent rule 54 - (p, q, r) -> q xor (p or r) - https://www.wolframalpha.com/input/?i=rule+54
			vbac[ri + 3] = vbac[ri + 7] ^ (vbac[ri + 3] | vbac[ri + 11]);			// equivalent rule 54 - (p, q, r) -> q xor (p or r) - https://www.wolframalpha.com/input/?i=rule+54
		}

		pgnc--;
	}

	return 0;
}

int64_t CA_CNFN_VBA_1x64(int64_t pgnc, caBitArray* vba) {
	convertBetweenCACTandCABitArray(vba, 0);
	while (pgnc > 0) {
		CABitArrayPrepareOL(vba);

		UINT64* vbac = (UINT64*)vba->v;
		for (int ri = 0; ri < vba->rc - vba->oc; ++ri) {
			vbac[ri] = vbac[ri + 1] ^ (vbac[ri] | vbac[ri + 2]);			// equivalent rule 54 - (p, q, r) -> q xor (p or r) - https://www.wolframalpha.com/input/?i=rule+54
		}

		pgnc--;
	}

	return 0;
}

int64_t CA_CNFN_VBA_2x256(int64_t pgnc, caBitArray* vba) {
	convertBetweenCACTandCABitArray(vba, 0);
	while (pgnc > 0) {
		CABitArrayPrepareOL(vba);

		__m256i* vbac = (__m256i*)vba->v;
		for (int ri = 0; ri < (vba->rc - vba->oc) * 2; ri += 2) {
			// first lane
			__m256i ymm0 = _mm256_load_si256(vbac + ri + 0);
			__m256i ymm1 = _mm256_load_si256(vbac + ri + 2);
			__m256i ymm2 = _mm256_load_si256(vbac + ri + 4);
			ymm0 = _mm256_or_si256(ymm0, ymm2);
			ymm0 = _mm256_xor_si256(ymm0, ymm1);
			_mm256_store_si256(vbac + ri + 0, ymm0);
			// second lane
			__m256i ymm3 = _mm256_load_si256(vbac + ri + 1);
			__m256i ymm4 = _mm256_load_si256(vbac + ri + 3);
			__m256i ymm5 = _mm256_load_si256(vbac + ri + 5);
			ymm3 = _mm256_or_si256(ymm3, ymm5);
			ymm3 = _mm256_xor_si256(ymm3, ymm4);
			_mm256_store_si256(vbac + ri + 1, ymm3);
		}

		pgnc--;
	}

	return 0;
}

/************************* Hach Computation Mode *************************/

/* Hash-Cell*/
typedef UINT32 HCI;							// hash-cell-index-type
#define HCITPBYC 4							// hash-cell-index-type-byte-count
UINT32 HCISZPT = 20;						// hash-cell-index-size as power of two, i.e. 8 = index size of 256;
UINT32 HCMXSC = 8;							// hash-cell-maximum-seek-count
#define HCTMXLV	128							// hash-cell-table-max-level

#define HCTBSPT	2							// hash-cell-table-base-size as power of two -- MUST BE 2 ATM
#define HCTBS (1<<HCTBSPT)					// hash-cell-table-base-size / size of lowest/smallest level
//#define HCTBSMK ((1<<HCTBSPT) - 1)			// hash-cell-table-base-size-mask

#define HCIMASK (((UINT32)1<<HCISZPT)-1)	// hash-cell-index-mask /* for example: (u_int32_t)0xffff */
#define HCISZ ((UINT32)1<<HCISZPT)		// hash-cell-index-size
#define HCUCMK (((UINT32)1<<24) - 1)		// hash-cell-usage-count-mask (use only last 24 bits)
#define HCLLMK ((((UINT32)1<<8) - 1)<<24)		// hash-cell-level-mask (use only first 8 bits)
//#define HCINCUC(x) (x = (x & HCLLMK) | ((x & HCUCMK) + 1))	// increase hash-cell-usage-count
//#define HCDECUC(x) (x = (x & HCLLMK) | ((x & HCUCMK) - 1))	// decrease hash-cell-usage-count
/* Hash-Cells-Node */
typedef struct
HCN {
	HCI ln;									// left-node-index
	HCI rn;									// right-node-index
	HCI r;									// result-node-index
	UINT32 uc;								// usage-count (last 24 bits) and level (first 8 bits)
	//	UINT32 ucd;								// usage-count (displayed nodes only)
	//	UINT8 fs;								// flags: 0 = node is empty; &1 = node not empty; &2 = node marked as active
	//	UINT8 ll;								// level
} HCN;
HCI hcln[HCTMXLV] = { 0 };					// hash-cells-left-node (used when building hash-table)
int hcls[HCTMXLV] = { 0 };					// hash-cells-left-node-set boolean (used when building hash-table) - indicates if a left-node is present in hcln for the given level
HCN* hct = NULL;							// hash-cell-table
HCI hc_ens[128] = { 0 };					// hash-cell empty nodes

//#define HCISKSZ 256							// hash-cell-stack-size
//HCI hciskps = 0;							// hash-cell-stack-position
//HCI hcisk[HCISKSZ] = { 0 };					// hash-cell-stack

HCI hc_sn = 0;								// node holding current space
int hc_sl = 0;								// level of current space

typedef struct
HC_STATS {
	int nc;									// node-count
	int sc;									// search-count
	int fc;									// found-count
	int ss;									// seek-sum (can be used with sc to calculate average seek-time)
	int rcct;								// recycled-count
	int adct;								// added-count
} HC_STATS;
HC_STATS hc_stats[HCTMXLV] = { 0 };

void HC_print_stats() {
	// display stats
	memset(&hc_stats[HCTMXLV - 1], 0, sizeof(hc_stats[HCTMXLV - 1]));	// reset sums
	printf("-\n");
	printf(" ll  %8s %4s  %8s %7s  %8s  %8s  %8s %4s  %8s  ll\n",
		"nc", "%", "sc", "fc%", "rqct", "adct", "seeks", "avgsk", "size");
	static HC_STATS sl = { 0 };		// stats-last
	HC_STATS sd = { 0 }; // stats-delta
	for (int i = 0; i < HCTMXLV; ++i) {
		if (i && hc_stats[i].nc < 2 && !hc_stats[i].sc && i < HCTMXLV - 2)
			continue;
		if (i < HCTMXLV - 2) {
			hc_stats[HCTMXLV - 1].nc += hc_stats[i].nc;
			hc_stats[HCTMXLV - 1].sc += hc_stats[i].sc;
			hc_stats[HCTMXLV - 1].fc += hc_stats[i].fc;
			hc_stats[HCTMXLV - 1].rcct += hc_stats[i].rcct;
			hc_stats[HCTMXLV - 1].adct += hc_stats[i].adct;
			hc_stats[HCTMXLV - 1].ss += hc_stats[i].ss;
		}
		else if (i == HCTMXLV - 2) {
			sd.rcct = hc_stats[HCTMXLV - 1].rcct - sl.rcct;
			sd.adct = hc_stats[HCTMXLV - 1].adct - sl.adct;
			sd.nc = hc_stats[HCTMXLV - 1].nc - sl.nc;
			sd.sc = hc_stats[HCTMXLV - 1].sc - sl.sc;
			sd.fc = hc_stats[HCTMXLV - 1].fc - sl.fc;
			sd.ss = hc_stats[HCTMXLV - 1].ss - sl.ss;
			hc_stats[HCTMXLV - 2] = sd;
			sl = hc_stats[HCTMXLV - 1];
			printf("=== DELTA%91s\n", "");
		}
		else
			printf("=== SUM%93s\n", "");

		printf("%3d  %8.2e %3.0f%%  %8.2e %6.2f%%  %8.2e  %8.2e  %8.2e %5.2f  %8.2e  %2d\n",
			i,
			(double)hc_stats[i].nc,
			(100.0 / (double)HCISZ * (double)hc_stats[i].nc),
			(double)hc_stats[i].sc,
			(100.0 / max(1, abs(hc_stats[i].sc)) * abs(hc_stats[i].fc)),
			(double)hc_stats[i].rcct,
			(double)hc_stats[i].adct,
			(double)hc_stats[i].ss,
			(double)hc_stats[i].ss / max(1.0, abs((double)hc_stats[i].sc)),
			(double)((UINT64)HCTBS << i),
			i);
	}
	printf("---%97s\n", "");
	printf("sz  tl %ub / %.2fmb  n %u / %.2fb/n\n", HCISZ * sizeof(HCN), (float)HCISZ * sizeof(HCN) / 1024.0 / 1024.0, HCISZ, (float)sizeof(HCN));
	printf("max-seek-count %d%20s\n", HCMXSC, "");
	printf("found-count%% %f%%\n", (100.0 / max(1, abs(hc_stats[HCTMXLV - 1].sc)) * abs(hc_stats[HCTMXLV - 1].fc)));
	printf("-%99s\n", "");
	printf("%100s\n", "");
	printf("%100s\n", "");
}

//int HC_clear_uc_stats(int ll, HCI n) {
//	if (hct[n].ucd == ~0)
//		return 0;
//	int cc = 1;
//	hct[n].ucd = ~0;
//	if (ll) {
//		cc += HC_clear_uc_stats(ll - 1, hct[n].ln);
//		cc += HC_clear_uc_stats(ll - 1, hct[n].rn);
//	}
//	return cc;
//}
//
///* call clear_stats first! */
//int HC_update_uc_stats(int ll, HCI n) {
//	int cc = 1;
//	if (hct[n].ucd == ~0) {
//		hct[n].ucd = 1;
//		if (ll) {
//			cc += HC_update_uc_stats(ll - 1, hct[n].ln);
//			cc += HC_update_uc_stats(ll - 1, hct[n].rn);
//		}
//	}
//	else
//		hct[n].ucd++;
//
//	return cc;
//}

void HC_print_node(int ll, int mxll, HCI n) {
	char indent[HCTMXLV * 2 + 1];
	memset(indent, ' ', HCTMXLV * 2);
	indent[(mxll - ll) * 2] = 0;

	if (ll < 0) {
		printf("%s", indent);
		print_bits(&n, 1, 0);
	}
	else {
		printf("%s#%2d/%08X", indent, ll, n);
		printf("\n");
		HC_print_node(ll - 1, mxll, hct[n].ln);
		HC_print_node(ll - 1, mxll, hct[n].rn);
		HC_print_node(ll - 1, mxll, hct[n].r);
		//if (ll == 0) {
		//	printf("%04x %04x  %X  ", hct[n].ln<<8, hct[n].rn, (hct[n].ln<<8) | hct[n].rn);
		//	print_byte_as_bits(calt[0xFFFF & ((hct[n].ln<<8) | hct[n].rn)]);
		//	printf("\n");
		//}
	}
}

/*
ll		level
dh		(calling-)depth
ln		left node	- MUST have a positive uc (usage count), otherwise there is the possibility of it to be overwritten when searching for an empty node
rn		right node	- MUST have a positive uc (usage count), otherwise there is the possibility of it to be overwritten when searching for an empty node
rt		unless null, will be set to the result node of the found or added branch - guaranteed to have a uc (usage count) greater than 0
returns index of found or added branch - if newly created this will have a uc (usage count) of 0
*/
	int hcfl = 0; // hash-cell-table-full flag
	HCI HC_find_or_add_branch(UINT32 ll, HCI ln, HCI rn, HCI* rt) {
		HCI cm;										// checksum
		// base / lowest level > result is guaranteed to be present
		//if (!ll) {
		//	cm = ((ln<<2) | rn) + 1;
		//	if (rt)
		//		*rt = hct[cm].r;
		//	return cm;
		//}

		// Generate checksum.
		UINT32 cm32;
		cm32 = fnv_32_buf(&ln, HCITPBYC, FNV1_32_INIT);
		cm32 = fnv_32_buf(&rn, HCITPBYC, cm32);
		cm = ((cm32 >> HCISZPT) ^ cm32);						// Xor-fold 32bit checksum to fit size of hash-table (needed mask is applied in while-loop).

		// Search for checksum.
		int sc = 0;												// seek-count
		HCI encm = 0;											// empty-node-checksum
		UINT32 enll = 0;										// empty-node-level
		while (1) {
			++sc;
			cm &= HCIMASK;
			cm = max(1 + HCTBS, cm);							// index can never be 0 or one of the base nodes of level -1
			if (!ll) {											// nodes on level 0 are created when initializing the hash table and we only need to return the checksum here
// move this (checksum creation) to cnitfn_hash - so we can remove this block here
				hc_stats[0].fc++;
				break;
			}
			// Is the index occupied (ln != 0)?
			if (hct[cm].ln) {
				// Searched node found
				if (hct[cm].ln == ln && hct[cm].rn == rn) {
					hc_stats[ll].fc++;
					break;
				}
				else {
					UINT32 cnll = (hct[cm].uc & HCLLMK) >> 24;	// current-node-level
					// Unused node found at checksum-index > recycle (delete unused node); prefer to recycle nodes with a higher level
					if ((!encm || cnll > enll) && (hct[cm].uc & HCUCMK) == 0) {
						encm = cm;
						enll = cnll;
					}
				}
			}
			//
			if (encm && sc > HCMXSC) {
				cm = encm;
				int rcll = (hct[cm].uc & HCLLMK) >> 24;	// recycled-node-level
// TODO clean this up: rcll check and > HCTBS + 1 checks should be able to be removed without side effects
				if (rcll) {
					if (hct[cm].ln > HCTBS + 1)	// TODO it should be possible to do this without the check, when uc is set on 1 (pos. 0) and basic nodes
						hct[hct[cm].ln].uc--;
					if (hct[cm].rn > HCTBS + 1)	// see above
						hct[hct[cm].rn].uc--;
					if (hct[cm].r > HCTBS + 1)		// see above
						hct[hct[cm].r].uc--;
					hct[cm].ln = 0;
					hc_stats[rcll].nc--;
					hc_stats[rcll].rcct++;
				}
			}
			// Hashtable at checksum-index is empty > add new entry
			if (!hct[cm].ln) {
				hc_stats[ll].nc++;
				hc_stats[ll].adct++;
				// Add new branch / entry to hashtable
				hct[cm].uc = ((UINT32)ll << 24) | 1;
				hct[cm].ln = ln;
				hct[cm].rn = rn;
				hct[ln].uc++;
				hct[rn].uc++;
				// Query result node - first pass.
				HCI fm, fmr, sl, slr, sr, srr, tb;
				fm = HC_find_or_add_branch(ll - 1, hct[ln].rn, hct[rn].ln, &fmr); // middle-result
				hct[fm].uc++;
				// Second pass.
				sl = HC_find_or_add_branch(ll - 1, hct[ln].r, fmr, &slr);
				hct[sl].uc++;
				sr = HC_find_or_add_branch(ll - 1, fmr, hct[rn].r, &srr);
				hct[sr].uc++;
				// Third pass.
				tb = HC_find_or_add_branch(ll - 1, slr, srr, NULL);
				hct[cm].r = tb;
				hct[tb].uc++;
				hct[fm].uc--;
				hct[sl].uc--;
				hct[sr].uc--;
				hct[cm].uc &= HCLLMK;	// set usage-count to 0 but keep level
				break;
			}
			++cm;
			// hashtable size to big > abort
			if (hcfl || sc >= HCISZ >> 2) {
				if (!hcfl)
					printf("ERROR! Hash-table-size insufficient! ABORTING! Hash-cell-table is CORRUPT now!\n");
				hcfl = 1;
				cm = hc_ens[ll];		// return empty node as result
				break;
			}
		}
		// Update stats.
		hc_stats[ll].sc++;
		hc_stats[ll].ss += sc;
		//
		if (rt)
			*rt = hct[cm].r;

	return cm;
}


/* IN DEVELOPMENT! */
// try to make other speeds possible than 2^n*(space-size / 2)
/*
HCI HC_NG(int ll, int64_t* tm, HCI ln, HCI rn) {
	HCI rt;
	if (!tm) {
		rt = ln;
	}
	else if (tm == (int64_t)(HCTBS / 4)<<ll) {
		rt = HC_find_or_add_branch(ll, ln, rn, NULL);
		rt = hct[rt].r;
		tm = 0;
	}
	else if (tm > (int64_t)(HCTBS / 4)<<ll) {
		HCI nl;
		nl = HC_find_or_add_branch(ll, ln, rn, NULL);
		rt = HC_NG(ll + 1, &tm, nl, nl);
	}
	else if (tm < (int64_t)(HCTBS / 4)<<ll) {
		HCI nl, nr;
		nl = HC_NG(ll - 1, &tm, hct[ln].ln, hct[ln].rn);
		nr = HC_NG(ll - 1, &tm, hct[ln].rn, hct[rn].ln);
		rt = HC_find_or_add_branch(ll, nl, nr, NULL);
	}
}
*/

/*
ll		level
mxll*	max-level

returns last added (and therefore top-most) node

NOTE on reference count (uc):
If a left node is added (uncomplete branch) its uc is at least 1
If a right node is added (branch is complete) the uc of left and right is decremented by one, but a new left node one level up is added which has an uc of at least 1
> use hct[result_node].uc &= HCLLMK;  to reset uc for last returned node
*/
HCI HC_add_node(int ll, HCI n, int* mxll) {
#define D 0				// debug-mode
#if D
	char indent[HCTMXLV * 2 + 1];
	memset(indent, ' ', HCTMXLV * 2);
	indent[ll * 2] = 0;
	printf("%sN l %d  n 0x%04X  (press any key to continue)\n", indent, ll, n);
	getch();
#endif

	// Add Node to branch.
	if (hcls[ll] == 0) {
		// Branch is empty > add first node and wait for the seconde one
		hcln[ll] = n;
		if (ll)
			hct[n].uc++;
		hcls[ll] = 1;
		return n;
	}
	else {
		*mxll = ll;
		HCI ln = hcln[ll];							// left-node
		HCI rn = n;									// right-node
		if (ll)
			hct[n].uc++;
		HCI nn;										// new-node
		static HCI tn = 0;
		hcls[ll] = 0;
		// Branch of two nodes is complete.
		tn = HC_find_or_add_branch(ll, ln, rn, NULL);
		// Prepare node one level up
		nn = HC_add_node(ll + 1, tn, mxll);
		if (ll) hct[ln].uc--;
		if (ll) hct[rn].uc--;
		//hct[tn].uc--;
		return nn;
	}
}

/*
vba		vba->v needs to point to linear-bit-array as created by convert_CA_CT_to_bit_array
*ll		will receive the level of the result-node

return node-id of result-node - use together with *ll to access result node
*/
HCI convert_bit_array_to_hash_array(caBitArray* vba, int* ll) {
#define D 0			// debug-flag
	int bi = 0;		// 16-bit-index
	int ml = 0;		// max-level
	HCI ltnd = 0;	// last added node
	if (HCTBS == 4) {
		for (int sz = vba->scsz / 8 / 1; bi < sz; ++bi) {
			int l = 0;		// level
			UINT8* bya = vba->v;				// need to adress on byte-basis
			ltnd = HC_add_node(0, 1 + ((bya[bi] >> 0) & 0x3), &l);
			ltnd = HC_add_node(0, 1 + ((bya[bi] >> 2) & 0x3), &l);
			ltnd = HC_add_node(0, 1 + ((bya[bi] >> 4) & 0x3), &l);
			ltnd = HC_add_node(0, 1 + ((bya[bi] >> 6) & 0x3), &l);
			if (l > ml)
				ml = l;
		}
	}
	else if (HCTBS == 8) {
		for (int sz = vba->scsz / 8 / 1; bi < sz; ++bi) {
			int l = 0;		// level
			UINT8* bya = vba->v;				// need to adress on byte-basis
			ltnd = HC_add_node(0, bya[bi] & 0xF, &l);
			ltnd = HC_add_node(0, bya[bi] >> 4, &l);
			if (l > ml)
				ml = l;
		}
	}
	else if (HCTBS == 16) {
		for (int sz = vba->scsz / 8 / 1; bi < sz; ++bi) {
			int l = 0;		// level
			UINT8* bya = vba->v;				// need to adress on byte-basis
			ltnd = HC_add_node(0, bya[bi], &l);
			if (l > ml)
				ml = l;
		}
	}

	*ll = ml;
	return ltnd;
}

/*
ll		level in hash-table
n		node-id of node to convert in level ll of hash-table
clv		cell-space first valid element
cli		cell-space first invalid element
ctsz	current size/position in clsc
mxsz	max size of clsc

return next free adress of cell-space
> if count of cells in hash-table is the same or more than cells in cell-space, cli is returned
*/
CA_CT* convert_hash_array_to_CA_CT(int ll, HCI n, CA_CT* clv, CA_CT* cli) {
	if (cli - clv < HCTBS / 2)
		return cli;

	if (ll < 0) {
		for (int i = 0; i < HCTBS / 2; i++) {
			*clv = (n - 1) & (1 << i) ? 1 : 0;
			++clv;
		}

		return clv;
	}
	else {
		clv = convert_hash_array_to_CA_CT(ll - 1, hct[n].ln, clv, cli);
		clv = convert_hash_array_to_CA_CT(ll - 1, hct[n].rn, clv, cli);
	}
	return clv;
}
/*
ll		level in hash-table
n		node-id of node to convert in level ll of hash-table
pbv		pixel-buffer first valid element
pbi		pixel-buffer first invalid element
mf		multiplication-factor
mfa		mf-acceleration, i.e. the factor of which mf is increased in next level

return next free adress of cell-space
> if count of cells in hash-table is the same or more than cells in cell-space, cli is returned
*/
///UINT32* display_hash_array(float* pbv, float* pbf, float* pbi, float v, int ll, HCI n, float* mxv, float* mnv) {
UINT32* old_display_hash_array(UINT32* pbv, UINT32* pbf, UINT32* pbi, UINT32 v, int ll, HCI n, UINT32* mxv, UINT32* mnv) {
	if (ds.hddh >= 0) {
		if (ll >= ds.hddh + ds.hdll) {
			//v *= max(1, (hct[n].uc & HCUCMK));
			v += (hct[n].uc & HCUCMK);
			//v = fnv_32_buf(&n, 4, v);
		}
	}
	else {
		if ((ds.hddh + 1) * -1 + ds.hdll == ll) {
			//v *= max(1, hct[n].ucd);
			v += hct[n].uc & HCUCMK;
		}
	}

	if (ll <= ds.hdll) {
		///v = fnv_32_buf(&n, 4, v);
		///v = fnv_32_buf(&n, 4, FNV1_32_INIT);
		if (v > *mxv || *mxv == 0)
			*mxv = v;
		if (v < *mnv || *mnv == 0)
			*mnv = v;

		*pbf = v;
		++pbf;
		if (pbf >= pbi)
			return pbi - 1;
	}
	else {
		pbf = old_display_hash_array(pbv, pbf, pbi, v, ll - 1, hct[n].ln, mxv, mnv);
		pbf = old_display_hash_array(pbv, pbf, pbi, v, ll - 1, hct[n].rn, mxv, mnv);
	}

	return pbf;
}

//__declspec(noinline)
UINT32*
display_hash_array(UINT32* pbv, UINT32* pbf, UINT32* pbi, int pll, HCI n, UINT32* pmxv, UINT32* pmnv) {
//	printf("xxx");
	int hpn[HCTMXLV] = { 0 };		// heap-node
	char hplnd[HCTMXLV] = { 0 };		// heap-left-node-done
//	double hpv[HCTMXLV] = { 0 };		// heap-value
	UINT32 hpv[HCTMXLV] = { 0 };		// heap-value
//	double v = 1;
	UINT32 v = 1;
	UINT32 ll = pll;

	int hddh = ds.hddh;
	int hdll = ds.hdll;

	UINT32 mxv; UINT32 mnv;
	mxv = 0;
	mnv = MAXUINT32;
	//printf("%d %d, ", ll, pll);
	while (ll <= pll) {
		if (hddh >= 0) {
			if (ll >= hddh + hdll) {
//				v *= max(1, (hct[n].uc & HCUCMK));
				v += hct[n].uc & HCUCMK;
			}
		}
		else {
			if (ll == hddh * -1 - 1 + hdll)
				v += hct[n].uc& HCUCMK;
		}

		if (ll <= hdll) {
//			v = log2(v) / 256.0 * (double)MAXUINT32;
			if (v > mxv)
				mxv = v;
			if (v < mnv)
				mnv = v;

			*pbf = v;
			++pbf;
			if (pbf >= pbi) {
				pbf = pbi - 1;
				break;
			}

			//
			ll++;
			while (!hplnd[ll])
				ll++;
			n = hpn[ll];
			v = hpv[ll];
		}


		if (!hplnd[ll]) {
			hplnd[ll] = 1;
			hpn[ll] = n;
			hpv[ll] = v;
			n = hct[n].ln;
			ll--;
		}
		else {
			hplnd[ll] = 0;
			n = hct[n].rn;
			ll--;
		}

	}

	*pmnv = mnv;
	*pmxv = mxv;
	return pbf;
}

int64_t CA_CNFN_HASH(int64_t pgnc, caBitArray* vba) {
	for (int i = 0; i < HCTMXLV - 2; i++) {
		hc_stats[i].fc = 0;
		hc_stats[i].rcct = 0;
		hc_stats[i].adct = 0;
		hc_stats[i].sc = 0;
		hc_stats[i].ss = 0;
	}

	HCI rtnd = NULL;		// result node
	///	while (pgnc >= (int64_t)(HCTBS / 2)<<hc_sl) {
	if (sdmrpt) {
		// Scale up space/size to allow calculate needed time
///		int ul = 0;
///		while (pgnc >= ((int64_t)(HCTBS / 2)<<hc_sl)<<ul && ul < 63) {
///			ul++;
///		}
///		for (int l = 0; l < ul; ++l) {
		for (int l = 0; l < sdmrpt; ++l) {
			hc_sl++;
			hct[hc_sn].uc++;
			hcfl = 0;
			HCI hn = HC_find_or_add_branch(hc_sl, hc_sn, hc_sn, &rtnd);
			if (hcfl) {
// TODO reset / rebuild hash table like in cnitfn_hash!!!
				SIMFW_SetFlushMsg(&sfw, "ERROR! Hash-table-size insufficient! ABORTING! Hash-cell-table is CORRUPT now!\n");
			}
			hct[hc_sn].uc--;
			hc_sn = hn;
		}
		//		hc_sn = hct[hc_sn].r;
		hc_sn = rtnd;
		hc_sl--;
		///		for (int l = 1; l < ul; ++l) {
		for (int l = 1; l < sdmrpt; ++l) {
			hc_sn = hct[hc_sn].ln;					// dont use result node here as it is later in time!
			hc_sl--;
		}
		///		break;
	}

	//
	if (ds.fsme < 2 || ds.tm == 2)
		convert_hash_array_to_CA_CT(hc_sl, hc_sn, vba->clsc, vba->clsc + vba->scsz);

	return 0;
}

void CA_SCFN_HASH(caBitArray* vba) {
	convert_hash_array_to_CA_CT(hc_sl, hc_sn, vba->clsc, vba->clsc + vba->scsz);
}

void CA_CNITFN_HASH(caBitArray* vba, CA_RULE* cr) {
	// Copied from CA_CNITFN_LUT(vba, cr);
	_aligned_free(vba->v);
	vba->ct = (vba->scsz + BPB - 1) / BPB * BPB + BPB;		// bit-array-count (rounded up / ceil) // allow one extra-element since when we process the last half block we read one half block past the end
	vba->v = _aligned_malloc(vba->ct / 8, BYAL);

	_aligned_free(hct);
	hct = _aligned_malloc(HCISZ * sizeof(HCN), 256);
	if (!hct) {
		SIMFW_Die("malloc failed at %s (:%d)\n", "__FUNCSIG__", __LINE__);		// TODO - check if funcsig works with gcc
		return;
	}

	// reset
	memset(hct, 0, HCISZ * sizeof(HCN));
	memset(&hcln, 0, sizeof(hcln));
	memset(&hcls, 0, sizeof(hcls));
	memset(&hc_stats, 0, sizeof(hc_stats));

	// Precalculate all states of a space of 16 cells after 4 time steps.
	// Ruleset: 2 states, 3 cells in a neighborhod
	// After 4 time steps 8 cells (1 byte in binary representation) will remain

	//if (HCTBS == 16) {
	//	UINT16 i = 0;
	//	while (1) {
	//		UINT16 cs = i;	// cell-space
	//		// get result byte
	//		for (int tm = 0; tm < HCTBS / 4; ++tm) {
	//			for (int p = 0; p < 14; ++p) {
	//				// get result bit
	//				int sr =
	//					cr->mntl[0] * (1 & (cs >> (p + 0))) +
	//					cr->mntl[1] * (1 & (cs >> (p + 1))) +
	//					cr->mntl[2] * (1 & (cs >> (p + 2)));
	//				// set result bit in cs
	//				if (cr->rltl[sr] == 0)
	//					cs &= ((UINT16)~((UINT16)1 << p));
	//				else
	//					cs |= (UINT16)1 << p;
	//			}
	//		}
	//		// store result byte
	//		HCI nn;
	//		nn = HC_find_or_add_branch(0, i & 0xFF, i >> 8, NULL);
	//		hct[nn].r = cs & ((1 << (HCTBS / 2)) - 1);
	//		//
	//		if (i == MAXUINT16)
	//			break;
	//		++i;
	//	}
	//}
	//else if (HCTBS == 8) {
	//	UINT8 i = 0;
	//	while (1) {
	//		UINT8 cs = i;	// cell-space
	//		// get result byte
	//		for (int tm = 0; tm < 2; ++tm) {
	//			for (int p = 0; p < 6; ++p) {
	//				// get result bit
	//				int sr =
	//					cr->mntl[0] * (1 & (cs >> (p + 0))) +
	//					cr->mntl[1] * (1 & (cs >> (p + 1))) +
	//					cr->mntl[2] * (1 & (cs >> (p + 2)));
	//				// set result bit in cs
	//				if (cr->rltl[sr] == 0)
	//					cs &= ((UINT8)~((UINT8)1 << p));
	//				else
	//					cs |= (UINT8)1 << p;
	//			}
	//		}
	//		// store result byte
	//		HCI nn;
	//		nn = HC_find_or_add_branch(0, i & 0xF, i >> 4, NULL);
	//		hct[nn].r = cs & 0xF;
	//		//
	//		if (i == MAXUINT8)
	//			break;
	//		++i;
	//	}
	//}
	if (HCTBS == 4) {
		UINT8 i = 0;
		while (1) {
			UINT8 cs = i;	// cell-space
			// get result byte
			for (int p = 0; p < 2; ++p) {
				// get result bit
				int sr =
					cr->mntl[0] * (1 & (cs >> (p + 0))) +
					cr->mntl[1] * (1 & (cs >> (p + 1))) +
					cr->mntl[2] * (1 & (cs >> (p + 2)));
				// set result bit in cs
				if (cr->rltl[sr] == 0)
					cs &= ((UINT8)~((UINT8)1 << p));
				else
					cs |= (UINT8)1 << p;
			}
			// store result byte
			HCI nn, ln, rn, rt;
			ln = 1 + ((i >> 0) & 0x3);
			rn = 1 + ((i >> 2) & 0x3);
			rt = 1 + (cs & 0x3);

			nn = HC_find_or_add_branch(0, ln, rn, NULL);
			//nn = (ln<<2 | rn) + 1;
//nn = i);
//hct[nn].ll = 0;
///hct[nn].fs = 1;
			hct[nn].uc = 1;		// level is 0
			hct[nn].ln = ln;
			hct[nn].rn = rn;
			hct[nn].r = rt;
			hc_stats[0].nc++;

			printf("base node  %d  cm %8X  ln %8X  rn %8X  rt %8X\n", i, nn, ln, rn, rt);
			//
			if (i == 0xF)
				break;
			++i;
		}
	}

	//	print_bits(&i, 2, 0);
	//	print_bits(&cs, 2, 0);

		//print_bits(&hct[nn].ln, 4, 4);
		//print_bits(&hct[nn].rn, 4, 4);
		//print_bits(&hct[nn].r, 4, 4);
		//print_bits(&hct[nn].fs, 1, 4);
		//print_bits(&hct[nn].ll, 1, 4);
			//	print_bits(&tt, 4, 4);
	//	print_bits(&hct[nn].r, 4, 4);
	//	printf("\n");
		// inc / finish


	HC_print_stats(0);
	printf("hash init 1  %d\n", log2fix(vba->scsz, 1));
	//getch();
	// build empty nodes with the same size as current ca
	HCI en = HC_find_or_add_branch(0, 1, 1, NULL);
	hc_ens[0] = en;
	for (int i = 1; i <= 126; i++) {
		hct[en].uc++;
		HCI tn = HC_find_or_add_branch(i, en, en, NULL);

		hc_ens[i] = tn;
		hct[tn].uc++;

		hct[en].uc--;
		en = tn;
	}
	//
	convert_CA_CT_to_bit_array(vba->clsc, (FBPT*)vba->v, vba->scsz);
	printf("hash init 2\n");
	hc_sn = convert_bit_array_to_hash_array(vba, &hc_sl);
	printf("hash init 3  ll %d  hc_sn %04x  uc %d  uc-ll %d\n", hc_sl, hc_sn, hct[hc_sn].uc & HCUCMK, hct[hc_sn].uc & HCLLMK);
	if (hc_sl)
		hct[hc_sn].uc &= HCLLMK;		// set usage-count to zero but keep value of level
	HC_print_stats(0);

	//print_bits(vba->v, vba->scsz / 8, 4);
	if (0 && hc_sl <= 5) {
		HC_print_node(hc_sl, 20, hc_sn);
		printf("press any key to continue.\n");
		getch();
	}
	//CA_PrintSpace("", vba->clsc, vba->scsz);
	//convert_hash_array_to_CA_CT(hc_sl, hc_sn, vba->clsc, vba->clsc + vba->scsz);
	//print_bits(vba->v, vba->scsz / 8, 4);
	//CA_PrintSpace("", vba->clsc, vba->scsz);
	//HC_print_stats(0);
	//printf("press any key\n");  getch();
}

/************************* Routines operating directly on (byte-based) cell-space *************************/

/* portable way to use visual studio like union acessors in gcc */
typedef union __m256i_u8 {
    __m256i m256i;
    UINT8   m256i_u8[32];
} __m256i_u8;

/* Calculate next generation - CAN ONLY HANDLE TOTALISTIC ATM, ncn fixed */
void
ca_next_gen_ncn3_simd(
	CA_RULE* cr,										// cellular-automaton rule
	CA_CT* clv,											// cell-line first valid element
	CA_CT* cli											// cell-line first invalid element
) {
	while (clv < cli) {
		__m256i_u8 ymm0;
		ymm0.m256i = _mm256_loadu_si256(clv);
		ymm0.m256i = _mm256_adds_epu8(ymm0.m256i, *(__m256i*)(clv + 1));
		ymm0.m256i = _mm256_adds_epu8(ymm0.m256i, *(__m256i*)(clv + 2));
		for (int i = 0; i < 32; i += 4)
			ymm0.m256i_u8[i + 0] = cr->rltl[ymm0.m256i_u8[i + 0]],
			ymm0.m256i_u8[i + 1] = cr->rltl[ymm0.m256i_u8[i + 1]],
			ymm0.m256i_u8[i + 2] = cr->rltl[ymm0.m256i_u8[i + 2]],
			ymm0.m256i_u8[i + 3] = cr->rltl[ymm0.m256i_u8[i + 3]];
		_mm256_storeu_si256(clv, ymm0.m256i);
		clv += 32;
	}
}

/* Calculate next generation - ford ONLY HANDLE absolute, ncn and ncs fixed */
void
ca_next_gen_abs_ncn3_ncs2_simd(
	CA_RULE* cr,										// cellular-automaton rule
	CA_CT* clv,											// cell-line first valid element
	CA_CT* cli											// cell-line first invalid element
) {
	while (clv < cli) {
		__m256i_u8 ymm0;
		ymm0.m256i = _mm256_loadu_si256(clv);
		ymm0.m256i = _mm256_slli_epi64(ymm0.m256i, 1);					// shift one bit to the left
		ymm0.m256i = _mm256_adds_epu8(ymm0.m256i, *(__m256i*)(clv + 1));
		ymm0.m256i = _mm256_slli_epi64(ymm0.m256i, 1);					// shift one bit to the left
		ymm0.m256i = _mm256_adds_epu8(ymm0.m256i, *(__m256i*)(clv + 2));

		for (int i = 0; i < 32; i += 4)
			ymm0.m256i_u8[i + 0] = cr->rltl[ymm0.m256i_u8[i + 0]],
			ymm0.m256i_u8[i + 1] = cr->rltl[ymm0.m256i_u8[i + 1]],
			ymm0.m256i_u8[i + 2] = cr->rltl[ymm0.m256i_u8[i + 2]],
			ymm0.m256i_u8[i + 3] = cr->rltl[ymm0.m256i_u8[i + 3]];
		_mm256_storeu_si256(clv, ymm0.m256i);
		clv += 32;
	}
}


/* Calculate next generation - flexible variant - can handle totalistic and absolute rules by use of multiplication table */
void
ca_next_gen__sisd(
	CA_RULE* cr,										// cellular-automaton rule
	CA_CT* clv,											// cell-line first valid element
	CA_CT* cli											// cell-line first invalid element
) {
	while (clv < cli) {
		*clv = cr->mntl[0] * *clv;
		for (int i = 1; i < cr->ncn; ++i)
			*clv += cr->mntl[i] * clv[i];
		*clv = cr->rltl[*clv];
		++clv;
	}
}

/* Calculate next generation - ncn fixed, can handle TOT and ABS */
void
ca_next_gen_ncn3_sisd(
	CA_RULE* cr,										// cellular-automaton rule
	CA_CT* clv,											// cell-line first valid element
	CA_CT* cli											// cell-line first invalid element
) {
	while (clv < cli) {
		*clv = cr->rltl[cr->mntl[0] * clv[0] + cr->mntl[1] * clv[1] + cr->mntl[2] * clv[2]];
		++clv;
	}
}

/* Calculate next generation - CAN ONLY HANDLE TOTALISTIC ATM   flexible variant - can handle totalistic and absolute rules by use of multiplication table */
void
ca_count__simd(
	const CA_CT* csv,									// cell-space first valid element
	const CA_CT* csf,									// cell-space element to start with
	const CA_CT* csi,									// cell-space first invalid element
	const UINT32* pbv,									// pixel-buffer first valid element
	const UINT32* pbi,									// pixel-buffer first invalid element
	const int hlzm,										// horizontal zoom
	const int hlfs,										// horizontal focus
	const int fsme,										// focus-mode - 0 = totalistic, 1 = absolute
	const int ncs										// number of cell-states
) {
	register UINT32* pbc = pbv;							// pixel-buffer cursor / current element
	register CA_CT* csc = csf;							// cell-space cursor / current element
	register CA_CT* csk = csc;							// cell-space check position - initialised to equal csc in order to trigger recalculation of csk in count_check
	__m256i_u8 ymm0;						    		// used by simd operations

	////	register __m256i ymm0;	// running sum
	////	int mli = 32; // simd loop iterator
	////
	////count_simd:;
	////	// init running sum
	////	mm256_zeroall();
	////	for (int i = 0; i < hlfs0; ++i)
	////		ymm0 = _mm256_adds_epu8(ymm0, *(__m256i*)(csc + i));
	////count_simd_loop:;
	////	if (csi - csc < 32)
	////		goto count_sisd;
	////	if (csc >= csk)
	////		goto count_check;
	////	//
	////	ymm0 = _mm256_adds_epu8(ymm0, *(__m256i*)(csc + hlfs0));
	////	//
	////	register int i = mli;
	////	for (i = i - 32; i < 32; i += hlzm) {
	////		*pbc += ymm0.m256i_u8[i];
	////		++pbc;
	////	}
	////	csc += 32 / hlzm;
	////	ymm0 = _mm256_adds_epu8(ymm0, *(__m256i*)(csc + hlfs0));
	////
	////	goto count_simd_loop;
	////count_sisd:

	// draws cell-space to fill pixel-buffer completely - if cell-space * hlzm is smaller than the pixel-buffer it will be drawn repeatedly
	register int i;
count_simd:;
	if (fsme)
		goto count_sisd;
	if (csk - csc < 32)							// single stepping may always be necesary as pixel-buffer may be of any size
		goto count_sisd;
	if (csc >= csk)
		goto count_check;
	ymm0.m256i = _mm256_loadu_si256(csc);
	for (i = 1; i < hlfs; ++i)
		ymm0.m256i = _mm256_adds_epu8(ymm0.m256i, *(__m256i*)(csc + i));
	for (i = 0; i < 32; i += hlzm) {
		*pbc = ymm0.m256i_u8[i];
		++pbc;
		csc += hlzm;
	}
	goto count_simd;
count_sisd:;
	if (csc >= csk)
		goto count_check;
	*pbc += *csc;
	for (i = 1; i < hlfs; ++i)
		if (!fsme)
			*pbc += csc[i];
		else
			*pbc += ipow(ncs, i) * csc[i];
	++pbc;
	csc += hlzm;
	goto count_sisd;
count_check:;
	csk = csc + hlzm * (pbi - pbc);
	if (csc >= csk)
		goto count_finished;
	else if (csc >= csi)
		csc -= csi - csv,						// wrap arround cursor position at end of cell-space
		csk = csc;								// force immediate recalculation of csk - relative to wrapp-arround-corrected cursor position - at the next loop cycle - this is to avoid repeating the calculation of csk above
	else if (csi < csk)
		csk = csi;
	goto count_simd;
count_finished:;
}
//
///* Calculate next generation - flexible variant - can handle totalistic and absolute rules by use of multiplication table */
//void
//ca_next_gen(
//	const CA_CT *clf,		// cell-line first element
//	const CA_CT *clb,		// cl beginning of line
//	const CA_CT *clwb,		// cl wrap buffer
//	const CA_CT *cljb,		// cl joint buffer
//	const int ncn,			// number of cells in a neighborhod
//	const CA_CT *rltl,		// rule-table
//	const CA_CT *mntl)		// multiplication-table
//{
//	/* Copy rule and multiplication table to local memory in hope to improve speed */
//	//CA_CT rltl[LV_MAXRLTLSZ];
//	//CA_CT mntl[LV_MAXRLTLSZ];
//	//for (int i = 0; i < cr->nns; i++) {
//	//	rltl[i] = cr->rltl[i];
//	//	mntl[i] = cr->mntl[i];
//	//}
//
//	register CA_CT *clc = clb;	// cell-line cursor / current element
//	register CA_CT *clck;		// cl check position - which is either the joint-buffer or the wrap-buffer
//	if (cljb > clc)
//		clck = cljb;
//	else
//		clck = clwb;
//
//	while (1) {
//		if (clc == clck)
//			if (clc == clwb) {
//				clc = clf;
//				clck = cljb;
//				continue;
//			}
//			else
//				break;
//		if (ncn == 3)
//			*clc = rltl[mntl[0] * clc[0] + mntl[1] * clc[1] + mntl[2] * clc[2]];
//		else {
//			*clc = mntl[0] * *clc;
//			for (int i = 1; i < ncn; ++i)
//				*clc += mntl[i] * clc[i];
//			*clc = rltl[*clc];
//		}
//		++clc;
//	}
//}

/* Calculate next generation - CAN ONLY HANDLE TOTALISTIC ATM  */
void
ca_next_gen__simd(
	CA_RULE* cr,	// cellular-automata rule
	CA_CT* clv,		// cell-line first valid element
	CA_CT* cli		// cell-line first invalid element
) {
	while (clv < cli) {
		__m256i_u8 ymm0;
		ymm0.m256i = _mm256_loadu_si256(clv);
		for (int i = 1; i < cr->ncn; ++i)
			ymm0.m256i = _mm256_adds_epu8(ymm0.m256i, *(__m256i*)(clv + i));
		for (int i = 0; i < 32; i += 4)
			ymm0.m256i_u8[i + 0] = cr->rltl[ymm0.m256i_u8[i + 0]],
			ymm0.m256i_u8[i + 1] = cr->rltl[ymm0.m256i_u8[i + 1]],
			ymm0.m256i_u8[i + 2] = cr->rltl[ymm0.m256i_u8[i + 2]],
			ymm0.m256i_u8[i + 3] = cr->rltl[ymm0.m256i_u8[i + 3]];
		_mm256_storeu_si256(clv, ymm0.m256i);
		clv += 32;
	}
}

int64_t CA_CNFN_SIMD(int64_t pgnc, caBitArray* vba) {
	while (pgnc > 0) {
		pgnc--;
		memcpy(vba->clsc + vba->scsz, vba->clsc, vba->brsz * sizeof * vba->clsc);

		if (vba->cr->tp == TOT)
			if (vba->cr->ncn == 3)
				ca_next_gen_ncn3_simd(vba->cr, vba->clsc, vba->clsc + vba->scsz);
			else
				ca_next_gen__simd(vba->cr, vba->clsc, vba->clsc + vba->scsz);
		else
			if (vba->cr->ncn == 3)
				if (vba->cr->ncs == 2)
					ca_next_gen_abs_ncn3_ncs2_simd(vba->cr, vba->clsc, vba->clsc + vba->scsz);
				else
					printf("ERROR ruleset not supported in this computing-mode\n");
	}
	return 0;
}

int64_t CA_CNFN_SISD(int64_t pgnc, caBitArray* vba) {
	while (pgnc > 0) {
		pgnc--;
		memcpy(vba->clsc + vba->scsz, vba->clsc, vba->brsz * sizeof * vba->clsc);

		if (vba->cr->ncn == 3)
			ca_next_gen_ncn3_sisd(vba->cr, vba->clsc, vba->clsc + vba->scsz);
		else
			ca_next_gen__sisd(vba->cr, vba->clsc, vba->clsc + vba->scsz);
	}
	return 0;
}

/************************* OpenCL *************************/
#if ENABLE_CL
static OCLCA oclca = { -1, NULL, NULL, NULL };
#endif
void CA_CNITFN_OPENCL(caBitArray* vba, CA_RULE* cr) {
#if ENABLE_CL
	if (oclca.success != CL_SUCCESS)
		oclca = OCLCA_Init("../opencl-vba.cl");
#else
	printf("OpenCL support is not available in this build!\n");
#endif

	//
	vba->lcpt = 8;
	vba->lwpt = 2 + 4;
	vba->mwpt = 2;
	initCAVerticalBitArray(vba, cr);
}

#if ENABLE_CL
// **********************************************************************************************************************************
// OpenCL
// *********************************************************************************************************************************
// OCL Declarations *****************************************************************************************************************
// **********************************************************************************************************************************
#define OCL_CHECK_ERROR(error) { \
		if ((error) != CL_SUCCESS) fprintf (stderr, "OpenCL error <%s:%i>: %s\n", __FILE__, __LINE__, ocl_strerr((error))); }

static const char* opencl_error_msgs[] = {
	"CL_SUCCESS",
	"CL_DEVICE_NOT_FOUND",
	"CL_DEVICE_NOT_AVAILABLE",
	"CL_COMPILER_NOT_AVAILABLE",
	"CL_MEM_OBJECT_ALLOCATION_FAILURE",
	"CL_OUT_OF_RESOURCES",
	"CL_OUT_OF_HOST_MEMORY",
	"CL_PROFILING_INFO_NOT_AVAILABLE",
	"CL_MEM_COPY_OVERLAP",
	"CL_IMAGE_FORMAT_MISMATCH",
	"CL_IMAGE_FORMAT_NOT_SUPPORTED",
	"CL_BUILD_PROGRAM_FAILURE",
	"CL_MAP_FAILURE",
	"CL_MISALIGNED_SUB_BUFFER_OFFSET",
	"CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST",
	/* next IDs start at 30! */
	"CL_INVALID_VALUE",
	"CL_INVALID_DEVICE_TYPE",
	"CL_INVALID_PLATFORM",
	"CL_INVALID_DEVICE",
	"CL_INVALID_CONTEXT",
	"CL_INVALID_QUEUE_PROPERTIES",
	"CL_INVALID_COMMAND_QUEUE",
	"CL_INVALID_HOST_PTR",
	"CL_INVALID_MEM_OBJECT",
	"CL_INVALID_IMAGE_FORMAT_DESCRIPTOR",
	"CL_INVALID_IMAGE_SIZE",
	"CL_INVALID_SAMPLER",
	"CL_INVALID_BINARY",
	"CL_INVALID_BUILD_OPTIONS",
	"CL_INVALID_PROGRAM",
	"CL_INVALID_PROGRAM_EXECUTABLE",
	"CL_INVALID_KERNEL_NAME",
	"CL_INVALID_KERNEL_DEFINITION",
	"CL_INVALID_KERNEL",
	"CL_INVALID_ARG_INDEX",
	"CL_INVALID_ARG_VALUE",
	"CL_INVALID_ARG_SIZE",
	"CL_INVALID_KERNEL_ARGS",
	"CL_INVALID_WORK_DIMENSION",
	"CL_INVALID_WORK_GROUP_SIZE",
	"CL_INVALID_WORK_ITEM_SIZE",
	"CL_INVALID_GLOBAL_OFFSET",
	"CL_INVALID_EVENT_WAIT_LIST",
	"CL_INVALID_EVENT",
	"CL_INVALID_OPERATION",
	"CL_INVALID_GL_OBJECT",
	"CL_INVALID_BUFFER_SIZE",
	"CL_INVALID_MIP_LEVEL",
	"CL_INVALID_GLOBAL_WORK_SIZE"
};
const char*
ocl_strerr(int error)
{
	int index = 0;

	if (error >= -14)
		index = -error;
	else if (error <= -30 && error >= -64)
		index = -error - 15;

	return opencl_error_msgs[index];
}


typedef struct OCLCA {
	cl_int				success;
	cl_context			context;
	cl_command_queue	command_queue;
	cl_kernel			kernel_ca_next_gen;
	cl_kernel			kernel_ca_next_gen_tot_ncn3;
	cl_kernel			kernel_ca_next_gen_abs_ncn3_ncs2;
	cl_kernel			kernel_ca_next_gen_range_tot_ncn3;
	cl_kernel			kernel_ca_next_gen_vba;
} OCLCA;
// **********************************************************************************************************************************
// OCL Functions ********************************************************************************************************************
// **********************************************************************************************************************************
// returns valid opencl kernel object
#define CL_MAX_PLATFORM_COUNT (4)
#define CL_MAX_DEVICE_COUNT (4)
OCLCA
OCLCA_Init(
	const char* cl_filename
) {
	OCLCA result;
	result.command_queue = NULL;
	result.context = NULL;
	result.kernel_ca_next_gen = NULL;
	result.kernel_ca_next_gen_tot_ncn3 = NULL;
	result.kernel_ca_next_gen_abs_ncn3_ncs2 = NULL;
	result.kernel_ca_next_gen_range_tot_ncn3 = NULL;
	result.success = CL_SUCCESS;

	//return result;

		////////// BEGIN SETUP GPU COMMUNICATION


		// Find the first GPU device
	cl_int cl_errcode_ret;				// used to retrieve opencl error codes
	cl_device_id selected_device = 0;

	cl_uint platform_count = 0;
	clGetPlatformIDs(0, NULL, &platform_count);
	cl_platform_id platform_ids[CL_MAX_PLATFORM_COUNT];
	clGetPlatformIDs(CL_MAX_PLATFORM_COUNT, platform_ids, &platform_count);

	/* Macros to help display device infos */
#define OCL_PRINT_DEVICE_INFO_STRING(cl_device_info) { \
		OCL_CHECK_ERROR(clGetDeviceInfo(device, cl_device_info, sizeof(string1024), &string1024, NULL)); \
		printf("clGetDeviceInfo %-40s %s\n", #cl_device_info, string1024); }
#define OCL_PRINT_DEVICE_INFO_UINT(cl_device_info) { \
		OCL_CHECK_ERROR(clGetDeviceInfo(device, cl_device_info, sizeof(rt_cl_uint), &rt_cl_uint, NULL)); \
		printf("clGetDeviceInfo %-40s %u\n", #cl_device_info, rt_cl_uint); }
		/* */
	char string1024[1024];
	cl_uint rt_cl_uint;
	for (int pi = 0; pi < platform_count; pi++) {
		cl_platform_id platform_id = platform_ids[pi];

		cl_uint device_count = 0;
		clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_ALL, 0, NULL, &device_count);
		cl_device_id device_ids[CL_MAX_DEVICE_COUNT];
		clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_ALL, CL_MAX_DEVICE_COUNT, device_ids, &device_count);

		fprintf(stderr, "SUC platform found  platform count %d  platform #%d  device count %d  device id #0 %d%d\n",
			platform_count, pi, device_count, device_ids[0]);

		for (int i = 0; i < device_count; i++) {
			cl_device_id device = device_ids[i];
			OCL_PRINT_DEVICE_INFO_STRING(CL_DEVICE_NAME)
				OCL_PRINT_DEVICE_INFO_STRING(CL_DEVICE_VENDOR)
				OCL_PRINT_DEVICE_INFO_STRING(CL_DEVICE_VERSION)
				OCL_PRINT_DEVICE_INFO_STRING(CL_DRIVER_VERSION)
				OCL_PRINT_DEVICE_INFO_UINT(CL_DEVICE_ADDRESS_BITS)
				OCL_PRINT_DEVICE_INFO_UINT(CL_DEVICE_MAX_CLOCK_FREQUENCY)
				OCL_PRINT_DEVICE_INFO_UINT(CL_DEVICE_MAX_COMPUTE_UNITS)
				OCL_PRINT_DEVICE_INFO_UINT(CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR)
				OCL_PRINT_DEVICE_INFO_UINT(CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT)
				OCL_PRINT_DEVICE_INFO_UINT(CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT)
				OCL_PRINT_DEVICE_INFO_UINT(CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG)
				OCL_PRINT_DEVICE_INFO_UINT(CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT)
				OCL_PRINT_DEVICE_INFO_UINT(CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE)
				OCL_PRINT_DEVICE_INFO_UINT(CL_DEVICE_NATIVE_VECTOR_WIDTH_CHAR)
				OCL_PRINT_DEVICE_INFO_UINT(CL_DEVICE_NATIVE_VECTOR_WIDTH_SHORT)
				OCL_PRINT_DEVICE_INFO_UINT(CL_DEVICE_NATIVE_VECTOR_WIDTH_INT)
				OCL_PRINT_DEVICE_INFO_UINT(CL_DEVICE_NATIVE_VECTOR_WIDTH_LONG)
				OCL_PRINT_DEVICE_INFO_UINT(CL_DEVICE_NATIVE_VECTOR_WIDTH_FLOAT)
				OCL_PRINT_DEVICE_INFO_UINT(CL_DEVICE_NATIVE_VECTOR_WIDTH_DOUBLE)
				//OCL_PRINT_DEVICE_INFO_STRING(CL_DEVICE_EXTENSIONS)

				if (pi == 0) {
					selected_device = device;
					printf("SELECTED\n");
				}
		}
	}

	if (!selected_device) {
		fprintf(stderr, "ERR failed to find any OpenCL GPU device. Sorry.\n");
		result.success = selected_device;
		return result;
	}

	// clCreateContext
	cl_context context = clCreateContext(NULL, 1, &selected_device, NULL, NULL, &cl_errcode_ret);
	OCL_CHECK_ERROR(cl_errcode_ret);
	// clCreateCommandQueue
	cl_command_queue command_queue = clCreateCommandQueue(context, selected_device, 0, &cl_errcode_ret);
	if (cl_errcode_ret == CL_SUCCESS)		printf("SUC clCreateCommandQueue @0x%x\n", command_queue);
	else									printf("ERR clCreateCommandQueue failed with error code %d\n", cl_errcode_ret);
	// clCreateProgramWithSource
	const char* program_code = read_file(cl_filename);
	cl_program program = clCreateProgramWithSource(context, 1, (const char* []) { program_code }, NULL, & cl_errcode_ret);
	if (cl_errcode_ret == CL_SUCCESS)		printf("SUC clCreateProgramWithSource @0x%x\n", program);
	else									printf("ERR clCreateProgramWithSource failed with error code %d\n", cl_errcode_ret);
	// clBuildProgram
	cl_errcode_ret = clBuildProgram(program, 0, NULL, "", NULL, NULL);
	if (cl_errcode_ret == CL_SUCCESS)		printf("SUC clBuildProgram\n");
	if (cl_errcode_ret != CL_SUCCESS) {
		printf("ERR clBuildProgram failed with error code %d\n", cl_errcode_ret);
		char compiler_log[4096] = "EMPTY\0";
		clGetProgramBuildInfo(program, selected_device, CL_PROGRAM_BUILD_LOG, sizeof(compiler_log), compiler_log, NULL);
		printf("    error log\n%s\n", compiler_log);
		result.success = cl_errcode_ret;
		return result;
	}
	// clCreateKernel
	////result.kernel_ca_next_gen = clCreateKernel(program, "ca_next_gen", &cl_errcode_ret);
	////OCL_CHECK_ERROR(cl_errcode_ret)
	////result.kernel_ca_next_gen_tot_ncn3 = clCreateKernel(program, "ca_next_gen_tot_ncn3", &cl_errcode_ret);
	////OCL_CHECK_ERROR(cl_errcode_ret)
	////result.kernel_ca_next_gen_abs_ncn3_ncs2 = clCreateKernel(program, "ca_next_gen_abs_ncn3_ncs2", &cl_errcode_ret);
	////OCL_CHECK_ERROR(cl_errcode_ret)
	////result.kernel_ca_next_gen_range_tot_ncn3 = clCreateKernel(program, "ca_next_gen_range_tot_ncn3", &cl_errcode_ret);
	////OCL_CHECK_ERROR(cl_errcode_ret)
	result.kernel_ca_next_gen_vba = clCreateKernel(program, "ca_next_gen_vba", &cl_errcode_ret);
	OCL_CHECK_ERROR(cl_errcode_ret)
		// get some kernel info
	{
		cl_kernel kernel = result.kernel_ca_next_gen_vba;
		size_t kwgs = 0;
		OCL_CHECK_ERROR(clGetKernelWorkGroupInfo(kernel, NULL, CL_KERNEL_WORK_GROUP_SIZE, sizeof(kwgs), &kwgs, NULL));
		printf("CL_KERNEL_WORK_GROUP_SIZE %u\n", kwgs);
		size_t pwgsm = 0;
		OCL_CHECK_ERROR(clGetKernelWorkGroupInfo(kernel, NULL, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(pwgsm), &pwgsm, NULL));
		printf("CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE %u\n", pwgsm);
		cl_ulong pms = 0;
		OCL_CHECK_ERROR(clGetKernelWorkGroupInfo(kernel, NULL, CL_KERNEL_PRIVATE_MEM_SIZE, sizeof(pms), &pms, NULL));
		printf("CL_KERNEL_PRIVATE_MEM_SIZE %u\n", pms);
		cl_ulong lms = 0;
		OCL_CHECK_ERROR(clGetKernelWorkGroupInfo(kernel, NULL, CL_KERNEL_LOCAL_MEM_SIZE, sizeof(lms), &lms, NULL));
		printf("CL_KERNEL_LOCAL_MEM_SIZE %u\n", pms);
	}
	//
	result.context = context;
	result.command_queue = command_queue;

	// cleanup
	//clReleaseProgram(program);
	//clReleaseCommandQueue(command_queue);
	//clReleaseContext(context);

	return result;
}
//**********************************************************************************************************************************
/* Calculate next generation OpenCL   flexible variant - can handle totalistic and absolute rules by use of multiplication table */
void OCLCA_Run(
	int ng,					// nr of generations
	int res,				// reset flag, is 1 if anything changes
	OCLCA oclca,
	CA_RULE* cr,
	int sync_memory,
	const CA_CT* clv,		// cell-line first valid element - this array must include brd > size must ne spsz + brd
	const int scsz,			// space-size
	const int brsz,			// border-size
	cl_uint rg)		// range per kernel
{
	rg = max(1, rg);

	cl_int cl_errcode_ret;	// used to retrieve opencl error codes

	// define how many kernels run in parallel
	cl_uint work_dim = 1;
	cl_uint gws[1];		// global work size
	cl_uint lws[1];		// local work size

	///printf("clv %x  scsz %d  gws[0] %u  klrg %u\n", clv, scsz, gws[0], rg);

	// create host to device memory bindings and transfer memory from host to device
	static cl_mem b_cli = NULL;			// opencl binding for cell-line
	static cl_mem b_clo = NULL;			// opencl binding for cell-line
	static cl_mem b_clsz = NULL;			// opencl binding for cell-line-size
	static cl_mem b_ncn = NULL;			// opencl binding for ncn
	static cl_mem b_rltl = NULL;			// opencl binding for
	static cl_mem b_mntl = NULL;			// opencl binding for
	static cl_mem b_rg = NULL;			// opencl binding for
	static int clfp = 0;			// cell-line flip to switch between cli and clo
	/* Init device memory and host-device-memory-bindings and kernel arguments */
	if (1 || sync_memory || res || b_cli == NULL) {
		//printf("opencl_ca memory reset\n");
		sync_memory = 1;
		clfp = 1;
		// clean up
		if (b_cli != NULL)	OCL_CHECK_ERROR(clReleaseMemObject(b_cli));
		if (b_clo != NULL)	OCL_CHECK_ERROR(clReleaseMemObject(b_clo));
		if (b_ncn != NULL)	OCL_CHECK_ERROR(clReleaseMemObject(b_ncn));
		if (b_rltl != NULL)	OCL_CHECK_ERROR(clReleaseMemObject(b_rltl));
		if (b_mntl != NULL)	OCL_CHECK_ERROR(clReleaseMemObject(b_mntl));
		if (b_rg != NULL)	OCL_CHECK_ERROR(clReleaseMemObject(b_rg));
		// cli
		b_cli = clCreateBuffer(oclca.context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, (scsz + brsz) * sizeof(*clv), clv, &cl_errcode_ret);
		OCL_CHECK_ERROR(cl_errcode_ret);
		// clo
		b_clo = clCreateBuffer(oclca.context, CL_MEM_READ_WRITE, (scsz + brsz) * sizeof(*clv), NULL, &cl_errcode_ret);
		OCL_CHECK_ERROR(cl_errcode_ret);
		// ncn
		b_ncn = clCreateBuffer(oclca.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cr->ncn), &cr->ncn, &cl_errcode_ret);
		OCL_CHECK_ERROR(cl_errcode_ret);
		// rule-table
		b_rltl = clCreateBuffer(oclca.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cr->rltl[0]) * cr->nns, &cr->rltl[0], &cl_errcode_ret);
		OCL_CHECK_ERROR(cl_errcode_ret);
		// multiplication-table
		b_mntl = clCreateBuffer(oclca.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cr->mntl[0]) * cr->ncn, &cr->mntl[0], &cl_errcode_ret);
		OCL_CHECK_ERROR(cl_errcode_ret);
		// rg
		b_rg = clCreateBuffer(oclca.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(rg), &rg, &cl_errcode_ret);
		OCL_CHECK_ERROR(cl_errcode_ret);
	}

	/* Run kernel */
	if (0 && cr->tp == TOT && cr->ncn == 3) {
		int LT = 1;
		ng = (ng + 1) / LT;

		if (1) {
			int ws = scsz;// +2 * ng;			// work-size - nr of cells to process
			int wi = 64;						// item-size in cells
			int wg = 128;						// group-size in items

			lws[0] = 128;
			gws[0] = ws / wi / wg * wg;
			while (gws[0] * wi < ws)
				gws[0] += wg;

			rg = 0;
			//ng = 1;
			clReleaseMemObject(b_rg);
			b_rg = clCreateBuffer(oclca.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(rg), &rg, &cl_errcode_ret);
			if (cl_errcode_ret != CL_SUCCESS)		printf("ERR clCreateBuffer failed with error code %d near line %d.\n", cl_errcode_ret, __LINE__);

			printf("ng %d  ws %d  wi %d  wg %d  gws[0] %d gws[0]*wi %d  max ws %d\n", ng, ws, wi, wg, gws[0], gws[0] * wi, scsz + brsz);
		}
		else {
			int ws = scsz + 2 * ng;			// work-size - nr of cells to process
			int gs = 128;					// group-size in items
			int gc = 1;						// group-count

			int gn = gs - 2 * LT;			// group-netto - netto amount of items fully processed after acessing gs items

			int gr;							// group-range per group - nr of items one group processes

			// count fixed, range free
			gr = ws / gc / gs;					// work is distributed among gc groups
			gr = gr / gn;					// each group processes gs items in parallel
			while (gr * gn * gc * gs < ws)		// make sure at least ndss items are processed
				++gr;

			// range fixed, count free
			//gr = 1;
			//gc = ws / gn;
			//while (gr * gn * gc < ws)		// make sure at least ndss items are processed
			//	++gc;


			static int dc = 0;
			if (1 | ++dc == 100) {
				dc = 0;
				printf("ng %d  ws %d  gc %d  gs %d  gn %d  gr %d  gr*gn*gc %d  max ws %d\n", ng, ws, gc, gs, gn, gr, gr * gn * gc, scsz + brsz);
			}
			if (gr * gn * gc * 16 > scsz + brsz) {
				printf("!!!! working size to big / border to small\n");
				gr = 0;
			}

			rg = gr;
			clReleaseMemObject(b_rg);
			b_rg = clCreateBuffer(oclca.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(rg), &rg, &cl_errcode_ret);
			if (cl_errcode_ret != CL_SUCCESS)		printf("ERR clCreateBuffer failed with error code %d near line %d.\n", cl_errcode_ret, __LINE__);

			// kernel ca_next_gen_tot_ncn3
			///lws[0] = 128;
			lws[0] = gs;
			//gws[0] = (scsz + brsz - cr->ncn + 1 - 512 /* LT */) / rg;
			///gws[0] = ((scsz + brsz - cr->ncn + 1) / rg + 1) / 128 * 128;
			//gws[0] = scsz / rg + 1;
			gws[0] = gc * gs;
		}
		///printf("gws %d    rg %d  gws*rg %d   scsz %d  brsz %d  sc+br %d\n", gws[0], rg, gws[0] * rg, scsz, brsz, scsz + brsz);
		// set constant kernel arguments
		cl_errcode_ret = clSetKernelArg(oclca.kernel_ca_next_gen_range_tot_ncn3, 2, sizeof(b_rltl), &b_rltl);
		if (cl_errcode_ret != CL_SUCCESS)		printf("ERR kernel set arg #2 failed with error code  %d.\n", cl_errcode_ret);
		cl_errcode_ret = clSetKernelArg(oclca.kernel_ca_next_gen_range_tot_ncn3, 3, sizeof(b_rg), &b_rg);
		if (cl_errcode_ret != CL_SUCCESS)		printf("ERR kernel set arg #2 failed with error code  %d.\n", cl_errcode_ret);
		// run kernel rt times
		for (int rt = 0; rt < ng; ++rt) {
			// advance clfp
			if (++clfp == 2) clfp = 0;
			///printf("clfp %d\n", clfp);
			// set variable kernel arguments
			cl_errcode_ret = clSetKernelArg(oclca.kernel_ca_next_gen_range_tot_ncn3, clfp == 1, sizeof(b_cli), &b_cli);
			if (cl_errcode_ret != CL_SUCCESS)		printf("ERR kernel set arg failed with error code  %d.\n", cl_errcode_ret);
			cl_errcode_ret = clSetKernelArg(oclca.kernel_ca_next_gen_range_tot_ncn3, clfp == 0, sizeof(b_clo), &b_clo);
			if (cl_errcode_ret != CL_SUCCESS)		printf("ERR kernel set arg failed with error code  %d.\n", cl_errcode_ret);
			// run kernel
			cl_errcode_ret = clEnqueueNDRangeKernel(oclca.command_queue, oclca.kernel_ca_next_gen_range_tot_ncn3, work_dim, NULL, &gws, &lws, 0, NULL, NULL);
			if (cl_errcode_ret != CL_SUCCESS)		printf("ERR clEnqueueNDRangeKernel for kenrel 'ca_next_gen_tot_ncn3' failed with error code %d near line %d.\n", cl_errcode_ret, __LINE__);
			//else									printf("SUC clEnqueueNDRangeKernel for kenrel 'ca_next_gen_tot_ncn3' near line %d.\n", __LINE__);
		}
	}
	// SIMPLE KERNEL
	else {
		/* Make sure kernrel is available */
		//if (oclca.kernel_ca_next_gen == NULL || oclca.kernel_ca_next_gen_tot_ncn3 == NULL)
		//	return 0;

		cl_kernel kernel = oclca.kernel_ca_next_gen_tot_ncn3;
		if (cr->ncn == 3 && cr->ncs == 2)
			kernel = oclca.kernel_ca_next_gen_abs_ncn3_ncs2;
		int mnsz = (scsz + ng * (cr->ncn - 1));	// minimum size needed to process
		int mxsz = scsz + brsz;	// maximum size
		lws[0] = 1024;
		gws[0] = mnsz / lws[0] * lws[0];
		while (gws[0] < mnsz)
			gws[0] += lws[0];
		//		printf("smpl krnl  ng %6d  gws %d    rg %d  gws*rg %d   scsz %d  brsz %d  sc+br %d\n", ng, gws[0], rg, gws[0] * rg, scsz, brsz, scsz + brsz);

		if (gws[0] > mxsz) {
			printf("space size insufficient! aborting.\n");
			printf("smpl krnl  ng %6d  gws %d    rg %d  gws*rg %d   scsz %d  brsz %d  sc+br %d\n", ng, gws[0], rg, gws[0] * rg, scsz, brsz, scsz + brsz);
			return;
		}

		//
		// set constant kernel arguments
		///OCL_CHECK_ERROR(clSetKernelArg(kernel, 2, sizeof(b_ncn), &b_ncn));
		OCL_CHECK_ERROR(clSetKernelArg(kernel, 2, sizeof(b_rltl), &b_rltl));
		///OCL_CHECK_ERROR(clSetKernelArg(kernel, 4, sizeof(b_mntl), &b_mntl));
		// run kernel rt times
		for (int rt = 0; rt < ng; ++rt) {
			// advance clfp
			if (++clfp == 2) clfp = 0;
			// set variable kernel arguments
			OCL_CHECK_ERROR(clSetKernelArg(kernel, clfp == 1, sizeof(b_cli), &b_cli));
			OCL_CHECK_ERROR(clSetKernelArg(kernel, clfp == 0, sizeof(b_clo), &b_clo));
			// run kernel
			OCL_CHECK_ERROR(clEnqueueNDRangeKernel(oclca.command_queue, kernel, work_dim, NULL, &gws, &lws, 0, NULL, NULL));
		}
	}
	/* Transfer device memory to host memory */
	if (sync_memory) {
		//if (1) {// && sync_memory && b_clo != NULL) {
		cl_mem b_cl;
		if (clfp == 0)	b_cl = b_clo;
		else			b_cl = b_cli;
		OCL_CHECK_ERROR(clEnqueueReadBuffer(oclca.command_queue, b_cl, CL_TRUE, 0, scsz * sizeof(*clv), clv, NULL, 0, NULL));

		//clEnqueueMapBuffer(oclca.command_queue, b_cl, CL_TRUE, CL_MAP_READ, 0, (scsz + brsz) * sizeof(*clv), 0, NULL, NULL, &cl_errcode_ret);
		//if (cl_errcode_ret != CL_SUCCESS)			printf("ERR clEnqueueMapBuffer failed with error code %d\n", cl_errcode_ret);
		////	///memcpy(DeMa, result, DMS * sizeof(*DeMa));
		//clEnqueueUnmapMemObject(oclca.command_queue, b_cl, 0, 0, NULL, NULL);
	}
}


// **********************************************************************************************************************************
/* Calculate next generation OpenCL   flexible variant - can handle totalistic and absolute rules by use of multiplication table */
void OCLCA_RunVBA(
	int gnc,										// nr of generations
	int res,									// reset flag, is 1 if anything changes
	OCLCA oclca,
	CA_RULE* cr,
	caBitArray vba,
	int sync_memory,
	cl_uint rg)									// range per kernel
{
	rg = max(1, rg);

	cl_int cl_errcode_ret;						// used to retrieve opencl error codes

	// create host to device memory bindings and transfer memory from host to device
	static cl_mem b_vba = NULL;					// opencl binding for vertical-bit-array
	/* Init device memory and host-device-memory-bindings and kernel arguments */
	if (1 || sync_memory || res || b_vba == NULL) {
		//printf("opencl_ca memory reset\n");
		sync_memory = 1;
		// clean up
		if (b_vba != NULL)	OCL_CHECK_ERROR(clReleaseMemObject(b_vba));
		// vba
		b_vba = clCreateBuffer(oclca.context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, vba.ct / 8 + vba.oc * vba.rw / 8, vba.v, &cl_errcode_ret);
		OCL_CHECK_ERROR(cl_errcode_ret);
	}

	/* Run kernel */
	// define how many kernels run in parallel
	size_t global = vba.lc;								// global work size
	///printf("gws %d  rg %d  rc %d  lc %d\n", gws[0], rg, vba.lc, vba.rc);
		// set kernel arguments
	cl_errcode_ret = clSetKernelArg(oclca.kernel_ca_next_gen_vba, 0, sizeof(b_vba), &b_vba);
	if (cl_errcode_ret != CL_SUCCESS)		printf("ERR kernel set arg #0 failed with error code  %d.\n", cl_errcode_ret);
	cl_errcode_ret = clSetKernelArg(oclca.kernel_ca_next_gen_vba, 1, sizeof(gnc), &gnc);
	if (cl_errcode_ret != CL_SUCCESS)		printf("ERR kernel set arg #2 failed with error code  %d.\n", cl_errcode_ret);
	cl_errcode_ret = clSetKernelArg(oclca.kernel_ca_next_gen_vba, 2, sizeof(vba.rc), &vba.rc);
	if (cl_errcode_ret != CL_SUCCESS)		printf("ERR kernel set arg #1 failed with error code  %d.\n", cl_errcode_ret);
	cl_errcode_ret = clSetKernelArg(oclca.kernel_ca_next_gen_vba, 3, sizeof(vba.lc), &vba.lc);
	if (cl_errcode_ret != CL_SUCCESS)		printf("ERR kernel set arg #3 failed with error code  %d.\n", cl_errcode_ret);
	// run kernel
	cl_errcode_ret = clEnqueueNDRangeKernel(oclca.command_queue, oclca.kernel_ca_next_gen_vba, 1, NULL, &global, NULL, 0, NULL, NULL);
	if (cl_errcode_ret != CL_SUCCESS)		printf("ERR clEnqueueNDRangeKernel for kenrel 'ca_next_gen_vba' failed with error code %d near line %d.\n", cl_errcode_ret, __LINE__);
	//
	clFinish(oclca.command_queue);
	/* Transfer device memory back to host memory */
	if (sync_memory) {
		printf("vba.ct %d\n", vba.ct);
		OCL_CHECK_ERROR(clEnqueueReadBuffer(oclca.command_queue, b_vba, CL_TRUE, 0, vba.ct / 8, vba.v, 0, NULL, NULL));
	}
}
// **********************************************************************************************************************************
#endif

int64_t CA_CNFN_OPENCL(int64_t pgnc, caBitArray* vba) {
#if ENABLE_CL
	OCLCA_RunVBA(pgnc, 0/*res*/, oclca, vba->cr, *vba, 1/*de*/, 16/*klrg*/);
	pgnc = 0;
	convertBetweenCACTandCABitArray(vba->clsc, vba->clsc + vba->scsz, vba, 1);
#else
	printf("OpenCL support is not available in this build!\n");
#endif
	return 0;
}

typedef enum {
	CM_DISABLED,										// computation disabled
	CM_SISD,											// byte-array			single instrunction single data
	CM_SIMD,											// byte-array			single instruction multiple data
	CM_LUT,												// bit-array			look-up-table
	CM_VBA_1x256,										// vertical-bit-array	AVX	- 1 lane with 256bits			rule 54 hardcoded
	CM_HASH,											// bit-array			hash-table
	CM_OMP_TEST,										// OpenMP Test
	CM_BOOL,											// bit-array			boolean operators					rule 54 hardcoded
	CM_VBA_1x32,										// vertical-bit-array	1 lane with 32bit					rule 54 hardcoded
	CM_VBA_2x32,										// vertical-bit-array	2 lanes with 32bit					rule 54 hardcoded
	CM_VBA_4x32,										// vertical-bit-array	4 lanes with 32bit					rule 54 hardcoded
	CM_VBA_1x64,										// vertical-bit-array	1 lane with 64bit					rule 54 hardcoded
	CM_VBA_2x256,										// vertical-bit-array	AVX - 2 lanes with 256bit			rule 54 hardcoded
	CM_VBA_16x256,										// vertical-bit-array	AVX - 16 lanes with 256bit			rule 54 hardcoded
	CM_OPENCL,											// OpenCL
	CM_MAX,
} caComputationMode;

// computation-settings-array
struct {
	char* name;
	int64_t(*cnfn)(int64_t, caBitArray*);				// computation-function
	void		(*itfn)(caBitArray*, CA_RULE* cr);	    // init-function
	void		(*scfn)(caBitArray*);					// sync-(ca-space)-function
} const ca_cnsgs[CM_MAX] = {
	[CM_DISABLED] = {
		.name = "CM_DISABLED",
		.cnfn = CA_CNFN_DISABLED,
		.itfn = CA_CNITFN_DISABLED
	},
	[CM_VBA_1x256] = {
		.name = "CM_VBA_1x256",
		.cnfn = CA_CNFN_VBA_1x256,
		.itfn = CA_CNITFN_VBA_1x256,
		.scfn = CA_SCFN_VBA,
	},
	[CM_OMP_TEST] = {
		.name = "CM_OMP_TEST",
		.cnfn = CA_CNFN_OMP_TEST,
		.itfn = CA_CNITFN_OMP_TEST,
		.scfn = CA_SCFN_VBA,
	},
	[CM_OPENCL] = {
		.name = "CM_OPENCL",
		.cnfn = CA_CNFN_OPENCL,
		.itfn = CA_CNITFN_OPENCL,
		.scfn = CA_SCFN_VBA,
	},
	[CM_VBA_16x256] = {
		.name = "CM_VBA_16x256",
		.cnfn = CA_CNFN_VBA_16x256,
		.itfn = CA_CNITFN_VBA_16x256,
		.scfn = CA_SCFN_VBA,
	},
	[CM_VBA_2x256] = {
		.name = "CM_VBA_2x256",
		.cnfn = CA_CNFN_VBA_2x256,
		.itfn = CA_CNITFN_VBA_2x256,
		.scfn = CA_SCFN_VBA,
	},
	[CM_VBA_1x32] = {
		.name = "CM_VBA_1x32",
		.cnfn = CA_CNFN_VBA_1x32,
		.itfn = CA_CNITFN_VBA_1x32,
		.scfn = CA_SCFN_VBA,
	},
	[CM_VBA_1x64] = {
		.name = "CM_VBA_1x64",
		.cnfn = CA_CNFN_VBA_1x64,
		.itfn = CA_CNITFN_VBA_1x64,
		.scfn = CA_SCFN_VBA,
	},
	[CM_VBA_2x32] = {
		.name = "CM_VBA_2x32",
		.cnfn = CA_CNFN_VBA_2x32,
		.itfn = CA_CNITFN_VBA_2x32,
		.scfn = CA_SCFN_VBA,
	},
	[CM_VBA_4x32] = {
		.name = "CM_VBA_4x32",
		.cnfn = CA_CNFN_VBA_4x32,
		.itfn = CA_CNITFN_VBA_4x32,
		.scfn = CA_SCFN_VBA,
	},
	[CM_SISD] = {
		.name = "CM_SISD",
		.cnfn = CA_CNFN_SISD,
	},
	[CM_SIMD] = {
		.name = "CM_SIMD",
		.cnfn = CA_CNFN_SIMD,
	},
	[CM_BOOL] = {
		.name = "CM_BOOL",
		.cnfn = CA_CNFN_BOOL,
		.itfn = CA_CNITFN_BOOL,
		.scfn = CA_SCFN_HBA,
	},
	[CM_LUT] = {
		.name = "CM_LUT",
		.cnfn = CA_CNFN_LUT,
		.itfn = CA_CNITFN_LUT,
		.scfn = CA_SCFN_HBA,
	},
	[CM_HASH] = {
		.name = "CM_HASH",
		.cnfn = CA_CNFN_HASH,
		.itfn = CA_CNITFN_HASH,
		.scfn = CA_SCFN_HASH,
	},
};
const char* cm_names[CM_MAX] = { 0 };	// flat array of strings of computation-function-names

#define CS_SYS_BLOCK_SZ	0x04			// system-block-size - used by bit-shifting operations
#define CS_BLOCK_SZ		0xFF			// size of one row / nr. of columns
#define CS_PRF_WIN_SZ	0xFF			// prefered window-size - used by row>col / col>row conversion functions

/* reverse: reverse string s in place */
void
reverse(char s[])
{
	int c, i, j;
	for (i = 0, j = strlen(s) - 1; i < j; i++, j--) {
		c = s[i];
		s[i] = s[j];
		s[j] = c;
	}
}
/* like sprintf_s but appends format to existing string */
sprintfa_s(char* sg, size_t sgsz, const char* format, ...) {
	if (format == NULL)
		return;

	/* string length - position of 0-bit */
	int sglh = strlen(sg);
	/* move char-pointer to sglh and reduce sgsz to remaining length */
	sg += sglh;
	sgsz -= sglh - 5;
	/* Access optional parameters */
	va_list argptr;
	va_start(argptr, format);
	/* Write additional part to string */
	vsprintf_s(sg, sgsz, format, argptr);
	/* Additional output to shell */
	printf("%s", sg);
}

/*
os	output string(char array) to receive info
sgsz	maximum string size / length of sg
*/
void
CA_RULE_PrintInfo(CA_RULE* cr, char* os, size_t sgsz) {
	/* Dont need to do anything if format is empty */
	if (os == NULL)
		return;
	/* Reset output-string */
	*os = 0;
	/* Print Info */
	sprintfa_s(os, sgsz, "rule info\n");
	sprintfa_s(os, sgsz, "%d  cells in a neighborhod\n", cr->ncn);
	sprintfa_s(os, sgsz, "%d  cell states\n", cr->ncs);
	if (cr->tp == ABS)
		sprintfa_s(os, sgsz, "%d  neighborhod states (absolute)\n", cr->nns);
	else if (cr->tp == TOT)
		sprintfa_s(os, sgsz, "%d  neighborhod states (totalistic)\n", cr->nns);
	sprintfa_s(os, sgsz, "%I64u  %d^%d  rules\n", cr->nrls, cr->ncs, cr->nns);
	//sprintfa_s(os, sgsz, "%I64u  maxuint64\n", MAXUINT64);
	sprintfa_s(os, sgsz, "%I64u  rule#\n", cr->rl);
	//sprintfa_s(os, sgsz, "  rule table - size %d x %d = %d\n", cr->nns, sizeof *cr->rltl, cr->nns * sizeof *cr->rltl);

	/* Print table neighborhod-states > new state */
	// table neighborhod-states > reduced-neighborhod-state MAX 500 ITEMS A 5000 BYTES
	char* tnst[500];
	for (int i = 0; i < cr->nns; ++i) {
		tnst[i] = malloc(5000);
		sprintf_s(tnst[i], 5000, "");
	}

	// go through all neighborhod-states
	for (int i = 0; i < ipow(cr->ncs, cr->ncn); ++i) {
		int nst = i;											// neighborhod-state
		const char snst[100] = "";								// string neighborhod-state
		int rds = 0;											// reduced neighborhod-state
		// go through all neighbor-cells
		for (int k = 0; k < cr->ncn; ++k) {
			// extract state of rightmost neighbor
			int rns = nst % cr->ncs;							// rightmost-neighbor-state
			nst /= cr->ncs;
			// reduce neighborhod-state
			if (cr->tp == TOT)									// totalistic
				rds += rns;
			else if (cr->tp == ABS)								// absolute
				rds += ipow(cr->ncs, cr->ncn - k - 1) * rns;
			// print
			sprintfa_s(snst, sizeof(snst), "%d", rns); // actually displays it in wrong order, i.e. rightmost neighbor is printed leftmost > has to be reversed before displaying
		}
		reverse(snst);
		//sprintfa_s(os, sgsz, "%2d   %s\n", rds, snst);
		sprintfa_s(tnst[rds], 500, "%s ", snst);
	}

	// print table
	for (int i = 0; i < cr->nns; ++i) {
		sprintfa_s(os, sgsz, "%2d   %d   %s", i, cr->rltl[i], tnst[i]);
		// for columns
		if ((i + 1) % 4)
			sprintfa_s(os, sgsz, "  |  ");
		else
			sprintfa_s(os, sgsz, "\n");
	}
}

/* HILBERT/MORTON CURVE TEST CODE */
__inline
uint32_t MortonEncode2(uint32_t x, uint32_t y)
{
	return _pdep_u32(y, 0xAAAAAAAA) | _pdep_u32(x, 0x55555555);
}


__inline
uint32_t MortonEncode3(uint32_t x, uint32_t y, uint32_t z)
{
	return _pdep_u32(y, 0x24924924) | _pdep_u32(y, 0x12492492) | _pdep_u32(x, 0x09249249);
}


__inline
void MortonDecode2(uint32_t code, uint32_t* outX, uint32_t* outY)
{
	*outX = _pext_u32(code, 0x55555555);
	*outY = _pext_u32(code, 0xAAAAAAAA);
}


__inline
void MortonDecode3(uint32_t code, uint32_t* outX, uint32_t* outY, uint32_t* outZ)
{
	*outX = _pext_u32(code, 0x09249249);
	*outY = _pext_u32(code, 0x12492492);
	*outZ = _pext_u32(code, 0x24924924);
}


/* Creates a complete CA_RULE struct by calculationg dynamic properties of prl */
CA_RULE
CA_Rule(CA_RULE prl) {
	prl.ncn = max(1, prl.ncn);
	prl.ncs = max(1, prl.ncs);
	/* Number of neighborhod-states */
	// totalistic
	if (prl.tp == TOT)
		prl.nns = (prl.ncs - 1) * prl.ncn + 1;
	// absolute
	else
		prl.nns = ipow(prl.ncs, prl.ncn);
	/* Number of rules */
	prl.nrls = ipow(prl.ncs, prl.nns); // TODO does not behove well due to integer overflow on high numbers
	if (prl.nrls == 0)
		prl.nrls = MAXUINT64;
	// Make sure rule is within range of number of rules
	prl.rl %= prl.nrls;
	/* Build multiplication table */
	//free(prl.mntl);
	prl.mntl = malloc(prl.ncn * sizeof * prl.mntl);
	// init multiplication table with multiplication factors for type
	for (int i = 0; i < prl.ncn; i++)
		// totalistic
		if (prl.tp == TOT)
			prl.mntl[i] = 1;
	// absolute
		else
			prl.mntl[i] = ipow(prl.ncs, prl.ncn - i - 1);
	/* Build rule table */
	//free(prl.rltl);
	prl.rltl = malloc(prl.nns * sizeof * prl.rltl);
	// init rule table with zeros
	for (int i = 0; i < prl.nns; i++)
		prl.rltl[i] = 0;
	// fill rule table
	UINT64 trl = prl.rl;
	int i = 0;
	while (trl) {
		prl.rltl[i] = trl % prl.ncs;
		trl /= prl.ncs;
		i++;
	}
	//for (int i = 0; i < prl.nns; ++i) {
	//	prl.rltl[i] = prl.rl / ipow(prl.ncs, i) % prl.ncs;
	//}

	/* Return result */
	return prl;
}


#define LV_MAXCLTLSZ 32			// max size of rule table
#define LV_MAXRLTLSZ 32			// max size of rule table

/*
scroll any array by d bytes, will ignore scrolling larger than array size
*/
void
scroll(UINT8* ayf, int ays, int d) {
	if (d < 0) {
		d = abs(d);
		if (d >= ays)
			return;
		memmove(
			ayf,
			ayf + d,
			(ays - d));

	}
	else if (d > 0) {
		if (d >= ays)
			return;
		memmove(
			ayf + d,
			ayf,
			(ays - d));
	}
}


/* Lindenmeyer array - first element has to be LMT, LMH or LMO, last element LME */
#define LMMXDH 32			// Lindenmeyer max depth
#define LMT 4				// Lindenmeyer tetragonal angles
#define LMH 6				// Lindenmeyer hexagonal angles
#define LMO 8				// Lindenmeyer octagonal angles
#define LME 126				// Lindenmeyer end
#define LMD 127				// Lindenmeyer draw -> goes forward than draws
#define LMF 0				// Lindenmeyer forward
#define LMI 124				// Lindenmeyer ignore / place holder
#define LM0 125				// Lindenmeyer next step only if at depth 0
#define LMTR +1				// Lindenmeyer tetragonal +90 degrees
#define LMTL +3				// Lindenmeyer tetragonal -90 degrees
#define LMHR +1				// Lindenmeyer hexagonal +60 degrees
#define LMHL +5				// Lindenmeyer hexagonal -60 degrees
#define LMHRR +2			// Lindenmeyer hexagonal +120 degrees
#define LMHLL +4			// Lindenmeyer hexagonal -120 degrees
#define LMOR +1				// Lindenmeyer octagonal +45 degrees
#define LMOL +7				// Lindenmeyer octagonal -45 degrees
#define LMORR +2			// Lindenmeyer octagonal +90 degrees
#define LMOLL +6			// Lindenmeyer octagonal -90 degrees
#define LMOM +4				// Lindenmeyer octagonal mirror angle / +/- 180 degrees



/*
http://mathforum.org/advanced/robertd/lsys2d.html

http://www.nahee.com/spanky/www/fractint/lsys/variations.html

https://de.wikipedia.org/wiki/E-Kurve

Peano L System    http://bl.ocks.org/nitaku/8949471

H Baum https://de.wikipedia.org/wiki/H-Baum

moore https://en.wikipedia.org/wiki/Moore_curve

peano http://www.iti.fh-flensburg.de/lang/theor/lindenmayer-space-filling.htm

sierpinski, penrose and more https://10klsystems.wordpress.com/examples/
*/

// Lindenmeyer names
enum LMS {
	LMS_SIERPINSKI_SQUARE,
	LMS_MOORE,
	LMS_GOSPER,
	LMS_PEANO,
	LMS_CANTOR,
	LMS_SIERPINSKI,
	LMS_SIERPINSKI_ARROWHEAD,
	LMS_SIERPINSKI_TRIANGLE,
	LMS_DRAGON,
	LMS_TWINDRAGON,
	LMS_TERADRAGON,
	LMS_HILBERT,
	LMS_COUNT
};
// Lindenmeyer arrays
int* LMSA[LMS_COUNT];

/* Gosper */
#define LMA -1
#define LMB -21
const int LMA_GOSPER[] = {
	LMH,
	LMA, LMHR, LMB, LMD, LMHRR, LMB, LMD, LMHL, LMD, LMA, LMHLL, LMD, LMA, LMD, LMA, LMHL, LMB, LMD, LMHR, LME,
	LMHL, LMD, LMA, LMHR, LMB, LMD, LMB, LMD, LMHRR, LMB, LMD, LMHR, LMD, LMA, LMHLL, LMD, LMA, LMHL, LMB, LME };
#undef LMA
#undef LMB
/* Dragon */
#define LMA -4
#define LMB -10
const int LMA_DRAGON[] = {
	LMT,
	LMD, LMA, LME,
	LMA, LMTR, LMB, LMD, LMTR, LME,
	LMTL, LMD, LMA, LMTL, LMB, LME };
#undef LMA
#undef LMB
/* Twin-Dragon */
#define LMA -8
#define LMB -13
const int LMA_TWINDRAGON[] = {
	LMT,
	LMD, LMA, LMTR, LMD, LMA, LMTR, LME,
	LMA, LMTR, LMB, LMD, LME,
	LMD, LMA, LMTL, LMB, LME };
#undef LMA
#undef LMB
/* Terdragon */
#define LMA -1
const int LMA_TERDRAGON[] = {
	LMH,
	LM0, LMD, LMA, LMHRR, LM0, LMD, LMA, LMHLL, LM0, LMD, LMA, LME };
#undef LMA
/* Sierpinski */
#define LMA -10
#define LMB -18
const int LMA_SIERPINSKI[] = {
	LMO,
	LMOR, LMA, LMOLL, LMD, LMOLL, LMA, LMOLL, LMD, LME,
	LMOR, LMB, LMOL, LMD, LMOL, LMB, LMOR, LME,
	LMOL, LMA, LMOR, LMD, LMOR, LMA, LMOL, LME };
#undef LMA
#undef LMB
/* Sierpinski Arrowhead */
#define LMA -1
#define LMB -9
const int LMA_SIERPINSKI_ARROWHEAD[] = {
	LMH,
	LMB, LMD, LMHR, LMA, LMD, LMHR, LMB, LME,
	LMA, LMD, LMHL, LMB, LMD, LMHL, LMA, LME };
#undef LMA
#undef LMB
/* Sierpinski Triangle */
#define LMA -7
#define LMB -27
const int LMA_SIERPINSKI_TRIANGLE[] = {
	LMH,
	LMA, LMHLL, LMB, LMHLL, LMB, LME,
	LMA, LM0, LMD, LMHLL, LMB, LM0, LMD, LMHRR, LMA, LM0, LMD, LMHRR, LMB, LM0, LMD, LMHLL, LMA, LM0, LMD, LME,
	LMB, LM0, LMD, LMB, LM0, LMD, LME };
#undef LMA
#undef LMB
/* Peano type - I think it is "Cantor-Diagonalisierung"
not self avoiding I suppose
*/
#define LMA -1
const int LMA_CANTOR[] = {
	LMT,
	LMA, LM0, LMD, LMTR, LMA, LM0, LMD, LMTL, LMA, LM0, LMD, LMTL, LMA, LM0, LMD, LMTL, LMA, LM0, LMD, LMTR, LMA, LM0, LMD, LMTR, LMA, LM0, LMD, LMTR, LMA, LM0, LMD, LMTL, LMA, LM0, LMD, LME };
#undef LMA
/* Hilbert */
#define LMA -4
#define LMB -16
const int LMA_HILBERT[] = {
	LMT,
	LMA, LMD, LME,
	LMTR, LMB, LMD, LMTL, LMA, LMD, LMA, LMTL, LMD, LMB, LMTR, LME,
	LMTL, LMA, LMD, LMTR, LMB, LMD, LMB, LMTR, LMD, LMA, LMTL, LME };
#undef LMA
#undef LMB
/* Moore */
#define LMA -12
#define LMB -24
const int LMA_MOORE[] = {
	LMT,
	LMA, LMD, LMA, LMTR, LMD, LMTR, LMA, LMD, LMA, LMD, LME,
	LMTL, LMB, LMD, LMTR, LMA, LMD, LMA, LMTR, LMD, LMB, LMTL, LME,
	LMTR, LMA, LMD, LMTL, LMB, LMD, LMB, LMTL, LMD, LMA, LMTR, LME };
#undef LMA
#undef LMB
/* Sierpinski Square - version by Matze - diamond sierpinski rotated and compacted so it is square and fills plane */
#define LMA -18
#define LMB -30
const int LMA_SIERPINSKI_SQUARE[] = {
	//LMO,
	  LMO,
	  //LMA, LMOLL, LMD, LMOLL, LMA, LMOLL, LMD, LME,
	  //LMOR, LMI, LMI, LMI, LMI, LMI, LMI, LMI, LMA,    LMORR, LMORR, LMI, LMI, LMI, LMI, LMA, LME,
	  ///LMOR, LMA, LMOLL, LMD, LMOLL, LMA, LMOLL, LMD, LME,LMF, LMOM, LMD, LMOM,LMF, LMOM, LMD, LMOM,
	  LMOR, LMA, LMOLL, LMD, LMOLL, LMA, LMOLL, LMD, LMF, LMOM, LMD, LMOM,LMF, LMOM, LMD, LMOM, LME,
	  LMOR, LMB, LMF, LMOM, LMD, LMOM, LMOL, LMD, LMOL, LMB, LMOR, LME,
	  LMOL, LMA, LMF, LMOM, LMD, LMOM, LMOR, LMD, LMOR, LMA, LMOL, LME };
#undef LMA
#undef LMB
/* Peano */
#define LMA -1
#define LMB -23
const int LMA_PEANO[] = {
	LMT,
	LMA, LMD, LMB, LMD, LMA, LMTL, LMD, LMTL, LMB, LMD, LMA, LMD, LMB, LMTR, LMD, LMTR, LMA, LMD, LMB, LMD, LMA, LME,
	LMB, LMD, LMA, LMD, LMB, LMTR, LMD, LMTR, LMA, LMD, LMB, LMD, LMA, LMTL, LMD, LMTL, LMB, LMD, LMA, LMD, LMB, LME };
#undef LMA
#undef LMB


void
init_lmas() {
	LMSA[LMS_MOORE] = &LMA_MOORE;
	LMSA[LMS_GOSPER] = &LMA_GOSPER;
	LMSA[LMS_PEANO] = &LMA_PEANO;
	LMSA[LMS_CANTOR] = &LMA_CANTOR;
	LMSA[LMS_SIERPINSKI] = &LMA_SIERPINSKI;
	LMSA[LMS_SIERPINSKI_SQUARE] = &LMA_SIERPINSKI_SQUARE;
	LMSA[LMS_SIERPINSKI_ARROWHEAD] = &LMA_SIERPINSKI_ARROWHEAD;
	LMSA[LMS_SIERPINSKI_TRIANGLE] = &LMA_SIERPINSKI_TRIANGLE;
	LMSA[LMS_DRAGON] = &LMA_DRAGON;
	LMSA[LMS_TWINDRAGON] = &LMA_TWINDRAGON;
	LMSA[LMS_TERADRAGON] = &LMA_TERDRAGON;
	LMSA[LMS_HILBERT] = &LMA_HILBERT;
}


void
display_2d_lindenmeyer(
	const int* lm,							// Lindenmeyer array
	UINT32* pnv,							// pixel-screen first valid element
	UINT32* pni,							// pixel-screen first invalid element
	const int plw,							// pixel-screen line-width
	const int paw,							// pixel-screen available with
	int vlpz,								// vertical pixel zoom
	int hlpz,								// horizontal pixel zoom
	const UINT32* pbv,						// pixel-buffer first valid element
	const UINT32* pbi,						// pixel-buffer first invalid element
	const int lmsw,							// Lindenmeyer step width
	int lmdh,								// Lindenmeyer depth - if < 0 it is calculated automatically
	int* lmvs,								// Lindenmeyer needed vertical screen size for given depth and plw - if not NULL no drawing is done, but size of Lindenmeyer system with given depth is calculated
	int* lmhs,								// Lindenmeyer needed horizontal screen size for given depth and plw - if not NULL no drawing is done, but size of Lindenmeyer system with given depth is calculated
	int* lmpbsz								// Lindenmeyer pixel-buffer-size - i. e. nr of pixels needed to fill Lindenmeyer of given depth, calculated if lmsnsz is not NULL, see line above
) {
	int rpnc = 0;							// pixel-screen cursor (relative, no pointer)
	int rpni = max(0, (pni - pnv) - plw * (vlpz - 0) - (hlpz - 0));		// pixel-screen first invalid element (relative, no pointer) - corrected for pixel-zoom (so we dont have to check for every zoomed pixel)
	UINT32* pbc = pbv;						// pixel-buffer cursor

	int plh = (pni - pnv) / plw;			// pixel-screen-height

	const int sg = 2;						// spacing
	int rc = 2;								// reduce (to compensate spacing)

	/* Automatically recalc depth and display-size if pb-size or Lindenmeyer array changes */
	static int ltsnsz = 0;					// last pixel-screen-size
	static int ltstpn = 0;					// last start-position
	static int ltpbsz = 0;					// last pixel-buffer-size
	static int ltlmdh = -1;					// last Lindenmeyer depth
	static int* ltlm = NULL;				// last Lindenmeyer array
	static int ltvlpz = 0;					// last vertical pixel zoom
	static int lthlpz = 0;					// last horizontal pixel zoom
	if (lmdh < 0) {
		/* recalculate depth and size*/
		if (ltlmdh < 0 || ltsnsz != pni - pnv || ltpbsz != pbi - pbv || ltlm != lm || ltvlpz != vlpz || lthlpz != hlpz) {
			int dh = 0;						// depth iterator
			int vs = 0;						// vertical screen size
			int hs = 0;						// horizontal screen size
			int pbsz = 0;					// pixel-buffer size
			ltlmdh = LMMXDH - 1;
			for (dh = 0; dh < LMMXDH; ++dh) {
				display_2d_lindenmeyer(lm, NULL, NULL, 8192 * 4, 8192 * 4, vlpz, hlpz, pbv, pbi, lmsw, dh, &vs, &hs, &pbsz);
				/* sufficient depth found */
				if (pbsz >= pbi - pbv) {
					ltlmdh = dh;
					break;
				}
			}
			printf("LM autosize  dh %d  sz %dx%d pbsz %d\n", dh, vs / rc, hs / rc, pbsz);
			/* Remember results and for which parameters they have been calculated */
			ltstpn = (plh / 2 - vs / rc) * rc * plw / rc * rc + (paw / 2 - hs / rc) * rc;		// vertical start position must be an even line
			ltsnsz = pni - pnv;
			ltlm = lm;
			ltvlpz = vlpz, lthlpz = hlpz;
			ltpbsz = pbi - pbv;
		}
		/* use stored depth and start postion */
		lmdh = ltlmdh;
		rpnc = ltstpn;
	}

	if (lmpbsz != NULL) {
		*lmpbsz = 0;
		plh = plw;
		rpnc = plh / 2 * plw + plw / 2;
	}

	int vlpzlw = vlpz * plw;				// pre-calc vlpz * plw
	int avc = 0;							// angle-cursor
	int avs[8];								// angle movement vectors
	const int alc = lm[0];					// angle count (4 = tetragonal, 6 = hexagonal)
	// hexagonal
	if (alc == LMH)
		avs[0] = +sg * hlpz, avs[1] = +sg * plw * vlpz + hlpz, avs[2] = +sg * plw * vlpz - hlpz,
		avs[3] = -sg * hlpz, avs[4] = -sg * plw * vlpz - hlpz, avs[5] = -sg * plw * vlpz + hlpz;
	//avs[0] = -2*lw, avs[1] = -1*lw +2, avs[2] = +1*lw +2,
	//avs[3] = +2*lw, avs[4] = +1*lw -2, avs[5] = -1*lw -2;
// tetragonal
	else if (alc == LMT)
		avs[0] = +sg * hlpz, avs[1] = +sg * plw * vlpz, avs[2] = -sg * hlpz, avs[3] = -sg * plw * vlpz;
	// octagonal
	else if (alc == LMO)
		avs[0] = +sg * hlpz, avs[1] = +sg * plw * vlpz + sg * hlpz, avs[2] = +sg * plw * vlpz, avs[3] = +sg * plw * vlpz - sg * hlpz,
		avs[4] = -sg * hlpz, avs[5] = -sg * plw * vlpz - sg * hlpz, avs[6] = -sg * plw * vlpz, avs[7] = -sg * plw * vlpz + sg * hlpz;

	int c;
	int cc = 0;
	int pns[LMMXDH];
	lmdh = min(lmdh, LMMXDH - 1);			// make sure depth it within limits
	int n = lmdh;							// current depth
	pns[n] = 1;

	int vsn = MAXINT, vsx = MININT, hsn = MAXINT, hsx = MININT;		// size min and max

	//printf("rpni %d\n", rpni);
	while (1) {
		c = lm[pns[n]];
		++pns[n];
		// Forward
		if (c == LMF || c == LMD) {
			avc %= alc;
			if (c == LMF) {
				rpnc += lmsw * avs[avc];
			}
			else {
				for (int swi = 0; swi < lmsw; ++swi) {
					/* draw */
					int trpnc = rpnc;
					if (lmvs == NULL) {
						if (pbc >= pbi)
							return;
						int crpnc = trpnc / rc;			// corrected position
						if (crpnc >= 0 && crpnc < rpni)
							for (int v = 0; v < vlpzlw; v += plw)
								for (int h = 0; h < hlpz; ++h)
									pnv[crpnc + v + h] = *pbc;
					}
					/* get size */
					else {
						if (pbc >= pbi || lmhs == NULL || lmpbsz == NULL) {
							//printf("sz %d\n", pbi - pbv);
							break;
						}
						int vs = trpnc / plw,
							hs = trpnc % plw;
						if (vs < vsn) vsn = vs;
						if (vs > vsx) vsx = vs;
						if (hs < hsn) hsn = hs;
						if (hs > hsx) hsx = hs;
						*lmvs = (vsx - vsn) / 2 + vsn - plh / 2 + vlpz;
						*lmhs = (hsx - hsn) / 2 + hsn - plw / 2 + hlpz;
						*lmpbsz += 1;
					}
					//
					++pbc;
					rpnc += avs[avc];
				}
			}
		}
		// End of pattern
		else if (c == LME) {
			++n;
			if (n > lmdh)
				break;
		}
		// Ignore if not at depth 0
		else if (c == LM0) {
			if (n)
				++pns[n];
		}
		// Always ignore
		else if (c == LMI) {
		}
		// Rotate
		else if (c > 0) {
			avc += c;
		}
		// Replace
		else if (n > 0) {
			--n;
			pns[n] = -c;
		}
	}
}

void
display_2d_matze(
	UINT32* pnv,												// pixel-screen first valid element
	UINT32* pni,												// pixel-screen first invalid element
	const int plw,												// pixel-screen line-width
	const int paw,												// pixel-screen available with
	const int vlpz,												// vertical-pixel-zoom
	const int hlpz,												// horizontal-pixel-zoom
	const UINT32* pbv,											// pixel-buffer first valid element
	const UINT32* pbi,											// pixel-buffer first invalid element
	const int md,												// mode - 0: line, 1: morton, 2: matze-fractal, 3: grid
	const int mdpm												// mode parameter - may be used differently by different modes - morton: > 0 display mdpm - 1 layer of 3d morton-cube
) {
	const UINT32 pbs = pbi - pbv;								// pixel-buffer-size
	UINT32* pbc = pbv;											// pixel-buffer cursor
	double level = log2(pbi - pbv) / 2.0;
	int ysz, xsz;
	ysz = xsz = ipow(2, level);
	if (md == 0 && mdpm) {
		int ds = pow(pbs, 1.0 / 3.0) + 5;						// dimension-size
		int hb = min(pow(pbs, 1.0 / 6.0) + 0.01, plw / ds - 1);	// horizontal-blocks
		//printf("%e  %d\n", pow(pbs, 1.0 / 6.0), hb);
		ysz = xsz = hb * ds;
		ysz = (ds - 5) / hb * ds;
	}

	if (md == 0)
		xsz = ysz = ceil(sqrt(pbs));
	else {
		if (fmod(level, 1.0) > 0.0)
			xsz *= 2;
		if (fmod(level, 1.0) > 0.5)
			ysz *= 2;
	}
	int h = (pni - pnv) / plw,									// height in plw-sized lines
		sy = h / 2 - ysz * vlpz / 2,
		sx = paw / 2 - xsz * hlpz / 2,
		cr = sy * plw + sx;										// transpose needed to center
	if (cr < 0) {
		cr = 0;	// TODO center
	}
	int vlpzplw = vlpz * plw;									// pre-calc vlpz * plw
	int rpnc = cr;												// pixel-screen position (relative, no pointer)
	pni -= plw * (vlpz - 1) + (hlpz - 1);						// correct pixel-screen first invalid for pixel-zoom (so we dont have to check for every zoomed pixel)

	// Linear-Memory-Layout
	if (md == 0) {
		if (!mdpm) {
			if (vlpz < 2 && hlpz < 2) {
				int b, bh, bv;
				_BitScanForward(&b, pbs);
				bh = bv = b >> 1;
				if (pbs > 1 << (bh + bv))
					bh++;
				if (pbs > 1 << (bh + bv))
					bv++;
				bh = 1 << bh;
				bv = 1 << bv;
//printf("pbs %d  %d  %d  %d\n", pbs, b, bh, bv);
				UINT32* pnc = pnv + rpnc - plw + bh;					// pixel-screen current element
				UINT32* pneol = pnc;									// pixel-screen end of current line
				while (1) {
					if (pnc >= pneol) {
						pnc += plw - bh;
						if (pbc >= pbi || pnc >= pni)
							return;
						pneol = min(pni, min(pnc + bh, pnc + (pbi - pbc) - 1));
						//printf("pbc %p  pbi %p  pnc %p  pni %p  pneol %p\n", pbc, pbi, pnc, pni, pneol);
					}
					else {
						*pnc = *pbc;
						++pnc;
						++pbc;
					}
				}
			}
			else {
				int sz = ceil(sqrt(pbs));
				for (int v = 0; v < sz * vlpzplw; v += vlpzplw)
					for (int h = v; h < v + sz * hlpz; h += hlpz) {
						UINT32* pnc = pnv + rpnc + h;					// pixel-screen current element
						if (pnc >= pnv && pnc < pni)
							for (int vv = 0; vv < vlpzplw; vv += plw)
								for (int hh = 0; hh < hlpz; ++hh)
									pnc[vv + hh] = *pbc;
						++pbc;
						if (pbc >= pbi)
							return;
					}
			}
		}
		else {
			int cs = pbi - pbv;
			UINT32 ds = pow(cs, 1.0 / 3.0) + 0.001;								// size of one dimension
			int dssg = ds + 5;											// size of one dimension + spacing
			int hb = min(pow(cs, 1.0 / 6.0) + 0.001, plw / dssg - 1);		// nr. of horizontal-blocks
			int mx = 0, tx = 0;
			int my = 0, ty = 0;
			for (UINT32 i = 0; i < cs; ++i) {
				UINT32 z, y, x;
				z = i / (ds * ds);
				y = (i % (ds * ds)) / ds;
				x = (i % (ds * ds)) % ds;
				tx = z % hb;
				ty = z / hb;

				//if (random01() > 0.99999)
				//	printf("i %u  z %u  y %u  x %u  ty %u  tx %u  ds %u  cs %d ds %e\n", i, z, y, x, ty, tx, ds, cs, pow(cs, 1.0 / 3.0));
								//if (z == mdpm - 1) {
				UINT32* pnc = pnv + cr + (y + dssg * ty) * vlpz * plw + (x + dssg * tx) * hlpz;				// pixel-screen current element
				if (pnc >= pnv && pnc < pni)
					for (int v = 0; v < vlpzplw; v += plw)
						for (int h = 0; h < hlpz; ++h)
							pnc[v + h] = *pbc;
				//}

				++pbc;
				if (pbc >= pbi)
					return;
			}
		}
		return;
	}
	// Morton-Code-Layout
	if (md == 1) {
		int cs = pbi - pbv;
		if (!mdpm) {
			for (UINT32 i = 0; i < cs; ++i) {
				UINT32 y, x;
				MortonDecode2(i, &x, &y);										// CPU-DEPENDANT !
				UINT32* pnc = pnv + cr + y * vlpz * plw + x * hlpz;				// pixel-screen current element
				if (pnc >= pnv && pnc < pni)
					for (int v = 0; v < vlpzplw; v += plw)
						for (int h = 0; h < hlpz; ++h)
							pnc[v + h] = *pbc;
				++pbc;
				if (pbc >= pbi)
					return;
			}
		}
		else {
			int ds = pow(cs, 1.0 / 3.0) + 5;			// dimension-size
			int hb = min(pow(cs, 1.0 / 6.0) + 0.001, plw / ds - 1);					// nr. of horizontal-blocks
			int mx = 0, tx = 0;
			int my = 0, ty = 0;
			for (UINT32 i = 0; i < cs; ++i) {
				UINT32 z, y, x;
				MortonDecode3(i, &x, &y, &z);										// CPU-DEPENDANT !
				//MortonDecode3(i, &z, &x, &y);										// CPU-DEPENDANT !
				tx = z % hb;
				ty = z / hb;
				//if (z == mdpm - 1) {
				UINT32* pnc = pnv + cr + (y + ds * ty) * vlpz * plw + (x + ds * tx) * hlpz;				// pixel-screen current element
				if (pnc >= pnv && pnc < pni)
					for (int v = 0; v < vlpzplw; v += plw)
						for (int h = 0; h < hlpz; ++h)
							pnc[v + h] = *pbc;
				//}

				++pbc;
				if (pbc >= pbi)
					return;
			}
		}
	}
	// Matze-Layout
	else if (md == 2) {
		int rpni = pni - pnv - plw * (vlpz - 1) - (hlpz - 1);				// pixel-screen first invalid position, corrected for zoom (relative, no pointer)
		int sd = level;														// start depth
		int dh = sd;														// depth
		int pd[16];															// position at depth
		pd[dh] = 0;
		int ag = 0;															// angle

	draw:;
		if (!dh) {
			if (rpnc >= 0 && rpnc < rpni)
				for (int v = 0; v < vlpzplw; v += plw)
					for (int h = 0; h < hlpz; ++h)
						pnv[rpnc + v + h] = *pbc;
			++pbc;
			if (pbc >= pbi)
				goto exit_draw;
		}
		else {
			--dh;
			pd[dh] = 0;
			goto draw;
		}
	move:;
		int sw = (2 << dh) - 1;		// step width
		switch (ag) {
		case 4: ag = 0;						// no break wanted here!
		case 0: rpnc += sw * hlpz;			break;
		case 1: rpnc += sw * plw * vlpz;	break;
		case 2: rpnc -= sw * hlpz;			break;
		case 3: rpnc -= sw * plw * vlpz;	break;
		}
		//printf("dh  %d   sw  %d   rpnc  %d   ag  %d   pn  %d x %d\n", dh, sw, pd[dh], ag, rpnc / plw, rpnc % plw);
		//
		++ag;
		++pd[dh];
		if (pd[dh] < 4)
			goto draw;
	finish_level:;
		if (dh < sd) {
			++dh;
			goto move;
		}
	exit_draw:;
	}
	else {
		// grid memory layout
		int s1z = ceil(sqrt(pbs));											// space size 1d in pixels
		s1z = ceil(pow(pbs, 1.0 / 3.0));
		int g1z = ceil(pow(s1z, 0.5));
		//printf("s1z %d  g1z  %d\n", s1z, g1z);
				//
		int s2z = s1z * s1z;												// space size 2d in pixels

		for (int gy = 0; gy < g1z; ++gy) {
			for (int gx = 0; gx < g1z; ++gx) {
				for (int sy = 0; sy < s1z; ++sy) {
					for (int sx = 0; sx < s1z; ++sx) {
						int lmc = (gy * (2 + s1z) + sy) * vlpz * plw +
							(gx * (2 + s1z) + sx) * hlpz;
						// pixel-zoom loops
						for (int vv = 0; vv < vlpzplw; vv += plw)
							for (int hh = 0; hh < hlpz; ++hh) {
								UINT32* pnc = pnv + cr + lmc + vv + hh;									// pixel-screen current element
								if (pnc >= pnv && pnc < pni)
									*pnc = *pbc;
							}

						++pbc;
						if (pbc >= pbi)
							return;
					}
				}
			}
		}

		//for (int gc = 0; g0c < g1n; ++g0c) {								// grid memory cursor / iterator
		//	int g2c = g0c / g2z;											// grid 2d cursor
		//	int g1c = g0c % g2z;											// grid 1d cursor - relative block cursor = position within block
		//	int lmc = ((g2c / g1n) * g1z + g1c / g1z) * vlpz * plw +		// linear memory cursor
		//			  ((g2c % g1n) * g1z + g1c % g1z) * hlpz;
		//	UINT32 *pnc = pnv + cr + lmc;									// pixel-screen current element
		//	if (pnc >= pnv && pnc < pni)
		//		for (int vv = 0; vv < vlpzplw; vv += plw)					// pixel-zoom loops
		//			for (int hh = 0; hh < hlpz; ++hh)
		//				pnc[vv + hh] = *pbc;
		//	++pbc;
		//	if (pbc >= pbi)
		//		return;
		//}
	}
}

void
display_2d_chaotic(
	UINT32* pnv,							// pixel-screen first valid element
	UINT32* pni,							// pixel-screen first invalid element
	const int plw,							// pixel-screen line-width
	const int dplw,							// pixel-screen display-line-width (right-aligned, so starts at plw - dplw and is dplw wide
	CA_CT* csv,						// cells first valid element
	CA_CT* csi,						// cells first invalid element
	const UINT32* pbv,						// pixel-buffer first valid element
	const UINT32* pbi,						// pixel-buffer first invalid element
	const int res							// reset flag
) {
	int h = (pni - pnv) / plw;
	static UINT32* spnc = NULL;
	if (res || spnc == NULL)
		spnc = pnv + (h / 2) * plw + dplw / 2;
	UINT32* pnc = spnc;
	const int avs[4] = { [0] = 0 - plw,[1] = +1,[2] = +plw,[3] = -1 };
	static int avc = 0;
	UINT32* pbc = pbv;						// pixel-buffer cursor
	for (CA_CT* csc = csv; csc < csi; ++csc) {
		if (*csc == 1)
			avc = (avc + 1) % 4;
		else if (*csc == 2)
			avc = (avc + 3) % 4;
		pnc += avs[avc];
		++pbc;
		if (pnc < pnv || pnc >= pni || pbc >= pbi)
			continue;
		*pnc = *pbc;
	}
	//spnc = pnc;
}

/* Compares two double values*/
int
dblcmpfunc(const void* a, const void* b) {
	if (*(double*)a < *(double*)b)
		return -1;
	if (*(double*)a > *(double*)b)
		return +1;
	return 0;
}


void
lifeanddrawnewcleanzoom(
	/*
	pixel-screen is a 4-byte rgba pixel array to draw to

	does scrolling itself!
	*/
	CA_RULE* cr,										// ca-rule configuration
	CA_CT* sc,											// space
	uint64_t scsz,											// space-size
	int64_t tm,											// time - nr of generations to evolve
	int res,											// ca-space-reset-flag
	int dyrt,											// display-reset-flag
	UINT32* pnv,										// pixel-screen first valid element
	const UINT32* pni,									// pixel-screen first invalid element
	int de,												// drawing enabled
	caComputationMode cnmd,								// computation-mode
	const unsigned int klrg,							// kernel-range (GPU mode)
	caDisplaySettings ds								// display settings
)
{
	/* Abort if time is zero */
	if (tm <= 0)
		return;

	/* Check for maximum size of rule tables */
	if (cr->nns > LV_MAXRLTLSZ) {
		printf("ERROR max rule table size to small!");
		return;
	}
	/* Check if computation-function is valid */
	if (ca_cnsgs[cnmd].cnfn == NULL) {
		printf("ERROR ca_cnfn is undefined  cnmd %d / %s!\n", cnmd, ca_cnsgs[cnmd].name);
		return;
	}
	/* Warn if hash-computation-mode is used with inappropiate settings */
	if (cnmd == CM_HASH && !ds.sfcm)
		printf("WARNING you need to enable screen-filling-curve-mode when using hash-computation-mode!\n");
	// i guess not needed anymore, since sdmrpt is used in hash mode to control speed
	//	if (cnmd == CM_HASH && tm < scsz / 2)
	//		printf("WARNING speed must at least be half of space-size when using hash-computation-mode!  tm %d\n", tm);
	//	if (cnmd == CM_HASH && ((tm & (tm - 1)) != 0 || (scsz & (scsz - 1)) != 0))
	//		printf("WARNING speed and size must be power of 2 when using hash-computation-mode!\n");

		/* Init vars and defines */
	ds.lgm %= 2;
	if (!ds.vlfs || !ds.hlfs)
		ds.stzm = 0;
	int tma = 0;										// time available (as stored in pixel buffer)
	UINT32* pnc = pni;									// pixel-screen cursor / current position

	ds.hlzm = max(1, ds.hlzm);							// make sure zoom-levels are at least 1
	ds.vlzm = max(1, ds.vlzm);
	ds.hlpz = max(1, ds.hlpz);
	ds.vlpz = max(1, ds.vlpz);

	int hwmn = 50;										// histogram-width-minimum
	int hw = hwmn;										// histogram width
	if (ds.sfcm)										// autosize of histogram-width when screen-filling-curve-mode is activated
		hw = max(hwmn, ds.plw - ceil(sqrt(scsz)) * ds.hlpz * 2);

	if (ds.fsme == 2 && cnmd != CM_HASH)				// focus-mode can only be in hash-mode when computing-mode is in hash-mode as well
		ds.fsme = 0;

	int pbs = 0;										// pixel-buffer-size (one line / space-size div horizontal-zoom)
	static int last_pbs = 0;
	if (ds.fsme == 2)
		pbs = scsz >> (HCTBSPT + ds.hdll);
	else
		pbs = scsz / ds.hlzm;
	pbs = max(1, pbs);
	const int stst = 0;									// state shift (nr. of state bits shifted out / disregarded)

	ds.vlzm = max(ds.vlzm, ds.vlfs);					// vertical zoom can not be larger than zoom since it not possible (atm) to go back to previous generatios=lines
	if (ds.vlfs == 0 || ds.hlfs == 0)
		ds.hlfs = ds.vlfs = 1;

	/* size of one buffer
	buffer must be large enough to always allow for readahead for next-gen-calculation (cr->ncn)
	or drawing-calucaltions (hlfs) */
	int brsz = max(ds.hlfs - 1, cr->ncn - 1);			// buffer-size
	// #TODO !! ??? Why is max 32 not enough???
	brsz += 32;
	brsz = max(1024, brsz);



	/* Scanline-mode initialization */
	static UINT32* spnc = NULL;							// static pixel screen cursor; needed for scanline mode
	if (!spnc || dyrt)
		spnc = pnv;

	/* Inititalize pixel-display-cursor */
	// screen-filling-curve mode
	if (ds.sfcm) {
		pnc = pnv;
	}
	// scroll-mode (scanline-mode disabled)
	else if (!ds.sm) {
		/* Scroll pixel-screen to make room to draw tm pixels */
		int64_t vlsl = tm / ds.vlzm * ds.vlpz;					// vertical scroll / lines to scroll
		tm = vlsl * ds.vlzm / ds.vlpz;
		int hlsl = 0;											// horizontal scroll
		///hw = 0;
				// horizontal scrolling is disabled atm since it does not work together with the histogram
				// and is only possible when output-width and display-width are the same
				// if (pbs == plw)
				//     hlsl = min(plw, (tm % (scsz * vlzm)) / max(1, vlzm * hlzm));				// horizontal scroll / nr. of pixels to scroll
		if (vlsl * ds.plw < pni - pnv) {
			pnc = max(pnv, pni - vlsl * ds.plw - hlsl);			// set cursor to first pixel that has to be drawn
			scroll(pnv, (pni - pnv) * 4, -4 * (pni - pnc));		// scroll is measured in nr of pixels > convert to argb-pixel with size of 4 byte
		}
		else
			pnc = pnv;
	}
	// scanline-mode
	else {
		pnc = spnc;
	}

	/* Clear screen on reset */
	if (dyrt || res) {
		UINT32* tpnc;
		if (!ds.sm)												// only clear area that will be drawn to and leave scrolled away area intact when in scroll-mode
			tpnc = pnc;
		else													// clear whole screen when in scanline-mode or screen-filling-curve-mode
			tpnc = pnv;
		for (; tpnc < pni; ++tpnc)
			*tpnc = 0;
	}

	/* Init pixel-buffer */
	static UINT32* pbv = NULL;									// pixel-buffer first valid element
	static UINT32* pbi = NULL;									// pixel-buffer first invalid element
	static UINT32* pbc = NULL;									// pixel-buffer cursor / current element
	if (!pbv || last_pbs != pbs || dyrt) {
		last_pbs = pbs;
		_aligned_free(pbv);
		pbv = _aligned_malloc(pbs * sizeof * pbv, BYAL);
		pbc = pbi = pbv + pbs;
	}
	/* Init cell-space-buffer */
	static CA_CT* csv = NULL;									// cell-array first valid element
	static CA_CT* csi = NULL;									// cell-array first invalid element
	static CA_CT* csf = NULL;									// position of first cell, corrected for shifting
	if (!csv || res) {
		_aligned_free(csv);
		csf = csv = _aligned_malloc((scsz + brsz) * sizeof * csv, BYAL);
		csi = csv + scsz;
		memcpy(csv, sc, scsz * sizeof * csv);
	}
	/* Init testing-cell-space-buffer */
	static CA_CT* tcsv = NULL;									// test-cell-array first valid element
	// temporary code for debugging
	static caBitArray tvba = { 0 };
	// end temporary code
	if (ds.tm == 2) {
		_aligned_free(tcsv);									// aligned memory management may be necesary for use of some intrinsic or opencl functions
		tcsv = _aligned_malloc((scsz + cr->ncn - 1) * sizeof *tcsv, BYAL);
		memcpy(tcsv, csv, scsz * sizeof *tcsv);
		// temporary code for debugging
		if (cnmd == CM_HASH) {
			if (tvba.v == NULL || res) {
				_aligned_free(tvba.v);									// aligned memory management may be necesary for use of some intrinsic or opencl functions
				tvba = (const struct caBitArray){ 0 };
				tvba.cr = cr;
				tvba.clsc = tcsv;
				tvba.scsz = scsz;
				tvba.brsz = brsz;
				CA_CNITFN_VBA_1x256(&tvba, cr);
			}
		}
		// end temporary code
	}

	/* Init bit-array-cell-space-buffer */
	static caBitArray vba = { 0 };
	if (vba.v == NULL || res) {
		_aligned_free(vba.v);									// aligned memory management may be necesary for use of some intrinsic or opencl functions
		vba = (const struct caBitArray){ 0 };
		vba.cr = cr;
		vba.clsc = csv;
		vba.scsz = scsz;
		vba.brsz = brsz;

		if (ca_cnsgs[cnmd].itfn != NULL) {
			printf("CA_CNITFN  %s\n", ca_cnsgs[cnmd].name);
			(ca_cnsgs[cnmd].itfn)(&vba, cr);

			/* Test cell-space conversion */
			if (ds.tm == 2) {
				if (ca_cnsgs[cnmd].scfn != NULL) {
					printf("Testing cell-space-conversion\n");
					printf("\tCA_SCFN  %s\n", ca_cnsgs[cnmd].name);
					CA_CT* ctcsv = NULL;
					ctcsv = malloc(scsz * sizeof * ctcsv);
					vba.clsc = ctcsv;
					(ca_cnsgs[cnmd].scfn)(&vba);
					vba.clsc = csv;
					int mcr = memcmp(csv, ctcsv, scsz * sizeof * csv);
					free(ctcsv);
					if (mcr != 0)
						printf("\tERROR  mcr %d  sz %d\n", mcr, scsz * sizeof * csv);
					else
						printf("\tsuccess\n");
				}
			}
		}
		else {
			printf("CA_CNITFN  %s  IS NULL\n", ca_cnsgs[cnmd].name);
			vba.v = _aligned_malloc(1, BYAL);					// needed, so that the initalization block is not called repeatetly - TODO: improve this
		}
		///		CA_PrintSpace("org csv", csv, csi - csv);
///		CA_PrintSpace("org csv", tcsv, scsz);
///		printf("csv memcmp  %d\n", memcmp(csv, tcsv, scsz * sizeof * csv));

///		print_bitblock_bits(tba.v, tba.ct / BPB + tba.oc * tba.rw / BPB, tba.rw / BPB);
///		CABitArrayPrepareOR(&tba);
		//print_bits(tba.v, tba.ct / 8 + tba.oc * tba.rw / 8, tba.rw / 8);
///		print_bitblock_bits(tba.v, tba.ct / BPB + tba.oc * tba.rw / BPB, tba.rw / BPB);
	}

	/* Init color table */
	int dyss;
	switch (ds.fsme) {
	case 0:
		dyss = max(1, (cr->ncs * (max(1, max(1, ds.hlfs) * max(1, ds.vlfs)))) >> stst);
		break;
	case 1:
		dyss = max(1, ipow(cr->ncs, ds.hlfs * ds.vlfs) >> stst);
		break;
	default:
		dyss = 0;
	}
	static UINT32* cltbl = NULL;							// color table
	if (dyss && (!cltbl || dyrt)) {
		free(cltbl);
		cltbl = malloc(dyss * sizeof * cltbl);
	}

	/* Init state count table */
	static UINT32* sctbl = NULL;							// state count table
	static double* dsctbl = NULL;							// double state count table
	static double* tdsctbl = NULL;							// temp double state count table
	if (!sctbl || dyrt) {
		free(sctbl);
		sctbl = malloc(dyss * sizeof * sctbl);
		free(dsctbl);
		dsctbl = malloc(dyss * sizeof * dsctbl);
		free(tdsctbl);
		tdsctbl = malloc(dyss * sizeof * tdsctbl);
	}
	//
	static double* spt = NULL;								// state position table
	static double* svt = NULL;								// state velocity table
	static int ltdyss = 0;
	if (ltdyss != dyss) {
		ltdyss = dyss;
		spt = malloc(dyss * sizeof * spt);
		svt = malloc(dyss * sizeof * svt);
		for (int i = 0; i < dyss; ++i) {
			spt[i] = 0.5;
			svt[i] = 0.0;
		}
	}

	/* */
	double dmn;												// double min
	double dmx;												// double max

	int64_t otm = tm;										// original time

	while (1) {
		/* DISPLAY - Copy as much pixels as possible from the pixel-buffer to the pixel-screen.
		   There are 3 drawing modes:
		   * 1. 1d-scroll-mode - new ca-lines are drawn at bottom of screen, old ca-lines scroll up
		   * 2. 1d-scanline-mode - a scanline moves repeatedly from top to bottom, new ca-lines are drawn at current scanline position
		   * 3. screen-filling-curve-mode - the 1d ca-line is drawn on the 2d screen using one of several screen-filling-curves
		*/
		if (de && pbc < pbi && pnc < pni) {
			/* Line-mode (scroll or scanline) */
			if (!ds.sfcm) {
				for (int vi = 0; vi < ds.vlpz; ++vi) {
					int cs = min(ds.plw, min((pni - pnc), ds.hlpz * (pbi - pbc)));		// size of copy
					/* Horizontal-centering */
					if (pbs * ds.hlpz > ds.plw - hw)
						pbc = pbv + (pbs * ds.hlpz - ds.plw - hw) / 2 / ds.hlpz;
					if (pbs * ds.hlpz < ds.plw - hw)
						pnc += hw + (ds.plw - pbs * ds.hlpz - hw) / 2;
					/* Copy bixel-buffer to pixel-screen */
					// horizontal-zoom enabled
					if (ds.hlpz > 1) {
						for (int hi = 0; hi < cs / ds.hlpz; ++hi) {
							for (int hzi = 0; hzi < ds.hlpz; ++hzi) {
								*pnc = *pbc;
								++pnc;
							}
							++pbc;
						}
					}
					// horizontal-zoom disabled
					else {
						memcpy(pnc, pbc, cs * 4);
						pbc = pbi;
						pnc += cs;
					}
					/* Set pixel-screen-cursor to next line */
					// scanline-mode disabled (scrolling enabled)
					if (!ds.sm) {
						pnc += (pni - pnc) % ds.plw;
						if (pnc >= pni)								// disable drawing if one complete screen (all available lines) has been drawn
							de = 0;
					}
					// scanline-mode enabled
					else {
						if (pbs < ds.plw)
							pnc = pnv + ((pnc - pnv) / ds.plw + 1) * ds.plw;
						if (pnc >= pni)
							pnc = pnv;
						if (pnc == spnc)							// disable drawing if one complete screen (all available lines) has been drawn
							de = 0;
					}
					// reset pixel-buffer-cursor
					pbc = pbv;
				}
			}
			/* Screen-filling-curve-modes */
			else if (ds.sfcm <= 4)
				display_2d_matze(pnv + hw, pni, ds.plw, ds.plw - hw, ds.vlpz, ds.hlpz, pbv, pbi, ds.sfcm - 1, ds.sfcmp);
			else if (ds.sfcm < 5 + LMS_COUNT)
				display_2d_lindenmeyer(LMSA[ds.sfcm - 5], pnv + hw, pni, ds.plw, ds.plw - hw, ds.vlpz, ds.hlpz, pbv, pbi, ds.sfcsw, -1, NULL, NULL, NULL);
			else
				display_2d_chaotic(pnv + hw, pni, ds.plw, ds.plw - hw, csv, csi, pbv, pbi, dyrt);
		}

		if (tm < 1)
			break;

		/* EVOLVE - Calculate next generation(s) of CA */
		for (int vlp = 0; vlp < ds.vlzm; ++vlp) {
			if (de && ds.fsme < 2) {
				if (vlp == 0) {
					/* Reset pbv accumulate buffer */
					memset(pbv, 0, (pbi - pbv) * sizeof * pbv);
				}
				if (vlp == 0 || vlp < ds.vlfs) {
					/* Count states */
					ca_count__simd(csv, csf, csi, pbv, pbi, ds.hlzm, ds.hlfs, ds.fsme, cr->ncs);
				}
			}
			/* Calculate next generation */
			int64_t ognc, gnc;							// (original) generations (scsz lines) to calculate
			ognc = tm;
			if (vlp < ds.vlfs - 1 || (!ds.sfcm && de))
				ognc = min(ognc, 1);
			gnc = ognc;
			// run selected computation function
			gnc = (ca_cnsgs[cnmd].cnfn)(gnc, &vba);
			//
			tm -= (ognc - gnc);											// adjust time
			csf -= ((ognc - gnc) * (cr->ncn / 2) + ds.mlsc) % scsz;		// correct for shift
			if (csf < csv)
				csf += scsz;
			if (csf >= csi)
				csf -= scsz;
		}

		if (de) {
			if (ds.fsme == 2 && cnmd == CM_HASH) {
				#define CRTLSZ (1 << 10)
				#define LOGPRECISION 4
				#define LOGSCALE (1U<<LOGPRECISION)

				unsigned int lgmd = ds.lgm;										// log-mode

				// Color-table.
				static UINT32 crtl[CRTLSZ] = { 0 };
				if (dyrt)
					for (int i = 0; i < CRTLSZ; i++)
						crtl[i] = getColor((double)i / (CRTLSZ - 1.0), ds.cm, ds.crct, ds.gm);

				//
				UINT32 mxv = 0;													// max v
				UINT32 mnv = 0;													// min v
				UINT32 rgv = 1;													// range v

				display_hash_array(pbv, pbv, pbi, hc_sl, hc_sn, &mxv, &mnv);

				rgv = max(1, mxv - mnv);
				//double mnlgv = log2((double)mnv);
				//double mxlgv = log2((double)mxv);
				double mnlgv;
				double mxlgv;
				double rglgv;
				if (lgmd) {
					mnlgv = (double)(log2fix(mnv << LOGPRECISION, LOGPRECISION)) / LOGSCALE;
					mxlgv = (double)(log2fix(mxv << LOGPRECISION, LOGPRECISION)) / LOGSCALE;
					rglgv = max(1.0, mxlgv - mnlgv);
				}

				//if (!(rand()%10))
				//	printf("mnv %u  mxv %u   fxlgmnv %f  fxlgmxv %f   lgmnv %f  lgmxv %f\n", mnv, mxv, mnlgv, mxlgv, log2(mnv), log2(mxv));

				double v;
				UINT32 lpbc = *pbc + 1, lcol = 0;
				for (UINT32* pbc = pbv; pbc < pbi; pbc++) {
					if (*pbc == lpbc)
						*pbc = lcol;
					else {
						if (lgmd) {
							//printf("%.2e  %.2e\n", (((double)log2fix(*pbc* LOGSCALE, LOGPRECISION)) / LOGSCALE), log2((double)*pbc));
							v = ((((double)log2fix(*pbc << LOGPRECISION, LOGPRECISION)) / LOGSCALE) - mnlgv) / rglgv;
							//v = (log2((double)*pbc) - mnlgv) / rglgv;
						}
						else
							v = ((double)*pbc - (double)mnv) / (double)rgv;
						lpbc = *pbc;
						*pbc = crtl[(int)(v * (CRTLSZ - 1) + .5)];
						lcol = *pbc;
					}
				}
			}
			else {
				// Sync cell-space
				if (ca_cnsgs[cnmd].scfn)
					ca_cnsgs[cnmd].scfn(&vba);
				// Reset state-count-table
				for (int i = 0; i < dyss; ++i)
					sctbl[i] = 0;
				// Count states
				if (ds.stzm || ds.ar)
					for (UINT32* pbc = pbv; pbc < pbi; ++pbc)
						++sctbl[(*pbc) >> stst];
				// Init min and max and state count table
				dmn = 1.0;
				if (!ds.stzm) {
					dmx = (double)dyss;
					for (int i = 0, cnt = 0; i < dyss; ++i)
						if (!ds.ar || (ds.ar && sctbl[i])) {
							++cnt;
							dsctbl[i] = (double)(cnt);
							sctbl[i] = cnt;
						}
						else
							dsctbl[i] = 0.0;
				}
				else {
					dmx = (double)(pbi - pbv);
					if (!ds.ar && ds.lgm)
						if (ds.cmm)
							dmx = dyss * (log2(dmx / dyss) + 1.0);
						else
							dmx = log2(dmx) + 1.0;
					// Convert to double
					for (int i = 0; i < dyss; ++i)
						dsctbl[i] = (double)sctbl[i];
					// Log mode
					if (ds.lgm) {
						for (int i = 0; i < dyss; ++i) {
							if (dsctbl[i] > 0.0)
								dsctbl[i] = log2(dsctbl[i]) + 1.0;
						}
					}
					// Cumulative modes
					if (ds.cmm == 1) {
						///qsort(dsctbl, dyss, sizeof(double), dblcmpfunc);
						// use temp table to store sctbl while calculating cumulative sums
						for (int i = 0; i < dyss; ++i)
							tdsctbl[i] = dsctbl[i];
						//
						for (int i = 0; i < dyss; ++i)
							if (dsctbl[i] > 0.0) {
								double sm = 0.0;
								for (int k = 0; k < dyss; ++k)
									if (tdsctbl[k] < dsctbl[i] || ((tdsctbl[k] == dsctbl[i]) && (i <= k)))
										sm += tdsctbl[k];
								dsctbl[i] = sm;
							}
					}
					else if (ds.cmm == 2) {
						for (int i = 1; i < dyss; ++i)
							dsctbl[i] += dsctbl[i - 1];
					}
					else if (ds.cmm == 3) {
						//
						double pos = 1.0;
						for (int i = 0; i < dyss; ++i) {
							double d = dsctbl[i];
							if (d > 0.0) {
								dsctbl[i] = pos + d / 2.0;
								pos += d;
							}
						}
						dmn = 1.0;
						dmx = pos;
					}
				}
				// Get min and max of state count table
				if (ds.ar && ds.cmm != 3) {
					if (ds.ar == 1 || ds.ar == 3)
						dmn = DBL_MAX;
					if (ds.ar == 2 || ds.ar == 3)
						dmx = 0.0;
					for (int i = 0; i < dyss; ++i)
						if (dsctbl[i] > 0.0) {
							if (dsctbl[i] < dmn)
								dmn = dsctbl[i];
							if (dsctbl[i] > dmx)
								dmx = dsctbl[i];
						}
				}
				// Convert to range 0.0 - 1.0
				double rng = dmx - dmn;
				if (rng == 0.0)
					rng = 1.0;
				for (int i = 0; i < dyss; ++i)
					if (dsctbl[i] > 0.0)
						dsctbl[i] = (dsctbl[i] - dmn) / rng;
				// Translate and spread
				for (int i = 0; i < dyss; ++i) {
					dsctbl[i] = dsctbl[i] + ds.te;
					dsctbl[i] = max(0.0, dsctbl[i]);
					dsctbl[i] = min(1.0, dsctbl[i]);
					if (ds.sd <= 1.0)
						dsctbl[i] = pow(dsctbl[i], ds.sd);
					else
						dsctbl[i] = pow(dsctbl[i], 1.0 / (2.0 - ds.sd));
				}
				// Auto-range delay
				for (int i = 0; i < dyss; ++i) {
					//!!!!!!!!!!	 original - should work fine, but need renaming / understanding
									/*
									svt[i] = svt[i] - svt[i] * ds.ard + (dsctbl[i] - spt[i]) * ds.arf;
									spt[i] += svt[i];
									*/
					spt[i] += (dsctbl[i] - spt[i]) * ds.ard;
					dsctbl[i] = spt[i];
					dsctbl[i] = max(0.0, dsctbl[i]);
					dsctbl[i] = min(1.0, dsctbl[i]);
				}
				// Build color-table
				for (int i = 0; i < dyss; ++i)
					cltbl[i] = getColor(dsctbl[i], ds.cm, ds.crct, ds.gm);
				// Draw space to pixel buffer
				for (UINT32* pbc = pbv; pbc < pbi; ++pbc)
					*pbc = cltbl[*pbc >> stst];
			}

			// Display test mode
			if (ds.tm == 1)
				for (UINT32* pbc = pbv; pbc < pbi; ++pbc)
					*pbc = getColor(1.0 / (pbi - pbv) * (pbc - pbv), ds.cm, ds.crct, ds.gm);
			// Move pixel-buffer cursor to beginning
			pbc = pbv;
		}
	}

	/* Test calculation */
	if (ds.tm == 2 && tcsv != NULL) {
		// calculate test-cell-space to compare
// temporary code for debugging
		if (cnmd == CM_HASH) {
			CA_CNFN_VBA_1x256(otm, &tvba);
			CA_SCFN_VBA(&tvba);
		}
		// end temporary code
		else {
			for (int g = 0; g < otm; ++g) {
				// copy wrap-arround-buffer / border
				for (int c = 0; c < cr->ncn - 1; ++c) {
					tcsv[scsz + c] = tcsv[c];
				}
				// calc next gen
				ca_next_gen__sisd(cr, tcsv, tcsv + scsz);
			}
		}
		// compare
		int mcr = memcmp(csv, tcsv, scsz * sizeof * csv);
		if (mcr != 0) {
			printf("TESTMODE ERROR  mcr %d  tm %lld  sz %d", mcr, otm, scsz * sizeof * csv);
			int esf = 0, pd = 1, ec = 0, lp = 0;		// esf=errors-found, pd=print-enabled, ec=error-count, lp=last-position
			for (int c = 0; c < scsz; ++c) {
				if (tcsv[c] != csv[c]) {
					ec++;
					if (pd)
						printf(" %d", c);
					esf++;
					lp = c;
					if (esf > 9 && pd) {
						printf("...");
						pd = 0;
					}
				}
			}
			printf("\n  lp %d  er %.2%%f\n", lp, 100.0 / scsz * ec);
			///			printf("  Press any key\n"); getch();
		}
	}

	/* Histogram */
	if (hw > 0) {
		int plh = (pni - pnv) / ds.plw;
		int vs = 20;							// vertical spacing to screen border of histogram
		int hh = plh - 2 * vs;					// histogram height
		int dph = 3;// hh / 100;				// histogram height of one datapoint
		/* history */
		static uint16_t* hy = NULL;				// history
		static int hyc = 0;						// history count
		static int hyi = -1;					// history iterator
		if (hyc != hw * dyss) {
			hyc = hw * dyss;
			free(hy);
			hy = malloc(hyc * sizeof(*hy));
			memset(hy, 0, hyc * sizeof(*hy));
		}
		++hyi;
		if (hyi >= hw)
			hyi = 0;
		/* background */
		for (int v = 0; v < plh; ++v) {
			UINT32 cl = 0x333333;
			if (v >= vs && v < plh - vs)
				cl = getColor(1.0 / hh * (v - vs), ds.cm, ds.crct, ds.gm);
			for (int h = 0; h < hw; ++h) {
				pnv[v * ds.plw + h] = cl;
			}
		}
		/* states */
		double rng = max(DBL_EPSILON, dmx - dmn);
		for (int md = 0; md < 2; ++md) {					// go through modes (see below where used)
			for (int i = 0; i < dyss; ++i) {
				//printf("Info  # %d  st %.2f  mn %.2f mx %.2f  sv %d\n", i, dsctbl[i], dmn, dmx, sv);
				int sbc = dsctbl[i] * hh;		// state bar center of y-histogram-cord

				// dont draw non existent states
//				if (dsctbl[i] < 0.0001 && sctbl[i] == 0)
				if (sctbl[i] == 0)
					continue;

				int sbt, sbb;								// ... top and bottom
				if (sbc - dph / 2 < 0)
					sbt = 0, sbb = dph;
				else if (sbc + dph / 2 >= hh)
					sbb = hh - 1, sbt = hh - 1 - dph;
				else
					sbt = sbc - dph / 2, sbb = sbc + dph / 2;
				sbt += vs, sbb += vs;
				//
				for (int v = sbt; v <= sbb; ++v) {
					int hx = 0.5 + hw * (i) / (dyss - 1);
					hx = max(5, hx);
					hx = min(hw - 5, hx);
					if (md == 0)
						hx = hw;
					for (int h = 0; h < hx; ++h) {
						if (pnv[v * ds.plw + h] == 0x000000)
							pnv[v * ds.plw + h] = 0xFFFFFF;
						else
							pnv[v * ds.plw + h] = 0x0;
					}
				}
			}
		}

		/* history */
		for (int i = 0; i < dyss; ++i) {
			// store
			hy[i * hw + hyi] = 0xFFFF * dsctbl[i];
			// draw
			int hyt = hyi;
			int x = 0;
			int lv = -1;
			while (1) {
				int v = hy[i * hw + hyt] * hh / 0xFFFF + vs;
				v = max(0, v);
				v = min(plh, v);
				if (lv < 0)
					lv = v;
				//				for (int vv = min(v, lv); vv <= max(v, lv); ++vv) {
				{
					int vv = v;
					//pnv[vv * plw + x] = 0xFFFFFF - pnv[vv * plw + x];
					if (pnv[vv * ds.plw + x] < 0x800000)
						pnv[vv * ds.plw + x] = 0xFFFFFF;
					else
						pnv[vv * ds.plw + x] = 0x0;

				}
				lv = v;
				++x;
				++hyt;
				if (hyt == hw)
					hyt = 0;
				if (hyt == hyi)
					break;
			}
		}
	}

	/* Remember current screen-position when in scanline-mode */
	if (ds.sm)
		spnc = pnc;
	/* Copy space back with correct alignment */
	if (ds.fsme < 2 || ds.tm == 2)
		for (int i = 0; i < scsz; ++i)
			sc[i] = csv[(csf - csv + i) % scsz];
}


/* variant with other way to create patterned background (by using large numbers instead of arrays - seems
to work and is probably much more efficient than using arrays, but becomes more difficult to handle when
larger patterns are needed
ALSO is probably a good way to make it possible to enumerate the background patterns and store/load this setting
*/
//void
//CA_RandomizeSpace(
//	CA_CT *spc,
//	int spcsz,
//	int sc,		/* count of possible cell states */
//	int rc		/* count of cells which will be randomized (density of random cells); everything else is background (which state is randomly choosen once */
//	) {
//	int randmax30bit = (RAND_MAX<<15) | RAND_MAX;	//
//	int rand30bit = (rand()<<15) | rand();
//	int ps = log(randmax30bit + 1) / log(sc); // , ipow(sc, rand() % 6; // pattern size
//	ps = rand() % ps + 1;
//	unsigned int bgi = rand30bit % ipow(sc, ps), bgp;
//
//
//	printf("randomize space  background %d  randomized cells %d\n", bgi, rc);
//	printf("\tpattern  #%u  size %d\n\t  pattern ", bgi, ps);
//	bgp = bgi;
//	for (int i = 0; i < ps; i++) {
//		printf("%u", bgp % sc);
//		bgp /= sc;
//	}
//	printf("\n\t  example ");
//	bgp = bgi;
//	for (int i = 0; i < 40; i++) {
//		printf("%u", bgp % sc);
//		bgp /= sc;
//		if(!bgp)
//			bgp = bgi;
//	}
//	printf("...\n");
//
//
//
//	register CA_CT *psn = spc;	// position
//	register CA_CT *bdr = spc + spcsz;	// border
//
//	bgp = bgi;
//	while (psn < bdr) {
//		if (rand() % spcsz < rc)
//			*psn = rand() % sc;
//		else
//			*psn = bgp % sc;
//		bgp /= sc;
//		if (!bgp)
//			bgp = bgi;
//		++psn;
//	}
//}

/*

rs		nr. of randomized cells
zero_position		flag - if set position of random cells is always 0, otherwise random

*/
void
CA_RandomizeHashSpace(int rs, int zero_position)
{
	//assert(HCTBSPT == 2 && "HCTBSPT must be 2"); // TODO why does this not compile???
	SIMFW_SetFlushMsg(&sfw, "hash-cells random size  %d  (use ctl to select leftmost node)\n", rs);
	memset(&hcls, 0, sizeof(hcls));				// reset hash-cells-set (even if this probably may not be necesary)
	// create a random hash-node of given size
	HCI rmnd;									// current random-node (and last-node after the for-loop is done)
	int ml = 0;									// max-level
	for (int i = 0; i < rs; i += HCTBS) {		// generate rs-number of random cells
		uint32_t rbs = pcg32_random();			// generate 32 bits of randomnes
		int l = 0;								// level
		rmnd = HC_add_node(0, 1 + ((rbs >> 0) & 0x3), &l);
		rmnd = HC_add_node(0, 1 + ((rbs >> 2) & 0x3), &l);
		if (l > ml)
			ml = l;
	}
	if (ml)										// ml should not be zero, since we never want to set the usage-count of a base (level 0) node to zero, as otherwise it would be recycled/reused at some point
		hct[rmnd].uc &= HCLLMK;					// set usage-count to zero but keep value of level (see notes on HC_add_node on why this is done)
	// select a random node the same level as ml
	HCI hcbt[HCTMXLV] = { 0 };					// hash-node-backtrack array
	int hcbtln[HCTMXLV] = { 0 };				// hash-node-backtrack-left-node-taken array
	int bti = 0;								// backtrack-index
	HCI cn = hc_sn;								// current node
	int cl = hc_sl;								// current level
	while (cl > ml) {
		// remember path taken
		hcbt[bti] = cn;
		// randomly select left or right node
		cl--;
		if (zero_position || (pcg32_random() & 1)) {
			cn = hct[cn].ln;
			hcbtln[bti] = 1;
		}
		else
			cn = hct[cn].rn;
		bti++;
	}
	// exchange selected node with the randomly created note and update all parent nodes
	cn = rmnd;
	for (int i = bti - 1; i >= 0; i--) {
		cl++;
		HCI tcn = cn;
		hct[cn].uc++;
		if (hcbtln[i])
			cn = HC_find_or_add_branch(cl, cn, hct[hcbt[i]].rn, NULL);
		else
			cn = HC_find_or_add_branch(cl, hct[hcbt[i]].ln, cn, NULL);
		hct[tcn].uc--;
		//
	}
	// hct[hc_sn].uc--;
	hc_sn = cn;
	// hct[hc_sn].uc++;
}

void
CA_RandomizeLinearSpace(
	CA_CT* spc,
	int spcsz,
	int sc,		/* count of possible cell states */
	int rc,		/* count of cells which will be randomized (density of random cells); everything else is background (which state is randomly choosen once */
	int ps		// background-pattern size
) {
	if (!ps) {
		ps = pow(2.0, log2(min(128, spcsz)) * random01());	// float log
	}
	CA_CT* pa;							// pattern array
	pa = malloc(ps * sizeof(*pa));		// allocate memory for pattern array
	if (pa == NULL) {
		SIMFW_Die("malloc failed at %s (:%d)\n", "__FUNCSIG__", __LINE__);		// TODO - check if funcsig works with gcc
		return;
	}
	for (int i = 0; i < ps; ++i)		// create random pattern
		pa[i] = pcg32_boundedrand(sc);

	/* Print info about random cells and pattern */
	printf("randomize space\n");
	printf("\tspace size  %d\n", spcsz);
	printf("\trandomized cells  %d\n", rc);
	printf("\tpattern size  %d\n", ps);
	printf("\tspace size / pattern size  %d %% %d\n", spcsz / ps, spcsz % ps);
	printf("\tpattern  ");
	for (int i = 0; i < ps; ++i)
		printf("%c", pa[i]);
	printf("\n");
	SIMFW_SetFlushMsg(&sfw, "randomize linear space\nrandomized cells %d\npattern size %d", rc, ps);

	/* Crete randomized space */
	register CA_CT* psn = spc;					// position
	register CA_CT* bdr = spc + spcsz;			// border
	register int pc = 0;						// pattern-cursor
	while (psn < bdr) {
		if (pcg32_boundedrand(spcsz) < rc)
			*psn = pcg32_boundedrand(sc);
		else
			*psn = pa[pc];
		++pc;
		if (pc >= ps)
			pc = 0;
		++psn;
	}
}


void
CA_MAIN(void) {
	init_lmas();								// init lindenmayer-arrays
	srand((unsigned)time(NULL));
	static pcg32_random_t pcgr;

	/* Init */
	SIMFW_Init(&sfw, SIMFW_TEST_TITLE, SIMFW_TEST_DSP_HEIGHT, SIMFW_TEST_DSP_WIDTH,
		SIMFW_TEST_DSP_WINDOW_FLAGS | SDL_WINDOW_RESIZABLE
		, 0, 0);

	double zoom = 1.0;

	printf("window  height %d  width %d\n", sfw.window_height, sfw.window_width);
	printf("sim  height %d  width %d\n", sfw.sim_height, sfw.sim_width);
	pixel_effect_moving_gradient(&sfw);

	int rel_start_speed = 512;
	int res = 1;				// ca-space-reset
	int dyrt = 1;				// display-reset
	int cnmd = CM_SISD;			// computation-mode
	int de = 1;					// drawing enabled
	int klrg = 32;				// kernel range if computation-mode is GPU

	ds.fsme = 0;
	ds.ar = 3;
	ds.ard = 0.1;
	ds.arf = 0.1;
	ds.cm = 2;
	ds.crct = 0;
	ds.gm = 1;
	ds.lgm = 1;
	ds.cmm = 0;
	ds.tm = 0;
	ds.sd = 1.0;
	ds.te = 0.0;
	ds.sm = 0;					// scanline-mode
	ds.stzm = 1;				// state zoom - 0 = direct zoom on intensity
	ds.sfcm = 1;				// screen-filling-curve-mode
	ds.sfcsw = 1;				// screen-filling-curve-step-width
	ds.hlfs = 1;				/* focus / sharpening mode */
	ds.vlfs = 1;				/* focus / sharpening mode */
	ds.vlzm = 1;				/* vertical zoom */
	ds.hlzm = 1;				/* horizontal zoom */
	ds.vlpz = 1;				/* vertical pixel zoom */
	ds.hlpz = 1;				/* horizontal pixel zoom */
	ds.sfcmp = 0;
	ds.hddh = 0;
	ds.hdll = 0;
	ds.mlsc = 0;

	uint64_t tm = 0;			/* time */

	int64_t speed = rel_start_speed, x_shift = 0;

	CA_RULE cr = CA_RULE_EMPTY;		/* ca configuration */
	// rule 153 (3 states) und  147 (2 states absolut) ähnlich 228!
	// neuentdeckung: rule 10699 mit 3 nb
	cr.rl = 54;//  3097483878567;// 1598;// 228; // 228;// 3283936144;// 228;// 228;// 90;// 30;  // 1790 Dup zu 228! // 1547 ähnliche klasse wie 228
	cr.tp = ABS;
	cr.ncs = 2;
	cr.ncn = 3;

	int window_x_position = -1;
	int window_y_position = -1;
	int window_width = SIMFW_TEST_DSP_WIDTH;
	int window_height = SIMFW_TEST_DSP_HEIGHT;
	/* Init key-bindings */
	{
		SIMFW_KeyBinding eikb;		// empty integer key binding
		eikb.name = "";
		eikb.description = "";
		eikb.value_strings = NULL;
		eikb.min = 0.0;
		eikb.max = 0.0;
		eikb.def = 0.0;
		eikb.step = 1.0;
		eikb.slct_key = 0;
		eikb.val_type = KBVT_INT;
		eikb.val = NULL;
		eikb.cgfg = NULL;
		eikb.wpad = 1;
		SIMFW_KeyBinding edkb;		// empty double key binding
		edkb = eikb;
		edkb.val_type = KBVT_DOUBLE;
		edkb.wpad = 0;

		SIMFW_KeyBinding nkb;		// new key binding

		//
		nkb = eikb;
		nkb.name = "computation-mode"; nkb.description = "";
		// create value-strings-array
		for (int c = 0; c < CM_MAX; c++) {
			cm_names[c] = ca_cnsgs[c].name;
		}
		nkb.value_strings = cm_names;
		//
		nkb.val = &cnmd;
		nkb.cgfg = &res;
		nkb.slct_key = SDLK_F5;
		nkb.min = 0; nkb.max = CM_MAX - 1;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		//nkb = eikb;
		//nkb.name = "kernel-range-(computation-mode)";
		//nkb.val = &klrg;
		////nkb.slct_key = SDLK_F6;
		//nkb.min = 0; nkb.max = 1024 * 8;
		//SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "hash-cell-count-(computation-mode)";
		nkb.description = "hash-cell-index-size as power of two";
		nkb.val = &HCISZPT;
		nkb.cgfg = &res;
		nkb.slct_key = SDLK_F6;
		nkb.min = 10; nkb.max = 30;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "hash-cell-maximum-seek-count-(computation-mode)";
		//nkb.description = "";
		nkb.val = &HCMXSC;
		nkb.slct_key = SDLK_F3;
		nkb.min = 0; nkb.max = 1024;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.val_type = KBVT_INT64;
		nkb.name = "space-size";
		nkb.val = &ca_space_sz;
		nkb.slct_key = SDLK_h;
		nkb.min = 256; nkb.max = 1024 * 1024 * 64;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "speed";
		nkb.val = &speed;
		nkb.min = 0; nkb.max = 1 << 30;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "drawing-enabled";
		nkb.val = &de;
		nkb.slct_key = SDLK_F7;
		nkb.min = 0; nkb.max = 1;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "display-test-mode"; nkb.description = "0: disabled, 1: graphics, 2: computation";
		nkb.val = &ds.tm;
		nkb.cgfg = &dyrt;
		nkb.slct_key = SDLK_F8;
		nkb.min = 0; nkb.max = 2;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "display-log-mode";
		nkb.val = &ds.lgm;
		nkb.slct_key = SDLK_b;
		nkb.min = 0; nkb.max = 1;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "display-cumulative-mode";
		nkb.val = &ds.cmm;
		nkb.slct_key = SDLK_n;
		nkb.min = 0; nkb.max = 3;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "frequency-analysis";
		nkb.val = &ds.stzm;
		nkb.slct_key = SDLK_j;
		nkb.min = 0; nkb.max = 1;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "auto-range"; nkb.description = "0: none, 1: auto min, 2: auto max, 3: auto min and max";
		nkb.val = &ds.ar;
		nkb.slct_key = SDLK_y;
		nkb.min = 0; nkb.max = 3;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = edkb;
		nkb.name = "auto-range-delay";
		nkb.val = &ds.ard;
		nkb.slct_key = SDLK_q;
		nkb.min = 0.0; nkb.max = 1.0; nkb.step = 1.0 / 100.0;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = edkb;
		nkb.name = "auto-range-force";
		nkb.val = &ds.arf;
		nkb.slct_key = SDLK_w;
		nkb.min = 0.0; nkb.max = 1.0; nkb.step = 1.0 / 100.0;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "display-scanline-mode";
		nkb.val = &ds.sm;
		nkb.slct_key = SDLK_m;
		nkb.min = 0; nkb.max = 1;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "screen-filling-curve-mode";
		nkb.val = &ds.sfcm; nkb.cgfg = &dyrt;
		nkb.slct_key = SDLK_f;
		nkb.min = 0; nkb.max = 32;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "screen-filling-curve-mode-parameter";
		nkb.val = &ds.sfcmp;
		nkb.cgfg = &dyrt;
		nkb.slct_key = SDLK_g;
		nkb.min = 0; nkb.max = 4096;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "screen-filling-curve-step-width";
		nkb.val = &ds.sfcsw;
		nkb.slct_key = SDLK_F9;
		nkb.min = 1; nkb.max = 4;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "color-mode"; nkb.description = "0: b&w, 1: gray, 2: color+b&w, 3: color, 4: oklab light, 5: oklab dark, 6-33: oklab gradients";
		nkb.val = &ds.cm;
		nkb.slct_key = SDLK_x;
		nkb.cgfg = &dyrt;
		nkb.min = 0; nkb.max = 33;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "color-count"; nkb.description = "0: unlimited; max: 0xFFFFFF";
		nkb.val = &ds.crct;
		nkb.slct_key = SDLK_c;
		nkb.cgfg = &dyrt;
		nkb.min = 0; nkb.max = 0xFFFFFF;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "gradient-mode"; nkb.description = "0: unchanged, 1: reversed, 2: mirrored, 3: reversed and mirrored";
		nkb.val = &ds.gm;
		nkb.slct_key = SDLK_v;
		nkb.cgfg = &dyrt;
		nkb.min = 0; nkb.max = 3;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = edkb;
		nkb.name = "spread"; nkb.description = "0.0 - 2.0";
		nkb.val = &ds.sd;
		nkb.slct_key = SDLK_a;
		nkb.min = 0.0; nkb.max = 2.0; nkb.step = 1.0 / 200.0; nkb.def = 1.0;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = edkb;
		nkb.name = "translate"; nkb.description = "-1.0 - 1.0";
		nkb.val = &ds.te;
		nkb.slct_key = SDLK_s;
		nkb.min = -1.0; nkb.max = 1.0; nkb.step = 1.0 / 100.0;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "focus-mode"; nkb.description = "0: absolute, 1: totalistic, 2: frequency (hash-cm only)";
		nkb.val = &ds.fsme;
		nkb.slct_key = SDLK_i; nkb.cgfg = &dyrt;
		nkb.min = 0; nkb.max = 2;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "hash-display-depth";
		nkb.val = &ds.hddh;
		nkb.slct_key = SDLK_z;
		nkb.min = -128; nkb.max = 128; nkb.wpad = 0;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "hash-display-level";
		nkb.val = &ds.hdll; nkb.cgfg = &dyrt;
		nkb.slct_key = SDLK_u;
		nkb.min = 0; nkb.max = 128; nkb.wpad = 0;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "manual-shift-correction";
		nkb.val = &ds.mlsc;
		nkb.slct_key = SDLK_F10;
		nkb.min = -(1 << 30); nkb.max = 1 << 30; nkb.wpad = 0;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "horizontal-focus";
		nkb.val = &ds.hlfs;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "vertical-focus";
		nkb.val = &ds.vlfs;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "horizontal-zoom";
		nkb.val = &ds.hlzm;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "vertical-zoom";
		nkb.val = &ds.vlzm;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "horizontal-pixel-zoom";
		nkb.val = &ds.hlpz;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "vertical-pixel-zoom";
		nkb.val = &ds.vlpz;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "rule-number";
		//nkb.slct_key = SDLK_r; nkb.cgfg = &res;
		nkb.val = &cr.rl;				// ATTENTION cr.rl is 64bit int
		//nkb.min = 0;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "rule-type"; nkb.description = "0:absolute, 1: totalistic";
		nkb.val = &cr.tp;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "rule-number-of-cell-states";
		nkb.val = &cr.ncs;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "rule-number-of-cells-in-a-neighborhod";
		nkb.val = &cr.ncn;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "window-x-position";
		nkb.val = &window_x_position;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "window-width";
		nkb.val = &window_width;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "window-height";
		nkb.val = &window_height;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "window-y-position";
		nkb.val = &window_y_position;
		SIMFW_AddKeyBindings(&sfw, nkb);

		/* Load configuration */
		SIMFW_LoadKeyBindings(&sfw, "settings.txt");
	}

	/* Restore window position */
	if (window_x_position == -1 && window_y_position == -1)
		SDL_WINDOWPOS_CENTERED_DISPLAY(1);
	else
		SDL_SetWindowPosition(sfw.window, window_x_position, window_y_position);
	SDL_SetWindowSize(sfw.window, window_width, window_height);
	sfw.window_height = window_height;
	sfw.window_width = window_width;
	SIMFW_SetSimSize(&sfw, sfw.window_height * zoom, sfw.window_width * zoom);

	/* Control-Mode */
	enum CLMD { CLMD_NONE, CLMD_RULE, CLMD_SCREEN } clmd = CLMD_NONE;

	ca_space_sz = max(1 << 10, ca_space_sz);
	ca_space_sz = min(1 << 20, ca_space_sz);

	int pitch = 0;
	cr = CA_Rule(cr);

	// try to load cell-space from file
	CA_CT* nwsc = NULL;										// new space
	int nwsz = 0;											// new size
	nwsc = CA_LoadFromFile("cellspace.bin", &nwsz);
	// file found
	if (nwsc && nwsz) {
		ca_space_sz = nwsz;									// remember new size
		ca_space = nwsc;									// set new space as curren
	}
	// file not found - create cellspace manually
	else {
		ca_space = malloc(ca_space_sz * sizeof *ca_space);
		memset(ca_space, 0, ca_space_sz * sizeof *ca_space);

		///	ca_space[ca_space_sz / 2] = 1;
		ca_space[0] = 1;
		ca_space[1] = 1;
		ca_space[3] = 1;
		ca_space[8] = 1;
		ca_space[16] = 1;
		for (int i = 0; i < 2; ++i) {
			int pos = pcg32_boundedrand(ca_space_sz);
			int lh = ipow(2, pcg32_boundedrand(3) + 6);
			printf("rdm  %d - %d\n", pos % ca_space_sz, lh);
			for (int i = 0; i < lh; ++i)
				*(ca_space + (pos + i) % ca_space_sz) = pcg32_boundedrand(cr.ncs);
		}

	}

	//
	int mouse_speed_control = 0;

	/* SDL loop */
	while (1) {
		/* SDL event handling */
		SDL_Event e;
		int fscd = 0;									// focus (hlfs, vlfs, vlzm, hlzm) has been changed
		int64_t org_speed = speed;
		uint64_t last_ca_space_sz = ca_space_sz;
		while (SDL_PollEvent(&e)) {
			const UINT8* kbst = SDL_GetKeyboardState(NULL); /* keyboard state */
			SDL_Keymod kbms = SDL_GetModState(); /* keyboard mod state */
			int lst = kbst[SDL_SCANCODE_LSHIFT];			// left shift
			int ctl = kbms & KMOD_CTRL;						// ctrl key pressed
			int alt = kbms & KMOD_ALT;						// alt key pressed
			int sft = kbms & KMOD_SHIFT;					// shift key pressed
			if (e.type == SDL_QUIT)
				break;
			if (e.type == SDL_WINDOWEVENT) {
				switch (e.window.event) {
				case SDL_WINDOWEVENT_RESIZED:
					sfw.window_height = e.window.data2;
					sfw.window_width = e.window.data1;
					SIMFW_SetSimSize(&sfw, sfw.window_height * zoom, sfw.window_width * zoom);
					dyrt = 1;

					break;
				}
			}
			else if (mouse_speed_control && e.type == SDL_MOUSEMOTION) {
				int ty = speed / ca_space_sz;
				int tx = speed % ca_space_sz;
				if (ctl)
					tx = 0;
				if (alt)
					tx = ty;
				if ((kbms & KMOD_CAPS) == KMOD_CAPS) {
					int y_save = 0;
					if (speed > ca_space_sz * sfw.sim_height)
						y_save = 1 + speed / ca_space_sz / sfw.sim_height * ca_space_sz * sfw.sim_height;
					speed =
						y_save +
						(sfw.sim_height - 1 - e.motion.y * sfw.sim_height / sfw.window_height) * ca_space_sz
						+ ((ca_space_sz - e.motion.x * sfw.sim_width / sfw.window_width)) % ca_space_sz;
					if (ctl)
						speed += (sfw.sim_height - 1 - e.motion.y * sfw.sim_height / sfw.window_height);
				}
				else {
					e.motion.y -= 50;
					e.motion.x -= 50;
					if (sft)
						e.motion.y /= 4,
						e.motion.x /= 4;
					ty -= e.motion.y;
					tx -= e.motion.x;
					speed = max(0, ty * ca_space_sz + tx);
					SDL_WarpMouseInWindow(sfw.window, 50, 50);
				}
			}
			else if (e.type == SDL_MOUSEBUTTONDOWN) {
				SIMFW_SetFlushMsg(&sfw, "mouse button %d", e.button.button);

				if (e.button.button == 3) {
					mouse_speed_control = !mouse_speed_control;
				}
			}
			else if (e.type == SDL_KEYDOWN) {
				int keysym = e.key.keysym.sym;
				int szcd = 0;									// size changed
				/* Screen-Control-Mode */
				if (clmd == CLMD_SCREEN) {
					int cd = 1;		// keysim consumed
					switch (keysym) {
					case SDLK_END:
						zoom = 1.0;
						break;
					case SDLK_HOME:
						zoom = 1.0 / 4.0;
						break;
					case SDLK_PAGEUP:
						zoom *= 2.0;
						break;
					case SDLK_PAGEDOWN:
						zoom /= 2.0;
						break;
					case SDLK_F12: {
						// FLIP VSYNC
						SDL_RendererInfo sri;
						SDL_GetRendererInfo(sfw.renderer, &sri);
						SDL_DestroyRenderer(sfw.renderer);
						if (sri.flags & SDL_RENDERER_PRESENTVSYNC)
							sfw.renderer = SDL_CreateRenderer(sfw.window, -1/*use default rendering driver*/, SDL_RENDERER_ACCELERATED);
						else
							sfw.renderer = SDL_CreateRenderer(sfw.window, -1/*use default rendering driver*/, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
						SIMFW_SetSimSize(&sfw, 0, 0);
						break; }
					default:
						cd = 0;
					}
					if (cd) {
						keysym = 0;
						SIMFW_SetSimSize(&sfw, sfw.window_height * zoom, sfw.window_width * zoom);
						dyrt = 1;
						continue;
					}
				}
				/* Rule-Control-Mode */
				if (clmd == CLMD_RULE) {
					int cd = 0;		// keysim consumed
					// allows to change 'digit' in rule number
					if (keysym >= SDLK_0 && keysym <= SDLK_9) {
						cd = 1;
						uint64_t dp; //
						// swap keys so that '1'-key is 0 and '0'-key is 9
						if (keysym == SDLK_0)
							dp = 9;
						else
							dp = keysym - SDLK_1;
						//
						if (lst)
							dp += 10;
						//
						dp = ipow(cr.ncs, dp);
						//
						if (ctl)
							cr.rl -= dp;
						else
							cr.rl += dp;
					}
					else switch (keysym) {
					case SDLK_r:
						cd = 1;
						break;
					case SDLK_DELETE:
						cd = 1;
						if (cr.tp == ABS)
							cr.tp = TOT;
						else
							cr.tp = ABS;
						break;
					case SDLK_INSERT:
						cd = 1;
						cr.rl = pcg32_boundedrand(cr.nrls);
						break;
					case SDLK_HOME:
						if (ctl)
							cd = 1,
							cr.rl = 0;
						break;
					case SDLK_END:
						if (ctl)
							cd = 1,
							cr.rl = cr.nrls;
						break;
					case SDLK_LEFT:
						cd = 1;
						cr.rl--;
						break;
					case SDLK_RIGHT:
						cd = 1;
						cr.rl++;
						break;
					case SDLK_UP:
						cd = 1;
						if (ctl)
							cr.ncn++;
						else
							cr.ncs++;
						break;
					case SDLK_DOWN:
						cd = 1;
						if (ctl)
							cr.ncn--;
						else
							cr.ncs--;
						break;
					}
					if (cd) {
						keysym = 0;
						cr = CA_Rule(cr);
						CA_RandomizeLinearSpace(ca_space, ca_space_sz, cr.ncs, ca_space_sz / ipow(2, pcg32_boundedrand(10)), 3);
						char buf[10000];
						CA_RULE_PrintInfo(&cr, &buf, sizeof(buf));
						SIMFW_SetFlushMsg(&sfw, &buf);
						res = 1;
					}
				}
				/* No control-mode selected */
				// insert random cells
				if (keysym >= SDLK_0 && keysym <= SDLK_9) {
					if (cnmd == CM_HASH) {
						int rs = HCTBS * ipow(2, min(hc_sl, keysym - SDLK_0));				// random size in nr. of cells
						CA_RandomizeHashSpace(rs, ctl);
					}
					else {
						int rcc = ipow(2, keysym - SDLK_0);									// random-cells-count - 0 = 1 random cell, 1 = 2, 2 = 4, 3 = 8, ..., 9 = 512
						int ps = 1;															// pattern-size (i.e. size of repeating background pattern)
						if (alt)
							ps = 0;
						if (sft)
							// 1 = random cells 10% of width, 2 = 20%, 9 = 90%, 0 = 100%
							rcc = ca_space_sz * (keysym - SDLK_0 + (keysym == SDLK_0) * 10) / 10;
						CA_RandomizeLinearSpace(ca_space, ca_space_sz, cr.ncs, rcc, ps);
						res = 1;
					}
				}
				//
				else switch (keysym) {
				case 0:
					break;
					/* Control Modes */
				case SDLK_r:
					if (sft) {
						clmd = CLMD_RULE;
						SIMFW_SetFlushMsg(&sfw, "control-mode  rule");
					}
					else {
						if (ctl)
							cr.rl--;
						else
							cr.rl++;
						cr = CA_Rule(cr);
						char buf[10000] = "";
						CA_RULE_PrintInfo(&cr, &buf, sizeof(buf));
						SIMFW_SetFlushMsg(&sfw, &buf);
						res = 1;
					}
					break;
					//case SDLK_F3:
					//	clmd = CLMD_SCREEN;
					//	SIMFW_SetFlushMsg(&sfw, "control-mode  screen");
					//	break;
				case SDLK_F10:
					if (ctl) {
						SIMFW_LoadKeyBindings(&sfw, "settings.txt");
						CA_CT* nwsc = NULL;									// new space
						int nwsz = 0;										// new size
						nwsc = CA_LoadFromFile("cellspace.bin", &nwsz);
						ca_space_sz = last_ca_space_sz = nwsz;				// remember new size
						free(ca_space);										// free old space
						ca_space = nwsc;									// set new space as current
						res = 1;
						SIMFW_SetFlushMsg(&sfw, "INFO: Settings and cell-space (%.2e bytes) loaded from file.", (double)sizeof(CA_CT) * ca_space_sz);
					}
					else {
						SIMFW_SaveKeyBindings(&sfw, "settings.txt");
						CA_SaveToFile("cellspace.bin", ca_space, ca_space_sz);
						SIMFW_SetFlushMsg(&sfw, "INFO: Settings and cell-space (%.2e bytes) saved to file.", (double)sizeof(CA_CT) * ca_space_sz);
					}
					break;
				case SDLK_F11:;
					static int fullscreen = 0;
					if (!fullscreen)
						SDL_SetWindowFullscreen(sfw.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
					else
						SDL_SetWindowFullscreen(sfw.window, 0);
					fullscreen = !fullscreen;
					SIMFW_SetSimSize(&sfw, 0, 0);
					break;
				case SDLK_ESCAPE:
					clmd = CLMD_NONE;
					SIMFW_ConsoleCLS();
					SIMFW_SetFlushMsg(&sfw, "");
					break;
				case SDLK_F4:
					goto behind_sdl_loop;
				case SDLK_a:
					res = 1;
					break;
				case SDLK_PERIOD:
					if (cnmd == CM_HASH) {
						SIMFW_SetFlushMsg(&sfw, "Not supported while using hash-computation-mode!");
					}
					else {
						memset(ca_space, 0, ca_space_sz * sizeof *ca_space);
						ca_space[ca_space_sz / 2] = pcg32_boundedrand(cr.ncs - 1) + 1;
						res = 1;
					}
					break;
				case SDLK_COMMA:
					if (cnmd == CM_HASH) {
						SIMFW_SetFlushMsg(&sfw, "Not supported while using hash-computation-mode!");
					}
					else {
						memset(ca_space, 0, ca_space_sz * sizeof *ca_space);
						int lt = cr.ncn * 3 * (1 + lst * pcg32_boundedrand(100));
						for (int i = 0; i < lt; i++)
							ca_space[(ca_space_sz / 2 - lt / 2 + i) % ca_space_sz] = pcg32_boundedrand(cr.ncs);
						res = 1;
					}
					break;
				case SDLK_t:
					if (cnmd == CM_HASH) {
						SIMFW_SetFlushMsg(&sfw, "Not supported while using hash-computation-mode!");
					}
					else {
						cr.rl = (((UINT64)pcg32_random_r(&pcgr)) << 32 | pcg32_random_r(&pcgr)) % cr.nrls;
						cr = CA_Rule(cr);
						CA_RandomizeLinearSpace(ca_space, ca_space_sz, cr.ncs, ipow(2, pcg32_boundedrand(10)), 3);
						char buf[10000] = "";
						CA_RULE_PrintInfo(&cr, &buf, sizeof(buf));
						SIMFW_SetFlushMsg(&sfw, &buf);
						tm = 0;
						res = 1;
					}
					break;
				case SDLK_BACKSPACE:
					if (cnmd == CM_HASH) {
						// remove unused nodes
						if (ctl) {
							hct[hc_sn].uc++;
							int tdc = 0;											// total-deleted-count
							int pc = 0;												// passes-count
							while (1) {
								int cdc = 0;										// current-deleted-count
								for (HCI i = HCTBS + 1; i < HCISZ; i++) {			// go through all nodes, except the base-nodes
									if (hct[i].ln && !(hct[i].uc & HCUCMK)) {
										cdc++;
										hct[hct[i].ln].uc--;
										hct[hct[i].rn].uc--;
										hct[hct[i].r].uc--;
										hct[i].ln = 0;
										int rcll = (hct[i].uc & HCLLMK) >> 24;		// recycled-node-level
										hc_stats[rcll].nc--;
										hc_stats[rcll].rcct++;
									}
								}
								pc++;
								tdc += cdc;
								if (sft || !cdc)
									break;
							}
							hct[hc_sn].uc--;
							SIMFW_SetFlushMsg(&sfw, "removed unused nodes\nchecked  %d  hash-cells\npasses   %d\ndeleted  %d", HCISZ, pc, tdc);
						}
						// remove all nodes
						else {
							// reset / clear hash-table
							//memset(hct + HCTBS + 2, 0, (HCISZ - HCTBS - 1) * sizeof(HCN));
// TODO reset / rebuild hash table like in cnitfn_hash!!!
							memset(&hcln, 0, sizeof(hcln));
							memset(&hcls, 0, sizeof(hcls));
							memset(&hc_stats[1], 0, (HCTMXLV - 1) * sizeof *hc_stats);
							for (HCI i = HCTBS + 2; i < HCISZ; i++) {
								if (hct[i].ln && !((hct[i].uc & HCLLMK) >> 24))		// do not delete nodes on level 0
									;
								else
									hct[i].ln = 0;
							}
							// build empty nodes
							HCI en = HC_find_or_add_branch(0, 1, 1, NULL);
							for (int i = 1; i <= 126; i++) {
								hct[en].uc++;
								HCI tn = HC_find_or_add_branch(i, en, en, NULL);
								hc_ens[i] = tn;
								hct[tn].uc++;
								hct[en].uc--;
								en = tn;
							}
							//
							hc_sn = hc_ens[hc_sl];
							//							hct[hc_sn].uc++;
							SIMFW_SetFlushMsg(&sfw, "cell-space cleared");
							SIMFW_ConsoleCLS(&sfw);
						}
					}
					else {
						memset(ca_space, 0, ca_space_sz * sizeof(*ca_space));
						res = 1;
					}
					break;
				case SDLK_PLUS:
					if (sft)
						ds.hlpz = ds.vlpz = min(ds.hlpz, ds.vlpz) + 1,
						fscd = 1;
					else if (cnmd == CM_HASH) {
						HCI en;		// new (empty or copied) node
						if (ctl) {
							// build empty node with the same size as current ca
							en = hc_ens[hc_sl];
						}
						else
							// copy the whole current cell-space
							en = hc_sn;
						// connect current ca with new (empty or copied) node
						hc_sl++;
						hct[en].uc++;
						HCI hn = HC_find_or_add_branch(hc_sl, hc_sn, en, NULL);
						hct[en].uc--;
						hc_sn = hn;

						ca_space_sz = ca_space_sz * 2;
						// TODO UGLY hack to avoid allocating memory for possibly very large cell-spaces
						if (ds.fsme == 2 && ca_space_sz >= 1 << 20) {
							last_ca_space_sz = ca_space_sz;
							SIMFW_SetFlushMsg(&sfw, "WARNING  Sync to global cell-space deactivated! You cannot use any other display mode or any functions that access the global cell-space (which are most)!");
						}
						dyrt = 1;
					}
					else if (ctl)
						ds.hlzm = ds.vlzm = min(ds.hlzm, ds.vlzm) + 1,
						fscd = 1;
					else if (alt)
						ca_space_sz = ca_space_sz + 1;
					else
						ca_space_sz = ca_space_sz * 2;
					break;
				case SDLK_MINUS:
					if (sft)
						ds.hlpz = ds.vlpz = min(ds.hlpz, ds.vlpz) - 1,
						fscd = 1;
					else if (cnmd == CM_HASH) {
						hc_sl--;
						hc_sn = hct[hc_sn].ln;

						ca_space_sz = ca_space_sz / 2;
						// TODO UGLY hack to avoid allocating memory for possibly very large cell-spaces
						if (ds.fsme == 2 && ca_space_sz >= 1 << 26) {
							last_ca_space_sz = ca_space_sz;
							SIMFW_SetFlushMsg(&sfw, "WARNING  Sync to global cell-space deactivated! You cannot use any other display mode or any functions that access the global cell-space (which are most)!");
						}
						dyrt = 1;
					}
					else if (ctl)
						ds.hlzm = ds.vlzm = min(ds.hlzm, ds.vlzm) - 1,
						fscd = 1;
					else if (alt)
						ca_space_sz = ca_space_sz - 1;
					else
						ca_space_sz = max(1, ca_space_sz / 2);
					break;
				case SDLK_TAB:;
					int cnt = (int)pow(2.0, pcg32_boundedrand(10));
					if (cnmd == CM_HASH) {
						int rs = HCTBS * ipow(2, min(hc_sl, keysym - SDLK_0));				// random size in nr. of cells
						CA_RandomizeHashSpace(cnt, ctl);
					}
					else {
						res = 1;
						double dy = pow(2.0, random01());	// density
						if (sft) {
							cnt = 64;
							dy = 1.0;
						}
						if (ctl) {
							cnt = 128;
							if (sft)
								cnt = 256;
							dy = 1.0;
						}

						{
							char info_str[1000] = "";
							sprintf_s(info_str, sizeof info_str, "rdm  ct %d/%.2f%%  dy %.2f%%\n", cnt, 100.0 * cnt / ca_space_sz, 100.0 - 100.0 / RAND_MAX * dy);
							SIMFW_SetFlushMsg(&sfw, "%s", info_str);
							printf("%s", info_str);
						}

						for (int i = 0, p = pcg32_boundedrand(ca_space_sz); i < cnt; ++i)
							if (random01() < dy)
								ca_space[(p + i) % ca_space_sz] = pcg32_boundedrand(cr.ncs);
					}
					break;
				case SDLK_END:
					speed = sfw.sim_height;
					break;
				case SDLK_HOME:
					speed = 4;
					break;
				case SDLK_DELETE:
					speed = x_shift = 0;
					break;
				case SDLK_d:
				{
					static int64_t last_manual_speed = 0;
					const int64_t ds = (int64_t)ca_space_sz;
					if (speed != ds) {
						last_manual_speed = speed;
						speed = ds;
					}
					else {
						speed = last_manual_speed;
					}
					break;
				}
				case SDLK_PAGEUP:
					if (cnmd == CM_HASH) {
						sdmrpt++;
					}
					else {
						if (kbms & KMOD_SHIFT)
							speed += 1;
						else
							speed *= 2;
					}
					break;
				case SDLK_PAGEDOWN:
					if (cnmd == CM_HASH) {
						if (sdmrpt)
							sdmrpt--;
					}
					else {
						if (kbms & KMOD_SHIFT)
							speed -= 1;
						else
							speed /= 2;
					}
					break;
				case SDLK_LEFT:
					x_shift--;
					break;
				case SDLK_RIGHT:
					x_shift++;
					break;
				case SDLK_o:
					if (ctl)		ds.hlzm++;
					else if (sft)	ds.hlpz++;
					else			ds.hlfs++;
					fscd = 1;
					break;
				case SDLK_p:
					if (ctl)		ds.hlzm--;
					else if (sft)	ds.hlpz--;
					else			ds.hlfs--;
					fscd = 1;
					break;
				case SDLK_k:
					if (ctl)		ds.vlzm++;
					else if (sft)	ds.vlpz++;
					else			ds.vlfs++;
					fscd = 1;
					break;
				case SDLK_l:
					if (ctl)		ds.vlzm--;
					else if (sft)	ds.vlpz--;
					else			ds.vlfs--;
					fscd = 1;
					break;
				default:
					SIMFW_HandleKeyBinding(&sfw, e.key.keysym.sym);
				}

				//
				ds.vlzm = max(1, ds.vlzm);
				ds.hlzm = max(1, ds.hlzm);
				ds.vlpz = max(1, ds.vlpz);
				ds.hlpz = max(1, ds.hlpz);
				ds.vlfs = max(0, ds.vlfs);
				ds.hlfs = max(0, ds.hlfs);

				if (fscd)
					dyrt = 1;

				/* CA-space-size has been changed */
				if (ca_space_sz != last_ca_space_sz && cnmd != CM_HASH) {
					printf("ca-space-resize  new %d  old %d\n", ca_space_sz, last_ca_space_sz);
					uint64_t nwsz = ca_space_sz;
					CA_CT* nwsc = NULL;									// new space
					nwsc = malloc(nwsz * sizeof * nwsc);				// memory allocation for new space
					if (!nwsc)
// TODO __FUNCSIG__ dows not work with gcc						SIMFW_Die("malloc failed at %s (:%d)\n", __FUNCSIG__, __LINE__);
						SIMFW_Die("malloc failed at %s (:%d)\n", "__FUNCSIG__", __LINE__);
					else {
						memset(nwsc, 0, nwsz * sizeof * nwsc);				// init new space to 0
						tm = 0;
						/* Copy old space into new space - centered */
						memcpy(
							max(nwsc, nwsc + (nwsz - last_ca_space_sz) / 2),
							max(ca_space, ca_space + (last_ca_space_sz - nwsz) / 2),
							min(nwsz, last_ca_space_sz));

						free(ca_space);										// release memory of old space

						speed = max(1, speed * 1.0 * nwsz / last_ca_space_sz);

						ca_space_sz = last_ca_space_sz = nwsz;				// remember new size
						ca_space = nwsc;									// set new space as current
						res = 1;
					}
				}

				if (1 && (fscd || szcd || speed != org_speed))
					SIMFW_SetFlushMsg(&sfw,
						"SPEED   %.4es\nTIME    %.4ec\n        %.4es\nSIZE    %.4ec\n\nS-ZOOM  %d x %d\nP-ZOOM  %d x %d\nFOCUS   %d x %d",
						(double)speed,
						(double)tm, (double)tm / (double)ca_space_sz,
						(double)ca_space_sz,
						ds.vlzm, ds.hlzm,
						ds.vlpz, ds.hlpz,
						ds.vlfs, ds.hlfs);

			}
			if (x_shift > sfw.sim_width)
				x_shift -= sfw.sim_width;
			if (x_shift < 0)
				x_shift += sfw.sim_width;
			if (x_shift != 0 && speed < sfw.sim_height)
				speed = sfw.sim_height;
		}

		ds.plw = sfw.sim_width;
		lifeanddrawnewcleanzoom(
			&cr,
			ca_space, ca_space_sz,
			speed,
			(res--) > 0,
			(dyrt--) > 0,
			sfw.sim_canvas,
			sfw.sim_canvas + sfw.sim_height * sfw.sim_width,
			de, cnmd, klrg, ds);
		// pixel_effect_moving_gradient(&sfw);
		tm += speed;

		/* Update status */
		SIMFW_UpdateStatistics(&sfw);
		SIMFW_SetStatusText(&sfw,
			"ZELLOSKOP   fps %.2f  cps #%.4e  sd %.2f  c# %.2e",
			1.0 / sfw.avg_duration,
			1.0 / sfw.avg_duration * speed * ca_space_sz,
			1.0 * speed,
			1.0 * tm * ca_space_sz);
		SIMFW_UpdateDisplay(&sfw);
		//
		static LONGLONG fpstr = 0;
		static uint64_t fpstm = 0;
		double trdn = timeit_duration_nr(fpstr);		// fps-timer-duration
		if (trdn >= 1.0) {
			static int fccls = 1;
			if (cnmd == CM_HASH) {
				if (fccls)
					SIMFW_ConsoleCLS();
				fccls = 0;
				SIMFW_ConsoleSetCursorPosition(0, 0);
				HC_print_stats();
				for (int i = 0; i < HCTMXLV - 2; i++) {
					hc_stats[i].fc = 0;
					hc_stats[i].rcct = 0;
					hc_stats[i].adct = 0;
					hc_stats[i].sc = 0;
					hc_stats[i].ss = 0;
				}
			}
			else
				fccls = 1;
			double ds;
			if (cnmd == CM_HASH) {
				if (sdmrpt)
					ds = (double)ca_space_sz * 2 * pow(2.0, sdmrpt - 3.0);
				else
					ds = 0.0;
			}
			else
				ds = (double)speed;
			printf("cnmd %s  sz %.2e  sd %.2e  cs %.2e  tm %.2e\n",
				ca_cnsgs[cnmd].name,
				(double)ca_space_sz,
				ds,
				ds * ca_space_sz / sfw.avg_duration, //ds* ca_space_sz / sfw.avg_duration / 8.0 / 1024.0 / 1024.0 / 1024.0,
				(double)tm* ca_space_sz);
			printf("%80s\n", "");
			printf("%80s\n", "");
			fpstr = 0;
		}
		if (!fpstr) {
			fpstr = timeit_start();
			fpstm = tm;
		}
	}

behind_sdl_loop:;
	/* Save configuration */
	// Save window position
	SDL_GetWindowPosition(sfw.window, &window_x_position, &window_y_position);
	SDL_GetWindowSize(sfw.window, &window_width, &window_height);
	SIMFW_SaveKeyBindings(&sfw, "settings.txt");
	CA_SaveToFile("cellspace.bin", ca_space, ca_space_sz);

	/* Cleanup */
	SIMFW_Quit(&sfw);
}

/*

*/
void
NEWDIFFUSION() {

	printf("%f  %f\n", log(0.0), 1.0 / log(0.0));

#define H 540
#define W 540

	/* Init */
	SIMFW simfw = { 0 };
	SIMFW_Init(&simfw, "NEW DIFFUSION", H, W, 0, H, W);

	/* Data ... */
	double* dpv;	// past values
	dpv = malloc(H * W * sizeof(*dpv));
	double* dfv;	// future values
	dfv = malloc(H * W * sizeof(*dfv));

	int MODE = 1;

	// light default for whole space
	for (int i = 0; i < H * W; ++i)
		if (MODE == 1)
			//dpv[i] = dfv[i] = DBL_MIN * 1e3;
			dpv[i] = dfv[i] = 1.0;
		else if (MODE == 2)
			dpv[i] = dfv[i] = 0;
		else if (MODE == 3)
			dpv[i] = dfv[i] = 0;

	double mn = 0.0;
	double mx = 0.0;
	int fc = 0; // frame counter
	/* SDL loop */
	while (1) {
		++fc;
		// light input at middle of space
		//if(fc < 3)
		if (MODE == 1)
			//dpv[H / 2 * W + W / 2] = DBL_MAX / 1e3;
			dpv[H / 2 * W + W / 2] = 1000.0;
		else if (MODE == 2)
			dpv[H / 2 * W + W / 2] = 100;
		else if (MODE == 3)
			dpv[H / 2 * W + W / 2] = 100;

		/* SDL event handling */
		SDL_Event e;
		if (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT)
				break;
		}

		/* Manipulate simulation canvas pixels */
		double* cc = dpv; // current cell in past values
		uint32_t* px = simfw.sim_canvas; // current pixel
		double ltmn = mn;	// last min
		double rg = max(1.0, mx - mn); // range
		mx = -DBL_MAX, mn = DBL_MAX;

		double* cfv = dfv; // current future value
		double sr12;
		sr12 = 1.0 / sqrt(2.0);

		for (int h = 0; h < H; ++h) {
			if (h == 0 || h == H - 1) {
				cc += W;
				px += W;
				cfv += W;
				continue;
			}
			++cc, ++px, ++cfv;
			for (int w = 0; w < W - 2; ++w) {
				if (MODE == 1) {
					// very simple diffusion
					//*cfv = (*cc + *(cc - W) + *(cc - 1) + *(cc + 1) + *(cc + W)) / 5.0;
					*cfv = (
						*cc
						+ *(cc - W) + *(cc - 1) + *(cc + 1) + *(cc + W)
						+ sr12 * (*(cc - W - 1)) + sr12 * (*(cc - W + 1)) + sr12 * (*(cc + W - 1)) + sr12 * (*(cc + W + 1))
						) / (5.0 + 4.0 * sr12);
					// drawing
					double lv = log(*cc); // log value
					//double lv = *cc; // log value
					if (lv > mx)
						mx = lv;
					if (lv < mn)
						mn = lv;
					uint8_t v;
					v = (uint8_t)((lv - ltmn) / rg * 0xFF * 10);
					*px = v << 16 | v << 8 | v;
					++cc,
						++px,
						++cfv;
				}
				else if (MODE == 2) {
					double lv = *cc;
					// very simple diffusion
					int avgc = 0;
					double avg = 0.0;
					if (*cc < *(cc - W))	++avgc, avg += *(cc - W);
					if (*cc < *(cc - 1))	++avgc, avg += *(cc - 1);
					if (*cc < *(cc + 1))	++avgc, avg += *(cc + 1);
					if (*cc < *(cc + W))	++avgc, avg += *(cc + W);
					*cfv = *cc;
					if (avgc) {
						//printf("arg %f  c %d\n", avg, avgc);
						if (*cc < avg / avgc)
							*cfv = (1.0 / (5 - avgc)) * avg / avgc;// (avg * 1e100 + *cc) / (1e100 + 1);
						//*cfv = sqrt(avg / avgc);
					}
					//if (*cc > 4 )
					//	//*cc -= 4,
					//	++*(cfv - W),
					//	++*(cfv - 1),
					//	++*(cfv + 1),
					//	++*(cfv + W);
					////
					//if (*cc > 4)
					//	*(cfv - W) += *cc / 4,
					//	*(cfv - 1) += *cc / 4,
					//	*(cfv + 1) += *cc / 4,
					//	*(cfv + W) += *cc / 4,
					//	*cc = 0;
					// drawing
					if (lv > mx)
						mx = lv;
					if (lv > 0.0) {
						if (lv < mn)
							mn = lv;
						uint8_t v;
						v = (uint8_t)((lv - ltmn) / rg * 0xFF * 10);
						*px = v << 16 | v << 8 | v;
					}
					++cc,
						++px,
						++cfv;
				}
				else if (MODE == 3) {
					// very simple diffusion
					//double dif;
					*cfv = *cc;
					// N
					//*cfv -= (*cc - *(cc - W)) / 4;
					//*cfv -= (*cc - *(cc + W)) / 4;
					//*cfv -= (*cc - *(cc - 1)) / 4;
					//*cfv -= (*cc - *(cc + 1)) / 4;
					*cfv =
						0.99 *
						max(*cc,
							max(
								max(*(cc - W), *(cc + W)),
								max(*(cc - 1), *(cc + 1))));
					//
					//*cfv = log(
					//	(
					//		pow(M_E, *(cc - W)) +
					//		pow(M_E, *(cc - 1)) +
					//		pow(M_E, *(cc + 1)) +
					//		pow(M_E, *(cc + W))
					//	) / 4.0
					//	);
					// drawing
					double lv = (*cc); //
					if (lv > mx)
						mx = lv;
					//if ((lv) && (lv < mn))
					if (lv < mn)
						mn = lv;
					uint8_t v;
					v = (uint8_t)((lv - ltmn) / rg * 0xFF * 4);
					*px = v << 16 | v << 8 | v;
					++cc,
						++px,
						++cfv;
				}
			}
			++cc, ++px, ++cfv;
		}

		//cc = pv;
		//double *cfv = fv; // current future value
		//for (int h = 0; h < H; ++h) {
		//	if (h == 0 || h == H - 1) {
		//		cc += W;
		//		cfv += W;
		//		continue;
		//	}
		//	++cc, ++cfv;
		//	for (int w = 0; w < W - 2; ++w) {
		//		// very simple diffusion
		//		*cfv = (*(cc - W) + *(cc - 1) + *(cc + 1) + *(cc + W)) / 4.0;
		//		// pre-test for new log diffusion
		//		double df;
		//		//df =
		//		//if(*(cc - W) / *cc > )

		//		//
		//		++cc, ++cfv;
		//	}
		//	++cc, ++cfv;
		//}
		// swap future and past
		double* tmp;
		tmp = dpv, dpv = dfv, dfv = tmp;

		// only update screen every nth pass
		if (fc % 1)
			continue;

		/* Update status and screen */
		SIMFW_SetStatusText(&simfw, "SIMFW TEST   fps %.2f  #%.4e  mx %.3e  mn %.3e", 1.0 / simfw.avg_duration, 1.0 / simfw.avg_duration * simfw.sim_height * simfw.sim_width, mx, mn);
		SIMFW_UpdateDisplay(&simfw);
	}

	/* Cleanup */
	SIMFW_Quit(&simfw);
}


void
ONED_DIFFUSION(void) {
	/* Init */
	//
	srand((unsigned)time(NULL));
	static pcg32_random_t pcgr;
	//
	SIMFW simfw = { 0 };
	SIMFW_Init(&simfw, SIMFW_TEST_TITLE, SIMFW_TEST_DSP_HEIGHT, SIMFW_TEST_DSP_WIDTH, SIMFW_TEST_DSP_WINDOW_FLAGS * 1, 0, 0);
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
	int yspeed = 1;
	double dwnr = 8; // draw - number of ranges
	uint64_t tm = 0; /* time */

	/* */
	const int scsz = simfw.sim_width;
	double* ans = malloc(scsz * sizeof(double));	// absolute numbers
	double* bans = malloc(scsz * sizeof(double));	// buffer for absolute numbers
	for (int i = 0; i < scsz; ++i)
		ans[i] = 1.0;
	double* rhlfs = malloc(scsz * sizeof(double));	// reflection factors
	for (int i = 0; i < scsz; ++i)
		if (i > scsz / 4 && i < scsz / 4 * 3)
			rhlfs[i] = 0.1;
		else
			rhlfs[i] = 0.9;



	ans[scsz / 2] = 2.0;
	//ans[scsz / 4] = 4.0;
	//ans[scsz / 8] = 8.0;
	//ans[scsz / 16] = 256;
	//ans[scsz / 2] = 256.0;




	/* SDL loop */
	int mouse_x = 0;
	while (1) {
		/* SDL event handling */
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			SDL_Keymod kbms = SDL_GetModState(); // keyboard mod state
			if (e.type == SDL_QUIT)
				break;
			else if (e.type == SDL_MOUSEMOTION) {
				mouse_x = e.motion.x;
			}
			else if (e.type == SDL_MOUSEBUTTONDOWN) {
				ans[mouse_x] *= 2.0;
			}
			else if (e.type == SDL_MOUSEWHEEL) {
				if (kbms & KMOD_SHIFT)
					dwnr *= pow(1.0001, e.wheel.y);
				else
					dwnr *= pow(1.01, e.wheel.y);
				SIMFW_SetFlushMsg(&simfw, "draw nr. ranges %.4e", dwnr);
			}
			else if (e.type == SDL_WINDOWEVENT_RESIZED) {
			}
			else if (e.type == SDL_KEYDOWN) {
				/* */
				int keysym = e.key.keysym.sym;
				switch (keysym) {
				case 0:
					break;
				case SDLK_F4:
					goto behind_sdl_loop;
				case SDLK_SPACE:
					for (int i = 0; i < scsz; ++i)
						ans[i] = 1.0;
					break;
				case SDLK_PERIOD:
					break;
				case SDLK_END:
					yspeed = simfw.sim_height;
					SIMFW_SetFlushMsg(&simfw, "yspeed %d", yspeed);
					break;
				case SDLK_HOME:
					yspeed = 1;
					SIMFW_SetFlushMsg(&simfw, "yspeed %d", yspeed);
					break;
				case SDLK_PAGEUP:
					if (kbms & KMOD_CTRL)
						yspeed += 1;
					else
						yspeed *= 2;
					SIMFW_SetFlushMsg(&simfw, "yspeed %d", yspeed);
					break;
				case SDLK_PAGEDOWN:
					if (kbms & KMOD_CTRL)
						yspeed -= 1;
					else
						yspeed /= 2;
					SIMFW_SetFlushMsg(&simfw, "yspeed %d", yspeed);
					break;
				}
			}
		}






		/* Scroll etc */
		yspeed = min(simfw.sim_height, yspeed);
		UINT32* px = simfw.sim_canvas + (simfw.sim_height - yspeed) * simfw.sim_width;
		SIMFW_Scroll(&simfw, 0, -yspeed * scsz);

		/* Draw new lines */
		double mn = 0, mx = 0;
		for (int rt = 0; rt < yspeed; ++rt) {
			/* get min/max */
			mn = DBL_MAX, mx = DBL_MIN;
			for (int i = 0; i < scsz; ++i) {
				if (ans[i] < mn) mn = ans[i];
				if (ans[i] > mx) mx = ans[i];
			}
			//mn = 1.0, mx = 256.0;
			/* draw */
			double lgmn = log(mn);
			double sl = 1.0 / (log(mx) - lgmn);	// scale
			for (int i = 0; i < scsz; ++i) {
				unsigned char bw;
				bw = 255 - (unsigned char)(255 * (dwnr - dwnr * sl * (log(ans[i]) - lgmn)));
				*px = bw << 16 | bw << 8 | bw;
				++px;
			}
			/* calc next */
			//ans[mouse_x] *= 1.001;
			ans[mouse_x] = 2;
			for (int i = 0; i < scsz; ++i) {
				// middle, left and right positions
				int pm = i, pl = pm - 1, pr = pm + 1;
				if (pl < 0) pl += scsz;
				if (pr >= scsz) pr -= scsz;
				//
				bans[pm] =
					ans[pm]
					+ rhlfs[pm] / 2.0 * (
						+rhlfs[pm] * (ans[pl] - ans[pm])
						+ rhlfs[pr] * (ans[pr] - ans[pm])
						);

				bans[pm] = max(DBL_MIN, bans[pm]);
			}
			/* swap current ans with buffered ones */
			double* sp;
			sp = ans, ans = bans, bans = sp;
		}

		/* Update status and screen */
		SIMFW_SetStatusText(&simfw,
			"SIMFW TEST   fps %.2f  sd %.2f\nmn %.2e/%.2e  mx %.2e/%.2e    mouse %.2e/%.2e\n%.4f",
			1.0 / simfw.avg_duration,
			1.0 * yspeed,
			mn, log(mn), mx, log(mx), ans[mouse_x], log(ans[mouse_x]),
			(log(ans[mouse_x]) - log(mn)) / (log(mx) - log(mn)));
		SIMFW_UpdateDisplay(&simfw);

	}
behind_sdl_loop:
	/* Cleanup */
	SIMFW_Quit(&simfw);
}
