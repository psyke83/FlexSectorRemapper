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
 *  @file       FSR_STL_Delete.c
 *  @brief      Sector Delete() function is implemented in this file.
 *  @author     Wonmoon Cheon
 *  @date       02-MAR-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  02-MAR-2007 [Wonmoon Cheon] : first writing
 *  @n  02-JULY-2007 [Wonhee Cho]   : v1.2.0 re-design
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
#if (OP_SUPPORT_PAGE_DELETE == 1)

PRIVATE INT32   _StoreDelLogGrp         (STLZoneObj *pstZone);

#endif  /* (OP_SUPPORT_PAGE_DELETE == 1) */

/*****************************************************************************/
/* Local (static)  Function Definition                                       */
/*****************************************************************************/

#if (OP_SUPPORT_PAGE_DELETE == 1)

/** 
 *  @brief  This function stores deleted log group's information.
 *
 *  @param[in]  pstZone     : zone object
 *  @param[in]  pstDev      : device object
 *
 *  @return     FSR_STL_SUCCESS
 *
 *  @author     Wonmoon Cheon, Wonhee Cho
 *  @version    1.2.0 *
*/
PRIVATE INT32
_StoreDelLogGrp    (STLZoneObj     *pstZone)
{
    const   RBWDevInfo     *pstDev  = pstZone->pstDevInfo;
    STLCtxInfoHdl          *pstCtx;
    STLLogGrpHdl           *pstLogGrp;
    STLLogGrpHdl           *pstTempLogGrp;
    STLLog                 *pstLog;
    BADDR                   nDgn;
    UINT16                 *pLogVPgCnt;
    UINT32                  nVPgCnt;
    UINT32                  nIdx;
    BOOL32                  bRet;
    INT32                   nRet;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get context info object pointer */
    pstCtx = pstZone->pstCtxHdl;

    /* get deleted log group handler pointer */
    pstLogGrp = pstZone->pstDelCtxObj->pstDelLogGrpHdl;
    nDgn      = pstLogGrp->pstFm->nDgn;

    /* get minimum valid page count log */
    pstLog = pstLogGrp->pstLogList + pstLogGrp->pstFm->nMinVPgLogIdx;

    if ((pstZone->pstML->nMaxFreeSlots - pstCtx->pstFm->nNumFBlks) <
          pstLogGrp->pstFm->nNumLogs)
    {
        /* Reserve meta pages */
        nRet= FSR_STL_ReserveMetaPgs(pstZone, 1, TRUE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return nRet;
        }

        /* before entering ReverseGC, we have to store current deleted log group 
           (shared resource : meta page buffer) */
        nRet = FSR_STL_StorePMTCtx(pstZone, 
                                   pstLogGrp,
                                   TRUE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return nRet;
        }

        /*process the garbage blocks if it needs*/
        nRet = FSR_STL_ReverseGC(pstZone, pstZone->pstRI->nK);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return nRet;
        }
    }

    /* Get the number of valid pages in log blk */
    pLogVPgCnt = pstLogGrp->pLogVPgCnt + (pstLog->nSelfIdx << pstDev->nNumWaysShift);
    nVPgCnt    = 0;
    for (nIdx = 0; nIdx < pstDev->nNumWays; nIdx++)
    {
        nVPgCnt += pLogVPgCnt[nIdx];
    }

    /* Reserve meta pages */
    nRet= FSR_STL_ReserveMetaPgs(pstZone, 1, TRUE32);
    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }

    /* recalculate compaction cost */
    FSR_STL_UpdatePMTCost(pstZone, pstLogGrp);

    if ((nVPgCnt         == 0) &&
        (pstLog->nCPOffs == pstDev->nPagesPerSBlk))
    {
        /* add the log into free list */
        FSR_STL_AddFreeList(pstZone, pstLog->nVbn, pstLog->nEC);

        /* store changed mapping info & insert deleted blocks into free list. */
        /* this consumes only one meta page. no need to store BMT */
        nRet = FSR_STL_StoreMapInfo(pstZone, pstLogGrp, pstLog, FALSE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return nRet;
        }

        /* when all logs are moved into free list */
        if (pstLogGrp->pstFm->nNumLogs == 0)
        {
            pstTempLogGrp = FSR_STL_SearchLogGrp(pstZone->pstActLogGrpList, nDgn);
            if (pstTempLogGrp != NULL)
            {
#if (OP_SUPPORT_RUNTIME_PMT_BUILDING == 1)
                /* Execute Run time scanning */
                nRet = FSR_STL_ScanActLogBlock(pstZone,nDgn);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR, 
                        (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                    return nRet;
                }
#endif /*OP_SUPPORT_RUNTIME_PMT_BUILDING*/
                /* current active log group moves to inactive log group */
                FSR_STL_RemoveLogGrp(pstZone, pstZone->pstActLogGrpList, pstTempLogGrp);
            }
        }
        else
        {
            bRet = FSR_STL_CheckFullInaLogs(pstLogGrp);
            if (bRet == TRUE32)
            {
                pstTempLogGrp = FSR_STL_SearchLogGrp(pstZone->pstActLogGrpList, nDgn);
                if (pstTempLogGrp != NULL)
                {
#if (OP_SUPPORT_RUNTIME_PMT_BUILDING == 1)
                    /* Execute Run time scanning */
                    nRet = FSR_STL_ScanActLogBlock(pstZone,nDgn);
                    if (nRet != FSR_STL_SUCCESS)
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR, 
                            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                        return nRet;
                    }
#endif /*OP_SUPPORT_RUNTIME_PMT_BUILDING*/
                    /* current active log group moves to inactive log group */
                    FSR_STL_RemoveLogGrp(pstZone, pstZone->pstActLogGrpList, pstTempLogGrp);
                }
            }
        }

    }
    else    /* when there is no garbage logs */
    {

        /* store PMT+CXT, update PMTDir*/
        nRet = FSR_STL_StorePMTCtx(pstZone, pstLogGrp, TRUE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return nRet;
        }
    }

    /* if the deleted group exists in the inactive PMT cache, remove it.*/
    pstTempLogGrp = FSR_STL_SearchLogGrp(pstZone->pstInaLogGrpCache, nDgn);
    if (pstTempLogGrp != NULL)
    {
        /* remove the group from inactive log group cache */
        FSR_STL_RemoveLogGrp(pstZone, pstZone->pstInaLogGrpCache, pstTempLogGrp);
    }

    /* clear deleted group info */
    pstZone->pstDelCtxObj->nDelPrevDgn      = NULL_DGN;
    pstZone->pstDelCtxObj->pstDelLogGrpHdl  = NULL;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}

#endif /* (OP_SUPPORT_PAGE_DELETE == 1) */

/*****************************************************************************/
/* Global Function Definition                                                */
/*****************************************************************************/

#if (OP_SUPPORT_PAGE_DELETE == 1)

/** 
 *  @brief  This function stores deleted information.
 *
 *  @param[in]  pstZone    : zone object
 *
 *  @return     FSR_STL_SUCCESS
 *
 *  @author         Wonmoon Cheon
 *  @version        1.1.0 *
 */
PUBLIC INT32
FSR_STL_StoreDeletedInfo(STLZoneObj* pstZone)
{
    STLDelCtxObj   *pstDelCtxObj;
    INT32           nRet            = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    do
    {
        /* get del ctx obj pointer*/
        pstDelCtxObj = pstZone->pstDelCtxObj;
        if (pstDelCtxObj == NULL)
        {
            break;
        }

        /* Both BMT and PMT are stored in meta block.*/
        if (pstDelCtxObj->nDelPrevLan != NULL_DGN)
        {
            /* Reserve meta pages */
            nRet= FSR_STL_ReserveMetaPgs(pstZone, 1, TRUE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }

            /* store previous BMT*/
            nRet = FSR_STL_StoreBMTCtx(pstZone, TRUE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                    break;
            }

            pstDelCtxObj->nDelPrevLan  = NULL_DGN;
        }

        if ((pstDelCtxObj->pstDelLogGrpHdl != NULL    ) &&
            (pstDelCtxObj->nDelPrevDgn     != NULL_DGN))
        {
            /* store previous deleted log group information*/
            nRet = _StoreDelLogGrp(pstZone);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                    break;
            }
        }

        /* clear deleted page info */
        pstDelCtxObj->nDelLpn        = NULL_VPN;
        pstDelCtxObj->nDelSBitmap    = 0;
    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}

/** 
 *  @brief  This function deletes the specified sectors.
 *
 *  @param[in]  nClstID       : volume id
 *  @param[in]  nZoneID      : zone id
 *  @param[in]  nLsn         : start logical sector number (0 ~ (total sectors - 1))
 *  @param[in]  nSectors     : number of sectors to write
 *  @param[in]  nFlag        : option flag
 *
 *  @return     FSR_STL_SUCCESS
 *
 *  @author         Wonmoon Cheon
 *  @version        1.1.0 *
 */
PUBLIC INT32
FSR_STL_DeleteZone(UINT32 nClstID,
                   UINT32 nZoneID,
                   UINT32 nLsn,
                   UINT32 nNumOfScts,
                   UINT32 nFlag)
{
    /* get device info pointer*/
    RBWDevInfo     *pstDev              = NULL;
    STLClstObj     *pstClst             = NULL;
    STLZoneObj     *pstZone             = NULL;
    PADDR           nStartLpn           = 0;
    PADDR           nEndLpn             = 0;
    PADDR           nCurLpn             = 0;
    UINT32          nDelSectors         = 0;
    UINT32          nTotalDelSectors    = 0;
    UINT32          nDelSBitmap         = 0;
    BOOL32          bLogFound;
    BADDR           nLbn;
    INT32           nRet                = FSR_STL_SUCCESS;
    POFFSET         nDBOffs;
    POFFSET         nMetaPOffs;
    POFFSET         nPOffs;
    UINT8           nLogIdx;
    STLBMTHdl      *pstBMT              = NULL;
    STLDelCtxObj   *pstDelCtxObj        = NULL;
    STLLogGrpHdl   *pstDelLogGrp        = NULL;
    BADDR           nLan                = NULL_VBN;
    BADDR           nDgn;
    UINT16         *pLogVPgCnt;
    UINT32          nVPgCnt;
    UINT16         *pMinLogVPgCnt;
    UINT32          nMinVPgCnt;
    UINT32          nIdx;
    BOOL32          bLBlkDeleted;
#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    STLBUCtxObj    *pstBUCtx;
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* volume id check*/
    if (nClstID >= MAX_NUM_CLUSTERS)
    {
        nRet = FSR_STL_INVALID_PARAM;
        return nRet;
    }

    /* get volume object pointer*/
    pstClst = gpstSTLClstObj[nClstID];
    if (pstClst == NULL)
    {
        nRet = FSR_STL_INVALID_PARAM;
        return nRet;
    }

    /* nFlag is not touched, but there is compiler warning message */

    /* get zone object pointer*/
    pstZone             = &(pstClst->stZoneObj[nZoneID]);

    /* get device info pointer*/
    pstDev              = pstZone->pstDevInfo;

    /* get del log grp pointer*/
    pstDelLogGrp        = pstZone->pstDelCtxObj->pstDelLogGrpHdl;

    /* get del ctx obj pointer*/
    pstDelCtxObj    = pstZone->pstDelCtxObj;

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    pstBUCtx = pstZone->pstBUCtxObj;
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */

    /* zone id check*/
    if (nZoneID >= pstClst->stRootInfoBuf.stRootInfo.nNumZone)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR]      Invalid Parameter\r\n")));
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR]      Invalid Zone ID! (nZoneID:%d)\r\n"), nZoneID));
        return FSR_STL_INVALID_PARAM;
    }

    /* check request sector range*/
    if ((nNumOfScts == 0) ||
        (nLsn >= pstZone->pstZI->nNumUserScts) ||
        ((nLsn + nNumOfScts) > pstZone->pstZI->nNumUserScts))
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR]      Invalid Parameter\r\n")));
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] Invalid address error! (nLsn:%d, nNumOfScts:%d)\r\n"), nLsn, nNumOfScts));
        return FSR_STL_INVALID_PARAM;
    }

    /* start logical page address*/
    nStartLpn = nLsn >> pstDev->nSecPerVPgShift;

    /* end logical page address*/
    nEndLpn = (nLsn + nNumOfScts - 1) >> pstDev->nSecPerVPgShift;

    /* set starting LPN*/
    nCurLpn = nStartLpn;

    /* initialize VFL parameter (including extended param) */
    FSR_STL_InitVFLParamPool(gpstSTLClstObj[nClstID]);

    /* Reserve meta page */
    nRet = FSR_STL_ReserveMetaPgs(pstZone, 1, TRUE32);
    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }

    /*        S0 S1 S2 S3 S4 S5 S6 S7
     * [head]       |->--------------  : nStartLpn
     * [body] -----------------------
     * [body] -----------------------
     * [tail] --->|                    : nEndLpn
     */
    do
    {
        if (nCurLpn < nEndLpn)
        {
            nDelSectors = pstDev->nSecPerVPg - (nLsn & (pstDev->nSecPerVPg - 1));
        }
        else
        {
            nDelSectors = (nNumOfScts - nTotalDelSectors);
        }

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
        /* write the previous misaligned sectors into log block */
        if ((nCurLpn > pstBUCtx->nBufferedLpn || nEndLpn < pstBUCtx->nBufferedLpn)
            && (pstBUCtx->nBufState != STL_BUFSTATE_CLEAN))
        {
            /*
             * flush currently buffered single page into log block
             * after log write, there is not valid buffer page any more
             */
            nRet = FSR_STL_FlushBufferPage(pstZone);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                return nRet;
            }

            /* BU invalidation */
            STL_BUFFER_CTX_AFTER_INVALIDATE(pstBUCtx);
        }
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */

        if ((nCurLpn > nStartLpn) &&
            (nCurLpn < nEndLpn))
        {
            /* body pages bitmap*/
            nDelSBitmap = pstDev->nFullSBitmapPerVPg;

            /* clear deleted page info */
            pstDelCtxObj->nDelLpn  = NULL_VPN;
            pstDelCtxObj->nDelSBitmap = 0;
        }
        else
        {
            /* head, tail page BITMAP*/
            nDelSBitmap = FSR_STL_SetSBitmap(nLsn,
                                             nDelSectors,
                                             pstDev->nSecPerVPg);

            if (pstDelCtxObj->nDelLpn  == nCurLpn)
            {
                /* include previously deleted sectors*/
                nDelSBitmap |= pstDelCtxObj->nDelSBitmap;
            }

            /* set deleted page info for later FSR_STL_Delete() */
            pstDelCtxObj->nDelLpn       = nCurLpn;
            pstDelCtxObj->nDelSBitmap   = nDelSBitmap;
        }

        /* only if full sectors are deleted */
        if (nDelSBitmap == pstDev->nFullSBitmapPerVPg)
        {
            /* search if the LPN exists in log block or not*/
            bLogFound    = TRUE32;
            bLBlkDeleted = FALSE32;

            nLbn = (BADDR)(nCurLpn >> pstDev->nPagesPerSbShift);
            nDgn = nLbn >> NUM_DBLKS_PER_GRP_SHIFT;

#if (OP_SUPPORT_RUNTIME_PMT_BUILDING == 1)
            /* Execute Run time scanning */
            nRet = FSR_STL_ScanActLogBlock(pstZone,nDgn);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR, 
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
                return nRet;
            }
#endif /*OP_SUPPORT_RUNTIME_PMT_BUILDING*/
            /* store previous deleted info*/
            if ((pstDelCtxObj->nDelPrevDgn != nDgn) &&
                (pstDelLogGrp != NULL))
            {
                nRet = FSR_STL_StoreDeletedInfo(pstZone);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR, 
                        (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    return nRet;
                }
            }

            /*
             * only if the DGN is changed, 
             * we need to check if the group is active or inactive
             */
            if ((bLogFound == TRUE32) &&
                (pstDelCtxObj->nDelPrevDgn != nDgn))
            {
                /* case1) check if the target page is in active log or not*/
                pstDelLogGrp = FSR_STL_SearchLogGrp(pstZone->pstActLogGrpList, nDgn);
                if (pstDelLogGrp == NULL)
                {
                    /* case2) check if the target page is in inactive log or not*/
                    pstDelLogGrp = FSR_STL_SearchLogGrp(pstZone->pstInaLogGrpCache, nDgn);
                    if (pstDelLogGrp == NULL)
                    {
                        nRet = FSR_STL_SearchPMTDir(pstZone,
                                             nDgn,
                                             &nMetaPOffs);
                        if (nMetaPOffs != NULL_POFFSET)
                        {
                            /* load inactive group PMT by using meta page Buffer */
                            nRet = FSR_STL_LoadPMT(pstZone, 
                                                   nDgn, 
                                                   nMetaPOffs, 
                                                   &(pstDelLogGrp), 
                                                   FALSE32);
                            if (nRet != FSR_STL_SUCCESS)
                            {
                                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR, 
                                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                                        __FSR_FUNC__, __LINE__, nRet));
                                return nRet;
                            }
                        }
                        else
                        {
                            nRet = FSR_STL_SUCCESS;
                        }
                    }
                }
#if (OP_SUPPORT_RUNTIME_PMT_BUILDING == 1)
                else
                {
                    /* Execute Run time scanning */
                    nRet = FSR_STL_ScanActLogBlock(pstZone,nDgn);
                    if (nRet != FSR_STL_SUCCESS)
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR, 
                            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                        return nRet;
                    }
                }
#endif /*OP_SUPPORT_RUNTIME_PMT_BUILDING*/
            }

            /* get page offset in the group PMT*/
            nPOffs = (POFFSET)(nCurLpn & (pstZone->pstML->nPagesPerLGMT - 1));
            if ((pstDelLogGrp                  != NULL) &&
                (pstDelLogGrp->pMapTbl[nPOffs] != NULL_POFFSET))
            {
                FSR_ASSERT(pstDelLogGrp->pstFm->nDgn == nDgn);

                /* get current log index */
                nLogIdx = (UINT8)(pstDelLogGrp->pMapTbl[nPOffs] >> pstDev->nPagesPerSbShift);
                FSR_ASSERT(nLogIdx < pstZone->pstRI->nK);

                pLogVPgCnt = pstDelLogGrp->pLogVPgCnt + (nLogIdx << pstDev->nNumWaysShift);
                nVPgCnt = 0;
                for (nIdx = 0; nIdx < pstDev->nNumWays; nIdx++)
                {
                    nVPgCnt += pLogVPgCnt[nIdx];
                }

                if (pLogVPgCnt[nPOffs & (pstDev->nNumWays - 1)] > 0)
                {
                    /* decrease num. of valid pages in that log */
                    pLogVPgCnt[nPOffs & (pstDev->nNumWays - 1)] -= 1;
                    nVPgCnt--;

                    /* update min log index */
                    if (nLogIdx != pstDelLogGrp->pstFm->nMinVPgLogIdx)
                    {
                        pMinLogVPgCnt = pstDelLogGrp->pLogVPgCnt +
                                        ((pstDelLogGrp->pstFm->nMinVPgLogIdx) << pstDev->nNumWaysShift);
                        nMinVPgCnt = 0;
                        for (nIdx = 0; nIdx < pstDev->nNumWays; nIdx++)
                        {
                            nMinVPgCnt += pMinLogVPgCnt[nIdx];
                        }

                        if (nVPgCnt < nMinVPgCnt)
                        {
                            pstDelLogGrp->pstFm->nMinVPgLogIdx = (UINT16)nLogIdx;
                        }
                    }

                }

                /* set invalid(deleted) page mark in log block*/
                pstDelLogGrp->pMapTbl[nPOffs] = NULL_POFFSET;

                if ((nVPgCnt == 0) &&
                    (pstDelLogGrp->pstLogList[nLogIdx].nCPOffs == pstDev->nPagesPerSBlk))
                {
                    /* set block deleted flag */
                    bLBlkDeleted = TRUE32;
                }

                pstDelCtxObj->nDelPrevDgn       = nDgn;
                pstDelCtxObj->pstDelLogGrpHdl   = pstDelLogGrp;
            }

            nLan = (BADDR)(nLbn >> pstZone->pstZI->nDBlksPerLAShift);

            if (pstDelCtxObj->nDelPrevLan != nLan)
            {
                /* store previous BMT */
                if (pstDelCtxObj->nDelPrevLan != NULL_DGN)
                {
                    /* Reserve meta pages */
                    nRet= FSR_STL_ReserveMetaPgs(pstZone, 1, TRUE32);
                    if (nRet != FSR_STL_SUCCESS)
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                                __FSR_FUNC__, __LINE__, nRet));
                        return nRet;
                    }

                    nRet = FSR_STL_StoreBMTCtx(pstZone, 
                                               TRUE32);
                    if (nRet != FSR_STL_SUCCESS)
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR, 
                            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                                __FSR_FUNC__, __LINE__, nRet));
                        return nRet;
                    }
                }

                /* change active context*/
                nRet = FSR_STL_LoadBMT(pstZone, 
                                       nLan, 
                                       FALSE32);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR, 
                        (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    return nRet;
                }

            }

            /* get data block offset*/
            /* nDBOffs = (POFFSET)(nLbn % pstZone->pstZI->nDBlksPerLA); */
            nDBOffs = (POFFSET)(nLbn & (pstZone->pstZI->nDBlksPerLA - 1));

            /* get logical page offset*/
            /* nPOffs = (POFFSET)(nCurLpn % pstDev->nPagesPerSBlk); */
            nPOffs = (POFFSET)(nCurLpn & (pstDev->nPagesPerSBlk - 1));

            /* get BMT object pointer*/
            pstBMT = pstZone->pstBMTHdl;

            /* check if all log pages are deleted */
            if ((pstDelLogGrp != NULL) && 
                 bLBlkDeleted &&
                (pstBMT->pMapTbl[nDBOffs].nVbn != NULL_VBN))
            {
                /* increase number of idle blocks */
                if ( !(pstBMT->pGBlkFlags[nDBOffs >> 3] & (1 << (nDBOffs & 0x07))) )
                {
                    pstZone->pstCtxHdl->pstFm->nNumIBlks++;
                }
                
                /* set this data block to garbage for GC */
                pstBMT->pGBlkFlags[nDBOffs >> 3] |= (1 << (nDBOffs & 0x07));

                /* set previous LAN for storing it next */
                pstDelCtxObj->nDelPrevLan = nLan;
            }
        }

        /* set to the next page*/
        nCurLpn++;

        /* increase sector count and LSN*/
        nLsn += nDelSectors;
        nTotalDelSectors += nDelSectors;
    } while ( nCurLpn <= nEndLpn );

    /* only if one more pages are deleted*/
    if (nLan != NULL_VBN)
    {
        /* update GC scan bitmap*/
        nRet = FSR_STL_UpdateGCScanBitmap(pstZone, 
                                          nLan, 
                                          TRUE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return nRet;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}
#endif /* (OP_SUPPORT_PAGE_DELETE == 1) */
