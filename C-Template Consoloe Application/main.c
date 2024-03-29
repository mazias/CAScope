//#include "mgm.c"
//#include <stdio.h>
//#include "stdafx.h"		// default includes (precompiled headers)
#include <basetsd.h>

typedef signed __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef signed __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef signed __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef signed __int64 int64_t;
typedef unsigned __int64 uint64_t;
#define MAXUINT32   ((UINT32)~((UINT32)0))

#include "color.h"

/* MAIN */
int main(int argc, char* args[]) {

	uint32_t v = 0xFFFF;
	printf("%x\n", v);
	printf("%x\n", v << 16);
	printf("%x\n", v << 14);
	printf("%x\n", v << -1);
	printf("%x\n", v << -5);

	//OutputDebugString(_T("Gentlemen...\n"));

	//SIMFW_Test_Mandelbrot();
	//MGM_MAIN();
	
	CA_MAIN();
	//SIMFW_TestBlockMemoryLayout();
	//sound_main();
	//ONED_DIFFUSION();
//	TWOD_DIFFUSION();
//TWOD_DIFFUSION_FP(); // fixed point diffusion



	//NEWDIFFUSION();
 	return 0;
	
	log2fix(MAXUINT32);
	log2fix(MAXUINT32 + 1);
	log2fix(0xFCDB);

	/* Do not close output window immediately */
	getchar();

	/* return status code for "no errors" */
	return 0;
}