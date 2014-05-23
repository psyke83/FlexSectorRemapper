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
 *  @file       FSR_STL_MetaMgr.c
 *  @brief      Root Block, Directory Header, BMT Context, PMT Context.
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
/* Header file inclusions                                                    */
/*****************************************************************************/

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

PRIVATE INT32   _UpdateValidPgCnt  (      STLZoneObj   *pstZone,
                                    const POFFSET       nOldMPOff,
                                    const POFFSET       nNewMPOff,
                                    const BOOL32        bEnableMetaWL);

PRIVATE BADDR   _MetaWearLevel     (      STLZoneObj *pstZone,
                                    const BOOL32      bEnableMetaWL,
                                          BOOL32     *pbWearLevel,
                                          BADDR       nVbn,
                                    const UINT16      nDstBlkOffset);

/*****************************************************************************/
/* Local (static)  Function Definition                                       */
/*****************************************************************************/

/**
 * @brief           This function updates the valid page count of meta units.
 *
 * @param[in]       pstZone         : pointer to STL Zone object
 * @param[in]       nOldMPOff       : Old meta page offset
 * @param[in]       nNewMPOff       : New meta page offset
 * @param[in]       bEnableMetaWL   : meta wear-leveling enable flag
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PRIVATE INT32
_UpdateValidPgCnt  (      STLZoneObj   *pstZone,
                    const POFFSET       nOldMPOff,
                    const POFFSET       nNewMPOff,
                    const BOOL32        bEnableMetaWL)
{
    const   RBWDevInfo     *pstDVI          = pstZone->pstDevInfo;
    const   STLZoneInfo    *pstZI           = pstZone->pstZI;
            STLCtxInfoFm   *pstCIFm         = pstZone->pstCtxHdl->pstFm;
    const   STLDirHdrHdl   *pstDH           = pstZone->pstDirHdrHdl;
            STLDirHeaderFm *pstDHFm         = pstDH->pstFm;
            UINT32          nIdleIdx;
            UINT32          nIdx;
            BOOL32          bMetaWL;
            INT32           nRet;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%1d, %d, %d, 0x%1x)\r\n"),
            __FSR_FUNC__, pstZone->nZoneID, nOldMPOff, nNewMPOff, bEnableMetaWL));

    FSR_ASSERT(nNewMPOff != NULL_POFFSET);
    nIdx = nNewMPOff >> pstDVI->nPagesPerSbShift;
    FSR_ASSERT(nIdx < pstZI->nNumMetaBlks);

    /*  set valid page header index & valid page index */
    pstDHFm->nLatestBlkIdx = (UINT16)(nIdx);
    pstDHFm->nLatestPOffs = pstDHFm->nCPOffs;

    /*  increase valid page count of meta block having current map unit */
    pstDH->pValidPgCnt[nIdx]++;

    /*  decrease valid page count of meta block having old map unit */
    if (nOldMPOff != NULL_POFFSET)
    {
        nIdx = nOldMPOff >> pstDVI->nPagesPerSbShift;
        FSR_ASSERT(nIdx < pstZI->nNumMetaBlks);

        /*  nIdx : meta block index having old map unit */
        FSR_ASSERT(pstDH->pValidPgCnt[nIdx] >= 1);
        pstDH->pValidPgCnt[nIdx]--;

        /**
         * process invalid meta block
         *  if valid page count becomes to zero, that block should be 
         *  registered into idle block list.
         */
        if (pstDH->pValidPgCnt[nIdx] == 0)
        {
            nIdleIdx = (pstDHFm->nIdleBoListHead + pstDHFm->nNumOfIdleBlk);
            if (nIdleIdx >= pstZI->nNumMetaBlks)
            {
                nIdleIdx -= pstZI->nNumMetaBlks;
            }
            FSR_ASSERT(nIdleIdx < pstZI->nNumMetaBlks);

            pstDH->pIdleBoList[nIdleIdx] = (UINT16)(nIdx);
            pstDHFm->nNumOfIdleBlk += 1;
        }

        FSR_ASSERT(pstDHFm->nNumOfIdleBlk <= pstZI->nNumMetaBlks);
    }

#if (OP_SUPPORT_STATISTICS_INFO == 1)
    pstZone->pstStats->nCtxPgmCnt++;
#endif  /* (OP_SUPPORT_STATISTICS_INFO == 1) */

    do
    {
        /*  move to next clean VPN */
        pstDHFm->nCPOffs++;
        if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
        {
            /*
             * if MLC, pages per super block is half because of consuming LSB page
             *  MLC LSB Half Pages Only 
             */
            pstDHFm->nCPOffs &= ((pstDVI->nPagesPerSBlk >> 1) - 1);
        }
        else
        {
            pstDHFm->nCPOffs &= ((pstDVI->nPagesPerSBlk      )- 1);
        }

        /*  if the current clean page offset is zero, context header should be stored. */
        if (pstDHFm->nCPOffs == 0)
        {
            /*  protect from occurring meta & data WearLevel coincidentally */
            nRet = FSR_STL_StoreDirHeader(pstZone,
                                          bEnableMetaWL,
                                          &bMetaWL);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - FSR_STL_StoreDirHeader returns error\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }
        }

        /*  reset merge information in the context */
        pstCIFm->nMergedDgn                 = NULL_DGN;
        pstCIFm->nMergedVbn                 = NULL_VBN;
        pstCIFm->nMergeVictimDgn            = NULL_DGN;
        pstCIFm->nMergeVictimFlag           = 0;

        pstCIFm->nUpdatedPMTWLGrpIdx        = 0xFFFF;
        pstCIFm->nMinECPMTIdx               = 0xFFFF;
        pstCIFm->stUpdatedPMTWLGrp.nDgn     = NULL_DGN;
        pstCIFm->stUpdatedPMTWLGrp.nMinEC   = 0xFFFFFFFF;
        pstCIFm->stUpdatedPMTWLGrp.nMinVbn  = NULL_VBN;

        /* reset modified cost information in the context (POR) */
        pstCIFm->nModifiedDgn               = NULL_DGN;
        pstCIFm->nModifiedCost              = (UINT16)(-1);

        nRet = FSR_STL_SUCCESS;
    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}
/**
 * @brief           This function processes Meta WearLevel.
 *
 * @param[in]       pstZone             : pointer to atl Zone object
 * @param[in]       bEnableMetaWL       : meta wear-leveling enable flag
 * @param[in]       pbWearLevel         : flag for meta wear-level
 * @param[in]       nVbn                : meta block virtual address
 * @param[in]       nDstBlkOffset       : destination block offset
 *
 * @return          nVbn
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PRIVATE BADDR
_MetaWearLevel (      STLZoneObj     *pstZone,
                const BOOL32          bEnableMetaWL,
                      BOOL32         *pbWearLevel,
                      BADDR           nVbn,
                const UINT16          nDstBlkOffset)
{
#if (OP_SUPPORT_META_WEAR_LEVELING == 1)

    const   UINT32          nClstID     = pstZone->nClstID;
    const   STLClstObj     *pstClst     = gpstSTLClstObj[nClstID];
            STLZoneInfo    *pstZI       = pstZone->pstZI;
            STLDirHdrHdl   *pstDH       = pstZone->pstDirHdrHdl;
            STLCtxInfoHdl  *pstCI       = pstZone->pstCtxHdl;
            STLCtxInfoFm   *pstCIFm     = pstCI->pstFm;
            BADDR           nFBlkVbn;
            UINT32          nMBlkEC;
            UINT32          nFBlkEC;
    const   UINT32          nMaxFBlk    = pstZone->pstML->nMaxFreeSlots;
            UINT32          nFBlk;
            UINT32          nFBlkIdx;
            UINT32          nIdx;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%1d, 0x%1x, 0x%8x, %d, %d)\r\n"),
            __FSR_FUNC__, pstZone->nZoneID, bEnableMetaWL, pbWearLevel, nVbn, nDstBlkOffset));

    FSR_ASSERT(nVbn != NULL_VBN);

    /* initialize meta wearlevel flag */
    *pbWearLevel = FALSE32;

    /*  meta block erase count */
    nMBlkEC = pstDH->pMetaBlksEC[nDstBlkOffset];
    /** 
     * if erase count of meta block is bigger than 2^32, meta wear-level stop.
     *  but normal operation is possible. when there is at least one free block 
     */
    if ((bEnableMetaWL == TRUE32) &&
        (nMBlkEC < 0xFFFFFFFF)    &&
        (pstCIFm->nNumFBlks > 0))
    {
        /* Find the free blk which has minimum EC */
        nFBlk    = pstCIFm->nFreeListHead;
        nFBlkIdx = (UINT32)(-1);
        nFBlkEC  = (UINT32)(-1);
        for (nIdx = 0; nIdx < pstCIFm->nNumFBlks; nIdx++)
        {
            if (nFBlk >= nMaxFBlk)
            {
                nFBlk -= nMaxFBlk;
            }

            if (pstCI->pFBlksEC[nFBlk] < nFBlkEC)
            {
                nFBlkIdx = nFBlk;
                nFBlkEC  = pstCI->pFBlksEC[nFBlk];
            }

            nFBlk++;
        }
        FSR_ASSERT(nFBlkEC != (UINT32)(-1));

        /*  swapping target free block */
        nFBlkVbn = pstCI->pFreeList[nFBlkIdx];
        FSR_ASSERT(nFBlkVbn != NULL_VBN);
        FSR_ASSERT(nFBlkEC  == pstCI->pFBlksEC[nFBlkIdx]);

        /**
         * if difference between free block EC & meta block EC is bigger than
         *  MWL_DIFF_EC_THRESHOLD, meta Wear-leveling trigger.
         */
        if ((nMBlkEC > nFBlkEC) &&
            ((nMBlkEC - nFBlkEC) > (pstClst->pstEnvVar->nECDiffThreshold) >> 1))
        {

            /*  exchange meta block nVbn with nFBlkVbn */
            pstZI->aMetaVbnList[nDstBlkOffset] = nFBlkVbn;
            pstCI->pFreeList[nFBlkIdx]         = nVbn;
            nVbn = pstZI->aMetaVbnList[nDstBlkOffset];

            /*  change the erase count with each other */
            pstDH->pMetaBlksEC[nDstBlkOffset] = nFBlkEC;
            pstCI->pFBlksEC[nFBlkIdx]         = nMBlkEC;

            /*  need to change the STL context block list, later STL context block  */
            /*  list info should be stored in VFL context */
            *pbWearLevel = TRUE32;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%01x, %d\r\n"),
            __FSR_FUNC__, __LINE__, *pbWearLevel, nVbn));
    return nVbn;

#endif /* (OP_SUPPORT_META_WEAR_LEVELING == 1) */
}

/*****************************************************************************/
/* Global Function Definition                                                */
/*****************************************************************************/

/**
 * @brief           This function reads the STL context header page to Flash.
 *
 * @param[in]       pstZone             : pointer to atl Zone object
 * @param[in]       nHeaderVbn          : meta block virtual address
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PUBLIC INT32
FSR_STL_ReadHeader (STLZoneObj *pstZone,
                    BADDR       nHeaderVbn)
{
    const   RBWDevInfo     *pstDVI          = pstZone->pstDevInfo;
    const   STLZoneInfo    *pstZI           = pstZone->pstZI;
    const   STLMetaLayout  *pstML           = pstZone->pstML;
            STLDirHdrHdl   *pstDH           = pstZone->pstDirHdrHdl;
            VFLParam       *pstVFLParam;
            PADDR           nVpn            = NULL_VPN;
            UINT32          nIdx;
            INT32           nRet            = FSR_STL_META_BROKEN;
            UINT32          nZBitCnt;
            UINT32          nInvertedZBC;
            UINT32          nZBCHeader;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%1d, %d)\r\n"),
            __FSR_FUNC__, pstZone->nZoneID, nHeaderVbn));

    pstVFLParam               = FSR_STL_AllocVFLParam(pstZone);
    pstVFLParam->bPgSizeBuf   = FALSE32;
    pstVFLParam->bUserData    = FALSE32;
    pstVFLParam->bSpare       = TRUE32;
    pstVFLParam->nBitmap      = pstML->nDirHdrSBM;
    pstVFLParam->nNumOfPgs    = 1;
    pstVFLParam->pExtParam    = FSR_STL_AllocVFLExtParam(pstZone);
    pstVFLParam->pExtParam->nNumExtSData = 0;

    /*  read whole header pages if multi-header page is true */
    pstVFLParam->pData = pstDH->pBuf;
    for (nIdx = 0; nIdx < pstZI->nNumDirHdrPgs; nIdx++)
    {
        if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
        {
            nVpn = (nHeaderVbn << pstDVI->nPagesPerSbShift)
                 + FSR_BML_GetVPgOffOfLSBPg(pstZone->nVolID, nIdx);
        }
        else
        {
            nVpn = (nHeaderVbn << pstDVI->nPagesPerSbShift)
                 + nIdx;
        }

        nRet         = FSR_STL_FlashCheckRead(pstZone, nVpn, pstVFLParam, 1, TRUE32);

        /* calculate zero bit count for header page confirm */
        nZBCHeader   = FSR_STL_GetZBC(pstVFLParam->pData,
                                      pstDVI->nBytesPerVPg);

        nZBitCnt     = pstVFLParam->nSData1;
        nInvertedZBC = pstVFLParam->nSData2;
        if (nRet == FSR_STL_CLEAN_PAGE)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:INF] %s() L(%d) : 0x%08x - clean page (nVpn = %d)\r\n"),
                    __FSR_FUNC__, __LINE__, FSR_STL_CLEAN_PAGE, nVpn));
            nRet = FSR_STL_CLEAN_PAGE;
            break;
        }
        else if (nRet == FSR_BML_READ_ERROR)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] %s() L(%d) : 0x%08x - U_ECC (nVpn = %d)\r\n"),
                    __FSR_FUNC__, __LINE__, FSR_STL_META_BROKEN, nVpn));
            nRet = FSR_STL_META_BROKEN;
            break;
        }
        else if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] %s() L(%d) : 0x%08x - VFL Error (nVpn = %d)\r\n"),
                    __FSR_FUNC__, __LINE__, nRet, nVpn));
            break;
        }
        else if ((nZBitCnt                    != nZBCHeader) ||
                 ((nInvertedZBC ^ 0xFFFFFFFF) != nZBCHeader))
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] %s() L(%d) : 0x%08x - ZeroBit Error (nVpn = %d)\r\n"),
                    __FSR_FUNC__, __LINE__, FSR_STL_META_BROKEN, nVpn));
            nRet = FSR_STL_META_BROKEN;
            break;
        }

        pstVFLParam->pData += pstDVI->nBytesPerVPg;
    }

    if (nIdx == pstZI->nNumDirHdrPgs)
    {
        /* CPO of meta block is next page of header page. */
        pstDH->pstFm->nCPOffs = (POFFSET)(pstZI->nNumDirHdrPgs);
        nRet = FSR_STL_SUCCESS;
    }

    FSR_STL_FreeVFLExtParam(pstZone, pstVFLParam->pExtParam);
    FSR_STL_FreeVFLParam(pstZone, pstVFLParam);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}

/**
 * @brief           This function programs the STL context header page to Flash
 *
 * @param[in]       pstZone             : pointer to atl Zone object
 * @param[in]       nHeaderVbn          : meta block virtual address
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PUBLIC INT32
FSR_STL_ProgramHeader  (STLZoneObj *pstZone,
                        BADDR       nHeaderVbn)
{
    const   RBWDevInfo     *pstDVI          = pstZone->pstDevInfo;
    const   STLZoneInfo    *pstZI           = pstZone->pstZI;
    const   STLMetaLayout  *pstML           = pstZone->pstML;
            STLDirHdrHdl   *pstDH           = pstZone->pstDirHdrHdl;
            VFLParam       *pstVFLParam;
            PADDR           nVpn            = NULL_VPN;
            UINT16          nPTF;
            UINT32          nIdx;
            INT32           nRet            = FSR_STL_CRITICAL_ERROR;
            UINT32          nZBCHeader;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%1d, %d)\r\n"),
            __FSR_FUNC__, pstZone->nZoneID, nHeaderVbn));

    /*  age decrement */
    if (pstDH->pstFm->nAge_Low == 0x00000000)
    {
        pstDH->pstFm->nAge_High -= 1;
        pstDH->pstFm->nAge_Low   = 0xFFFFFFFF;
    }
    else
    {
        pstDH->pstFm->nAge_Low -= 1;
    }

    /*  set PTF value */
    nPTF = FSR_STL_SetPTF(pstZone,
                          TF_META,
                          MT_HEADER,
                          TF_NULL,
                          TF_NULL);

    pstVFLParam               = FSR_STL_AllocVFLParam(pstZone);
    pstVFLParam->bPgSizeBuf   = FALSE32;
    pstVFLParam->bUserData    = FALSE32;
    pstVFLParam->bSpare       = TRUE32;
    pstVFLParam->nBitmap      = pstML->nDirHdrSBM;
    pstVFLParam->nNumOfPgs    = 1;

    /*pstVFLParam->nSData1 will be set in later codes*/    
    pstVFLParam->nSData3      = (nPTF << 16) | (nPTF^0xFFFF);

    pstVFLParam->pExtParam    = FSR_STL_AllocVFLExtParam(pstZone);
    pstVFLParam->pExtParam->nNumExtSData  = 0;

    pstVFLParam->pData = pstDH->pBuf;
    for (nIdx = 0; nIdx < pstZI->nNumDirHdrPgs; nIdx++)
    {
        /*  target VPN  */
        /*  header page will be the first page of that context block */
        if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
        {
            nVpn = (nHeaderVbn << pstDVI->nPagesPerSbShift)
                 + FSR_BML_GetVPgOffOfLSBPg(pstZone->nVolID, nIdx);
        }
        else
        {
            nVpn = (nHeaderVbn << pstDVI->nPagesPerSbShift) + nIdx;
        }

        /*  Meta Confirm : Zero Bit Count */
        nZBCHeader = FSR_STL_GetZBC(pstVFLParam->pData,
                                    pstDVI->nBytesPerVPg);
        pstVFLParam->nSData1      = nZBCHeader;
        pstVFLParam->nSData2      = nZBCHeader ^ 0xFFFFFFFF;
        nRet = FSR_STL_FlashProgram(pstZone,
                                    nVpn,
                                    pstVFLParam);
        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR]  --%s() L(%d) : 0x%08x - FSR_STL_FlashProgram returns error\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }
        FSR_STL_FlashFlush(pstZone);

        pstVFLParam->pData += pstDVI->nBytesPerVPg;
    }

    if (nIdx == pstZI->nNumDirHdrPgs)
    {
        pstDH->pstFm->nCPOffs = (POFFSET)(pstZI->nNumDirHdrPgs);
        nRet = FSR_STL_SUCCESS;
    }

    FSR_STL_FreeVFLExtParam(pstZone, pstVFLParam->pExtParam);
    FSR_STL_FreeVFLParam(pstZone, pstVFLParam);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}

/**
 * @brief           This function processes meta block reclaim.
 *
 * @param[in]       pstZone             : pointer to stl Zone object
 * @param[in]       nVbn                : meta block virtual address
 * @param[in]       nDstBlkOffset       : destination block offset
 * @param[in]       nVictimIdx          : victim meta block index
 * @param[in]       nMinPgCnt           : victim meta block valid page count
 * @param[in]       nStartIdx           : reclaim start index
 * @param[in]       bReserveMeta        : reserve meta page flag
 * @param[out]      nEndCPOffs          : reclaim end index
 * @param[in]       bOpenFlag           : open flag
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PUBLIC INT32
FSR_STL_ReclaimMeta(STLZoneObj *pstZone,
                    BADDR       nVbn,
                    UINT16      nDstBlkOffset,
                    UINT32      nVictimIdx,
                    UINT16      nMinPgCnt,
                    BOOL32      bReserveMeta)
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
            BADDR           nVictimVbn      = NULL_VBN;
            PADDR           nCxtVpn;
            PADDR           nBMTVpn         = NULL_VPN;
            UINT32          i;
            POFFSET         nMetaPOffset    = NULL_POFFSET;
            INT32           nRet;
            UINT8          *aValidPgs       = pstZone->pTempMetaPgBuf;
            UINT16         *naLoopIdx       = (UINT16*)(pstZone->pTempMetaPgBuf + pstDVI->nPagesPerSBlk);
    const   UINT32          nPgsMsk         = pstDVI->nPagesPerSBlk - 1;
    const   UINT32          nWayMsk         = pstDVI->nNumWays - 1;
            UINT32          nPOff           = (UINT32)(-1);
            UINT32          nRemainPgs;
            UINT32          naWayIdx[FSR_MAX_WAYS];
            UINT32          nWay;
            BOOL32          bWayBreak;
            UINT32          nPOffIdx        = (UINT32)(-1);
            STLClstObj     *pstClst         = FSR_STL_GetClstObj(pstZone->nClstID);
    const   UINT32          n1stVun         = pstClst->pstEnvVar->nStartVbn;
            BMLCpBkArg    **ppstBMLCpBk     = pstClst->pstBMLCpBk;
            BMLCpBkArg     *pstCpBkArg      = pstClst->staBMLCpBk;
            BMLRndInArg    *pstRndIn        = pstClst->staBMLRndIn;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    if ((pstDHFm->nNumOfIdleBlk > 1) ||
        ((pstDHFm->nNumOfIdleBlk != 0) && (bReserveMeta == FALSE32)))
    {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, FSR_STL_SUCCESS));
            return FSR_STL_SUCCESS;
    }

    /**
     * the victim context block is moved to the current context block.
     * page copies : nMinPgCnt 
     */
    if (nMinPgCnt > 0)
    {
        pstVFLParam = FSR_STL_AllocVFLParam(pstZone);

        /*  get victim VBN using victim index */
        nVictimVbn = pstZI->aMetaVbnList[nVictimIdx];

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
        nZBCCtx = FSR_STL_GetZBC((UINT8*)pstCI->pBuf,
                                         pstCI->nCfmBufSize);

        /* set ctx's zero bit count */
        pstCI->pstCfm->nZBC    = nZBCCtx;
        pstCI->pstCfm->nInvZBC = nZBCCtx ^ 0xFFFFFFFF;

        FSR_OAM_MEMSET(ppstBMLCpBk, 0x00, sizeof(long) * FSR_MAX_WAYS);
        for (nWay = 0; nWay < pstDVI->nNumWays; nWay++)
        {
            naWayIdx[nWay] = nWay;
        }

        /* copy source log's pages to destination log */
        nRemainPgs = nMinPgCnt;
        bWayBreak  = FALSE32;
        nWay       = pstDHFm->nCPOffs & (pstDVI->nNumWays - 1);
        while (nRemainPgs != 0)
        {
            while (naWayIdx[nWay] < pstDVI->nPagesPerSBlk)
            {
                if (aValidPgs[naWayIdx[nWay]] != 0)
                {
                    if (((pstDHFm->nCPOffs) & nWayMsk) != nWay)
                    {
                        bWayBreak = TRUE32;
                        break;
                    }
                    /* valid context page is copied to new block. */
                    if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
                    {
                        nPOff = FSR_BML_GetVPgOffOfLSBPg(pstZone->nVolID, pstDHFm->nCPOffs);
                    }
                    else
                    {
                        nPOff = pstDHFm->nCPOffs;
                    }

                    ppstBMLCpBk[nWay] = &(pstCpBkArg[nWay]);
                }

                if (aValidPgs[naWayIdx[nWay]] == MT_BMT)
                {
                    /* Update BMT directory */
                    pstDH->pBMTDir[naLoopIdx[naWayIdx[nWay]]] =
                            (POFFSET)((nDstBlkOffset << pstDVI->nPagesPerSbShift) + nPOff);

                    pstRndIn[nWay].nOffset       = (UINT16)(pstML->nBMTBufSize);
                    pstRndIn[nWay].nNumOfBytes   = (UINT16)(pstML->nCtxBufSize);
                    pstRndIn[nWay].pBuf          = pstCI->pBuf;
                }
                else if (aValidPgs[naWayIdx[nWay]] == MT_PMT)
                {
                    /* Update PMT directory */
                    nLoopIdx = naLoopIdx[naWayIdx[nWay]];
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

                    pstRndIn[nWay].nOffset       = (UINT16)0;
                    pstRndIn[nWay].nNumOfBytes   = (UINT16)(pstML->nCtxBufSize);
                    pstRndIn[nWay].pBuf          = pstCI->pBuf;
                }

                if (ppstBMLCpBk[nWay] != NULL)
                {
                    FSR_ASSERT(aValidPgs[naWayIdx[nWay]] != 0);

                    pstCpBkArg[nWay].nSrcVun        = (UINT16)(nVictimVbn + n1stVun);
                    pstCpBkArg[nWay].nSrcPgOffset   = (UINT16)(naWayIdx[nWay]);
                    pstCpBkArg[nWay].nDstVun        = (UINT16)(nVbn       + n1stVun);
                    pstCpBkArg[nWay].nDstPgOffset   = (UINT16)(nPOff);
                    pstCpBkArg[nWay].nRndInCnt      = 1;
                    pstCpBkArg[nWay].pstRndInArg    = &(pstRndIn[nWay]);

                    /*  set valid page header index & page index */
                    pstDHFm->nLatestBlkIdx     = nDstBlkOffset;
                    pstDHFm->nLatestPOffs      = pstDHFm->nCPOffs;

                    /*  clean page moves to the next */
                    FSR_ASSERT(pstDHFm->nCPOffs < nPgsPerSBlk);
                    pstDHFm->nCPOffs++;
                    pstDH->pValidPgCnt[nDstBlkOffset]++;
                    pstDH->pValidPgCnt[nVictimIdx]--;

                    aValidPgs[naWayIdx[nWay]] = 0;
                    nRemainPgs--;

#if (OP_SUPPORT_STATISTICS_INFO == 1)
                    pstZone->pstStats->nCtxPgmCnt++;
#endif
                    naWayIdx[nWay] += pstDVI->nNumWays;
                    break;
                }

                FSR_ASSERT(pstDHFm->nCPOffs <= nPgsPerSBlk);
                naWayIdx[nWay] += pstDVI->nNumWays;
            }

            nWay++;
            if (nWay >= pstDVI->nNumWays)
            {
                nWay = 0;
            }

            if ((nWay       == 0) ||
                (nRemainPgs == 0) ||
                (bWayBreak  == TRUE32))
            {
#if defined (FSR_POR_USING_LONGJMP)
                FSR_FOE_BeginWriteTransaction(0);
#endif
                nRet = FSR_BML_CopyBack(pstZone->nVolID, ppstBMLCpBk, FSR_BML_FLAG_NONE);
                if (nRet != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR]  --%s() L(%d) : 0x%08x - FSR_BML_CopyBack\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    FSR_ASSERT(nRet != FSR_BML_VOLUME_NOT_OPENED);
                    FSR_ASSERT(nRet != FSR_BML_INVALID_PARAM);
                    FSR_ASSERT(nRet != FSR_BML_WR_PROTECT_ERROR);

                    FSR_STL_FreeVFLExtParam(pstZone, pstVFLParam->pExtParam);
                    FSR_STL_FreeVFLParam(pstZone, pstVFLParam);

                    FSR_STL_LockPartition(pstZone->nClstID);
                    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x - FSR_BML_CopyBack return error\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    return nRet;
                }

                FSR_OAM_MEMSET(ppstBMLCpBk, 0x00, sizeof(long) * FSR_MAX_WAYS);
            }

            if (bWayBreak == TRUE32)
            {
                break;
            }
        }

        if (nRemainPgs != 0)
        {
            /* Find minimum index */
            nPOffIdx = pstDVI->nPagesPerSBlk;
            for (nWay = 0; nWay < pstDVI->nNumWays; nWay++)
            {
                if (naWayIdx[nWay] < nPOffIdx)
                {
                    nPOffIdx = naWayIdx[nWay];
                }
            }
            FSR_ASSERT(nPOffIdx < pstDVI->nPagesPerSBlk);
        }

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

                    FSR_STL_LockPartition(pstZone->nClstID);
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

#if (OP_SUPPORT_STATISTICS_INFO == 1)
                pstZone->pstStats->nCtxPgmCnt++;
#endif
            }

            FSR_ASSERT(nPOffIdx < pstDVI->nPagesPerSBlk);
            nPOffIdx++;
        }

        FSR_STL_FreeVFLExtParam(pstZone, pstVFLParam->pExtParam);
        FSR_STL_FreeVFLParam(pstZone, pstVFLParam);
    }

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
 * @brief           This function processes meta block reclaim and reserve meta pages.
 *
 * @param[in]       pstZone             : pointer to atl Zone object
 * @param[in]       nNCleanPgs          : clean pages needed for processing next sequence
 * @param[in].......bEnableMetaWL       : enable meta wearlevel flag
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PUBLIC INT32
FSR_STL_ReserveMetaPgs( STLZoneObj *pstZone,
                        UINT16      nNCleanPgs,
                        BOOL32      bEnableMetaWL)
{
    const   RBWDevInfo     *pstDVI          = pstZone->pstDevInfo;
    const   STLZoneInfo    *pstZI           = pstZone->pstZI;
    const   STLDirHdrHdl   *pstDH           = pstZone->pstDirHdrHdl;
            STLDirHeaderFm *pstDHFm         = pstDH->pstFm;
            UINT32          nPgsPerSBlk     = pstDVI->nPagesPerSBlk;
            UINT32          nIdx;
            UINT32          i;
            UINT32          nVictimIdx      = 0;
            UINT16          nMinPgCnt;
            BOOL32          bWearLevel      = FALSE32;
            INT32           nRet;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%1d, %d, 0x%1x)\r\n"),
            __FSR_FUNC__, pstZone->nZoneID, nNCleanPgs, bEnableMetaWL));

    /*
     * if MLC, pages per super block is half because of consuming LSB page
     *  MLC LSB Half Pages Only 
     */
    if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
    {
        nPgsPerSBlk >>= 1;
    }

    if (((pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
        && (pstDHFm->nCPOffs == nPgsPerSBlk))
        || (pstDHFm->nCPOffs == pstDVI->nPagesPerSBlk))

    {
        BOOL32 bMetaWL;
        nRet = FSR_STL_StoreDirHeader(pstZone,
                                  bEnableMetaWL,
                                  &bMetaWL);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return nRet;
        }
    }
    

    /**
     * 1. compare between request pages & current clean pages.
     * if current clean pages is smaller than request pages, 
     * erase next idle block for reclaiming meta.
     */
    if ((pstDHFm->nNumOfIdleBlk > 1) ||
        (nNCleanPgs <= nPgsPerSBlk - pstDHFm->nCPOffs))
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, FSR_STL_SUCCESS));
        return FSR_STL_SUCCESS;
    }


    /**
     *  2. search victim index & valid page count
     *  block having the smallest valid page is victim.
     *  the smallest valid page count block is replaced.
     */
    nMinPgCnt = NULL_POFFSET;
    for (nIdx = 0; nIdx < pstZI->nNumMetaBlks; nIdx++)
    {
        if ((nIdx != pstDHFm->nLatestBlkIdx) &&
            (nIdx != pstDH->pIdleBoList[pstDHFm->nIdleBoListHead]) &&
            (nIdx != pstDHFm->nHeaderIdx))
        {
            if (nMinPgCnt > pstDH->pValidPgCnt[nIdx])
            {
                nVictimIdx = nIdx;
                nMinPgCnt  = pstDH->pValidPgCnt[nIdx];
            }
        }
    }
    FSR_ASSERT(nMinPgCnt < nPgsPerSBlk - 1);

    pstDHFm->nVictimIdx = nVictimIdx;

    if (pstDH->pValidPgCnt[nVictimIdx] == 0)
    {
        FSR_ASSERT(nMinPgCnt == 0);

        i = (pstDHFm->nIdleBoListHead + pstDHFm->nNumOfIdleBlk);
        if (i >= pstZI->nNumMetaBlks)
        {
            i -= pstZI->nNumMetaBlks;
        }
        FSR_ASSERT(i < pstZI->nNumMetaBlks);

        /* add idle block to list */
        pstDH->pIdleBoList[i] = (UINT16)(nVictimIdx);
        pstDHFm->nNumOfIdleBlk++;

        if (pstDHFm->nNumOfIdleBlk > 1)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, FSR_STL_SUCCESS));
            return FSR_STL_SUCCESS;
        }
    }

    do
    {
        nRet = FSR_STL_ReclaimMeta(pstZone,
                                   pstZI->aMetaVbnList[pstDHFm->nHeaderIdx],
                                   pstDHFm->nHeaderIdx,
                                   nVictimIdx,
                                   nMinPgCnt,
                                   TRUE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x - FSR_STL_ReclaimMeta returns error\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        /* reset nCPOffs to prepare new meta blk */
        pstDHFm->nCPOffs = 0;
        nRet = FSR_STL_StoreDirHeader(pstZone,
                                      TRUE32,
                                      &bWearLevel);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - FSR_STL_StoreDirHeader returns error\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        FSR_ASSERT(nNCleanPgs <= nPgsPerSBlk - pstDHFm->nCPOffs);
        FSR_ASSERT(pstDHFm->nNumOfIdleBlk != 0);
        nRet = FSR_STL_SUCCESS;
    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}


/**
 * @brief           This function stores the directory header page to Flash.
 * @n               It makes a buffer to process the header page in the VFL.
 *
 * @param[in]       pstZone             : pointer to atl Zone object
 * @param[in]       bEnableMetaWL       : meta wear-leveling enable flag
 * @param[out]      pbWearLevel         : flag for meta wear-level
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PUBLIC INT32
FSR_STL_StoreDirHeader (STLZoneObj *pstZone,
                        BOOL32      bEnableMetaWL,
                        BOOL32     *pbWearLevel)
{
    const   UINT32          nClstID         = pstZone->nClstID;
            STLClstObj     *pstClst         = gpstSTLClstObj[nClstID];
    const   RBWDevInfo     *pstDVI          = pstZone->pstDevInfo;
    const   STLZoneInfo    *pstZI           = pstZone->pstZI;
            STLDirHdrHdl   *pstDH           = pstZone->pstDirHdrHdl;
            STLDirHeaderFm *pstDHFm         = pstDH->pstFm;
            BADDR           nVbn            = NULL_VBN;
            UINT16          nDstBlkOffset   = 0;
            INT32           nRet;
            UINT32          nIdx            = 0;
            BOOL32          bStoreMeta      = FALSE32;
            UINT32          nVictimIdx      = 0;
            UINT16          nMinPgCnt       = (UINT16)(pstDVI->nPagesPerSBlk);
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%1d, 0x%1x, 0x%08x)\r\n"),
            __FSR_FUNC__, pstZone->nZoneID, bEnableMetaWL, pbWearLevel));

    FSR_ASSERT(pstDHFm->nNumOfIdleBlk >= 1);

    /*  head position dose not exceed the number of whole meta block. */
    FSR_ASSERT(pstDHFm->nIdleBoListHead < pstZI->nNumMetaBlks);

    nDstBlkOffset = pstDH->pIdleBoList[pstDHFm->nIdleBoListHead];

    /*  nBlkOffset dose not exceed the number of whole meta block. */
    FSR_ASSERT(nDstBlkOffset < pstZI->nNumMetaBlks);

    /*  Destination meta block does not have valid page. */
    FSR_ASSERT(pstDH->pValidPgCnt[nDstBlkOffset] == 0);

    /*  get destination vbn */
    nVbn = pstZI->aMetaVbnList[nDstBlkOffset];

    do
    {

#if (OP_SUPPORT_META_WEAR_LEVELING == 1)
        if (bEnableMetaWL == TRUE32)
        {
            BADDR nTempVbn = nVbn;

            nVbn = _MetaWearLevel(pstZone,
                                  bEnableMetaWL,
                                  pbWearLevel,
                                  nTempVbn,
                                  nDstBlkOffset);
            if (nVbn == NULL_VBN)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  ++%s() L(%d) : 0x%08x - _MetaWearLevel returns NULL_VBN\r\n"),
                        __FSR_FUNC__, __LINE__, nVbn));

                nRet = FSR_STL_CRITICAL_ERROR;
                FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                    (TEXT("[SIF:ERR]  ++%s() L(%d) : 0x%08x - _MetaWearLevel returns NULL_VBN\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }
        }
#endif  /* (OP_SUPPORT_META_WEAR_LEVELING == 1) */

        /*  increase meta block erase count */
        pstDH->pMetaBlksEC[nDstBlkOffset]++;

        /*  block erase */
        nRet = FSR_STL_FlashErase(pstZone, nVbn);
        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR]  ++%s() L(%d) : 0x%08x - FSR_STL_FlashErase returns error\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        /*  number of idle block decrement */
        pstDHFm->nNumOfIdleBlk  -= 1;
        pstDHFm->nIdleBoListHead = ++(pstDHFm->nIdleBoListHead);
        if (pstDHFm->nIdleBoListHead >= pstZI->nNumMetaBlks)
        {
            pstDHFm->nIdleBoListHead = pstDHFm->nIdleBoListHead - pstZI->nNumMetaBlks;
        }
        FSR_ASSERT(pstDHFm->nIdleBoListHead < pstZI->nNumMetaBlks);

        if (pstDHFm->nNumOfIdleBlk == 0)
        {
            /**
             *  block having the smallest valid page is victim.
             *  the smallest valid page count block is replaced.
             */
            for (nIdx = 0; nIdx < pstZI->nNumMetaBlks; nIdx++)
            {
                if ((nIdx != nDstBlkOffset) &&
                    (nIdx != pstDHFm->nLatestBlkIdx))
                {
                    if (nMinPgCnt > pstDH->pValidPgCnt[nIdx])
                    {
                        nVictimIdx = nIdx;
                        nMinPgCnt  = pstDH->pValidPgCnt[nIdx];
                    }
                }
            }

#if (OP_STL_DEBUG_CODE == 1)
            if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
            {
                FSR_ASSERT(nMinPgCnt < (pstDVI->nPagesPerSBlk >> 1) - 1);
            }
            else
            {
                FSR_ASSERT(nMinPgCnt < pstDVI->nPagesPerSBlk - 1);
            }
#endif  /* (OP_STL_DEBUG_CODE == 1) */

            pstDHFm->nVictimIdx = nVictimIdx;

            if (nMinPgCnt == 0)
            {
                bStoreMeta = TRUE32;
            }
        }
        else
        {
            bStoreMeta = TRUE32;
        }

        /* header program */
        nRet = FSR_STL_ProgramHeader(pstZone, nVbn);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR]  ++%s() L(%d): 0x%08x - FSR_STL_ProgramHeader returns error\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        /*  change the context header index */
        pstDHFm->nHeaderIdx = nDstBlkOffset;

#if (OP_SUPPORT_STATISTICS_INFO == 1)
        pstZone->pstStats->nCtxPgmCnt++;
#endif  /* (OP_SUPPORT_STATISTICS_INFO == 1) */

        nRet = FSR_STL_ReclaimMeta(pstZone,
                                   nVbn,
                                   nDstBlkOffset,
                                   nVictimIdx,
                                   nMinPgCnt,
                                   FALSE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR]  ++%s() L(%d) : 0x%08x - FSR_STL_ReclaimMeta returns error\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        /*
         * when meta block is changed, need to store context info reason to change
         * free list (meta WL, merge ... etc)
         */
        if ((*pbWearLevel == TRUE32) &&
            (bStoreMeta   == TRUE32))
        {
            nRet = FSR_STL_StoreBMTCtx(pstZone, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  ++%s() L(%d) : 0x%08x - FSR_STL_StoreBMTCtx returns error\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }
        }

        /*  we have to remain clean pages to write  */
        FSR_ASSERT(pstDHFm->nCPOffs < pstDVI->nPagesPerSBlk);

#if (OP_SUPPORT_META_WEAR_LEVELING == 1)
        /*  when meta block list is changed by meta wear-level, need to store root info */
        if (*pbWearLevel == TRUE32)
        {
            /*  Store root info block for managing meta vbn list */
            nRet = FSR_STL_StoreRootInfo(pstClst);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - FSR_STL_StoreRootInfo returns error\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }

        }
#endif  /* (OP_SUPPORT_META_WEAR_LEVELING == 1) */

        nRet = FSR_STL_SUCCESS;
    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}

/**
 * @brief           This function writes root info page to NAND flash
 *
 * @param[in]       pstClst         : pointer to cluster object
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PUBLIC INT32    
FSR_STL_StoreRootInfo  (STLClstObj *pstClst)
{
    const   RBWDevInfo     *pstDVI          = pstClst->pstDevInfo;
            STLRootInfo    *pstRI           = &(pstClst->stRootInfoBuf.stRootInfo);
            STLZoneObj     *pstZone         = &(pstClst->stZoneObj[0]);
            VFLParam       *pstVFLParam;
            UINT32          nPgsPerBlk      = pstDVI->nPagesPerSBlk;
            UINT32          nPgsSft         = pstDVI->nPagesPerSbShift;
            BADDR           nVbn;
            PADDR           nVpn;
            UINT16          nPTF;
            INT32           nRet;
            UINT32          nZBCRoot;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%d, %d)\r\n"),
            __FSR_FUNC__, pstClst->nVolID, pstZone->nClstID));

    if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
    {
        nPgsPerBlk >>= 1;
        nPgsSft--;
    }

    FSR_ASSERT(pstRI->nRootCPOffs <= (pstRI->nNumRootBlks << nPgsSft));

    /*  age decrement */
    if (pstRI->nAge_Low == 0x00000000)
    {
        pstRI->nAge_High -= 1;
        pstRI->nAge_Low = 0xFFFFFFFF;
    }
    else
    {
        pstRI->nAge_Low -= 1;
    }

    /* The latest pages in each way is used source page of copy-back
     * in order to partial page program. Therefore, whenever the pages is
     * loaded the load operation must be success.
     * In order to remain the pages to be clean, we skip the pages */
    if ((pstRI->nRootCPOffs >= nPgsPerBlk - pstDVI->nNumWays) &&
        (pstRI->nRootCPOffs <  nPgsPerBlk))
    {
        pstRI->nRootCPOffs = (UINT16)nPgsPerBlk;
    }

    /*  check if current rootinfo page is last page in super block or not */
    if ((pstRI->nRootCPOffs & (nPgsPerBlk - 1)) == 0)
    {
        /*  check if current rootinfo block is nNumRootBlks or not */
        if ((pstRI->nRootCPOffs >> nPgsSft) == pstRI->nNumRootBlks)
        {
            /*  initialize nRootCPOffs */
            pstRI->nRootCPOffs = 0;

            nVbn = pstRI->nRootStartVbn;
        }
        else
        {
            nVbn = (BADDR)(pstRI->nRootStartVbn + (pstRI->nRootCPOffs >> nPgsSft));
        }

        nRet = FSR_STL_FlashErase(pstZone, nVbn);
        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR]  --%s() L(%d) : 0x%08x - FSR_STL_FlashErase(%d) return error\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));

            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x - FSR_STL_FlashErase(%d) return error\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return nRet;
        }

        /*  Update Root block EC */
        pstRI->aRootBlkEC[pstRI->nRootCPOffs >> nPgsSft]++;
    }

    nVbn = (BADDR)(pstRI->nRootStartVbn + (pstRI->nRootCPOffs >> nPgsSft));
    if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
    {
        nVpn = (nVbn << pstDVI->nPagesPerSbShift)
             + FSR_BML_GetVPgOffOfLSBPg(pstZone->nVolID,
                                        pstRI->nRootCPOffs & (nPgsPerBlk - 1));
    }
    else
    {
        nVpn = (nVbn << pstDVI->nPagesPerSbShift)
             + (pstRI->nRootCPOffs & (nPgsPerBlk - 1));
    }
    pstRI->nRootCPOffs++;

    /*  Meta Confirm : Zero Bit Count */
    nZBCRoot = FSR_STL_GetZBC((UINT8*)&(pstClst->stRootInfoBuf),
                                       sizeof(STLRootInfo));

    /*  set PTF value */
    nPTF = FSR_STL_SetPTF(pstZone,
                          TF_META,
                          MT_ROOT,
                          TF_NULL,
                          TF_NULL);

    pstVFLParam               = FSR_STL_AllocVFLParam(pstZone);
    pstVFLParam->pData        = (UINT8*)&(pstClst->stRootInfoBuf);
    pstVFLParam->bPgSizeBuf   = FALSE32;
    pstVFLParam->bUserData    = FALSE32;
    pstVFLParam->bSpare       = TRUE32;

    pstVFLParam->nSData1      = nZBCRoot;
    pstVFLParam->nSData2      = nZBCRoot ^ 0xFFFFFFFF;
    pstVFLParam->nSData3      = (nPTF << 16) | (nPTF^0xFFFF);
    pstVFLParam->nBitmap      = ROOT_INFO_BUF_BITMAP;
    pstVFLParam->nNumOfPgs    = 1;

    pstVFLParam->pExtParam    = FSR_STL_AllocVFLExtParam(pstZone);
    pstVFLParam->pExtParam->nNumExtSData  = 0;

    do
    {
        nRet = FSR_STL_FlashProgram(pstZone,
                                    nVpn,
                                    pstVFLParam);
        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x - FSR_STL_FlashProgram returns error\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }
        FSR_STL_FlashFlush(pstZone);

        nRet = FSR_STL_SUCCESS;
    } while (0);

    FSR_STL_FreeVFLExtParam(pstZone, pstVFLParam->pExtParam);
    FSR_STL_FreeVFLParam(pstZone, pstVFLParam);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d), : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}

/**
 * @brief           This function stores the BMT & context page to Flash.
 * @n               BMT context information contains a BMT and free block list...
 * @n               To write the page in VFL, it is required to make a buffer.
 *
 * @param[in]       pstZone                 : pointer to atl Zone object
 * @param[in]       bEnableMetaWL           : meta wear-leveling enable flag
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PUBLIC INT32
FSR_STL_StoreBMTCtx(STLZoneObj  *pstZone,
                    BOOL32       bEnableMetaWL)
{
    const   STLCtxInfoHdl  *pstCI           = pstZone->pstCtxHdl;
            STLCtxInfoFm   *pstCIFm         = pstCI->pstFm;
    const   RBWDevInfo     *pstDVI          = pstZone->pstDevInfo;
    const   STLZoneInfo    *pstZI           = pstZone->pstZI;
    const   STLMetaLayout  *pstML           = pstZone->pstML;
    const   STLDirHdrHdl   *pstDH           = pstZone->pstDirHdrHdl;
            STLDirHeaderFm *pstDHFm         = pstDH->pstFm;
    const   STLBMTHdl      *pstBMT          = pstZone->pstBMTHdl;
            UINT8          *pMPgBF          = pstZone->pMetaPgBuf;
            VFLParam       *pstVFLParam;
            BADDR           nHeaderVbn;
            PADDR           nCxtVpn         = NULL_VPN;
            POFFSET         nMetaPOffset    = 0;
            POFFSET         nOldMPOff;
            BADDR           nLan;
            UINT16          nPTF;
            UINT32          nZBCBMT;
            UINT32          nZBCCtx;
    /*  Assign MetaType */
    const   UINT16          nMetaType       = MT_BMT;
            INT32           nRet;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%1d, 0x%1x)\r\n"),
            __FSR_FUNC__, pstZone->nZoneID, bEnableMetaWL));

#if (OP_SUPPORT_META_WEAR_LEVELING == 0)
    bEnableMetaWL = FALSE32;
#endif  /* (OP_SUPPORT_META_WEAR_LEVELING == 0) */

    /* age decrement */
    if (pstCIFm->nAge_Low == 0x00000000)
    {
        pstCIFm->nAge_High -= 1;
        pstCIFm->nAge_Low   = 0xFFFFFFFF;
    }
    else
    {
        pstCIFm->nAge_Low  -= 1;
    }

    /*  set meta reclaim flag */
    pstCIFm->nMetaReclaimFlag = 0;

    /*  set LA number */
    nLan = pstBMT->pstFm->nLan;

    /*  calculate zero bit count */
    nZBCBMT = FSR_STL_GetZBC((UINT8*)pstBMT->pBuf,
                                     pstBMT->nCfmBufSize);

    pstBMT->pstCfm->nZBC    = nZBCBMT;
    pstBMT->pstCfm->nInvZBC = nZBCBMT ^ 0xFFFFFFFF;

    nZBCCtx = FSR_STL_GetZBC((UINT8*)pstCI->pBuf,
                                     pstCI->nCfmBufSize);

    pstCI->pstCfm->nZBC     = nZBCCtx;
    pstCI->pstCfm->nInvZBC  = nZBCCtx ^ 0xFFFFFFFF;

    /*
     * copy BMT of full BMT table to context buffer.
     * occur twice memcpy unnecessarily during format process. 
     */
    FSR_ASSERT(pstBMT->pBuf != pMPgBF);

    FSR_OAM_MEMCPY(pMPgBF,
                   &(pstZone->pFullBMTBuf[pstBMT->pstFm->nLan * pstML->nBMTBufSize]),
                   pstML->nBMTBufSize);

    /*  set PTF value */
    nPTF = FSR_STL_SetPTF(pstZone,
                          TF_META,
                          MT_BMT,
                          TF_NULL,
                          TF_NULL);

    /*  store the STL context to Flash */
    pstVFLParam               = FSR_STL_AllocVFLParam(pstZone);
    pstVFLParam->pData        = (UINT8*)pMPgBF;
    pstVFLParam->bPgSizeBuf   = FALSE32;
    pstVFLParam->bUserData    = FALSE32;
    pstVFLParam->bSpare       = TRUE32;
    pstVFLParam->nSData1      = (nMetaType << 16) | (nMetaType ^ 0xFFFF);
    pstVFLParam->nSData3      = (nPTF      << 16) | (nPTF      ^ 0xFFFF);
    pstVFLParam->nBitmap      = pstML->nBMTCtxSBM;
    pstVFLParam->nNumOfPgs    = 1;
    pstVFLParam->pExtParam    = NULL;

    nHeaderVbn  = pstZI->aMetaVbnList[pstDHFm->nHeaderIdx];
    /*  VPN to write the context */
    if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
    {
        nCxtVpn = (nHeaderVbn << pstDVI->nPagesPerSbShift)
                + FSR_BML_GetVPgOffOfLSBPg(pstZone->nVolID,
                                           pstDHFm->nCPOffs);
    }
    else
    {
        nCxtVpn = (nHeaderVbn << pstDVI->nPagesPerSbShift)
                + pstDHFm->nCPOffs;
    }

    /*  Store BMTCxt page */
    do
    {
        nRet = FSR_STL_FlashProgram(pstZone,
                                    nCxtVpn,
                                    pstVFLParam);
        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - FSR_STL_FlashProgram(%d) returns error\r\n"),
                    __FSR_FUNC__, __LINE__, nRet, nCxtVpn));
            break;
        }
        FSR_STL_FlashFlush(pstZone);

        /* Set new meta index */
        nMetaPOffset = (POFFSET)((pstDHFm->nHeaderIdx << pstDVI->nPagesPerSbShift)
                     + (nCxtVpn & (pstDVI->nPagesPerSBlk - 1)));
        FSR_ASSERT(nMetaPOffset != NULL_POFFSET);

        /* update new VPN of the BMT to directory */
        nOldMPOff = pstDH->pBMTDir[nLan];
        pstDH->pBMTDir[nLan] = nMetaPOffset;

        /* update valid page count of meta units */
        nRet = _UpdateValidPgCnt(pstZone, nOldMPOff, nMetaPOffset, bEnableMetaWL);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - _UpdateValidPgCnt(%d, %d) returns error\r\n"),
                    __FSR_FUNC__, __LINE__, nRet, nOldMPOff, nMetaPOffset));
            break;
        }

        nRet = FSR_STL_SUCCESS;
    } while (0);

    FSR_STL_FreeVFLParam(pstZone, pstVFLParam);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__,__LINE__, nRet));
    return nRet;
}

/**
 * @brief           This function reads STL context pages to construct the data block
 * @n               mapping table and log block mapping table. Also, it scans pages in log blocks
 * @n               to build the log page mapping table.
 *
 * @param[in]       pstZone             : pointer to atl Zone object
 * @param[in]       nLan                : logical area number
 * @param[in]       bOpenFlag           : boolean parameter for open function call
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PUBLIC INT32
FSR_STL_LoadBMT(STLZoneObj *pstZone,
                BADDR       nLan,
                BOOL32      bOpenFlag)
{
    const   STLMetaLayout  *pstML           = pstZone->pstML;
            STLBMTHdl      *pstBMT          = pstZone->pstBMTHdl;
            INT32           nRet            = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%1d, %5d, 0x%1x)\r\n"),
            __FSR_FUNC__, pstZone->nZoneID, nLan, bOpenFlag));

    /*  if the current BMT is the requested LAN, we don't need to load the BMT. */
    if (pstBMT->pstFm->nLan == nLan)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:OUT] --%s() L(%d) : 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, FSR_STL_SUCCESS));
        return FSR_STL_SUCCESS;
    }


    FSR_STL_SetBMTHdl(pstZone,
                      pstBMT,
                      &(pstZone->pFullBMTBuf[nLan * pstML->nBMTBufSize]),
                      pstML->nBMTBufSize);

    /**
     * LAN is discorded because of reading BMT only. 
     * set LAN of ctxinfo by compulsion.
     */
    pstBMT->pstFm->nLan = nLan;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}

/**
 * @brief           This function stores the PMT context page to Flash.
 * @n               PMT context information contains a PMT and free block list...
 * @n               To write the page in VFL, it is required to make a buffer.
 *
 * @param[in]       pstZone             : pointer to atl Zone object
 * @param[in]       pstLogGrp           : pointer to log group
 * @param[in]       bEnableMetaWL       : meta wear-leveling enable flag
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PUBLIC INT32
FSR_STL_StorePMTCtx    (STLZoneObj     *pstZone,
                        STLLogGrpHdl   *pstLogGrp,
                        BOOL32          bEnableMetaWL)
{
    const   RBWDevInfo     *pstDVI          = pstZone->pstDevInfo;
    const   STLZoneInfo    *pstZI           = pstZone->pstZI;
    const   STLMetaLayout  *pstML           = pstZone->pstML;
    const   STLCtxInfoHdl  *pstCI           = pstZone->pstCtxHdl;
            STLCtxInfoFm   *pstCIFm         = pstCI->pstFm;
    const   STLDirHdrHdl   *pstDH           = pstZone->pstDirHdrHdl;
            STLDirHeaderFm *pstDHFm         = pstDH->pstFm;
            STLPMTHdl      *pstPMT          = pstZone->pstPMTHdl;
            UINT8          *pMPgBF          = pstZone->pMetaPgBuf;
            UINT8          *pTmpMPgBF       = pstZone->pTempMetaPgBuf;
    const   UINT16          nBsPLGrp        = (UINT16)pstML->nBytesPerLogGrp;
    const   UINT16          nLGrpPPMT       = pstZI->nNumLogGrpPerPMT;
            VFLParam       *pstVFLParam;
            PADDR           nCxtVpn         = NULL_VPN;
            PADDR           nSrcVpn         = NULL_VPN;
            BADDR           nHeaderVbn;
            POFFSET         nMetaPOffset    = 0;
            POFFSET         nOldMPOff;
            BADDR           nMetaIdx;
            POFFSET         nDgnOffs;
            INT32           nRet;
            UINT32          nIdx            = 0;
            UINT16          nPTF;
            UINT32          nZBCPMT;
            UINT32          nZBCCtx;
    /*  Assign MetaType */
    const   UINT16          nMetaType       = MT_PMT;
            UINT16          nStartOffs;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%1d, %5d, 0x%1x)\r\n"),
            __FSR_FUNC__, pstZone->nZoneID, pstLogGrp->pstFm->nDgn, bEnableMetaWL));

#if (OP_SUPPORT_META_WEAR_LEVELING == 0)
    bEnableMetaWL   = FALSE32;
#endif  /* (OP_SUPPORT_META_WEAR_LEVELING == 0) */

    /* Check parameter of LogGrp address */
    if ((pstLogGrp->pBuf >= pTmpMPgBF) && 
        (pstLogGrp->pBuf <= &pTmpMPgBF[pstML->nBMTBufSize + pstML->nCtxBufSize + pstML->nPMTBufSize]))
    {
        FSR_ASSERT(FALSE32);
    }

#if (OP_STL_DEBUG_CODE == 1)
    if ((pstCIFm->stUpdatedPMTWLGrp.nDgn != NULL_DGN) &&
        (pstCIFm->stUpdatedPMTWLGrp.nDgn >=
         ((pstLogGrp->pstFm->nDgn >> DEFAULT_PMT_EC_GRP_SIZE_SHIFT) + 1)
            << DEFAULT_PMT_EC_GRP_SIZE_SHIFT)         ||
        (pstCIFm->stUpdatedPMTWLGrp.nDgn <
         ((pstLogGrp->pstFm->nDgn >> DEFAULT_PMT_EC_GRP_SIZE_SHIFT)
            << DEFAULT_PMT_EC_GRP_SIZE_SHIFT)))
    {
        FSR_ASSERT(FALSE32);
    }
#endif

    /* age decrement */
    if (pstCIFm->nAge_Low == 0x00000000)
    {
        pstCIFm->nAge_High -= 1;
        pstCIFm->nAge_Low   = 0xFFFFFFFF;
    }
    else
    {
        pstCIFm->nAge_Low  -= 1;
    }

    /*  set meta reclaim flag */
    pstCIFm->nMetaReclaimFlag = 0;

    /* calculate zero bit count */
    nZBCPMT = FSR_STL_GetZBC(pstLogGrp->pBuf, pstLogGrp->nCfmBufSize);

    pstLogGrp->pstCfm->nZBC    = nZBCPMT;
    pstLogGrp->pstCfm->nInvZBC = nZBCPMT ^ 0xFFFFFFFF;

    nZBCCtx = FSR_STL_GetZBC(pstCI->pBuf, pstCI->nCfmBufSize);

    pstCI->pstCfm->nZBC    = nZBCCtx;
    pstCI->pstCfm->nInvZBC = nZBCCtx ^ 0xFFFFFFFF;

    nDgnOffs      = pstLogGrp->pstFm->nDgn % nLGrpPPMT;
    nStartOffs    = nDgnOffs * nBsPLGrp;

    /*  set PTF value */
    nPTF = FSR_STL_SetPTF(pstZone,
                          TF_META,
                          MT_PMT,
                          TF_NULL,
                          TF_NULL);

    pstVFLParam     = FSR_STL_AllocVFLParam(pstZone);

    /* set Extended Parameter */
    pstVFLParam->pExtParam    = FSR_STL_AllocVFLExtParam(pstZone);

    /*  store the PMT context to Flash */
    pstVFLParam->pData        = (UINT8*)(pstCI->pBuf);
    pstVFLParam->bPgSizeBuf   = FALSE32;
    pstVFLParam->bUserData    = FALSE32;
    pstVFLParam->bSpare       = TRUE32;
    pstVFLParam->nSData1      = (nMetaType << 16) | (nMetaType ^ 0xFFFF);
    pstVFLParam->nSData3      = (nPTF      << 16) | (nPTF      ^ 0xFFFF);
    pstVFLParam->nBitmap      = pstML->nCtxPMTSBM;
    pstVFLParam->nNumOfPgs    = 1;

    /*  VPN to write the context */
    nHeaderVbn = pstZI->aMetaVbnList[pstDHFm->nHeaderIdx];
    if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
    {
        nCxtVpn = (nHeaderVbn << pstDVI->nPagesPerSbShift)
                + FSR_BML_GetVPgOffOfLSBPg(pstZone->nVolID,
                                           pstDHFm->nCPOffs);
    }
    else
    {
        nCxtVpn = (nHeaderVbn << pstDVI->nPagesPerSbShift)
                + pstDHFm->nCPOffs;
    }

    /* search the VPN and find out the previous meta block index */
    FSR_STL_SearchMetaPg(pstZone,
                         pstLogGrp->pstFm->nDgn,
                         &nOldMPOff);

    if (nOldMPOff != NULL_POFFSET)
    {
        nMetaIdx = (BADDR)(nOldMPOff >> pstDVI->nPagesPerSbShift);
        FSR_ASSERT(nMetaIdx < pstZI->nNumMetaBlks);
        nSrcVpn  = (pstZI->aMetaVbnList[nMetaIdx] << pstDVI->nPagesPerSbShift)
                 + (nOldMPOff & (pstDVI->nPagesPerSBlk - 1));

        /* set Extended Parameter */
        pstVFLParam->pExtParam->nNumExtSData = 0;
        pstVFLParam->pExtParam->nNumRndIn    = 2;

        FSR_STL_SetRndInArg(pstVFLParam->pExtParam,
                            0,
                            (UINT16)pstML->nCtxBufSize,
                            (VOID*)pstCI->pBuf,
                            0);
        FSR_STL_SetRndInArg(pstVFLParam->pExtParam,
                            (UINT16)(pstML->nCtxBufSize + nStartOffs),
                            nBsPLGrp,
                            (VOID*)pstLogGrp->pBuf,
                            1);

        /* set source vpn & destination vpn for modify-copyback */
        pstVFLParam->pExtParam->nSrcVpn = nSrcVpn;
        pstVFLParam->pExtParam->nDstVpn = nCxtVpn;

        /* In case that there is pMetaPgBuf */
        if (pstLogGrp->pstFm->nDgn == pstPMT->astLogGrps[nDgnOffs].pstFm->nDgn)
        {
            /* check range of pstLogGrp address */
            if ((pstLogGrp->pBuf <  pMPgBF) ||
                (pstLogGrp->pBuf >= &(pMPgBF[pstML->nBMTBufSize + pstML->nCtxBufSize + pstML->nPMTBufSize])))
            {
                FSR_OAM_MEMCPY(pstPMT->astLogGrps[nDgnOffs].pBuf,
                               pstLogGrp->pBuf,
                               nBsPLGrp);
            }
        }

        /* initialize bitmap because of not using BML */
        pstVFLParam->nBitmap = 0x00000000;
    }
    else
    {
        if ((pstLogGrp->pBuf >= pMPgBF) && 
            (pstLogGrp->pBuf <  &(pMPgBF[pstML->nBMTBufSize + pstML->nCtxBufSize + pstML->nPMTBufSize])))
        {
            FSR_ASSERT(FALSE32);
        }

        nOldMPOff = NULL_POFFSET;

        for (nIdx = 0; nIdx < pstZI->nNumLogGrpPerPMT; nIdx++)
        {
            /* initialize all log group objects in PMT buffer */
            FSR_STL_InitLogGrp(pstZone, &(pstPMT->astLogGrps[nIdx]));
        }

        /* copy the log group contents */
        FSR_OAM_MEMCPY(pstPMT->pBuf + nStartOffs,
                       pstLogGrp->pBuf,
                       pstLogGrp->nBufSize);

        /* set Extended Parameter */
        pstVFLParam->pExtParam->nNumRndIn       = 0;
        pstVFLParam->pExtParam->nNumExtSData    = 0;
    }

    /*  store PMTCxt page to NAND flash */
    do
    {
        if (pstVFLParam->nBitmap == pstDVI->nFullSBitmapPerVPg)
        {
            nRet = FSR_STL_FlashProgram(pstZone,
                                        nCxtVpn,
                                        pstVFLParam);
        }
        else
        {
            nRet = FSR_STL_FlashModiCopyback(pstZone,
                                             nSrcVpn,
                                             nCxtVpn,
                                             pstVFLParam);
        }
        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - FSR_STL_FlashModiCopyback returns error\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }
        FSR_STL_FlashFlush(pstZone);

        /* Set new meta index */
        nMetaPOffset = (POFFSET)((pstDHFm->nHeaderIdx << pstDVI->nPagesPerSbShift)
                     + (nCxtVpn & (pstDVI->nPagesPerSBlk - 1)));
        FSR_ASSERT(nMetaPOffset != NULL_POFFSET);

        FSR_STL_UpdatePMTDir(pstZone,
                             pstLogGrp->pstFm->nDgn,
                             nMetaPOffset,
                             pstLogGrp->pstFm->nMinEC,
                             pstLogGrp->pstFm->nMinVbn,
                             FALSE32);

        nRet = _UpdateValidPgCnt(pstZone, nOldMPOff, nMetaPOffset, bEnableMetaWL);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - _UpdateValidPgCnt(%d, %d) returns error\r\n"),
                    __FSR_FUNC__, __LINE__, nRet, nOldMPOff, nMetaPOffset));
            break;
        }

        nRet = FSR_STL_SUCCESS;
    } while (0);


    FSR_STL_FreeVFLExtParam(pstZone, pstVFLParam->pExtParam);
    FSR_STL_FreeVFLParam(pstZone, pstVFLParam);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}

/**
 * @brief           This function reads PMT pages to construct the data block
 * @n               mapping table and log block mapping table. Also, it scans pages in log blocks
 * @n               to build the active log page mapping table.
 *
 * @param[in]       pstZone             : pointer to atl Zone object
 * @param[in]       nDgn                : data group number
 * @param[in]       nMetaPOffs          : meta page offset
 * @param[out]      pstLogGrp           : pointer to log group
 * @param[in]       bOpenFlag           : boolean parameter for open function call
 *
 * @return          FSR_STL_SUCCESS
 *
 * @author          Jongtae Park
 * @version         1.2.0
 *
 */
PUBLIC INT32
FSR_STL_LoadPMT    (STLZoneObj     *pstZone,
                    BADDR           nDgn,
                    POFFSET         nMetaPOffs,
                    STLLogGrpHdl  **pstLogGrp,
                    BOOL32          bOpenFlag)
{
    const   RBWDevInfo     *pstDVI          = pstZone->pstDevInfo;
    const   STLZoneInfo    *pstZI           = pstZone->pstZI;
    const   STLMetaLayout  *pstML           = pstZone->pstML;
    const   UINT32          nPgsPerSBlk     = pstDVI->nPagesPerSBlk;
    const   UINT32          nPgsSft         = pstDVI->nPagesPerSbShift;
            STLPMTHdl      *pstPMT          = pstZone->pstPMTHdl;
            STLLogGrpHdl   *pstDstLogGrp    = *pstLogGrp;
            STLLogGrpHdl   *pstSrcLopGrp;
            VFLParam       *pstVFLParam;
            BADDR           nVbn;
            PADDR           nVpn;
            UINT32          nZBCPMT;
            INT32           nRet;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%1d, %5d, %4d, 0x%08x, 0x%01x)\r\n"),
            __FSR_FUNC__, pstZone->nZoneID, nDgn, nMetaPOffs, pstLogGrp, bOpenFlag));

    pstSrcLopGrp = &(pstPMT->astLogGrps[nDgn % pstZI->nNumLogGrpPerPMT]);

    /* if LogGrp exists already in MetaPgBuf, destination LogGrp is Source LogGrp. */ 
    if (((*pstLogGrp == NULL) || ((*pstLogGrp)->pBuf == pstSrcLopGrp->pBuf)) &&
        (pstSrcLopGrp->pstFm->nDgn == nDgn))
    {
        (*pstLogGrp) = pstSrcLopGrp;
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"),
                __FSR_FUNC__, FSR_STL_SUCCESS));
        return FSR_STL_SUCCESS;
    }

    nVbn = pstZI->aMetaVbnList[nMetaPOffs >> nPgsSft];
    nVpn = (nVbn << nPgsSft) + (nMetaPOffs & (nPgsPerSBlk - 1));

    pstVFLParam               = FSR_STL_AllocVFLParam(pstZone);
    pstVFLParam->pData        = (UINT8*)pstPMT->pBuf;
    pstVFLParam->bPgSizeBuf   = FALSE32;
    pstVFLParam->bUserData    = FALSE32;
    pstVFLParam->bSpare       = TRUE32;
    pstVFLParam->nBitmap      = pstML->nPMTSBM;
    pstVFLParam->nNumOfPgs    = 1;
    pstVFLParam->pExtParam    = NULL;

    do
    {
        nRet = FSR_STL_FlashRead(pstZone, nVpn, pstVFLParam, FALSE32);
        if (nRet == FSR_BML_READ_ERROR)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d) : 0x%08x - U_ECC(%d)\r\n"),
                    __FSR_FUNC__, __LINE__, nRet, nVpn));
            break;
        }
        else if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d) : 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        /* calculate zero bit count for PMT confirm */
        if (bOpenFlag == TRUE32)
        {
            nZBCPMT  = FSR_STL_GetZBC(pstSrcLopGrp->pBuf,
                                      pstSrcLopGrp->nCfmBufSize);

            if ((pstSrcLopGrp->pstCfm->nZBC    != nZBCPMT) ||
                (pstSrcLopGrp->pstCfm->nInvZBC != (nZBCPMT ^ 0xFFFFFFFF)))
            {
                nRet = FSR_STL_META_BROKEN;
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d) : 0x%08x - ZBit_ECC(%d)\r\n"),
                        __FSR_FUNC__, __LINE__, nRet, nVpn));
                break;
            }
        }

        if (pstDstLogGrp == NULL)
        {
            pstDstLogGrp = pstSrcLopGrp;
        }
        else if (pstSrcLopGrp->pBuf != pstDstLogGrp->pBuf)
        {
            FSR_OAM_MEMCPY(pstDstLogGrp->pBuf, pstSrcLopGrp->pBuf, pstML->nBytesPerLogGrp);
        }

        *pstLogGrp = pstDstLogGrp;

        (*pstLogGrp)->pPrev = NULL;
        (*pstLogGrp)->pNext = NULL;

        FSR_ASSERT(pstDstLogGrp->pstFm->nDgn == nDgn);

        nRet = FSR_STL_SUCCESS;
    } while (0);

    FSR_STL_FreeVFLParam(pstZone, pstVFLParam);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}

