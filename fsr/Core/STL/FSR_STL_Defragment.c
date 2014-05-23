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
 *  @file       FSR_STL_Defragment.c
 *  @brief      STL defragmentation utility.
 *  @author     Jeong-Uk Kang
 *  @date       09-MAR-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  09-OCT-2007 [Jeong-Uk Kang] : first writing
 *  @n  29-JAN-2008 [MinYoung Kim] : dead code elimination
 *
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

#if defined (FSR_STL_FOR_PRE_PROGRAMMING)
/*****************************************************************************/
/* Global variable definitions                                               */
/*****************************************************************************/

/*****************************************************************************/
/* Imported variable declarations                                            */
/*****************************************************************************/

/*****************************************************************************/
/* Imported function prototype                                               */
/*****************************************************************************/

/*****************************************************************************/
/* Local macro                                                               */
/*****************************************************************************/
#define     UNIT_TYPE_NONE      ((UNITTYPE)(-1))
#define     UNIT_TYPE_ROOT      (1)
#define     UNIT_TYPE_META      (2)
#define     UNIT_TYPE_FREE      (3)
#define     UNIT_TYPE_USER      (4)
#define     UNIT_TYPE_RSRV      (5)

#define     REPORT_PROGRESS(pstDfrg)                                    \
        if (pstDfrg->pfProgress != NULL)                                \
        {                                                               \
            pstDfrg->nPrgCnt++;                                         \
            if ((pstDfrg->nPrgCnt % PROGRESS_INTERVAL) == 0)            \
            {                                                           \
                pstDfrg->pfProgress(MAX_STAGE + 1,                      \
                                    pstDfrg->nCurStage,                 \
                                    NULL);                      \
            }                                                           \
        }

#define     STAGE_PROGRESS(stDfrg)                              \
        stDfrg.nCurStage++;                                     \
        stDfrg.nPrgCnt = 0;                                     \
        if (stDfrg.pfProgress != NULL)                          \
        {                                                       \
            stDfrg.pfProgress(MAX_STAGE + 1,                    \
                              stDfrg.nCurStage,                 \
                              gszStageDesc[stDfrg.nCurStage]);  \
        }

/*****************************************************************************/
/* Local type defines                                                        */
/*****************************************************************************/

typedef     UINT8      UNITTYPE;

typedef struct {
    UNITTYPE        nType;
    UINT8           nZoneID;
    BADDR           nDgn;
} StlVUInfo;

typedef struct {
    BADDR           nStartVbn;
    BADDR           nNumVbn;
    BADDR           nNumMeta;
    BADDR           nNumUser;
    BADDR           nNumUMeta;
    BADDR           nUsedSrt;
} StlZoneVU;

typedef struct {
    STLClstObj     *pstClst;
    UINT32          nMaxBlk;
    UINT32          nPrgCnt;
    UINT32          nCurStage;
    FSRStlPrgFp     pfProgress;
    StlZoneVU      *pstaZVUs;
    StlVUInfo      *pstaVUs;
} StlDfrgArg;

/*****************************************************************************/
/* Local constant definitions                                                */
/*****************************************************************************/
#define     PROGRESS_INTERVAL           (10)
#define     MAX_STAGE                   (6)

PRIVATE INT8  *gszStageDesc[MAX_STAGE + 2] = {
        "Stage Description",
        "Resetting erase counter of each virtual unit",
        "Compacting all user data",
        "Scanning and initializing all virtual units",
        "Defragmenting all user data",
        "Erasing all invalid units",
        "Defragmenting all meta data",
        "Finalizing..."
};

/*****************************************************************************/
/* Local (static) function prototype                                         */
/*****************************************************************************/
PRIVATE INT32   _FillVirtualUnitStatus (STLZoneObj     *pstZone,
                                        StlDfrgArg     *pstDfrg);

PRIVATE INT32   _CheckSpare4Copyback   (STLZoneObj     *pstZone,
                                        const PADDR     nLpn,
                                        const PADDR     nSrcOff,
                                        const PADDR     nDstOff,
                                        FSRSpareBuf    *pstSBuf);

PRIVATE INT32   _CopyLogicalUnit       (STLZoneObj     *pstZone,
                                        STLLogGrpHdl   *pstLogGrp,
                                        BADDR           nTrgVbn,
                                        BOOL32          bAddFblk,
                                        BOOL32         *bUseTrg);

PRIVATE INT32   _CopyLogGroup          (STLZoneObj     *pstZone,
                                        const BADDR     nDgn,
                                        const BADDR     nTrgVbn,
                                              BOOL32   *bUseTrg);

PRIVATE INT32   _DefragmentLogGroups   (STLZoneObj     *pstZone,
                                        StlDfrgArg     *pstDfrg);

PRIVATE INT32   _DefragmentZone        (STLZoneObj     *pstZone,
                                        StlDfrgArg     *pstDfrg);

PRIVATE INT32   _ResetEraseCount       (StlDfrgArg     *pstDfrg);

PRIVATE INT32   _CopyMetaUnit          (STLZoneObj     *pstZone,
                                        BADDR           nSrcVbn,
                                        BADDR           nDstVbn);

PRIVATE INT32   _CopyUsedUnit          (STLClstObj     *pstClst,
                                        const BADDR     nSrcVun,
                                        const BADDR     nDstVun,
                                        StlVUInfo      *pstaVUs);

PRIVATE INT32   _RearrangeLogGroups    (STLClstObj     *pstClst,
                                        const UINT32    nTrgZone,
                                        StlDfrgArg     *pstDfrg);

PRIVATE INT32   _RearrangeVUs          (STLClstObj     *pstClst,
                                        StlDfrgArg     *pstDfrg);

PRIVATE INT32   _ResetFreeUnits        (STLClstObj     *pstClst,
                                        StlDfrgArg     *pstDfrg);

PUBLIC INT32    _ReclaimMeta           (STLZoneObj     *pstZone,
                                        UINT32          nVictimIdx);

PRIVATE INT32   _DegragmentMetas       (STLClstObj     *pstClst,
                                        StlDfrgArg     *pstDfrg);


/*****************************************************************************/
/* Local (static)  Function Definition                                       */
/*****************************************************************************/

/**
 *  @brief          Fill Virtual Unit Status.
 *
 *  @param[in]      pstZone     : zone object
 *  @param[in]      nNumBlks    : The total number of unit
 *  @param[out]     pstaVU      : The status of each unit
 *
 *  @return         FSR_STL_SUCCESS
 *  @author         Jeong-Uk Kang
 *  @version        1.1.0
 */
PRIVATE INT32 
_FillVirtualUnitStatus (STLZoneObj     *pstZone,
                        StlDfrgArg     *pstDfrg)
{
    STLCtxInfoHdl  *pstCtx      = pstZone->pstCtxHdl;
    STLDirHdrHdl   *pstDH       = pstZone->pstDirHdrHdl;
    STLZoneInfo    *pstZI       = pstZone->pstZI;
    const   UINT32  nMaxFBlk    = pstZone->pstML->nMaxFreeSlots;
    const   UINT32  nNumBlks    = pstDfrg->nMaxBlk;
    StlVUInfo      *pstaVU      = pstDfrg->pstaVUs;
    BADDR           nVbn;
    UINT32          nIdx;
    UINT32          nTempIdx;
    STLLogGrpHdl   *pstLogGrp   = NULL;
    STLLog         *pstLog      = NULL;
    BADDR           nLan;
    UINT32          nNumDBlks;
    STLBMTHdl      *pstBMT      = NULL;
    UINT32          nPMTIdx;
    BADDR           nDgn;
    POFFSET         nMetaPOffs;
    INT32           nRet        = FSR_STL_SUCCESS;
    UINT32          nPMTEntryCnt;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* step0) Active log list and inactive log list must be flushed */
    FSR_ASSERT(pstZone->pstActLogGrpList->nNumLogGrps  == 0);
    FSR_ASSERT(pstZone->pstInaLogGrpCache->nNumLogGrps == 0);

    /* step1) root blocks' EC */
    for (nIdx = 0; nIdx < pstZone->pstRI->nNumRootBlks; nIdx++)
    {
        nVbn = (BADDR)(nIdx);

        if (nVbn < nNumBlks)
        {
            FSR_ASSERT((pstaVU[nVbn].nType == UNIT_TYPE_NONE) ||
                       (pstaVU[nVbn].nType == UNIT_TYPE_ROOT));
            pstaVU[nVbn].nType   = UNIT_TYPE_ROOT;
            pstaVU[nVbn].nDgn    = NULL_DGN;
        }
    }

    /* step2) meta blocks' EC */
    for (nIdx = 0; nIdx < pstZI->nNumMetaBlks; nIdx++)
    {
        nVbn = pstZI->aMetaVbnList[nIdx];

        if (nVbn < nNumBlks)
        {
            FSR_ASSERT(pstaVU[nVbn].nType == UNIT_TYPE_NONE);
            pstaVU[nVbn].nType   = UNIT_TYPE_META;
            pstaVU[nVbn].nZoneID = (UINT8)pstZone->nZoneID;
            pstaVU[nVbn].nDgn    = NULL_DGN;
        }
    }

    /* step3) buffer, temporary blocks' EC */
    nVbn = pstCtx->pstFm->nBBlkVbn;

    if (nVbn < nNumBlks)
    {
        FSR_ASSERT(pstaVU[nVbn].nType == UNIT_TYPE_NONE);
        pstaVU[nVbn].nType   = UNIT_TYPE_FREE;
        pstaVU[nVbn].nZoneID = (UINT8)pstZone->nZoneID;
        pstaVU[nVbn].nDgn    = NULL_DGN;
    }

    nVbn = pstCtx->pstFm->nTBlkVbn;

    if (nVbn < nNumBlks)
    {
        FSR_ASSERT(pstaVU[nVbn].nType == UNIT_TYPE_NONE);
        pstaVU[nVbn].nType   = UNIT_TYPE_FREE;
        pstaVU[nVbn].nZoneID = (UINT8)pstZone->nZoneID;
        pstaVU[nVbn].nDgn    = NULL_DGN;
    }

    /* step4) free/garbage blocks' EC */
    for (nIdx = 0; nIdx < pstCtx->pstFm->nNumFBlks; nIdx++)
    {
        nTempIdx = (pstCtx->pstFm->nFreeListHead + nIdx);
        if (nTempIdx >= nMaxFBlk)
        {
            nTempIdx -= nMaxFBlk;
        }
        FSR_ASSERT(nTempIdx < nMaxFBlk);
        nVbn = pstCtx->pFreeList[nTempIdx];

        if (nVbn < nNumBlks)
        {
            FSR_ASSERT(pstaVU[nVbn].nType == UNIT_TYPE_NONE);
            pstaVU[nVbn].nType   = UNIT_TYPE_FREE;
            pstaVU[nVbn].nZoneID = (UINT8)pstZone->nZoneID;
            pstaVU[nVbn].nDgn    = NULL_DGN;
        }
    }

    REPORT_PROGRESS(pstDfrg);

    /* step5) data blocks' EC (read all BMTs) */
    for (nLan = 0; nLan < pstZI->nNumLA; nLan++)
    {
        nRet = FSR_STL_LoadBMT(pstZone, nLan, FALSE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
            return nRet;
        }

        pstBMT = pstZone->pstBMTHdl;

        if (nLan ==(pstZI->nNumLA - 1))
        {
            /* the last LA */
            FSR_ASSERT(pstZI->nNumUserBlks > 0);
            nNumDBlks = ((pstZI->nNumUserBlks - 1) & (pstZI->nDBlksPerLA - 1)) + 1;
        }
        else
        {
            nNumDBlks = pstZI->nDBlksPerLA;
        }

        for (nIdx = 0; nIdx < nNumDBlks; nIdx++)
        {
            nVbn = pstBMT->pMapTbl[nIdx].nVbn;

            if ((nVbn <  nNumBlks) &&
                (nVbn != NULL_VBN))
            {
                FSR_ASSERT(pstaVU[nVbn].nType == UNIT_TYPE_NONE);
                FSR_ASSERT((pstBMT->pGBlkFlags[nIdx >> 3] & (1 << (nIdx & 0x07))) != 0);
                pstaVU[nVbn].nType   = UNIT_TYPE_FREE;
                pstaVU[nVbn].nZoneID = (UINT8)pstZone->nZoneID;
                pstaVU[nVbn].nDgn    = NULL_DGN;
            }
        }

        REPORT_PROGRESS(pstDfrg);
    }

    /* step6) data blocks' EC (read all PMTs) */
    nPMTEntryCnt = ((pstZI->nNumUserBlks - 1) >> pstZone->pstRI->nNShift) + 1;
    for (nPMTIdx = 0; nPMTIdx < nPMTEntryCnt; nPMTIdx++)
    {
        nDgn        = (BADDR)nPMTIdx;
        nMetaPOffs  = pstDH->pstPMTDir[nPMTIdx].nPOffs;
        if (nMetaPOffs == NULL_POFFSET)
        {
            continue;
        }

        pstLogGrp = NULL;
        nRet = FSR_STL_LoadPMT(pstZone, nDgn, nMetaPOffs, &pstLogGrp, FALSE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
            return nRet;
        }

        if (pstLogGrp->pstFm->nNumLogs > 0)
        {
            pstLog = pstLogGrp->pstLogList + pstLogGrp->pstFm->nHeadIdx;
            for (nIdx = 0; nIdx < pstLogGrp->pstFm->nNumLogs; nIdx++)
            {
                nVbn = pstLog->nVbn;
                FSR_ASSERT(nVbn != NULL_VBN);

                if (nVbn < nNumBlks)
                {
                    FSR_ASSERT(pstaVU[nVbn].nType == UNIT_TYPE_NONE);
                    pstaVU[nVbn].nType   = UNIT_TYPE_USER;
                    pstaVU[nVbn].nZoneID = (UINT8)pstZone->nZoneID;
                    pstaVU[nVbn].nDgn    = pstLogGrp->pstFm->nDgn;
                }

                pstLog = pstLogGrp->pstLogList + pstLog->nNextIdx;
            }
        }

        REPORT_PROGRESS(pstDfrg);
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}

/**
 *  @brief        This function checks and make the PTF.
 *
 *  @param[in]    pstZone  : zone object
 *  @param[in]    nLpn     : LPN of source (data)
 *  @param[in]    nSrcOff  : The page offset of source VPN
 *  @param[in]    nDstOff  : The page offset of destination VPN
 *  @param[in]    pstSBuf  : Spare buffer of BML
 *
 *  @return       FSR_STL_SUCCESS
 *  @author       Jeong-Uk Kang
 *  @version      1.1.0
 */
PRIVATE INT32 
_CheckSpare4Copyback   (STLZoneObj     *pstZone,
                        const PADDR     nLpn,
                        const PADDR     nSrcOff,
                        const PADDR     nDstOff,
                        FSRSpareBuf    *pstSBuf)
{
    const   BOOL32      bSrcLsb     = FSR_STL_FlashIsLSBPage(pstZone, nSrcOff);
    const   BOOL32      bDstLsb     = FSR_STL_FlashIsLSBPage(pstZone, nDstOff);
            UINT32      nLpo;
            UINT32      nPTF;
            UINT32      nRndInCnt;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    if (bSrcLsb == bDstLsb)
    {
        nRndInCnt = 0;
    }
    else
    {
        /*
         * prepare spare data
         * First, fIll SData1 and SData2 as follows
         *      For LSB, SData1 = LPO.~LPO, SData2 = 0xFFFFFFFF
         *      For LSB, SData1 = 0xFFFFFFFF, SData2 = LPO.~LPO
         */
        nLpo = (GET_REMAINDER(nLpn, pstZone->pstDevInfo->nPagesPerSbShift + pstZone->pstRI->nNShift) & 0xffff);
        if (bDstLsb)
        {
            pstSBuf->pstSpareBufBase->nSTLMetaBase0 = (nLpo << 16) | (nLpo ^ 0xFFFF);
            pstSBuf->pstSpareBufBase->nSTLMetaBase1 = (UINT32)(-1);
            nPTF = FSR_STL_SetPTF(pstZone, TF_LOG, 0, MIDLSB, 0);
        }
        else
        {
            pstSBuf->pstSpareBufBase->nSTLMetaBase0 = (UINT32)(-1);
            pstSBuf->pstSpareBufBase->nSTLMetaBase1 = (nLpo << 16) | (nLpo ^ 0xFFFF);
            nPTF = FSR_STL_SetPTF(pstZone, TF_LOG, 0, MIDMSB, 0);
        }

        /* Fill PTF into SData3 */
        pstSBuf->pstSpareBufBase->nSTLMetaBase2 = (nPTF << 16) | (0xffff & (~nPTF));

        nRndInCnt = 1;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRndInCnt));
    return nRndInCnt;
}

/**
 *  @brief          Copy all valid pages in the given log group to the given target vbn
 *
 *  @param[in]      pstZone     : STLZoneObj pointer object
 *  @param[in]      pstLogGrp   : The given log group object
 *  @param[in]      nTrgVbn     : The Virtual Unit Number (VBN) of destination
 *  @param[in]      nTrgEC      : The Erase Counter (EC)        of destination
 *  @param[out]     bUseTrg     : Is the nTrgVbn used?
 *
 *  @return       FSR_STL_SUCCESS
 *  @author       Jeong-Uk Kang
 *  @version      1.1.0
 */
PRIVATE INT32
_CopyLogicalUnit       (STLZoneObj     *pstZone,
                        STLLogGrpHdl   *pstLogGrp,
                        BADDR           nTrgVbn,
                        BOOL32          bAddFblk,
                        BOOL32         *bUseTrg)
{
    const RBWDevInfo       *pstDev      = pstZone->pstDevInfo;
    const UINT32            nPgsPerUnit = pstDev->nPagesPerSBlk;
    const UINT32            nPgsSft     = pstDev->nPagesPerSbShift;
          STLLogGrpFm      *pstLogGrpFm = pstLogGrp->pstFm;
          STLLog           *pstLogList  = pstLogGrp->pstLogList;
          STLLog           *pstLog;
          UINT16           *pLogVPgCnt;
          PADDR             nStartLpn;
          UINT8             nLogIdx;
          BADDR             nSrcVbn;
          POFFSET           nSrcOff;
          STLClstObj       *pstClst     = FSR_STL_GetClstObj(pstZone->nClstID);
    const UINT32            n1stVun     = pstClst->pstEnvVar->nStartVbn;
          BMLCpBkArg      **ppstBMLCpBk = pstClst->pstBMLCpBk;
          BMLCpBkArg       *pstCpBkArg  = pstClst->staBMLCpBk;
          BMLRndInArg      *pstRndIn    = pstClst->staBMLRndIn;
          FSRSpareBuf      *pstSBuf     = pstClst->staSBuf;
          PADDR            *naSrcVpn;
          PADDR            *naSrcLpn;
          UINT32            nRemainPgs;
          UINT32            nRemainCnt;
          UINT32            naDstIdx[FSR_MAX_WAYS];
          UINT32            naSrcIdx[FSR_MAX_WAYS];
          UINT32            nWay;
          INT32             nRet        = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    FSR_ASSERT(pstZone->pstRI->nN    == 1);
    FSR_ASSERT(pstLogGrpFm->nNumLogs >  0)
    FSR_ASSERT(pstLogGrpFm->nNumLogs <= pstZone->pstRI->nK);
    /* We use the pTempPgBuf to save PMT and LPN */
    FSR_ASSERT(nPgsPerUnit * sizeof(PADDR) * 2 <= pstDev->nBytesPerVPg);

    /* initialize naSrcVpn, naSrcLpn, and bUseTrg */
    *bUseTrg = FALSE32;
    naSrcVpn = (PADDR*)(pstClst->pTempPgBuf);
    naSrcLpn = (PADDR*)(pstClst->pTempPgBuf + (sizeof (PADDR) << nPgsSft));
    FSR_OAM_MEMSET(naSrcVpn, 0xFF, sizeof (PADDR) << nPgsSft);
    FSR_OAM_MEMSET(naSrcLpn, 0xFF, sizeof (PADDR) << nPgsSft);

    /* Find the number of valid pages in this log group */
    nRemainPgs = 0;
    nLogIdx    = pstLogGrpFm->nHeadIdx;
    while (nLogIdx != NULL_LOGIDX)
    {
        pLogVPgCnt = pstLogGrp->pLogVPgCnt
                   + (nLogIdx << pstDev->nNumWaysShift);
        for (nWay = 0; nWay < pstDev->nNumWays; nWay++)
        {
            nRemainPgs += pLogVPgCnt[nWay];
        }

        nLogIdx = pstLogList[nLogIdx].nNextIdx;
    }

    do
    {
        /* process the garbage blocks if required */
        nRet = FSR_STL_ReverseGC(pstZone, pstLogGrpFm->nNumLogs);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        /* Reserve meta page */
        nRet = FSR_STL_ReserveMetaPgs(pstZone, 1, FALSE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        /* Get all valid pages from PMT */
        for (nWay = 0; nWay < pstDev->nNumWays; nWay++)
        {
            naSrcIdx[nWay] = nWay;
            naDstIdx[nWay] = nWay;
        }
        nWay       = 0;
        nStartLpn  = pstLogGrp->pstFm->nDgn << (pstZone->pstRI->nNShift + nPgsSft);
        nRemainCnt = nRemainPgs;
        while (nRemainCnt != 0)
        {
            while (naSrcIdx[nWay] < nPgsPerUnit)
            {
                if (pstLogGrp->pMapTbl[naSrcIdx[nWay]] != NULL_POFFSET)
                {
                    /* source page address*/
                    nLogIdx = (UINT8)  ((pstLogGrp->pMapTbl[naSrcIdx[nWay]]) >> nPgsSft);
                    nSrcOff = (POFFSET)((pstLogGrp->pMapTbl[naSrcIdx[nWay]]) & (nPgsPerUnit - 1));
                    nSrcVbn = pstLogList[nLogIdx].nVbn;

                    /* Copy VPN and LPN of source */
                    naSrcVpn[naDstIdx[nWay]] = (nSrcVbn << nPgsSft) + nSrcOff;
                    naSrcLpn[naDstIdx[nWay]] = nStartLpn + naSrcIdx[nWay];

                    /* Reset PMT */
                    pstLogGrp->pMapTbl[naSrcIdx[nWay]] = NULL_POFFSET;

                    naSrcIdx[nWay] += pstDev->nNumWays;
                    naDstIdx[nWay] += pstDev->nNumWays;

                    FSR_ASSERT(nRemainCnt > 0);
                    nRemainCnt--;
                    break;
                }

                naSrcIdx[nWay] += pstDev->nNumWays;
            }

            nWay++;
            if (nWay >= pstDev->nNumWays)
            {
                nWay = 0;
            }
        }

        /* Remove all log from log group */
        nLogIdx = pstLogGrpFm->nTailIdx;
        while (nLogIdx != NULL_LOGIDX)
        {
            FSR_ASSERT(nLogIdx < pstZone->pstRI->nK);

            pstLog = pstLogList + nLogIdx;
            FSR_ASSERT(pstLog->nVbn != NULL_VBN);
            FSR_ASSERT(FSR_STL_CheckFullFreeList(pstZone) == FALSE32);
            FSR_ASSERT((pstLog->nState & (1 << LS_ACTIVE_SHIFT)) == 0);

            if (bAddFblk == TRUE32)
            {
                FSR_STL_AddFreeList(pstZone, pstLog->nVbn, 0);
            }

            /* We get the next log index before removing the log */
            nLogIdx = pstLogList[nLogIdx].nPrevIdx;

            /* remove the log from the group*/
            FSR_STL_RemoveLog(pstZone, pstLogGrp, pstLog);
        }

        if (nRemainPgs > 0)
        {
            FSR_ASSERT(nTrgVbn != NULL_VBN);
            /* Erase the free block to copy data in the current log block.*/
            nRet = FSR_STL_FlashErase(pstZone, nTrgVbn);
            if (nRet != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }

            /* Set New log and the CPOffs to last */
            pstLog = FSR_STL_AddNewLog(pstZone, pstLogGrp,
                            pstLogGrp->pstFm->nDgn << pstZone->pstRI->nNShift,
                            nTrgVbn, 1);
            if (pstLog == NULL)
            {
                /* critical error*/
                nRet = FSR_STL_ERR_NO_LOG;
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }

            pstLog->nCPOffs = (POFFSET)nPgsPerUnit;
        }
        else
        {
            pstLog = NULL;
        }

        /* Copy all valid pages from aValidPgs[] */
        FSR_OAM_MEMSET(ppstBMLCpBk, 0x00, sizeof(long) * FSR_MAX_WAYS);
        for (nWay = 0; nWay < pstDev->nNumWays; nWay++)
        {
            naDstIdx[nWay] = nWay;
        }
        nWay       = 0;
        nRemainCnt = nRemainPgs;
        while (nRemainCnt != 0)
        {
            while (naDstIdx[nWay] < nPgsPerUnit)
            {
                if (naSrcVpn[naDstIdx[nWay]] != NULL_VPN)
                {
                    FSR_ASSERT(naSrcLpn[naDstIdx[nWay]] != NULL_VPN);

                    /* source page address*/
                    nSrcVbn = (BADDR)  (naSrcVpn[naDstIdx[nWay]] >> nPgsSft);
                    nSrcOff = (POFFSET)(naSrcVpn[naDstIdx[nWay]] & (nPgsPerUnit - 1));

                    pstCpBkArg[nWay].nSrcVun        = (UINT16)(nSrcVbn + n1stVun);
                    pstCpBkArg[nWay].nSrcPgOffset   = (UINT16)(nSrcOff);
                    pstCpBkArg[nWay].nDstVun        = (UINT16)(nTrgVbn + n1stVun);
                    pstCpBkArg[nWay].nDstPgOffset   = (UINT16)(naDstIdx[nWay]);
                    pstCpBkArg[nWay].nRndInCnt      =_CheckSpare4Copyback(pstZone,
                                                        naSrcLpn[naDstIdx[nWay]],
                                                        nSrcOff, naDstIdx[nWay],
                                                        &(pstSBuf[nWay]));
                    if (pstCpBkArg[nWay].nRndInCnt == 1)
                    {
                        pstRndIn[nWay].nNumOfBytes      = (UINT16)(FSR_SPARE_BUF_STL_BASE_SIZE);
                        pstRndIn[nWay].nOffset          = (UINT16)(FSR_STL_CPBK_SPARE_BASE);
                        pstRndIn[nWay].pBuf             = &(pstSBuf[nWay].pstSpareBufBase->nSTLMetaBase0);

                        pstCpBkArg[nWay].pstRndInArg    = (BMLRndInArg*)&(pstRndIn[nWay]);
                    }
                    else
                    {
                        FSR_ASSERT(pstCpBkArg[nWay].nRndInCnt == 0);
                        pstCpBkArg[nWay].pstRndInArg    = NULL;
                    }

                    ppstBMLCpBk[nWay] = &(pstCpBkArg[nWay]);

                    FSR_ASSERT(nRemainCnt > 0);
                    nRemainCnt--;

                    /* log group PMT update*/
                    FSR_STL_UpdatePMT(pstZone, pstLogGrp, pstLog, naSrcLpn[naDstIdx[nWay]],
                                        (nTrgVbn << nPgsSft) + naDstIdx[nWay]);

                    /* change the state of log block*/
                    FSR_STL_ChangeLogState(pstZone, pstLogGrp, pstLog, naSrcLpn[naDstIdx[nWay]]);

                    naDstIdx[nWay] += pstDev->nNumWays;
                    break;
                }

                naDstIdx[nWay] += pstDev->nNumWays;
            }

            nWay++;
            if (nWay >= pstDev->nNumWays)
            {
                nWay = 0;
            }

            if ((nWay       == 0) ||
                (nRemainCnt == 0))
            {
                nRet = FSR_BML_CopyBack(pstZone->nVolID, ppstBMLCpBk, FSR_BML_FLAG_NONE);
                if (nRet != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR] %s() L(%d) - FSR_BML_CopyBack (0x%x)\r\n"), 
                        __FSR_FUNC__, __LINE__, nRet));

                    FSR_ASSERT(nRet != FSR_BML_VOLUME_NOT_OPENED);
                    FSR_ASSERT(nRet != FSR_BML_INVALID_PARAM);
                    FSR_ASSERT(nRet != FSR_BML_WR_PROTECT_ERROR);

                    return nRet;
                }

                FSR_OAM_MEMSET(ppstBMLCpBk, 0x00, sizeof(long) * FSR_MAX_WAYS);
            }
        }

        /* Update meta information */

        /* update merge cost */
        FSR_STL_UpdatePMTCost(pstZone, pstLogGrp);

        /* clear the merge victim flag bit*/
        FSR_STL_SetMergeVictimGrp(pstZone, pstLogGrp->pstFm->nDgn, FALSE32);

        /* Store the PMT */
        nRet = FSR_STL_StorePMTCtx(pstZone, pstLogGrp, FALSE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        FSR_ASSERT(pstLogGrpFm->nNumLogs < 2);

        if (pstLog != NULL)
        {
            *bUseTrg = TRUE32;
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}

/**
 *  @brief        This function checks and make the PTF.
 *
 *  @param[in]    pstZone  : zone object
 *  @param[in]    nLpn     : LPN of source (data)
 *  @param[in]    nSrcOff  : The page offset of source VPN
 *  @param[in]    nDstOff  : The page offset of destination VPN
 *  @param[in]    pstSBuf  : Spare buffer of BML
 *
 *  @return       FSR_STL_SUCCESS
 *  @author       Jeong-Uk Kang
 *  @version      1.1.0
 */
PRIVATE INT32
_CopyLogGroup          (STLZoneObj     *pstZone,
                        const BADDR     nDgn,
                        const BADDR     nTrgVbn,
                              BOOL32   *bUseTrg)
{
    STLLogGrpHdl   *pstLogGrp;
    POFFSET         nMPOff;
    INT32           nRet;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    do
    {
#if (OP_STL_DEBUG_CODE == 1)
        /* Find log group.*/
        pstLogGrp = FSR_STL_SearchLogGrp(pstZone->pstActLogGrpList, nDgn);
        FSR_ASSERT(pstLogGrp == NULL);
        pstLogGrp = FSR_STL_SearchLogGrp(pstZone->pstInaLogGrpCache, nDgn);
        FSR_ASSERT(pstLogGrp == NULL);
#endif /* (OP_STL_DEBUG_CODE == 1) */
        pstLogGrp = NULL;
        nRet = FSR_STL_SearchPMTDir(pstZone, nDgn, &nMPOff);
        if (nMPOff != NULL_POFFSET)
        {
            /* load PMT*/
            nRet = FSR_STL_LoadPMT(pstZone, nDgn, nMPOff, &pstLogGrp, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }
        }

        if (pstLogGrp != NULL)
        {
            FSR_ASSERT(pstLogGrp->pstFm->nDgn == nDgn);

            nRet = _CopyLogicalUnit(pstZone, pstLogGrp, nTrgVbn, FALSE32, bUseTrg);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }
        }
    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}

/**
 *  @brief          Defragment all log groups to reduce the number of logs
 *
 *  @param[in]      pstZone     : partition object
 * 
 *  @return         FSR_STL_SUCCESS
 *
 *  @return         FSR_STL_SUCCESS
 *  @author         Jeong-Uk Kang
 *  @version        1.1.0
 */
PRIVATE INT32
_DefragmentLogGroups   (STLZoneObj     *pstZone,
                        StlDfrgArg     *pstDfrg)
{
    /* get device info pointer*/
    const UINT32        nMaxPMT         = ((pstZone->pstZI->nNumUserBlks - 1) >> pstZone->pstRI->nNShift) + 1;
          STLLogGrpHdl *pstLogGrp;
          POFFSET       nMPOff;
          BADDR         nDgn;
          BADDR         nFBlkVbn;
          UINT32        nFBlkEC;
          BOOL32        bUseFlk         = FALSE32;
          INT32         nRet            = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s\r\n"), __FSR_FUNC__));

    /* 1. Compact all log groups */
    nFBlkVbn = NULL_VBN;
    nFBlkEC  = (UINT32)(-1);
    for (nDgn = 0; nDgn < nMaxPMT; nDgn++)
    {
#if (OP_STL_DEBUG_CODE == 1)
        /* Find log group.*/
        pstLogGrp = FSR_STL_SearchLogGrp(pstZone->pstActLogGrpList, nDgn);
        FSR_ASSERT(pstLogGrp == NULL);
        pstLogGrp = FSR_STL_SearchLogGrp(pstZone->pstInaLogGrpCache, nDgn);
        FSR_ASSERT(pstLogGrp == NULL);
#endif /* (OP_STL_DEBUG_CODE == 1) */
        pstLogGrp = NULL;
        nRet = FSR_STL_SearchPMTDir(pstZone, nDgn, &nMPOff);
        if (nMPOff != NULL_POFFSET)
        {
            /* load PMT*/
            nRet = FSR_STL_LoadPMT(pstZone, nDgn, nMPOff, &pstLogGrp, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }
        }

        REPORT_PROGRESS(pstDfrg);

        if ((pstLogGrp != NULL) &&
            (pstLogGrp->pstFm->nNumLogs > 0))
        {
            FSR_ASSERT(pstLogGrp->pstFm->nDgn == nDgn);

            if (nFBlkVbn == NULL_VBN)
            {
                FSR_ASSERT(nFBlkEC  == (UINT32)(-1));

                /* get free block from free block list*/
                nRet = FSR_STL_GetFreeBlk(pstZone, &nFBlkVbn, &nFBlkEC);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    break;
                }
            }

            nRet = _CopyLogicalUnit(pstZone, pstLogGrp, nFBlkVbn, TRUE32, &bUseFlk);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }
            if (bUseFlk == TRUE32)
            {
                nFBlkVbn = NULL_VBN;
                nFBlkEC  = (UINT32)(-1);
            }
        }
        else
        {
            nRet = FSR_STL_SUCCESS;
        }
    }

    /* Restore free blk if the free blk is not uesd */
    while ((bUseFlk  == FALSE32 ) &&
           (nFBlkVbn != NULL_VBN))
    {
        /* process the garbage blocks if required */
        nRet = FSR_STL_ReverseGC(pstZone, 1);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        /* Reserve meta page */
        nRet = FSR_STL_ReserveMetaPgs(pstZone, 1, FALSE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        FSR_STL_AddFreeList(pstZone, nFBlkVbn, 0);
        nFBlkVbn = NULL_VBN;

        /* Store the BMT for saving context */
        nRet = FSR_STL_StoreBMTCtx(pstZone, FALSE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}

/**
 *  @brief          Prepare to defragment the given zone
 *
 *  @param[in]      pstZone     : Zone object
 *
 *  @return         FSR_STL_SUCCESS
 *  @author         Jeong-Uk Kang
 *  @version        1.1.0
 */
PRIVATE INT32
_DefragmentZone        (STLZoneObj     *pstZone,
                        StlDfrgArg     *pstDfrg)
{
    STLLogGrpList  *pstLogGrpList;
    STLLogGrpHdl   *pstLogGrp;
    STLLogGrpFm    *pstLogGrpFm;
    STLLog         *pstLogList;
    STLLog         *pstLog;
    UINT8           nLogIdx;
    INT32           nRet;
#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
            STLBUCtxObj        *pstBUCtx        = pstZone->pstBUCtxObj;
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */
#if (OP_STL_DEBUG_CODE == 1)
            BOOL32              bFound;
#endif
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s\r\n"), __FSR_FUNC__));

    do
    {

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
        /* write the previous misaligned sectors into log block */
        if (pstBUCtx->nBufState != STL_BUFSTATE_CLEAN)
        {
            /* flush currently buffered single page into log block */
            nRet = FSR_STL_FlushBufferPage(pstZone);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }
        }
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */

        REPORT_PROGRESS(pstDfrg);

#if (OP_SUPPORT_PAGE_DELETE == 1)
        /*
         * store previous deleted page info not to lose current deleted information
         * which exists in volatile memory only.
         */
        nRet = FSR_STL_StoreDeletedInfo(pstZone);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }
#endif  /* (OP_SUPPORT_PAGE_DELETE == 1) */

        REPORT_PROGRESS(pstDfrg);

        /* 1. Flush active log list */
        pstLogGrpList = pstZone->pstActLogGrpList;
        while (pstLogGrpList->nNumLogGrps > 0)
        {
            /* Reserve meta page */
            nRet = FSR_STL_ReserveMetaPgs(pstZone, 1, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }

#if (OP_STL_DEBUG_CODE == 1)
            bFound      = FALSE32;
#endif
            pstLogGrp   = pstLogGrpList->pstTail;
            FSR_ASSERT(pstLogGrp != NULL);

            pstLogGrpFm = pstLogGrp->pstFm;
            pstLogList  = pstLogGrp->pstLogList;
            nLogIdx     = pstLogGrpFm->nTailIdx;

#if (OP_SUPPORT_RUNTIME_PMT_BUILDING == 1)
            /* Execute Run time scanning */
            nRet = FSR_STL_ScanActLogBlock(pstZone,pstLogGrp->pstFm->nDgn);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR, 
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
                return nRet;
            }
#endif /*OP_SUPPORT_RUNTIME_PMT_BUILDING*/
            while (nLogIdx != NULL_LOGIDX)
            {
                FSR_ASSERT(nLogIdx < pstZone->pstRI->nK);
                pstLog = pstLogList + nLogIdx;

                /* we change active log to inactive log */
                if (pstLog->nState & (1 << LS_ACTIVE_SHIFT))
                {
                    FSR_STL_RemoveActLogList(pstZone, pstLogGrpFm->nDgn, pstLog);
#if (OP_STL_DEBUG_CODE == 1)
                    bFound = TRUE32;
#endif
                }
                nLogIdx = pstLogList[nLogIdx].nPrevIdx;
            }
#if (OP_STL_DEBUG_CODE == 1)
            FSR_ASSERT(bFound == TRUE32);
#endif

            /* store PMT+CXT, update PMTDir*/
            nRet = FSR_STL_StorePMTCtx(pstZone, pstLogGrp, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }

            FSR_STL_RemoveLogGrp(pstZone, pstLogGrpList, pstLogGrp);

            REPORT_PROGRESS(pstDfrg);
        }

        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }

        /* 2. Flush inctive log list */
        pstLogGrpList = pstZone->pstInaLogGrpCache;
        while (pstLogGrpList->nNumLogGrps > 0)
        {
            pstLogGrp   = pstLogGrpList->pstTail;
            FSR_ASSERT(pstLogGrp != NULL);

#if (OP_STL_DEBUG_CODE == 1)
            bFound      = FALSE32;
            pstLogGrpFm = pstLogGrp->pstFm;
            pstLogList  = pstLogGrp->pstLogList;
            nLogIdx     = pstLogGrpFm->nTailIdx;
            while (nLogIdx != NULL_LOGIDX)
            {
                FSR_ASSERT(nLogIdx < pstZone->pstRI->nK);
                pstLog = pstLogList + nLogIdx;

                /* we change active log to inactive log */
                if (pstLog->nState & (1 << LS_ACTIVE_SHIFT))
                {
                    FSR_STL_RemoveActLogList(pstZone, pstLogGrpFm->nDgn, pstLog);
                    bFound = TRUE32;
                }

                nLogIdx = pstLogList[nLogIdx].nPrevIdx;
            }
            FSR_ASSERT(bFound == TRUE32);
#endif /* (OP_STL_DEBUG_CODE == 1) */
            FSR_STL_RemoveLogGrp(pstZone, pstLogGrpList, pstLogGrp);
        }

        REPORT_PROGRESS(pstDfrg);

        /* 3. Compact all log group */
        nRet = _DefragmentLogGroups(pstZone, pstDfrg);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}

/**
 *  @brief        This function reset erase count of meta, idle, and free blk except log blk
 *
 *  @param[in]    pstClst   : cluster object
 *
 *  @return       FSR_STL_SUCCESS
 *  @author       Jeong-Uk Kang
 *  @version      1.1.0
 */
PRIVATE INT32
_ResetEraseCount       (StlDfrgArg     *pstDfrg)
{
    STLClstObj     *pstClst     = pstDfrg->pstClst;
    STLRootInfo    *pstRI       = &(pstClst->stRootInfoBuf.stRootInfo);
    const   UINT32  nMaxZone    = pstRI->nNumZone;
    STLZoneObj     *pstZone;
    STLZoneInfo    *pstZI;
    STLCtxInfoHdl  *pstCtx;
    STLCtxInfoFm   *pstCtxFm;
    STLDirHdrHdl   *pstDH;
    STLBMTHdl      *pstBMT;
    UINT32          nZone;
    UINT32          nMaxFBlks;
    UINT32          nNumFBlks;
    BADDR           nLan;
    UINT32          nNumDBlks;
    UINT32          nVun;
    UINT32          nIdx;
    INT32           nRet        = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    for (nIdx = 0; nIdx < MAX_ROOT_BLKS; nIdx++)
    {
        pstRI->aRootBlkEC[nIdx] = 1;
    }

    /* Set free and idle blk list */
    for (nZone = 0; nZone < nMaxZone; nZone++)
    {
        pstZone  = pstClst->stZoneObj + nZone;
        pstZI    = pstZone->pstZI;
        pstCtx   = pstZone->pstCtxHdl;
        pstCtxFm = pstCtx->pstFm;
        pstDH    = pstZone->pstDirHdrHdl;

        /* reset erase count of meta unit */
        for (nIdx = 0; nIdx < pstZI->nNumMetaBlks; nIdx++)
        {
            pstDH->pMetaBlksEC[nIdx] = 1;
        }

        nMaxFBlks = pstZone->pstML->nMaxFreeSlots;
        nNumFBlks = pstCtxFm->nNumFBlks;
        for (nIdx = 0; nIdx < nMaxFBlks; nIdx++)
        {
            nVun = pstCtxFm->nFreeListHead + nIdx;
            if (nVun >= nMaxFBlks)
            {
                nVun -= nMaxFBlks;
            }
            FSR_ASSERT(nVun < nMaxFBlks);

            if (nIdx < nNumFBlks)
            {
                FSR_ASSERT(pstCtx->pFreeList[nVun] != NULL_VBN);
                pstCtx->pFBlksEC [nVun] = 0;
            }
            else
            {
                FSR_ASSERT(pstCtx->pFreeList[nVun] == NULL_VBN);
                pstCtx->pFBlksEC [nVun] = (UINT32)(-1);
            }
        }

        pstCtxFm->nBBlkEC   = 0;
        pstCtxFm->nTBlkEC   = 0;

        /* reset idle blk */
        for (nLan = 0; nLan < pstZI->nNumLA; nLan++)
        {
            pstCtx->pMinEC[nLan]    = 0;
            pstCtx->pMinECIdx[nLan] = 0;
        }
        pstCtxFm->nMinECLan = 0;

        for (nLan = 0; nLan < pstZI->nNumLA; nLan++)
        {
            /* Reserve meta page */
            nRet = FSR_STL_ReserveMetaPgs(pstZone, 1, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }

            nRet = FSR_STL_LoadBMT(pstZone, nLan, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  --%s() L(%d) : 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }

            pstBMT = pstZone->pstBMTHdl;

            if (nLan ==(pstZI->nNumLA - 1))
            {
                /* the last LA */
                FSR_ASSERT(pstZI->nNumUserBlks > 0);
                nNumDBlks = ((pstZI->nNumUserBlks - 1) & (pstZI->nDBlksPerLA - 1)) + 1;
            }
            else
            {
                nNumDBlks = pstZI->nDBlksPerLA;
            }

            for (nIdx = 0; nIdx < nNumDBlks; nIdx++)
            {
                if ((pstBMT->pGBlkFlags[nIdx >> 3] & (1 << (nIdx & 0x07))) != 0)
                {
                    FSR_ASSERT(pstBMT->pMapTbl[nIdx].nVbn != NULL_VBN);
                    pstBMT->pDBlksEC[nIdx] = 0;
                }
                else
                {
                    FSR_ASSERT(pstBMT->pMapTbl[nIdx].nVbn == NULL_VBN);
                    pstBMT->pDBlksEC[nIdx] = (UINT32)(-1);
                }
            }

            if (pstBMT->pMapTbl[0].nVbn == NULL_VBN)
            {
                pstCtx->pMinEC[nLan]    = (UINT32)(-1);
                pstCtx->pMinECIdx[nLan] = (UINT16)(-1);
            }

            nRet = FSR_STL_StoreBMTCtx(pstZone, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  --%s() L(%d) : 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }

            REPORT_PROGRESS(pstDfrg);
        }
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}

/**
 * @brief           This function copies all valid meta pages
 *
 * @param[in]       pstZone             : pointer to stl Zone object
 * @param[in]       nSrcVbn             : VUN of source
 * @param[in]       nDstVbn             : VUN of destination
 *
 *  @return         FSR_STL_SUCCESS
 *  @author         Jeong-Uk Kang
 *  @version        1.1.0
 */
PRIVATE INT32
_CopyMetaUnit          (STLZoneObj     *pstZone,
                        BADDR           nSrcVbn,
                        BADDR           nDstVbn)
{
            STLZoneInfo    *pstZI           = pstZone->pstZI;
    const   RBWDevInfo     *pstDVI          = pstZone->pstDevInfo;
            UINT32          nPgsPerSBlk     = pstDVI->nPagesPerSBlk;
    const   UINT32          nPgsMsk         = pstDVI->nPagesPerSBlk - 1;
            STLDirHdrHdl   *pstDH           = pstZone->pstDirHdrHdl;
            STLDirHeaderFm *pstDHFm         = pstDH->pstFm;
            UINT32          nSrcIdx;
            POFFSET         nMetaPOffset    = NULL_POFFSET;
            UINT32          nLoopIdx;
            UINT8          *aValidPgs       = pstZone->pTempMetaPgBuf;
            UINT32          nPOff           = (UINT32)(-1);
            UINT32          nRemainPgs;
            UINT32          i;
            INT32           nRet;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    FSR_ASSERT(nSrcVbn != nDstVbn);

    /* 1. find Meta indexes of source and destination VBN */
    nSrcIdx = (UINT32)(-1);
    for (nLoopIdx = 0; nLoopIdx < pstZI->nNumMetaBlks; nLoopIdx++)
    {
        if (pstZI->aMetaVbnList[nLoopIdx] == nSrcVbn)
        {
            FSR_ASSERT(nSrcIdx == (UINT32)(-1))
            nSrcIdx = nLoopIdx;
#if (OP_STL_DEBUG_CODE == 0)
            break;
#endif
        }
    }
    FSR_ASSERT(nSrcIdx < pstZI->nNumMetaBlks);

    /* Meta uses only LSB pages */
    if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
    {
        nPgsPerSBlk >>= 1;
    }

    /* Get the number of valid pages in source VBN */
    nRemainPgs = pstDH->pValidPgCnt[nSrcIdx];
    FSR_ASSERT(nRemainPgs <= nPgsPerSBlk);

    if (nRemainPgs > 0)
    {
        nRemainPgs += pstZI->nNumDirHdrPgs;

        /* 3. Copy all valid pages in source VBN to destinatio VBN */

        nRet = FSR_STL_FlashErase(pstZone, nDstVbn);
        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return nRet;
        }
        /* 1) Initialize aValidPgs...
         * Because MT_BMT and MT_PMT are 0x04 and ox08, respectively.
         * we initialize this memory buffer to 0x00 which means UNMAPPED
         */
        FSR_OAM_MEMSET(aValidPgs, 0x00, pstDVI->nPagesPerSBlk);         /* sizeof (UINT8)  */

        /* Directory Header checkup */
        for (nLoopIdx = 0; nLoopIdx < pstZI->nNumDirHdrPgs; nLoopIdx++)
        {
            nPOff = nLoopIdx;
            FSR_ASSERT(aValidPgs[nPOff] == 0);
            aValidPgs[nPOff] = MT_HEADER;
            nRemainPgs--;
        }

        /* 3) BMT checkup : check if the nCxtVpn is valid or not */
        for (nLoopIdx = 0; nLoopIdx < pstZI->nNumLA; nLoopIdx++)
        {
            if (nRemainPgs == 0)
            {
                break;
            }

            nMetaPOffset = pstDH->pBMTDir[nLoopIdx];
            if ((UINT32)((nMetaPOffset >> pstDVI->nPagesPerSbShift)) == nSrcIdx)
            {
                nPOff = nMetaPOffset & nPgsMsk;
                FSR_ASSERT(aValidPgs[nPOff] == 0);
                aValidPgs[nPOff] = MT_BMT;
                nRemainPgs--;
            }
        }

        /* 4) PMT checkup : check if the nCxtVpn is valid or not */
        for (nLoopIdx = 0; nLoopIdx < pstZI->nMaxPMTDirEntry; nLoopIdx += pstZI->nNumLogGrpPerPMT)
        {
            if (nRemainPgs == 0)
            {
                break;
            }

            for (i = 0; i < pstZI->nNumLogGrpPerPMT; i++)
            {
                nMetaPOffset = pstDH->pstPMTDir[nLoopIdx + i].nPOffs;
                if ((UINT32)((nMetaPOffset >> pstDVI->nPagesPerSbShift)) == nSrcIdx)
                {
                    nPOff = nMetaPOffset & nPgsMsk;
                    FSR_ASSERT(aValidPgs[nPOff] == 0);
                    aValidPgs[nPOff] = MT_PMT;
                    nRemainPgs--;
                    break;
                }
            }
        }
        FSR_ASSERT(nRemainPgs == 0);

        /* 4) Copy valid pages */
        nRemainPgs = pstDH->pValidPgCnt[nSrcIdx] + pstZI->nNumDirHdrPgs;
        for (nLoopIdx = 0; nLoopIdx < pstDVI->nPagesPerSBlk; nLoopIdx++)
        {
            if (nRemainPgs == 0)
            {
                break;
            }

            if (aValidPgs[nLoopIdx] != 0)
            {
                nRet = FSR_STL_FlashCopyback(pstZone,
                            (nSrcVbn << pstDVI->nPagesPerSbShift) + nLoopIdx,
                            (nDstVbn << pstDVI->nPagesPerSbShift) + nLoopIdx,
                            pstDVI->nFullSBitmapPerVPg);
                if (nRet != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR]  --%s() L(%d) : 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));

                    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    return nRet;
                }
                FSR_STL_FlashFlush(pstZone);
            }
        }
    }
    else
    {
        for (nLoopIdx = 0; nLoopIdx < pstDHFm->nNumOfIdleBlk; nLoopIdx++)
        {
            i = pstDHFm->nIdleBoListHead + nLoopIdx;
            if (i >= pstZI->nNumMetaBlks)
            {
                i -= pstZI->nNumMetaBlks;
            }
            FSR_ASSERT(i < pstZI->nNumMetaBlks);

            if (pstDH->pIdleBoList[i] == nSrcIdx)
            {
                break;
            }
        }

        if (nLoopIdx == pstDHFm->nNumOfIdleBlk)
        {
            i = (pstDHFm->nIdleBoListHead + pstDHFm->nNumOfIdleBlk);
            if (i >= pstZI->nNumMetaBlks)
            {
                i -= pstZI->nNumMetaBlks;
            }
            FSR_ASSERT(i < pstZI->nNumMetaBlks);

            /*  add idle block to list */
            pstDH->pIdleBoList[i] = (UINT16)(nSrcIdx);
            pstDHFm->nNumOfIdleBlk++;
        }
    }

    pstZI->aMetaVbnList[nSrcIdx] = nDstVbn;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, FSR_STL_SUCCESS));
    return FSR_STL_SUCCESS;
}


/**
 *  @brief        This function copies used unit
 *
 *  @param[in]    pstZone   : zone object
 *  @param[in]    pstSrcVU  : The Virtual Unit (VU) status of source
 *  @param[in]    pstDstVU  : The Virtual Unit (VU) status of destination
 *
 *  @return       FSR_STL_SUCCESS
 *  @author       Jeong-Uk Kang
 *  @version      1.1.0
 */
PRIVATE INT32
_CopyUsedUnit          (STLClstObj     *pstClst,
                        const BADDR     nSrcVun,
                        const BADDR     nDstVun,
                        StlVUInfo      *pstaVUs)
{
    STLZoneObj     *pstZone;
    StlVUInfo      *pstSrcVU = pstaVUs + nSrcVun;
    StlVUInfo      *pstDstVU = pstaVUs + nDstVun;
    UINT32          nZone;
    BADDR           nDgn;
    BOOL32          bUseTrg;
    INT32           nRet;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));
    FSR_ASSERT((pstSrcVU->nType == UNIT_TYPE_META) ||
               (pstSrcVU->nType == UNIT_TYPE_USER));
    FSR_ASSERT((pstDstVU->nType == UNIT_TYPE_FREE) &&
               (pstDstVU->nDgn  == NULL_DGN));

    do
    {
        nZone   = pstSrcVU->nZoneID;;
        FSR_ASSERT(nZone < pstClst->stRootInfoBuf.stRootInfo.nNumZone);
        pstZone = pstClst->stZoneObj + nZone;
        if (pstSrcVU->nType == UNIT_TYPE_META)
        {
            nDgn = NULL_DGN;
            nRet = _CopyMetaUnit(pstZone, nSrcVun, nDstVun);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }

            pstDstVU->nType   = UNIT_TYPE_META;
        }
        else if (pstSrcVU->nType == UNIT_TYPE_USER)
        {
            nDgn = pstSrcVU->nDgn;
            FSR_ASSERT(nDgn <= (pstZone->pstZI->nNumUserBlks >> pstZone->pstRI->nNShift));
            nRet = _CopyLogGroup(pstZone, nDgn, nDstVun, &bUseTrg);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }
            FSR_ASSERT(bUseTrg == TRUE32);

            pstDstVU->nType   = UNIT_TYPE_USER;
        }
        else
        {
            FSR_ASSERT(0);
        }

        pstSrcVU->nType   = UNIT_TYPE_FREE;
        pstSrcVU->nDgn    = NULL_DGN;
        pstSrcVU->nZoneID = pstDstVU->nZoneID;

        pstDstVU->nDgn    = nDgn;
        pstDstVU->nZoneID = (UINT8)nZone;

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}

/**
 *  @brief        This function rearranges all log groups
 *
 *  @param[in]    pstZone   : zone object
 *  @param[in]    pstZVU    : The Virtual Unit (VU) information of the given zone
 *  @param[in]    pstaVUs   : The Virtual Unit (VU) status of the given cluster
 *
 *  @return       FSR_STL_SUCCESS
 *  @author       Jeong-Uk Kang
 *  @version      1.1.0
 */
PRIVATE INT32
_RearrangeLogGroups    (STLClstObj     *pstClst,
                        const UINT32    nTrgZone,
                        StlDfrgArg     *pstDfrg)
{
    /* get device info pointer*/
    const   UINT32          nMaxBlk         = pstDfrg->nMaxBlk;
            StlZoneVU      *pstaZVUs        = pstDfrg->pstaZVUs;
            StlVUInfo      *pstaVUs         = pstDfrg->pstaVUs;
            STLZoneObj     *pstZone         = pstClst->stZoneObj + nTrgZone;
    const   UINT32          nMaxPMT         = ((pstZone->pstZI->nNumUserBlks - 1) >> pstZone->pstRI->nNShift) + 1;
            StlZoneVU      *pstZVU          = pstaZVUs + pstZone->nZoneID;
            STLLogGrpHdl   *pstLogGrp;
            STLLogGrpFm    *pstLogGrpFm;
            STLLog         *pstLogList;
            STLLog         *pstLog;
            UINT8           nLogIdx;
            POFFSET         nMPOff;
            BADDR           nDgn;
            BADDR           nTrgVbn;
            BADDR           nFBlkVbn;
            BADDR           nLogVbn;
            BOOL32          bUseTrg;
            INT32           nRet            = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s\r\n"), __FSR_FUNC__));

    /* 1. Rearrange all log groups */
    nTrgVbn  = pstZVU->nStartVbn + pstZVU->nNumMeta;
    nFBlkVbn = (BADDR)(nMaxBlk - 1);
    for (nDgn = 0; nDgn < nMaxPMT; nDgn++)
    {
        /* Find free unit */
        while (nFBlkVbn >= MAX_ROOT_BLKS)
        {
            if (pstaVUs[nFBlkVbn].nType == UNIT_TYPE_FREE)
            {
                FSR_ASSERT(pstaVUs[nFBlkVbn].nDgn == NULL_DGN);
                break;
            }
            nFBlkVbn--;
        }
        FSR_ASSERT(pstaVUs[nFBlkVbn].nType == UNIT_TYPE_FREE);

        /* Copy valid pages in order to make free unit */
        if (pstaVUs[nTrgVbn].nType != UNIT_TYPE_FREE)
        {
            nRet = _CopyUsedUnit(pstClst, nTrgVbn, nFBlkVbn, pstaVUs);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                return nRet;
            }

            REPORT_PROGRESS(pstDfrg);
        }
        FSR_ASSERT(pstaVUs[nTrgVbn].nType == UNIT_TYPE_FREE);
        FSR_ASSERT(pstaVUs[nTrgVbn].nDgn  == NULL_DGN);

        /* Find log group.*/
#if (OP_STL_DEBUG_CODE == 1)
        pstLogGrp = FSR_STL_SearchLogGrp(pstZone->pstActLogGrpList, nDgn);
        FSR_ASSERT(pstLogGrp == NULL);
        pstLogGrp = FSR_STL_SearchLogGrp(pstZone->pstInaLogGrpCache, nDgn);
        FSR_ASSERT(pstLogGrp == NULL);
#endif /* (OP_STL_DEBUG_CODE == 1) */
        pstLogGrp = NULL;
        nRet = FSR_STL_SearchPMTDir(pstZone, nDgn, &nMPOff);
        if (nMPOff != NULL_POFFSET)
        {
            /* load PMT*/
            nRet = FSR_STL_LoadPMT(pstZone, nDgn, nMPOff, &pstLogGrp, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }

            REPORT_PROGRESS(pstDfrg);
        }

        if (pstLogGrp != NULL)
        {
            if (pstLogGrp->pstFm->nNumLogs > 0)
            {
                /* Reset erase count of all VU to 1 */
                FSR_ASSERT(pstLogGrp->pstFm->nDgn     == nDgn);
                FSR_ASSERT(pstLogGrp->pstFm->nNumLogs == 1   );

                pstLogGrpFm = pstLogGrp->pstFm;
                pstLogList  = pstLogGrp->pstLogList;
                nLogIdx     = pstLogGrpFm->nTailIdx;
                FSR_ASSERT(pstLogGrpFm->nTailIdx == pstLogGrpFm->nHeadIdx);
                nLogVbn = NULL_VBN;
                while (nLogIdx != NULL_LOGIDX)
                {
                    FSR_ASSERT(nLogIdx < pstZone->pstRI->nK);
                    pstLog = pstLogList + nLogIdx;

                    pstLog->nEC = 1;

                    FSR_ASSERT(nLogVbn == NULL_VBN);
                    nLogVbn = pstLog->nVbn;
                    if (pstLog->nVbn > nFBlkVbn)
                    {
                        nFBlkVbn = pstLog->nVbn;
                    }

                    nLogIdx = pstLogList[nLogIdx].nPrevIdx;
                    FSR_ASSERT(nLogIdx == NULL_LOGIDX);
                }
                FSR_ASSERT(nLogVbn != NULL_VBN);

                FSR_ASSERT(pstaVUs[nTrgVbn].nType == UNIT_TYPE_FREE);
                nRet = _CopyLogicalUnit(pstZone, pstLogGrp, nTrgVbn, FALSE32, &bUseTrg);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    break;
                }
                FSR_ASSERT(bUseTrg == TRUE32);

                pstaVUs[nLogVbn].nType   = pstaVUs[nTrgVbn].nType;
                pstaVUs[nLogVbn].nDgn    = pstaVUs[nTrgVbn].nDgn;
                pstaVUs[nLogVbn].nZoneID = pstaVUs[nTrgVbn].nZoneID;

                pstaVUs[nTrgVbn].nType   = UNIT_TYPE_USER;
                pstaVUs[nTrgVbn].nDgn    = nDgn;
                pstaVUs[nTrgVbn].nZoneID = (UINT8)nTrgZone;

                nTrgVbn++;
            }
            else
            {
                /* Reserve meta page */
                nRet = FSR_STL_ReserveMetaPgs(pstZone, 1, FALSE32);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    break;
                }

                /* Store the PMT */
                nRet = FSR_STL_StorePMTCtx(pstZone, pstLogGrp, FALSE32);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    break;
                }
            }

            REPORT_PROGRESS(pstDfrg);
        }
        else
        {
            nRet = FSR_STL_SUCCESS;
        }
    }
    if (nRet == FSR_STL_SUCCESS)
    {
        pstZVU->nNumUser = (BADDR)(nTrgVbn - pstZVU->nNumMeta - pstZVU->nStartVbn);
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}

/**
 *  @brief        This function rearrange virtual units
 *
 *  @param[in]    pstZone   : zone object
 *  @param[in]    pstZVU    : The Virtual Unit (VU) information of the given zone
 *  @param[in]    pstaVUs   : The Virtual Unit (VU) status of the given clsuter
 *
 *  @return       FSR_STL_SUCCESS
 *  @author       Jeong-Uk Kang
 *  @version      1.1.0
 */
PRIVATE INT32
_RearrangeVUs          (STLClstObj     *pstClst,
                        StlDfrgArg     *pstDfrg)
{
    STLRootInfo    *pstRI       = &(pstClst->stRootInfoBuf.stRootInfo);
    const UINT32    nMaxBlks    = pstDfrg->nMaxBlk;
    StlZoneVU      *pstaZVUs    = pstDfrg->pstaZVUs;
    StlVUInfo      *pstaVUs     = pstDfrg->pstaVUs;
    STLZoneObj     *pstZone;
    STLZoneInfo    *pstZI;
    BADDR           nLstFBlk;
    BADDR           n1stUBlk;
    UINT32          nZone;
    BADDR           nVun;
    UINT32          nBlk;
    INT32           nRet        = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

#if (OP_STL_DEBUG_CODE == 1)
    for (nVun = 0; nVun < MAX_ROOT_BLKS; nVun++)
    {
        FSR_ASSERT(pstaVUs[nVun].nType   == UNIT_TYPE_ROOT);
        FSR_ASSERT(pstaVUs[nVun].nDgn    == NULL_DGN);
        FSR_ASSERT(pstaVUs[nVun].nZoneID == (UINT8)(-1));
    }
#endif

    for (nZone = 0; nZone < pstRI->nNumZone; nZone++)
    {
        n1stUBlk = pstaZVUs[nZone].nStartVbn;
        pstZone  = pstClst->stZoneObj + nZone;
        pstZI    = pstZone->pstZI;
        nLstFBlk = (BADDR)(nMaxBlks - 1);

        /* Copy all valid meta page */
        for (nBlk = 0; nBlk < pstZI->nNumMetaBlks; nBlk++)
        {
            /* Find free unit */
            while (nLstFBlk >= MAX_ROOT_BLKS)
            {
                if (pstaVUs[nLstFBlk].nType == UNIT_TYPE_FREE)
                {
                    FSR_ASSERT(pstaVUs[nLstFBlk].nDgn == NULL_DGN);
                    break;
                }
                nLstFBlk--;
            }
            FSR_ASSERT(pstaVUs[nLstFBlk].nType == UNIT_TYPE_FREE);

            /* Copy valid pages in order to make free unit */
            if (pstaVUs[n1stUBlk].nType != UNIT_TYPE_FREE)
            {
                nRet = _CopyUsedUnit(pstClst, n1stUBlk, nLstFBlk, pstaVUs);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    break;
                }

                REPORT_PROGRESS(pstDfrg);
            }
            FSR_ASSERT(pstaVUs[n1stUBlk].nType == UNIT_TYPE_FREE);
            FSR_ASSERT(pstaVUs[n1stUBlk].nDgn  == NULL_DGN);

            nVun = pstZI->aMetaVbnList[nBlk];
            nRet = _CopyUsedUnit(pstClst, nVun, n1stUBlk, pstaVUs);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }

            REPORT_PROGRESS(pstDfrg);

            if (nVun > nLstFBlk)
            {
                nLstFBlk = nVun;
            }

            n1stUBlk++;
        }
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }

        /* Copy all valid user page */
        nRet = _RearrangeLogGroups(pstClst, nZone, pstDfrg);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }
    }
    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }

    /* rearrange free units to each zone area */
    for (nZone = 0; nZone < pstRI->nNumZone; nZone++)
    {
        nVun     = pstaZVUs[nZone].nStartVbn
                 + pstaZVUs[nZone].nNumMeta
                 + pstaZVUs[nZone].nNumUser;
        nBlk     = pstaZVUs[nZone].nStartVbn
                 + pstaZVUs[nZone].nNumVbn;
        nLstFBlk = (BADDR)(nMaxBlks - 1);
        while (nVun < nBlk)
        {
            /* find the free blk which belong to the other zone */
            while (nVun < nBlk)
            {
                FSR_ASSERT(pstaVUs[nVun].nType == UNIT_TYPE_FREE);
                FSR_ASSERT(pstaVUs[nVun].nDgn  == NULL_DGN);
                if (pstaVUs[nVun].nZoneID != nZone)
                {
                    break;
                }
                nVun++;
            }
            if (nVun >= nBlk)
            {
                break;
            }

            /* Find the free blk in the other zone area */
            while (nLstFBlk >= nBlk)
            {
                if (pstaVUs[nLstFBlk].nZoneID == nZone)
                {
                    FSR_ASSERT(pstaVUs[nLstFBlk].nType == UNIT_TYPE_FREE);
                    FSR_ASSERT(pstaVUs[nLstFBlk].nDgn  == NULL_DGN);
                    break;
                }
                nLstFBlk--;
            }
            FSR_ASSERT(nLstFBlk >= nBlk);

            /* exchange free blk */
            pstaVUs[nLstFBlk].nZoneID = pstaVUs[nVun].nZoneID;
            pstaVUs[nVun    ].nZoneID = (UINT8)nZone;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}

/**
 *  @brief        This function allocate new blk to idle and free blk and
 *  @n            reset erase count of meta, idle, and free blk except log blk
 *
 *  @param[in]    pstClst   : cluster object
 *
 *  @return       FSR_STL_SUCCESS
 *  @author       Jeong-Uk Kang
 *  @version      1.1.0
 */
PRIVATE INT32
_ResetFreeUnits        (STLClstObj     *pstClst,
                        StlDfrgArg     *pstDfrg)
{
    STLRootInfo    *pstRI       = &(pstClst->stRootInfoBuf.stRootInfo);
    const   UINT32  nMaxZone    = pstRI->nNumZone;
    StlZoneVU      *pstaZVUs    = pstDfrg->pstaZVUs;
    StlVUInfo      *pstaVUs     = pstDfrg->pstaVUs;
    STLZoneObj     *pstZone;
    STLZoneInfo    *pstZI;
    STLCtxInfoHdl  *pstCtx;
    STLCtxInfoFm   *pstCtxFm;
    STLDirHdrHdl   *pstDH;
    STLBMTHdl      *pstBMT;
    UINT32          nZone;
    UINT32          nMaxFBlks;
    BADDR           nLan;
    UINT32          nNumDBlks;
    BADDR           nVbn;
    BADDR           nEndVbn;
    UINT32          nIdx;
    INT32           nRet        = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    do
    {
#if (OP_STL_DEBUG_CODE == 1)
        /* Check VUs info and status */
        for (nIdx = 0; nIdx < MAX_ROOT_BLKS; nIdx++)
        {
            FSR_ASSERT(pstaVUs[nIdx].nType == UNIT_TYPE_ROOT);
            FSR_ASSERT(pstaVUs[nIdx].nDgn  == NULL_DGN);
        }

        nVbn = MAX_ROOT_BLKS;
        for (nZone = 0; nZone < nMaxZone; nZone++)
        {
            FSR_ASSERT(pstaZVUs[nZone].nStartVbn == nVbn);
            pstZone = pstClst->stZoneObj + nZone;
            pstZI   = pstZone->pstZI;

            nVbn = nVbn + pstaZVUs[nZone].nNumMeta;
            for (nIdx = pstaZVUs[nZone].nStartVbn; nIdx < nVbn; nIdx++)
            {
                FSR_ASSERT(pstaVUs[nIdx].nZoneID == nZone);
                FSR_ASSERT(pstaVUs[nIdx].nType   == UNIT_TYPE_META);
                FSR_ASSERT(pstaVUs[nIdx].nDgn    == NULL_DGN);
            }

            nVbn    = nVbn + pstaZVUs[nZone].nNumUser;
            nEndVbn = nVbn + pstaZVUs[nZone].nNumUser;

            for (nIdx = nVbn; nIdx < nEndVbn; nIdx++)
            {
                FSR_ASSERT(pstaVUs[nIdx].nZoneID == nZone);
                FSR_ASSERT(pstaVUs[nIdx].nType   == UNIT_TYPE_USER);
                FSR_ASSERT(pstaVUs[nIdx].nDgn    <= (pstZI->nNumUserBlks >> pstZone->pstRI->nNShift));
            }

            nVbn    = nEndVbn;
            nEndVbn = pstaZVUs[nZone].nStartVbn + pstaZVUs[nZone].nNumVbn;
            for (nIdx = nVbn; nIdx < nEndVbn; nIdx++)
            {
                FSR_ASSERT(pstaVUs[nIdx].nZoneID == nZone);
                FSR_ASSERT(pstaVUs[nIdx].nType   == UNIT_TYPE_FREE);
                FSR_ASSERT(pstaVUs[nIdx].nDgn    == NULL_DGN);
            }

            nVbn = nEndVbn;
        }
#endif

        /* Erase all free blk */
        for (nZone = 0; nZone < nMaxZone; nZone++)
        {
            pstZone = pstClst->stZoneObj + nZone;
            pstZI   = pstZone->pstZI;

            nVbn    = pstaZVUs[nZone].nStartVbn + pstaZVUs[nZone].nNumMeta;
            nEndVbn = nVbn + pstaZVUs[nZone].nNumUser;
            for (nIdx = nVbn; nIdx < nEndVbn; nIdx++)
            {
                FSR_ASSERT(pstaVUs[nIdx].nZoneID == nZone);
                FSR_ASSERT(pstaVUs[nIdx].nType   == UNIT_TYPE_USER);
                FSR_ASSERT(pstaVUs[nIdx].nDgn    <= (pstZI->nNumUserBlks >> pstZone->pstRI->nNShift));
            }

            nVbn    = pstaZVUs[nZone].nStartVbn + pstaZVUs[nZone].nNumMeta + pstaZVUs[nZone].nNumUser;
            nEndVbn = pstaZVUs[nZone].nStartVbn + pstaZVUs[nZone].nNumVbn;
            for (nIdx = nVbn; nIdx < nEndVbn; nIdx++)
            {
                FSR_ASSERT(pstaVUs[nIdx].nZoneID == nZone);
                FSR_ASSERT(pstaVUs[nIdx].nType   == UNIT_TYPE_FREE);
                FSR_ASSERT(pstaVUs[nIdx].nDgn    == NULL_DGN);
                nRet = FSR_STL_FlashErase(pstZone, nIdx);
                if (nRet != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR]  --%s() L(%d) : 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    break;
                }
                nRet = FSR_STL_SUCCESS;

                REPORT_PROGRESS(pstDfrg);
            }
        }
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }

        for (nIdx = 0; nIdx < MAX_ROOT_BLKS; nIdx++)
        {
            pstRI->aRootBlkEC[nIdx] = 1;
        }

        /* Set free and idle blk list */
        for (nZone = 0; nZone < nMaxZone; nZone++)
        {
            pstZone  = pstClst->stZoneObj + nZone;
            pstZI    = pstZone->pstZI;
            pstCtx   = pstZone->pstCtxHdl;
            pstCtxFm = pstCtx->pstFm;
            pstDH    = pstZone->pstDirHdrHdl;

            /* reset erase count of meta unit */
            for (nIdx = 0; nIdx < pstZI->nNumMetaBlks; nIdx++)
            {
                pstDH->pMetaBlksEC[nIdx] = 1;
            }

            nVbn     = pstaZVUs[nZone].nStartVbn
                     + pstaZVUs[nZone].nNumMeta
                     + pstaZVUs[nZone].nNumUser;
            nMaxFBlks = pstZone->pstML->nMaxFreeSlots;
            FSR_ASSERT(pstCtxFm->nNumFBlks == 0);
            for (nIdx = 0; nIdx < nMaxFBlks; nIdx++)
            {
                FSR_ASSERT(pstCtx->pFreeList[nIdx] == NULL_VBN);
                FSR_ASSERT(pstCtx->pFBlksEC [nIdx] == 0);
                if (nIdx < pstZI->nNumFBlks)
                {
                    pstCtx->pFreeList[nIdx] = nVbn;
                    pstCtx->pFBlksEC [nIdx] = 0;
                    nVbn++;
                }
            }
            pstCtxFm->nFreeListHead = 0;
            pstCtxFm->nNumFBlks = pstZI->nNumFBlks;

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
            pstCtxFm->nBBlkVbn  = nVbn;
            pstCtxFm->nBBlkEC   = 0;
            nVbn++;
#endif

            nEndVbn = pstaZVUs[nZone].nStartVbn + pstaZVUs[nZone].nNumVbn;

            FSR_ASSERT(pstCtxFm->nNumIBlks == 0);
            pstCtxFm->nNumIBlks = nEndVbn - nVbn;

            FSR_ASSERT(pstCtxFm->nNumLBlks == 0);

            /* reset idle blk */
            for (nLan = 0; nLan < pstZI->nNumLA; nLan++)
            {
                pstCtx->pMinEC[nLan]    = 0;
                pstCtx->pMinECIdx[nLan] = 0;
            }
            pstCtxFm->nMinECLan = 0;

            for (nLan = 0; nLan < pstZI->nNumLA; nLan++)
            {
                /* Reserve meta page */
                nRet = FSR_STL_ReserveMetaPgs(pstZone, 1, FALSE32);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    break;
                }

                nRet = FSR_STL_LoadBMT(pstZone, nLan, FALSE32);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR]  --%s() L(%d) : 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    break;
                }

                REPORT_PROGRESS(pstDfrg);

                pstBMT = pstZone->pstBMTHdl;

                if (nLan ==(pstZI->nNumLA - 1))
                {
                    /* the last LA */
                    FSR_ASSERT(pstZI->nNumUserBlks > 0);
                    nNumDBlks = ((pstZI->nNumUserBlks - 1) & (pstZI->nDBlksPerLA - 1)) + 1;
                }
                else
                {
                    nNumDBlks = pstZI->nDBlksPerLA;
                }

                for (nIdx = 0; nIdx < nNumDBlks; nIdx++)
                {
                    if (nVbn < nEndVbn)
                    {
                        FSR_ASSERT(pstaVUs[nVbn].nType   == UNIT_TYPE_FREE);
                        FSR_ASSERT(pstaVUs[nVbn].nZoneID == (UINT8)pstZone->nZoneID);
                        FSR_ASSERT(pstaVUs[nVbn].nDgn    == NULL_DGN);

                        pstBMT->pMapTbl[nIdx].nVbn = nVbn;
                        pstBMT->pDBlksEC[nIdx] = 0;
                        pstBMT->pGBlkFlags[nIdx >> 3] |= (1 << (nIdx & 0x07));
                        nVbn++;
                    }
                    else
                    {
                        pstBMT->pMapTbl[nIdx].nVbn = NULL_VBN;
                        pstBMT->pDBlksEC[nIdx] = (UINT32)(-1);
                        pstBMT->pGBlkFlags[nIdx >> 3] &= ~(1 << (nIdx & 0x07));
                    }
                }

                if (pstBMT->pMapTbl[0].nVbn == NULL_VBN)
                {
                    pstCtx->pMinEC[nLan]    = (UINT32)(-1);
                    pstCtx->pMinECIdx[nLan] = (UINT16)(-1);
                }

                nRet = FSR_STL_StoreBMTCtx(pstZone, FALSE32);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR]  --%s() L(%d) : 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    break;
                }

                REPORT_PROGRESS(pstDfrg);
            }
            if (nRet != FSR_STL_SUCCESS)
            {
                break;
            }
            FSR_ASSERT(nVbn == nEndVbn);
        }
    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}

/**
 * @brief           This function processes meta block reclaim to header idx
 *
 * @param[in]       pstZone             : pointer to stl Zone object
 * @param[in]       nVictimIdx          : victim meta block index
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PUBLIC INT32
_ReclaimMeta           (STLZoneObj     *pstZone,
                        UINT32          nVictimIdx)
{
    const   STLZoneInfo    *pstZI           = pstZone->pstZI;
    const   STLMetaLayout  *pstML           = pstZone->pstML;
    const   RBWDevInfo     *pstDVI          = pstZone->pstDevInfo;
            UINT32          nPgsPerSBlk     = pstDVI->nPagesPerSBlk;
            STLCtxInfoHdl  *pstCI           = pstZone->pstCtxHdl;
            STLDirHdrHdl   *pstDH           = pstZone->pstDirHdrHdl;
            STLDirHeaderFm *pstDHFm         = pstDH->pstFm;
            VFLParam       *pstVFLParam     = NULL;
            UINT32          nZBCCtx;
            UINT32          nLoopIdx;
            UINT16          nDstBlkOffset   = pstDHFm->nHeaderIdx;
            BADDR           nVbn;
            BADDR           nVictimVbn      = NULL_VBN;
            PADDR           nCxtVpn;
            PADDR           nBMTVpn         = NULL_VPN;
            UINT32          i;
            POFFSET         nMetaPOffset    = NULL_POFFSET;
            UINT16          nMinPgCnt;
            INT32           nRet;
            UINT8          *aValidPgs       = pstZone->pTempMetaPgBuf;
            UINT16         *naLoopIdx       = (UINT16*)(pstZone->pTempMetaPgBuf + pstDVI->nPagesPerSBlk);
    const   UINT32          nPgsMsk         = pstDVI->nPagesPerSBlk - 1;
            UINT32          nPOff           = (UINT32)(-1);
            UINT32          nRemainPgs;
            UINT32          nPOffIdx        = (UINT32)(-1);
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));


    /**
     * the victim context block is moved to the current context block.
     * page copies : nMinPgCnt 
     */
    nMinPgCnt  = pstDH->pValidPgCnt[nVictimIdx];
    FSR_ASSERT(nMinPgCnt > 0);

    /*  get victim VBN using victim index */
    nVbn       = pstZI->aMetaVbnList[nDstBlkOffset];
    nVictimVbn = pstZI->aMetaVbnList[nVictimIdx];

    pstVFLParam = FSR_STL_AllocVFLParam(pstZone);
    pstVFLParam->bPgSizeBuf = FALSE32;
    pstVFLParam->bUserData  = FALSE32;
    pstVFLParam->bSpare     = TRUE32;
    pstVFLParam->pExtParam  = FSR_STL_AllocVFLExtParam(pstZone);

    /* Meta uses only LSB pages */
    if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
    {
        nPgsPerSBlk >>= 1;
    }

    /* 1. initialize aValidPgs...
     * Because MT_BMT and MT_PMT are 0x04 and ox08, respectively.
     * we initialize this memory buffer to 0x00 which means UNMAPPED
     */
    FSR_ASSERT(nMinPgCnt == pstDH->pValidPgCnt[nVictimIdx]);
    FSR_OAM_MEMSET(aValidPgs, 0x00, pstDVI->nPagesPerSBlk);         /* sizeof (UINT8)  */
    FSR_OAM_MEMSET(naLoopIdx, 0x00, (pstDVI->nPagesPerSBlk) << 1);  /* sizeof (UINT16) */

    /* 2. Find valid pages in victim blk */
    nRemainPgs = nPgsPerSBlk - pstDHFm->nCPOffs;
    if (nRemainPgs > pstDH->pValidPgCnt[nVictimIdx])
    {
        nRemainPgs = pstDH->pValidPgCnt[nVictimIdx];
    }
    else
    {
        nRemainPgs = nPgsPerSBlk - pstDHFm->nCPOffs;
    }
    nMinPgCnt = (UINT16)nRemainPgs;
    FSR_ASSERT((UINT32)(pstDHFm->nCPOffs + nMinPgCnt) <= nPgsPerSBlk);

    /* 1) BMT checkup : check if the nCxtVpn is valid or not */
    for (nLoopIdx = 0; nLoopIdx < pstZI->nNumLA; nLoopIdx++)
    {
        if (nRemainPgs == 0)
        {
            break;
        }

        nMetaPOffset = pstDH->pBMTDir[nLoopIdx];
        if ((UINT32)((nMetaPOffset >> pstDVI->nPagesPerSbShift)) == nVictimIdx)
        {
            nPOff = nMetaPOffset & nPgsMsk;
            aValidPgs[nPOff] = MT_BMT;
            naLoopIdx[nPOff] = (UINT16)nLoopIdx;
            nRemainPgs--;
        }
    }

    /*  2) PMT checkup : check if the nCxtVpn is valid or not */
    for (nLoopIdx = 0; nLoopIdx < pstZI->nMaxPMTDirEntry; nLoopIdx += pstZI->nNumLogGrpPerPMT)
    {
        if (nRemainPgs == 0)
        {
            break;
        }

        for (i = 0; i < pstZI->nNumLogGrpPerPMT; i++)
        {
            nMetaPOffset = pstDH->pstPMTDir[nLoopIdx + i].nPOffs;
            if ((UINT32)((nMetaPOffset >> pstDVI->nPagesPerSbShift)) == nVictimIdx)
            {
                nPOff = nMetaPOffset & nPgsMsk;
                aValidPgs[nPOff] = MT_PMT;
                naLoopIdx[nPOff] = (UINT16)nLoopIdx;
                nRemainPgs--;
                break;
            }
        }
    }
    FSR_ASSERT(nRemainPgs == 0);

    /* Copy valid pages */

    /* 1) Set Context Info */
    /* set meta reclaim flag 
     * PMT of meta reclaim does not sort for preventing violation of LRU base policy.
     */
    pstCI->pstFm->nMetaReclaimFlag = 1;

    /* calculate zero bit count of ctxinfo */
    nZBCCtx = (UINT32)FSR_STL_GetZBC((UINT8*)pstCI->pBuf,
                                      pstCI->nCfmBufSize);

    /* set ctx's zero bit count */
    pstCI->pstCfm->nZBC    = nZBCCtx;
    pstCI->pstCfm->nInvZBC = nZBCCtx ^ 0xFFFFFFFF;

    /* copy source log's pages to destination log */
    nRemainPgs = nMinPgCnt;
    nPOffIdx   = 0;
    while (nRemainPgs != 0)
    {
        if (aValidPgs[nPOffIdx] != 0)
        {
            /* valid context page is copied to new block. */
            if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
            {
                nPOff = FSR_BML_GetVPgOffOfLSBPg(pstZone->nVolID, pstDHFm->nCPOffs);
            }
            else
            {
                nPOff = pstDHFm->nCPOffs;
            }
            nCxtVpn = (nVictimVbn << pstDVI->nPagesPerSbShift) + nPOffIdx;
            nBMTVpn = (nVbn       << pstDVI->nPagesPerSbShift) + nPOff;

            /* set Extended Parameter */
            pstVFLParam->pExtParam->nNumExtSData = 0;
            pstVFLParam->pExtParam->nNumRndIn    = 1;

            if (aValidPgs[nPOffIdx] == MT_BMT)
            {
                pstDH->pBMTDir[naLoopIdx[nPOffIdx]] =
                        (POFFSET)((nDstBlkOffset << pstDVI->nPagesPerSbShift) + nPOff);

                FSR_STL_SetRndInArg(pstVFLParam->pExtParam,
                                    (UINT16)pstML->nBMTBufSize,
                                    (UINT16)pstML->nCtxBufSize,
                                    (VOID*) pstCI->pBuf,
                                    0);
            }
            else if (aValidPgs[nPOffIdx] == MT_PMT)
            {
                /* Update PMT directory */
                nLoopIdx = naLoopIdx[nPOffIdx];
                for (i = 0; i < pstZI->nNumLogGrpPerPMT; i++)
                {
                    nMetaPOffset = pstDH->pstPMTDir[nLoopIdx].nPOffs;
                    if (nMetaPOffset != NULL_POFFSET)
                    {
                        break;
                    }
                    nLoopIdx++;
                }
                FSR_ASSERT(i < pstZI->nNumLogGrpPerPMT);

                /* Update Page Offset of PMTDir */
                for (; i < pstZI->nNumLogGrpPerPMT; i++)
                {
                    if (pstDH->pstPMTDir[nLoopIdx].nPOffs != NULL_POFFSET)
                    {
                        /*  valid context page is copied to new block. */
                        pstDH->pstPMTDir[nLoopIdx].nPOffs =
                            (POFFSET)((nDstBlkOffset << pstDVI->nPagesPerSbShift) + nPOff);
                    }
                    nLoopIdx++;
                }

                FSR_STL_SetRndInArg(pstVFLParam->pExtParam,
                                    0,
                                    (UINT16)pstML->nCtxBufSize,
                                    (VOID*)pstCI->pBuf,
                                    0);
            }

            /* set source vpn & destination vpn for modify-copyback */
            pstVFLParam->pExtParam->nSrcVpn = nCxtVpn;
            pstVFLParam->pExtParam->nDstVpn = nBMTVpn;

            nRet = FSR_STL_FlashModiCopyback(pstZone, 
                         nCxtVpn,
                         nBMTVpn,
                         pstVFLParam);
            if (nRet != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  --%s() L(%d) : 0x%08x - FSR_STL_FlashModiCopyback return error\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));

                FSR_STL_FreeVFLExtParam(pstZone, pstVFLParam->pExtParam);
                FSR_STL_FreeVFLParam(pstZone, pstVFLParam);

                FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                    (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x - FSR_STL_FlashModiCopyback return error\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                return nRet;
            }
            FSR_STL_FlashFlush(pstZone);

            /*  set valid page header index & page index */
            pstDHFm->nLatestBlkIdx     = nDstBlkOffset;
            pstDHFm->nLatestPOffs      = pstDHFm->nCPOffs;

            /*  clean page moves to the next */
            pstDHFm->nCPOffs++;
            pstDH->pValidPgCnt[nDstBlkOffset]++;
            pstDH->pValidPgCnt[nVictimIdx]--;

            nRemainPgs--;
        }

        FSR_ASSERT(nPOffIdx < pstDVI->nPagesPerSBlk);
        nPOffIdx++;
    }

    FSR_STL_FreeVFLExtParam(pstZone, pstVFLParam->pExtParam);
    FSR_STL_FreeVFLParam(pstZone, pstVFLParam);

    if (pstDH->pValidPgCnt[nVictimIdx] == 0)
    {
        i = (pstDHFm->nIdleBoListHead + pstDHFm->nNumOfIdleBlk);
        if (i >= pstZI->nNumMetaBlks)
        {
            i -= pstZI->nNumMetaBlks;
        }
        FSR_ASSERT(i < pstZI->nNumMetaBlks);

        /*  add idle block to list */
        pstDH->pIdleBoList[i] = (UINT16)(nVictimIdx);
        pstDHFm->nNumOfIdleBlk++;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, FSR_STL_SUCCESS));
    return FSR_STL_SUCCESS;
}

/**
 *  @brief        Defragment meta unit.
 *
 *  @param[in]    pstClst  : cluster object
 *
 *  @return       FSR_STL_SUCCESS
 *  @author       Jeong-Uk Kang
 *  @version      1.1.0
 */
PRIVATE INT32 
_DegragmentMetas       (STLClstObj     *pstClst,
                        StlDfrgArg     *pstDfrg)
{
    const   UINT32          nMaxZone        = pstClst->stRootInfoBuf.stRootInfo.nNumZone;
            UINT32          nPgsPerSBlk     = pstClst->pstDevInfo->nPagesPerSBlk;
            StlZoneVU      *pstaZVU         = pstDfrg->pstaZVUs;
            STLZoneObj     *pstZone;
            STLZoneInfo    *pstZI;
            STLDirHdrHdl   *pstDH;
            STLDirHeaderFm *pstDHFm;
            UINT32          nZone;
            UINT32          nNumMeta;
            UINT32          nLastIdx;
            UINT32          nMetaIdx;
            UINT32          nTrgIdx;
            BOOL32          bWL;
            UINT32          nIdx;
            INT32           nRet        = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Meta uses only LSB pages */
    if (pstClst->pstDevInfo->nDeviceType == RBW_DEVTYPE_MLC)
    {
        nPgsPerSBlk >>= 1;
    }

    /* Reduce all meta unit */
    for (nZone = 0; nZone < nMaxZone; nZone++)
    {
        pstZone  = pstClst->stZoneObj + nZone;
        pstZI    = pstZone->pstZI;
        pstDH    = pstZone->pstDirHdrHdl;
        pstDHFm  = pstDH->pstFm;
        nNumMeta = pstZI->nNumMetaBlks;

        /* Insert all invalid meta unit to idle blk list */
        for (nLastIdx = 0; nLastIdx < pstZI->nNumMetaBlks; nLastIdx++)
        {
            if (pstDH->pValidPgCnt[nLastIdx] == 0)
            {
                /* Check whether the LastIdx's meta unit is idle or not */
                for (nIdx = 0; nIdx < pstDHFm->nNumOfIdleBlk; nIdx++)
                {
                    nMetaIdx = pstDHFm->nIdleBoListHead + nIdx;
                    if (nMetaIdx >= nNumMeta)
                    {
                        nMetaIdx -= nNumMeta;
                    }
                    FSR_ASSERT(nMetaIdx < nNumMeta);

                    if (pstDH->pIdleBoList[nMetaIdx] == nLastIdx)
                    {
                        break;
                    }
                }
                /* Insert the meta unit to idle blk list */
                if (nIdx == pstDHFm->nNumOfIdleBlk)
                {
                    nMetaIdx = pstDHFm->nIdleBoListHead + pstDHFm->nNumOfIdleBlk;
                    if (nMetaIdx >= nNumMeta)
                    {
                        nMetaIdx -= nNumMeta;
                    }
                    FSR_ASSERT(nMetaIdx < nNumMeta);

                    pstDH->pIdleBoList[nMetaIdx] = (UINT16)nLastIdx;
                    pstDHFm->nNumOfIdleBlk++;
                }
            }
        }
        FSR_ASSERT(pstDHFm->nNumOfIdleBlk > 0);

        nLastIdx = nNumMeta - 1;
        /* If the latest meta unit is header,
           we compact the other valid meta page */
        if (pstDHFm->nHeaderIdx == nLastIdx)
        {
            nIdx = 0;
            while (pstDHFm->nCPOffs < nPgsPerSBlk)
            {
                while (nIdx < pstZI->nNumMetaBlks)
                {
                    if (pstDH->pValidPgCnt[nIdx] > 0)
                    {
                        break;
                    }
                    nIdx++;
                }
                if (nIdx >= pstZI->nNumMetaBlks)
                {
                    break;
                }

                nRet = _ReclaimMeta(pstZone, nIdx);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    break;
                }

                REPORT_PROGRESS(pstDfrg);
            }
            if (nRet != FSR_STL_SUCCESS)
            {
                break;
            }

            nRet = FSR_STL_StoreDirHeader(pstZone, FALSE32, &bWL);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }

            REPORT_PROGRESS(pstDfrg);
        }

        /* Make the latest meta unit to idle meta unit */
        while (pstDH->pValidPgCnt[nLastIdx] != 0)
        {
            nRet = _ReclaimMeta(pstZone, nLastIdx);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }

            REPORT_PROGRESS(pstDfrg);

            if (pstDHFm->nCPOffs >= nPgsPerSBlk)
            {
                nRet = FSR_STL_StoreDirHeader(pstZone, FALSE32, &bWL);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    break;
                }

                REPORT_PROGRESS(pstDfrg);
            }
            else
            {
                FSR_ASSERT(pstDH->pValidPgCnt[nLastIdx] == 0);
            }
        }
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }

        /* reset erase count of meta unit */
        for (nIdx = 0; nIdx < pstZI->nNumMetaBlks; nIdx++)
        {
            pstDH->pMetaBlksEC[nIdx] = 0;
        }

        nTrgIdx = nLastIdx - 1;
        while (nTrgIdx < nNumMeta)
        {
            /* Compact to the latest meta unit */
            nMetaIdx = (UINT32)(-1);
            for (nIdx = 0; nIdx < pstDHFm->nNumOfIdleBlk; nIdx++)
            {
                nMetaIdx = pstDHFm->nIdleBoListHead + nIdx;
                if (nMetaIdx >= nNumMeta)
                {
                    nMetaIdx -= nNumMeta;
                }
                FSR_ASSERT(nMetaIdx < nNumMeta);

                if (pstDH->pIdleBoList[nMetaIdx] == nLastIdx)
                {
                    break;
                }
            }
            FSR_ASSERT(nIdx < pstDHFm->nNumOfIdleBlk);

            if (nMetaIdx != pstDHFm->nIdleBoListHead)
            {
                FSR_ASSERT(pstDH->pIdleBoList[nMetaIdx] == nLastIdx);
                pstDH->pIdleBoList[nMetaIdx] = pstDH->pIdleBoList[pstDHFm->nIdleBoListHead];
                pstDH->pIdleBoList[pstDHFm->nIdleBoListHead] = (UINT16)nLastIdx;
            }
            nRet = FSR_STL_StoreDirHeader(pstZone, FALSE32, &bWL);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }

            REPORT_PROGRESS(pstDfrg);

            while (pstDHFm->nCPOffs < nPgsPerSBlk)
            {
                while (nTrgIdx < nNumMeta)
                {
                    if (pstDH->pValidPgCnt[nTrgIdx] > 0)
                    {
                        break;
                    }
                    nTrgIdx--;
                }
                if (nTrgIdx >= nNumMeta)
                {
                    break;
                }

                nRet = _ReclaimMeta(pstZone, nTrgIdx);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR]  --%s() L(%d) : 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    break;
                }

                REPORT_PROGRESS(pstDfrg);
            }

            FSR_ASSERT(nLastIdx > 0);
            nLastIdx--;
        }
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }

        pstaZVU[nZone].nUsedSrt  = pstZI->aMetaVbnList[nLastIdx + 1];
        pstaZVU[nZone].nNumUMeta = (BADDR)(nNumMeta - nLastIdx - 1);

        /* erase idle meta unit */
        for (nIdx = 0; nIdx < nLastIdx; nIdx++)
        {
            nRet = FSR_STL_FlashErase(pstZone, pstZI->aMetaVbnList[nIdx]);
            if (nRet != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  --%s() L(%d) : 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }
            nRet = FSR_STL_SUCCESS;

            REPORT_PROGRESS(pstDfrg);
        }
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}

/*****************************************************************************/
/* Global Function Definition                                                */
/*****************************************************************************/

/**
 *  @brief          This function defragments the specified zone
 *
 *  @param[in]      pstZone     : partition object
 *  @param[in]      nMaxBlks    : The total units
 * 
 *  @return         FSR_STL_SUCCESS
 *  
 *  @author         Jeong-Uk Kang
 *  @version        1.1.0
 */
PUBLIC INT32
FSR_STL_DefragmentCluster  (STLClstObj     *pstClst,
                            const UINT32    nMaxBlks,
                            FSRStlDfrg     *pstSTLDfra,
                            FSRStlPrgFp     pfProgress)
{
            StlDfrgArg      stDfrg;
            STLRootInfo    *pstRI           = &(pstClst->stRootInfoBuf.stRootInfo);
    const   UINT32          nMaxZone        = pstRI->nNumZone;
            STLClstEnvVar  *pstClstEnv      = pstClst->pstEnvVar;
    const   UINT32          nECTH           = pstClstEnv->nECDiffThreshold;
            StlZoneVU       staZVUs[MAX_ZONE];
            STLZoneObj     *pstZone;
            STLZoneInfo    *pstZI;
            STLCtxInfoHdl  *pstCtx;
            STLCtxInfoFm   *pstCtxFm;
            UINT32          nMaxFlks;
            UINT32          nZone;
            BADDR           nVbn;
            UINT32          nIdx;
            INT32           nRet            = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s\r\n"), __FSR_FUNC__));

    FSR_ASSERT((nMaxZone >  0       ) &&
               (nMaxZone <= MAX_ZONE));

    stDfrg.pstaVUs = NULL;
    do
    {
        /* Allocate Virtual Unit Status */
        stDfrg.pstaVUs = FSR_STL_MALLOC(sizeof(StlVUInfo) * nMaxBlks,
                                FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_DRAM);
        if (stDfrg.pstaVUs == NULL)
        {
            nRet = FSR_STL_OUT_OF_MEMORY;
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }
        stDfrg.pstClst    = pstClst;
        stDfrg.nMaxBlk    = nMaxBlks;
        stDfrg.pstaZVUs   = staZVUs;

        /* disable wear-leveling */
        pstClstEnv->nECDiffThreshold = (UINT32)(-1);

        stDfrg.pfProgress = pfProgress;
        stDfrg.nCurStage  = 0;
        STAGE_PROGRESS(stDfrg);

        /* Reset erase counter */
        nRet = _ResetEraseCount(&stDfrg);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        STAGE_PROGRESS(stDfrg);

        /* Reduce all log group */
        for (nZone = 0; nZone < nMaxZone; nZone++)
        {
            pstZone = &(pstClst->stZoneObj[nZone]);
            nRet = _DefragmentZone(pstZone, &stDfrg);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }
        }
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }

        STAGE_PROGRESS(stDfrg);

        /* Get status of all Virtual Unit (VU) */
        FSR_OAM_MEMSET(stDfrg.pstaVUs, 0xFF, sizeof (StlVUInfo) * nMaxBlks);
        for (nZone = 0; nZone < nMaxZone; nZone++)
        {
            pstZone = &(pstClst->stZoneObj[nZone]);
            nRet = _FillVirtualUnitStatus(pstZone, &stDfrg);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }
        }
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }

#if (OP_STL_DEBUG_CODE == 1)
        for (nIdx = 0; nIdx < nMaxBlks; nIdx++)
        {
            FSR_ASSERT(stDfrg.pstaVUs[nIdx].nType != UNIT_TYPE_NONE);
            if (stDfrg.pstaVUs[nIdx].nType != UNIT_TYPE_ROOT)
            {
                FSR_ASSERT(stDfrg.pstaVUs[nIdx].nZoneID < nMaxZone);
            }
        }
#endif

        /* Reset all free blk and  Get VU information of each Zone. */
        FSR_OAM_MEMSET(staZVUs, 0xFF, sizeof (staZVUs));
        nVbn = MAX_ROOT_BLKS;
        for (nZone = 0; nZone < nMaxZone; nZone++)
        {
            pstZone  = &(pstClst->stZoneObj[nZone]);
            pstCtx   = pstZone->pstCtxHdl;
            pstCtxFm = pstCtx->pstFm;

            FSR_ASSERT(pstCtxFm->nNumLBlks == 0);
            FSR_OAM_MEMSET(pstCtx->pstActLogList, 0xFF, sizeof (STLActLogEntry) * MAX_ACTIVE_LBLKS);

            pstCtxFm->nNumIBlks = 0;
            nMaxFlks = pstZone->pstML->nMaxFreeSlots;
            pstCtxFm->nNumFBlks = 0;
            FSR_OAM_MEMSET(pstCtx->pFreeList, 0xFF, sizeof (BADDR)  * nMaxFlks);
            FSR_OAM_MEMSET(pstCtx->pFBlksEC,  0x00, sizeof (UINT32) * nMaxFlks);

            pstCtxFm->nBBlkVbn  = NULL_VBN;
            pstCtxFm->nBBlkEC   = (UINT32)(-1);

            pstCtxFm->nTBlkVbn  = NULL_VBN;
            pstCtxFm->nTBlkEC   = (UINT32)(-1);

            pstZI = pstZone->pstZI;
            staZVUs[nZone].nStartVbn = nVbn;
            staZVUs[nZone].nNumVbn   = pstZI->nNumTotalBlks;
            staZVUs[nZone].nNumMeta  = pstZI->nNumMetaBlks;
            nVbn = nVbn + pstZI->nNumTotalBlks;
        }

        STAGE_PROGRESS(stDfrg);

        /* Rearrange VUs */
        nRet = _RearrangeVUs(pstClst, &stDfrg);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        STAGE_PROGRESS(stDfrg);

        /* Reset free units */
        nRet = _ResetFreeUnits(pstClst, &stDfrg);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        STAGE_PROGRESS(stDfrg);

        nRet = _DegragmentMetas(pstClst, &stDfrg);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        STAGE_PROGRESS(stDfrg);

        /* Store root information after erase all root units */
        for (nIdx = 0; nIdx < MAX_ROOT_BLKS; nIdx++)
        {
            FSR_ASSERT(stDfrg.pstaVUs[nIdx].nType == UNIT_TYPE_ROOT);
            FSR_ASSERT(stDfrg.pstaVUs[nIdx].nDgn  == NULL_DGN);
            nRet = FSR_STL_FlashErase(pstZone, nIdx);
            if (nRet != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  --%s() L(%d) : 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }

            pstRI->aRootBlkEC[nIdx] = 0;
            nRet = FSR_STL_SUCCESS;
        }
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }
        pstRI->nRootCPOffs = 0;
        nRet = FSR_STL_StoreRootInfo(pstClst);

        pstSTLDfra->nNumGroup = nMaxZone + 1;
        pstSTLDfra->staGroup[0].nStartVun = 0;
        pstSTLDfra->staGroup[0].nNumOfVun = 1;
        for (nZone = 0; nZone < nMaxZone; nZone++)
        {
            pstSTLDfra->staGroup[nZone + 1].nStartVun = staZVUs[nZone].nUsedSrt;
            pstSTLDfra->staGroup[nZone + 1].nNumOfVun = staZVUs[nZone].nNumUMeta + staZVUs[nZone].nNumUser;
        }

    } while (0);

    if (stDfrg.pstaVUs != NULL)
    {
        FSR_OAM_Free(stDfrg.pstaVUs);
        stDfrg.pstaVUs = NULL;
    }

    /* re-enable wear-leveling */
    pstClstEnv->nECDiffThreshold = nECTH;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d)\r\n"),
            __FSR_FUNC__, __LINE__));
    return FSR_STL_SUCCESS;
}

#endif  /* #if defined (FSR_STL_FOR_PRE_PROGRAMMING) */

