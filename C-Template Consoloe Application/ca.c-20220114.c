﻿
// rule 1507 und 1146 in stzm 1 schön

#include "stdafx.h"		// default includes (precompiled headers)
#include "simfw.h"
#include "color.h"
#include "float.h"
#include "hp_timer.h"
#include "pcg_basic.h"			// Random Number Library

#include "math.h"
#define M_E        2.71828182845904523536   // e

/* OpenCL */
// to enable OpenCL also include OpenCL headers in Settings > Compiler > Additional Include Directories and the lib file in Settings > Linker > Additional Dependencies / Zusätzliche Abhängigkeiten
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS // https://stackoverflow.com/questions/28500496/opencl-function-found-deprecated-by-visual-studio
#include <CL/cl.h>

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

typedef struct caDisplaySettings {
	int fsme;										// focus-mode - 0: absolute, 1: totalistic
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
	int vlfs;										// focus > nr. of hlfs cells next to eachother are summed... TODO 
	int hlfs;										// focus > nr. of hlfs cells next to eachother are summed... TODO 
	int vlzm;										// vertical zoom - how many vertical cells will make out one pixel (together with hor. zoom); 0 = disable drawing, < 0 = how many vertical pixels a cell will make out
	int hlzm;										// horizontal zoom - how many horizontal cells will make out one pixel (together with vert. zoom); 0 = disable drawing, < 0 = how many horizontal pixels a cell will make out
	int vlpz;										// vertical pixel zoom
	int hlpz;										// horizontal pixel zoom
	int stzm;										// state zoom - 0 = direct zoom on intensity
	int sm;											// stripe-mode
	int mdpm;										// mode-paramter (used by sfcm-modes)
} caDisplaySettings;

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
#define VBBT UINT32									// vertical-bit-block-type
#define VBBB 32										// vertical-bits-per-bit-block-type
#define VBPBS 5										// vertical bits per block shift - i.e. nr of bit shifts needed to divide by VBBB
#define VBPBMM (VBBB - 1)							// vertical-bits-per-block-mod-mask - X & (BPB - 1) is equivalent to X % BPB when BPB is power of 2 
#define FBPT UINT16									// full-bit-block c-data-type
#define HBPT UINT8									// half-bit-block c-data-type
#define BPB 16										// bits per block, i.e. bits per elemet in bta 
#define BPBMM (BPB - 1)								// bits-per-block-mod-mask - X & (BPB - 1) is equivalent to X % BPB when BPB is power of 2 
#define BPBS 4										// bits per block shift - i.e. nr of bit shifts needed to divide by BPB
typedef struct caBitArray {
	int lw;											// lane-width in bits
	int lc;											// lane-count in nr. of lanes
	int rw;											// row-width / column-count in bits; rw = lw * lc
	int rc;											// row-count
	int oc;											// overflow-row-count - set to 0 for auto-initialization
	int mmwn;										// memory-window in rows/lines - must be power of two! - (each rw wide; should allow memory accesses to stay within fastest cache
	int ct;											// count in bits
	int sz;											// size in bytes
	VBBT* v;										// first valid element / array-pointer to bit array
	CA_CT* clsc;									// cell-space - original cell-space-array
	int scsz;										// original space-size
	int brsz;										// original border-size
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

// fnv hash: http://www.isthe.com/chongo/tech/comp/fnv/index.html 

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

// **********************************************************************************************************************************
/* OpenCL */
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

/**
* from, to		in bits, must be multiple of size of ba->v type (currently 32)
*/
__inline
void
CABitArrayPrepareOR(caBitArray* ba, int from, int to) {
	int bbrw = ba->rw / VBBB;			// bit-blocks per row
	if (to == 0)
		to = bbrw;
	else
		to = to / VBBB;
	for (int r = 0; r < ba->oc; ++r) {
		for (int c = from / VBBB; c < to; ++c) {
			ba->v[(r + ba->rc) * bbrw + c] =
				(ba->v[r * bbrw + c] >> 1) | (ba->v[r * bbrw + ((c + 1) % bbrw)] << VBPBMM);
			//			printf("VB OR  bbrw %d  x %d  x+1 %d %d\n", bbrw, c, (c + 1) & (bbrw - 1), ((c + 1) % bbrw));
		}
	}
}

/*
* See notes of caBitArray struct!
* TODO: check if mmwn is power of 2 and greater 0
*/
void
initCABitArray(caBitArray* ba, CA_RULE* rl) {
	if (ba->lw && ba->lc)
		ba->rw = ba->lw * ba->lc;
	else {
		ba->lw = ba->rw = ba->ct;
		ba->lc = 1;
	}
	ba->rc = max(1, (ba->ct + ba->rw - 1) / ba->rw);						// calculate row-count by dividing ct by rw rounding up
	ba->rc = ((ba->rc + ba->mmwn - 1) / ba->mmwn) * ba->mmwn;					// align row-count with memory-window
	ba->ct = ba->rc * ba->rw;
	if (ba->oc == 0)
		ba->oc = rl->ncn - 1;
	ba->sz = (ba->ct + ba->oc * ba->rw) / 8;
	_aligned_free(ba->v);							// aligned memory management may be necesary for use of some intrinsic or opencl functions
	ba->v = _aligned_malloc(ba->sz, BYAL);
	printf("initCABitArray   end    ct %d  rc %d  oc %d  rw %d  lc %d  lw %d  mmwn %d  sz %d  v %p\n", ba->ct, ba->rc, ba->oc, ba->rw, ba->lc, ba->lw, ba->mmwn, ba->sz, ba->v);
}

/*
csv		cell-space first valid element
csi		cell-space first invalid element
ba		bit-array
dr		direction - 0: csv -> ba; !0 ba -> csv
*/
void
convertBetweenCACTandCABitArray(CA_CT* csv, CA_CT* csi, caBitArray* ba, int dr) {
#define DG 0
	if (DG) printf("\33[2J\33[0;0fBIT-ARRAY (rc %d  rw %d  ct %d) [\n", ba->rc, ba->rw, ba->ct);

	int cp = 0;												// check-position-counter
	int dcl = 0;											// destination-column
	int drw = 0;											// destination-row
	CA_CT* csc = csv;										// cell-space-cursor / source-pointer

	if (!dr)
		memset((UINT8*)ba->v, 0, ba->rc * ba->rw / 8);		// ba has to be zero'd before use, as individual bits are 'ored' in

	goto calculate_next_check_position;

check_position:
	//if (DG) getch();
	if (!(drw & (ba->mmwn - 1))) {							// destination-row behind memory-window or last row
		drw -= ba->mmwn;									// > move destination-row back to beginning of memory-window
		++dcl;												// > move to next column
//		printf("drw %d  dcl %d\n", drw, dcl);
		if (dcl == ba->rw) {								// destination-column behind row-width
			drw += ba->mmwn;								// > move destination-row to beginning of next memory-window
			dcl = 0;										// > move destination-column to beginning / zero
			if (drw >= ba->rc)								// if destination-row is behind last row
				goto end_loop;								// > conversion is finished
		}
	}
calculate_next_check_position:;								// calculate position in (horizontal-)source-byte-array
	int i = drw + dcl * ba->rc;								// index
	int l = csi - csv;										// length
	if (i >= l) {											// avoid using expensive modulo operator
		i -= l;
		if (i >= l)
			i %= l;
	}
	csc = csv + i;
	cp = min(ba->mmwn, csi - csc);							// next check-position is next memory-window or earlier when size of source array is not big enough
convert_next_bit:;
	VBBT* bac = ba->v + ((drw * ba->rw + dcl) >> VBPBS);	// bit-array-cursor
	unsigned int bacsp = ba->rw >> VBPBS;					// bit-array-cursor-step (in bytes)
	unsigned int bacst = dcl & VBPBMM;						// bit-array-cursor-shift (in bits)
convert_next_bit_inner:
	///		if (DG) printf("\33[%d;%df%.3x %c ", drw + 2, 8 + dcl * 6, csc - csv, *csc ? '#' : '.');
	if (DG) printf("\tcsc %d  bar %d  bac %d   bax #%x  %c\n", csc - csv, drw, dcl, bac - ba->v, *csc ? '#' : '.');
	if (dr)
		*csc = 1 & ((*bac) >> bacst);						// convert ba -> csv
	else
		*bac |= ((VBBT)*csc) << bacst;						// convert csv -> ba 
	//
	++drw;
	++csc;
	--cp;
	if (!cp)
		goto check_position;
	bac += bacsp;
	goto convert_next_bit_inner;
end_loop:

	if (DG) printf("]\n");
}
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
		//csv[bi] = (FBPT)1 & (bav[bi / BPB] >> ((bi % BPB)));
}

/* Calculate next generation - CAN ONLY HANDLE TOTALISTIC ATM, ncn fixed */
void
ca_next_gen_ncn3_simd(
	CA_RULE* cr,	// cellular-automaton rule
	CA_CT* clv,		// cell-line first valid element
	CA_CT* cli		// cell-line first invalid element
) {
	while (clv < cli) {
		__m256i ymm0 = _mm256_loadu_si256(clv);
		//_mm256_sll // shift?
		//_mm256_slli_si256
		ymm0 = _mm256_adds_epu8(ymm0, *(__m256i*)(clv + 1));
		ymm0 = _mm256_adds_epu8(ymm0, *(__m256i*)(clv + 2));
		for (int i = 0; i < 32; i += 4)
			ymm0.m256i_u8[i + 0] = cr->rltl[ymm0.m256i_u8[i + 0]],
			ymm0.m256i_u8[i + 1] = cr->rltl[ymm0.m256i_u8[i + 1]],
			ymm0.m256i_u8[i + 2] = cr->rltl[ymm0.m256i_u8[i + 2]],
			ymm0.m256i_u8[i + 3] = cr->rltl[ymm0.m256i_u8[i + 3]];
		_mm256_storeu_si256(clv, ymm0);
		clv += 32;
	}
}

/* Calculate next generation - ford ONLY HANDLE absolute, ncn and ncs fixed */
void
ca_next_gen_abs_ncn3_ncs2_simd(
	CA_RULE* cr,	// cellular-automaton rule
	CA_CT* clv,		// cell-line first valid element
	CA_CT* cli		// cell-line first invalid element
) {
	while (clv < cli) {
		__m256i ymm0 = _mm256_loadu_si256(clv);
		ymm0 = _mm256_slli_epi64(ymm0, 1);					// shift one bit to the left
		ymm0 = _mm256_adds_epu8(ymm0, *(__m256i*)(clv + 1));
		ymm0 = _mm256_slli_epi64(ymm0, 1);					// shift one bit to the left
		ymm0 = _mm256_adds_epu8(ymm0, *(__m256i*)(clv + 2));


		/*
		//old code using propably not optimal method for shifting
		__m256i ymm0 = _mm256_loadu_si256(clv);
		__m128i xmm0 = _mm_setzero_si128();
		xmm0.m128i_i64[0] = 1;
		ymm0 = _mm256_sll_epi64(ymm0, xmm0);		// shift one bit to the left
		ymm0 = _mm256_adds_epu8(ymm0, *(__m256i*)(clv + 1));
		ymm0 = _mm256_sll_epi64(ymm0, xmm0);		// shift one bit to the left
		ymm0 = _mm256_adds_epu8(ymm0, *(__m256i*)(clv + 2));
		*/

		for (int i = 0; i < 32; i += 4)
			ymm0.m256i_u8[i + 0] = cr->rltl[ymm0.m256i_u8[i + 0]],
			ymm0.m256i_u8[i + 1] = cr->rltl[ymm0.m256i_u8[i + 1]],
			ymm0.m256i_u8[i + 2] = cr->rltl[ymm0.m256i_u8[i + 2]],
			ymm0.m256i_u8[i + 3] = cr->rltl[ymm0.m256i_u8[i + 3]];
		_mm256_storeu_si256(clv, ymm0);
		clv += 32;
	}
}


/* Calculate next generation - flexible variant - can handle totalistic and absolute rules by use of multiplication table */
void
ca_next_gen__sisd(
	CA_RULE* cr,			// cellular-automaton rule
	CA_CT* clv,				// cell-line first valid element
	CA_CT* cli				// cell-line first invalid element
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
	CA_RULE* cr,			// cellular-automaton rule
	CA_CT* clv,				// cell-line first valid element
	CA_CT* cli				// cell-line first invalid element
) {
	while (clv < cli) {
		*clv = cr->rltl[cr->mntl[0] * clv[0] + cr->mntl[1] * clv[1] + cr->mntl[2] * clv[2]];
		++clv;
	}
}


/* Calculate next generation - CAN ONLY HANDLE TOTALISTIC ATM   flexible variant - can handle totalistic and absolute rules by use of multiplication table */
void
ca_count__simd(
	const CA_CT* csv,					// cell-space first valid element
	const CA_CT* csf,					// cell-space element to start with
	const CA_CT* csi,					// cell-space first invalid element
	const UINT32* pbv,					// pixel-buffer first valid element
	const UINT32* pbi,					// pixel-buffer first invalid element
	const int hlzm,						// horizontal zoom
	const int hlfs,						// horizontal focus
	const int fsme,						// focus-mode - 0 = totalistic, 1 = absolute
	const int ncs						// number of cell-states
) {
	register UINT32* pbc = pbv;			// pixel-buffer cursor / current element
	register CA_CT* csc = csf;			// cell-space cursor / current element
	register CA_CT* csk = csc;			// cell-space check position - initialised to equal csc in order to trigger recalculation of csk in count_check
	register __m256i ymm0;				// used by simd operations

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
	if (fsme == 1)
		goto count_sisd;
	if (csk - csc < 32)							// single stepping may always be necesary as pixel-buffer may be of any size
		goto count_sisd;
	if (csc >= csk)
		goto count_check;
	ymm0 = _mm256_loadu_si256(csc);
	for (i = 1; i < hlfs; ++i)
		ymm0 = _mm256_adds_epu8(ymm0, *(__m256i*)(csc + i));
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
		if (fsme == 0)
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
		__m256i ymm0 = _mm256_loadu_si256(clv);
		for (int i = 1; i < cr->ncn; ++i)
			ymm0 = _mm256_adds_epu8(ymm0, *(__m256i*)(clv + i));
		for (int i = 0; i < 32; i += 4)
			ymm0.m256i_u8[i + 0] = cr->rltl[ymm0.m256i_u8[i + 0]],
			ymm0.m256i_u8[i + 1] = cr->rltl[ymm0.m256i_u8[i + 1]],
			ymm0.m256i_u8[i + 2] = cr->rltl[ymm0.m256i_u8[i + 2]],
			ymm0.m256i_u8[i + 3] = cr->rltl[ymm0.m256i_u8[i + 3]];
		_mm256_storeu_si256(clv, ymm0);
		clv += 32;
	}
}



typedef enum {
	CM_DISABLED,			// no computation 
	CM_SISD,				// single instrunction single data
	CM_SIMD,				// single instruction multiple data
	CM_LUT,					// look-up-table		bit-array
	CM_HASH,				// hash-table			bit-array needed
	CM_VBA_1x256,			// vertical-bit-array - 1 lane with 256bits (AVX)
	CM_BOOL,				// boolean operators	bit-array
	CM_VBA_1x32,			// vertical-bit-array - 1 lane with 32bit
	CM_VBA_2x32,			// vertical-bit-array - 2 lanes with 32bit
	CM_VBA_4x32,			// vertical-bit-array - 4 lanes with 32bit
	CM_OMP_VBA_8x32,		// vertical-bit-array - 2 lanes with 256bits (AVX)
	CM_VBA_1x64,			// vertical-bit-array - 1 lane with 64bit
	CM_VBA_16x256,			// vertical-bit-array - 16 lanes with 256bits (AVX)
	CM_VBA_2x256,			// vertical-bit-array - 2 lanes with 256bits (AVX)
	CM_OMP_VBA_8x1x256,		// vertical-bit-array - 8 lanes with 256bits (AVX)
	CM_OMP_TEST,
	CM_OPENCL,				// vertical-bit-array 
	CM_MAX,
} caComputationMode;

static UINT8* calt = NULL;							// ca-lookup-table/lut

void CA_CNITFN_LUT(caBitArray* vba, CA_RULE* cr) {
	_aligned_free(vba->v);
	vba->ct = (vba->ct + BPB - 1) / BPB * BPB + BPB;		// bit-array-count (rounded up / ceil) // allow one extra-element since when we process the last half block we read one half block past the end
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
			calt[(dn - 1)<<16 | i] = (UINT8)cs;
			// inc / finish
			if (i == MAXUINT16)
				break;
			++i;
		}
	}
}


/* Hash-Cell*/


typedef UINT32 HCI;							// hash-cell-index-type
#define HCITPBYC 4							// hash-cell-index-type-byte-count
UINT32 HCISZPT = 20;							// hash-cell-index-size as power of two, i.e. 8 = index size of 256;
#define HCTMXLV	128							// hash-cell-table-max-level

#define HCTBSPT	3							// hash-cell-table-base-size as power of two
#define HCTBS (1<<HCTBSPT)					// hash-cell-table-base-size / size of lowest/smallest level
#define HCTBSMK ((1<<HCTBSPT) - 1)			// hash-cell-table-base-size-mask 

#define HCIMASK (((UINT32)1<<HCISZPT)-1)	// hash-cell-index-mask /* for example: (u_int32_t)0xffff */
#define HCISZ ((UINT32)1 << HCISZPT)		// hash-cell-index-size

/* Hash-Cells-Node */
typedef struct
HCN {
	HCI ln;									// left-node-index
	HCI rn;									// right-node-index
	HCI r;									// result-node-index
	UINT8 fs;								// flags: 0 = node is empty; &1 = node not empty; &2 = node marked as active
	UINT8 ll;								// level
	UINT32 uc;								// usage-count
} HCN;
HCI hcln[HCTMXLV] = { 0 };					// hash-cells-left-node (used when building hash-table)
int hcls[HCTMXLV] = { 0 };					// hash-cells-left-node-set boolean (used when building hash-table)
HCN* hct = NULL;							// hash-cell-table

#define HCISKSZ 256							// hash-cell-stack-size
HCI hciskps = 0;							// hash-cell-stack-position
HCI hcisk[HCISKSZ] = { 0 };					// hash-cell-stack

UINT8 hc_cg = 0;							// hash-cell-current-generation
HCI hc_sn = 0;								// node holding current space
int hc_sl = 0;								// level of current space

typedef struct
HC_STATS {
	int nc;									// node-count
	int sc;									// search-count
	int fc;									// found-count
	int ss;									// seek-sum (can be used with sc to calculate average seek-time)
	int rqct;								// recyqle-count
	int uc;									// unique-count
	int ucmn;								// uinque-count minimum
	int ucmx;								// unique-count maximum
	int tc;									// total-count
} HC_STATS;
HC_STATS hc_stats[HCTMXLV] = { 0 };

void HC_print_stats(int crsn) {
	if (crsn)
		printf("\33[2J\33[0;0f");
	// display stats
	memset(&hc_stats[HCTMXLV - 1], 0, sizeof(hc_stats[HCTMXLV - 1]));	// reset sums
	printf("-\n");
	printf("ll  %8s %4s  %8s %8s %9s  %8s  %9s %4s  %9s %9s %9s %9s  %10s  ll\n", 
		"nc", "%", "sc", "fc", "%", "rqct", "seeks", "avgsk", "uc", "ucmn", "ucmx", "tc", "size");
	static HC_STATS sl = { 0 };		// stats-last
	HC_STATS sd = { 0 }; // stats-delta
	for (int i = 0; i < HCTMXLV; ++i) {
		if (!hc_stats[i].nc && i < HCTMXLV - 2)
			continue;
		if (i < HCTMXLV - 2) {
			hc_stats[HCTMXLV - 1].nc += hc_stats[i].nc;
			hc_stats[HCTMXLV - 1].sc += hc_stats[i].sc;
			hc_stats[HCTMXLV - 1].fc += hc_stats[i].fc;
			hc_stats[HCTMXLV - 1].rqct += hc_stats[i].rqct;
			hc_stats[HCTMXLV - 1].ss += hc_stats[i].ss;
			hc_stats[HCTMXLV - 1].uc += hc_stats[i].uc;
			if (hc_stats[i].ucmn < hc_stats[HCTMXLV - 1].ucmn || !hc_stats[HCTMXLV - 1].ucmn)
				hc_stats[HCTMXLV - 1].ucmn = hc_stats[i].ucmn;
			hc_stats[HCTMXLV - 1].ucmx = max(hc_stats[i].ucmx, hc_stats[HCTMXLV - 1].ucmx);
			hc_stats[HCTMXLV - 1].tc += hc_stats[i].tc;
		}
		else if (i == HCTMXLV - 2) {
			sd.rqct = hc_stats[HCTMXLV - 1].rqct - sl.rqct;
			sd.nc = hc_stats[HCTMXLV - 1].nc - sl.nc;
			sd.sc = hc_stats[HCTMXLV - 1].sc - sl.sc;
			sd.fc = hc_stats[HCTMXLV - 1].fc - sl.fc;
			sd.ss = hc_stats[HCTMXLV - 1].ss - sl.ss;
			sd.uc = hc_stats[HCTMXLV - 1].uc - sl.uc;
			hc_stats[HCTMXLV - 2] = sd;
			sl = hc_stats[HCTMXLV - 1];
			printf("=== DELTA\n");
		} else
			printf("=== SUM\n");

		printf("%2d  %8d %3.0f%%  %8d %8d %8.4f%%  %8d  %9d %5.2f  %9d %9d %9d %9d  %10I64u  %2d\n",
			i,
			hc_stats[i].nc,
			(100.0 / (double)HCISZ * (double)hc_stats[i].nc),
			hc_stats[i].sc,
			hc_stats[i].fc,
			(100.0 / max(1, abs(hc_stats[i].sc)) * abs(hc_stats[i].fc)),
			hc_stats[i].rqct,
			hc_stats[i].ss,
			(double)hc_stats[i].ss / max(1.0, abs((double)hc_stats[i].sc)),
			hc_stats[i].uc,
			hc_stats[i].ucmn,
			hc_stats[i].ucmx,
			hc_stats[i].tc,
			(UINT64)HCTBS << i,
			i);
	}
	printf("---\n");
	printf("sz  tl %ub / %.2fmb  n %u / %.2fb/n\n", HCISZ * sizeof(HCN), (float)HCISZ * sizeof(HCN) / 1024.0 / 1024.0, HCISZ, (float)sizeof(HCN));
	printf("-\n");
}

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
		//	printf("%04x %04x  %X  ", hct[n].ln << 8, hct[n].rn, (hct[n].ln << 8) | hct[n].rn);
		//	print_byte_as_bits(calt[0xFFFF & ((hct[n].ln << 8) | hct[n].rn)]);
		//	printf("\n");
		//}
	}
}

/*
vba		vba->v needs to point to linear-bit-array as created by convert_CA_CT_to_bit_array
*ll		will receive the level of the result-node

return node-id of result-node - use together with *ll to access result node
*/
HCI convert_bit_array_to_hash_array(caBitArray* vba, int* ll) {
#define D 0			// debug-flag
	int bi = 0;								// 16-bit-index
	int ml = 0;		// max-level
	HCI ltnd = 0;	// last added node
	if (HCTBS == 8) {
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
			*clv = n & (1 << i) ? 1 : 0;
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
pbv		pixelo-buffer first valid element
pbi		pixelo-buffer first invalid element
mf		multiplication-factor
mfa		mf-acceleration, i.e. the factor of which mf is increased in next level

return next free adress of cell-space
> if count of cells in hash-table is the same or more than cells in cell-space, cli is returned
*/
UINT32* display_hash_array(caDisplaySettings ds, UINT32* pbv, UINT32* pbf, UINT32* pbi, double mf, double mfa, double v, int ll, HCI n) {


	int rg;
	rg = hc_stats[ll].ucmx - hc_stats[ll].ucmn;
	if (rg > 0) {
		if (mfa == 1.0) {
			//v *= 1.0 + (hc_stats[ll].ucmx - hc_stats[ll].ucmn) - ((double)hct[n].uc - hc_stats[ll].ucmn);
			//v += log2(1.0+((double)hc_stats[ll].ucmx - hc_stats[ll].ucmn) - ((double)hct[n].uc - hc_stats[ll].ucmn));

			//if (!ll) 
				v += log2((double)hct[n].uc / hc_stats[ll].ucmn);


			//v *= 1.0 + ((double)hct[n].uc - hc_stats[ll].ucmn);
		}
		else {
			v += mf * ((double)(hct[n].uc - hc_stats[ll].ucmn) / (double)rg);

			//if (rand() % 30000 == 1)
			//	printf("mf %e  mfa %e  v %e  rg %d  uc %d\n", mf, mfa, v, rg, hct[n].uc);
			mf *= mfa;
		}
	}
	if (ll == 0) {
//		if (v > 1.01)
		UINT32 c = v * 0xFFFFFF;
		if (v > 0xFFFFFFFF / 4)
			printf("v! %f\n", v);
//		v = min(1.0, max(0.0, v));
//		UINT32 c = (double)0xFFFFFFFF * v;
		for (int i = 0; i < 8 / ds.hlzm; i++) {
			if (pbf < pbv)
				break;// pbf += pbi - pbv;
			if (pbf >= pbi)
				break;// pbf -= pbi - pbv;

			*pbf = c;

			++pbf;
		}
	}
	else {
		pbf = display_hash_array(ds, pbv, pbf, pbi, mf, mfa, v, ll - 1, hct[n].ln);
		pbf = display_hash_array(ds, pbv, pbf, pbi, mf, mfa, v, ll - 1, hct[n].rn);
	}
	return pbf;
}

/*
ll			level
n			node

marks active nodes and clears uc/usage-count
*/
int HC_mark_active_nodes(int ll, HCI n) {
	int mc = 0;

	ll = hct[n].ll;

	//if (hct[n].fs) 
	{
		if ((hct[n].fs == 1))
		{
			mc++;
			hct[n].fs = 2;
			if (ll) {
				mc += HC_mark_active_nodes(ll - 1, hct[n].ln);
				mc += HC_mark_active_nodes(ll - 1, hct[n].rn);
				mc += HC_mark_active_nodes(ll - 1, hct[n].r);
			}
		}
	}
	return mc;
}

void HC_sweep_inactive_nodes(mxlv) {
	memset(hc_stats, 0, sizeof(hc_stats));

	for (int ni = 0; ni < HCISZ; ++ni) {
		//if (hct[ni].fs) {
		//if (hct[ni].fs == 2) 
		{
			if (hct[ni].fs == 2 || (hct[ni].ll == 0 && hct[ni].fs == 1)) {
				//hct[ni].fs &= ~2;
				hct[ni].fs = 1;
				hc_stats[hct[ni].ll].nc++;
			}
			else {
				//hc_stats[hct[ni].ll].nc--;
				hct[ni].fs = 0;
			}
		}
	}
}

/*
ll			level
*/
void HC_clear_usage_stats(int ll, HCI n) {
	if (hct[n].fs & 4)
		return;
	hct[n].fs |= 4;
//	hct[n].uc = 0;
	if (ll > 0) {
		HC_clear_usage_stats(ll - 1, hct[n].ln);
		HC_clear_usage_stats(ll - 1, hct[n].rn);
	}
}

/*
ll			level

Call HC_clear_usage_stats first, to clear uc!
Counts usage of active nodes for display (result-nodes are not counted).
*/
void HC_nodes_count_usage(int ll, HCI n) {
	hc_stats[ll].tc++;
	//hct[n].uc++;
	//if (hct[n].uc == 1) {
	//	hc_stats[ll].uc++;
	//	if (ll > 0) {
	//		HC_nodes_count_usage(ll - 1, hct[n].ln);
	//		HC_nodes_count_usage(ll - 1, hct[n].rn);
	//	}
	//}
}

/*
ll			level

Call HC_nodes_count_usage first, to update uc/usage-count!
Updates usage stats of active nodes for display (result-nodes are not counted).
*/
void HC_update_usage_stats(int ll, HCI n) {
	if (!(hct[n].fs & 4))
		return;
	hct[n].fs &= ~4;

	if (hct[n].uc < hc_stats[ll].ucmn || !hc_stats[ll].ucmn)
		hc_stats[ll].ucmn = hct[n].uc;
	if (hct[n].uc > hc_stats[ll].ucmx)
		hc_stats[ll].ucmx = hct[n].uc;
	if (ll > 0) {
		HC_update_usage_stats(ll - 1, hct[n].ln);
		HC_update_usage_stats(ll - 1, hct[n].rn);
	}
}

int HC_remove_node(HCI n) {
	if (!hct[n].fs || !hct[n].ll)
		return 0;
	if (hct[n].uc)
		hct[n].uc--;
	int m = 0;
if (0 && !hct[n].uc && hct[n].ll) {
		m++;
		hct[n].fs = 0;
		m += HC_remove_node(hct[n].ln);
		m += HC_remove_node(hct[n].rn);
		m += HC_remove_node(hct[n].r);

		hc_stats[hct[n].ll].nc--;
		hc_stats[hct[n].ll].rqct++;
	}
	return m;
}

/*
ll		level
dh		(calling-)depth
ln		left node
rn		right node
rt		unless null, will be set to the result node of the found or added branch
returns index of found or added branch
*/
HCI HC_find_or_add_branch(int ll, HCI ln, HCI rn, HCI* rt) {
	// Calculate 32 checksum.
	UINT32 cm32;
	cm32 = fnv_32_buf(&ll, 1, FNV1_32_INIT);
	cm32 = fnv_32_buf(&ln, HCITPBYC, cm32);
	cm32 = fnv_32_buf(&rn, HCITPBYC, cm32);
	// Xor-fold 32bit checksum to fit size of hash-table (needed mask is applied in while-loop).
	HCI cm;
	cm = ((cm32 >> HCISZPT) ^ cm32);
	// Update stats.
	hc_stats[ll].sc++;
	// Search for checksum.
	int sc = 0;		// seek count
	HCI otcm = 0; // oldest-cm
	UINT8 og = 0; // oldest-generation
	while (1) {
		++sc;
		cm &= HCIMASK;
		// Checksum-index found > check if branches are the same.
		if (hct[cm].fs != 0) {
			if (hct[cm].ln == ln && hct[cm].rn == rn && hct[cm].ll == ll) {
				// stats
				hc_stats[ll].fc++;
				hct[cm].uc++;
				break;
			}
			//
			else {
				if (1 && ll && hct[cm].uc == 0 && hct[cm].ll > 0 /* && hct[cm].ll < hc_sl*/) {
					//UINT8 ag  = hc_cg;;// -hct[cm].ls;
					//if (ag > og) 
					{
						//og = ag;
						otcm = cm;
					}
					//if (og > 0 && sc > 0) 
					if (sc > 0) 
					{
						if (0)
							HC_remove_node(otcm);
						//if (!(rand() & 0xF)) 
						else {
							hct[otcm].fs = 0;
							hct[hct[otcm].ln].uc--;
							hct[hct[otcm].rn].uc--;
							hct[hct[otcm].r].uc--;
							//printf("removing  %X  ll %d  ls %d  cg %d \n", otcm, hct[otcm].ll, hct[otcm].ls, hc_cg);
							cm = otcm;
							hc_stats[hct[otcm].ll].nc--;
							hc_stats[hct[otcm].ll].rqct++;
							continue;
						}
					}
				}
			}
		}
		// Hashtable at checksum-index is empty > add new entry
		else {
			//hcisk[hciskps] = cm;
			//hciskps++;
			// stats
			if (!ll)
				printf("!!ll  %X  %X  %X  sc %d\n", cm, ln, rn, sc);
			hc_stats[ll].nc++;
			// Add new branch / entry to hashtable
			hct[cm].fs = 1;
			hct[cm].ln = ln;
			hct[cm].rn = rn;
			hct[cm].ll = ll;

//			hct[cm].ls = hc_cg;
			hct[cm].uc = 1;
			//
			if (ll) {
				hct[ln].uc++;
				hct[rn].uc++;
				// First pass.
				HCI fm, fmr, sl, slr, sr, srr, tb;
				fm = HC_find_or_add_branch(ll - 1, hct[ln].rn, hct[rn].ln, &fmr); // middle-result
				// Second pass.
				sl = HC_find_or_add_branch(ll - 1, hct[ln].r, fmr, &slr);
				sr = HC_find_or_add_branch(ll - 1, fmr, hct[rn].r, &srr);
				// Third pass.
				tb = HC_find_or_add_branch(ll - 1, slr, srr, NULL);
				hct[cm].r = tb;
HC_remove_node(fm);
HC_remove_node(sl);
HC_remove_node(sr);
//hct[fm].uc--;
//hct[sl].uc--;
//hct[sr].uc--;
			}
			//
			//hciskps--;
			//
			break;
		}
		++cm;
		if (sc >= HCISZ) {
			printf("ERROR! hash table at level %d is full with %d cm %08X\n", ll, sc, cm);
			getch();
			//int mc = HC_mark_active_nodes(hct[hc_sn].ll, hc_sn);
			//printf("marked  %5d  active nodes from  %6X\n", mc, hc_sn);
			//for (int i = 0; i < hciskps; i++) {
			//	int mc = HC_mark_active_nodes(hct[hcisk[i]].ll, hcisk[i]);
			//	printf("marked  %5d  active nodes from  %6X\n", mc, hcisk[i]);
			//}
			//HC_sweep_inactive_nodes(0);
			//cm = HC_find_or_add_branch(ll, ln, rn, rt);
			break;
		}
	}
	//
	hc_stats[ll].ss += sc;
	//
	if (rt)
		*rt = hct[cm].r;
	//if (hct[cm].uc == 0)
	return cm;
}

int64_t CA_CNFN_HASH(int64_t pgnc, caBitArray* vba) {
	while (pgnc >= (int64_t)(HCTBS / 2) << hc_sl) {
		// Check hash table fill-rate and reset if to high.
		//UINT32 smnc = 0;		// sum node-count
		//for (int i = 0; i < HCTMXLV - 2; ++i) {
		//	smnc += hc_stats[i].nc;
		//}
		//if (0 && smnc > HCISZ / 4 * 3) {
		//	printf("hash table size of with %d to high! reseting\n",
		//		smnc);
		//	HC_mark_active_nodes(hct[hc_sn].ll, hc_sn);
		//	//HC_mark_active_nodes(hc_sl, hc_sn);
		//	HC_sweep_inactive_nodes(0);
		//	HC_print_stats(0);
		//}
		for (int i = 0; i < HCTMXLV; ++i) {
			hc_stats[i].rqct = 0;
			hc_stats[i].fc = 0;
			hc_stats[i].sc = 0;
			hc_stats[i].ss = 0;
			hc_stats[i].tc = 0;
			hc_stats[i].uc = 0;
			hc_stats[i].ucmn = 0;
			hc_stats[i].ucmx = 0;
		}
		// Scale up space/size to allow calculate needed time
		hc_cg++;

		int ul = 0;
		while (pgnc >= ((int64_t)(HCTBS / 2) << hc_sl) << ul && ul < 63) {
			ul++;
		}
		for (int l = 0; l < ul; ++l) {
HCI hn;
			hc_sl++;
			hn = HC_find_or_add_branch(hct[hc_sn].ll + 1, hc_sn, hc_sn, NULL);
			//hn = HC_find_or_add_branch(hc_sl, hc_sn, hc_sn, NULL);
//hct[hc_sn].uc--;
HC_remove_node(hc_sn);
			hc_sn = hn;
		}
//hct[hc_sn].uc--;
HC_remove_node(hc_sn);
	hc_sn = hct[hc_sn].r;
hct[hc_sn].uc++;
		hc_sl--;
		for (int l = 1; l < ul; ++l) {
			hc_sl--;
//hct[hc_sn].uc--;
HC_remove_node(hc_sn);
	hc_sn = hct[hc_sn].ln;					// dont use result node here as it is later in time!
hct[hc_sn].uc++;
		}
		break;
	}

	//
	convert_hash_array_to_CA_CT(hct[hc_sn].ll, hc_sn, vba->clsc, vba->clsc + vba->scsz);

	return 0;
}

/*
ll		level
mxll*	max-level

returns last added (and therefor top-most) node
*/
HCI HC_add_node(int ll, HCI n, int* mxll) {
#define D 0				// debug-mode

	char indent[HCTMXLV * 2 + 1];
	memset(indent, ' ', HCTMXLV * 2);
	indent[ll * 2] = 0;

	if (D) printf("%sN l %d  n 0x%04X\n", indent, ll, n);

	// Add Node to branch.
	if (hcls[ll] == 0) {
		// Branch is empty > add first node and wait for the seconde one
		hcln[ll] = n;
		hcls[ll] = 1;
		return n;
	}
	else {
		*mxll = ll;
		HCI ln = hcln[ll];							// left-node
		HCI rn = n;									// right-node
		HCI nn;										// new-node
		hcls[ll] = 0;
		// Branch of two nodes is complete.
		nn = HC_find_or_add_branch(ll, ln, rn, NULL);
		// Prepare node one level up
		return HC_add_node(ll + 1, nn, mxll);
	}
}

void CA_CNITFN_HASH(caBitArray* vba, CA_RULE* cr) {
	// Copied from CA_CNITFN_LUT(vba, cr);
	_aligned_free(vba->v);
	vba->v = _aligned_malloc(vba->ct / 8, BYAL);
	vba->ct = (vba->ct + BPB - 1) / BPB * BPB + BPB;		// bit-array-count (rounded up / ceil) // allow one extra-element since when we process the last half block we read one half block past the end

	_aligned_free(hct);
	hct = _aligned_malloc(HCISZ * sizeof(HCN), 256);

	// reset
	memset(hct, 0, HCISZ * sizeof(HCN));
	memset(&hcln, 0, sizeof(hcln));
	memset(&hcls, 0, sizeof(hcls));
	memset(&hc_stats, 0, sizeof(hc_stats));

	// Precalculate all states of a space of 16 cells after 4 time steps.
	// Ruleset: 2 states, 3 cells in a neighborhod
	// After 4 time steps 8 cells (1 byte in binary representation) will remain

	if (HCTBS == 16) {
		UINT16 i = 0;
		while (1) {
			UINT16 cs = i;	// cell-space
			// get result byte
			for (int tm = 0; tm < HCTBS / 4; ++tm) {
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
			HCI nn;
			nn = HC_find_or_add_branch(0, i & 0xFF, i >> 8, NULL);
			hct[nn].r = cs & ((1 << (HCTBS / 2)) - 1);
			//
			if (i == MAXUINT16)
				break;
			++i;
		}
	}
	else if (HCTBS == 8) {
		UINT8 i = 0;
		while (1) {
			UINT8 cs = i;	// cell-space
			// get result byte
			for (int tm = 0; tm < 2; ++tm) {
				for (int p = 0; p < 6; ++p) {
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
			}
			// store result byte
			HCI nn;
			nn = HC_find_or_add_branch(0, i & 0xF, i >> 4, NULL);
			hct[nn].r = cs & 0xF;
			//
			if (i == MAXUINT8)
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

//
	convert_CA_CT_to_bit_array(vba->clsc, (FBPT*)vba->v, vba->scsz);
	hc_sn = convert_bit_array_to_hash_array(vba, &hc_sl);

	HC_print_stats(0);

	//print_bits(vba->v, vba->scsz / 8, 4);
	//HC_print_node(hc_sl, 20, hc_sn);
	//CA_PrintSpace("", vba->clsc, vba->scsz);
	//convert_hash_array_to_CA_CT(hc_sl, hc_sn, vba->clsc, vba->clsc + vba->scsz);
	//print_bits(vba->v, vba->scsz / 8, 4);
	//CA_PrintSpace("", vba->clsc, vba->scsz);
	//HC_print_stats(0);
	//printf("press any key\n");  getch();
}

int64_t CA_CNFN_LUT(int64_t pgnc, caBitArray* vba) {
	convert_CA_CT_to_bit_array(vba->clsc, (FBPT*)vba->v, vba->scsz);

	if (pgnc > 0) {
		while (pgnc > 3) {
			// copy wrap arround buffer
			uint8_t* wb = vba->v;
			wb[vba->scsz / 8] = wb[0];
			//
			for (int bi = 0, sz = vba->scsz / 8; bi < sz; ++bi) {
				UINT8* bya = vba->v;  // need to adress on byte-basis
				bya += bi;
				*bya = calt[((UINT32)3<<16) | *(UINT16*)bya];
			}
			pgnc -= 4;
		}
		if (pgnc) {
			// copy wrap arround buffer
			uint8_t* wb = vba->v;
			wb[vba->scsz / 8] = wb[0];
			//
			for (int bi = 0, sz = vba->scsz / 8; bi < sz; ++bi) {
				UINT8* bya = vba->v;  // need to adress on byte-basis
				bya += bi;
				*bya = calt[((UINT32)(pgnc - 1)<<16) | *(UINT16*)bya];
			}
			pgnc = 0;
		}
	}

	convert_bit_array_to_CA_CT((FBPT*)vba->v, vba->clsc, vba->scsz);

	return 0;
}

void CA_CNITFN_BOOL(caBitArray* vba, CA_RULE* cr) {
	_aligned_free(vba->v);
	vba->ct = (vba->ct + BPB - 1) / BPB * BPB + BPB;		// bit-array-count - round up count and allow one extra-element since when we process the last half block we read one half block past the end
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
				//l = ~((l >> 1) ^ (l & (l >> 2)));
				// equivalent rule 54 - (p, q, r) -> q xor (p or r) - https://www.wolframalpha.com/input/?i=rule+54
				l = (l >> 1) ^ (l | (l >> 2));
			}
			*(HBPT*)bya = (HBPT)l;
		}
		pgnc -= min(4, pgnc);
	}

	convert_bit_array_to_CA_CT((FBPT*)vba->v, vba->clsc, vba->scsz);

	return 0;
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

void CA_CNITFN_DISABLED(caBitArray* vba, CA_RULE* cr) {
}

int64_t CA_CNFN_DISABLED(int64_t pgnc, caBitArray* vba) {
	printf("CM_DISABLED  computation disabled!\n");
	return 0;
}

static OCLCA oclca = { -1, NULL, NULL, NULL };
void CA_CNITFN_OPENCL(caBitArray* vba, CA_RULE* cr) {
	if (oclca.success != CL_SUCCESS)
		oclca = OCLCA_Init("../opencl-vba.cl");

	//
	vba->lc = 256;
	vba->lw = 4 * 16;
	vba->mmwn = 4;
	initCABitArray(vba, cr);
}

void CA_CNITFN_OMP_VBA_8x32(caBitArray* vba, CA_RULE* cr) {
	vba->lc = 8;
	vba->lw = 32;
	vba->mmwn = 4;
	initCABitArray(vba, cr);
}

void CA_CNITFN_OMP_VBA_8x1x256(caBitArray* vba, CA_RULE* cr) {
	vba->lc = 2;
	vba->lw = 512;
	vba->mmwn = 4;
	vba->oc = 4;
	initCABitArray(vba, cr);
}

int64_t CA_CNFN_OMP_VBA_8x1x256(int64_t pgnc, caBitArray* vba) {
	convertBetweenCACTandCABitArray(vba->clsc, vba->clsc + vba->scsz, vba, 0);

#pragma omp parallel num_threads(2)
	{
		int ln = omp_get_thread_num();
		int gnc = pgnc;
		while (gnc-- > 0) {
			//
#pragma omp barrier
			{
				//				CABitArrayPrepareOR(vba, ln * 512, (ln + 1) * 512);
				int bbrw = vba->rw / VBBB;			// bit-blocks per row
				int from = ln * 512 / VBBB;
				int to = (ln + 1) * 512 / VBBB;
				for (int c = from; c < to; ++c) {
					vba->v[(0 + vba->rc) * bbrw + c] = (vba->v[0 * bbrw + c] >> 1) | (vba->v[0 * bbrw + ((c + 1) % bbrw)] << VBPBMM);
					vba->v[(1 + vba->rc) * bbrw + c] = (vba->v[1 * bbrw + c] >> 1) | (vba->v[1 * bbrw + ((c + 1) % bbrw)] << VBPBMM);
				}
				//				printf("SYNC  gnc %d  ln %d\n", gnc, ln);
			}
#pragma omp barrier
			//
			__m256i* vbac;
			vbac = (__m256i*)vba->v + ln * 2;
			for (int ri = 0; ri < vba->rc; ++ri) {
				//				printf("gnc %d  ln %d  ri %d  vbac %p  vbai %p\n", gnc, ln, ri, vbac, (char*)vba->v + vba->sz);
								// first lane
				__m256i ymm0 = _mm256_load_si256(vbac + 0);
				__m256i ymm1 = _mm256_load_si256(vbac + 4);
				__m256i ymm2 = _mm256_load_si256(vbac + 8);
				ymm0 = _mm256_or_si256(ymm0, ymm2);
				ymm0 = _mm256_xor_si256(ymm0, ymm1);
				_mm256_store_si256(vbac + 0, ymm0);
				// second lane
				__m256i ymm3 = _mm256_load_si256(vbac + 1);
				__m256i ymm4 = _mm256_load_si256(vbac + 5);
				__m256i ymm5 = _mm256_load_si256(vbac + 9);
				ymm3 = _mm256_or_si256(ymm3, ymm5);
				ymm3 = _mm256_xor_si256(ymm3, ymm4);
				_mm256_store_si256(vbac + 1, ymm3);
				//
				vbac += 4;
			}
		}
	}

	convertBetweenCACTandCABitArray(vba->clsc, vba->clsc + vba->scsz, vba, 1);
	return 0;
}

void CA_CNITFN_OMP_TEST(caBitArray* vba, CA_RULE* cr) {
	vba->lc = 4;
	vba->lw = 2 * 256;
	vba->mmwn = 4;
	initCABitArray(vba, cr);
}

int64_t CA_CNFN_OMP_TEST(int64_t pgnc, caBitArray* vba) {
	convertBetweenCACTandCABitArray(vba->clsc, vba->clsc + vba->scsz, vba, 0);

	int ln;

#pragma omp parallel for
	for (ln = 0; ln < vba->lc; ln++)
	{
		int gnc = pgnc;
		__m256i* vbacb;
		vbacb = (__m256i*)vba->v + ln * vba->lc * vba->lw / 256 * vba->rc / vba->lc;
		while (gnc > 0) {
			gnc--;
			__m256i* vbac = vbacb;
			for (int ri = 0; ri < vba->rc; ++ri) {
				__m256i ymm0 = _mm256_load_si256(vbac + 0);
				__m256i ymm1 = _mm256_load_si256(vbac + 1);
				///				ymm0 = _mm256_add_epi32(ymm0, _mm256_set1_epi32(1));
				///				ymm1 = _mm256_add_epi32(ymm1, _mm256_set1_epi32(1));
				ymm0 = _mm256_or_si256(ymm0, ymm1);
				ymm0 = _mm256_xor_si256(ymm0, ymm1);
				ymm1 = _mm256_or_si256(ymm1, ymm0);
				ymm1 = _mm256_xor_si256(ymm1, ymm0);
				_mm256_store_si256(vbac + 0, ymm0);
				_mm256_store_si256(vbac + 1, ymm1);
				//
				vbac += 2;
			}
		}
		//int gnc = pgnc;
		//while (gnc > 0) {
		//	gnc--;
		//	__m256i* vbac;
		//	vbac = (__m256i*)vba->v + ln * 2;
		//	for (int ri = 0; ri < vba->rc; ++ri) {
		//		__m256i ymm0 = _mm256_load_si256(vbac + 0);
		//		__m256i ymm1 = _mm256_load_si256(vbac + 1);
		//		ymm0 = _mm256_add_epi32(ymm0, _mm256_set1_epi32(1));
		//		ymm1 = _mm256_add_epi32(ymm1, _mm256_set1_epi32(1));
		//		_mm256_store_si256(vbac + 0, ymm0);
		//		_mm256_store_si256(vbac + 1, ymm1);
		//		//
		//		vbac += 8;
		//	}
		//}
	}

	convertBetweenCACTandCABitArray(vba->clsc, vba->clsc + vba->scsz, vba, 1);
	return 0;
}

void CA_CNITFN_VBA_16x256(caBitArray* vba, CA_RULE* cr) {
	vba->lc = 1;
	vba->lw = 256;
	vba->mmwn = 8;
	vba->oc = 14;
	initCABitArray(vba, cr);
}

int64_t CA_CNFN_VBA_16x256(int64_t pgnc, caBitArray* vba) {
	// WORK IN PROGRESS
	convertBetweenCACTandCABitArray(vba->clsc, vba->clsc + vba->scsz, vba, 0);
	while (pgnc > 0) {
		pgnc -= 4;
		CABitArrayPrepareOR(vba, 0, 0);

		register __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;
		register __m256i ymm8, ymm9;
		register __m256i ymm10, ymm11, ymm12, ymm13, ymm14, ymm15;

		for (int ri = 0; ri < vba->rc; ri += 8) {
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
	convertBetweenCACTandCABitArray(vba->clsc, vba->clsc + vba->scsz, vba, 1);
	return 0;
}


void CA_CNITFN_VBA_2x256(caBitArray* vba, CA_RULE* cr) {
	vba->lc = 2;
	vba->lw = 256;
	vba->mmwn = 8;
	vba->oc = 2;
	initCABitArray(vba, cr);
}

void CA_CNITFN_VBA_1x32(caBitArray* vba, CA_RULE* cr) {
	vba->lc = 1;
	vba->lw = 32;
	vba->mmwn = 64;
	initCABitArray(vba, cr);
}

void CA_CNITFN_VBA_1x64(caBitArray* vba, CA_RULE* cr) {
	vba->lc = 1;
	vba->lw = 64;
	vba->mmwn = 32;
	initCABitArray(vba, cr);
}

void CA_CNITFN_VBA_2x32(caBitArray* vba, CA_RULE* cr) {
	vba->lc = 2;
	vba->lw = 32;
	vba->mmwn = 32;
	initCABitArray(vba, cr);
}

void CA_CNITFN_VBA_4x32(caBitArray* vba, CA_RULE* cr) {
	vba->lc = 4;
	vba->lw = 32;
	vba->mmwn = 16;
	initCABitArray(vba, cr);
}

void CA_CNITFN_VBA_1x256(caBitArray* vba, CA_RULE* cr) {
	vba->lc = 1;
	vba->lw = 256;
	vba->mmwn = 32;
	initCABitArray(vba, cr);
}

int64_t CA_CNFN_VBA_1x256(int64_t gnc, caBitArray* vba) {
	convertBetweenCACTandCABitArray(vba->clsc, vba->clsc + vba->scsz, vba, 0);
	while (gnc > 0) {
		gnc--;
		CABitArrayPrepareOR(vba, 0, 0);
		__m256i* vbac = (__m256i*)vba->v;
		int rc = vba->rc;
		register __m256i ymm0;
		for (int ri = 0; ri < rc; ++ri) {
			ymm0 = _mm256_or_si256(_mm256_load_si256(vbac + ri), _mm256_load_si256(vbac + ri + 2));
			ymm0 = _mm256_xor_si256(ymm0, _mm256_load_si256(vbac + ri + 1));
			_mm256_store_si256(vbac + ri, ymm0);
			// works, but seems to be slower
			//*((__m256i*)vba->v + ri) = _mm256_or_si256(*((__m256i*)vba->v + ri), *((__m256i*)vba->v + ri + 2));
			//*((__m256i*)vba->v + ri) = _mm256_xor_si256(*((__m256i*)vba->v + ri), *((__m256i*)vba->v + ri + 1));
		}
	}
	convertBetweenCACTandCABitArray(vba->clsc, vba->clsc + vba->scsz, vba, 1);
	return gnc;
}

int64_t CA_CNFN_VBA_1x32(int64_t pgnc, caBitArray* vba) {
	convertBetweenCACTandCABitArray(vba->clsc, vba->clsc + vba->scsz, vba, 0);
	while (pgnc > 0) {
		CABitArrayPrepareOR(vba, 0, 0);

		UINT32* vbac = (UINT32*)vba->v;
		for (int ri = 0; ri < vba->rc; ++ri) {
			vbac[ri] = vbac[ri + 1] ^ (vbac[ri] | vbac[ri + 2]);			// equivalent rule 54 - (p, q, r) -> q xor (p or r) - https://www.wolframalpha.com/input/?i=rule+54
		}

		pgnc--;
	}
	convertBetweenCACTandCABitArray(vba->clsc, vba->clsc + vba->scsz, vba, 1);
	return 0;
}

int64_t CA_CNFN_VBA_2x32(int64_t pgnc, caBitArray* vba) {
	convertBetweenCACTandCABitArray(vba->clsc, vba->clsc + vba->scsz, vba, 0);
	while (pgnc > 0) {
		CABitArrayPrepareOR(vba, 0, 0);

		UINT32* vbac = (UINT32*)vba->v;
		for (int ri = 0; ri < vba->rc * 2; ri += 2) {
			vbac[ri + 0] = vbac[ri + 2] ^ (vbac[ri + 0] | vbac[ri + 4]);			// equivalent rule 54 - (p, q, r) -> q xor (p or r) - https://www.wolframalpha.com/input/?i=rule+54
			vbac[ri + 1] = vbac[ri + 3] ^ (vbac[ri + 1] | vbac[ri + 5]);			// equivalent rule 54 - (p, q, r) -> q xor (p or r) - https://www.wolframalpha.com/input/?i=rule+54
		}

		pgnc--;
	}
	convertBetweenCACTandCABitArray(vba->clsc, vba->clsc + vba->scsz, vba, 1);
	return 0;
}

int64_t CA_CNFN_VBA_4x32(int64_t pgnc, caBitArray* vba) {
	convertBetweenCACTandCABitArray(vba->clsc, vba->clsc + vba->scsz, vba, 0);
	while (pgnc > 0) {
		CABitArrayPrepareOR(vba, 0, 0);

		UINT32* vbac = (UINT32*)vba->v;
		for (int ri = 0; ri < vba->rc * 4; ri += 4) {
			vbac[ri + 0] = vbac[ri + 4] ^ (vbac[ri + 0] | vbac[ri + 8]);			// equivalent rule 54 - (p, q, r) -> q xor (p or r) - https://www.wolframalpha.com/input/?i=rule+54
			vbac[ri + 1] = vbac[ri + 5] ^ (vbac[ri + 1] | vbac[ri + 9]);			// equivalent rule 54 - (p, q, r) -> q xor (p or r) - https://www.wolframalpha.com/input/?i=rule+54
			vbac[ri + 2] = vbac[ri + 6] ^ (vbac[ri + 2] | vbac[ri + 10]);			// equivalent rule 54 - (p, q, r) -> q xor (p or r) - https://www.wolframalpha.com/input/?i=rule+54
			vbac[ri + 3] = vbac[ri + 7] ^ (vbac[ri + 3] | vbac[ri + 11]);			// equivalent rule 54 - (p, q, r) -> q xor (p or r) - https://www.wolframalpha.com/input/?i=rule+54
		}

		pgnc--;
	}
	convertBetweenCACTandCABitArray(vba->clsc, vba->clsc + vba->scsz, vba, 1);
	return 0;
}

int64_t CA_CNFN_OMP_VBA_8x32(int64_t pgnc, caBitArray* vba) {
	convertBetweenCACTandCABitArray(vba->clsc, vba->clsc + vba->scsz, vba, 0);
	while (pgnc > 0) {
		CABitArrayPrepareOR(vba, 0, 0);

		int ln;// , ri;
		int c = 0;
#pragma omp parallel for //private (vbac, ln, ri)
		for (ln = 0; ln < 8; ln++) {
			UINT32* vbac;
			vbac = (UINT32*)vba->v + ln;
			for (int ri = 0; ri < vba->rc * 8; ri += 8) {
				++c;
				vbac[ri] = vbac[ri + 8] ^ (vbac[ri] | vbac[ri + 16]);			// equivalent rule 54 - (p, q, r) -> q xor (p or r) - https://www.wolframalpha.com/input/?i=rule+54
			}
		}
		//printf("c %d\n", c);

		pgnc--;
	}
	convertBetweenCACTandCABitArray(vba->clsc, vba->clsc + vba->scsz, vba, 1);
	return 0;
}

int64_t CA_CNFN_VBA_1x64(int64_t pgnc, caBitArray* vba) {
	convertBetweenCACTandCABitArray(vba->clsc, vba->clsc + vba->scsz, vba, 0);
	while (pgnc > 0) {
		CABitArrayPrepareOR(vba, 0, 0);

		UINT64* vbac = (UINT64*)vba->v;
		for (int ri = 0; ri < vba->rc; ++ri) {
			vbac[ri] = vbac[ri + 1] ^ (vbac[ri] | vbac[ri + 2]);			// equivalent rule 54 - (p, q, r) -> q xor (p or r) - https://www.wolframalpha.com/input/?i=rule+54
		}

		pgnc--;
	}
	convertBetweenCACTandCABitArray(vba->clsc, vba->clsc + vba->scsz, vba, 1);
	return 0;
}

int64_t CA_CNFN_VBA_2x256(int64_t pgnc, caBitArray* vba) {
	convertBetweenCACTandCABitArray(vba->clsc, vba->clsc + vba->scsz, vba, 0);
	while (pgnc > 0) {
		CABitArrayPrepareOR(vba, 0, 0);

		__m256i* vbac = (__m256i*)vba->v;
		for (int ri = 0; ri < vba->rc * 2; ri += 2) {
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
	convertBetweenCACTandCABitArray(vba->clsc, vba->clsc + vba->scsz, vba, 1);
	return 0;
}

int64_t CA_CNFN_OPENCL(int64_t pgnc, caBitArray* vba) {
	OCLCA_RunVBA(pgnc, 0/*res*/, oclca, vba->cr, *vba, 1/*de*/, 16/*klrg*/);
	pgnc = 0;
	convertBetweenCACTandCABitArray(vba->clsc, vba->clsc + vba->scsz, vba, 1);
	return 0;
}

//typedef struct CA_CNSG {
//	char* name;
//	int (*cnfn)(int, caBitArray*);
//	void (*itfn)(caBitArray*);
//};

struct {
	char* name;
	int64_t(*cnfn)(int64_t, caBitArray*);
	void		(*itfn)(caBitArray*);
} const ca_cnsgs[CM_MAX] = {
	[CM_VBA_1x256] = {
		.name = "CM_VBA_1x256",
		.cnfn = CA_CNFN_VBA_1x256,
		.itfn = CA_CNITFN_VBA_1x256
	},
	[CM_DISABLED] = {
		.name = "CM_DISABLED",
		.cnfn = CA_CNFN_DISABLED,
		.itfn = CA_CNITFN_DISABLED
	},
	[CM_OMP_TEST] = {
		.name = "CM_OMP_TEST",
		.cnfn = CA_CNFN_OMP_TEST,
		.itfn = CA_CNITFN_OMP_TEST
	},
	[CM_OPENCL] = {
		.name = "CM_OPENCL",
		.cnfn = CA_CNFN_OPENCL,
		.itfn = CA_CNITFN_OPENCL
	},
	[CM_OMP_VBA_8x32] = {
		.name = "CM_OMP_VBA_8x32",
		.cnfn = CA_CNFN_OMP_VBA_8x32,
		.itfn = CA_CNITFN_OMP_VBA_8x32
	},
	[CM_OMP_VBA_8x1x256] = {
		.name = "CM_OMP_VBA_8x1x256",
		.cnfn = CA_CNFN_OMP_VBA_8x1x256,
		.itfn = CA_CNITFN_OMP_VBA_8x1x256
	},
	[CM_VBA_16x256] = {
		.name = "CM_VBA_16x256",
		.cnfn = CA_CNFN_VBA_16x256,
		.itfn = CA_CNITFN_VBA_16x256
	},
	[CM_VBA_2x256] = {
		.name = "CM_VBA_2x256",
		.cnfn = CA_CNFN_VBA_2x256,
		.itfn = CA_CNITFN_VBA_2x256
	},
	[CM_VBA_1x32] = {
		.name = "CM_VBA_1x32",
		.cnfn = CA_CNFN_VBA_1x32,
		.itfn = CA_CNITFN_VBA_1x32
	},
	[CM_VBA_1x64] = {
		.name = "CM_VBA_1x64",
		.cnfn = CA_CNFN_VBA_1x64,
		.itfn = CA_CNITFN_VBA_1x64
	},
	[CM_VBA_2x32] = {
		.name = "CM_VBA_2x32",
		.cnfn = CA_CNFN_VBA_2x32,
		.itfn = CA_CNITFN_VBA_2x32
	},
	[CM_VBA_4x32] = {
		.name = "CM_VBA_4x32",
		.cnfn = CA_CNFN_VBA_4x32,
		.itfn = CA_CNITFN_VBA_4x32
	},
	[CM_SISD] = {
		.name = "CM_SISD",
		.cnfn = CA_CNFN_SISD,
		.itfn = NULL
	},
	[CM_SIMD] = {
		.name = "CM_SIMD",
		.cnfn = CA_CNFN_SIMD,
		.itfn = NULL
	},
	[CM_BOOL] = {
		.name = "CM_BOOL",
		.cnfn = CA_CNFN_BOOL,
		.itfn = CA_CNITFN_BOOL
	},
	[CM_LUT] = {
		.name = "CM_LUT",
		.cnfn = CA_CNFN_LUT,
		.itfn = CA_CNITFN_LUT
	},
	[CM_HASH] = {
		.name = "CM_HASH",
		.cnfn = CA_CNFN_HASH,
		.itfn = CA_CNITFN_HASH
	},
};

////typedef int CA_CNFN(int, caBitArray*);					// celluar-automaton-computation-function type
////CA_CNFN* const ca_cnfns[CM_MAX] = {							// celluar-automaton-computation-functions
////};
////
////typedef void CA_CNITFN(caBitArray*);					// celluar-automaton-computation-initialization-function type
////CA_CNFN* const ca_cnitfns[CM_MAX] = {							// celluar-automaton-computation-initialization-functions
////
////};
////
////const char* const cm_names[CM_MAX] = {
////};


#define CS_SYS_BLOCK_SZ	0x04			// system-block-size - used by bit-shifting operations
#define CS_BLOCK_SZ		0xFF			// size of one row / nr. of columns
#define CS_PRF_WIN_SZ	0xFF			// prefered window-size - used by row>col / col>row conversion functions

/* reverse: reverse string s in place */
void reverse(char s[])
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
		int nst = i; // neighborhod-state
		char snst[100] = "";	// string neighborhod-state
		int rds = 0; // reduced neighborhod-state
		// go through all neighbor-cells
		for (int k = 0; k < cr->ncn; ++k) {
			// extract state of rightmost neighbor
			int rns = nst % cr->ncs; // rightmost-neighbor-state
			nst /= cr->ncs;
			// reduce neighborhod-state
			if (cr->tp == TOT) // totalistic
				rds += rns;
			else if (cr->tp == ABS) // absolute
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

//rotate/flip a quadrant appropriately
__inline
void ht_rot(int n, int* x, int* y, int rx, int ry) {
	if (ry == 0) {
		if (rx == 1) {
			*x = n - 1 - *x;
			*y = n - 1 - *y;
		}

		//Swap x and y
		int t = *x;
		*x = *y;
		*y = t;
	}
}

//convert (x,y) to d
__inline
int xy2d(int n, int x, int y) {
	int rx, ry, s, d = 0;
	for (s = n / 2; s > 0; s /= 2) {
		rx = (x & s) > 0;
		ry = (y & s) > 0;
		d += s * s * ((3 * rx) ^ ry);
		ht_rot(s, &x, &y, rx, ry);
	}
	return d;
}

//convert d to (x,y)
__inline
void d2xy(int n, int d, int* x, int* y) {
	int rx, ry, s, t = d;
	*x = *y = 0;
	for (s = 1; s < n; s *= 2) {
		rx = 1 & (t / 2);
		ry = 1 & (t ^ rx);
		ht_rot(s, x, y, rx, ry);
		*x += s * rx;
		*y += s * ry;
		t /= 4;
	}
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


/* Type for signed 32bit integer array */
typedef struct
S32IRS {
	int32_t	ct;		/* count of elements */
	int32_t* es;	/* elements */
} S32IRS;

/* Initializes S32IRS struct, ct must be valid count, es must be null or allocated mem */
void
init_S32IRS(S32IRS* p) {
	/* Free previously allocated memory */
	if (p->es)
		free(p->es);
	/* Allocate new memory */
	p->es = malloc(p->ct * sizeof(*p->es));
	/* Initialize memory */
	for (int i = 0; i < p->ct; ++i)
		p->es[i] = i;
}
/* Creates empty S32IRS struct, ct must be valid count
ct	count
*/
S32IRS
create_S32IRS(uint32_t ct) {
	S32IRS r = { ct, NULL };
	init_S32IRS(&r);
	return r;
}
/* Print S32IRS struct
mx		max items to print
*/
void
print_S32IRS(S32IRS* p, int32_t mx) {
	printf("s32irs\n");
	for (int i = 0; i < mx && i < p->ct; ++i)
		printf("%d ", p->es[i]);
	printf("\n");
}

void
print_M256I_I32(__m256i t) {
	printf(
		"m256i_i32   %d  %d  %d  %d  %d  %d  %d  %d\n",
		t.m256i_i32[0], t.m256i_i32[1], t.m256i_i32[2], t.m256i_i32[3],
		t.m256i_i32[4], t.m256i_i32[5], t.m256i_i32[6], t.m256i_i32[7]);
}

void
test_simd_intrinsic() {
	printf("test simd intrinsic\n");

	const int SZ = 256;
	UINT8* bts;
	bts = _aligned_malloc(SZ + 2, 64);
	for (int i = 0; i < SZ + 2; i++)
		bts[i] = i;

	register __m256i t;
	t = _mm256_adds_epu8(*(__m256i*)(bts + 0), *(__m256i*)(bts + 1));
	t = _mm256_adds_epu8(t, *(__m256i*)(bts + 2));

	// for disasm tests
	if (t.m256i_u8[27] == 99)
		return;
	if (t.m256i_u8[3] == 99)
		return;

	_mm256_store_si256(bts, t);

	for (int i = 0; i < 32; i++)
		printf("  %2d %d\n", i, bts[i]);

	// test theoretical throughput
	LONGLONG hptrc; // high performance timer counter
	int64_t spct = 0; // sample count
	//
	const int32_t irsct = 8 * 256; // count of integers
	int32_t* irs; // integers
	irs = malloc((irsct + 2) * sizeof(*irs));
	// Init
	int32_t* i = irs;
	int ii = 0;
	for (; ii < irsct; ++i, ++ii)
		*i = ii;
	printf(
		"init		0 %d  1 %d  2 %d\n",
		irs[3], irs[1], irs[2]);

	// integer
	hptrc = timeit_start();
	while (timeit_duration_nr(hptrc) < .1) {
		spct += irsct * 1000;
		for (int j = 0; j < 1000; j++)
		{
			int32_t* ip = irs, * ib = irs + irsct;
		int_loop:
			if (ip >= ib)
				goto int_loop_end;
			*ip = *ip + 3;
			++ip;
			goto int_loop;
		int_loop_end:;
		}
	}
	printf(
		"init		0 %d  1 %d  2 %d\n",
		irs[3], irs[1], irs[2]);
	printf(
		"integer		samples %.2e  speed %.2e	0 %d  1 %d  2 %d\n",
		1.0 * spct, (double)spct / timeit_duration_nr(hptrc),
		irs[3], irs[1], irs[2]);

	// intrinsic
	{
		printf("Intrinsic\n");
		// Init
		int spct = 0;
		S32IRS is = create_S32IRS(8 * 256);
		LONGLONG hptrc = timeit_start();
		print_S32IRS(&is, 8);
		register __m256i ymm0 = *(__m256i*)is.es;

		while (timeit_duration_nr(hptrc) < 0.1) {
			spct += is.ct;
			for (register int i = 0; i < is.ct; i += 8)
				//	*(__m256i*)(is.es + i) = _mm256_add_epi32(*(__m256i*)(is.es + i), *(__m256i*)(is.es));
				*(__m256i*)(is.es + i) = _mm256_add_epi32(*(__m256i*)(is.es + i), ymm0);
		}
		print_S32IRS(&is, 8);
		printf(
			"intrinsic	samples %.2e  speed %.2e\n",
			1.0 * spct, spct / timeit_duration_nr(hptrc));
	}
	// intrinsic register
	{
		printf("IntrinsicR\n");
		// Init
		int spct = 0;
		S32IRS is = create_S32IRS(8);
		print_S32IRS(&is, 8);
		//
		LONGLONG hptrc = timeit_start();
		register __m256i ymm0 = *(__m256i*)is.es;
		register __m256i ymm1 = *(__m256i*)is.es;

		print_M256I_I32(ymm0);

		while (timeit_duration_nr(hptrc) < 0.1) {
			spct += 1000000 * 8;
			for (register int i = 0; i < 1000000; ++i)
				ymm0 = _mm256_add_epi32(ymm0, ymm1);
		};

		_mm256_storeu_si256(is.es, ymm0);
		print_S32IRS(&is, 8);

		printf(
			"intrinsicR	samples %.2e  speed %.2e\n",
			1.0 * spct, spct / timeit_duration_nr(hptrc));
		//
		++ymm0.m256i_i32[3];
		if (ymm0.m256i_i32[3] == 11)
			printf("11");
		//
		printf("12");

	}
	//
}


void
test_parallel_add() {
	test_simd_intrinsic();
	printf("test parallel add\n");

	UINT8 bts[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

	*(UINT64*)(bts) = *((UINT64*)(bts)) + *((UINT64*)(bts + 1)) + *((UINT64*)(bts + 2));// = *(int*)bts[0] + *(int*)bts[1] + *(int*)bts[2];

	printf("%d %d %d %d %d %d %d %d\n", bts[0], bts[1], bts[2], bts[3], bts[4], bts[5], bts[6], bts[7]);
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
};		// Lindenmeyer names
int* LMSA[LMS_COUNT];											// Lindenmeyer arrays

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
	int* lmvs,								// Lindenmeyer needed screen size for given depth and plw - if not NULL no drawing is done, but size of Lindenmeyer system with given depth is calculated
	int* lmhs,								// Lindenmeyer needed screen size for given depth and plw - if not NULL no drawing is done, but size of Lindenmeyer system with given depth is calculated
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
			int vs = 0;						// screen size
			int hs = 0;						// screen size
			int pbsz = 0;					// pixel-buffer size
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
	lmdh = min(lmdh, LMMXDH);				// make sure depth it within limits
	int n = lmdh;							// current depth
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
	UINT32* pnv,							// pixel-screen first valid element
	UINT32* pni,							// pixel-screen first invalid element
	const int plw,							// pixel-screen line-width
	const int paw,							// pixel-screen available with
	const int vlpz,							// vertical-pixel-zoom
	const int hlpz,							// horizontal-pixel-zoom
	const UINT32* pbv,						// pixel-buffer first valid element
	const UINT32* pbi,						// pixel-buffer first invalid element
	const int md,							// mode - 0: morton, 1: line, 2: grid, 3: matze-fractal
	const int mdpm							// mode parameter - may be used differently by different modes - morton: > 0 display mdpm - 1 layer of 3d morton-cube
) {
	const UINT32 pbs = pbi - pbv;			// pixel-buffer-size

	UINT32* pbc = pbv;						// pixel-buffer cursor
	double level = log(pbi - pbv) / log(4.0);
	int ysz, xsz;
	ysz = xsz = ipow(2, level);
	if (md == 0 && mdpm) {
		int ds = pow(pbs, 1.0 / 3.0) + 5;			// dimension-size
		int hb = min(pow(pbs, 1.0 / 6.0) + 0.01, plw / ds - 1);		// horizontal-blocks
		//printf("%e  %d\n", pow(pbs, 1.0 / 6.0), hb);
		ysz = xsz = hb * ds;
		ysz = (ds - 5) / hb * ds;
	}

	if (fmod(level, 1.0) > 0.0)
		xsz *= 2;
	if (fmod(level, 1.0) > 0.5)
		ysz *= 2;
	int h = (pni - pnv) / plw,				// height in plw-sized lines
		sy = h / 2 - ysz * vlpz / 2,
		sx = paw / 2 - xsz * hlpz / 2,
		cr = sy * plw + sx;					// transpose needed to center
	int vlpzplw = vlpz * plw;											// pre-calc vlpz * plw
	int rpnc = cr;														// pixel-screen position (relative, no pointer)
	pni -= plw * (vlpz - 1) + (hlpz - 1);				// correct pixel-screen first invalid for pixel-zoom (so we dont have to check for every zoomed pixel)



	if (md == 0) {
		int cs = pbi - pbc;
		// morton code layout
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
			int hb = min(pow(cs, 1.0 / 6.0) + 0.001, plw / ds - 1);		// horizontal-blocks
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
	else if (md == 1) {
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
	else if (md == 2) {
		// linear memory layout
		int sz = ceil(sqrt(pbs));
		for (int v = 0; v < sz * vlpzplw; v += vlpzplw)
			for (int h = v; h < v + sz * hlpz; h += hlpz) {
				UINT32* pnc = pnv + rpnc + h;								// pixel-screen current element
				if (pnc >= pnv && pnc < pni)
					for (int vv = 0; vv < vlpzplw; vv += plw)
						for (int hh = 0; hh < hlpz; ++hh)
							pnc[vv + hh] = *pbc;
				++pbc;
				if (pbc >= pbi)
					return;
			}
		return;
	}
	else {
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
}

void
display_2d_chaotic(
	UINT32* pnv,							// pixel-screen first valid element
	UINT32* pni,							// pixel-screen first invalid element
	const int plw,							// pixel-screen line-width
	const int dplw,							// pixel-screen display-line-width (right-aligned, so starts at plw - dplw and is dplw wide
	static CA_CT* csv,						// cells first valid element
	static CA_CT* csi,						// cells first invalid element
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
	if (*(double*)a > * (double*)b)
		return +1;
	return 0;
}


void
liveanddrawnewcleanzoom(
	/*
	pixel-screen is a 4-byte rgba pixel array to draw to

	does scrolling itself!
	*/
	CA_RULE* cr,										// ca-rule configuration 
	CA_CT* sc,											// space 
	int scsz,											// space-size 
	int64_t tm,											// time - nr. cells (tm=width=line=1=full generation) to live 
	int res,											// reset-flag (because of the static vars)
	UINT32* pnv,										// pixel-screen first valid element
	const UINT32* pni,									// pixel-screen first invalid element
	int de,												// drawing enabled
	caComputationMode cnmd,								// computation-mode
	const unsigned int klrg,							// kernel-range (GPU mode)
	caDisplaySettings ds								// display settings
)
{
	/* Return if time is zero */
	if (!tm)
		return;

	/* Automatically reset if computation-mode has been changed */
	static caComputationMode ltcnmd = 0;				// last computation-mode
	if (cnmd != ltcnmd) {
		ltcnmd = cnmd;
		res = 1;
	}

	/* Check for maximum size of rule tables */
	if (cr->nns > LV_MAXRLTLSZ) {
		printf("ERROR max rule table size to small");
		return;
	}

	/* Init vars and defines */
	ds.lgm %= 2;
	if (!ds.vlfs || !ds.hlfs)
		ds.stzm = 0;
	int tma = 0;										// time available (as stored in pixel buffer)

	/* Make sure zoom-levels are at least 1 */
	ds.hlzm = max(1, ds.hlzm);
	ds.vlzm = max(1, ds.vlzm);
	ds.hlpz = max(1, ds.hlpz);
	ds.vlpz = max(1, ds.vlpz);

	/* Clear screen on reset - do this before pnv is moved to center/align the image */
	if (res)
		for (UINT32* tpnc = pnv; tpnc < pni; ++tpnc)
			*tpnc = 0;

	/* Histogram-width */
	int hwmn = 50;									// histogram-width-minimum
	int hw = hwmn;									// histogram width
	if (ds.sfcm)
		hw = max(hwmn, ds.plw - ceil(sqrt(scsz)) * ds.hlpz * 2);		// histogram width

	int pbs = scsz / ds.hlzm;						// pixel-buffer-size (one line / space-size div horizontal-zoom)
	//if (scsz % hlzm)
	//	++pbs;// += scsz % hlzm;
	const int stst = 0;								// state shiift (nr. of state bits shifted out / disregarded)

	ds.vlzm = max(ds.vlzm, ds.vlfs);				// vertical zoom can not be larger than zomm since it not possible (atm) to go back to previous generatios=lines
	if (ds.vlfs == 0 || ds.hlfs == 0)
		ds.hlfs = ds.vlfs = 1;

	/* size of one buffer
	buffer must be large enough to always allow for readahead for next-gen-calculation (cr->ncn)
	or drawing-calucaltions (hlfs) */
	int brsz = max(ds.hlfs - 1, cr->ncn - 1);	// 32 to make sure simd instructions have enough room
	brsz += 32;  // !! ??? Why is max 32 not enough???
	brsz = max(1024, brsz);

	UINT32* pnc = pni;									// pixel-screen cursor / current position 

	/* Stripe-mode initialization */
	if (ds.sm) {
		// center verticaly
		int al = (pni - pnv) / ds.plw;
		int ls = 0;
		if (al > pbs)
			ls = al % pbs;
		pnv += ds.plw * (ls / 2 + ls % 2);
		pni -= ds.plw * (ls / 2);
		//// center horizontaly
		if (ds.plw > pbs)
			pnv += (ds.plw % pbs) / 2;
	}
	static UINT32* spnc = NULL;	// static pixel screen cursor; needed for stripe mode
	if (!spnc || res)
		spnc = pnv;

	/* Inititalize pixel-display-cursor */
	if (ds.sfcm) {
		// screen-filling-curve mode enabled
		pnc = pnv;
	}
	else if (!ds.sm) {
		// scroll-mode
		/* Scroll pixel-screen to make room to draw tm pixels */
		int64_t vlsl = tm / max(1, ds.vlzm) * ds.vlpz;				// vertical scroll / lines to scroll
		tm = vlsl * ds.vlzm / ds.vlpz;
		int hlsl = 0;										// horizontal scroll
		///if (pbs == plw) {									// horizontal scrolling is (atm) only possible when output- and display-width are the same 
		///	hlsl = min(plw, (tm % (scsz * vlzm)) / max(1, vlzm * hlzm));				// horizontal scroll / nr. of pixels to scroll
		///}
		if (vlsl * ds.plw < pni - pnv) {
			pnc = max(pnv, pni - vlsl * ds.plw - hlsl);			// set cursor to first pixel that has to be drawn
			scroll(pnv, (pni - pnv) * 4, -(pni - pnc) * 4);		// scroll is measured in nr of pixels > convert to argb-pixel with size of 4 byte
		}
		else
			pnc = pnv;
	}
	else {
		pnc = spnc;
	}

	if (pbs != ds.plw) {
		//printf("tm %d sl %d/%d  plw %d   pc %d %d %d\n", tm, vlsl, hlsl, plw, pnc - pnv, pni - pnv, pni - pnc);		// scroll measures in bytes - 1 argb-pixel = 4 byte
	}

	/* Init pixel-buffer */
	static UINT32* pbv = NULL;							// pixel-buffer first valid element
	static UINT32* pbi = NULL;							// pixel-buffer first invalid element
	static UINT32* pbc = NULL;							// pixel-buffer cursor / current element
	if (!pbv || res) {
		_aligned_free(pbv);
		pbv = _aligned_malloc(pbs * sizeof * pbv, BYAL);
		pbc = pbi = pbv + pbs;
	}
	/* Init cell-space-buffer */
	static CA_CT* csv = NULL;							// cell-array first valid element
	static CA_CT* csi = NULL;							// cell-array first invalid element
	static CA_CT* csf = NULL;							// position of first cell, corrected for shifting
	if (!csv || res) {
		_aligned_free(csv);
		csf = csv = _aligned_malloc((scsz + brsz) * sizeof * csv, BYAL);
		csi = csv + scsz;
		memcpy(csv, sc, scsz * sizeof * csv);
	}
	/* Init testing-cell-space-buffer */
	static CA_CT* tcsv = NULL;							// test-cell-array first valid element
	if (ds.tm == 2) {
		_aligned_free(tcsv);							// aligned memory management may be necesary for use of some intrinsic or opencl functions
		tcsv = _aligned_malloc((scsz + cr->ncn - 1) * sizeof * tcsv, BYAL);
		memcpy(tcsv, csv, scsz * sizeof * tcsv);

		// compare
		int mcr = memcmp(csv, tcsv, scsz * sizeof * csv);
		if (mcr != 0) {
			printf("INIT TESTMODE ERROR  mcr %d  sz %d\n", mcr, scsz * sizeof * csv);
			for (int c = 0; c < scsz; ++c) {
				if (tcsv[c] != csv[c])
					printf("  error at  pos %d  test %d  org %d\n", c, tcsv[c], csv[c]);
			}
		}


	}

	/* Init bit-array-cell-space-buffer */
	static caBitArray vba = { .v = NULL, .rc = 0, .rw = 0, .clsc = NULL, .ct = 0, .lc = 0, .lw = 0, .mmwn = 0, .oc = 0, .scsz = 0 };				// row-first/vertical-bit-array
	if (vba.v == NULL || res) {
		vba.ct = scsz;
		vba.cr = cr;
		vba.clsc = csv;
		vba.scsz = scsz;
		vba.brsz = brsz;

		if (ca_cnsgs[cnmd].itfn != NULL) {
			printf("CA_CNITFN  %s\n", ca_cnsgs[cnmd].name);
			(ca_cnsgs[cnmd].itfn)(&vba, cr);
		}
		else
			printf("CA_CNITFN  %s  IS NULL\n", ca_cnsgs[cnmd].name);
		///		CA_PrintSpace("org csv", csv, csi - csv);
///		CA_PrintSpace("org csv", tcsv, scsz);
///		printf("csv memcmp  %d\n", memcmp(csv, tcsv, scsz * sizeof * csv));

///		print_bitblock_bits(tba.v, tba.ct / BPB + tba.oc * tba.rw / BPB, tba.rw / BPB);
///		CABitArrayPrepareOR(&tba);
		//print_bits(tba.v, tba.ct / 8 + tba.oc * tba.rw / 8, tba.rw / 8);
///		print_bitblock_bits(tba.v, tba.ct / BPB + tba.oc * tba.rw / BPB, tba.rw / BPB);
	}
	{
		// compare
		/*
		CA_CT* tcsv;
		tcsv = _aligned_malloc(scsz * sizeof * tcsv,  BYAL);
		convertBetweenCACTandCABitArray(csv, csv + scsz, &vba, 0);
		convertBetweenCACTandCABitArray(tcsv, tcsv + scsz, &vba, 1);
		int mcr = memcmp(csv, tcsv, scsz * sizeof * csv);
		if (mcr != 0) {
			printf("CONVERT ERROR  mcr %d  sz %d\n", mcr, scsz * sizeof * csv);
			for (int c = 0; c < scsz; ++c) {
				if (tcsv[c] != csv[c])
					printf("  error at  pos %d  test %d  org %d\n", c, tcsv[c], csv[c]);
			}
		}
		else
			;// printf("CONVERT TEST SUCCESS\n");
		_aligned_free(tcsv);
		*/
	}
	/* Init color table */
	int dyss;
	if (ds.fsme == 0)
		dyss = max(1, (cr->ncs * (max(1, max(1, ds.hlfs) * max(1, ds.vlfs)))) >> stst);
	else
		dyss = max(1, ipow(cr->ncs, ds.hlfs * ds.vlfs) >> stst);
	static UINT32* cltbl = NULL;						// color table
	if (!cltbl || res) {
		free(cltbl);
		cltbl = malloc(dyss * sizeof * cltbl);
	}

	/* Init state count table */
	static UINT32* sctbl = NULL;						// state count table
	static double* dsctbl = NULL;						// double state count table
	static double* tdsctbl = NULL;						// temp double state count table
	if (!sctbl || res) {
		free(sctbl);
		sctbl = malloc(dyss * sizeof * sctbl);
		free(dsctbl);
		dsctbl = malloc(dyss * sizeof * dsctbl);
		free(tdsctbl);
		tdsctbl = malloc(dyss * sizeof * tdsctbl);
	}
	//
	static double* spt = NULL;							// state position table		
	static double* svt = NULL;							// state velocity table
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
	double dmn;											// double min
	double dmx;											// double max

	if (pnc >= pni)										// drawing enabled
		de = 0;

	int64_t otm = tm;									// original time

	//int mcr = memcmp(csv, tcsv, scsz * sizeof * csv);
	//if (mcr != 0) 
	//	printf("PRE TESTMODE ERROR  calculated  mcr %d  sz %d\n", mcr, scsz * sizeof * csv);

	while (1) {
		/* DISPLAY - Copy as much pixels as possible from the pixel-buffer to the pixel-screen */
		if (de && pbc < pbi) {
			/* There are 3 drawing modes
			 * 1. 1d-scroll-mode - new ca-lines are drawn at bottom of screen, old ca-lines scroll up
			 * 2. 1d-scanline-mode - a scanline moves repeatedly from top to bottom, new ca-lines are drawn at current scanline position
			 * 3. screen-filling-curve-mode - the 1d ca-line is drawn on the 2d screen using one of several screen-filling-curves
			 *
			 * 1. and 2. can draw up to screen-height generations per frame and are best suited for small ca-spaces.
			 * 3. can draw only one generation per frame and is best suited for very large ca-spaces.
			*/
			if (!ds.sfcm) {
				for (int vi = 0; vi < ds.vlpz; ++vi) {
					int cs =
						(
							min(ds.plw,
								max(0,
									min(
										(pni - pnc),
										ds.hlpz * (pbi - pbc)))));		// size of copy
					if (cs > 0) {
						if (!ds.sm && pbs * ds.hlpz > ds.plw)
							pbc = pbv + (pbs * ds.hlpz - ds.plw) / 2 / ds.hlpz;
						if (!ds.sm && pbs * ds.hlpz < ds.plw)
							pnc += (ds.plw - pbs * ds.hlpz) / 2;

						if (ds.hlpz > 1) {
							for (int hi = 0; hi < cs / ds.hlpz; ++hi) {
								for (int hzi = 0; hzi < ds.hlpz; ++hzi) {
									*pnc = *pbc;
									++pnc;
								}
								++pbc;
							}
						}
						else {
							memcpy(pnc, pbc, cs * 4);
							pbc = pbi;
							pnc += cs;
						}

						if (ds.sm) {
							if (pbs < ds.plw)
								pnc += ds.plw - pbs;
							if (pnc >= pni) {
								if (pbs > ds.plw)
									pnc = pnv;
								else
									pnc = pnv + (pnc - pnv) % ds.plw + pbs;
								if (pnc + pbs > pnv + ds.plw)
									pnc = pnv;
								if (tm > pni - pnv)
									de = 0;
							}
							spnc = pnc;
						}
						pnc += (pni - pnc) % ds.plw;
						pbc = pbv;
					}
					else {
						if (pni - pnc == 0)
							de = 0;
					}
				}
			}
			else if (ds.sfcm >= 1 && ds.sfcm <= 4) {
				//printf("display_2d_matze ds   vlpz %d  hlpz %d  pbv %x pbi %x\n", ds.vlpz, ds.hlpz, pbv, pbi);
				display_2d_matze(pnv + hw, pni, ds.plw, ds.plw - hw, ds.vlpz, ds.hlpz, pbv, pbi, ds.sfcm - 1, ds.mdpm);
				//de = 0;
			}
			else if (ds.sfcm > 4 && ds.sfcm < 5 + LMS_COUNT) {
				display_2d_lindenmeyer(LMSA[ds.sfcm - 5], pnv + hw, pni, ds.plw, ds.plw - hw, ds.vlpz, ds.hlpz, pbv, pbi, ds.sfcsw, -1, NULL, NULL, NULL);
				//de = 0;
			}
			else {
				display_2d_chaotic(pnv + hw, pni, ds.plw, ds.plw - hw, csv, csi, pbv, pbi, res);
			}
		}

		if (tm < 1)
			break;

		/* ONE line */
		for (int vlp = 0; vlp < ds.vlzm; ++vlp) {
			/* Copy cells of wrap behind buffer (last cells in line) */
			memcpy(csv + scsz, csv, brsz * sizeof * csv);
			if (de) {
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
			if (ca_cnsgs[cnmd].cnfn != NULL) {
				//printf("CA_CNFN %s  GNC %d", ca_cnsgs[cnmd].name, gnc);
				gnc = (ca_cnsgs[cnmd].cnfn)(gnc, &vba);
			}
			else {
				printf("CA_CNFN  UNDEFINED  cnmd %d / %s\n", cnmd, ca_cnsgs[cnmd].name);
				gnc = 0;
			}
			//
//printf("/ %d   tm %u   de %d   scsz %d   brsz %d\n", gnc, tm, de, scsz, brsz);
			tm -= (ognc - gnc);											// adjust time
			csf -= ((ognc - gnc) * (cr->ncn / 2)) % scsz;								// correct for shift
			if (csf < csv)
				csf += scsz;
			if (csf >= csi)
				csf -= scsz;
		}

		if (de) {
			if (ds.hlfs==1 && (cnmd == CM_HASH || cnmd == CM_LUT || cnmd == CM_VBA_1x256)) {
				if (cnmd == CM_LUT || cnmd == CM_VBA_1x256) {
					if (cnmd == CM_VBA_1x256)
						//convert_CA_CT_to_bit_array(vba.clsc, (FBPT*)vba.v, vba.scsz + 8);
						convert_CA_CT_to_bit_array(csv, vba.v, scsz);
					// reset
					memset(hct, 0, HCISZ * sizeof(HCN));
					memset(&hcln, 0, sizeof(hcln));
					memset(&hcls, 0, sizeof(hcls));
					memset(&hc_stats, 0, sizeof(hc_stats));
					hc_sn = convert_bit_array_to_hash_array(&vba, &hc_sl);
				}
				HC_clear_usage_stats(hc_sl, hc_sn);
				HC_nodes_count_usage(hc_sl, hc_sn);
				for (int i = 0; i < hc_sl; ++i)
					hc_stats[i].ucmx = hc_stats[i].ucmn = 0;
				HC_update_usage_stats(hc_sl, hc_sn);
				double mfa = 1.0;
				double mf = 0.0;
				double v = 0.0;
				if (mfa == 1.0) {
					v = 1.0;
					for (int i = 0; i < hc_sl; ++i)
						v *= hc_stats[i].ucmx;
					v = 1.0 / v;
					//mf = 1.0 / (double)(hc_sl);
v = 1.0;
				} 
				else
					mf = pow(mfa, -1.0 * (double)(hc_sl + 1));
				display_hash_array(ds, pbv, pbv - (csf - csv), pbi, mf, mfa, v, hc_sl, hc_sn);

				UINT32 mxv = 0;	// max v
				UINT32 mnv = MAXUINT32;	// min v
				for (UINT32* pbc = pbv; pbc < pbi; ++pbc) {
					if (*pbc > mxv)
						mxv = *pbc;
					if (*pbc < mnv)
						mnv = *pbc;
				}
//if (1)printf("mxv %u  mnv %u\n", mxv, mnv);
				double mnvlg = log2((double)mnv);
				double vlgrg = max(1.0, log2((double)mxv) - mnvlg);
				mxv = mxv - mnv;
				for (UINT32* pbc = pbv; pbc + 7 < pbi;) {
					double v;
					if (ds.lgm) {
						v = (log2((double)*pbc) - mnvlg) / vlgrg;
					}
					else {
						v = ((double)*pbc - mnv) / (double)mxv;
					}

					UINT32 c;
					c = getColor(v, ds.cm, ds.crct, ds.gm);
					//c = 0xFF - 0xFF * v;
					c = c << 16 | c << 8 | c;
					for (int i = 0; i < 8; i++) {
						*pbc = c;
						++pbc;
					}
				}
			}
			else {
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

	/* test calculation */
	if (ds.tm == 2 && tcsv != NULL) {
		// calculate test-cell-space to compare
		int64_t ognc = otm;
		for (int g = 0; g < ognc; ++g) {
			// copy wrap-arround-buffer / border
			for (int c = 0; c < cr->ncn - 1; ++c) {
				tcsv[scsz + c] = tcsv[c];
			}
			// calc next gen
			ca_next_gen__sisd(cr, tcsv, tcsv + scsz);
		}
		// compare
		int mcr = memcmp(csv, tcsv, scsz * sizeof * csv);
		if (mcr != 0) {
			printf("TESTMODE ERROR  mcr %d  gnc %d  sz %d  pos", ognc, mcr, scsz * sizeof * csv);
			int esf = 0;		// errors-found
			for (int c = 0; c < scsz; ++c) {
				if (tcsv[c] != csv[c]) {
					printf(" %d", c);
					esf++;
					if (esf > 9) {
						printf("...");
						break;
					}
				}
			}
			printf("\n");
			getch();
		}
	}

	/* Histogram */
	if (hw > 0) {
		int plh = (pni - pnv) / ds.plw;
		int vs = 20;			// vertical spacing to screen border of histogram
		int hh = plh - 2 * vs;	// histogram height
		int dph = 3;// hh / 100;	// histogram height of one datapoint
		/* history */
		static uint16_t* hy = NULL;			// history
		static int hyc = 0;					// history count
		static int hyi = -1;				// history iterator
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

	/* Copy space back with correct alignment */
	for (int i = 0; i < scsz; ++i)
		sc[i] = csv[(csf - csv + i) % scsz];
}


/*
CA_CT *rltl,		// rule table
CA_CT *ca_space,	// space
int ca_space_sz,	// space-size
int ca_time			// time - nr. of steps/generations to live
*/
void
CA_LiveAndDrawR(
	CA_RULE* cr,		/* ca configuration */
	CA_CT* spc,			/* space */
	int spcsz,			/* space-size */
	int zoom,
	int tm,				/* time - nr. of steps/generations to live */
	//int ncr,		/* number of cells needed to calculate next generation (rule table lookup) */
	int ncd,			/* number of cells for drawing */
	UINT32* px,		/* rgba pixel array to draw to */
	int res,			/* reset-flag (because of the static vars)*/
	UINT32* pxbrd,
	UINT32* pxfrst,
	int pitch,
	int sm,		/* sharpening mode */
	int ar		/* auto-range - 0: none, 1: auto min, 2: auto max, 3: auto min and max */
) {
	int zmp = 0; /* position for zoom */
	int zmsm; /* sum for zoom */

	/* memory size of one cell */
	int soc = sizeof(CA_CT);
	/* maximum needed number of cells to the left of current cell */
	int nlc = (max(cr->ncn, ncd) - 1) / 2;
	/* maximum needed number of cells to the right of current cell */
	int nrc = (max(cr->ncn, ncd) - 1) - nlc;
	/* current space */
	static CA_CT* cs = NULL;
	if (!cs | res) {
		cs = malloc((nlc + spcsz + nrc) * soc);
		memcpy(cs + nlc, spc, spcsz * soc);
	}
	/* new space (next generation) */
	static CA_CT* ns = NULL;
	if (!ns) {
		ns = malloc((nlc + spcsz + nrc) * soc);
		memcpy(ns + nlc, spc, spcsz * soc);
	}

	/* color table and state count table */
	int nbh_states_cnt = (cr->ncs - 1) * 4/*nbh_cnt*/ + 1;    // count of different neighborhods
	//int nbh_states_cnt = 1024;    // count of different neighborhods
	nbh_states_cnt = min(0xFFFF, nrc);
	sm = min(sm, nrc);
	static UINT32* cltbl = NULL;
	if (!cltbl)
		cltbl = malloc(nbh_states_cnt * sizeof * cltbl);
	static int* sctbl = NULL;	// state count table
	if (!sctbl)
		sctbl = malloc(nbh_states_cnt * sizeof * sctbl);

	/* Calculate multiplication factors for summing the neighbour cells */
	// old todo text: use a lookup table here as well - could also be used for totalistic if all values would be 1
	static int* pmftbl = NULL;
	if (!pmftbl)
		pmftbl = malloc(cr->ncn * sizeof * pmftbl);
	for (int i = 0; i < cr->ncn; i++)
		if (cr->tp == ABS) 	// absolute
			pmftbl[i] = ipow(cr->ncs, cr->ncn - i - 1);
		else
			// totalistic
			pmftbl[i] = 1;

	/* repeat tm times */
	for (int tmi = 0; tmi < tm; tmi++) {
		/*
		spcsz = 6, nlc = 2
		0123456789
		LLSSSSSSRR
		*/
		memcpy(cs, cs + spcsz, nlc * soc);
		memcpy(cs + nlc + spcsz, cs + nlc, nrc * soc);
		memcpy(ns, ns + spcsz, nlc * soc);
		memcpy(ns + nlc + spcsz, ns + nlc, nrc * soc);

		if (sm > 0 && px < pxbrd) {
			/* get min (excluding zero) and max of state count table */
			int min = MAXINT, max = 0;
			if (ar == 0 || ar == 1)
				min = 0;
			if (ar == 0 || ar == 2)
				max = spcsz;
			if (ar != 0) {
				for (int i = 0; i < nbh_states_cnt; i++) {
					//if (sctbl[i] < min)
					if (sctbl[i] > 0 && sctbl[i] < min)
						min = sctbl[i];
					if (sctbl[i] > max)
						max = sctbl[i];
				}
				if (max <= min)
					max = min + 1;
			}

			/* build color table from state count table and reset state count table*/
			//printf("min %6d  max %6d  log max-min %f  %f\n  color table  ", min, max, log(max - min), log(2));
			for (int i = 0; i < nbh_states_cnt; i++) {

				//cltbl[i] = 0xFF - (double)0xFF * (sctbl[i] - min) / (max - min);
				//cltbl[i] = 1.0 * 0xFF / log(max - min + 1) * log(sctbl[i] - min + 1);
				//cltbl[i] = 0xFF - min(0xFF, cltbl[i]);
				//cltbl[i] = 0xFF - ((cltbl[i]) % 0x100);
				//cltbl[i] = min(0xFF, cltbl[i]);
				//cltbl[i] = 0xFF - cltbl[i];
				//cltbl[i] = cltbl[i] << 16 | cltbl[i] << 8 | cltbl[i];

				double v = (double)(sctbl[i] - min) / (max - min);
				double l = 0.5, s = 1.0;

				/* log mode */
				if (0) {
					l = 1.0 - 1.0 / log(max - min) * log(sctbl[i] - min + 1.0);
					l = 1.0 - 1.0 / (log(max) - log(min)) * (log(sctbl[i]) - log(min));

					l = 1.0 - 1.0 / (max - min) * (sctbl[i] - min);
					l -= 1.0 - 1.0 / max * sctbl[i];

					v = l;
					s = 0.0;
				}

				/* grayscale */
				if (0) {
					UINT32 gv = 0xFF - 0xFF * v;
					cltbl[i] = gv << 16 | gv << 8 | gv;
				}
				/* b&w */
				else if (0) {
					if (v > 0.5)
						cltbl[i] = 0xFFFFFF;
					else
						cltbl[i] = 0x000000;
				}
				else {
					v = 1.0 - v;
					/* bw & color */
					double sh = 1.0 / 2.0 / 4.0;
					if (v <= sh) {
						l = 0.5 - 0.5 / sh * (sh - v);
						l = 0.5 / sh * v;
						s = 1.0 / sh * v;
						v = 0;
					}
					else if (v < 1.0 - sh)
						v = (v - sh) / (1.0 - 2.0 * sh);
					else {
						l = 0.5 + 0.5 / sh * (v - 1.0 + sh);
						l = 1.0 - 0.5 / sh * (1.0 - v);
						s = 1.0 / sh * (1.0 - v);
						v = 0;
					}
					//l = v;
					//if (v < 0.5)
					//	s = v * 2.0;
					//else
					//	s = 1.0 - (v - 0.5) * 2.0;


					//cltbl[i] = flp_ConvertHSLtoRGB(
					//	v,
					//	s,
					//	l);
					cltbl[i] = HSL_to_RGB_16b_2(
						0xFFFF * v,
						0xFFFF * s,
						0xFFFF * l);
					//printf("%d-%d  ", sctbl[i], cltbl[i]);
				}
				/* reset state count table */
				sctbl[i] = 0;
			}
			//printf("\n");
		}

		/* current cell in current space */
		CA_CT* ccs = cs + nlc - cr->ncn / 2;
		/* current cell in new space */
		register CA_CT* cns = ns + nlc;
		/* border of new space */
		register CA_CT* cnsb = cns + spcsz;

		//while (1) {
		zmsm = 0;
		zmp = 0;
		CA_CT td = 0xFF / (cr->ncs - 1); //tmp test value
	lwhile:
		//for (int i = 0; i < spcsz; i++) {
		/* draw current generation */
		if (sm == 0) {
			if (px < pxbrd) {
				//*px = 0xFF * *ccs / (cr->ncs - 1); 
				//*px = *px << 16 | *px << 8 | *px;
				// TODO test lookup table
				*px = *ccs * td;
				*px = *px << 16 | *px << 8 | *px;
				++px;
			}
		}// hack for speed testing
		else if (sm >= 0) {
			zmsm += *ccs;// 0xFF / (cr->ncs - 1) * *ccs;
			zmp++;
			if (zmp == zoom) {
				if (px < pxbrd) {
					if (!sm) {
						*px = 0xFF * zmsm / (cr->ncs - 1) / zoom;
						////*px = 0xFF / (cr->ncs - 1) * *ccs;
						*px = *px << 16 | *px << 8 | *px;
					}
					else {
						int sccs = 0, scns = 0; /* sum of ccs/cns */
						for (int i = 0; i < sm; i++) {
							sccs += ccs[i];
							scns += cns[i];
							//sccs += ccs[i] * ipow(cr->ncs, i);
							//scns += cns[i] * ipow(cr->ncs, i);
						}
						*px = cltbl[scns];// +cns[1] * 3 + cns[2] * 9 + cns[3] * 0];
						++sctbl[sccs];// +ccs[1] + ccs[2] + ccs[3] * 1];
					}
					//++sctbl[ccs[0] + ccs[1] * 3 + ccs[2] * 9 + ccs[3] * 0];
					++px;
				}
				zmp = 0;
				zmsm = 0;
			}
		}
		/* calculate next generation */
		//*cns = cr->rltl[ccs[0] + ccs[1] + ccs[2]]; // static 3-cell-neighborhod

		/* absolute */
		if (cr->tp == ABS) {
			int sum = 0;
			for (int i = 0; i < cr->ncn; i++) {
				//sum += ccs[i] * ipow(2, i + 1);
				// TODO use a lookup table here as well - could also be used for totalistic if all values would be 1
				sum += ccs[i] * ipow(cr->ncs, cr->ncn - i - 1);
			}
			*cns = cr->rltl[sum];
			//*cns = cr->rltl[ccs[0] + 2 * ccs[1] + 4 * ccs[2] + 8 * ccs[3] + 16 * ccs[4]]; // static 3-cell-neighborhod
		}
		else if (cr->tp == TOT) {
			/* totalisitc */
			int sum = ccs[0];
			for (int i = 1; i < cr->ncn; i++)
				sum += ccs[i];
			*cns = cr->rltl[sum];
		}
		// shorter version but worser speed for totalistic (slightly) absolute increase in speed not verified yet
		//int sum = 0;
		//for (int i = 0; i < cr->ncn; i++)
		//	sum += ccs[i] * pmftbl[i];
		//*cns = cr->rltl[sum];

		++ccs;
		++cns;
		//if (cns == ns + nlc + spcsz)
		//if (cns == cnsb)
		//	break;
		if (cns < cnsb)
			goto lwhile;
		//}

		/* pitch */
		px += pitch;

		/* swap ns with cs */
		CA_CT* ss;
		ss = ns, ns = cs, cs = ss;
	}

	/**/
	memcpy(spc, cs + nlc, spcsz * soc);
	//free(cs);
	//free(ns);
}

/* variant with other way to create patterned background (by using large numbers instead of arrays - seems
to work and is probably much more efficient than using arrays, but becomes more defficult to handle when
larger patterns are needed */
//void
//CA_RandomizeSpace(
//	CA_CT *spc,
//	int spcsz,
//	int sc,		/* count of possible cell states */
//	int rc		/* count of cells which will be randomized (density of random cells); everything else is background (which state is randomly choosen once */
//	) {
//	int randmax30bit = (RAND_MAX << 15) | RAND_MAX;	// 
//	int rand30bit = (rand() << 15) | rand();
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

void
CA_RandomizeSpace(
	CA_CT* spc,
	int spcsz,
	int sc,		/* count of possible cell states */
	int rc,		/* count of cells which will be randomized (density of random cells); everything else is background (which state is randomly choosen once */
	int ps		// background-pattern size
) {
	if (!ps) {
		ps = rand() % (spcsz / 2);		// pattern size - linear
		ps = 1 << (1 + rand() % ((int)(log(spcsz / 2) / log(2))));	// integer log 2
		ps = pow(2.0, log(spcsz / 2.0) / log(2.0) / RAND_MAX * rand());	// float log
	}
	CA_CT* pa;							// pattern array
	pa = malloc(ps * sizeof(*pa));		// allocate memory for pattern array
	for (int i = 0; i < ps; ++i)		// create random pattern
		pa[i] = (int)rand() % sc;

	/* Print info about random cells and pattern */
	printf("randomize space\n");
	printf("\tspace size  %d\n", spcsz);
	printf("\trandomized cells  %d\n", rc);
	printf("\tpattern size  %d\n", ps);
	printf("\tspace size / pattern size  %d %% %d\n", spcsz / ps, spcsz % ps);
	printf("\tpattern  ");
	for (int i = 0; i < ps; ++i)
		printf("%u", pa[i]);
	printf("\n");

	/* Cretae randomized space */
	register CA_CT* psn = spc;					// position
	register CA_CT* bdr = spc + spcsz;			// border
	register int pc = 0;						// pattern cursor
	while (psn < bdr) {
		if ((rand() << 15 | rand()) % spcsz < rc)
			*psn = rand() % sc;
		else
			*psn = pa[pc];
		++pc;
		if (pc >= ps)
			pc = 0;
		++psn;
	}
}


void pixel_effect_hilbert(SIMFW* simfw) {
	//gosper(simfw);

	return;

	// size & needed shift to center
	int lw = simfw->sim_width;
	int sz = min(simfw->sim_height, simfw->sim_width);
	sz = log2(sz);
	sz = ipow(2, sz);
	printf("HILBERT\nsz  %d\n", sz);
	int sttp = (simfw->sim_height - sz) / 2;			// shift from top
	int stlt = (simfw->sim_width - sz) / 2;			// shift from left
	int ar = sz * sz;

	//
	double lgmx, lgmx12, lf;
	lf = 1e-0;
	lgmx = log(ar * lf + 1);
	lgmx12 = log(ar * lf / 2 + 1);
	//
	uint32_t* px = simfw->sim_canvas;
	for (int i = 0; i < ar; ++i) {
		int y, x;
		d2xy(sz, i, &x, &y);
		//MortonDecode2(i, &x, &y);
		//y = i / sz, x = i %sz;
		//printf("%d %d\t", y, x);
		uint32_t col;
		col = HSL_to_RGB_16b_2((MAXUINT32 / ar * i) >> 16, 0xFFFF, 0x8000);
		col = 255.0 / ar * 2.0 * abs(i - ar / 2), col = col << 16 | col << 8 | col;

		//col = i % 256, col = col << 16 | col << 8 | col;
		col = 255.0 / lgmx12 * log(abs(i - ar / 2) * lf + 1), col = col << 16 | col << 8 | col;

		px[y * lw + x + sttp * lw + stlt] = col;
		//px = HSL_to_RGB_16b_2((MAXUINT32 / ar * i) >> 16, 0xFFFF, 0x8000);
		//++px;
	}


}



void
CA_MAIN(void) {
	//test_parallel_add();
	init_lmas();

	srand((unsigned)time(NULL));
	static pcg32_random_t pcgr;

	/* Init */
	//SDL_WINDOWPOS_CENTERED_DISPLAY(1)

	SIMFW sfw = { 0 };
	SIMFW_Init(&sfw, SIMFW_TEST_TITLE, SIMFW_TEST_DSP_HEIGHT, SIMFW_TEST_DSP_WIDTH,
		SIMFW_TEST_DSP_WINDOW_FLAGS | SDL_WINDOW_RESIZABLE
		, 0, 0);

	//SIMFW_SetSimSize(&sfw, sfw.window_height * 2.0, sfw.window_width * 2.0);
	//SIMFW_SetSimSize(&sfw, sfw.window_height * 4.0, sfw.window_width * 4.0);
	//SIMFW_SetSimSize(&sfw, sfw.window_height / 5.0, sfw.window_width / 5.0);
	//SIMFW_SetSimSize(&sfw, sfw.window_height / 4.0, sfw.window_width / 4.0);

	double zoom = 1.0;

	printf("window  height %d  width %d\n", sfw.window_height, sfw.window_width);
	printf("sim  height %d  width %d\n", sfw.sim_height, sfw.sim_width);
	pixel_effect_moving_gradient(&sfw);
	pixel_effect_hilbert(&sfw);
	//sfw_test_rand(&sfw);


	int ca_space_sz = 1 << 16;// sfw.sim_width * 1; // 256;
	int rel_start_speed = 512;
	int res = 1;
	int cnmd = CM_SISD;				// computation-mode
	int de = 1;					// drawing enabled
	int klrg = 32;				// kernel range if computation-mode is GPU

	caDisplaySettings ds;		// display settings
	ds.fsme = 0;
	ds.ar = 3;
	ds.ard = 0.1;
	ds.arf = 0.1;
	ds.cm = 3;
	ds.crct = 0;
	ds.gm = 1;
	ds.lgm = 1;
	ds.cmm = 0;
	ds.tm = 0;
	ds.sd = 1.0;
	ds.te = 0.0;
	ds.sm = 0;					// stripe mode
	ds.stzm = 1;				// state zoom - 0 = direct zoom on intensity
	ds.sfcm = 1;				// screen-filling-curve-mode
	ds.sfcsw = 1;				// screen-filling-curve-step-width
	ds.hlfs = 1;				/* focus / sharpening mode */
	ds.vlfs = 1;				/* focus / sharpening mode */
	ds.vlzm = 1;				/* vertical zoom */
	ds.hlzm = 1;				/* horizontal zoom */
	ds.vlpz = 1;				/* vertical pixel zoom */
	ds.hlpz = 1;				/* horizontal pixel zoom */
	ds.mdpm = 0;

	uint64_t tm = 0;			/* time */

	int64_t speed = rel_start_speed, x_shift = 0;

	CA_RULE cr = CA_RULE_EMPTY;		/* ca configuration */
									// rule 153 (3 states) und  147 (2 states absolut) ähnlich 228!
									// neuentdeckung: rule 10699 mit 3 nb
	cr.rl = 228;//  3097483878567;// 1598;// 228; // 228;// 3283936144;// 228;// 228;// 90;// 30;  // 1790 Dup zu 228! // 1547 ähnliche klasse wie 228
	cr.tp = TOT;
	cr.ncs = 3;
	cr.ncn = 3;

	int window_x_position = 0;
	int window_y_position = 0;
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
		nkb.name = "window-x-position";
		nkb.val = &window_x_position;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		SDL_GetWindowPosition(sfw.window, &window_x_position, &window_y_position);
		nkb = eikb;
		nkb.name = "window-width";
		nkb.val = &window_width;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		SDL_GetWindowPosition(sfw.window, &window_x_position, &window_y_position);
		nkb = eikb;
		nkb.name = "window-height";
		nkb.val = &window_height;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "window-y-position";
		nkb.val = &window_y_position;
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
		nkb.name = "display-log-mode";
		nkb.val = &ds.lgm;
		nkb.slct_key = SDLK_b;
		nkb.min = 0; nkb.max = 1;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "display-stripe-mode";
		nkb.val = &ds.sm;
		nkb.slct_key = SDLK_m;
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
		nkb.name = "display-test-mode"; nkb.description = "0: disabled, 1: graphics, 2: computation";
		nkb.val = &ds.tm;
		nkb.slct_key = SDLK_F8;
		nkb.min = 0; nkb.max = 2;
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
		nkb.name = "computation-mode"; nkb.description = "";
		/// CNMD! nkb.value_strings = cm_names; 
		nkb.val = &cnmd;
		nkb.slct_key = SDLK_F5;
		nkb.min = 0; nkb.max = CM_MAX - 1;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "auto-range"; nkb.description = "0: none, 1: auto min, 2: auto max, 3: auto min and max";
		nkb.val = &ds.ar;
		nkb.slct_key = SDLK_y;
		nkb.min = 0; nkb.max = 3;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "color-mode"; nkb.description = "0: b&w, 1: gray, 2: color+b&w, 3: color, 4: oklab light, 5: oklab dark, 6-33: oklab gradients";
		nkb.val = &ds.cm;
		nkb.slct_key = SDLK_x;
		nkb.min = 0; nkb.max = 33;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "color-count"; nkb.description = "0: unlimited: max: 0xFFFFFF";
		nkb.val = &ds.crct;
		nkb.slct_key = SDLK_c;
		nkb.min = 0; nkb.max = 0xFFFFFF;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "gradient-mode"; nkb.description = "0: unchanged 1: reversed 2: mirrored 3: reversed and mirrord";
		nkb.val = &ds.gm;
		nkb.slct_key = SDLK_v;
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
		nkb.name = "drawing-enabled";
		nkb.val = &de;
		nkb.slct_key = SDLK_F7;
		nkb.min = 0; nkb.max = 1;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "kernel-range-(computation-mode)";
		nkb.val = &klrg;
		nkb.slct_key = SDLK_F6;
		nkb.min = 0; nkb.max = 1024 * 8;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "screen-filling-curve-mode";
		nkb.val = &ds.sfcm; nkb.cgfg = &res;
		nkb.slct_key = SDLK_f;
		nkb.min = 0; nkb.max = 32;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "screen-filling-curve-mode-parameter";
		nkb.val = &ds.mdpm; nkb.cgfg = &res;
		nkb.slct_key = SDLK_g;
		nkb.min = 0; nkb.max = 4096;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "space-size";
		nkb.val = &ca_space_sz; //nkb.cgfg = &res;
		nkb.slct_key = SDLK_h;
		nkb.min = 256; nkb.max = 1024 * 1024 * 64;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "speed";
		nkb.val = &speed;
		//nkb.slct_key = SDLK_g;
		nkb.min = 0; nkb.max = 1 << 30;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "focus-mode"; nkb.description = "0: absolute, 1: totalistic";
		nkb.val = &ds.fsme;
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
		nkb.val = &cr.rl;				// ATTENTION cr.rl is 64bit int
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

		/* Load configuration */
		SIMFW_LoadKeyBindings(&sfw, "settings.txt");
	}

	/* Restore window position */
	SDL_SetWindowPosition(sfw.window, window_x_position, window_y_position);
	SDL_SetWindowSize(sfw.window, window_width, window_height);
	sfw.window_height = window_height;
	sfw.window_width = window_width;
	SIMFW_SetSimSize(&sfw, sfw.window_height * zoom, sfw.window_width * zoom);

	/* Control-Mode */
	enum CLMD { CLMD_NONE, CLMD_RULE, CLMD_SCREEN } clmd = CLMD_NONE;


	int pitch = 0;

	cr = CA_Rule(cr);

	// initial ca-"line"
	CA_CT* ca_space = malloc(ca_space_sz * sizeof * ca_space);
	for (int i = 0; i < ca_space_sz; i++) {
		ca_space[i] = 0;
	}

	///	ca_space[ca_space_sz / 2] = 1;

	ca_space[0] = 1;
	ca_space[1] = 1;
	ca_space[3] = 1;
	ca_space[8] = 1;
	ca_space[16] = 1;
	for (int i = 0; i < 2; ++i) {
		int pos = rand() << 15 || rand();
		int lh = ipow(2, rand() % 3 + 6);
		printf("rdm  %d - %d\n", pos % ca_space_sz, lh);
		for (int i = 0; i < lh; ++i)
			*(ca_space + (pos + i) % ca_space_sz) = rand() % cr.ncs;
	}



	int mouse_speed_control = 0;

	/* SDL loop */
	while (1) {
		/* SDL event handling */
		SDL_Event e;
		int64_t org_speed = speed;
		int last_ca_space_sz = ca_space_sz;
		while (SDL_PollEvent(&e)) {
			const UINT8* kbst = SDL_GetKeyboardState(NULL); /* keyboard state */
			SDL_Keymod kbms = SDL_GetModState(); /* keyboard mod state */
			int lst = kbst[SDL_SCANCODE_LSHIFT]; // left shift
			int ctl = kbms & KMOD_CTRL; // ctrl key pressed
			int alt = kbms & KMOD_ALT; // alt key pressed
			int sft = kbms & KMOD_SHIFT; // shift key pressed
			if (e.type == SDL_QUIT)
				break;
			if (e.type == SDL_WINDOWEVENT) {
				switch (e.window.event) {
				case SDL_WINDOWEVENT_RESIZED:
					sfw.window_height = e.window.data2;
					sfw.window_width = e.window.data1;
					SIMFW_SetSimSize(&sfw, sfw.window_height * zoom, sfw.window_width * zoom);
					res = 1;

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

				if (0 && e.button.button == 1) {
					CA_RandomizeSpace(ca_space, ca_space_sz, cr.ncs, ca_space_sz / ipow(2, rand() % 10), 3);
					res = 1;
				}
				if (e.button.button == 2) {
					for (int i = 0, p = rand(), cnt = cr.ncn * (rand() % 5 + 1); i < cr.ncn; ++i)
						ca_space[(p + 1) % ca_space_sz] = rand() % cr.ncs;
					res = 1;
				}
				if (e.button.button == 3) {
					//CA_RandomizeSpace(ca_space, ca_space_sz, cr.ncs, 0, 0);
					//res = 1;
					mouse_speed_control = !mouse_speed_control;
				}
			}
			else if (e.type == SDL_WINDOWEVENT_RESIZED) {
			}
			else if (e.type == SDL_KEYDOWN) {
				//SDL_Keymod kbms = SDL_GetModState(); // keyboard mod state
				/* */
				int keysym = e.key.keysym.sym;
				/**/
				int fscd = 0;									// focus (hlfs, vlfs, vlzm or hlzm) has been changed
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
					default:
						cd = 0;
					}
					if (cd) {
						keysym = 0;
						SIMFW_SetSimSize(&sfw, sfw.window_height * zoom, sfw.window_width * zoom);
						res = 1;
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
						CA_RandomizeSpace(ca_space, ca_space_sz, cr.ncs, ca_space_sz / ipow(2, rand() % 10), 3);
						char buf[10000];
						CA_RULE_PrintInfo(&cr, &buf, sizeof(buf));
						SIMFW_SetFlushMsg(&sfw, &buf);
						res = 1;
					}
				}

				// random
				if (keysym >= SDLK_0 && keysym <= SDLK_9) {
					tm = 0;
					int rcc = 0;							// random cells count
					int ps = 1;								// pattern size
					if (alt)
						ps = 0;
					if (kbms & KMOD_SHIFT)
						// 1 = random cells 10% of width, 2 = 20%, 9 = 90%, 0 = 100%
						rcc = ca_space_sz * (keysym - SDLK_0 + (keysym == SDLK_0) * 10) / 10;
					else {
						// 0 = 1 random cell, 1 = 2, 2 = 4, 3 = 8, ..., 9 = 512
						rcc = ipow(2, keysym - SDLK_0);
						if (keysym == SDLK_0)
							rcc = 0;
					}

					if (ctl) {
						// random isles of variing length
						//int br = rand() % cr.ncs;
						//for (int i = 0; i < ca_space_sz; ++i)
						//	ca_space[i] = br;
						if (1 || keysym == SDLK_9) {
							CA_CT* cc = ca_space;
							CA_CT* ci = ca_space + ca_space_sz;
							int szsr = keysym - SDLK_0;
							if (szsr == 0)
								szsr = 10;
							szsr *= 64;
							int sz = ca_space_sz / szsr, sg = sz * 8;
							while (cc + sz < ci) {
								int rsz = ceil(log2(sz));
								printf("rsz %d", rsz);
								rsz = rand() % rsz + 2;
								rsz = ipow(2, rsz);
								printf("/%d\n", rsz);
								// random cells
								for (int i = 0; i < rsz; ++i) {
									*cc = rand() % cr.ncs;
									++cc;
								}
								// spacing
									// background cells
								//int bd = rand() % cr.ncs;								// center 
								//for (int ii = 0; ii < sg; ++ii) {
								//	if (cc >= ci)
								//		goto rme;
								//	*cc = bd;
								//	++cc;
								//}
								cc += sg;
							}
						}
						else if (keysym == SDLK_0) {
							CA_CT* cc = ca_space;
							CA_CT* ci = ca_space + ca_space_sz;
							int rg = 10, sg = rg * 10, ct = 100;
							while (1) {
								for (int i = 0; i < ct; ++i) {
									// random cells
									for (int ii = 0; ii < rg; ++ii) {
										if (cc >= ci)
											goto rme;
										*cc = rand() % cr.ncs;
										++cc;
									}
									// background cells
									int bd = rand() % cr.ncs;								// center 
									for (int ii = 0; ii < sg; ++ii) {
										if (cc >= ci)
											goto rme;
										*cc = bd;
										++cc;
									}
								}
								//
								rg *= 2;
								sg *= 2;
								ct = max(1, ct / 2);
							}
						rme:;

							//CA_CT *ca = ca_space;
							//int rg = ca_space_sz;
							//int dy = 32;		// inverse density (1/dy)
							//while (ca + rg < ca_space + ca_space_sz + 1) {
							//	printf("rg %d   ca %d\n", rg, ca - ca_space);
							//	for (CA_CT *cs = ca + rg / dy; ca < cs; ++ca)				// fill 
							//		*ca = rand() % cr.ncs;								// with random cells
							//	int bd = rand() % cr.ncs;								// center 
							//	for (CA_CT *cs = ca + rg / dy; ca < cs; ++ca)			// fill seconde half
							//		*ca = bd;											// with background-state
							//	if (rg < dy)
							//		break;
							//	rg = rg - rg / dy * 2;
							//}
						}
						else {
							for (int i = 0; i < keysym - SDLK_0 + 1; ++i) {
								int pos = rand() << 15 || rand();
								int lh = ipow(2, rand() % 3 + 6);
								printf("rdm  %d - %d\n", pos % ca_space_sz, lh);
								for (int i = 0; i < lh; ++i)
									*(ca_space + (pos + i) % ca_space_sz) = rand() % cr.ncs;
							}
						}
					}
					else
						CA_RandomizeSpace(ca_space, ca_space_sz, cr.ncs, rcc, ps);
					res = 1;
				}
				//
				else switch (keysym) {
				case 0:
					break;
					/* Control Modes */
				case SDLK_r:
					clmd = CLMD_RULE;
					SIMFW_SetFlushMsg(&sfw, "control-mode  rule");
					break;
				case SDLK_F3:
					clmd = CLMD_SCREEN;
					SIMFW_SetFlushMsg(&sfw, "control-mode  screen");
					break;
					/**/
				case SDLK_F11:;
					static int fullscreen = 0;
					if (!fullscreen)
						SDL_SetWindowFullscreen(sfw.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
					else
						SDL_SetWindowFullscreen(sfw.window, 0);
					fullscreen = !fullscreen;
					SIMFW_SetSimSize(&sfw, 0, 0);
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
				case SDLK_ESCAPE:
					clmd = CLMD_NONE;
					SIMFW_SetFlushMsg(&sfw, "control-mode  none");
					break;
				case SDLK_F4:
					goto behind_sdl_loop;
				case SDLK_SPACE:
					CA_RandomizeSpace(ca_space, ca_space_sz, cr.ncs, ca_space_sz / ipow(2, rand() % 10), 3);
					res = 1;
					break;
				case SDLK_PERIOD:
					for (int i = 0; i < ca_space_sz; i++)
						ca_space[i] = 0;
					ca_space[ca_space_sz / 2] = rand() % cr.ncs;
					res = 1;
					break;
				case SDLK_a:
					res = 1;
					break;
				case SDLK_COMMA:
					for (int i = 0; i < ca_space_sz; i++)
						ca_space[i] = 0;
					int lt = cr.ncn * 3 * (1 + lst * rand() % 100);
					for (int i = 0; i < lt; i++)
						ca_space[(ca_space_sz / 2 - lt / 2 + i) % ca_space_sz] = rand() % cr.ncs;
					res = 1;
					break;
				case SDLK_e:
					CA_RandomizeSpace(ca_space, ca_space_sz, cr.ncs, 0, 3);
					int wf = 8;
					CA_RandomizeSpace(ca_space + ca_space_sz / 2 - ca_space_sz / wf / 2, ca_space_sz / wf, cr.ncs, RAND_MAX, 3);
					break;
				case SDLK_t:
					cr.rl = (((UINT64)pcg32_random_r(&pcgr)) << 32 | pcg32_random_r(&pcgr)) % cr.nrls;

					cr = CA_Rule(cr);
					CA_RandomizeSpace(ca_space, ca_space_sz, cr.ncs, ipow(2, rand() % 10), 3);
					char buf[10000];
					CA_RULE_PrintInfo(&cr, &buf, sizeof(buf));
					SIMFW_SetFlushMsg(&sfw, &buf);
					tm = 0;
					res = 1;
					break;
					/* Change space size */
				case SDLK_PLUS:
					if (ctl)
						ds.hlzm = ds.vlzm = min(ds.hlzm, ds.vlzm) + 1,
						fscd = 1;
					else if (sft)
						ds.hlpz = ds.vlpz = min(ds.hlpz, ds.vlpz) + 1,
						fscd = 1;
					else if (alt)
						ca_space_sz = ca_space_sz + 1;
					else
						ca_space_sz = ca_space_sz * 2;
					break;
				case SDLK_MINUS:
					if (ctl)
						ds.hlzm = ds.vlzm = min(ds.hlzm, ds.vlzm) - 1,
						fscd = 1;
					else if (sft)
						ds.hlpz = ds.vlpz = min(ds.hlpz, ds.vlpz) - 1,
						fscd = 1;
					else if (alt)
						ca_space_sz = ca_space_sz - 1;
					else
						ca_space_sz = max(1, ca_space_sz / 2);
					break;
				case SDLK_TAB:;
					int cnt;
					//cnt = (rand() << 9 | rand()) % (ca_space_sz / 64);
					if (sft)
						cnt = pow(2.0, log2(ca_space_sz / 32.0) / RAND_MAX * rand());
					else if (ctl)
						cnt = pow(2.0, rand() % 10);
					else
						cnt = 1024;// a_space_sz / 32;
					//int dy = 1 + rand() % 100;	// density
					int dy = pow(2.0, log2(RAND_MAX) / RAND_MAX * rand());	// density
					if (ctl)
						cnt = cr.ncn + cr.ncn * rand() % 5;
					printf("rdm  ct %d/%.2f%%  dy %.2f%%\n", cnt, 100.0 * cnt / ca_space_sz, 100.0 - 100.0 / RAND_MAX * dy);
					for (int i = 0, p = (rand() << 15 | rand()); i < cnt; ++i)
						if (rand() < dy)
							ca_space[(p + i) % ca_space_sz] = rand() % cr.ncs;
					res = 1;
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
					if (kbms & KMOD_SHIFT)
						speed += 1;
					else
						speed *= 2;
					break;
				case SDLK_PAGEDOWN:
					if (kbms & KMOD_SHIFT)
						speed -= 1;
					else
						speed /= 2;
					break;
				case SDLK_LEFT:
					x_shift--;
					break;
				case SDLK_RIGHT:
					x_shift++;
					break;
				case SDLK_i:
					ds.fsme = !ds.fsme;
					fscd = 1;
					break;
				case SDLK_o:
					if (ctl)
						ds.hlzm++;
					else if (sft)
						ds.hlpz++;
					else
						ds.hlfs++;
					fscd = 1;
					break;
				case SDLK_p:
					if (ctl)
						ds.hlzm--;
					else if (sft)
						ds.hlpz--;
					else
						ds.hlfs--;
					fscd = 1;
					break;
				case SDLK_k:
					if (ctl)
						ds.vlzm++;
					else if (sft)
						ds.vlpz++;
					else
						ds.vlfs++;
					fscd = 1;
					break;
				case SDLK_l:
					if (ctl)
						ds.vlzm--;
					else if (sft)
						ds.vlpz--;
					else
						ds.vlfs--;
					fscd = 1;
					break;
				default:
					SIMFW_HandleKeyBinding(&sfw, e.key.keysym.sym);
				}

				//// random rule for big rule counts (>maxuint64)
				//for (int i = 0; i < cr.ncn; i++)
				//	cr.rltl[i] = rand() % cr.ncs;

				//
				ds.vlzm = max(1, ds.vlzm);
				ds.hlzm = max(1, ds.hlzm);
				ds.vlpz = max(1, ds.vlpz);
				ds.hlpz = max(1, ds.hlpz);
				ds.vlfs = max(0, ds.vlfs);
				ds.hlfs = max(0, ds.hlfs);

				if (fscd)
					res = 1;

				if (ca_space_sz != last_ca_space_sz) {
					printf("ca-space-resize  new %d  old %d\n", ca_space_sz, last_ca_space_sz);
					int nwsz = ca_space_sz;
					CA_CT* nwsc = NULL;									// new space
					nwsc = malloc(nwsz * sizeof * nwsc);					// memory allocation for new space
					for (int i = 0; i < nwsz; ++i)						// init new space to 0
						nwsc[i] = 0;
					tm = 0;
					/* Copy old space into new space - centered */
					memcpy(
						max(nwsc, nwsc + (nwsz - last_ca_space_sz) / 2),
						max(ca_space, ca_space + (last_ca_space_sz - nwsz) / 2),
						min(nwsz, last_ca_space_sz));


					free(ca_space);										// release memory of old space

					speed = speed * 1.0 * nwsz / last_ca_space_sz;

					ca_space_sz = nwsz;									// remember new size
					last_ca_space_sz = ca_space_sz;
					ca_space = nwsc;									// set new space as current
					res = 1;
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

		/* Manipulate simulation canvas pixels */
		UINT32* ll = sfw.sim_canvas;

		/* Scroll */
		speed = max(speed, 0);
		if (speed <= sfw.sim_height) {
			ll = sfw.sim_canvas + (sfw.sim_height * sfw.sim_width) - speed * sfw.sim_height; /* pos to start drawing */
		}

		ds.plw = sfw.sim_width;
		liveanddrawnewcleanzoom(
			&cr,
			ca_space, ca_space_sz,
			speed,
			(res--) > 0,
			sfw.sim_canvas,
			sfw.sim_canvas + sfw.sim_height * sfw.sim_width,
			de, cnmd, klrg, ds);
		//			pixel_effect_moving_gradient(&sfw);
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
			for (int i = 0; i < HCTMXLV; ++i) {
				hc_stats[i].tc = 0;
				hc_stats[i].uc = 0;
				hc_stats[i].ucmn = 0;
				hc_stats[i].ucmx = 0;
			}
			HC_clear_usage_stats(hc_sl, hc_sn);
			HC_nodes_count_usage(hc_sl, hc_sn);
			HC_update_usage_stats(hc_sl, hc_sn);
			HC_print_stats(1);
			printf("cnmd %s  sz %.2e  sd %.2e  cs %.2e  tm %.2e\n", 
				ca_cnsgs[cnmd].name, 
				(double)ca_space_sz, 
				(double)speed,
				(double)speed * ca_space_sz / sfw.avg_duration,
				(double)tm * ca_space_sz);
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
					v = (lv - ltmn) / rg * 0xFF * 10;
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
						v = (lv - ltmn) / rg * 0xFF * 10;
						*px = v << 16 | v << 8 | v;
					}
					++cc,
						++px,
						++cfv;
				}
				else if (MODE == 3) {
					// very simple diffusion
					double dif;
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
					v = (lv - ltmn) / rg * 0xFF * 4;
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
		double mn, mx;
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
				bw = 255 * (dwnr - dwnr * sl * (log(ans[i]) - lgmn));
				bw = 255 - bw;
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
			"SIMFW TEST   fps %.2f  sd %.2f\nmn %.2e/%.2e  mx %.2e/%.2e    moude %.2e/%.2e\n%.4f",
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




















