#include "stdafx.h"

double timeit_duration(LONGLONG *start_time);

LONGLONG timeit_start(void) {
	// get ticks
	LARGE_INTEGER ticks = { 0 };
	QueryPerformanceCounter(&ticks);
	return ticks.QuadPart;
}

/*
returns time in seconds between consecutive calls
*/
double timeit_duration(LONGLONG *start_time) {
	// query timer frequency if not done so already
	static __int64 timer_perf_freq = 0;
	if (!timer_perf_freq) {
		LARGE_INTEGER timeit_qp_pntr = { 0 };
		QueryPerformanceFrequency(&timeit_qp_pntr);
		timer_perf_freq = timeit_qp_pntr.QuadPart;
	}

	// get ticks
	LARGE_INTEGER ticks = { 0 };
	QueryPerformanceCounter(&ticks);

	// convert ticks to seconds
	double result = 1.0 / timer_perf_freq * (ticks.QuadPart - *start_time);
	// TODO put in extra function reset start time
	*start_time = ticks.QuadPart;

	return result;
}
/*
no reset version of fctn above
*/
double timeit_duration_nr(LONGLONG start_time_ticks) {
	// query timer frequency if not done so already
	static __int64 timer_perf_freq = 0;
	if (!timer_perf_freq) {
		LARGE_INTEGER timeit_qp_pntr = { 0 };
		QueryPerformanceFrequency(&timeit_qp_pntr);
		timer_perf_freq = timeit_qp_pntr.QuadPart;
	}

	// get ticks
	LARGE_INTEGER ticks = { 0 };
	QueryPerformanceCounter(&ticks);

	// convert ticks to seconds
	double result = 1.0 / timer_perf_freq * (ticks.QuadPart - start_time_ticks);

	return result;
}