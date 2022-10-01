/* This file was automatically generated.  Do not edit! */
void SIMFW_Test_Mandelbrot();
__inline double random01();
void SIMFW_Test();
typedef struct tagSIMFW tagSIMFW;
typedef struct tagSIMFW_KeyBinding tagSIMFW_KeyBinding;
struct tagSIMFW_KeyBinding {
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
	enum {KBVT_INT, KBVT_DOUBLE} val_type;
	void* val;
	int* cgfg;	// change-flag - is set to one if val changed
	int wpad;	// wrap-around

};
typedef struct tagSIMFW_KeyBinding SIMFW_KeyBinding;
struct tagSIMFW {
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
};
typedef struct tagSIMFW SIMFW;
void pixel_effect_moving_gradient(SIMFW *simfw);
void pixel_effect_prototype_while(SIMFW *simfw);
void SIMFW_Scroll(SIMFW *sfw,int sy,int sx);
void simfw_test_rand(SIMFW *simfw);
void pixel_effect_prototype_goto(SIMFW *simfw);
void pixel_effect_prototype(SIMFW *simfw);
void SIMFW_Quit(SIMFW *simfw);
void SIMFW_UpdateStatistics(SIMFW *simfw);
void SIMFW_Wait(void);
SIMFW_Init(SIMFW *simfw,const char *window_title,int window_height,int window_width,UINT32 window_flags,int sim_height,int sim_width);
void SIMFW_SetStatusText(SIMFW *simfw,const char *format,...);
void SIMFW_RenderStringOnTexture(SIMFW *simfw,SDL_Texture **txt,const char *string,int width);
void SIMFW_HandleKeyBinding(SIMFW *sfw,int keysim);
void SIMFW_AddKeyBindings(SIMFW *sfw,SIMFW_KeyBinding kb);
void SIMFW_LoadKeyBindings(SIMFW *sfw,const char *filename);
void SIMFW_SaveKeyBindings(SIMFW *sfw,const char *filename);
void SIMFW_UpdateDisplay(SIMFW *simfw);
void SIMFW_SaveBMP(SIMFW *sfw,const char *filename,int hide_controls);
void SIMFW_SetFlushMsg(SIMFW *sfw,const char *format,...);
void SIMFW_DisplayKeyBindings(SIMFW *sfw);
Uint32 SIMFW_GetMouseState(SIMFW *sfw,int *y,int *x);
void SIMFW_SetSimSize(SIMFW *simfw,int height,int width);
void SIMFW_DbgMsg(const char *format,...);
void SIMFW_Die(const char *format,...);
void SIMFW_ConsoleCLS();
void SIMFW_ConsoleSetCursorPosition(int x,int y);
#define INTERFACE 0
