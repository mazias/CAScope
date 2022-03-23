// col-bit-array-conversion

// Version 1

/* Bit-array */
#define FBPT UINT16									// full-bit-block c-data-type
#define HBPT UINT8									// half-bit-block c-data-type
#define BPB 16										// bits per block, i.e. bits per elemet in bta 
typedef struct caBitArray {
	int ct;											// minimum count in bits - final count after initialization may be greater
	int rw;											// row-width / column-count in bits
	int rc;											// row-count
	FBPT* v;										// first valid element / array-pointer to bit array
} caBitArray;

void
initCABitArray(caBitArray* ba) {
	ba->rc = (ba->ct + ba->rw - 1) / ba->rw;				
	ba->ct = ba->rc * ba->rw;
	ba->v = _aligned_malloc(ba->ct / 8, 64);
}

/*
csv		cell-space first valid element
csi		cell-space first invalid element
ct		count / nr. cells to convert
mmwn	memory-window in rows/lines of clcn rows; should allow memory accesses to stay within fastest cache
---
returns pointer to bit-array
*/
void
convert_between_CA_CT_and_col_bit_array(CA_CT* csv, CA_CT* csi, caBitArray* ba, int mmwn) {
#define DG 1
	if (DG) printf("\33[2J\33[0;0fBIT-ARRAY (rc %d  rw %d  ct %d) [\n", ba->rc, ba->rw, ba->ct);

	int cp = 0;													// check-position-counter
	int dcl = 0;												// destination-column
	int drw = 0;												// destination-row
	CA_CT* csc = csv;											// cell-space-cursor / source-pointer
	while (1) {
		if (!cp) {
			if (DG) getch();
			if (drw && !(drw % mmwn))							// destination-row behind memory-window
				drw -= mmwn,									// > move destination-row back to beginning of memory-windows
				++dcl;											// > move to next column
			else if (drw == ba->rc)								// destination-row behind last row
				drw -= ba->rc % mmwn,							// > move destination-row back to beginning of memory-windows
				++dcl;											// > move to next column
			if (dcl == ba->rw) {								// destination-column behind row-width
				drw += mmwn;									// > move destination-row to beginning of next memory-windows
				dcl = 0;										// > move destination-column to beginning / zero
				if (drw >= ba->rc)								// destination-row behind last row
					break;										// > conversion finished
			}
			cp = min(mmwn, ba->rc - drw);						// next check-position is on next memory-windows or end of memory
			if (!dcl && !(drw % mmwn))							// when entering new memory-window 
				memset((UINT8*)ba->v + drw * ba->rw / 8, 0,		// > zero the new memory-window
					cp * ba->rw / 8);				
			csc = csv + (dcl * ba->rc + drw) % (csi - csv);		// calculate position in (horizontal-)source-byte-array
			cp = min(cp, csi - csc);							// next check-position is earlier when size of source array is not big enough
		}
		//
		if (DG) printf("\33[%d;%df%.4x ", drw * 2 + 2, 8 + dcl * 5, csc - *csv);
		if (DG) printf("\33[%d;%df%c%3x", drw * 2 + 3, 8 + dcl * 5, *csc ? '#' : '.',   drw * ba->rw / 8 + (dcl / BPB));
		//
		if (*csc) {
			FBPT* bac = ba->v + drw * ba->rw / BPB + (dcl / BPB);// bit-array-cursor
			*bac |= ((FBPT)1) << (dcl % BPB);
		}
		//
		++drw;
		++csc;
		--cp;
	}

	if (DG) printf("]\n");
}
