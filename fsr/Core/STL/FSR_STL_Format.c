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
 *  @file       FSR_STL_Format.c
 *  @brief      STL zone format
 *  @author     Jongtae Park
 *  @date       05-MAR-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  05-MAR-2007 [Jongtae Park]  : first writing
 *  @n  02-JUL-2007 [Wonmoon Cheon] : re-design for v1.2.0
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

PRIVATE INT32   _FormatZone        (STLZoneObj     *pstZone,
                                    STLFmtInfo     *pstFmt);

/*****************************************************************************/
/* Local (static)  Function Definition                                       */
/*****************************************************************************/

/**
 * @brief       This function formats a STL zone.
 * 
 * @param[in]   pstZone     : zone object pointer 
 *
 * @return      FSR_STL_SUCCESS
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PRIVATE INT32
_FormatZone        (STLZoneObj     *pstZone,
                    STLFmtInfo     *pstFmt)
{
    STLZoneInfo    *pstZI;
    STLDirHdrHdl   *pstDH;
    BADDR           nBlkOffs;
    BADDR           nVbn;
    BADDR           nLan;
    BOOL32          bMetaWL;
    INT32           nRet;
    UINT8          *pCurBuf;
#if (OP_SUPPORT_REMEMBER_ECNT == 1)
    STLBMTHdl      *pstBMT;
    UINT32          nNumDBlks;
#endif  /* (OP_SUPPORT_REMEMBER_ECNT == 1) */
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    nRet    = FSR_STL_SUCCESS;

    /* get object pointers */
    pstZI   = pstZone->pstZI;
    pstDH   = pstZone->pstDirHdrHdl;

    /* initialize zone object */
    FSR_STL_InitZone(pstZone);

#if (OP_SUPPORT_REMEMBER_ECNT == 1)
    /* Update Erase count of meta unit */
    for (nBlkOffs = 0; nBlkOffs < pstZI->nNumMetaBlks; nBlkOffs++)
    {
        nVbn = pstZI->aMetaVbnList[nBlkOffs];
        if (nVbn < pstFmt->nNumECnt)
        {
            pstDH->pMetaBlksEC[nBlkOffs] = (pstFmt->pnECnt[nVbn] & (~FSR_STL_META_MARK));
        }
        else
        {
            pstDH->pMetaBlksEC[nBlkOffs] = pstFmt->nAvgECnt;
        }
    }

    /* Update erase count of free unit */
    for (nBlkOffs = 0; nBlkOffs < pstZI->nNumFBlks; nBlkOffs++)
    {
        nVbn = pstZone->pstCtxHdl->pFreeList[nBlkOffs];
        if (nVbn < pstFmt->nNumECnt)
        {
            pstZone->pstCtxHdl->pFBlksEC[nBlkOffs] = (pstFmt->pnECnt[nVbn] & (~FSR_STL_META_MARK));
        }
        else
        {
            pstZone->pstCtxHdl->pFBlksEC[nBlkOffs] = pstFmt->nAvgECnt;
        }
    }
#endif  /* (OP_SUPPORT_REMEMBER_ECNT == 1) */

    do
    {
        /* erase all meta blocks */
        for (nBlkOffs = 1; nBlkOffs < pstZI->nNumMetaBlks; nBlkOffs++)
        {
            nVbn = *(pstZI->aMetaVbnList + nBlkOffs);

            nRet = FSR_STL_FlashErase(pstZone, nVbn);
            if (nRet != FSR_BML_SUCCESS)
            {
                break;
            }

            pstDH->pMetaBlksEC[nBlkOffs]++;
        }
        if (nRet != FSR_BML_SUCCESS)
        {
            break;
        }

        /* store directory header */
        nRet = FSR_STL_StoreDirHeader(pstZone, FALSE32, &bMetaWL);
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }

         /* store BMT+CTX to Flash */
#if (OP_SUPPORT_META_WEAR_LEVELING == 1)
         bMetaWL = TRUE32;
#else
         bMetaWL = FALSE32;
#endif

        /* store all BMTs */
        pCurBuf = pstZone->pFullBMTBuf;

        for (nLan = 0; nLan < pstZI->nNumLA; nLan++)
        {
            /* get BMT handle */
            FSR_STL_SetBMTHdl(pstZone, pstZone->pstBMTHdl, pCurBuf, pstZone->pstML->nBMTBufSize);

            FSR_STL_InitBMT(pstZone, pstZone->pstBMTHdl, nLan);

#if (OP_SUPPORT_REMEMBER_ECNT == 1)
            /* Update erase count */
            if (nLan ==(pstZI->nNumLA - 1))
            {
                /* the last LA */
                FSR_ASSERT(pstZI->nNumUserBlks > 0);
                nNumDBlks = ((pstZI->nNumUserBlks - 1) & (pstZI->nDBlksPerLA - 1)) + 1;
            }
            else
            {
                nNumDBlks = pstZI->nDBlksPerLA;
            }

            for (nBlkOffs = 0; nBlkOffs < nNumDBlks; nBlkOffs++)
            {
                pstBMT = pstZone->pstBMTHdl;
                nVbn   = pstBMT->pMapTbl[nBlkOffs].nVbn;
                if (nVbn < pstFmt->nNumECnt)
                {
                    pstBMT->pDBlksEC[nBlkOffs] = (pstFmt->pnECnt[nVbn] & (~FSR_STL_META_MARK));
                }
                else
                {
                    pstBMT->pDBlksEC[nBlkOffs] = pstFmt->nAvgECnt;
                }
            }

            /* Update minimum erase count */
            FSR_STL_UpdateBMTMinEC(pstZone);

#endif  /* (OP_SUPPORT_REMEMBER_ECNT == 1) */

            nRet = FSR_STL_StoreBMTCtx(pstZone, bMetaWL);
            if (nRet != FSR_STL_SUCCESS)
            {
                break;
            }

            /* set next BMT buffer pointer */;
            pCurBuf += pstZone->pstML->nBMTBufSize;
        }


#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
        nVbn = pstZone->pstCtxHdl->pstFm->nBBlkVbn;

#if (OP_SUPPORT_REMEMBER_ECNT == 1)
        /* Update erase count */
        if (nVbn < pstFmt->nNumECnt)
        {
            pstZone->pstCtxHdl->pstFm->nBBlkEC = (pstFmt->pnECnt[nVbn] & (~FSR_STL_META_MARK));
        }
        else
        {
            pstZone->pstCtxHdl->pstFm->nBBlkEC = pstFmt->nAvgECnt;
        }
#endif  /* (OP_SUPPORT_REMEMBER_ECNT == 1) */

        /* Erase buffer block */
        if (nVbn != NULL_VBN)
        {
            nRet = FSR_STL_FlashErase(pstZone, nVbn);
            if (nRet != FSR_BML_SUCCESS)
            {
                break;
            }

            pstZone->pstCtxHdl->pstFm->nBBlkEC++;
        }
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */

        nRet = FSR_STL_SUCCESS;
    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}

/*****************************************************************************/
/* Global Function Definition                                                */
/*****************************************************************************/

/**
 * @brief       This function formats a STL cluster.
 * 
 * @param[in]   nClstID         : cluster id
 * @param[in]   nStartVbn       : start virtual block address
 * @param[in]   nNumZones       : total number of zones
 * @param[in]   pnBlksPerZone   : array of number of blocks per a zone
 *
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_INVALID_PARAM
 *
 * @author      Jongtae Park, Wonmoon Cheon
 * @version     1.1.0
 *
 */
PUBLIC INT32
FSR_STL_FormatCluster  (UINT32          nClstID,
                        UINT16          nStartVbn,
                        UINT32          nNumZones,
                        UINT16         *pnBlksPerZone,
                        STLFmtInfo     *pstFmt)
{
    UINT16          nIdx;
    BADDR           nVbn;
    STLClstObj     *pstClst;
    STLZoneObj     *pstZone;
    STLRootInfo    *pstRI       = NULL;
    INT32           nRet;
    UINT16          nBlksPerZone;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    nRet = FSR_STL_SUCCESS;
    pstClst = NULL;

    do
    {
        /* cluster id check */
        if (nClstID >= MAX_NUM_CLUSTERS)
        {
            nRet = FSR_STL_INVALID_PARAM;
            break;
        }
        
        /* get object pointer */
        pstClst = gpstSTLClstObj[nClstID];
        if (pstClst == NULL)
        {
            nRet = FSR_STL_INVALID_PARAM;
            break;
        }

        /* get root info object pointer */
        pstRI       = &(pstClst->stRootInfoBuf.stRootInfo);

        /* set start VBN of this cluster */
        pstRI->nRootStartVbn = nStartVbn;

        /* allocate & initialize all zone objects */
        nVbn = nStartVbn + MAX_ROOT_BLKS;
        for (nIdx = 0; nIdx < nNumZones; nIdx++)
        {
            /* get zone object */
            pstZone = &(pstClst->stZoneObj[nIdx]);

            /* setup zone info parameters */
            nBlksPerZone = *(pnBlksPerZone + nIdx);
            if (nIdx == 0)  /* only if it is master zone */
            {
                /* reduce by the number of root blocks */
                nBlksPerZone -= MAX_ROOT_BLKS;
            }

            if ((nBlksPerZone > pstZone->pstML->nMaxBlksPerZone) || 
                (nBlksPerZone < (MIN_META_BLKS + MAX_TOTAL_FBLKS + NUM_BU_BLKS + 1)))
            {
                nRet = FSR_STL_INVALID_PARAM;
                break;
            }
            

            nRet = FSR_STL_SetupZoneInfo(pstClst, pstZone, nVbn, nBlksPerZone);
            if (nRet != FSR_STL_SUCCESS)
            {
                break;
            }
            
            /* zone memory allocation */
            nRet = FSR_STL_AllocZoneMem(pstZone);
            if (nRet != FSR_STL_SUCCESS)
            {
                return nRet;
            }

            /* calculate start VBN of the next zone */
            nVbn = nVbn + nBlksPerZone;
        }

        /* check error */
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }

        /* get first zone object */
        pstZone = &(pstClst->stZoneObj[0]);

#if (OP_SUPPORT_REMEMBER_ECNT == 1)
        /* Update Erase count */
        for (nIdx = 0; nIdx < MAX_ROOT_BLKS; nIdx++)
        {
            if (nIdx < pstFmt->nNumECnt)
            {
                pstRI->aRootBlkEC[nIdx] = (pstFmt->pnECnt[nIdx] & (~FSR_STL_META_MARK));
            }
            else
            {
                pstRI->aRootBlkEC[nIdx] = pstFmt->nAvgECnt;
            }
        }
#endif  /* (OP_SUPPORT_REMEMBER_ECNT == 1) */

        /* erase all root blocks */
        for (nIdx = 1; nIdx < MAX_ROOT_BLKS; nIdx++)
        {
            nVbn = pstRI->nRootStartVbn + nIdx;
        
            nRet = FSR_STL_FlashErase(pstZone, nVbn);
            if (nRet != FSR_BML_SUCCESS)
            {
                break;
            }

            /*  Update Root block EC */
            pstRI->aRootBlkEC[nIdx]++;
        }

        /* check error */
        if (nRet != FSR_BML_SUCCESS)
        {
            break;
        }

        /* set number of zones */
        pstRI->nNumZone = (UINT16)nNumZones;
        
        /* store root info & zone info into root block */
        nRet = FSR_STL_StoreRootInfo(pstClst);
        if (nRet != FSR_STL_SUCCESS)
        {
            break;
        }

        /* format all zones */
        for (nIdx = 0; nIdx < nNumZones; nIdx++)
        {
            /* get zone object */
            pstZone = &(pstClst->stZoneObj[nIdx]);

            /* format a zone */
            nRet = _FormatZone(pstZone, pstFmt);
            if (nRet != FSR_STL_SUCCESS)
            {
                break;
            }
        }

    } while (0);

    /* Release allocated memory */
    if (pstClst != NULL)
    {
        for (nIdx = 0; nIdx < nNumZones; nIdx++)
        {
            pstZone = &(pstClst->stZoneObj[nIdx]);
            FSR_STL_FreeZoneMem(pstZone);
        }
    }

    /* if it fails  clear number of zones */
    if (nRet != FSR_STL_SUCCESS)
    {
        if (pstRI != NULL)
        {
            pstRI->nNumZone = 0;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}
