/* This file was automatically generated.  Do not edit! */
UINT32 getColor(double v,int cm,int crct,int gm);
uint32_t oklab_lch_to_srgb(float luminance,float chroma,float hue);
uint32_t oklab_mix_two_rgb_colors(float ar,float ag,float ab,float br,float bg,float bb,float mix);
__inline float mixf(float a,float b,float v);
struct Lab linear_srgb_to_oklab(float r,float g,float b);
uint32_t oklab_to_srgb(float L,float a,float b);
__inline float rgb_gamma_to_linear(float x);
float rgb_linear_to_gamma(float x);
UINT32 getColorEx(double v,int huem,int satm,int lumm,double huef,double satf,double lumf);
uint32_t HSL_to_RGB_16b_2(uint16_t hue,uint16_t sat,uint16_t lum);
uint32_t flp_ConvertHSLtoRGB(double h,double s,double l);
double flp_HSLConvertHueToRGB(double a,double b,double h);
