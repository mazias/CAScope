#include "stdafx.h"		// default includes (precompiled headers)

#include "color.h"
#include <float.h>
#include "hp_timer.h"
#include "pcg_basic.h"			// Random Number Library

// defines for gcc
#define MAXUINT16 ((UINT16)~ ((UINT16)0))
#define MAXUINT32 ((UINT32)~ ((UINT32)0))
#define MAXUINT64   ((UINT64)~((UINT64)0))
#define MAXUINT     ((UINT)~((UINT)0))
#define MAXINT      ((INT)(MAXUINT >> 1))
#define MININT      ((INT)~MAXINT)
// end defines for gcc

#define SIMFW_TEST_TITLE	"MGM GRAND"
#define SIMFW_FONTSIZE			12
#define SIMFW_TEST_DSP_WIDTH	700
#define SIMFW_TEST_DSP_HEIGHT	700
#define SIMFW_TEST_DSP_WINDOW_FLAGS	0 //SDL_WINDOW_FULLSCREEN_DESKTOP
#define SIMFW_TEST_SIM_WIDTH	512
#define SIMFW_TEST_SIM_HEIGHT	512
#define SIMFW_TEST_DSP_WIDTH	1200
#define SIMFW_TEST_DSP_HEIGHT	800
#define SIMFW_TEST_FPS_LIMIT	0
//
#define SIMFW_TEST_SIM_WIDTH	1200
#define SIMFW_TEST_SIM_HEIGHT	800


//
#define SIMFW_MAX_KEYBINDINGS	0xFF
#define SIMFW_SDL_PIXELFORMAT	SDL_PIXELFORMAT_ARGB8888
#define SIMFW_SDL_TEXTUREACCESS	SDL_TEXTUREACCESS_STREAMING



//typedef struct SIZE {
//	int y, x;
//} SIZE;


#if INTERFACE
typedef struct tagSIMFW_KeyBinding {
	const char *name;					// should not contain white-space
	const char *description;
	const char* const* value_strings;				// pointer to array of strings - MUST hold a value for all indizes from min to max (or be null)
	//double *dbl_var;
	//int *int_var;
	int slct_key;
	double min;
	double max;
	double def;		// default
	double step; // step
	enum {KBVT_INT, KBVT_INT64, KBVT_DOUBLE} val_type;
	void* val;
	int* cgfg;	// change-flag - is set to one if val changed
	int wpad;	// wrap-around

} SIMFW_KeyBinding;
/* Simulation Framework Structure */
typedef struct tagSIMFW {
	TTF_Font *font;						// used for text output on graphic display
	SDL_Window *window;
	SDL_Surface *window_surface;
	int window_height, window_width;
	SDL_Renderer *renderer;				// interface to gpu
	SDL_Texture *sim_texture;			// texture/memory to store simulation canvas
	SDL_Texture *status_text_texture,
		*flush_msg_texture;
	UINT32 *sim_canvas;					// pixel access (ARGB/32bit) to simulation canvas
	int sim_height, sim_width;

	int max_fps;						// maximum frames per second
	double zoom;	// TODO use more meaningful name  scale_factor
	int frame_count;
	// timer vars
	LONGLONG hp_timer_ticks;
	double avg_duration;
	int avg_duration_smpcnt;	// count of samples averaged in avg_duration
	// key binding
	SIMFW_KeyBinding *key_bindings;
	int key_bindings_count;
} SIMFW;
#endif
#include "simfw.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// from: https://stackoverflow.com/questions/34842526/update-console-without-flickering-c
static HANDLE hOut = NULL;
// x is the column, y is the row. The origin (0,0) is top-left.
void SIMFW_ConsoleSetCursorPosition(int x, int y)
{
	// Get the Win32 handle representing standard output.
	// This generally only has to be done once, so we make it static.
	if (!hOut)
		hOut = GetStdHandle(STD_OUTPUT_HANDLE);

	COORD coord = { (SHORT)x, (SHORT)y };
	SetConsoleCursorPosition(hOut, coord);
}

void SIMFW_ConsoleCLS()
{
	// Get the Win32 handle representing standard output.
	// This generally only has to be done once, so we make it static.
	if (!hOut)
		hOut = GetStdHandle(STD_OUTPUT_HANDLE);

	CONSOLE_SCREEN_BUFFER_INFO csbi;
	COORD topLeft = { 0, 0 };

	// Figure out the current width and height of the console window
	if (!GetConsoleScreenBufferInfo(hOut, &csbi)) {
		// TODO: Handle failure!
		abort();
	}
	DWORD length = csbi.dwSize.X * csbi.dwSize.Y;

	DWORD written;

	// Flood-fill the console with spaces to clear it
	FillConsoleOutputCharacter(hOut, TEXT(' '), length, topLeft, &written);

	// Reset the attributes of every character to the default.
	// This clears all background colour formatting, if any.
	FillConsoleOutputAttribute(hOut, csbi.wAttributes, length, topLeft, &written);

	// Move the cursor back to the top left for the next sequence of writes
	SetConsoleCursorPosition(hOut, topLeft);
}

/* Prints error message and aborts program */
// source: https://www.lemoda.net/c/die/
void
SIMFW_Die(const char* format, ...)
{
	va_list vargs;
	va_start(vargs, format);
	vfprintf(stderr, format, vargs);
	va_end(vargs);
	exit(1);
}

/* outputs debug str to visual studio output window */
void
SIMFW_DbgMsg(const char *format, ...) {
	// local vars
	char status_text_str[1000];
	// access optional parameters
	va_list argptr;
	va_start(argptr, format);
	//
	vsprintf_s(status_text_str, sizeof status_text_str, format, argptr);
	//

	/* Output to console */
	if (1) {
		printf("%s", status_text_str);
	}
	else {
		/* Output to Visual Studio Output Window*/
		// Convert char* string to a wchar_t* string.
		size_t convertedChars = 0;
		wchar_t uc_status_text_str[1000];
		mbstowcs_s(&convertedChars, uc_status_text_str, 1000, status_text_str, _TRUNCATE);

		/* */
		OutputDebugString(uc_status_text_str);
	}

	/* Cleanup */
	va_end(argptr);
}


/*
Changing the zoom factor will (re-)initialize the simulation texture (and canvas)

Creates a texture which will be sized relatively to win_height/width.

texture will be freed if needed
*/
void
SIMFW_SetSimSize(SIMFW *simfw, int height, int width) {
	/* Free old texture */
	if (simfw->sim_texture)
		SDL_DestroyTexture(simfw->sim_texture);

	/* Store new size */
	// default size is the window size
	if (!height && !width)
		SDL_GetWindowSize(simfw->window, &width, &height);
	SDL_RendererInfo renderer_info;// = { 0 };
	SDL_GetRendererInfo(simfw->renderer, &renderer_info);
	simfw->sim_height = min(renderer_info.max_texture_height, height);
	simfw->sim_width = min(renderer_info.max_texture_width, width);

	/* Create the new texture */
	if (!(simfw->sim_texture = SDL_CreateTexture(simfw->renderer, SIMFW_SDL_PIXELFORMAT, SIMFW_SDL_TEXTUREACCESS, simfw->sim_width, simfw->sim_height))) {
		printf("SDL_Create_Texture failed: %s\n", SDL_GetError());
		simfw->sim_height = simfw->sim_width = 0;
	}

	/* Allow access to pixels of texture */
	int pitch;
	SDL_LockTexture(simfw->sim_texture, NULL, &simfw->sim_canvas, &pitch);	// lock texture to be able to access pixels

	/* Reset average fps timer and samples counter, since it will likely change */
	simfw->hp_timer_ticks = timeit_start();
	simfw->avg_duration_smpcnt = 0;

	/* Debug output */
	printf("%s  win h %d w %d  sim h %d w %d  texture %p\n", __FUNCTION__, simfw->window_height, simfw->window_width, simfw->sim_height, simfw->sim_width, simfw->sim_texture);
}


/*
simular to SDL_GetMouseState but position is returned scaled/according to zoom and y and x parameter position is excahnged
*/
Uint32
SIMFW_GetMouseState(SIMFW *sfw, int *y, int *x) {
	Uint32 bs; // mouse button state
	bs = SDL_GetMouseState(x, y);
	if (y && x)
		*x = (double)sfw->sim_width / sfw->window_width * *x,
		*y = (double)sfw->sim_height / sfw->window_height * *y;
	return bs;
}


/* Displays all key bindings */
void
SIMFW_DisplayKeyBindings(SIMFW *sfw) {
	char val[100] = "";
	char buffer[16000] = "";
	int j = 0;												// nr. of byte written to buffer

	SIMFW_KeyBinding *kb = sfw->key_bindings;
	while (kb < sfw->key_bindings + sfw->key_bindings_count) {
		if (kb->value_strings != NULL)
			sprintf_s(val, sizeof(val), "%s", kb->value_strings[*(int*)kb->val]);
		else if (kb->val_type == KBVT_INT)
			sprintf_s(val, sizeof(val), "%5d", *(int*)kb->val);
		else if (kb->val_type == KBVT_INT64)
			sprintf_s(val, sizeof(val), "%5I64d", *(int*)kb->val);
		else if (kb->val_type == KBVT_DOUBLE)
			sprintf_s(val, sizeof(val), "%.3f", *(double*)kb->val);
		j += sprintf_s(buffer + j, sizeof(buffer) - j, "% 4s % 8s  %s %s\n", SDL_GetKeyName(kb->slct_key), val, kb->name, kb->description);
		kb++;

	}
	printf("%s", buffer);

	SIMFW_SetFlushMsg(sfw, buffer);
}


void
SIMFW_SaveBMP(SIMFW *sfw, const char *filename, int hide_controls) {
////	SDL_Window *window = NULL;
////	SDL_Renderer *renderer = NULL;
////
////	// init window
////	if (!(window = SDL_CreateWindow("Screenshot", 30, 30, sfw->sim_width, sfw->sim_height, 0))) {
////		printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
////	}
////	// init renderer
////	if (!(renderer = SDL_CreateRenderer(window, -1/*use default rendering driver*/, SDL_RENDERER_ACCELERATED))) {
////		printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
////	}
//////	windows->
//////	SDL_CreateSoftwareRenderer(window.)
////
////	SDL_RenderClear(renderer);
////
////	/* Copy simulation canvas on screen */
////	SDL_UnlockTexture(sfw->sim_texture);
////	if(SDL_RenderCopy(renderer, sfw->sim_texture, NULL, NULL < 0))
////		printf("SDL_RenderCopy failed: %s\n", SDL_GetError());
////	// re-allow access to pixels of texture
////	int pitch;
////	SDL_LockTexture(sfw->sim_texture, NULL, &sfw->sim_canvas, &pitch);	// lock texture to be able to access pixels
////
////
////	SDL_RenderCopy(renderer, sfw->flush_msg_texture, NULL, NULL);
////	//
////	SDL_RenderPresent(renderer);
	///SDL_Surface *sshot = SDL_CreateRGBSurface(0, sfw->sim_width, sfw->sim_height, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);

//	SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, sshot->pixels, sshot->pitch);
	//SDL_DestroyRenderer(renderer);
	//SDL_DestroyWindow(window);

	// Hide all overlay displays/controls
	if (hide_controls) {
		sfw->status_text_texture = NULL;
		sfw->flush_msg_texture = NULL;
		SIMFW_UpdateDisplay(sfw);
	}

	//
	SDL_Surface *sshot = SDL_CreateRGBSurface(0, sfw->window_width, sfw->window_height, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
	SDL_RenderReadPixels(sfw->renderer, NULL, SDL_PIXELFORMAT_ARGB8888, sshot->pixels, sshot->pitch);


	//if (MessageBox(NULL, "Save Screenshot?", 'MGM', MB_OKCANCEL) == 1) {
	char auto_filename[1000] = "";
	sprintf_s(auto_filename, sizeof(auto_filename), "%u.bmp", (unsigned)time(NULL));
	if (filename == NULL)
		filename = auto_filename;
	SDL_SaveBMP(sshot, filename);
	// save settings as well
	char settings_filename[1000] = "";
	strcat_s(settings_filename, sizeof(settings_filename), filename);
	strcat_s(settings_filename, sizeof(settings_filename), ".settings.txt");
	SIMFW_SaveKeyBindings(sfw, settings_filename);
	// user info
	SIMFW_SetFlushMsg(sfw, "screenshot saved as  %s\nsettings saved as  %s", filename, settings_filename);

	// cleanup
	SDL_FreeSurface(sshot);
}


/*
Saves all variables in keybindings to a file
*/
void
SIMFW_SaveKeyBindings(SIMFW *sfw, const char *filename) {
	FILE *stream;
	errno_t err = fopen_s(&stream, filename, "w");
	if (err)
		SIMFW_DbgMsg("The file  %s  could not be opened. Err: %d\n", filename, err);
	else {
		SIMFW_KeyBinding *kb = sfw->key_bindings;
		while (kb < sfw->key_bindings + sfw->key_bindings_count) {
			if (kb->val_type == KBVT_DOUBLE)
				fprintf_s(stream, "%s = %.20le\n", kb->name, *(double*)kb->val);
			if (kb->val_type == KBVT_INT)
				fprintf_s(stream, "%s = %d\n", kb->name, *(int*)kb->val);
			if (kb->val_type == KBVT_INT64)
				fprintf_s(stream, "%s = %I64d\n", kb->name, *(int64_t*)kb->val);

			SIMFW_DbgMsg("saved %s", kb->name);

			kb++;
		}
		fclose(stream);
	}
}


/*
Load all variables in keybindings to a file
*/
void
SIMFW_LoadKeyBindings(SIMFW *sfw, const char *filename) {
	FILE *stream;
	errno_t err = fopen_s(&stream, filename, "r");
	if (err)
		SIMFW_DbgMsg("The file  %s  could not opened. Err: %d\n", filename, err);
	else {
		const char name[100];
		while (1) {
			int nvr; // number of values read
			nvr = fscanf_s(stream, "%s = ", name, _countof(name));
			if (nvr == EOF)
				break;
			if (nvr != 1)
				continue;

			SIMFW_KeyBinding *kb = sfw->key_bindings;
			while (kb < sfw->key_bindings + sfw->key_bindings_count) {
				if (strcmp(kb->name, name) == 0) {
					if (kb->val_type == KBVT_DOUBLE) {
						double val;
						nvr = fscanf_s(stream, "%le\n", &val);
						if (nvr == 1) {
							*(double*)kb->val = val;
							SIMFW_DbgMsg("load  %s = %le\n", name, val);
						}
					}
					else if (kb->val_type == KBVT_INT) {
						int val;
						nvr = fscanf_s(stream, "%d\n", &val);
						if (nvr == 1) {
							*(int*)kb->val = val;
							SIMFW_DbgMsg("load  %s = %d\n", name, val);
						}
					}
					else if (kb->val_type == KBVT_INT64) {
						int64_t val;
						nvr = fscanf_s(stream, "%I64d\n", &val);
						if (nvr == 1) {
							*(int64_t*)kb->val = val;
							SIMFW_DbgMsg("load  %s = %I64d\n", name, val);
						}
					}
				}
				kb++;
			}

			kb++;
		}
		fclose(stream);
	}
}


/*
Add a key-binding
*/
void
SIMFW_AddKeyBindings(SIMFW *sfw, SIMFW_KeyBinding kb) {
	// abort if key-binding can not be added
	if (sfw->key_bindings_count == SIMFW_MAX_KEYBINDINGS) {
		SIMFW_DbgMsg("SIMFW_AddKeyBindings - can not add key-binding - SIMFW_MAX_KEYBINDINGS reached.");
		return;
	}
	//  add key-binding
	sfw->key_bindings_count++;
	sfw->key_bindings[sfw->key_bindings_count - 1] = kb;
}


void
SIMFW_HandleKeyBinding(SIMFW *sfw, int keysim) {
	const UINT8 *kbst = SDL_GetKeyboardState(NULL);			// keyboard state
	SDL_Keymod kbms = SDL_GetModState();					// keyboard mod state

	static SIMFW_KeyBinding *slkb = NULL;					// currently selected keybinding

	const char *action = "select";

	/* Select a keybinding */
	SIMFW_KeyBinding *kb = sfw->key_bindings;
	while (kb < sfw->key_bindings + sfw->key_bindings_count) {
		if (kb->slct_key == keysim) {
			// if already selected use to increase the value
			if (slkb == kb)
				keysim = SDLK_UP;
			// remember selected keybinding
			slkb = kb;
			goto keybinding_selected;
		}
		kb++;
	}

	// no keybinding selected
	/* Default keybindings */
	switch (keysim) {
	case SDLK_F1:
		SIMFW_DisplayKeyBindings(sfw);
		break;
	case SDLK_ESCAPE:
		SIMFW_SetFlushMsg(sfw, NULL);
		break;
	case SDLK_UP:
	case SDLK_RIGHT:
	case SDLK_DOWN:
	case SDLK_LEFT:
	case SDLK_RETURN:
		goto keybinding_selected;
	}
	return;

keybinding_selected:
	/* Modify vlues for the selected key binding */
	if (slkb) {
		double cv;	// current value
		double ov;	// original value
		/* Load current value */
		if (slkb->val_type == KBVT_DOUBLE)	cv = *(double*)	slkb->val;
		if (slkb->val_type == KBVT_INT)		cv = *(int*)slkb->val;
		if (slkb->val_type == KBVT_INT64)	cv = *(int64_t*)	slkb->val;
		ov = cv;
		/* Direction (up or down) of change */
		int dir = 0;
		if (keysim == SDLK_DOWN || keysim == SDLK_LEFT)
			dir = -1;
		else if (keysim == SDLK_UP || keysim == SDLK_RIGHT)
			dir = +1;
		/* Reset */
		if (keysim == SDLK_RETURN) {
			cv = slkb->def;
			action = "reset";
		}
		/* Change step size permanently */
		if (keysim == SDLK_RIGHT)
			slkb->step *= 2;
		else if (keysim == SDLK_LEFT)
			slkb->step /= 2;
		/* Change step size temporary */
		double cf = slkb->step;	/* change factor */
		// half of difference to min/max
		if (kbms & KMOD_LCTRL) {
			if (dir < 0)	cf = (cv - slkb->min) / 2.0;
			else			cf = (slkb->max - cv) / 2.0;
		}
		if (kbms & KMOD_SHIFT)
			cf /= 10.0;
		if (kbms & KMOD_RCTRL)
			cf *= 10.0;
		if (kbms & KMOD_ALT) {	// alt-key to set value to min/max
			if (dir > 0)	cf = cv;
			else			cf = cv / 2.0;
		}
		/* Increase/decrease value */
		/* Decrease */
		if (dir < 0) {
			action = "decrease";
			cv -= cf;
		}
		/* Increase */
		else if (dir > 0) {
			action = "increase";
			cv += cf;
		}
		/* Min/max handling */
		if (cv > slkb->max) {
			if (slkb->wpad) cv = slkb->min;
			else			cv = slkb->max;
		}
		if (cv < slkb->min) {
			if (slkb->wpad) cv = slkb->max;
			else			cv = slkb->min;
		}
		/* Change-flag */
		if (slkb->cgfg != NULL && ov != cv)
			*slkb->cgfg = 1;
		/* Value-String */
		char* vs[100];
		*vs = 0;
		if (slkb->value_strings != NULL) {
			sprintf_s(vs, 100, "%s  ", slkb->value_strings[(int)cv]);
		}
		/* Display user info */
		SIMFW_SetFlushMsg(sfw, "%s  %s   %s\nval  %s%e  %f  1/%e\n[%.2e-%.2e; %.2e]", action, slkb->name, slkb->description, vs, cv, cv, 1.0 / cv, slkb->min, slkb->max, slkb->step);
		/* Save current value */
		if (slkb->val_type == KBVT_DOUBLE)	*(double*)slkb->val	= cv;
		if (slkb->val_type == KBVT_INT)		*(int*)slkb->val = cv;
		if (slkb->val_type == KBVT_INT64)	*(int64_t*)slkb->val = cv;
	}
}


void
SIMFW_RenderStringOnTexture(SIMFW *simfw, SDL_Texture **txt, const char *string, int width) {
	/* Init */
	// local vars
	SDL_Surface *status_text_srfc = NULL;
	// free old status text texture
	if (*txt)
		SDL_DestroyTexture(*txt);

	if (string == NULL)
		return;

	/* Render text to surface; convert surface to texture; render texture to display */
	SDL_Color color = { 0xFF, 0xFF, 0xFF, 0xFF };

	/* Get max line width */
	char *c = string;
	int ml = 0, cl = 0;
	while (*c) {
		if (*c == '\n')
			cl = 0;
		else {
			++cl;
			if (cl > ml)
				ml = cl;
		}
		++c;
	}
	width = min(width, ml * 10);

	//status_text_srfc = TTF_RenderText_Solid(simfw->font, string, color);
	status_text_srfc = TTF_RenderText_Blended_Wrapped(simfw->font, string, color, width);

	SDL_SetColorKey(status_text_srfc, 0, 0);	// opaque background
	*txt = SDL_CreateTextureFromSurface(simfw->renderer, status_text_srfc);

	/* Cleanup */
	SDL_FreeSurface(status_text_srfc);
}

//void
//SIMFW_RenderStringOnARGBArray(SIMFW *simfw, SDL_Texture **txt, const char *string, int width) {
//}

void
SIMFW_SetStatusText(SIMFW *simfw, const char *format, ...) {
	// local vars
	char status_text_str[10000];
	// access optional parameters
	va_list argptr;
	va_start(argptr, format);
	//
	vsprintf_s(status_text_str, sizeof status_text_str, format, argptr);
	//
	SIMFW_RenderStringOnTexture(simfw, &simfw->status_text_texture, status_text_str, simfw->window_width);

	/* Cleanup */
	va_end(argptr);
}


void
SIMFW_SetFlushMsg(SIMFW *sfw, const char *format, ...) {
	if (format == NULL) {
		SDL_DestroyTexture(sfw->flush_msg_texture);
		sfw->flush_msg_texture = NULL;
		return;
	}

	// local vars
	char status_text_str[10000];
	// access optional parameters
	va_list argptr;
	va_start(argptr, format);
	//
	vsprintf_s(status_text_str, sizeof status_text_str, format, argptr);
	//

	SIMFW_RenderStringOnTexture(sfw, &sfw->flush_msg_texture, status_text_str, sfw->window_width * 3 / 4);

	/* Cleanup */
	va_end(argptr);
}


// TODO fehlerbehandlung wenn width/height groesser als bildschirmaufloesung
SIMFW_Init(SIMFW *simfw, const char *window_title, int window_height, int window_width, UINT32 window_flags, int sim_height, int sim_width) {
	SIMFW_DbgMsg("SIMFW - initializing...");

	/* Init SIMFW struct */
	//*simfw = { 0 };
	simfw->status_text_texture = NULL;
	simfw->flush_msg_texture = NULL;

	/* Init SDL */
	// init library
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("SDL_Init failed: %s\n", SDL_GetError());
		return simfw;
	}
	// get display info
	SDL_DisplayMode display_mode_info = { 0 };
	SDL_GetCurrentDisplayMode(0, &display_mode_info);
	// init window
	//if (!(simfw->window = SDL_CreateWindow(window_title, 20, 50, window_width, window_height, window_flags))) {
	if (!(simfw->window = SDL_CreateWindow(window_title, SDL_WINDOWPOS_CENTERED_DISPLAY(1), SDL_WINDOWPOS_CENTERED_DISPLAY(1), window_width, window_height, window_flags))) {
		printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
		return simfw;
	}
	// get size of window
	SDL_GetWindowSize(simfw->window, &simfw->window_width, &simfw->window_height);
	// renderer (interface to gpu); enable VSYNC: | SDL_RENDERER_PRESENTVSYNC
	if (!(simfw->renderer = SDL_CreateRenderer(simfw->window, -1/*use default rendering driver*/, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC))) {
	//if (!(simfw->renderer = SDL_CreateRenderer(simfw->window, -1/*use default rendering driver*/, SDL_RENDERER_ACCELERATED))) {
		printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
		return simfw;
	}
	// get window surface
	simfw->window_surface = SDL_GetWindowSurface(simfw->window);	// visible surface of the window
	// SDL hints / configuration
	//SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2"); // anti-alias
	// position window
	///disabled - above the macro SDL_WINDOWPOS_CENTERED_DISPLAY is used to center window
	///SDL_SetWindowPosition(simfw->window, display_mode_info.w - simfw->window_width, (display_mode_info.h - simfw->window_height) / 2); // horiz right vert center
	// init simulation canvas
	simfw->sim_height = simfw->sim_width = 0;
	if (sim_height < 0)
		sim_height = window_height / sim_height * -1;
	if (sim_width < 0)
		sim_width = window_width / sim_width * -1;
	SIMFW_SetSimSize(simfw, sim_height, sim_width);
	// timer vars
	simfw->hp_timer_ticks = timeit_start();
	simfw->avg_duration_smpcnt = 0;
	//
	simfw->avg_duration = 1; // prevent division by zero if simulation uses this to calculate fps

	/* Init TTF_Font */
	if (TTF_Init()) {
		printf("TTF_Init failed: %s\n", TTF_GetError());
		return simfw;
	}
	if (!(simfw->font = TTF_OpenFont("FreeMonoBold.ttf", SIMFW_FONTSIZE))) {
		printf("TTF_OpenFont failed: %s\n", TTF_GetError());
		// print current directory
		char* p;
		p = getcwd(NULL, 0);
		if (p != NULL)
		{
			printf("The current directory is: %s", p);
			free(p);
		}
		printf("\n(Press any key to continue)\n");
		getch();
	}

	/* Init key bindings */
	simfw->key_bindings_count = 0;
	simfw->key_bindings = malloc(SIMFW_MAX_KEYBINDINGS * sizeof(SIMFW_KeyBinding));

	/* Debug info */
	printf("%s  h %d  w %d\n", __FUNCTION__, window_height, window_width);
	printf("%s  Display #%d: current display mode is %dx%dpx @ %dhz. \n", __FUNCTION__, 0, display_mode_info.w, display_mode_info.h, display_mode_info.refresh_rate);

	/* Return newly created SIMFW struct */
	return simfw;
}


/* Waits until application is closed or key is pressed */
void
SIMFW_Wait(void) {
	while (1) {
		SDL_Event e;
		if (SDL_PollEvent(&e))
			if (e.type == SDL_QUIT || e.type == SDL_KEYUP)
				break;
		SDL_Delay(50);
	}
}

void
SIMFW_UpdateStatistics(SIMFW *simfw) {
	/* Calculate statistics like average fps; limit FPS */
	++simfw->frame_count;
	// Average frame duration
	double current_frame_duration = timeit_duration_nr(simfw->hp_timer_ticks);
	++simfw->avg_duration_smpcnt;
	if (current_frame_duration > 1.0) {
		simfw->avg_duration = current_frame_duration / (double)simfw->avg_duration_smpcnt;
		simfw->avg_duration_smpcnt = 0;
		simfw->hp_timer_ticks = timeit_start();
		///printf("fps  %.2f\n", 1.0 / simfw->avg_duration);
	}
}
/* Refreshes/updates the computer screen/display with content of simulation canvas */
void
SIMFW_UpdateDisplay(SIMFW *simfw) {
	// use windows canvas directly
	if (0) {
		simfw->sim_canvas = simfw->window_surface->pixels;
		SDL_UpdateWindowSurface(simfw->window);
		//sprintf_s(status_text_str, 100, "SW   fps %.2f  #%.2e", 100.0 / avg_duration, 100.0 / avg_duration * height * width);
		//status_text_srfc = TTF_RenderText_Solid(font, status_text_str, (SDL_Color){ 0xFF, 0xFF, 0xFF });
		//SDL_SetColorKey(status_text_srfc, 0, 0);	// opaque background
		//SDL_BlitSurface(status_text_srfc, NULL, surface, NULL);	// draw status on visible window surface
		//SDL_FreeSurface(status_text_srfc);

		return;
	}

	//SDL_RenderClear(simfw->renderer);	// as we are drawing the whole screen everytime we maybe do not need to call this

	/* Copy simulation canvas on screen */
	SDL_UnlockTexture(simfw->sim_texture);
	SDL_RenderCopy(simfw->renderer, simfw->sim_texture, NULL, NULL);
	// re-allow access to pixels of texture
	int pitch;
	SDL_LockTexture(simfw->sim_texture, NULL, &simfw->sim_canvas, &pitch);	// lock texture to be able to access pixels

	/* Copy status text to display */
	if (simfw->status_text_texture) {
		SDL_Rect dst_rct = { 0 };
		SDL_QueryTexture(simfw->status_text_texture, NULL, NULL, &dst_rct.w, &dst_rct.h);
		dst_rct.x = 0;// (simfw->window_width - dst_rct.w) / 2;
		// semi transparent background
		SDL_SetRenderDrawColor(simfw->renderer, 0x00, 0x00, 0x00, 0xE0);
		SDL_SetRenderDrawBlendMode(simfw->renderer, SDL_BLENDMODE_BLEND);
		SDL_RenderFillRect(simfw->renderer, &dst_rct);	/* opaque background */

		SDL_RenderCopy(simfw->renderer, simfw->status_text_texture, NULL, &dst_rct);
	}

	/* Copy flush msg to display */
	if (simfw->flush_msg_texture) {
		SDL_Rect dst_rct = { 0 };
		SDL_QueryTexture(simfw->flush_msg_texture, NULL, NULL, &dst_rct.w, &dst_rct.h);
		//dst_rct.x = (simfw->window_width - dst_rct.w) * 1 / 2;
		dst_rct.x = (simfw->window_width - dst_rct.w);
		dst_rct.y = simfw->window_height - dst_rct.h;// *7 / 10;
		SDL_RenderFillRect(simfw->renderer, &dst_rct);	/* opaque background */
		SDL_RenderCopy(simfw->renderer, simfw->flush_msg_texture, NULL, &dst_rct);
	}

	// wait, if FPS is limited
	//if (simfw->max_fps && current_frame_duration < 1.0 / simfw->max_fps) {
	//	SDL_Delay(1000.0 / simfw->max_fps - 1000.0 * current_frame_duration);
	//	while (current_frame_duration < 1.0 / simfw->max_fps)
	//		current_frame_duration = timeit_duration_nr(simfw->hp_timer_ticks);
	//}

	/* Refresh hardware display */
	SDL_RenderPresent(simfw->renderer);

}


void
SIMFW_Quit(SIMFW *simfw) {
	/* Cleanup */
	SDL_DestroyWindow(simfw->window);
	SDL_DestroyRenderer(simfw->renderer);
	TTF_Quit();
	SDL_Quit();
}


void pixel_effect_prototype(SIMFW *simfw) {
	/* loop through all pixels */
	int size = simfw->sim_width * simfw->sim_height;	// iterator outer loop border
	static int color = 0;
	//color = (color + 1) % 0xFF;
	//++color;
	for (int i = 0; i < size;) {
		int iib = i + simfw->sim_width;	// iterator inner loop border
		for (; i < iib; i++) {
			simfw->sim_canvas[i] = color++;
		}
	}
}

/*
Test for code that uses labels and gotos instead of loop contructs to have better readability
*/
void pixel_effect_prototype_goto(SIMFW *simfw) {
	static UINT32
		Sclr = 0; // static color
	register UINT32
		clr = Sclr, // color
		*psn = simfw->sim_canvas,	// position
		*bdr = psn + simfw->sim_width * simfw->sim_height;	// border
set_next_pixel:
	if (psn == bdr)
		goto all_pixels_set;
	*psn = clr;
	++clr;
	++psn;
	goto set_next_pixel;

all_pixels_set:
	Sclr = clr;
}


void
simfw_test_rand(SIMFW *simfw) {
	static pcg32_random_t pcgr;
	register UINT32 *psn = simfw->sim_canvas;	// position
	register UINT32 *bdr = psn + simfw->sim_width * simfw->sim_height;	// border

	while (psn < bdr) {
		//*psn += 1;
		//*psn = rand();
		*psn = pcg32_random_r(&pcgr);
		++psn;
	}
}

/*
sy	scroll in y direction
sx	scroll in x direction
*/
void
SIMFW_Scroll(SIMFW *sfw, int sy, int sx) {
	int
		d = sy * sfw->sim_width + sx,
		sz = sfw->sim_width * sfw->sim_height; // size

	if (d < 0) {
		d = abs(d);
		if (d >= sz)
			return;
		memmove(
			sfw->sim_canvas,
			sfw->sim_canvas + d,
			(sz - d) * sizeof *sfw->sim_canvas
			);

	}
	else if (d > 0) {
		if (d >= sz)
			return;
		memmove(
			sfw->sim_canvas + d,
			sfw->sim_canvas,
			(sz - d) * sizeof *sfw->sim_canvas
			);

	}
}

void pixel_effect_prototype_while(SIMFW *simfw) {
	static   UINT32 Sclr = 0; // static color
	register UINT32 clr = Sclr; // color
	register UINT32 *psn = simfw->sim_canvas;	// position
	register UINT32 *bdr = psn + simfw->sim_width * simfw->sim_height;	// border

	while (psn < bdr) {
		*psn = clr;
		++clr;
		++psn;
	}
	Sclr = clr;
}


void pixel_effect_moving_gradient(SIMFW* simfw) {
	// gradient should move smoothly
	static uint32_t color_shift = 0;
	uint32_t color_step;
	color_step = (double)MAXUINT32 / 5.0 * simfw->avg_duration;
	color_shift += color_step;

	/* loop through all pixels */
	int size = simfw->sim_width * simfw->sim_height;	// iterator outer loop border
	for (int i = 0; i < size;) {
		int iib = i + simfw->sim_width;	// iterator inner loop border
		uint32_t color = HSL_to_RGB_16b_2((MAXUINT32 / size * i + color_shift) >> 16, 0xFFFF, 0x8000);
		//uint32_t color = flp_ConvertHSLtoRGB(1.0 / size * i, 1.0, 0.5);
		for (; i < iib; i++) {
			simfw->sim_canvas[i] = color;
		}
	}
}


/*
demonstrates use of sdl texture pixel access
*/
void
SIMFW_Test() {
	/* Init */
	SIMFW simfw = { 0 };
	SIMFW_Init(&simfw, SIMFW_TEST_TITLE, SIMFW_TEST_DSP_HEIGHT, SIMFW_TEST_DSP_WIDTH, SIMFW_TEST_DSP_WINDOW_FLAGS, SIMFW_TEST_SIM_HEIGHT, SIMFW_TEST_SIM_WIDTH);

	/* SDL loop */
	while (1) {
		/* SDL event handling */
		SDL_Event e;
		if (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT)
				break;
			else if (e.type == SDL_KEYDOWN) {
				switch (e.key.keysym.sym) {
				case SDLK_PAGEUP:
					SIMFW_SetSimSize(&simfw, simfw.sim_height * 1.25, simfw.sim_width * 1.25);
					break;
				case SDLK_PAGEDOWN:
					SIMFW_SetSimSize(&simfw, simfw.sim_height * 0.75, simfw.sim_width * 0.75);
					break;
				}
			}
		}

		/* Manipulate simulation canvas pixels */
		pixel_effect_moving_gradient(&simfw);
		//simfw_test_rand(&simfw);
		//pixel_effect_prototype_while(&simfw);

		/* Update status and screen */
		SIMFW_SetStatusText(&simfw, "SIMFW TEST   fps %.2f  #%.4e", 1.0 / simfw.avg_duration, 1.0 / simfw.avg_duration * simfw.sim_height * simfw.sim_width);
		SIMFW_UpdateDisplay(&simfw);
	}

	/* Cleanup */
	SIMFW_Quit(&simfw);
}

// Return a random double value in the range [0, 1)
__inline double random01() {
	return ldexp(pcg32_random(), -32);
}

void
SIMFW_Test_Mandelbrot() {
	/* Init */
	SIMFW simfw = { 0 };
	SIMFW_Init(&simfw, SIMFW_TEST_TITLE, SIMFW_TEST_DSP_HEIGHT, SIMFW_TEST_DSP_WIDTH, SIMFW_TEST_DSP_WINDOW_FLAGS, SIMFW_TEST_SIM_HEIGHT, SIMFW_TEST_SIM_WIDTH);

	double zm = 3.0;		// zoom
	double tx = -0.75, ty = 0.0;
	int max_iteration = 16;
	tx = 0.0;

	double mn = 0.0, mx = max_iteration, mnimx = 0.0, mximx = max_iteration;
	double imn[128] = { 0 };
	int imax = 0, imin = 0;
	double dymxrg = 0.0;

	int crmd = 2;	// color-mode

	int wh = simfw.sim_width;							// width
	int ht = simfw.sim_height;							// height

	uint32_t* sc = NULL;								// space
	int sz = ht * wh;									// size of space
	if (sc == NULL) {
		sc = malloc(sz * sizeof(sc[0]));
		for (int i = 0; i < sz; ++i)
			sc[i] = 0;
	}

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
	nkb.name = "max-iteration";
	nkb.val = &max_iteration;
	nkb.slct_key = SDLK_o;
	nkb.min = 1; nkb.max = 4096;
	SIMFW_AddKeyBindings(&simfw, nkb);
	//
	nkb = edkb;
	nkb.name = "display-range";
	nkb.val = &dymxrg;
	nkb.slct_key = SDLK_i;
	nkb.min = -1e100; nkb.max = 1e100;
	SIMFW_AddKeyBindings(&simfw, nkb);
	//
	nkb = eikb;
	nkb.name = "color-mode";
	nkb.val = &crmd;
	nkb.slct_key = SDLK_x;
	nkb.min = 0; nkb.max = 50;
	SIMFW_AddKeyBindings(&simfw, nkb);

	/* SDL loop */
	while (1) {
		/* SDL event handling */
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT)
				break;
			else if (e.type == SDL_KEYDOWN) {
				switch (e.key.keysym.sym) {
				case SDLK_PAGEUP:
					//ty += zm / 1.25 / 2.0;
					//ty = (ty + zm / 2.0) - 1.0 / (zm * 1.25) / 2.0;
					//tx = (tx + zm / 2.0) - 1.0 / (zm * 1.25) / 2.0;
					zm /= 1.25;
					break;
				case SDLK_PAGEDOWN:
					zm *= 1.25;
					break;
				case SDLK_s:
					ty += zm / 16.0;
					break;
				case SDLK_w:
					ty -= zm / 16.0;
					break;
				case SDLK_d:
					tx += zm / 16.0;
					break;
				case SDLK_a:
					tx -= zm / 16.0;
					break;
				default:
					SIMFW_HandleKeyBinding(&simfw, e.key.keysym.sym);
				}
				printf("tx %f  ty %f  zm %f\n", tx, ty), zm;
				for (int i = 0; i < sz; ++i)
					sc[i] = 0;
			}
		}

		/* Manipulate simulation canvas pixels */

		if (0) {
			int itct = 20;			// iteration-count
			uint32_t inside = 0;
			uint32_t outside = 0;
			//for (int i = 0; i < sz; ++i)
			//	sc[i] = 0;
			for (int n = 0; n < 100000; ++n) {
				double x0 = tx - zm / 4.0 + random01() * zm / 2.0;
				double y0 = ty - zm / 4.0 + random01() * zm / 2.0;
				double dx = 0.0;
				double dy = 0.0;
				uint32_t iv = 1 << (itct - 1);
				while (iv) {
					double xtemp = dx * dx - dy * dy + x0;
					dy = 2 * dx * dy + y0;
					dx = xtemp;
					//
					///int x = (dx - tx + zm / 2.0) / zm * wh;
					///int y = (dy - ty + zm / 2.0) / zm * ht;
					int x = (dx - tx) / zm * wh + wh / 2.0;
					int y = (dy - ty) / zm * ht + ht / 2.0;
					if (x >= 0 && y >= 0 && x < wh && y < ht) {
						inside += iv;
						sc[y * wh + x] += iv;
					}
					else {
						outside += iv;
					}
					//
					iv = iv >> 1;
				}
			}

			//
			uint32_t mn = 0xFFFFFFFF, mx = 0;
			for (int i = 0; i < ht * wh; ++i) {
				//sc[i] = log2(sc[i]);
				if (sc[i] < mn)
					mn = sc[i];
				if (sc[i] > mx)
					mx = sc[i];
			}


			for (int y = 0; y < ht; ++y) {
				for (int x = 0; x < wh; ++x) {
					//if (mn > sc[y * wh + x])
					//	sc[y * wh + x] = mn;

					double v = 1.0 / log2(mx - mn + 1.0) * (log2(sc[y * wh + x] + 1.0) - log2(mn + 1.0));
					//double v = 1.0 / (mx - mn) * ((sc[y * wh + x]) - (mn));
					uint32_t color;
					color = 0xFF * v;
					color = color | color << 8 | color << 16;
					//color = getColor(v, 2, 0, 0);
					simfw.sim_canvas[y * wh + x] = color;
				}
			}

			printf("mn %u  mx %u  outside  %f\n", mn, mx, (double)outside / (inside + outside));
			//pixel_effect_moving_gradient(&simfw);
		}
		if (0) {
			/* loop through all pixels */
			double nmx = -DBL_MAX, nmn = DBL_MAX;
			int nimx = 0;// , nimn = max_iteration;	// iterator max
			double nimn[128] = { 0 };
			for (int i = 0; i < 128; ++i)
				nimn[i] = 0.0;
			for (int y = 0; y < simfw.sim_height; ++y) {
				for (int x = 0; x < simfw.sim_width; ++x) {

					double avg_iteration = 0.0;
					for (int sc = 0; sc < 32; sc++) {
						double x0 = tx - zm / 2.0 + zm * (double)x / (double)simfw.sim_width + random01() * zm / (double)simfw.sim_width;
						double y0 = ty - zm / 2.0 + zm * (double)y / (double)simfw.sim_width + random01() * zm / (double)simfw.sim_width;
						double dx = 0.0;
						double dy = 0.0;
						int iteration = 0;
						while (dx * dx + dy * dy < 4.0 && iteration < max_iteration) {
							///while (iteration < max_iteration) {
							double xtemp = dx * dx - dy * dy + x0;
							dy = 2 * dx * dy + y0;
							dx = xtemp;
							iteration++;
						}
						avg_iteration += iteration;
					}
					avg_iteration /= 32;

					uint32_t color = 0;
					if (avg_iteration < max_iteration) {
						double v = 1.0 / (max_iteration) * (avg_iteration);
						v *= pow(2.0, dymxrg / 10.0); v = v - floor(v);
						color = getColor(v, 2, 0, 0);
					}

					//else {
					//	double v = (log2(dx * dx + dy * dy + 1.0));
					//	if (nmn == 0.0 || v < nmn)
					//		nmn = v;

					//	v = max(v, mn);
					//	v = min(v, mn + 1.0);
					//	color = getColor(v - mn, 2, 0, 0);
					//}
					////else {
					////	int v = iteration;
					////	if (v > nimx)
					////		nimx = v;
					////	if (v < nimn)
					////		nimn = v;
					////	color = getColor(1.0 / (imx - imn) * (v - imn), 2, 0, 0);
					////	//color = 0;
					////}
					//else v = 1.0;
					///double v = x * dx + dy * dy;
					//v = log(v);

					simfw.sim_canvas[y * simfw.sim_width + x] = color;
				}
			}
			mx = nmx;
			mn = nmn;
			mx = min(mx, mn + pow(2.0, dymxrg / 10.0));
			printf("mn %e  mx %e  mnf %f\n", mn, mx, mn);
			imax = nimx;
			for (int i = 0; i < 128; ++i) {
				imn[i] = nimn[i];
			}
		}
		else if (0) {
			/* loop through all pixels */
			double nmx = -DBL_MAX, nmn = DBL_MAX;
			int nimin = max_iteration;
			int nimx = 0;// , nimn = max_iteration;	// iterator max
			double nimn[128] = { 0 };
			for (int i = 0; i < 128; ++i)
				nimn[i] = 0.0;
			for (int y = 0; y < simfw.sim_height; ++y) {
				for (int x = 0; x < simfw.sim_width; ++x) {
					double x0 = tx - zm / 2.0 + zm * (double)x / (double)simfw.sim_width;// * 2.47 - 2.0;
					double y0 = ty - zm / 2.0 + zm * (double)y / (double)simfw.sim_width;// *2.24 - 1.12;
					double dx = 0.0;
					double dy = 0.0;
					int iteration = 0;
					while (dx * dx + dy * dy < 4.0 && iteration < max_iteration) {
						///while (iteration < max_iteration) {
						double xtemp = dx * dx - dy * dy + x0;
						dy = 2 * dx * dy + y0;
						dx = xtemp;
						iteration++;
					}

					uint32_t color = 0;
					if (iteration < max_iteration) {
						//double v = (log2((dx - x0) * (dx - x0) + (dy - y0) * (dy - y0) + 1.0));


						////double v = (log2((dx) * (dx) + (dy) * (dy) + 1.0));

						////if (nimn[iteration] == 0.0 || v < nimn[iteration])
						////	nimn[iteration] = v;
						if (iteration < nimin)
							nimin = iteration;
						////v = max(v, imn[iteration]);
						////v = min(v, imn[iteration] + pow(2.0, dymxrg / 10.0));
						////v = 1.0 / (max_iteration - imin) * (iteration - imin + 1.0 - (v - imn[iteration]) / pow(2.0, dymxrg / 10.0));
						////

						double v = 1.0 / (max_iteration - imin - 1) * (iteration - imin);
						//v = log2(v);
						v *= pow(2.0, dymxrg / 10.0); v = v - floor(v);
						//v /= 4.0;


						color = getColor(v, 2, 0, 0);

						//color = getColor((v - imn[iteration]) / pow(2.0, dymxrg / 10.0), 2, 0, 0);
					}
					else if (0) {
						double v = (log2(dx * dx + dy * dy + 1.0));
						if (nmn == 0.0 || v < nmn)
							nmn = v;

						v = max(v, mn);
						v = min(v, mn + pow(2.0, dymxrg / 10.0));
						v = 1.0 / (mx - mn) * (v - mn);
						//v *= 8; v = v - floor(v);
						color = getColor(v, 2, 0, 0);
					}
					////else {
					////	int v = iteration;
					////	if (v > nimx)
					////		nimx = v;
					////	if (v < nimn)
					////		nimn = v;
					////	color = getColor(1.0 / (imx - imn) * (v - imn), 2, 0, 0);
					////	//color = 0;
					////}
					//else v = 1.0;
					///double v = x * dx + dy * dy;
					//v = log(v);

					simfw.sim_canvas[y * simfw.sim_width + x] = color;
				}
			}
			mx = nmx;
			mn = nmn;
			mx = mn + pow(2.0, dymxrg / 10.0);
			printf("mn %e  mx %e  mnf %f  dymxrf %e\n", mn, mx, mn, dymxrg);
			imax = nimx;
			imin = nimin;
			for (int i = 0; i < max_iteration; ++i) {
				//printf("i %d  imn[i] %e\n", i, nimn[i]);
				imn[i] = min(mx, nimn[i] + pow(2.0, dymxrg / 10.0));
			}
		}
		else if (1) {
			/* loop through all pixels */
			double nmx = -DBL_MAX, nmn = DBL_MAX;
			double nmximx = -DBL_MAX, nmnimx = DBL_MAX;
			int nimin = max_iteration, nimax = 0;
			double nimn[128] = { 0 };
			for (int i = 0; i < 128; ++i)
				nimn[i] = 0.0;
			for (int y = 0; y < simfw.sim_height; ++y) {
				for (int x = 0; x < simfw.sim_width; ++x) {
					double cx = tx - zm / 2.0 + zm * (double)x / (double)simfw.sim_width;// * 2.47 - 2.0;
					double cy = ty - zm / 2.0 + zm * (double)y / (double)simfw.sim_width;// *2.24 - 1.12;

					//#if 1
					//	{
					//		float c2 = dot(c, c);
					//		// skip computation inside M1 - http://iquilezles.org/www/articles/mset_1bulb/mset1bulb.htm
					//		if (256.0 * c2 * c2 - 96.0 * c2 + 32.0 * c.x - 3.0 < 0.0) return 0.0;
					//		// skip computation inside M2 - http://iquilezles.org/www/articles/mset_2bulb/mset2bulb.htm
					//		if (16.0 * (c2 + 2.0 * c.x + 1.0) - 1.0 < 0.0) return 0.0;
					//	}
					//#endif

					// iterate
					double di = 1.0;
					double zx = 0.0, tzx, zy = 0.0;
					double m2 = 0.0;
					double dzx = 0.0, tdzx, dzy = 0.0;
					double dotz;
					int i;
/*
					for (i = 0; i < max_iteration; i++) {
						if (m2 > 1024.0) {
							di = 0.0;
							break;
						}

						// Z' -> 2·Z·Z' + 1
						//dz = 2.0 * vec2(z.x * dz.x - z.y * dz.y, z.x * dz.y + z.y * dz.x) + vec2(1.0, 0.0);
						tdzx = 2.0 * zx * dzx - zy * dzy + 1.0;
						dzy = 2.0 * zx * dzy + zy * dzx + 0.0;
						dzx = tdzx;

						// Z -> Z² + c
						//z = vec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;
						tzx = zx * zx - zy * zy + cx;
						zy = 2.0 * zx * zy + cy;
						zx = tzx;

						m2 = zx * zx + zy * zy;
					}

					// distance
					// d(c) = |Z|·log|Z|/|Z'|
					///double d = 0.5 * sqrt((zx * zx + zy * zy) / (dzx * dzx + dzy * dzy)) * log((zx * zx + zy * zy));
					double d = sqrt((zx * zx + zy * zy) / sqrt(dzx * dzx + dzy * dzy)) * log(sqrt(zx * zx + zy * zy));
					//if (di > 0.5) d = 0.0;

					double v = (double)i + d;
					v = d;


					v = max(v, imn[i]);
					v = min(v, imn[i] + pow(2.0, dymxrg / 10.0));
					v = 1.0 / (max_iteration - imin) * (i - imin + 1.0 - (v - imn[i]) / pow(2.0, dymxrg / 10.0));
					//v = 1.0 / (max_iteration - imin - 1) * (i - imin);
*/



					double v;
					i = 0;
					double lx = 0.0, ly = 0.0, rd = 0.0;
					while (zx * zx + zy * zy <= (1<<16) && i < max_iteration) {
						tzx = zx * zx - zy * zy + cx;
						zy = 2.0 * zx * zy + cy;
						zx = tzx;

						rd += sqrt((zx - lx) * (zx - lx) + (zy - ly) * (zy - ly));
						lx = zx; ly = zy;

						i++;
					}

					// Used to avoid floating point issues with points inside the set.
					if (i < max_iteration) {
						// sqrt of inner term removed using log simplification rules.
						double log_zn = log(zx * zx + zy * zy) / 2.0;
						double nu = log(log_zn / log(2.0)) / log(2.0);
						// Rearranging the potential function.
						// Dividing log_zn by log(2) instead of log(N = 1<<8)
						// because we want the entire palette to range from the
						// center to radius 2, NOT our bailout radius.
						v = i + 1.0 - nu;
						if (v < nmn)	nmn = v;
						if (v > nmx)	nmx = v;

						v = max(v, mn);
						v = min(v, mx);

						v = 1.0 / (mx - mn) * (v - mn);
					}
					else {
						v = log(zx * zx + zy * zy + 1.0);

						v = log(rd + 1.0);

						//printf("%e  %e\n", log_zn, nu);
						if (v < nmnimx)	nmnimx = v;
						if (v > nmximx)	nmximx = v;
						v = max(v, mnimx);
						v = min(v, mximx);

						v = 1.0 / (mximx - mnimx) * (v - mnimx);
					}

					//if (i < nimin)	nimin = i;
					//if (i > nimax)	nimax = i;
					//if (nimn[i] == 0.0 || v < nimn[i])
					//	nimn[i] = v;

					//v *= 8; v = v - floor(v);

					double color = getColor(v, crmd, 0, 0);

					simfw.sim_canvas[y * simfw.sim_width + x] = color;
				}
			}
			mx = nmx;
			mn = nmn;
			mx = min(mx, mn + pow(2.0, dymxrg / 10.0));
			mximx = nmximx;
			mnimx = nmnimx;
			mximx = min(mximx, mnimx + pow(2.0, dymxrg / 10.0));
			//mximx = min(mximx, mnimx + 1.0);

			printf("mn %e  mx %e  mnimx %e  mximx %e  mnf %f  dymxrf %e\n", mn, mx, mnimx, mximx, mn, dymxrg);
			imin = nimin;
			imax = nimax;
			for (int i = 0; i < max_iteration; ++i) {
				//printf("i %d  imn[i] %e\n", i, nimn[i]);
				imn[i] = min(mx, nimn[i] + pow(2.0, dymxrg / 10.0));
			}
		}

		int msy, msx;
		//SIMFW_GetMouseState(&simfw, &msy, &msx);
		//ty = ty - zm / 2.0 + zm * (double)msy / simfw.sim_height;
		//tx = tx - zm / 2.0 + zm * (double)msx / simfw.sim_width;
		//zm /= 1.01;
		//SDL_WarpMouseInWindow(simfw.window, simfw.sim_width / 2, simfw.sim_height / 2);

		/* Update status and screen */
		SIMFW_UpdateStatistics(&simfw);
		SIMFW_SetStatusText(&simfw, "SIMFW TEST   fps %.2f  #%.4e", 1.0 / simfw.avg_duration, 1.0 / simfw.avg_duration * simfw.sim_height * simfw.sim_width);
		SIMFW_UpdateDisplay(&simfw);
	}
	/* Cleanup */
	SIMFW_Quit(&simfw);
}
