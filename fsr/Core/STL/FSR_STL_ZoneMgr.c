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
 *  @file       FSR_STL_ZoneMgr.c
 *  @brief      This function has functions related management a Zone.
 *  @author     Wonmoon Cheon
 *  @date       5-MAR-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  05-MAR-2007 [Wonmoon Cheon] : first writing
 *  @n  07-MAR-2007 [Kangho Roh]    : Implement DoGC() and some other functions.
 *  @n  02-JUL-2007 [Wonmoon Cheon] : re-design for v1.2.0
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

/*****************************************************************************/
/* Local (static)  Function Definition                                       */
/*****************************************************************************/

/*****************************************************************************/
/* Global Function Definition                                                */
/*****************************************************************************/

/**
 * @brief       This function resets all zone objects' members.
 *
 * @param[in]   nVolID      : volume id
 * @param[in]   pstDevInfo  : device info
 * @param[in]   pstRI       : root info
 * @param[in]   pstML       : meta layout
 * @param[in]   pstZone     : zone object to initialize.
 *
 * @return      none
 *
 * @author      Wonmoon Cheon
 * @version     1.2.0
 *
 */
PUBLIC VOID
FSR_STL_ResetZoneObj   (UINT32          nVolID,
                        RBWDevInfo     *pstDevInfo,
                        STLRootInfo    *pstRI,
                        STLMetaLayout  *pstML,
                        STLZoneObj     *pstZone)
{
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    pstZone->bOpened            = FALSE32;
    pstZone->nVolID             = nVolID;
    pstZone->pstDevInfo         = pstDevInfo;
    pstZone->pstRI              = pstRI;
    pstZone->pstML              = pstML;
    pstZone->pstZI              = &(pstRI->aZone[pstZone->nZoneID]);

    /* Initialize every pointer variables. */
    pstZone->pstDirHdrHdl       = NULL;
    pstZone->pDirHdrBuf         = NULL;
    pstZone->pstCtxHdl          = NULL;
    pstZone->pstBMTHdl          = NULL;
    pstZone->pstPMTHdl          = NULL;
    pstZone->pMetaPgBuf         = NULL;
    pstZone->pTempMetaPgBuf     = NULL;
    pstZone->pstActLogGrpList   = NULL;
    pstZone->pstActLogGrpPool   = NULL;
    pstZone->pActLogGrpPoolBuf  = NULL;
    pstZone->pstInaLogGrpCache  = NULL;
    pstZone->pstInaLogGrpPool   = NULL;
    pstZone->pInaLogGrpPoolBuf  = NULL;
    pstZone->pGCScanBitmap      = NULL;

#if (OP_SUPPORT_PAGE_DELETE == 1)
    /* Initialize deleted sector info */
    pstZone->pstDelCtxObj       = NULL;
#endif  /* (OP_SUPPORT_PAGE_DELETE == 1) */

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    /* Initialize BU info */
    pstZone->pstBUCtxObj        = NULL;
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */

    pstZone->pFullBMTBuf        = NULL;

#if (OP_SUPPORT_STATISTICS_INFO == 1)
    pstZone->pstStats           = NULL;
#endif  /* (OP_SUPPORT_STATISTICS_INFO == 1) */

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}


/**
 * @brief       This function allocates RAM memory for zone instance
 *
 * @param[in]   pstZone     : pointer to zone object
 *
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_OUT_OF_MEMORY
 *
 * @author      Jongtae Park, Wonmoon Cheon
 * @version     1.2.0
 *
 */
PUBLIC INT32
FSR_STL_AllocZoneMem(STLZoneObj *pstZone)
{
    INT32           nRet        = FSR_STL_SUCCESS;
    UINT32          nDramSize   = 0;
    UINT32          nSramSize   = 0;
    UINT32          nSize       = 0;
    STLMetaLayout  *pstML;
    STLZoneInfo    *pstZI;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    FSR_ASSERT(pstZone != NULL);
    FSR_ASSERT(pstZone->pstZI != NULL);

    /* get info object pointers */
    pstML = pstZone->pstML;
    pstZI = pstZone->pstZI;

    /* actually loop repetition : just one time */
    while (pstZone->pstDirHdrHdl == NULL)
    {
        /* directory header handle object */
        nSize = sizeof(STLDirHdrHdl);
        pstZone->pstDirHdrHdl = (STLDirHdrHdl*)FSR_STL_MALLOC(nSize,
                                        FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_SRAM);
        if (pstZone->pstDirHdrHdl == NULL)
        {
            nRet = FSR_STL_OUT_OF_MEMORY;
            break;
        }
        else if (((UINT32)(pstZone->pstDirHdrHdl)) & 0x03)
        {
            nRet = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }

        nSramSize += nSize;

        /* directory header buffer */
        nSize = pstML->nDirHdrBufSize;
        pstZone->pDirHdrBuf = (UINT8*)FSR_STL_MALLOC(nSize,
                                        FSR_STL_MEM_NONCACHEABLE, FSR_STL_MEM_DRAM);
        if (pstZone->pDirHdrBuf == NULL)
        {
            nRet = FSR_STL_OUT_OF_MEMORY;
            break;
        }
        else if (((UINT32)(pstZone->pDirHdrBuf)) & 0x03)
        {
            nRet = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }

        nDramSize += nSize;

        /* context info handle object */
        nSize = sizeof(STLCtxInfoHdl);
        pstZone->pstCtxHdl = (STLCtxInfoHdl*)FSR_STL_MALLOC(nSize,
                                        FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_SRAM);
        if (pstZone->pstCtxHdl == NULL)
        {
            nRet = FSR_STL_OUT_OF_MEMORY;
            break;
        }
        else if (((UINT32)(pstZone->pstCtxHdl)) & 0x03)
        {
            nRet = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }

        nSramSize += nSize;

        /* BMT handle object */
        nSize = sizeof(STLBMTHdl);
        pstZone->pstBMTHdl = (STLBMTHdl*)FSR_STL_MALLOC(nSize,
                                        FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_SRAM);
        if (pstZone->pstBMTHdl == NULL)
        {
            nRet = FSR_STL_OUT_OF_MEMORY;
            break;
        }
        else if (((UINT32)(pstZone->pstBMTHdl)) & 0x03)
        {
            nRet = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }

        nSramSize += nSize;

        /* PMT handle object */
        nSize = sizeof(STLPMTHdl);
        pstZone->pstPMTHdl = (STLPMTHdl*)FSR_STL_MALLOC(nSize,
                                        FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_SRAM);
        if (pstZone->pstPMTHdl == NULL)
        {
            nRet = FSR_STL_OUT_OF_MEMORY;
            break;
        }
        else if (((UINT32)(pstZone->pstPMTHdl)) & 0x03)
        {
            nRet = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }

        nSramSize += nSize;

        /* meta page buffer */
        nSize = pstML->nBMTBufSize + pstML->nCtxBufSize + pstML->nPMTBufSize;
        pstZone->pMetaPgBuf = (UINT8*)FSR_STL_MALLOC(nSize,
                                        FSR_STL_MEM_NONCACHEABLE, FSR_STL_MEM_DRAM);
        if (pstZone->pMetaPgBuf == NULL)
        {
            nRet = FSR_STL_OUT_OF_MEMORY;
            break;
        }
        else if (((UINT32)(pstZone->pMetaPgBuf)) & 0x03)
        {
            nRet = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }

        nDramSize += nSize;

        /* temp meta page buffer */
        pstZone->pTempMetaPgBuf = (UINT8*)FSR_STL_MALLOC(nSize,
                                        FSR_STL_MEM_NONCACHEABLE, FSR_STL_MEM_DRAM);
        if (pstZone->pTempMetaPgBuf == NULL)
        {
            nRet = FSR_STL_OUT_OF_MEMORY;
            break;
        }
        else if (((UINT32)(pstZone->pTempMetaPgBuf)) & 0x03)
        {
            nRet = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }

        nDramSize += nSize;

        /* active log group list */
        nSize = sizeof(STLLogGrpList);
        pstZone->pstActLogGrpList = (STLLogGrpList*)FSR_STL_MALLOC(nSize,
                                        FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_SRAM);
        if (pstZone->pstActLogGrpList == NULL)
        {
            nRet = FSR_STL_OUT_OF_MEMORY;
            break;
        }
        else if (((UINT32)(pstZone->pstActLogGrpList)) & 0x03)
        {
            nRet = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }

        nSramSize += nSize;

        /* active log group pool - handle */
        nSize = sizeof(STLLogGrpHdl) * ACTIVE_LOG_GRP_POOL_SIZE;
        pstZone->pstActLogGrpPool = (STLLogGrpHdl*)FSR_STL_MALLOC(nSize,
                                        FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_SRAM);
        if (pstZone->pstActLogGrpPool == NULL)
        {
            nRet = FSR_STL_OUT_OF_MEMORY;
            break;
        }
        else if (((UINT32)(pstZone->pstActLogGrpPool)) & 0x03)
        {
            nRet = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }

        nSramSize += nSize;

        /* active log group pool - buffer */
        nSize = pstML->nBytesPerLogGrp * ACTIVE_LOG_GRP_POOL_SIZE;
        pstZone->pActLogGrpPoolBuf = (UINT8*)FSR_STL_MALLOC(nSize,
                                        FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_SRAM);
        if (pstZone->pActLogGrpPoolBuf == NULL)
        {
            nRet = FSR_STL_OUT_OF_MEMORY;
            break;
        }
        else if (((UINT32)(pstZone->pActLogGrpPoolBuf)) & 0x03)
        {
            nRet = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }

        nSramSize += nSize;

        /* inactive log group cache */
        nSize = sizeof(STLLogGrpList);
        pstZone->pstInaLogGrpCache = (STLLogGrpList*)FSR_STL_MALLOC(nSize,
                                        FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_SRAM);
        if (pstZone->pstInaLogGrpCache == NULL)
        {
            nRet = FSR_STL_OUT_OF_MEMORY;
            break;
        }
        else if (((UINT32)(pstZone->pstInaLogGrpCache)) & 0x03)
        {
            nRet = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }

        nSramSize += nSize;

        /* inactive log group cache - handle */
        nSize = sizeof(STLLogGrpHdl) * INACTIVE_LOG_GRP_POOL_SIZE;
        pstZone->pstInaLogGrpPool = (STLLogGrpHdl*)FSR_STL_MALLOC(nSize,
                                        FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_SRAM);
        if (pstZone->pstInaLogGrpPool == NULL)
        {
            nRet = FSR_STL_OUT_OF_MEMORY;
            break;
        }
        else if (((UINT32)(pstZone->pstInaLogGrpPool)) & 0x03)
        {
            nRet = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }

        nSramSize += nSize;

        /* inactive log group cache - buffer */
        nSize = pstML->nBytesPerLogGrp * INACTIVE_LOG_GRP_POOL_SIZE;
        pstZone->pInaLogGrpPoolBuf = (UINT8*)FSR_STL_MALLOC(nSize,
                                        FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_SRAM);
        if (pstZone->pInaLogGrpPoolBuf == NULL)
        {
            nRet = FSR_STL_OUT_OF_MEMORY;
            break;
        }
        else if (((UINT32)(pstZone->pInaLogGrpPoolBuf)) & 0x03)
        {
            nRet = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }

        nSramSize += nSize;
        nSramSize += nSize;

        /* GC scan bitmap */
        nSize = (pstZI->nNumLA >> 3) + 1;
        if ((nSize & 0x03) != 0)
        {
            nSize = (nSize + 4) & (~(0x03));
        }
        pstZone->pGCScanBitmap = (UINT8*)FSR_STL_MALLOC(nSize,
                                        FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_SRAM);
        if (pstZone->pGCScanBitmap == NULL)
        {
            nRet = FSR_STL_OUT_OF_MEMORY;
            break;
        }
        else if (((UINT32)(pstZone->pGCScanBitmap)) & 0x03)
        {
            nRet = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }

        nSramSize += nSize;

#if (OP_SUPPORT_PAGE_DELETE == 1)
        /* Deleted context info */
        nSize = sizeof(STLDelCtxObj);
        pstZone->pstDelCtxObj = (STLDelCtxObj*)FSR_STL_MALLOC(nSize,
                                        FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_SRAM);
        if (pstZone->pstDelCtxObj == NULL)
        {
            nRet = FSR_STL_OUT_OF_MEMORY;
            break;
        }
        else if (((UINT32)(pstZone->pstDelCtxObj)) & 0x03)
        {
            nRet = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }

        nSramSize += nSize;
#endif  /* (OP_SUPPORT_PAGE_DELETE == 1) */

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
        /* BU context info */
        nSize = sizeof(STLBUCtxObj);
        pstZone->pstBUCtxObj = (STLBUCtxObj*)FSR_STL_MALLOC(nSize,
                                        FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_SRAM);
        if (pstZone->pstBUCtxObj == NULL)
        {
            nRet = FSR_STL_OUT_OF_MEMORY;
            break;
        }
        else if (((UINT32)(pstZone->pstBUCtxObj)) & 0x03)
        {
            nRet = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }

        nSramSize += nSize;
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */

        /* when every BMTs are in RAM */
        nSize = pstML->nBMTBufSize * pstZI->nNumLA;
        pstZone->pFullBMTBuf = (UINT8*)FSR_STL_MALLOC(nSize,
                                        FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_DRAM);
        if (pstZone->pFullBMTBuf == NULL)
        {
            nRet = FSR_STL_OUT_OF_MEMORY;
            break;
        }
        else if (((UINT32)(pstZone->pFullBMTBuf)) & 0x03)
        {
            nRet = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }

        nDramSize += nSize;

#if (OP_SUPPORT_STATISTICS_INFO == 1)
        /*  Statistics information */
        nSize = sizeof(STLStats);
        pstZone->pstStats = (STLStats *)FSR_STL_MALLOC(nSize,
                                     FSR_STL_MEM_NONCACHEABLE, FSR_STL_MEM_DRAM);
        if (pstZone->pstStats == NULL)
        {
            nRet = FSR_STL_OUT_OF_MEMORY;
            break;
        }
        else if (((UINT32)(pstZone->pstStats)) & 0x03)
        {
            nRet = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }

        nDramSize += nSize;
#endif  /* (OP_SUPPORT_STATISTICS_INFO == 1) */

        /* exit while() loop */
        break;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:DBG] SRAM Allocation Size : %d\r\n"), nSramSize));
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:DBG] DRAM Allocation Size : %d\r\n"), nDramSize));

    if (nRet != FSR_STL_SUCCESS)
    {
        FSR_STL_FreeZoneMem(pstZone);
        if (nRet == FSR_STL_OUT_OF_MEMORY)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR]    Insufficient memory!\r\n")));
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}


/**
 * @brief       This function frees allocated memory of zone object
 *
 * @param[in]   pstZone     : pointer to zone object
 *
 * @return      none
 *
 * @author      Jongtae Park, Wonmoon Cheon
 * @version     1.2.0
 *
 */
PUBLIC VOID
FSR_STL_FreeZoneMem(STLZoneObj *pstZone)
{
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    if (pstZone == NULL)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
        return;
    }

    /*  free all allocated memory one by one */
    if (pstZone->pstDirHdrHdl != NULL)
    {
        /* free object of directory header handle */
        FSR_OAM_Free(pstZone->pstDirHdrHdl);
        pstZone->pstDirHdrHdl = NULL;
    }

    if (pstZone->pDirHdrBuf != NULL)
    {
        /* free directory header pages buffer */
        FSR_OAM_Free(pstZone->pDirHdrBuf);
        pstZone->pDirHdrBuf = NULL;
    }

    if (pstZone->pstCtxHdl != NULL)
    {
        /* free object of context info handle */
        FSR_OAM_Free(pstZone->pstCtxHdl);
        pstZone->pstCtxHdl = NULL;
    }

    if (pstZone->pstBMTHdl != NULL)
    {
        /* free object of BMT handle */
        FSR_OAM_Free(pstZone->pstBMTHdl);
        pstZone->pstBMTHdl = NULL;
    }

    if (pstZone->pstPMTHdl != NULL)
    {
        /* free object of PMT handle */
        FSR_OAM_Free(pstZone->pstPMTHdl);
        pstZone->pstPMTHdl = NULL;
    }

    if (pstZone->pMetaPgBuf != NULL)
    {
        /* free meta page buffer */
        FSR_OAM_Free(pstZone->pMetaPgBuf);
        pstZone->pMetaPgBuf = NULL;
    }

    if (pstZone->pTempMetaPgBuf != NULL)
    {
        /* free temp meta page buffer */
        FSR_OAM_Free(pstZone->pTempMetaPgBuf);
        pstZone->pTempMetaPgBuf = NULL;
    }

    if (pstZone->pstActLogGrpList != NULL)
    {
        /* free active log group list */
        FSR_OAM_Free(pstZone->pstActLogGrpList);
        pstZone->pstActLogGrpList = NULL;
    }

    if (pstZone->pstActLogGrpPool != NULL)
    {
        /* free active log group pool - handle */
        FSR_OAM_Free(pstZone->pstActLogGrpPool);
        pstZone->pstActLogGrpPool = NULL;
    }

    if (pstZone->pActLogGrpPoolBuf != NULL)
    {
        /* free active log group pool - buffer */
        FSR_OAM_Free(pstZone->pActLogGrpPoolBuf);
        pstZone->pActLogGrpPoolBuf = NULL;
    }

    if (pstZone->pstInaLogGrpCache != NULL)
    {
        /* free inactive log group cache */
        FSR_OAM_Free(pstZone->pstInaLogGrpCache);
        pstZone->pstInaLogGrpCache = NULL;
    }

    if (pstZone->pstInaLogGrpPool != NULL)
    {
        /* free inactive log group cache - handle */
        FSR_OAM_Free(pstZone->pstInaLogGrpPool);
        pstZone->pstInaLogGrpPool = NULL;
    }

    if (pstZone->pInaLogGrpPoolBuf != NULL)
    {
        /* free inactive log group cache - buffer */
        FSR_OAM_Free(pstZone->pInaLogGrpPoolBuf);
        pstZone->pInaLogGrpPoolBuf = NULL;
    }

    if (pstZone->pGCScanBitmap != NULL)
    {
        /* free GC scan bitmap */
        FSR_OAM_Free(pstZone->pGCScanBitmap);
        pstZone->pGCScanBitmap = NULL;
    }

#if (OP_SUPPORT_PAGE_DELETE == 1)
    if (pstZone->pstDelCtxObj != NULL)
    {
        /* free deleted context info */
        FSR_OAM_Free(pstZone->pstDelCtxObj);
        pstZone->pstDelCtxObj = NULL;
    }
#endif  /* (OP_SUPPORT_PAGE_DELETE == 1) */

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    if (pstZone->pstBUCtxObj != NULL)
    {
        /* free BU context info */
        FSR_OAM_Free(pstZone->pstBUCtxObj);
        pstZone->pstBUCtxObj = NULL;
    }
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */

    if (pstZone->pFullBMTBuf != NULL)
    {
        /* free full BMT buffer */
        FSR_OAM_Free(pstZone->pFullBMTBuf);
        pstZone->pFullBMTBuf = NULL;
    }

#if (OP_SUPPORT_STATISTICS_INFO == 1)
    if (pstZone->pstStats != NULL)
    {
        /* free statistics information memory */
        FSR_OAM_Free(pstZone->pstStats);
        pstZone->pstStats = NULL;
    }
#endif  /* (OP_SUPPORT_STATISTICS_INFO == 1) */

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}

/**
 * @brief       This function initializes zone information.
 *
 * @param[in]   pstZI     : pointer to zone info object
 *
 * @return      none
 *
 * @author      Wonmoon Cheon
 * @version     1.2.0
*
*/
PUBLIC VOID
FSR_STL_InitZoneInfo   (STLZoneInfo    *pstZI)
{
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* initialize all members to zero */
    FSR_OAM_MEMSET(pstZI, 0x00, sizeof(STLZoneInfo));

    /* initialize the list of meta blocks  */
    FSR_OAM_MEMSET(pstZI->aMetaVbnList, 0xFF, sizeof(BADDR) * MAX_META_BLKS);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}

/**
 * @brief       This function sets the zone range with the specified parameters.
 *
 * @param[in]   pstZone     : pointer to zone object.
 * @param[in]   nStartVbn   : VBN of the first block in a zone.
 * @param[in]   nNumBlks    : number of total virtual blocks to setup.
 *
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_INVALID_PARAM
 * @return      FSR_STL_ERR_UNKNOWN_META_FORMAT
 *
 * @author      Wonmoon Cheon
 * @version     1.2.0
 *
 */
PUBLIC INT32
FSR_STL_SetupZoneInfo  (STLClstObj *pstClst,
                        STLZoneObj *pstZone,
                        UINT16      nStartVbn,
                        UINT16      nNumBlks)
{
    STLZoneInfo    *pstZI;
    STLRootInfo    *pstRI;
    UINT32          nIdx;
    UINT32          nMetaFormat;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get object pointer */
    pstRI = &(pstClst->stRootInfoBuf.stRootInfo);
    pstZI = pstZone->pstZI;

    /*   Overall Block Layout
         _______________________
        | Root blocks           |
        |_______________________|
        |                       |
        | Meta blocks           |
        |-----------------------|
        | Buffer blocks (opt)   |
        |-----------------------|
        | Temporary blocks (opt)|
        |-----------------------|
        | Initial Free blocks   |
        |-----------------------|
        | User data blocks      |
        |                       |
        |_______________________|
     */

    /* StartVbn and nNumTotalBlk Setting.     */
    pstZI->nStartVbn          = nStartVbn;
    pstZI->nNumTotalBlks      = nNumBlks;

    /* The number of meta blocks must be larger than 1. */
    /* 2 <= nNumMetaBlks <= MAX_META_BLKS */
    pstZI->nNumMetaBlks       = (UINT16)(nNumBlks * META_BLOCK_RATIO / 1000 + 1);
    if (((pstZI->nNumMetaBlks) & 0x01) != 0)
    {
        pstZI->nNumMetaBlks++;
    }
    if (pstZI->nNumMetaBlks > MAX_META_BLKS)
    {
        pstZI->nNumMetaBlks = MAX_META_BLKS;
    }
    else if (pstZI->nNumMetaBlks < MIN_META_BLKS)
    {
        pstZI->nNumMetaBlks = MIN_META_BLKS;
    }

    /* Note that stMetaLayout has been configured before the following statement.
       The nDirHdrBufSize must be multiple of virtual page size. */
    pstZI->nNumDirHdrPgs      = (UINT16)pstClst->stML.nDirHdrBufSize /
                                (UINT16)pstZone->pstDevInfo->nBytesPerVPg;

    /* multiple of 2 */
    pstZI->nNumLogGrpPerPMT   = ((UINT16)pstClst->stML.nPMTBufSize /
                                    (UINT16)pstClst->stML.nBytesPerLogGrp) >> 1;
    pstZI->nNumLogGrpPerPMT   <<= 1;

    /* number of initial free blocks */
    pstZI->nNumFBlks          = MAX_TOTAL_FBLKS;

    /* number of user accessible blocks */
    pstZI->nNumUserBlks       = pstZI->nNumTotalBlks
                              - pstZI->nNumMetaBlks
                              - pstZI->nNumFBlks;

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    /* there is always only one buffer block */
    pstZI->nNumUserBlks      -= NUM_BU_BLKS;
#endif

    /* number of user accessible pages */
    pstZI->nNumUserPgs        = pstZI->nNumUserBlks
                                  * pstZone->pstDevInfo->nPagesPerSBlk;

    /* number of user accessible sectors */
    pstZI->nNumUserScts       = pstZI->nNumUserBlks
                                  * pstZone->pstDevInfo->nSecPerSBlk;

    /* max number of inactive log group entries in PMT directory */
    if (pstClst->stML.nMaxInaLogGrps > (UINT32)(pstZI->nNumUserBlks >> pstRI->nNShift))
    {
        pstZI->nMaxPMTDirEntry = (pstZI->nNumUserBlks >> pstRI->nNShift);
    }
    else
    {
        pstZI->nMaxPMTDirEntry = (UINT16)pstClst->stML.nMaxInaLogGrps;
    }

    /* number of blocks per local area (BMT) */
    nMetaFormat = GET_META_PAGE_SIZE(pstRI->nMetaVersion);
    switch (nMetaFormat)
    {
        case 0x02 :     /* 2KB page format */
            pstZI->nDBlksPerLA = META_2KB_DBLKS_PER_LA;
            break;
        case 0x04 :     /* 4KB page format */
            pstZI->nDBlksPerLA = META_4KB_DBLKS_PER_LA;
            break;
        case 0x08 :     /* 8KB page format */
            pstZI->nDBlksPerLA = META_8KB_DBLKS_PER_LA;
            break;
        case 0x10 :     /* 16KB page format */
            pstZI->nDBlksPerLA = META_16KB_DBLKS_PER_LA;
            break;
        default :
            return FSR_STL_CRITICAL_ERROR;
    }

    /* bit shift value */
    pstZI->nDBlksPerLAShift = FSR_STL_GetShiftBit(pstZI->nDBlksPerLA);

    /* number of total LA */
    pstZI->nNumLA = (UINT16)(((pstZI->nNumUserBlks - 1) >> pstZI->nDBlksPerLAShift) + 1);

    /* Initially, the first nNumMetaBlks blocks are assigned as meta blocks in the zone */
    for (nIdx = 0; nIdx < pstZI->nNumMetaBlks; nIdx++)
    {
        pstZI->aMetaVbnList[nIdx] = (BADDR)((pstZI->nStartVbn) + nIdx);
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}

/**
 * @brief       This function initializes all global objects in a zone.
 *
 * @param[in]   pstZone     : pointer to zone object
 *
 * @return      none
 *
 * @author      Jongtae Park, Wonmoon Cheon
 * @version     1.1.0
 *
 */
PUBLIC VOID
FSR_STL_InitZone(STLZoneObj *pstZone)
{
    UINT32          nIdx;
    STLMetaLayout   *pstML;
    STLLogGrpHdl    *pstLogGrp;
    UINT8           *pCurBuf;
    BADDR           nLan;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    FSR_ASSERT(pstZone != NULL);
    FSR_ASSERT(pstZone->pstDirHdrHdl != NULL);

    /* get meta layout pointer */
    pstML = pstZone->pstML;
   
    /* initialization of directory header */
    FSR_STL_SetDirHdrHdl(pstZone, pstZone->pstDirHdrHdl, 
                         pstZone->pDirHdrBuf, 
                         pstML->nDirHdrBufSize);
    FSR_STL_InitDirHdr(pstZone, pstZone->pstDirHdrHdl);

    /* BMT initialization */
    FSR_STL_SetBMTHdl(pstZone, pstZone->pstBMTHdl, 
                          pstZone->pMetaPgBuf, 
                          pstML->nBMTBufSize);
    FSR_STL_InitBMT(pstZone, pstZone->pstBMTHdl, 0);
        
    /* Context initialization */
    FSR_STL_SetCtxInfoHdl(pstZone, pstZone->pstCtxHdl, 
                          pstZone->pMetaPgBuf + pstML->nBMTBufSize,
                          pstML->nCtxBufSize);
    FSR_STL_InitCtx(pstZone, pstZone->pstCtxHdl);

    /* PMT initialization */
    FSR_STL_SetPMTHdl(pstZone, pstZone->pstPMTHdl, 
                          pstZone->pMetaPgBuf + pstML->nBMTBufSize + pstML->nCtxBufSize, 
                          pstML->nPMTBufSize);

    /* active log group list initialization */
    FSR_STL_InitLogGrpList(pstZone->pstActLogGrpList);

    /* active log group pool initialization */
    pCurBuf = pstZone->pActLogGrpPoolBuf;
    for (nIdx = 0; nIdx < ACTIVE_LOG_GRP_POOL_SIZE; nIdx++)
    {
        /* get log group handle */
        pstLogGrp = pstZone->pstActLogGrpPool + nIdx;
        
        FSR_STL_SetLogGrpHdl(pstZone, pstLogGrp, pCurBuf, pstML->nBytesPerLogGrp);
        FSR_STL_InitLogGrp(pstZone, pstLogGrp);

        /* set next log group buffer pointer */;
        pCurBuf += pstML->nBytesPerLogGrp;
    }
    
    /* inactive log group cache initialization */
    FSR_STL_InitLogGrpList(pstZone->pstInaLogGrpCache);

    /* inactive log group pool initialization */
    pCurBuf = pstZone->pInaLogGrpPoolBuf;
    for (nIdx = 0; nIdx < INACTIVE_LOG_GRP_POOL_SIZE; nIdx++)
    {
        /* get log group handle */
        pstLogGrp = pstZone->pstInaLogGrpPool + nIdx;
        
        FSR_STL_SetLogGrpHdl(pstZone, pstLogGrp, pCurBuf, pstML->nBytesPerLogGrp);
        FSR_STL_InitLogGrp(pstZone, pstLogGrp);
        
        /* set next log group buffer pointer */;
        pCurBuf += pstML->nBytesPerLogGrp;
    }

    /* initialization of GC scan bitmap */
    FSR_OAM_MEMSET(pstZone->pGCScanBitmap, 0xFF, (pstZone->pstZI->nNumLA >> 3) + 1); 

    /* initialization of deleted sector context info */
#if (OP_SUPPORT_PAGE_DELETE == 1)
    FSR_STL_InitDelCtx(pstZone->pstDelCtxObj);
#endif  /* (OP_SUPPORT_PAGE_DELETE == 1) */

    /* initialization of BU context */
#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    FSR_STL_InitBUCtx(pstZone->pstBUCtxObj);
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */

    pCurBuf = pstZone->pFullBMTBuf;
    for (nLan = 0; nLan < pstZone->pstZI->nNumLA; nLan++)
    {
        /* get BMT handle */
        FSR_STL_SetBMTHdl(pstZone, pstZone->pstBMTHdl, pCurBuf, pstML->nBMTBufSize);
        FSR_STL_InitBMT(pstZone, pstZone->pstBMTHdl, nLan);

        /* set next BMT buffer pointer */;
        pCurBuf += pstML->nBMTBufSize;
    }

    /* initialization of statistics info */
#if (OP_SUPPORT_STATISTICS_INFO == 1)
    FSR_OAM_MEMSET(pstZone->pstStats, 0x00, sizeof(STLStats));
#endif  /* (OP_SUPPORT_STATISTICS_INFO == 1) */

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}

