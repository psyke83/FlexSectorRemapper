/**
 *   @mainpage   Flex Sector Remapper
 *
 *   @section Intro
 *       Flash Translation Layer for Flex-OneNAND and OneNAND
 *    
 *    @section  Copyright
 *            COPYRIGHT. 2007-2009 SAMSUNG ELECTRONICS CO., LTD.               
 *                            ALL RIGHTS RESERVED                              
 *                                                                             
 *     Permission is hereby granted to licensees of Samsung Electronics        
 *     Co., Ltd. products to use or abstract this computer program for the     
 *     sole purpose of implementing a product based on Samsung                 
 *     Electronics Co., Ltd. products. No other rights to reproduce, use,      
 *     or disseminate this computer program, whether in part or in whole,      
 *     are granted.                                                            
 *                                                                             
 *     Samsung Electronics Co., Ltd. makes no representation or warranties     
 *     with respect to the performance of this computer program, and           
 *     specifically disclaims any responsibility for any damages,              
 *     special or consequential, connected with the use of this program.       
 *
 *     @section Description
 *
 */

/**
 *  @file       FSR_STL_Init.c
 *  @brief      Zone, Log group, Context object initialization
 *  @author     Jongtae Park
 *  @date       05-MAR-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  05-MAR-2007 [Jongtae Park] : first writing
 *  @n  27-JUN-2007 [Wonmoon Cheon]: v1.2.0 re-design
 *  @n  29-JAN-2008 [MinYoung Kim] : dead code elimination
 */

/*****************************************************************************/
/* Header file inclusions                                                    */
/*****************************************************************************/
#include "FSR.h"

#include "FSR_STL_CommonType.h"
#include "FSR_STL_Config.h"
#include "FSR_STL_Interface.h"
#include "FSR_STL_Types.h"
#include "FSR_STL_Common.h"

/*****************************************************************************/
/* Global variable definitions                                               */
/*****************************************************************************/
PRIVATE const STLMetaLayout  gastCfgTbl[4] =
{
/*   DH    BMT   CTX    PMT    DH_SBM    BMTCTX_SBM  CTXPMT_SBM    BMT_SBM    PMT_SBM    MAX_INACTIVE_LOG_GRP */
/* 2KB PAGE FORMAT */
{  6144,  1024,  512,  1536, 0x0000000F, 0x00000007, 0x0000000F, 0x00000003, 0x0000000E, 1024, 0, 0, 0, 0 },
/* 4KB PAGE FORMAT */
{ 12288,  3584,  512,  3584, 0x000000FF, 0x000000FF, 0x000000FF, 0x0000007F, 0x000000FE, 2048, 0, 0, 0, 0 },
/* 8KB PAGE FORMAT */
{ 24576,  3584, 1024,  7168, 0x0000FFFF, 0x000001FF, 0x0000FFFF, 0x0000007F, 0x0000FFFC, 4096, 0, 0, 0, 0 },
/* 16KB PAGE FORMAT */
{ 49152, 12800, 1024, 15360, 0xFFFFFFFF, 0x07FFFFFF, 0xFFFFFFFF, 0x01FFFFFF, 0xFFFFFFFC, 8192, 0, 0, 0, 0 },
};

/*****************************************************************************/
/* Imported variable declarations                                            */
/*****************************************************************************/

/*****************************************************************************/
/* Imported function prototype                                               */
/*****************************************************************************/

/*****************************************************************************/
/* Local macro                                                               */
/*****************************************************************************/

/*****************************************************************************/
/* Local type defines                                                        */
/*****************************************************************************/

/*****************************************************************************/
/* Local constant definitions                                                */
/*****************************************************************************/

/*****************************************************************************/
/* Local (static) function prototype                                         */
/*****************************************************************************/

/*****************************************************************************/
/* Local (static)  Function Definition                                       */
/*****************************************************************************/

/*****************************************************************************/
/* Global Function Definition                                                */
/*****************************************************************************/

/**
 * @brief       This function initializes VFL parameter pool.
 *
 * @param[in]   pstClst      : pointer to cluster object
 *
 * @return      none
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC VOID
FSR_STL_InitVFLParamPool(STLClstObj *pstClst)
{
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    pstClst->nVFLParamCnt = 0;
    pstClst->nVFLExtCnt   = 0;

    FSR_OAM_MEMSET(pstClst->astParamPool,    0xFF, sizeof(VFLParam) * MAX_VFL_PARAMS);
    FSR_OAM_MEMSET(pstClst->astExtParamPool, 0xFF, sizeof(VFLExtParam) * MAX_VFLEXT_PARAMS);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}

/**
 * @brief       This function sets pointer values of members in the specified 
 * @n           directory header object with buffer memory.
 *
 * @param[in]   pstZone     : zone object pointer
 * @param[in]   pstDH       : pointer to DirHdr object
 * @param[in]   pBuf        : buffer pointer to set DirHdr
 * @param[in]   nBufSize    : buffer size
 *
 * @return      none
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC VOID
FSR_STL_SetDirHdrHdl   (STLZoneObj     *pstZone,
                        STLDirHdrHdl   *pstDH,
                        UINT8          *pBuf,
                        UINT32          nBufSize)
{
    STLZoneInfo    *pstZI;
    UINT32          nNumBMT;
    UINT32          nNumDirEntries;
    UINT8          *pCurBuf         = pBuf;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get zone info pointer */
    pstZI = pstZone->pstZI;
    
    /* set main buffer pointer */
    pstDH->pBuf              = pBuf;
    pstDH->nBufSize          = nBufSize;

    /* set pointer of idle block offset list */
    pstDH->pIdleBoList       = (UINT16*)pCurBuf;
    pCurBuf                 += (sizeof(UINT16) * pstZI->nNumMetaBlks);

    /* set pointer of valid page count list */
    pstDH->pValidPgCnt       = (UINT16*)pCurBuf;
    pCurBuf                 += (sizeof(UINT16) * pstZI->nNumMetaBlks);

    /* set pointer of meta block erase count list */
    pstDH->pMetaBlksEC       = (UINT32*)pCurBuf;
    pCurBuf                 += (sizeof(UINT32) * pstZI->nNumMetaBlks);

    /* Compensate the number of BMT to a multiple of 2 */
    nNumBMT                  = ((pstZI->nNumLA) & 0x1) ? pstZI->nNumLA + 1 : pstZI->nNumLA;

    /* set pointer of BMT directory */
    pstDH->pBMTDir           = (POFFSET*)pCurBuf;
    pCurBuf                 += (sizeof(POFFSET) * nNumBMT);

    /* get number of total log groups entries in PMT directory */
    nNumDirEntries           = pstZone->pstML->nMaxInaLogGrps;

    /* set pointer of log group merge flag bitstream */
    pstDH->pPMTMergeFlags    = (UINT8*)pCurBuf;
    pCurBuf                 += (sizeof(UINT8) * (nNumDirEntries >> 3));

    /* set pointer of PMT directory */
    pstDH->pstPMTDir         = (STLPMTDirEntry*)pCurBuf;
    pCurBuf                 += (sizeof(STLPMTDirEntry) * nNumDirEntries);
    
    /* set pointer of PMT directory */
    pstDH->pstPMTWLGrp       = (STLPMTWLGrp*)pCurBuf;
    pCurBuf                 += (sizeof(STLPMTWLGrp) * (nNumDirEntries >> DEFAULT_PMT_EC_GRP_SIZE_SHIFT));

    /* set fixed members pointer */
    pstDH->pstFm             = (STLDirHeaderFm*)pCurBuf;
    pCurBuf                 += sizeof(STLDirHeaderFm);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}


/**
 * @brief       This function sets pointer values of members in the specified 
 * @n           context info object with buffer memory.
 *
 * @param[in]   pstZone     : zone object pointer
 * @param[in]   pstCtx      : pointer to CtxInfo object
 * @param[in]   pBuf        : buffer pointer to set pstCtx
 * @param[in]   nBufSize    : buffer size
 *
 * @return      none
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC VOID
FSR_STL_SetCtxInfoHdl  (STLZoneObj     *pstZone,
                        STLCtxInfoHdl  *pstCtx,
                        UINT8          *pBuf,
                        UINT32          nBufSize)
{
    STLZoneInfo    *pstZI;
    UINT8          *pCurBuf = pBuf;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get zone info pointer */
    pstZI = pstZone->pstZI;
    
    /* set main buffer pointer */
    pstCtx->pBuf             = pBuf;
    pstCtx->nBufSize         = nBufSize;

    /* set fixed members pointer */
    pstCtx->pstFm            = (STLCtxInfoFm*)pCurBuf;
    pCurBuf                 += sizeof(STLCtxInfoFm);

    /* set active log list pointer */
    pstCtx->pstActLogList    = (STLActLogEntry*)pCurBuf;
    pCurBuf                 += (sizeof(STLActLogEntry) * MAX_ACTIVE_LBLKS);

    /* set free list pointer */
    pstCtx->pFreeList        = (BADDR*)pCurBuf;
    pCurBuf                 += (sizeof(BADDR) * pstZone->pstML->nMaxFreeSlots);

    /* set free blocks EC table pointer */
    pstCtx->pFBlksEC         = (UINT32*)pCurBuf;
    pCurBuf                 += (sizeof(UINT32) * pstZone->pstML->nMaxFreeSlots);

    /* set minimum EC (BMT) pointer */
    pstCtx->pMinEC           = (UINT32*)pCurBuf;
    pCurBuf                 += (sizeof(UINT32) * pstZI->nNumLA);

    /* set minimum EC index (BMT) pointer */
    pstCtx->pMinECIdx        = (UINT16*)pCurBuf;
    pCurBuf                 += (sizeof(UINT16) * pstZI->nNumLA);

    /* Align 4 bytes for ARM9 (ARM9 does not support unaligned access) */
    if (((UINT32)(pCurBuf)) & 0x03)
    {
        pCurBuf += 0x04 - (((UINT32)(pCurBuf)) & 0x03);
    }

    /* set ZBC confirm pointer */
    pstCtx->pstCfm           = (STLZbcCfm*)pCurBuf;

    pstCtx->nCfmBufSize      = (UINT32)(pCurBuf - pBuf);
    FSR_ASSERT(pstCtx->nCfmBufSize <= nBufSize);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}


/**
 * @brief       This function sets pointer values of members in the specified 
 * @n           BMT object with buffer memory.
 *
 * @param[in]   pstZone     : zone object pointer
 * @param[in]   pstBMT      : pointer to STLBMTHdl object
 * @param[in]   pBuf        : buffer pointer to set STLBMTHdl
 * @param[in]   nBufSize    : buffer size
 *
 * @return      none
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC VOID
FSR_STL_SetBMTHdl  (STLZoneObj     *pstZone,
                    STLBMTHdl      *pstBMT,
                    UINT8          *pBuf,
                    UINT32          nBufSize)
{
    STLZoneInfo    *pstZI;
    UINT8          *pCurBuf = pBuf;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get zone info pointer */
    pstZI = pstZone->pstZI;
    
    /* set main buffer pointer */
    pstBMT->pBuf              = pBuf;
    pstBMT->nBufSize          = nBufSize;

    /* set fixed members pointer */
    pstBMT->pstFm            = (STLBMTFm*)pCurBuf;
    pCurBuf                 += sizeof(STLBMTFm);

    /* set pointer of data block mapping unit */
    pstBMT->pMapTbl          = (STLBMTEntry*)pCurBuf;
    pCurBuf                 += (sizeof(STLBMTEntry) * pstZI->nDBlksPerLA);

    /* set pointer of data block erase count table */
    pstBMT->pDBlksEC         = (UINT32*)pCurBuf;
    pCurBuf                 += (sizeof(UINT32) * pstZI->nDBlksPerLA);

    /* set pointer of garbage block bit flags */
    pstBMT->pGBlkFlags       = (UINT8*)pCurBuf;
    pCurBuf                 += (pstZI->nDBlksPerLA >> 3);

    /* Align 4 bytes for ARM9 (ARM9 does not support unaligned access) */
    if (((UINT32)(pCurBuf)) & 0x03)
    {
        pCurBuf += 0x04 - (((UINT32)(pCurBuf)) & 0x03);
    }

    /* set ZBC confirm pointer */
    pstBMT->pstCfm           = (STLZbcCfm*)pCurBuf;

    pstBMT->nCfmBufSize      = (UINT32)(pCurBuf - pBuf);
    FSR_ASSERT(pstBMT->nCfmBufSize <= nBufSize);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}


/**
 * @brief       This function sets pointer values of members in the specified 
 * @n           log group object with buffer memory.
 *
 * @param[in]   pstZone     : zone object pointer
 * @param[in]   pstLogGrp   : pointer to STLLogGrpHdl object
 * @param[in]   pBuf        : buffer pointer to set pstLogGrp
 * @param[in]   nBufSize    : buffer size
 *
 * @return      none
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC VOID
FSR_STL_SetLogGrpHdl   (STLZoneObj     *pstZone,
                        STLLogGrpHdl   *pstLogGrp,
                        UINT8          *pBuf,
                        UINT32          nBufSize)
{
    const STLRootInfo  *pstRI   = pstZone->pstRI;
    const RBWDevInfo   *pstDev  = pstZone->pstDevInfo;
    UINT8              *pCurBuf = pBuf;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* set main buffer pointer */
    pstLogGrp->pBuf          = pBuf;
    pstLogGrp->nBufSize      = nBufSize;

    /* set fixed members pointer */
    pstLogGrp->pstFm         = (STLLogGrpFm*)pCurBuf;
    pCurBuf                 += sizeof(STLLogGrpFm);

    /* set pointer of log list */
    pstLogGrp->pstLogList    = (STLLog*)pCurBuf;
    pCurBuf                 += (sizeof(STLLog) << pstRI->nKShift);

    /* set pointer of LBN list */
    pstLogGrp->pLbnList      = (BADDR*)pCurBuf;
    pCurBuf                 += (sizeof(BADDR) << (pstRI->nNShift + pstRI->nKShift));

    /* set pointer of list of number of data blocks */
    pstLogGrp->pNumDBlks     = (UINT16*)pCurBuf;
    pCurBuf                 += (sizeof(UINT16) << pstRI->nKShift);

    /* set pointer of list of number of valid pages in a log */
    pstLogGrp->pLogVPgCnt    = (UINT16*)pCurBuf;
    pCurBuf                 += (sizeof(UINT16) << (pstRI->nKShift + pstDev->nNumWaysShift));

    /* set pointer of log group page mapping table */
    pstLogGrp->pMapTbl       = (POFFSET*)pCurBuf;
    pCurBuf                 += (sizeof(POFFSET) << (pstRI->nNShift + pstDev->nPagesPerSbShift));

    /* Align 4 bytes for ARM9 (ARM9 does not support unaligned access) */
    if (((UINT32)(pCurBuf)) & 0x03)
    {
        pCurBuf += 0x04 - (((UINT32)(pCurBuf)) & 0x03);
    }

    /* set ZBC confirm pointer */
    pstLogGrp->pstCfm        = (STLZbcCfm*)pCurBuf;

    pstLogGrp->nCfmBufSize   = (UINT32)(pCurBuf - pBuf);
    FSR_ASSERT(pstLogGrp->nCfmBufSize <= nBufSize);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}


/**
 * @brief       This function sets pointer values of members in the specified 
 * @n           PMT object with buffer memory.
 *
 * @param[in]   pstZone     : zone object pointer
 * @param[in]   pstPMT      : pointer to STLPMTHdl object
 * @param[in]   pBuf        : buffer pointer to set STLPMTHdl
 * @param[in]   nBufSize    : buffer size
 *
 * @return      none
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC VOID
FSR_STL_SetPMTHdl  (STLZoneObj     *pstZone,
                    STLPMTHdl      *pstPMT,
                    UINT8          *pBuf,
                    UINT32          nBufSize)
{
    UINT32          nIdx;
    UINT32          nBytesPerLogGrp;
    STLZoneInfo    *pstZI;
    UINT8          *pCurBuf = pBuf;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get zone info pointer */
    pstZI = pstZone->pstZI;

    /* set main buffer pointer */
    pstPMT->pBuf            = pBuf;
    pstPMT->nBufSize        = nBufSize;

    /* actual number of log groups in astLogGrps[] */
    pstPMT->nNumLogGrps     = pstZI->nNumLogGrpPerPMT;

    /* get bytes per log group */
    nBytesPerLogGrp = pstZone->pstML->nBytesPerLogGrp;

    for (nIdx = 0; nIdx < pstZI->nNumLogGrpPerPMT; nIdx++)
    {
        /* assign each STLLogGrpHdl pointer */
        FSR_STL_SetLogGrpHdl(pstZone, &(pstPMT->astLogGrps[nIdx]), pCurBuf, nBytesPerLogGrp);
        FSR_STL_InitLogGrp  (pstZone, &(pstPMT->astLogGrps[nIdx]));

        /* move buffer pointer */
        pCurBuf += nBytesPerLogGrp;
    }

    /* set remaining pointers to NULL */
    for ( ; nIdx < MAX_LOGGRP_PER_PMT; nIdx++)
    {
        FSR_OAM_MEMSET(&(pstPMT->astLogGrps[nIdx]), 0x00, sizeof(STLLogGrpHdl));
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}


/**
 * @brief       This function initializes directory header object.
 *
 * @param[in]   pstZone     : zone object pointer
 * @param[in]   pstDH       : pointer to directory header object
 *
 * @return      none
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC VOID
FSR_STL_InitDirHdr (STLZoneObj     *pstZone,
                    STLDirHdrHdl   *pstDH)
{
    STLDirHeaderFm     *pstFm;
    BADDR               nBlkOffs;
    UINT32              nNumBMT;
    UINT32              nNumDirEntries;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get fixed size member pointer */
    pstFm = pstDH->pstFm;

    /* initialize age information (age value : 64bits decremental order) */
    pstFm->nAge_High        = 0xFFFFFFFF;
    pstFm->nAge_Low         = 0xFFFFFFFF;

    /* initialize meta block index for meta compaction */
    pstFm->nVictimIdx       = 0;

    /* initialize working meta block */
    pstFm->nHeaderIdx       = NULL_VBN;
    pstFm->nCPOffs          = 0;

    /* initialize the latest header block index & context page offset */
    pstFm->nLatestBlkIdx    = 0;
    pstFm->nLatestPOffs     = 0;

    /* initialize idle meta block list */
    pstFm->nIdleBoListHead  = 0;
    pstFm->nNumOfIdleBlk    = pstZone->pstZI->nNumMetaBlks;

    /* initialize the index of minimum EC group of PMT */
    pstFm->nMinECPMTIdx     = 0;

    /* initialize number of log groups in PMT directory */
    pstFm->nPMTEntryCnt     = 0;

    /* initialize idle meta block list */
    for (nBlkOffs = 0; nBlkOffs < pstZone->pstZI->nNumMetaBlks; nBlkOffs++)
    {
        *(pstDH->pIdleBoList + nBlkOffs) = nBlkOffs;
    }

    /* initialize valid page count of each meta block */
    FSR_OAM_MEMSET(pstDH->pValidPgCnt, 0x00, 
                   pstZone->pstZI->nNumMetaBlks * sizeof(UINT16));

    /* initialize erase count of meta blocks */
    FSR_OAM_MEMSET(pstDH->pMetaBlksEC, 0x00, 
                   pstZone->pstZI->nNumMetaBlks * sizeof(UINT32));

    /* Compensate the number of BMT to a multiple of 2 */
    nNumBMT                  = ((pstZone->pstZI->nNumLA) & 0x1) ? pstZone->pstZI->nNumLA + 1 : pstZone->pstZI->nNumLA;

    /* initialize BMT directory */
    FSR_OAM_MEMSET(pstDH->pBMTDir, 0xFF, sizeof(POFFSET) * nNumBMT);

    /* get number of total log groups entries in PMT directory */
    nNumDirEntries = pstZone->pstML->nMaxInaLogGrps;

    /* initialize merge status flag bitstream */
    FSR_OAM_MEMSET(pstDH->pPMTMergeFlags, 0x00, sizeof(UINT8) * (nNumDirEntries >> 3));

    /* initialize PMT directory */
    FSR_OAM_MEMSET(pstDH->pstPMTDir, 0xFF, sizeof(STLPMTDirEntry) * nNumDirEntries);

    /* initialize PMT wear-leveling group information */
    FSR_OAM_MEMSET(pstDH->pstPMTWLGrp, 0xFF, 
                   sizeof(STLPMTWLGrp) * (nNumDirEntries >> DEFAULT_PMT_EC_GRP_SIZE_SHIFT));

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}


/**
 * @brief       This function initializes the context info object.
 *
 * @param[in]   pstZone     : zone object pointer
 * @param[in]   pstCtx      : pointer to CtxInfo object
 *
 * @return      none
 * @author      Jongtae Park, Wonmoon Cheon
 * @version     1.2.0
 *
 */
PUBLIC VOID
FSR_STL_InitCtx    (STLZoneObj     *pstZone,
                    STLCtxInfoHdl  *pstCtx)
{
    UINT32          nIdx;
    UINT32          nSystemBlks;
    STLZoneInfo    *pstZI;
    STLCtxInfoFm   *pstCI;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get zone info pointer */
    pstZI = pstZone->pstZI;

    /* get context info fixed members pointer */
    pstCI  = pstCtx->pstFm;

    /* fixed size members initialization --------------------------------------*/
    
    /* initialize age information (age value : 64bits decremental order) */
    pstCI->nAge_High            = 0xFFFFFFFF;
    pstCI->nAge_Low             = 0xFFFFFFFF;

    /* reset merge information in the context (POR) */
    pstCI->nMergedDgn           = NULL_DGN;
    pstCI->nMergedVbn           = NULL_VBN;
    pstCI->nMetaReclaimFlag     = 0;
    pstCI->nMergeVictimDgn      = NULL_DGN;
    pstCI->nMergeVictimFlag     = 0;

    /* reset modified cost information in the context (POR) */
    pstCI->nModifiedDgn         = NULL_DGN;
    pstCI->nModifiedCost        = (UINT16)(-1);

    /* reset number of active log */
    pstCI->nNumLBlks            = 0;

    /* reset number of free blocks */
    pstCI->nNumFBlks            = (UINT16)(pstZI->nNumFBlks);
    pstCI->nFreeListHead        = 0;

    /* reset number of idle data blocks */
    pstCI->nNumIBlks            = pstZI->nNumUserBlks;

    /* set minimum EC LAN to the last one */
    pstCI->nMinECLan            = (UINT16)(pstZI->nNumLA - 1);

    /* reset buffer block VBN */
#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    pstCI->nBBlkVbn             = (BADDR)(pstZI->nStartVbn + pstZI->nNumMetaBlks);
    nSystemBlks                 = pstZI->nNumMetaBlks + NUM_BU_BLKS;
#else   /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 0) */
    pstCI->nBBlkVbn             = NULL_VBN;
    nSystemBlks                 = pstZI->nNumMetaBlks;
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */
    
    /* reset temporary block VBN */
    if (pstZone->pstDevInfo->nDeviceType == RBW_DEVTYPE_MLC)
    {
        pstCI->nTBlkVbn         = NULL_VBN;
    }
    else
    {
        pstCI->nTBlkVbn         = NULL_VBN;
    }

    /* reset buffer block erase count */
    pstCI->nBBlkEC              = 0;

    /* reset temporary block erase count */
    pstCI->nTBlkEC              = 0;

    /* reset zone wear-leveling information */
    pstCI->nZoneWLMark          = 0;
    pstCI->nZone1Idx            = 0xFFFF;
    pstCI->nZone2Idx            = 0xFFFF;
    pstCI->nZone1Vbn            = NULL_VBN;
    pstCI->nZone2Vbn            = NULL_VBN;
    pstCI->nZone1EC             = 0;
    pstCI->nZone2EC             = 0;

    /* reset PMT wear-leveling information */
    pstCI->nMinECPMTIdx         = 0xFFFF;
    pstCI->nUpdatedPMTWLGrpIdx  = 0xFFFF;
    FSR_OAM_MEMSET(&(pstCI->stUpdatedPMTWLGrp), 0xFF, sizeof(STLPMTWLGrp));

    /* variable size members initialization ---------------------------------- */

    /* reset active log list */
    FSR_OAM_MEMSET(pstCtx->pstActLogList, 0xFF, sizeof(STLActLogEntry) * MAX_ACTIVE_LBLKS);

    /* set initial free block VBN list */
    for (nIdx = 0; nIdx < pstZI->nNumFBlks; nIdx++)
    {
        *(pstCtx->pFreeList + nIdx) = (BADDR)(pstZI->nStartVbn + nSystemBlks + nIdx);
    }
    for (; nIdx < pstZone->pstML->nMaxFreeSlots; nIdx++)
    {
        *(pstCtx->pFreeList + nIdx) = NULL_VBN;
    }

    /* initialize free blocks' EC */
    FSR_OAM_MEMSET(pstCtx->pFBlksEC,  0x00, sizeof(UINT32) * pstZone->pstML->nMaxFreeSlots);

    /* initialize minimum EC (BMT) */
    FSR_OAM_MEMSET(pstCtx->pMinEC,    0x00, sizeof(UINT32) * pstZI->nNumLA);

    /* initialize minimum EC index (BMT) */
    FSR_OAM_MEMSET(pstCtx->pMinECIdx, 0x00, sizeof(UINT16) * pstZI->nNumLA);

    /* ZBC confirm members initialization ------------------------------------ */
    pstCtx->pstCfm->nZBC        = FSR_STL_GetZBC(pstCtx->pBuf, pstCtx->nCfmBufSize);
    pstCtx->pstCfm->nInvZBC     = pstCtx->pstCfm->nZBC ^ 0xFFFFFFFF;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}


/**
 * @brief       This function initializes BMT object with the specified LAN.
 *
 * @param[in]   pstZone     : zone object pointer
 * @param[in]   pstBMT      : pointer to BMT object
 * @param[in]   nLan        : LAN to initialize
 *
 * @return      none
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC VOID
FSR_STL_InitBMT    (STLZoneObj     *pstZone,
                    STLBMTHdl      *pstBMT,
                    BADDR           nLan)
{
    BADDR           nBlkOffs;
    BADDR           nFirstVbn;
    UINT16          nNumDBlks;
    STLZoneInfo    *pstZI;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get zone info pointer */
    pstZI = pstZone->pstZI;

    /* set fixed size members */
    pstBMT->pstFm->nLan     = nLan;
    pstBMT->pstFm->nInvLan  = nLan ^ NULL_VBN;

    /* get first VBN of this local area */
#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    nFirstVbn  = pstZI->nStartVbn + pstZI->nNumMetaBlks + NUM_BU_BLKS;
#else
    nFirstVbn  = pstZI->nStartVbn + pstZI->nNumMetaBlks;
#endif
    nFirstVbn += pstZI->nNumFBlks + (BADDR)(nLan << pstZI->nDBlksPerLAShift);

    /* the last LA */
    if (nLan == (pstZI->nNumLA - 1))
    {
        /* In the last LA, there are only 
          (pstZI->nNumUserBlks % pstZI->nDBlksPerLA) blocks.*/
        nNumDBlks = ((pstZI->nNumUserBlks - 1) & (pstZI->nDBlksPerLA - 1)) + 1;
    }
    else
    {
        nNumDBlks = pstZI->nDBlksPerLA;
    }
    
    /* set variable size members */
    for (nBlkOffs = 0; nBlkOffs < pstZI->nDBlksPerLA; nBlkOffs++)
    {
        if (nBlkOffs < nNumDBlks)
        {
            pstBMT->pMapTbl[nBlkOffs].nVbn = nFirstVbn + nBlkOffs; 
        }
        else
        {
            /* invalid range */
            pstBMT->pMapTbl[nBlkOffs].nVbn = NULL_VBN;
        }

        /*  data block erase count table */
        pstBMT->pDBlksEC[nBlkOffs] = 0x00;
    }

    /* initially, all data blocks are garbage */
    FSR_OAM_MEMSET(pstBMT->pGBlkFlags, 0xFF, (pstZI->nDBlksPerLA >> 3));

    /* set confirm info */
    pstBMT->pstCfm->nZBC        = FSR_STL_GetZBC(pstBMT->pBuf, pstBMT->nCfmBufSize);
    pstBMT->pstCfm->nInvZBC     = pstBMT->pstCfm->nZBC ^ 0xFFFFFFFF;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}


/**
 * @brief       This function initializes all members in BU context.
 *
 * @param[in]   pstBUCtx    : pointer to BU context object
 *
 * @return      none
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC VOID
FSR_STL_InitBUCtx  (STLBUCtxObj    *pstBUCtx)
{
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    pstBUCtx->nBufState         = STL_BUFSTATE_CLEAN;
    pstBUCtx->nValidSBitmap     = 0;
    pstBUCtx->nBBlkCPOffset     = 0;
    pstBUCtx->nBufferedDgn      = NULL_DGN;
    pstBUCtx->nBufferedLpn      = NULL_VPN;
    pstBUCtx->nBufferedVpn      = NULL_VPN;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}


/**
 * @brief       This function initializes all members in deleted info context.
 *
 * @param[in]   pstDelCtx   : pointer to deleted info context object
 *
 * @return      none
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC VOID
FSR_STL_InitDelCtx (STLDelCtxObj   *pstDelCtx)
{
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    pstDelCtx->nDelPrevDgn      = NULL_DGN;
    pstDelCtx->nDelPrevLan      = NULL_VBN;
    pstDelCtx->pstDelLogGrpHdl  = NULL;
    pstDelCtx->nDelLpn          = NULL_VPN;
    pstDelCtx->nDelSBitmap      = 0;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}


/**
 * @brief       This function intializes the specified log object
 *
 * @param[in]   pstZone    : zone object 
 * @param[in]   pstLogGrp  : pointer to log group object
 * @param[in]   pstLog     : pointer to log object
 *
 * @return      none
 * @author      Jongtae Park, Wonmoon Cheon
 * @version     1.1.0
 *
 */
PUBLIC VOID
FSR_STL_InitLog(STLZoneObj     *pstZone,
                STLLogGrpHdl   *pstLogGrp,
                STLLog         *pstLog)
{
    const RBWDevInfo   *pstDev = pstZone->pstDevInfo;
    UINT16             *pLogVPgCnt;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    pstLog->nState          = LS_FREE;

    /* pstLog->nSelfIdx : DO NOT initialize self index */
    pstLog->nPrevIdx        = NULL_LOGIDX;
    pstLog->nNextIdx        = NULL_LOGIDX;

    pstLog->nVbn            = NULL_VBN;
    pstLog->nLastLpo        = NULL_POFFSET;

    pstLog->nCPOffs         = 0;
    pstLog->nLSBOffs        = 0;

    pstLog->nNOP            = 0;
    pstLog->nCSOffs         = 0;

    pstLog->nEC             = 0;

    /* initialize LBN list */
    FSR_OAM_MEMSET(&(pstLogGrp->pLbnList[pstLog->nSelfIdx]), 0xFF, 
                   sizeof(BADDR) << pstZone->pstRI->nNShift);

    /* initialize list of number of data block */
    pstLogGrp->pNumDBlks[pstLog->nSelfIdx] = 0;

    /* initialize list of number of valid pages in a log */
    pLogVPgCnt = pstLogGrp->pLogVPgCnt
               + (pstLog->nSelfIdx << pstDev->nNumWaysShift);
    FSR_OAM_MEMSET(pLogVPgCnt, 0x00, sizeof(UINT16) << pstDev->nNumWaysShift);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}


/**
 * @brief       This function initializes log group object.
 *
 * @param[in]   pstZone       : zone object
 * @param[in]   pstLogGrp     : pointer to log group handle
 *
 * @return      none
 *
 * @author      Jongtae Park, Wonmoon Cheon
 * @version     1.1.0
 *
 */
PUBLIC VOID
FSR_STL_InitLogGrp (STLZoneObj     *pstZone,
                    STLLogGrpHdl   *pstLogGrp)
{
    INT32           nIdx;
    STLLogGrpFm    *pstFm;
    STLLog         *pstLog;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get fixed size member pointer */
    pstFm = pstLogGrp->pstFm;

    /* initialize fixed size members */
    pstFm->nDgn             = NULL_DGN;
    pstFm->nMinVPgLogIdx    = NULL_LOGIDX;

    pstFm->nState           = 0;
    pstFm->nNumLogs         = 0;
    pstFm->nHeadIdx         = 0xFF;     /* 0xFF means null pointer */
    pstFm->nTailIdx         = 0xFF;

    pstFm->nMinVbn          = NULL_VBN;
    pstFm->nMinIdx          = 0;
    pstFm->nMinEC           = 0xFFFFFFFF;

    /* initialize log list */
    for (nIdx = 0; nIdx < pstZone->pstRI->nK; nIdx++)
    {
        /* get log object pointer */
        pstLog = pstLogGrp->pstLogList + nIdx;

        /* set self index */
        pstLog->nSelfIdx = (UINT8)nIdx;

        /* initialize log object */
        FSR_STL_InitLog(pstZone, pstLogGrp, pstLog);
    }

    /* initialize page mapping table */
    FSR_OAM_MEMSET(pstLogGrp->pMapTbl, 0xFF, 
                   sizeof(POFFSET) * pstZone->pstML->nPagesPerLGMT);

    /* initialize confirm value */
    pstLogGrp->pstCfm->nZBC     = FSR_STL_GetZBC(pstLogGrp->pBuf, pstLogGrp->nCfmBufSize);
    pstLogGrp->pstCfm->nInvZBC  = pstLogGrp->pstCfm->nZBC ^ 0xFFFFFFFF;

    /* initialize link pointer for doubly linked list */
    pstLogGrp->pPrev            = NULL;
    pstLogGrp->pNext            = NULL;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}


/**
 * @brief       This function initializes log group list
 *
 * @param[in]   pstLogGrpList     : pointer to log group List
 *
 * @return      none
 *
 * @author      Jongtae Park
 * @version     1.0.0
 *
 */
PUBLIC VOID
FSR_STL_InitLogGrpList (STLLogGrpList  *pstLogGrpList)
{
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /*  Log Group List Initialize */
    pstLogGrpList->nNumLogGrps  = 0;
    pstLogGrpList->pstHead      = NULL;
    pstLogGrpList->pstTail      = NULL;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}


/**
 * @brief       This function initializes STLRootInfo object.
 *
 * @param[in]   pstDev          : pointer to RBWDevInfo object
 * @param[in]   pstRI           : pointer to STLRootInfo object
 *
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_ERROR
 *
 * @author      Wonmoon Cheon
 * @version     1.2.0
 *
 */
PUBLIC INT32    
FSR_STL_InitRootInfo   (RBWDevInfo     *pstDev,
                        STLRootInfo    *pstRI)
{
    UINT32          nIdx;
    UINT32          nMetaPageSz;
    STLZoneInfo    *pstZI;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    FSR_ASSERT(sizeof(STLRootInfo) < ROOT_INFO_BUF_SIZE);

    /* set STL version code */
    pstRI->aSignature[0]  = 'F';
    pstRI->aSignature[1]  = 'S';
    pstRI->aSignature[2]  = 'R';
    pstRI->aSignature[3]  = '_';
    pstRI->aSignature[4]  = 'S';
    pstRI->aSignature[5]  = 'T';
    pstRI->aSignature[6]  = 'L';
    pstRI->aSignature[7]  = '\0';
    pstRI->nVersion       = FSR_VersionCode();

    /* set initial age value to all 0xFF (decremental order) */
    pstRI->nAge_High      = 0xFFFFFFFF;
    pstRI->nAge_Low       = 0xFFFFFFFF;

    /* set meta version by meta page size */
    nMetaPageSz = pstDev->nBytesPerVPg >> 10;
    switch (nMetaPageSz)
    {
        case 2 :    /* 2KB page format */
            pstRI->nMetaVersion = STL_META_VER_2K;
            /* mapping constant 'k' */
            pstRI->nK           = LBLKS_PER_GRP_2K_FORMAT;
            break;
        case 4 :    /* 4KB page format */
            pstRI->nMetaVersion = STL_META_VER_4K;
            /* mapping constant 'k' */
            pstRI->nK           = LBLKS_PER_GRP_4K_FORMAT;
            break;
        case 8 :    /* 8KB page format */
            pstRI->nMetaVersion = STL_META_VER_8K;
            /* mapping constant 'k' */
            pstRI->nK           = LBLKS_PER_GRP_8K_FORMAT;
            break;
        case 16 :   /* 16KB page format */
            pstRI->nMetaVersion = STL_META_VER_16K;
            /* mapping constant 'k' */
            pstRI->nK           = LBLKS_PER_GRP_16K_FORMAT;
            break;
        default :
            return FSR_STL_CRITICAL_ERROR;
    }

    /* Always root start VBN is 0 */
    pstRI->nRootStartVbn  = 0;
    pstRI->nRootCPOffs    = 0;

    pstRI->nNumZone       = 0;
    pstRI->nNumRootBlks   = MAX_ROOT_BLKS;
    FSR_OAM_MEMSET(pstRI->aRootBlkEC, 0x00, sizeof(UINT32) * MAX_ROOT_BLKS);

    /* default mapping constant 'N' (fixed) */
    pstRI->nN             = NUM_DBLKS_PER_GRP;
    pstRI->nNShift        = NUM_DBLKS_PER_GRP_SHIFT;

    /* 'k' shift value */
    pstRI->nKShift        = (UINT16)FSR_STL_GetShiftBit(pstRI->nK);
    
    /* initialize zone info objects */
    for (nIdx = 0; nIdx < MAX_ZONE; nIdx++)
    {
        /* get zone info object pointer */
        pstZI = &(pstRI->aZone[nIdx]);

        /* initialize zone info */
        FSR_STL_InitZoneInfo(pstZI);
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}


/**
 * @brief       This function initializes meta layout format.
 * @n           Before calling this, RBWDevInfo members must be calculated.
 *
 * @param[in]   pstDev          : pointer to RBWDevInfo
 * @param[in]   pstRI           : root info object pointer
 * @param[in]   pstML           : pointer to STLMetaLayout object
 *
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_ERR_UNKNOWN_META_FORMAT
 *
 * @author      Wonmoon Cheon
 * @version     1.2.0
 *
 */
PUBLIC INT32    
FSR_STL_InitMetaLayout (RBWDevInfo     *pstDev,
                        STLRootInfo    *pstRI,
                        STLMetaLayout  *pstML)
{
    UINT32      nMetaPageSz;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    nMetaPageSz = pstDev->nBytesPerVPg >> 10;
    switch (nMetaPageSz)
    {
        case 2 :    /* 2KB page format */
            FSR_OAM_MEMCPY(pstML, (VOID*)&(gastCfgTbl[0]), sizeof(STLMetaLayout));
            pstML->nMaxBlksPerZone = MAX_BLKS_PER_ZONE_2KF;
            break;

        case 4 :    /* 4KB page format */
            FSR_OAM_MEMCPY(pstML, (VOID*)&(gastCfgTbl[1]), sizeof(STLMetaLayout));
            pstML->nMaxBlksPerZone = MAX_BLKS_PER_ZONE_4KF;
            break;

        case 8 :    /* 8KB page format */
            FSR_OAM_MEMCPY(pstML, (VOID*)&(gastCfgTbl[2]), sizeof(STLMetaLayout));
            pstML->nMaxBlksPerZone = MAX_BLKS_PER_ZONE_8KF;
            break;

        case 16 :   /* 16KB page format */
            FSR_OAM_MEMCPY(pstML, (VOID*)&(gastCfgTbl[3]), sizeof(STLMetaLayout));
            pstML->nMaxBlksPerZone = MAX_BLKS_PER_ZONE_16KF;
            break;

        default :
            return FSR_STL_CRITICAL_ERROR;
    }

    /* set pages per log group page mapping table */
    pstML->nPagesPerLGMT    = pstRI->nN << pstDev->nPagesPerSbShift;

    /* set log group size in bytes */
    pstML->nBytesPerLogGrp  = sizeof(STLLogGrpFm) + 
                              (sizeof(STLLog)  << pstRI->nKShift) +
                              (sizeof(BADDR)   << (pstRI->nNShift + pstRI->nKShift)) +          /* pLbnList     */
                              (sizeof(UINT16)  << pstRI->nKShift) +                             /* pNumDBlks    */
                              (sizeof(UINT16)  << (pstRI->nKShift + pstDev->nNumWaysShift)) +   /* pLogVPgCnt   */
                              (sizeof(POFFSET) << (pstRI->nNShift + pstDev->nPagesPerSbShift)) +/* pMapTbl      */
                              sizeof(STLZbcCfm);

    /* Align 4 bytes for ARM9 (ARM9 does not support unaligned access) */
    if (pstML->nBytesPerLogGrp & 0x03)
    {
        pstML->nBytesPerLogGrp += 0x04 - (pstML->nBytesPerLogGrp & 0x03);
    }

    /* set maximum number of slots in free block list.
       this value must be multiple of 4. */
    pstML->nMaxFreeSlots    = (MAX_TOTAL_FBLKS + (3 * pstRI->nK)) >> 2;
    pstML->nMaxFreeSlots    <<= 2;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}


/**
 * @brief       This function registers operation mode and event callback funtion.
 * @n           If the operation mode is STL_OPMODE_SYNC, pfEventCbFunc
 * @n           parameter will be NULL. Otherwise, STL_Init() maintains the callback
 * @n           function pointer internally. If user want to initialize all clusters,
 * @n           this STL_Init() function should be called repeatedly for every volumes.
 * @n           This function allocates memory for STL cluster, and initializes
 * @n           root information of a cluster.
 * @n           [Initialization call sequence] : (pre-condition : LLD init was done)
 * @n             1) FSR_STL_InitCluster()
 * @n                2) FSR_STL_InitRootInfo()
 * @n                   3) FSR_STL_InitZoneInfo()
 * @n                4) FSR_STL_InitMetaLayout()
 * @n                5) FSR_STL_InitZoneObj()
 *
 * @param[in]   nClstID         : cluster id to initialize
 * @param[in]   nVolID          : volume identifier (RBW_VOLID_0, RBW_VOLID_1, ...)
 * @param[in]   eMode           : operation mode (RBW_OPMODE_SYNC or RBW_OPMODE_ASYNC)
 * @param[in]   pfEventCbFunc   : event callback funtion pointer (only for RBW_OPMODE_ASYNC)
 *
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_OUT_OF_MEMORY
 * @return      FSR_STL_INVALID_VOLUME_ID
 * @return      FSR_STL_INVALID_PARAM_OPMODE
 * @return      FSR_STL_ERR_UNKNOWN_META_FORMAT
 *
 * @author      Jongtae Park
 * @version     1.0.0
 *
 */
PUBLIC INT32
FSR_STL_InitCluster(UINT32              nClstID,
                    UINT32              nVolID,
                    UINT32              nPart,
                    RBW_OpMode          eMode,
                    RBW_AsyncEvtFunc    pfEventCbFunc)
{
    UINT32          nIdx;
    UINT32          nNumPart;
    UINT32          nZoneIdx;
    UINT32          nPartIdx;
    STLClstObj     *pstClst         = NULL;
    STLRootInfo    *pstRootInfo     = NULL;
    STLZoneObj     *pstZone         = NULL;
    STLPartObj     *pstPartObj      = NULL;
    STLPartObj     *pstTmpPartObj   = NULL;
    RBWDevInfo     *pstDevInfo      = NULL;
    BOOL32          bNewCluster     = FALSE32;
    INT32           nRet            = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    do
    {
        /* cluster id check */
        if (nClstID >= MAX_NUM_CLUSTERS)
        {
            nRet = FSR_STL_INVALID_PARAM;
            break;
        }

        /* volume id check */
        if (nVolID >= MAX_NUM_VOLUME)
        {
            nRet = FSR_STL_INVALID_VOLUME_ID;
            break;
        }

        /* get cluster object pointer */
        pstClst = gpstSTLClstObj[nClstID];

        /* Get STL partition object */
        pstPartObj = &(gstSTLPartObj[nVolID][nPart]);

        /* cluser object memory allocation */
        if (pstClst == NULL)
        {
            pstClst = (STLClstObj*)FSR_STL_MALLOC(sizeof(STLClstObj), 
                                            FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_SRAM);
            if (pstClst == NULL)
            {
                nRet = FSR_STL_OUT_OF_MEMORY;
                break;
            }
            else if (((UINT32)(pstClst)) & 0x03)
            {
                nRet = FSR_OAM_NOT_ALIGNED_MEMPTR;
                break;
            }

            /* set cluster object pointer */
            gpstSTLClstObj[nClstID] = pstClst;

            bNewCluster = TRUE32;
        }
        else
        {
            /* cluster object instantiation and initialization was already done */
            FSR_ASSERT(pstClst->pstDevInfo != NULL);

            /* get device info object pointer */
            pstDevInfo = pstClst->pstDevInfo;
            
            /* initialization of VFL parameters */
            FSR_STL_InitVFLParamPool(pstClst);

            break;
        }

        /* set volume id */
        pstClst->nVolID = nVolID;

        /* set global volume info pointer */
        pstClst->pstDevInfo = &(pstPartObj->stClstEV.stDevInfo);

        /* get device info object pointer */
        pstDevInfo = pstClst->pstDevInfo;

        /* get root info object pointer */
        pstRootInfo = &(pstClst->stRootInfoBuf.stRootInfo);

        /* initialize root info */
        nRet = FSR_STL_InitRootInfo(pstDevInfo, pstRootInfo);
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }

        /* initialize meta layout info */
        nRet = FSR_STL_InitMetaLayout(pstDevInfo, pstRootInfo, &(pstClst->stML));
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }

        /* initialization of VFL parameters */
        FSR_STL_InitVFLParamPool(pstClst);

        /* pTempPgBuf memory allocation */
        pstClst->pTempPgBuf = (UINT8*)FSR_STL_MALLOC(pstDevInfo->nBytesPerVPg,
                                  FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_SRAM);
        if (pstClst->pTempPgBuf == NULL)
        {
            nRet = FSR_STL_OUT_OF_MEMORY;
            break;
        }
        else if (((UINT32)(pstClst->pTempPgBuf)) & 0x03)
        {
            nRet = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }

        /* Initialize transaction begin mark */
        pstClst->bTransBegin = FALSE32;

        /* The maximum BML_Write request depends on pTempPgBuf size */
        pstClst->nMaxWriteReq = pstDevInfo->nBytesPerVPg / ((sizeof(FSRSpareBuf) + sizeof(FSRSpareBufBase)) * pstDevInfo->nSecPerVPg);

        for(nIdx = 0; nIdx < FSR_MAX_WAYS; nIdx++)
        {
            pstClst->staSBuf[nIdx].pstSpareBufBase = &(pstClst->staSBase[nIdx]);
        }

        for (nIdx = 0; bNewCluster && (nIdx < MAX_ZONE); nIdx++)
        {
            /* get zone object pointer */
            pstZone = &(pstClst->stZoneObj[nIdx]);

            /* set cluster id */
            pstZone->nClstID = nClstID;

            /* set zone id */
            pstZone->nZoneID = nIdx;

            /* set zone objects' every pointers to null */
            FSR_STL_ResetZoneObj(nVolID,
                                pstDevInfo,
                                pstRootInfo,
                               &(pstClst->stML),
                                pstZone);
        }

        /* Set partition index */
        pstTmpPartObj = pstPartObj->pst1stPart;
        nNumPart      = pstPartObj->nNumPart;
        for (nPartIdx = 0; nPartIdx < nNumPart; nPartIdx++)
        {
            for (nZoneIdx = 0; nZoneIdx < pstTmpPartObj->nNumZone; nZoneIdx++)
            {
                pstClst->stZoneObj[pstTmpPartObj->nZoneID + nZoneIdx].nPart = pstPartObj->nPart;
            }
            pstTmpPartObj++;
        }

        /* check operation mode */
        if (eMode == RBW_OPMODE_ASYNC)
        {
            nRet = FSR_STL_INVALID_PARAM_OPMODE;
            break;
        }
    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}

/**
 * @brief       This function return cluster object.
 *
 * @param[in]   nClstID         : cluster id
 *
 * @return      cluster object pointer
 *
 * @author      Wonmoon Cheon
 * @version     1.2.0
 *
 */
PUBLIC STLClstObj*
FSR_STL_GetClstObj  (UINT32 nClstID)
{
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return gpstSTLClstObj[nClstID];
}
