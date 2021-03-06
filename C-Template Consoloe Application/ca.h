/* This file was automatically generated.  Do not edit! */
void ONED_DIFFUSION(void);
void NEWDIFFUSION();
void CA_MAIN(void);
void pixel_effect_hilbert(SIMFW *simfw);
void CA_RandomizeSpace(CA_CT *spc,int spcsz,int sc,int rc,int ps);
void CA_LiveAndDrawR(CA_RULE *cr,CA_CT *spc,int spcsz,int zoom,int tm,int ncd,UINT32 *px,int res,UINT32 *pxbrd,UINT32 *pxfrst,int pitch,int sm,int ar);
void liveanddrawnewcleanzoom(CA_RULE *cr,CA_CT *sc,int scsz,int64_t tm,int res,UINT32 *pnv,const UINT32 *pni,int de,caComputationMode cnmd,const unsigned int klrg,caDisplaySettings ds);
void print_bitblock_bits(FBPT *fbp,size_t num_elements,int elpr);
void print_bits(unsigned char *bytes,size_t num_bytes,int bypr);
void print_byte_as_bits(char val);
int dblcmpfunc(const void *a,const void *b);
void display_2d_matze(UINT32 *pnv,UINT32 *pni,const int plw,const int paw,const int vlpz,const int hlpz,const UINT32 *pbv,const UINT32 *pbi,const int md);
void display_2d_lindenmeyer(const int *lm,UINT32 *pnv,UINT32 *pni,const int plw,const int paw,int vlpz,int hlpz,const UINT32 *pbv,const UINT32 *pbi,const int lmsw,int lmdh,int *lmvs,int *lmhs,int *lmpbsz);
void init_lmas();
extern const int LMA_PEANO[];
extern const int LMA_SIERPINSKI_SQUARE[];
extern const int LMA_MOORE[];
extern const int LMA_HILBERT[];
extern const int LMA_CANTOR[];
extern const int LMA_SIERPINSKI_TRIANGLE[];
extern const int LMA_SIERPINSKI_ARROWHEAD[];
extern const int LMA_SIERPINSKI[];
extern const int LMA_TERDRAGON[];
extern const int LMA_TWINDRAGON[];
extern const int LMA_DRAGON[];
extern const int LMA_GOSPER[];
extern int *LMSA[LMS_COUNT];
void ca_count__simd(const CA_CT *csv,const CA_CT *csf,const CA_CT *csi,const UINT32 *pbv,const UINT32 *pbi,const int hlzm,const int hlfs,const int fsme,const int ncs);
void ca_next_gen_ncn3_sisd(CA_RULE *cr,CA_CT *clv,CA_CT *cli);
void ca_next_gen__sisd(CA_RULE *cr,CA_CT *clv,CA_CT *cli);
void ca_next_gen_abs_ncn3_ncs2_simd(CA_RULE *cr,CA_CT *clv,CA_CT *cli);
void ca_next_gen_ncn3_simd(CA_RULE *cr,CA_CT *clv,CA_CT *cli);
void ca_next_gen__simd(CA_RULE *cr,CA_CT *clv,CA_CT *cli);
void scroll(UINT8 *ayf,int ays,int d);
void test_parallel_add();
void test_simd_intrinsic();
void print_M256I_I32(__m256i t);
void print_S32IRS(S32IRS *p,int32_t mx);
S32IRS create_S32IRS(uint32_t ct);
void init_S32IRS(S32IRS *p);
void CA_PrintSpace(char **caption,CA_CT *spc,int sz);
CA_RULE CA_Rule(CA_RULE prl);
__inline void d2xy(int n,int d,int *x,int *y);
__inline int xy2d(int n,int x,int y);
__inline void ht_rot(int n,int *x,int *y,int rx,int ry);
__inline void MortonDecode3(uint32_t code,uint32_t *outX,uint32_t *outY,uint32_t *outZ);
__inline void MortonDecode2(uint32_t code,uint32_t *outX,uint32_t *outY);
__inline uint32_t MortonEncode3(uint32_t x,uint32_t y,uint32_t z);
__inline uint32_t MortonEncode2(uint32_t x,uint32_t y);
void CA_RULE_PrintInfo(CA_RULE *cr,char *os,size_t sgsz);
sprintfa_s(char *sg,size_t sgsz,const char *format,...);
void reverse(char s[]);
void OCLCA_RunVBA(int gnc,int res,OCLCA oclca,CA_RULE *cr,caBitArray vba,int sync_memory,cl_uint rg);
void OCLCA_Run(int ng,int res,OCLCA oclca,CA_RULE *cr,int sync_memory,const CA_CT *clv,const int scsz,const int brsz,cl_uint rg);
OCLCA OCLCA_Init(const char *cl_filename);
const char *ocl_strerr(int error);
__inline UINT64 ipow(UINT64 base,int exp);
char *read_file(const char *filename);
void convert_bit_array_to_CA_CT(FBPT *bav,CA_CT *csv,int ct);
void convert_CA_CT_to_bit_array(CA_CT *csv,FBPT *bav,int ct);
extern const char *const cm_names[CM_MAX];
extern CA_CNFN *const ca_cnitfns[CM_MAX];
extern CA_CNFN *const ca_cnfns[CM_MAX];
extern struct {
	char*	name;
	int		(*cnfn)(int, caBitArray*);
	void	(*infn)(caBitArray*);
}const ca_cnsgs[CM_MAX];
int CA_CNFN_VBA_1x256(int gnc,caBitArray *vba);
void CA_CNITFN_VBA_1x256(caBitArray *vba,CA_RULE *cr);
void CA_CNITFN_VBA_4x32(caBitArray *vba,CA_RULE *cr);
void CA_CNITFN_VBA_2x32(caBitArray *vba,CA_RULE *cr);
void CA_CNITFN_VBA_1x64(caBitArray *vba,CA_RULE *cr);
void CA_CNITFN_VBA_1x32(caBitArray *vba,CA_RULE *cr);
void CA_CNITFN_VBA_2x256(caBitArray *vba,CA_RULE *cr);
void CA_CNITFN_VBA_16x256(caBitArray *vba,CA_RULE *cr);
int CA_CNFN_OMP_TEST(int pgnc,caBitArray *vba);
void CA_CNITFN_OMP_TEST(caBitArray *vba,CA_RULE *cr);
int CA_CNFN_OMP_VBA_8x1x256(int pgnc,caBitArray *vba);
void CA_CNITFN_OMP_VBA_8x1x256(caBitArray *vba,CA_RULE *cr);
void CA_CNITFN_OMP_VBA_8x32(caBitArray *vba,CA_RULE *cr);
void CA_CNITFN_OPENCL(caBitArray *vba,CA_RULE *cr);
int CA_CNFN_DISABLED(int gnc,caBitArray *vba);
void CA_CNITFN_DISABLED(caBitArray *vba,CA_RULE *cr);
void convertBetweenCACTandCABitArray(CA_CT *csv,CA_CT *csi,caBitArray *ba,int dr);
void initCABitArray(caBitArray *ba,CA_RULE *rl);
__inline void CABitArrayPrepareOR(caBitArray *ba,int from,int to);
extern const CA_RULE CA_RULE_EMPTY;
