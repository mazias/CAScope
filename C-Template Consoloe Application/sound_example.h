/* This file was automatically generated.  Do not edit! */
int sound_main(int argc,char *argv[]);
int onExit(void);
int init(void);
void audioCallback(void *unused,Uint8 *byteStream,int byteStreamLength);
extern Uint32 floatStreamLength;
void logWavedata(float *floatStream,Uint32 floatStreamLength,Uint32 increment);
void logVoice(voice *v);
void logSpec(SDL_AudioSpec *as);
void buildSineWave(float *data,Uint32 length);
int getWaveformLength(double pitch);
double getFrequency(double pitch);
void speak(voice *v);
extern SDL_bool running;
extern SDL_Event event;
extern SDL_AudioSpec audioSpec;
extern SDL_AudioDeviceID AudioDevice;
extern Uint8 audioMainAccumulator;
extern Sint32 audioMainLeftOff;
extern SDL_atomic_t audioCallbackLeftOff;
extern float *audioBuffer;
extern Uint32 audioBufferLength;
extern double practicallySilent;
extern Uint32 msPerFrame;
extern Uint32 samplesPerFrame;
extern Uint32 frameRate;
extern Uint32 sampleRate;
extern const double Tao;
extern const double ChromaticRatio;
