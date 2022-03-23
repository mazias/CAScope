#include "stdafx.h"

/* Floating point version of the hsl-to-rgb algorithm */
double flp_HSLConvertHueToRGB(double a, double b, double h)
{
	// wrap around; value range is 0-1
	h += h < 1.0;
	h -= h > 1.0;

	if (6.0 * h < 1.0)
		return a + (b - a) * 6.0 * h;
	if (2.0 * h < 1.0)
		return b;
	if (3.0 * h < 2.0)
		return a + (b - a) * 6.0 * (2.0 / 3.0 - h);
	return a;
}

/*
+ note the usage of .5 to round floating point values when converting to int, which is important for the result (http:\\c-faq.com/fp/round.html)
+ why is  float  version slower than  double  version????

*/
uint32_t flp_ConvertHSLtoRGB(double h, double s, double l)
{
	//    printf("flp hsl %.4f %.4f %.4f", h, s, l);

	uint8_t r = 0, g = 0, b = 0;
	// Grayscale
	//if (0 & !s)
	//	r = g = b = (l+.5) * 0xFF;
	// Chromatic
	{
		double va, vb;

		if (l <= 0.5)
			vb = l * (s + 1.0);
		else
			vb = (l + s) - (l * s);

		va = 2.0 * l - vb;
		r = flp_HSLConvertHueToRGB(va, vb, h + 1.0 / 3.0) * 0xFF + 0.5;
		g = flp_HSLConvertHueToRGB(va, vb, h) * 0xFF + 0.5;
		b = flp_HSLConvertHueToRGB(va, vb, h - 1.0 / 3.0) * 0xFF + 0.5;
	}

	// first byte, missing here, is alpha
	return (uint32_t)r << 16 | g << 8 | b; // todo: find out why  +  instead of  |   does not work
}







/*
Diese Funktion soll HSL in RGB umwandeln, ohne Fließkommazahlen zu benutzen.
HSL Werte werden mit 16bit Genauigkeit angegeben, wovon 12bit nötig sind um den kompletten RGB Farbraum zu erfassen.
(vgl.: https://code.google.com/p/streumix-frei0r-goodies/wiki/Integer_based_RGB_HSV_conversion "Due to the non-linear nature of the RGB to HSV (HSI) color space conversion, it's a well known fact that most conversion routines are utilizing floating-point arithmetic and store HSV values in floating point numbers, respectively. Furthermore, it's impossible to store full color infomation of RGB color space with a given numeric precicion in HSV space with identical precision. The non-linear transformation requires higher precision in HSV space to get a lossless, neutral RGB->HSV->RGB conversion chain. ")

*/
uint32_t HSL_to_RGB_16b_2(uint16_t hue, uint16_t sat, uint16_t lum) {
	UINT8 pr, pg, pb;
	uint16_t v, m, fract, vsf, mid1, mid2, r = 0, g = 0, b = 0;
	uint8_t sextant;

	UINT8 bits = 16;

	//hue >>= (16-bits); sat >>= (16-bits); lum >>= (16-bits);

	/*		
	if (l > .5)
		vb = (l + s) - (l * s);
	else
		vb = l * (1 + s);
	*/
	if (lum & 0x8000) // lum is in upper half of 0xFFFF
		v = ((lum + sat) << bits) - lum * sat >> bits;
	else
		v = lum * (0xFFFF + sat) >> bits;

	m = lum + lum - v;
	//hue *= 6;
	sextant = hue * 6 >> bits;    // note that  x = y * z >> n;  is not equal to  x = y * z; x >> n;
	fract = hue * 6 - (sextant << bits);
	vsf = fract * (v - m) >> bits;
	mid1 = m + vsf;
	mid2 = v - vsf;

	switch (sextant) {
	case 0:
		r = v;
		g = mid1;
		b = m;
		break;
	case 1:
		r = mid2;
		g = v;
		b = m;
		break;
	case 2:
		r = m;
		g = v;
		b = mid1;
		break;
	case 3:
		r = m;
		g = mid2;
		b = v;
		break;
	case 4:
		r = mid1;
		g = m;
		b = v;
		break;
	case 5:
		r = v;
		g = m;
		b = mid2;
		break;
	}

	pr = r >> (bits - 8);
	pg = g >> (bits - 8);
	pb = b >> (bits - 8);

	return (uint32_t)pr << 16 | pg << 8 | pb;
	//return (uint32_t)r<<8 | g | b>>8; // todo: find out why  +  instead of  |   does not work

	//printf("r %04x g %04x b %04x | v %04x  m %04x  vsf %04x  m1 %04x  m2 %04x  sx %d  fr %04x\n", r, g, b, v, m, vsf, mid1, mid2, sextant, fract);
}


/*
//### http://www.chilliant.com/rgb2hsv.html
//Interessant, andere color spaces
//
//fast Hue only
//void HUEtoRGB(unsigned int hue, unsigned char out[3])
//{
//int r,g,b;
//r = (abs(hue * (6<<8) - (3<<16)) - (1<<16)) >> 8;
//g = ((2<<16) - abs(hue * (6<<8) - (2<<16))) >> 8;
//b = ((2<<16) - abs(hue * (6<<8) - (4<<16))) >> 8;
//out[0] = MIN(255, MAX(r, 0));
//out[1] = MIN(255, MAX(g, 0));
//out[2] = MIN(255, MAX(b, 0));
//}
//
//
//### https://developer.mbed.org/forum/mbed/topic/1251/?page=1#comment-6216
///*---------------8-Bit-PWM--------------------------
//hue: 0 to 1529
//sat: 0 to 256 (0 to 255 with small inaccuracy)
//bri: 0 to 255
//all variables uint16_t
//*/
//
//while (hue > 1529) hue -= 1530;
//while (hue < 0) hue += 1530;
//
//if (hue < 255) {
//	red_val = 255;
//	green_val = (65280 - sat * (255 - hue)) >> 8;
//	blue_val = 255 - sat;
//}
//elseif(hue < 510) {
//	red_val = (65280 - sat * (hue - 255)) >> 8;
//	green_val = 255;
//	blue_val = 255 - sat;
//}
//elseif(hue < 765) {
//	red_val = 255 - sat;
//	green_val = 255;
//	blue_val = (65280 - sat * (765 - hue)) >> 8;
//}
//elseif(hue < 1020) {
//	red_val = 255 - sat;
//	green_val = (65280 - sat * (hue - 765)) >> 8;
//	blue_val = 255;
//}
//elseif(hue < 1275) {
//	red_val = (65280 - sat * (1275 - hue)) >> 8;
//	green_val = 255 - sat;
//	blue_val = 255;
//}
//else {
//	red_val = 255;
//	green_val = 255 - sat;
//	blue_val = (65280 - sat * (hue - 1275)) >> 8;
//}
//
//red = ((bri + 1) * red_val) >> 8;
//green = ((bri + 1) * green_val) >> 8;
//blue = ((bri + 1) * blue_val) >> 8;
//
//
///*-------8-Bit-PWM-|-Light-Emission-normalized------
//hue: 0 to 764
//sat: 0 to 128 (0 to 127 with small inaccuracy)
//bri: 0 to 255
//all variables int16_t
//*/
//
//while (hue > 764) hue -= 765;
//while (hue < 0) hue += 765;
//
//if (hue < 255) {
//	red_val = (10880 - sat * (hue - 170)) >> 7;
//	green_val = (10880 - sat * (85 - hue)) >> 7;
//	blue_val = (10880 - sat * 85) >> 7;
//}
//elseif(hue < 510) {
//	red_val = (10880 - sat * 85) >> 7;
//	green_val = (10880 - sat * (hue - 425)) >> 7;
//	blue_val = (10880 - sat * (340 - hue)) >> 7;
//}
//else {
//	red_val = (10880 - sat * (595 - hue)) >> 7;
//	green_val = (10880 - sat * 85) >> 7;
//	blue_val = (10880 - sat * (hue - 680)) >> 7;
//}
//
//red = (uint16_t)((bri + 1) * red_val) >> 8;
//green = (uint16_t)((bri + 1) * green_val) >> 8;
//blue = (uint16_t)((bri + 1) * blue_val) >> 8;
///*------------------12-Bit-PWM----------------------
//hue: 0 to 24569
//sat: 0 to 4096 (0 to 4095 with small inaccuracy)
//bri: 0 to 4095
//all variables uint32_t or int32_t
//*/
//
//while (hue > 24569) hue -= 24570;
//while (hue < 0) hue += 24570;
//
//if (hue < 4095) {
//	red_val = 4095;
//	green_val = (16773120 - sat * (4095 - hue)) >> 12;
//	blue_val = 4095 - sat;
//}
//elseif(hue < 8190) {
//	red_val = (16773120 - sat * (hue - 4095)) >> 12;
//	green_val = 4095;
//	blue_val = 4095 - sat;
//}
//elseif(hue < 12285) {
//	red_val = 4095 - sat;
//	green_val = 4095;
//	blue_val = (16773120 - sat * (12285 - hue)) >> 12;
//}
//elseif(hue < 16380) {
//	red_val = 4095 - sat;
//	green_val = (16773120 - sat * (hue - 12285)) >> 12;
//	blue_val = 4095;
//}
//elseif(hue < 20475) {
//	red_val = (16773120 - sat * (20475 - hue)) >> 12;
//	green_val = 4095 - sat;
//	blue_val = 4095;
//}
//else {
//	red_val = 4095;
//	green_val = 4095 - sat;
//	blue_val = (16773120 - sat * (hue - 20475)) >> 12;
//}
//
//red = ((bri + 1) * red_val) >> 12;
//green = ((bri + 1) * green_val) >> 12;
//blue = ((bri + 1) * blue_val) >> 12;
//
//
///*-------12-Bit-PWM-|-Light-Emission-normalized-----
//hue: 0 to 12284
//sat: 0 to 4096 (0 to 4095 with small inaccuracy)
//bri: 0 to 4095
//all variables int32_t
//*/
//
//while (hue > 12284) hue -= 12285;
//while (hue < 0) hue += 12285;
//
//if (hue < 4095) {
//	red_val = (5591040 - sat * (hue - 2730)) >> 12;
//	green_val = (5591040 - sat * (1365 - hue)) >> 12;
//	blue_val = (5591040 - sat * 1365) >> 12;
//}
//elseif(hue < 8190) {
//	red_val = (5591040 - sat * 1365) >> 12;
//	green_val = (5591040 - sat * (hue - 6825)) >> 12;
//	blue_val = (5591040 - sat * (5460 - hue)) >> 12;
//}
//else {
//	red_val = (5591040 - sat * (9555 - hue)) >> 12;
//	green_val = (5591040 - sat * 1365) >> 12;
//	blue_val = (5591040 - sat * (hue - 10920)) >> 12;
//}
//
//
//red = ((bri + 1) * red_val) >> 12;
//green = ((bri + 1) * green_val) >> 12;
//blue = ((bri + 1) * blue_val) >> 12;
//
//
//### http://web.mit.edu/storborg/Public/hsvtorgb.c
///* HSV to RGB conversion function with only integer
//* math */
//void
//hsvtorgb(unsigned char *r, unsigned char *g, unsigned char *b, unsigned char h, unsigned char s, unsigned char v)
//{
//	unsigned char region, fpart, p, q, t;
//
//	if (s == 0) {
//		/* color is grayscale */
//		*r = *g = *b = v;
//		return;
//	}
//
//	/* make hue 0-5 */
//	region = h / 43;
//	/* find remainder part, make it from 0-255 */
//	fpart = (h - (region * 43)) * 6;
//
//	/* calculate temp vars, doing integer multiplication */
//	p = (v * (255 - s)) >> 8;
//	q = (v * (255 - ((s * fpart) >> 8))) >> 8;
//	t = (v * (255 - ((s * (255 - fpart)) >> 8))) >> 8;
//
//	/* assign temp vars based on color cone region */
//	switch (region) {
//	case 0:
//		*r = v; *g = t; *b = p; break;
//	case 1:
//		*r = q; *g = v; *b = p; break;
//	case 2:
//		*r = p; *g = v; *b = t; break;
//	case 3:
//		*r = p; *g = q; *b = v; break;
//	case 4:
//		*r = t; *g = p; *b = v; break;
//	default:
//		*r = v; *g = p; *b = q; break;
//	}
//
//	return;
//}
//
//
//## http://rosettacode.org/wiki/Sierpinski_triangle/Graphical#C
//ich vermute hue only
//void h_rgb(long long x, long long y)
//{
//	rgb *p = &pix[y][x];
//
//#	define SAT 1
//	double h = 6.0 * clen / cscale;
//	double VAL = 1;
//	double c = SAT * VAL;
//	double X = c * (1 - fabs(fmod(h, 2) - 1));
//
//	switch ((int)h) {
//	case 0: p->r += c; p->g += X; return;
//	case 1:	p->r += X; p->g += c; return;
//	case 2: p->g += c; p->b += X; return;
//	case 3: p->g += X; p->b += c; return;
//	case 4: p->r += X; p->b += c; return;
//	default:
//		p->r += c; p->b += X;
//	}
//}
//
//## Variante die ich grade habe
//void HSL_to_RGB(int hue, int sat, int lum, int* r, int* g, int* b)
//{
//	int v;
//
//	v = (lum < 128) ? (lum * (256 + sat)) >> 8 :
//		(((lum + sat) << 8) - lum * sat) >> 8;
//	if (v <= 0) {
//		*r = *g = *b = 0;
//	}
//	else {
//		int m;
//		int sextant;
//		int fract, vsf, mid1, mid2;
//
//		m = lum + lum - v;
//		hue *= 6;
//		sextant = hue >> 8;
//		fract = hue - (sextant << 8);
//		vsf = v * fract * (v - m) / v >> 8;
//		mid1 = m + vsf;
//		mid2 = v - vsf;
//		switch (sextant) {
//		case 0: *r = v; *g = mid1; *b = m; break;
//		case 1: *r = mid2; *g = v; *b = m; break;
//		case 2: *r = m; *g = v; *b = mid1; break;
//		case 3: *r = m; *g = mid2; *b = v; break;
//		case 4: *r = mid1; *g = m; *b = v; break;
//		case 5: *r = v; *g = m; *b = mid2; break;
//		}
//	}
//}

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
		lum = lum * lumf;
		lum = lum - floor(lum);

		//lum = modf(lum * lumf, &lum); // modf seems to be expenisve
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
		huem = 0;
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
x	rgb component
*/
float
rgb_linear_to_gamma(float x)
{
	if (x >= 0.0031308)
		return 1.055 * powf(x, 1.0 / 2.4) - 0.055;
	else
		return 12.92 * x;
}

/*
x	rgb component
*/
__inline
float
rgb_gamma_to_linear(float x)
{
	if (x >= 0.04045)
		return powf((x + 0.055) / (1 + 0.055), 2.4);
	else
		return x / 12.92;
}



uint32_t
oklab_to_srgb(float L, float a, float b)
{
	float l_ = L + 0.3963377774f * a + 0.2158037573f * b;
	float m_ = L - 0.1055613458f * a - 0.0638541728f * b;
	float s_ = L - 0.0894841775f * a - 1.2914855480f * b;

	float l = l_ * l_ * l_;
	float m = m_ * m_ * m_;
	float s = s_ * s_ * s_;

	float flr = +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
	float flg = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
	float flb = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;

	float fsr = rgb_linear_to_gamma(flr);
	float fsg = rgb_linear_to_gamma(flg);
	float fsb = rgb_linear_to_gamma(flb);

	fsr = max(0.0, min(1.0, fsr));
	fsg = max(0.0, min(1.0, fsg));
	fsb = max(0.0, min(1.0, fsb));

	uint32_t ir = 255.0 * fsr + .5;
	uint32_t ig = 255.0 * fsg + .5;
	uint32_t ib = 255.0 * fsb + .5;

	//if (rand() == 0) {
		//printf("L  %.4f  a %.4f  b %.4f\n", L, a, b);
		//printf("c  255  lRGB  sRGB\n");
		//printf("r  %d   %3.4f  %3.4f\n", ir, flr, fsr * 255.0);
		//printf("g  %d   %3.4f  %3.4f\n", ig, flg, fsg * 255.0);
		//printf("b  %d   %3.4f  %3.4f\n", ib, flb, fsb * 255.0);
		//printf("\n");
	//}

	return ir << 16 | ig << 8 | ib;
}

struct Lab { float L; float a; float b; };
struct RGB { float r; float g; float b; };

/*
rgb		range 0.0 to 1.0
*/
struct Lab linear_srgb_to_oklab(float r, float g, float b)
{
	float l = 0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b;
	float m = 0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b;
	float s = 0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b;

	float l_ = cbrtf(l);
	float m_ = cbrtf(m);
	float s_ = cbrtf(s);

	struct Lab rlab;
	rlab.L = 0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_;
	rlab.a = 1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_;
	rlab.b = 0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_;
	return rlab;
}

/*
v		mix value, range 0.0 to 1.0
*/
__inline
float
mixf(float a, float b, float v) {
	return (a * ((float)1.0 - v)) + (b * v);
}

/*
mixes two rgb colors by using oklab color space
outbut is 4 byte (a)rgb value
TODO: SLOW
*/
uint32_t
oklab_mix_two_rgb_colors(float ar, float ag, float ab, float br, float bg, float bb, float mix) {
	struct Lab aL;
	aL = linear_srgb_to_oklab(rgb_gamma_to_linear(ar), rgb_gamma_to_linear(ag), rgb_gamma_to_linear(ab));
	struct Lab bL;
	bL = linear_srgb_to_oklab(rgb_gamma_to_linear(br), rgb_gamma_to_linear(bg), rgb_gamma_to_linear(bb));
	struct Lab mL;
	mL.L = mixf(aL.L, bL.L, mix);
	mL.a = mixf(aL.a, bL.a, mix);
	mL.b = mixf(aL.b, bL.b, mix);
	return oklab_to_srgb(mL.L, mL.a, mL.b);
}

/*
luminance range 0.0 to 1.0 suggestest, but more is possible
chroma range 0.0 to 1.0/3.0 suggestet, but more is possible
full hue range is from 0.0 to 1.0

use luminance = .8 and chroma = .1 to avoid capping rgb values and have color banding

https://www.shadertoy.com/view/WtccD7#
*/
uint32_t
oklab_lch_to_srgb(float luminance, float chroma, float hue)
{
	float theta = 2.0 * M_PI * hue;
	return oklab_to_srgb(luminance, chroma * cosf(theta), chroma * sinf(theta));
}



/*
v - value to make color for - has to be between 0 and 1
cm - color mode
crct - color count
gm - gradient mode
*/
UINT32
getColor(double v, int cm, int crct, int gm /* gradient mode */) {
	cm %= 34;

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
		///v = (double)((int)(v * (crct)+0.5)) / (crct);
		v = floor(v * (crct + 1)) / (crct + 1);

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

	/* b&w */
	if (cm == 0) {
		if (v >= 0.5)
			return 0xFFFFFF;
		else
			return 0x000000;
	}
	/* grayscale */
	else if (cm == 1) {
		UINT32 gv = 255.0 * v;
		return gv << 16 | gv << 8 | gv;
	}
	/* bw & color */
	else if (cm == 2) {
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
	/* color only */
	else if (cm == 3) {
		return HSL_to_RGB_16b_2(
			0xFFFF * v,
			0xFFFF,
			0x7FFF);
	}
	/* oklab */
	else if (cm == 4) {
		return oklab_lch_to_srgb(0.8, 1.0 / 3.0, v);
	}
	else if (cm == 5) {
		return oklab_lch_to_srgb(0.4, 1.0 / 3.0, v);
	}
	else if (cm >= 6 && cm < 6 + 28) {
		float cols[8][3] = {
			{1.0, 1.0, 1.0},
			{0.0, 0.0, 0.0},
			{1.0, 0.0, 0.0},
			{0.0, 1.0, 0.0},
			{0.0, 0.0, 1.0},
			{1.0, 1.0, 0.0},
			{1.0, 0.0, 1.0},
			{0.0, 1.0, 1.0},
		};
		int s = cm - 5;
		for (int c1 = 0; c1 < 7; ++c1) {
			for (int c2 = c1 + 1; c2 < 8; ++c2) {
				s--;
				if (s == 0)
					return oklab_mix_two_rgb_colors(cols[c1][0], cols[c1][1], cols[c1][2], cols[c2][0], cols[c2][1], cols[c2][2], v);
			}
		}
	}
}