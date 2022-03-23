#include "stdafx.h"		// default includes (precompiled headers)
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <SDL.h>
#include "hp_timer.h"
#include "simfw.h"

const double ChromaticRatio = 1.059463094359295264562;
const double Tao = 6.283185307179586476925;

Uint32 sampleRate = 48000;
Uint32 floatStreamLength = 1024;
Uint32 msPerFrame;
double practicallySilent = 0.001;

Uint32 audioBufferLength = 48000;
float *audioBuffer;

SDL_atomic_t audioCallbackLeftOff;
Sint32 audioMainLeftOff;
Uint8 audioMainAccumulator;

SDL_AudioDeviceID AudioDevice;
SDL_AudioSpec audioSpec;

SDL_Event event;
SDL_bool running = SDL_TRUE;



double fy = 48000.0 / 128.0; // frequency
double fys = M_PI / 48000.0; // frequency step
int shift = 0;

SIMFW simfw = { 0 };


typedef struct {
	float *waveform;
	Uint32 waveformLength;
	double volume;
	double pan;
	double frequency;
	double phase;
} voice;
void logSpec(SDL_AudioSpec *as) {
	printf(
		" freq______%5d\n"
		" format____%5d\n"
		" channels__%5d\n"
		" silence___%5d\n"
		" samples___%5d\n"
		" size______%5d\n\n",
		(int)as->freq,
		(int)as->format,
		(int)as->channels,
		(int)as->silence,
		(int)as->samples,
		(int)as->size
		);
}

void audioCallbacku8(
	void *unused, 
	Uint8 *byteStream, 
	int bsl /* byte stream length */ ) {
	Uint8* u8Stream = (Uint8*)byteStream;
	Uint8 v;
	Uint32 i;
	static double p = 0.0;
	static int sp = 0;

	Uint32 *cp = simfw.sim_canvas;		// current pixel

	if (bsl <= simfw.sim_height * simfw.sim_width) {
		cp = simfw.sim_canvas + (simfw.sim_height * simfw.sim_width) - bsl; /* pos to start drawing */
		SIMFW_Scroll(&simfw, 0, -bsl - shift);
	}

	for (i = 0; i < bsl; i++) {
		switch (1) {
		case 1:
			if (sp % (int)fy)
				u8Stream[i] = 0xFF;
			else
				u8Stream[i] = 0x00;
			v = u8Stream[i];

			break;
		case 2:
			v = abs(0xF0 * sin(p) + 0.5f);
			u8Stream[i] = v;

			break;
		}
		*cp = v << 16 | v << 8 | v;
		++cp;

		p += fys;
		//printf("%d ", u8Stream[i]);
		if (++sp % 48000 == 0 | sp < bsl * 0)
		//if (1)
			printf("bsl %d sp %d p %f  sm %3d  fy %f\n", bsl, sp, p, u8Stream[i], fy);
	}


	*(cp - 1) = 0xFF0000;
	//cp = simfw.sim_canvas + (simfw.sim_height * simfw.sim_width) - bsl; /* pos to start drawing */
	//*cp = 0xFF0000;


	SIMFW_SetStatusText(&simfw,
		"Audio TEST   fps %.2f  bsl %d  fy %.2f  fys %.4f  len pi %.2f",
		1.0 / simfw.avg_duration,
		bsl,
		fy, fys,
		M_PI / fys);
	SIMFW_UpdateDisplay(&simfw);

}

int onExit(void) {
	SDL_CloseAudioDevice(AudioDevice);
	SDL_Quit();
	return 0;
}

int sound_main(int argc, char *argv[]) {
	/* Audio */
	SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_VIDEO);
	SDL_AudioSpec want;
	SDL_zero(want);

	/* Video */
	//SIMFW_Init(&simfw, "Audio Test", 512, 512, 0, 128, 128);
	SIMFW_Init(&simfw, "Audio Test", 512, 512, 0, 512, 512);
	pixel_effect_moving_gradient(&simfw);



	want.freq = sampleRate;
	want.format = AUDIO_U8;
	want.channels = 1;
	want.samples = floatStreamLength;
	want.callback = audioCallbacku8;

	AudioDevice = SDL_OpenAudioDevice(NULL, 0, &want, &audioSpec, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
	if (AudioDevice == 0) {
		printf("\nFailed to open audio: %s\n", SDL_GetError());
		return 1;
	}

	printf("want:\n");
	logSpec(&want);
	printf("audioSpec:\n");
	logSpec(&audioSpec);

	SDL_PauseAudioDevice(AudioDevice, 0);


	

	LONGLONG st = timeit_start();

	while (running) {
		//fy = 1.0/(timeit_duration_nr(st)) ;
		//printf("1  fy  %f\n", fy);


		while (SDL_PollEvent(&event)) {
			//printf("fy  %f\n", fy);

			if (event.type == SDL_QUIT) {
				running = SDL_FALSE;
			}
			else if (event.type == SDL_KEYDOWN) {
				printf("fy  %f\n", fy);
				//SDL_Keymod kbms = SDL_GetModState(); // keyboard mod state
				/* */
				int keysym = event.key.keysym.sym;
				switch (keysym) {
				case SDLK_PAGEUP:
					//fy *= 1.125;
					fy += 1.0;
					printf("fy  %f\n", fy);
					break;
				case SDLK_PAGEDOWN:
					//fy /= 1.125;
					fy -= 1.0;
					printf("fy  %f\n", fy);
					break;
				case SDLK_LEFT:
					--shift;
					break;
				case SDLK_RIGHT:
					++shift;
					break;
				}
				fys = M_PI / 48000.0 * fy;
			}
		}

		/* ... */
	}
	onExit();
	return 0;
}