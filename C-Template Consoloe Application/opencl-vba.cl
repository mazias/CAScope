/* Calculate next generation - fixed variant for neighbor-size = 3 and totalistic mode */
__kernel
void
ca_next_gen_vba(
	__global	uint16*		vba,		// cell-line input 
				uint		gnc,		// generation-count
				uint		rc,			// row-count
				uint		lc			// lane-count
)
{
	uint gid = get_global_id(0);
//printf("#  gid %d  gnc %d rc %d  lc %d\n", gid, gnc, rc, lc);
//return;
	for(int gi = 0; gi < gnc; ++gi) {
		//
		//
		for(int ri = 0; ri < rc; ++ri) {
			__global uint16* vbc = vba + gid + ri * lc;
			//vbc[0] |= vbc[2 * lc]; 
			//vbc[0] ^= vbc[1 * lc]; 

			//vbc[0] = 0xF0;
			vba[0] = gi + ri;
		}
	}
}