/*
// generate random number
!!!!!!!!! http://cas.ee.ic.ac.uk/people/dt10/research/rngs-gpu-mwc64x.html#source_code
ulong seed = rs[0] + i;
seed = (seed * 0x5DEECE66DL + 0xBL) & ((1L << 48) - 1);
uint rnd = (seed >> 47);
*/

#define CA_CT uchar

/* Calculate next generation - fixed variant for neighbor-size = 3 and totalistic mode */
__kernel
void
ca_next_gen_tot_ncn3(
	__global	CA_CT	*cli,		// cell-line input 
	__global	CA_CT	*clo,		// cell-line output 
	__constant	CA_CT	*rltl)		// rule-table
{
	uint gid = get_global_id(0);
	
	clo[gid] = rltl[ cli[gid] + cli[gid + 1] + cli[gid + 2] ];
}

/* Calculate next generation - fixed variant for neighbor-size = 3 and absolute mode, 2 states */
__kernel
void
ca_next_gen_abs_ncn3_ncs2(
	__global	CA_CT	*cli,		// cell-line input 
	__global	CA_CT	*clo,		// cell-line output 
	__constant	CA_CT	*rltl)		// rule-table
{
	uint gid = get_global_id(0);
	
	clo[gid] = rltl[ 4 * cli[gid] + 2 * cli[gid + 1] + cli[gid + 2] ];
}

/* Calculate next generation - fixed variant for neighbor-size = 3 and totalistic mode */
#define LT (1)
//#define LS (1024 * 32)
#define LS (1024)
__kernel
void
ca_next_gen_range_tot_ncn3(
	__global	CA_CT	*cli,		// cell-line input 
	__global	CA_CT	*clo,		// cell-line output 
	__constant	CA_CT	*rltl,		// rule-table
	__constant	uint	*rg)		// range
{
/*
	// vector copy
	if (1) { 
		if (0&get_global_id(0) == 12) {
			uchar16 c;
			c = vload16(0, cli);
			printf("- %d %d %d %d cli %d %d %d %d\n", c.s0, c.s1, c.s2, c.s3, cli[0],cli[1],cli[2], cli[3]);
			c = vload16(1, cli);
			//uchar *pc;
			//pc = &c;
			//pc[2] = 0;
			printf("  %d %d %d %d cli %d %d %d %d\n", c.s0, c.s1, c.s2, c.s3, cli[1],cli[2],cli[3], cli[4]);
		}
		//size_t ls = get_local_size(0U);// - 2;
		//if (get_local_id(0U) < ls) {
#define BT	uint		// base type
#define BS	4			// base size in bytes
#define VT	uint16		// vector type
#define VS	16			// vector size in base items
#define WI	(BS * VS)


if (0) {
	size_t ts = WI * get_global_id(0U);
	for (size_t i = 0; i < WI; ++i) {
		clo[ts + i] = cli[ts + i];
	}
}
else if (0) {
	size_t gi = WI * get_global_id(0U);
	uchar c[WI];
	for (size_t i = 0; i < WI; ++i)
		clo[gi + i] = cli[gi + i];
}
else if (1) {
	size_t gi = WI * get_global_id(0U);
	uchar c[WI];
	for (size_t i = 0; i < WI; ++i)
		c[i] = cli[gi + i];
	for (size_t i = 0; i < WI; ++i)
		c[i] = 0;
	for (size_t i = 0; i < WI; ++i)
		clo[gi + i] = c[i];
}
else if (1) {
	size_t gi = get_global_id(0U);
	VT *vi = cli;
	VT *vo = clo;

	VT vc = vi[gi];
	uchar *c = &vc;

//	if (gi == 0)
//		printf("v %u\n", c[0]);

	for (size_t i = 0; i < WI / 2; ++i)
		c[i] = 0;//i % 3;
		
	vo[gi] = vc;
}
else {
	size_t ts = WI * get_global_id(0U);
	VT ci;
	ci = *((VT*)(&cli[ts]));
	//if (ts < 512)
		ci.s0 = 0;
	//*c = *c & 0x1111;
	*((VT*)(&clo[ts])) = ci;
}
//VT *co;
//co = &clo[ts];

//*co = *ci;

			//size_t te = ts + WI;
			//for (; ts < te; ts+=WI) {
//				ulong16 c;
//				c = vload16(0, (ulong*)(cli + ts));
//				//c.s1 = 0;
				//c.s2 = 0;
				//c.s3 = 0;
				//c.s0 = 0;
				//uint *cb = &c;
				//for( size_t ci = 0; ci < 16; ++ci )
				//	cb[ci] = 0;//ci % 3;
//				vstore16(c, 0, (ulong*)(clo + ts));
				//c.s0 = 0; c.s1 = 0; c.s2 = 0; c.s3 = 0; c.s4=0;c.s5=0;
			//	vstore16(c, 0, clo + ts);
				//jkjkclo[ts] = c.s0;//cli[ts];
			//}
		//}
	}
	// continueos copy
	if (0) { 
		size_t ls = get_local_size(0U) - 2;
		if (get_local_id(0U) < ls) {
			//size_t ts = *rg * get_global_id(0U);
			size_t ts = *rg * (ls * get_group_id(0U) + get_local_id(0U));
			size_t te = ts + *rg;
			for (; ts < te; ++ts)
				clo[ts] = cli[ts];
		}
	}
	// strided?? copy
	if (0) {
		size_t ls = get_local_size(0U) - 2;
		if (get_local_id(0U) < ls) {
			size_t ts = ls * *rg * get_group_id(0U) + get_local_id(0U);
			size_t te = ts + *rg * ls;
			if (0&&get_local_id(0) == 0 )
				printf("gid %d  s %d  e %d  ls %d  rg %d\n", get_group_id(0), ts, te, ls, *rg);
			for (; ts < te; ts += ls) { 
				clo[ts] = cli[ts];
				//barrier(CLK_GLOBAL_MEM_FENCE);
			}

			return;
		}
	}
	// strided?? life
	if (0) {
		local CA_CT lcl[LS];
		size_t ls = get_local_size(0U) - 2 * LT;
		size_t lid = get_local_id(0U);
		size_t gid = ls * *rg * get_group_id(0U) + lid;
		size_t te = gid + *rg * ls;
		for (; gid < te; gid += ls) { 
			lcl[lid] = cli[gid];
			CA_CT n = cli[gid];
			//barrier(CLK_LOCAL_MEM_FENCE);
			//if (gid == 1121 || gid == 1122 || gid == 1123)
			//	printf("lid %d  lcl %d %d %d  cli %d %d %d\n", lid, lcl[lid + 0], lcl[lid + 1], lcl[lid + 2], cli[gid], cli[gid+1], cli[gid+2]);

			for (size_t ti = 0; ti < LT; ++ti) {
				if (lid < get_local_size(0U) - 2) {
					//if (0&&get_local_id(0) == 0 )		printf("gid %d  s %d  e %d  ls %d  rg %d\n", get_group_id(0), ts, te, ls, *rg);
					//barrier(CLK_LOCAL_MEM_FENCE);
			barrier(CLK_LOCAL_MEM_FENCE);
					n += lcl[lid + 1] + lcl[lid + 2];
					n = rltl[n];
					lcl[lid] = n;				
				}
			}

			if (lid < ls)
				clo[gid] = lcl[lid];
		}
	}
*/
}

void
REMca_next_gen_range_tot_ncn3(
	__global	CA_CT	*cli,		// cell-line input 
	__global	CA_CT	*clo,		// cell-line output 
	__constant	CA_CT	*rltl,		// rule-table
	__constant	uint	*rg)		// range
{

/*
	for (int ri = *rg - 1; ri >= 0; --ri) { 
		uint gid = ri + *rg * get_global_id(0);	
		clo[gid] = rltl[ cli[gid] + cli[gid + 1] + cli[gid + 2] ];
	}
return;
*/
//barrier(CLK_LOCAL_MEM_FENCE);
	CA_CT lcl[LS];
	uint e = *rg;
	uint gs = e * get_global_id(0);	// global start
	uint ls = (e + LT * 2) * get_local_id(0);	// local start
	ls = 0;

	//if (s == 0) { 
	if (0&get_global_id(0) == 0) { 
		printf("local sz %d  lid %d  gid %d\n", get_local_size(0), get_local_id(0), get_global_id(0));
	}

	e += 2 * LT;
	for (uint i = 0; i < e; ++i)
		lcl[ls + i] = cli[gs + i];
	for (uint t = 0; t < LT; ++t) { 
		e -= 2;
		for (uint i = 0; i < e; ++i)
			lcl[ls + i] = rltl[ lcl[ls + i] + lcl[ls + i + 1] + lcl[ls + i + 2] ];
	}
	for (uint i = 0; i < e; ++i)
		clo[gs + i] = lcl[ls + i];
//barrier(CLK_GLOBAL_MEM_FENCE);


	//
//	int i = *rg * get_global_id(0);
//	int e = i + *rg;
//	for (; i < e; ++i) { 
//		clo[i] = lrltl[ cli[i] + cli[i + 1] + cli[i + 2] ];
//	}
	//
//	for (int ri = *rg - 1; ri >= 0; --ri) { 
//		uint gid = ri + *rg * get_global_id(0);	
//		clo[gid] = rltl[ cli[gid] + cli[gid + 1] + cli[gid + 2] ];
//	}
}

/* Calculate next generation - flexible variant - can handle totalistic and absolute rules by use of multiplication table */
__kernel
void
ca_next_gen(
	__global	CA_CT	*cli,		// cell-line input 
	__global	CA_CT	*clo,		// cell-line output 
	__constant	uint	*ncn,		// number of cells in a neighborhod
	__constant	CA_CT	*rltl,		// rule-table
	__constant	CA_CT	*mntl)		// multiplication-table
{
#define TM 8
	if (0) {
		size_t gid = 16 * get_global_id(0U);
		for (size_t i = 0; i < 16; ++i) {
			clo[gid + i] = cli[gid + i];
		}
	}
	if (1) {
		local CA_CT lc[512];
		//if (lc[500] != 0)	printf("!!!\n");
		//size_t gid = 16 * get_global_id(0U) + get_local_id(0U);

		//
		size_t lid = get_local_id(0U);
		size_t gid = get_group_id(0U) * (get_local_size(0U) - 2 * TM) + lid;




		//if (lid < get_local_size(0U) - 2)
		//	clo[gid] = rltl[ cli[gid] + cli[gid + 1] + cli[gid + 2] ];
		//return;


		
		//if (0|gid < get_local_size(0U))	printf("groupdid %d gid %d - %d\n", get_group_id(0U), gid, gid + get_local_size(0U));

		lc[lid] = cli[gid];
		barrier(CLK_LOCAL_MEM_FENCE);
		//barrier(CLK_GLOBAL_MEM_FENCE);
		//
		if (lid < get_local_size(0U) - 2) {
			for (size_t t = 1; t <= TM; ++t) {
				CA_CT n = 0;
				n = lc[lid] + lc[lid + 1] + lc[lid + 2];
				n = rltl[ n ];
				//n = rltl[ cli[gid] + cli[gid + 1] + cli[gid + 2] ];
				//clo[gid] = n;//rltl[ cli[gid] + cli[gid + 1] + cli[gid + 2] ];
				
				//n = rltl[ n ];
				//n = lc[lid];
				lc[lid] = n;
				barrier(CLK_LOCAL_MEM_FENCE);
				//
				//if (n < 3)
			}
			if (lid < get_local_size(0U) - 2 * TM)
				clo[gid] = lc[lid];
		}
		//if (lid < get_local_size(0U) - 2 * TM)
	}

	return;

//	size_t gid = get_global_id(0U);
	//CA_CT n;
///	clo[gid] = rltl[ cli[gid] + cli[gid + 1] + cli[gid + 2] ];
	//clo[gid] = n;

	//if (1 && gid == 0)		printf("gid %u  ncn %u  rltl[0] %d  mntl[0] %d  cli[0] %d\n", gid, *ncn, rltl[0], mntl[0], cli[0]);
	//if (1 && gid == 0)		printf("ca_next_gen  llsz %d\n", get_local_size(0));
}

/*
// Calculate next generation - flexible variant - can handle all neighborhod sizes and totalistic and absolute rules by use of multiplication table 
// ~15fps
__kernel
void
ca_next_gen(
	__global	CA_CT	*cli,		// cell-line input 
	__global	CA_CT	*clo,		// cell-line output 
	__constant	uint	*ncn,		// number of cells in a neighborhod
	__constant	CA_CT	*rltl,		// rule-table
	__constant	CA_CT	*mntl)		// multiplication-table
{
	size_t gid = get_global_id(0U);
	CA_CT n = mntl[0] * cli[gid];
	for (size_t i = 1; i < *ncn; ++i)
		n += mntl[i] * cli[gid + i];
	clo[gid] = rltl[n];

	//if (1 && gid == 0)		printf("gid %u  ncn %u  rltl[0] %d  mntl[0] %d  cli[0] %d\n", gid, *ncn, rltl[0], mntl[0], cli[0]);
	//if (1 && gid == 0)		printf("ca_next_gen  llsz %d\n", get_local_size(0));
}




*/