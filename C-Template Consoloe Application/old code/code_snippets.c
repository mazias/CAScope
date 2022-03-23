/* other pixel effects */
//	/* manipulate pixels */
//	// color for color-pulsing
//	////uint32_t pulse_color = flp_ConvertHSLtoRGB(1. / 10 / fps * (frames % ((int)fps * 10)), 1., .5);
//	// loop through all pixels
//	for (int i = 0; i < HEIGHT * WIDTH; i += TEST_STEP) {
//		// Moving gradient
//		////pixels[i] = HSL_to_RGB_16b_2((0xFFFFFFFF / (HEIGHT * WIDTH) * i + frames * 0xFFFFFF) >> 16, 0xFFFF, 0x8000);
//		//pixels[i] = flp_ConvertHSLtoRGB(1. / HEIGHT / WIDTH * i, 1., .5);
//
//		// Farbrauschen
//		//pixels[i] = rand() % 0xFF << 16 | rand() % 0xFF << 8 | rand() % 0xFF;
//
//		// Farbpulsieren
//		//pixels[i] = pulse_color;
//
//
//		//pixels[i] = 0xFF800000 | (++red+frames<<16);// rand();// 0xFFFFFFFF;
//	}
//
//}

/* Use of SDL2 Surface */
// fps here is double of texture-version-fps; i guess the reason is that the data has to be copied to gpu and back to windows
int sdl_surface_test(SDL_Window* window, TTF_Font* font) {
	/* Init */
	// get window size
	int height, width;
	SDL_GetWindowSize(window, &width, &height);
	//
	SDL_Surface *surface = SDL_GetWindowSurface(window);	// visible surface of the window	
	uint32_t *pixels = surface->pixels;	// typed pointer to access pixels of surface; type/size must be compatible with SDL_PIXELFORMAT
	//
	char status_text_str[1000];
	SDL_Surface *status_text_srfc = NULL;	// surface to which status text is rendered to
	// timer vars
	LONGLONG hp_timer_ticks = timeit_start();
	double avg_duration = 1;

	int frames = 0;

	/* SDL loop */
	while (1) {
		/* SDL event handling */
		SDL_Event e;
		if (SDL_PollEvent(&e))
			if (e.type == SDL_QUIT || e.type == SDL_KEYUP)
				break;

		/* Manipulate pixels */
		//pixel_effect_moving_gradient(pixels, height, width);
		//pixel_effect_prototype(pixels, height, width);
		//pixel_effect_sand(pixels, height, width, 0);
		pixel_effect_block_sand(pixels, height, width, 0);

		/* Get things on the screen */
		// status display
		if (!(++frames % 1)) {
			sprintf_s(status_text_str, 100, "SW   fps %.2f  #%.2e", 100.0 / avg_duration, 100.0 / avg_duration * height * width);
			status_text_srfc = TTF_RenderText_Solid(font, status_text_str, (SDL_Color){ 0xFF, 0xFF, 0xFF });
			SDL_SetColorKey(status_text_srfc, 0, 0);	// opaque background
			SDL_BlitSurface(status_text_srfc, NULL, surface, NULL);	// draw status on visible window surface
			SDL_FreeSurface(status_text_srfc);
		}
		//
		SDL_UpdateWindowSurface(window);

		/* Calc. avg. frame duration */
		avg_duration = avg_duration / 100.0 * 99.0 + timeit_duration(&hp_timer_ticks);
	}
}



// Get the current working directory: 
		char* buffer;
		if ((buffer = _getcwd(NULL, 0)) == NULL)
			perror("_getcwd error");
		else
		{
			printf("%s \nLength: %d\n", buffer, strnlen(buffer, 1024));
			free(buffer);
		}
