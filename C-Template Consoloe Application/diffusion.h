/* This file was automatically generated.  Do not edit! */
void TWOD_DIFFUSION_FP(void);
void init_sint(CTP *ans,int h,int w);
void box_sparse(CTP *ans,int h,int w);
void circle_sint(CTP *ans,int h,int w);
void box_outline_sint(CTP *ans,int h,int w,int b,int ac);
void box_sint(CTP *ans,int h,int w);
__inline void MortonDecode2(uint32_t code,uint32_t *outX,uint32_t *outY);
__inline uint32_t YXE(uint32_t y,uint32_t x);
void TWOD_DIFFUSION(void);
int log2t(double d);
