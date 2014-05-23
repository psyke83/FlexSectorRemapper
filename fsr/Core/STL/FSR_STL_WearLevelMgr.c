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
 *  @file       FSR_STL_WearLevelMgr.c
 *  @brief      This file contains function related Wear-Leveling.
 *  @author     Kangho Roh
 *  @date       23-MAY-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  23-MAY-2007 [Wonmoon Cheon] : first writing
 *  @n  02-JUL-2007 [Kangho Roh]    : first writing
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
typedef struct {
    UINT32      nFBlkIdx;
    BADDR       nTrgVbn;
    BADDR       nMinVbn;
    UINT32      nTrgEC;
    UINT32      nMinEC;
}   StlWLArg;

/*****************************************************************************/
/* Local constant definitions                                                */
/*****************************************************************************/

/* For global wear-leveling */
#define MASTER_ZONE_ID                  (0)
#define GLOBAL_WL_START                 (1)
#define GLOBAL_WL_END                   (2)

/* For PMT wear-leveling */
#define STL_VPN_MAPPED                  (0x01)

/*****************************************************************************/
/* Local (static) function prototype                                         */
/*****************************************************************************/

#if (OP_SUPPORT_DATA_WEAR_LEVELING == 1)

PRIVATE BOOL32  _FindMinEC         (STLZoneObj *pstZone,
                                    UINT32     *pnMinEC,
                                    BOOL32     *pbMinPMT);

PRIVATE INT32   _WearLevelBMT      (STLZoneObj *pstZone,
                                    StlWLArg   *pstWL,
                                    BOOL32     *pbLoadBMT);

PRIVATE INT32   _WearLevelPMT      (STLZoneObj *pstZone,
                                    StlWLArg   *pstWL,
                                    BOOL32     *pbLoadPMT);

#if (OP_SUPPORT_GLOBAL_WEAR_LEVELING == 1)

PRIVATE INT32   _WearLevelGlobal   (STLZoneObj *pstOrgZone,
                                    UINT32      nNumWLBlk,
                                    BOOL32     *pbLoadBMT,
                                    BOOL32     *pbLoadPMT);

#else   /* (OP_SUPPORT_GLOBAL_WEAR_LEVELING == 0) */

PRIVATE INT32   _WearLevelLocal    (STLZoneObj *pstZone,
                                    UINT32      nNumWLBlk,
                                    BOOL32     *pbLoadBMT,
                                    BOOL32     *pbLoadPMT);

#endif  /* (OP_SUPPORT_GLOBAL_WEAR_LEVELING == 1) */

#endif  /* (OP_SUPPORT_DATA_WEAR_LEVELING == 1) */

/*****************************************************************************/
/* Local (static)  Function Definition                                       */
/*****************************************************************************/

#if (OP_SUPPORT_DATA_WEAR_LEVELING == 1)

/**
 *  @brief      This function returns minimum erase count in the zone.
 *
 *  @param[in]  pstZone         : zone object pointer
 *  @param[out] pnMinEC         : the found minimum erase count
 *  @param[out] pbMinPMT        : the type of found blk
 *
 *  @retval     TRUE32  : if the erase counter is smaller
 *  @retval     FALSE32 : other case
 */
PRIVATE BOOL32
_FindMinEC     (STLZoneObj *pstZone,
                UINT32     *pnMinEC,
                BOOL32     *pbMinPMT)
{
    const STLCtxInfoHdl    *pstCtx      = pstZone->pstCtxHdl;
    const STLDirHdrHdl     *pstDirHdr   = pstZone->pstDirHdrHdl;
          BOOL32            bFind       = FALSE32;
          BADDR             nLan;
          POFFSET           nPOff;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /*
     * if pCtx->aFBlksEC[pCxt->nFBlkHead] becomes 0xFFFFFFFF
     * which is the maximum value can be expressed by 32bit integer,
     * we do not execute wear-level.
     * We assume that the erase count does not exceed 0xFFFFFFFF
     * because the life limit of NAND flash...
     */
    nPOff = pstDirHdr->pstFm->nMinECPMTIdx;
    if ((nPOff != NULL_POFFSET) &&
        (pstDirHdr->pstPMTWLGrp[nPOff].nMinEC < *pnMinEC))
    {
        *pbMinPMT = TRUE32;
        *pnMinEC  = pstDirHdr->pstPMTWLGrp[nPOff].nMinEC;
         bFind    = TRUE32;
    }

    /* find the EC of an LA which has minimum EC */
    nLan = pstCtx->pstFm->nMinECLan;
    if ((nLan < pstZone->pstZI->nNumLA) &&
        (pstCtx->pMinEC[nLan] < *pnMinEC))
    {
        *pbMinPMT = FALSE32;
        *pnMinEC  = pstCtx->pMinEC[nLan];
         bFind    = TRUE32;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT] --%s() : 0x%08x\r\n"), __FSR_FUNC__, bFind));
    return bFind;
}

/**
 *  @brief          This function does data wear leveling for BMT when the
 *  @n              trigger condition meets. ( |FBlk_EC - min_EC| > EC_THRESHOLD )
 *
 *  @param[in]      pstZone     : STLZoneObj pointer object
 *  @param[in,out]  pstWL       : StlWLArg pointer object
 *  @param[out]     pbLoadBMT   : if this value is true, we must reload the BMT because the BMT buffer has been changed
 *
 *  @return         FSR_STL_SUCCESS
 *
 *  @author         Kangho Roh
 *  @version        1.2.0
 */
PRIVATE INT32
_WearLevelBMT      (STLZoneObj *pstZone,
                    StlWLArg   *pstWL,
                    BOOL32     *pbLoadBMT)
{
    const STLCtxInfoHdl    *pstCtx          = pstZone->pstCtxHdl;
    const RBWDevInfo       *pstDev          = pstZone->pstDevInfo;
    const UINT32            nPgsPerBlk      = pstDev->nPagesPerSBlk >> pstDev->nNumWaysShift;
    const UINT32            nMaxFBlk        = pstZone->pstML->nMaxFreeSlots;
    const BADDR             nOrgLan         = pstZone->pstBMTHdl->pstFm->nLan;
    const UINT32            nFBlkIdx        = pstWL->nFBlkIdx;
    const BADDR             nTrgVbn         = pstWL->nTrgVbn;
    const UINT32            nTrgEC          = pstWL->nTrgEC;
          STLBMTHdl        *pstBMT;
    const BADDR             nMinLan         = pstCtx->pstFm->nMinECLan;
    const UINT32            nMinMap         = pstCtx->pMinECIdx[nMinLan];
          BADDR             nMinVbn;
          UINT32            nMinEC;
          UINT32            nBlk;
          INT32             nRet            = FSR_STL_SUCCESS;
          STLClstObj       *pstClst         = FSR_STL_GetClstObj(pstZone->nClstID);
    const UINT32            n1stVun         = pstClst->pstEnvVar->nStartVbn;
          BMLCpBkArg      **ppstBMLCpBk     = pstClst->pstBMLCpBk;
          BMLCpBkArg       *pstCpBkArg      = pstClst->staBMLCpBk;
          UINT32            nWay;
          UINT32            nOff;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    FSR_ASSERT(nTrgVbn != NULL_VBN);

    /* Initialize for BMT wear-leveling */
    do
    {
        /* First, reserve meta page */
        if (nFBlkIdx < nMaxFBlk)
        {
            nRet = FSR_STL_ReserveMetaPgs(pstZone, 1, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                break;
            }
        }

        /* If current BMT is not the BMT of minimum EC */
        if (nOrgLan != nMinLan)
        {
            *pbLoadBMT = TRUE32;

            /* pstBMT : LoadBMT with nMinLan.*/
            nRet = FSR_STL_LoadBMT(pstZone, nMinLan, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                break;
            }
        }
        pstBMT  = pstZone->pstBMTHdl;

        /*Get the VBN of a data block which has minimum EC in the current LA.*/
        nMinVbn = pstBMT->pMapTbl[nMinMap].nVbn;
        nMinEC  = pstBMT->pDBlksEC[nMinMap];
        FSR_ASSERT(nMinVbn != NULL_VBN);
        FSR_ASSERT(nTrgVbn != nMinVbn );
        FSR_ASSERT(nMinEC  == pstWL->nMinEC);

        /* Check whether the nMinVbn is garbage blk */
        if ((pstBMT->pGBlkFlags[nMinMap >> 3] & (1 << (nMinMap & 7))) == 0)
        {
            /*Erase the free block to copy data in the current data block.*/
            nRet = FSR_STL_FlashErase(pstZone, nTrgVbn);
            if (nRet != FSR_BML_SUCCESS)
            {
                break;
            }

            /* Copy every pages to the free block.*/
            nOff = 0;
            for (nBlk = 0; nBlk < nPgsPerBlk; nBlk++)
            {
                for (nWay = 0; nWay < pstDev->nNumWays; nWay++)
                {
                    pstCpBkArg[nWay].nSrcVun        = (UINT16)(nMinVbn + n1stVun);
                    pstCpBkArg[nWay].nSrcPgOffset   = (UINT16)(nOff);
                    pstCpBkArg[nWay].nDstVun        = (UINT16)(nTrgVbn + n1stVun);
                    pstCpBkArg[nWay].nDstPgOffset   = (UINT16)(nOff);
                    pstCpBkArg[nWay].nRndInCnt      = 0;
                    pstCpBkArg[nWay].pstRndInArg    = NULL;

                    ppstBMLCpBk[nWay] = &(pstCpBkArg[nWay]);

                    nOff++;
                }

#if defined (FSR_POR_USING_LONGJMP)
                FSR_FOE_BeginWriteTransaction(0);
#endif
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
            }
#if (OP_SUPPORT_MSB_PAGE_WAIT == 1)
            for (nWay = 0; nWay < pstDev->nNumWays; nWay++)
            {
                pstClst->baMSBProg[nWay] = TRUE32;
            }
#endif
        }

        /* Update meta information */

        /* Update BMT using nTagVbn */
        pstBMT->pMapTbl[nMinMap].nVbn = nTrgVbn;
        pstBMT->pDBlksEC[nMinMap]     = nTrgEC + 1;

        /* Update CTX using nMinVbn */
        if (nFBlkIdx < nMaxFBlk)
        {
            FSR_ASSERT(pstCtx->pFreeList[nFBlkIdx] == nTrgVbn);
            FSR_ASSERT(pstCtx->pFBlksEC[nFBlkIdx]  == nTrgEC );
            pstCtx->pFreeList[nFBlkIdx] = nMinVbn;
            pstCtx->pFBlksEC[nFBlkIdx]  = nMinEC;
        }
        else
        {
            pstWL->nMinVbn  = nMinVbn;
            FSR_ASSERT(pstWL->nMinEC == nMinEC);
        }

        /* Recalculation Minimum EC */
        FSR_STL_UpdateBMTMinEC(pstZone);

        /* Store the BMT Context. */
        nRet = FSR_STL_StoreBMTCtx(pstZone, FALSE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }

    } while(0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}

/**
 *  @brief          This function does data wear leveling for PMT when the
 *  @n              trigger condition meets. ( |FBlk_EC - min_EC| > EC_THRESHOLD )
 *
 *  @param[in]      pstZone     : STLZoneObj pointer object
 *  @param[in,out]  pstWL       : StlWLArg pointer object
 *  @param[out]     pbLoadBMT   : if this value is true, we must reload the BMT because the PMT buffer has been changed
 *
 *  @return         FSR_STL_SUCCESS
 *
 *  @author         Kangho Roh
 *  @version        1.2.0
 */
PRIVATE INT32
_WearLevelPMT      (STLZoneObj *pstZone,
                    StlWLArg   *pstWL,
                    BOOL32     *pbLoadPMT)
{
    const STLCtxInfoHdl    *pstCtx      = pstZone->pstCtxHdl;
    const UINT32            nMaxFBlk    = pstZone->pstML->nMaxFreeSlots;
    const RBWDevInfo       *pstDev      = pstZone->pstDevInfo;
    const UINT32            nPgsPerUnit = pstDev->nPagesPerSBlk;
    const UINT32            nPgsSft     = pstDev->nPagesPerSbShift;
    /* Search the DGN and VBN of the block which has minimum erase count */
    const STLDirHdrHdl     *pstDirHdr   = pstZone->pstDirHdrHdl;
    const UINT32            nMinPMTIdx  = pstDirHdr->pstFm->nMinECPMTIdx;
    const STLPMTWLGrp      *pstPMTWLGrp = &(pstDirHdr->pstPMTWLGrp[nMinPMTIdx]);
    const UINT32            nFBlkIdx    = pstWL->nFBlkIdx;
    const BADDR             nTrgVbn     = pstWL->nTrgVbn;
    const UINT32            nTrgEC      = pstWL->nTrgEC;
    const BADDR             nMinDgn     = pstPMTWLGrp->nDgn;
    const BADDR             nMinVbn     = pstPMTWLGrp->nMinVbn;
    const UINT32            nMinEC      = pstPMTWLGrp->nMinEC;
          UINT16            nBlkIdx;
          UINT8            *aValidPgs;
          POFFSET           nMetaPOffs;
          STLLogGrpHdl     *pstLogGrp;
          UINT32            nMinIdx;
          STLLog           *pstMinLog;
          STLActLogEntry   *pstActLog;
          UINT32            nIdx;
          UINT32            nCPOff;
          INT32             nRet        = FSR_STL_SUCCESS;
          STLClstObj       *pstClst     = FSR_STL_GetClstObj(pstZone->nClstID);
    const UINT32            n1stVun     = pstClst->pstEnvVar->nStartVbn;
          BMLCpBkArg      **ppstBMLCpBk = pstClst->pstBMLCpBk;
          BMLCpBkArg       *pstCpBkArg  = pstClst->staBMLCpBk;
          UINT32            nRemainPgs;
          UINT32            naWayIdx[FSR_MAX_WAYS];
          UINT32            nWay;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    FSR_ASSERT(nTrgVbn != NULL_VBN);
    FSR_ASSERT(nMinVbn != NULL_VBN);
    FSR_ASSERT(nTrgVbn != nMinVbn );
    FSR_ASSERT(nMinEC  == pstWL->nMinEC);

    do
    {
        /* First, reserve meta page */
        if (nFBlkIdx < nMaxFBlk)
        {
            nRet = FSR_STL_ReserveMetaPgs(pstZone, 1, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                break;
            }
        }

        /* Search whether the group is active log group or not */
        pstLogGrp = FSR_STL_SearchLogGrp(pstZone->pstActLogGrpList, nMinDgn);
        if (pstLogGrp == NULL)
        {
            /* Search whether the group is inactive log group or not */
            pstLogGrp = FSR_STL_SearchLogGrp(pstZone->pstInaLogGrpCache, nMinDgn);
            if (pstLogGrp == NULL)
            {
                /* Find Meta Page Offset */
                nRet = FSR_STL_SearchPMTDir(pstZone, nMinDgn, &nMetaPOffs);
                if (nRet < 0)
                {
                    nRet = FSR_STL_ERR_PMT;
                    break;
                }
                FSR_ASSERT(nMetaPOffs != NULL_POFFSET);

                /* load the PMT */
                *pbLoadPMT = TRUE32;
                nRet = FSR_STL_LoadPMT(pstZone, nMinDgn, nMetaPOffs, &pstLogGrp, FALSE32);
                if (nRet != FSR_STL_SUCCESS)
                {
                    break;
                }
            }
        }
#if (OP_SUPPORT_RUNTIME_PMT_BUILDING == 1)
        else
        {
            /* Execute Run time scanning */
            nRet = FSR_STL_ScanActLogBlock(pstZone,nMinDgn);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR, 
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
                return nRet;
            }
        }
#endif /*OP_SUPPORT_RUNTIME_PMT_BUILDING*/

        FSR_ASSERT(pstLogGrp != NULL);
        FSR_ASSERT(pstLogGrp->pstFm->nMinVbn == nMinVbn);
        FSR_ASSERT(pstLogGrp->pstFm->nMinEC  == nMinEC);

        /* Find log */
        nMinIdx   = pstLogGrp->pstFm->nMinIdx;
        pstMinLog = &(pstLogGrp->pstLogList[nMinIdx]);
        FSR_ASSERT(pstMinLog->nVbn == nMinVbn);

        /* Erase the free block to copy data in the current log block.*/
        nRet = FSR_STL_FlashErase(pstZone, nTrgVbn);
        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
            return nRet;
        }

        /* find valid pages */
        nBlkIdx   = pstMinLog->nSelfIdx;
        aValidPgs = pstZone->pTempMetaPgBuf;

        /*
         * initialize aValidPgs...
         * Because STL_VPN_MAPPED is 0x01, we initialize this memory buffer to 0x00
         * which means UNMAPPED
         */
        FSR_OAM_MEMSET(aValidPgs, 0x00, nPgsPerUnit);
        nRemainPgs  = 0;
        for (nIdx = 0; nIdx < nPgsPerUnit; nIdx++)
        {
            if ((pstLogGrp->pMapTbl[nIdx] >> nPgsSft) == nBlkIdx)
            {
                aValidPgs[pstLogGrp->pMapTbl[nIdx] & (nPgsPerUnit - 1)] = STL_VPN_MAPPED;
                nRemainPgs++;
            }
        }

        /*
         * Execute Copyback every valid pages
         * from the source block to the destination block that is the free block
         */
        nCPOff  = pstMinLog->nCPOffs;
        for (nWay = 0; nWay < pstDev->nNumWays; nWay++)
        {
            naWayIdx[nWay] = nWay;
        }
        FSR_OAM_MEMSET(ppstBMLCpBk, 0x00, sizeof(long) * FSR_MAX_WAYS);

        nWay = 0;
        while (nRemainPgs != 0)
        {
            while (naWayIdx[nWay] < nCPOff)
            {
                if (aValidPgs[naWayIdx[nWay]] == STL_VPN_MAPPED)
                {
                    pstCpBkArg[nWay].nSrcVun        = (UINT16)(nMinVbn + n1stVun);
                    pstCpBkArg[nWay].nSrcPgOffset   = (UINT16)(naWayIdx[nWay]);
                    pstCpBkArg[nWay].nDstVun        = (UINT16)(nTrgVbn + n1stVun);
                    pstCpBkArg[nWay].nDstPgOffset   = (UINT16)(naWayIdx[nWay]);
                    pstCpBkArg[nWay].nRndInCnt      = 0;
                    pstCpBkArg[nWay].pstRndInArg    = NULL;

                    ppstBMLCpBk[nWay] = &(pstCpBkArg[nWay]);

                    nRemainPgs--;

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

            if ((nWay       == 0) ||
                (nRemainPgs == 0))
            {
#if defined (FSR_POR_USING_LONGJMP)
                FSR_FOE_BeginWriteTransaction(0);
#endif
                nRet = FSR_BML_CopyBack(pstZone->nVolID, ppstBMLCpBk, FSR_BML_FLAG_NONE);
                if (nRet != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR] %s() L(%d) - FSR_BML_CopyBack return error (0x%x)\r\n"), 
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

        /* Update Active Log List */
        if (pstMinLog->nState & (1 << LS_ACTIVE_SHIFT))
        {
            pstActLog = pstCtx->pstActLogList;
            for (nIdx = 0; nIdx < pstCtx->pstFm->nNumLBlks; nIdx++)
            {
                if (pstActLog->nVbn == nMinVbn)
                {
                    pstActLog->nVbn = nTrgVbn;
                    break;
                }
                pstActLog++;
            }
        }

        /* Update PMT using nTagVbn */
        pstMinLog->nVbn     = nTrgVbn;
        pstMinLog->nEC      = nTrgEC + 1;

        /* Update CTX using nMinVbn */
        if (nFBlkIdx < nMaxFBlk)
        {
            FSR_ASSERT(pstCtx->pFreeList[nFBlkIdx] == nTrgVbn);
            FSR_ASSERT(pstCtx->pFBlksEC[nFBlkIdx]  == nTrgEC );
            pstCtx->pFreeList[nFBlkIdx] = nMinVbn;
            pstCtx->pFBlksEC[nFBlkIdx]  = nMinEC;
        }
        else
        {
            pstWL->nMinVbn  = nMinVbn;
            FSR_ASSERT(pstWL->nMinEC == nMinEC);
        }

        /*
         * Recalculation Minimum EC for the pstLogGrp
         * and update minimum erase count
         */

        /* find the minimum DGN which is related with the current WLGrp */
        *pbLoadPMT = TRUE32;
        nRet = FSR_STL_UpdatePMTMinEC(pstZone, pstLogGrp, nMinVbn);
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }

        /* Store the PMT */
        nRet = FSR_STL_StorePMTCtx(pstZone, pstLogGrp, FALSE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}

#if (OP_SUPPORT_GLOBAL_WEAR_LEVELING == 1)

/**
 *   @brief     This function purifies free blocks in the free block list from header to tail.
 *
 *   @param[in]  pstOrgZone     : zone object pointer 
     @param[in]  nNumWLBlk      : the number of block to do Wear-Leveling
 *   @param[out] pbLoadBMT      : Dose this function load BMT?
 *   @param[out] pbLoadPMT      : Dose this function load PMT?
 *
 *   @retval    FSR_STL_SUCCESS : successful
 */
PRIVATE INT32
_WearLevelGlobal   (STLZoneObj *pstOrgZone,
                    UINT32      nNumWLBlk,
                    BOOL32     *pbLoadBMT,
                    BOOL32     *pbLoadPMT)
{
    const UINT32    nWLThreshold    = FSR_STL_GetClstObj(pstOrgZone->nClstID)->pstEnvVar->nECDiffThreshold;
    STLClstObj     *pstClst;
    STLZoneObj     *pstZone;
    STLCtxInfoHdl  *pstCtx;
    STLCtxInfoFm   *pstCtxFm;
    STLCtxInfoFm   *pstZeroCtxFm;
    STLZoneObj     *pstMinZone;
    BOOL32          bMinZone;
    BOOL32          bMinPMT         = TRUE32;
    StlWLArg        stWL;
    UINT32          nOrgEC;
    UINT32          nOrgFBidx;
    UINT32          nMinFBidx       = (UINT32)-1;
    UINT32          nZone;
    UINT32          nMaxFB;
    UINT32          nWLCnt;
    UINT32          nIdx;
    UINT32          nMinEcFIdx;
    UINT32          nMinEcFBlk;
    BOOL32          bLoad;
    INT32           nRet            = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /*
     * if the current number of free blocks is smaller than nNumWLBlk,
     * nNumWLBlk becomes the current number of free blocks
     */
    if (pstOrgZone->pstCtxHdl->pstFm->nNumFBlks < nNumWLBlk)
    {
        nNumWLBlk = pstOrgZone->pstCtxHdl->pstFm->nNumFBlks;
    }

    nWLCnt = 0;
    while (nWLCnt < nNumWLBlk)
    {
        bLoad = FALSE32;

        /* Get the erase count of target free block */
        pstCtx    = pstOrgZone->pstCtxHdl;
        nOrgFBidx = pstCtx->pstFm->nFreeListHead + nWLCnt;
        nMaxFB    = pstOrgZone->pstML->nMaxFreeSlots;
        if (nOrgFBidx >= nMaxFB)
        {
            nOrgFBidx -= nMaxFB;
        }
        FSR_ASSERT(nOrgFBidx < nMaxFB);
        nOrgEC = pstCtx->pFBlksEC[nOrgFBidx];
        if (nOrgEC == (UINT32)(-1))
        {
            nWLCnt++;
            continue;
        }

        /* Get the cluster object */
        pstClst = FSR_STL_GetClstObj(pstOrgZone->nClstID);

        /* First, find minimum erase count in the self zone */
        pstMinZone  = pstOrgZone;
        stWL.nMinEC = (UINT32)(-1);
        bMinZone    = _FindMinEC(pstOrgZone, &(stWL.nMinEC), &bMinPMT);

        /* If there is no idle block and no log block in self partition, _FindMinEC will be able to 
           find minimum EC in PMT and BMT, otherwise _FindMinEC must find it.
           NOTE: The condition that there is no idle block and no log block in a partition 
           is possible when the parition is very small */
        FSR_ASSERT((bMinZone == TRUE32) ||
                   ((bMinZone == FALSE32) &&
                    (pstCtx->pstFm->nNumFBlks == pstOrgZone->pstZI->nNumUserBlks + pstOrgZone->pstZI->nNumFBlks)));        

        if ((bMinZone == FALSE32) &&
            (pstCtx->pstFm->nNumFBlks == pstOrgZone->pstZI->nNumUserBlks + pstOrgZone->pstZI->nNumFBlks))
        {
            return FSR_STL_SUCCESS;
        }

        /* Find minimum erase count among zones */
        for (nZone = 0; nZone < pstOrgZone->pstRI->nNumZone; nZone++)
        {
            if (nZone == pstOrgZone->nZoneID)
                continue;

            pstZone = &(pstClst->stZoneObj[nZone]);

            if (pstZone->bOpened == TRUE32)
            {
                bMinZone = _FindMinEC(pstZone, &stWL.nMinEC, &bMinPMT);
                if (bMinZone == TRUE32)
                {
                    pstMinZone = pstZone;                    
                }
            }
        }        


        /* Erase count overflow */
        if ((stWL.nMinEC >  stWL.nMinEC + nWLThreshold) ||
            (nOrgEC      <= stWL.nMinEC + nWLThreshold))
        {
            nWLCnt++;
            continue;
        }

            /* If the minimum zone is self */
        if (pstMinZone == pstOrgZone)
        {
            /* Use target free block for data wear-level */
            pstCtx        = pstOrgZone->pstCtxHdl;
            stWL.nFBlkIdx = nOrgFBidx;
        }
        else
        {
            /* We must flush delete information
               before local wear-leveling of the minimum zone */
            nRet = FSR_STL_StoreDeletedInfo(pstMinZone);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  %s() L(%d) : 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }

            /* Select free block for data wear-level */
            pstCtx    = pstMinZone->pstCtxHdl;
            pstCtxFm  = pstMinZone->pstCtxHdl->pstFm;

            /* Find the free blk which has the smaller erase count */
            nMinEcFBlk = (UINT32)(-1);
            nMinFBidx  = (UINT32)(-1);
            nMaxFB     = pstMinZone->pstML->nMaxFreeSlots;
            nMinEcFIdx = pstCtxFm->nFreeListHead;
            for (nIdx = 0; nIdx < pstCtxFm->nNumFBlks; nIdx++)
            {
                if (pstCtx->pFBlksEC[nMinEcFIdx] < nMinEcFBlk)
                {
                    nMinEcFBlk = pstCtx->pFBlksEC[nMinEcFIdx];
                    nMinFBidx  = nMinEcFIdx;
                }

                nMinEcFIdx++;
                if (nMinEcFIdx >= nMaxFB)
                {
                    nMinEcFIdx -= nMaxFB;
                }
                FSR_ASSERT(nMinEcFIdx < nMaxFB);
            }
            FSR_ASSERT(nMinFBidx != (UINT32)(-1));

            stWL.nFBlkIdx = nMinFBidx;
        }

        stWL.nTrgVbn = pstCtx->pFreeList[stWL.nFBlkIdx];
        stWL.nTrgEC  = pstCtx->pFBlksEC[stWL.nFBlkIdx];

        if (stWL.nTrgEC > stWL.nMinEC)
        {
            /* Wear-leveling to get the minimum erase blk */
            if (bMinPMT == FALSE32)
            {
                nRet = _WearLevelBMT(pstMinZone, &(stWL), &bLoad);
            }
            else
            {
                nRet = _WearLevelPMT(pstMinZone, &(stWL), &bLoad);
            }
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  %s() L(%d) : 0x%08x - PMT[%d]\r\n"),
                        __FSR_FUNC__, __LINE__, nRet, bMinPMT));
                break;
            }
        }
        else
        {
            stWL.nMinVbn = pstCtx->pFreeList[stWL.nFBlkIdx];
        }
        
        /* Use target free block for data wear-level */
        if (pstMinZone != pstOrgZone)
        {
            FSR_ASSERT(nMinFBidx != (UINT32)-1);

            /* Reserve meta page */
            FSR_ASSERT((pstOrgZone->nZoneID != 0) ||
                       (pstMinZone->nZoneID != 0));
            /* For start and end logging mark */
            nRet = FSR_STL_ReserveMetaPgs(&(pstClst->stZoneObj[MASTER_ZONE_ID]), 2, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  %s() L(%d) : 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }

            /* reserve meta pages */
            if (pstOrgZone->nZoneID != MASTER_ZONE_ID)
            {
                nRet = FSR_STL_ReserveMetaPgs(pstOrgZone, 1, FALSE32);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR]  %s() L(%d) : 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    break;
                }
            }
            if (pstMinZone->nZoneID != MASTER_ZONE_ID)
            {
                nRet = FSR_STL_ReserveMetaPgs(pstMinZone, 1, FALSE32);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR]  %s() L(%d) : 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    break;
                }
            }

            /* Change the target free block with the minimum erase count free block */
            pstZeroCtxFm = pstClst->stZoneObj[MASTER_ZONE_ID].pstCtxHdl->pstFm;

            /* Set target free block */
            pstZeroCtxFm->nZone1Vbn   = pstOrgZone->pstCtxHdl->pFreeList[nOrgFBidx];
            pstZeroCtxFm->nZone1Idx   = (UINT16)pstOrgZone->nZoneID;
            pstZeroCtxFm->nZone1EC    = (UINT32)nOrgEC;

            /* Set the minimum erase count free block */
            pstCtx = pstMinZone->pstCtxHdl;
            pstZeroCtxFm->nZone2Vbn   = pstCtx->pFreeList[nMinFBidx];
            pstZeroCtxFm->nZone2Idx   = (UINT16)pstMinZone->nZoneID;
            pstZeroCtxFm->nZone2EC    = (UINT32)(pstCtx->pFBlksEC[nMinFBidx]);

            /* Exchange free blocks */
            pstCtx->pFreeList[nMinFBidx] = pstZeroCtxFm->nZone1Vbn;
            pstCtx->pFBlksEC[nMinFBidx]  = pstZeroCtxFm->nZone1EC;
            pstCtx = pstOrgZone->pstCtxHdl;
            pstCtx->pFreeList[nOrgFBidx] = pstZeroCtxFm->nZone2Vbn;
            pstCtx->pFBlksEC[nOrgFBidx]  = pstZeroCtxFm->nZone2EC;

            /* Write start log */
            pstZeroCtxFm->nZoneWLMark = GLOBAL_WL_START;
            nRet = FSR_STL_StoreBMTCtx(&(pstClst->stZoneObj[MASTER_ZONE_ID]), FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  %s() L(%d) : 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }

            /* Write target zone */
            if (pstOrgZone->nZoneID != MASTER_ZONE_ID)
            {
                nRet = FSR_STL_StoreBMTCtx(pstOrgZone, FALSE32);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR]  %s() L(%d) : 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    break;
                }
            }

            /* Write the minimum zone */
            if (pstMinZone->nZoneID != MASTER_ZONE_ID)
            {
                nRet = FSR_STL_StoreBMTCtx(pstMinZone, FALSE32);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR]  %s() L(%d) : 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                    break;
                }
            }
            /* Write end log */
            pstZeroCtxFm->nZoneWLMark = GLOBAL_WL_END;
            nRet = FSR_STL_StoreBMTCtx(&(pstClst->stZoneObj[MASTER_ZONE_ID]), FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  %s() L(%d) : 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }

            /* reset ctx info */
            pstZeroCtxFm->nZoneWLMark = (UINT16)(-1);

            pstZeroCtxFm->nZone1Vbn   = (UINT16)(-1);
            pstZeroCtxFm->nZone1Idx   = (UINT16)(-1);
            pstZeroCtxFm->nZone1EC    = (UINT32)(-1);

            pstZeroCtxFm->nZone2Vbn   = (UINT16)(-1);
            pstZeroCtxFm->nZone2Idx   = (UINT16)(-1);
            pstZeroCtxFm->nZone2EC    = (UINT32)(-1);
        }
        else
        {
            if (bLoad == TRUE32)
            {
                if (bMinPMT == TRUE32)
                {
                    *pbLoadPMT = TRUE32;
                }
                *pbLoadBMT = TRUE32;
            }
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}

#else   /* (OP_SUPPORT_GLOBAL_WEAR_LEVELING == 0) */

/**
 *   @brief      This function purifies free blocks in the free block list from header to tail.
 *
 *   @param[in]  pstOrgZone     : zone object pointer 
     @param[in]  nNumWLBlk      : the number of block to do Wear-Leveling
 *   @param[out] pbLoadBMT      : Dose this function load BMT?
 *   @param[out] pbLoadPMT      : Dose this function load PMT?
 *
 *   @retval    FSR_STL_SUCCESS : successful
 */
PRIVATE INT32
_WearLevelLocal    (STLZoneObj *pstZone,
                    UINT32      nNumWLBlk,
                    BOOL32     *pbLoadBMT,
                    BOOL32     *pbLoadPMT)
{
    const UINT32    nWLThreshold    = FSR_STL_GetClstObj(pstOrgZone->nClstID)->pstEnvVar->nECDiffThreshold;
    STLCtxInfoHdl  *pstCtx          = pstZone->pstCtxHdl;
    STLCtxInfoFm   *pstCtxFm        = pstCtx->pstFm;
    const UINT32    nMaxFB          = pstZone->pstML->nMaxFreeSlots;
    StlWLArg        stWL;
    BOOL32          bMinPMT;
    BOOL32          bFindMin;
    UINT32          nWLCnt;
    BOOL32          bLoad;
    INT32           nRet            = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    if (pstCtxFm->nNumFBlks < nNumWLBlk)
    {
        nNumWLBlk = pstCtxFm->nNumFBlks;
    }

    nWLCnt = 0;
    while (nWLCnt < nNumWLBlk)
    {
        bLoad = FALSE32;

        /* Get the erase count of target free block */
        stWL.nFBlkIdx = pstCtxFm->nFreeListHead + nWLCnt;
        if (stWL.nFBlkIdx >= nMaxFB)
        {
            FSR_ASSERT(stWL.nFBlkIdx < (nMaxFB << 1));
            stWL.nFBlkIdx -= nMaxFB;
        }
        stWL.nTrgEC    = pstCtx->pFBlksEC[stWL.nFBlkIdx];
        if (stWL.nTrgEC == (UINT32)(-1))
        {
            nWLCnt++;
            continue;
        }

        /* First, find minimum erase count in the self zone */
        stWL.nMinEC = (UINT32)(-1);
        bFindMin    = _FindMinEC(pstZone, &(stWL.nMinEC), &bMinPMT);
        FSR_ASSERT(bFindMin == TRUE32);

        /* Erase count overflow */
        if ((stWL.nMinEC >  stWL.nMinEC + nWLThreshold) ||
            (stWL.nTrgEC <= stWL.nMinEC + nWLThreshold))
        {
            nWLCnt++;
            continue;
        }

        stWL.nTrgVbn = pstCtx->pFreeList[stWL.nFBlkIdx];
        FSR_ASSERT(stWL.nTrgVbn != NULL_VBN);

        /* Wear-leveling to get the minimum erase blk */
        if (bMinPMT == FALSE32)
        {
            nRet = _WearLevelBMT(pstZone, &(stWL), &bLoad);
        }
        else
        {
            nRet = _WearLevelPMT(pstZone, &(stWL), &bLoad);
        }
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }

        if (bLoad == TRUE32)
        {
            if (bMinPMT == TRUE32)
                *pbLoadPMT = TRUE32;
            else
                *pbLoadBMT = TRUE32;
        }
        nWLCnt++;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}

#endif  /* (OP_SUPPORT_GLOBAL_WEAR_LEVELING == 1) */

#endif  /* (OP_SUPPORT_DATA_WEAR_LEVELING == 1) */


/*****************************************************************************/
/* Global Function Definition                                                */
/*****************************************************************************/

#if (OP_SUPPORT_DATA_WEAR_LEVELING == 1)
/**
 *  @brief          This function updates the Minimum EC value for the BMT
 *
 *  @param[in]      pstZone     : STLZoneObj pointer object
 *
 *  @return         FSR_STL_SUCCESS
 *
 *  @author         Kangho Roh
 *  @version        1.2.0 
 */
PUBLIC VOID
FSR_STL_UpdateBMTMinEC (STLZoneObj *pstZone)
{
    STLCtxInfoHdl  *pstCI   = pstZone->pstCtxHdl;
    STLZoneInfo    *pstZI   = pstZone->pstZI;
    STLBMTHdl      *pstBMT  = pstZone->pstBMTHdl;
    const   UINT32  nLan    = pstBMT->pstFm->nLan;
    UINT32          nNumLA;
    UINT32          nNumDBlks;
    UINT32          nMinIdx;
    UINT32          nMinEC;
    UINT32          nIdx;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* phase1. update the minEC in the LA that contains it. */
    if (nLan == (UINT32)(pstZI->nNumLA - 1))
    {
        /* the last LA */
        FSR_ASSERT(pstZI->nNumUserBlks > 0);
        nNumDBlks = ((pstZI->nNumUserBlks - 1) & (pstZI->nDBlksPerLA - 1)) + 1;
    }
    else
    {
        nNumDBlks = pstZI->nDBlksPerLA;
    }

    nMinIdx = (UINT32)(-1);
    nMinEC  = (UINT32)(-1);
    for (nIdx = 0; nIdx < nNumDBlks; nIdx++)
    {
        if (pstBMT->pMapTbl[nIdx].nVbn == NULL_VBN)
        {
            continue;
        }

        if (pstBMT->pDBlksEC[nIdx] < nMinEC)
        {
            nMinIdx = nIdx;
            nMinEC  = pstBMT->pDBlksEC[nIdx];
        }
    }

    /* Update Min EC of the LA */
    pstCI->pMinEC   [nLan] = nMinEC;
    pstCI->pMinECIdx[nLan] = (UINT16)nMinIdx;

    /* phase2. update the location of global min. EC block */
    nNumLA  = pstZI->nNumLA;
    nMinIdx = (UINT32)(-1);
    nMinEC  = (UINT32)(-1);
    for (nIdx = 0; nIdx < nNumLA; nIdx++)
    {
        if (pstCI->pMinEC[nIdx] < nMinEC)
        {
            nMinIdx = nIdx;
            nMinEC  = pstCI->pMinEC[nIdx];
        }
    }

    /* Update Min EC */
    pstCI->pstFm->nMinECLan = (UINT16)nMinIdx;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}

/**
 *   @brief     this function recover the global Wear-Leveling info. when a sudden power-off occurs
 *
 *   @param[in] pstGivenZone    : STLZoneObj object pointer
 *
 *   @retval    FSR_STL_SUCCESS : successful
 *
 */
PUBLIC INT32
FSR_STL_RecoverGWLInfo(STLZoneObj *pstGivenZone)
{
    STLClstObj     *pstClst         = FSR_STL_GetClstObj(pstGivenZone->nClstID);
    STLZoneObj     *pstMasterZone   = &(pstClst->stZoneObj[MASTER_ZONE_ID]);
    STLZoneObj     *pstWLZone1;
    STLZoneObj     *pstWLZone2;
    STLCtxInfoHdl  *pstGivenCtx;
    STLCtxInfoHdl  *pstCtx;
    UINT32          nIdx;
    UINT32          nMaxFBlks;
    UINT32          nFBlk;
    BADDR           nVbn;
    INT32           nRet            = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /*
     * 0. check whether a power-off occurs during the global wear-leveling
     * if nZoneWLMark is not GLOBAL_WL_START, we will do nothing here
     */
    pstGivenCtx   = pstMasterZone->pstCtxHdl;
    if (pstGivenCtx->pstFm->nZoneWLMark != GLOBAL_WL_START)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }

    /* case that an power-off occurs during global wear-leveling */

    /* get the two STLZoneObjs */
    pstWLZone1 = &(pstClst->stZoneObj[pstGivenCtx->pstFm->nZone1Idx]);
    pstWLZone2 = &(pstClst->stZoneObj[pstGivenCtx->pstFm->nZone2Idx]);
    FSR_ASSERT(pstWLZone1 != NULL);
    FSR_ASSERT(pstWLZone2 != NULL);

    /* 1. check the info in pstWLZone1 was changed */
    pstCtx       = pstWLZone1->pstCtxHdl;
    nMaxFBlks = pstCtx->pstFm->nNumFBlks;
    nFBlk     = pstCtx->pstFm->nFreeListHead;
    for(nIdx = 0; nIdx < nMaxFBlks; nIdx++)
    {
        if (nFBlk >= pstWLZone1->pstML->nMaxFreeSlots)
        {
            nFBlk -= pstWLZone1->pstML->nMaxFreeSlots;
        }
        nVbn = pstCtx->pFreeList[nFBlk];

        /* case that the global wear-leveling was not completed */
        if (nVbn == pstGivenCtx->pstFm->nZone1Vbn)
        {
            /* reserve 1 meta page */
            nRet = FSR_STL_ReserveMetaPgs(pstWLZone1, 1, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - FSR_STL_ReserveMetaPgs returns error\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }
            
            /* update the context info */
            pstCtx->pFreeList[nFBlk] = pstGivenCtx->pstFm->nZone2Vbn;
            pstCtx->pFBlksEC [nFBlk] = pstGivenCtx->pstFm->nZone2EC;

            nRet = FSR_STL_StoreBMTCtx(pstWLZone1, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - FSR_STL_StoreBMTCtx returns error\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }
            
            break;
        }
        /* case that the global wear-leveling for the pstWLZone1 has been completed */
        else if (nVbn == pstGivenCtx->pstFm->nZone2Vbn)
        {
            break;
        }

        nFBlk++;
    }
    FSR_ASSERT(nIdx < nMaxFBlks);

    if (nRet != FSR_STL_SUCCESS)
        {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }

    /* 2. check the info in pstWLZone2 was changed */
    pstCtx       = pstWLZone2->pstCtxHdl;
    nMaxFBlks = pstCtx->pstFm->nNumFBlks;
    nFBlk     = pstCtx->pstFm->nFreeListHead;
    for(nIdx = 0; nIdx < nMaxFBlks; nIdx++)
    {
        if (nFBlk >= pstWLZone2->pstML->nMaxFreeSlots)
        {
            nFBlk -= pstWLZone2->pstML->nMaxFreeSlots;
        }
        nVbn = pstCtx->pFreeList[nFBlk];

        /* case that the global wear-leveling was not completed */
        if (nVbn == pstGivenCtx->pstFm->nZone2Vbn)
        {
            /* reserve 1 meta page */
            nRet = FSR_STL_ReserveMetaPgs(pstWLZone2, 1, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - FSR_STL_ReserveMetaPgs returns error\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }
            
            /* update the context info */
            pstCtx->pFreeList[nFBlk] = pstGivenCtx->pstFm->nZone1Vbn;
            pstCtx->pFBlksEC [nFBlk] = pstGivenCtx->pstFm->nZone1EC;

            nRet = FSR_STL_StoreBMTCtx(pstWLZone2, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - FSR_STL_StoreBMTCtx returns error\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }
            
            break;
        }
        /* case that the global wear-leveling for the pstWLZone2 has been completed */
        else if (nVbn == pstGivenCtx->pstFm->nZone1Vbn)
        {
            break;
        }

        nFBlk++;
    }
    FSR_ASSERT(nIdx < nMaxFBlks);

    if (nRet != FSR_STL_SUCCESS)
        {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
        return nRet;
    }

    /* 3. store the Master zone */
    do 
    {
    /* reserve 1 meta page */
    nRet = FSR_STL_ReserveMetaPgs(pstMasterZone, 1, FALSE32);
    if (nRet != FSR_STL_SUCCESS)
    {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - FSR_STL_ReserveMetaPgs returns error\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
    }

    /* initialize the global Wear-Leveling info. */
    pstGivenCtx->pstFm->nZone1Vbn   = NULL_VBN;
    pstGivenCtx->pstFm->nZone1Idx   = (UINT16)(-1);
    pstGivenCtx->pstFm->nZone1EC    = (UINT32)(-1);
    pstGivenCtx->pstFm->nZone2Vbn   = NULL_VBN;
    pstGivenCtx->pstFm->nZone2Idx   = (UINT16)(-1);
    pstGivenCtx->pstFm->nZone2EC    = (UINT32)(-1);
    pstGivenCtx->pstFm->nZoneWLMark = GLOBAL_WL_END;

    nRet = FSR_STL_StoreBMTCtx(pstMasterZone, FALSE32);
    if (nRet != FSR_STL_SUCCESS)
    {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x - FSR_STL_StoreBMTCtx returns error\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
    }

    } while (0);
    FSR_ASSERT(pstMasterZone->pstCtxHdl->pstFm->nZoneWLMark == GLOBAL_WL_END);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
            __FSR_FUNC__, __LINE__, nRet));
    return nRet;
}


/**
 *   @brief             This function updates the minimum EC for the PMT.
 *
 *   @param[in]         pstZone         : STLZoneObj object pointer
 *   @param[in,out]     STLLogGrpHdl    : STLLogGrpHdl object pointer
 *   @param[in]         nRemovalVbn     : the VBN of the block which is removed from pstCurLogGrp
 *
 *   @retval            FSR_STL_SUCCESS : successful
 *
 */
PUBLIC INT32 
FSR_STL_UpdatePMTMinEC (STLZoneObj     *pstZone,
                        STLLogGrpHdl   *pstCurLogGrp,
                        UINT32          nRemovalVbn)
{
    BADDR           nDgn;
    UINT32          nStartDgnInWLGrp;
    UINT16          nWLGrpIdx;
    BADDR           nCurDgn;
    INT32           nDirIdx;
    POFFSET         nMetaPOffs;
    INT32           nIdx;
    INT32           nIdx2;
    STLLogGrpHdl   *pstLogGrp           = NULL;
    INT32           nRet                = FSR_STL_SUCCESS;
    UINT32          nMinECCntInWLGrp    = 0xFFFFFFFF;
    BADDR           nMINECDgnInWLGrp    = NULL_DGN;
    BADDR           nMINECVbnInWLGrp    = NULL_VBN;
    STLDirHdrHdl   *pstDH;
    STLCtxInfoHdl  *pstCtx;
    STLPMTHdl      *pstPMT;
    STLLog         *pstTempLog;
    STLLog         *pstLogs;
    UINT8           nTempIdx;
    BOOL32          bLoadPMT            = FALSE32;
    BOOL32          bFirst              = TRUE32;
    UINT8           nDgnOffinPMT;
    INT32           nNumOfBlkinWLGrp;
    BOOL32          bReloadPMT          = FALSE32;
    BOOL32          bDoStep3            = FALSE32;
    BADDR           nValidStartDgn;
    BADDR           nValidEndDgn;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* if the nVbn is not the minimum in pstLogGrp, we will do nothing. */
    if ((nRemovalVbn != NULL_VBN) &&
        (pstCurLogGrp->pstFm->nMinVbn != nRemovalVbn))
    {
        return FSR_STL_SUCCESS;
    }

    FSR_ASSERT(pstCurLogGrp != NULL);

    pstLogGrp           = NULL;
    nRet                = FSR_STL_SUCCESS;
    nMinECCntInWLGrp    = 0xFFFFFFFF;
    nMINECDgnInWLGrp    = NULL_DGN;
    nMINECVbnInWLGrp    = NULL_VBN;
    bLoadPMT            = FALSE32;
    bFirst              = TRUE32;
    bReloadPMT          = FALSE32;
    bDoStep3            = FALSE32;

    pstDH               = pstZone->pstDirHdrHdl;
    pstCtx              = pstZone->pstCtxHdl;
    pstPMT              = pstZone->pstPMTHdl;
    nDgn                = pstCurLogGrp->pstFm->nDgn;

    /***********************************************************/
    /* Step 1 : Update the minimum Log in the current LogGrp   */
    /***********************************************************/

    /* if there is only one Log in the Log group */

    pstCurLogGrp->pstFm->nMinEC  = 0xFFFFFFFF;
    pstCurLogGrp->pstFm->nMinIdx = NULL_POFFSET;
    pstCurLogGrp->pstFm->nMinVbn = NULL_VBN;
    pstLogs                      = pstCurLogGrp->pstLogList;

    /* Scan every block in the LogGrp one by one */
    nTempIdx = pstCurLogGrp->pstFm->nHeadIdx;
    while (nTempIdx != 0xFF)
    {
        pstTempLog = pstLogs + nTempIdx;

        /*
         * if the nVbn of nTempLog is nVbn, skip nTempLog
         * because pstTempLog will be removed from the LogGrp
         */
        if (pstTempLog->nVbn == nRemovalVbn)
        {
            nTempIdx = pstTempLog->nNextIdx;
            continue;
        }

        /* Update the current minimum EC */
        if (pstTempLog->nEC < pstCurLogGrp->pstFm->nMinEC)
        {
            pstCurLogGrp->pstFm->nMinEC    = pstTempLog->nEC;
            pstCurLogGrp->pstFm->nMinVbn   = pstTempLog->nVbn;
            pstCurLogGrp->pstFm->nMinIdx   = nTempIdx;
        }

        nTempIdx = pstTempLog->nNextIdx;
    }

    FSR_ASSERT(pstCurLogGrp->pstFm->nMinVbn != nRemovalVbn);

    /***********************************************************/
    /* Step 2 : Update the nMinVbn in the current WL Group     */
    /***********************************************************/
    nDirIdx = FSR_STL_SearchPMTDir(pstZone, nDgn, &nMetaPOffs);
    FSR_ASSERT(nDirIdx      >= 0);
    FSR_ASSERT(nMetaPOffs   != NULL_POFFSET);

    /* find the WL group which is associated with the current LogGrp */
    nWLGrpIdx           = (UINT16)(nDirIdx   >> DEFAULT_PMT_EC_GRP_SIZE_SHIFT);
    nStartDgnInWLGrp    = (nDgn >> DEFAULT_PMT_EC_GRP_SIZE_SHIFT)
                                << DEFAULT_PMT_EC_GRP_SIZE_SHIFT;

    if ((pstDH->pstPMTWLGrp[pstDH->pstFm->nMinECPMTIdx].nMinVbn == nRemovalVbn))
    {
        FSR_ASSERT(pstDH->pstPMTWLGrp[nWLGrpIdx].nMinVbn == nRemovalVbn);
        bDoStep3 = TRUE32;
    }

    /*
     * if the nRemovalVbn is the same as the minimum Vbn in the current WL Group,
     * update the minimum information of the WL group
     */
    if (pstDH->pstPMTWLGrp[nWLGrpIdx].nMinVbn == nRemovalVbn || nRemovalVbn == NULL_VBN)
    {
        nMinECCntInWLGrp    = pstCurLogGrp->pstFm->nMinEC;
        nMINECDgnInWLGrp    = pstCurLogGrp->pstFm->nDgn;
        nMINECVbnInWLGrp    = pstCurLogGrp->pstFm->nMinVbn;

        /* compute the number of blocks in the current WL Group */
        nNumOfBlkinWLGrp = DEFAULT_PMT_EC_GRP_SIZE;
        if (nStartDgnInWLGrp + nNumOfBlkinWLGrp >= pstZone->pstZI->nNumUserBlks)
        {
            nNumOfBlkinWLGrp = pstZone->pstZI->nNumUserBlks - nStartDgnInWLGrp;
        }

        /* Search every LogGrp in the current WL Grp one by one */
        for (nIdx = 0; nIdx < nNumOfBlkinWLGrp; nIdx++)
        {
            nCurDgn = (BADDR)(nStartDgnInWLGrp + nIdx);

            /* compute the offset in the PMTBuf */
            nDgnOffinPMT = (UINT8)(nCurDgn % pstZone->pstZI->nNumLogGrpPerPMT);

            /*
             * if this is the first time of the for loop
             * and nDgn is not in the PMTbuf 
             * we will load ...
             */
            if (bFirst == TRUE32)
            {
                nValidStartDgn = (UINT16)(-1);
                for(nIdx2 = 0; nIdx2 < pstZone->pstZI->nNumLogGrpPerPMT; nIdx2++)
                {
                    if (pstZone->pstPMTHdl->astLogGrps[nIdx2].pstFm->nDgn != NULL_POFFSET)
                    {
                        nValidStartDgn = (BADDR)nIdx2;
                        break;
                    }
                }
                FSR_ASSERT(nValidStartDgn != (UINT16)(-1));

                nValidEndDgn = (UINT16)(-1);
                for(nIdx2 = pstZone->pstZI->nNumLogGrpPerPMT - 1; nIdx2 >= 0; nIdx2--)
                {
                    if (pstZone->pstPMTHdl->astLogGrps[nIdx2].pstFm->nDgn != NULL_POFFSET)
                    {
                        nValidEndDgn = (BADDR)nIdx2;
                        break;
                    }
                }
                FSR_ASSERT(nValidEndDgn != (UINT16)(-1));
                FSR_ASSERT(nValidEndDgn >= nValidStartDgn);
                FSR_ASSERT(pstZone->pstPMTHdl->astLogGrps[nValidStartDgn].pstFm->nDgn != 0xFFFF);
                FSR_ASSERT(pstZone->pstPMTHdl->astLogGrps[nValidEndDgn].pstFm->nDgn != 0xFFFF);
                
                if ((pstZone->pstPMTHdl->astLogGrps[nValidStartDgn].pstFm->nDgn > nCurDgn) ||
                    (pstZone->pstPMTHdl->astLogGrps[nValidEndDgn].pstFm->nDgn < nCurDgn))
                {
                    bLoadPMT = TRUE32;
                }
                bFirst = FALSE32;
            }
            /*
             * if nDgnOffinPMT is 0 and this is not the first loop,
             * we will load new PMT
             */
            else if(nDgnOffinPMT == 0)
            {
                bLoadPMT    = TRUE32;
                bFirst      = FALSE32;
            }

            /* if the nCurDgn is the same as nDgn, skip this loop */
            if(nCurDgn == nDgn)
            {
                continue;
            }

            /* load PMT */
            if (bLoadPMT == TRUE32)
            {
                /* Find Meta Page OFFset */
                nDirIdx = FSR_STL_SearchPMTDir(pstZone, nCurDgn, &nMetaPOffs);

                /* nCurDgn is not existed in PMT directory */
                if (nDirIdx < 0)
                {
                    continue;
                }

                FSR_ASSERT(nMetaPOffs != NULL_POFFSET);

                /* backup PMT buffer to pTempMetaPgBuf when this is the first PMT load */
                if (bReloadPMT == FALSE32)
                {
                    FSR_OAM_MEMCPY(pstZone->pTempMetaPgBuf, pstPMT->pBuf, pstPMT->nBufSize);
                }

                /* load PMT */
                pstLogGrp   = NULL;
                bReloadPMT  = TRUE32;
                nRet = FSR_STL_LoadPMT(pstZone, nCurDgn, nMetaPOffs, &pstLogGrp, FALSE32);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
                    return nRet;
                }

                bLoadPMT = FALSE32;
            }

            if (nCurDgn == nDgn)
            {
                FSR_ASSERT(FALSE32);
            }
            else
            {
                pstLogGrp = &(pstPMT->astLogGrps[nDgnOffinPMT]);
            }

            FSR_ASSERT(pstLogGrp != NULL);

            /* if pstLogGrp has the minimum VBN, update the MinEC info... */
            if (pstLogGrp->pstFm->nMinEC < nMinECCntInWLGrp)
            {
                nMinECCntInWLGrp = pstLogGrp->pstFm->nMinEC;
                nMINECDgnInWLGrp = nCurDgn;
                nMINECVbnInWLGrp = pstLogGrp->pstFm->nMinVbn;
            }
        }

        /* if bReloadPMT is TRUE, recopy PMT info from pMetaPgBuf */
        if (bReloadPMT == TRUE32)
        {
            FSR_OAM_MEMCPY(pstPMT->pBuf, pstZone->pTempMetaPgBuf, pstPMT->nBufSize);
        }

        /* Update the minimum info. of the current WL group */
        if (nMINECVbnInWLGrp == NULL_VBN)
        {
            pstDH->pstPMTWLGrp[nWLGrpIdx].nDgn          = NULL_DGN;
        }
        else
        {
            pstDH->pstPMTWLGrp[nWLGrpIdx].nDgn          = nMINECDgnInWLGrp;
        }
        pstDH->pstPMTWLGrp[nWLGrpIdx].nMinEC        = nMinECCntInWLGrp;
        pstDH->pstPMTWLGrp[nWLGrpIdx].nMinVbn       = nMINECVbnInWLGrp;

        FSR_ASSERT(pstDH->pstPMTWLGrp[nWLGrpIdx].nMinVbn != nRemovalVbn);

        /***********************************************************/
        /* Step 3 : Update the nMinVbn in the current WL Group     */
        /***********************************************************/

        /* if the globally minimum is larger than the minimum of the current WL Grp, update it */
        if ((bDoStep3    == TRUE32  ) ||
            (nRemovalVbn == NULL_VBN))
        {
            FSR_STL_UpdatePMTDirMinECGrp(pstZone, pstDH);
            FSR_ASSERT((pstDH->pstFm->nMinECPMTIdx == 0xFFFF) ||
                       (pstDH->pstPMTWLGrp[pstDH->pstFm->nMinECPMTIdx].nMinVbn != nRemovalVbn));
        }

        /* Update Context */
        pstCtx->pstFm->nUpdatedPMTWLGrpIdx          = nWLGrpIdx;
        pstCtx->pstFm->stUpdatedPMTWLGrp.nDgn       = pstDH->pstPMTWLGrp[nWLGrpIdx].nDgn;
        pstCtx->pstFm->stUpdatedPMTWLGrp.nMinVbn    = pstDH->pstPMTWLGrp[nWLGrpIdx].nMinVbn;
        pstCtx->pstFm->stUpdatedPMTWLGrp.nMinEC     = pstDH->pstPMTWLGrp[nWLGrpIdx].nMinEC;
        pstCtx->pstFm->nMinECPMTIdx                 = pstDH->pstFm->nMinECPMTIdx;
    }

    FSR_ASSERT(pstDH->pstPMTWLGrp[nWLGrpIdx].nMinVbn    != nRemovalVbn);
    FSR_ASSERT(pstCurLogGrp->pstFm->nMinVbn             != nRemovalVbn);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}

/**
 *   @brief     This function purifies nNum free blocks in the free block list from header to tail.
 *
 *   @param[in] pstZone     : STLZoneObj object pointer
 *   @param[in] nNum        : # of free blocks to be purified
 *   @param[in] nRcvDgn     : the DGN of the current LogGrp;
 *
 *   @retval    FSR_STL_SUCCESS : successful
 *
 */
PUBLIC INT32
FSR_STL_WearLevelFreeBlk   (STLZoneObj *pstZone,
                            UINT32      nNum,
                            BADDR       nRcvDgn)
{
    const BADDR     nOrgLan     = pstZone->pstBMTHdl->pstFm->nLan;
    STLLogGrpHdl   *pstLogGrp       = NULL;
    POFFSET         nMetaPOffs;
    BOOL32          bLoadBMT        = FALSE32;
    BOOL32          bLoadPMT        = FALSE32;
    INT32           nRet            = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    do
    {
#if (OP_SUPPORT_GLOBAL_WEAR_LEVELING == 1)
        nRet = _WearLevelGlobal(pstZone, nNum, &bLoadBMT, &bLoadPMT);
#else
        nRet = _WearLevelLocal(pstZone, nNum, &bLoadBMT, &bLoadPMT);
#endif
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }

        /* If bLoadBMT is TRUE32, reload the BMT with nOrgLan */
        if (bLoadBMT == TRUE32)
        {
            /* Return to the original BMT. */
            nRet = FSR_STL_LoadBMT(pstZone, nOrgLan, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                break;
            }
        }

        if ((nRcvDgn  != NULL_DGN) &&
            (bLoadPMT == TRUE32))
        {
            /*
             * Find meta offset of the given nRcvDgn.
             * Must exist !!! 
             */
            nRet = FSR_STL_SearchPMTDir(pstZone, nRcvDgn, &nMetaPOffs);
            FSR_ASSERT(nRet       >= 0);
            FSR_ASSERT(nMetaPOffs != NULL_POFFSET);

            /* Load PMT of the given nRcvDgn */
            pstLogGrp = NULL;
            nRet = FSR_STL_LoadPMT(pstZone, nRcvDgn, nMetaPOffs, &pstLogGrp, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                break;
            }
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : %x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}

#endif /* (OP_SUPPORT_DATA_WEAR_LEVELING) */

#if (0)
//#if (OP_SUPPORT_META_WEAR_LEVELING == 1)
/**
 *   @brief      This function purifies free blocks in the free block list from header to tail.
 *
 *   @param[in]  pstOrgZone     : zone object pointer 
     @param[in]  nNumWLBlk      : the number of block to do Wear-Leveling
 *   @param[out] pbLoadBMT      : Dose this function load BMT?
 *   @param[out] pbLoadPMT      : Dose this function load PMT?
 *
 *   @retval    FSR_STL_SUCCESS : successful
 */
PUBLIC INT32
FSR_STL_ForceMetaWearLevel (STLZoneObj *pstZone)
{
    const UINT32    nWLThreshold    = (FSR_STL_GetClstObj(pstZone->nClstID)->pstEnvVar->nECDiffThreshold);
    RBWDevInfo     *pstDev      = pstZone->pstDevInfo;
    STLCtxInfoHdl  *pstCtx      = pstZone->pstCtxHdl;
    STLCtxInfoFm   *pstCtxFm    = pstCtx->pstFm;
    STLZoneInfo    *pstZI       = pstZone->pstZI;
    STLDirHdrHdl   *pstDH       = pstZone->pstDirHdrHdl;
    STLDirHeaderFm *pstDHFm     = pstDH->pstFm;
    const UINT32    nMaxFBlk    = pstZone->pstML->nMaxFreeSlots;
    const BADDR     nMetaVbn    = pstZI->aMetaVbnList[pstDHFm->nHeaderIdx];
    const UINT32    nMetaEC     = pstDH->pMetaBlksEC [pstDHFm->nHeaderIdx];
    StlWLArg        stWL;
    BOOL32          bMinPMT;
    BOOL32          bFindMin;
    BOOL32          bLoad;
    UINT32          nFBlk;
    UINT32          nIdx;
    PADDR           nSrcVpn;
    PADDR           nDstVpn;
    PADDR           nPOff;
    UINT32          nMetaPgs;
    INT32           nRet            = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%d)\r\n"),
            __FSR_FUNC__, pstZone->nZoneID));

    do
    {
        /* First, find minimum erase count in the self zone */
        stWL.nMinEC = (UINT32)(-1);
        bFindMin    = _FindMinEC(pstZone, &(stWL.nMinEC), &bMinPMT);
        if (bFindMin == FALSE32)
        {
            nRet = FSR_STL_CRITICAL_ERROR;
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR]  %s() L(%d) : 0x%08x - Can not find the unit of which erase count is the minimum\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));

            FSR_ASSERT(bFindMin == TRUE32);
            break;
        }

        /* Erase count overflow */
        if ((stWL.nMinEC >  stWL.nMinEC + nWLThreshold) ||
            (nMetaEC     <= stWL.nMinEC + nWLThreshold))
        {
            nRet = FSR_STL_SUCCESS;
            break;
        }

        /* Second, find the free blk of which erase count is the maximum */
        nFBlk         = pstCtxFm->nFreeListHead;
        stWL.nFBlkIdx = (UINT32)(-1);
        stWL.nTrgEC   = 0;
        for (nIdx = 0; nIdx < pstCtxFm->nNumFBlks; nIdx++)
        {
            if (stWL.nFBlkIdx >= nMaxFBlk)
            {
                stWL.nFBlkIdx -= nMaxFBlk;
            }

            if ((pstCtx->pFBlksEC[nFBlk] != (UINT32)(-1)) &&
                (pstCtx->pFBlksEC[nFBlk] > stWL.nTrgEC))
            {
                stWL.nFBlkIdx = nFBlk;
                stWL.nTrgEC   = pstCtx->pFBlksEC[nFBlk];
            }

            nFBlk++;
        }
        if (stWL.nFBlkIdx == (UINT32)(-1))
        {
            nRet = FSR_STL_CRITICAL_ERROR;
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x - Can not find the free unit.\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));

            FSR_ASSERT(stWL.nFBlkIdx != (UINT32)(-1));
            break;
        }
        stWL.nTrgVbn = pstCtx->pFreeList[stWL.nFBlkIdx];

        /* Third, Wear-leveling to get the minimum erase blk */
        if (bMinPMT == FALSE32)
        {
            nRet = _WearLevelBMT(pstZone, &(stWL), &bLoad);
        }
        else
        {
            nRet = _WearLevelPMT(pstZone, &(stWL), &bLoad);
        }
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:OUT]  --%s(0x%01x) L(%d) : 0x%08x - _WearLevel{BMT|PMT} returns error.\r\n"),
                    __FSR_FUNC__, bMinPMT, __LINE__, nRet));
            break;
        }

        /* Forth, Force Meta wear-leveling */
        stWL.nMinVbn = pstCtx->pFreeList[stWL.nFBlkIdx];
        nMetaPgs     = pstDHFm->nCPOffs;

        /*  block erase */
        FSR_ASSERT(stWL.nMinVbn == pstCtx->pFreeList[stWL.nFBlkIdx]);
        nRet = FSR_STL_FlashErase(pstZone, stWL.nMinVbn);
        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR]  ++%s() L(%d) : 0x%08x - FSR_STL_FlashErase returns error\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        /* Update Directory Header (DH) */
        pstZI->aMetaVbnList[pstDHFm->nHeaderIdx] = stWL.nMinVbn;
        pstDH->pMetaBlksEC [pstDHFm->nHeaderIdx] = stWL.nMinEC + 1;

        nRet = FSR_STL_ProgramHeader(pstZone, stWL.nMinVbn);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR]  ++%s() L(%d): 0x%08x - FSR_STL_ProgramHeader returns error\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        /* Copyback all valid page in current header index */
        nSrcVpn = (nMetaVbn     << pstDev->nPagesPerSbShift);
        nDstVpn = (stWL.nMinVbn << pstDev->nPagesPerSbShift);
        for (nIdx = pstZI->nNumDirHdrPgs; nIdx < nMetaPgs; nIdx++)
        {
            if (pstDev->nDeviceType == RBW_DEVTYPE_MLC)
            {
                nPOff = FSR_BML_GetVPgOffOfLSBPg(pstZone->nVolID, nIdx);
            }
            else
            {
                nPOff = nIdx;
            }

            nRet = FSR_STL_FlashCopyback(pstZone,
                                         nSrcVpn + nPOff,
                                         nDstVpn + nPOff,
                                         pstDev->nFullSBitmapPerVPg);
            if (nRet != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  ++%s() L(%d): 0x%08x - FSR_STL_FlashCopyback returns error\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }
            nRet = FSR_STL_SUCCESS;
        }
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }
        pstDHFm->nCPOffs = nMetaPgs;

        /* Update context information for free blk info */
        pstCtx->pFreeList[stWL.nFBlkIdx] = nMetaVbn;
        pstCtx->pFBlksEC [stWL.nFBlkIdx] = nMetaEC;

        nRet = FSR_STL_StoreBMTCtx(pstZone, FALSE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR]  ++%s() L(%d): 0x%08x - FSR_STL_StoreBMTCtx returns error\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

        /* Update root information for Meta VBN list */
        nRet = FSR_STL_StoreRootInfo(FSR_STL_GetClstObj(pstZone->nClstID));
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR]  ++%s() L(%d): 0x%08x - FSR_STL_StoreBMTCtx returns error\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            break;
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"),
            __FSR_FUNC__, nRet));
    return nRet;
}

#endif

/**
 *  @brief          This function reads the EC info for all blocks.
 *
 *  @param[in]      pstZone     : STLZoneObj pointer object
 *  @param[in,out]  paBlkEC     : virtual block erase count array
 *  @param[in]      nNumBlks    : paBlkEC[] array size
 *
 *  @return         FSR_STL_SUCCESS
 *
 *  @author         Wonmoon Cheon
 *  @version        1.1.0
 */
PUBLIC INT32
FSR_STL_GetAllBlksEC   (STLZoneObj *pstZone,
                        UINT32     *paBlkEC,
                        UINT32     *pRootBlksECNT,
                        UINT32     *pZoneBlksECNT,
                        UINT32      nNumBlks)
{
    STLCtxInfoHdl  *pstCtx      = pstZone->pstCtxHdl;
    STLDirHdrHdl   *pstDH       = pstZone->pstDirHdrHdl;
    STLZoneInfo    *pstZI       = pstZone->pstZI;
    const   UINT32  nMaxFBlk    = pstZone->pstML->nMaxFreeSlots;
    BADDR           nVbn;
    UINT32          nEC;
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
    INT32           nRet;
    UINT32          nPMTEntryCnt;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Initialize output variables */
    if (pRootBlksECNT != NULL)
    {
        *pRootBlksECNT = 0;
    }

    if (pZoneBlksECNT != NULL)
    {
        *pZoneBlksECNT = 0;
    }

    /* step1) root blocks' EC */
    for (nIdx = 0; nIdx < pstZone->pstRI->nNumRootBlks; nIdx++)
    {
        nVbn = (BADDR)(nIdx);
        nEC  = pstZone->pstRI->aRootBlkEC[nIdx];

        if (nVbn < nNumBlks)
        {
            if (paBlkEC != NULL)
            {
                paBlkEC[nVbn] = (nEC | FSR_STL_META_MARK);
            }
            if (pRootBlksECNT != NULL)
            {
                *pRootBlksECNT += nEC;
            }
        }
    }

    /* step2) meta blocks' EC */
    for (nIdx = 0; nIdx < pstZI->nNumMetaBlks; nIdx++)
    {
        nVbn = pstZI->aMetaVbnList[nIdx];
        nEC  = pstDH->pMetaBlksEC[nIdx];

        if (nVbn < nNumBlks)
        {
            if (paBlkEC != NULL)
            {
                paBlkEC[nVbn] = (nEC | FSR_STL_META_MARK);
            }
            if (pZoneBlksECNT != NULL)
            {
                *pZoneBlksECNT += nEC;
            }
        }
    }

    /* step3) buffer, temporary blocks' EC */
    nVbn = pstCtx->pstFm->nBBlkVbn;
    nEC  = pstCtx->pstFm->nBBlkEC;

    if (nVbn < nNumBlks)
    {
        if (paBlkEC != NULL)
        {
            paBlkEC[nVbn] = (nEC | FSR_STL_META_MARK);
        }
        if (pZoneBlksECNT != NULL)
        {
            *pZoneBlksECNT += nEC;
        }
    }

    nVbn = pstCtx->pstFm->nTBlkVbn;
    nEC  = pstCtx->pstFm->nTBlkEC;

    if (nVbn < nNumBlks)
    {
        if (paBlkEC != NULL)
        {
            paBlkEC[nVbn] = (nEC | FSR_STL_META_MARK);
        }
        if (pZoneBlksECNT != NULL)
        {
            *pZoneBlksECNT += nEC;
        }
    }

    /* step4) free/garbage blocks' EC */
    for (nIdx = 0; nIdx < pstCtx->pstFm->nNumFBlks; nIdx++)
    {
        /* nTempIdx = (pstCtx->pstFm->nFreeListHead + nIdx) % pstZoneInfo->nNumFBlks; */
        nTempIdx = (pstCtx->pstFm->nFreeListHead + nIdx);
        if (nTempIdx >= nMaxFBlk)
        {
            nTempIdx -= nMaxFBlk;
        }
        FSR_ASSERT(nTempIdx < nMaxFBlk);
        nVbn = pstCtx->pFreeList[nTempIdx];
        nEC  = pstCtx->pFBlksEC[nTempIdx];

        if (nVbn < nNumBlks)
        {
            if (paBlkEC != NULL)
            {
                paBlkEC[nVbn] = nEC;
            }
            if (pZoneBlksECNT != NULL)
            {
                *pZoneBlksECNT += nEC;
            }
        }
    }

    /* step5) Active log blocks' EC */
    pstLogGrp = pstZone->pstActLogGrpList->pstHead;
    while (pstLogGrp != NULL)
    {
        if (pstLogGrp->pstFm->nNumLogs > 0)
        {
            pstLog = &(pstLogGrp->pstLogList[pstLogGrp->pstFm->nHeadIdx]);
            for (nIdx = 0; nIdx < pstLogGrp->pstFm->nNumLogs; nIdx++)
            {
                nVbn = pstLog->nVbn;
                nEC  = pstLog->nEC;
                FSR_ASSERT(nVbn != NULL_VBN);

                if (nVbn < nNumBlks)
                {
                    if (paBlkEC != NULL)
                    {
                        paBlkEC[nVbn] = nEC;
                    }
                    if (pZoneBlksECNT != NULL)
                    {
                        *pZoneBlksECNT += nEC;
                    }
                }

                pstLog = &(pstLogGrp->pstLogList[pstLog->nNextIdx]);
            }
        }

        pstLogGrp = pstLogGrp->pNext;
    }

    /* step6) data blocks' EC (read all BMTs) */
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

        if (nLan == (pstZI->nNumLA - 1))
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
            nEC  = pstBMT->pDBlksEC[nIdx];

            if (nVbn < nNumBlks)
            {
                if (paBlkEC != NULL)
                {
                    paBlkEC[nVbn] = nEC;
                }
                if (pZoneBlksECNT != NULL)
                {
                    *pZoneBlksECNT += nEC;
                }
            }
        }
    }

    /* step7) log blocks' EC (read all PMTs) */
    nPMTEntryCnt = ((pstZI->nNumUserBlks - 1) >> pstZone->pstRI->nNShift) + 1;
    for (nPMTIdx = 0; nPMTIdx < nPMTEntryCnt; nPMTIdx++)
    {
        nDgn        = (BADDR)nPMTIdx;

        /* Pass if nDgn is an active log group */
        pstLogGrp = FSR_STL_SearchLogGrp(pstZone->pstActLogGrpList, nDgn);
        if (pstLogGrp != NULL)
        {
            continue;
        }

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
            pstLog = &(pstLogGrp->pstLogList[pstLogGrp->pstFm->nHeadIdx]);
            for (nIdx = 0; nIdx < pstLogGrp->pstFm->nNumLogs; nIdx++)
            {
                nVbn = pstLog->nVbn;
                nEC  = pstLog->nEC;
                FSR_ASSERT(nVbn != NULL_VBN);

                if (nVbn < nNumBlks)
                {
                    if (paBlkEC != NULL)
                    {
                        paBlkEC[nVbn] = nEC;
                    }
                    if (pZoneBlksECNT != NULL)
                    {
                        *pZoneBlksECNT += nEC;
                    }
                }

                if (pstLog->nNextIdx != 0xFF)
                {
                    pstLog = &(pstLogGrp->pstLogList[pstLog->nNextIdx]);
                }
                else
                {
                    break;
                }
            }
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}

PUBLIC  UINT32  
FSR_STL_FindMinEC      (STLZoneObj *pstOrgZone)       
{    
    STLClstObj     *pstClst;
    STLZoneObj     *pstZone;
#if (OP_STL_DEBUG_CODE == 1)
    STLZoneObj     *pstMinZone;
    BOOL32          bMinZone;
#endif    
    BOOL32          bMinPMT         = TRUE32;
    UINT32          nZone;
    UINT32          nMinEc;

    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Get the cluster object */
    pstClst = FSR_STL_GetClstObj(pstOrgZone->nClstID);

    /* First, find minimum erase count in the self zone */
    nMinEc      = (UINT32)(-1);
#if (OP_STL_DEBUG_CODE == 1)
    pstMinZone  = pstOrgZone;
    bMinZone    = _FindMinEC(pstOrgZone, &(nMinEc), &bMinPMT);
    FSR_ASSERT(bMinZone == TRUE32);
#else
    _FindMinEC(pstOrgZone, &(nMinEc), &bMinPMT);
#endif

    /* Find minimum erase count among zones */
    for (nZone = 0; nZone < MAX_ZONE; nZone++)
    {
        if (nZone == pstOrgZone->nZoneID) 
            continue;
        
        pstZone = &(pstClst->stZoneObj[nZone]);

        if (pstZone->bOpened == TRUE32)
        {
#if (OP_STL_DEBUG_CODE == 1)
            bMinZone = _FindMinEC(pstZone, &(nMinEc), &bMinPMT);
            if (bMinZone == TRUE32)
            {
                pstMinZone = pstZone;
            }
#else
            _FindMinEC(pstZone, &(nMinEc), &bMinPMT);
#endif
        }
    }

    return nMinEc;
}
