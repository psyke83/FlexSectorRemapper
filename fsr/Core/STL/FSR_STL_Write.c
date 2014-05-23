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
 *  @file       FSR_STL_Write.c
 *  @brief      Sector write API is implemented.
 *  @author     Wonmoon Cheon
 *  @date       02-MAR-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  02-MAR-2007 [Wonmoon Cheon] : first writing
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

PRIVATE INT32   _MakeRoomForActiveSlot (STLZoneObj     *pstZone,
                                        STLCtxInfoHdl  *pstCtx,
                                        STLLogGrpHdl   *pstActLogGrp);

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 0)
PRIVATE INT32   _WriteLogSinglePage    (STLZoneObj     *pstZone,
                                        STLLogGrpHdl   *pstLogGrp,
                                        STLLog         *pstLog,
                                        VFLParam       *pstParam,
                                        BOOL32          bConfirmPage);
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 0) */

PRIVATE INT32   _WriteLogMultiPages    (STLZoneObj     *pstZone,
                                        STLLogGrpHdl   *pstLogGrp,
                                        STLLog         *pstLog,
                                        VFLParam       *pstParam,
                                        UINT32         *pnWrPages);

PRIVATE VOID    _CopyVFLParamBuffer    (STLZoneObj *pstZone,
                                        VFLParam   *pstSrcParam,
                                        VFLParam   *pstDstParam);

/*****************************************************************************/
/* Local (static)  Function Definition                                       */
/*****************************************************************************/

/**
 * @brief       This function makes active log slot in active list.
 *
 * @param[in]   pstZone         : zone object
 * @param[in]   pstCtx          : context object 
 * @param[in]   pstActLogGrp    : active log group object pointer
 *
 * @return      none
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PRIVATE INT32 
_MakeRoomForActiveSlot (STLZoneObj     *pstZone,
                        STLCtxInfoHdl  *pstCtx,
                        STLLogGrpHdl   *pstActLogGrp)
{
    STLLog     *pstVictimLog    = NULL;
    BADDR       nSelfDgn;
    INT32       nRet            = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /*
     * check if there is empty slot in active log list of context
     * It is guaranteed that there is at least one act lblk in this lgrp.
     * Thus, we make one active lblk inactive. 
     */

    /* 1) select victim log to be inactive */
    pstVictimLog = FSR_STL_SelectVictimLog(pstActLogGrp, TRUE32);

    if (pstVictimLog != NULL)
    {
        /* if we can find an active log in itself */

        /* 2) change log state to inactive */
        pstVictimLog->nState &= ~(1 << LS_ACTIVE_SHIFT);

        /* 3) remove the victim log from active log list */
        FSR_STL_RemoveActLogList(pstZone,
                                 pstActLogGrp->pstFm->nDgn,
                                 pstVictimLog);
        FSR_ASSERT(pstCtx->pstFm->nNumLBlks < MAX_ACTIVE_LBLKS);
    }
    else if (pstCtx->pstFm->nNumLBlks >= MAX_ACTIVE_LBLKS)
    {
        /*
         * when we fail to inactivate a log in its group,
         * try to do in other groups 
         */

        /** 
         * if all active logs are included in one same group,
         * there is no victim group except itself.
         */
        if ((pstActLogGrp->pstFm->nNumLogs > 1) &&
            (pstZone->pstActLogGrpList->nNumLogGrps == 1))
        {
            nSelfDgn = NULL_DGN;
        }
        else
        {
            nSelfDgn = pstActLogGrp->pstFm->nDgn;
        }

        /* prepare to make new active log group */
        nRet = FSR_STL_MakeRoomForActLogGrpList(pstZone, nSelfDgn, FALSE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG| FSR_DBZ_ERROR, 
                (TEXT("[SIF:ERR]  --%s() L(%d): 0x%8x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%8x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 0)
/**
 * @brief       This function writes one page in the specified log block.
 *
 * @param[in]   pstZone             : zone object
 * @param[in]   pstLogGrp           : log group 
 * @param[in]   pstLog              : log
 * @param[in]   pstParam            : VFLParam object
 * @param[in]   bConfirmPage        : flag that denotes the current page is the confirm age or not.
 *
 * @return      FSR_STL_SUCCESS
 *
 * @author      Wonmoon Cheon, Jaesoo Lee
 * @version     1.1.0
 *
 */
PRIVATE INT32
_WriteLogSinglePage    (STLZoneObj     *pstZone,
                        STLLogGrpHdl   *pstLogGrp,
                        STLLog         *pstLog,
                        VFLParam       *pstParam,
                        BOOL32          bConfirmPage)
{
    const RBWDevInfo   *pstDev          = pstZone->pstDevInfo;
    const UINT32        nSctPerVPg      = pstDev->nSecPerVPg;
    const UINT32        nFBitmap        = pstDev->nFullSBitmapPerVPg;
          PADDR         nVpn;
          PADDR         nLpn;
          PADDR         nSrcVpn         = NULL_VPN;
          BOOL32        bPageDeleted    = TRUE32;
          INT32         nRet;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    FSR_ASSERT(pstLogGrp->pstFm->nDgn != NULL_DGN);
    FSR_ASSERT(pstLog->nVbn != NULL_VBN);
    FSR_ASSERT(pstParam->nNumOfPgs == 1);

    nLpn = pstParam->nLpn;
    FSR_ASSERT(nLpn >= (PADDR)(((pstLogGrp->pstFm->nDgn    ) << pstZone->pstRI->nNShift) << pstDev->nPagesPerSbShift));
    FSR_ASSERT(nLpn <  (PADDR)(((pstLogGrp->pstFm->nDgn + 1) << pstZone->pstRI->nNShift) << pstDev->nPagesPerSbShift));

    /* get clean page address of the log */
    nVpn = FSR_STL_GetLogCPN(pstZone, pstLog, nLpn);

    /* increase the clean page offset of the log */
    pstLog->nCPOffs++;

#if (OP_SUPPORT_MLC_LSB_ONLY == 1)
    FSR_STL_CheckMLCFastMode(pstZone, pstLog);
#endif

    if ((pstParam->nBitmap & nFBitmap) != nFBitmap)
    {
        /* find out the old source VPN */
        nRet = FSR_STL_GetLpn2Vpn(pstZone, nLpn, &nSrcVpn, &bPageDeleted);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
            return nRet;
        }
    }
    else
    {
        /*
         * It has same effect with the case when the page is deleted
         * So, we set the bPageDelete to TRUE for convenience.
         */
        bPageDeleted = TRUE32;
    }

    /* set spare data 1, 2, 3 */
    FSR_STL_SetLogSData123(pstZone, nLpn, nVpn, pstParam, bConfirmPage);

    if ((bPageDeleted == TRUE32) ||
        ((pstParam->nBitmap & nFBitmap) == nFBitmap))
    {
        if (bConfirmPage)
        {
            /* Initialize CRC */
            FSR_STL_InitCRCs(pstZone, pstParam);
            FSR_STL_ComputeCRCs(pstZone, NULL, pstParam, FALSE32);
            pstParam->pExtParam->nNumExtSData = (UINT16)nSctPerVPg;
        }

        nRet = FSR_STL_Convert_ExtParam(pstZone, pstParam);
        nRet = FSR_STL_FlashProgram(pstZone, nVpn, pstParam);
    }
    else
    {
        if (bConfirmPage)
        {
            /* Initialize CRC */
            FSR_STL_InitCRCs(pstZone, pstParam);
            nRet = FSR_STL_FlashModiCopybackCRC(pstZone, nSrcVpn, nVpn, pstParam);
        }
        else
        {
            pstParam->pExtParam->nNumExtSData = 0;
            nRet = FSR_STL_Convert_ExtParam(pstZone, pstParam);
            nRet = FSR_STL_FlashModiCopyback(pstZone, nSrcVpn, nVpn, pstParam);

        }
    }

    if (nRet != FSR_BML_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }  

    /* log group PMT update */
    FSR_STL_UpdatePMT(pstZone, pstLogGrp, pstLog, nLpn, nVpn);

    /* change the state of log block */
    FSR_STL_ChangeLogState(pstZone, pstLogGrp, pstLog, nLpn);

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    /* TODO : why it is required ?? */
    if (nLpn == pstZone->pstBUCtxObj->nBufferedLpn)
    {
        STL_BUFFER_CTX_AFTER_FLUSH(pstZone->pstBUCtxObj);
    }
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */

#if (OP_SUPPORT_STATISTICS_INFO == 1)
    pstZone->pstStats->nTotalLogPgmCnt++;
#endif  /* (OP_SUPPORT_STATISTICS_INFO == 1) */

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}

#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 0) */


/**
 * @brief       This function writes multiple pages in the specified log block.
 *
 * @param[in]       pstZone         : zone object
 * @param[in]       pstLogGrp       : log group 
 * @param[in]       pstLog          : log
 * @param[in]       pstParam        : VFLParam object
 * @param[in,out]   pnWrPages       : number of write pages (IN: request, OUT:actual)
 *
 * @return          FSR_STL_SUCCESS : success
 *
 * @author          Wonmoon Cheon
 * @version         1.2.0
 *
 */
PRIVATE INT32
_WriteLogMultiPages    (STLZoneObj     *pstZone,
                        STLLogGrpHdl   *pstLogGrp,
                        STLLog         *pstLog,
                        VFLParam       *pstParam,
                        UINT32         *pnWrPages)
{
    RBWDevInfo     *pstDev          = NULL;
    PADDR           nVpn;
    PADDR           nStartVpn       = NULL_VPN;
    PADDR           nLpn;
    PADDR           nStartLpn;
    INT32           nRet;
    BOOL32          bFlag           = TRUE32;
    UINT32          nPrevVpn        = NULL_VPN;
    UINT32          nActualWrPages  = 0;
    BOOL32          bFirst          = TRUE32;
#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    STLBUCtxObj    *pstBUCtx;
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */
    const   UINT32  nMaxWR          = FSR_STL_GetClstObj(pstZone->nClstID)->nMaxWriteReq;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    FSR_ASSERT(pstLogGrp->pstFm->nDgn != NULL_DGN);
    FSR_ASSERT(pstLog->nVbn != NULL_VBN);

    /* get device info object pointer */
    pstDev = pstZone->pstDevInfo;

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    pstBUCtx = pstZone->pstBUCtxObj;
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */

    nLpn        = pstParam->nLpn;
    nStartLpn   = nLpn;

    FSR_ASSERT(nLpn >= (PADDR)(((pstLogGrp->pstFm->nDgn    ) << pstZone->pstRI->nNShift) << pstDev->nPagesPerSbShift));
    FSR_ASSERT(nLpn <  (PADDR)(((pstLogGrp->pstFm->nDgn + 1) << pstZone->pstRI->nNShift) << pstDev->nPagesPerSbShift));

    do
    {
        FSR_ASSERT(pstLog->nCPOffs < pstDev->nPagesPerSBlk);

        /* get clean page address of the log */
        nVpn = FSR_STL_GetLogCPN(pstZone, pstLog, nLpn);

        if (bFirst == TRUE32)
        {
            nStartVpn = nVpn;
        }

        /* increase the clean page offset of the log */
        pstLog->nCPOffs++;

        nLpn++;
        nActualWrPages++;

        if ((pstLog->nCPOffs >= pstDev->nPagesPerSBlk) ||
            ((bFirst != TRUE32) && (nVpn > nPrevVpn + 1)))
        {
            bFlag = FALSE32;
        }
        else
        {
            nPrevVpn = nVpn;
        }

        if ((*pnWrPages <= nActualWrPages))
        {
            bFlag = FALSE32;
        }

        if (nActualWrPages >= nMaxWR)
        {
            bFlag = FALSE32;
        }

#if (OP_SUPPORT_MLC_LSB_ONLY == 1)
        FSR_STL_CheckMLCFastMode(pstZone, pstLog);
#endif  /* (OP_SUPPORT_MLC_LSB_ONLY == 1) */

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
        if (nLpn == pstBUCtx->nBufferedLpn)
        {
            bFlag = FALSE32;
        }
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */

        bFirst = FALSE32;
    } while (bFlag == TRUE32);

    FSR_STL_SetLogSData123(pstZone, nStartLpn, nStartVpn, pstParam, FALSE32);
    pstParam->pExtParam->nNumExtSData = 0;

    FSR_ASSERT((pstParam->nBitmap &pstDev->nFullSBitmapPerVPg) == pstDev->nFullSBitmapPerVPg);
    FSR_ASSERT(nStartVpn != NULL_VPN);

    pstParam->nNumOfPgs = nActualWrPages;

    nRet = FSR_STL_FlashProgram(pstZone, nStartVpn, pstParam);
    if (nRet != FSR_BML_SUCCESS)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
        return nRet;
    }

    while (nStartLpn < nLpn)
    {
        /* log group PMT update (RAM) */
        FSR_STL_UpdatePMT(pstZone, pstLogGrp, pstLog, nStartLpn, nStartVpn);

        /* change the state of log block (RAM) */
        FSR_STL_ChangeLogState(pstZone, pstLogGrp, pstLog, nStartLpn);

#if (OP_SUPPORT_STATISTICS_INFO == 1)
        pstZone->pstStats->nTotalLogPgmCnt++;
#endif  /* (OP_SUPPORT_STATISTICS_INFO == 1) */

        nStartLpn++;
        nStartVpn++;
    }

    /* set actual written pages */
    *pnWrPages = nActualWrPages;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}


/**
* @brief       This function inserts the specified log group into inactive group cache.
*
* @param[in]   pstZone         : zone object
* @param[in]   pstLogGrp       : log group object pointer to insert
*
* @return      none
* @author      Wonmoon Cheon
* @version     1.0.0
*
*/
PUBLIC VOID 
FSR_STL_InsertIntoInaLogGrpCache   (STLZoneObj     *pstZone,
                                    STLLogGrpHdl   *pstLogGrp)
{
    STLLogGrpHdl   *pstFreeGrp      = NULL;
    STLLogGrpHdl   *pstTempLogGrp   = NULL;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    if (pstZone->pstInaLogGrpCache->nNumLogGrps >= INACTIVE_LOG_GRP_POOL_SIZE)
    {
        pstTempLogGrp = FSR_STL_SelectVictimLogGrp(pstZone->pstInaLogGrpCache, NULL_DGN);
        FSR_ASSERT(pstTempLogGrp != NULL);
        FSR_STL_RemoveLogGrp(pstZone, pstZone->pstInaLogGrpCache, pstTempLogGrp);
    }

    pstFreeGrp = FSR_STL_AllocNewLogGrp(pstZone, pstZone->pstInaLogGrpCache);
    FSR_ASSERT(pstFreeGrp != NULL);

    FSR_OAM_MEMCPY(pstFreeGrp->pBuf, pstLogGrp->pBuf, pstLogGrp->nBufSize);
    FSR_STL_AddLogGrp(pstZone->pstInaLogGrpCache, pstFreeGrp);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}

/**
 * @brief       This function makes slot to insert in active group list.
 *
 * @param[in]   pstZone             : zone object
 * @param[in]   nSelfDgn            : data group number to be excluded in victim selection
 * @param[in]   bEntireLogs         :flag to inactivate entire logs in the group or not
 * @n                                if this is FALSE32, only one active log becomes inactive.
 *
 * @return      FSR_STL_SUCCESS     : success
 * @return      FSR_STL_ERR_LOGGRP  : when victim log group selection fails
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC INT32
FSR_STL_MakeRoomForActLogGrpList   (STLZoneObj     *pstZone,
                                    BADDR           nSelfDgn,
                                    BOOL32          bEntireLogs)
{
    STLLogGrpHdl   *pstVictimLogGrp     = NULL;
    STLLog         *pstVictimLog        = NULL;
    BOOL32          bFullInaLogGrp      = FALSE32;
    INT32           nRet                = FSR_STL_SUCCESS;
    BOOL32          bMergeVictimGrp;
    STLCtxInfoHdl  *pstCtx              = NULL;
#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    STLBUCtxObj    *pstBUCtx;
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    pstCtx = pstZone->pstCtxHdl;

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    pstBUCtx = pstZone->pstBUCtxObj;
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */

    /**
     * maximum number of log groups always equals to the max. number of active logs
     * in worst case, there is only one active log per each log group.
     * Instead of number of log groups, # of active logs is being used.
     */
    if ((pstZone->pstActLogGrpList->nNumLogGrps < MAX_ACTIVE_LBLKS) && (pstCtx->pstFm->nNumLBlks < MAX_ACTIVE_LBLKS))
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
        return FSR_STL_SUCCESS;
    }

    nRet = FSR_STL_ReserveMetaPgs(pstZone, 1, TRUE32);
    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }

    /* when there is no space to insert this group */
    /* 1) select victim log group to be replaced */
    pstVictimLogGrp = FSR_STL_SelectVictimLogGrp(pstZone->pstActLogGrpList, nSelfDgn);
    if (pstVictimLogGrp == NULL)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
        return FSR_STL_ERR_LOGGRP;
    }
#if (OP_SUPPORT_RUNTIME_PMT_BUILDING == 1)
    else
    {
        /* Execute Run time scanning */
        nRet = FSR_STL_ScanActLogBlock(pstZone,pstVictimLogGrp->pstFm->nDgn);
        if (nRet != FSR_STL_SUCCESS)
        {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR, 
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
        return FSR_STL_ERR_LOGGRP;
        }
    }
#endif /*OP_SUPPORT_RUNTIME_PMT_BUILDING*/

    /* 2) select victim log to be inactive */
    pstVictimLog = FSR_STL_SelectVictimLog(pstVictimLogGrp, TRUE32);
    if (pstVictimLog != NULL)
    {
#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1 && OP_SUPPORT_BU_DELAYED_FLUSH == 1)
    /* write the previous misaligned sectors into log block */
        if ((pstBUCtx->nBufferedDgn == pstVictimLogGrp->pstFm->nDgn) &&
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

    /* 3) change log state to inactive */
    /* 4) remove the victim log from active log list */
    FSR_STL_RemoveActLogList(pstZone, pstVictimLogGrp->pstFm->nDgn, pstVictimLog);
    }


    if (bEntireLogs == TRUE32) /* make all active logs -> inactive */
    {
        while (pstVictimLog != NULL)
        {
            pstVictimLog = FSR_STL_SelectVictimLog(pstVictimLogGrp, TRUE32);

            if (pstVictimLog == NULL)
            {
                break;
            }
            FSR_STL_RemoveActLogList(pstZone, pstVictimLogGrp->pstFm->nDgn, pstVictimLog);
        }
    }

    /** 
     * In this case, pstVictimLogGrp always exists in the PMTDir[].
     * check if the victim group can be merge victim or not.
     */
    nRet = FSR_STL_CheckMergeVictimGrp(pstZone, pstVictimLogGrp, FALSE32, &bMergeVictimGrp);
    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
        return nRet;
    }

    if (bMergeVictimGrp == TRUE32)
    {
        FSR_STL_SetMergeVictimGrp(pstZone, pstVictimLogGrp->pstFm->nDgn, TRUE32);
    }

    /* store PMT+CXT, update PMTDir */
    nRet = FSR_STL_StorePMTCtx(pstZone, pstVictimLogGrp, TRUE32);
    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }

    /* check if all logs in the group are inactive or not */
    bFullInaLogGrp = FSR_STL_CheckFullInaLogs(pstVictimLogGrp);
    if (bFullInaLogGrp == TRUE32)
    {
        /* insert into inactive log group cache */
        FSR_STL_InsertIntoInaLogGrpCache(pstZone, pstVictimLogGrp);

        /* current active log group moves to inactive log group */
        FSR_STL_RemoveLogGrp(pstZone, pstZone->pstActLogGrpList, pstVictimLogGrp);
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}


/**
* @brief       This function allocates new active log group.
*
* @param[in]   pstZone     : zone object
* @param[in]   nDgn        : data group number to write
*
* @return      Newly allocated active log group pointer
*
* @author      Wonmoon Cheon
* @version     1.0.0
*
*/
PUBLIC STLLogGrpHdl*
FSR_STL_NewActLogGrp   (STLZoneObj     *pstZone,
                        BADDR           nDgn)
{
    STLLogGrpHdl   *pstNewLogGrp    = NULL;
    POFFSET         nMetaPOffs;
    INT32           nRet;
    STLLogGrpHdl   *pstCachedLogGrp = NULL;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* search if the DGN exists in the PMTDir */
    nRet = FSR_STL_SearchPMTDir(pstZone, nDgn, &nMetaPOffs);

    if (nMetaPOffs != NULL_POFFSET) /* when it is inactive log group */
    {
        /* allocate new log group from active log group pool */
        pstNewLogGrp = FSR_STL_AllocNewLogGrp(pstZone, pstZone->pstActLogGrpList);
        FSR_ASSERT(pstNewLogGrp != NULL);

        pstCachedLogGrp = FSR_STL_SearchLogGrp(pstZone->pstInaLogGrpCache, nDgn);

        if (pstCachedLogGrp == NULL)
        {
            /* if the DGN does not exist in the cache, load the PMT */
            nRet = FSR_STL_LoadPMT(pstZone, nDgn, nMetaPOffs, &pstNewLogGrp, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                return NULL;
            }
        }
        else
        {
            FSR_OAM_MEMCPY(pstNewLogGrp->pBuf, pstCachedLogGrp->pBuf, pstCachedLogGrp->nBufSize);

            /* Make sure that the log group does not exist in the cache */
            FSR_STL_RemoveLogGrp(pstZone, pstZone->pstInaLogGrpCache, pstCachedLogGrp);
        }

        /* add the log group into active log group list */
        FSR_STL_AddLogGrp(pstZone->pstActLogGrpList, pstNewLogGrp);

    }
    else    /* when it is data group */
    {
        /* allocate new log group from active log group pool */
        pstNewLogGrp = FSR_STL_AllocNewLogGrp(pstZone, pstZone->pstActLogGrpList);
        FSR_ASSERT(pstNewLogGrp != NULL);

        /* add the log group into active log group list */
        FSR_STL_AddLogGrp(pstZone->pstActLogGrpList, pstNewLogGrp);

        /* set DGN in the newly allocated log group */
        pstNewLogGrp->pstFm->nDgn = nDgn;
    }

    /* return the newly allocated active log group */
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return pstNewLogGrp;
}

/**
 *  @brief      This function returns active log group for the specified data group.
 *  @n          A new active log group is generated if required.
 *
 *  @param[in]  pstZone   : zone object
 *  @param[in]  nLpn      : logical page number to write
 *
 *  @return     newly allocated active log group pointer
 *
 *  @author     Jaesoo Lee
 *  @version    1.1.0
 */
PUBLIC STLLogGrpHdl*
FSR_STL_PrepareActLGrp     (STLZoneObj     *pstZone,
                            PADDR           nLpn)
{
    INT32           nRet            = FSR_STL_SUCCESS;
    STLLogGrpHdl   *pstActLogGrp    = NULL;
    BADDR           nLbn            = NULL_VBN;
    BADDR           nDgn            = NULL_VBN;
    RBWDevInfo     *pstDev          = NULL;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get device info pointer */
    pstDev = pstZone->pstDevInfo;

    /* get DGN(data group number) according to the specified LPN */
    nLbn = (BADDR)(nLpn >> pstDev->nPagesPerSbShift);
    nDgn = nLbn >> NUM_DBLKS_PER_GRP_SHIFT;

    /* check if log group exists or not corresponding to the nDgn */
    pstActLogGrp = FSR_STL_SearchLogGrp(pstZone->pstActLogGrpList, nDgn);
#if (OP_SUPPORT_RUNTIME_PMT_BUILDING == 1)
    if (pstActLogGrp != NULL)
    {
        /* Execute Run time scanning */
        nRet = FSR_STL_ScanActLogBlock(pstZone,nDgn);
        if (nRet != FSR_STL_SUCCESS)
        {
            return NULL;
        }        
    }
#endif /*OP_SUPPORT_RUNTIME_PMT_BUILDING*/

    while (pstActLogGrp == NULL) /* fail to find log group */
    {
        /* prepare to make new active log group */
        nRet = FSR_STL_MakeRoomForActLogGrpList(pstZone, nDgn, TRUE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        /* prepare new active log group */
        pstActLogGrp = FSR_STL_NewActLogGrp(pstZone, nDgn);
        if (pstActLogGrp == NULL)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, pstActLogGrp));
    return pstActLogGrp;
}


/*****************************************************************************/
/* Global Function Definition                                                */
/*****************************************************************************/

/**
 * @brief       This function checks if all logs in the group are inactive or not.
 *
 * @param[in]   pstLogGrp       : log group object pointer
 *
 * @return      TRUE32          : full inactive logs
 * @return      FALSE32         : one of logs is the group is not a inactive log
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC BOOL32 
FSR_STL_CheckFullInaLogs   (STLLogGrpHdl   *pstLogGrp )
{
    STLLog     *pstLogs     = NULL;
    STLLog     *pstTempLog  = NULL;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    FSR_ASSERT(pstLogGrp->pstFm->nHeadIdx != NULL_LOGIDX);

    pstLogs = pstLogGrp->pstLogList;

    pstTempLog = pstLogs + pstLogGrp->pstFm->nHeadIdx;

    /* check if all logs are active */
    while (pstTempLog != NULL)
    {
        if (pstTempLog->nState &(1 << LS_ACTIVE_SHIFT))
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
            return FALSE32;
        }

        if (pstTempLog->nNextIdx != NULL_LOGIDX)
        {
            pstTempLog = pstLogs + pstTempLog->nNextIdx;
        }
        else
        {
            break;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return TRUE32;
}

/**
 * @brief       This function gets available log group and log block.
 *
 * @param[in]       pstZone         : zone object
 * @param[in]       nLpn            : logical page number
 * @param[out]      pstTargetGrp    : log group to write
 * @param[out]      pstTargetLog    : log block to write
 * @param[out]      pnNumAvailPgs   : the number of the available pages
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC INT32 
FSR_STL_GetLogToWrite  (STLZoneObj     *pstZone,
                        PADDR           nLpn,
                        STLLogGrpHdl   *pstActLogGrp,
                        STLLog        **pstTargetLog,
                        UINT32         *pnNumAvailPgs)
{
    RBWDevInfo     *pstDev          = NULL;
    BADDR           nLbn            = NULL_VBN;
    BADDR           nDgn            = NULL_VBN;
    UINT32          nNumAvailPgs    = 0;
    STLCtxInfoHdl  *pstCtxInfo      = NULL;
    STLLog         *pstActLog       = NULL;
    INT32           nRet            = FSR_STL_SUCCESS;
    POFFSET         nMetaPOffs      = NULL_POFFSET;
    BOOL32          bNewLogAllocated = FALSE32;
    BADDR           nFBlkVbn        = NULL_VBN;
    UINT32          nFBlkEC         = 0;
    BOOL32          bMergeVictimGrp;

    UINT32          nPgsPerSBlk;
    POFFSET         nCPOffs;
    POFFSET         nWayPOffs;
    
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get device info pointer */
    pstDev = pstZone->pstDevInfo;

    /* get context info pointer */
    pstCtxInfo = pstZone->pstCtxHdl;

    /* get DGN(data group number) according to the specified LPN */
    nLbn = (BADDR)(nLpn >> pstDev->nPagesPerSbShift);
    nDgn = nLbn >> NUM_DBLKS_PER_GRP_SHIFT;

    FSR_ASSERT(FSR_STL_SearchLogGrp(pstZone->pstActLogGrpList, nDgn) != NULL);

    do{
        /* find available log in the active log group (don't care active/inactive log) */
        pstActLog = FSR_STL_FindAvailLog(pstZone, pstActLogGrp, nLpn, &nNumAvailPgs);

        if (pstActLog == NULL) /* when there is no available log */
        {
            /* check the current number of logs in the group */
            if (pstActLogGrp->pstFm->nNumLogs >= pstZone->pstRI->nK) /* equal or more than 'k' */
            {
                /* compaction */
                nRet = FSR_STL_CompactLog(pstZone, pstActLogGrp, &pstActLog);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    return nRet;
                }
            }

            if (pstActLog == NULL)
            {
                /* if there is no free block, we need to merge inactive log group */
                if (pstCtxInfo->pstFm->nNumFBlks <= 1)
                {
                    /* produce free block by merge */
                    nRet = FSR_STL_ReserveFreeBlks(pstZone, nDgn, 2, NULL);
                    if (nRet != FSR_STL_SUCCESS)
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                                __FSR_FUNC__, __LINE__, nRet));
                        return nRet;
                    }
                }

                /* when there is available free block */
                nRet = FSR_STL_GetFreeBlk(pstZone, &nFBlkVbn, &nFBlkEC);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    return nRet;
                }

                /* add the free block into active log group */
                pstActLog = FSR_STL_AddNewLog(pstZone, pstActLogGrp, nLbn, nFBlkVbn, nFBlkEC);
                if (pstActLog == NULL)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, FSR_STL_ERR_NO_LOG));

                    /* critical error */
                    return FSR_STL_ERR_NO_LOG;
                }
            }

            /* set flag for merge victim group */
            bNewLogAllocated = TRUE32;
        }

        if (!(pstActLog->nState &(1 << LS_ACTIVE_SHIFT))) /* if inactive log */
        {
            /* check if there is empty slot in active log list of context */
            nRet = _MakeRoomForActiveSlot(pstZone, pstCtxInfo, pstActLogGrp);

            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                    (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
                return nRet;
            }

            /* change the log state */
            /* add the inactive log into active log list of context */
            FSR_STL_AddActLogList(pstZone, pstActLogGrp->pstFm->nDgn, pstActLog);

            /* rearrange active log group LRU */
            FSR_STL_MoveLogGrp2Head(pstZone->pstActLogGrpList, pstActLogGrp);

            /* only if new log block is allocated, we need to check the merge victim flag. */
            if (bNewLogAllocated == TRUE32)
            {
                /* search if the DGN exists in the PMTDir */
                nRet = FSR_STL_SearchPMTDir(pstZone, pstActLogGrp->pstFm->nDgn, &nMetaPOffs);

                if (nMetaPOffs != NULL_POFFSET)
                {
                    /** 
                     * Only if the active group exists in PMTDir[],
                     * we need to set merge victim flag
                     */
                    nRet = FSR_STL_CheckMergeVictimGrp(pstZone, pstActLogGrp, FALSE32, &bMergeVictimGrp);

                    if (nRet != FSR_STL_SUCCESS)
                    {
                        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                            (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
                        return nRet;
                    }

                    if (bMergeVictimGrp == TRUE32)
                    {
                        FSR_STL_SetMergeVictimGrp(pstZone, pstActLogGrp->pstFm->nDgn, TRUE32);
                    }

#if (OP_SUPPORT_DATA_WEAR_LEVELING == 1)
                    /* update minimum erase count */
                    FSR_STL_UpdatePMTDir(pstZone, pstActLogGrp->pstFm->nDgn, nMetaPOffs, pstActLogGrp->pstFm->nMinEC,
                        pstActLogGrp->pstFm->nMinVbn, FALSE32);
#else
                    FSR_STL_UpdatePMTDir(pstZone, pstActLogGrp->pstFm->nDgn, nMetaPOffs, 0, 0, FALSE32);
#endif
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

            /* store PMT+CXT */
            nRet = FSR_STL_StorePMTCtx(pstZone, pstActLogGrp, TRUE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                return nRet;
            }
        }

        nPgsPerSBlk = pstDev->nPagesPerSBlk;

        /* get its clean page offset */
        nCPOffs = pstActLog->nCPOffs;

        /*  get actual clean page offset considering way interleaving */
        nWayPOffs = FSR_STL_GetWayCPOffset(pstDev, nCPOffs, nLpn);

        /* set number of available pages */
        nNumAvailPgs  = nPgsPerSBlk - nWayPOffs;

    } while(nNumAvailPgs == 0);

    if (pnNumAvailPgs)
    {   
        * pnNumAvailPgs = nNumAvailPgs;
    }

    *pstTargetLog = pstActLog;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}

/**
 *  @brief      This function sets the spare data that is SData 1, 2, and 3.
 * 
 *  @param[in]      pstZone       : zone object
 *  @param[in]      nLpn          : logical page number
 *  @param[in]      nDstVpn       : destination virtual page number
 *  @param[in]      pstParam      : VFL parameter object
 *  @param[in]      bConfirmPage  : flag that this page is the confirm page or not.
 *
 *  @return         FSR_STL_SUCCESS
 *
 *  @author         Wonmoon Cheon
 *  @version        1.1.0
 */
 
PUBLIC VOID 
FSR_STL_SetLogSData123( STLZoneObj *pstZone, 
                       PADDR nLpn, 
                       PADDR nDstVpn, 
                       VFLParam *pstParam,
                       BOOL32 bConfirmPage )
{
    RBWDevInfo *pstDev;
    BOOL32      bIsLSBPage;
    UINT32      nLpo;
    UINT8       nLogType;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get device info object pointer */
    pstDev = pstZone->pstDevInfo;

    /*
     * prepare spare data
     * First, fIll SData1 and SData2 as follows
     *      For LSB, SData1 = LPO.~LPO, SData2 = 0xFFFFFFFF
     *      For LSB, SData1 = 0xFFFFFFFF, SData2 = LPO.~LPO
     */
    bIsLSBPage = FSR_STL_FlashIsLSBPage(pstZone, nDstVpn);

    nLpo = (GET_REMAINDER(nLpn, pstDev->nPagesPerSbShift + pstZone->pstRI->nNShift) & 0xffff);

    pstParam->nSData1 = (nLpo << 16) | (nLpo ^ 0xFFFF);

    if (bIsLSBPage)
    {
        pstParam->nSData2 = 0xFFFFFFFF;
        nLogType = (bConfirmPage ? ENDLSB : MIDLSB);
    }
    else
    {
        pstParam->nSData2 = pstParam->nSData1;
        pstParam->nSData1 = 0xFFFFFFFF;
        nLogType = (bConfirmPage ? ENDMSB : MIDMSB);
    }

    /* Fill PTF into SData3 */
    pstParam->nSData3 = FSR_STL_SetPTF(pstZone, TF_LOG, 0, nLogType, 0);
    pstParam->nSData3 = (pstParam->nSData3 << 16) | (0xffff & (~pstParam->nSData3));

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}


/**
 *
 *  @brief        This function fills the RndIn info of pstParam.
 *              
 * 
 *  @param[in]    pstZone   : zone object
 *  @param[in]    pstParam  : VFL parameter object
 *
 *  @return       FSR_STL_SUCCESS
 *  @author       Jaesoo Lee
 *  @version      1.1.0
 */ 
PUBLIC INT32 
FSR_STL_Convert_ExtParam( STLZoneObj    *pstZone, 
                          VFLParam      *pstParam )
{
    RBWDevInfo *pstDev = NULL;
    UINT16      nNumIn = 0;
    BOOL32      bInFrag = FALSE32;
    BOOL32      bDataStarted = FALSE32;
    UINT8      *pBuf;
    UINT16      nSize = 0;
    UINT32      nIdx = 0;
    UINT16      nNumExtSData;
    UINT16      nFlashOffset = 0;
    UINT16      nSctPerVPg = (UINT16)pstZone->pstDevInfo->nSecPerVPg;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get device info object pointer */
    pstDev = pstZone->pstDevInfo;

    /* When the OneNand 2K device, we only use 2 extensive meta data */
    nNumExtSData = (nSctPerVPg == STL_2KB_PG) ? nSctPerVPg >> 1 : nSctPerVPg;

    if (pstParam->pExtParam == NULL)
    {
        return FSR_STL_SUCCESS;
    }

    if (pstParam->bPgSizeBuf)
    {
        bDataStarted = TRUE32;
    }

    pBuf = pstParam->pData;

    if ((pstParam->nBitmap &pstDev->nFullSBitmapPerVPg) == pstDev->nFullSBitmapPerVPg)
    {
        nSize = (UINT16)(BYTES_PER_SECTOR << pstDev->nSecPerVPgShift);
        FSR_STL_SetRndInArg(pstParam->pExtParam, nFlashOffset, nSize, (VOID *)pBuf, nNumIn++);
    }
    else if ((pstParam->nBitmap &pstDev->nFullSBitmapPerVPg) != 0)
    {
        for (nIdx = 0; nIdx < nSctPerVPg; nIdx++)
        {
            if (!bInFrag)
            {
                if (pstParam->nBitmap &(1 << nIdx))
                {
                    /* Start of new fragment */
                    bInFrag = TRUE32;
                    nSize = BYTES_PER_SECTOR;
                    bDataStarted = TRUE32;
                }
                else
                {
                    /* move buffer pointer */
                    if (bDataStarted)
                    {
                        pBuf += BYTES_PER_SECTOR;
                    }
                    nFlashOffset += BYTES_PER_SECTOR;
                }
            }
            else
            {
                if (pstParam->nBitmap &(1 << nIdx))
                {
                    nSize += BYTES_PER_SECTOR;
                }
                else
                {
                    /*
                     * End of a fragment
                     * make a new random input argument
                     */
                    FSR_STL_SetRndInArg(pstParam->pExtParam, nFlashOffset, nSize, (VOID *)pBuf, nNumIn++);

                    bInFrag = FALSE32;
                    pBuf += nSize + BYTES_PER_SECTOR;
                    nFlashOffset += nSize + BYTES_PER_SECTOR;
                }
            }
        }

        if (bInFrag)
        {
            /*
             * End of a fragment
             * make a new random input argument
             */
            FSR_STL_SetRndInArg(pstParam->pExtParam, nFlashOffset, nSize, (VOID *)pBuf, nNumIn++);
        }
    }

    if (pstParam->bSpare)
    {
        nFlashOffset = nSctPerVPg * BYTES_PER_SECTOR;

        FSR_STL_SetRndInArg(pstParam->pExtParam, FSR_STL_CPBK_SPARE_BASE, 3 * sizeof(UINT32), /* 3 SData in total */
        (VOID *) &pstParam->nSData1, nNumIn++);

        if (pstParam->pExtParam && pstParam->pExtParam->nNumExtSData > 0)
        {
            FSR_STL_SetRndInArg(pstParam->pExtParam, FSR_STL_CPBK_SPARE_EXT, nSctPerVPg * sizeof(UINT32),
                (VOID *) &pstParam->pExtParam->aExtSData, nNumIn++);
            pstParam->pExtParam->nNumExtSData = nNumExtSData;
        }
    }

    pstParam->pExtParam->nNumRndIn = nNumIn;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}


/**
 *  @brief      This function initializes
 *              CRC values in the given VFLExtParam to 0xFFFFFFFF
 *              
 * 
 *  @param[in]  pstZone       : partition object
 *  @param[in]  pstParam      : VFL parameter object
 *
 *  @return     FSR_STL_SUCCESS
 *
 *  @author     Jaesoo Lee
 *  @version    1.1.0
 */
PUBLIC VOID
FSR_STL_InitCRCs   (STLZoneObj *pstZone,
                    VFLParam   *pstParam)
{
    const UINT32        nSctPerVPg  = pstZone->pstDevInfo->nSecPerVPg;
          VFLExtParam  *pstVFLExt;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    pstVFLExt = pstParam->pExtParam;
    pstVFLExt->nNumExtSData = 0;

    /* sizeof (UINT32) * nSctPerVPg */
    FSR_OAM_MEMSET(pstVFLExt->aExtSData, 0xFF, nSctPerVPg << 2);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}


/**
 *  @brief          This function computes CRC
 *
 *  @param[in]      pstZone       : zone object
 *  @param[in]      pstSrcParam   : source VFLParam object
 *  @param[in]      pstDstParam   : destination VFLParam object
 *  @param[in]      bCRCValid     : flag for the CRC is valid or not
 *
 *  @return         FSR_STL_SUCCESS
 *
 *  @author         Jaesoo Lee
 *  @version        1.1.0
 */

PUBLIC VOID
FSR_STL_ComputeCRCs    (STLZoneObj *pstZone,
                        VFLParam   *pstSrcParam,
                        VFLParam   *pstDstParam,
                        BOOL32      bCRCValid )
{
    const UINT32    nSctPerVPg      = pstZone->pstDevInfo->nSecPerVPg;
    RBWDevInfo     *pstDev          = pstZone->pstDevInfo;
    VFLParam       *pstTmpParam     = NULL;
    UINT32         *pnCurDstCRC;
    UINT32         *pnCurSrcCrc;
    UINT8          *pCurDataPos;
    UINT8          *pCurBufPos;
    UINT32          nIdx;    
    UINT32          nSectorsPerCRC;
    UINT32          nSizePerCRC;
    UINT32          nSpareIdx       = 0;    
    BOOL32          bDataStarted    = FALSE32;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Compute CRC per 2 sectors for OneNAND, 1 sector for Flex-OneNAND */
    nSectorsPerCRC = (pstDev->nDeviceType & RBW_DEVTYPE_ONENAND) ? 2 : 1;

    /* Compute CRC per 1024 bytes for OneNAND, 512 bytes for Flex-OneNAND */
    nSizePerCRC = BYTES_PER_SECTOR * nSectorsPerCRC;

    /* Set variable, pCurBufPos */
    if (pstSrcParam)
    {
        FSR_ASSERT(pstSrcParam->bPgSizeBuf);
        pCurBufPos = pstSrcParam->pData;
    }
    else
    {
        if (pstDev->nDeviceType & RBW_DEVTYPE_ONENAND)
        {
            /* If the source data is NULL, then we manually allocate a buffer */
            pstTmpParam             = FSR_STL_AllocVFLParam(pstZone);
            pstTmpParam->nBitmap    = pstDev->nFullSBitmapPerVPg;
            pstTmpParam->pData      = (UINT8 *) pstZone->pTempMetaPgBuf;

            FSR_OAM_MEMSET((UINT8*)pstZone->pTempMetaPgBuf,0xFF, nSctPerVPg * BYTES_PER_SECTOR);

            /* Copy pstDstParam to pstTmpParam to fill empty sectors with 0xFF */
            _CopyVFLParamBuffer(pstZone, pstDstParam, pstTmpParam);

            pCurBufPos = pstTmpParam->pData;
        }
        else
        {
            pCurBufPos = NULL;
        }
    }

    pCurDataPos  = pstDstParam->pData;
    bDataStarted = pstDstParam->bPgSizeBuf;

    /* Compute CRCs */
    for (nIdx = 0; nIdx < nSctPerVPg; nIdx = nIdx + nSectorsPerCRC)
    {
        if (pstDev->nDeviceType & RBW_DEVTYPE_ONENAND)
        {
            nSpareIdx = GET_ONENAND_SPARE_IDX(nIdx);
        }
        else
        {
            nSpareIdx = nIdx;
        }

        /* 
         * CRCs for sectors whose index is >= 3 are stored at the extended area 
         * For simplicity, we compute a pointer for the space where the CRC is stored.
         */
        pnCurDstCRC = &(pstDstParam->pExtParam->aExtSData[nSpareIdx]);

        /* For OneNAND */
        if (pstDev->nDeviceType & RBW_DEVTYPE_ONENAND)
        {
            if (pstSrcParam)
            {
                pnCurSrcCrc = &(pstSrcParam->pExtParam->aExtSData[nSpareIdx]);

                /* Compute CRC from data existing either in log or in data block */
                *pnCurDstCRC = FSR_STL_CalcCRC32(pCurBufPos, nSizePerCRC);
            }
            else if(pstTmpParam)
            {
                /* Compute CRC from data existing either in log or in data block */
                *pnCurDstCRC = FSR_STL_CalcCRC32(pCurBufPos, nSizePerCRC);
            }
            else
            {
                FSR_ASSERT(FALSE32);
            }
            FSR_ASSERT(*pnCurDstCRC == FSR_STL_CalcCRC32(pCurBufPos, nSizePerCRC));
        }
        /* For Flex-OneNAND */
        else
        {            
            if (pstDstParam->nBitmap & (1 << nIdx))
            {
                /* Set flag required to move data pointer correctly */
                bDataStarted = TRUE32;

                /* Compute CRC from input data */
                if (*pnCurDstCRC == 0xFFFFFFFF)
                {
                    *pnCurDstCRC = FSR_STL_CalcCRC32(pCurDataPos, nSizePerCRC);
                }
                FSR_ASSERT(*pnCurDstCRC == FSR_STL_CalcCRC32(pCurDataPos, nSizePerCRC));
            }
            else if (pstSrcParam)
            {
                pnCurSrcCrc = &(pstSrcParam->pExtParam->aExtSData[nSpareIdx]);

                /*
                 * Reclaim CRCs from the source page if available.
                 */
                if (pstSrcParam->nBitmap & (1 << nIdx))
                {
                    /* src from log or data block */
                    if (!bCRCValid)
                    {
                        /* Impossible to reuse CRC */
                        /* Compute CRC from data existing either in log or in data block */
                        *pnCurDstCRC = FSR_STL_CalcCRC32(pCurBufPos, nSizePerCRC);
                    }
                    else
                    {
                        /* Great! We can reuse the CRC value */
                        *pnCurDstCRC = *pnCurSrcCrc;
                    }
                } /* another case never occurs */
                else
                {
                    /* It should be placed in the user buffer */
                    FSR_ASSERT(FALSE32);
                    /*FSR_ASSERT(pstDstParam->nBitmap & (1 << nIdx));*/
                }
            }
            else
            {
                /* Maybe this should be for partial program!! */
                *pnCurDstCRC = STL_DELETED_SCT_CRC_VALUE;
            }
        }

        pCurBufPos += nSizePerCRC;
        if (bDataStarted)
        {
            pCurDataPos += nSizePerCRC;
        }
    }

    if (pstTmpParam != NULL) 
    {
        FSR_STL_FreeVFLParam(pstZone, pstTmpParam);
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}


/**
 *  @brief          This function copies the buffer data from pstUserParam to pstDstParam.
 *                  There are two cases.
 *                  1) The pointers of pData are same with pstUserParam and pstDstParam.
 *                  We only updates the bitmap setting of pstUserParam.
 *                  2) The pointers of pData of pstUserParam and pstDstParam are different.
 *                  The data of pstUserParam can be empty. So, the buffer pointer
 *                  of pstUserParam should be increased considering bit map offset.
 *                  However, pstDstParam has full space for containing full virtual page.
 *
 *  @param[in]      pstZone       : zone object
 *  @param[in]      pstUserParam   : source VFLParam object
 *  @param[in]      pstDstParam   : destination VFLParam object 
 *
 *  @return         
 *
 *  @author         SangHoon Choi
 *  @version        1.1.0
 */
PRIVATE VOID
_CopyVFLParamBuffer     (STLZoneObj *pstZone,
                        VFLParam   *pstUserParam,
                        VFLParam   *pstDstParam)
{
    const UINT32    nSctPerVPg      = pstZone->pstDevInfo->nSecPerVPg;
    UINT8          *pCurSrcBufPos;
    UINT8          *pCurDstBufPos;
    UINT32          nIdx;
    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    FSR_ASSERT((pstUserParam != NULL) && (pstDstParam != NULL));

    /* Only updates the bitmap setting since the both data addresses are same. */
    if (pstUserParam->pData == pstDstParam->pData)
    {
        pstDstParam->nBitmap |= pstUserParam->nBitmap;
        return ;
    }

    pCurSrcBufPos   = pstUserParam->pData;
    pCurDstBufPos   = pstDstParam->pData;

    /* Copy pstUserParam->pData to pstDstParam->pData */
    if(pCurSrcBufPos != NULL)
    {
        /* Copy user data to src */
        for (nIdx = 0; nIdx < nSctPerVPg; nIdx++)
        {
            if (pstUserParam->nBitmap & (1 << nIdx))
            {
                FSR_OAM_MEMCPY(pCurDstBufPos,pCurSrcBufPos,BYTES_PER_SECTOR);
                /* The pointer of pCurSrcBufPos should be increased only when the bitmap is set */
                pCurSrcBufPos  += BYTES_PER_SECTOR;
                pstDstParam->nBitmap |= 1 << nIdx;
            }
            /* We assume pstDstParam->pData has full space for a whole virtual page. */
            pCurDstBufPos += BYTES_PER_SECTOR;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}


/**
 *  @brief      This function makes a bitmap 
 *
 *  @param      nSrcBitmap[IN]: source bitmap
 *
 *  @return     Sector bitmap data
 *
 *  @author     Jaesoo Lee
 *  @version    1.1.0
 */
PUBLIC SBITMAP
FSR_STL_MakeCompSBitmap( SBITMAP nSrcBitmap )
{
    SBITMAP     nSBitmap = 0;
    INT32       i;
    UINT32      nMaxIdx;
    UINT32      nMSBIdx = 0;
    UINT32      nLSBIdx = 0;
    BOOL32      bOneFound = FALSE32;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ] ++%s()\r\n"), __FSR_FUNC__));

    if (nSrcBitmap == 0)
    {
        return 0;
    }

    /*  the number of bits in the bitmap structure*/
    nMaxIdx = (sizeof(SBITMAP) << 3) - 1; /* i.e., * 8 bits */

    for (i = nMaxIdx; i >= 0; i--)
    {
        if (!bOneFound && (nSrcBitmap &(1 << i)))
        {
            bOneFound = TRUE32;
            nMSBIdx = i;
        }

        if (bOneFound && (nSrcBitmap &(1 << i)))
        {
            nLSBIdx = i;
        }
    }

    nSBitmap = ((1 << (nMSBIdx + 1)) - 1) ^ ((1 << nLSBIdx) - 1);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nSBitmap));
    return nSBitmap;
}

/**
 * @brief       This function writes specified log page.
 * @n           In doing so, it computes CRCs of each sector and append them at 
 * @n           the spare area.
 * @n           If any CRC value included in the pstParam is valid (i.e., != 0xFF),
 * @n           of course, the corressponding CRC doesn't have to be computed again.
 *
 * @param       pstZone  [IN] : partition object
 * @param       nSrcVpn  [IN] : VPN of source page (if any)
 * @param       nDstVpn  [IN] : VPN of dest page
 * @param       pstParam [IN] : VFL parameter object
 *
 * @return      FSR_STL_SUCCESS
 *
 * @author      Jaesoo Lee
 * @version     1.1.0
 */
PUBLIC INT32
FSR_STL_FlashModiCopybackCRC( STLZoneObj   *pstZone, 
                              PADDR         nSrcVpn, 
                              PADDR         nDstVpn, 
                              VFLParam     *pstParam )
{
    RBWDevInfo *pstDev;
    INT32       nRet;
    UINT32      nIdx;
    UINT32     *pnCurDstCRC;
    VFLParam   *pstVFLParam;
    BOOL32      bCRCValid = FALSE32;
    UINT16      nSctPerVPg = (UINT16)pstZone->pstDevInfo->nSecPerVPg;
    UINT16      nNumExtSData;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /*  get device info pointer*/
    pstDev = pstZone->pstDevInfo;

    /* We write CRCs at the spare area */
    pstParam->bSpare = TRUE32;

    pstVFLParam            = FSR_STL_AllocVFLParam(pstZone);
    pstVFLParam->nBitmap   = 0;
    pstVFLParam->pExtParam = FSR_STL_AllocVFLExtParam(pstZone);
    pstVFLParam->pExtParam->nNumExtSData = (UINT16)nSctPerVPg;

    nNumExtSData = (nSctPerVPg == STL_2KB_PG) ? nSctPerVPg >> 1 : nSctPerVPg;

    /* initialize Source CRC values to 0xFFFFFFFF */
    FSR_STL_InitCRCs(pstZone, pstVFLParam);

    /* Ignore source page if we are writing full page */
    if ((pstParam->nBitmap &pstDev->nFullSBitmapPerVPg) == pstDev->nFullSBitmapPerVPg)
    {
        nSrcVpn = NULL_VPN;
    }
    else if (nSrcVpn == NULL_VPN)
    {
        for (nIdx = 0; nIdx < nSctPerVPg; nIdx++)
        {
            /* 
             * CRCs for sectors whose index is >= 3 are stored at the extended area 
             * For simplicity, we compute a pointer for the space where the CRC is stored.
             */
            pnCurDstCRC = &pstParam->pExtParam->aExtSData[nIdx];

            if (!(pstParam->nBitmap &(1 << nIdx)))
                * pnCurDstCRC = STL_DELETED_SCT_CRC_VALUE;
        }
    }
    else if (nSrcVpn != NULL_VPN)
    {
        /* 
         *  we have to perform read and program instead of a copyback program
         *  in order (1) to reclaim pre computed CRC values if any, and 
         *  (2) to compute CRCs if not exist 
         */
        pstVFLParam->nLpn = nSrcVpn;
        pstVFLParam->bPgSizeBuf = TRUE32;
        pstVFLParam->bUserData = FALSE32;
        pstVFLParam->bSpare = TRUE32;
        pstVFLParam->pExtParam->nNumExtSData = nNumExtSData;

        pstVFLParam->nBitmap = pstDev->nFullSBitmapPerVPg &(~pstParam->nBitmap);

        /* BML do not allow spitted bitmap */
        if (pstParam->nBitmap != 0 && !(pstParam->nBitmap & 1) && !(pstParam->nBitmap &(1 << (nSctPerVPg - 1))))
        {
            pstVFLParam->nBitmap = pstDev->nFullSBitmapPerVPg;
        /* (OBSOLETED) FSR_STL_MakeCompSBitmap (pstVFLParam->nBitmap); */
        }

        pstVFLParam->nNumOfPgs = 1;
        pstVFLParam->pData = (UINT8 *)pstZone->pTempMetaPgBuf;

        nRet = FSR_STL_FlashRead(pstZone, nSrcVpn, pstVFLParam, FALSE32);
        if (nRet != FSR_BML_SUCCESS)
        {
            return nRet;
        }

        /* Make CRC values to 0xFFFFFFF except the ones which will be reclaimed */
        for (nIdx = 0; nIdx < nSctPerVPg; nIdx++)
        {
            if (!(pstVFLParam->nBitmap &(1 << nIdx)))
            {
                pstVFLParam->pExtParam->aExtSData[nIdx] = 0xFFFFFFFF;
            }
        }

        if (IS_LOG_CRC_VALID(pstVFLParam->nSData3 >> 16) || IS_BU_PAGE(pstZone, pstVFLParam->nSData3 >> 16))
        { 
            /* CRC is valid */
            bCRCValid = TRUE32;
        }
    }

    if (pstDev->nDeviceType & RBW_DEVTYPE_ONENAND)
    {
        /* 
        * Copy User to Src.
        * After we copy src to the user buffer, we recalculate CRC
        * So, we don't care the previous calculated value.
        */
        _CopyVFLParamBuffer(pstZone, pstParam, pstVFLParam);
        FSR_STL_ComputeCRCs(pstZone, pstVFLParam, pstParam, FALSE32);
    }
    else
    {
        FSR_STL_ComputeCRCs(pstZone, pstVFLParam, pstParam, bCRCValid);
    }

    pstParam->pExtParam->nNumExtSData = nNumExtSData;

    if (nSrcVpn == NULL_VPN)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
            (TEXT("FSR_STL_FlashProgram : nDstVbn = %d, nDstVpn = %d(%d), nSBitmap = 0x%x\n"),
                nDstVpn >> pstDev->nPagesPerSbShift, nDstVpn, nDstVpn % pstDev->nPagesPerSBlk, pstParam->nBitmap));
        nRet = FSR_STL_Convert_ExtParam(pstZone, pstParam);

        /* No need to copy source sectors */
        nRet = FSR_STL_FlashProgram(pstZone, nDstVpn, pstParam);
    }
    else
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
            (TEXT("VFL_ModiCopyback : nSrcVpn = %d, nDstVpn = %d(%d), nSBitmap = 0x%x\n"),
                nSrcVpn, nDstVpn, nDstVpn % pstDev->nPagesPerSBlk, pstParam->nBitmap));
        nRet = FSR_STL_Convert_ExtParam(pstZone, pstParam);
        nRet = FSR_STL_FlashModiCopyback(pstZone, nSrcVpn, nDstVpn, pstParam);
    }

    if (nRet != FSR_BML_SUCCESS)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
        return nRet;
    }
    
    FSR_STL_FreeVFLParam(pstZone,
                            pstVFLParam);

    FSR_STL_FreeVFLExtParam(pstZone,
                            pstVFLParam->pExtParam);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_BML_SUCCESS;
}

/**
 *  @brief      This function copies back specified log page.
 *  @n          In doing so, it computes CRCs of each sector and append them at 
 *  @n          the spare area.
 *  @n          IMPORTANT: this function assumes that the page is copied as a log.
 * 
 *  @param      pstZone [IN] : partition object
 *  @param      nLpn    [IN] : LPN
 *  @param      nSrcVpn [IN] : VPN of source page (if any)
 *  @param      nDstVpn [IN] : VPN of dest page
 *  @param      nBitmap [IN] : Bitmap 
 *
 *  @return     FSR_STL_SUCCESS
 *
 *  @author      Jaesoo Lee
 *  @version     1.1.0
 */

PUBLIC INT32
FSR_STL_FlashCopybackCRC   (STLZoneObj *pstZone,
                            PADDR       nLpn,
                            PADDR       nSrcVpn,
                            PADDR       nDstVpn,
                            SBITMAP     nBitmap)
{
          RBWDevInfo   *pstDev = NULL;
          VFLParam     *pstVFLParam;
    const UINT16        nSctPerVPg = (UINT16)pstZone->pstDevInfo->nSecPerVPg;
          INT32         nRet;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get device info object pointer */
    pstDev = pstZone->pstDevInfo;

    /* 
     * We should not use pstVFLExtParam2 since FSR_STL_FlashModiCopybackCRC
     * could use it
     */
    pstVFLParam             = FSR_STL_AllocVFLParam(pstZone);
    pstVFLParam->pExtParam  = FSR_STL_AllocVFLExtParam(pstZone);

    pstVFLParam->pData      = (UINT8 *)(pstZone->pTempMetaPgBuf);
    pstVFLParam->bPgSizeBuf = FALSE32;
    pstVFLParam->bUserData  = FALSE32;
    pstVFLParam->nNumOfPgs  = 1;
    pstVFLParam->bSpare     = TRUE32;
    pstVFLParam->pExtParam->nNumExtSData = nSctPerVPg;

    /* initialize CRC values to 0xFFFFFFFF */
    FSR_STL_InitCRCs(pstZone, pstVFLParam);

    if (nBitmap == pstDev->nFullSBitmapPerVPg)
    {
        pstVFLParam->nBitmap = 0;
    }
    else
    {
        FSR_ASSERT(FALSE32);
    }

    FSR_STL_SetLogSData123(pstZone, nLpn, nDstVpn, pstVFLParam, TRUE32);
    nRet = FSR_STL_FlashModiCopybackCRC(pstZone, nSrcVpn, nDstVpn, pstVFLParam);

    FSR_STL_FreeVFLParam(pstZone,
                            pstVFLParam);
    FSR_STL_FreeVFLExtParam(pstZone,
                            pstVFLParam->pExtParam);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}


/**
 * @brief       This function writes sectors from the specified logical 
 * @n           sector number. When the asynchronous mode is used, user buffer must 
 * @n           not be released until the corresponding event callback function 
 * @n           will be called.
 *
 * @param[in]   nVolID          : volume identifier (RBW_VOLID_0, RBW_VOLID_1, ...)
 * @param[in]   nZoneID         : partition id
 * @param[in]   nLsn            : start logical sector number (0 ~ (total sectors - 1))
 * @param[in]   nSectors        : number of sectors to write
 * @param[in]   pBuf            : user buffer pointer 
 * @param[in]   nFlag           : 
 *
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_ERR_PARAM
 * @return      FSR_STL_ERR_PHYSICAL
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC INT32 
FSR_STL_WriteZone  (UINT32      nClstID,
                    UINT32      nZoneID,
                    UINT32      nLsn,
                    UINT32      nNumOfScts,
                    UINT8      *pBuf,
                    UINT32      nFlag)
{
    STLClstObj     *pstClst;
    STLZoneObj     *pstZone             = NULL;
    RBWDevInfo     *pstDev              = NULL;
    PADDR           nStartLpn           = 0;
    PADDR           nEndLpn             = 0;
    PADDR           nCurLpn             = 0;
    UINT32          nWrSectors          = 0;
    UINT32          nTotalWrSectors     = 0;
    UINT32          nWrPages            = 0;
    VFLParam       *pstParam            = NULL;
    INT32           nRet                = FSR_STL_SUCCESS;
    STLLogGrpHdl   *pstLogGrp           = NULL;
    STLLog         *pstLog              = NULL;
    POFFSET         nWayCPOffs          = 0;
    BOOL32          bConfirmRequired    = FALSE32;
    PADDR           nDgrpEndLpn         = 0;
    PADDR           nTempLbn            = 0;
    UINT32          nNumAvailPgs        = 0;
#if (OP_SUPPORT_RUNTIME_PMT_BUILDING == 1)
    BADDR           nLbn                = NULL_VBN;
    BADDR           nDgn                = NULL_VBN;
#endif /*OP_SUPPORT_RUNTIME_PMT_BUILDING*/

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    STLBUCtxObj    *pstBUCtx            = NULL;
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* check input parameters */
    nRet = FSR_STL_CheckArguments(nClstID, nZoneID, nLsn, nNumOfScts, pBuf);
    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }

    /* Get cluster object pointer */
    pstClst = gpstSTLClstObj[nClstID];

    /* get partition object pointer */
    pstZone = &(gpstSTLClstObj[nClstID]->stZoneObj[nZoneID]);

    /* get device info pointer */
    pstDev = pstZone->pstDevInfo;

    /* initialize VFL parameter (including extended param) */
    FSR_STL_InitVFLParamPool(pstClst);

    pstParam = FSR_STL_AllocVFLParam(pstZone);

    pstParam->pExtParam = FSR_STL_AllocVFLExtParam(pstZone);
    pstParam->pExtParam->nNumExtSData = 0;
    pstParam->pExtParam->nNumRndIn = 0;

    /* set transaction begin mark */
    pstClst->bTransBegin = TRUE32;

    /* Reserve meta page */
    nRet = FSR_STL_ReserveMetaPgs(pstZone, 1, TRUE32);
    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }

#if (OP_SUPPORT_PAGE_DELETE == 1)
    /* store previous deleted page info */
    nRet = FSR_STL_StoreDeletedInfo(pstZone);
    if (nRet != FSR_STL_SUCCESS)
    {
        return nRet;
    }
#endif  /* (OP_SUPPORT_PAGE_DELETE == 1) */

    /* start logical page address */
    nStartLpn = nLsn >> pstDev->nSecPerVPgShift;

    /* end logical page address */
    nEndLpn = (nLsn + nNumOfScts - 1) >> pstDev->nSecPerVPgShift;

    /* set starting LPN */
    nCurLpn = nStartLpn;

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    pstBUCtx = pstZone->pstBUCtxObj;

 #if (OP_SUPPORT_BU_DELAYED_FLUSH == 0)
    /* write the previous misaligned sectors into log block */
    if ((pstBUCtx->nBufState != STL_BUFSTATE_CLEAN) &&
        ((nCurLpn > pstBUCtx->nBufferedLpn) ||
         (nEndLpn < pstBUCtx->nBufferedLpn)))
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
    }
 #endif /* (OP_SUPPORT_BU_DELAYED_FLUSH == 0) */
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */

    /* VFL parameter setting*/
    pstParam->bUserData = TRUE32;
    pstParam->bSpare = TRUE32;

    /**          S0 S1 S2 S3 S4 S5 S6 S7
     * [head]        |->--------------  : nStartLpn
     * [body] -----------------------
     * [body] -----------------------
     * [tail] --->|                    : nEndLpn
     */
    do
    {
#if (OP_SUPPORT_RUNTIME_PMT_BUILDING == 1)
        /* get DGN(data group number) according to the specified LPN */
        nLbn = (BADDR)(nCurLpn >> pstDev->nPagesPerSbShift);
        nDgn = nLbn >> NUM_DBLKS_PER_GRP_SHIFT;

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

        pstLogGrp = FSR_STL_PrepareActLGrp(pstZone, nCurLpn);
        if (pstLogGrp == NULL)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return FSR_STL_ERR_NEW_LOGGRP;
        }

        /*
         * Compute the start LPN of the current data group
         * Firstly, compute the data group number
         */
        nTempLbn = (nCurLpn >> pstDev->nPagesPerSbShift) >> NUM_DBLKS_PER_GRP_SHIFT;

        /* Secondly, compute the LPN of the last page in the current DGN */
        nDgrpEndLpn = (((nTempLbn + 1) << NUM_DBLKS_PER_GRP_SHIFT) << pstDev->nPagesPerSbShift) - 1;

        /* Thirdly, take the smaller one*/
        if (nDgrpEndLpn > nEndLpn)
            nDgrpEndLpn = nEndLpn;

        /* loop for a data group */
        do
        {
            /* Although, we reserved the free blk when the handling of request is end,
             * In case of sudden power-off, There may not be a reserved free blk.
             */
                nRet = FSR_STL_ReserveYFreeBlks(pstZone, pstLogGrp->pstFm->nDgn, 2, NULL);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    return nRet;
                }

            /**
             * FSR_STL_GetLogToWrite() is main core routine ----------------------------
             * get available log block which is active log. To get available active log,
             * it might need very expensive cost such as GC, compaction, merge,
             * wear-leveling, or meta reclaim.
             * IMPORTANT: In the current implementation, the log 
             */
            nRet = FSR_STL_GetLogToWrite(pstZone, nCurLpn, pstLogGrp, &pstLog, &nNumAvailPgs);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                return nRet;
            }

            FSR_ASSERT(nNumAvailPgs    > 0);

            /* set transaction begin mark */
            pstClst->bTransBegin = TRUE32;

            /* Loop for a single log block */
            do
            {
                FSR_ASSERT(pstLog->nCPOffs + nNumAvailPgs <= pstDev->nPagesPerSBlk);

                bConfirmRequired = FALSE32;

                /* VFL parameter setting */
                pstParam->pData     = pBuf;
                pstParam->nLpn      = nCurLpn;
                pstParam->nNumOfPgs = 1;
                pstParam->bUserData = TRUE32;

                if (nCurLpn < nEndLpn)
                {
                    nWrSectors = pstDev->nSecPerVPg - (nLsn &(pstDev->nSecPerVPg - 1));
                }
                else
                {
                    nWrSectors = (nNumOfScts - nTotalWrSectors);
                }
                pstParam->bPgSizeBuf = (nWrSectors == pstDev->nSecPerVPg) ? TRUE32 : FALSE32;

                if ((nCurLpn > nStartLpn) &&
                    (nCurLpn < nEndLpn))
                {
                    /* body pages bitmap*/
                    pstParam->nBitmap = pstDev->nFullSBitmapPerVPg;
                }
                else
                {
                    /* head, tail page bitmap*/
                    pstParam->nBitmap = FSR_STL_SetSBitmap(nLsn, nWrSectors, pstDev->nSecPerVPg);
                }

                /* write multiple pages */
#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 0)
                if ((pstParam->nBitmap == pstDev->nFullSBitmapPerVPg) &&
                    (nCurLpn           != nDgrpEndLpn))
#else /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */
                if ((pstParam->nBitmap == pstDev->nFullSBitmapPerVPg) &&
                    (nCurLpn           != nDgrpEndLpn) &&
                    (nCurLpn           != pstBUCtx->nBufferedLpn))
#endif /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 0) */
                {
                    /* body pages (full sector bitmap) */
                    nWrSectors = (nNumOfScts - nTotalWrSectors);

                    /*
                     * The last page should be confirmed !!
                     * So, we should not include it in the current write.
                     */
                    nWrPages = (nDgrpEndLpn - nCurLpn);

                    FSR_ASSERT(nWrPages >= 1);

                    /*
                     * Note that we don't have to care about buffered one here
                     *      because _WriteLogMultiPages() will take care of it. 
                     */
                    nRet = _WriteLogMultiPages(pstZone, pstLogGrp, pstLog, pstParam, &nWrPages);
                    if (nRet != FSR_STL_SUCCESS)
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                                __FSR_FUNC__, __LINE__, nRet));
                        return nRet;
                    }

                    /* nWrSectors : actual written sectors */
                    nWrSectors = nWrPages << pstDev->nSecPerVPgShift;

                }
                else
                {
                    nWrPages = 1;

                    /*
                     * Note that we don't have to confirm the current page 
                     *      even though it is the last page in the current log block,
                     *      since We store PMT at the boundary of the log block
                     *      when the log block become inactive.
                     * Thus, it is enough to leave confirm sig for the last dgrp lpn.
                     */
                    if (nCurLpn == nDgrpEndLpn)
                    {
                        bConfirmRequired = TRUE32;
                    }

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
                    /* head, tail page : write target sectors into log block */
                    nRet = FSR_STL_WriteSinglePage(pstZone, pstLogGrp, pstLog, pstParam, nNumOfScts, bConfirmRequired);
#else   /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 0) */
                    /* head, tail page : write target sectors into log block */
                    nRet = _WriteLogSinglePage(pstZone, pstLogGrp, pstLog, pstParam, bConfirmRequired);
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */
                    if (nRet != FSR_STL_SUCCESS)
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                                __FSR_FUNC__, __LINE__, nRet));
                        return nRet;
                    }

                }
                FSR_ASSERT(nNumAvailPgs >= nWrPages);

                /* set to the next page */
                nCurLpn         += nWrPages;

                nLsn            += nWrSectors;
                nTotalWrSectors += nWrSectors;
                pBuf            += (nWrSectors << BYTES_SECTOR_SHIFT);

                /* recalculate the number of clean page in log blk */
                nWayCPOffs   = FSR_STL_GetWayCPOffset(pstDev, pstLog->nCPOffs, nCurLpn);
                nNumAvailPgs = pstDev->nPagesPerSBlk - nWayCPOffs;

            } while ((nNumAvailPgs >  0) &&
                     (nCurLpn      <= nDgrpEndLpn));

            FSR_STL_UpdatePMTCost(pstZone, pstLogGrp);

            /* When the handling of request is end,
             * we must reserve free blk for the next request.
             */
                nRet = FSR_STL_ReserveYFreeBlks(pstZone, pstLogGrp->pstFm->nDgn, 2, NULL);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    return nRet;
                }

        } while (nCurLpn <= nDgrpEndLpn);

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
        /* reserve one free log page for the future flush operation */

        if ((pstBUCtx->nBufferedDgn == pstLogGrp->pstFm->nDgn) &&
            (pstBUCtx->nBufState    != STL_BUFSTATE_CLEAN))
        {
            nRet = FSR_STL_ReserveYFreeBlks(pstZone, pstLogGrp->pstFm->nDgn, 2, NULL);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                return nRet;
            }

            nRet = FSR_STL_GetLogToWrite(pstZone, pstBUCtx->nBufferedLpn, pstLogGrp, &pstLog, &nNumAvailPgs);
        }
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */
    } while (nCurLpn <= nEndLpn);

    FSR_STL_FreeVFLParam   (pstZone, pstParam);
    FSR_STL_FreeVFLExtParam(pstZone, pstParam->pExtParam);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}
