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
 *  @file       FSR_STL_FlashIO.c
 *  @brief      BML interface functions are implemented.
 *  @author     Wonmoon Cheon
 *  @date       02-MAR-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  02-MAR-2007 [Wonmoon Cheon] : first writing
 *  @n  14-JUN-2007 [Wonhee.Cho]    : Add BML supporting
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
#define BML_SPARE_EXT_SHIFT                 (2)
                        /* ((sizeof (FSRSpareBufExt)) / sizeof (UINT32)) */

/*****************************************************************************/
/* the local constant definitions                                            */
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

PRIVATE VOID    _SplitBitnumCheck  (STLZoneObj     *pstZone,
                                    const UINT32    nBitmap,
                                          UINT32   *pnSBitNum,
                                          UINT32   *pnEBitNum);

/*****************************************************************************/
/* Local (static)  Function Definition                                       */
/*****************************************************************************/

/*
 * @brief       This function gets the start & end bit num of continuous bit
 * @n           stream and the status of splitting
 *
 * @param[in]   pstZone         : Zone object
 * @param[in]   nBitmap         : bitmap
 * @param[out]  pnSBitNum       : start bit number of 1
 * @param[out]  pnEBitNum       : end   bit number of 1
 *
 * @return      TRUE32          : split
 * @return      FALSE32         : not split
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PRIVATE VOID
_SplitBitnumCheck  (STLZoneObj     *pstZone,
                    const UINT32    nBitmap,
                          UINT32   *pnSBitNum,
                          UINT32   *pnEBitNum)
{
    const UINT32    nSctsPerPg = pstZone->pstDevInfo->nSecPerVPg;
    UINT32          nIdx;
    UINT32          nBitMask;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%x, %x, %x, %x)\r\n"),
            __FSR_FUNC__, pstZone, nBitmap, pnSBitNum, pnEBitNum));

    /* Initialize result to full page */
    *pnSBitNum = 0;
    *pnEBitNum = nSctsPerPg;

    /* Find the first 1 bit index from 0*/
    nBitMask = 1;
    nIdx     = 0;
    do 
    {
        if ((nBitmap & nBitMask) != 0)
        {
            *pnSBitNum = nIdx;
            break;
        }

        nBitMask <<= 1;
        nIdx++;
    } while(nIdx < nSctsPerPg);

#if 0
    /* Find the next 0 bit index */
    nBitMask <<= 1;
    nIdx++;
    while (nIdx < nSctsPerPg)
    {
        if ((nBitmap & nBitMask) == 0)
        {
            *pnEBitNum = nIdx;
            break;
        }

        nBitMask <<= 1;
        nIdx++;
    }
#else
    /* Find the first 1 bit from last */
    nBitMask = 1 << (nSctsPerPg - 1);
    nIdx     = nSctsPerPg;
    do 
    {
        if ((nBitmap & nBitMask) != 0)
        {
            *pnEBitNum = nIdx;
            break;
        }

        nBitMask >>= 1;
        nIdx--;
    } while(nIdx > 0);
    FSR_ASSERT(*pnEBitNum >= *pnSBitNum);
#endif

#if (OP_STL_DEBUG_CODE == 1)

    /* Check the split bitmap */
    nBitMask <<= 1;
    nIdx++;
    while (nIdx < nSctsPerPg)
    {
        if ((nBitmap & nBitMask) != 0)
        {
            FSR_ASSERT(FALSE32);
        }

        nBitMask <<= 1;
        nIdx++;
    }

#endif
    /* Check full page bitmap */
    FSR_ASSERT((*pnSBitNum != 0) ||
               (*pnEBitNum != nSctsPerPg));

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d)\r\n"),
            __FSR_FUNC__, __LINE__));
}

/*****************************************************************************/
/* Global Function Definition                                                */
/*****************************************************************************/

/**
 * @brief       This function converts  STL params of read to BML params
 *
 * @param[in]   pstZone                 : Zone object
 * @param[in]   nVpn
 * @param[in]   pstParam
 * @param[in]   bCleanCheck
 *
 * @return      FSR_BML_SUCCESS         : VFL success.
 * @return      FSR_STL_CLEAN_PAGE      :
 * @return      FSR_BML_READ_ERROR      :
 * @return      FSR_BML_CRITICAL_ERROR  : There is unexpected error.
 *
 * @author      Wonhee Cho
 * @version     1.0.0
 *
 */
PUBLIC INT32
FSR_STL_FlashRead  (STLZoneObj *pstZone,
                    UINT32      nVpn,
                    VFLParam   *pstParam,
                    BOOL32      bCleanCheck)
{
          STLClstObj   *pstClst         = FSR_STL_GetClstObj(pstZone->nClstID);
          FSRSpareBuf  *pstSBuf;
    const RBWDevInfo   *pstDI           = pstZone->pstDevInfo;
    const UINT32        nFBitmap        = pstDI->nFullSBitmapPerVPg;
    const UINT32        nSctsPerPg      = pstDI->nSecPerVPg;
          UINT32        nIdx;
          UINT32        nVpnBML;
          UINT8        *pData;
          UINT32        nFlag;
          UINT32        nSBitNum;
          UINT32        nEBitNum;
          UINT32       *pnTmpBuf;
          UINT32        nDWPerPg;
          UINT32        nNumExtSData    = 0;
          INT32         nErr            = FSR_BML_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%d, %d, %x)\r\n"),
            __FSR_FUNC__, pstZone->nZoneID, nVpn, pstParam));
    /* check the main and spare buffer pointer */
    FSR_ASSERT((pstParam->pData  != NULL   ) || 
               (pstParam->bSpare == TRUE32));

    /* Setup Start bit, End bit and BML flag */
    if (pstParam->nBitmap != nFBitmap)
    {
        FSR_ASSERT(pstParam->nNumOfPgs == 1   );
        FSR_ASSERT(pstParam->pData     != NULL);
        FSR_ASSERT(pstParam->nBitmap   != 0   );

        /* Get Read flag for partial page read */
        _SplitBitnumCheck(pstZone,
                          pstParam->nBitmap,
                          &nSBitNum,
                          &nEBitNum);

        /************************************************************************************************
        nFlag :
        31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
        ----------------------- ----------------------- -----------                --    --    --    --
        The Sct offset value    The Sct offset value    priorities                OTP   R1  Flush FOTMAT
        for 1st page             for last page        for OpQ                              Op
        *************************************************************************************************/
        nFlag    = (nSBitNum << 24) | ((nSctsPerPg - nEBitNum) << 16);
        if (nEBitNum == 0)
        {
            pData = NULL;
        }
        else
        {
            if (pstParam->bPgSizeBuf == TRUE32)
            {
                pData = pstParam->pData + (nSBitNum << BYTES_SECTOR_SHIFT);
            }
            else
            {
                pData = pstParam->pData;
            }
        }
    }
    else
    {
        nSBitNum = 0;
        nEBitNum = nSctsPerPg;
        nFlag    = FSR_BML_FLAG_NONE;
        pData    = pstParam->pData;
    }

    /* Translate VFL address space to BML's */
    nVpnBML = pstClst->pstEnvVar->nStartVpn + nVpn;

    /* Set default flag for BML read */
    if (pstParam->bSpare == TRUE32)
    {
        FSR_ASSERT(pstParam->nNumOfPgs == 1);

        /* Store spare buffer */
        nFlag  |= FSR_BML_FLAG_USE_SPAREBUF;
        pstSBuf = pstClst->staSBuf;
        if ((pstParam->pExtParam               != NULL) &&
            (pstParam->pExtParam->nNumExtSData >  0   ))
        {
            FSR_ASSERT(pstParam->pExtParam->nNumExtSData <= FSR_STL_SPARE_EXT_MAX_SIZE);
            nNumExtSData           = (pstDI->nSecPerVPg) >> BML_SPARE_EXT_SHIFT;
            pstSBuf->pstSTLMetaExt = (FSRSpareBufExt*)(pstParam->pExtParam->aExtSData);
        }
        else
        {
            nNumExtSData          = 0;
            pstSBuf->pstSTLMetaExt = NULL;
        }
        pstSBuf->nNumOfMetaExt = nNumExtSData;
    }
    else
    {
        FSR_ASSERT(bCleanCheck == FALSE32);
        pstSBuf = NULL;
        if (pData == NULL)
        {
            nErr = FSR_STL_INVALID_PARAM;
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() L(%d) : %x\r\n"),
                    __FSR_FUNC__, __LINE__, nErr));
            return nErr;
        }
    }

#if (OP_SUPPORT_MSB_PAGE_WAIT == 1)
    /* Reset MSB page written flag */
    FSR_OAM_MEMSET(pstClst->baMSBProg, FALSE32, sizeof (pstClst->baMSBProg));
#endif

    /* Reset transaction begin mark */
    pstClst->bTransBegin = FALSE32;

    /* Read page from BML */
    nErr = FSR_BML_Read(pstZone->nVolID,        /* Volume ID for BML    */
                        nVpnBML,                /* VPN for BML          */
                        pstParam->nNumOfPgs,    /* The number of Pgs    */
                        pData,                  /* Data buffer          */
                        pstSBuf,                /* Spare buffer         */
                        nFlag);                 /* Read Flag            */
    /* Convert spare buffer regardless of return value */
    if (pstParam->bSpare == TRUE32)
    {
        pstParam->nSData1 = pstSBuf->pstSpareBufBase->nSTLMetaBase0;
        pstParam->nSData2 = pstSBuf->pstSpareBufBase->nSTLMetaBase1;
        pstParam->nSData3 = pstSBuf->pstSpareBufBase->nSTLMetaBase2;
    }
    /* Check the return value of FSR_BML_Read */
    if (nErr != FSR_BML_SUCCESS)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nErr));
        FSR_ASSERT(nErr != FSR_BML_VOLUME_NOT_OPENED);
        FSR_ASSERT(nErr != FSR_BML_INVALID_PARAM);
        return nErr;
    }

    /* Check.clean page */
    if (bCleanCheck == TRUE32)
    {
        FSR_ASSERT(pstParam->pData     != NULL);
        FSR_ASSERT(pstParam->nNumOfPgs == 1   );
        FSR_ASSERT(pstParam->bSpare    == TRUE32);

        /* pstParam->pData must be aligned */
        FSR_ASSERT(((UINT32)((pstParam->pData)) & (sizeof(UINT32) - 1)) == 0);
        pnTmpBuf = (UINT32*)(pstParam->pData);
        /* nBytesPerVpg / sizeof(UINT32) */
        nDWPerPg = ((nEBitNum - nSBitNum) << (BYTES_SECTOR_SHIFT)) >> 2;
        for (nIdx = 0; nIdx < nDWPerPg; nIdx++)
        {
            if (*pnTmpBuf != 0xFFFFFFFF)
            {
                break;
            }
            pnTmpBuf++;
        }

        if (nIdx == nDWPerPg)
        {
            /*  Check clean spare data */
            if ((pstSBuf->pstSpareBufBase->nSTLMetaBase0 == 0xFFFFFFFF) &&
                (pstSBuf->pstSpareBufBase->nSTLMetaBase1 == 0xFFFFFFFF) &&
                (pstSBuf->pstSpareBufBase->nSTLMetaBase2 == 0xFFFFFFFF))
            {
                pnTmpBuf = pstParam->pExtParam->aExtSData;
                nNumExtSData <<= BML_SPARE_EXT_SHIFT;     /* multiply by 4 */

                for (nIdx = 0; nIdx < nNumExtSData; nIdx++)
                {
                    if ((*pnTmpBuf) != 0xFFFFFFFF)
                    {
                        break;
                    }
                    pnTmpBuf++;
                }
                if (nIdx == nNumExtSData)
                {
                    nErr = FSR_STL_CLEAN_PAGE;
                }
            }
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nErr));
    return nErr;
}

/**
 * @brief       This function converts  STL params of program to BML params
 *
 * @param[in]   pstZone                     : Zone object
 * @param[in]   nVpn                        : 
 * @param[in]   pstParam                    : 
 *
 * @return      FSR_BML_SUCCESS             : VFL success.
 * @return      FSR_BML_CRITICAL_ERROR      : There is unexpected error.
 * @return      FSR_BML_WRITE_ERROR         :
 * @return      FSR_BML_WR_PROTECT_ERROR    :
 *
 * @author      Wonhee Cho
 * @version     1.0.0
 *
 */
PUBLIC INT32
FSR_STL_FlashProgram   (STLZoneObj *pstZone,
                        UINT32      nVpn,
                        VFLParam   *pstParam)
{
          STLClstObj       *pstClst         = FSR_STL_GetClstObj(pstZone->nClstID);
          BMLCpBkArg      **ppstBMLCpBk;
          BMLCpBkArg       *pstCpBkArg;
          BMLRndInArg      *pstRndInArg;
    const RBWDevInfo       *pstDI           = pstZone->pstDevInfo;
    const UINT32            nFBitmap        = pstDI->nFullSBitmapPerVPg;
    const UINT32            nReqPgs         = pstParam->nNumOfPgs;
    const UINT32            nNumWay         = pstDI->nNumWays;
    const UINT32            nPgsPerBlk      = pstDI->nPagesPerSBlk;
    const UINT32            nPgsSft         = pstDI->nPagesPerSbShift;
    const UINT32            nPgsMsk         = nPgsPerBlk - 1;
    const UINT32            n1stVun         = pstClst->pstEnvVar->nStartVbn;
    const UINT32            n1stVpn         = pstClst->pstEnvVar->nStartVpn;
          UINT32            nWay            = nVpn & (nNumWay - 1);
          UINT32            nVpnBML;
          UINT32            nFlag;
          UINT32            nSBitNum;
          UINT32            nEBitNum;
          UINT32            nPgs;
          UINT32            nPOff;
          UINT32            nLPO;
          UINT32            nPTF;
          BOOL32            bLSBPg;
          BOOL32            bWait           = FALSE32;
          INT32             nErr;
          FSRSpareBuf      *pstSBuf;
          FSRSpareBufBase  *pstSBufBase;

    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%d, %d, %x)\r\n"),
            __FSR_FUNC__, pstZone->nZoneID, nVpn, pstParam));

    /* check the main and spare buffer pointer */
    FSR_ASSERT(pstParam->pData  != NULL  );
    FSR_ASSERT(pstParam->bSpare == TRUE32);

    if ((pstParam->nBitmap & nFBitmap) != nFBitmap)
    {
        /* In case of partial page program */
        FSR_ASSERT (nReqPgs == 1);

        /* 1. Set main data for partial page write */
        _SplitBitnumCheck(pstZone,
                          pstParam->nBitmap,
                         &nSBitNum,
                         &nEBitNum);

        ppstBMLCpBk = pstClst->pstBMLCpBk;
        pstCpBkArg  = pstClst->staBMLCpBk;
        pstRndInArg = pstClst->staBMLRndIn;

        /* 2. Set Random-In argument for partial page write */
        pstRndInArg[0].nOffset     = (UINT16)((nSBitNum           ) << BYTES_SECTOR_SHIFT);
        pstRndInArg[0].nNumOfBytes = (UINT16)((nEBitNum - nSBitNum) << BYTES_SECTOR_SHIFT);
        if (pstParam->bPgSizeBuf == TRUE32)
        {
            pstRndInArg[0].pBuf    = pstParam->pData + (nSBitNum << BYTES_SECTOR_SHIFT);
        }
        else
        {
            pstRndInArg[0].pBuf    = pstParam->pData;
        }

        /* Set spare buffer of STL base for full page write; */
        pstSBuf = pstClst->staSBuf;
        pstSBuf->pstSpareBufBase->nSTLMetaBase0 = pstParam->nSData1;
        pstSBuf->pstSpareBufBase->nSTLMetaBase1 = pstParam->nSData2;
        pstSBuf->pstSpareBufBase->nSTLMetaBase2 = pstParam->nSData3;
        pstRndInArg[1].nNumOfBytes = (UINT16)(FSR_SPARE_BUF_STL_BASE_SIZE);
        pstRndInArg[1].nOffset     = (UINT16)(FSR_STL_CPBK_SPARE_BASE);
        pstRndInArg[1].pBuf        = &(pstSBuf->pstSpareBufBase->nSTLMetaBase0);

        /* Set spare buffer of STL extension for full page write; */
        if (pstParam->pExtParam != NULL)
        {
            FSR_ASSERT(pstParam->pExtParam->nNumExtSData <= FSR_STL_SPARE_EXT_MAX_SIZE);
            pstRndInArg[2].nNumOfBytes = (UINT16)((pstParam->pExtParam->nNumExtSData) << 2);
                                                /* multiply sizeof (UINT32) */
            pstRndInArg[2].nOffset     = (UINT16)(FSR_STL_CPBK_SPARE_EXT);
            pstRndInArg[2].pBuf        = pstParam->pExtParam->aExtSData;

            pstCpBkArg->nRndInCnt        = 3;         /*   Data & spare  */
        }
        else
        {
            pstCpBkArg->nRndInCnt        = 2;         /*   Data & spare  */
        }

        /* 2. Set BML CopyBack argument for partial page write */
        /* Set source to the last page of Root unit */
        pstCpBkArg->nSrcVun      = (UINT16)(n1stVun);
        pstCpBkArg->nSrcPgOffset = (UINT16)(nPgsPerBlk + nWay - nNumWay);
        pstCpBkArg->nDstVun      = (UINT16)((nVpn >> nPgsSft) + n1stVun);
        pstCpBkArg->nDstPgOffset = (UINT16)(nVpn & (nPgsPerBlk - 1));
        pstCpBkArg->pstRndInArg  = pstRndInArg;

        /* reset pointer array to NULL */
        FSR_OAM_MEMSET(ppstBMLCpBk, 0, sizeof (long) * FSR_MAX_WAYS);
        ppstBMLCpBk[nWay]= pstCpBkArg;

        /* 3. Call CopyBack of BML */
#if defined (FSR_POR_USING_LONGJMP)
        FSR_FOE_BeginWriteTransaction(0);
#endif
        nErr = FSR_BML_CopyBack(pstZone->nVolID,
                                ppstBMLCpBk,
                                FSR_BML_FLAG_NONE);
        if (nErr != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] %s(), L(%d) - FSR_BML_CopyBack return error (nSrcVun=%d nSrcPgOffset=%d nDstVun=%d nSrcPgOffset=%d) (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, pstCpBkArg->nSrcVun, pstCpBkArg->nSrcPgOffset, pstCpBkArg->nDstVun, pstCpBkArg->nDstPgOffset, nErr));
            
            FSR_ASSERT(nErr != FSR_BML_VOLUME_NOT_OPENED);
            FSR_ASSERT(nErr != FSR_BML_WR_PROTECT_ERROR);
        }
    }
    else
    {
        /* Setup spare buffer */
        if (pstParam->bSpare == TRUE32)
        {
            if (nReqPgs > 1)
            {
                /* Multi-page write option is allowed only user data */
                FSR_ASSERT(pstParam->bUserData == TRUE32);
                /* We use pTempPgBuf, so the number of pages is restrict by the spare buffer size */
                FSR_ASSERT(nReqPgs * (sizeof(FSRSpareBuf) + sizeof(FSRSpareBufBase)) < pstDI->nBytesPerVPg);
                
                /* Set FSRSpareBuf and FSRSpareBufBase */
                pstSBuf     = (FSRSpareBuf *) pstClst->pTempPgBuf;
                pstSBufBase = (FSRSpareBufBase *)(pstClst->pTempPgBuf + nReqPgs * sizeof(FSRSpareBuf));

                nPOff       = (nVpn & nPgsMsk);
                FSR_ASSERT(( nVpn            >> nPgsSft) == ((nVpn           + nReqPgs -1) >> nPgsSft));
                FSR_ASSERT(((pstParam->nLpn) >> nPgsSft) == ((pstParam->nLpn + nReqPgs -1) >> nPgsSft));

                for (nPgs = 0; nPgs < nReqPgs; nPgs++)
                {
                    /* Assign the address of pstSpareBufBase */
                    pstSBuf->pstSpareBufBase = pstSBufBase;

                    nLPO = GET_REMAINDER(pstParam->nLpn + nPgs,
                                         pstDI->nPagesPerSbShift + pstZone->pstRI->nNShift);

                    bLSBPg = FSR_STL_FlashIsLSBPage(pstZone, nPOff);
                    if (bLSBPg == TRUE32)
                    {
                        /* in case of LSB page of MLC device or normal page of SLC device*/
                        pstSBuf->pstSpareBufBase->nSTLMetaBase0 = (nLPO << 16) | (nLPO ^ 0xFFFF);
                        pstSBuf->pstSpareBufBase->nSTLMetaBase1 = 0xFFFFFFFF;
                        nPTF = FSR_STL_SetPTF(pstZone, TF_LOG, 0, MIDLSB, 0);
                    }
                    else
                    {
                        /* in case of MSB page of MLC device */
                        pstSBuf->pstSpareBufBase->nSTLMetaBase0 = 0xFFFFFFFF;
                        pstSBuf->pstSpareBufBase->nSTLMetaBase1 = (nLPO << 16) | (nLPO ^ 0xFFFF);
                        nPTF = FSR_STL_SetPTF(pstZone, TF_LOG, 0, MIDMSB, 0);
                    }
                    pstSBuf->pstSpareBufBase->nSTLMetaBase2 = (nPTF << 16) | (0xffff & (~nPTF));

                    pstSBuf->nNumOfMetaExt = 0;
                    pstSBuf->pstSTLMetaExt = NULL;
                    
                    /* To next FSRSpareBuf */
                    pstSBuf++;
                    pstSBufBase++;

                    nPOff++;
                }
                
                pstSBuf--;

                if ((pstParam->pExtParam               != NULL) &&
                    (pstParam->pExtParam->nNumExtSData >  0   ))
                {
                    FSR_ASSERT(pstParam->pExtParam->nNumExtSData <= FSR_STL_SPARE_EXT_MAX_SIZE);
                    pstSBuf->nNumOfMetaExt = (pstDI->nSecPerVPg) >> BML_SPARE_EXT_SHIFT;
                    pstSBuf->pstSTLMetaExt =(FSRSpareBufExt *)(pstParam->pExtParam->aExtSData);
                }
                else
                {
                    pstSBuf->nNumOfMetaExt = 0;
                    pstSBuf->pstSTLMetaExt = NULL;
                }
                pstSBuf = (FSRSpareBuf *)(pstClst->pTempPgBuf);
            }
            else
            {
                pstSBuf = pstClst->staSBuf;

                pstSBuf->pstSpareBufBase->nSTLMetaBase0 = pstParam->nSData1;
                pstSBuf->pstSpareBufBase->nSTLMetaBase1 = pstParam->nSData2;
                pstSBuf->pstSpareBufBase->nSTLMetaBase2 = pstParam->nSData3;

                if ((pstParam->pExtParam               != NULL) &&
                    (pstParam->pExtParam->nNumExtSData >  0   ))
                {
                    FSR_ASSERT(pstParam->pExtParam->nNumExtSData <= FSR_STL_SPARE_EXT_MAX_SIZE);
                    pstSBuf->nNumOfMetaExt = (pstDI->nSecPerVPg) >> BML_SPARE_EXT_SHIFT;
                    pstSBuf->pstSTLMetaExt =(FSRSpareBufExt *)(pstParam->pExtParam->aExtSData);
                }
                else
                {
                    pstSBuf->nNumOfMetaExt = 0;
                    pstSBuf->pstSTLMetaExt = NULL;
                }
            }

            nFlag   = FSR_BML_FLAG_USE_SPAREBUF;
        }
        else
        {
            pstSBuf = NULL;
            nFlag   = FSR_BML_FLAG_NONE;
        }

        /* Translate VFL address space to BML's */
        nVpnBML = n1stVpn + nVpn;

        /* Wait program of previous transaction */
        if (nNumWay > 1)
        {
            bLSBPg = FSR_STL_FlashIsLSBPage(pstZone, nVpn);
            if (pstClst->bTransBegin == TRUE32)
            {
                pstClst->bTransBegin = FALSE32;
                bWait = TRUE32;
            }
#if (OP_SUPPORT_MSB_PAGE_WAIT == 1)
            else if (bLSBPg == TRUE32)
            {
                /* Check MSB page is witting,
                 * if the first page is LSB page for POR */
                for (nWay = 0; nWay < nNumWay; nWay++)
                {
                    if (pstClst->baMSBProg[nWay] == TRUE32)
                    {
                        break;
                    }
                }
                if (nWay < nNumWay)
                {
                    bWait = TRUE32;
                }
            }
#endif

            if (bWait == TRUE32)
            {
            /* We must wait to finish the writing of the previous MSB page */
                nErr = FSR_BML_FlushOp(pstZone->nVolID, FSR_BML_FLAG_NORMAL_MODE);
                if (nErr != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR] %s() L(%d) - FSR_BML_FlushOp return error (%x)\r\n"),
                            __FSR_FUNC__, __LINE__, nErr));
                    FSR_ASSERT(nErr != FSR_BML_VOLUME_NOT_OPENED);

                    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nErr));
                    return nErr;                
                }
            }
        }

#if defined (FSR_POR_USING_LONGJMP)
        FSR_FOE_BeginWriteTransaction(0);
#endif
        nErr = FSR_BML_Write(pstZone->nVolID,
                             nVpnBML,
                             pstParam->nNumOfPgs,
                             pstParam->pData,
                             pstSBuf,
                             nFlag);
        if (nErr != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] %s() L(%d) - FSR_BML_Write return error (%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nErr));
            FSR_ASSERT(nErr != FSR_BML_VOLUME_NOT_OPENED);
            FSR_ASSERT(nErr != FSR_BML_WR_PROTECT_ERROR);
        }
    }

    /* Set MSB page write flag,
     * if all MSB pages are not written.
     */
#if (OP_SUPPORT_MSB_PAGE_WAIT == 1)
    FSR_OAM_MEMSET(pstClst->baMSBProg, FALSE32, sizeof (pstClst->baMSBProg));
    nPOff = ((nVpn + nReqPgs - 1) & nPgsMsk);
    nWay  = nPOff & (nNumWay - 1);
    nPgs  = (nReqPgs > nNumWay) ? nNumWay : nReqPgs;
    if ((nWay < nNumWay - 1) ||
        (nPgs < nNumWay))
    {
        while (nPgs != 0)
        {
            bLSBPg = FSR_STL_FlashIsLSBPage(pstZone, nPOff);
            pstClst->baMSBProg[nWay] = !(bLSBPg);

            if (nWay == 0)
            {
                nWay = (nNumWay - 1);
            }
            else
            {
                nWay --;
            }
            FSR_ASSERT((nPOff > 0) || (nPgs == 1));
            nPOff--;
            nPgs--;
        }
    }
#endif

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nErr));
    return nErr;
}

/**
 * @brief       This function directs the erase operation to BML.
 *
 * @param[in]   pstZone                     : Zone object
 * @param[in]   nVbn                        : Virtual Block Number
 *
 * @return      FSR_BML_SUCCESS             : VFL erase is success.
 * @return      FSR_BML_CRITICAL_ERROR      : There is unexpected error.
 * @return      FSR_BML_ERASE_ERROR         : There is unexpected error.
 * @return      FSR_BML_WR_PROTECT_ERROR    : There is unexpected error.
 *
 * @author      Wonhee Cho
 * @version     1.0.0
 *
 */
PUBLIC INT32
FSR_STL_FlashErase (STLZoneObj *pstZone,
                    UINT32      nVbn)
{
    STLClstObj *pstClst         = FSR_STL_GetClstObj(pstZone->nClstID);
    UINT32      nBMLVbn;
    INT32       nErr;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%d, %d)\r\n"),
            __FSR_FUNC__, pstZone->nZoneID, nVbn));

    /* Get Virtual Unit Number for BML */
    nBMLVbn = pstClst->pstEnvVar->nStartVbn + nVbn;

    /* Call erase function of BML */
    nErr = FSR_BML_Erase(pstZone->nVolID,       /* Volume ID for BML        */
                        &nBMLVbn,               /* Virtual Unit Number      */
                         1,                     /* The number of VUN which must be 1    */
                         FSR_BML_FLAG_NONE);    /* Must be FSR_BML_FLAG_NONE            */
    if (nErr != FSR_BML_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] %s() L(%d) - FSR_BML_Erase return error (%x)\r\n"),
                __FSR_FUNC__, __LINE__, nErr));
        FSR_ASSERT(nErr != FSR_BML_VOLUME_NOT_OPENED);
        FSR_ASSERT(nErr != FSR_BML_WR_PROTECT_ERROR);
    }

#if (OP_SUPPORT_MSB_PAGE_WAIT == 1)
    /* Reset MSB page written flag */
    FSR_OAM_MEMSET(pstClst->baMSBProg, FALSE32, sizeof (pstClst->baMSBProg));
#endif

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nErr));
    return nErr;
}

/**
* @brief       This function converts  STL params of copyback to BML params
 *
 * @param[in]   pstZone                     : Zone object
 * @param[in]   nSrcVpn                     : The VPN of Source
 * @param[in]   nDstVpn                     : The VPN of Destination
 * @param[in]   nSBitmap
 *
 * @return      FSR_BML_SUCCESS  :          : VFL success.
 * @return      FSR_BML_CRITICAL_ERROR      : There is unexpected error.
 * @return      FSR_BML_READ_ERROR          :
 * @return      FSR_BML_WRITE_ERROR         :
 * @return      FSR_BML_WR_PROTECT_ERROR    :
 *
 * @author      Wonhee Cho
 * @version     1.0.0
 *
 */
PUBLIC INT32
FSR_STL_FlashCopyback  (STLZoneObj *pstZone,
                        UINT32      nSrcVpn,
                        UINT32      nDstVpn,
                        UINT32      nSBitmap)
{
    VFLParam    stParam;
    INT32       nErr;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%d, %d, %d, %x)\r\n"),
            __FSR_FUNC__, pstZone->nZoneID, nSrcVpn, nDstVpn, nSBitmap));

    stParam.nBitmap   = nSBitmap;
    stParam.bSpare    = FALSE32;
    stParam.pExtParam = NULL;

    nErr = FSR_STL_FlashModiCopyback(pstZone, nSrcVpn, nDstVpn, &stParam);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nErr));
    return nErr;
}

/**
 * @brief       This function converts  STL params of modi-copyback to BML params
 *
 * @param[in]   pstZone                     : Zone object
 * @param[in]   nSrcVpn                     : The VPN of Source
 * @param[in]   nDstVpn                     : The VPN of Destination
 * @param[in]   pstParam                    : argument
 *
 * @return      FSR_BML_SUCCESS  :          : VFL success.
 * @return      FSR_BML_CRITICAL_ERROR      : There is unexpected error.
 * @return      FSR_BML_READ_ERROR          :
 * @return      FSR_BML_WRITE_ERROR         :
 * @return      FSR_BML_WR_PROTECT_ERROR    :
*
* @author      Wonhee Cho
* @version     1.0.0
*
*/
PUBLIC INT32
FSR_STL_FlashModiCopyback  (STLZoneObj *pstZone,
                            UINT32      nSrcVpn,
                            UINT32      nDstVpn,
                            VFLParam   *pstParam)
{
          STLClstObj   *pstClst     = FSR_STL_GetClstObj(pstZone->nClstID);
          FSRSpareBuf  *pstSBuf;
          BMLCpBkArg  **ppstBMLCpBk;
          BMLCpBkArg   *pstCpBkArg;
          VFLRndInArg  *pstRndIn;
    const RBWDevInfo   *pstDI       = pstZone->pstDevInfo;
    const UINT32        nPgsPerBlk  = pstDI->nPagesPerSBlk;
    const UINT32        nPgsSft     = pstDI->nPagesPerSbShift;
    const UINT32        nNumWay     = pstDI->nNumWays;
    const UINT32        nSrcWay     = nSrcVpn & (nNumWay - 1);  /* copyback src ways */
    const UINT32        nDstWay     = nDstVpn & (nNumWay - 1);
    const UINT32        n1stVpn     = pstClst->pstEnvVar->nStartVpn;
    const UINT32        n1stVun     = pstClst->pstEnvVar->nStartVbn;
          UINT32        nNumRndIn;
          UINT8        *pTmpBuf;
          UINT32        nFlag;
          UINT32        nIdx;
#if (OP_SUPPORT_MSB_PAGE_WAIT == 1)
          BOOL32        bIsLSB;
#endif
          INT32         nErr        = FSR_BML_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%d, %d, %d, %x)\r\n"),
            __FSR_FUNC__, pstZone->nZoneID, nSrcVpn, nDstVpn, pstParam));

    /*  1. Address remapping for BML */
    if (pstParam->pExtParam == NULL)
    {
        nNumRndIn = 0;
        pstRndIn  = NULL;
    }
    else
    {
        nNumRndIn = pstParam->pExtParam->nNumRndIn;
        pstRndIn  = pstParam->pExtParam->astRndIn;
        FSR_ASSERT(nNumRndIn < MAX_RND_IN_ARGS - 1);    /* for spare area   */
    }

    /* In case of the way of each source and destination are different */
    if (nSrcWay != nDstWay)
    {
        /* Setup spare buffer */
        nFlag   = FSR_BML_FLAG_USE_SPAREBUF;
        pstSBuf = pstClst->staSBuf;
        pstSBuf->pstSTLMetaExt = pstClst->staSExt;
        pstSBuf->nNumOfMetaExt = (pstDI->nSecPerVPg) >> BML_SPARE_EXT_SHIFT;

        pTmpBuf = pstClst->pTempPgBuf;

        /* 1. Read page from BML */
        nErr = FSR_BML_Read(pstZone->nVolID,        /* Volume ID for BML    */
                            nSrcVpn + n1stVpn,      /* VPN for BML          */
                            1,                      /* The number of Pgs    */
                            pTmpBuf,                /* Data buffer          */
                            pstSBuf,                /* Spare buffer         */
                            nFlag);                 /* Read Flag            */
        if (nErr != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] %s() L(%d) - FSR_BML_Read return error (%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nErr));
            FSR_ASSERT(nErr != FSR_BML_VOLUME_NOT_OPENED);
            FSR_ASSERT(nErr != FSR_BML_INVALID_PARAM);
            return nErr;
        }

        for (nIdx = 0; nIdx < nNumRndIn; nIdx++)
        {
            if (pstRndIn->nOffset < FSR_LLD_CPBK_SPARE)
            {
                /* 2. Modify main data */
                FSR_OAM_MEMCPY(pTmpBuf + pstRndIn->nOffset,
                    pstRndIn->pBuf, pstRndIn->nNumOfBytes);
            }
            else if (pstRndIn->nOffset < FSR_STL_CPBK_SPARE_EXT)
            {
                /* 3 Modify spare area */
                FSR_OAM_MEMCPY((UINT8 *)(pstSBuf->pstSpareBufBase) + (pstRndIn->nOffset - FSR_LLD_CPBK_SPARE),
                    pstRndIn->pBuf, pstRndIn->nNumOfBytes);
            }
            else
            {
                /* 3 Modify spare ext area */
                FSR_OAM_MEMCPY((UINT8 *)(pstSBuf->pstSTLMetaExt) + (pstRndIn->nOffset - FSR_STL_CPBK_SPARE_EXT),
                    pstRndIn->pBuf, pstRndIn->nNumOfBytes);
            }

            pstRndIn++;
        }

#if 0
        /* 3 Modify spare area */
        if (pstParam->bSpare == TRUE32)
        {
            pstSBuf->nSTLMetaBase0 = pstParam->nSData1;
            pstSBuf->nSTLMetaBase1 = pstParam->nSData2;
            pstSBuf->nSTLMetaBase2 = pstParam->nSData3;
            if ((pstParam->pExtParam != NULL) &&
                (pstParam->pExtParam->nNumExtSData > 0))
            {
                FSR_ASSERT(pstParam->pExtParam->nNumExtSData <= MAX_SECTORS_PER_VPAGE);
                FSR_ASSERT(pstParam->pExtParam->nNumExtSData <= FSR_STL_SPARE_EXT_MAX_SIZE);
                FSR_OAM_MEMCPY(pstSBuf->pstSTLMetaExt,
                               pstParam->pExtParam->aExtSData,
                               pstParam->pExtParam->nNumExtSData);
            }
        }
#endif

        /* 4. Write page to BML */
#if defined (FSR_POR_USING_LONGJMP)
        FSR_FOE_BeginWriteTransaction(0);
#endif
        nErr = FSR_BML_Write(pstZone->nVolID,       /* Volume ID for BML    */
                             nDstVpn + n1stVpn,     /* VPN for BML          */
                             1,                     /* The number of Pgs    */
                             pTmpBuf,               /* Data buffer          */
                             pstSBuf,               /* Spare buffer         */
                             nFlag);                /* Write Flag           */
        if (nErr != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] %s() L(%d) - FSR_BML_Write return error (%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nErr));
            FSR_ASSERT(nErr != FSR_BML_VOLUME_NOT_OPENED);
            FSR_ASSERT(nErr != FSR_BML_WR_PROTECT_ERROR);
        }
    }
    else
    {
        /* 2. Set CopyBack Arguments, which are VUN, page offset of source and destination*/
        pstCpBkArg = pstClst->staBMLCpBk;
        pstCpBkArg->nSrcVun          = (UINT16)((nSrcVpn >> nPgsSft) + n1stVun);
        pstCpBkArg->nSrcPgOffset     = (UINT16)(nSrcVpn & (nPgsPerBlk - 1));
        pstCpBkArg->nDstVun          = (UINT16)((nDstVpn >> nPgsSft) + n1stVun);
        pstCpBkArg->nDstPgOffset     = (UINT16)(nDstVpn & (nPgsPerBlk - 1));
        pstCpBkArg->nRndInCnt        = nNumRndIn;
        pstCpBkArg->pstRndInArg      = (BMLRndInArg*)pstRndIn;

        ppstBMLCpBk          = pstClst->pstBMLCpBk;
        FSR_OAM_MEMSET(ppstBMLCpBk, 0, sizeof(long) * FSR_MAX_WAYS);
        ppstBMLCpBk[nSrcWay] = pstCpBkArg;

        /* 2. Call BML CopyBack */
#if defined (FSR_POR_USING_LONGJMP)
        FSR_FOE_BeginWriteTransaction(0);
#endif
        nErr = FSR_BML_CopyBack(pstZone->nVolID,    /* The Volume ID for BML            */
                                ppstBMLCpBk,        /* The copyback argument for BML    */
                                FSR_BML_FLAG_NONE); /* Must be FSR_BML_FLAG_NONE        */
        if (nErr != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] %s(), L(%d) - FSR_BML_CopyBack return error (nSrcVun=%d nSrcPgOffset=%d nDstVun=%d nSrcPgOffset=%d) (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, pstCpBkArg->nSrcVun, pstCpBkArg->nSrcPgOffset, pstCpBkArg->nDstVun, pstCpBkArg->nDstPgOffset, nErr));
            FSR_ASSERT(nErr != FSR_BML_VOLUME_NOT_OPENED);
            FSR_ASSERT(nErr != FSR_BML_WR_PROTECT_ERROR);
        }
    }

#if (OP_SUPPORT_MSB_PAGE_WAIT == 1)
    /* Set MSB page write flag. */
    FSR_OAM_MEMSET(pstClst->baMSBProg, FALSE32, sizeof(pstClst->baMSBProg));
    bIsLSB = FSR_STL_FlashIsLSBPage(pstZone, nDstVpn);
    pstClst->baMSBProg[nDstWay] = !bIsLSB;
#endif

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nErr));
    return nErr;
}

/**
 * @brief       This function does FlashFlush
 *
 * @param[in]   pstZone             : Zone object
 *
 * @return      FSR_STL_SUCCESS     : success.
 *
 * @author      Wonhee Cho
 * @version     1.0.0
 *
 */
PUBLIC INT32
FSR_STL_FlashFlush(STLZoneObj *pstZone)
{
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}

/**
 * @brief       This function does FlushRun
 *
 * @param[in]   pstZone             : Zone object
 *
 * @return      FSR_STL_SUCCESS     : success.
 *
 * @author      Wonhee Cho
 * @version     1.0.0
 *
 */
PUBLIC INT32
FSR_STL_FlashRun(STLZoneObj *pstZone)
{
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}

/**
 * @brief       This function returns TRUE32 when the nVpn is the LSB page
 *
 * @param[in]   pstZone             : Zone object
 * @param[in]   nVpn                : VPN of a page
 *
 * @return      FSR_STL_SUCCESS
 *
 * @author      Kangho Roh
 * @version     1.2.0
 *
 */
PUBLIC BOOL32
FSR_STL_FlashIsLSBPage (STLZoneObj *pstZone,
                        UINT32      nVpn)
{
    const RBWDevInfo   *pstDI   = pstZone->pstDevInfo;
          UINT32        nOff;
          UINT32        nPairedOff;
          BOOL32        nRet    = TRUE32;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%d, %d)\r\n"),
            __FSR_FUNC__, pstZone->nZoneID, nVpn));
    do
    {
        if (pstDI->nDeviceType == RBW_DEVTYPE_SLC ||
            pstDI->nDeviceType == RBW_DEVTYPE_ONENAND)
        {
            nRet = TRUE32;
            break;
        }

        nOff       = nVpn & (pstDI->nPagesPerSBlk - 1);
        nPairedOff = (UINT32)(FSR_BML_GetPairedVPgOff(pstZone->nVolID, nOff));
        if (nOff > nPairedOff)
        {
            nRet = FALSE32;
        }

    } while(0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}

/**
 * @brief       This function read a page using LSB recovery read command
 *
 * @param[in]   pstZone                 : Zone object
 * @param[in]   nVpn
 * @param[in]   pstParam
 *
 * @return      FSR_BML_SUCCESS         : VFL success.
 * @return      FSR_STL_CLEAN_PAGE      :
 * @return      FSR_BML_READ_ERROR      :
 * @return      FSR_BML_CRITICAL_ERROR  : There is unexpected error.
 *
 * @author      Wonhee Cho
 * @version     1.0.0
 *
 */
PUBLIC INT32
FSR_STL_FlashReliableRead  (STLZoneObj *pstZone,
                            UINT32      nVpn,
                            VFLParam   *pstParam)
{
    STLClstObj     *pstClst     = FSR_STL_GetClstObj(pstZone->nClstID);
    FSRSpareBuf    *pstSBuf     = pstClst->staSBuf;
    RBWDevInfo     *pstDI       = pstZone->pstDevInfo;
    UINT32          nVpnBML;
    UINT32          nFlag;
    INT32           nErr;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%d, %d)\r\n"),
            __FSR_FUNC__, pstZone->nZoneID, nVpn));

    FSR_ASSERT(pstParam->pData     != NULL  );
    FSR_ASSERT(pstParam->nNumOfPgs == 1     );
    FSR_ASSERT(pstParam->nBitmap   == pstDI->nFullSBitmapPerVPg);
    FSR_ASSERT(pstParam->bSpare    == TRUE32);
    FSR_ASSERT(pstParam->pExtParam != NULL  );

    /* Set flag for Reliable (R1) read */
    nFlag = FSR_BML_FLAG_USE_SPAREBUF | FSR_BML_FLAG_LSB_RECOVERY_LOAD;

    /* Set spare buffer */
    FSR_ASSERT(pstParam->pExtParam->nNumExtSData <= FSR_STL_SPARE_EXT_MAX_SIZE);
    pstSBuf->nNumOfMetaExt = (pstDI->nSecPerVPg) >> BML_SPARE_EXT_SHIFT;
    pstSBuf->pstSTLMetaExt = (FSRSpareBufExt*)(pstParam->pExtParam->aExtSData);

    /* Translate VFL address space to BML's */
    nVpnBML = pstClst->pstEnvVar->nStartVpn + nVpn;

#if (OP_SUPPORT_MSB_PAGE_WAIT == 1)
    /* Reset MSB page written flag */
    FSR_OAM_MEMSET(pstClst->baMSBProg, FALSE32, sizeof (pstClst->baMSBProg));
#endif

    /* Reset transaction begin mark */
    pstClst->bTransBegin = FALSE32;

    /* Read page from BML */
    nErr = FSR_BML_Read(pstZone->nVolID,        /* Volume ID for BML    */
                        nVpnBML,                /* VPN for BML          */
                        pstParam->nNumOfPgs,    /* The number of Pgs    */
                        pstParam->pData,        /* Data buffer          */
                        pstSBuf,                /* Spare buffer         */
                        nFlag);                 /* Read Flag            */
    /* Convert spare buffer regardless of return value */
    pstParam->nSData1 = pstSBuf->pstSpareBufBase->nSTLMetaBase0;
    pstParam->nSData2 = pstSBuf->pstSpareBufBase->nSTLMetaBase1;
    pstParam->nSData3 = pstSBuf->pstSpareBufBase->nSTLMetaBase2;
    /* Check the return value of FSR_BML_Read */
    if (nErr != FSR_BML_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x - FSR_BML_Read returns error\r\n"),
                __FSR_FUNC__, __LINE__, nErr));
        FSR_ASSERT(nErr != FSR_BML_VOLUME_NOT_OPENED);
        FSR_ASSERT(nErr != FSR_BML_INVALID_PARAM);
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nErr));
    return nErr;
}

/**
 * @brief       This function read a page checking ECC error bit
 *
 * @param[in]   pstZone                 : Zone object
 * @param[in]   nVpn
 * @param[in]   pstParam
 *
 * @return      FSR_BML_SUCCESS         : VFL success.
 * @return      FSR_STL_CLEAN_PAGE      :
 * @return      FSR_BML_READ_ERROR      :
 * @return      FSR_BML_CRITICAL_ERROR  : There is unexpected error.
 *
 * @author      Wonhee Cho
 * @version     1.0.0
 *
 */
PUBLIC INT32
FSR_STL_FlashCheckRead     (STLZoneObj *pstZone,
                            const UINT32    nVpn,
                            VFLParam   *pstParam,
                            const UINT32    nRetryCnt,
                            const BOOL32    bReadRefresh)
{
    STLClstObj     *pstClst     = FSR_STL_GetClstObj(pstZone->nClstID);
    FSRSpareBuf    *pstSBuf     = pstClst->staSBuf;
    const RBWDevInfo   *pstDI           = pstZone->pstDevInfo;
    const UINT32        nFBitmap        = pstDI->nFullSBitmapPerVPg;
    const UINT32        nSctsPerPg      = pstDI->nSecPerVPg;
          UINT32        nIdx;
    UINT32          nVpnBML;
          UINT8        *pData;
    UINT32          nFlag;
    UINT32          nCnt;
          UINT32        nSBitNum;
          UINT32        nEBitNum;
    UINT32         *pnTmpBuf;
    UINT32          nDWPerPg;
    UINT32          nNumExtSData    = 0;
          INT32         nErr            = FSR_BML_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%d, %d, %x)\r\n"),
            __FSR_FUNC__, pstZone->nZoneID, nVpn, pstParam));
    /* check the main and spare buffer pointer */
    FSR_ASSERT(pstParam->pData     != NULL  );
    FSR_ASSERT(pstParam->nNumOfPgs == 1     );
    FSR_ASSERT(pstParam->nBitmap   != 0     );
    FSR_ASSERT(pstParam->bSpare    == TRUE32);

    /* Setup Start bit, End bit and BML flag */
    if (pstParam->nBitmap != nFBitmap)
    {
        /* Get Read flag for partial page read */
        _SplitBitnumCheck(pstZone,
                          pstParam->nBitmap,
                          &nSBitNum,
                          &nEBitNum);

        /************************************************************************************************
        nFlag :
        31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
        ----------------------- ----------------------- -----------                --    --    --    --
        The Sct offset value    The Sct offset value    priorities                OTP   R1  Flush FOTMAT
        for 1st page             for last page        for OpQ                              Op
        *************************************************************************************************/
        nFlag    = (nSBitNum << 24) | ((nSctsPerPg - nEBitNum) << 16);
        if (nEBitNum == 0)
        {
            pData = NULL;
        }
        else
        {
            if (pstParam->bPgSizeBuf == TRUE32)
            {
                pData = pstParam->pData + (nSBitNum << BYTES_SECTOR_SHIFT);
            }
            else
            {
                pData = pstParam->pData;
            }
        }
    }
    else
    {
        nSBitNum = 0;
        nEBitNum = nSctsPerPg;
        nFlag    = FSR_BML_FLAG_NONE;
        pData    = pstParam->pData;
    }

    /* Translate VFL address space to BML's */
    nVpnBML = pstClst->pstEnvVar->nStartVpn + nVpn;

    /* Store spare buffer */
    nFlag  |= FSR_BML_FLAG_USE_SPAREBUF;
    if (bReadRefresh == TRUE32)
    {
        nFlag |= FSR_BML_FLAG_FORCED_ERASE_REFRESH;
    }
    else
    {
        /*If read disturbance is occured in BML_Read, BML should inform it to STL and ignore it. */
        nFlag |= FSR_BML_FLAG_INFORM_DISTURBANCE_ERROR | FSR_BML_FLAG_IGNORE_READ_DISTURBANCE;
    }

    if ((pstParam->pExtParam               != NULL) &&
        (pstParam->pExtParam->nNumExtSData >  0   ))
    {
        FSR_ASSERT(pstParam->pExtParam->nNumExtSData <= FSR_STL_SPARE_EXT_MAX_SIZE);
        nNumExtSData           = (pstDI->nSecPerVPg) >> BML_SPARE_EXT_SHIFT;
        pstSBuf->pstSTLMetaExt = (FSRSpareBufExt*)(pstParam->pExtParam->aExtSData);
    }
    else
    {
        nNumExtSData          = 0;
        pstSBuf->pstSTLMetaExt = NULL;
    }
    pstSBuf->nNumOfMetaExt = nNumExtSData;

#if (OP_SUPPORT_MSB_PAGE_WAIT == 1)
    /* Reset MSB page written flag */
    FSR_OAM_MEMSET(pstClst->baMSBProg, FALSE32, sizeof (pstClst->baMSBProg));
#endif

    /* Reset transaction begin mark */
    pstClst->bTransBegin = FALSE32;

    nCnt   = 0;
    while (nCnt < nRetryCnt)
    {
        /* Read page from BML */
        nErr = FSR_BML_Read(pstZone->nVolID,        /* Volume ID for BML    */
                            nVpnBML,                /* VPN for BML          */
                            1,                      /* The number of Pgs    */
                            pData,                  /* Data buffer          */
                            pstSBuf,                /* Spare buffer         */
                            nFlag);                 /* Read Flag            */
        if (nErr != FSR_BML_READ_ERROR)
        {
            break;
        }
        nCnt++;
    }

    /* Convert spare buffer regardless of return value */
    pstParam->nSData1 = pstSBuf->pstSpareBufBase->nSTLMetaBase0;
    pstParam->nSData2 = pstSBuf->pstSpareBufBase->nSTLMetaBase1;
    pstParam->nSData3 = pstSBuf->pstSpareBufBase->nSTLMetaBase2;

    /* Check the return value of FSR_BML_Read */
    /* If the original data is a clean and READ_DISTURBANCE is occured during the read operation,
       return FSR_STL_CLEAN_PAGE. This is because the caller operation of this function operates
       assuming that all clean pages return FSR_STL_CLEAN_PAGE even though READ_DISTURBANCE is occured.
    */
    if ((nErr != FSR_BML_SUCCESS) && (nErr != FSR_BML_READ_DISTURBANCE_ERROR))
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:INF]  --%s() L(%d) : 0x%08x - Read Error\r\n"),
                __FSR_FUNC__, __LINE__, nErr));
        FSR_ASSERT(nErr != FSR_BML_VOLUME_NOT_OPENED);
        FSR_ASSERT(nErr != FSR_BML_INVALID_PARAM);
    }
    else
    {
        /* Check.clean page */

        /* pstParam->pData must be aligned */
        FSR_ASSERT(((UINT32)((pstParam->pData)) & (sizeof(UINT32) - 1)) == 0);
        pnTmpBuf = (UINT32*)(pstParam->pData);
        /* nBytesPerVpg / sizeof(UINT32) */
        nDWPerPg = ((nEBitNum - nSBitNum) << (BYTES_SECTOR_SHIFT)) >> 2;
        for (nIdx = 0; nIdx < nDWPerPg; nIdx++)
        {
            if (*pnTmpBuf != 0xFFFFFFFF)
            {
                break;
            }
            pnTmpBuf++;
        }

        if (nIdx == nDWPerPg)
        {
            /*  Check clean spare data */
            if ((pstSBuf->pstSpareBufBase->nSTLMetaBase0 == 0xFFFFFFFF) &&
                (pstSBuf->pstSpareBufBase->nSTLMetaBase1 == 0xFFFFFFFF) &&
                (pstSBuf->pstSpareBufBase->nSTLMetaBase2 == 0xFFFFFFFF))
            {
                pnTmpBuf = pstParam->pExtParam->aExtSData;
                nNumExtSData <<= BML_SPARE_EXT_SHIFT;     /* multiply by 4 */
                for (nIdx = 0; nIdx < nNumExtSData; nIdx++)
                {
                    if ((*pnTmpBuf) != 0xFFFFFFFF)
                    {
                        break;
                    }
                    pnTmpBuf++;

                }
                if (nIdx == nNumExtSData)
                {
                    nErr = FSR_STL_CLEAN_PAGE;
                }
            }
        }
    }

#if (OP_STL_DEBUG_CODE == 1)
    if (nErr == FSR_BML_READ_ERROR)
    {
        FSR_ASSERT(nCnt >= nRetryCnt);
    }
    else if (nErr == FSR_BML_READ_DISTURBANCE_ERROR)
    {
        FSR_ASSERT(bReadRefresh == FALSE32);
    }
#endif

    if ((nErr == FSR_BML_SUCCESS) &&
        (bReadRefresh == FALSE32)         &&
        (nCnt >  0))
    {
        nErr = FSR_BML_READ_DISTURBANCE_ERROR;
    }

#if 0 // For only test
    if ((nErr == FSR_BML_SUCCESS) &&
        (bReadRefresh == FALSE32))
    {
        if ((rand() % 200) > 195)
        {
            nErr = FSR_BML_READ_DISTURBANCE_ERROR;
        }
    }
#endif

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nErr));
    return nErr;
}


/**
 * @brief       This function informs whether the page is clean page or not.
 *
 * @param[in]   pstZone                 : Zone object 
 * @param[in]   pstParam
 *
 * @return      TRUE32                  : Clean Page
 * @return      FALSE32                 : Not clean page
 *
 * @author      SangHoon choi
 * @version     1.0.0
 *
 */
PUBLIC BOOL32   FSR_STL_FlashIsCleanPage    (STLZoneObj     *pstZone,
                                             VFLParam       *pstParam)
{
    STLClstObj     *pstClst     = FSR_STL_GetClstObj(pstZone->nClstID);
    FSRSpareBuf    *pstSBuf     = pstClst->staSBuf;
    const RBWDevInfo   *pstDI           = pstZone->pstDevInfo;
    const UINT32        nFBitmap        = pstDI->nFullSBitmapPerVPg;
    const UINT32        nSctsPerPg      = pstDI->nSecPerVPg;
    UINT32              nIdx;    
    UINT32              nSBitNum;
    UINT32              nEBitNum;
    UINT32             *pnTmpBuf;
    UINT32              nDWPerPg;
    UINT32              nNumExtSData    = 0;
    INT32               nErr            = FSR_BML_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%d, %x)\r\n"),
            __FSR_FUNC__, pstZone->nZoneID, pstParam));

    /* check the main and spare buffer pointer */
    FSR_ASSERT(pstParam->pData     != NULL  );
    FSR_ASSERT(pstParam->nNumOfPgs == 1     );
    FSR_ASSERT(pstParam->nBitmap   != 0     );
    FSR_ASSERT(pstParam->bSpare    == TRUE32);

     /* Set default flag for BML read */
    if (pstParam->bSpare == TRUE32)
    {
        FSR_ASSERT(pstParam->nNumOfPgs == 1);

        /* Store spare buffer */        
        pstSBuf = pstClst->staSBuf;
        if ((pstParam->pExtParam               != NULL) &&
            (pstParam->pExtParam->nNumExtSData >  0   ))
        {
            FSR_ASSERT(pstParam->pExtParam->nNumExtSData <= FSR_STL_SPARE_EXT_MAX_SIZE);
            nNumExtSData           = (pstDI->nSecPerVPg) >> BML_SPARE_EXT_SHIFT;
            pstSBuf->pstSTLMetaExt = (FSRSpareBufExt*)(pstParam->pExtParam->aExtSData);
        }
        else
        {
            nNumExtSData          = 0;
            pstSBuf->pstSTLMetaExt = NULL;
        }
        pstSBuf->nNumOfMetaExt = nNumExtSData;
    }
    else
    {
        pstSBuf = NULL;
    }

    /* Setup Start bit, End bit and BML flag */
    if (pstParam->nBitmap != nFBitmap)
    {
        /* Get Read flag for partial page read */
        _SplitBitnumCheck(pstZone,
                          pstParam->nBitmap,
                          &nSBitNum,
                          &nEBitNum);        
    }
    else
    {
        nSBitNum = 0;
        nEBitNum = nSctsPerPg;        
    }

    /* Check.clean page */
    /* pstParam->pData must be aligned */
    FSR_ASSERT(((UINT32)((pstParam->pData)) & (sizeof(UINT32) - 1)) == 0);
    pnTmpBuf = (UINT32*)(pstParam->pData);

    /* nBytesPerVpg / sizeof(UINT32) */
    nDWPerPg = ((nEBitNum - nSBitNum) << (BYTES_SECTOR_SHIFT)) >> 2;
    for (nIdx = 0; nIdx < nDWPerPg; nIdx++)
    {
        if (*pnTmpBuf != 0xFFFFFFFF)
        {
            break;
        }
        pnTmpBuf++;
    }

    if (nIdx == nDWPerPg)
    {
        /*  Check clean spare data */
        if ((pstParam->nSData1 == 0xFFFFFFFF) &&
            (pstParam->nSData2 == 0xFFFFFFFF) &&
            (pstParam->nSData3 == 0xFFFFFFFF))
        {
            pnTmpBuf = pstParam->pExtParam->aExtSData;
            nNumExtSData <<= BML_SPARE_EXT_SHIFT;     /* multiply by 4 */
            for (nIdx = 0; nIdx < nNumExtSData; nIdx++)
            {
                if ((*pnTmpBuf) != 0xFFFFFFFF)
                {
                    break;
                }
                pnTmpBuf++;
            }
            if (nIdx == nNumExtSData)
            {
                nErr = FSR_STL_CLEAN_PAGE;
            }
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nErr));

    if (nErr == FSR_STL_CLEAN_PAGE)
    {
        return TRUE32;
    }
    
    return FALSE32;
}

