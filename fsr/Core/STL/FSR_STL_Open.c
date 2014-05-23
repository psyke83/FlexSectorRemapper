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
 *  @file       FSR_STL_Open.c
 *  @brief      This file contains functions relatedd Open process
 *  @author     Jongtae Park
 *  @date       05-MAR-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  05-MAR-2007 [Jongtae Park] : first writing
 *  @n  27-JUN-2007 [Jongtae Park] : re-writing for orange V1.2.0
 *  @n  28-JAN-2008 [Seunghyun Han] : dead code elimination
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
#define     PAGE_CORRUPTED_NONE         (0x00)
#define     PAGE_CORRUPTED_ALONE        (0x01)
#define     PAGE_CORRUPTED_PAIRED       (0x02)
#define     PAGE_CORRUPTED_MASK         (0x03)
#define     PAGE_READ_DISTURBANCE       (0x04)

typedef struct {
    UINT8       bClean;
    UINT8       bConfirm;
    UINT16      nCorrupted;
    PADDR       nLpn;
} STLTmpVPmt;

/*****************************************************************************/
/* Local constant definitions                                                */
/*****************************************************************************/
#if defined(FSR_ONENAND_EMULATOR)
#define MAX_TRY_COUNT_FOR_CHECK_READ        (1)
#else
#define MAX_TRY_COUNT_FOR_CHECK_READ        (4)
#endif

/*****************************************************************************/
/* Local (static) function prototype                                         */
/*****************************************************************************/
PRIVATE INT32   _ScanRootInfo          (STLZoneObj     *pstZone);

PRIVATE INT32   _ScanDirHeader         (STLZoneObj     *pstZone,
                                        UINT32         *nHeaderIdx);

PRIVATE INT32   _CheckMetaMerge        (STLZoneObj     *pstZone,
                                        UINT32          nHeaderIdx);

PRIVATE INT32   _LoadDirUpdate         (STLZoneObj     *pstZone,
                                        UINT32          nHeaderIdx);

PRIVATE INT32   _LoadLatestCxt         (STLZoneObj     *pstZone,
                                        UINT32          nFlag);

PRIVATE INT32   _MakeActivePMT         (STLZoneObj     *pstZone,
                                        STLLogGrpHdl   *pLogGrp,
                                        STLLog         *pLogObj,
                                        UINT32          LogIdx);

PRIVATE INT32   _BuildPMTforActiveLog  (STLZoneObj     *pstZone,
                                                 STLActLogEntry *pActLog,
                                                 UINT32         nIdx);

PRIVATE INT32   _RebuildLogPageMap     (STLZoneObj     *pstZone);

#if (OP_SUPPORT_RUNTIME_PMT_BUILDING == 1)
PRIVATE VOID                    _SetInitialActLogInfos      (STLZoneObj *pstZone);
PRIVATE STLActLogEntry*         _GetActLog                  (STLZoneObj *pstZone,
                                                            BADDR       nDgn,
                                                            UINT32     *nIdxInActLogList);
#endif /*OP_SUPPORT_RUNTIME_PMT_BUILDING*/

PRIVATE INT32   _OpenZoneMeta          (STLZoneObj     *pstZone,
                                        UINT32          nFlag);

PRIVATE INT32   _OpenZonePost          (STLZoneObj     *pstZone,
                                        UINT32          nFlag);

PRIVATE INT32   _LoadFullBMT           (STLZoneObj     *pstZone);

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
PRIVATE INT32   _ScanBufferBlk         (STLZoneObj     *pstZone);

PRIVATE INT32   _FlushBufferBlk        (STLZoneObj     *pstZone);
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */

#if (OP_SUPPORT_FONAND_RELIABLE_WRITE == 1)
PRIVATE INT32   _FONANDReliableWrite   (STLZoneObj     *pstZone,
                                        STLLogGrpHdl   *pLogGrp,
                                        STLLog         *pLogObj,
                                        const UINT32    LogIdx,
                                        const UINT32    nLastOff,
                                        const POFFSET   nConfirmIdx,
                                        const POFFSET   nPrecConfim);
#endif  /* (OP_SUPPORT_FONAND_RELIABLE_WRITE == 1) */

PRIVATE BOOL32  _CheckPageValidity     (STLZoneObj     *pstZone,
                                        VFLParam       *pstParam,
                                        BOOL32          bLsbPg,
                                        POFFSET        *pnLpo,
                                        BOOL32         *pbConfirm);

PRIVATE INT32   _ForceToCorruptPage    (STLZoneObj     *pstZone,
                                        VFLParam       *pstParam,
                                        PADDR           nDstVpn);

/*****************************************************************************/
/* Local (static)  Function Definition                                       */
/*****************************************************************************/

/**
 * @brief           This function reads root info page from searching root block
 *
 * @param[in]       pstZone       : pointer to atl Zone object
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PRIVATE INT32
_ScanRootInfo(STLZoneObj *pstZone)
{
    INT32           nRet;    
    UINT32          nClstID;
    UINT32          nVpn            = NULL_VPN;
    UINT32          nMinAge_High    = 0xFFFFFFFF;
    UINT32          nMinAge_Low     = 0xFFFFFFFF;
    UINT32          nRetryCnt       = 0;
    BOOL32          bIsFormatted    = FALSE32;    
    STLClstObj     *pstClst         = NULL;
    STLRootInfo    *pstRI           = NULL;
    RBWDevInfo     *pstDVI          = NULL;
    UINT16          nIdx;
    UINT16          nNumRootBlks    = 0xFFFF;    
    UINT16          nRootBlkIdx     = NULL_DGN;
    BADDR           nVbn            = NULL_VBN;
    BADDR           nRootStartVbn   = NULL_VBN;
    UINT8           aSignature[8]   = "FSR_STL";
    VFLParam       *pstVFLParam;
    UINT32          nPgsPerSBlk;
    UINT32          nPgsPerSBlkSht;
    UINT32          nZBCRoot;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    nClstID         = pstZone->nClstID;
    pstClst         = gpstSTLClstObj[nClstID];
    pstRI           = &(pstClst->stRootInfoBuf.stRootInfo);
    pstDVI          = pstClst->pstDevInfo;
    nNumRootBlks    = pstRI->nNumRootBlks;
    nRootStartVbn   = pstRI->nRootStartVbn;
    nPgsPerSBlk     = pstDVI->nPagesPerSBlk;
    nPgsPerSBlkSht  = pstDVI->nPagesPerSbShift;
    pstVFLParam     = FSR_STL_AllocVFLParam(pstZone);

    pstVFLParam->pData        = (UINT8*)&(pstClst->stRootInfoBuf);
    pstVFLParam->bPgSizeBuf   = FALSE32;
    pstVFLParam->bUserData    = FALSE32;
    pstVFLParam->bSpare       = TRUE32;
    pstVFLParam->nBitmap      = ROOT_INFO_BUF_BITMAP;
    pstVFLParam->nNumOfPgs    = 1;

    pstVFLParam->pExtParam    = FSR_STL_AllocVFLExtParam(pstZone);

    pstVFLParam->pExtParam->nNumExtSData  = 0;

    if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
    {
        /**
         * if MLC, pages per super block is half because of consuming LSB page
         * MLC LSB Half Pages Only
         */
        nPgsPerSBlk >>= 1;
        nPgsPerSBlkSht -= 1;
    }

    /**
     * root info page spare layout
     * ------------------------------------------------------------------------------
     * nSData1   | nSData2   | nSData3       | nSData4   | nSData5    | nSData6
     * ------------------------------------------------------------------------------
     * Age_Low(4) Age_Low'(4) PTF(2)|PTF'(2) Age_High(4)  Age_High'(4) ZBC(2)|ZBC'(2)
     */

    /* 1. Search minimum age root block */
    for (nIdx = 0; nIdx < nNumRootBlks; nIdx++)
    {
        nVbn = nRootStartVbn + nIdx;

        if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
        {
            nVpn = (nVbn << pstDVI->nPagesPerSbShift) 
                        + FSR_BML_GetVPgOffOfLSBPg(pstZone->nVolID, 0);
        }
        else
        {
            nVpn = (nVbn << pstDVI->nPagesPerSbShift) + 0;
        }

        /* first, read main & spare data to check up age and signature */
        nRet = FSR_STL_FlashCheckRead(pstZone, nVpn, pstVFLParam, 1, TRUE32);

        /* calculate zero bit count */
        nZBCRoot = FSR_STL_GetZBC(pstVFLParam->pData, sizeof(STLRootInfo));
        if (nRet == FSR_STL_CLEAN_PAGE)
        {
            /* After search, meet clean page */
            continue;
        }
        else if (nRet == FSR_BML_READ_ERROR)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] Root page : U_ECC (nVpn = %d)\r\n"), nVpn));
            _ForceToCorruptPage(pstZone, pstVFLParam, nVpn);
            continue;
        }
        else if ((pstVFLParam->nSData1 != nZBCRoot) ||
                 (pstVFLParam->nSData2 != (pstVFLParam->nSData1 ^ 0xFFFFFFFF)))
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] Root page : ZeroBit Error (nVpn = %d)\r\n"), nVpn));
                
            continue;
        }
        else if (nRet != FSR_BML_SUCCESS)
        {
            return nRet;
        }

        /* If the block has a valid ZBC value, then checks whether it has valid root block signature
        and updates the block index when the block is younger than previous block. */
        if (pstVFLParam->nSData1 == nZBCRoot)
        {
            /* Check if there is valid root block by confirming the Root Block signature */
            if (bIsFormatted == FALSE32)
            {
                if ((pstRI->aSignature[0] == aSignature[0]) &&
                    (pstRI->aSignature[1] == aSignature[1]) &&
                    (pstRI->aSignature[2] == aSignature[2]) &&
                    (pstRI->aSignature[3] == aSignature[3]) &&
                    (pstRI->aSignature[4] == aSignature[4]) &&
                    (pstRI->aSignature[5] == aSignature[5]) &&
                    (pstRI->aSignature[6] == aSignature[6]) &&
                    (pstRI->aSignature[7] == aSignature[7]))
                {
                    bIsFormatted = TRUE32;
                }
            }


            /* minimum age page becomes the latest Directory header */
            if (pstRI->nAge_High <= nMinAge_High)
            {
                if (pstRI->nAge_Low <= nMinAge_Low)
                {
                    nMinAge_High = pstRI->nAge_High;
                    nMinAge_Low  = pstRI->nAge_Low;
                    nRootBlkIdx  = (UINT16)(nIdx);
                }
            }
        }
    }

    nVbn = nRootStartVbn + nRootBlkIdx;

    /*
    Check whether the partition is formatted or not by checking the root block
    - (nVbn == NULL_VBN)
    If the nVbn is same as NULL_VBN, it means it is never updated 
    since there is no block which ZBC value is valid. 
    This case can be interpreted as the partition is not formatted yet.

    - (bIsFormatted == FALSE32)
    If the value of bIsFormatted is same as FALSE32, it means there is no root block
    which has valid root block signature. It also can be interpreted as
    that the partition is not formatted yet.
    */    
    if ((nVbn == NULL_VBN) || (bIsFormatted == FALSE32))
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] STL Partition(Cluster) is not formatted. \r\n")));
        return FSR_STL_UNFORMATTED;
    }

    /* 2. Search clean page offset(CPO) in STLRootInfo Blocks */
    for (nIdx = 0; nIdx < nPgsPerSBlk; nIdx++)
    {
        /* get VPN address of header page of each meta blocks */
        if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
        {
            nVpn = (nVbn << pstDVI->nPagesPerSbShift) 
                        + FSR_BML_GetVPgOffOfLSBPg(pstZone->nVolID, nIdx);
        }
        else
        {
            nVpn = (nVbn << pstDVI->nPagesPerSbShift)
                 + nIdx;
        }

        /* first, read main & spare data to check up age and signature */
        nRet = FSR_STL_FlashCheckRead(pstZone, nVpn, pstVFLParam, 1, TRUE32);

        /* calculate zero bit count */
        nZBCRoot = FSR_STL_GetZBC(pstVFLParam->pData, sizeof(STLRootInfo));
        if (nRet == FSR_STL_CLEAN_PAGE)
        {
            /*after search, meet clean page */
            break;
        }
        else if (nRet == FSR_BML_READ_ERROR)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] Root page : U_ECC (nVpn = %d)\r\n"), nVpn));
            continue;
        }
        else if ((pstVFLParam->nSData1 != nZBCRoot) ||
                 (pstVFLParam->nSData2 != (pstVFLParam->nSData1 ^ 0xFFFFFFFF)))
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] Root page : ZeroBit Error (nVpn = %d)\r\n"), nVpn));
                
            continue;
        }
        else if (nRet != FSR_BML_SUCCESS)
        {
            return nRet;
        }
        else if (nRet == FSR_BML_SUCCESS)
        {
            continue;
        }
    }

    /* 3. read valid root page & ask for meta vbn list */
    do
    {
        if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
        {
            nVpn = (nVbn << pstDVI->nPagesPerSbShift) 
                        + FSR_BML_GetVPgOffOfLSBPg(pstZone->nVolID, ((nIdx - 1) - nRetryCnt));
        }
        else
        {
            nVpn = (nVbn << pstDVI->nPagesPerSbShift)
                 + ((nIdx - 1) - nRetryCnt);
        }

        /* Read the latest root info */
        nRet = FSR_STL_FlashCheckRead(pstZone, nVpn, pstVFLParam, 1, TRUE32);

        /* calculate zero bit count */
        nZBCRoot = FSR_STL_GetZBC(pstVFLParam->pData, sizeof(STLRootInfo));
        if (nRet == FSR_BML_READ_ERROR)
        {
            /* sudden power off case */
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] Root page : U_ECC (nVpn = %d)\r\n"), nVpn));
            _ForceToCorruptPage(pstZone, pstVFLParam, nVpn);
            nRetryCnt++;
        }
        else if ((pstVFLParam->nSData1 != nZBCRoot) ||
                 (pstVFLParam->nSData2 != (pstVFLParam->nSData1 ^ 0xFFFFFFFF)))
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] Root page : ZeroBit Error (nVpn = %d)\r\n"), nVpn));
            nRetryCnt++;
        }
        else if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] Root page : VFL Error (nVpn = %d)\r\n"), nVpn));                
            return nRet;
        }
        else
        {
           /* successful to read */
            break;
        }
        
    }while (nRetryCnt < pstDVI->nPagesPerSBlk);

    pstRI->nRootCPOffs = (UINT16)(((((pstRI->nRootCPOffs >> nPgsPerSBlkSht) 
                            + 1) % MAX_ROOT_BLKS) << nPgsPerSBlkSht));

    FSR_STL_FreeVFLParam(pstZone, pstVFLParam);
    FSR_STL_FreeVFLExtParam(pstZone, pstVFLParam->pExtParam);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}

/**
 * @brief           This function scans the directory header page to Flash.
 *
 * @param[in]       pstZone       : pointer to atl Zone object
 * @param[out]      nHeaderIdx    : header block index
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */

PRIVATE INT32
_ScanDirHeader (STLZoneObj *pstZone,
                UINT32     *nHeaderIdx)
{
    BADDR           nVbn            = NULL_VBN;
    BADDR           nHeaderVbn      = NULL_VBN;
    PADDR           nVpn            = NULL_VPN;
    UINT32          nIdx            = 0;
    INT32           nRet;
    UINT32          nMinAge_High    = 0xFFFFFFFF;
    UINT32          nMinAge_Low     = 0xFFFFFFFF;
    STLDirHdrHdl   *pstDH           = NULL;
    STLZoneInfo    *pstZI           = NULL;
    RBWDevInfo     *pstDVI          = NULL;
    STLMetaLayout  *pstML           = NULL;
    VFLParam       *pstVFLParam;
    BOOL32          bValid          = FALSE32;
    UINT32          nZBitCnt;
    UINT32          nInvertedZBC;
    UINT32          i;
    INT32           nLockRet;
    UINT32          nZBCHeader;
    
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    pstZI               = pstZone->pstZI;
    pstDH               = pstZone->pstDirHdrHdl;
    pstDVI              = pstZone->pstDevInfo;
    pstML               = pstZone->pstML;
    pstVFLParam         = FSR_STL_AllocVFLParam(pstZone);
    *nHeaderIdx         = 0xFFFFFFFF;
    
    /* Find out the latest Directory header VBN */
    pstVFLParam->bPgSizeBuf   = FALSE32;
    pstVFLParam->bUserData    = TRUE32;
    pstVFLParam->bSpare       = TRUE32;
    pstVFLParam->nBitmap      = pstML->nDirHdrSBM;
    pstVFLParam->nNumOfPgs    = 1;
    pstVFLParam->pExtParam    = FSR_STL_AllocVFLExtParam(pstZone);
    pstVFLParam->pExtParam->nNumExtSData = 0;

    /**
     * directory header page spare layout
     * -------------------------------------------------------------------------
     * nSData1   | nSData2  | nSData3     | nSData4   | nSData5    | nSData6
     * -------------------------------------------------------------------------
     * Age_Low(4) Age_Low'(4)PTF(2)|PTF'(2)Age_High(4) Age_High'(4) ZBC(2)|ZBC'(2)
     */
    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:INF]   =====> STL ZoneID[%2d] Meta Block List \r\n"), pstZone->nZoneID));

    /* search latest directory header in Meta blocks */
    for (nIdx = 0; nIdx < pstZI->nNumMetaBlks; nIdx++)
    {
        nVbn = *(pstZI->aMetaVbnList + nIdx);

        {
            i = (pstZI->nNumDirHdrPgs - 1);

            /* check header page validity */
            bValid = FALSE32;

            /* get VPN address of header page of each meta blocks */
            if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
            {
                nVpn = (nVbn << pstDVI->nPagesPerSbShift) 
                                + FSR_BML_GetVPgOffOfLSBPg(pstZone->nVolID, i);
            }
            else
            {
                nVpn = (nVbn << pstDVI->nPagesPerSbShift)
                                + i;
            }

            pstVFLParam->pData = ((UINT8*)&(pstDH->pBuf[pstDVI->nBytesPerVPg * i]));

            /* first, read main & spare data to check up age and signature */
            nRet            = FSR_STL_FlashCheckRead(pstZone, nVpn, pstVFLParam, 1, TRUE32);
            nZBitCnt        = pstVFLParam->nSData1;
            nInvertedZBC    = pstVFLParam->nSData2;

            /* calculate zero bit count for header page confirm */
            nZBCHeader = FSR_STL_GetZBC(pstVFLParam->pData, pstDVI->nBytesPerVPg);
            if (nRet == FSR_STL_CLEAN_PAGE)
            {
                 continue;
            }
            else if (nRet == FSR_BML_READ_ERROR)
            {
                /* sudden power off case */
                FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
                    (TEXT("[SIF:DBG] CxtHeader : U_ECC (nVpn = %d)\r\n"), nVpn));

                _ForceToCorruptPage(pstZone, pstVFLParam, nVpn);
                continue;
            }
            else if ((nZBitCnt                != nZBCHeader) ||
                     ((nZBitCnt ^ 0xFFFFFFFF) != nInvertedZBC))
            {
                /* sudden power off case */
                FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
                    (TEXT("[SIF:DBG] CxtHeader : Meta Broken (nVpn = %d)\r\n"), nVpn));
                continue;
            }
            else if (nRet != FSR_BML_SUCCESS)
            {
                FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
                    (TEXT("[SIF:DBG] CxtHeader : BML Error (nVpn = %d)\r\n"), nVpn));
                return nRet;
            }
            else
            {
                bValid = TRUE32;
            }
        }

        /* check header page validity */
        if (bValid == FALSE32)
        {
            continue;
        }

        /* minimum age page becomes the latest Directory header */
        if (pstDH->pstFm->nAge_High <= nMinAge_High)
        {
            if (pstDH->pstFm->nAge_Low <= nMinAge_Low)
            {
                nMinAge_High = pstDH->pstFm->nAge_High;
                nMinAge_Low  = pstDH->pstFm->nAge_Low;
                *nHeaderIdx = nIdx;
            }
        }
        
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:INF]   nVbn(%5d)         :       nAge(0x%08x)\r\n"), nVbn, pstVFLParam->nSData1));
    }

    if (*nHeaderIdx >= pstZI->nNumMetaBlks)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, FSR_STL_META_BROKEN));
        return FSR_STL_META_BROKEN;
    }

    /**
     * read the directory header page.
     * get directory header VBN address
     */
    nHeaderVbn = pstZI->aMetaVbnList[*nHeaderIdx];

    nRet = FSR_STL_ReadHeader(pstZone, nHeaderVbn);
    if (nRet != FSR_STL_SUCCESS)
    {
        /* process to return READ Only partition */
        nLockRet = FSR_STL_LockPartition(pstZone->nClstID);
        if (nLockRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nLockRet));
            return nLockRet;
        }

        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
        return nRet;
    }

    pstDH->pstFm->nHeaderIdx = (UINT16)(*nHeaderIdx);

    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:INF]   Current Header Index       (%5d)  Vbn    (%5d) \r\n"), *nHeaderIdx, nHeaderVbn));

    FSR_STL_FreeVFLParam(pstZone, pstVFLParam);
    FSR_STL_FreeVFLExtParam(pstZone, pstVFLParam->pExtParam);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}

/**
 * @brief           This function updates directory information
 *
 * @param[in]       pstZone       : pointer to stl Zone object
 * @param[in]       nHeaderIdx    : header block index
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PRIVATE INT32
_LoadDirUpdate (STLZoneObj *pstZone,
                UINT32      nHeaderIdx)
{
    PADDR           nVpn            = NULL_VPN;
    INT32           nRet;
    UINT32          nIdx, i;
    INT32           nIdleIdx;
    UINT32          nPgsPerSBlk     = 0;
    POFFSET         nMetaPOffs      = NULL_POFFSET;
    STLDirHdrHdl   *pstDH           = NULL;
    STLMetaLayout  *pstML           = NULL;
    UINT8          *pMPgBF          = NULL;
    UINT8          *pTmpMPgBF       = NULL;
    RBWDevInfo     *pstDVI          = NULL;
    STLZoneInfo    *pstZI           = NULL;
    STLLogGrpHdl   *pstLogGrp       = NULL;
    STLBMTHdl      *pstBMT          = NULL;
    STLPMTHdl      *pstPMT          = NULL;
    STLCtxInfoHdl  *pstCI           = NULL;
    STLCtxInfoFm   *pstCIFm         = NULL;
    VFLParam       *pstVFLParam     = NULL;
    BADDR           nLan            = NULL_DGN;
    BADDR           nDgn            = NULL_DGN;
    BADDR           nHeaderVbn;
    UINT16          nMetaType;
    UINT32          nZBCBMT;
    UINT32          nZBCPMT;
    UINT32          nZBCCtx;
    UINT16          nPrevHdrIdx     = NULL_POFFSET;
    BOOL32          bCheckLocked    = FALSE32;
    BOOL32          bCorrupted;
    INT32           nLockRet;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    pstDVI          = pstZone->pstDevInfo;
    pstZI           = pstZone->pstZI;
    pstML           = pstZone->pstML;
    pTmpMPgBF       = pstZone->pTempMetaPgBuf;
    pMPgBF          = pstZone->pMetaPgBuf;
    pstDH           = pstZone->pstDirHdrHdl;
    pstBMT          = pstZone->pstBMTHdl;
    pstPMT          = pstZone->pstPMTHdl;
    pstCI           = pstZone->pstCtxHdl;
    pstCIFm         = pstCI->pstFm;
    nPgsPerSBlk     = pstDVI->nPagesPerSBlk;
    pstVFLParam     = FSR_STL_AllocVFLParam(pstZone);

    if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
    {
        /**
         * if MLC, pages per super block is half because of consuming LSB page
         * MLC LSB Half Pages Only
         */
        nPgsPerSBlk >>= 1;
    }

    pstVFLParam->pData        = (UINT8*)pTmpMPgBF;
    pstVFLParam->bPgSizeBuf   = FALSE32;
    pstVFLParam->bUserData    = FALSE32;
    pstVFLParam->bSpare       = TRUE32;
    pstVFLParam->nBitmap      = pstDVI->nFullSBitmapPerVPg;
    pstVFLParam->nNumOfPgs    = 1;
    pstVFLParam->pExtParam    = NULL;

    FSR_STL_SetBMTHdl(pstZone,
                      pstBMT,
                      pMPgBF,
                      pstML->nBMTBufSize);

    nHeaderVbn = pstZI->aMetaVbnList[pstDH->pstFm->nHeaderIdx];
    for (nIdx = pstZI->nNumDirHdrPgs; nIdx < nPgsPerSBlk; nIdx++)
    {
        /* Check if PMT is corrupted or not */
        bCorrupted = FALSE32;

        if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
        {
            /* adjust into LSB page address, when it is MLC */
            nVpn = (nHeaderVbn << pstDVI->nPagesPerSbShift)
                 + FSR_BML_GetVPgOffOfLSBPg(pstZone->nVolID, nIdx);
        }
        else
        {
            nVpn = (nHeaderVbn << pstDVI->nPagesPerSbShift) 
                 + nIdx;
        }

        nRet = FSR_STL_FlashCheckRead(pstZone, nVpn, pstVFLParam, 1, TRUE32);
        if (nRet == FSR_STL_CLEAN_PAGE)
        {
            break;
        }

        /**
         * If the error occurs in the last page, it abandons the corrupted last page.
         * If the error occurs in the middle of the meta block, however, it locks the partition.
         * The reason for calling continue operation below is to check if the page is last page.
         * If the page is the last page, next time when FSR_STL_FlashCheckRead is called 
         * it returns FSR_STL_CLEAN_PAGE.
         * So, it will escape from this for-loop. This means that this partition will not be locked.
         * But if the next page is not a clean page, this partition will be locked.
        */
        else if (nRet == FSR_BML_READ_ERROR)
        {
            bCheckLocked = TRUE32;

            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
                (TEXT("[SIF:INF] Cxt : U_ECC (nVpn = %d)\r\n"), nVpn));

            _ForceToCorruptPage(pstZone, pstVFLParam, nVpn);
            continue;
        }
        else if (nRet != FSR_BML_SUCCESS)
        {
            return nRet;
        }

        /**
         * update the directory header
         * nSData1 : Meta_Type(2) | Meta_Type'(2)
         */
        nMetaType = (UINT16)(pstVFLParam->nSData1 >> 16);

        if (nMetaType == MT_BMT)
        {
            FSR_OAM_MEMCPY(pMPgBF, pTmpMPgBF, (pstML->nBMTBufSize + pstML->nCtxBufSize));

            nZBCBMT = FSR_STL_GetZBC((UINT8*)pstBMT->pBuf,
                                             pstBMT->nCfmBufSize);

            if ((pstBMT->pstCfm->nZBC != nZBCBMT) ||
                ((pstBMT->pstCfm->nZBC ^ 0xFFFFFFFF) != pstBMT->pstCfm->nInvZBC))
            {
                bCheckLocked = TRUE32;
                continue;
            }
        
            nLan = pstBMT->pstFm->nLan;
        }
        else
        {
            FSR_OAM_MEMCPY(&(pMPgBF[pstBMT->nBufSize]), pTmpMPgBF, 
                            pstML->nCtxBufSize + pstML->nPMTBufSize);
            
            for (i = 0; i < pstZI->nNumLogGrpPerPMT; i++)
            {
                if (pstPMT->astLogGrps[i].pstFm->nDgn != NULL_DGN)
                {
                    nDgn = pstPMT->astLogGrps[i].pstFm->nDgn;
                    nZBCPMT = FSR_STL_GetZBC((UINT8*)&(pstPMT->pBuf[pstPMT->astLogGrps[i].nBufSize * i]),
                                                       pstPMT->astLogGrps[i].nCfmBufSize);

                    if ((pstPMT->astLogGrps[i].pstCfm->nZBC != nZBCPMT) ||
                        ((pstPMT->astLogGrps[i].pstCfm->nZBC ^ 0xFFFFFFFF) != pstPMT->astLogGrps[i].pstCfm->nInvZBC))
                    {
                        bCorrupted = TRUE32;
                        break;
                    }
                }
            }

            if (bCorrupted == TRUE32)
            {
                bCheckLocked = TRUE32;
                continue;
            }
        }

        nZBCCtx = FSR_STL_GetZBC((UINT8*)pstCI->pBuf,
                                         pstCI->nCfmBufSize);

        if ((pstCI->pstCfm->nZBC != nZBCCtx) ||
            ((pstCI->pstCfm->nZBC ^ 0xFFFFFFFF) != pstCI->pstCfm->nInvZBC))
        {
            bCheckLocked = TRUE32;
            continue;
        }

        if (bCheckLocked == TRUE32)
        {
            /* if corrupted page is more than 1 page, process lock partition */
            /* process to return READ Only partition */
            nLockRet = FSR_STL_LockPartition(pstZone->nClstID);
            if (nLockRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nLockRet));
                return nLockRet;
            }
        }

        /* update the meta blocks' valid page count in the header */
        pstDH->pValidPgCnt[nHeaderIdx]++;

        /* search the VPN and find out the previous meta block index */
        if (nMetaType == MT_BMT)
        {
            if (pstDH->pBMTDir[nLan] != NULL_POFFSET)
            {
                nPrevHdrIdx = (UINT16)(pstDH->pBMTDir[nLan] >> pstDVI->nPagesPerSbShift);
            }
            else
            {
                nPrevHdrIdx = NULL_POFFSET;
            }
        }
        else
        {
            FSR_STL_SearchMetaPg(pstZone, nDgn, &nMetaPOffs);
            if (nMetaPOffs != NULL_POFFSET)
            {
                nPrevHdrIdx = (UINT16)(nMetaPOffs >> pstDVI->nPagesPerSbShift);
            }
            else
            {
                nPrevHdrIdx = NULL_POFFSET;
            }
        }

        /**
         * need to search previous vbn 'for () loop' because of meta vbn list changed 
         * when meta wear-leveling occurs.
         */
        if (nPrevHdrIdx != NULL_POFFSET)
        {
            /* nPrevHdrIdx : meta block index having old map unit */
            FSR_ASSERT(pstDH->pValidPgCnt[nPrevHdrIdx] >= 1);
            pstDH->pValidPgCnt[nPrevHdrIdx]--;

            /**
             * process invalid meta block
             * if valid page count becomes to zero, that block should be
             * registered into idle block list.
             */
            if (pstDH->pValidPgCnt[nPrevHdrIdx] == 0)
            {
                nIdleIdx = (pstDH->pstFm->nIdleBoListHead + pstDH->pstFm->nNumOfIdleBlk)
                            % pstZI->nNumMetaBlks;
                pstDH->pIdleBoList[nIdleIdx] = (UINT16)(nPrevHdrIdx);
                pstDH->pstFm->nNumOfIdleBlk += 1;

                FSR_ASSERT(pstDH->pstFm->nNumOfIdleBlk <= pstZI->nNumMetaBlks);
            }
        }

        nMetaPOffs = (POFFSET)((nHeaderIdx << pstDVI->nPagesPerSbShift) 
                   + (nVpn & (pstDVI->nPagesPerSBlk - 1)));

        if (nMetaType == MT_BMT)
        {
            /*update the new position of BMT (this header block) */
            pstDH->pBMTDir[nLan] = nMetaPOffs;

             /* Process nMergeVictimFlag for power-off recovery between GC & ReGC */
            if (pstCIFm->nMergeVictimDgn != NULL_DGN)
            {
                if (pstCIFm->nMergeVictimFlag == 1)
                {
                    /* set the flag bit */
                    FSR_STL_SetMergeVictimGrp(pstZone,
                                          pstCIFm->nMergeVictimDgn,
                                          TRUE32);
                }
                else
                {
                    /* clear the flag bit */
                    FSR_STL_SetMergeVictimGrp(pstZone,
                                          pstCIFm->nMergeVictimDgn,
                                          FALSE32);
                }
            }
        }
        else
        {
            for (i = 0; i < pstZI->nNumLogGrpPerPMT; i++)
            {
                pstLogGrp = &(pstPMT->astLogGrps[i]);
                nDgn = pstLogGrp->pstFm->nDgn;

                if ((nDgn == NULL_DGN) ||
                    (nDgn >= (pstZI->nMaxPMTDirEntry)))
                {
                    continue;
                }
                
                /* update the new position of PMT (this header block) */
                FSR_STL_UpdatePMTDir(pstZone,
                                 pstLogGrp->pstFm->nDgn,
                                 nMetaPOffs,
                                 pstLogGrp->pstFm->nMinEC,
                                 pstLogGrp->pstFm->nMinVbn,
                                 TRUE32);

                /* update WL group index */
                if (pstCIFm->nUpdatedPMTWLGrpIdx != 0xFFFF)
                {
                    pstDH->pstFm->nMinECPMTIdx = pstCI->pstFm->nMinECPMTIdx;
                    pstDH->pstPMTWLGrp[pstCIFm->nUpdatedPMTWLGrpIdx].nDgn = pstCIFm->stUpdatedPMTWLGrp.nDgn;
                    pstDH->pstPMTWLGrp[pstCIFm->nUpdatedPMTWLGrpIdx].nMinEC = pstCIFm->stUpdatedPMTWLGrp.nMinEC;
                    pstDH->pstPMTWLGrp[pstCIFm->nUpdatedPMTWLGrpIdx].nMinVbn = pstCIFm->stUpdatedPMTWLGrp.nMinVbn;
                }
            }

             /**
              * Process nMergeVictimFlag for power-off recovery between GC & ReGC
              * or log compaction
              */
            if (pstCIFm->nMergeVictimDgn != NULL_DGN)
            {
                if (pstCIFm->nMergeVictimFlag == 1)
                {
                    /* set the flag bit */
                    FSR_STL_SetMergeVictimGrp(pstZone,
                                          pstCIFm->nMergeVictimDgn,
                                          TRUE32);
                }
                else
                {
                    /* clear the flag bit */
                    FSR_STL_SetMergeVictimGrp(pstZone,
                                          pstCIFm->nMergeVictimDgn,
                                          FALSE32);
                }
            }

            /* Update LogGrp Cost in Hdr */
            if (pstCIFm->nModifiedDgn != NULL_DGN)
            {
                pstDH->pstPMTDir[pstCIFm->nModifiedDgn].nCost = pstCIFm->nModifiedCost;
            }
        }

        /* set valid page index */
        pstDH->pstFm->nLatestPOffs = (UINT16)(nIdx);

        /* set valid page header index */
        pstDH->pstFm->nLatestBlkIdx = (UINT16)(nHeaderIdx);
    }

    pstDH->pstFm->nCPOffs = (POFFSET)(nIdx);
    pstDH->pstFm->nCPOffs &= (nPgsPerSBlk - 1);

    FSR_STL_FreeVFLParam(pstZone, pstVFLParam);

    FSR_STL_SetBMTHdl(pstZone,
                      pstBMT,
                      &(pstZone->pFullBMTBuf[pstBMT->pstFm->nLan * pstML->nBMTBufSize]),
                      pstML->nBMTBufSize);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}

/**
 * @brief           This function checks if meta block reclaim is necessary or not
 *
 * @param[in]       pstZone       : pointer to atl Zone object
 * @param[in]       nHeaderIdx    : header block index
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PRIVATE INT32
_CheckMetaMerge(STLZoneObj *pstZone,
                UINT32 nHeaderIdx)
{
    BADDR           nHeaderVbn      = NULL_VBN;
    PADDR           nVpn            = NULL_VPN;
    INT32           nRet;
    UINT32          nIdx;
    UINT32          nIdleIdx        = 0;
    STLDirHdrHdl   *pstDH           = NULL;
    RBWDevInfo     *pstDVI          = NULL;
    STLZoneInfo    *pstZI           = NULL;
    STLMetaLayout  *pstML           = NULL;
    UINT8          *pMPgBF          = NULL;
    UINT8          *pTmpMPgBF       = NULL;
    STLBMTHdl      *pstBMT          = NULL;
    STLPMTHdl      *pstPMT          = NULL;
    STLCtxInfoHdl  *pstCI           = NULL;
    VFLParam       *pstVFLParam     = NULL;
    UINT32          nVictimIdx      = 0;
    UINT16          nVictimPgCnt    = 0;
    UINT16          nMetaType;
    UINT32          nZBCBMT;
    UINT32          nZBCPMT;
    UINT32          nZBCCtx;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    pstDVI          = pstZone->pstDevInfo;
    pstDH           = pstZone->pstDirHdrHdl;
    pMPgBF          = pstZone->pMetaPgBuf;
    pTmpMPgBF       = pstZone->pTempMetaPgBuf;
    pstBMT          = pstZone->pstBMTHdl;
    pstPMT          = pstZone->pstPMTHdl;
    pstCI           = pstZone->pstCtxHdl;
    pstZI           = pstZone->pstZI;
    pstML           = pstZone->pstML;
    pstVFLParam     = FSR_STL_AllocVFLParam(pstZone);

    /**
     * when consecutive power off occurs only after header page programs, 
     * victim meta block does not have valid page.
     * according to above, victim meta block should be idle block when open.
     */
    if (pstDH->pstFm->nNumOfIdleBlk == 0)
    {
        for (nIdx = 0; nIdx < pstZI->nNumMetaBlks; nIdx++)
        {
            if ((nIdx != nHeaderIdx) && 
                (pstDH->pValidPgCnt[nIdx] == 0))
            {
                nIdleIdx = (pstDH->pstFm->nIdleBoListHead + pstDH->pstFm->nNumOfIdleBlk)
                            % pstZI->nNumMetaBlks;
                pstDH->pIdleBoList[nIdleIdx] = (UINT16)(nIdx);
                pstDH->pstFm->nNumOfIdleBlk += 1;
            }
        }
    }
    
    /**
     * when sudden power off occurs during meta reclaim process,
     * open process has to recover meta reclaim
     */
    if (pstDH->pstFm->nNumOfIdleBlk == 0)
    {

        /**
         * 1. Read the Directory header page. Get Directory header VBN address
         */
        nHeaderVbn = pstZI->aMetaVbnList[nHeaderIdx];

        nRet = FSR_STL_ReadHeader(pstZone, nHeaderVbn);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
            return nRet;
        }

        pstDH->pstFm->nHeaderIdx = (UINT16)nHeaderIdx;
        
        /** 
         * 2. Read latest cxtInfo page, identity meta type of latest valid meta page
         */
        pstVFLParam->pData        = (UINT8*)pTmpMPgBF;
        pstVFLParam->nBitmap      = pstDVI->nFullSBitmapPerVPg;
        pstVFLParam->bPgSizeBuf   = FALSE32;
        pstVFLParam->bUserData    = FALSE32;
        pstVFLParam->bSpare       = TRUE32;
        pstVFLParam->nNumOfPgs    = 1;
        pstVFLParam->pExtParam    = NULL;

        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
            (TEXT("[SIF:DBG] FSR_STL_FlashRead : nVpn = %d(%d)\r\n"), nVpn, nVpn % pstDVI->nPagesPerSBlk));

        if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
        {
            /* adjust into LSB page address, when it is MLC */
            nVpn = (pstZI->aMetaVbnList[pstDH->pstFm->nLatestBlkIdx] << pstDVI->nPagesPerSbShift)
                 + FSR_BML_GetVPgOffOfLSBPg(pstZone->nVolID, pstDH->pstFm->nLatestPOffs);
        }
        else
        {
            nVpn = (pstZI->aMetaVbnList[pstDH->pstFm->nLatestBlkIdx] << pstDVI->nPagesPerSbShift)
                 + pstDH->pstFm->nLatestPOffs;
        }

        nRet = FSR_STL_FlashCheckRead(pstZone, nVpn, pstVFLParam, 1, TRUE32);
        if (nRet == FSR_BML_READ_ERROR)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
            return nRet;
        }
        else if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
            return nRet;
        }

        nMetaType = (UINT16)(pstVFLParam->nSData1 >> 16);
        
        /* according to the meta type, set read buffer point */
        if (nMetaType == MT_BMT)
        {
            FSR_OAM_MEMCPY(pMPgBF, pTmpMPgBF, pstML->nBMTBufSize +
                            pstML->nCtxBufSize);
        }
        else
        {
            FSR_OAM_MEMCPY(&(pMPgBF[pstBMT->nBufSize]), pTmpMPgBF, 
                            pstML->nCtxBufSize + pstML->nPMTBufSize);
        }


        /* calculate zero bit count */
        if (nMetaType == MT_BMT)
        {
            nZBCBMT = FSR_STL_GetZBC((UINT8*)pstBMT->pBuf,
                                             pstBMT->nCfmBufSize);

            if ((pstBMT->pstCfm->nZBC != nZBCBMT) ||
                ((pstBMT->pstCfm->nZBC ^ 0xFFFFFFFF) != pstBMT->pstCfm->nInvZBC))
            {
                FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                    (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, FSR_STL_META_BROKEN));
                return FSR_STL_META_BROKEN;
            }
        }
        else
        {
            for (nIdx = 0; nIdx < pstZI->nNumLogGrpPerPMT; nIdx++)
            {
                nZBCPMT = FSR_STL_GetZBC((UINT8*)&(pstPMT->pBuf[pstPMT->astLogGrps[nIdx].nBufSize * nIdx]),
                                                   pstPMT->astLogGrps[nIdx].nCfmBufSize);

                if ((pstPMT->astLogGrps[nIdx].pstCfm->nZBC != nZBCPMT) ||
                    ((pstPMT->astLogGrps[nIdx].pstCfm->nZBC ^ 0xFFFFFFFF) != pstPMT->astLogGrps[nIdx].pstCfm->nInvZBC))
                {
                    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, FSR_STL_META_BROKEN));
                    return FSR_STL_META_BROKEN;
                }
            }
        }

        nZBCCtx = FSR_STL_GetZBC((UINT8*)pstCI->pBuf,
                                         pstCI->nCfmBufSize);

        if ((pstCI->pstCfm->nZBC != nZBCCtx) ||
            ((pstCI->pstCfm->nZBC ^ 0xFFFFFFFF) != pstCI->pstCfm->nInvZBC))
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, FSR_STL_META_BROKEN));
            return FSR_STL_META_BROKEN;
        }

        /**
         * 3. Header block erase ( Open meta merge policy: reclaim after erase)
         */
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
            (TEXT("[SIF:DBG] FSR_STL_FlashErase : nVbn = %d\r\n"), nHeaderVbn));

        /*  increase meta block erase count */
        pstDH->pMetaBlksEC[nHeaderIdx]++;

        nRet = FSR_STL_FlashErase(pstZone, nHeaderVbn);
        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
            return nRet;
        }

        /**
         * 4. Header page program 
         */
        nRet = FSR_STL_ProgramHeader(pstZone, nHeaderVbn);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
            return nRet;
        }

        /* reset victim meta page index */
        nVictimIdx = pstDH->pstFm->nVictimIdx;
        nVictimPgCnt = pstDH->pValidPgCnt[nVictimIdx];

        /**
         * 5. initiates meta reclaim
         */
        nRet = FSR_STL_ReclaimMeta(pstZone,
                                   nHeaderVbn,
                                   (UINT16)nHeaderIdx,
                                   nVictimIdx,
                                   nVictimPgCnt, 
                                   FALSE32);

        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
            return nRet;
        }
    }

    FSR_STL_FreeVFLParam(pstZone, pstVFLParam);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}

/**
 * @brief           This function reads the latest context page
 *
 * @param[in]       pstZone         : pointer to atl Zone object
 * @param[in]       nFlag           :  Zone attribute flag
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PRIVATE INT32
_LoadLatestCxt (STLZoneObj *pstZone,
                UINT32      nFlag)
{
    UINT16          nMetaType;
    UINT32          nZBCBMT;
    UINT32          nZBCPMT;
    UINT32          nZBCCtx;
    INT32           nRet;
    UINT32          nIdx;
    UINT32          nPgsPerSBlk     = 0;
    BADDR           nVPgHeaderVbn   = NULL_VBN;
    PADDR           nVpn            = NULL_VPN;
    BOOL32          bMetaWL         = FALSE32;
    RBWDevInfo     *pstDVI          = NULL;
    STLDirHdrHdl   *pstDH           = NULL;
    STLBMTHdl      *pstBMT          = NULL;
    STLPMTHdl      *pstPMT          = NULL;
    UINT8          *pMPgBF          = NULL;
    UINT8          *pTmpMPgBF       = NULL;
    STLZoneInfo    *pstZI           = NULL;
    STLRootInfo    *pstRI           = NULL;
    STLCtxInfoHdl  *pstCI           = NULL;
    STLCtxInfoFm   *pstCIFm         = NULL;
    STLMetaLayout  *pstML           = NULL;
    BOOL32          bStorePMT       = FALSE32;
    BOOL32          bLogFound;
    BADDR           nInvLbn         = NULL_VBN;
    POFFSET         nLbOffs         = NULL_POFFSET;
    STLLog         *pstLog          = NULL;
    STLLogGrpHdl   *pstLogGrp       = NULL;
    UINT16          tempBMTLan      = 0;
    POFFSET         nMetaPOffs      = NULL_POFFSET;
    VFLParam       *pstVFLParam     = NULL;
    INT32           nDBIdx;
    BOOL32          bMergeVictimGrp;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    pstDVI          = pstZone->pstDevInfo;
    pstRI           = pstZone->pstRI;
    pstDH           = pstZone->pstDirHdrHdl;
    pstML           = pstZone->pstML;
    pMPgBF          = pstZone->pMetaPgBuf;
    pTmpMPgBF       = pstZone->pTempMetaPgBuf;
    pstBMT          = pstZone->pstBMTHdl;
    pstPMT          = pstZone->pstPMTHdl;
    pstZI           = pstZone->pstZI;
    pstCI           = pstZone->pstCtxHdl;
    pstCIFm         = pstCI->pstFm;
    nPgsPerSBlk     = pstDVI->nPagesPerSBlk;
    pstVFLParam     = FSR_STL_AllocVFLParam(pstZone);

    FSR_STL_SetBMTHdl(pstZone,
                      pstBMT,
                      pMPgBF,
                      pstML->nBMTBufSize);

    if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
    {
        /**
         * if MLC, pages per super block is half because of consuming LSB page
         * MLC LSB Half Pages Only
         */
        nPgsPerSBlk >>= 1;
    }

    pstVFLParam->pData        = (UINT8*)(pTmpMPgBF);
    pstVFLParam->nBitmap      = pstDVI->nFullSBitmapPerVPg;
    pstVFLParam->bPgSizeBuf   = FALSE32;
    pstVFLParam->bUserData    = FALSE32;
    pstVFLParam->bSpare       = TRUE32;
    pstVFLParam->nNumOfPgs    = 1;
    pstVFLParam->pExtParam    = NULL;

    nVPgHeaderVbn = *(pstZI->aMetaVbnList + pstDH->pstFm->nLatestBlkIdx);
    if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
    {
        nVpn = (nVPgHeaderVbn << pstDVI->nPagesPerSbShift)
             + FSR_BML_GetVPgOffOfLSBPg(pstZone->nVolID, pstDH->pstFm->nLatestPOffs);
    }
    else
    {
        nVpn = (nVPgHeaderVbn << pstDVI->nPagesPerSbShift)
             + pstDH->pstFm->nLatestPOffs;
    }

    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:INF]   Current Context BlockIdx   (%5d)  Offset (%5d)  Vpn (%5d) \r\n"), 
        pstDH->pstFm->nLatestBlkIdx, pstDH->pstFm->nLatestPOffs, nVpn));

    nRet = FSR_STL_FlashCheckRead(pstZone, nVpn, pstVFLParam, 1, TRUE32);
    if (nRet == FSR_BML_READ_ERROR)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR]  --%s() L(%d) : 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }
    else if (nRet != FSR_BML_SUCCESS)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR]  --%s() L(%d) : 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }
        
    nMetaType = (UINT16)(pstVFLParam->nSData1 >> 16);
    FSR_ASSERT(nMetaType == MT_BMT || nMetaType == MT_PMT);

    if (nMetaType == MT_BMT)
    {
        FSR_OAM_MEMCPY(pMPgBF, pTmpMPgBF,
                       pstML->nBMTBufSize + pstML->nCtxBufSize);
    }
    else
    {
        FSR_OAM_MEMCPY(&(pMPgBF[pstBMT->nBufSize]), pTmpMPgBF, 
                        pstML->nCtxBufSize + pstML->nPMTBufSize);
    }
    
    /* calculate zero bit count for meta page confirm */
    if (nMetaType == MT_BMT)
    {
        nZBCBMT = FSR_STL_GetZBC((UINT8*)pstBMT->pBuf,
                                         pstBMT->nCfmBufSize);

        if ((pstBMT->pstCfm->nZBC != nZBCBMT) ||
            ((pstBMT->pstCfm->nZBC ^ 0xFFFFFFFF) != pstBMT->pstCfm->nInvZBC))
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, FSR_STL_META_BROKEN));
            return FSR_STL_META_BROKEN;
        }
    }
    else
    {
        for (nIdx = 0; nIdx < pstZI->nNumLogGrpPerPMT; nIdx++)
        {
            pstLogGrp = &(pstPMT->astLogGrps[nIdx]);

            if (pstLogGrp->pstFm->nDgn != NULL_DGN)
            {
                nZBCPMT = FSR_STL_GetZBC((UINT8*)pstLogGrp->pBuf,
                                                 pstLogGrp->nCfmBufSize);

                if ((pstLogGrp->pstCfm->nZBC != nZBCPMT) ||
                    ((pstLogGrp->pstCfm->nZBC ^ 0xFFFFFFFF) != pstLogGrp->pstCfm->nInvZBC))
                {
                    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, FSR_STL_META_BROKEN));
                    return FSR_STL_META_BROKEN;
                }
            }
            
        }
    }

    nZBCCtx = FSR_STL_GetZBC((UINT8*)pstCI->pBuf, pstCI->nCfmBufSize);
    if ((nZBCCtx != pstCI->pstCfm->nZBC) ||
       ((pstCI->pstCfm->nZBC ^ 0xFFFFFFFF) != pstCI->pstCfm->nInvZBC))
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, FSR_STL_META_BROKEN));
        return FSR_STL_META_BROKEN;
    }

    /*  reset merge information in the context */
    {
        pstCI->pstFm->nMergedDgn                 = NULL_DGN;
        pstCI->pstFm->nMergedVbn                 = NULL_VBN;
        pstCI->pstFm->nMergeVictimDgn            = NULL_DGN;
        pstCI->pstFm->nMergeVictimFlag           = 0;

        pstCI->pstFm->nUpdatedPMTWLGrpIdx        = 0xFFFF;
        pstCI->pstFm->nMinECPMTIdx               = 0xFFFF;
        pstCI->pstFm->stUpdatedPMTWLGrp.nDgn     = NULL_DGN;
        pstCI->pstFm->stUpdatedPMTWLGrp.nMinEC   = 0xFFFFFFFF;
        pstCI->pstFm->stUpdatedPMTWLGrp.nMinVbn  = NULL_VBN;

        /* reset modified cost information in the context (POR) */
        pstCI->pstFm->nModifiedDgn               = NULL_DGN;
        pstCI->pstFm->nModifiedCost              = (UINT16)(-1);
    }

    /* if last page is PMTCxt, load LAN 0 BMT. */
    if (nMetaType == MT_PMT)
    {
        tempBMTLan = 0;
        pstBMT->pstFm->nLan = NULL_DGN;

        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
            (TEXT("[SIF:INF] MapIdx = %d\n"), tempBMTLan));

        nRet = FSR_STL_LoadBMT(pstZone,
                           tempBMTLan,
                           TRUE32);

        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
            return nRet;
        }
    }
    else
    {
        FSR_STL_SetBMTHdl(pstZone,
                  pstBMT,
                  &(pstZone->pFullBMTBuf[pstBMT->pstFm->nLan * pstML->nBMTBufSize]),
                  pstML->nBMTBufSize);
    }

    FSR_ASSERT(pstBMT->pBuf != pMPgBF);
    
    /** 
     * process : MergedDgn & MergedVbn
     * check nMergedDgn is NULL 
     */
    if (pstCIFm->nMergedDgn != NULL_DGN)
    {
        bStorePMT = FALSE32;
        
        /* Search a PMTDir about nMergedDgn */
        nRet = FSR_STL_SearchPMTDir(pstZone,
                         pstCIFm->nMergedDgn,
                         &nMetaPOffs);

        if (nMetaPOffs != NULL_POFFSET)
        {
            pstLogGrp = &(pstPMT->astLogGrps[pstCIFm->nMergedDgn % pstZI->nNumLogGrpPerPMT]);
            
            nRet = FSR_STL_LoadPMT(pstZone,
                               pstCIFm->nMergedDgn,
                               nMetaPOffs,
                               &pstLogGrp,
                               TRUE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                    (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
                return nRet;
            }
            
            bLogFound = FSR_STL_SearchLog(pstLogGrp,
                                      pstCIFm->nMergedVbn,
                                      &pstLog);

            if (bLogFound == TRUE32)
            {
                FSR_ASSERT(pstLog != NULL);
                
                /* re-construct PMT in LogGrp for removing nMergedVbn */
                for (nDBIdx = 0; nDBIdx < *(pstLogGrp->pNumDBlks + pstLog->nSelfIdx); nDBIdx++)
                {
                    nInvLbn = *(pstLogGrp->pLbnList + pstLog->nSelfIdx * pstZone->pstRI->nN);
                    /*
                    nLbOffs = nInvLbn % pstRI->nN;
                    FSR_OAM_MEMSET(&(pstLogGrp->pMapTbl[nLbOffs * nPgsPerSBlk]), 0xFF, 
                                nPgsPerSBlk * sizeof(POFFSET));
                    */
                    nLbOffs = nInvLbn & (pstRI->nN - 1);
                    FSR_OAM_MEMSET(&(pstLogGrp->pMapTbl[nLbOffs << pstDVI->nPagesPerSbShift]), 0xFF, 
                                sizeof(POFFSET) << pstDVI->nPagesPerSbShift);
                }
                
                FSR_STL_RemoveLog(pstZone,
                                  pstLogGrp,
                                  pstLog);

                bStorePMT = TRUE32;

                pstDH->pstFm->nCPOffs = (POFFSET)(pstDVI->nPagesPerSBlk);

                if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
                {
                    pstDH->pstFm->nCPOffs &= ((pstDVI->nPagesPerSBlk >> 1) - 1);
                }
                else
                {
                    pstDH->pstFm->nCPOffs &= (pstDVI->nPagesPerSBlk - 1);
                }

                /* if Zone attribute is read only, prevent to program */
                if ((nFlag & FSR_STL_FLAG_RO_PARTITION) == 0)
                {
                    if (pstDH->pstFm->nCPOffs == 0)
                    {
                        /* protect from occurring meta & data WearLevel coincidentally */
                        nRet = FSR_STL_StoreDirHeader(pstZone,
                                                  FALSE32,
                                                  &bMetaWL);
                        if (nRet != FSR_STL_SUCCESS)
                        {
                            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                                    __FSR_FUNC__, __LINE__, nRet));
                            return nRet;
                        }
                    }

                    if (pstLogGrp->pstFm->nNumLogs == 0)
                    {
                        /* clear the flag bit */
                        FSR_STL_SetMergeVictimGrp(pstZone,
                                              pstLogGrp->pstFm->nDgn,
                                              FALSE32);
                    }

                    /* Store PMTCxt for update PMT */
                    nRet = FSR_STL_StorePMTCtx(pstZone,
                                               pstLogGrp,
                                               FALSE32);
                    if (nRet != FSR_STL_SUCCESS)
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                                __FSR_FUNC__, __LINE__, nRet));
                        return nRet;
                    }
                }

                /**************************************************************/
                /*             Process : Set PMTMergeFlag                     */
                /**************************************************************/
                /**
                 * In this case, power off occurs during merge process
                 * check if the victim group can be merge victim or not.
                 */
                nRet = FSR_STL_CheckMergeVictimGrp(pstZone,
                                                          pstLogGrp,
                                                          FALSE32,
                                                          &bMergeVictimGrp);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    return nRet;
                }
            
                if (bMergeVictimGrp == TRUE32)
                {
                    /* set the flag bit */
                    FSR_STL_SetMergeVictimGrp(pstZone,
                                          pstLogGrp->pstFm->nDgn,
                                          TRUE32);
                }
                else
                {
                    FSR_STL_SetMergeVictimGrp(pstZone,
                                          pstLogGrp->pstFm->nDgn,
                                          FALSE32);
                }
            }
        }
    }

    /**
     * if CPO is zero when PMTCxt is stored after processing MergedDgn,
     * meta block should be changed and then,
     * prevent to change meta block for Soft Program issue using bStorePMT.
     */
    if (bStorePMT == FALSE32)
    {
        if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
        {
            /**
             * if MLC, pages per super block is half because of consuming LSB page
             * MLC LSB Half Pages Only
             */
            pstDH->pstFm->nCPOffs = (POFFSET)(pstDVI->nPagesPerSBlk >> 1);
        }
        else
        {
            pstDH->pstFm->nCPOffs = (POFFSET)(pstDVI->nPagesPerSBlk);
        }
    }

    FSR_STL_FreeVFLParam(pstZone, pstVFLParam);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;

}

#if (OP_SUPPORT_FONAND_RELIABLE_WRITE == 1)

/**
 * @brief           This function reads backuped LSB pages using FlexOneNAND special command
 *
 * @param[in]       pstZone          : pointer to atl Zone object
 * @param[in]       pLogGrp          : pointer to log group
 * @param[in]       pLogObj          : pointer to log object
 * @param[in]       nDgn             : data group number
 * @param[in]       nValidIdx        : valid page index
 * @param[in]       nLastVpn         : last broken vpn
 * @param[in]       nConfirmIdx      : flag for confirm index.
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PRIVATE INT32
_FONANDReliableWrite   (STLZoneObj     *pstZone,
                        STLLogGrpHdl   *pLogGrp,
                        STLLog         *pLogObj,
                        const UINT32    LogIdx,
                        const UINT32    nLastOff,
                        const POFFSET   nConfirmIdx,
                        const POFFSET   nPrevConfim)
{
    RBWDevInfo     *pstDVI          = pstZone->pstDevInfo;
    STLRootInfo    *pstRI           = pstZone->pstRI;
    STLCtxInfoHdl  *pstCI           = pstZone->pstCtxHdl;
    STLDirHdrHdl   *pstDH           = pstZone->pstDirHdrHdl;
    UINT8          *pTmpMPgBF       = pstZone->pTempMetaPgBuf;
    const   BADDR   nDgn            = pLogGrp->pstFm->nDgn;
    BADDR           nLbn;
    PADDR           nLpn;
    POFFSET         nLpo            = NULL_POFFSET;
    VFLParam       *pstParam        = NULL;
    UINT32          nIdx;
    UINT32          nWayIdx;
    STLActLogEntry *pActLog         = NULL;
    STLPMTWLGrp    *stWLGrp;
    BADDR           nFBlkVbn;
    UINT32          nFBlkEC;
    const   BADDR   nSrcVbn         = pLogObj->nVbn;
    const   UINT32  nSrcEC          = pLogObj->nEC;
    PADDR           nSrcVpn         = NULL_VPN;
    PADDR           nDstVpn         = NULL_VPN;
    const   UINT32  nSBitmap        = 0xFFFFFFFF;
    BOOL32          bLsbPg          = TRUE32;
    BOOL32          bConfirm;
    BOOL32          bReliableRead   = FALSE32;
    BOOL32          bCorrupted      = FALSE32;
    INT32           nRet;
    INT32           nLockRet;
    BOOL32          bLocked         = FALSE32;
    UINT32          nLastConfirm    = nConfirmIdx;
    POFFSET         nPairedIdx[FSR_MAX_WAYS];
    /* CAUTION : pTempPgBuf may be used in FSR_STL_ReserveMetaPgs() and FSR_STL_StorePMT() */
    STLTmpVPmt     *pstVPmt         = (STLTmpVPmt *)(FSR_STL_GetClstObj(pstZone->nClstID)->pTempPgBuf);
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:INF] R1-Recovery1 : DGN(%5d) VBN(%5d) PG[%3d-%3d-%3d]\r\n"),
            nDgn, nSrcVbn, nLastOff, nConfirmIdx, pLogObj->nCPOffs));

    /* reserve Meta page */
    nRet = FSR_STL_ReserveMetaPgs(pstZone, 1, FALSE32);
    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }

    /* search paired LSB index for processing read & program operation */
    FSR_OAM_MEMSET(nPairedIdx, 0xFF, sizeof (nPairedIdx[FSR_MAX_WAYS]));
    for (nWayIdx = 0; nWayIdx < pstDVI->nNumWays; nWayIdx++)
    {
        if (pLogObj->nCPOffs > nWayIdx)
        {
            bLsbPg = FSR_STL_FlashIsLSBPage(pstZone, (pLogObj->nCPOffs - (nWayIdx + 1)));
            if (bLsbPg == FALSE32)
            {
                nPairedIdx[nWayIdx] = (POFFSET)FSR_BML_GetPairedVPgOff(pstZone->nVolID, (pLogObj->nCPOffs - (nWayIdx + 1)));
            }
        }
    }

    /* get free block */
    nFBlkVbn = pstCI->pFreeList[pstCI->pstFm->nFreeListHead];
    nFBlkEC  = pstCI->pFBlksEC [pstCI->pstFm->nFreeListHead] + 1;

    /* Get buffer blocks' VBN */
    nRet = FSR_STL_FlashErase(pstZone, nFBlkVbn);
    if (nRet != FSR_BML_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }

    /* Swap VBNs & ECs */
    pstCI->pFreeList[pstCI->pstFm->nFreeListHead] = nSrcVbn;
    pstCI->pFBlksEC [pstCI->pstFm->nFreeListHead] = nSrcEC;

    /* Log->nVbn change */
    pLogObj->nVbn = nFBlkVbn;
    pLogObj->nEC  = nFBlkEC;

    pstParam              = FSR_STL_AllocVFLParam(pstZone);
    pstParam->pData       = (UINT8*)(pTmpMPgBF);
    pstParam->nBitmap     = pstDVI->nFullSBitmapPerVPg;
    pstParam->bPgSizeBuf  = TRUE32;
    pstParam->bUserData   = TRUE32;
    pstParam->bSpare      = TRUE32;
    pstParam->nNumOfPgs   = 1;
    pstParam->pExtParam   = FSR_STL_AllocVFLExtParam(pstZone);
    pstParam->pExtParam->nNumExtSData = (UINT16)pstDVI->nSecPerVPg;

    /**
     * 1. recover paired LSB page and obtain free block
     */

    /* initialize Log State & CPO*/
    pLogObj->nCPOffs = 0;
    pLogObj->nState  &= ~LS_STATES_BIT_MASK;
    pLogObj->nState  |= LS_ALLOC;

    /**
     * copy log block to new free block (before broken MSB page)
     * copy paired LSB page to new free block using R1_read() command
     */
    for (nIdx = 0; nIdx <= nConfirmIdx; nIdx++)
    {
        pstVPmt[nIdx].bClean      = FALSE32;
        pstVPmt[nIdx].bConfirm    = FALSE32;
        pstVPmt[nIdx].nCorrupted  = PAGE_CORRUPTED_NONE;
        pstVPmt[nIdx].nLpn        = NULL_VPN;

        bReliableRead = FALSE32;
        bCorrupted    = FALSE32;

        nSrcVpn = (nSrcVbn       << pstDVI->nPagesPerSbShift) + nIdx;
        nDstVpn = (pLogObj->nVbn << pstDVI->nPagesPerSbShift) + nIdx;

        pLogObj->nCPOffs++;

        bLsbPg = FSR_STL_FlashIsLSBPage(pstZone, nIdx);
        nRet   = FSR_STL_FlashCheckRead(pstZone, nSrcVpn, pstParam, MAX_TRY_COUNT_FOR_CHECK_READ, FALSE32);
        if (nRet == FSR_STL_CLEAN_PAGE)
        {
            pstVPmt[nIdx].bClean = TRUE32;
            continue;
        }
        else if (nRet == FSR_BML_READ_DISTURBANCE_ERROR)
        {
            pstVPmt[nIdx].nCorrupted = PAGE_READ_DISTURBANCE;
            bReliableRead = TRUE32;
        }
        else if (nRet == FSR_BML_READ_ERROR)
        {
            pstVPmt[nIdx].nCorrupted = PAGE_CORRUPTED_ALONE;
            if (bLsbPg == TRUE32)
            {
                nRet = FSR_STL_FlashReliableRead(pstZone, nSrcVpn, pstParam);
                if (nRet == FSR_BML_SUCCESS)
                {
                    pstVPmt[nIdx].nCorrupted = PAGE_CORRUPTED_PAIRED;
                    bReliableRead = TRUE32;
                }
            }
        }
        else if (nRet != FSR_BML_SUCCESS)
        {
            bLocked = TRUE32;
            break;
        }

        for (nWayIdx = 0; nWayIdx < pstDVI->nNumWays; nWayIdx++)
        {
            if ((nPairedIdx[nWayIdx] != NULL_POFFSET) &&
                (nIdx == nPairedIdx[nWayIdx]))
            {
                bReliableRead = TRUE32;
                break;
            }
        }

        FSR_ASSERT(bLocked == FALSE32);
        bCorrupted = _CheckPageValidity(pstZone, pstParam, bLsbPg, &nLpo, &bConfirm);
        if (bCorrupted == TRUE32)
        {
            pstVPmt[nIdx].nCorrupted |= PAGE_CORRUPTED_ALONE;
        }

        nLbn = nDgn << pstRI->nNShift;
        nLpn = ((nLbn << pstRI->nNShift) << pstDVI->nPagesPerSbShift) + nLpo;

        pstVPmt[nIdx].bConfirm = (UINT8)bConfirm;
        pstVPmt[nIdx].nLpn     = nLpn;

        if ((pstVPmt[nIdx].nCorrupted & PAGE_CORRUPTED_ALONE) != 0)
        {
            if ((nPrevConfim == NULL_POFFSET) ||
                (nIdx        >  nPrevConfim ))
            {
                nLastConfirm = nPrevConfim;
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
                    (TEXT("[SIF:INF] R1-Recovery2 : DGN(%5d) VBN(%5d) PG[%3d-%3d-%3d]\r\n"),
                        nDgn, nSrcVbn, nLastOff, nLastConfirm, pLogObj->nCPOffs));
            }
            else
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  %s() L(%d) : unexpected page is corrupted PI[%3d-%3d-%3d\r\n"),
                        __FSR_FUNC__, __LINE__, nIdx, nPrevConfim, nConfirmIdx));
                bLocked = TRUE32;
            }
            break;
        }

        /* copy to new Free Block */
        if (bReliableRead == FALSE32)
        {
            nRet = FSR_STL_FlashCopyback(pstZone,
                                         nSrcVpn,
                                         nDstVpn,
                                         nSBitmap);
        }
        else
        {
            nRet = FSR_STL_FlashProgram(pstZone,
                                        nDstVpn,
                                        pstParam);
        }

        if (nRet != FSR_BML_SUCCESS)
        {
            bLocked = TRUE32;
            break;
        }
    }

    while (bLocked == FALSE32)
    {
        /* Update Page Map Table (PMT) */
        if (nLastConfirm != NULL_POFFSET)
        {
            for (nIdx = nLastOff; nIdx <= nLastConfirm; nIdx++)
            {
                if (pstVPmt[nIdx].bClean == FALSE32)
                {
                    FSR_ASSERT((pstVPmt[nIdx].nCorrupted & PAGE_CORRUPTED_ALONE) == 0);
                    FSR_ASSERT(pstVPmt[nIdx].nLpn >> pstRI->nNShift >> pstDVI->nPagesPerSbShift == nDgn);

                    nDstVpn = (pLogObj->nVbn << pstDVI->nPagesPerSbShift) + nIdx;
                    nLpn    = pstVPmt[nIdx].nLpn;

                    /* If PMT is not updated, log group PMT update */
                    FSR_STL_UpdatePMT(pstZone, pLogGrp, pLogObj, nLpn, nDstVpn);

                    /* change the state of log block */
                    FSR_STL_ChangeLogState(pstZone, pLogGrp, pLogObj, nLpn);
                }
                else
                {
                    FSR_ASSERT(pstVPmt[nIdx].bConfirm   == FALSE32);
                    FSR_ASSERT(pstVPmt[nIdx].nCorrupted == PAGE_CORRUPTED_NONE);
                    FSR_ASSERT(pstVPmt[nIdx].nLpn       == NULL_VPN);
                }
            }
        }
        FSR_STL_UpdatePMTCost(pstZone, pLogGrp);

        pLogObj->nVbn = nSrcVbn;
        pLogObj->nEC  = nSrcEC;
        nRet = FSR_STL_UpdatePMTMinEC(pstZone, pLogGrp, nSrcVbn);
        if (nRet != FSR_STL_SUCCESS)
        {
            bLocked = TRUE32;

            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        pLogObj->nVbn = nFBlkVbn;
        pLogObj->nEC  = nFBlkEC;

        /* Update global wear-level group & self index's minimum */
        if (pLogGrp->pstFm->nMinEC > pLogObj->nEC)
        {
            pLogGrp->pstFm->nMinIdx = pLogObj->nSelfIdx;
            pLogGrp->pstFm->nMinVbn = pLogObj->nVbn;
            pLogGrp->pstFm->nMinEC  = pLogObj->nEC;

            stWLGrp = &(pstZone->pstDirHdrHdl->pstPMTWLGrp[nDgn >> DEFAULT_PMT_EC_GRP_SIZE_SHIFT]);
            if (stWLGrp->nMinEC > pLogObj->nEC)
            {
                stWLGrp->nDgn = pLogGrp->pstFm->nDgn;
                stWLGrp->nMinEC = pLogObj->nEC;
                stWLGrp->nMinVbn = pLogObj->nVbn;

                /* update WLGrp information */
                pstCI->pstFm->nUpdatedPMTWLGrpIdx       = stWLGrp->nDgn >> DEFAULT_PMT_EC_GRP_SIZE_SHIFT;
                pstCI->pstFm->stUpdatedPMTWLGrp.nDgn    = stWLGrp->nDgn;
                pstCI->pstFm->stUpdatedPMTWLGrp.nMinVbn = stWLGrp->nMinVbn;
                pstCI->pstFm->stUpdatedPMTWLGrp.nMinEC  = stWLGrp->nMinEC;                
            }
            FSR_STL_UpdatePMTDirMinECGrp(pstZone, pstZone->pstDirHdrHdl);
            pstCI->pstFm->nMinECPMTIdx = pstDH->pstFm->nMinECPMTIdx;
        }

        pActLog       = &(pstCI->pstActLogList[LogIdx]);
        pActLog->nVbn = pLogObj->nVbn;

        /* store PMT + Ctx */
        nRet = FSR_STL_StorePMTCtx(pstZone, pLogGrp, FALSE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            bLocked = TRUE32;
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
           break;
        }
        break;
    }

    if (bLocked == TRUE32)
    {
        /* roll-back */
        pLogObj->nVbn    = nSrcVbn;
        pLogObj->nEC     = nSrcEC;
        pLogObj->nCPOffs = (POFFSET)pstDVI->nPagesPerSBlk;

        pstCI->pFreeList[pstCI->pstFm->nFreeListHead] = nFBlkVbn;
        pstCI->pFBlksEC [pstCI->pstFm->nFreeListHead] = nFBlkEC;

        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR]  %s() L(%d) : 0x%08x - Locked PG[%d]-RR[%d]-L[%d]-C[%d]\r\n"),
                __FSR_FUNC__, __LINE__, nRet, nIdx, bReliableRead, bLsbPg, bCorrupted));

        nLockRet = FSR_STL_LockPartition(pstZone->nClstID);
        if (nLockRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR]  --%s() L(%d) : 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nLockRet));
        }
    }

    FSR_STL_FreeVFLExtParam(pstZone, pstParam->pExtParam);
    FSR_STL_FreeVFLParam(pstZone, pstParam);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}

#endif /* (OP_SUPPORT_FONAND_RELIABLE_WRITE == 1) */

/**
 * @brief           This function checks the validity of spare data
 *
 * @param[in]       pstZone         : pointer to STL Zone object
 * @param[in]       pstParam        : VFL param for spare data
 * @param[out]      pnLpo           : Logical Page Offset (LPO)
 * @param[out]      pbConfirm       : Whether confirmation page is
 *
 * @return          Whether the spare data is corrupted or not
 *
 * @author          Jongtae Park
 * @version         1.2.0
 */
PRIVATE BOOL32
_CheckPageValidity (STLZoneObj     *pstZone,
                    VFLParam       *pstParam,
                    BOOL32          bLsbPg,
                    POFFSET        *pnLpo,
                    BOOL32         *pbConfirm)
{
    const   RBWDevInfo *pstDVI          = pstZone->pstDevInfo;
            UINT32      nSpareLpo;
            UINT16      nLpo;
            UINT16      nPTF;
            UINT32      nCRC32Val;
            UINT32      nIdx;
            UINT32      nSpareIdx       = 0;                
            UINT32      nSizePerCRC;
            UINT32      nSectorsPerCRC  = 1;
            BOOL32      bCorrupted;

    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    (*pbConfirm) = FALSE32;

    if (pstDVI->nDeviceType & RBW_DEVTYPE_ONENAND)
    {
        nSectorsPerCRC = 2;
    }

    nSizePerCRC = BYTES_PER_SECTOR * nSectorsPerCRC;

    /* get LPN (Logical Page Number) from spare data1 (LSB) or spare data2 (MSB)*/
    if ((bLsbPg            == TRUE32    ) &&
        (pstParam->nSData2 == 0xFFFFFFFF))
    {
        nSpareLpo = pstParam->nSData1;
    }
    else if ((bLsbPg            == FALSE32   ) &&
             (pstParam->nSData1 == 0xFFFFFFFF))
    {
        nSpareLpo = pstParam->nSData2;
    }
    else
    {
        nSpareLpo = 0xFFFFFFFF;
    }

    nLpo = (UINT16)(nSpareLpo >> 16);
    if ((nLpo ^ 0xFFFF) == (UINT16)(nSpareLpo & 0x0000FFFF))
    {
        (*pnLpo)   = nLpo;
        bCorrupted = FALSE32;
    }
    else
    {
        (*pnLpo)   = NULL_POFFSET;
        bCorrupted = TRUE32;
    }

    /* get the PTF information */
    nPTF = (UINT16)(pstParam->nSData3 >> 16);
    while (bCorrupted == FALSE32)
    {
        bCorrupted = TRUE32;

        if ((nPTF ^ 0xFFFF) != (UINT16)(pstParam->nSData3 & 0x0000FFFF))
        {
            break;      /* Spare broken         */
        }
        if ((nPTF & (0x01 << 4)) == 0)
        {
            break;      /* This is not log unit */
        }
        if (( bLsbPg         == TRUE32 ) &&
            ((nPTF & 0xFC00) != 0))
        {
            break;      /* This is not LSB page */
        }
        if (( bLsbPg         == FALSE32) &&
            ((nPTF & 0xF300) != 0))
        {
            break;      /* This is not MSB page */
        }

        bCorrupted = FALSE32;
        break;
    }

    if ((bCorrupted == FALSE32) &&
         (((nPTF & (0x01 <<  9)) != 0) ||
          ((nPTF & (0x01 << 11)) != 0)))
    {
        /* identify transaction confirmation */
        (*pbConfirm) = TRUE32;

        /* check the CRC info */
        for (nIdx = 0; nIdx < pstDVI->nSecPerVPg; nIdx = nIdx + nSectorsPerCRC)
        {
            if (pstDVI->nDeviceType & RBW_DEVTYPE_ONENAND)
            {
                nSpareIdx = GET_ONENAND_SPARE_IDX(nIdx);
            }
            else
            {
                nSpareIdx = nIdx;
            }

            nCRC32Val = FSR_STL_CalcCRC32(pstParam->pData + (nIdx << BYTES_SECTOR_SHIFT), nSizePerCRC);
            if (nCRC32Val != pstParam->pExtParam->aExtSData[nSpareIdx])
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG,
                    (TEXT("[SIF:INF]  %s() L(%d) : nLpo(%d), aExtSData[%d](%x)\r\n"),                        
                        __FSR_FUNC__, __LINE__, nLpo, nSpareIdx,pstParam->pExtParam->aExtSData[nSpareIdx]));
                bCorrupted = TRUE32;
                break;
            }
        }
    }

    if (bCorrupted == TRUE32)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:INF]  %s() L(%d) : the page[%3d] is corrupted - S[0x%08x-0x%08x-0x%08x]\r\n"),
                __FSR_FUNC__, __LINE__, nLpo, pstParam->nSData1, pstParam->nSData2, pstParam->nSData3));
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%01x\r\n"),
            __FSR_FUNC__, __LINE__, bCorrupted));
    return bCorrupted;
}

/**
 * @brief           This function force to corrupt page,
 * @n               when the page is read in the future,
 * @n               in order to occur uncorrectable ECC error.
 *
 * @param[in]       pstZone         : pointer to STL Zone object
 * @param[in]       pstParam        : VFL param for spare data
 * @param[out]      pnLpo           : Logical Page Offset (LPO)
 * @param[out]      pbConfirm       : Whether confirmation page is
 *
 * @return          Whether the spare data is corrupted or not
 *
 * @author          Jongtae Park
 * @version         1.2.0
 */
PRIVATE INT32
_ForceToCorruptPage    (STLZoneObj     *pstZone,
                        VFLParam       *pstParam,
                        PADDR           nDstVpn)
{
    const RBWDevInfo   *pstDev          = pstZone->pstDevInfo;
    STLClstObj         *pstClst         = FSR_STL_GetClstObj(pstZone->nClstID);
    const UINT32        n1stVun         = pstClst->pstEnvVar->nStartVbn;
    BMLCpBkArg        **ppstBMLCpBk     = pstClst->pstBMLCpBk;
    BMLCpBkArg         *pstCpBkArg      = pstClst->staBMLCpBk;
    BMLRndInArg        *pstRndIn        = pstClst->staBMLRndIn;
    FSRSpareBuf        *pstSBuf         = pstClst->staSBuf;
    const UINT32        nWay            = nDstVpn & (pstDev->nNumWays - 1);
    const UINT32        nDstVbn         = nDstVpn >> pstDev->nPagesPerSbShift;
    const UINT32        nDstOff         = nDstVpn & (pstDev->nPagesPerSBlk - 1);
    INT32               nRet            = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    if (pstParam->nSData3 != 0)
    {
#if defined (FSR_ONENAND_EMULATOR)
        FSR_FOE_EnableOverwrite(pstZone->nVolID);
#endif

        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:INF] VPN[%5d] is corrupted purposely\r\n"), nDstVpn));

        pstSBuf[nWay].pstSpareBufBase->nSTLMetaBase0 = pstParam->nSData1;
        pstSBuf[nWay].pstSpareBufBase->nSTLMetaBase1 = pstParam->nSData2;
        pstSBuf[nWay].pstSpareBufBase->nSTLMetaBase2 = 0;

        pstRndIn[nWay].nNumOfBytes      = (UINT16)(FSR_SPARE_BUF_STL_BASE_SIZE);
        pstRndIn[nWay].nOffset          = (UINT16)(FSR_STL_CPBK_SPARE_BASE);
        pstRndIn[nWay].pBuf             = &(pstSBuf[nWay].pstSpareBufBase->nSTLMetaBase0);

        pstCpBkArg[nWay].nSrcVun        = (UINT16)(n1stVun);
        pstCpBkArg[nWay].nSrcPgOffset   = (UINT16)(pstDev->nPagesPerSBlk + nWay - pstDev->nNumWays);
        pstCpBkArg[nWay].nDstVun        = (UINT16)(nDstVbn + n1stVun);
        pstCpBkArg[nWay].nDstPgOffset   = (UINT16)(nDstOff);
        pstCpBkArg[nWay].nRndInCnt      = 1;
        pstCpBkArg[nWay].pstRndInArg    = (BMLRndInArg*)&(pstRndIn[nWay]);

        FSR_OAM_MEMSET(ppstBMLCpBk, 0x00, sizeof(long) * FSR_MAX_WAYS);
        ppstBMLCpBk[nWay] = &(pstCpBkArg[nWay]);

        nRet = FSR_BML_CopyBack(pstZone->nVolID, ppstBMLCpBk, FSR_BML_FLAG_NONE);
        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] %s(), L(%d) - FSR_BML_CopyBack(nSrcVun=%d nSrcPgOffset=%d nDstVun=%d nSrcPgOffset=%d) (0x%x)\r\n"), 
                __FSR_FUNC__, __LINE__, pstCpBkArg[nWay].nSrcVun, pstCpBkArg[nWay].nSrcPgOffset, pstCpBkArg[nWay].nDstVun, pstCpBkArg[nWay].nDstPgOffset, nRet));            
            FSR_ASSERT(nRet != FSR_BML_VOLUME_NOT_OPENED);
            FSR_ASSERT(nRet != FSR_BML_INVALID_PARAM);
            FSR_ASSERT(nRet != FSR_BML_WR_PROTECT_ERROR);

            FSR_STL_LockPartition (pstZone->nClstID);
            return nRet;
        }

#if defined (FSR_ONENAND_EMULATOR)
        FSR_FOE_DisableOverwrite(pstZone->nVolID);
#endif
        nRet = FSR_STL_SUCCESS;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%01x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}


/**
 * @brief           This function reads pages in the log block to construct
 * @n               active log page mapping table at STL open time.
 *                  If there is a corrupted page in the log block, 
 *                  log block should be replaced with a free block and 
 *                  page mapping should be done again.
 *                  _MakeActivePMT 
 *                  1. Checks if there is a corrupted page,
 *                  2. recovers until the latest CRC confirm page
 *                  3. Buffer unit informatino update.
 *                     (If the log block has a page which page is same as a buffer unit,
 *                      it sets null to the buffer unit so that buffer unit flush should be avoided.
 *                  4. Update page mapping information.
 *                  
 *
 * @param[in]       pstZone          : pointer to atl Zone object
 * @param[in]       pLogGrp          : pointer to log group
 * @param[in]       pLogObj          : pointer to log object
 * @param[in]       LogIdx           : log index
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 */
PRIVATE INT32
_MakeActivePMT (STLZoneObj     *pstZone,
                STLLogGrpHdl   *pLogGrp,
                STLLog         *pLogObj,
                UINT32          LogIdx)
{
    const   RBWDevInfo     *pstDVI          = pstZone->pstDevInfo;
    const   STLRootInfo    *pstRI           = pstZone->pstRI;
            STLPartObj     *pstSTLPartObj;
            STLPartObj     *pstTmpSTLPart;
            UINT32          nPgsPerSBlk     = pstDVI->nPagesPerSBlk;
    const   POFFSET         nSrtOffs        = pLogObj->nCPOffs;
            POFFSET         nCheckOff;
            POFFSET         nLastPgIdx;
            UINT32          nTmpOpenFlag=0; /* Used to keep the original open flag */    
            POFFSET         nPrevConfirm;
            UINT32          nPgIdx;
            UINT32          nCleanCnt;
            PADDR           nVpn;
            POFFSET         nLpo;
            BOOL32          bConfirm;
            BOOL32          bCorrupted;
            BOOL32          bLsbPg;
            VFLParam       *pstParam;
            VFLParam       *pstParam1;
            VFLParam       *pstParam2;
            POFFSET         nPairedLSBIdx;
            BOOL32          bReliableRead   = FALSE32;
            BOOL32          bEnableRTRR;    /* Run-time Read Refresh */
            BOOL32          bCleanPage;
            STLTmpVPmt     *pstVPmt         = (STLTmpVPmt *)(FSR_STL_GetClstObj(pstZone->nClstID)->pTempPgBuf);
            INT32           nRet            = FSR_STL_SUCCESS;
#if 0
            UINT32          nPairedLSBVpn;
#endif
#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    STLBUCtxObj    *pstBUObj        = pstZone->pstBUCtxObj;
#endif
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    if (nSrtOffs >= pstDVI->nPagesPerSBlk)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }

    if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
    {
        nPgsPerSBlk >>= 1;
        /* In case of MLC, we use R1-recovery routine to rewrite pages which
           contain FSR_BML_READ_DISTURBANCE_ERROR by sudden power-loss.
           Because, we cannot use run-time read refresh feature of BML,
           in order to protect LSB pages corrupted by sudden power-loss
           during programming paired MSB pages, which may be used to recovery.
           */
        bEnableRTRR   = FALSE32;
    }
    else
    {
        /* In case of SLC, we use run-time read refresh feature of BML
           to rewrite pages which contain FSR_BML_READ_DISTURBANCE_ERROR
           by sudden power-loss */
        bEnableRTRR   = TRUE32;
    }

    /* To scan pages */
    pstParam1              = FSR_STL_AllocVFLParam(pstZone);
    /* need to buffer for reading main data */
    pstParam1->pData       = (UINT8*)(pstZone->pTempMetaPgBuf);
    pstParam1->nBitmap     = pstDVI->nFullSBitmapPerVPg;
    pstParam1->bPgSizeBuf  = TRUE32;
    pstParam1->bUserData   = FALSE32;
    pstParam1->bSpare      = TRUE32;
    pstParam1->nNumOfPgs   = 1;
    pstParam1->pExtParam   = FSR_STL_AllocVFLExtParam(pstZone);
    pstParam1->pExtParam->nNumExtSData = (UINT16)pstDVI->nSecPerVPg;

    /* For paired page */
    pstParam2              = FSR_STL_AllocVFLParam(pstZone);
    /* need to buffer for reading main data */
    pstParam2->pData       = (UINT8*)(pstZone->pTempMetaPgBuf);
    pstParam2->nBitmap     = pstDVI->nFullSBitmapPerVPg;
    pstParam2->bPgSizeBuf  = TRUE32;
    pstParam2->bUserData   = FALSE32;
    pstParam2->bSpare      = TRUE32;
    pstParam2->nNumOfPgs   = 1;
    pstParam2->pExtParam   = FSR_STL_AllocVFLExtParam(pstZone);
    pstParam2->pExtParam->nNumExtSData = (UINT16)pstDVI->nSecPerVPg;

    /*
     * 1. Initialize Virtual Page Map Table
     */
    /* We use pTempPgBuf in STL cluster object to store STLTmpVpmt temporally */
    FSR_ASSERT((sizeof (STLTmpVPmt) << pstDVI->nPagesPerSbShift)
                <= pstDVI->nBytesPerVPg);

    /* Initialize Temporal Virtual Page Map Table */
    FSR_ASSERT(nSrtOffs < pstDVI->nPagesPerSBlk);
    if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
    {
        for (nPgIdx = nSrtOffs; nPgIdx < pstDVI->nPagesPerSBlk; nPgIdx++)
        {
            bLsbPg = FSR_STL_FlashIsLSBPage(pstZone, nPgIdx);
            if (bLsbPg == FALSE32)
            {
                break;
            }
        }
        nCheckOff = (POFFSET)FSR_BML_GetPairedVPgOff(pstZone->nVolID, nPgIdx);
    }
    else
    {
        nCheckOff = nSrtOffs;
    }

    /*
     * 2. scan active log for checking status of each virtual page
     */
    pLogObj->nCPOffs = (POFFSET)(pstDVI->nPagesPerSBlk);
    nLastPgIdx       = NULL_POFFSET;
    nCleanCnt        = 0;
    for (nPgIdx = nCheckOff; nPgIdx < pstDVI->nPagesPerSBlk; nPgIdx++)
    {
        /* Initialize the state of Virtual Page of nPgIdx */
        pstVPmt[nPgIdx].bClean      = FALSE32;
        pstVPmt[nPgIdx].bConfirm    = FALSE32;
        pstVPmt[nPgIdx].nCorrupted  = PAGE_CORRUPTED_NONE;
        pstVPmt[nPgIdx].nLpn        = NULL_VPN;

        nVpn = (pLogObj->nVbn << pstDVI->nPagesPerSbShift) + nPgIdx;
        bLsbPg = FSR_STL_FlashIsLSBPage(pstZone, nPgIdx);

        pstParam = pstParam1;
        nRet = FSR_STL_FlashCheckRead(pstZone, nVpn, pstParam, 1, bEnableRTRR);
        if (nRet == FSR_STL_CLEAN_PAGE)
        {
            pstVPmt[nPgIdx].bClean = TRUE32;

            /* if WearLevel triggers, there is many clean pages in PMT */
            if (nPgIdx < nSrtOffs)
            {
                continue;
            }

            /* stop at clean page position */
            nCleanCnt++;

            if (nCleanCnt > pstDVI->nNumWays)
            {
                break;
            }

            continue;
        }
        else if (nRet == FSR_BML_READ_DISTURBANCE_ERROR)
        {
            pstVPmt[nPgIdx].nCorrupted = PAGE_READ_DISTURBANCE;
        }
        else if (nRet == FSR_BML_READ_ERROR)
        {
            pstVPmt[nPgIdx].nCorrupted = PAGE_CORRUPTED_ALONE;
            if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
            {
                if (bLsbPg == TRUE32)
                {
                    /* Check whether the page is corrupted by paired MSB page or not */
                    nRet = FSR_STL_FlashReliableRead(pstZone, nVpn, pstParam2);
                    if (nRet == FSR_BML_SUCCESS)
                    {
                        pstParam = pstParam2;
                        pstVPmt[nPgIdx].nCorrupted = PAGE_CORRUPTED_PAIRED;

                        /*
                        if the recovered page is a clean page, page validation checking should not be proceeded.
                        So, below checks whether the page is a clean page, and if so then execute the continue statement.
                        */
                        bCleanPage = FSR_STL_FlashIsCleanPage(pstZone,pstParam);
                        if (bCleanPage == TRUE32)
                        {
                            pstVPmt[nPgIdx].bClean = TRUE32;
                            continue;
                        }
                    }
                }
            }
            else
            {
                FSR_ASSERT(bEnableRTRR == TRUE32);
                _ForceToCorruptPage(pstZone, pstParam, nVpn);
            }
        }
        else if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR]  --%s() L(%d) : 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        bCorrupted = _CheckPageValidity(pstZone, pstParam, bLsbPg, &nLpo, &bConfirm);
        pstVPmt[nPgIdx].bConfirm = (UINT8)bConfirm;
        pstVPmt[nPgIdx].nLpn     = ((pLogGrp->pstFm->nDgn << pstRI->nNShift) << pstDVI->nPagesPerSbShift) + nLpo;
        if (bCorrupted == TRUE32)
        {
            /*
            if the corrupted page is a LSB page, and the main data is not a style of log block,
            it is meaningless to recover the page, so set the PAGE_CORRUPTED_ALONE 
            by executing 'pstVPmt[nPgIdx].nCorrupted = PAGE_CORRUPTED_ALONE;'
            not by executing 'pstVPmt[nPgIdx].nCorrupted = PAGE_CORRUPTED_ALONE;'
            This is for removing PAGE_CORRUPTED_PAIRED'
            */
            pstVPmt[nPgIdx].nCorrupted = PAGE_CORRUPTED_ALONE;
        }

        if ((pstVPmt[nPgIdx].nCorrupted & PAGE_CORRUPTED_MASK) != PAGE_CORRUPTED_NONE)
        {
            pLogObj->nState |= LS_RANDOM;

#if 1
            if (bLsbPg == FALSE32)
            {
                /* Check whether the paired page is corrupted by this page or not */
                nPairedLSBIdx = (POFFSET)(FSR_BML_GetPairedVPgOff(pstZone->nVolID, nPgIdx));
                FSR_ASSERT((nPairedLSBIdx >= nCheckOff) &&
                           (nPairedLSBIdx <  nPgIdx   ));
                /* 
                The error bit of pstVPmt[nPairedLSBIdx].nCorrupted should be either  
                PAGE_CORRUPTED_ALONE  or PAGE_CORRUPTED_PAIRED.
                */
                if ((pstVPmt[nPairedLSBIdx].nCorrupted & PAGE_CORRUPTED_ALONE) == 0)
                {
                    pstVPmt[nPairedLSBIdx].nCorrupted |= PAGE_CORRUPTED_PAIRED;
                }
            }
#else
            if (bLsbPg == FALSE32)
            {
                /* Check whether the paired page is corrupted by this page or not */
                nPairedLSBIdx = (POFFSET)(FSR_BML_GetPairedVPgOff(pstZone->nVolID, nPgIdx));
                FSR_ASSERT((nPairedLSBIdx >= nCheckOff) &&
                           (nPairedLSBIdx <  nPgIdx   ));
                if (nPairedLSBIdx < nSrtOffs)
                {
                    nPairedLSBVpn = (pLogObj->nVbn << pstDVI->nPagesPerSbShift) + nPairedLSBIdx;

                    nRet = FSR_STL_FlashCheckRead(pstZone, nPairedLSBVpn, pstParam2, FALSE32);
                    if (nRet == FSR_BML_READ_ERROR)
                    {
                        pstVPmt[nPairedLSBIdx].nCorrupted |= PAGE_CORRUPTED_PAIRED;
                    }
                    else if ((nRet == FSR_BML_SUCCESS) ||
                             (nRet == FSR_BML_READ_DISTURBANCE_ERROR))
                    {
                        // FIXME : LPO
                        bCorrupted = _CheckPageValidity(pstZone, pstParam2, TRUE32, &nLpo, &bConfirm);
                        pstVPmt[nPairedLSBIdx].bConfirm = (UINT8)bConfirm;
                        pstVPmt[nPairedLSBIdx].nLpn     = ((pLogGrp->pstFm->nDgn << pstRI->nNShift) << pstDVI->nPagesPerSbShift) + nLpo;
                        if (bCorrupted == TRUE32)
                        {
                            pstVPmt[nPairedLSBIdx].nCorrupted |= PAGE_CORRUPTED_PAIRED;
                        }
                        if (nRet == FSR_BML_READ_DISTURBANCE_ERROR)
                        {
                            pstVPmt[nPairedLSBIdx].nCorrupted |= PAGE_READ_DISTURBANCE;
                        }
                    }
                    else if (nRet != FSR_STL_CLEAN_PAGE)
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                            (TEXT("[SIF:ERR]  --%s() L(%d) : 0x%08x\r\n"),
                                __FSR_FUNC__, __LINE__, nRet));
                        break;
                    }
                }
            }
#endif
        }

        if ((pstVPmt[nPgIdx].nCorrupted & PAGE_CORRUPTED_ALONE) == 0)
        {
            nLastPgIdx = (POFFSET)nPgIdx;
        }

        /**
         * if clean page skip is found, log state becomes to be LS_RANDOM.
         * this case happens in MLC LSB only mode.
         */
        if (nCleanCnt > 0)
        {
            pLogObj->nState |= LS_RANDOM;
        }

        /**
         * reset nCleanCnt which represent the number of clean pages 
         * from last programmed page
         */
        nCleanCnt = 0;
        nRet      = FSR_STL_SUCCESS;
    }

    /* Check whether there is error or not */
    if ((nRet != FSR_STL_SUCCESS   ) &&
        (nRet != FSR_STL_CLEAN_PAGE))
    {
        FSR_STL_FreeVFLExtParam(pstZone, pstParam2->pExtParam);
        FSR_STL_FreeVFLParam(pstZone, pstParam2);

        FSR_STL_FreeVFLExtParam(pstZone, pstParam1->pExtParam);
        FSR_STL_FreeVFLParam(pstZone, pstParam1);

        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
            (TEXT("[SIF:INF]   nDgn(%5d) nVbn(%5d) S(%3d)-E(%3d) nCPOffs(%3d)\r\n"),
                pLogGrp->pstFm->nDgn, pLogObj->nVbn, nCheckOff, nPgIdx, pLogObj->nCPOffs));

        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));

        FSR_STL_LockPartition(pstZone->nClstID);
        return nRet;
    }

    if (nPgIdx == pstDVI->nPagesPerSBlk)
    {
        nCleanCnt++;
    }
    pLogObj->nCPOffs  = (UINT16)(nPgIdx - (nCleanCnt - 1));

    nRet = FSR_STL_SUCCESS;

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    /* identify that last Lpn is existed in BU */
    if (nLastPgIdx != NULL_POFFSET)
    {
        if (((pstVPmt[nLastPgIdx].nLpn + 1) == pstBUObj->nBufferedLpn) &&
            ((pLogObj->nCPOffs - 1) == nLastPgIdx))
        {
            pstVPmt[nLastPgIdx].bConfirm = TRUE32;
        }

        if (pstVPmt[nLastPgIdx].nLpn == pstBUObj->nBufferedLpn)
        {
            pstBUObj->nBBlkCPOffset     = (UINT16)nPgsPerSBlk;    /* 1 plus index of the last page of the BU block */
            pstBUObj->nBufferedLpn      = NULL_VPN;
            pstBUObj->nBufferedDgn      = NULL_DGN;
            pstBUObj->nBufferedVpn      = NULL_VPN;
            pstBUObj->nBufState         = STL_BUFSTATE_CLEAN;
            pstBUObj->nValidSBitmap     = 0x00;
        }
    }
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */

    /*
     * 3. Find the latest valid confirmation page and
     *    the 2nd valid confirmation pages to prepare roll-back
     *    when the latest page may be corrupted by read-disturbance error.
     */
    nPgIdx     = nLastPgIdx;
    nLastPgIdx = NULL_POFFSET;
    nPrevConfirm = NULL_POFFSET;
    while ((nPgIdx >= nSrtOffs) &&
           (nPgIdx <  pstDVI->nPagesPerSBlk))
    {
        if ((pstVPmt[nPgIdx].bConfirm == TRUE32) &&
            ((pstVPmt[nPgIdx].nCorrupted & PAGE_CORRUPTED_ALONE) == 0))
        {
            FSR_ASSERT(pstVPmt[nPgIdx].bClean == FALSE32);

            /* Check whether there is corrupted pages in a transaction or not.*/
            nPrevConfirm = (POFFSET)nPgIdx;
            nPgIdx--;
            while ((nPgIdx >= nSrtOffs) &&
                   (nPgIdx <  pstDVI->nPagesPerSBlk))
            {
                if ((pstVPmt[nPgIdx].nCorrupted & PAGE_CORRUPTED_ALONE) != 0)
                {
                    nPrevConfirm = NULL_POFFSET;
                    break;
                }
                if (pstVPmt[nPgIdx].bConfirm == TRUE32)
                {
                    break;
                }
                nPgIdx--;
            }

            if (nPrevConfirm != NULL_POFFSET)
            {
                if (nLastPgIdx == NULL_POFFSET)
                {
                    /* Find the latest valid confirmation page */
                    nLastPgIdx   = nPrevConfirm;
                    nPrevConfirm = NULL_POFFSET;
                }
                else
                {
                    /* Find the 2nt valid confirmation page */
                    break;
                }
            }
        }
        nPgIdx--;
    }

    if (nSrtOffs > 0)
    {
        if (nLastPgIdx == NULL_POFFSET)
        {
            FSR_ASSERT(nPrevConfirm == NULL_POFFSET);
            nLastPgIdx = nSrtOffs - 1;
            nPrevConfirm = nLastPgIdx;
        }
        else if (nPrevConfirm == NULL_POFFSET)
        {
            FSR_ASSERT((nLastPgIdx >= nSrtOffs        ) &&
                       (nLastPgIdx <  pLogObj->nCPOffs));
            nPrevConfirm = nSrtOffs - 1;
        }
        else
        {
            FSR_ASSERT((nLastPgIdx   >  nPrevConfirm    ) &&
                       (nLastPgIdx   <  pLogObj->nCPOffs));
            FSR_ASSERT((nPrevConfirm >= nSrtOffs        ) &&
                       (nPrevConfirm <  nLastPgIdx      ));
        }
    }
#if (OP_STL_DEBUG_CODE == 1)
    else
    {
        if (nLastPgIdx != NULL_POFFSET)
        {
            FSR_ASSERT((nLastPgIdx >= nSrtOffs        ) &&
                       (nLastPgIdx <  pLogObj->nCPOffs));
            if (nPrevConfirm != NULL_POFFSET)
            {
                FSR_ASSERT((nLastPgIdx   >  nPrevConfirm));

                FSR_ASSERT((nPrevConfirm >= nSrtOffs    ) &&
                           (nPrevConfirm <  nLastPgIdx  ));
            }
        }
        else
        {
            FSR_ASSERT(nPrevConfirm == NULL_POFFSET);
        }
    }
#endif

    /* If there are pages which should be updated to PMT */
    if (nLastPgIdx != NULL_POFFSET)
    {
        /*
         * 4. Check whether the recovery is needed by paired page corruption.
         */
        bReliableRead = FALSE32;
        nPairedLSBIdx = NULL_POFFSET;
        if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
        {
            for (nPgIdx = nCheckOff; nPgIdx <= nLastPgIdx; nPgIdx++)
            {
                if ((pstVPmt[nPgIdx].nCorrupted & PAGE_CORRUPTED_PAIRED) != 0)
                {
                    FSR_ASSERT(nPairedLSBIdx == NULL_POFFSET);
                    FSR_ASSERT(FSR_STL_FlashIsLSBPage(pstZone, nPgIdx) == TRUE32);
                    FSR_ASSERT((pstVPmt[nPgIdx].nCorrupted & PAGE_CORRUPTED_ALONE) == 0);
                    bReliableRead = TRUE32;
                    nPairedLSBIdx = (POFFSET)nPgIdx;
                    break;
                }
                else if ((pstVPmt[nPgIdx].nCorrupted & PAGE_READ_DISTURBANCE) != 0)
                {
                    bReliableRead = TRUE32;
                }
#if (OP_STL_DEBUG_CODE == 1)
                else
                {
                    FSR_ASSERT(pstVPmt[nPgIdx].nCorrupted == PAGE_CORRUPTED_NONE);
                }
#endif
            }
        }

        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
            (TEXT("[SIF:INF]   nDgn(%5d) nVbn(%5d) S(%3d)-E(%3d) nCPOffs(%3d)\r\n"),
                pLogGrp->pstFm->nDgn, pLogObj->nVbn, nCheckOff, nLastPgIdx, pLogObj->nCPOffs));

        /*
         * 5. Update Page Map Table (PMT) of active log.
         */
        if (bReliableRead == TRUE32)
        {
            /*1. Get Partition object*/
            pstSTLPartObj   = &(gstSTLPartObj[pstZone->nVolID][pstZone->nPart]);
            if (pstSTLPartObj->nZoneID == (UINT32)(-1))
            {
                FSR_ASSERT(pstSTLPartObj->nClstID == (UINT32)(-1));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                    (TEXT("[SIF:ERR] Invalid Partition ID (PartID %d)\r\n"), pstZone->nPart + FSR_PARTID_STL0));
            }

            /*1. keep the original state*/
            pstTmpSTLPart   = pstSTLPartObj->pst1stPart;
            nTmpOpenFlag  = pstTmpSTLPart->nOpenFlag;

            /* To make the partition writable, change the partition as BML RW */
            if ((pstTmpSTLPart->nOpenFlag & FSR_STL_FLAG_RW_PARTITION) == 0)
            {
                nRet = FSR_STL_ChangePartAttr(pstZone->nVolID,pstZone->nPart + FSR_PARTID_STL0,FSR_STL_FLAG_OPEN_READWRITE);
                if (nRet != FSR_STL_SUCCESS)
                {
                    /* don't execute break. */
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
                                   (TEXT("[SIF:INF] --%s() L(%d): Unable to change the Partition attribute\r\n"),
                                   __FSR_FUNC__, __LINE__, nRet));
                }
            }

            nRet = _FONANDReliableWrite(pstZone,
                                        pLogGrp,
                                        pLogObj,
                                        LogIdx,
                                        nSrtOffs,
                                        nLastPgIdx,
                                        nPrevConfirm);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  --%s() L(%d) : 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
            }

            /* recover to the state of original partition attribute */
            if ((nTmpOpenFlag & FSR_STL_FLAG_RO_PARTITION) != 0)
            {
                nRet = FSR_STL_ChangePartAttr(pstZone->nVolID,pstZone->nPart + FSR_PARTID_STL0,FSR_STL_FLAG_OPEN_READONLY);
                if (nRet != FSR_STL_SUCCESS)
                {
                    /* don't execute break. */
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
                                   (TEXT("[SIF:INF] --%s() L(%d): Unable to change the Partition attribute\r\n"),
                                   __FSR_FUNC__, __LINE__, nRet));
                }
                else
                {
                    pstTmpSTLPart->nOpenFlag = nTmpOpenFlag;
                }                
            }
        }
        else
        {
            for (nPgIdx = nSrtOffs; nPgIdx <= nLastPgIdx; nPgIdx++)
            {
                if (pstVPmt[nPgIdx].nLpn != NULL_VPN)
                {
                    FSR_ASSERT(pstVPmt[nPgIdx].bClean     == FALSE32);
                    if ((pstVPmt[nPgIdx].nCorrupted & PAGE_CORRUPTED_MASK) != PAGE_CORRUPTED_NONE)
                    {
                        /* process to return READ Only partition */
                        nRet = FSR_STL_LockPartition(pstZone->nClstID);
                        if (nRet != FSR_STL_SUCCESS)
                        {
                            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                                (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                                    __FSR_FUNC__, __LINE__, nRet));
                            break;
                        }
                    }

                    /* Update Page Mapping Table */
                    nVpn = (pLogObj->nVbn << pstDVI->nPagesPerSbShift) + nPgIdx;
                    FSR_STL_UpdatePMT(pstZone, pLogGrp, pLogObj,
                                      pstVPmt[nPgIdx].nLpn, nVpn);

                    /* change log state */
                    FSR_STL_ChangeLogState(pstZone, pLogGrp, pLogObj,
                                           pstVPmt[nPgIdx].nLpn);
                }
#if (OP_STL_DEBUG_CODE == 1)
                else
                {
                    FSR_ASSERT(pstVPmt[nPgIdx].bClean     == TRUE32);
                    FSR_ASSERT(pstVPmt[nPgIdx].bConfirm   == FALSE32);
                    FSR_ASSERT(pstVPmt[nPgIdx].nCorrupted == PAGE_CORRUPTED_NONE)
                }
#endif
            }
            FSR_STL_UpdatePMTCost(pstZone, pLogGrp);
            nRet = FSR_STL_SUCCESS;
        }
    }

    /* 
    * If bReliableRead is set as TRUE, it means new block is allocated.
    * So, it is not needed to set CPOffs as the last page of the block
    * for the software programming issue
    */
    if (bReliableRead == FALSE32)
    {
        if (pLogObj->nCPOffs < pstDVI->nPagesPerSBlk)
        {
            pLogObj->nCPOffs = (POFFSET)(pstDVI->nPagesPerSBlk);
            pLogObj->nState |= LS_RANDOM;
        }
    }

    FSR_STL_FreeVFLExtParam(pstZone, pstParam2->pExtParam);
    FSR_STL_FreeVFLParam(pstZone, pstParam2);

    FSR_STL_FreeVFLExtParam(pstZone, pstParam1->pExtParam);
    FSR_STL_FreeVFLParam(pstZone, pstParam1);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}


/**
 * @brief           This function builds the PMT table for a active log
 * @n               that the log index is the same as the nIdx at STL open time.
 *
 * @param[in]       pstZone         : pointer to atl Zone object
 * @param[in]       pActLog         : pointer to a Active Log
 * @param[in]       nIdx            : log index
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PRIVATE INT32
_BuildPMTforActiveLog  (STLZoneObj        *pstZone,
                                STLActLogEntry    *pActLog,
                                UINT32             nIdx)
{
#if (OP_SUPPORT_RUNTIME_PMT_BUILDING == 0)
    BADDR           nVbn;
    BOOL32          bFoundRet;
    STLLog         *pLogObj;
#endif /*OP_SUPPORT_RUNTIME_PMT_BUILDING*/
    POFFSET         nMetaPOffs;
    STLLogGrpHdl   *pstLogGrp          = NULL;
    BADDR           nDgn;
    INT32           nRet                = FSR_STL_CRITICAL_ERROR;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    nDgn  = pActLog->nDgn;
    pstLogGrp = FSR_STL_SearchLogGrp(pstZone->pstActLogGrpList, nDgn);
    if (pstLogGrp == NULL)
    {
        /* (0) pstLogGrp Memory Allocation */
        pstLogGrp = FSR_STL_AllocNewLogGrp(pstZone, pstZone->pstActLogGrpList);
        if (pstLogGrp == NULL)
        {
            /* There is no available log group in the pool */
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] FSR_STL_AllocNewLogGrp() Error : there is no available log group in the pool\r\n")));
            return FSR_STL_OUT_OF_MEMORY;
        }

        /* Search STLLogGrpHdl in PMTDir */
        nRet = FSR_STL_SearchPMTDir(pstZone, nDgn, &nMetaPOffs);
        if (nMetaPOffs != NULL_POFFSET)
        {
            /* (2) load PMT */
            nRet = FSR_STL_LoadPMT(pstZone, nDgn, nMetaPOffs, &pstLogGrp, TRUE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                    (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
                return nRet;
            }
        }
        else
        {
            FSR_ASSERT(FALSE32);
        }

        /* 
        * (3) assign new STLLogGrpHdl in LogGrpHdl Pool(RAM) 
        */
        FSR_STL_AddLogGrp(pstZone->pstActLogGrpList, pstLogGrp);
    }

#if (OP_SUPPORT_RUNTIME_PMT_BUILDING == 0)
    /* Search log in STLLogGrpHdl */
    nVbn      = pActLog->nVbn;
    bFoundRet = FSR_STL_SearchLog(pstLogGrp, nVbn, &pLogObj);
    if (bFoundRet != TRUE32)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, FSR_STL_ERR_NO_LOG));
        return FSR_STL_ERR_NO_LOG;
    }
    FSR_ASSERT(bFoundRet == TRUE32);
    FSR_ASSERT((pLogObj->nState & (1 << LS_ACTIVE_SHIFT)));

    if (pLogObj->nCPOffs < pstZone->pstDevInfo->nPagesPerSBlk)
    {
        /** 
         * Update PMT & log state & CPO from scanning log block 
         * log block scan & construct log page mapping table of each log 
         */
        nRet = _MakeActivePMT(pstZone, pstLogGrp, pLogObj, nIdx);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return nRet;
        }
    }
#endif /*OP_SUPPORT_RUNTIME_PMT_BUILDING*/
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}

/**
 * @brief           This function rebuilds the PMTs for the log group related with active logs
 *
 * @param[in]       pstZone         : zone object
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PRIVATE INT32   
_RebuildLogPageMap(STLZoneObj* pstZone)
{
            STLCtxInfoHdl  *pstCI       = pstZone->pstCtxHdl;
            UINT32          nIdx;
            STLActLogEntry *pActLog;
            INT32           nRet        = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    for (nIdx = 0; nIdx < pstCI->pstFm->nNumLBlks; nIdx++)
    {
        pActLog = &(pstCI->pstActLogList[nIdx]);
        nRet = _BuildPMTforActiveLog(pstZone, pActLog, nIdx);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - _BuildPMTforActiveLog returns error\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}


#if (OP_SUPPORT_RUNTIME_PMT_BUILDING == 1)
/**
* @brief           This function sets initial Active Log Block infos. The information is used 
*                  to decide whether the log scanning is needed or not when Read/Write/Delete APIs are called.
*                  It also used to decide whether to do the scanning when the active log group is 
*                  moved from active group to inactive log group.
*
* @param[in]       pstZone         : zone object
*
* @return          FSR_STL_SUCCESS
*
* @author          SangHoon Choi
* @version         1.1.0
*
*/
PRIVATE VOID   
_SetInitialActLogInfos(STLZoneObj* pstZone)
{
    STLCtxInfoHdl  *pstCI       = pstZone->pstCtxHdl;
    STLActLogEntry *pActLog;    
    UINT32          nIdx;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG,(TEXT("[SIF:INF] OP_SUPPORT_RUNTIME_PMT_BUILDING IS ENABLED\r\n")));

    /* Set Initial Active Log's info */
    for (nIdx = 0; nIdx < pstCI->pstFm->nNumLBlks; nIdx++)
    {        
        pActLog = &(pstCI->pstActLogList[nIdx]);
        pstZone->abInitActLogDgn[nIdx] = pActLog->nDgn;
    }

    /* Initialize default value for remaining slots */
    for (; nIdx < MAX_ACTIVE_LBLKS; nIdx++)
    {        
        /* NULL_DGN is used to inform that scanning for the abInitActLogDgn is not necessary*/
        pstZone->abInitActLogDgn[nIdx] = NULL_DGN;
    }

    /*When there are one or more active log blocks, checking for scanning is needed*/
    if (pstCI->pstFm->nNumLBlks != 0)
    {
        pstZone->abActLogScanCompleted = FALSE32;
    }
    else
    {
        pstZone->abActLogScanCompleted = TRUE32;
    }
    
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : \r\n"),
        __FSR_FUNC__, __LINE__)); 
}


/**
* @brief           This function scans the initial active log if the requested 
*                  Active Log Unit is not scanned yet.
*                  Every function which refers to the valid page count of Active Log grp
*                  Should firstly call FSR_STL_ScanActLogBlock, since before it was called
*                  the value of valid page count is not exactly correct.
*                  
*
* @param[in]       pstZone         : zone object
*                  nDgn            : 
*
* @return          FSR_STL_SUCCESS : Scanning is done successfully.
*                  FSR_STL_ERROR   : Error occus during the scanning.
*
* @author          SangHoon Choi
* @version         1.1.0
*
*/
PUBLIC INT32   
FSR_STL_ScanActLogBlock(STLZoneObj     *pstZone, BADDR nDgn)
{
    BADDR           nVbn;
    BOOL32          bFoundRet;
    STLLogGrpHdl   *pstLogGrp          = NULL;
    STLLog         *pLogObj;    
    UINT32          nIdx;
    UINT32          nIdxInActLogList   =0;
    STLActLogEntry *pActLog;
    INT32           nRet               = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    if ((pstZone->abActLogScanCompleted == TRUE32) || (nDgn == NULL_DGN))
    {
        return FSR_STL_SUCCESS;
    }

    /* Set Initial Active Log's info */
    for (nIdx = 0; nIdx < MAX_ACTIVE_LBLKS; nIdx++)
    {
        /* 
        If the abActLogScanCompleted[nIdx] is FALSE, 
        it means the Active Log Block of pstActLogList[nIdx]
        has the same nDGN value with abInitActLogDgn[nIdx]. 
        */
        if (pstZone->abInitActLogDgn[nIdx] == nDgn)
        {
            /*scan the initial active log block linked to the nDgn*/
            pActLog = _GetActLog(pstZone, nDgn,&nIdxInActLogList);
            FSR_ASSERT(pActLog != NULL);
            if (pActLog == NULL)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, FSR_STL_ERROR));
                return FSR_STL_ERROR;
            }

            /* if the ActLog is placed, then the log group related to the log should be placed */
            pstLogGrp = FSR_STL_SearchLogGrp(pstZone->pstActLogGrpList, nDgn);
            FSR_ASSERT(pstLogGrp != NULL);
            if (pstLogGrp == NULL)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, FSR_STL_ERROR));
                return FSR_STL_ERROR;
            }

            /* Search log in STLLogGrpHdl */
            nVbn      = pActLog->nVbn;
            bFoundRet = FSR_STL_SearchLog(pstLogGrp, nVbn, &pLogObj);
            FSR_ASSERT(bFoundRet == TRUE32);
            if (bFoundRet != TRUE32)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, FSR_STL_ERR_NO_LOG));
                return FSR_STL_ERROR;
            }

            if (pLogObj->nCPOffs < pstZone->pstDevInfo->nPagesPerSBlk)
            {
                /** 
                * Update PMT & log state & CPO from scanning log block 
                * log block scan & construct log page mapping table of each log 
                */
                nRet = _MakeActivePMT(pstZone, pstLogGrp, pLogObj, nIdxInActLogList);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                    return FSR_STL_ERROR;
                }
            }

            /* Inform that pstZone->abInitActLogDgn[nIdx] is scanned */
            pstZone->abInitActLogDgn[nIdx] = NULL_DGN;   

            /*Since the scanning is done here, subsequent checking is not necessary.*/
            break;
        } 
    }

    /* Set Initial Active Log's info */
    for (nIdx = 0; nIdx < MAX_ACTIVE_LBLKS; nIdx++)
    {
        /*
         * If the pstZone->abInitActLogDgn[nIdx] is not NULL_DGN, 
         * it means there are one or more initial active log blocks
         * In this case, checking for scanning is needed. 
         * If all values of pstZone->abInitActLogDgn[nIdx] are NULL_DGN, 
         * * pstZone->abActLogScanCompleted will be set as TRUE32
         * so that to avoid subsequent checking for scanning.
        */
        if (pstZone->abInitActLogDgn[nIdx] != NULL_DGN)
        {
            break;
        }
    }

    /* Since all the active log blocks are scanned, set abActLogScanCompleted to avoid subsequent checking. */
    if (nIdx == MAX_ACTIVE_LBLKS)
    {
        pstZone->abActLogScanCompleted = TRUE32;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : \r\n"),
        __FSR_FUNC__, __LINE__));
    return nRet;
}

/**
* @brief           This function returns active log Unit
*
* @param[in]       pstZone         : zone object
*                  nDgn            : Data Group Number which is used to find the active log block
*                  nIdxInActLogList : Index of the active log in ActLogList
*
* @return          Active Log Block
* @return          NULL
*
* @author          SangHoon Choi
* @version         1.1.0
*
*/
PRIVATE STLActLogEntry*   
_GetActLog(STLZoneObj     *pstZone,
           BADDR          nDgn,
           UINT32         *nIdxInActLogList)
{
    STLCtxInfoHdl  *pstCI       = pstZone->pstCtxHdl;
    UINT32          nIdx;
    STLActLogEntry *pActLog;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    for (nIdx = 0; nIdx < pstCI->pstFm->nNumLBlks; nIdx++)
    {        
        pActLog = &(pstCI->pstActLogList[nIdx]);
        if (pActLog->nDgn == nDgn)
        {
            *nIdxInActLogList = nIdx;
            return pActLog;
        }
    }
    return NULL;
}

#endif /*OP_SUPPORT_RUNTIME_PMT_BUILDING*/


/**
 * @brief           This function reads BMT context pages to construct full mapping table
 *
 * @param[in]       pstZone       : pointer to atl Zone object
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PRIVATE INT32
_LoadFullBMT(STLZoneObj *pstZone)
{
    STLZoneInfo    *pstZI           = NULL;
    RBWDevInfo     *pstDVI          = NULL;
    STLDirHdrHdl   *pstDH           = NULL;
    UINT8          *pMPgBF          = NULL;
    STLBMTHdl      *pstBMT          = NULL;
    STLMetaLayout  *pstML           = NULL;
    UINT32          nPagesPerSBlk;
    PADDR           nVpn            = NULL_VPN;
    UINT32          nIdx;
    INT32           nRet;
    VFLParam       *pstVFLParam     = NULL;
    POFFSET         nBOffs;
    UINT32          nZBCBMT;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    pstDVI          = pstZone->pstDevInfo;
    pstDH           = pstZone->pstDirHdrHdl;
    pstZI           = pstZone->pstZI;
    pMPgBF          = pstZone->pMetaPgBuf;
    pstBMT          = pstZone->pstBMTHdl;
    pstML           = pstZone->pstML;
    nPagesPerSBlk   = pstZone->pstDevInfo->nPagesPerSBlk;
    pstVFLParam     = FSR_STL_AllocVFLParam(pstZone);

    /* set BMT Handler of Lan 0 */
    FSR_STL_SetBMTHdl(pstZone, 
                pstBMT, 
                pMPgBF,
                pstML->nBMTBufSize);

    /**
     * read all context pages
     * for reducing open time, it may load map on demand whenever need
     * and then, each ctxinfo need additional flag of load or not
     */
    for (nIdx = 0; nIdx < pstZI->nNumLA; nIdx++)
    {
        nBOffs  = (POFFSET)(pstDH->pBMTDir[nIdx] >> pstDVI->nPagesPerSbShift);
        nVpn    = (pstZI->aMetaVbnList[nBOffs] << pstDVI->nPagesPerSbShift)
                    + (pstDH->pBMTDir[nIdx] & (nPagesPerSBlk - 1));

        pstVFLParam->pData        = (UINT8*)pMPgBF;
        pstVFLParam->bPgSizeBuf   = FALSE32;
        pstVFLParam->bUserData    = FALSE32;
        pstVFLParam->bSpare       = TRUE32;
        pstVFLParam->nBitmap      = pstML->nBMTSBM;
        pstVFLParam->nNumOfPgs    = 1;
        pstVFLParam->pExtParam    = NULL;

        nRet = FSR_STL_FlashCheckRead(pstZone, nVpn, pstVFLParam, 1, TRUE32);

        /* calculate zero bit count of BMT */
        nZBCBMT = FSR_STL_GetZBC((UINT8*)pstBMT->pBuf, pstBMT->nCfmBufSize);
        if (nRet == FSR_BML_READ_ERROR)
        {
            return nRet;
        }
        else if (nRet != FSR_BML_SUCCESS)
        {
            return nRet;
        }
        if ((pstBMT->pstCfm->nZBC != nZBCBMT) ||
            ((pstBMT->pstCfm->nZBC ^ 0xFFFFFFFF) != pstBMT->pstCfm->nInvZBC))
        {
            return FSR_STL_META_BROKEN;
        }

        /* copy BMT to full BMT table from context buffer. */
        FSR_OAM_MEMCPY(&(pstZone->pFullBMTBuf[nIdx * pstML->nBMTBufSize]), pstBMT->pBuf, pstBMT->nBufSize);
    }

    /* set BMT Handler of Lan 0 */
    FSR_STL_SetBMTHdl(pstZone, 
                pstBMT, 
                &(pstZone->pFullBMTBuf[0]),
                pstML->nBMTBufSize);

    FSR_STL_FreeVFLParam(pstZone, pstVFLParam);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}


#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
/**
 * @brief           This function scans the buffer block at the open time.
 * @n               and sets the BU info.
 *
 * @param[in]       pstZone          : pointer to atl Zone object
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PRIVATE INT32
_ScanBufferBlk     (STLZoneObj     *pstZone)
{
    UINT32          nIdx;
    INT32           nRet;
    STLCtxInfoHdl  *pstCI               = NULL;
    STLCtxInfoFm   *pstCIFm             = NULL;
    VFLParam       *pstVFLParam;
    UINT32          nVpn                = NULL_VPN;
    UINT16          nPTF = 0x0000;
    RBWDevInfo     *pstDVI              = NULL;
    STLBUCtxObj    *pstBUObj            = NULL;
    BOOL32          bBrokenBUInfo       = TRUE32;
    UINT32          nNumOfSctsPerPage;
    UINT8          *pTmpMPgBF           = NULL;
    UINT32          nPgsPerSBlk;
    UINT32          nPgsPerSBlkSht;
    UINT32          nCRC32Val;
    UINT32          nSpareIdx           = 0;
    UINT32          nSectorsPerCRC      = 1;
    UINT32          nSizePerCRC;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    pstCI               = pstZone->pstCtxHdl;
    pstCIFm             = pstCI->pstFm;
    pstDVI              = pstZone->pstDevInfo;
    pstBUObj            = pstZone->pstBUCtxObj;
    pTmpMPgBF           = pstZone->pTempMetaPgBuf;
    pstVFLParam         = FSR_STL_AllocVFLParam(pstZone);

    nPgsPerSBlk     = pstDVI->nPagesPerSBlk;
    nPgsPerSBlkSht  = pstDVI->nPagesPerSbShift;
    if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
    {
        /**
         * if MLC, pages per super block is half because of consuming LSB page
         * MLC LSB Half Pages Only
         */
        nPgsPerSBlk >>= 1;
        nPgsPerSBlkSht -= 1;
    }

    if (pstDVI->nDeviceType & RBW_DEVTYPE_ONENAND)
    {
        nSectorsPerCRC = 2;
    }

    nSizePerCRC = BYTES_PER_SECTOR * nSectorsPerCRC;

    /*
     * Scan BU...
     * From the last page to the first page
     */
    pstVFLParam->pData            = (UINT8*)pTmpMPgBF;
    pstVFLParam->bPgSizeBuf       = FALSE32;
    pstVFLParam->bUserData        = FALSE32;  /* Not User Data */
    pstVFLParam->bSpare           = TRUE32;
    pstVFLParam->nBitmap          = pstDVI->nFullSBitmapPerVPg;
    pstVFLParam->nNumOfPgs        = 1;
    pstVFLParam->pExtParam        = FSR_STL_AllocVFLExtParam(pstZone);
    pstVFLParam->pExtParam->nNumExtSData  = (UINT16)pstDVI->nSecPerVPg;

    for (nIdx = 0; nIdx < nPgsPerSBlk; nIdx++)
    {
        /*
         * Compute the nVpn in the Buffer Block from the last page to the first page.
         */
        if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
        {
        nVpn = (pstCIFm->nBBlkVbn << pstDVI->nPagesPerSbShift)
            + FSR_BML_GetVPgOffOfLSBPg(pstZone->nVolID, nIdx);
        }
        else
        {
            nVpn = (pstCIFm->nBBlkVbn << pstDVI->nPagesPerSbShift)
                 + nIdx;
        }

        nRet = FSR_STL_FlashCheckRead(pstZone, nVpn, pstVFLParam, 1, TRUE32);
        if (nRet == FSR_STL_CLEAN_PAGE)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG,
                           (TEXT("[SIF:INF]   BU VBN  (%5d)  CPOffset  (%5d) \r\n"), 
                           pstCIFm->nBBlkVbn, nIdx));
            break;
        }
        else if (nRet == FSR_BML_READ_ERROR)
        {
            break;
        }
        else if (nRet != FSR_BML_SUCCESS)
        {
            FSR_STL_FreeVFLExtParam(pstZone, pstVFLParam->pExtParam);
            FSR_STL_FreeVFLParam(pstZone, pstVFLParam);

            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
            return nRet;
        }
    }

    /*
     * reload the latest page
     */
    if (nIdx != 0)
    {
        nIdx--;

        if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
        {
            nVpn = (pstCIFm->nBBlkVbn << pstDVI->nPagesPerSbShift)
                + FSR_BML_GetVPgOffOfLSBPg(pstZone->nVolID, nIdx);
        }
        else
        {
            nVpn = (pstCIFm->nBBlkVbn << pstDVI->nPagesPerSbShift)
                 + nIdx;
        }

        nRet = FSR_STL_FlashCheckRead(pstZone, nVpn, pstVFLParam, 1, TRUE32);
        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_STL_FreeVFLExtParam(pstZone, pstVFLParam->pExtParam);
            FSR_STL_FreeVFLParam(pstZone, pstVFLParam);

            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
            return nRet;
        }

        nPTF = (UINT16)(pstVFLParam->nSData3 >> 16);
        FSR_ASSERT((nPTF & (1 << 5)) != 0);
        /*
         * Check the CRC of the last page.
         * if the CRC info is not correct, the BU info may be broken
         */
        nNumOfSctsPerPage = pstDVI->nSecPerVPg;
        if ((pstVFLParam->nSData1 ^ 0xFFFFFFFF) == pstVFLParam->nSData2)
        {
            for (nIdx = 0; nIdx < nNumOfSctsPerPage; nIdx = nIdx + nSectorsPerCRC)
            {
                if (pstDVI->nDeviceType & RBW_DEVTYPE_ONENAND)
                {
                    nSpareIdx = GET_ONENAND_SPARE_IDX(nIdx);
                }
                else
                {
                    nSpareIdx = nIdx;                    
                }

                nCRC32Val = FSR_STL_CalcCRC32(pstVFLParam->pData + (nIdx << BYTES_SECTOR_SHIFT), nSizePerCRC);
                if (nCRC32Val != pstVFLParam->pExtParam->aExtSData[nSpareIdx])
                {
                    bBrokenBUInfo = TRUE32;
                    break;
                }
            }

            if (nIdx >= nNumOfSctsPerPage)
            {
                bBrokenBUInfo = FALSE32;
            }
        }
    }
    
    /*
     * if the BU info is valid...
     * Build the BU Obj and the PMT info for the block which related to BU.
     */
    if (bBrokenBUInfo == FALSE32)
    {
        pstBUObj->nBBlkCPOffset     = (UINT16)nPgsPerSBlk;    /* 1 plus index of the last page of the BU block */
        pstBUObj->nBufferedLpn      = pstVFLParam->nSData1;
        pstBUObj->nBufferedDgn      = (BADDR)((pstBUObj->nBufferedLpn
            >> pstDVI->nPagesPerSbShift) >> pstZone->pstRI->nNShift);
        pstBUObj->nBufferedVpn      = nVpn;
        pstBUObj->nBufState         = STL_BUFSTATE_DIRTY;
        pstBUObj->nValidSBitmap     = 0x00;
        for (nIdx = 0; nIdx < (UINT32)(nPTF >> 8); nIdx++)
        {
            pstBUObj->nValidSBitmap |= (0x01 << nIdx);
        }
    }

    /*
     * if the BU info in invalid...
     * initialize all information for BU...
     */
    if (bBrokenBUInfo == TRUE32)
    {
        pstBUObj->nBBlkCPOffset     = (UINT16)nPgsPerSBlk;    /* 1 plus index of the last page of the BU block */
        pstBUObj->nBufferedLpn      = NULL_VPN;
        pstBUObj->nBufferedDgn      = NULL_DGN;
        pstBUObj->nBufferedVpn      = NULL_VPN;
        pstBUObj->nBufState         = STL_BUFSTATE_CLEAN;
        pstBUObj->nValidSBitmap     = 0x00;
    }

    FSR_STL_FreeVFLExtParam(pstZone, pstVFLParam->pExtParam);
    FSR_STL_FreeVFLParam(pstZone, pstVFLParam);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}

/**
 * @brief           This function flush the buffer block at the open time.
 * @n               and sets the BU info.
 *
 * @param[in]       pstZone          : pointer to atl Zone object
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PRIVATE INT32
_FlushBufferBlk     (STLZoneObj     *pstZone)
{
    STLCtxInfoHdl  *pstCI       = pstZone->pstCtxHdl;
    STLCtxInfoFm   *pstCIFm     = pstCI->pstFm;
    STLBUCtxObj    *pstBUObj    = pstZone->pstBUCtxObj;
    STLLogGrpHdl   *pstLogGrp   = NULL;
    STLLog         *pstLog;
    UINT32          nIdx;
    INT32           nLogIdx;
    BOOL32          bState;
    BADDR           nBBlkVbn;
    UINT32          nBBlkEC;
    INT32           nRet        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

#if (OP_SUPPORT_RUNTIME_PMT_BUILDING == 1)
    /* Execute Run time scanning */
    nRet = FSR_STL_ScanActLogBlock(pstZone,pstBUObj->nBufferedDgn);
    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR, 
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }
#endif /*OP_SUPPORT_RUNTIME_PMT_BUILDING*/

    /*
     * 1. BU Flush
     */
    while ((pstBUObj->nBufferedLpn != NULL_VPN) &&
           (pstBUObj->nBufState    != STL_BUFSTATE_CLEAN))
    {
        /* Reserve meta page */
        nRet = FSR_STL_ReserveMetaPgs(pstZone, 1, TRUE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        nRet = FSR_STL_FlushBufferPage(pstZone);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        /*
         * 2. identify that LogGrp flushed BU has 1more active log or not
         */
        pstLogGrp = FSR_STL_PrepareActLGrp(pstZone, pstBUObj->nBufferedLpn);
        if (pstLogGrp == NULL)
        {
            nRet = FSR_STL_ERR_NEW_LOGGRP;
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        /* check every logs in this log group */
        nLogIdx = pstLogGrp->pstFm->nHeadIdx;
        for (nIdx = 0; nIdx < pstLogGrp->pstFm->nNumLogs; nIdx++)
        {
            /* get log object pointer */
            pstLog = pstLogGrp->pstLogList + nLogIdx;
            bState    = ((pstLog->nState) & (1 << LS_ACTIVE_SHIFT)) ? TRUE32 : FALSE32;
            if (bState)
            {
                /*
                 * if  pstLog in pstLogGrp is Active state and pstLog don't have bufferedLpn,
                 * remove ActiveLogList, change state as inactive
                 */
                if (pstLog->nLastLpo != 
                    (POFFSET)(pstBUObj->nBufferedLpn & (pstZone->pstML->nPagesPerLGMT - 1)))
                {
                    FSR_STL_RemoveActLogList(pstZone, pstLogGrp->pstFm->nDgn, pstLog);
                }
            }

            /* move to next log */
            nLogIdx = pstLog->nNextIdx;
        }

        /*
         * 3. BU Block swap Free Block
         */
        nRet = FSR_STL_ReserveMetaPgs(pstZone, 1, FALSE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        FSR_ASSERT(pstCIFm->nBBlkVbn != NULL_VBN);
        if (pstCIFm->nBBlkEC > pstCI->pFBlksEC [pstCIFm->nFreeListHead])
        {
            /* Swap VBNs & ECs */
            nBBlkVbn = pstCI->pFreeList[pstCIFm->nFreeListHead];
            nBBlkEC  = pstCI->pFBlksEC [pstCIFm->nFreeListHead] + 1;

            pstCI->pFreeList[pstCIFm->nFreeListHead] = pstCIFm->nBBlkVbn;
            pstCI->pFBlksEC[pstCIFm->nFreeListHead]  = pstCIFm->nBBlkEC;

            pstCIFm->nBBlkVbn = nBBlkVbn;
            pstCIFm->nBBlkEC  = nBBlkEC;
        }
        else
        {
            pstCIFm->nBBlkEC++;
            nBBlkVbn = pstCIFm->nBBlkVbn;
            nBBlkEC  = pstCIFm->nBBlkEC;
        }
        FSR_ASSERT(nBBlkVbn != NULL_VBN);

        /* Get buffer blocks' VBN */
        nRet = FSR_STL_FlashErase(pstZone, nBBlkVbn);
        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        pstBUObj->nBBlkCPOffset = 0;

        /*
         * 4. store PMTCtx
         */
        nRet = FSR_STL_StorePMTCtx(pstZone, pstLogGrp, FALSE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        /*
         * 5. initialize BU Object
         */
        pstBUObj->nBBlkCPOffset     = 0;    /* 1 plus index of the last page of the BU block */
        pstBUObj->nBufferedLpn      = NULL_VPN;
        pstBUObj->nBufferedDgn      = NULL_DGN;
        pstBUObj->nBufferedVpn      = NULL_VPN;
        pstBUObj->nBufState         = STL_BUFSTATE_CLEAN;
        pstBUObj->nValidSBitmap     = 0x00;
        break;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}

#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */

/**
 * @brief            This function reads STL context pages.
 *
 * @param[in]       pstZone         : pointer to STL Zone object
 * @param[in]       nFlag           : Open flag
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PRIVATE INT32
_OpenZoneMeta      (STLZoneObj     *pstZone,
                    UINT32          nFlag)
{
    UINT32          nHeaderIdx;
    INT32           nRet;
#if !defined(FSR_OAM_RTLMSG_DISABLE)
    STLCtxInfoHdl  *pstCI       = NULL;
#endif
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    if (pstZone->bOpened == TRUE32)
    {
        return FSR_STL_SUCCESS;
    }

    /**
     * 1. Allocation STL Zone Memory
     */
    nRet = FSR_STL_AllocZoneMem(pstZone);
    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - Out of memory\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }

    /**
     * 2. Initialize Zone & Zone Information
     */
    FSR_STL_InitZone(pstZone);

    do
    {
        /*
         * 3. Update DirHeader & Load The Latest Meta Context
         */
        /* (1) read current directory header from scanning meta block. */
        nRet = _ScanDirHeader(pstZone, &nHeaderIdx);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - Directory header may be corrupted\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        /* (2) update PMTDir, BMTDir reading current meta block. */
        nRet = _LoadDirUpdate(pstZone, nHeaderIdx);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - Directory update fails\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }
        
        /**
         * (3) identity case of power-off during Meta reclaim. 
         * if Zone attribute is read only, prevent to program
         */
        if ((nFlag & FSR_STL_FLAG_RO_PARTITION) == 0)
        {
            nRet = _CheckMetaMerge(pstZone, nHeaderIdx);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - MetaMerge check fails\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }
        }

        /* (4) Full BMT Loading */
        nRet = _LoadFullBMT(pstZone);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - BMT load fails\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        /* (5) Load the latest context */
        nRet = _LoadLatestCxt(pstZone, nFlag);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - The latest context load fails\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

#if !defined(FSR_OAM_RTLMSG_DISABLE)
        /* ---------------------------------------------------------- */
        /* Debug Code                                                 */
        /* ---------------------------------------------------------- */
        {
            pstCI       = pstZone->pstCtxHdl;

            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:INF]   Number of Free Block       (%5d) \r\n"),
                    pstCI->pstFm->nNumFBlks));

            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:INF]   Active Log List     : Num  (%5d) \r\n"),
                    pstCI->pstFm->nNumLBlks));
        }
        /* ---------------------------------------------------------- */
#endif
        /* set zone open flag */
        pstZone->bOpened = TRUE32;
        nRet = FSR_STL_SUCCESS;
    } while (0);

    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_STL_FreeZoneMem(pstZone);
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}

/**
 * @brief           This function rebuild log block mapping table and flushes BU.
 *
 * @param[in]       pstZone         : pointer to STL Zone object
 * @param[out]      pstSTLInfo      : STL sector information
 * @param[in]       nFlag           : Open flag
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PRIVATE INT32
_OpenZonePost      (STLZoneObj     *pstZone,
                    UINT32          nFlag)
{
    INT32           nRet;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    do
    {
#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
        /*
         * 6. Scan BU for searching valid page
         */
        nRet = _ScanBufferBlk(pstZone);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - The BU context load fails\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }
#endif

#if (OP_SUPPORT_RUNTIME_PMT_BUILDING == 1)
        /* Set information of initial active log blocks */
        _SetInitialActLogInfos(pstZone);
#endif /*OP_SUPPORT_RUNTIME_PMT_BUILDING*/

        /*
         * 7. make PMT of all active logs 
         */
        nRet = _RebuildLogPageMap(pstZone);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - The PMT load fails\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
        /*
         * 8. Flush BU for initializing Buffer Block
         */
        if ((nFlag & FSR_STL_FLAG_RO_PARTITION) == 0)
        {
            nRet = _FlushBufferBlk(pstZone);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - The BU flush fails\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }
        }
#endif

#if (OP_STL_DEBUG_CODE == 1)
        FSR_STL_DBG_CheckValidity(pstZone);
        FSR_STL_DBG_CheckValidPgCnt(pstZone);
        FSR_STL_DBG_CheckMergeVictim(pstZone);
        FSR_STL_DBG_CheckWLGrp(pstZone);
#endif

        nRet = FSR_STL_SUCCESS;
    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}

/*****************************************************************************/
/* Global Function Definition                                                */
/*****************************************************************************/

/**
 * @brief            This function opens all zones which belong to the cluster
 *
 * @param[in]       nClstID         : volume identifier (RBW_VOLID_0, RBW_VOLID_1, ...)
 * @param[in]       nFlag           : Open flag
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PUBLIC INT32
FSR_STL_OpenCluster(UINT32          nClstID,
                    UINT32          nNumZone,
                    UINT32          nFlag)
{
    STLClstObj     *pstClst;
    STLZoneObj     *pstZone;
    UINT32          nZone;
    INT32           nRet        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* volume id check */
    if (nClstID >= MAX_NUM_CLUSTERS)
    {
        return FSR_STL_INVALID_VOLUME_ID;
    }

     /* Zone id check */
    if ((nNumZone == 0       ) ||
        (nNumZone >  MAX_ZONE))
    {
        return FSR_STL_INVALID_PARTITION_ID;
    }

    pstClst = gpstSTLClstObj[nClstID];

    /**
     * 1. Scan STLRootInfo page
     * search meta vbn list from scanning root block
     * if master zone is opened, other zone no more scan root block.
     */
    pstZone = &(pstClst->stZoneObj[0]);
    if (pstZone->bOpened != TRUE32)
    {
        nRet = _ScanRootInfo(pstZone);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - Root may be corrupted\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return nRet;
        }
    }

    do
    {
        /* 2. Rebuild all Meta object */
        for (nZone = 0; nZone < nNumZone; nZone++)
        {
            pstZone = &(pstClst->stZoneObj[nZone]);

            nRet = _OpenZoneMeta(pstZone, nFlag);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x \r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }
        }
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }

#if (OP_SUPPORT_GLOBAL_WEAR_LEVELING == 1)
        /* 3. Recovery Global WearLevel Information */
        if (nNumZone > 1)
        {
            pstZone = &(pstClst->stZoneObj[nNumZone - 1]);
            nRet = FSR_STL_RecoverGWLInfo(pstZone);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x \r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }
        }
#endif

        /* 4. Rebuild Active Page Map Table (PMT) */
        for (nZone = 0; nZone < nNumZone; nZone++)
        {
            pstZone = &(pstClst->stZoneObj[nZone]);

            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:INF]   =====> STL ZoneID[%2d] Active Log Block List\r\n"),
                    nZone));

            nRet = _OpenZonePost(pstZone,
                                 nFlag);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x \r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }
        }
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }
    } while (0);

    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_ASSERT(nZone < nNumZone);
        for (nZone = 0; nZone < nNumZone; nZone++)
        {
            pstZone = &(pstClst->stZoneObj[nZone]);
            FSR_STL_CloseZone(nClstID, nZone);
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}

