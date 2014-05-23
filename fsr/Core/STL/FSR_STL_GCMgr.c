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
 *  @file       FSR_STL_GCMgr.c
 *  @brief      This function contains the function for GC
 *  @author     Kangho Roh
 *  @date       05-MAR-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  05-MAR-2007 [Wonmoon Cheon] : first writing
 *  @n  07-MAR-2007 [Kangho Roh]    : Implement DoGC() and some other functions.
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
#define     UINT8_SHIFT     (3)
#define     UINT8_SIZE      (1 << UINT8_SHIFT)
#define     UINT8_MASK      (UINT8_SIZE - 1)

/*****************************************************************************/
/* Local type defines                                                        */
/*****************************************************************************/

/*****************************************************************************/
/* Local constant definitions                                                */
/*****************************************************************************/

/*****************************************************************************/
/* Local (static) function prototype                                         */
/*****************************************************************************/
PRIVATE INT32   _FindInvalidBlkInBMT   (STLZoneObj     *pstZone,
                                        const BADDR     nTCurLA,
                                              UINT32   *nInvBlkIdx,
                                              BADDR    *pnVbn,
                                              UINT32   *pnEC);
PRIVATE BOOL32  _GetGCLABitmap         (STLZoneObj     *pstZone,
                                        const BADDR     nAddLA);

/*****************************************************************************/
/* Local (static)  Function Definition                                       */
/*****************************************************************************/
/**
 *  @brief      This function finds an invalid block in a Log Group
 *
 *  @param[in]      pstBMT      : A PMT Buffer
 *  @param[in]      nTCurLA     : Current Local Area Number
 *  @param[in,out]  nStartOffs  : starting offset in a BMT
 *  @param[in,out]  nVbn        : Vbn of an Invalid Block
 *  @param[in,out]  pnEC        : Erase Count of an Invalid Block
 *
 *  @return         FSR_STL_SUCCESS
 *
 *  @author         Kangho Roh
 *  @version        1.2.0
 */
PRIVATE INT32
_FindInvalidBlkInBMT   (STLZoneObj       *pstZone,
                        const BADDR       nTCurLA,
                              UINT32     *nStartOffs,
                              BADDR      *nVbn,
                              UINT32     *pnEC)
{
    const STLBMTHdl    *pstCurBMT       = pstZone->pstBMTHdl;
    const UINT32        nDBlksPerLA     = pstZone->pstZI->nDBlksPerLA;
          STLBMTEntry  *pstCurBMTEntry;
          UINT32        nIdx;
          INT32         nRet;

    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /*
     * A new LA comes...
     * If nInvBlkIdx != 0, every data blocks in the current LA have not be scanned.
     */
    if ((*nStartOffs == 0) &&
        (pstCurBMT->pstFm->nLan != nTCurLA))
    {
        /*In this case, Load a BMT with nTCurLA.*/
        nRet = FSR_STL_LoadBMT(pstZone, nTCurLA, FALSE32);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
            return nRet;
        }
    }

    *nVbn = NULL_VBN;
    for (nIdx = *nStartOffs; nIdx < nDBlksPerLA; nIdx++)
    {
        /* Get a STLBMTEntry. */
        pstCurBMTEntry = &(pstCurBMT->pMapTbl[nIdx]);
    
        if (pstCurBMTEntry->nVbn == NULL_VBN)
        {
            continue;
        }

        /*
         * If OP_SUPPORT_PAGE_DELETE == 1, a block can be invalid one even if nVbn is not NULL_VBN
         * In this case, if aPVB of every page is "0xFF", the block is invalid one.
         */
#if 1
        if (pstCurBMT->pGBlkFlags[nIdx >> 3] & (0x01 << (nIdx & 7)))
        {
            *nStartOffs = nIdx + 1;
            *nVbn       = pstCurBMTEntry->nVbn;
            *pnEC       = pstCurBMT->pDBlksEC[nIdx];
             break;
        }
#else
         for (i = 0; i < BMT_PVB_SIZE; i++)
         {
             if (pstCurBMTEntry->aPVB[i] != 0x00)
             {
                 break;
             }
         }

         if (i == BMT_PVB_SIZE)
         {
             *nStartOffs = nIdx + 1;
             *nVbn       = pstCurBMTEntry->nVbn;
             *pnEC       = pstCurBMT->pDBlksEC[nIdx];
             break;
         }
#endif
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}

/**
 *  @brief      This function returns the bitmap with nAddLA.
 *
 *  @param[in]  pstZone     : STLZoneObj pointer
 *  @param[in]  nAddLA      : Address of a Local Area
 *
 *  @return     Bitmap      : 0 or 1
 *
 *  @author     Kangho Roh
 *  @version    1.2.0
 */
PRIVATE BOOL32
_GetGCLABitmap (STLZoneObj     *pstZone,
                const BADDR     nAddLA)
{
    const UINT8     nBitMap     = pstZone->pGCScanBitmap[nAddLA >> UINT8_SHIFT];
    const UINT32    nBitMsk     = 1 << (nAddLA & UINT8_MASK);
          BOOL32    bBit;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    bBit    = (nBitMap & nBitMsk) ? TRUE32 : FALSE32;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, bBit));
    return bBit;
}

/*****************************************************************************/
/* Global Function Definition                                                */
/*****************************************************************************/

/**
 *  @brief      This function moves all garbage blocks from garbage block list to data blocks.
 *
 *  @param[in]  pstZone             : STLZoneObj pointer
 *  @param[in]  nNumOfReservedSlots : the number of slots to be reserved
 *
 *  @return     FSR_STL_SUCCESS
 *
 *  @author     Kangho Roh
 *  @version    1.2.0
 */
PUBLIC INT32
FSR_STL_ReverseGC  (STLZoneObj *pstZone,
                    UINT32      nNumOfReservedSlots)
{
    const STLCtxInfoHdl    *pstCtx      = pstZone->pstCtxHdl;
    const STLZoneInfo      *pstZI       = pstZone->pstZI;
    const STLBMTHdl        *pstBMT      = pstZone->pstBMTHdl;
    const BADDR             nMaxLA      = pstZI->nNumLA;
    const UINT32            nMaxFBlk    = pstZone->pstML->nMaxFreeSlots;
          STLCtxInfoFm     *pstCtxFm    = pstCtx->pstFm;
    const BADDR             nOrgLan     = pstBMT->pstFm->nLan;
          BADDR             nTmpLan;
          UINT32            nBlksInaLA;
          STLBMTEntry      *pstBMTEntry;
          BOOL32            bStoreBMT;
          UINT32            nFreeIdx;
          INT32             nNumOfBlkforGC;
          STLLogGrpHdl     *pstLogGrp;
          POFFSET           nMetaPOffs;
          BOOL32            bLoadPMT    = FALSE32;
          BADDR             nRcvDgn     = NULL_DGN;
          UINT32            nIdx;
          INT32             nRet        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));
    FSR_ASSERT(OP_SUPPORT_PAGE_DELETE == 1);
    FSR_ASSERT(pstZone->pstML->nMaxFreeSlots >= nNumOfReservedSlots);

    /* The number of blocks to do ReverseGC is ... */
    nNumOfBlkforGC = nNumOfReservedSlots
                   - (pstZone->pstML->nMaxFreeSlots - pstCtxFm->nNumFBlks);

    /* The number of free slots is larger than nNumOfReservedSlots. */
    if (nNumOfBlkforGC <= 0)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
        return FSR_STL_SUCCESS;
    }

    /* backup a DGN in the current PMTHdr */
    pstLogGrp = pstZone->pstPMTHdl->astLogGrps;
    for (nIdx = 0; nIdx < pstZI->nNumLogGrpPerPMT; nIdx++)
    {
        nRcvDgn = pstLogGrp->pstFm->nDgn;
        if (nRcvDgn != NULL_DGN)
        {
            break;
        }

        pstLogGrp++;
    }

    /* Initialize some parameters */
    nTmpLan = nOrgLan;

    /* Scan every LA one by one and find a block with NULL_VBN.
     * Then push a block in the free block list to there
     */
    do
    {
        bStoreBMT = FALSE32;

        if (nTmpLan == nMaxLA - 1)
        {
            /*In the last LA, there are only (nDataAreaSize % MAX_DBLKS_PER_LA) blocks.*/
            nBlksInaLA = ((pstZI->nNumUserBlks - 1) & (pstZI->nDBlksPerLA - 1)) + 1;
        }
        else
        {
            nBlksInaLA = pstZI->nDBlksPerLA;
        }

        /* Scan every data block and find the block with NULL_VBN.
         * The Block(s) will be replaced with the block in Garbage Block List.
         */
        for (nIdx = 0; nIdx < nBlksInaLA; nIdx++)
        {
            /* nNumOfBlkforGC blks has been moved to BMT from the free blk list. */
            if (nNumOfBlkforGC <= 0)
            {
                break;
            }

            pstBMTEntry = &(pstBMT->pMapTbl[nIdx]);

            /* A data block with NULL_VBN is found.
             * We will insert a block into the slot.
             */
            if (pstBMTEntry->nVbn == NULL_VBN)
            {
                /* Set the information of the invalid block. */
                nFreeIdx = (pstCtxFm->nFreeListHead + pstCtxFm->nNumFBlks - 1);
                if (nFreeIdx >= nMaxFBlk)
                {
                    nFreeIdx -= nMaxFBlk;
                }
                FSR_ASSERT(nFreeIdx < nMaxFBlk);

                /* Copy the GU from the free blks list to the BMT */
                pstBMTEntry->nVbn = pstCtx->pFreeList[nFreeIdx];

                /* Set Garbage mark in the BMT */
                pstBMT->pGBlkFlags[nIdx >> 3] |= (1 << (nIdx & 0x07));

#if (OP_SUPPORT_DATA_WEAR_LEVELING == 1)
                pstBMT->pDBlksEC[nIdx]  = pstCtx->pFBlksEC[nFreeIdx];
                FSR_STL_UpdateBMTMinEC(pstZone);
#endif  /* (OP_SUPPORT_DATA_WEAR_LEVELING == 1) */

                /* Remove GU info from the free blk list */
                pstCtx->pFreeList[nFreeIdx] = NULL_VBN;

                /* Decrease the number of garbage blocks by 1 */
                pstCtxFm->nNumFBlks--;

                /* Increase number of idle data blocks */
                pstCtxFm->nNumIBlks++;

                /* Set bAddGBlk = TRUE32. */
                bStoreBMT = TRUE32;

                nNumOfBlkforGC--;
            }
        }
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }

        /* If bAddBlk == TRUE32, we must store the context */
        if (bStoreBMT == TRUE32)
        {
            /* reserve meta page */
            nRet = FSR_STL_ReserveMetaPgs(pstZone, 1, TRUE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                break;
            }

            /* GCLABitmap of this LA becomes 1
             * because there is at least one invalid block in the LA.
             */
            nRet = FSR_STL_UpdateGCScanBitmap(pstZone, nTmpLan, TRUE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                break;
            }
            
#if (OP_SUPPORT_DATA_WEAR_LEVELING == 1)
            /* Update MinEC */
            FSR_STL_UpdateBMTMinEC(pstZone);
#endif

            /* store the BMT */
            nRet = FSR_STL_StoreBMTCtx(pstZone, TRUE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                break;
            }
        }

        /* Check the ending condition */
        if (nNumOfBlkforGC <= 0)
        {
            /* Original BMT Loading. */
            if (nTmpLan != nOrgLan)
            {
                /* In this case, Load a BMT with nCurLA. */
                nRet = FSR_STL_LoadBMT(pstZone, nOrgLan, FALSE32);
                if (nRet != FSR_STL_SUCCESS)
                {
                    break;
                }
            }

            /* Original PMT Loading */
            if ((bLoadPMT == TRUE32  ) &&
                (nRcvDgn  != NULL_DGN))
            {
                /* Find meta offset of the given nRcvDgn.
                   Must exist !!! */
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

            break;
        }
        else
        {
            /* Move to the next LA. */
            nTmpLan++;
            if (nTmpLan >= nMaxLA)
            {
                nTmpLan = nTmpLan - nMaxLA;
            }
            FSR_ASSERT(nTmpLan != nOrgLan);

            /* In this case, Load a BMT with next LA. */
            nRet = FSR_STL_LoadBMT(pstZone, nTmpLan, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                break;
            }
        }

    } while (TRUE32);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : %x\r\n"), __FSR_FUNC__, nRet));
    return nRet; 
}

/**
 *  @brief      This function sets the GCLABitmap to 1 or 0.
 *
 *  @param[in]  pstZone    : STLZoneObj pointer
 *  @param[in]  nLan       : Address of a Local Area
 *  @param[in]  bSet       : TRUE32 or FALSE32
 *
 *  @return     FSR_STL_SUCCESS
 *
 *  @author     Kangho Roh
 *  @version    1.2.0
 */
PUBLIC INT32
FSR_STL_UpdateGCScanBitmap  (STLZoneObj    *pstZone,
                             BADDR          nLan,
                             BOOL32         bSet)
{
    UINT8      *pBitMap;
    UINT8       nBitMsk;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));
    FSR_ASSERT(OP_SUPPORT_PAGE_DELETE == 1);
    FSR_ASSERT(nLan < pstZone->pstZI->nNumLA);

    pBitMap = &(pstZone->pGCScanBitmap[nLan >> UINT8_SHIFT]);
    nBitMsk = (UINT8)(0x01 << (nLan & UINT8_MASK));

    /* Make the bitmap to 1 */
    if (bSet == TRUE32)
    {
        (*pBitMap) |= nBitMsk;
    }
    /* Make the bitmap to 0 */
    else
    {
        (*pBitMap) &= ~nBitMsk;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}


/**
 *  @brief      This function sets the GCLABitmap to 1 or 0.
 *
 *  @param[in]  pstZone    : STLZoneObj pointer
 *  @param[in]  nLan       : Address of a Local Area
 *  @param[in]  bSet       : TRUE32 or FALSE32
 *
 *  @return     FSR_STL_SUCCESS
 *
 *  @author     Kangho Roh
 *  @version    1.2.0
 */
PUBLIC INT32
FSR_STL_GC(STLZoneObj    *pstZone)
{
    const STLBMTHdl    *pstBMT          = pstZone->pstBMTHdl;
    const BADDR         nMaxLA          = pstZone->pstZI->nNumLA;
    const BADDR         nCurLA          = pstBMT->pstFm->nLan;
          BADDR         nTmpCurLA;
          UINT32        nStartOffs;
          BADDR         nVbn;
          UINT32        nEC             = (UINT32)(-1);
          BOOL32        bNeedStoreBMT   = FALSE32;
          INT32         nRet            = FSR_STL_SUCCESS;
          BOOL32        bRet;

    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));
    FSR_ASSERT(OP_SUPPORT_PAGE_DELETE == 1);
    FSR_ASSERT(nCurLA < nMaxLA);

    /* 
     * After FSR_STL_GC() starts, It scans every BMTHdr one by one, and
     * find the invalid blocks in each BMTHdr and push in free block list
     * until the free block list becomes full
     */

    if (pstZone->pstCtxHdl->pstFm->nNumIBlks == 0)
    {
        return nRet;
    }

    /* At first, scan the current BMT */
    nTmpCurLA  = nCurLA;
    nStartOffs = 0;
    while (TRUE32)
    {
        /* If the GCLABitmap of current LA is 0,
         * pass the current LA because it does not have any invalid blocks.
         */
        bRet = _GetGCLABitmap(pstZone, nTmpCurLA);
        if (bRet== FALSE32)
        {
            /* nTmpCurLA is increased by 1 */
            nTmpCurLA++;
            if (nTmpCurLA >= nMaxLA)
            {
                nTmpCurLA = nTmpCurLA - nMaxLA;
            }
            FSR_ASSERT(nTmpCurLA < nMaxLA);

            /* if every LA has been scanned, this function will go to final step. */
            if (nTmpCurLA == nCurLA)
            {
                break;
            }

            continue;
        }

        /* Find a invalid block in the BMTHdr with nTmpCurLA */
        nRet = _FindInvalidBlkInBMT(pstZone, nTmpCurLA, &nStartOffs, &nVbn, &nEC);
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }

        /* 
         * If there is no invalid block in this LA,
         * set the GCLABitmap of this LA to 0.
         */
        if (nVbn == NULL_VBN)
        {
            /* GCLABitmap of nTmpCurLA becomes 0.*/
            nRet = FSR_STL_UpdateGCScanBitmap(pstZone, nTmpCurLA, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                break;
            }

            if (bNeedStoreBMT == TRUE32)
            {
                /* reserve meta page */
                nRet = FSR_STL_ReserveMetaPgs(pstZone, 1, TRUE32);
                if (nRet != FSR_STL_SUCCESS)
                {
                    break;
                }

#if (OP_SUPPORT_DATA_WEAR_LEVELING == 1)
                /* Update MinEC */
                FSR_STL_UpdateBMTMinEC(pstZone);
#endif

                /* Store the BMT of the current LA */
                nRet = FSR_STL_StoreBMTCtx(pstZone, TRUE32);
                if (nRet != FSR_STL_SUCCESS)
                {
                    break;
                }

                /* reset the flag */
                bNeedStoreBMT = FALSE32;
            }

            /* In this case, every blocks in LA must be checked.
             * Thus, nInvBlkInx becomes 0
             */
            nStartOffs = 0;

            /* nTmpCurLA is increased by 1.
             * If nTmpCurLA == nCurLA, every BMT has been scanned.
             * Thus, go to the end of FSR_STL_GC()
             */
            nTmpCurLA++;
            if (nTmpCurLA >= nMaxLA)
            {
                nTmpCurLA = nTmpCurLA - nMaxLA;
            }
            FSR_ASSERT(nTmpCurLA < nMaxLA);

            if (nTmpCurLA == nCurLA)
            {
                break;
            }
        }
        /* If there is one or more invalid blocks,
         * inserts the blocks to free block list
         */
        else
        {
            /* Find EC of nInvBlk.
             * And push nInvBlk to the Free Block List.
             */
            FSR_STL_AddFreeList(pstZone, nVbn, nEC);
            FSR_ASSERT(nStartOffs >= 1);

            pstBMT->pGBlkFlags[(nStartOffs - 1) >> 3] &= ~(1 << ((nStartOffs - 1) & 0x07));

            /* Decrease number of idle data blocks */
            pstZone->pstCtxHdl->pstFm->nNumIBlks--;
            
            /* nVbn of the invalid block become NULL_VBN.
             * And update BMT Context.
             */
            pstBMT->pMapTbl[nStartOffs - 1].nVbn = NULL_VBN;

#if (OP_SUPPORT_DATA_WEAR_LEVELING == 1)
            /* Update MinEC */
            pstBMT->pDBlksEC[nStartOffs - 1] = 0xFFFFFFFF;
            FSR_STL_UpdateBMTMinEC(pstZone);
#endif  /* (OP_SUPPORT_DATA_WEAR_LEVELING == 1) */

            /* we need to store BMT/CXT out of this loop*/
            bNeedStoreBMT = TRUE32;

            bRet = FSR_STL_CheckFullFreeList(pstZone);
            if (bRet == TRUE32)
            {
                break;
            }

            if (pstZone->pstCtxHdl->pstFm->nNumIBlks == 0)
            {
                break;
            }
        }
    }

    while (nRet == FSR_STL_SUCCESS)
    {
        /* If bNeedStoreBMT == TRUE32, Store the BMT.*/
        if (bNeedStoreBMT == TRUE32)
        {
            /* reserve meta page */
            nRet = FSR_STL_ReserveMetaPgs(pstZone, 1, TRUE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                break;
            }

#if (OP_SUPPORT_DATA_WEAR_LEVELING == 1)
            /* Update MinEC */
            FSR_STL_UpdateBMTMinEC(pstZone);
#endif  /* (OP_SUPPORT_DATA_WEAR_LEVELING == 1) */

            /* Store BMT. */
            nRet = FSR_STL_StoreBMTCtx(pstZone, TRUE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                break;
            }
        }

        /* Return to the original BMT if nCurLA != nTmpCurLA */
        if (nCurLA != nTmpCurLA)
        {
            /* In this case, Load a BMT with nTmpCurLA. */
            nRet = FSR_STL_LoadBMT(pstZone, nTmpCurLA, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                break;
            }
        }

        /* For error handling */
        break;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : %x\r\n"), __FSR_FUNC__, nRet));

    return nRet;
}
