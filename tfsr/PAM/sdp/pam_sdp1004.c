/*
 */

/**
 * @version     RFS_1.3.1_b068-LinuStoreIII_1.2.0_b027-FSR_1.2.1_b122_RC
 * @file        FSR_PAM_Poseidon.c
 * @brief       This file is a basement for FSR adoption. It povides
 *              partition management, contexts management
 *
 */

#include "FSR.h"

/*****************************************************************************/
/* [PAM customization]                                                       */
/* The following 5 parameters can be customized                              */
/*                                                                           */
/* - FSR_ONENAND_PHY_BASE_ADDR                                               */
/* - FSR_ENABLE_WRITE_DMA                                                    */
/* - FSR_ENABLE_READ_DMA                                                     */
/* - FSR_ENABLE_FLEXOND_LFT                                                  */
/* - FSR_ENABLE_ONENAND_LFT                                                  */
/*                                                                           */
/*****************************************************************************/
/**< if FSR_ENABLE_FLEXOND_LFT is defined, 
     Low level function table is linked with Flex-OneNAND LLD */
//#define     FSR_ENABLE_FLEXOND_LFT

/**< if FSR_ENABLE_ONENAND_LFT is defined, 
     Low level function table is linked with OneNAND LLD */
#define     FSR_ENABLE_ONENAND_LFT

#define     FSR_ENABLE_NAND_LFT

/**< if FSR_ENABLE_4K_ONENAND_LFT is defined, 
     Low level function table is linked with OneNAND LLD */
//#define     FSR_ENABLE_4K_ONENAND_LFT

// #if defined(FSR_LINUX_OAM)

    #if defined(TINY_FSR)
    /* Setting at Kernel configuration */
        #define     FSR_ONENAND_PHY_BASE_ADDR       CONFIG_TINY_FLASH_PHYS_ADDR
    #else
    	#define     FSR_ONENAND_PHY_BASE_ADDR       CONFIG_FSR_FLASH_PHYS_ADDR
    #endif

    /**< if FSR_ENABLE_WRITE_DMA is defined, write DMA is enabled */
    #undef      FSR_ENABLE_WRITE_DMA
    /**< if FSR_ENABLE_READ_DMA is defined, read DMA is enabled */
    #undef      FSR_ENABLE_READ_DMA

// #endif // defined(FSR_LINUX_OAM)

#if defined(FSR_ENABLE_FLEXOND_LFT)
    #include "FSR_LLD_FlexOND.h"
#endif

#if defined(FSR_ENABLE_ONENAND_LFT)
    #include "FSR_LLD_OneNAND.h"
#endif

#if defined(FSR_ENABLE_4K_ONENAND_LFT)
    #include "FSR_LLD_4K_OneNAND.h"
#endif

/* memcpy_sdp.c */
void* memcpy32(void *pDst, const void *pSrc, unsigned int nLen);

#if defined(FSR_ENABLE_NAND_LFT)
#include "FSR_LLD_PureNAND.h"
#endif

/*****************************************************************************/
/* Global variables definitions                                              */
/*****************************************************************************/

/*****************************************************************************/
/* Local #defines                                                            */
/*****************************************************************************/

#define     DBG_PRINT(x)
#define     RTL_PRINT(x)


/*****************************************************************************/
/* Local typedefs                                                            */
/*****************************************************************************/

/*****************************************************************************/
/* Static variables definitions                                              */
/*****************************************************************************/

PRIVATE FsrVolParm              gstFsrVolParm[FSR_MAX_VOLS];
PRIVATE BOOL32                  gbPAMInit                   = FALSE32;
PRIVATE BOOL32                  gbUseWriteDMA               = FALSE32;
PRIVATE BOOL32                  gbUseReadDMA                = FALSE32;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#ifdef __cplusplus
}
#endif /* __cplusplus */

/*****************************************************************************/
/* Function Implementation                                                   */
/*****************************************************************************/

/**
 * @brief           This function initializes PAM
 *                  this function is called by FSR_BML_Init
 *
 * @return          FSR_PAM_SUCCESS
 *
 * @author          SongHo Yoon
 * @version         1.0.0
 *
 */
PUBLIC INT32
FSR_PAM_Init(VOID)
{
        INT32   nRe = FSR_PAM_SUCCESS;
    UINT32  nPNDVirBaseAddr;

    FSR_STACK_VAR;

    FSR_STACK_END;

    if (gbPAMInit == TRUE32)
    {
        return FSR_PAM_SUCCESS;
    }
    gbPAMInit     = TRUE32;
    
    RTL_PRINT((TEXT("[PAM:   ] ++%s\r\n"), __FSR_FUNC__));

    do
    {
#if defined(FSR_ENABLE_WRITE_DMA)
        gbUseWriteDMA = TRUE32;
#else
        gbUseWriteDMA = FALSE32;
#endif

#if defined(FSR_ENABLE_READ_DMA)
        gbUseReadDMA  = TRUE32;
#else
        gbUseReadDMA  = FALSE32;
#endif

        nPNDVirBaseAddr  = FSR_OAM_Pa2Va(0x30020000); // <-@@@

        RTL_PRINT((TEXT("[PAM:   ]   OneNAND physical base address       : 0x%08x\r\n"), 0x30020000));
        RTL_PRINT((TEXT("[PAM:   ]   OneNAND virtual  base address       : 0x%08x\r\n"), nPNDVirBaseAddr));

        gstFsrVolParm[0].nBaseAddr[0] = nPNDVirBaseAddr;
        gstFsrVolParm[0].nBaseAddr[1] = FSR_PAM_NOT_MAPPED;
        gstFsrVolParm[0].nIntID[0]    = FSR_INT_ID_NAND_0;
        gstFsrVolParm[0].nIntID[1]    = FSR_INT_ID_NONE;
        gstFsrVolParm[0].nDevsInVol   = 1;
        gstFsrVolParm[0].bProcessorSynchronization = FALSE32;
        gstFsrVolParm[0].pExInfo      = NULL;

        gstFsrVolParm[1].nBaseAddr[0] = FSR_PAM_NOT_MAPPED;
        gstFsrVolParm[1].nBaseAddr[1] = FSR_PAM_NOT_MAPPED;
        gstFsrVolParm[1].nIntID[0]    = FSR_INT_ID_NONE;;
        gstFsrVolParm[1].nIntID[1]    = FSR_INT_ID_NONE;
        gstFsrVolParm[1].nDevsInVol   = 0;
        gstFsrVolParm[1].bProcessorSynchronization = FALSE32;
        gstFsrVolParm[1].pExInfo      = NULL;

    } while (0);

    RTL_PRINT((TEXT("[PAM:   ] --%s\r\n"), __FSR_FUNC__));

    return nRe;
}

/**
 * @brief           This function returns FSR volume parameter
 *                  this function is called by FSR_BML_Init
 *
 * @param[in]       stVolParm[FSR_MAX_VOLS] : FsrVolParm data structure array
 *
 * @return          FSR_PAM_SUCCESS
 * @return          FSR_PAM_NOT_INITIALIZED
 *
 * @author          SongHo Yoon
 * @version         1.0.0
 *
 */
PUBLIC INT32
FSR_PAM_GetPAParm(FsrVolParm stVolParm[FSR_MAX_VOLS])
{
    FSR_STACK_VAR;

    FSR_STACK_END;

    if (gbPAMInit == FALSE32)
    {
        return FSR_PAM_NOT_INITIALIZED;
    }

    FSR_OAM_MEMCPY(&(stVolParm[0]), &gstFsrVolParm[0], sizeof(FsrVolParm));
    FSR_OAM_MEMCPY(&(stVolParm[1]), &gstFsrVolParm[1], sizeof(FsrVolParm));

    return FSR_PAM_SUCCESS;
}

/**
 * @brief           This function registers LLD function table
 *                  this function is called by FSR_BML_Open
 *
 * @param[in]      *pstLFT[FSR_MAX_VOLS] : pointer to FSRLowFuncTable data structure
 *
 * @return          none
 *
 * @author          SongHo Yoon
 * @version         1.0.0
 *
 */
PUBLIC INT32
FSR_PAM_RegLFT(FSRLowFuncTbl  *pstLFT[FSR_MAX_VOLS])
{
    UINT32  nVolIdx;
    FSR_STACK_VAR;

    FSR_STACK_END;

    if (gbPAMInit == FALSE32)
    {
        return FSR_PAM_NOT_INITIALIZED;
    }

        nVolIdx = 0;

    pstLFT[nVolIdx]->LLD_Init               = FSR_PND_Init;
    pstLFT[nVolIdx]->LLD_Open               = FSR_PND_Open;
    pstLFT[nVolIdx]->LLD_Close              = FSR_PND_Close;
    pstLFT[nVolIdx]->LLD_Erase              = FSR_PND_Erase;
    pstLFT[nVolIdx]->LLD_ChkBadBlk          = FSR_PND_ChkBadBlk;
    pstLFT[nVolIdx]->LLD_FlushOp            = FSR_PND_FlushOp;
    pstLFT[nVolIdx]->LLD_GetDevSpec         = FSR_PND_GetDevSpec;
    pstLFT[nVolIdx]->LLD_Read               = FSR_PND_Read;
    pstLFT[nVolIdx]->LLD_ReadOptimal        = FSR_PND_ReadOptimal;
    pstLFT[nVolIdx]->LLD_Write              = FSR_PND_Write;
    pstLFT[nVolIdx]->LLD_CopyBack           = FSR_PND_CopyBack;
    pstLFT[nVolIdx]->LLD_GetPrevOpData      = FSR_PND_GetPrevOpData;
    pstLFT[nVolIdx]->LLD_IOCtl              = FSR_PND_IOCtl;
    pstLFT[nVolIdx]->LLD_InitLLDStat        = FSR_PND_InitLLDStat;
    pstLFT[nVolIdx]->LLD_GetStat            = FSR_PND_GetStat;
    pstLFT[nVolIdx]->LLD_GetBlockInfo       = FSR_PND_GetBlockInfo;
    pstLFT[nVolIdx]->LLD_GetNANDCtrllerInfo = FSR_PND_GetNANDCtrllerInfo;

    RTL_PRINT((TEXT("[PAM:   ] --%s\r\n"), __FSR_FUNC__));

    return FSR_PAM_SUCCESS;
        }

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

/* ij.jang : add sdp92 support */
/************************************************************
 * SDP83 support
 ************************************************************/
void FSR_PAM_WRITE16(volatile unsigned short *reg, unsigned short v)
{
	FSR_STACK_VAR;
	FSR_STACK_END;

	FSR_ASSERT( (((unsigned int)reg) & 3) == 0);

	*reg = v;
}

void FSR_PAM_MEMSET_DATARAM(void * s,int c, unsigned int count)
{
	FSR_STACK_VAR;
	FSR_STACK_END;

	FSR_OAM_MEMSET(s, c, count);
}

/**
 * @brief           This function transfers data to NAND
 *
 * @param[in]      *pDst  : Destination array Pointer to be copied
 * @param[in]      *pSrc  : Source data allocated Pointer
 * @param[in]      *nSize : length to be transferred
 *
 * @return          none
 *
 * @author          SongHo Yoon
 * @version         1.0.0
 * @remark          pDst / pSrc address should be aligned by 4 bytes.
 *                  DO NOT USE memcpy of standard library 
 *                  memcpy (RVDS2.2) can do wrong memory copy operation.
 *                  memcpy32 is optimized by using multiple load/store instruction
 *
 */
PUBLIC VOID
FSR_PAM_TransToNAND(volatile VOID *pDst,
                    VOID          *pSrc,
                    UINT32        nSize)
{
    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_ASSERT(((UINT32) pSrc & 0x03) == 0x00000000);
    FSR_ASSERT(((UINT32) pDst & 0x03) == 0x00000000);
    FSR_ASSERT(nSize > sizeof(UINT32));

	memcpy32((void *)pDst, pSrc, nSize);
}

/**
 * @brief           This function transfers data from NAND
 *
 * @param[in]      *pDst  : Destination array Pointer to be copied
 * @param[in]      *pSrc  : Source data allocated Pointer
 * @param[in]      *nSize : length to be transferred
 *
 * @return          none
 *
 * @author          SongHo Yoon
 * @version         1.0.0
 * @remark          pDst / pSrc address should be aligned by 4 bytes
 *                  DO NOT USE memcpy of standard library 
 *                  memcpy (RVDS2.2) can do wrong memory copy operation
 *                  memcpy32 is optimized by using multiple load/store instruction
 *
 */
VOID
FSR_PAM_TransFromNAND(VOID          *pDst,
                      volatile VOID *pSrc,
                      UINT32         nSize)
{
    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_ASSERT(((UINT32) pSrc & 0x03) == 0x00000000);
    FSR_ASSERT(((UINT32) pDst & 0x03) == 0x00000000);
    FSR_ASSERT(nSize > sizeof(UINT32));

    memcpy32((void *) pDst, pSrc, nSize);
}

/**
 * @brief           This function initializes the specified logical interrupt.
 *
 * @param[in]       nLogIntId : Logical interrupt id
 * 
 * @return          FSR_PAM_SUCCESS
 *
 * @author          SongHo Yoon
 *
 * @version         1.0.0
 *
 * @remark          this function is used to support non-blocking I/O feature of FSR
 *
 */
PUBLIC INT32
FSR_PAM_InitInt(UINT32 nLogIntId)
{

    return FSR_PAM_SUCCESS;
}

/**
 * @brief           This function deinitializes the specified logical interrupt.
 *
 * @param[in]       nLogIntId : Logical interrupt id
 * 
 * @return          FSR_PAM_SUCCESS
 *
 * @author          SongHo Yoon
 *
 * @version         1.0.0
 *
 * @remark          this function is used to support non-blocking I/O feature of FSR
 *
 */
PUBLIC INT32
FSR_PAM_DeinitInt(UINT32 nLogIntId)
{
    return FSR_PAM_SUCCESS;
}

/**
 * @brief           This function returns the physical interrupt ID from the logical interrupt ID
 *
 * @param[in]       nLogIntID : Logical interrupt id
 *
 * @return          physical interrupt ID
 *
 * @author          SongHo Yoon
 *
 * @version         1.0.0
 *
 */
UINT32
FSR_PAM_GetPhyIntID(UINT32  nLogIntID)
{
    return 0;
}

/**
 * @brief           This function enables the specified interrupt.
 *
 * @param[in]       nLogIntID : Logical interrupt id
 *
 * @return          FSR_PAM_SUCCESS
 *
 * @author          Seunghyun Han
 *
 * @version         1.0.0
 *
 */
INT32
FSR_PAM_ClrNEnableInt(UINT32 nLogIntID)
{
    switch (nLogIntID)
    {
    case FSR_INT_ID_NAND_0:

#if defined(FSR_SYMOS_OAM)
        pNandGpioInterrupt->AckInterruptNotif(); /* Interrupt Clear   */
        pNandGpioInterrupt->Enable();            /* Interrupt Enable  */
#elif defined(FSR_WINCE_OAM)

#elif defined(FSR_LINUX_OAM)

#else /* for RTOS or OSLess*/

#endif
        break;
    default:
        break;
    }

    return FSR_PAM_SUCCESS;
}

/**
 * @brief           This function disables the specified interrupt.
 *
 * @param[in]       nLogIntID : Logical interrupt id
 *
 * @return          FSR_PAM_SUCCESS
 *
 * @author          Seunghyun Han
 *
 * @version         1.0.0
 *
 */
INT32
FSR_PAM_ClrNDisableInt(UINT32 nLogIntID)
{
    switch (nLogIntID)
    {
    case FSR_INT_ID_NAND_0:

#if defined(FSR_SYMOS_OAM)
        pNandGpioInterrupt->AckInterruptNotif(); /* Interrupt Clear   */
        pNandGpioInterrupt->Enable(EFalse);      /* Interrupt Disable */
#elif defined(FSR_WINCE_OAM)

#elif defined(FSR_LINUX_OAM)

#else /* for RTOS or OSLess*/

#endif
        break;
    default:
        break;
    }

    return FSR_PAM_SUCCESS;
}


#if defined(FSR_SYMOS_OAM)
/**
 * @brief           This function is used only for SymOS.
 *
 * @param[in]       aPtr
 * @param[in]       aHandle
 *
 * @return          none
 *
 * @author          WooYoungYang
 *
 * @version         1.0.0
 *
 */
void TNandGpioInterrupt::NandInterrupt(TAny* aPtr, TOmapGpioPinHandle aHandle)
{
    FSR_PAM_ClrNDisableInt(FSR_INT_ID_NAND_0);
	pMDNand->iNandIreqDfc.Add();
}

/**
 * @brief           This function is used only for SymOS.
 *
 * @param[in]       aPinId
 *
 * @return          TInt
 *
 * @author          WooYoungYang
 *
 * @version         1.0.0
 *
 */
TInt TNandGpioInterrupt::Bind(TInt aPinId)
{
    iPinHandle = OmapGpioPinMgr::AcquirePin(aPinId);

    if (iPinHandle == KOmapGpioInvalidHandle)
        return KErrBadHandle;

	OmapGpioPinMgr::SetPinAsInput(iPinHandle);

    iPinWrapper = new TOmapGpioInputPinWrapper(iPinHandle);
    if (!iPinWrapper)
    {
        return KErrNoMemory;
    }

    // Configure the Intr on the falling edge
    iPinWrapper->SetEventDetection(EOmapGpioEDTEdge, EOmapGpioEDSUp);

    // Enable module wake-up on this pin (interrupt is also enabled)
    iPinWrapper->EnableWakeUpNotif();

    // Disable interrupt generation on the input pin
    iPinWrapper->EnableInterruptNotif(EFalse);

    return iPinWrapper->BindIsr(NandInterrupt, this);
}
#endif

/**
 * @brief           This function creates spin lock for dual core.
 *
 * @param[out]     *pHandle : Handle of semaphore
 * @param[in]       nLayer  : 0 : FSR_OAM_SM_TYPE_BDD
 *                            0 : FSR_OAM_SM_TYPE_STL
 *                            1 : FSR_OAM_SM_TYPE_BML
 *                            2 : FSR_OAM_SM_TYPE_LLD
 * 
 * @return          TRUE32   : this function creates spin lock successfully
 * @return          FALSE32  : fail
 *
 * @author          DongHoon Ham
 * @version         1.0.0
 * @remark          An initial count of spin lock object is 1.
 *                  An maximum count of spin lock object is 1.
 *
 */
BOOL32  
FSR_PAM_CreateSL(UINT32  *pHandle, UINT32  nLayer)
{
    return TRUE32;
}

/**
 * @brief          This function acquires spin lock for dual core.
 *
 * @param[in]       nHandle : Handle of semaphore to be acquired
 * @param[in]       nLayer  : 0 : FSR_OAM_SM_TYPE_BDD
 *                            0 : FSR_OAM_SM_TYPE_STL
 *                            1 : FSR_OAM_SM_TYPE_BML
 *                            2 : FSR_OAM_SM_TYPE_LLD
 * 
 * @return          TRUE32   : this function acquires spin lock successfully
 * @return          FALSE32  : fail
 *
 * @author          DongHoon Ham
 * @version         1.0.0
 *
 */
BOOL32  
FSR_PAM_AcquireSL(UINT32  nHandle, UINT32  nLayer)
{
    return TRUE32;
}

/**
 * @brief           This function releases spin lock for dual core.
 *
 * @param[in]       nHandle : Handle of semaphore to be released
 * @param[in]       nLayer  : 0 : FSR_OAM_SM_TYPE_BDD
 *                            0 : FSR_OAM_SM_TYPE_STL
 *                            1 : FSR_OAM_SM_TYPE_BML
 *                            2 : FSR_OAM_SM_TYPE_LLD
 *
 * @return          TRUE32   : this function releases spin lock successfully
 * @return          FALSE32  : fail
 *
 * @author          DongHoon Ham
 * @version         1.0.0
 *
 */
BOOL32
FSR_PAM_ReleaseSL(UINT32  nHandle, UINT32  nLayer)
{
    return TRUE32;
}
