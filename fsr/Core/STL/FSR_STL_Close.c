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
 *  @file       FSR_STL_Close.c
 *  @brief      This file has functions related closing a Zone.
 *  @author     Wonmoon Cheon
 *  @date       29-MAY-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  29-MAY-2007 [Wonmoon Cheon] : first writing
 *	@n	27-JUNE-2007 [Jongtae Park] : re-writing for orange V1.2.0
 *  @n  29-JAN-2008 [MinYoung Kim] : dead code elimination
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
#if defined (FSR_POR_USING_LONGJMP)
BOOL32          gIsPorClose = FALSE32;
#endif

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
 *  @brief  This function closes the specified STL partition.
 *
 *  @param[in]  nClstID      : cluster id
 *  @param[in]  nZoneID      : zone id
 *
 *  @return FSR_STL_SUCCESS
 *  @return FSR_STL_INVALID_VOLUME_ID
 *  @return FSR_STL_INVALID_PARTITION_ID
 *
 *  @author         Wonmoon Cheon, Jongtae Park
 *  @version        1.2.0
 */
PUBLIC INT32
FSR_STL_CloseZone (UINT32 nClstID,
                   UINT32 nZoneID)
{
    STLZoneObj *pstZone;
    INT32       nRet        = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    do
    {
        /* volume id check*/
        if (nClstID >= MAX_NUM_CLUSTERS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): Invalid volume id(%d)\r\n"),
                    __FSR_FUNC__, __LINE__, nClstID));
            nRet = FSR_STL_INVALID_VOLUME_ID;
            break;
        }

        /* partition id check*/
        if (nZoneID >= MAX_ZONE)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): Invalid zone id(%d)\r\n"),
                    __FSR_FUNC__, __LINE__, nZoneID));
            nRet = FSR_STL_INVALID_PARTITION_ID;
            break;
        }

        pstZone = &(gpstSTLClstObj[nClstID]->stZoneObj[nZoneID]);

        if ((gpstSTLClstObj[nClstID] == NULL   ) ||
            (pstZone                 == NULL   ))
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): Partition is not opened\r\n"),
                    __FSR_FUNC__, __LINE__));
            nRet = FSR_STL_PARTITION_NOT_OPENED;
            break;
        }

#if defined (FSR_POR_USING_LONGJMP)
        if (pstZone->bOpened != FALSE32 && gIsPorClose != TRUE32)
#else
        if (pstZone->bOpened != FALSE32)
#endif 
        {
#if (OP_SUPPORT_PAGE_DELETE == 1)
            /*
             * store previous deleted page info not to lose current deleted information
             * which exists in volatile memory only.
             */
            nRet = FSR_STL_StoreDeletedInfo(pstZone);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                break;
            }
#endif

#if (OP_SUPPORT_CLOSE_TIME_UPDATE_FOR_FASTER_OPEN == 1)
            /*
             * Change the states of every active log block to inactive
             * This is for reducing the open time by avoiding active log block scanning.
             */
            FSR_STL_FlushAllMetaInfo(pstZone);
#endif 
        }

        /* Set closed flag */
        pstZone->bOpened = FALSE32;

        /* release the allocated partition object memory*/
        FSR_STL_FreeZoneMem(pstZone);

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}


/** 
 *  @brief  This function closes the specified STL cluster.
 *
 *  @param[in]  nClstID      : cluster id
 *
 *  @return FSR_STL_SUCCESS
 *  @return FSR_STL_INVALID_VOLUME_ID
 *  @return FSR_STL_INVALID_PARTITION_ID
 *
 *  @author         Wonmoon Cheon, Jongtae Park
 *  @version        1.2.0
 */
PUBLIC INT32
FSR_STL_CloseCluster   (UINT32 nClstID)
{
    STLClstObj *pstClst;
    UINT32      nZoneId;
    INT32       nErr    = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));
    do 
    {
        /* volume id check*/
        if (nClstID >= MAX_NUM_CLUSTERS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): Invalid volume id(%d)\r\n"),
                    __FSR_FUNC__, __LINE__, nClstID));
            nErr = FSR_STL_INVALID_VOLUME_ID;
            break;
        }

        /* Get STL cluster object */
        pstClst = gpstSTLClstObj[nClstID];
        if (pstClst == NULL)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): Partition is not opened\r\n"),
                    __FSR_FUNC__, __LINE__));
            nErr = FSR_STL_PARTITION_NOT_OPENED;
            break;
        }

        /* Check where all zones is closed */
        for (nZoneId = 0; nZoneId < MAX_ZONE; nZoneId++)
        {
            if (pstClst->stZoneObj[nZoneId].bOpened != FALSE32)
            {
                FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): Partition is Already Opened (%d)\r\n"),
                        __FSR_FUNC__, __LINE__,nZoneId));
                nErr = FSR_STL_PARTITION_ALREADY_OPENED;
                break;
            }
        } 

        if (nZoneId < MAX_ZONE)
        {
            break;
        }
        FSR_ASSERT(nErr == FSR_STL_SUCCESS);

        /* free temporary page buffer */
        if (pstClst->pTempPgBuf != NULL)
        {
            FSR_OAM_Free(pstClst->pTempPgBuf);
            pstClst->pTempPgBuf = NULL;
        }

        /* release the allocated cluster object memory*/
        FSR_OAM_Free(pstClst);
        gpstSTLClstObj[nClstID] = NULL;

    } while(0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nErr));
    return nErr;
}
