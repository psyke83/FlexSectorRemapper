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
 *  @file       FSR_STL_MergeMgr.c
 *  @brief      STL Compaction/Merge Manager.
 *  @author     Wonmoon Cheon
 *  @date       09-MAR-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  09-MAR-2007 [Wonmoon Cheon] : first writing
 *  @n  28-JAN-2008 [Seunghyun Han] : dead code elimination
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

/*****************************************************************************/
/* Local type defines                                                        */
/*****************************************************************************/

/*****************************************************************************/
/* Local constant definitions                                                */
/*****************************************************************************/

/*****************************************************************************/
/* Local (static) function prototype                                         */
/*****************************************************************************/

PRIVATE BADDR   _GetRepValidLbn        (STLZoneObj     *pstZone,
                                        STLLogGrpHdl   *pstLogGrp,
                                        STLLog         *pstLog);

PRIVATE INT32   _CheckSpare4Copyback   (STLZoneObj     *pstZone,
                                        const PADDR     nLpn,
                                        const PADDR     nSrcOff,
                                        const PADDR     nDstOff,
                                        FSRSpareBuf    *pstSBuf);

/*****************************************************************************/
/* Local (static)  Function Definition                                       */
/*****************************************************************************/

/**
 *  @brief          This function returns a representative LBN 
 *  @n               which is stored in the specified log block.
 *
 *  @param[in]      pstZone         : zone object
 *  @param[in]      pstLogGrp       : log group object
 *  @param[in]      pstLog          : target log
 *
 *  @return         representative LBN in the min log
 *
 *  @author         Wonmoon Cheon
 *  @version        1.2.0
 */
PRIVATE BADDR 
_GetRepValidLbn    (STLZoneObj     *pstZone,
                    STLLogGrpHdl   *pstLogGrp,
                    STLLog         *pstLog)
{
    const   UINT32  nNSft       = pstZone->pstRI->nNShift;
    const   UINT32  nPgsPUint   = pstZone->pstDevInfo->nPagesPerSBlk;
    const   UINT32  nPgsSft     = pstZone->pstDevInfo->nPagesPerSbShift;
    const   UINT32  nLogIdx     = pstLog->nSelfIdx;
            UINT32  nIdx;
            BADDR   nVLbn       = NULL_VBN;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    for (nIdx = 0; nIdx < (nPgsPUint << nNSft); nIdx++)
    {
        if (pstLogGrp->pMapTbl[nIdx] != NULL_POFFSET)
        {
            if ((UINT32)(pstLogGrp->pMapTbl[nIdx] >> nPgsSft) == nLogIdx)
            {
                nVLbn = (BADDR)(((pstLogGrp->pstFm->nDgn) << nNSft) + (nIdx >> nPgsSft));
                break;
            }
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : %d\r\n"), __FSR_FUNC__, nVLbn));
    return nVLbn;
}

/**
 *    @brief        This function check and make the PTF.
 * 
 *    @param[in]    pstZone  : zone object
 *    @param[in]    nLpn     : LPN of source (data)
 *    @param[in]    nSrcOff  : The page offset of source VPN
 *    @param[in]    nDstOff  : The page offset of destination VPN
 *    @param[in]    pstSBuf  : Spare buffer of BML
 *
 *    @return       FSR_STL_SUCCESS
 *    @author       Jaesoo Lee
 *    @version      1.1.0
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
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRndInCnt));
    return nRndInCnt;
}


/*****************************************************************************/
/* Global Function Definition                                                */
/*****************************************************************************/

/**
 *  @brief      This function stores BMT/PMT map pages into context block.
 *
 *  @param[in]      pstZone    : STLZoneObj pointer
 *  @param[in]      pstLogGrp  : log group object
 *  @param[in]      pstLog     : log object
 *  @param[in]      bMergeFlag : merge flag
 *
 *  @return         FSR_STL_SUCCESS
 *
 *  @author         Wonmoon Cheon
 *  @version        1.1.0
 */
PUBLIC INT32 
FSR_STL_StoreMapInfo   (STLZoneObj     *pstZone,
                        STLLogGrpHdl   *pstLogGrp,
                        STLLog         *pstLog,
                        BOOL32          bMergeFlag)
{
    RBWDevInfo     *pstDev      = NULL;
    STLCtxInfoHdl  *pstCtxInfo  = NULL;
    INT32           nRet        = FSR_STL_SUCCESS;
    STLLog         *pstTempLog  = NULL;
    STLLog         *pstLogList  = pstLogGrp->pstLogList;
    UINT32          nIdx;
    UINT16         *pLogVPgCnt;
    UINT32          nVPgCnt;
    UINT32          nWay;
    BOOL32          bVictim;
    BOOL32          bRet;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s\r\n"), __FSR_FUNC__));

    /* get device info pointer*/
    pstDev = pstZone->pstDevInfo;

    /* get context info pointer*/
    pstCtxInfo = pstZone->pstCtxHdl;

    /*
     * active log is changed to inactive log
     * to remove the log from PMT at open time. 
     * nMergedDgn/nMergedVbn members should be reset by FSR_STL_StoreBMTCtx()
     */
    if (pstLog->nState &(1 << LS_ACTIVE_SHIFT))
    {
        FSR_STL_RemoveActLogList(pstZone, pstLogGrp->pstFm->nDgn, pstLog);
    }

    /*setting the removed log info for sudden power-off failure*/
    pstCtxInfo->pstFm->nMergedDgn = pstLogGrp->pstFm->nDgn;
    pstCtxInfo->pstFm->nMergedVbn = pstLog->nVbn;

    if (bMergeFlag == TRUE32)
    {
        /* store BMT+CXT*/

        nRet = FSR_STL_StoreBMTCtx(pstZone, FALSE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return nRet;
        }
    }

    /* remove the log from the group*/
    FSR_STL_RemoveLog(pstZone, pstLogGrp, pstLog);

    /* insert other log blocks with no valid page*/
    if (pstLogGrp->pstFm->nNumLogs > 1)
    {
        /* we need to avoid the removal of this log group itself when merge (POR) */
        for (nIdx = 0; (nIdx < pstZone->pstRI->nK) && bMergeFlag; nIdx++)
        {
            pstTempLog = pstLogList + nIdx;

            pLogVPgCnt = pstLogGrp->pLogVPgCnt + (nIdx << pstDev->nNumWaysShift);
            nVPgCnt    = 0;
            for (nWay = 0; nWay < pstDev->nNumWays; nWay++)
            {
                nVPgCnt += pLogVPgCnt[nWay];
            }

            if ((nVPgCnt             == 0       ) &&
                (pstTempLog->nVbn    != NULL_VBN) &&
                (pstTempLog->nCPOffs >= pstDev->nPagesPerSBlk))
            {
                /* active log is changed to inactive log*/
                if (pstTempLog->nState &(1 << LS_ACTIVE_SHIFT))
                {
                    FSR_STL_RemoveActLogList(pstZone, pstLogGrp->pstFm->nDgn, pstTempLog);
                }

                bRet = FSR_STL_CheckFullFreeList(pstZone);
                FSR_ASSERT(!bRet);
                FSR_STL_AddFreeList(pstZone, pstTempLog->nVbn, pstTempLog->nEC);

                /* remove the log from the group*/
                FSR_STL_RemoveLog(pstZone, pstLogGrp, pstTempLog);
            }
        }
    }

    /* check if the group is still merge victim group or not*/
    bRet = FSR_STL_IsMergeVictimGrp(pstZone, pstLogGrp);
    if (bRet == TRUE32)
    {
        nRet = FSR_STL_CheckMergeVictimGrp(pstZone, pstLogGrp, FALSE32, &bVictim);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return nRet;
        }

        if (bVictim == FALSE32)
        {
            /* clear the flag bit*/
            FSR_STL_SetMergeVictimGrp(pstZone, pstLogGrp->pstFm->nDgn, FALSE32);
        }
    }

    /* setting the removed log info for sudden power-off failure*/
    pstCtxInfo->pstFm->nMergedDgn = pstLogGrp->pstFm->nDgn;

    /* store PMT+CXT, update PMTDir*/

    nRet = FSR_STL_StorePMTCtx(pstZone, pstLogGrp, FALSE32);
    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}


/**
 *  @brief   This function compacts the specified log group to reduce the number of logs in that log group.
 *
 *  @param[in]   pstZone     : partition object
 *  @param[in]   pstLogGrp   : log group object
 *  @param[in]   pstLog      : log object pointer
 * 
 *  @return      FSR_STL_SUCCESS
 *  
 *  @author         Wonmoon Cheon
 *  @version        1.1.0
 */
PUBLIC INT32 
FSR_STL_CompactLog     (STLZoneObj     *pstZone,
                        STLLogGrpHdl   *pstLogGrp,
                        STLLog        **pstLog)
{
    /* get device info pointer*/
    const RBWDevInfo   *pstDev          = pstZone->pstDevInfo;
    STLLogGrpHdl       *pstActLogGrp;
    STLLogGrpFm        *pstLogGrpFm     = pstLogGrp->pstFm;
    STLLog             *pstLogList      = pstLogGrp->pstLogList;
    const BADDR         nTrgDgn         = pstLogGrpFm->nDgn;
    POFFSET             nMPOff;
    STLLog             *pstVictimLog1;
    STLLog             *pstVictimLog2;
    BADDR               nVL1Vbn;
    UINT32              nVL1EC;
    UINT8               nVL1Idx;
    STLLog             *pstNewLog;
    BADDR               nFBlkVbn;
    UINT32              nFBlkEC;
    PADDR               nDstVpn;
    STLLog             *pstTempLog;
    UINT8               nTempIdx;
    UINT32              nSecondMin;
    BOOL32              bVictim         = FALSE32;
    BADDR               nRepLbn;
    const UINT32        nUnitPgs        = pstZone->pstRI->nN << pstDev->nPagesPerSbShift;
    UINT32              nRemainPgs      = 0;
    UINT16             *pLogVPgCnt;
    UINT32              nL1VPgCnt;
    UINT32              nL2VPgCnt;
    PADDR               nStartLpn;
    UINT8               nLogIdx;
    BADDR               nSrcVbn;
    POFFSET             nSrcPOffs;
    PADDR               nLpn;
    UINT32              nWay;
    UINT32              naWayIdx[FSR_MAX_WAYS];
    INT32               nRet;
    STLClstObj         *pstClst     = FSR_STL_GetClstObj(pstZone->nClstID);
    const UINT32        n1stVun     = pstClst->pstEnvVar->nStartVbn;
    BMLCpBkArg        **ppstBMLCpBk = pstClst->pstBMLCpBk;
    BMLCpBkArg         *pstCpBkArg  = pstClst->staBMLCpBk;
    BMLRndInArg        *pstRndIn    = pstClst->staBMLRndIn;
    FSRSpareBuf        *pstSBuf     = pstClst->staSBuf;
#if (OP_SUPPORT_MSB_PAGE_WAIT == 1)
    UINT32              nIdx;
    BOOL32              bIsLSB;
#endif
#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1 && OP_SUPPORT_BU_DELAYED_FLUSH == 1)
    STLBUCtxObj        *pstBUCtx        = pstZone->pstBUCtxObj;
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1 && OP_SUPPORT_BU_DELAYED_FLUSH == 1) */
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s\r\n"), __FSR_FUNC__));

    /* process the garbage blocks if required */
    nRet = FSR_STL_ReverseGC(pstZone, pstZone->pstRI->nK);
    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }

#if (OP_SUPPORT_DATA_WEAR_LEVELING == 1)

    nRet = FSR_STL_WearLevelFreeBlk(pstZone, 1, NULL_DGN);
    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }

#endif /* (OP_SUPPORT_DATA_WEAR_LEVELING == 1) */

#if (OP_SUPPORT_STATISTICS_INFO == 1)
    pstZone->pstStats->nCompactionCnt++;
#endif

    /* Even though compacting the log group, the log group state should be same.*/
    pstActLogGrp = FSR_STL_SearchLogGrp(pstZone->pstActLogGrpList, nTrgDgn);
    FSR_ASSERT((pstActLogGrp == NULL) ||
               (pstActLogGrp == pstLogGrp));

#if (OP_SUPPORT_RUNTIME_PMT_BUILDING == 1)
    if (pstActLogGrp != NULL)
    {
        /* Execute Run time scanning */
        nRet = FSR_STL_ScanActLogBlock(pstZone,nTrgDgn);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR, 
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
            return nRet;
        }
    }
#endif /*OP_SUPPORT_RUNTIME_PMT_BUILDING*/


#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1 && OP_SUPPORT_BU_DELAYED_FLUSH == 1)
    /* write the previous misaligned sectors into log block */

    if ((pstBUCtx->nBufferedDgn == nTrgDgn) &&
        (pstBUCtx->nBufState    != STL_BUFSTATE_CLEAN))
    {
        /* flush currently buffered single page into log block */
        nRet = FSR_STL_FlushBufferPage(pstZone);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return nRet;
        }
    }
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1 && OP_SUPPORT_BU_DELAYED_FLUSH == 1) */

    /* By wear-level, if the contents of pMetaPgBuf to which pstLogGrp points is changed,
     * we reload the PMT of target DGN to pMetaPgBuf */
    if (nTrgDgn != pstLogGrpFm->nDgn)
    {
        FSR_ASSERT(pstActLogGrp == NULL);
        FSR_ASSERT(FSR_STL_SearchLogGrp(pstZone->pstInaLogGrpCache, nTrgDgn) == NULL);

        nMPOff = NULL_POFFSET;
        FSR_STL_SearchPMTDir(pstZone, nTrgDgn, &nMPOff);
        if (nMPOff == NULL_POFFSET)
        {
            nRet = FSR_STL_CRITICAL_ERROR;
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return nRet;
        }
        FSR_ASSERT(nMPOff != NULL_POFFSET);

        nRet = FSR_STL_LoadPMT(pstZone, nTrgDgn, nMPOff, &(pstActLogGrp), FALSE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return nRet;
        }
        FSR_ASSERT(pstLogGrp == pstActLogGrp);

        pstActLogGrp = NULL;
    }

    /* Reserve meta page */
    nRet = FSR_STL_ReserveMetaPgs(pstZone, 1, TRUE32);
    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }

    /* Get the 1st victim log which has the minimum valid pages */
    pstVictimLog1   = pstLogList + pstLogGrpFm->nMinVPgLogIdx;
    nRepLbn         = _GetRepValidLbn(pstZone, pstLogGrp, pstVictimLog1);

    FSR_ASSERT(pstLogGrpFm->nNumLogs > 1);

    /* Calculate the number of valid pages */
    pLogVPgCnt = pstLogGrp->pLogVPgCnt
               + (pstVictimLog1->nSelfIdx << pstDev->nNumWaysShift);
    nL1VPgCnt  = 0;
    for (nWay = 0; nWay < pstDev->nNumWays; nWay++)
    {
        nL1VPgCnt     += pLogVPgCnt[nWay];
        naWayIdx[nWay] = nWay;
    }

    if (nL1VPgCnt != 0)
    {
        /* get free block from free block list*/
        nRet = FSR_STL_GetFreeBlk(pstZone, &nFBlkVbn, &nFBlkEC);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return nRet;
        }
    }

    /* Backup the VBN and the erase counter of the 1st victim log 
     * This value is used when AddFreeList(), 
     * this valid is reset when removed by RemoveLog()
     */
    nVL1Vbn = pstVictimLog1->nVbn;
    nVL1EC  = pstVictimLog1->nEC;
    nVL1Idx = pstVictimLog1->nSelfIdx;

    /* active log is changed to inactive log*/
    if (pstVictimLog1->nState & (1 << LS_ACTIVE_SHIFT))
    {
        FSR_STL_RemoveActLogList(pstZone, pstLogGrp->pstFm->nDgn, pstVictimLog1);
    }

    /* remove the log from the group*/
    FSR_STL_RemoveLog(pstZone, pstLogGrp, pstVictimLog1);

    if (nL1VPgCnt != 0)
    {
        /* Find the 2nd victim log which has the 2nd minimum valid pages */
        pstVictimLog2 = NULL;
        nSecondMin    = (UINT32)(-1);

        nTempIdx      = pstLogGrpFm->nHeadIdx;
        while (nTempIdx != NULL_LOGIDX)
        {
            if (nTempIdx != nVL1Idx)
            {
                pLogVPgCnt = pstLogGrp->pLogVPgCnt + (nTempIdx << pstDev->nNumWaysShift);
                nL2VPgCnt  = 0;
                for (nWay = 0; nWay < pstDev->nNumWays; nWay++)
                {
                    nL2VPgCnt += pLogVPgCnt[nWay];
                }

                if (nL2VPgCnt < nSecondMin)
                {
                    nSecondMin      = nL2VPgCnt;
                    pstVictimLog2   = pstLogList + nTempIdx;
                }
            }

            nTempIdx = pstLogList[nTempIdx].nNextIdx;
        }
        FSR_ASSERT(pstVictimLog2 != NULL);

        pstVictimLog2->nCPOffs = (POFFSET)(pstDev->nPagesPerSBlk);
        nL2VPgCnt   = nSecondMin;
        nRemainPgs  = nL1VPgCnt + nL2VPgCnt;

        /*
         * when 'd' is greater than 1, this code may make a problem
         * add the free block into the log group
         */
        pstNewLog = FSR_STL_AddNewLog(pstZone, pstLogGrp, nRepLbn, nFBlkVbn, nFBlkEC);
        if (pstNewLog == NULL)
        {
            /* critical error*/
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, FSR_STL_ERR_NO_LOG));
            return FSR_STL_ERR_NO_LOG;
        }

        FSR_OAM_MEMSET(ppstBMLCpBk, 0x00, sizeof(long) * FSR_MAX_WAYS);

        nStartLpn = (pstLogGrp->pstFm->nDgn << (pstZone->pstRI->nNShift + pstDev->nPagesPerSbShift));

        /* copy source log's pages to destination log */
        nWay = 0;
        while (nRemainPgs != 0)
        {
            while (naWayIdx[nWay] < nUnitPgs)
            {
                nLogIdx  = (UINT8)(pstLogGrp->pMapTbl[naWayIdx[nWay]] >> pstDev->nPagesPerSbShift);

                /*  valid page exists in the victim log we need to copy the page to new free block*/
                if (nLogIdx == nVL1Idx)
                {
                    /* source page address : victim log1*/
                    nSrcVbn = nVL1Vbn;
                }
                else if (nLogIdx == pstVictimLog2->nSelfIdx)
                {
                    /* source page address : victim log2*/
                    nSrcVbn = pstVictimLog2->nVbn;
                }
                else
                {
                    nSrcVbn = NULL_VBN;
                }

                if (nSrcVbn != NULL_VBN)
                {
                    nLpn = nStartLpn + naWayIdx[nWay];

                    /* source page address*/
                    nSrcPOffs = (POFFSET)((pstLogGrp->pMapTbl[naWayIdx[nWay]]) & (pstDev->nPagesPerSBlk - 1));

                    /* destination page address*/
                    nDstVpn = FSR_STL_GetLogCPN(pstZone, pstNewLog, nLpn);

                    pstCpBkArg[nWay].nSrcVun        = (UINT16)(nSrcVbn + n1stVun);
                    pstCpBkArg[nWay].nSrcPgOffset   = (UINT16)(nSrcPOffs);
                    pstCpBkArg[nWay].nDstVun        = (UINT16)(pstNewLog->nVbn + n1stVun);
                    pstCpBkArg[nWay].nDstPgOffset   = (UINT16)(pstNewLog->nCPOffs);
                    pstCpBkArg[nWay].nRndInCnt      =_CheckSpare4Copyback(pstZone, nLpn, nSrcPOffs, pstNewLog->nCPOffs, &(pstSBuf[nWay]));
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

                    /* increase the clean page offset of the log*/
                    pstNewLog->nCPOffs++;

                    FSR_ASSERT(nRemainPgs > 0);
                    nRemainPgs--;

                    /* Reset PMT to reduce overhead,
                     * because Victim Log1 and Log2 will be removed
                     */
                    pstLogGrp->pMapTbl[naWayIdx[nWay]] = NULL_POFFSET;
                    /* log group PMT update*/
                    FSR_STL_UpdatePMT(pstZone, pstLogGrp, pstNewLog, nLpn, nDstVpn);

                    /* change the state of log block*/
                    FSR_STL_ChangeLogState(pstZone, pstLogGrp, pstNewLog, nLpn);

                    naWayIdx[nWay] += pstDev->nNumWays;
                    break;
                }

                naWayIdx[nWay] += pstDev->nNumWays;
            }

            nWay++;
            if (nWay >= pstDev->nNumWays)
            {
                nWay = 0;
            }

            if ((nWay == 0) || (nRemainPgs == 0))
            {
#if defined (FSR_POR_USING_LONGJMP)
                FSR_FOE_BeginWriteTransaction(0);
#endif
                nRet = FSR_BML_CopyBack(pstZone->nVolID, ppstBMLCpBk, FSR_BML_FLAG_NONE);
                if (nRet != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR] %s(), L(%d) - FSR_BML_CopyBack (0x%x)\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                    
                    FSR_ASSERT(nRet != FSR_BML_VOLUME_NOT_OPENED);
                    FSR_ASSERT(nRet != FSR_BML_INVALID_PARAM);
                    FSR_ASSERT(nRet != FSR_BML_WR_PROTECT_ERROR);

                    FSR_STL_AddFreeList(pstZone, nFBlkVbn, nFBlkEC + 1);
                    FSR_STL_LockPartition (pstZone->nClstID);
                    return nRet;
                }

                if (nRemainPgs == 0)
                {
#if (OP_SUPPORT_MSB_PAGE_WAIT == 1)
                    for (nIdx = 0; nIdx < pstDev->nNumWays; nIdx++)
                    {
                        if (ppstBMLCpBk[nIdx] != NULL)
                        {
                            bIsLSB = FSR_STL_FlashIsLSBPage(pstZone, ppstBMLCpBk[nIdx]->nDstPgOffset);
                            pstClst->baMSBProg[nIdx] = !bIsLSB;
                        }
                        else
                        {
                            pstClst->baMSBProg[nIdx] = FALSE32;
                        }
                    }
#endif
                }
                else
                {
                    FSR_OAM_MEMSET(ppstBMLCpBk, 0x00, sizeof(long) * FSR_MAX_WAYS);
                }
            }

#if (OP_STL_DEBUG_CODE == 1)
            {
                UINT32 i;
                for (i = 0; i < pstDev->nNumWays; i++)
                {
                    if (naWayIdx[i] < nUnitPgs)
                        break;
                }
                FSR_ASSERT((i < pstDev->nNumWays) ||
                           (nRemainPgs == 0))
            }
#endif
        }
    }
    else
    {
        pstVictimLog2 = NULL;
        pstNewLog     = NULL;
    }

    FSR_STL_UpdatePMTCost(pstZone, pstLogGrp);

    FSR_ASSERT((nL1VPgCnt  == 0) ||
               (nRemainPgs == 0));
    FSR_ASSERT(!FSR_STL_CheckFullFreeList(pstZone));

    /* Return garbage blk */
    FSR_STL_AddFreeList(pstZone, nVL1Vbn, nVL1EC);
    if (pstVictimLog2 != NULL)
    {
        FSR_STL_AddFreeList(pstZone, pstVictimLog2->nVbn, pstVictimLog2->nEC);

        /* active log is changed to inactive log*/
        if (pstVictimLog2->nState & (1 << LS_ACTIVE_SHIFT))
        {
            FSR_STL_RemoveActLogList(pstZone, pstLogGrp->pstFm->nDgn, pstVictimLog2);
        }

        /* remove the log from the group*/
        FSR_STL_RemoveLog(pstZone, pstLogGrp, pstVictimLog2);
    }

    /* check if the group is still merge victim group or not*/
    nRet = FSR_STL_IsMergeVictimGrp(pstZone, pstLogGrp);
    if (nRet == TRUE32)
    {
        nRet = FSR_STL_CheckMergeVictimGrp(pstZone, pstLogGrp, FALSE32, &bVictim);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return nRet;
        }

        if (bVictim == FALSE32)
        {
            /* clear the flag bit*/
            FSR_STL_SetMergeVictimGrp(pstZone, pstLogGrp->pstFm->nDgn, FALSE32);
        }
    }

    /*
     * After compaction, check if all logs in the group are inactive or not
     * We should not change the group state (active/inactive).
     *  BEFORE : Active -> AFTER : Inactive
     */
    if ((pstActLogGrp != NULL) &&
        (FSR_STL_CheckFullInaLogs(pstLogGrp) == TRUE32))
    {
        /* we need to make a log be active.*/
        if (pstNewLog != NULL)
        {
            pstTempLog = pstNewLog;
        }
        else
        {
            /* always, there is one more logs.*/
            FSR_ASSERT(pstLogGrp->pstFm->nNumLogs >= 1);
            FSR_ASSERT(pstLogGrp->pstFm->nHeadIdx != NULL_LOGIDX);

            /* let's make the head log be active.*/
            pstTempLog = &(pstLogGrp->pstLogList[pstLogGrp->pstFm->nHeadIdx]);
        }

        FSR_STL_AddActLogList(pstZone, pstLogGrp->pstFm->nDgn, pstTempLog);
    }

    /* store PMT+CXT, update PMTDir*/
    nRet = FSR_STL_StorePMTCtx(pstZone, pstLogGrp, TRUE32);
    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }

    /* output : new log*/
    *pstLog = pstNewLog;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}


/**
*  @brief       This function checks if the log group can be compacted from source to destination log.
*
*  @param[in]   pstZone         : partition object
*  @param[in]   pstLogGrp       : log group object
*  @param[out]  nSrcLogIdx      : source log index when compaction
*  @param[out]  nDstLogIdx      : destination log index when compaction
*  @param[in]   bActiveDstOnly  : flag that destination should be active log
* 
*  @return      FSR_STL_SUCCESS
*  
*  @author         Wonmoon Cheon
*  @version        1.1.0
*/
PUBLIC BOOL32 
FSR_STL_CheckCopyCompaction    (STLZoneObj     *pstZone,
                                STLLogGrpHdl   *pstLogGrp,
                                UINT8          *pnSrcLogIdx,
                                UINT8          *pnDstLogIdx,
                                BOOL32          bActiveDstOnly)
{
    /* get device info pointer*/
    const RBWDevInfo   *pstDev  = pstZone->pstDevInfo;
    BOOL32              bCopyCompact = FALSE32;
    UINT16             *pLogVPgCnt;
    UINT32              nVPgCnt;
    UINT32              nWay;
    UINT8               nIdx1;
    UINT8               nIdx2;
    STLLog             *pstLog1;
    STLLog             *pstLog2;
    UINT16              naClnPgs[FSR_MAX_WAYS];
    UINT32              nClnOff;
    UINT16              nClnPgs;
    UINT32              nMinValidPgs = (UINT32)(-1);
    BOOL32              bCond;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s\r\n"), __FSR_FUNC__));

    /*
     * possible condition :
     * 1) at least one log has clean pages
     * 2) there is sufficient clean pages to copy from other log
     * 3) we should consider way offset
     */

    if (pstLogGrp->pstFm->nNumLogs > pstZone->pstRI->nN)
    {
        /* loop until we can find source/destination pair*/
        nIdx1 = pstLogGrp->pstFm->nHeadIdx;

        while (nIdx1 != NULL_LOGIDX)
        {
            pstLog1 = &(pstLogGrp->pstLogList[nIdx1]);

                /* when bActiveDstOnly is true, pstLog1 have to be active*/
            if ((pstLog1->nCPOffs < pstDev->nPagesPerSBlk) &&
                ((bActiveDstOnly == FALSE32) ||
                 ((pstLog1->nState & (1 << LS_ACTIVE_SHIFT)) != 0)))
            {
                /*
                 * In worst case, actual number of clean pages can be reduced by nNumWays,
                 * because we should consider way offset.
                 */
                nClnPgs = (UINT16)((pstDev->nPagesPerSBlk - pstLog1->nCPOffs) >> pstDev->nNumWaysShift);
                for (nWay = 0; nWay < pstDev->nNumWays; nWay++)
                {
                    naClnPgs[nWay] = nClnPgs;
                }
                nClnOff = (pstLog1->nCPOffs & (pstDev->nNumWays - 1));
                if (nClnOff > 0)
                {
                    for (nWay = nClnOff; nWay < pstDev->nNumWays; nWay++)
                    {
                        naClnPgs[nWay]++;
                    }
                }

                nIdx2 = pstLogGrp->pstFm->nTailIdx;
                while (nIdx2 != NULL_LOGIDX)
                {
                    pstLog2 = &(pstLogGrp->pstLogList[nIdx2]);

                    if (pstLog1 != pstLog2)
                    {
                        bCond      = FALSE32;
                        pLogVPgCnt = pstLogGrp->pLogVPgCnt + (pstLog2->nSelfIdx << pstDev->nNumWaysShift);
                        nVPgCnt    = 0;
                        for (nWay = 0; nWay < pstDev->nNumWays; nWay++)
                        {
                            if (pLogVPgCnt[nWay] > naClnPgs[nWay])
                            {
                                bCond = TRUE32;
                            }
                            nVPgCnt += pLogVPgCnt[nWay];
                        }

                        if ((bCond   == FALSE32      ) &&
                            (nVPgCnt <  nMinValidPgs ))
                        {
                            /*
                             * copy compaction is possible
                             * source log has minimum valid pages
                             * destination log has enough clean pages to copy
                             */
                            *pnSrcLogIdx = pstLog2->nSelfIdx;
                            *pnDstLogIdx = pstLog1->nSelfIdx;
                            nMinValidPgs = nVPgCnt;
                            bCopyCompact = TRUE32;
                            /*
                             * we can not exit this loop, though we are successful to find
                             * because another src/dst pair can be better than this one.
                             */
                        }
                    }

                    nIdx2 = pstLog2->nPrevIdx;
                }
            }

            nIdx1 = pstLog1->nNextIdx;
        }
    }

#if (OP_STL_DEBUG_CODE == 1)
    if (bCopyCompact == TRUE32)
    {
        pstLog1 = &(pstLogGrp->pstLogList[*pnDstLogIdx]);

        pLogVPgCnt = pstLogGrp->pLogVPgCnt + ((*pnSrcLogIdx) << pstDev->nNumWaysShift);
        nVPgCnt    = 0;
        for (nWay = 0; nWay < pstDev->nNumWays; nWay++)
        {
            nVPgCnt += pLogVPgCnt[nWay];
        }

        FSR_ASSERT(nVPgCnt <= (pstDev->nPagesPerSBlk - pstLog1->nCPOffs));
    }
#endif

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, bCopyCompact));
    return bCopyCompact;
}

/**
 *  @brief      This function checks if the log group can be compacted.
 *
 *  @param[in]  pstZone         : zone object
 *  @param[in]  pstDirHdr       : directory header object
 *  @param[in]  nDirIdx         : directory entry index of victim log group
 *  @param[out] pbCompaction    : flag for compaction
 *  @param[out] nSrcLogIdx      : source log index when compaction
 *  @param[out] nSrcLogIdx      : destination log index when compaction
 * 
 *  @return     FSR_STL_SUCCESS
 *  
 *  @author     Wonmoon Cheon
 *  @version    1.1.0
 */
PUBLIC INT32 
FSR_STL_CheckInaLogGrpCompaction   (STLZoneObj     *pstZone,
                                    STLDirHdrHdl   *pstDirHdr,
                                    UINT32          nDirIdx,
                                    BOOL32         *pbCompaction,
                                    UINT8          *pnSrcLogIdx,
                                    UINT8          *pnDstLogIdx)
{
    STLLogGrpHdl   *pstLogGrp;
    POFFSET         nMetaPOffs  = NULL_POFFSET;
    BADDR           nDgn;
    UINT8           nSrcIdx     = NULL_LOGIDX;
    UINT8           nDstIdx     = NULL_LOGIDX;
    INT32           nRet;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s\r\n"), __FSR_FUNC__));

    nDgn = (BADDR)nDirIdx;

    /* get DGN, meta page offset of the log group*/

    nMetaPOffs = pstDirHdr->pstPMTDir[nDirIdx].nPOffs;

    /* load the log group from flash*/
    pstLogGrp = NULL;
    nRet      = FSR_STL_LoadPMT(pstZone, nDgn, nMetaPOffs, &pstLogGrp, FALSE32);
    if (nRet == FSR_STL_SUCCESS)
    {
        *pbCompaction = FSR_STL_CheckCopyCompaction(pstZone, pstLogGrp, &nSrcIdx, &nDstIdx, FALSE32);
        *pnSrcLogIdx = nSrcIdx;
        *pnDstLogIdx = nDstIdx;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}

/**
 *  @brief   This function checks if the log group can be compacted.
 *
 *  @param[in]   pstZone         : partition object
 *  @param[in]   pstLogGrp       : active log group object
 *  @param[in]   nSrcLogIdx      : source log index 
 *  @param[in]   nSrcLogIdx      : destination log index
 * 
 *  @return      FSR_STL_SUCCESS
 *  
 *  @author         Wonmoon Cheon
 *  @version        1.1.0
 */
PUBLIC INT32 
FSR_STL_CompactActLogGrp   (STLZoneObj     *pstZone,
                            STLLogGrpHdl   *pstLogGrp,
                            UINT8           nSrcLogIdx,
                            UINT8           nDstLogIdx)
{
    /* get device info pointer*/
    const RBWDevInfo   *pstDev      = pstZone->pstDevInfo;
    /* get context info object*/
    STLCtxInfoHdl      *pstCtx      = pstZone->pstCtxHdl;
    STLLog             *pstSrcLog;
    STLLog             *pstDstLog;
    INT32               nRet;
    UINT8               nLogIdx;
    PADDR               nLpn;
    PADDR               nStartLpn;
    PADDR               nDstVpn;
    POFFSET             nSrcPOffs;
    BOOL32              bVictim;
    UINT16             *pLogVPgCnt;
    UINT16              nRemainPgs;
    const UINT32        nUnitPgs    = pstZone->pstRI->nN << pstDev->nPagesPerSbShift;
    UINT32              nWay;
    UINT32              naWayIdx[FSR_MAX_WAYS];
    STLClstObj         *pstClst     = FSR_STL_GetClstObj(pstZone->nClstID);
    const UINT32        n1stVun     = pstClst->pstEnvVar->nStartVbn;
    BMLCpBkArg        **ppstBMLCpBk = pstClst->pstBMLCpBk;
    BMLCpBkArg         *pstCpBkArg  = pstClst->staBMLCpBk;
    BMLRndInArg        *pstRndIn    = pstClst->staBMLRndIn;
    FSRSpareBuf        *pstSBuf     = pstClst->staSBuf;
#if (OP_SUPPORT_MSB_PAGE_WAIT == 1)
    UINT32              nIdx;
    BOOL32              bIsLSB;
#endif
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s\r\n"), __FSR_FUNC__));

    /* process the garbage blocks if required */
    nRet = FSR_STL_ReverseGC(pstZone, pstZone->pstRI->nK);
    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }

#if (OP_SUPPORT_STATISTICS_INFO == 1)
    pstZone->pstStats->nActCompactCnt++;
#endif

    /* get log object pointer*/
    pstSrcLog = &(pstLogGrp->pstLogList[nSrcLogIdx]);
    pstDstLog = &(pstLogGrp->pstLogList[nDstLogIdx]);

    /* change the log state only if the destination log is inactive*/
    if (((pstDstLog->nState >> LS_ACTIVE_SHIFT) & 0x01) == 0)
    {
        nRet = FSR_STL_ReserveMetaPgs(pstZone, 1, TRUE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return nRet;
        }

        /* add the inactive log into active log list of context */
        FSR_STL_AddActLogList(pstZone, pstLogGrp->pstFm->nDgn, pstDstLog);

        /* store PMT+CXT*/
        nRet = FSR_STL_StorePMTCtx(pstZone, pstLogGrp, TRUE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return nRet;
        }
    }

    nRet = FSR_STL_ReserveMetaPgs(pstZone, 1, TRUE32);
    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }

    /* get #valid pgs in src log */
    pLogVPgCnt = pstLogGrp->pLogVPgCnt + (nSrcLogIdx << pstDev->nNumWaysShift);
    nRemainPgs = 0;
    for (nWay = 0; nWay < pstDev->nNumWays; nWay++)
    {
        nRemainPgs     = nRemainPgs + pLogVPgCnt[nWay];
        naWayIdx[nWay] = nWay;
    }

    nStartLpn = (pstLogGrp->pstFm->nDgn << (pstZone->pstRI->nNShift + pstDev->nPagesPerSbShift));

    FSR_OAM_MEMSET(ppstBMLCpBk, 0x00, sizeof(long) * FSR_MAX_WAYS);

    /* copy source log's pages to destination log */
    nWay = pstDstLog->nCPOffs & (pstDev->nNumWays - 1);
    while (nRemainPgs != 0)
    {
        while (naWayIdx[nWay] < nUnitPgs)
        {
            nLogIdx = (UINT8)(pstLogGrp->pMapTbl[naWayIdx[nWay]] >> pstDev->nPagesPerSbShift);

            if (nLogIdx == nSrcLogIdx)
            {
                nLpn = nStartLpn + naWayIdx[nWay];

                /* source page address*/
                nSrcPOffs = (POFFSET)((pstLogGrp->pMapTbl[naWayIdx[nWay]]) & (pstDev->nPagesPerSBlk - 1));

                /* destination page address*/
                nDstVpn = FSR_STL_GetLogCPN(pstZone, pstDstLog, nLpn);

                pstCpBkArg[nWay].nSrcVun        = (UINT16)(pstSrcLog->nVbn + n1stVun);
                pstCpBkArg[nWay].nSrcPgOffset   = (UINT16)(nSrcPOffs);
                pstCpBkArg[nWay].nDstVun        = (UINT16)(pstDstLog->nVbn + n1stVun);
                pstCpBkArg[nWay].nDstPgOffset   = (UINT16)(pstDstLog->nCPOffs);
                pstCpBkArg[nWay].nRndInCnt      =_CheckSpare4Copyback(pstZone, nLpn, nSrcPOffs, pstDstLog->nCPOffs, &(pstSBuf[nWay]));
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

                /* increase the clean page offset of the log*/
                pstDstLog->nCPOffs++;

                FSR_ASSERT(nRemainPgs > 0);
                nRemainPgs--;

                /* Reset PMT to reduce overhead,
                 * because Victim Log will be removed
                 */
                pstLogGrp->pMapTbl[naWayIdx[nWay]] = NULL_POFFSET;
                /* log group PMT update*/
                FSR_STL_UpdatePMT(pstZone, pstLogGrp, pstDstLog, nLpn, nDstVpn);

                /* change the state of log block*/
                FSR_STL_ChangeLogState(pstZone, pstLogGrp, pstDstLog, nLpn);

                naWayIdx[nWay] += pstDev->nNumWays;
                break;
            }

            naWayIdx[nWay] += pstDev->nNumWays;
        }

        nWay++;
        if (nWay >= pstDev->nNumWays)
        {
            nWay = 0;
        }

        if ((nWay == 0) || (nRemainPgs == 0))
        {
#if defined (FSR_POR_USING_LONGJMP)
            FSR_FOE_BeginWriteTransaction(0);
#endif
            nRet = FSR_BML_CopyBack(pstZone->nVolID, ppstBMLCpBk, FSR_BML_FLAG_NONE);
            if (nRet != FSR_BML_SUCCESS)
            {
                FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] %s(), L(%d) - FSR_BML_CopyBack (0x%x)\r\n"), __FSR_FUNC__, __LINE__, nRet));
                FSR_ASSERT(nRet != FSR_BML_VOLUME_NOT_OPENED);
                FSR_ASSERT(nRet != FSR_BML_INVALID_PARAM);
                FSR_ASSERT(nRet != FSR_BML_WR_PROTECT_ERROR);

                FSR_STL_LockPartition (pstZone->nClstID);
                return nRet;
            }

            if (nRemainPgs == 0)
            {
#if (OP_SUPPORT_MSB_PAGE_WAIT == 1)
                for (nIdx = 0; nIdx < pstDev->nNumWays; nIdx++)
                {
                    if (ppstBMLCpBk[nIdx] != NULL)
                    {
                        bIsLSB = FSR_STL_FlashIsLSBPage(pstZone, ppstBMLCpBk[nIdx]->nDstPgOffset);
                        pstClst->baMSBProg[nIdx] = !bIsLSB;
                    }
                    else
                    {
                        pstClst->baMSBProg[nIdx] = FALSE32;
                    }
                }
#endif
            }
            else
            {
                FSR_OAM_MEMSET(ppstBMLCpBk, 0x00, sizeof(long) * FSR_MAX_WAYS);
            }
        }

#if (OP_STL_DEBUG_CODE == 1)
        {
            UINT32 i;
            for (i = 0; i < pstDev->nNumWays; i++)
            {
                if (naWayIdx[i] < nUnitPgs)
                    break;
            }
            FSR_ASSERT((i < pstDev->nNumWays) ||
                       (nRemainPgs == 0))
        }
#endif
    }

    FSR_STL_UpdatePMTCost(pstZone, pstLogGrp);

    /* remove the source log from active log list, if it was active*/
    if ((pstSrcLog->nState & (1 << LS_ACTIVE_SHIFT)))
    {
        FSR_STL_RemoveActLogList(pstZone, pstLogGrp->pstFm->nDgn, pstSrcLog);
    }

    /*
     * at this point, there is no valid pages in source log
     * we have to insert the source log into free block list
     *  log block moves to free block list
     */
    FSR_STL_AddFreeList(pstZone, pstSrcLog->nVbn, pstSrcLog->nEC);

    /* remove the log from the group*/
    FSR_STL_RemoveLog(pstZone, pstLogGrp, pstSrcLog);

    /* check if the group is still merge victim group or not*/
    nRet = FSR_STL_IsMergeVictimGrp(pstZone, pstLogGrp);
    if (nRet == TRUE32)
    {
        nRet = FSR_STL_CheckMergeVictimGrp(pstZone, pstLogGrp, FALSE32, &bVictim);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return nRet;
        }

        if (bVictim == FALSE32)
        {
            /* clear the flag bit*/
            FSR_STL_SetMergeVictimGrp(pstZone, pstLogGrp->pstFm->nDgn, FALSE32);
        }
    }

    /* for sudden power-off failure*/
    pstCtx->pstFm->nMergedDgn = pstLogGrp->pstFm->nDgn;

    /* store PMT+CXT, update PMTDir*/
    nRet = FSR_STL_StorePMTCtx(pstZone, pstLogGrp, TRUE32);
    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}
