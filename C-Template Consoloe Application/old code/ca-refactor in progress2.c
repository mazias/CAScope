// rule 1507 und 1146 in stzm 1 schön

#include "stdafx.h"		// default includes (precompiled headers)
#include "simfw.h"
#include "color.h"
#include "float.h"
#include "hp_timer.h"

#include "math.h"
#define M_E        2.71828182845904523536   // e


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
//#define SIMFW_TEST_DSP_WIDTH	1024
//#define SIMFW_TEST_DSP_HEIGHT	750
//#define SIMFW_TEST_DSP_WINDOW_FLAGS	SDL_WINDOW_FULLSCREEN_DESKTOP
#define SIMFW_TEST_DSP_WINDOW_FLAGS	0

//#define SIMFW_TEST_SIM_WIDTH	70
//#define SIMFW_TEST_SIM_HEIGHT	700
#define SIMFW_TEST_FPS_LIMIT	100
//


//
#define SIMFW_SDL_PIXELFORMAT		SDL_PIXELFORMAT_ARGB8888












// returns 0-terminated string of content of filename
char * read_file(const char * filename) {
	printf("INF read file  %s", filename);
	// Get the current working directory:   
	char* cwdbuf[1024];
	if (_getcwd(cwdbuf, sizeof(cwdbuf)) == NULL)
		perror("_getcwd error\n");
	else {
		printf("\tsearch path  %s\n", cwdbuf);
	}

	char * buffer = 0;
	long length;
	FILE * f = NULL;
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
// ***************************************************************************************************************************************************************


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



typedef cl_uchar CA_CT; /* memory-type of a cell */
						//typedef UINT8 CA_CT; /* memory-type of a cell */
						//typedef int CA_CT; /* memory-type of a cell */

typedef struct
CA_RULE {
	UINT64	rl;		/* rule */
	int ncs;		/* number of cell-states */
	int ncn;		/* number of cells in a neighborhod */
	int nns;		/* number of neighborhod-states */
	UINT64 nrls;	/* number of rules */
	CA_CT *rltl;	/* rule table - maps neighborhod-state of present generation to cell-state of future generation */
	CA_CT *mntl;	/* multiplication table - all 1 for totalistic, xx for absolute TODO */
	enum { ABS, TOT } tp;	/* type - absolute or totalistic*/
} CA_RULE;

const CA_RULE CA_RULE_EMPTY = { 0, 0, 0, 0, 0, 0, 0, 0 };




// **********************************************************************************************************************************
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
} OCLCA;
// **********************************************************************************************************************************
// OCL Functions ********************************************************************************************************************
// **********************************************************************************************************************************
// returns valid opencl kernel object
#define CL_MAX_PLATFORM_COUNT (4)
#define CL_MAX_DEVICE_COUNT (4)
OCLCA
OCLCA_Init(
	const char *cl_filename,
	const char *cl_kernelname
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
	cl_program program = clCreateProgramWithSource(context, 1, (const char*[]) { program_code }, NULL, &cl_errcode_ret);
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
	result.kernel_ca_next_gen = clCreateKernel(program, "ca_next_gen", &cl_errcode_ret);
	OCL_CHECK_ERROR(cl_errcode_ret)
		result.kernel_ca_next_gen_tot_ncn3 = clCreateKernel(program, "ca_next_gen_tot_ncn3", &cl_errcode_ret);
	OCL_CHECK_ERROR(cl_errcode_ret)
		result.kernel_ca_next_gen_abs_ncn3_ncs2 = clCreateKernel(program, "ca_next_gen_abs_ncn3_ncs2", &cl_errcode_ret);
	OCL_CHECK_ERROR(cl_errcode_ret)
		result.kernel_ca_next_gen_range_tot_ncn3 = clCreateKernel(program, "ca_next_gen_range_tot_ncn3", &cl_errcode_ret);
	OCL_CHECK_ERROR(cl_errcode_ret)
		// get some kernel info
	{
		cl_kernel kernel = result.kernel_ca_next_gen_tot_ncn3;
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
	CA_RULE *cr,
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
				printf("ng %d  ws %d  gc %d  gs %d  gn %d  gr %d  gr*gn*gc %d  max ws %d\n", ng, ws, gc, gs, gn, gr, gr*gn*gc, scsz + brsz);
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
sprintfa_s(char *sg, size_t sgsz, const char *format, ...) {
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
CA_RULE_PrintInfo(CA_RULE *cr, char *os, size_t sgsz) {
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
	char *tnst[500];
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
void MortonDecode2(uint32_t code, uint32_t *outX, uint32_t *outY)
{
	*outX = _pext_u32(code, 0x55555555);
	*outY = _pext_u32(code, 0xAAAAAAAA);
}

__inline
void MortonDecode3(uint32_t code, uint32_t *outX, uint32_t *outY, uint32_t *outZ)
{
	*outX = _pext_u32(code, 0x09249249);
	*outY = _pext_u32(code, 0x12492492);
	*outZ = _pext_u32(code, 0x24924924);
}

//rotate/flip a quadrant appropriately
__inline
void ht_rot(int n, int *x, int *y, int rx, int ry) {
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
	for (s = n / 2; s>0; s /= 2) {
		rx = (x & s) > 0;
		ry = (y & s) > 0;
		d += s * s * ((3 * rx) ^ ry);
		ht_rot(s, &x, &y, rx, ry);
	}
	return d;
}

//convert d to (x,y)
__inline
void d2xy(int n, int d, int *x, int *y) {
	int rx, ry, s, t = d;
	*x = *y = 0;
	for (s = 1; s<n; s *= 2) {
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
	// force rule to lie within range of number of rules
	prl.rl %= prl.nrls;
	/* Build multiplication table */
	//free(prl.mntl);
	prl.mntl = malloc(prl.ncn * sizeof *prl.mntl);
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
	prl.rltl = malloc(prl.nns * sizeof *prl.rltl);
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


/*
CA_CT *rltl,		// rule table
CA_CT *ca_space,	// space
int ca_space_sz,	// space-size
int ca_time			// time - nr. of steps/generations to live
*/
void
CA_Live(
	CA_CT *rltl,		/* rule table */
	CA_CT *spc,			/* space */
	int spcsz,			/* space-size */
	int tm				/* time - nr. of steps/generations to live */
	) {
	int i;
	register int t, tp; /* buffers for cell values - these should be the native int size even if CA_CT is of different (smaller) type- reason is that conversion seems to cost a lot of performance */
	register CA_CT
		*ca_space_ps, // current cell
		*lstps = spc + spcsz - 1; /* last position */

	for (i = 0; i < tm; i++) {
		spc[0] = spc[spcsz - 2]; /* right boundary */
		spc[spcsz - 1] = spc[1]; /* left boundary */

		t = spc[0];
		for (ca_space_ps = spc + 1; ca_space_ps < lstps; ca_space_ps++) {
			tp = *ca_space_ps;

			// absolute
			//*ca_space_ps = rltl[ca_space_ps[1] + 2 * tp + 4 * t];
			// totalistic
			*ca_space_ps = rltl[ca_space_ps[1] + tp + t];

			t = tp;
		}
	}
}


void
CA_PrintSpace(char **caption, CA_CT *spc, int sz) {
	printf("print space  %s", caption);
	for (int i = 0; i < sz; i++) {
		printf("%d", spc[i]);
	}
	printf("\n");
}





/*
v - value to make color for - has to be between 0 and 1
huem, satm, valm - hue-, sat- and val-modes:
*/
UINT32
getColorEx(double v, int huem, int satm, int lumm, double huef, double satf, double lumf) {
	double hue, sat, lum;
	hue = sat = lum = v;
	/* lum mode */
	if (lumm == 0)
		lum = 0.5;
	else {
		// TODO replace lum,hue,sat with UINT compatible
		lum = modf(lum * lumf, &lum); // modf seems to be expenisve
									  //lum *= lumf;

									  // anti-alias border between zones; aazb = 0.5 is the same as mirror mode
									  /*
									  const double aazb = 0.95; // anti-alias zone begin; >= 0, < 1
									  if (lum >= aazb)
									  lum = 1.0 - (lum - aazb)  * 1.0 / (1.0 - aazb);
									  else
									  lum *= 1.0 / aazb; // aazb can not be zero here, since lum >= 0 is always true
									  */
		if (lumm == 3 || lumm == 4)
			if (lum >= 0.5)
				lum = 2.0 - 2.0 * lum;
			else
				lum *= 2.0;
		if (lumm == 2 || lumm == 4)
			lum = 1.0 - lum;
	}
	/* sat mode */
	if (satm == 0) {
		sat = 0.0;
///		huem = 0;
	}
	else if (satm == 1)
		sat = 1.0;
	else {
		sat = modf(sat * satf, &sat);
		if (satm == 4 || satm == 5)
			if (sat < 0.5)
				sat *= 2.0;
			else
				sat = 2.0 - 2.0 * sat;
		if (satm == 3 || satm == 5)
			sat = 1.0 - sat;
	}
	/* hue mode */
	if (huem == 0)
		hue = 0.0;
	else if (huem == 1)
		hue = 4.0 / 6.0;
	else {
		hue = modf(hue * huef, &hue);
		if (huem == 4 || huem == 5)
			if (hue < 0.5)
				hue *= 2.0;
			else
				hue = 2.0 - 2.0 * hue;
		if (huem == 3 || huem == 5)
			hue = 1.0 - hue;
	}

	return HSL_to_RGB_16b_2(hue * 0xFFFF, sat * 0xFFFF, lum * 0xFFFF);
}



/*
v - value to make color for - has to be between 0 and 1
cm - color mode
crct - color count
gm - gradient mode
*/
UINT32
getColor(double v, int cm, int crct, int gm /* gradient mode */) {
	cm %= 4;

	// gradient-mode mirror
	if (gm == 2 || gm == 3)
		if (v >= 0.5)
			v = 2.0 - 2.0 * v;
		else
			v *= 2.0;
	// gradient-mode reverse
	if (gm == 1 || gm == 3)
		v = 1.0 - v;

	if (crct > 0)
		v = (double)((int)(v * (crct)+0.5)) / (crct);

	double l = 0.5, s = 1.0;

	//if (v < 0.5)
	//	v *= (0.5 - v) * 2.0;
	//else
	//	v = (v - 0.5) * 2.0;

	//if (v < 0.5)
	//	v *= v * 2.0;
	//else
	//	v = (1.0 - v) * 2.0;

	/* log mode */
	//if (0) {
	//	l = 1.0 - 1.0 / log(max - min) * log(sctbl[i] - min + 1.0);
	//	l = 1.0 - 1.0 / (log(max) - log(min)) * (log(sctbl[i]) - log(min));

	//	l = 1.0 - 1.0 / (max - min) * (sctbl[i] - min);
	//	l -= 1.0 - 1.0 / max * sctbl[i];

	//	v = l;
	//	s = 0.0;
	//}

	/* grayscale */
	if (cm == 1) {
		UINT32 gv = 255.0 * v;
		return gv << 16 | gv << 8 | gv;
	}
	/* b&w */
	else if (cm == 0) {
		if (v > 0.5)
			return 0xFFFFFF;
		else
			return 0x000000;
	}
	/* color only */
	else if (cm == 2) {
		return HSL_to_RGB_16b_2(
			0xFFFF * v,
			0xFFFF,
			0x7FFF);
	}
	/* bw & color */
	else {
		const double sh = 1.0 / 8.0;
		if (v <= sh) {
			l = 0.5 - 0.5 / sh * (sh - v);
			l = 0.5 / sh * v;
			s = 1.0 / sh / sh * v * v;
			v = 0.0;
		}
		else if (v < 1.0 - sh)
			v = (v - sh) / (1.0 - 2.0 * sh);
		else {
			l = 0.5 + 0.5 / sh * (v - 1.0 + sh);
			l = 1.0 - 0.5 / sh * (1.0 - v);
			s = 1.0 / sh / sh * (1.0 - v) * (1.0 - v);
			v = 1.0;
		}

		const double vs = 0.0;// 0.5;		// v-shift
		const double mr = 0.85;// 0.5;		// max-range - use 0.85 to avoid having red two times / stop at lila
		v = vs + v * mr;

		return HSL_to_RGB_16b_2(
			0xFFFF * v,
			0xFFFF * s,
			0xFFFF * l);
		//l = v;
		//if (v < 0.5)
		//	s = v * 2.0;
		//else
		//	s = 1.0 - (v - 0.5) * 2.0;


		//cltbl[i] = flp_ConvertHSLtoRGB(
		//	v,
		//	s,
		//	l);
	}
}

/* Type for signed 32bit integer array */
typedef struct
S32IRS {
	int32_t	ct;		/* count of elements */
	int32_t	*es;	/* elements */
} S32IRS;

/* Initializes S32IRS struct, ct must be valid count, es must be null or allocated mem */
void
init_S32IRS(S32IRS *p) {
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
print_S32IRS(S32IRS *p, int32_t mx) {
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
	UINT8 *bts;
	bts = _aligned_malloc(SZ + 2, 4096);
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
	int32_t *irs; // integers
	irs = malloc((irsct + 2) * sizeof(*irs));
	// Init
	int32_t *i = irs;
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
			int32_t *ip = irs, *ib = irs + irsct;
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


void
livenew(
	/*
	CA_CT *rltl,		// rule table
	CA_CT *ca_space,	// space
	int ca_space_sz,	// space-size
	int ca_time			// time - nr. of steps/generations to live
	*/
	CA_RULE const *cr,		/* ca configuration */
	int const scsz,			/* space-size */
	int const brsz, // buffer size
	int tm,				/* time - nr. cells (tm=width=line=1=full generation) to live */
						//int *sp, // start position
	int *spps, // stop position
	CA_CT *cs,  // current space - must be properly initialized
	CA_CT **pccs, // current cell in space
	CA_CT **pckpn // check position
	) {
	if (!tm)
		return;

	// current cell in space - convert so we dont have to deal with pointers of pointer. needs to be saved back at the end
	register CA_CT *ccs = *pccs;
	register CA_CT *ckpn = *pckpn;
	///CA_CT *wbpn = *pckpn;
	/* memory size of one cell */
	const int clsz = sizeof(CA_CT);
	const int ls = cr->ncn / 2; // left shift
	const int wbpn = scsz + brsz; /* Wrap buffer position */

	CA_CT rltl[10];
	//rltl = malloc(10);
	for (int i = 0; i < cr->nns; i++)
		rltl[i] = cr->rltl[i];
	//CA_CT *rltl = cr->rltl;

life:
	/* Check if loop-unroll is possible */
	if ((ckpn - ccs) % 32 == 0)	goto life_loop_ic256u8;
	//if ((ckpn - ccs) % 8 == 0)	goto life_loop_ic256u32;
	//if ((ckpn - ccs) % 4 == 0)	goto life_loop_tbi_1x32b;
	//if ((ckpn - ccs) % 16 == 0)	goto life_loop_unroll;
	/**/
	if (ccs == ckpn)
		goto life_check_position;
	/* Next generation */
	*ccs = rltl[ccs[0] + ccs[1] + ccs[2]];
	++ccs;
	goto life;
	/* Version using intrisic 256bit=8 int32 SIMD , needs bytes remaining % (mod) 32 = 0 */
	{	life_loop_ic256u32:
	if (ccs == ckpn)
		goto life_check_position;
	register __m256i t;
	t = _mm256_add_epi32(*(__m256i*)(ccs + 0), *(__m256i*)(ccs + 1));
	t = _mm256_add_epi32(t, *(__m256i*)(ccs + 2));

	//*(__m256i*)(ccs + 0) = _mm256_i32gather_epi32(&rltl, t, sizeof(CA_CT));

	t = _mm256_i32gather_epi32(&rltl, t, 4);
	_mm256_storeu_si256((__m256i*)(ccs + 0), t);

	//for (register int i = 0; i < 8; i++) {
	//	//ccs[i] = rltl[ccs[i] + ccs[i + 1] + ccs[i + 2]];
	//	//ccs[i] = rltl[t.m256i_i32[i]];
	//	ccs[i] = t.m256i_i32[i];
	//}
	ccs += 8;
	goto life_loop_ic256u32;
	}
	{
		/* Version using intrisic 256bit=32 uint8 SIMD , needs bytes remaining % (mod) 32 = 0 */
	life_loop_ic256u8:
		if (ccs == ckpn)
			goto life_check_position;
		register __m256i t;// = { 0 };

		t = _mm256_loadu_si256((__m256i*)(ccs + 0));
		t = _mm256_adds_epu8(t, *(__m256i*)(ccs + 1));
		t = _mm256_adds_epu8(t, *(__m256i*)(ccs + 2));
		//_mm256_zeroall();
		//t = _mm256_adds_epu8(t, *(__m256i*)(ccs + 0));
		//t = _mm256_adds_epu8(t, *(__m256i*)(ccs + 1));
		//t = _mm256_adds_epu8(t, *(__m256i*)(ccs + 2));
		////*(__m256i*)(ccs + 0) = _mm256_adds_epu8(*(__m256i*)(ccs + 0), *(__m256i*)(ccs + 1));
		////*(__m256i*)(ccs + 0) = _mm256_adds_epu8(*(__m256i*)(ccs + 0), *(__m256i*)(ccs + 2));
		////ccs[i] = rltl[ccs[i]];

		for (register int i = 0; i < 32; ++i) {
			//t.m256i_u8[i] = rltl[t.m256i_u8[i]];
			ccs[i] = rltl[t.m256i_u8[i]];
			//*ccs = rltl[t.m256i_u8[i]]; ++ccs;
			//// much slower!
			////switch (t.m256i_u8[i]) {
			////case 0: t.m256i_u8[i] = 0; break; case 5: t.m256i_u8[i] = 0; break; case 6: t.m256i_u8[i] = 0; break;
			////case 1: case 2: t.m256i_u8[i] = 1; break;
			////case 3: case 4: t.m256i_u8[i] = 2; break;		
			////}
		}
		//_mm256_storeu_si256(ccs, t);
		ccs += 32;

		////t.m256i_u8[0] = rltl[t.m256i_u8[0]];  	t.m256i_u8[1] = rltl[t.m256i_u8[1]];  
		////t.m256i_u8[2] = rltl[t.m256i_u8[2]];  	t.m256i_u8[3] = rltl[t.m256i_u8[3]];  
		////t.m256i_u8[4] = rltl[t.m256i_u8[4]];  	t.m256i_u8[5] = rltl[t.m256i_u8[5]];  
		////t.m256i_u8[6] = rltl[t.m256i_u8[6]];  	t.m256i_u8[7] = rltl[t.m256i_u8[7]];  
		////t.m256i_u8[8] = rltl[t.m256i_u8[8]];  	t.m256i_u8[9] = rltl[t.m256i_u8[9]];  
		////t.m256i_u8[10] = rltl[t.m256i_u8[10]];	t.m256i_u8[11] = rltl[t.m256i_u8[11]];
		////t.m256i_u8[12] = rltl[t.m256i_u8[12]];	t.m256i_u8[13] = rltl[t.m256i_u8[13]];
		////t.m256i_u8[14] = rltl[t.m256i_u8[14]];	t.m256i_u8[15] = rltl[t.m256i_u8[15]];
		////t.m256i_u8[16] = rltl[t.m256i_u8[16]];	t.m256i_u8[17] = rltl[t.m256i_u8[17]];
		////t.m256i_u8[18] = rltl[t.m256i_u8[18]];	t.m256i_u8[19] = rltl[t.m256i_u8[19]];
		////t.m256i_u8[20] = rltl[t.m256i_u8[20]];	t.m256i_u8[21] = rltl[t.m256i_u8[21]];
		////t.m256i_u8[22] = rltl[t.m256i_u8[22]];	t.m256i_u8[23] = rltl[t.m256i_u8[23]];
		////t.m256i_u8[24] = rltl[t.m256i_u8[24]];	t.m256i_u8[25] = rltl[t.m256i_u8[25]];
		////t.m256i_u8[26] = rltl[t.m256i_u8[26]];	t.m256i_u8[27] = rltl[t.m256i_u8[27]];
		////t.m256i_u8[28] = rltl[t.m256i_u8[28]];	t.m256i_u8[29] = rltl[t.m256i_u8[29]];
		////t.m256i_u8[30] = rltl[t.m256i_u8[30]];	t.m256i_u8[31] = rltl[t.m256i_u8[31]];
		////_mm256_storeu_si256(ccs, t);
		////ccs += 32;
		goto life_loop_ic256u8;
	}
life_loop_tbi_64b: // test bigger int addition to parallel add smaller ints
	if (ccs == ckpn)
		goto life_check_position;
	// ccs needs to be unsigned uint8 for this to work!
	*(UINT64*)(ccs + 0) = *(UINT64*)(ccs + 0) + *(UINT64*)(ccs + 1) + *(UINT64*)(ccs + 2);// = *(int*)bts[0] + *(int*)bts[1] + *(int*)bts[2];
	*(UINT64*)(ccs + 8) = *(UINT64*)(ccs + 8) + *(UINT64*)(ccs + 9) + *(UINT64*)(ccs + 10);// = *(int*)bts[0] + *(int*)bts[1] + *(int*)bts[2];
	ccs[0] = rltl[ccs[0]];	ccs[1] = rltl[ccs[1]];
	ccs[2] = rltl[ccs[2]];	ccs[3] = rltl[ccs[3]];
	ccs[4] = rltl[ccs[4]];	ccs[5] = rltl[ccs[5]];
	ccs[6] = rltl[ccs[6]];	ccs[7] = rltl[ccs[7]];
	ccs[8] = rltl[ccs[8]];	ccs[9] = rltl[ccs[9]];
	ccs[10] = rltl[ccs[10]]; ccs[11] = rltl[ccs[11]];
	ccs[12] = rltl[ccs[12]]; ccs[13] = rltl[ccs[13]];
	ccs[14] = rltl[ccs[14]]; ccs[15] = rltl[ccs[15]];
	ccs += 16;
	goto life_loop_tbi_64b;
life_loop_tbi_1x32b: // test bigger int addition to parallel add smaller ints
	if (ccs == ckpn)
		goto life_check_position;
	// ccs needs to be unsigned uint8 for this to work!
	*(UINT32*)(void*)(ccs + 0) = *((UINT32*)(void*)(ccs + 0)) + *((UINT32*)(void*)(ccs + 1)) + *((UINT32*)(void*)(ccs + 2));// = *(int*)bts[0] + *(int*)bts[1] + *(int*)bts[2];
	ccs[0] = rltl[ccs[0]];
	ccs[1] = rltl[ccs[1]];
	ccs[2] = rltl[ccs[2]];
	ccs[3] = rltl[ccs[3]];
	ccs += 4;
	goto life_loop_tbi_1x32b;
life_loop_tbi_4x32b: // test bigger int addition to parallel add smaller ints
	if (ccs == ckpn)
		goto life_check_position;
	// ccs needs to be unsigned uint8 for this to work!
	*(UINT32*)(void*)(ccs + 0) = *((UINT32*)(void*)(ccs + 0)) + *((UINT32*)(void*)(ccs + 1)) + *((UINT32*)(void*)(ccs + 2));// = *(int*)bts[0] + *(int*)bts[1] + *(int*)bts[2];
	*(UINT32*)(void*)(ccs + 4) = *((UINT32*)(void*)(ccs + 4)) + *((UINT32*)(void*)(ccs + 5)) + *((UINT32*)(void*)(ccs + 6));// = *(int*)bts[0] + *(int*)bts[1] + *(int*)bts[2];
	*(UINT32*)(void*)(ccs + 8) = *((UINT32*)(void*)(ccs + 8)) + *((UINT32*)(void*)(ccs + 9)) + *((UINT32*)(void*)(ccs + 10));// = *(int*)bts[0] + *(int*)bts[1] + *(int*)bts[2];
	*(UINT32*)(void*)(ccs + 12) = *((UINT32*)(void*)(ccs + 12)) + *((UINT32*)(void*)(ccs + 13)) + *((UINT32*)(void*)(ccs + 14));// = *(int*)bts[0] + *(int*)bts[1] + *(int*)bts[2];
	ccs[0] = rltl[ccs[0]]; ccs[1] = rltl[ccs[1]];
	ccs[2] = rltl[ccs[2]];	ccs[3] = rltl[ccs[3]];
	ccs[4] = rltl[ccs[4]];	ccs[5] = rltl[ccs[5]];
	ccs[6] = rltl[ccs[6]];	ccs[7] = rltl[ccs[7]];
	ccs[8] = rltl[ccs[8]];	ccs[9] = rltl[ccs[9]];
	ccs[10] = rltl[ccs[10]]; ccs[11] = rltl[ccs[11]];
	ccs[12] = rltl[ccs[12]]; ccs[13] = rltl[ccs[13]];
	ccs[14] = rltl[ccs[14]]; ccs[15] = rltl[ccs[15]];
	ccs += 16;
	goto life_loop_tbi_4x32b;
life_loop_unroll:
	if (ccs == ckpn)
		goto life_check_position;
	ccs[0] = rltl[ccs[0] + ccs[1] + ccs[2]]; ccs[1] = rltl[ccs[1] + ccs[2] + ccs[3]];
	ccs[2] = rltl[ccs[2] + ccs[3] + ccs[4]]; ccs[3] = rltl[ccs[3] + ccs[4] + ccs[5]];
	ccs[4] = rltl[ccs[4] + ccs[5] + ccs[6]]; ccs[5] = rltl[ccs[5] + ccs[6] + ccs[7]];
	ccs[6] = rltl[ccs[6] + ccs[7] + ccs[8]]; ccs[7] = rltl[ccs[7] + ccs[8] + ccs[9]];
	ccs[8] = rltl[ccs[8] + ccs[9] + ccs[10]]; ccs[9] = rltl[ccs[9] + ccs[10] + ccs[11]];
	ccs[10] = rltl[ccs[10] + ccs[11] + ccs[12]]; ccs[11] = rltl[ccs[11] + ccs[12] + ccs[13]];
	ccs[12] = rltl[ccs[12] + ccs[13] + ccs[14]]; ccs[13] = rltl[ccs[13] + ccs[14] + ccs[15]];
	ccs[14] = rltl[ccs[14] + ccs[15] + ccs[16]]; ccs[15] = rltl[ccs[15] + ccs[16] + ccs[17]];
	ccs += 16;
	goto life_loop_unroll;
life_check_position:
	/* Simulation time reached */
	if (!tm)
		goto life_finished;
	/* Wrap arround on right end */
	if (ccs == cs + wbpn)
		ccs = cs;
	/* Stop position reached > refill buffers, count states, create color table */
	if (ccs == cs + *spps) {
		/* Copy cells to joint buffer (where left- and rightmost cell meet) */
		for (int i = 0; i < brsz; i++)
			if (i < brsz - ls)
				cs[(*spps + i) % (scsz + brsz)] = cs[(*spps + i + brsz) % (scsz + brsz)];
			else
				cs[(*spps + i) % (scsz + brsz)] = cs[(*spps + i + scsz) % (scsz + brsz)];
		/* Copy cells of wrap behind (last cell) buffer */
		for (int i = 0; i < brsz; i++)
			cs[scsz + brsz + i] = cs[i]; // wrap arround buhlfspace
										 /* Move start and stop positions according to ls (left shift)  */
										 //*sp = (*spps + brsz - ls) % (scsz + brsz);
										 //ccs = cs + *sp;
		ccs = cs + (ccs - cs + brsz - ls) % (scsz + brsz);
		*spps = (*spps + scsz + brsz - ls) % (scsz + brsz);
	}
	/* Get next position where a check will be necesary */
	// the wrap buffer is the highest possible stop position
	ckpn = cs + wbpn;
	// the joint buffer can be between the current position and the wrap buffer
	if (ccs < cs + *spps)
		ckpn = cs + *spps;
	// the remaining time (cells to life) may be smaller than the amount of cells that would life until the check position is be reached
	if (tm < ckpn - ccs)
		ckpn = ccs + tm;
	// adjust remaining time (see comment above)
	tm -= ckpn - ccs;
	goto life;


life_finished:

	//
	// write back pccs since: CA_CT *ccs = *pccs;
	*pccs = ccs;
	*pckpn = ckpn;
}


void
liveanddrawnew(
	/*
	CA_CT *rltl,				// rule table
	CA_CT *ca_space,			// space
	int ca_space_sz,			// space-size
	int ca_time					// time - nr. of steps/generations to live
	*/
	CA_RULE *cr,				/* ca configuration */
	CA_CT *sc,					/* space */
	const int scsz,				/* space-size */
	const int zoom,
	int tm,						/* time - nr. cells (tm=width=line=1=full generation) to live */
								//int ncr,					/* number of cells needed to calculate next generation (rule table lookup) */
	int ncd,					/* number of cells for drawing */
	UINT32 *ppx,				/* rgba pixel array to draw to */
	int res,					/* reset-flag (because of the static vars)*/
	const UINT32 *pxbrd,
	const UINT32 *pxfrst,
	const unsigned int pitch,
	const int hlfs,				/* focus > nr. of hlfs cells next to eachother are summed... TODO */
	const int hlfssr,				/* = 1 > focus is done on same row; 0 > on next row */
	const unsigned int ar,		/* auto-range - 0: none, 1: auto min, 2: auto max, 3: auto min and max */
	const unsigned int cm,		/* color mode */
	const unsigned int crct,	/* color count */
	const unsigned int gm		/* gradient mode */
	) {
	register UINT32 *px = ppx;	/* rgba pixel array to draw to */

								//tm = tm / scsz * scsz;

	int ltm = 0;

	if (!tm)
		return;
	/* memory size of one cell */
	const int clsz = sizeof(CA_CT);
	/* size of one buffer (we will need two of this size: joint and wrap buffer) */
	const int brsz = max(hlfs - 1, cr->ncn - 1);

	static int sp;					// start position
	static int spps;				// stop position
	static CA_CT *sc_ckpn;			// check position	
	static CA_CT *sc_ccs = NULL;	// current position
	const int wbpn = scsz + brsz;	// wrap buffer position

	register CA_CT *ccs, *ckpn;
	ccs = sc_ccs, ckpn = sc_ckpn;

	/* current space */
	static CA_CT *cs = NULL;
	if (!cs | res) {
		//free(cs);
		//cs = malloc((scsz + brsz + brsz) * clsz);
		cs = _aligned_malloc((scsz + brsz + brsz) * clsz, 4096);
		sp = spps = scsz;
		ckpn = ccs = cs + spps;
		memcpy(cs, sc, scsz * clsz);
	}

	/* color table and state count table */
	int dyss = cr->ncs * max(1, hlfs);
	////static UINT32 *cltbl = NULL;
	static UINT32 cltbl[132];
	if (!cltbl | res) {
		//free(cltbl);
		////cltbl = malloc(dyss * sizeof *cltbl);
	}
	// focus = 0 > static color table, otherwise automatically initialized below
	if (hlfs == 0)
		for (int i = 0; i < dyss; i++)
			cltbl[i] = getColor(1.0 / (dyss - 1) * i, cm, crct, gm);
	////static int *sctbl = NULL;	// state count table
	static uint32_t sctbl[132];	// state count table
	if (!sctbl | res) {
		//free(sctbl);
		////sctbl = malloc(dyss * sizeof *sctbl);
		for (int i = 0; i < dyss; i++)
			sctbl[i] = 0;
	}

	/* copy rule tbl to local memory to improve speed */
	CA_CT rltl[32];
	for (int i = 0; i < cr->nns; i++)
		rltl[i] = cr->rltl[i];

	// HARDCODE MAX SPEED to screen width * height
	//////tm = min(tm, pxbrd - px);
	/* special routine for calculating portions not displayed on the screen */
	//// livenew only works for special cases i. e. 3-neighbors
	//if (tm > pxbrd - px) {
	//	//livenew(cr, scsz, brsz, tm, &sp, &spps, cs, &ccs, &ckpn);
	//	//return;
	//	sc_ccs = ccs, sc_ckpn = ckpn;
	//	livenew(cr, scsz, brsz, tm - (pxbrd - px), &sp, &spps, cs, &sc_ccs, &sc_ckpn);
	//	ccs = sc_ccs, ckpn = sc_ckpn;
	//	tm = pxbrd - px;
	//}

	const int ls = cr->ncn / 2; // left shift

	int min, max;

	/* Test: complete rule table in integer register */
	register uint32_t irt = 0x00000294;	// 00 00 10 10 01 01 00  rule table of 228 harcoded

life:
	////if ((ckpn - ccs) % 32 == 0)	goto life_loop_ic256u8;
	if (ccs == ckpn)
		goto life_check_position;
	/* Draw - fast variant - hardcoded for totalistic 4 neighbor sharpening */
	/**px = cltbl[ccs[0] + ccs[1] + ccs[2] + ccs[3]];
	++px;*/
	/* Draw - flexible variant - can handle totalisitc sharpening with up to ??? neighbors */
	if (px < pxbrd) {
		switch (hlfs) {
		case 4: *px = cltbl[ccs[0] + ccs[1] + ccs[2] + ccs[3]]; break;
		case 0:
		case 1: *px = cltbl[ccs[0]]; break;
		case 2: *px = cltbl[ccs[0] + ccs[1]]; break;
		case 3: *px = cltbl[ccs[0] + ccs[1] + ccs[2]]; break;
		case 5: *px = cltbl[ccs[0] + ccs[1] + ccs[2] + ccs[3] + ccs[4]]; break;
		default:;
			int ctpn = 0; // color table position
			for (int i = 0; i < hlfs; i++)
				ctpn += ccs[i];
			*px = cltbl[ctpn];
		}
		++px;
	}
	/* Next generation - test for rule table in integer -- works but is not faster */
	/*if (rltl[ccs[0] + ccs[1] + ccs[2]] != ((irt >> ((ccs[0] + ccs[1] + ccs[2]) * 2)) & 0x3))
	printf(
	"%d  %x  %d\n",
	rltl[ccs[0] + ccs[1] + ccs[2]],
	(irt >> ((ccs[0] + ccs[1] + ccs[2]) * 2)),
	(irt >> ((ccs[0] + ccs[1] + ccs[2]) * 2)) & 0x3
	);
	*ccs = (irt >> ((ccs[0] + ccs[1] + ccs[2]) * 2)) & 0x3;
	++ccs;*/
	/* Next generation - fast variant - hardcoded for totalistic 3 neighbors */
	/**ccs = rltl[ccs[0] + ccs[1] + ccs[2]];
	++ccs;*/
	/* Next generation - flexible variant - can handle totalistic and absolute with at least 3 neighbors */
	switch (cr->ncn) {
	case 3: *ccs = cr->rltl[cr->mntl[0] * ccs[0] + cr->mntl[1] * ccs[1] +
		cr->mntl[2] * ccs[2]]; break;
	case 4: *ccs = cr->rltl[cr->mntl[0] * ccs[0] + cr->mntl[1] * ccs[1] +
		cr->mntl[2] * ccs[2] + cr->mntl[3] * ccs[3]]; break;
	case 5: *ccs = cr->rltl[cr->mntl[0] * ccs[0] + cr->mntl[1] * ccs[1] +
		cr->mntl[2] * ccs[2] + cr->mntl[3] * ccs[3] + cr->mntl[4] * ccs[4]]; break;
	default:;
		int rtpn = 0; // rule table position
		for (int i = 0; i < cr->ncn; i++)
			rtpn += cr->mntl[i] * ccs[i];
		*ccs = cr->rltl[rtpn];
	}
	//
	++ccs;
	goto life;
life_loop_ic256u8:
	// version using intrisic 256bit=32 uint8 SIMD , needs bytes remaining % (mod) 32 = 0
	{
		if (ccs == ckpn)
			goto life_check_position;
		register __m256i t3, t4;// = _mm256_setzero_si256(), t4;
		t3 = _mm256_loadu_si256((__m256i*)(ccs + 0));// _mm256_adds_epu8(t3, *(__m256i*)(ccs + 0));
													 //t3 = _mm256_adds_epu8(t3, *(__m256i*)(ccs + 0));
		t3 = _mm256_adds_epu8(t3, *(__m256i*)(ccs + 1));
		t3 = _mm256_adds_epu8(t3, *(__m256i*)(ccs + 2));
		t4 = _mm256_adds_epu8(t3, *(__m256i*)(ccs + 3));

		for (register int i = 0; i < 32; ++i)
			px[i] = cltbl[t4.m256i_u8[i]];
		for (register int i = 0; i < 32; ++i)
			ccs[i] = rltl[t3.m256i_u8[i]];


		ccs += 32;
		px += 32;

		goto life_loop_ic256u8;
	}
life_loop_unroll:
	// description ....
	{
		if (ccs == ckpn)
			goto life_check_position;
		*px = cltbl[ccs[0] + ccs[1] + ccs[2] + ccs[3]]; *ccs = rltl[ccs[0] + ccs[1] + ccs[2]]; ++ccs; ++px;
		*px = cltbl[ccs[0] + ccs[1] + ccs[2] + ccs[3]]; *ccs = rltl[ccs[0] + ccs[1] + ccs[2]]; ++ccs; ++px;
		*px = cltbl[ccs[0] + ccs[1] + ccs[2] + ccs[3]]; *ccs = rltl[ccs[0] + ccs[1] + ccs[2]]; ++ccs; ++px;
		*px = cltbl[ccs[0] + ccs[1] + ccs[2] + ccs[3]]; *ccs = rltl[ccs[0] + ccs[1] + ccs[2]]; ++ccs; ++px;
		goto life_loop_unroll;
	}
life_check_position:

	////pxt :
	////	;
	////	UINT32 *pxckpn = px + ltm;
	////pxt_loop:
	////	if (px == pxckpn){
	////		--px;
	////		goto pxt_finished;
	////	}
	////	*px += 0xff;
	////	++px;
	////	goto pxt_loop;
	////pxt_finished:
	////	;
	/* Simulation time reached */
	if (!tm)
		goto life_finished;
	/* Wrap arround on right end */
	if (ccs == cs + wbpn) {
		ccs = cs;
	}
	/* Stop position reached > refill buffers, count states, create color table */
	if (ccs == cs + spps) {
		/* Copy cells to joint buffer (where left- and rightmost cell meet) */
		for (int i = 0; i < brsz; i++) {
			if (i < brsz - ls)
				cs[(spps + i) % (scsz + brsz)] = cs[(spps + i + brsz) % (scsz + brsz)];
			else
				cs[(spps + i) % (scsz + brsz)] = cs[(spps + i + scsz) % (scsz + brsz)];
		}
		/* Copy cells of wrap behind (last cell) buffer */
		for (int i = 0; i < brsz; i++)
			cs[scsz + brsz + i] = cs[i]; // wrap arround buhlfspace
										 /* Move start and stop positions according to ls (left shift)  */
		sp = (spps + brsz - ls) % (scsz + brsz);
		ccs = cs + sp;

		///		ccs = cs + (spps + brsz - ls) % (scsz + brsz);
		spps = (spps + scsz + brsz - ls) % (scsz + brsz);
		//ccs = cs + sp;


		/* Count states and create color table */
		if (1 && px < pxbrd && hlfs) {
			/* Count states */
			if (1 & hlfssr) {
				////while (1) {
				////	if (ccs == cs + wbpn)
				////		ccs = cs;
				////	if (ccs == cs + spps)
				////		break;
				////	switch (hlfs) {
				////	case 1: sctbl[ccs[0]]++; break;
				////	case 3: sctbl[ccs[0] + ccs[1] + ccs[2]]++; break;
				////	case 4: sctbl[ccs[0] + ccs[1] + ccs[2] + ccs[3]]++; break;
				////	case 5: sctbl[ccs[0] + ccs[1] + ccs[2] + ccs[3] + ccs[4]]++; break;
				////	default: {
				////		int stpn = 0; // state table position
				////		for (int i = 0; i < hlfs; i++)
				////			stpn += ccs[i]; // cannot use multiplication table here atm -- may be to short
				////		sctbl[stpn]++;
				////	}
				////	}
				////	////sctbl[cr->rltl[stpn]]++;
				////	ccs++;
				////}
				////ccs = cs + sp;
				/*if (spps < sp) {
				memcpy(px, cs + sp, (wbpn - sp) * clsz);
				memcpy(px + wbpn - sp, cs, spps * clsz);
				} else
				memcpy(px, cs + sp, (spps - sp) * clsz);
				*/
				/* Get next position where a check will be necesary */
				// the wrap buffer is the highest possible stop position
				ckpn = cs + wbpn;
				// the joint buffer can be between the current position and the wrap buffer
				if (cs + spps > ccs)
					ckpn = cs + spps;

				//goto count_states_finished;
			count_states:
				//if ((ckpn - ccs) % 8 == 0)	goto count_states_loop_unroll;
				//if ((ckpn - ccs) % 32 == 0)	goto count_states_loop_ic256u8;
				////if ((ckpn - ccs) % 32 == 0)	goto count_states_loop_ic256u8_clever;
				if (ccs == ckpn)
					goto count_states_check_position;
				////++sctbl[ccs[0] + ccs[1] + ccs[2] + ccs[3]];
				////++ccs;
				switch (hlfs) {
				case 1: ++sctbl[ccs[0]]; break;
				case 3: ++sctbl[ccs[0] + ccs[1] + ccs[2]]; break;
				case 4: ++sctbl[ccs[0] + ccs[1] + ccs[2] + ccs[3]]; break;
				case 5: ++sctbl[ccs[0] + ccs[1] + ccs[2] + ccs[3] + ccs[4]]; break;
				default:;
					int stpn = 0; // rule table position
					for (int i = 0; i < hlfs; i++)
						stpn += ccs[i];
					++sctbl[stpn];
				}
				++ccs;
				goto count_states;
				{
				count_states_loop_ic256u8: // version using intrisic 256bit=32 uint8 SIMD , needs bytes remaining % (mod) 32 = 0
					if (ccs == ckpn)
						goto count_states_check_position;
					register __m256i t;
					t = _mm256_adds_epu8(*(__m256i*)(ccs + 0), *(__m256i*)(ccs + 1));
					t = _mm256_adds_epu8(t, *(__m256i*)(ccs + 2));
					t = _mm256_adds_epu8(t, *(__m256i*)(ccs + 3));
					for (register int i = 0; i < 32; ++i)
						++sctbl[t.m256i_u8[i]];
					ccs += 32;
					goto count_states_loop_ic256u8;
				}
				{
				count_states_loop_ic256u8_clever:; // version using intrisic 256bit=32 uint8 SIMD , needs bytes remaining % (mod) 32 = 0
												   //register __m256i ymm0, ymm1, ymm2, ymm3;
												   //ymm0 = ymm1 = ymm2 = ymm3 = _mm256_setzero_si256();
					static int ymm[64]; for (int i = 0; i < 64; ++i) ymm[i] = 0;
					register int *ymm0 = ymm + 0, *ymm1 = ymm + 16, *ymm2 = ymm + 32, *ymm3 = ymm + 48;
				count_states_loop_ic256u8_clever_inner:;
					if (ccs == ckpn) {
						for (register int i = 0; i < 16; ++i)
							sctbl[i] += ymm[i + 0],
							sctbl[i] += ymm[i + 16],
							sctbl[i] += ymm[i + 32],
							sctbl[i] += ymm[i + 48];
						//sctbl[i] += ymm0.m256i_u16[i],
						//sctbl[i] += ymm1.m256i_u16[i],
						//sctbl[i] += ymm2.m256i_u16[i],
						//sctbl[i] += ymm3.m256i_u16[i];
						goto count_states_check_position;
					}
					register __m256i t;
					t = _mm256_adds_epu8(*(__m256i*)(ccs + 0), *(__m256i*)(ccs + 1));
					t = _mm256_adds_epu8(t, *(__m256i*)(ccs + 2));
					t = _mm256_adds_epu8(t, *(__m256i*)(ccs + 3));
					for (register int i = 0; i < 32; i += 4)
						++ymm[0 + t.m256i_u8[i + 0]],
						++ymm[16 + t.m256i_u8[i + 1]],
						++ymm[32 + t.m256i_u8[i + 2]],
						++ymm[48 + t.m256i_u8[i + 3]];
					////++ymm0[t.m256i_u8[i + 0]],
					////++ymm1[t.m256i_u8[i + 1]],
					////++ymm2[t.m256i_u8[i + 2]],
					////++ymm3[t.m256i_u8[i + 3]];
					//++ymm0.m256i_u16[t.m256i_u8[i]],
					//++ymm1.m256i_u16[t.m256i_u8[i + 1]],
					//++ymm2.m256i_u16[t.m256i_u8[i + 2]],
					//++ymm3.m256i_u16[t.m256i_u8[i + 3]];
					ccs += 32;
					goto count_states_loop_ic256u8_clever_inner;
				}
			count_states_loop_unroll:
				if (ccs == ckpn)
					goto count_states_check_position;
				++sctbl[ccs[0] + ccs[1] + ccs[2] + ccs[3]];
				++sctbl[ccs[1] + ccs[2] + ccs[3] + ccs[4]];
				++sctbl[ccs[2] + ccs[3] + ccs[4] + ccs[5]];
				++sctbl[ccs[3] + ccs[4] + ccs[5] + ccs[6]];
				++sctbl[ccs[4] + ccs[5] + ccs[6] + ccs[7]];
				++sctbl[ccs[5] + ccs[6] + ccs[7] + ccs[8]];
				++sctbl[ccs[6] + ccs[7] + ccs[8] + ccs[9]];
				++sctbl[ccs[7] + ccs[8] + ccs[9] + ccs[10]];
				ccs += 8;
				goto count_states_loop_unroll;
			count_states_check_position:
				if (ccs == cs + wbpn)
					ccs = cs,
					ckpn = cs + spps;
				if (ccs == cs + spps)
					goto count_states_finished;
				goto count_states;
			count_states_finished:
				ccs = cs + sp;
				////int tspps = (sp + scsz - 10) % (scsz + brsz);
				////int plsp = px + scsz - 10;
				////while (1) {
				////	if (px >= plsp)
				////		break;
				////	if (ccs == cs + wbpn)
				////		ccs = cs;
				////	if (ccs == cs + tspps)
				////		break;
				////	*px = ccs[0] + ccs[1] + ccs[2] + ccs[3];
				////	//*px = px[0] + px[1] + px[2] + px[3];
				////	sctbl[*px]++;
				////	++px;
				////	++ccs;
				////}
				////ccs = cs + sp;
			}
			/* Automatic-contrast mode */
			if (ar == 0 || ar == 1)		min = 0;	else min = MAXINT;
			if (ar == 0 || ar == 2)		max = scsz;	else max = 0;
			/* Log mode */
			//for (int i = 0; i < dyss; i++)
			//	sctbl[i] = (uint32_t)(log((double)sctbl[i] + 1.0) * 1e7);
			/* Get min (excluding zero) and max of state count table */
			if (ar != 0) {
				for (int i = 0; i < dyss; i++) {
					if (sctbl[i] > 0 && sctbl[i] < min)
						min = sctbl[i];
					if (sctbl[i] > max)
						max = sctbl[i]; //max += sctbl[i];
				}
				if (max <= min)
					max = min + 1;
			}
			/* Build color table from state count table and reset state count table */
			//printf("min %6d  max %6d  log max-min %f  %f\n  color table  ", min, max, log(max - min), log(2));
			for (int i = dyss - 1; i >= 0; i--) {
				cltbl[i] = getColor((double)(sctbl[i] - min) / (max - min + 1), cm, crct, gm);
				//printf("%d-%d  ", sctbl[i], cltbl[i]);
				/* reset state count table */
				sctbl[i] = 0;
			}

			/* Pitch for scanline */
			px += pitch;
		}

		while (0) {
			if (ccs == cs + wbpn)
				ccs = cs;
			if (ccs == cs + spps)
				break;
			++sctbl[ccs[0] + ccs[1] + ccs[2] + ccs[3]];
			++ccs;
		}
		ccs = cs + sp;
		////
		while (0) {
			if (ccs == cs + wbpn)
				ccs = cs;
			if (ccs == cs + spps)
				break;
			//*px = ccs[0] + ccs[1] + ccs[2] + ccs[3];
			//*px = 2;// cltbl[2];
			*px = cltbl[ccs[0]];
			++px;
			++ccs;
		}
		ccs = cs + sp;
	}
	/* Get next position where a check will be necesary */
	// the wrap buffer is the highest possible stop position
	ckpn = cs + wbpn;
	// the joint buffer can be between the current position and the wrap buffer
	if (ccs < cs + spps)
		ckpn = cs + spps;
	// the remaining time (cells to life) may be smaller than the amount of cells that would life until the check position is be reached
	if (tm < ckpn - ccs)
		ckpn = ccs + tm;
	// adjust remaining time (see comment above)
	tm -= ckpn - ccs;

	ltm = ckpn - ccs;

	goto life;


life_finished:

	/**/
	for (int i = 0; i < scsz; i++)
		sc[i] = cs[(sp + i) % (scsz + brsz)];


	sc_ccs = ccs, sc_ckpn = ckpn;
}



#define LV_MAXCLTLSZ 32			// max size of rule table
#define LV_MAXRLTLSZ 32			// max size of rule table
void
liveanddrawnewclean(
	/*
	CA_CT *rltl,				// rule table
	CA_CT *ca_space,			// space
	int ca_space_sz,			// space-size
	int ca_time					// time - nr. of steps/generations to live
	*/
	CA_RULE *cr,				/* ca configuration */
	CA_CT *sc,					/* space */
	const int scsz,				/* space-size */
	const int zoom,
	int tm,						/* time - nr. cells (tm=width=line=1=full generation) to live */
								//int ncr,					/* number of cells needed to calculate next generation (rule table lookup) */
	int ncd,					/* number of cells for drawing */
	UINT32 *ppx,				/* rgba pixel array to draw to */
	int res,					/* reset-flag (because of the static vars)*/
	const UINT32 *pxbrd,
	const UINT32 *pxfrst,
	const unsigned int pitch,
	const int hlfs,				/* focus > nr. of hlfs cells next to eachother are summed... TODO */
	const int hlfssr,				/* = 1 > focus is done on same row; 0 > on next row */
	const unsigned int ar,		/* auto-range - 0: none, 1: auto min, 2: auto max, 3: auto min and max */
	const unsigned int cm,		/* color mode */
	const unsigned int crct,	/* color count */
	const unsigned int gm		/* gradient mode */
	) {
	register UINT32 *px = ppx;	/* rgba pixel array to draw to */

								//tm = tm / scsz * scsz;

	int ltm = 0;

	if (!tm)
		return;

	/* memory size of one cell */
	const int clsz = sizeof(CA_CT);
	/* size of one buffer (we will need two of this size: joint and wrap buffer);
	buffer must be large enough to always allow for readahead for next-gen-calculation (cr->ncn)
	or drawing-calucaltion (hlfs) */
	const int brsz = max(hlfs - 1, cr->ncn - 1);

	static int sp;					// start position
	static int spps;				// stop position
	static CA_CT *sc_ckpn;			// check position	
	static CA_CT *sc_ccs = NULL;	// current position
	const int wbpn = scsz + brsz;	// wrap buffer position

	const int ls = cr->ncn / 2; // left shift

	register CA_CT *ccs, *ckpn;
	ccs = sc_ccs, ckpn = sc_ckpn;

	/* current space */
	static CA_CT *cs = NULL;
	if (!cs | res) {
		//free(cs);
		//cs = malloc((scsz + brsz + brsz) * clsz);
		// aligned memory allocation might be necesary for use of some intrinsic functions
		_aligned_free(cs);
		cs = _aligned_malloc((scsz + brsz + brsz) * clsz, 4096);
		//
		memcpy(cs, sc, scsz * clsz);
		sp = spps = scsz;
		ckpn = cs + spps;
		ccs = ckpn;
		ccs - cs;
	}

	/* check for maximum size of state tables */
	int dyss = cr->ncs * max(1, hlfs);
	if (dyss > LV_MAXCLTLSZ) {
		printf("ERROR max color table size to small");
		return;
	}
	if (cr->nns > LV_MAXRLTLSZ) {
		printf("ERROR max rule table size to small");
		return;
	}

	/* init color table */
	static UINT32 cltbl[LV_MAXCLTLSZ];
	// focus == 0 > static color table, otherwise automatically initialized below
	if (hlfs == 0)
		for (int i = 0; i < dyss; i++)
			cltbl[i] = getColor(1.0 / (dyss - 1) * i, cm, crct, gm);

	/* init state count table */
	static uint32_t sctbl[LV_MAXCLTLSZ] = { 0 };	// state count table
	if (res) {
		for (int i = 0; i < dyss; i++)
			sctbl[i] = 0;
	}

	/* copy rule and multilication table to local memory to improve speed */
	static CA_CT rltl[LV_MAXRLTLSZ];
	static CA_CT mntl[LV_MAXRLTLSZ];
	for (int i = 0; i < cr->nns; i++) {
		rltl[i] = cr->rltl[i];
		mntl[i] = cr->mntl[i];
	}


	int min, max;

life:
	if (ccs == ckpn)
		goto life_check_position;
	/* Draw - flexible variant - can handle totalisitic-count sharpening */
	if (px < pxbrd) {
		switch (hlfs) {
		case 4: *px = cltbl[ccs[0] + ccs[1] + ccs[2] + ccs[3]]; break;
		case 0:
		case 1: *px = cltbl[ccs[0]]; break;
		case 2: *px = cltbl[ccs[0] + ccs[1]]; break;
		case 3: *px = cltbl[ccs[0] + ccs[1] + ccs[2]]; break;
		case 5: *px = cltbl[ccs[0] + ccs[1] + ccs[2] + ccs[3] + ccs[4]]; break;
		default:;
			int ctpn = 0; // color table position
			for (int i = 0; i < hlfs; i++)
				ctpn += ccs[i];
			*px = cltbl[ctpn];
		}
		++px;
	}
	/* Next generation - flexible variant - can handle totalistic and absolute rules */
	switch (cr->ncn) {
	case 3: *ccs = rltl[mntl[0] * ccs[0] + mntl[1] * ccs[1] + mntl[2] * ccs[2]]; break;
	case 4: *ccs = rltl[mntl[0] * ccs[0] + mntl[1] * ccs[1] + mntl[2] * ccs[2] + mntl[3] * ccs[3]]; break;
	case 5: *ccs = rltl[mntl[0] * ccs[0] + mntl[1] * ccs[1] + mntl[2] * ccs[2] + mntl[3] * ccs[3] + mntl[4] * ccs[4]]; break;
	default:;
		int rtpn = 0; // rule table position
		for (int i = 0; i < cr->ncn; i++)
			rtpn += mntl[i] * ccs[i];
		*ccs = rltl[rtpn];
	}
	++ccs;

	goto life;
life_check_position:
	/* Simulation time reached */
	if (!tm)
		goto life_finished;
	/* Wrap arround on right end */
	if (ccs == cs + wbpn) {
		ccs = cs;
	}
	/* Stop position reached > refill buffers, count states, create color table */
	if (ccs == cs + spps) {
		/* Copy cells of joint buffer (where left- and rightmost cell meet) */
		for (int i = 0; i < brsz; i++) {
			if (i < brsz - ls)
				cs[(spps + i) % (scsz + brsz)] = cs[(spps + i + brsz) % (scsz + brsz)];
			else
				cs[(spps + i) % (scsz + brsz)] = cs[(spps + i + scsz) % (scsz + brsz)];
		}
		/* Copy cells of wrap behind buffer (last cells in line) */
		for (int i = 0; i < brsz; i++)
			cs[scsz + brsz + i] = cs[i]; // wrap arround buhlfspace
										 /* Move start and stop positions according to ls (left shift)  */
		sp = (spps + brsz - ls) % (scsz + brsz);
		spps = (spps + scsz + brsz - ls) % (scsz + brsz);

		/* Count states and create color table */
		if (px < pxbrd && hlfs) {
			ccs = cs + sp;
			/* Get next position where a check will be necesary */
			// the wrap buffer is the highest possible stop position
			ckpn = cs + wbpn;
			// the joint buffer can be between the current position and the wrap buffer
			if (cs + spps > ccs)
				ckpn = cs + spps;
		count_states:
			if (ccs == ckpn)
				goto count_states_check_position;
			switch (hlfs) {
			case 1: ++sctbl[ccs[0]]; break;
			case 3: ++sctbl[ccs[0] + ccs[1] + ccs[2]]; break;
			case 4: ++sctbl[ccs[0] + ccs[1] + ccs[2] + ccs[3]]; break;
			case 5: ++sctbl[ccs[0] + ccs[1] + ccs[2] + ccs[3] + ccs[4]]; break;
			default:;
				int stpn = 0; // rule table position
				for (int i = 0; i < hlfs; i++)
					stpn += ccs[i];
				++sctbl[stpn];
			}
			++ccs;
			goto count_states;
		count_states_check_position:
			if (ccs == cs + wbpn)
				ccs = cs,
				ckpn = cs + spps;
			if (ccs == cs + spps)
				goto count_states_finished;
			goto count_states;
		count_states_finished:
			/* Automatic-contrast mode */
			if (ar == 0 || ar == 1)		min = 0;	else min = MAXINT;
			if (ar == 0 || ar == 2)		max = scsz;	else max = 0;
			/* Log mode */
			//for (int i = 0; i < dyss; i++)
			//	sctbl[i] = (uint32_t)(log((double)sctbl[i] + 1.0) * 1e7);
			/* Get min (excluding zero) and max of state count table */
			if (ar != 0) {
				for (int i = 0; i < dyss; i++) {
					if (sctbl[i] > 0 && sctbl[i] < min)
						min = sctbl[i];
					if (sctbl[i] > max)
						max = sctbl[i]; //max += sctbl[i];
				}
				if (max <= min)
					max = min + 1;
			}
			/* Build color table from state count table and reset state count table */
			//printf("min %6d  max %6d  log max-min %f  %f\n  color table  ", min, max, log(max - min), log(2));
			for (int i = dyss - 1; i >= 0; i--) {
				cltbl[i] = getColor((double)(sctbl[i] - min) / (max - min + 1), cm, crct, gm);
				//printf("%d-%d  ", sctbl[i], cltbl[i]);
				/* reset state count table */
				sctbl[i] = 0;
			}
		}
		ccs = cs + sp;
	}
	/* Get next position where a check will be necesary */
	// the wrap buffer is the highest possible stop position
	ckpn = cs + wbpn;
	// the joint buffer can be between the current position and the wrap buffer
	if (ccs < cs + spps)
		ckpn = cs + spps;
	// the remaining time (cells to life) may be smaller than the amount of cells that would life until the check position is be reached
	if (tm < ckpn - ccs)
		ckpn = ccs + tm;
	// adjust remaining time (see comment above)
	tm -= ckpn - ccs;

	ltm = ckpn - ccs;

	goto life;


life_finished:

	/**/
	for (int i = 0; i < scsz; i++)
		sc[i] = cs[(sp + i) % (scsz + brsz)];


	sc_ccs = ccs, sc_ckpn = ckpn;
}

/*
scroll any array by d bytes, will ignore scrolling larger than array size
*/
void
scroll(UINT8 *ayf, int ays, int d) {
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


/* Calculate next generation - CAN ONLY HANDLE TOTALISTIC ATM  */
void
ca_next_gen__simd(
	CA_RULE *cr,	// cellular-automata rule
	CA_CT *clv,		// cell-line first valid element
	CA_CT *cli		// cell-line first invalid element
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

/* Calculate next generation - CAN ONLY HANDLE TOTALISTIC ATM, ncn fixed */
void
ca_next_gen_ncn3_simd(
	CA_RULE *cr,	// cellular-automaton rule
	CA_CT *clv,		// cell-line first valid element
	CA_CT *cli		// cell-line first invalid element
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
	CA_RULE *cr,	// cellular-automaton rule
	CA_CT *clv,		// cell-line first valid element
	CA_CT *cli		// cell-line first invalid element
	) {
	while (clv < cli) {
		__m256i ymm0 = _mm256_loadu_si256(clv);
		__m128i xmm0 = _mm_setzero_si128();
		xmm0.m128i_i64[0] = 1;
		ymm0 = _mm256_sll_epi64(ymm0, xmm0);
		ymm0 = _mm256_adds_epu8(ymm0, *(__m256i*)(clv + 1));
		ymm0 = _mm256_sll_epi64(ymm0, xmm0);
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


/* Calculate next generation - flexible variant - can handle totalistic and absolute rules by use of multiplication table */
void
ca_next_gen__sisd(
	CA_RULE *cr,			// cellular-automaton rule
	CA_CT *clv,				// cell-line first valid element
	CA_CT *cli				// cell-line first invalid element
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
	CA_RULE *cr,			// cellular-automaton rule
	CA_CT *clv,				// cell-line first valid element
	CA_CT *cli				// cell-line first invalid element
	) {
	while (clv < cli) {
		*clv = cr->rltl[cr->mntl[0] * clv[0] + cr->mntl[1] * clv[1] + cr->mntl[2] * clv[2]];
		++clv;
	}
}


/* Calculate next generation - CAN ONLY HANDLE TOTALISTIC ATM   flexible variant - can handle totalistic and absolute rules by use of multiplication table */
void
ca_count__simd(
	const CA_CT *csv,					// cell-space first valid element
	const CA_CT *csf,					// cell-space element to start with
	const CA_CT *csi,					// cell-space first invalid element
	const UINT32 *pbv,					// pixel-buffer first valid element
	const UINT32 *pbi,					// pixel-buffer first invalid element
	const int hlzm,						// horizontal zoom
	const int hlfs,						// horizontal focus
	const int fsme,						// focus-mode - 0 = totalistic, 1 = absolute
	const int ncs						// number of cell-states
	) {
	register UINT32 *pbc = pbv;			// pixel-buffer cursor / current element
	register CA_CT *csc = csf;			// cell-space cursor / current element
	register CA_CT *csk = csc;			// cell-space check position - initialised to equal csc in order to trigger recalculation of csk in count_check
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
		*pbc += ymm0.m256i_u8[i];
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
//	/* Copy rule and multilication table to local memory in hope to improve speed */
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
http://images.google.de/imgres?imgurl=http%3A%2F%2Fmathforum.org%2Fadvanced%2Frobertd%2F32seg.gif&imgrefurl=http%3A%2F%2Fmathforum.org%2Fadvanced%2Frobertd%2Flsys2d.html&h=282&w=282&tbnid=kPGGL2g7pcjaLM%3A&vet=1&docid=8RBroO2UQY83qM&ei=yc1XWIvnB-zLgAamv6SYBg&tbm=isch&iact=rc&uact=3&dur=271&page=7&start=341&ndsp=48&ved=0ahUKEwjL76i-nIDRAhXsJcAKHaYfCWM4rAIQMwg3KDUwNQ&safe=off&bih=1134&biw=1763

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
int *LMSA[LMS_COUNT];											// Lindenmeyer arrays

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
	const int *lm,							// Lindenmeyer array
	UINT32 *pnv,							// pixel-screen first valid element
	UINT32 *pni,							// pixel-screen first invalid element
	const int plw,							// pixel-screen line-width
	const int paw,							// pixel-screen available with
	int vlpz,								// vertical pixel zoom
	int hlpz,								// horizontal pixel zoom
	const UINT32 *pbv,						// pixel-buffer first valid element
	const UINT32 *pbi,						// pixel-buffer first invalid element
	const int lmsw,							// Lindenmeyer step width
	int lmdh,								// Lindenmeyer depth - if < 0 it is calculated automatically
	int *lmvs,								// Lindenmeyer needed screen size for given depth and plw - if not NULL no drawing is done, but size of Lindenmeyer system with given depth is calculated
	int *lmhs,								// Lindenmeyer needed screen size for given depth and plw - if not NULL no drawing is done, but size of Lindenmeyer system with given depth is calculated
	int *lmpbsz								// Lindenmeyer pixel-buffer-size - i. e. nr of pixels needed to fill Lindenmeyer of given depth, calculated if lmsnsz is not NULL, see line above
	) {
	int rpnc = 0;							// pixel-screen cursor (relative, no pointer)
	int rpni = max(0, (pni - pnv) - plw * (vlpz - 0) - (hlpz - 0));		// pixel-screen first invalid element (relative, no pointer) - corrected for pixel-zoom (so we dont have to check for every zoomed pixel)
	UINT32 *pbc = pbv;						// pixel-buffer cursor

	int plh = (pni - pnv) / plw;			// pixel-screen-height

	const int sg = 2;						// spacing
	int rc = 2;								// reduce (to compensate spacing)

											/* Automatically recalc depth and display-size if pb-size or Lindenmeyer array changes */
	static int ltsnsz = 0;					// last pixel-screen-size
	static int ltstpn = 0;					// last start-position
	static int ltpbsz = 0;					// last pixel-buffer-size
	static int ltlmdh = -1;					// last Lindenmeyer depth
	static int *ltlm = NULL;				// last Lindenmeyer array
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
		avs[0] = +sg*hlpz, avs[1] = +sg*plw*vlpz + hlpz, avs[2] = +sg*plw*vlpz - hlpz,
		avs[3] = -sg*hlpz, avs[4] = -sg*plw*vlpz - hlpz, avs[5] = -sg*plw*vlpz + hlpz;
	//avs[0] = -2*lw, avs[1] = -1*lw +2, avs[2] = +1*lw +2,
	//avs[3] = +2*lw, avs[4] = +1*lw -2, avs[5] = -1*lw -2;
	// tetragonal
	else if (alc == LMT)
		avs[0] = +sg*hlpz, avs[1] = +sg*plw*vlpz, avs[2] = -sg*hlpz, avs[3] = -sg*plw*vlpz;
	// octagonal
	else if (alc == LMO)
		avs[0] = +sg*hlpz, avs[1] = +sg*plw*vlpz + sg*hlpz, avs[2] = +sg*plw*vlpz, avs[3] = +sg*plw*vlpz - sg*hlpz,
		avs[4] = -sg*hlpz, avs[5] = -sg*plw*vlpz - sg*hlpz, avs[6] = -sg*plw*vlpz, avs[7] = -sg*plw*vlpz + sg*hlpz;

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
	UINT32 *pnv,							// pixel-screen first valid element
	UINT32 *pni,							// pixel-screen first invalid element
	const int plw,							// pixel-screen line-width
	const int paw,							// pixel-screen available with
	const int vlpz,							// vertical-pixel-zoom
	const int hlpz,							// horizontal-pixel-zoom
	const UINT32 *pbv,						// pixel-buffer first valid element
	const UINT32 *pbi,						// pixel-buffer first invalid element
	const int md							// mode - 0: morton, 1: line, 2: grid, 3: matze-fractal
	) {

	UINT32 *pbc = pbv;						// pixel-buffer cursor
	double level = log(pbi - pbv) / log(4.0);
	int ysz, xsz;
	ysz = xsz = ipow(2, level);
	if (fmod(level, 1.0) > 0.0)
		xsz *= 2;
	if (fmod(level, 1.0) > 0.5)
		ysz *= 2;
	int h = (pni - pnv) / plw,
		sy = h / 2 - ysz * vlpz / 2,
		sx = paw / 2 - xsz * hlpz / 2,
		cr = sy * plw + sx;					// transpose needed to center
	int vlpzplw = vlpz * plw;											// pre-calc vlpz * plw
	int rpnc = cr;														// pixel-screen position (relative, no pointer)
	pni -= plw * (vlpz - 1) + (hlpz - 1);				// correct pixel-screen first invalid for pixel-zoom (so we dont have to check for every zoomed pixel)



	if (md == 0) {
		// morton code layout
		int cs = pbi - pbc;
		for (UINT32 i = 0; i < cs; ++i) {
			UINT32 y, x;
			MortonDecode2(i, &x, &y);										// CPU-DEPENDANT !
			UINT32 *pnc = pnv + cr + y * vlpz * plw + x * hlpz;				// pixel-screen current element
			if (pnc >= pnv && pnc < pni)
				for (int v = 0; v < vlpzplw; v += plw)
					for (int h = 0; h < hlpz; ++h)
						pnc[v + h] = *pbc;
			++pbc;
			if (pbc >= pbi)
				return;
		}
	}
	else if (md == 1) {
		// grid memory layout
		int s1z = ceil(sqrt(pbi - pbv));									// space size 1d in pixels
		int s2z = s1z * s1z;												// space size 2d in pixels
		int g1z = 16;// s1z / 16;											// grid size 1d in pixels
		int g2z = g1z * g1z;												// grid size 2d in pixels
		int g1n = max(1, s1z / g1z);										// grid number 1d of grid-size blocks

		for (int g0c = 0; g0c < s2z; ++g0c) {								// grid memory cursor / iterator
			int g2c = g0c / g2z;											// grid 2d cursor
			int g1c = g0c % g2z;											// grid 1d cursor - relative block cursor = position within block
			int lmc = ((g2c / g1n) * g1z + g1c / g1z) * vlpz * plw +		// linear memory cursor
				((g2c % g1n) * g1z + g1c % g1z) * hlpz;
			UINT32 *pnc = pnv + cr + lmc;									// pixel-screen current element
			if (pnc >= pnv && pnc < pni)
				for (int vv = 0; vv < vlpzplw; vv += plw)					// pixel-zoom loops
					for (int hh = 0; hh < hlpz; ++hh)
						pnc[vv + hh] = *pbc;
			++pbc;
			if (pbc >= pbi)
				return;
		}
	}
	else if (md == 2) {
		// linear memory layout
		int sz = ceil(sqrt(pbi - pbv));
		for (int v = 0; v < sz * vlpzplw; v += vlpzplw)
			for (int h = v; h < v + sz * hlpz; h += hlpz) {
				UINT32 *pnc = pnv + rpnc + h;								// pixel-screen current element
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
	UINT32 *pnv,							// pixel-screen first valid element
	UINT32 *pni,							// pixel-screen first invalid element
	const int plw,							// pixel-screen line-width
	static CA_CT *csv,						// cells first valid element
	static CA_CT *csi,						// cells first invalid element
	const UINT32 *pbv,						// pixel-buffer first valid element
	const UINT32 *pbi,						// pixel-buffer first invalid element
	const int res							// reset flag
	) {
	int h = (pni - pnv) / plw;
	int sp = (h / 2) * plw + plw / 2;
	static UINT32 *spnc = NULL;
	if (res || !spnc)
		spnc = pnv + sp;
	UINT32 *pnc = spnc;
	int avs[4];
	avs[0] = 0 - plw;
	avs[1] = +1; avs[2] = +plw; avs[3] = -1;
	static int avc = 0;
	UINT32 *pbc = pbv;						// pixel-buffer cursor
	for (CA_CT *csc = csv; csc < csi; ++csc) {
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
dblcmpfunc(const void *a, const void *b) {
	if (*(double*)a < *(double*)b)
		return -1;
	if (*(double*)a > *(double*)b)
		return +1;
	return 0;
}

typedef struct caDisplaySettings {
	int fsme;										// focus-mode - 0: absolute, 1: totalistic
	int tm;											// test mode
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
	int tmfs;										// time focus
	int vlzm;										// vertical zoom - how many vertical cells will make out one pixel (together with hor. zoom); 0 = disable drawing, < 0 = how many vertical pixels a cell will make out
	int hlzm;										// horizontal zoom - how many horizontal cells will make out one pixel (together with vert. zoom); 0 = disable drawing, < 0 = how many horizontal pixels a cell will make out
	int vlpz;										// vertical pixel zoom
	int hlpz;										// horizontal pixel zoom
	unsigned int stzm;								// state zoom - 0 = direct zoom on intensity
} caDisplaySettings;

#define FBPT UINT64							// full-bit-block c-data-type
#define HBPT UINT32							// half-bit-block c-data-type
#define BPB 64								// bits per block, i.e. bits per elemet in bta 

/*
csv		cell-space first valid element
bav		bit-array first valid element
ct		count / nr. cells to convert
---
csv and bav must allow for access of  (ct + (BPB - 1)) / BPB  elements
*/
void
convert_CA_CT_to_bit_array(CA_CT* csv, FBPT* bav, int ct) {
	memset(bav, 0, ct / 8);				// must be zero'd, since individual bits are or'ed in
	int bib = 0;						// bit index in individual bit-block
	int bia = 0;						// bit index in whole array
	ct /= BPB;
	while (1) {
		if (*csv != 0)
			*bav |= (FBPT)1 << bib;
		++csv;
		++bib;
		if (bib == BPB) {
			bib = 0;
			++bav;
			++bia;
			if (bia == ct)
				break;
		}
	}
}

void
convert_bit_array_to_CA_CT(FBPT* bav, CA_CT* csv, int ct) {
	memset(csv, 0, ct);					// must be zero'd, since individual bits are or'ed in
	int bib = 0;						// bit index in individual bit-block
	int bia = 0;						// bit index in whole array
	ct /= BPB;
	while (1) {
		if ((FBPT)1 & (*bav >> bib))
			*csv = 1;
		//*csv = (FBPT)1 & (*bav >> bib);
		++csv;
		++bib;
		if (bib == BPB) {
			bib = 0;
			++bav;
			++bia;
			if (bia == ct)
				break;
		}
	}

	//for (int bi = 0; bi <= ct; ++bi)
	//csv[bi] = (FBPT)1 & (bav[bi / BPB] >> ((bi % BPB)));
}

void
render_ca_line(
	caDisplaySettings ds,
	UINT32 *pbv,							// pixel-buffer first valid element
	UINT32 *pbi,							// pixel-buffer first invalid element
	int res,									// reset flag
	int cr_ncs
	) {
	UINT32 *pbc;							// pixel-buffer cursor / current element

	UINT64 dyss;
	const int stst = 0;									// state shift (nr. of state bits shifted out / disregarded)
	if (ds.fsme == 0)
		dyss = max(1, (cr_ncs * (max(1, max(1, ds.hlfs) * max(1, ds.vlfs) * max(1, ds.tmfs + 1)))) >> stst);
	else if (ds.fsme == 1)
		dyss = max(1, ipow(cr_ncs, ds.hlfs * ds.vlfs * (ds.tmfs + 1)) >> stst);
	//printf("cr->ncs %d   fsme %d   dyss %d\n", cr->ncs, fsme, dyss);
	//dyss = max(0xFF, dyss);
	// Init state count table.
	static UINT32 *sctbl = NULL;		// state count table
	static double *dsctbl = NULL;		// double state count table
	static double *tdsctbl = NULL;		// temp double state count table
	if (!sctbl || res) {
		free(sctbl);
		free(dsctbl);
		free(tdsctbl);
		sctbl = malloc(dyss * sizeof *sctbl);
		dsctbl = malloc(dyss * sizeof *dsctbl);
		tdsctbl = malloc(dyss * sizeof *tdsctbl);
	}
	/* Init color table */
	static UINT32 *cltbl = NULL;						// color table
	if (!cltbl || res) {
		free(cltbl);
		cltbl = malloc(dyss * sizeof *cltbl);
	}
	static double *spt = NULL;			// state position table		
	static double *svt = NULL;			// state velocity table
	static int ltdyss = 0;
	if (ltdyss != dyss) {
		ltdyss = dyss;
		spt = malloc(dyss * sizeof *spt);
		svt = malloc(dyss * sizeof *svt);
		for (int i = 0; i < dyss; ++i) {
			spt[i] = 0.5;
			svt[i] = 0.0;
		}
	}

	// Reset state-count-table.
	for (int i = 0; i < dyss; ++i)
		sctbl[i] = 0;
	// Count states.
	if (ds.stzm || ds.ar)
		for (pbc = pbv; pbc < pbi; ++pbc)
			++sctbl[(*pbc) >> stst];
	// Init min and max and state count table.
	double dmn;	// double min
	double dmx;	// double max
	dmn = 1.0;
	if (ds.stzm) {
		dmx = (double)(pbi - pbv);
		if (ds.lgm)
			if (ds.cmm)
				dmx = dyss * (log2(dmx / dyss) + 1.0);
			else
				dmx = log2(dmx) + 1.0;
	}
	else {
		dmx = (double)dyss;
		for (int i = 0; i < dyss; ++i)
			if (!ds.ar || (ds.ar && sctbl[i]))
				sctbl[i] = i + 1;
	}
	// Convert to double.
	for (int i = 0; i < dyss; ++i)
		dsctbl[i] = (double)sctbl[i];
	// Log mode.
	if (ds.stzm && ds.lgm) {
		for (int i = 0; i < dyss; ++i) {
			if (dsctbl[i] > 0.0)
				dsctbl[i] = log2(dsctbl[i]) + 1.0;
		}
	}

	//// Debug
	//printf("dmx  %10.2f\n", dmx);
	//printf("dsctcm   ");
	//double dsctblcm = 0.0;
	//for (int i = 0; i < dyss; ++i) {
	//	dsctblcm += dsctbl[i];
	//	printf("%7.2f  ", dsctbl[i]);
	//}
	//printf(" | dsctblsm %7.2f\n", dsctblcm);

	// Cumulative modes.
	if (ds.stzm && ds.cmm == 1) {
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
	else if (ds.stzm && ds.cmm == 2) {
		for (int i = 1; i < dyss; ++i)
			if (dsctbl[i] > 0.0)
				dsctbl[i] += dsctbl[i - 1];
	}

	// Debug
	/*
	printf("dsctbl   ");
	double dsctblsm = 0.0;
	for (int i = 0; i < dyss; ++i) {
	dsctblsm += dsctbl[i];
	printf("%7.2f  ", dsctbl[i]);
	}
	printf(" | dsctblsm %7.2f\n", dsctblsm);
	*/
	// Get min and max of state count table.
	if (ds.ar) {
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
	// Convert to range 0.0 - 1.0.
	double rng = dmx - dmn;
	if (rng == 0.0)
		rng = 1.0;
	for (int i = 0; i < dyss; ++i)
		if (dsctbl[i] > 0.0)
			dsctbl[i] = (dsctbl[i] - dmn) / rng;
	// Translate and spread.
	for (int i = 0; i < dyss; ++i) {
		dsctbl[i] = dsctbl[i] + ds.te;
		dsctbl[i] = max(0.0, dsctbl[i]);
		dsctbl[i] = min(1.0, dsctbl[i]);
		if (ds.sd <= 1.0)
			dsctbl[i] = pow(dsctbl[i], ds.sd);
		else
			dsctbl[i] = pow(dsctbl[i], 1.0 / (2.0 - ds.sd));
	}
	// Auto-range delay.
	for (int i = 0; i < dyss; ++i) {
		svt[i] = svt[i] - svt[i] * ds.ard + (dsctbl[i] - spt[i]) * ds.arf;
		spt[i] += svt[i];
		dsctbl[i] = spt[i];
		dsctbl[i] = max(0.0, dsctbl[i]);
		dsctbl[i] = min(1.0, dsctbl[i]);
	}
	// Build color-table.
	for (int i = 0; i < dyss; ++i)
		cltbl[i] = getColor(dsctbl[i], ds.cm, ds.crct, ds.gm);
	// Draw space to pixel buffer.
	for (pbc = pbv; pbc < pbi; ++pbc)
		*pbc = cltbl[*pbc >> stst];
	// Display test mode.
	if (ds.tm)
		for (pbc = pbv; pbc < pbi; ++pbc)
			*pbc = getColor(1.0 / (pbi - pbv) * (pbc - pbv), ds.cm, ds.crct, ds.gm);
	// Debug.
	//printf("mn %.4f  mx %.4f\n", dmn, dmx);
	//for (int i = 0; i < dyss; ++i)
	//	printf("col  %d  %.4f\n", i, dsctbl[i]);
}

UINT32*
ca_display_line(
	int tm,												// time - nr. cells (tm=width=line=1=full generation) to live 
	int sm, // stripe mode

	caDisplaySettings ds,
	UINT32 *pnv,										// pixel-screen first valid element
	const UINT32 *pni,									// pixel-screen first invalid element
	UINT32 *pbv,							// pixel-buffer first valid element
	UINT32 *pbi,							// pixel-buffer first invalid element
	int res,									// reset flag
	UINT32 *spnc,	// static pixel screen cursor; needed for stripe mode
	int pbs,	// pixel buffer size
	int hw	// histogram width
	)
{
	UINT32 *pnc = pnv;										// pixel-screen first valid element
	UINT32 *pbc = pbv;
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
			//					int cs = min(plw, ( min(pni - pnc, pbi - pbc)));		// size of copy
			int cs =
				(
					min(ds.plw,
						max(0,
							min(
								(pni - pnc),
								ds.hlpz * (pbi - pbc)))));		// size of copy
																//printf("pn %d  pb %d   cs %d  plw %d\n", (pni-pnc), pbi-pbc,cs, plw);
			if (cs > 0) {
				if (!sm && pbs * ds.hlpz > ds.plw)
					pbc = pbv + (pbs * ds.hlpz - ds.plw) / 2 / ds.hlpz;
				if (!sm && pbs * ds.hlpz < ds.plw)
					pnc += (ds.plw - pbs * ds.hlpz) / 2;

				if (ds.hlpz > 1) {
					//if (pni - pnc >= cs / hlpz * hlpz)
					{
						for (int hi = 0; hi < cs / ds.hlpz; ++hi) {
							for (int hzi = 0; hzi < ds.hlpz; ++hzi) {
								*pnc = *pbc;
								++pnc;
							}
							++pbc;
						}
					}
				}
				else {
					memcpy(pnc, pbc, cs * 4);
					pbc = pbi;
					pnc += cs;
				}

				if (sm) {
					if (pbs < ds.plw)
						pnc += ds.plw - pbs;
					//pnc += max(0, plw - pbs);
					if (pnc >= pni) {
						//pnc = pnv + ((pnc - pnv) % plw + pbs);
						if (pbs > ds.plw)
							pnc = pnv;
						else
							pnc = pnv + (pnc - pnv) % ds.plw + pbs;
						if (pnc + pbs > pnv + ds.plw)
							pnc = pnv;
						if (tm > pni - pnv)
							;//de = 0;
					}
					spnc = pnc;
				}

				//if (pbs * hlpz < plw)
				//	pnc += (plw - pbs * hlpz) / 2 + (plw - pbs * hlpz) % 2;
				pnc += (pni - pnc) % ds.plw;
				//if (vlpz)
				pbc = pbv;
			}
		}
	}
	else if (ds.sfcm >= 1 && ds.sfcm <= 4) {
		display_2d_matze(pnv + hw, pni, ds.plw, ds.plw - hw, ds.vlpz, ds.hlpz, pbv, pbi, ds.sfcm - 1);
		//pbc = pbi;
		//de = 0;
	}
	else if (ds.sfcm > 4 && ds.sfcm < 5 + LMS_COUNT) {
		display_2d_lindenmeyer(LMSA[ds.sfcm - 5], pnv + hw, pni, ds.plw, ds.plw - hw, ds.vlpz, ds.hlpz, pbv, pbi, ds.sfcsw, -1, NULL, NULL, NULL);
		//pbc = pbi;
		//de = 0;
	}
	else {
		////display_2d_chaotic(pnv, pni, ds.plw, csv, csi, pbv, pbi, res);
	}

	return pnc;
}

void ca_evolve(
	CA_RULE *cr,										// ca configuration 
	CA_CT *csv,											// cells first valid element
	CA_CT *csi,											// cells first invalid element
	int scsz,											// space-size 
	int mnbrsz,		// minimum border-size,
	int mxbrsz,		// maximum border-size
	int ng,												// number of generations to evolve
	int res,											// reset-flag (because of the static vars)
	const int cnmd,										// computation-mode - 0 = CPU, 1 = GPU
	const unsigned int klrg								// kernel-range (GPU mode)
	)
{
	if (scsz < mnbrsz) {
		printf("ERROR scsz to small to fill brsz!\n");
		return;
	}
//	printf("ca_evolve   %d gens  %d scsz  %d mnbrsz  %d mxbrsz  %d cdsz\n", ng, scsz, mnbrsz, mxbrsz, scsz + mxbrsz);
	for (int gi = 0; gi < ng; ++gi) {
		memcpy(csv + scsz, csv, mnbrsz * sizeof *csv);
		ca_next_gen__sisd(cr, csv, csi);
	}


	// SIMD 	// 32 to make sure simd instructions have enough room

	////// Copy cells of wrap behind buffer (last cells in line).
	//////*csf = 0;							// border-mode - emptiness
	//////for (int i = 0; i < cr->ncn - 1; ++i)	// emptiness
	//////	csv[(csf - csv + i) % scsz] = 0;

	//////*csf = rand() % cr->ncn;			// - random

	//////const int nr = 4;					// nr. of random points
	//////const int sr = scsz / nr;			// spacing of random pints
	//////for (int ri = sr / 2; ri < scsz; ri += sr) {
	//////	//csv[((csf - csv) + ri) % scsz] = csv[((csf - csv) + ri - 1) % scsz] = rand() % cr->ncn;
	//////	csv[((csf - csv) + ri) % scsz] = rand() % cr->ncn;
	//////}
	//////for (int i = 0; i < cr->ncn - 1; ++i)	// mirror 
	//////	csv[(csf - csv + i + cr->ncn) % scsz] = csv[(csf - csv + i) % scsz];

	//////memcpy(csv, csv + scsz, 32 * sizeof *csv);
	////memcpy(csv + scsz, csv, brsz * sizeof *csv);
	//////CA_CT *csb = csv + scsz;
	//////for (int i = 0; i < brsz; ++i)
	//////	csb[i] = csv[i];

////	/* Calculate next generation */
////	int ng = 0; // number of generations calculated
////				//printf("de %d  tm %d\n", de, tm);
////				/* GPU = 3 */
////	if (cnmd == 3) {
////		if (!de)
////			ng = tm / scsz;
////		tm -= ng * scsz;
////		//
////		OCLCA_Run(ng, res, oclca, cr, de, csv, scsz, brsz, klrg);
////	}
////	/* CPU = 1 | 2; CPU SIMD = 2 - only in TOT/total mode!; CPU SISD = 1 */
////	else if (cnmd != 0) {
////		while (tm >= scsz) {
////			++ng;
////			tm -= scsz;
////			//
////			if (cnmd == 2 && cr->tp == TOT)
////				if (cr->ncn == 3)
////					ca_next_gen_ncn3_simd(cr, csv, csi);
////				else
////					ca_next_gen__simd(cr, csv, csi);
////			else
////				if (cr->ncn == 3)
////					if (cr->ncs == 2)
////						ca_next_gen_abs_ncn3_ncs2_simd(cr, csv, csi);
////					else
////						ca_next_gen_ncn3_sisd(cr, csv, csi + 32);
////				else
////					ca_next_gen__sisd(cr, csv, csi + 32);
////			if (de)
////				break;
////		}
////	}
////	// CPU Boolean = > 3 & < 1 - only rule 147 absolute!
////	else {
////		//
////		if (!de)
////			ng = tm / scsz;
////		//
////		//#define RT 1								// repeats / generations
////#define DG 0		// Debug
////		static FBPT* bav = NULL;		// bit-array - first valid element
////		int olas = csi - csv + 256;		// !!!! original array count
////		int bact = (olas + BPB - 1) / BPB + 1;	// bit array count (rounded up / ceil) // allow one extra-element since when we process the last half block we read one half block past the end
////		int bas = bact * (BPB / 8);	// bit array size in bytes (rounded up / ceil)
////		if (bav == NULL | res) {
////			if (DG)	printf("creating bit array - oas %d  ba-count %d  ba-size %d\n", olas, bact, bas);
////			_aligned_free(bav);								// aligned memory management may be necesary for use of some intrinsic or opencl functions
////			bav = _aligned_malloc(bas, 4096);
////		}
////		convert_CA_CT_to_bit_array(csv, bav, olas);
////
////		if (DG) {
////			// advance original
////			for (int r = 0; r < ng; ++r)
////				ca_next_gen__sisd(cr, csv, csi);
////			// print original-array
////			printf("cs  ");
////			for (CA_CT* csc = csv; csc < csv + (int)(BPB * 1); ++csc) {
////				if ((csc - csv) % 8 == 0)
////					printf(".");
////				printf("%d", *csc);
////			}
////			printf("\n");
////		}
////		// advance bit-array
////		if (1) {
////			// LOOKUP Table
////			static UINT8* calt = NULL;
////			if (calt == NULL) {
////				calt = malloc(MAXUINT16);
////
////				// Precalculate all states of a space of 16 cells after 4 time steps.
////				// Ruleset: 2 states, 3 cells in a neighborhod
////				// After for time steps 8 cells (1 byte in binary representation) will remain
////				UINT16 i = 0;
////				while (1) {
////					UINT16 cs = i;	// cell-space
////									// get result byte
////					for (int tm = 0; tm < 4; ++tm) {
////						for (int p = 0; p < 14; ++p) {
////							// get result bit
////							int sr =
////								cr->mntl[0] * (1 & (cs >> (p + 0))) +
////								cr->mntl[1] * (1 & (cs >> (p + 1))) +
////								cr->mntl[2] * (1 & (cs >> (p + 2)));
////							// set result bit in cs
////							if (cr->rltl[sr] == 0)
////								cs &= ~((UINT16)1 << p);
////							else
////								cs |= (UINT16)1 << p;
////						}
////					}
////					// store result byte
////					calt[i] = (UINT8)cs;
////					// debug
////					//printf("CALT   %5d  %3d\n", i, calt[i]);
////					// inc / finish
////					if (i == MAXUINT16)
////						break;
////					++i;
////				}
////			}
////
////			// get next gen
////			//ng = min(4, ng);
////			ng = 4;
////			tm -= ng * scsz;
////			for (int bi = 0; bi < bas - 1; ++bi) {
////				UINT8* bya = bav;  // need to adress on byte-basis
////				bya += bi;
////
////				*bya = calt[*(UINT16*)bya];
////			}
////		}
////		else {
////			ng = min(16, ng);
////			tm -= ng * scsz;
////			// BOOLEAN go through half-blocks
////			for (int bi = 0; bi < bact * 2; ++bi) {
////				UINT8* bya = bav;  // need to adress on byte-basis
////				bya += bi * (BPB / 8 / 2);
////
////				if (0) {
////					register FBPT l0, l1, l2, l3;
////					l0 = *(FBPT*)bya;
////
////					for (int r = 0; r < ng; ++r) {
////						l1 = l0 >> 1;
////						l2 = l0 >> 2;
////						l1 = (l1 & l2) ^ l0;
////
////						l2 = ~(l0 >> 2);
////						l3 = l0 >> 1;
////						l2 = l2 & l3;
////						l0 = ~(l1 | l2);
////						//l = ~(((l >> 1) & (~(l >> 2))) | ((l >> 2) & ((l >> 1) ^ l)));
////					}
////					*(HBPT*)bya = (HBPT)l0;
////				}
////				else {
////					register FBPT l;
////					l = *(FBPT*)bya;
////
////					for (int r = 0; r < ng; ++r) {
////						//FBPT c = l >> 1,
////						//	r = l >> 2;
////						//l = ~((c & (~r)) | (r & (c ^ l)));
////						l = ~(((l >> 1) & (~(l >> 2))) | ((l >> 2) & ((l >> 1) ^ l)));
////					}
////					*(HBPT*)bya = (HBPT)l;
////				}
////			}
////		}
////		convert_bit_array_to_CA_CT(bav, csv, olas);
////
////		// print converted-back original array again
////		if (DG) {
////			printf("csR ");
////			for (CA_CT* csc = csv; csc < csv + (int)(BPB * 1); ++csc) {
////				if ((csc - csv) % 8 == 0)
////					printf(".");
////				printf("%d", *csc);
////			}
////			printf("\n\n");
////		}
////	}
}

void
liveanddrawnewcleanzoom(
	/*
	pixel-screen is a 4-byte rgba pixel array to draw to

	does scrolling itself!
	*/
	CA_RULE *cr,										// ca configuration 
	CA_CT *sc,											// space 
	int scsz,											// space-size 
	int tm,												// time - nr. cells (tm=width=line=1=full generation) to live 
	int res,											// reset-flag (because of the static vars)
	int de,												// drawing enabled
	UINT32 *pnv,										// pixel-screen first valid element
	const UINT32 *pni,									// pixel-screen first invalid element
	const int cnmd,										// computation-mode - 0 = CPU, 1 = GPU
	const unsigned int klrg,							// kernel-range (GPU mode)
	caDisplaySettings ds								// display settings
	)
{
	ds.lgm %= 2;

	/* OpenCL */
	static OCLCA oclca = { -1, NULL, NULL, NULL };
	if (cnmd == 3) {
		if (oclca.success != CL_SUCCESS)
			oclca = OCLCA_Init("../opencl-test.cl", "ca_next_gen");
	}

	ds.tmfs = 0;
	if (!tm)											// nothing to do if time is zero
		return;

	ds.vlpz = max(1, ds.vlpz);
	ds.hlpz = max(1, ds.hlpz);

	if (!ds.vlfs || !ds.hlfs)
		ds.stzm = 0;

	/* Check for maximum size of rule tables */
	if (cr->nns > LV_MAXRLTLSZ) {
		printf("ERROR max rule table size to small");
		return;
	}

	if (ds.sfcm && res)
		for (uint32_t *pnc = pnv; pnc < pni; ++pnc)
			*pnc = 0;// 0x333333;


					 //res = 1;
	ds.hlzm = max(1, ds.hlzm);
	ds.vlzm = max(1, ds.vlzm);
	ds.hlpz = max(1, ds.hlpz);
	ds.vlpz = max(1, ds.vlpz);

	//int hw = plw / 4;		// histogram width
	int hw = 50;//
	if (ds.sfcm)
		hw = max(0, ds.plw - ceil(sqrt(scsz)) * ds.hlpz);		// histogram width

	int tma = 0;										// time available (as stored in pixel buffer)
	const int clsz = sizeof(CA_CT);						// memory size of one cell
	int pbs = scsz / ds.hlzm;								// pixel-buffer size
															//if (scsz % hlzm)
															//	++pbs;// += scsz % hlzm;

	ds.vlzm = max(ds.vlzm, ds.vlfs);								// vertical zoom can not be larger than zomm since it not possible (atm) to go back to previous generatios=lines
	if (ds.vlfs == 0 || ds.hlfs == 0)
		ds.hlfs = ds.vlfs = 0;

	/* size of buffer which starts right behind csv + scsz -- called 'border'
	buffer must be large enough to always allow for readahead for next-gen-calculation (cr->ncn)
	or drawing-calucaltions (hlfs) */
	int mnbrsz = max(ds.hlfs - 1, cr->ncn - 1);		// minimum border-size
	// mxbrsz should be at least 4096, scsz + mxbrsz should be multiple of 256
	int mxbrsz = (scsz + 4096 + 256 - 1) / 256 * 256;

	UINT32 *pnc = pni;									// pixel-screen cursor / current position 
	int sm = 0; // stripe mode

	if (sm) {
		// paint screen background gray
		if (res)
			for (UINT32 *tpnc = pnv; tpnc < pni; ++tpnc)
				*tpnc = 0;// 0x333333;
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

	static UINT32 *spnc = NULL;	// static pixel screen cursor; needed for stripe mode
	if (!spnc || res)
		spnc = pnv;

	if (ds.sfcm)
		pnc = pnv;
	else {
		/* Scroll pixel-screen to make room to draw tm pixels */
		int vlsl = tm / max(1, scsz * ds.vlzm) * ds.vlpz;				// vertical scroll / lines to scroll
		tm = vlsl * ds.vlzm * scsz / ds.vlpz;// tm / (vlzm * scsz) * (vlzm * scsz);
		int hlsl = 0;										// horizontal scroll
															///if (pbs == plw) {									// horizontal scrolling is (atm) only possible when output- and display-width are the same 
															///	hlsl = min(plw, (tm % (scsz * vlzm)) / max(1, vlzm * hlzm));				// horizontal scroll / nr. of pixels to scroll
															///}
		if (sm) {
			pnc = spnc;
		}
		else if (vlsl * ds.plw < pni - pnv) {
			pnc = max(pnv, pni - vlsl * ds.plw - hlsl);			// set cursor to first pixel that has to be drawn
			scroll(pnv, (pni - pnv) * 4, -(pni - pnc) * 4);		// scroll measures in bytes - 1 argb-pixel = 4 byte
		}
		else
			pnc = pnv;
	}
	//if (!sm && pbs * hlpz < plw)
	//	for (UINT32 *tpnc = pnc; tpnc < pni; ++tpnc)
	//		*tpnc = 0x333333;
	if (pbs != ds.plw) {
		tm = tm / scsz * scsz;
		//printf("tm %d sl %d/%d  plw %d   pc %d %d %d\n", tm, vlsl, hlsl, plw, pnc - pnv, pni - pnv, pni - pnc);		// scroll measures in bytes - 1 argb-pixel = 4 byte
	}

	/* Init pixel-acumulator-buffer - used when time-focus is activated */
	static UINT32 *abv = NULL;							// pixel-buffer first valid element
	static UINT32 *abi = NULL;							// pixel-buffer first invalid element
	static UINT32 *abc = NULL;							// pixel-buffer cursor / current element
	if ((!abv | res) && ds.tmfs > 0) {
		_aligned_free(abv);								// aligned memory management may be necesary for use of some intrinsic functions
		abv = _aligned_malloc(pbs * 4, 4096);
		memset(abv, 0, pbs * 4);
		abi = abv + pbs;
		abc = abi;
	}
	/* Init pixel-buffer */
	static UINT32 *pbv = NULL;							// pixel-buffer first valid element
	static UINT32 *pbi = NULL;							// pixel-buffer first invalid element
	static UINT32 *pbc = NULL;							// pixel-buffer cursor / current element
	if (!pbv | res) {
		_aligned_free(pbv);								// aligned memory management may be necesary for use of some intrinsic functions
		pbv = _aligned_malloc(pbs * 4, 4096);
		pbi = pbv + pbs;
		pbc = pbi;
	}
	/* Init cell-space buffer */
	static CA_CT *csv = NULL;							// cells first valid element
	static CA_CT *csi = NULL;							// cells first invalid element
	static CA_CT *csf = NULL;							// position of first cell, corrected for shifting
	if (!csv | res) {
		_aligned_free(csv);								// aligned memory management may be necesary for use of some intrinsic or opencl functions
		csv = _aligned_malloc((scsz + mxbrsz) * clsz, 4096);
		csi = csv + scsz;
		memcpy(csv, sc, scsz * clsz);
		csf = csv;
	}


	// workflow: 
	// analyze		- only whole cell-spaces / lines;	nr. of lines depends on vlpz;	counts states as set in hlfs, vlfs, fsme
	// render		- only whole cell-spaces / lines;	when enough cells have been analyzed, they are renderes
	// display		- zoom possible; parts smaller than cell-space can be displayed
	// evolve		- only whole cell-spaces / lines


	// available space on screen to display, in nr of cells - can include cells are outside screen borders and therefore wouldn't be visible
	int assn;
	if (ds.sfcm == 0)
		assn = 0;			// todo
	else
		assn = scsz;		// todo what if time < scsz / allow scrolling for screen-filling-curves?
	// available cells ready to draw in pixel-buffer
	static int acpb = 0;
	// available cells ready to render to pixel-buffer
	//static int acrr = scsz;




	if (pnc >= pni)			// drawing enabled
		de = 0;
	int vlp = 0;		// vertical position
	while (1) {
		// Display. Transfer as much pixels as possible from the pixel-buffer to the pixel-screen 
		if (acpb > 0) {
			//ca_display_line(tm, sm, ds, pnv, pni, pbv, pbi, res, spnc, pbs, hw);
			pnc = ca_display_line(tm, sm, ds, pnc, pni, pbv, pbi, res, spnc, pbs, hw);
			if (ds.sfcm > 0) {
				de = 0;
				pbc = pbi;
				acpb = 0;
			}
			/// need to check here if finished - can happen when only parts of caspace are drawn
		}
		//
		if (tm <= 0)
			break;
		//
		int ng;
		// Analyze & Evolve & Render.
		if (de) {
			/// ! tmfs > 0 not working / disabled atm
			// Reset pbv accumulation buffer.
			if (vlp == 0)
				memset(pbv, 0, (pbi - pbv) * sizeof *pbv);
			// Analyze / count states
			if (vlp < ds.vlfs) {
				ca_count__simd(csv, csf, csi, pbv, pbi, ds.hlzm, ds.hlfs, ds.fsme, cr->ncs);
				/* tmfs > 0 / Accumulate states */
				////else {
				////	static ic = 0;
				////	if (ic >= ds.tmfs) {
				////		ic = 0;
				////		for (abc = abv; abc < abi; ++abc) {
				////			if (*abc >= cr->ncs)
				////				*abc -= cr->ncs;
				////			else
				////				*abc = 0;
				////		}
				////	}
				////	ic++;
				////	//
				////	ca_count__simd(csv, csf, csi, abv, abi, ds.hlzm, ds.hlfs, ds.fsme, cr->ncs);
				////	/* Accumulate states */
				////	if (ds.tmfs > 0) {
				////		for (pbc = pbv, abc = abv; pbc < pbi; ++pbc, ++abc)
				////			*pbc = max(dyss - 1, *abc);
				////	}
				////}
			}
			
			// Render.
			++vlp;
			ng = 1;
			if (vlp == ds.vlfs) {
				if (ds.sfcm > 0)
					ng = tm / scsz;
				else
					ng = 1 + ds.vlzm - ds.vlfs;
				render_ca_line(ds, pbv, pbi, res, cr->ncs);
				vlp = 0;
				//pbc = pbv;
				acpb = pbi - pbv;
			}
		}
		else
			ng = tm / scsz;
		// Evolve.
		ca_evolve(cr, csv, csi, scsz, mnbrsz, mxbrsz, ng, res, cnmd, klrg);
		tm -= ng * scsz;
		printf("ng  %d   tm  %d\n", ng, tm);
		/* Correct for shift */
		csf -= ng *cr->ncn / 2;
		while (csf < csv)
			csf += scsz;
	}

	/* Histogram */
	//if (hw > 0) {
	//	int plh = (pni - pnv) / ds.plw;
	//	int vs = 20;			// vertical spacing to screen border of histogram
	//	int hh = plh - 2 * vs;	// histogram height
	//	int dph = hh / 100;	// histogram height of one datapoint
	//	/* history */
	//	static uint16_t *hy = NULL;			// history
	//	static int hyc = 0;					// history count
	//	static int hyi = -1;				// history iterator
	//	if (hyc != hw * dyss) {
	//		hyc = hw * dyss;
	//		free(hy);
	//		hy = malloc(hyc * sizeof(*hy));
	//		memset(hy, 0, hyc * sizeof(*hy));
	//	}
	//	++hyi;
	//	if (hyi >= hw)
	//		hyi = 0;
	//	/* background */
	//	for (int v = 0; v < plh; ++v) {
	//		UINT32 cl = 0x333333;
	//		if (v >= vs && v < plh - vs)
	//			cl = getColor(1.0 / hh * (v - vs), ds.cm, ds.crct, ds.gm);
	//		for (int h = 0; h < hw; ++h) {
	//			pnv[v * ds.plw + h] = cl;
	//		}
	//	}
	//	/* states */
	//	//double rng = max(DBL_EPSILON, dmx - dmn);
	//	for (int md = 0; md < 2; ++md) {					// go through modes (see below where used)
	//		for (int i = 0; i < dyss; ++i) {
	//			//printf("Info  # %d  st %.2f  mn %.2f mx %.2f  sv %d\n", i, dsctbl[i], dmn, dmx, sv);
	//			int sbc = dsctbl[i] * hh;		// state bar center of y-histogram-cord

	//			// dont draw non existent states
	//			if (dsctbl[i] < 0.0001 && sctbl[i] == 0)
	//				continue;

	//			int sbt, sbb;								// ... top and bottom
	//			if (sbc - dph / 2 < 0)
	//				sbt = 0, sbb = dph;
	//			else if (sbc + dph / 2 >= hh)
	//				sbb = hh - 1, sbt = hh - 1 - dph;
	//			else
	//				sbt = sbc - dph / 2, sbb = sbc + dph / 2;
	//			sbt += vs, sbb += vs;
	//			//
	//			for (int v = sbt; v <= sbb; ++v) {
	//				int hx = 0.5 + hw * (i + 1) / (dyss + 1);
	//				if (md == 0)
	//					hx = hw;
	//				for (int h = 0; h < hx; ++h) {
	//					if (pnv[v * ds.plw + h] == 0x000000)
	//						pnv[v * ds.plw + h] = 0xFFFFFF;
	//					else
	//						pnv[v * ds.plw + h] = 0x0;
	//				}
	//			}
	//		}
	//	}

	//	/* history */
	//	for (int i = 0; i < dyss; ++i) {
	//		// store
	//		hy[i * hw + hyi] = 0xFFFF * dsctbl[i];
	//		// draw
	//		int hyt = hyi;
	//		int x = 0;
	//		int lv = -1;
	//		while (1) {
	//			int v = hy[i * hw + hyt] * hh / 0xFFFF + vs;
	//			v = max(0, v);
	//			v = min(plh, v);
	//			if (lv < 0)
	//				lv = v;
	//			for (int vv = min(v, lv); vv <= max(v, lv); ++vv) {
	//				//pnv[vv * plw + x] = 0xFFFFFF - pnv[vv * plw + x];
	//				if (pnv[vv * ds.plw + x] < 0x800000)
	//					pnv[vv * ds.plw + x] = 0xFFFFFF;
	//				else
	//					pnv[vv * ds.plw + x] = 0x0;
	//			}
	//			lv = v;
	//			++x;
	//			++hyt;
	//			if (hyt == hw)
	//				hyt = 0;
	//			if (hyt == hyi)
	//				break;
	//		}
	//	}
	//}

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
	CA_RULE *cr,		/* ca configuration */
	CA_CT *spc,			/* space */
	int spcsz,			/* space-size */
	int zoom,
	int tm,				/* time - nr. of steps/generations to live */
						//int ncr,		/* number of cells needed to calculate next generation (rule table lookup) */
	int ncd,			/* number of cells for drawing */
	UINT32 *px,		/* rgba pixel array to draw to */
	int res,			/* reset-flag (because of the static vars)*/
	UINT32 *pxbrd,
	UINT32 *pxfrst,
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
	static CA_CT *cs = NULL;
	if (!cs | res) {
		cs = malloc((nlc + spcsz + nrc) * soc);
		memcpy(cs + nlc, spc, spcsz * soc);
	}
	/* new space (next generation) */
	static CA_CT *ns = NULL;
	if (!ns) {
		ns = malloc((nlc + spcsz + nrc) * soc);
		memcpy(ns + nlc, spc, spcsz * soc);
	}

	/* color table and state count table */
	int nbh_states_cnt = (cr->ncs - 1) * 4/*nbh_cnt*/ + 1;    // count of different neighborhods
															  //int nbh_states_cnt = 1024;    // count of different neighborhods
	nbh_states_cnt = min(0xFFFF, nrc);
	sm = min(sm, nrc);
	static UINT32 *cltbl = NULL;
	if (!cltbl)
		cltbl = malloc(nbh_states_cnt * sizeof *cltbl);
	static int *sctbl = NULL;	// state count table
	if (!sctbl)
		sctbl = malloc(nbh_states_cnt * sizeof *sctbl);

	/* Calculate multiplication factors for summing the neighbour cells */
	// old todo text: use a lookup table here as well - could also be used for totalistic if all values would be 1
	static int *pmftbl = NULL;
	if (!pmftbl)
		pmftbl = malloc(cr->ncn * sizeof *pmftbl);
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
		CA_CT *ccs = cs + nlc - cr->ncn / 2;
		/* current cell in new space */
		register CA_CT *cns = ns + nlc;
		/* border of new space */
		register CA_CT *cnsb = cns + spcsz;

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
		CA_CT *ss;
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
	CA_CT *spc,
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
	CA_CT *pa;							// pattern array
	pa = malloc(ps * sizeof(*pa));		// allocate memory for pattern array
	for (int i = 0; i < ps; ++i)		// create random pattern
		pa[i] = rand() % sc;

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
	register CA_CT *psn = spc;					// position
	register CA_CT *bdr = spc + spcsz;			// border
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


void pixel_effect_hilbert(SIMFW *simfw) {
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
	uint32_t *px = simfw->sim_canvas;
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
		col = 255.0 / lgmx12 * log(abs(i - ar / 2)*lf + 1), col = col << 16 | col << 8 | col;

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
	int cnmd = 3;				// computation-mode - 0 = CPU, 1 = GPU
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
	ds.stzm = 1;
	ds.sfcm = 1;
	ds.sfcsw = 1;
	ds.hlfs = 1;
	ds.tmfs = 1;
	ds.vlfs = 1;
	ds.vlzm = 1;
	ds.hlzm = 1;
	ds.vlpz = 1;
	ds.hlpz = 1;

	uint64_t tm = 0;			/* time */

	int speed = ca_space_sz * rel_start_speed, x_shift = 0;

	CA_RULE cr = CA_RULE_EMPTY;		/* ca configuration */
									// rule 153 (3 states) und  147 (2 states absolut) ähnlich 228!
									// neuentdeckung: rule 10699 mit 3 nb
	cr.rl = 228;//  3097483878567;// 1598;// 228; // 228;// 3283936144;// 228;// 228;// 90;// 30;  // 1790 Dup zu 228! // 1547 ähnliche klasse wie 228
	cr.tp = TOT;
	cr.ncs = 3;
	cr.ncn = 3;

	/* Init key-bindings */
	{
		SIMFW_KeyBinding eikb;		// empty integer key binding
		eikb.name = "";
		eikb.description = "";
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
		nkb.name = "display-cumulative-mode";
		nkb.val = &ds.cmm;
		nkb.slct_key = SDLK_n;
		nkb.min = 0; nkb.max = 2;
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
		nkb.name = "display-test-mode";
		nkb.val = &ds.tm;
		nkb.slct_key = SDLK_F8;
		nkb.min = 0; nkb.max = 1;
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
		nkb.name = "computation-mode"; nkb.description = "0 = None, 1, 2 = CPU, 3 = GPU";
		nkb.val = &cnmd;
		nkb.slct_key = SDLK_F5;
		nkb.min = 0; nkb.max = 3;
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
		nkb.name = "color-mode"; nkb.description = "0: b&w, 1: gray, 2: color, 3: color + b&w";
		nkb.val = &ds.cm;
		nkb.slct_key = SDLK_x;
		nkb.min = 0; nkb.max = 3;
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
		nkb.name = "space-size";
		nkb.val = &ca_space_sz; //nkb.cgfg = &res;
		nkb.slct_key = SDLK_h;
		nkb.min = 256; nkb.max = 1024 * 1024 * 64;
		SIMFW_AddKeyBindings(&sfw, nkb);
		//
		nkb = eikb;
		nkb.name = "speed";
		nkb.val = &speed;
		nkb.slct_key = SDLK_g;
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


	/* Control-Mode */
	enum CLMD { CLMD_NONE, CLMD_RULE, CLMD_SCREEN } clmd = CLMD_NONE;


	int pitch = 0;

	cr = CA_Rule(cr);

	// initial ca-"line"
	CA_CT *ca_space = malloc(ca_space_sz * sizeof *ca_space);
	for (int i = 0; i < ca_space_sz; i++) {
		ca_space[i] = 0;
	}
	ca_space[ca_space_sz / 2] = 1;


	int mouse_speed_control = 0;

	/* SDL loop */
	while (1) {
		/* SDL event handling */
		SDL_Event e;
		int org_speed = speed;
		int last_ca_space_sz = ca_space_sz;
		while (SDL_PollEvent(&e)) {
			const UINT8 *kbst = SDL_GetKeyboardState(NULL); /* keyboard state */
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
						y_save = 1 + speed / ca_space_sz / sfw.sim_height  * ca_space_sz * sfw.sim_height;
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
						if (keysym == SDLK_9) {
							CA_CT *cc = ca_space;
							CA_CT *ci = ca_space + ca_space_sz;
							int sz = 100, sg = 900;
							while (cc + sz < ci) {
								// random cells
								for (int i = 0; i < sz; ++i) {
									*cc = rand() % cr.ncs;
									++cc;
								}
								// spacing
								cc += sg;
							}
						}
						else if (keysym == SDLK_0) {
							CA_CT *cc = ca_space;
							CA_CT *ci = ca_space + ca_space_sz;
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
				case SDLK_F11:
					SDL_SetWindowFullscreen(sfw.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
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
						ds.hlfs = ds.vlfs = ds.hlzm = ds.vlzm = min(ds.hlzm, ds.vlzm) + 1,
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
						ds.hlfs = ds.vlfs = ds.hlzm = ds.vlzm = min(ds.hlzm, ds.vlzm) - 1,
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
						cnt = rand() % 100;
					else
						cnt = pow(2.0, rand() % 10);
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
					if (kbms & KMOD_CTRL)
						speed = (sfw.sim_height * ca_space_sz + ca_space_sz) / 4;
					else
						speed = sfw.sim_height * ca_space_sz;
					break;
				case SDLK_HOME:
					speed = ca_space_sz;
					break;
				case SDLK_DELETE:
					speed = x_shift = 0;
					break;
				case SDLK_d:
					speed = MAXINT; // 1000000 * ca_space_sz;
					break;
				case SDLK_PAGEUP:
					if (kbms & KMOD_SHIFT)
						speed += ca_space_sz;
					else if (kbms & KMOD_ALT)
						speed += (ca_space_sz * sfw.sim_height + ca_space_sz) / 8;
					else
						speed *= 2;
					break;
				case SDLK_PAGEDOWN:
					if (kbms & KMOD_SHIFT)
						speed -= ca_space_sz;
					else if (kbms & KMOD_ALT)
						speed -= (ca_space_sz * sfw.sim_height + ca_space_sz) / 8;
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
					printf("!!!!!!!!!!!! %d %d\n", ca_space_sz, last_ca_space_sz);
					int nwsz = ca_space_sz;
					CA_CT *nwsc = NULL;									// new space
					nwsc = malloc(nwsz * sizeof *nwsc);					// memory allocation for new space
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

				//if (speed != org_speed)
				//	SIMFW_SetFlushMsg(&sfw, "speed  %d   y/x %5d/%5d",//  %.2f/%.2f",
				//		speed,
				//		speed / ca_space_sz, speed % ca_space_sz);
				//		//1.0 * ty / sfw.sim_height,
				//		//1.0 * tx / sfw.sim_width);

				if (1 && (fscd || szcd || speed != org_speed))
					SIMFW_SetFlushMsg(&sfw,
						"SPEED   %.4es\n        %.4ec\nTIME    %.4ec\n        %.4es\nSIZE    %.4ec\n\nS-ZOOM  %d x %d\nP-ZOOM  %d x %d\nFOCUS   %d x %d",
						(double)speed / ca_space_sz, (double)speed,
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
		//pixel_effect_moving_gradient(&sfw);
		//sfw_test_rand(&sfw);
		//pixel_effect_prototype_while(&sfw);





		UINT32 *ll = sfw.sim_canvas;

		/* Scroll */
		//SIMFW_Scroll(&sfw, 0, -x_shift);
		int espeed = speed;

		//espeed = ca_space_sz + 1;

		//espeed = min(espeed, sfw.sim_height * sfw.sim_width);
		espeed = max(espeed, 0);
		//printf("espeed %d\n", espeed);

		if (espeed <= sfw.sim_height * sfw.sim_width) {
			ll = sfw.sim_canvas + (sfw.sim_height * sfw.sim_width) - espeed; /* pos to start drawing */
																			 //// cleanzoom version does scrolling itself  SIMFW_Scroll(&sfw, 0, -espeed);
		}
		// print ca_space
		//CA_LiveAndDraw(rule_tbl, ca_space, ca_space_sz, speed, 20, 20, ll, (res--)>0, sfw.sim_canvas + sfw.sim_height * sfw.sim_width, sfw.sim_canvas);


		//pitch = ca_space_sz;
		//CA_LiveAndDrawR(&cr, ca_space, tspsz/*ca_space_sz*/, zoom, speed, 0xFF * 10, ll, (res--)>0, sfw.sim_canvas + sfw.sim_height * sfw.sim_width, sfw.sim_canvas, pitch, hlfs, ar);
		//liveanddrawnewclean(&cr, ca_space, ca_space_sz, 1.0/*zoom*/, espeed, 0, ll, (res--) > 0, sfw.sim_canvas + sfw.sim_height * sfw.sim_width, sfw.sim_canvas, pitch, hlfs, hlfssr, ar, cm, crct - 1, gm);

		ds.plw = sfw.sim_width;
		ds.pnp = pitch;

		liveanddrawnewcleanzoom(
			&cr,
			ca_space, ca_space_sz,
			espeed,
			(res--) > 0,
			de,
			sfw.sim_canvas,
			sfw.sim_canvas + sfw.sim_height * sfw.sim_width,
			cnmd, klrg, ds);
		//ar, ard, cm, crct - 1, gm, lgm, cmm, co, stzm, de, cnmd, klrg);
		//			pixel_effect_moving_gradient(&sfw);
		tm += espeed;

		/* Update status */
		SIMFW_UpdateStatistics(&sfw);
		SIMFW_SetStatusText(&sfw,
			"ZELLOSKOP   fps %.2f  #%.4e  sd %.2f  c# %.2e",
			1.0 / sfw.avg_duration,
			1.0 / sfw.avg_duration * speed,
			1.0 * speed / ca_space_sz,
			1.0 * tm);
		SIMFW_UpdateDisplay(&sfw);
	}

behind_sdl_loop:

	/* Save configuration */
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
	double *dpv;	// past values
	dpv = malloc(H * W *sizeof(*dpv));
	double *dfv;	// future values
	dfv = malloc(H * W *sizeof(*dfv));

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
			double *cc = dpv; // current cell in past values
			uint32_t *px = simfw.sim_canvas; // current pixel

			double ltmn = mn;	// last min
			double rg = max(1.0, mx - mn); // range
			mx = -DBL_MAX, mn = DBL_MAX;

			double *cfv = dfv; // current future value
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
			double *tmp;
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
	double *ans = malloc(scsz * sizeof(double));	// absolute numbers
	double *bans = malloc(scsz * sizeof(double));	// buffer for absolute numbers
	for (int i = 0; i < scsz; ++i)
		ans[i] = 1.0;
	double *rhlfs = malloc(scsz * sizeof(double));	// reflection factors
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
		UINT32 *px = simfw.sim_canvas + (simfw.sim_height - yspeed) * simfw.sim_width;
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
			double *sp;
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




















