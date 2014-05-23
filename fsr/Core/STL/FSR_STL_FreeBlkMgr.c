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
 *  @file       FSR_STL_FreeBlkMgr.c
 *  @brief      This file contains global data and utility functions for STL.
 *  @author     Wonmoon Cheon
 *  @date       02-MAR-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  02-MAR-2007 [Wonmoon Cheon] : first writing
 *  @n  29-JAN-2008 [MinYoung Kim] : dead code elimination
 *
 */

/*****************************************************************************/
/* Header file inclusions                                                    */
/*****************************************************************************/
#include "FSR.h"

#include "FSR_STL_Config.h"
#include "FSR_STL_CommonType.h"
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

PRIVATE INT32   _CompactInactiveLogGrps(STLZoneObj     *pstZone,
                                        const BADDR     nDgn,
                                        const UINT32    nRsrvNum);

PRIVATE INT32   _CompactActLogGrp      (STLZoneObj     *pstZone,
                                        STLLogGrpHdl   *pstLogGrp,
                                        BOOL32         bECConsider);

PRIVATE INT32   _CompactActiveLogGrps  (STLZoneObj     *pstZone,
                                        const UINT32    nRsrvNum);

/*****************************************************************************/
/* Local (static)  Function Definition                                       */
/*****************************************************************************/

/**
 * @brief        This function compacts inactive log group
 *
 * @param[in]   pstZone     : zone object
 * @param[in]   nDgn        : data group number to write
 * @param[in]   nRsrvNum    : # of free blks after this routine finishes
 *
 * @return      FSR_STL_SUCCESS
 *
 * @author      Jaesoo Lee
 * @version     1.2.0
 *
 */
PRIVATE INT32
_CompactInactiveLogGrps    (STLZoneObj     *pstZone,
                            const BADDR     nDgn,
                            const UINT32    nRsrvNum)
{
    /* Get context object pointer */
    STLCtxInfoHdl  *pstCtxInfo      = pstZone->pstCtxHdl;
    STLCtxInfoFm   *pstCtxFm        = pstCtxInfo->pstFm;
    /* Get directory header pointer */
    STLDirHdrHdl   *pstDirHdr       = pstZone->pstDirHdrHdl;
    STLLogGrpHdl   *pstTempLogGrp   = NULL;
    BOOL32          bFindVictim     = FALSE32;
    BADDR           nVictimGrpDgn   = NULL_DGN;
    UINT32          nVictimIdx      = 0;
    BOOL32          bCompact;
    UINT8           nSrcLogIdx;
    UINT8           nDstLogIdx;
    POFFSET         nMetaPOffs;
    INT32           nRet            = FSR_STL_SUCCESS;
#if (OP_STL_DEBUG_CODE == 1)
    INT32       nPrevNumFBlks   = 0;
#endif  /* (OP_STL_DEBUG_CODE == 1) */
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    while (pstCtxFm->nNumFBlks < nRsrvNum)
    {
        /* select a victim log group */
        bFindVictim = FSR_STL_SearchMergeVictimGrp(pstZone,
                                                  &nVictimGrpDgn,
                                                  &nVictimIdx);

        FSR_ASSERT((bFindVictim == FALSE32) ||
                   ((bFindVictim == TRUE32) && (nVictimGrpDgn != nDgn)));

        /*  if we success to find an inactive group, let's try to do compaction */
        if (bFindVictim == TRUE32)
        {
            FSR_ASSERT(nVictimGrpDgn != NULL_DGN);

            /*  check if compaction is possible or not */
            nRet = FSR_STL_CheckInaLogGrpCompaction(pstZone, 
                                                    pstDirHdr,
                                                    nVictimIdx,
                                                   &bCompact,
                                                   &nSrcLogIdx,
                                                   &nDstLogIdx);
            if (nRet != FSR_STL_SUCCESS)
            {
                break;
            }

            /*  ok to do compaction */
            if (bCompact == TRUE32)
            {
#if (OP_STL_DEBUG_CODE == 1)
                nPrevNumFBlks = pstCtxFm->nNumFBlks;
#endif  /* (OP_STL_DEBUG_CODE == 1) */

                /*  search if the DGN exists in the PMTDir */
                nRet = FSR_STL_SearchPMTDir(pstZone,
                                            nVictimGrpDgn,
                                           &nMetaPOffs);
                FSR_ASSERT(nMetaPOffs != NULL_POFFSET);

                /*
                 * destination log block should be active log, because
                 * we do not scan inactive log at open time.
                 * And also, to avoid soft-program problem, destination log
                 * should be active log.
                 */
                
                /*  we make the victim group be active temporarily for compaction */
                nRet = FSR_STL_MakeRoomForActLogGrpList(pstZone,
                                                        nDgn,
                                                        TRUE32);
                if (nRet != FSR_STL_SUCCESS)
                {
                    break;
                }

                /*  prepare new active log group */
                pstTempLogGrp = FSR_STL_NewActLogGrp(pstZone,
                                                     nVictimGrpDgn);
                if (pstTempLogGrp == NULL)
                {
                    nRet = FSR_STL_ERR_NEW_LOGGRP;
                    break;
                }
                
                /*  compact the victim log group */
                nRet = FSR_STL_CompactActLogGrp(pstZone,
                                                pstTempLogGrp,
                                                nSrcLogIdx,
                                                nDstLogIdx);
                if (nRet != FSR_STL_SUCCESS)
                {
                    break;
                }

#if (OP_STL_DEBUG_CODE == 1)
                FSR_ASSERT(pstCtxFm->nNumFBlks > nPrevNumFBlks);
#endif  /* (OP_STL_DEBUG_CODE == 1) */

            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}

/**
 * @brief       This function compacts active log group
 *
 * @param[in]   pstZone         : zone object
 * @param[in]   pstLogGrp       : Log group pointer which will be compacted
 * @param[in]   bECConsider       : Determines whether the ECC considered compaction is used or not.
 *
 * @return      FSR_STL_SUCCESS
 *
 * @author      Jaesoo Lee
 * @version     1.2.0
 *
 */
PRIVATE INT32
_CompactActLogGrp      (STLZoneObj     *pstZone,
                        STLLogGrpHdl   *pstLogGrp,
                        BOOL32         bECConsider)
{
    /* Get context object pointer */
    STLLog         *pstTmpLog       = NULL;
    BOOL32          bCompact;
    UINT8           nSrcLogIdx;
    UINT8           nDstLogIdx;
    INT32           nRet            = FSR_STL_SUCCESS;
#if ((OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) && (OP_SUPPORT_BU_DELAYED_FLUSH == 1))
    STLBUCtxObj    *pstBUCtx       = pstZone->pstBUCtxObj;
#endif
#if (OP_STL_DEBUG_CODE == 1)
    STLCtxInfoHdl  *pstCtxInfo      = pstZone->pstCtxHdl;
    INT32           nPrevNumFBlks;
#endif  /* (OP_STL_DEBUG_CODE == 1) */

    const UINT32    nWLThreshold    = FSR_STL_GetClstObj(pstZone->nClstID)->pstEnvVar->nECDiffThreshold;
    STLLog         *pstLog1;
    UINT8           nIdx1;
    UINT32          nMinEC;

    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

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

    /* 
    Check if there is a block which Erase Count exceeds allowed boundary.
    If the erase count of the block exceeds the boundary, it will be a victim of Free Block
    Wear-Leveling. So, Erase Count considered compaction scheme skips the compaction
    if it finds those blocks
        1) Calculate minumum EC value among all blocks
        2) Check if there is a block which Erase Count is more than allowed value
    */

    /* 1) Calculate minumum EC value among all blocks */    
    nMinEC = FSR_STL_FindMinEC(pstZone);

    /* nMinEC must be non-all F value, since this funtion is called 
       when merging logs in active log group is required. */
    FSR_ASSERT(nMinEC != 0xFFFFFFFF);

    /* 2) Check if there is a block which Erase Count is more than allowed value */
    if (bECConsider == TRUE32)
    {
        if (pstLogGrp->pstFm->nNumLogs > pstZone->pstRI->nN)
        {
            /* loop until we can find source/destination pair */
            nIdx1 = pstLogGrp->pstFm->nHeadIdx;

            while (nIdx1 != NULL_LOGIDX)
            {
                pstLog1 = &(pstLogGrp->pstLogList[nIdx1]);
                if (pstLog1->nEC - nMinEC > nWLThreshold)
                {
                    return FSR_STL_SUCCESS;
                }
                nIdx1 = pstLog1->nNextIdx;
            }
        }
    }

    /*  only if number of logs is greater than 'N' */
    while (pstLogGrp->pstFm->nNumLogs > pstZone->pstRI->nN)
    {
#if ((OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) && (OP_SUPPORT_BU_DELAYED_FLUSH == 1))
        /* reserve one free log page for the future flush operation */
        if ((pstBUCtx->nBufferedDgn == pstLogGrp->pstFm->nDgn) &&
            (pstBUCtx->nBufState    != STL_BUFSTATE_CLEAN))
        {
            nRet = FSR_STL_FlushBufferPage(pstZone);
            if (nRet != FSR_STL_SUCCESS)
            {
                break;
            }
        }
#endif /* ((OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) && (OP_SUPPORT_BU_DELAYED_FLUSH == 1)) */

        /* check if copy compaction is possible or not */
        bCompact = FSR_STL_CheckCopyCompaction(pstZone,
                                               pstLogGrp, 
                                              &nSrcLogIdx,
                                              &nDstLogIdx, 
                                               TRUE32);
#if (OP_STL_DEBUG_CODE == 1)
        nPrevNumFBlks = pstCtxInfo->pstFm->nNumFBlks;
#endif  /* (OP_STL_DEBUG_CODE == 1) */

        if (bCompact == TRUE32)
        {
            /*  after this routine, it is guaranteed that free block */
            /*  is generated. */
            nRet = FSR_STL_CompactActLogGrp(pstZone,
                                            pstLogGrp,
                                            nSrcLogIdx,
                                            nDstLogIdx);
        }
        else
        {
            /*  reduce number of logs of this group, luckily we may obtain */
            /*  a free block */
            nRet = FSR_STL_CompactLog(pstZone,
                                      pstLogGrp,
                                     &pstTmpLog);
        }

        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }

#if (OP_STL_DEBUG_CODE == 1)
        FSR_ASSERT(pstCtxInfo->pstFm->nNumFBlks > nPrevNumFBlks);
#endif  /* (OP_STL_DEBUG_CODE == 1) */

        break;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}

/**
 * @brief       This function compacts active log group
 *
 * @param[in]   pstZone     : zone object
 * @param[in]   nDgn        : data group number to write
 * @param[in]   nRsrvNum    : # of free blks after this routine finishes
 *
 * @return      FSR_STL_SUCCESS
 *
 * @author      Jaesoo Lee
 * @version     1.2.0
 *
 */
PRIVATE INT32
_CompactActiveLogGrps  (STLZoneObj     *pstZone,
                        const UINT32    nRsrvNum)
{
    const RBWDevInfo   *pstDev          = pstZone->pstDevInfo;
    STLCtxInfoFm       *pstCtxFm        = pstZone->pstCtxHdl->pstFm;
    STLLogGrpHdl       *pstTmpLogGrp;
    STLLog             *pstTmpLog;
    STLLogGrpHdl       *apstLogGrps[MAX_ACTIVE_LBLKS];
          UINT16        anValidPgs[MAX_ACTIVE_LBLKS];
          UINT32        nActLogs        = 0;
          UINT32        nLogs           = 0;
          UINT16        nTmpValidPgs;
          BOOL32        bSwapped;
          UINT16       *pLogVPgCnt;
          UINT32        nIdx;
          INT32         nRet            = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Initialize arrays */
    pstTmpLogGrp = pstZone->pstActLogGrpList->pstTail;
    nActLogs     = 0;
    while (pstTmpLogGrp != NULL)
    {
        if (pstTmpLogGrp->pstFm->nNumLogs > 1)
        {
            pLogVPgCnt = pstTmpLogGrp->pLogVPgCnt
                       + ((pstTmpLogGrp->pstFm->nMinVPgLogIdx) << pstDev->nNumWaysShift);
            anValidPgs [nActLogs] = 0;
            for (nIdx = 0; nIdx < pstDev->nNumWays; nIdx++)
            {
                anValidPgs[nActLogs] = anValidPgs[nActLogs] + pLogVPgCnt[nIdx];
            }
            apstLogGrps[nActLogs] = pstTmpLogGrp;
            nLogs += pstTmpLogGrp->pstFm->nNumLogs - 1;
            nActLogs++;
        }

        /*  let's look at the next group */
        pstTmpLogGrp = pstTmpLogGrp->pPrev;
    }

    /* If the number of extra log blk in the active log group is smaller than 3
       we take log blk form the inactive log group */
    if ((nActLogs == 0) ||
        (nLogs    <  3))
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
        return nRet;
    }

    /* We sort the array according to the vgpcnt (using bubble sort algorithm) */
    do
    {
        bSwapped = FALSE32;
        for(nIdx = 0; nIdx < nActLogs - 1; nIdx++)
        {
            pstTmpLogGrp = apstLogGrps[nIdx + 1];
            pstTmpLog    = &(pstTmpLogGrp->pstLogList[pstTmpLogGrp->pstFm->nMinVPgLogIdx]);
            if ((anValidPgs[nIdx + 1] == 0) && 
                (pstTmpLog->nCPOffs   >= pstDev->nPagesPerSBlk - pstDev->nNumWays - 1) &&
                (anValidPgs[nIdx + 1] <  anValidPgs[nIdx]))
            {
                /* swap them */
                nTmpValidPgs          = anValidPgs [nIdx];
                pstTmpLogGrp          = apstLogGrps[nIdx];

                anValidPgs [nIdx]     = anValidPgs [nIdx + 1];
                apstLogGrps[nIdx]     = apstLogGrps[nIdx + 1];

                anValidPgs [nIdx + 1] = nTmpValidPgs;
                apstLogGrps[nIdx + 1] = pstTmpLogGrp;

                bSwapped = TRUE32;
            }
        }
    } while (bSwapped);

    /*  check each log group in active log group list */
    for(nIdx = 0; nIdx < nActLogs; nIdx++)
    {
       if (pstCtxFm->nNumFBlks >= nRsrvNum)
       {
           break;
       }

        nRet = _CompactActLogGrp(pstZone, apstLogGrps[nIdx],TRUE32);        
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }
    }

    /* Execute compaction without considering Erase Count */
    for(nIdx = 0; nIdx < nActLogs; nIdx++)
    {
        if (pstCtxFm->nNumFBlks >= nRsrvNum)
        {
            break;
        }

        nRet = _CompactActLogGrp(pstZone, apstLogGrps[nIdx],FALSE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}

/**
 * @brief       This function compacts active log group
 *
 * @param[in]   pstZone     : zone object
 * @param[in]   nDgn        : data group number to write
 * @param[in]   nRsrvNum    : # of free blks after this routine finishes
 *
 * @return      FSR_STL_SUCCESS
 *
 * @author      Jaesoo Lee
 * @version     1.2.0
 *
 */
PRIVATE INT32
_MergeActiveLogGrps    (STLZoneObj     *pstZone,
                        const UINT32    nRsrvNum)
{
    STLCtxInfoFm       *pstCtxFm        = pstZone->pstCtxHdl->pstFm;
    STLLogGrpHdl       *pstLogGrp;
          INT32         nRet            = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    pstLogGrp = pstZone->pstActLogGrpList->pstTail;
    while ((pstCtxFm->nNumFBlks < nRsrvNum) &&
           (pstLogGrp != NULL))
    {
        /* check each log group in active log group list */
        if (pstLogGrp->pstFm->nNumLogs > 1)
        {
            nRet = _CompactActLogGrp(pstZone, pstLogGrp,FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  --%s() L(%d) : 0x%08x - _CompactActLogGrp returns error\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }
        }

        pstLogGrp = pstLogGrp->pPrev;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}

/**
 * @brief       This function compacts inactive log group
 *
 * @param[in]   pstZone     : zone object
 * @param[in]   nDgn        : data group number to write
 * @param[in]   nRsrvNum    : # of free blks after this routine finishes
 *
 * @return      FSR_STL_SUCCESS
 *
 * @author      Jaesoo Lee
 * @version     1.2.0
 *
 */
PRIVATE INT32
_MergeInactiveLogGrps  (STLZoneObj     *pstZone,
                        const BADDR     nDgn,
                        const UINT32    nRsrvNum)
{
    /* Get context object pointer */
    STLCtxInfoHdl  *pstCtxInfo      = pstZone->pstCtxHdl;
    STLCtxInfoFm   *pstCtxFm        = pstCtxInfo->pstFm;
    POFFSET         nMetaPOffs;
    STLLogGrpHdl   *pstLogGrp       = NULL;
    STLLog         *pstLog          = NULL;
    /* Get directory header pointer */
    UINT32          nDirIdx         = 0;
    BOOL32          bFindVictim     = FALSE32;
    BADDR           nVictimGrpDgn   = NULL_DGN;
    INT32           nRet            = FSR_STL_SUCCESS;
#if (OP_STL_DEBUG_CODE == 1)
    INT32           nPrevNumFBlks   = 0;
#endif  /* (OP_STL_DEBUG_CODE == 1) */

    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    while (pstCtxFm->nNumFBlks < nRsrvNum)
    {
        /* select a victim log group */
        bFindVictim = FSR_STL_SearchMergeVictimGrp(pstZone,
                                                  &nVictimGrpDgn,
                                                  &nDirIdx);

        FSR_ASSERT((bFindVictim == FALSE32) ||
                   ((bFindVictim == TRUE32) && (nVictimGrpDgn != nDgn)));

        if (bFindVictim == TRUE32)
        {
            FSR_ASSERT(nVictimGrpDgn != NULL_DGN);

#if (OP_STL_DEBUG_CODE == 1)
            nPrevNumFBlks = pstCtxFm->nNumFBlks;
#endif  /* (OP_STL_DEBUG_CODE == 1) */

            /* if the victim group exists in the inactive PMT cache, remove it.*/
            pstLogGrp = FSR_STL_SearchLogGrp(pstZone->pstInaLogGrpCache, nVictimGrpDgn);
            if (pstLogGrp == NULL)
            {
                nRet = FSR_STL_SearchPMTDir(pstZone, nVictimGrpDgn, &nMetaPOffs);
                FSR_ASSERT(nMetaPOffs != NULL_POFFSET);

                /* load PMT*/
                nRet = FSR_STL_LoadPMT(pstZone, nVictimGrpDgn, nMetaPOffs, &pstLogGrp, FALSE32);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - FSR_STL_LoadPMT returns error\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    break;
                }
            }

            FSR_ASSERT(pstLogGrp->pstFm->nNumLogs <= pstZone->pstRI->nK);
            FSR_ASSERT(pstLogGrp->pstFm->nNumLogs > 1);

            nRet = FSR_STL_CompactLog(pstZone, pstLogGrp, &pstLog);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - FSR_STL_CompactLog returns error\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }

#if (OP_STL_DEBUG_CODE == 1)
            FSR_ASSERT(nPrevNumFBlks != pstCtxFm->nNumFBlks);
#endif  /* (OP_STL_DEBUG_CODE == 1) */

        }
        else
        {
            break;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}


/*****************************************************************************/
/* Global Function Definition                                                */
/*****************************************************************************/
/** 
 *  @brief      This function returns the status of the free list whether it is full or not.
 *
 *  @param[in]  pstZone      : zone object
 *
 *  @return     TRUE32       : Full
 *  @return     FALSE32      : Not Full
 *
 *  @author     Wonmoon Cheon, Wonhee Cho
 *  @version    1.2.0 *
 */
PUBLIC BOOL32
FSR_STL_CheckFullFreeList(STLZoneObj   *pstZone)
{
    /* get the number of maximum free blocks */
    const UINT32    nTotalFBlks = pstZone->pstML->nMaxFreeSlots;
    /* get the number of current free blocks */
    const UINT32    nCurFBlks   = pstZone->pstCtxHdl->pstFm->nNumFBlks;
    BOOL32          bRet;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* current free blks are full? */
    bRet = (nCurFBlks < nTotalFBlks) ? FALSE32 : TRUE32;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, bRet));
    return bRet;
}


/** 
 *  @brief      This function function inserts the specified VBN into free block pool.
 *
 *  @param[in]  pstZone  : partition object
 *  @param[in]  nVbn     : VBN to insert in free block 
 *  @param[in]  nEC      : erase count of nVbn block
 *
 *  @return     none
 *
 *  @author     Wonmoon Cheon, Wonhee Cho
 *  @version    1.2.0 *
 */
PUBLIC VOID
FSR_STL_AddFreeList(STLZoneObj *pstZone,
                    BADDR       nVbn,
                    UINT32      nEC)
{
    /* get context info pointer*/
    STLCtxInfoHdl  *pstCtxInfo      = pstZone->pstCtxHdl;
    STLCtxInfoFm   *pstCtxFm        = pstCtxInfo->pstFm;
    /* get the maximum number of free blocks.*/
    const UINT32    nTotalFBlks     = pstZone->pstML->nMaxFreeSlots;
    UINT32          nIdx;

#if (OP_STL_DEBUG_CODE == 1)
    BADDR           nListIdx;
#endif

#if (OP_SUPPORT_SORTED_FREE_BLOCK_LIST == 1)
    UINT16          nTgtIdx;
    UINT16          nSrcIdx;
    UINT16          nInsIdx;
    UINT32         *pFBlksEC     = pstCtxInfo->pFBlksEC;
    BADDR          *pFreeList    = pstCtxInfo->pFreeList;
#endif

    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));
    FSR_ASSERT(nVbn != NULL_VBN);

#if (OP_STL_DEBUG_CODE == 1)
    nListIdx = pstCtxFm->nFreeListHead;
    for (nIdx = 0; nIdx < pstCtxFm->nNumFBlks; nIdx++)
    {
        FSR_ASSERT(pstCtxInfo->pFreeList[nListIdx] != nVbn);
        nListIdx++;
        if (nListIdx >= nTotalFBlks)
        {
            FSR_ASSERT(nListIdx < (nTotalFBlks << 1));
            nListIdx = (BADDR)(nListIdx - nTotalFBlks);
        }
    }
#endif  /* (OP_STL_DEBUG_CODE == 1) */

    FSR_ASSERT(pstCtxFm->nNumFBlks < nTotalFBlks);

#if (OP_SUPPORT_SORTED_FREE_BLOCK_LIST == 1)
    /* Sort Free List with EC*/

    /* Search index to insert new elements */
    nInsIdx = pstCtxFm->nFreeListHead;
    for (nIdx = 0; nIdx < pstCtxFm->nNumFBlks; nIdx++)
    {
        /* For Optimization */
        if(((nIdx * 2) < pstCtxFm->nNumFBlks) && (pFBlksEC[nInsIdx] >= nEC))
        {
            break;
        }
        else if(pFBlksEC[nInsIdx] > nEC)
        {
            break;
        }

        nInsIdx ++;
        if (nInsIdx >= nTotalFBlks)
        {
            FSR_ASSERT(nInsIdx < (nTotalFBlks << 1));
            nInsIdx = (UINT16)(nInsIdx - nTotalFBlks);
        }
    }
    
    /* Confirm whether the number of left-side elements are smaller than that of right-side elements. For Optimization */
    if((nIdx * 2) < pstCtxFm->nNumFBlks)
    {
        /* Move elements from listhead ~ index elements to listhead-1 ~ index-1 elements  to reserve the space where new element at the index is inserted  */
        nTgtIdx = (UINT16) ((pstCtxFm->nFreeListHead != 0)? (pstCtxFm->nFreeListHead - 1) : (nTotalFBlks-1));
        
        /* Iterate over the number of the moved elements
           nIdx = the number of elements to be copied*/
        FSR_ASSERT(pFreeList[nTgtIdx] == NULL_VBN);
        for(; nIdx > 0 ; nIdx --)
        {
            /* Because Free Block List is circular list using array, previous index is not -1, but the size of list - 1 when index = 0  */
            nSrcIdx = (UINT16) ((nTgtIdx + 1) % nTotalFBlks);

            /* Copy the element at nSrcIdx to the elements at nTgtIdx */
            pFreeList[nTgtIdx] = pFreeList[nSrcIdx];
#if (OP_SUPPORT_DATA_WEAR_LEVELING == 1)
            pstCtxInfo->pFBlksEC[nTgtIdx]  = pstCtxInfo->pFBlksEC[nSrcIdx];
#endif

            nTgtIdx++;
            if (nTgtIdx >= nTotalFBlks)
            {
                FSR_ASSERT(nTgtIdx < (nTotalFBlks << 1));
                nTgtIdx = (UINT16)(nTgtIdx - nTotalFBlks);
            }
        }
        
        /* Because we copied head-side elements to previous index, 
           list head will be also moved and nInsIdx related to list head will be  */
        pstCtxFm->nFreeListHead  = (pstCtxFm->nFreeListHead != 0) ? 
                                   (pstCtxFm->nFreeListHead - 1) : ((UINT16)(nTotalFBlks - 1));
        nInsIdx                  = (nInsIdx != 0) ? 
                                   (nInsIdx - 1) : ((UINT16)(nTotalFBlks - 1));
    }
    else 
    /* ((nIdx * 2) >= pstCtxFm->nNumFBlks) */
    {
        /* Move from tail ~ index  elements to tail + 1 ~ index+1 elements to insert new element at the index */
        nTgtIdx = (UINT16) ((pstCtxFm->nFreeListHead + pstCtxFm->nNumFBlks) % nTotalFBlks); /* Tail Idx + 1 */	
        
        FSR_ASSERT(pFreeList[nTgtIdx] == NULL_VBN);
        /* Iterate over the number of the moved elements */
        for(nIdx = pstCtxFm->nNumFBlks - nIdx ; nIdx > 0 ;nIdx--)
        {
            /* Because Free Block List is circular list using array, previous index is not -1, but the size of list - 1 when index = 0  */
            nSrcIdx = (UINT16) ((nTgtIdx!=0)?(nTgtIdx - 1):(nTotalFBlks - 1));

            /* Copy the elements at nSrcIdx to the element at nTgtIdx*/
            pFreeList[nTgtIdx] = pFreeList[nSrcIdx];
#if (OP_SUPPORT_DATA_WEAR_LEVELING == 1)
            pstCtxInfo->pFBlksEC[nTgtIdx]  = pstCtxInfo->pFBlksEC[nSrcIdx];
#endif

            nTgtIdx = (nTgtIdx != 0)?(nTgtIdx - 1):((UINT16)(nTotalFBlks - 1));
        }
    } /* ((nIdx * 2) < pstCtxFm->nNumFBlks) */

    /* Insert new element*/
    pFreeList[nInsIdx] = nVbn;
#if (OP_SUPPORT_DATA_WEAR_LEVELING == 1)
    pstCtxInfo->pFBlksEC[nInsIdx]  = nEC;
#endif

#else
    nIdx = pstCtxFm->nFreeListHead + pstCtxFm->nNumFBlks;
    if (nIdx >= nTotalFBlks)
    {
        nIdx -= nTotalFBlks;
    }
    FSR_ASSERT(nIdx <  nTotalFBlks);

    pstCtxInfo->pFreeList[nIdx] = nVbn;
#if (OP_SUPPORT_DATA_WEAR_LEVELING == 1)
    pstCtxInfo->pFBlksEC[nIdx]  = nEC;
#endif

#endif

    /* increase the number of free blocks */
    pstCtxFm->nNumFBlks++;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}

/** 
 *  @brief      This function produces free blocks until the number of free blks
 *  @\n         become nNumRsvdFBlks.
 *
 *  @param[in]  pstZone         : partition object
 *  @param[in]  nDgn            : data group number to write
 *  @param[in]  nNumRsvdFBlks   : # of free blks after this routine finishes
 *  @param[out] pnNumRsvd       : # of free blks actually reserved
 *
 *  @return     FSR_STL_SUCCESS
 *
 *  @author     Wonmoon Cheon, Jaesoo Lee, Wonhee Cho
 *  @version    1.2.0 *
 */

PUBLIC INT32
FSR_STL_ReserveFreeBlks    (STLZoneObj *pstZone,
                            BADDR       nDgn,
                            UINT32      nNumRsvdFBlks,
                            UINT32     *pnNumRsvd)
{
    /* Get context object pointer */
    STLCtxInfoHdl  *pstCtxInfo      = pstZone->pstCtxHdl;
    STLCtxInfoFm   *pstCtxFm        = pstCtxInfo->pstFm;
    INT32           nRet            = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));
    FSR_ASSERT(nNumRsvdFBlks <= pstZone->pstML->nMaxFreeSlots);

    while (pstCtxFm->nNumFBlks < nNumRsvdFBlks)
    {
#if (OP_SUPPORT_PAGE_DELETE == 1)
        nRet = FSR_STL_GC(pstZone);
        if ((nRet != FSR_STL_SUCCESS) ||
            (pstCtxFm->nNumFBlks >= nNumRsvdFBlks))
        {
            break;
        }
#endif /* (OP_SUPPORT_PAGE_DELETE == 1) */

        /* ---------------------------------------------------------------- */
        /* [STEP1] : TRY TO GET FREE BLOCK BY COMPACTION OF                 */
        /*                                              INACTIVE LOG GROUP. */
        /* ---------------------------------------------------------------- */
        nRet = _CompactInactiveLogGrps(pstZone, nDgn, nNumRsvdFBlks);
        if ((nRet != FSR_STL_SUCCESS) ||
            (pstCtxFm->nNumFBlks >= nNumRsvdFBlks))
        {
            break;
        }

        /* ---------------------------------------------------------------- */
        /*  [STEP2] : WHEN STEP1 FAILS,                                     */
        /*         TRY TO GET FREE BLOCK BY COMPACTION OF ACTIVE LOG GROUP. */
        /* ---------------------------------------------------------------- */
        nRet = _CompactActiveLogGrps(pstZone, nNumRsvdFBlks);
        if ((nRet != FSR_STL_SUCCESS) ||
            (pstCtxFm->nNumFBlks >= nNumRsvdFBlks))
        {
            break;
        }

        /*  --------------------------------------------------------------- */
        /*  [STEP3] : WHEN STEP2 FAILS,                                     */
        /*            TRY TO GET FREE BLOCK BY MERGE OF INACTIVE LOG GROUP. */
        /*  --------------------------------------------------------------- */
        nRet = _MergeInactiveLogGrps(pstZone, nDgn, nNumRsvdFBlks);
        if ((nRet != FSR_STL_SUCCESS) ||
            (pstCtxFm->nNumFBlks >= nNumRsvdFBlks))
        {
            break;
        }

        /*  --------------------------------------------------------------- */
        /*  [STEP4] : WHEN STEP3 FAILS,                                     */
        /*              TRY TO GET FREE BLOCK BY MERGE OF ACTIVE LOG GROUP. */
        /*  --------------------------------------------------------------- */
        nRet = _MergeActiveLogGrps(pstZone, nNumRsvdFBlks);
        if ((nRet != FSR_STL_SUCCESS) ||
            (pstCtxFm->nNumFBlks >= nNumRsvdFBlks))
        {
            break;
        }

        /*  to avoid the infinite loop, but it can not happen!! */
        if (pstCtxFm->nNumLBlks == 0) 
        {
            nRet = FSR_STL_CRITICAL_ERROR;
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR]  --%s() L(%d) : 0x%08x - There are no free units.\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));

            FSR_ASSERT(pstCtxFm->nNumFBlks < nNumRsvdFBlks);
            break;
        }
    }

    if (pnNumRsvd != NULL)
    {
        *pnNumRsvd = pstCtxFm->nNumFBlks;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}

/**
 *  @brief          This function reserves specified number of YOUNG free blks. 
 *
 *  @param[in]      pstZone         : partition object
 *  @param[in]      nDgn            : data group number to write
 *  @param[in]      nNumRsvdYFBlks  : # of free blks after this routine finishes
 *  @param[out]    *pnNumRsvd      : # of YOUNG free blks actually reserved
 *
 *  @return         FSR_STL_SUCCESS
 *
 *  @author         Jaesoo Lee
 *  @version        1.2.0 *
 */
PUBLIC INT32
FSR_STL_ReserveYFreeBlks   (STLZoneObj *pstZone,
                            BADDR       nDgn,
                            UINT32      nNumRsvdFBlks,
                            UINT32     *pnNumRsvd)
{
    UINT32  nNumRsvdFree;
    UINT32  nNumCurCFree;
    INT32   nRet            = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));
    FSR_ASSERT(nNumRsvdFBlks > 0);

    nNumCurCFree = 0;
    while(nNumCurCFree < nNumRsvdFBlks)
    {
        nNumCurCFree++;

        /* Get nNumCurCFree free blocks */
        nRet = FSR_STL_ReserveFreeBlks(pstZone,
                                       nDgn,
                                       nNumCurCFree,
                                      &nNumRsvdFree);
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }
        FSR_ASSERT(nNumRsvdFree >= nNumCurCFree);

        /* Adjust the number of blocks which will be wear-level */
        nNumCurCFree = (nNumRsvdFree > nNumRsvdFBlks) ? nNumRsvdFBlks : nNumRsvdFree;

#if (OP_SUPPORT_DATA_WEAR_LEVELING == 1)

        nRet = FSR_STL_WearLevelFreeBlk(pstZone,
                                        nNumCurCFree,
                                        NULL_DGN);
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }

#endif /* (OP_SUPPORT_DATA_WEAR_LEVELING == 1) */

    }

    if (pnNumRsvd != NULL)
    {
        *pnNumRsvd = nNumCurCFree;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}

/** 
 *  @brief      This function returns the free VBN in free block pool.
 *
 *  @param[in]      pstZone         : partition object
 *  @param[out]     pnVbn           : free block VBN
 *  @param[out]     pnEC            : free block erase count
 *
 *  @return         FSR_STL_SUCCESS
 *  @return         FSR_STL_ERR_PHYSICAL
 *
 *  @author         Wonmoon Cheon
 *  @version        1.1.0 *
*/
PUBLIC INT32
FSR_STL_GetFreeBlk (STLZoneObj *pstZone,
                    BADDR      *pnVbn,
                    UINT32     *pnEC)
{
    STLCtxInfoHdl  *pstCtxInfo  = pstZone->pstCtxHdl;
    STLCtxInfoFm   *pstCtxFm    = pstCtxInfo->pstFm;
    const UINT32    nMaxFBlk    = pstZone->pstML->nMaxFreeSlots;
    BADDR           nVbn        = NULL_VBN;
    INT32           nRet;
#if (OP_STL_DEBUG_CODE == 1)
    STLBMTHdl         *pstBMT;
    INT32           i;
    UINT32          j;
    UINT32          nIdx;
    BADDR           nVbnI;
    BADDR           nVbnJ;
#endif
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));
    FSR_ASSERT(pstCtxFm->nNumFBlks > 0);

    do
    {
        nVbn = pstCtxInfo->pFreeList[pstCtxFm->nFreeListHead];
        FSR_ASSERT(nVbn != NULL_VBN);

        /* Erase the free block to use...*/
        nRet = FSR_STL_FlashErase(pstZone, nVbn);
        if (nRet != FSR_BML_SUCCESS)
        {
            break;
        }

#if (OP_SUPPORT_DATA_WEAR_LEVELING == 1)
        if (pstCtxInfo->pFBlksEC[pstCtxFm->nFreeListHead] < 0xFFFFFFFF)
        {
            pstCtxInfo->pFBlksEC[pstCtxFm->nFreeListHead]++;
        }
        *pnEC = pstCtxInfo->pFBlksEC[pstCtxFm->nFreeListHead];
#endif /* (OP_SUPPORT_DATA_WEAR_LEVELING == 1) */

        pstCtxInfo->pFreeList[pstCtxFm->nFreeListHead] = NULL_VBN;

        pstCtxFm->nFreeListHead++;
        if (pstCtxFm->nFreeListHead >= nMaxFBlk)
        {
            pstCtxFm->nFreeListHead = (UINT16)(pstCtxFm->nFreeListHead - (nMaxFBlk));
        }
        FSR_ASSERT(pstCtxFm->nFreeListHead < nMaxFBlk);
        pstCtxFm->nNumFBlks--;

        *pnVbn = nVbn;

#if (OP_STL_DEBUG_CODE == 1)
        /* debug code*/
        for (i = 0; i < pstCtxFm->nNumFBlks; i++)
        {
            nIdx    = (pstCtxFm->nFreeListHead + i);
            if (nIdx >= nMaxFBlk)
            {
                nIdx -= nMaxFBlk;
            }
            FSR_ASSERT(nIdx < nMaxFBlk);
            nVbnI   = pstCtxInfo->pFreeList[nIdx];
            pstBMT  = pstZone->pstBMTHdl;

            for (j = 0; j < pstZone->pstZI->nDBlksPerLA; j++)
            {
                nVbnJ = pstBMT->pMapTbl[j].nVbn;

                if (nVbnI == nVbnJ)
                {
                    FSR_ASSERT(FALSE32);
                }
            }
        }
#endif /* (OP_STL_DEUG_CODE == 1)*/

        nRet = FSR_STL_SUCCESS;
    } while(0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}
