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
 *  @file       FSR_STL_BufferBlkMgr.c
 *  @brief      This file contains buffer block related codes.
 *  @author     Wonmoon Cheon
 *  @date       23-MAY-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  23-MAY-2007 [Wonmoon Cheon] : first writing
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

/*****************************************************************************/
/* Local (static)  Function Definition                                       */
/*****************************************************************************/

/*****************************************************************************/
/* Global Function Definition                                                */
/*****************************************************************************/

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)

/**
 * @brief       This function writes the buffered page into log block.
 *
 * @param[in]   pstZone                     : Zone object
 *
 * @return      FSR_STL_SUCCESS
 *
 * @author      Jaesoo Lee
 * @version     1.1.0
 */
PUBLIC INT32 
FSR_STL_FlushBufferPage    (STLZoneObj *pstZone)
{
    RBWDevInfo     *pstDev;
    STLLogGrpHdl   *pstLogGrp       = NULL;
    STLLog         *pstLog          = NULL;
    PADDR           nLpn;
    PADDR           nDstVpn;
    PADDR           nSrcVpn;
    STLBUCtxObj    *pstBUCtx;
    PADDR           nLogSrcVpn      = NULL_VPN;
    SBITMAP         nLogSrcBitmap   = 0;
    BOOL32          bPageDeleted    = FALSE32;
    VFLParam       *pstVFLParam;
    UINT16          nSctPerVPg      = (UINT16)pstZone->pstDevInfo->nSecPerVPg;
    INT32           nRet            = FSR_STL_SUCCESS;
    UINT16          nNumExtSData;

    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Get device info pointer */
    pstDev = pstZone->pstDevInfo;

    pstBUCtx = pstZone->pstBUCtxObj;

    nNumExtSData = (nSctPerVPg == STL_2KB_PG) ? nSctPerVPg >> 1 : nSctPerVPg;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:INF] (nVolID:%d, nZoneID:%d, nBufferedLpn:%d, nValidSBitmap:%08b)\r\n"),
            pstZone->nVolID, pstZone->nZoneID, pstBUCtx->nBufferedLpn, pstBUCtx->nValidSBitmap));

#if (OP_SUPPORT_RUNTIME_PMT_BUILDING == 1)
    /* Execute Run time scanning */
    nRet = FSR_STL_ScanActLogBlock(pstZone,pstBUCtx->nBufferedDgn);
    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR, 
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }
#endif /*OP_SUPPORT_RUNTIME_PMT_BUILDING*/

    if (pstBUCtx->nBufState == STL_BUFSTATE_CLEAN)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
        return nRet;
    }

    FSR_ASSERT(pstBUCtx->nBufferedLpn != NULL_VPN);

    pstVFLParam = FSR_STL_AllocVFLParam(pstZone);

    /*
     * In order to avoid deadlock, we change the buf state a priori.
     * Change the buffer state to CLEAN
     */
    STL_BUFFER_CTX_AFTER_FLUSH(pstBUCtx);

    /* LPN to write */
    nLpn = pstBUCtx->nBufferedLpn;

    /*
     * Before we flush the buffered page into log block, we need an active log
     * to flush it. 
     */

    pstLogGrp = FSR_STL_PrepareActLGrp(pstZone, nLpn);
    if (pstLogGrp == NULL)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, FSR_STL_ERR_NEW_LOGGRP));
        return FSR_STL_ERR_NEW_LOGGRP;
    }

    nRet = FSR_STL_GetLogToWrite(pstZone, nLpn, pstLogGrp, &pstLog, NULL);
    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }
    /* Get clean page address of the log */
    nDstVpn = FSR_STL_GetLogCPN(pstZone, pstLog, nLpn);

    /* Increase the clean page offset of the log */
    pstLog->nCPOffs++;

#if (OP_SUPPORT_MLC_LSB_ONLY == 1)
    /*
     * For MLC LSB only mode, we moves the clean page offset according to the
     * LSB/MSB page address scramble.
     */

    FSR_STL_CheckMLCFastMode(pstZone, pstLog);
#endif  /* (OP_SUPPORT_MLC_LSB_ONLY == 1) */

    /*
     * nBufferedVpn points the buffered page position
     * there is always only one valid buffer page in a buffer block.
     */
     
    nSrcVpn = pstBUCtx->nBufferedVpn;

    if (pstBUCtx->nValidSBitmap != pstDev->nFullSBitmapPerVPg)
    {
        /*
         * Some scts from the end of page are missing 
         * find out the old source VPN
         */
         
        nRet = FSR_STL_GetLpn2Vpn(pstZone, nLpn, &nLogSrcVpn, &bPageDeleted);

        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
            return nRet;
        }

        if (!bPageDeleted)
        {
            nLogSrcBitmap = pstDev->nFullSBitmapPerVPg & ~(pstBUCtx->nValidSBitmap);
        }
        else
        {
            nLogSrcBitmap = 0;
        }
    }

    /* Initialize VFL params */
    pstVFLParam->bPgSizeBuf = FALSE32;
    pstVFLParam->bUserData  = FALSE32;
    pstVFLParam->bSpare     = TRUE32;
    pstVFLParam->nNumOfPgs  = 1;
    pstVFLParam->pData      = (UINT8 *)pstZone->pTempMetaPgBuf;
    pstVFLParam->pExtParam  = FSR_STL_AllocVFLExtParam(pstZone);

    FSR_STL_InitCRCs(pstZone, pstVFLParam);
    pstVFLParam->pExtParam->nNumExtSData = nNumExtSData;

    if (!nLogSrcBitmap)
    {
        pstVFLParam->nBitmap = 0;

        FSR_STL_SetLogSData123(pstZone, nLpn, nDstVpn, pstVFLParam, TRUE32);

        nRet = FSR_STL_Convert_ExtParam(pstZone, pstVFLParam);

        nRet = FSR_STL_FlashModiCopybackCRC(pstZone, nSrcVpn, nDstVpn, pstVFLParam);
        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
            return nRet;
        }
    }
    else
    {
        /* Note that buffered data is always the most recent */
        pstVFLParam->nBitmap   = pstBUCtx->nValidSBitmap;
        pstVFLParam->bUserData = FALSE32;

        nRet = FSR_STL_FlashRead(pstZone, nSrcVpn, pstVFLParam, FALSE32);
        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
            return nRet;
        }

        /* We guarantee that the CRC in buffer is always valid*/
        FSR_STL_SetLogSData123(pstZone, nLpn, nDstVpn, pstVFLParam, TRUE32);

        nRet = FSR_STL_Convert_ExtParam(pstZone, pstVFLParam);

        nRet = FSR_STL_FlashModiCopybackCRC(pstZone, nLogSrcVpn, nDstVpn, pstVFLParam);
        if (nRet != FSR_BML_SUCCESS)
        {
            return nRet;
        }
    }

    /* Update PMT of log group */
    FSR_STL_UpdatePMT(pstZone, pstLogGrp, pstLog, nLpn, nDstVpn);
    FSR_STL_UpdatePMTCost(pstZone, pstLogGrp);

    /* Change the state of log block */
    FSR_STL_ChangeLogState(pstZone, pstLogGrp, pstLog, nLpn);

#if (OP_SUPPORT_STATISTICS_INFO == 1)
    /* Experimental data generation for analysis */
    pstZone->pstStats->nTotalLogPgmCnt++;
#endif

#if (OP_SUPPORT_STATISTICS_INFO == 1)
    pstZone->pstStats->nBufferMissCnt++;
#endif

    FSR_STL_FreeVFLParam(pstZone,
                            pstVFLParam);

    FSR_STL_FreeVFLExtParam(pstZone,
                            pstVFLParam->pExtParam);


    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return FSR_STL_SUCCESS;
}

/**
 *  @brief      This function determines where to write, buffer or log block. 
 * 
 *  @param[in]  pstZone       : zone object
 *  @param[in]  pstLog        : log object
 *  @param[in]  pstParam      : VFL parameter object
 *  @param[in]  nTrTotalSize  : the size of the transaction
 *
 *  @return     TRUE32  : buffer block write
 *  @return     FALSE32 : log block write
 *
 *  @author     Jaesoo Lee
 *  @version    1.1.0
 */

PUBLIC BOOL32 
FSR_STL_DetermineBufferWrite   (STLZoneObj     *pstZone,
                                STLLog         *pstLog,
                                VFLParam       *pstParam,
                                UINT32          nTrTotalSize)
{
    RBWDevInfo     *pstDev;
    STLBUCtxObj   *pstBUCtx;
    BOOL32          bBufferWrite = FALSE32;
    UINT32          nSctPerVPg;
    PADDR           nLpn;
    POFFSET         nCurPOffs = 0;
    POFFSET         nExpectedPOffs = 0;

    FSR_STACK_VAR;
    FSR_STACK_END;

    /* Get the buffer unit context */
    pstBUCtx = pstZone->pstBUCtxObj;

    nLpn = pstParam->nLpn;

    /* Get device info pointer */
    pstDev = pstZone->pstDevInfo;

    nSctPerVPg = pstZone->pstDevInfo->nSecPerVPg;

    /* Determine where to write, buffer or log.*/
    if ((nLpn == pstBUCtx->nBufferedLpn && /* overwrite case */
    (pstParam->nBitmap &pstBUCtx->nValidSBitmap)))
    {
        bBufferWrite = TRUE32;
    }
    else if (!(pstParam->nBitmap &(1 << (nSctPerVPg - 1))))
    {
        /* end missing case */
        if (nTrTotalSize >= nSctPerVPg)
        {
            if ((pstLog->nState &LS_STATES_BIT_MASK) == LS_ALLOC)
            {
                if (GET_REMAINDER(nLpn, pstDev->nPagesPerSbShift) == 0)
                {
                    bBufferWrite = TRUE32;
                }
            }
            else if ((pstLog->nState &LS_STATES_BIT_MASK) == LS_SEQ)
            {
                nExpectedPOffs = (POFFSET)GET_REMAINDER(pstLog->nCPOffs, pstDev->nPagesPerSbShift);
                nCurPOffs = (POFFSET)GET_REMAINDER(nLpn, pstDev->nPagesPerSbShift);

                if (nExpectedPOffs == nCurPOffs)
                {
                    bBufferWrite = TRUE32;
                }
            }
        }
    }
    return bBufferWrite;
}

/**
 *  @brief          This function writes the specified nLpn page into
 *  @n              buffer block.
 * 
 *  @param[in]      pstZone     : partition object
 *  @param[in]      pstLogGrp   : log group object
 *  @param[in]      pstLog      : log pointer
 *  @param[in]      pstParam    : VFL parameter object
 *
 *  @return         FSR_STL_SUCCESS
 *
 *  @author         Jaesoo Lee
 *  @version        1.1.0
 */
PUBLIC INT32
FSR_STL_WriteSinglePage    (STLZoneObj     *pstZone,
                            STLLogGrpHdl   *pstLogGrp,
                            STLLog         *pstLog,
                            VFLParam       *pstParam,
                            UINT32 nTrTotalSize,
                            BOOL32 bConfirmPage)
{
    const   RBWDevInfo *pstDev      = pstZone->pstDevInfo;
    STLCtxInfoHdl      *pstCtx      = pstZone->pstCtxHdl;
    STLCtxInfoFm       *pstCtxFm    = pstCtx->pstFm;
    STLBUCtxObj    *pstBUCtx;

    PADDR           nLpn;
    POFFSET         nBufNumMaxPgs = 0;

    BADDR           nBBlkVbn;
    UINT32          nBBlkEC = 0;

    PADDR           nBufSrcVpn = NULL_VPN;
    SBITMAP         nBufSrcBitmap = 0;
    PADDR           nLogSrcVpn = NULL_VPN;
    SBITMAP         nLogSrcBitmap = 0;
    PADDR           nDstVpn = NULL_VPN;
    SBITMAP         nDstBitmap = 0;
    PADDR           nSrcVpn = NULL_VPN;
    SBITMAP         nSrcBitmap = 0;

    BOOL32          bPageDeleted = FALSE32;
    VFLParam       *pstVFLParam;
    UINT8          *pCurBufPos;
    UINT8          *pCurDataPos;
    BOOL32          bStartCopied = FALSE32;

    const   UINT16  nSctPerVPg      = (UINT16)(pstDev->nSecPerVPg);
    BOOL32          bBufferWrite    = FALSE32;
    UINT32          nIdx            = 0;
    INT32           nRet;
    BOOL32          bCRCValid = FALSE32;
    UINT32         *pnCurDstCRC;
    UINT32         *pnCurSrcCrc;
    UINT16          nNumExtSData;

    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    FSR_ASSERT(pstLogGrp->pstFm->nDgn != NULL_DGN);
    FSR_ASSERT(pstLog->nVbn != NULL_VBN);
    FSR_ASSERT(pstParam->nNumOfPgs == 1);

    /* When the OneNand 2K device, we only use 2 extensive meta data */
    nNumExtSData = (nSctPerVPg == STL_2KB_PG) ? nSctPerVPg >> 1 : nSctPerVPg;

    /* Get the buffer unit context */
    pstBUCtx = pstZone->pstBUCtxObj;

    nLpn = pstParam->nLpn;

    pstParam->nBitmap = pstParam->nBitmap & pstDev->nFullSBitmapPerVPg;

    FSR_ASSERT(nLpn >= (UINT32)(((pstLogGrp->pstFm->nDgn    ) << pstZone->pstRI->nNShift) << pstDev->nPagesPerSbShift));
    FSR_ASSERT(nLpn <  (UINT32)(((pstLogGrp->pstFm->nDgn + 1) << pstZone->pstRI->nNShift) << pstDev->nPagesPerSbShift));

    FSR_STL_InitCRCs(pstZone, pstParam);

    pstVFLParam = FSR_STL_AllocVFLParam(pstZone);

    pstVFLParam->pExtParam = FSR_STL_AllocVFLExtParam(pstZone);
    pstVFLParam->pExtParam->nNumExtSData = 0;
    pstVFLParam->pExtParam->nNumRndIn = 0;

    bBufferWrite = FSR_STL_DetermineBufferWrite(
        pstZone, pstLog, pstParam, nTrTotalSize);

#if (OP_SUPPORT_STATISTICS_INFO == 1)
    if(bBufferWrite)
    {
        pstZone->pstStats->nBufferWriteCnt++;

        if ((pstBUCtx->nBufState == STL_BUFSTATE_DIRTY) &&
            (pstBUCtx->nBufferedLpn == nLpn))
        {
            pstZone->pstStats->nBufferHitCnt++;
        }
    }
#endif

    if(!bBufferWrite)
    {
        /* get clean page address of the log */
        nDstVpn = FSR_STL_GetLogCPN(pstZone, pstLog, nLpn);

        /* increase the clean page offset of the log */
        pstLog->nCPOffs++;

#if (OP_SUPPORT_MLC_LSB_ONLY == 1)
        FSR_STL_CheckMLCFastMode(pstZone, pstLog);
#endif
    }
    else if(bBufferWrite)
    {
        /* Context has to be stored if new unit is allocated */
        nRet = FSR_STL_ReserveMetaPgs(pstZone, 1, TRUE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
            return nRet;
        }
        
        /* We always confirm the buffer */
        bConfirmPage = TRUE32;
        
        if (pstDev->nDeviceType == RBW_DEVTYPE_MLC)
        {
            /* When MLC case, we use only LSB pages. */
            nBufNumMaxPgs = (POFFSET)(pstDev->nPagesPerSBlk >> 1);
        }
        else
        {
            nBufNumMaxPgs = (POFFSET)(pstDev->nPagesPerSBlk);
        }

#if (OP_SUPPORT_DATA_WEAR_LEVELING == 1)
        if (pstBUCtx->nBBlkCPOffset >= nBufNumMaxPgs)
        {
            nRet = FSR_STL_WearLevelFreeBlk(pstZone, 1, NULL_DGN);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                return nRet;
            }
        }
#endif  /* (OP_SUPPORT_DATA_WEAR_LEVELING == 1) */
    }


/* 
 * Note: we don't have to initialize CRC field, since we guarantee
 * that CRC field has been initialized before here.
 */

    if((!bBufferWrite) &&
       (pstParam->nBitmap == pstDev->nFullSBitmapPerVPg))
    {
        /* set spare data 1, 2, 3 */
        FSR_STL_SetLogSData123(pstZone, nLpn, nDstVpn, pstParam, bConfirmPage);

        if (bConfirmPage)
        {
            /* Initialize CRC */
            FSR_STL_InitCRCs(pstZone, pstParam);
            FSR_STL_ComputeCRCs(pstZone, NULL, pstParam, FALSE32);
            pstParam->pExtParam->nNumExtSData = nNumExtSData;
        }

        nRet = FSR_STL_Convert_ExtParam(pstZone, pstParam);        
        nRet = FSR_STL_FlashProgram(pstZone, nDstVpn, pstParam);
        if ( nRet != FSR_BML_SUCCESS )
        {
            return nRet;
        }
    }
    else
    {
        /*
         *  flush the buffered page if
         *  (1) we have to write the data into the buffer and
         *  (2) the buffered data is dirty and 
         *  (3) the buffered data is not conflict with the current one.
         */

        if (bBufferWrite &&
                pstBUCtx->nBufState == STL_BUFSTATE_DIRTY && 
                pstBUCtx->nBufferedLpn != nLpn)
        {
            nRet = FSR_STL_FlushBufferPage(pstZone);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                    (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
                return nRet;
            }
        }

        /* Compute source pages and bitmaps */
        nSrcVpn = NULL_VPN;
        nSrcBitmap = 0;

        /*
         * 1. Src from the buffer
         * Check for overwrite case
         */
        if ((pstBUCtx->nBufState    == STL_BUFSTATE_DIRTY) &&
            (pstBUCtx->nBufferedLpn == nLpn))
        {
            nBufSrcBitmap = pstBUCtx->nValidSBitmap;
            /* Clear bit map for sectors being overwritten */
            nBufSrcBitmap = nBufSrcBitmap &(~pstParam->nBitmap);
        }
        else
        {
            nBufSrcBitmap = 0;
        }
        nBufSrcVpn = (nBufSrcBitmap == 0 ? NULL_VBN : pstBUCtx->nBufferedVpn);

        /*
         * 2. Src from log block or data block
         *  2.1. Compute the resultant valid bitmap
         */
        nDstBitmap = nBufSrcBitmap | pstParam->nBitmap;
        /* 2.2. Missing scts from start of the page should be copied from data or log block */
        nLogSrcBitmap = FSR_STL_MarkSBitmap(nDstBitmap);

        if (!bBufferWrite || nLogSrcBitmap)
        {
            /* 
             * Some scts from the start of page are missing 
             * Thus, we have read it first
             * find out the old source VPN
             */
            nRet = FSR_STL_GetLpn2Vpn(pstZone, nLpn, &nLogSrcVpn, &bPageDeleted);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                    (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
                return nRet;
            }

            if (!bPageDeleted)
            {
                if(bBufferWrite)
                {
                    ;/* nLogSrcBitmap = FSR_STL_MarkZeroUntilLastOneofSBitmap(nDstBitmap); */
                }
                else
                {
                    nLogSrcBitmap = (~nDstBitmap) & pstDev->nFullSBitmapPerVPg;
                }

                nDstBitmap = nDstBitmap | nLogSrcBitmap;
            }
            else
            {
                nLogSrcBitmap = 0;
                nLogSrcVpn = NULL_VPN;
            }
        }

        /*
         * Prepare data 
         * The 1st case is where data from previous buffer and log or data block should be merged
         */
        if (nLogSrcBitmap && nBufSrcBitmap)
        {
            /*
             * Since we guarantee that a buffered sectors are contiguous and start from 
             * the first sector of the page,
             * there is only one possible layout as belows.
             *             _________________
             *             |   |   |   |   |
             *             -----------------
             *     src f/  -->          <--- src f/ log
             *       buf            
             *  (1) When the new dat overlaps with the src from the buffer
             *      - Buffer write && NO SRC FROM LOG --> contradiction
             *  (2) Whe the new data overlaps with the log src
             *      - (1) end-full case --> log write
             *      - (2) otherwise --> buffer write && new data wrapped by log src
             */

            pstVFLParam->bPgSizeBuf = TRUE32;
            pstVFLParam->bUserData = FALSE32;
            pstVFLParam->bSpare = TRUE32;

            FSR_STL_InitCRCs(pstZone, pstVFLParam);
            pstVFLParam->pExtParam->nNumExtSData = nNumExtSData;

            pstVFLParam->nBitmap = nLogSrcBitmap;
            pstVFLParam->nNumOfPgs = 1;
            pstVFLParam->pData     = (UINT8 *)(pstZone->pTempMetaPgBuf);

            nRet = FSR_STL_FlashRead(pstZone, nLogSrcVpn, pstVFLParam, FALSE32);
            if (nRet != FSR_BML_SUCCESS)
            {
                FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                    (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
                return nRet;
            }

            if (IS_LOG_CRC_VALID(pstVFLParam->nSData3 >> 16))
            { 
                bCRCValid = TRUE32;
            }

            pCurDataPos = pstParam->pData;
            pCurBufPos   = (UINT8 *)(pstZone->pTempMetaPgBuf);
            bStartCopied = FALSE32;

            for (nIdx = 0; nIdx < nSctPerVPg; nIdx++)
            {
                pnCurSrcCrc = &pstVFLParam->pExtParam->aExtSData[nIdx];
                pnCurDstCRC = &pstParam->pExtParam->aExtSData[nIdx];

                if ((pstParam->nBitmap) &(1 << nIdx))
                {
                    FSR_OAM_MEMCPY(pCurBufPos, pCurDataPos, BYTES_PER_SECTOR);
                    bStartCopied = TRUE32;
                }
                /* We reclaim pre-computed CRC value at best-effort */
                else if ((nLogSrcBitmap & (1 << nIdx)) &&
                         (bCRCValid))
                {
                    *pnCurDstCRC = *pnCurSrcCrc;
                }

                if (pstParam->bPgSizeBuf || bStartCopied)
                {
                    pCurDataPos += BYTES_PER_SECTOR;
                }
                pCurBufPos += BYTES_PER_SECTOR;
            }

            nSrcBitmap = nBufSrcBitmap;
            nSrcVpn = nBufSrcVpn;

            pstParam->nBitmap = pstParam->nBitmap | nLogSrcBitmap;
            pstParam->pData      = (UINT8 *)(pstZone->pTempMetaPgBuf);
            pstParam->bPgSizeBuf = TRUE32;
        }
        else if (nLogSrcBitmap && !nBufSrcBitmap)
        {
            nSrcVpn = nLogSrcVpn;
            nSrcBitmap = nLogSrcBitmap;
        }
        else if (!nLogSrcBitmap && nBufSrcBitmap)
        {
            nSrcVpn = nBufSrcVpn;
            nSrcBitmap = nBufSrcBitmap;
        }
        else
        {
            nSrcVpn = NULL_VPN;
            nSrcBitmap = 0;
        }

        if(bBufferWrite)
        {
            /*
             * When current clean page offset equals zero, we need to prepare empty buffer block.
             * The new buffer block is obtained from free list
             * NOTE: the context has to be stored after the current buffer page has been written completely.
             */
            if (pstBUCtx->nBBlkCPOffset >= nBufNumMaxPgs)
            {
                FSR_ASSERT(pstCtxFm->nBBlkVbn != NULL_VBN);

                /* Swap VBNs & ECs */
                nBBlkVbn = pstCtx->pFreeList[pstCtxFm->nFreeListHead];
                nBBlkEC  = pstCtx->pFBlksEC [pstCtxFm->nFreeListHead] + 1;

                pstCtx->pFreeList[pstCtxFm->nFreeListHead] = pstCtxFm->nBBlkVbn;
                pstCtx->pFBlksEC [pstCtxFm->nFreeListHead] = pstCtxFm->nBBlkEC;

                pstCtxFm->nBBlkVbn = nBBlkVbn;
                pstCtxFm->nBBlkEC  = nBBlkEC;

                /* Get buffer blocks' VBN */
                nRet = FSR_STL_FlashErase(pstZone, nBBlkVbn);
                if (nRet != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
                    return nRet;
                }

                FSR_ASSERT(nBBlkVbn != NULL_VBN);

                pstBUCtx->nBBlkCPOffset = 0;
            }
            else
            {
                /* Get buffer blocks' VBN */
                nBBlkVbn = pstCtxFm->nBBlkVbn;
                FSR_ASSERT(nBBlkVbn != NULL_VBN);
            }

            /*
             * Destination address : inside of buffer block
             * VPN to write the context
             */
            if (pstDev->nDeviceType == RBW_DEVTYPE_MLC)
            {
                nDstVpn = (nBBlkVbn << pstDev->nPagesPerSbShift)
                              + FSR_BML_GetVPgOffOfLSBPg(pstZone->nVolID, pstBUCtx->nBBlkCPOffset);
            }
            else
            {
                nDstVpn = (nBBlkVbn << pstDev->nPagesPerSbShift)
                        + pstBUCtx->nBBlkCPOffset;
            }

            pstParam->bUserData = FALSE32;
            pstParam->bSpare = TRUE32;

            /*
             * Prepare spare data
             * First, fIll SData1 ~ SData2 as follows
             *      SData1: LPN
             *      SData2: ~LPN
             *      SData3: PTF.~PTF
             */
            pstParam->nSData1 = nLpn;
            pstParam->nSData2 = ~nLpn;

            /* Fill PTF into SData3 */
            pstParam->nSData3 = FSR_STL_SetPTF(pstZone, TF_BU, 0, 0,
                                    sizeof(SBITMAP) * 8 - (UINT8)FSR_STL_GetZBC((UINT8 *)( &nDstBitmap), sizeof(SBITMAP)));
            pstParam->nSData3 = (pstParam->nSData3 << 16) | (0xffff & (~pstParam->nSData3));
        }
        else
        {
            if(nBufSrcBitmap)
            {
                bConfirmPage = TRUE32;
            }
            /* set spare data 1, 2, 3 */
            FSR_STL_SetLogSData123(pstZone, nLpn, nDstVpn, pstParam, bConfirmPage); 
        }

        
        /* If the target page is deleted, there is no valid data in that logical page.*/
        if ((nSrcBitmap == 0) ||
            (nSrcVpn    == NULL_VPN))
        {
            /* Buffer is filled solely by the source data */
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("VFL_Program : nLpn = %d, nDstVpn = %d(%d)\n"), nLpn, nDstVpn, GET_REMAINDER(nDstVpn, pstDev->
                nPagesPerSbShift) ));

            if(bConfirmPage)
            {
                FSR_STL_ComputeCRCs(pstZone, NULL, pstParam, FALSE32);
                pstParam->pExtParam->nNumExtSData = nNumExtSData;
            }

            nRet = FSR_STL_Convert_ExtParam(pstZone, pstParam);
            nRet = FSR_STL_FlashProgram(pstZone, nDstVpn, pstParam);
        }
        else
        {
            /* Buffer has to be filled by the temporal buffer */
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("VFL_Copyback : nLpn = %d, nSrcVpn = %d, nDstVbn = %d, nDstVpn = %d(%d)\n"), nLpn, nSrcVpn, nDstVpn
                / pstDev->nPagesPerSBlk, nDstVpn, GET_REMAINDER(nDstVpn, pstDev->nPagesPerSbShift) ));

            if(bConfirmPage)
            {
                nRet = FSR_STL_FlashModiCopybackCRC(pstZone, nSrcVpn, nDstVpn, pstParam);
            }
            else
            {        
                nRet = FSR_STL_Convert_ExtParam(pstZone, pstParam);
        
                nRet = FSR_STL_FlashModiCopyback(pstZone, nSrcVpn, nDstVpn, pstParam);
            }
        }

        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
            return nRet;
        }

        if(bBufferWrite)
        {
            /* Update state */
            pstBUCtx->nBufferedLpn = nLpn;
            pstBUCtx->nBufState = STL_BUFSTATE_DIRTY;
            pstBUCtx->nBufferedDgn = pstLogGrp->pstFm->nDgn;
            pstBUCtx->nBufferedVpn = nDstVpn;
            pstBUCtx->nValidSBitmap = nDstBitmap;

            /* Store context info */
            nRet = FSR_STL_StoreBMTCtx(pstZone, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                    (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
                return nRet;
            }

            /* Move clean page offset */
            pstBUCtx->nBBlkCPOffset++;
        }
        else if (nLpn == pstZone->pstBUCtxObj->nBufferedLpn)
        {
            STL_BUFFER_CTX_AFTER_FLUSH(pstZone->pstBUCtxObj);
        }
    }

#if (OP_SUPPORT_STATISTICS_INFO == 1)
    if(!bBufferWrite && nBufSrcBitmap) 
    {
        pstZone->pstStats->nBufferHitCnt++;
    }
#endif

    if(!bBufferWrite)
    {
        /* log group PMT update */
        FSR_STL_UpdatePMT(pstZone, pstLogGrp, pstLog, nLpn, nDstVpn);

        /* change the state of log block */
        FSR_STL_ChangeLogState(pstZone, pstLogGrp, pstLog, nLpn);

#if (OP_SUPPORT_STATISTICS_INFO == 1)
        pstZone->pstStats->nTotalLogPgmCnt++;
#endif  /* (OP_SUPPORT_STATISTICS_INFO == 1) */
    }

    FSR_STL_FreeVFLExtParam(pstZone, pstVFLParam->pExtParam);

    FSR_STL_FreeVFLParam(pstZone, pstVFLParam);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return FSR_STL_SUCCESS;
}

#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */
