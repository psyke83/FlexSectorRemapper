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
 *  @file       FSR_STL_Read.c
 *  @brief      This file contains function related Read process
 *  @author     Wonmoon Cheon
 *  @date       23-MAY-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  23-MAY-2007 [Wonmoon Cheon] : first writing
 *  @n  02-JUL-2007 [Jaesoo Lee]    : v1.2.0 re-design
 *  @n  02-JUL-2007 [Wonhee Cho]    : v1.2.0 modify
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
#define DATA_IN_DELETE_PAGE         (0xFF)

/*****************************************************************************/
/* Local (static) function prototype                                         */
/*****************************************************************************/
PRIVATE INT32   _ReadSinlgePage(STLZoneObj *pstZone,
                                UINT32      nLpn,
                                VFLParam   *pstParam);

PRIVATE INT32   _ReadMultiPages(STLZoneObj *pstZone,
                                UINT32      nLpn,
                                VFLParam   *pstParam,
                                UINT32     *pnRdPages);

PRIVATE VOID    _FillDelPage   (UINT8      *pBuf,
                                UINT32      nSecPerVPg,
                                UINT32      nSBitmap,
                                BOOL32      bPageSzBuf);

/*****************************************************************************/
/* Local (static)  Function Definition                                       */
/*****************************************************************************/

/**
 *  @brief   This function fills zero bytes in the specified buffer.
 *
 *  @param[in]   pBuf           : buffer pointer
 *  @param[in]   nSecPerVPg     : number of sectors per page
 *  @param[in]   nSBitmap       : sector bitmap
 *  @param[in]   bPageSzBuf     : boolean flag that denotes the page size is the same as the page size or not.
 *
 *  @return     none
 *
 *  @author         Wonmoon Cheon
 *  @version        1.0.0
 */
PRIVATE VOID    
_FillDelPage(UINT8  *pBuf,
             UINT32  nSecPerVPg,
             UINT32  nSBitmap,
             BOOL32  bPageSzBuf)
{
    UINT32      nStartOff       = 0;
    UINT32      nSOffs          = 0;
    UINT32      nBitMask        = 0x1;
    
    if (bPageSzBuf == TRUE32)
    {
        /* mask with sector bitmap */
        for (nSOffs = 0; nSOffs < nSecPerVPg; nSOffs++)
        {
            if (nSBitmap & (1 << nSOffs))
            {
                FSR_OAM_MEMSET(pBuf + (nSOffs << BYTES_SECTOR_SHIFT),
                               DATA_IN_DELETE_PAGE,
                               BYTES_PER_SECTOR);
            }
        }
    }
    else
    {
        /* need to find the first sector position */
        nBitMask = 1;
        do
        {
            if ((nSBitmap & nBitMask) != 0)
            {
                /* when we found the first sector */
                break;
            }
            nBitMask <<= 1;
            nSOffs++;
        } while (nSOffs < nSecPerVPg);

        /* buffer address starts from the first sector */
        while (nSOffs < nSecPerVPg)
        {
            if ((nSBitmap & nBitMask) != 0)
            {
                FSR_OAM_MEMSET(pBuf + (nStartOff << BYTES_SECTOR_SHIFT),
                                DATA_IN_DELETE_PAGE,
                                BYTES_PER_SECTOR);
            }
            nStartOff++;
            nBitMask <<= 1;
            nSOffs++;
        }
    }
}

/**
 *  @brief   This function reads the specified single logical page.
 *
 *  @param[in]   pstZone    : STLZoneObj pointer object
 *  @param[in]   nLpn       : logical page number to read
 *  @param[in]   pstParam   : VFLParam pointer object
 *
 *  @return FSR_STL_SUCCESS
 *  @return FSR_STL_INVALID_PARAM
 *  @return FSR_BML_READ_ERROR
 *
 *  @author         Wonmoon Cheon, Wonhee Cho
 *  @version        1.2.0
 */
PRIVATE INT32
_ReadSinlgePage    (STLZoneObj     *pstZone,
                    UINT32          nLpn,
                    VFLParam       *pstParam)
{
    PADDR           nVpn            = 0;
    BOOL32          bPageDeleted    = FALSE32;
    INT32           nRet            = FSR_STL_SUCCESS;
    UINT32          nSecPerVPg;
#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    UINT32          nIdx;
    SBITMAP         nOrgSctBitmap   = 0;
    STLBUCtxObj    *pstBUCtx        = NULL;
    PADDR           nBufSrcVpn      = NULL_VPN;
    SBITMAP         nBufSrcBitmap   = 0;
    UINT8          *pBufPos         = NULL;
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* In this function, we can read only one page */
    FSR_ASSERT(pstParam->nNumOfPgs == 1);

    do
    {
    /* translate LPN to VPN by BMT and PMT. */
    nRet = FSR_STL_GetLpn2Vpn(pstZone, 
                              nLpn, 
                              &nVpn, 
                              &bPageDeleted);
    if (nRet != FSR_STL_SUCCESS)
    {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
            break;
    }

    nSecPerVPg = pstZone->pstDevInfo->nSecPerVPg;

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    pstBUCtx = pstZone->pstBUCtxObj;

    /* check if the target page exists in BU or not */
    if ((pstBUCtx->nBufState != STL_BUFSTATE_CLEAN) && 
        (nLpn == pstBUCtx->nBufferedLpn))
    {
        nOrgSctBitmap   = pstParam->nBitmap;
        nBufSrcBitmap   = pstBUCtx->nValidSBitmap & nOrgSctBitmap;
        nBufSrcVpn      = pstBUCtx->nBufferedVpn;
    }

    /* when some sectors are in buffer block */
    if (nBufSrcBitmap != 0)
    {
        /* replace original bitmap with masked sector bitmap */
        pstParam->nBitmap = nBufSrcBitmap;

        /* read from buffer block */
        nRet = FSR_STL_FlashRead(pstZone,
                                 nBufSrcVpn,
                                 pstParam,
                                 FALSE32); 
        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR, 
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
                break;
        }

        /* remained sectors bitmap */
        pstParam->nBitmap = nOrgSctBitmap & (~nBufSrcBitmap);   

        /* move buffer pointer */
        if ((pstParam->nBitmap != 0) && (pstParam->bPgSizeBuf == FALSE32))
        {                
            pBufPos = (UINT8*) pstParam->pData;
            for (nIdx = 0; nIdx < nSecPerVPg; nIdx++)
            {
                if (nBufSrcBitmap & (1<<nIdx))
                {
                    pBufPos += BYTES_PER_SECTOR;
                }
            }       
            pstParam->pData = pBufPos;
        }
     }
 #endif

    /* when the target is not a deleted page */
    if (pstParam->nBitmap != 0)
    {
        if (bPageDeleted == FALSE32)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
                           (TEXT("VFL_Read : nVpn = %d, nSBitmap = 0x%x\n"), nVpn, pstParam->nBitmap));
            
            nRet = FSR_STL_FlashRead(pstZone, 
                                     nVpn, 
                                     pstParam, 
                                     FALSE32);
            if (nRet != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR, 
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                    break;
            }
        }
        else    /* when the tarage page is deleted, we have to fill zero */
        {
            /* return with all 0x00 filled page */
            _FillDelPage(pstParam->pData,
                         nSecPerVPg,
                         pstParam->nBitmap, 
                         pstParam->bPgSizeBuf);
        }
    }

        nRet = FSR_STL_SUCCESS;
    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}


/**
 *  @brief      This function reads the specified multiple logical pages.
 *
 *  @param[in]  pstZone     : STLZoneObj pointer object
 *  @param[in]  nLpn        : logical page number to read
 *  @param[in]  stParam    : VFLParam pointer object
 *  @param[out] pnRdPages   : the number of read pages
 *
 *  @return     FSR_STL_SUCCESS
 *  @return     FSR_BML_READ_ERROR
 *
 *  @author         Wonmoon Cheon, JaeSoo Lee, Wonhee Cho
 *  @version        1.2.0
 */
PRIVATE INT32
_ReadMultiPages(STLZoneObj *pstZone,
                UINT32      nLpn,
                VFLParam   *pstParam,
                UINT32     *pnRdPages)
{
    PADDR       nVpn            = 0;
    BOOL32      bPageDeleted    = FALSE32;
    BOOL32      bFirst          = TRUE32;
    INT32       nRet;
    PADDR       nPrevVpn        = NULL_VPN;
    PADDR       nStartVpn       = NULL_VPN;
    UINT32      nActualRdPages  = 0;
    UINT32      nSOffs;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    do
    {
        /*translate LPN to VPN by BMT and PMT.*/
        nRet = FSR_STL_GetLpn2Vpn(pstZone,
                                  nLpn,
                                  &nVpn,
                                  &bPageDeleted);

        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR]  --%s() L(%d) : 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return nRet;
        }

        if (bPageDeleted == TRUE32) 
        {
            break;
        }

        if (bFirst == TRUE32)
        {
            nStartVpn = nVpn;
            nPrevVpn  = nVpn;
        }
        else if (nVpn != nPrevVpn + 1)
        {
            break;
        }
        else
        {
            nPrevVpn = nVpn;
        }

        nLpn++;
        nActualRdPages++;

        bFirst = FALSE32;

    } while (*pnRdPages > nActualRdPages);
    
    FSR_ASSERT(pstParam->nBitmap == pstZone->pstDevInfo->nFullSBitmapPerVPg);
    
    pstParam->nNumOfPgs = nActualRdPages;
    
    if (nActualRdPages > 0) 
    {
        nRet = FSR_STL_FlashRead(pstZone,
                                 nStartVpn,
                                 pstParam,
                                 FALSE32);
        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR]  --%s() L(%d) : 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return nRet;
        }
        nRet = FSR_STL_SUCCESS;
    }

    if (bPageDeleted == TRUE32)
    {
        nSOffs = nActualRdPages << pstZone->pstDevInfo->nSecPerVPgShift;
        FSR_OAM_MEMSET(pstParam->pData + (nSOffs << BYTES_SECTOR_SHIFT),
                    DATA_IN_DELETE_PAGE, pstZone->pstDevInfo->nBytesPerVPg);
        nActualRdPages++;
    }

    /*set actual read pages*/
    *pnRdPages = nActualRdPages;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT] --%s\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}


/*****************************************************************************/
/* Global Function Definition                                                */
/*****************************************************************************/

/** 
 *  @brief      This function reads sectors from the specified logical sector number.
 *  @n          When the asynchronous mode is used, user buffer must not be released until
 *  @n          the corresponding event callback function will be called.
 *
 *  @param[in]  nClstID      : volume id
 *  @param[in]  nZoneID     : zone id
 *  @param[in]  nLsn        : start logical sector number (0 ~ (total sectors - 1))
 *  @param[in]  nSectors    : number of sectors to write
 *  @param[in]  pBuf        : user buffer pointer 
 *  @param[in]  nFlag
 *
 *  @return     FSR_STL_SUCCESS
 *  @return     FSR_STL_INVALID_PARAM
 *
 *  @author     Wonmoon Cheon, Wonhee Cho
 *  @version    1.2.0 *
 */
PUBLIC INT32
FSR_STL_ReadZone   (UINT32      nClstID,
                    UINT32      nZoneID,
                    UINT32      nLsn,
                    UINT32      nNumOfScts,
                    UINT8      *pBuf,
                    UINT32      nFlag)
{
    STLZoneObj     *pstZone         = NULL;
    RBWDevInfo     *pstDevInfo      = NULL;
    PADDR           nStartLpn       = 0;
    PADDR           nEndLpn         = 0;
    PADDR           nCurLpn         = 0;
    UINT32          nRdSectors      = 0;
    UINT32          nTotalRdSectors = 0;
    UINT32          nRdPages        = 0;
    VFLParam       *pstParam;
    INT32           nRet            = FSR_STL_SUCCESS;
    UINT32          nPagesPerSBlk;
#if (OP_SUPPORT_RUNTIME_PMT_BUILDING == 1)
    BADDR           nLbn            = NULL_VBN;
    BADDR           nDgn            = NULL_VBN;
#endif /*OP_SUPPORT_RUNTIME_PMT_BUILDING*/
    UINT32          nRdStartLogNum;
    UINT32          nRdEndLogNum;
#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    STLBUCtxObj    *pstBUCtxObj     = NULL;
#endif /*(OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)*/
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /*1. Check Input Parameter : nClstID, nZoneID, nLsn, nNumScts, pBuf*/
    nRet = FSR_STL_CheckArguments(nClstID,
                                  nZoneID,
                                  nLsn,
                                  nNumOfScts,
                                  pBuf);
    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }

    /* 2. Get the pointer to corresponding Zone object */
    pstZone     = &(gpstSTLClstObj[nClstID]->stZoneObj[nZoneID]);
    pstDevInfo  = pstZone->pstDevInfo;

    /* initialize VFL parameter (including extended param) */
    FSR_STL_InitVFLParamPool(gpstSTLClstObj[nClstID]);

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    /* get BU context info pointer */
    pstBUCtxObj = pstZone->pstBUCtxObj;
#endif/*(OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)*/

#if (OP_SUPPORT_PAGE_DELETE == 1)
    /* store previous deleted page info */
    nRet = FSR_STL_StoreDeletedInfo(pstZone);
    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }
#endif
    
    /* 3. Calculate starting LPN and last LPN */
    
    /* start logical page address */
    nStartLpn = nLsn >>  pstDevInfo->nSecPerVPgShift;
    
    /* end logical page address */
    nEndLpn = (nLsn + nNumOfScts - 1) >> pstDevInfo->nSecPerVPgShift;

    nPagesPerSBlk = pstDevInfo->nPagesPerSBlk;

    /* set starting LPN */
    nCurLpn = nStartLpn;

    /* VFL parameter setting */
    pstParam            = FSR_STL_AllocVFLParam(pstZone);
    pstParam->bUserData = TRUE32;
    pstParam->bSpare    = FALSE32;

    /*
                S0 S1 S2 S3 S4 S5 S6 S7
    [head]      |->--------------  : nStartLpn
    [body] -----------------------
    [body] -----------------------
    [tail] --->|                   : nEndLpn
    */
    
    /* 4. Iterate the following processes from starting LPN to last LPN */

    do 
    {
        /* VFL parameter setting */
        pstParam->pData     = pBuf;
        pstParam->nSData1   = nCurLpn;
        pstParam->nNumOfPgs = 1;

#if (OP_SUPPORT_RUNTIME_PMT_BUILDING == 1)
        /* get DGN(data group number) according to the specified LPN */
        nLbn = (BADDR)(nCurLpn >> pstDevInfo->nPagesPerSbShift);
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

        /* 5. Calculate the numbers of the sectors */

        if (nCurLpn < nEndLpn)
        {
            nRdSectors = pstDevInfo->nSecPerVPg - (nLsn & (pstDevInfo->nSecPerVPg - 1));
        }
        else
        {
            nRdSectors = (nNumOfScts - nTotalRdSectors);
        }
            
        /* 6. Calculate the Sector Bitmap */
        if ((nCurLpn > nStartLpn) && (nCurLpn < nEndLpn))
        {
            /* body pages bitmap */
            pstParam->nBitmap = pstDevInfo->nFullSBitmapPerVPg;
        }
        else
        {
            /* head, tail page BITMAP */
            pstParam->nBitmap = FSR_STL_SetSBitmap(nLsn, nRdSectors, pstDevInfo->nSecPerVPg);
        }

        /* 7. ReadSinglePage */
        if (nRdSectors == pstDevInfo->nSecPerVPg)
        {
            pstParam->bPgSizeBuf = TRUE32;
        }
        else
        {
            pstParam->bPgSizeBuf = FALSE32;
        }

        /* read multiple pages*/
        if (pstParam->nBitmap == pstDevInfo->nFullSBitmapPerVPg)
        {
            /* body pages (full sector bitmap)*/
            nRdSectors = (nNumOfScts - nTotalRdSectors);
            /* if (nRdSectors % pstDevInfo->nSecPerVPg) */
            if (nRdSectors & (pstDevInfo->nSecPerVPg - 1))
            {
                nRdPages = (nEndLpn - nCurLpn);
                nRdSectors = nRdPages * pstDevInfo->nSecPerVPg;
            }
            else
            {
                nRdPages = (nEndLpn - nCurLpn) + 1;
            }

            nRdStartLogNum = nCurLpn >> pstDevInfo->nPagesPerSbShift;
            nRdEndLogNum   = nEndLpn >> pstDevInfo->nPagesPerSbShift;
            
            if (nRdStartLogNum != nRdEndLogNum)
            {
                nRdPages   = nPagesPerSBlk - (nCurLpn & (nPagesPerSBlk - 1));
            }
            
            #if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
            if ((nCurLpn <= pstBUCtxObj->nBufferedLpn) && 
                (pstBUCtxObj->nBufferedLpn < (nCurLpn + nRdPages)))
            {
                if (nCurLpn == pstBUCtxObj->nBufferedLpn)
                {
                    nRdPages = 1;
                    nRet = _ReadSinlgePage(pstZone, nCurLpn, pstParam);
                }
                else
                {
                    nRdPages = (pstBUCtxObj->nBufferedLpn - nCurLpn);
                    nRet = _ReadMultiPages(pstZone, nCurLpn, pstParam, &nRdPages);
                }
            }
            else
            {
                nRet = _ReadMultiPages(pstZone, nCurLpn, pstParam, &nRdPages);
            }
            #else/*(OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)*/
                nRet = _ReadMultiPages(pstZone, nCurLpn, pstParam, &nRdPages);

            #endif/*(OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)*/
            /* nRdSectors : actual read sectors*/
            nRdSectors = nRdPages * pstDevInfo->nSecPerVPg;
        }
        else
        {
            /* head, tail page which has not full sector bitmap.*/
            nRdPages = 1;

            nRet = _ReadSinlgePage(pstZone, nCurLpn, pstParam);
        }

        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        /* 8. Add LPN, total sector numbers, and pBuf*/
        
        /* set to the next page*/
        nCurLpn += nRdPages;
        
        nLsn += nRdSectors;
        nTotalRdSectors += nRdSectors;
        pBuf += (BYTES_PER_SECTOR * nRdSectors);
        
    } while (nCurLpn <= nEndLpn);

    /* release VFL parameter */
    FSR_STL_FreeVFLParam(pstZone, pstParam);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}
