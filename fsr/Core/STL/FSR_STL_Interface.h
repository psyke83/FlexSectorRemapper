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
 *  @file       FSR_STL_Interface.h
 *  @brief      This header file defines APIs for STL(Sector Translation Layer).
 *  @author     Wonmoon Cheon
 *  @date       02-MAR-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  02-MAR-2007 [Wonmoon Cheon] : first writing
 *  @n  29-JAN-2008 [MinYoung Kim] : dead code elimination
 *
 */

#ifndef __FSR_STL_INTERFACE_H__
#define __FSR_STL_INTERFACE_H__

/*****************************************************************************/
/* Header file inclusions                                                    */
/*****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/*****************************************************************************/
/* Macro                                                                     */
/*****************************************************************************/

/*****************************************************************************/
/* Type defines                                                              */
/*****************************************************************************/

/*****************************************************************************/
/* Constant definitions                                                      */
/*****************************************************************************/
/* The partition status flag for FSR_STL_OpenZone       */
#define FSR_STL_FLAG_RW_PARTITION               (1 << 0)
#define FSR_STL_FLAG_RO_PARTITION               (1 << 1)
#define FSR_STL_FLAG_LOCK_PARTITION             (1 << 2)

/* Format Information */
typedef struct {
    UINT32      nAvgECnt;
    UINT32      nNumECnt;
    UINT32     *pnECnt;
} STLFmtInfo;

#define FSR_STL_SPARE_EXT_MAX_SIZE  (8)
#define FSR_STL_CPBK_SPARE_BASE     (FSR_LLD_CPBK_SPARE      + FSR_SPARE_BUF_BML_BASE_SIZE)
#define FSR_STL_CPBK_SPARE_EXT      (FSR_STL_CPBK_SPARE_BASE + FSR_SPARE_BUF_STL_BASE_SIZE)

/*****************************************************************************/
/* Exported variable declarations                                            */
/*****************************************************************************/

/*****************************************************************************/
/* Exported function prototype                                               */
/*****************************************************************************/
PUBLIC INT32    FSR_STL_InitCluster     (UINT32             nClstID,
                                         UINT32             nVolID,
                                         UINT32             nPart,
                                         RBW_OpMode         eMode,
                                         RBW_AsyncEvtFunc   pfEventCbFunc);

PUBLIC INT32    FSR_STL_FormatCluster  (UINT32              nClstID,
                                        UINT16              nStartVbn,
                                        UINT32              nNumZones,
                                        UINT16             *pnBlksPerZone,
                                        STLFmtInfo         *pstFmt);

PUBLIC INT32    FSR_STL_OpenCluster    (UINT32          nClstID,
                                        UINT32          nNumZone,
                                        UINT32          nFlag);

PUBLIC INT32    FSR_STL_CloseCluster   (UINT32 nClstID);

PUBLIC INT32    FSR_STL_CloseZone      (UINT32              nClstID,
                                        UINT32              nZoneID);

PUBLIC INT32    FSR_STL_ReadZone       (UINT32              nClstID,
                                        UINT32              nZoneID,
                                        UINT32              nLsn,
                                        UINT32              nNumOfScts,
                                        UINT8              *pBuf,
                                        UINT32              nFlag);

PUBLIC INT32    FSR_STL_WriteZone      (UINT32              nClstID,
                                        UINT32              nZoneID,
                                        UINT32              nLsn,
                                        UINT32              nNumOfScts,
                                        UINT8              *pBuf,
                                        UINT32              nFlag);

PUBLIC INT32    FSR_STL_ChangePartAttr (UINT32       nVol,
                                        UINT32       nPartID,
                                        UINT32       nAttr);

#if (OP_SUPPORT_PAGE_DELETE == 1)

PUBLIC INT32    FSR_STL_DeleteZone     (UINT32              nClstID,
                                        UINT32              nZoneID,
                                        UINT32              nLsn,
                                        UINT32              nNumOfScts,
                                        UINT32              nFlag);

#endif  /* OP_SUPPORT_PAGE_DELETE == 1      */

PUBLIC  INT32   FSR_STL_LockPartition  (const   UINT32      nClstID);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif  /* __FSR_STL_INTERFACE_H__  */
