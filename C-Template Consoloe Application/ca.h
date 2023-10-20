/* This file was automatically generated.  Do not edit! */
void ONED_DIFFUSION(void);
void NEWDIFFUSION();
void CA_MAIN(void);
void CA_RandomizeSpace(CA_CT *spc,int spcsz,int sc,int rc,int ps);
extern caDisplaySettings ds;
void lifeanddrawnewcleanzoom(CA_RULE *cr,CA_CT *sc,uint64_t scsz,int64_t tm,int res,int dyrt,UINT32 *pnv,const UINT32 *pni,int de,caComputationMode cnmd,const unsigned int klrg,caDisplaySettings ds);
int dblcmpfunc(const void *a,const void *b);
void display_2d_matze(UINT32 *pnv,UINT32 *pni,const int plw,const int paw,const int vlpz,const int hlpz,const UINT32 *pbv,const UINT32 *pbi,const int md,const int mdpm);
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
void scroll(UINT8 *ayf,int ays,int d);
CA_RULE CA_Rule(CA_RULE prl);
__inline void MortonDecode3(uint32_t code,uint32_t *outX,uint32_t *outY,uint32_t *outZ);
__inline void MortonDecode2(uint32_t code,uint32_t *outX,uint32_t *outY);
__inline uint32_t MortonEncode3(uint32_t x,uint32_t y,uint32_t z);
__inline uint32_t MortonEncode2(uint32_t x,uint32_t y);
void CA_RULE_PrintInfo(CA_RULE *cr,char *os,size_t sgsz);
sprintfa_s(char *sg,size_t sgsz,const char *format,...);
void reverse(char s[]);
extern const char *cm_names[CM_MAX];
extern struct {
	char* name;
	int64_t(*cnfn)(int64_t, caBitArray*);				// computation-function
	void		(*itfn)(caBitArray*);					// init-function
	void		(*scfn)(caBitArray*);					// sync-(ca-space)-function
}const ca_cnsgs[CM_MAX];
int64_t CA_CNFN_OPENCL(int64_t pgnc,caBitArray *vba);
#if ENABLE_CL
void OCLCA_RunVBA(int gnc,int res,OCLCA oclca,CA_RULE *cr,caBitArray vba,int sync_memory,cl_uint rg);
void OCLCA_Run(int ng,int res,OCLCA oclca,CA_RULE *cr,int sync_memory,const CA_CT *clv,const int scsz,const int brsz,cl_uint rg);
const char *ocl_strerr(int error);
OCLCA OCLCA_Init(const char *cl_filename);
#endif
void CA_CNITFN_OPENCL(caBitArray *vba,CA_RULE *cr);
int64_t CA_CNFN_SISD(int64_t pgnc,caBitArray *vba);
int64_t CA_CNFN_SIMD(int64_t pgnc,caBitArray *vba);
void ca_next_gen__simd(CA_RULE *cr,CA_CT *clv,CA_CT *cli);
void ca_count__simd(const CA_CT *csv,const CA_CT *csf,const CA_CT *csi,const UINT32 *pbv,const UINT32 *pbi,const int hlzm,const int hlfs,const int fsme,const int ncs);
void ca_next_gen_ncn3_sisd(CA_RULE *cr,CA_CT *clv,CA_CT *cli);
void ca_next_gen__sisd(CA_RULE *cr,CA_CT *clv,CA_CT *cli);
void ca_next_gen_abs_ncn3_ncs2_simd(CA_RULE *cr,CA_CT *clv,CA_CT *cli);
void ca_next_gen_ncn3_simd(CA_RULE *cr,CA_CT *clv,CA_CT *cli);
void CA_CNITFN_HASH(caBitArray *vba,CA_RULE *cr);
void CA_SCFN_HASH(caBitArray *vba);
int64_t CA_CNFN_HASH(int64_t pgnc,caBitArray *vba);
HCI HC_find_or_add_branch(UINT32 ll,HCI ln,HCI rn,HCI *rt);
extern int hcfl;
UINT32 *display_hash_array(UINT32 *pbv,UINT32 *pbf,UINT32 *pbi,UINT32 pv,int pll,HCI n,UINT32 *pmxv,UINT32 *pmnv);
UINT32 *old_display_hash_array(UINT32 *pbv,UINT32 *pbf,UINT32 *pbi,UINT32 v,int ll,HCI n,UINT32 *mxv,UINT32 *mnv);
CA_CT *convert_hash_array_to_CA_CT(int ll,HCI n,CA_CT *clv,CA_CT *cli);
HCI HC_add_node(int ll,HCI n,int *mxll);
HCI convert_bit_array_to_hash_array(caBitArray *vba,int *ll);
void HC_print_node(int ll,int mxll,HCI n);
void HC_print_stats();
extern HC_STATS hc_stats[HCTMXLV];
extern int hc_sl;
extern HCI hc_sn;
extern HCI hc_ens[128];
extern HCN *hct;
extern HCI hcln[HCTMXLV];
extern UINT32 HCMXSC;
extern UINT32 HCISZPT;
int64_t CA_CNFN_VBA_2x256(int64_t pgnc,caBitArray *vba);
int64_t CA_CNFN_VBA_1x64(int64_t pgnc,caBitArray *vba);
int64_t CA_CNFN_VBA_4x32(int64_t pgnc,caBitArray *vba);
int64_t CA_CNFN_VBA_2x32(int64_t pgnc,caBitArray *vba);
int64_t CA_CNFN_VBA_1x32(int64_t pgnc,caBitArray *vba);
void CA_CNITFN_VBA_1x256(caBitArray *vba,CA_RULE *cr);
void CA_CNITFN_VBA_4x32(caBitArray *vba,CA_RULE *cr);
void CA_CNITFN_VBA_2x32(caBitArray *vba,CA_RULE *cr);
void CA_CNITFN_VBA_1x64(caBitArray *vba,CA_RULE *cr);
void CA_CNITFN_VBA_1x32(caBitArray *vba,CA_RULE *cr);
void CA_CNITFN_VBA_2x256(caBitArray *vba,CA_RULE *cr);
int64_t CA_CNFN_VBA_16x256(int64_t pgnc,caBitArray *vba);
int64_t CA_CNFN_OMP_TEST(int64_t gnc,caBitArray *vba);
int64_t CA_CNFN_VBA_1x256(int64_t gnc,caBitArray *vba);
void CA_CNITFN_OMP_TEST(caBitArray *vba,CA_RULE *cr);
void CA_CNITFN_VBA_16x256(caBitArray *vba,CA_RULE *cr);
int64_t CA_CNFN_DISABLED(int64_t pgnc,caBitArray *vba);
void CA_CNITFN_DISABLED(caBitArray *vba,CA_RULE *cr);
int64_t CA_CNFN_BOOL(int64_t pgnc,caBitArray *vba);
void CA_CNITFN_BOOL(caBitArray *vba,CA_RULE *cr);
int64_t CA_CNFN_LUT(int64_t pgnc,caBitArray *vba);
void CA_CNITFN_LUT(caBitArray *vba,CA_RULE *cr);
void convert_CA_CT_to_bit_array(CA_CT *csv,FBPT *bav,int ct);
void CA_SCFN_HBA(caBitArray *vba);
void convert_bit_array_to_CA_CT(FBPT *bav,CA_CT *csv,int ct);
void CA_SCFN_VBA(caBitArray *vba);
void convertBetweenCACTandCABitArray(caBitArray *ba,int dr);
void initCAVerticalBitArray(caBitArray *ba,CA_RULE *rl);
__inline void CABitArrayPrepareOL(caBitArray *ba);
void print_bits(unsigned char *bytes,size_t num_bytes,int bypr);
void print_byte_as_bits(char val);
void print_bitblock_bits(FBPT *fbp,size_t num_elements,int elpr);
CA_CT *CA_LoadFromFile(const char *filename,int *ct);
void CA_SaveToFile(const char *filename,CA_CT *spc,int ct);
void CA_PrintSpace(char **caption,CA_CT *spc,int sz);
char *read_file(const char *filename);
UINT16 fnv_16_buf(void *buf,size_t len);
UINT32 fnv_32_buf(void *buf,size_t len,UINT32 hval);
__inline int32_t log2fix(uint32_t x,size_t precision);
__inline UINT64 ipow(UINT64 base,int exp);
extern const CA_RULE CA_RULE_EMPTY;
extern int sdmrpt;
extern uint64_t ca_space_sz;
extern CA_CT *ca_space;
extern SIMFW sfw;
__inline double random01();
