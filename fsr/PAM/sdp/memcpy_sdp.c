#include <FSR.h>

/* ij.jang add basic memcpy functions, these can be inline functions */
/* oam_memcpy32 : copy by 8 words, onenand 16 burst */
static void oam_memcpy32(unsigned int* dst, unsigned int* src, unsigned int len)
{
	asm volatile (
		"1:\n\t"
		"ldmia	%1!, {r0,r1,r2,r3,r4,r5,r6,r7}"	"\n\t"
		"stmia	%0!, {r0,r1,r2,r3,r4,r5,r6,r7}"	"\n\t"
		"subs	%2, %2, #32"	"\n\t"
		"bgt	1b\n\t"
		: "=r"(dst), "=r"(src), "=r"(len)
		: "0"(dst), "1"(src), "2"(len)
		: "r0","r1","r2","r3","r4","r5","r6","r7","cc"
	);
}

/* copy by 4 * 8 words, onenand 16 burst */
static void oam_memcpy128(unsigned int* dst, unsigned int* src, unsigned int len)
{ 
	asm volatile (
		"1:\n\t"
		"ldmia	%1!, {r0,r1,r2,r3,r4,r5,r6,r7}"	"\n\t"
		"stmia	%0!, {r0,r1,r2,r3,r4,r5,r6,r7}"	"\n\t"
		"ldmia	%1!, {r0,r1,r2,r3,r4,r5,r6,r7}"	"\n\t"
		"stmia	%0!, {r0,r1,r2,r3,r4,r5,r6,r7}"	"\n\t"
		"ldmia	%1!, {r0,r1,r2,r3,r4,r5,r6,r7}"	"\n\t"
		"stmia	%0!, {r0,r1,r2,r3,r4,r5,r6,r7}"	"\n\t"
		"ldmia	%1!, {r0,r1,r2,r3,r4,r5,r6,r7}"	"\n\t"
		"stmia	%0!, {r0,r1,r2,r3,r4,r5,r6,r7}"	"\n\t"
		"subs	%2, %2, #128"	"\n\t"
		"bgt	1b\n\t"
		: "=r"(dst), "=r"(src), "=r"(len)
		: "0"(dst), "1"(src), "2"(len)
		: "r0","r1","r2","r3","r4","r5","r6","r7","cc"
	);
}

void* memcpy32(void *pDst, const void *pSrc, unsigned int nLen)
{
#if defined(OAM_USE_STDLIB)
	memcpy(pDst, pSrc, nLen);
#else /* OAM_USE_STDLIB */	
	unsigned int dstl, srcl;
	dstl = (unsigned int)pDst;
	srcl = (unsigned int)pSrc;

	if (!nLen)
		return pDst;

	if ( !(dstl & 3) && !(srcl & 3) ) {
		if (!(nLen & 127)) {
			oam_memcpy128((unsigned int*)pDst,
					(unsigned int*)pSrc, nLen);
			return pDst;
		} else if (!(nLen & 31)) {
			oam_memcpy32((unsigned int*)pDst,
					(unsigned int*)pSrc, nLen);
			return pDst;
		}
	}
	FSR_OAM_MEMCPY((unsigned char*)pDst, (unsigned char*)pSrc, nLen);
    return pDst;
#endif		
}
