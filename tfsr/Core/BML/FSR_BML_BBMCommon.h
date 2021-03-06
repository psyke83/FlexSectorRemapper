/**
 *   @mainpage   Flex Sector Remapper : RFS_1.3.1_b046-LinuStoreIII_1.1.0_b016-FSR_1.1.1_b109_Houdini
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
 * @file      FSR_BML_BBMCommon.h
 * @brief     This header contains global type definitions, global macros, 
 *            prototypes of functions for FSR_BML operation
 * @author    MinYoung Kim
 * @author    SuRyun Lee
 * @date      11-JAN-2007
 * @remark
 * REVISION HISTORY
 * @n  31-MAY-2007 [MinYoung Kim]: first writing
 *
 */

#ifndef _FSR_BML_BBMCOMMON_H_
#define _FSR_BML_BBMCOMMON_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*****************************************************************************/
/* Signature for tightly Lockable Pool Control Header                        */
/*****************************************************************************/
#define     BML_LPCH_SIG            (UINT8 *) "LPCH"

/*****************************************************************************/
/* Signature for Unlockable Pool Control Header                              */
/*****************************************************************************/
#define     BML_UPCH_SIG            (UINT8 *) "UPCH"

/*****************************************************************************/
/* property of data                                                          */
/*****************************************************************************/
#define     BML_META_DATA           (1)
#define     BML_USER_DATA           (2)

/*****************************************************************************/
/* Type of PCB                                                               */
/*****************************************************************************/
#define     BML_TYPE_LPCB           (0x00000001)
#define     BML_TYPE_UPCB           (0x00000002)
#define     BML_TYPE_TPCB           (0x00000004)

/*****************************************************************************/
/*  number of BMS                                                            */
/*****************************************************************************/
#define     BML_NUM_OF_BMS_2KB_PG       (4)             /* 2KB page device   */
#define     BML_NUM_OF_BMS_4KB_PG       (10)            /* 4KB page device   */

/*****************************************************************************/
/* number of bits in a byte                                                  */
/*****************************************************************************/
#define     BML_NUM_OF_BITS_IN_1BYTE    (8)

/*****************************************************************************/
/* maximum string length of PCH signature                                    */
/*****************************************************************************/
#define     BML_MAX_PCH_SIG             (4)

/*****************************************************************************/
/* maximum number of Block Map Field per sector                              */
/*****************************************************************************/
#define     BML_BMFS_PER_BMS            (127)

/*****************************************************************************/
/* maximum number of reservoir candidate block field per sector              */
/*****************************************************************************/
#define     BML_RCBFS_PER_RCBS          (256)

/*****************************************************************************/
/* sector offset in PCB                                                      */
/*****************************************************************************/
#define     BML_PCH_SCT_OFF             (0x00000000) 
#define     BML_PIA_SCT_OFF             (0x00000001)
#define     BML_ERL_SCT_OFF             (0x00000002)
#define     BML_BAB_SCT_OFF             (0x00000003)

/*****************************************************************************/
/*  Layout type                                                              */
/*****************************************************************************/
#define     BML_2KB_PG                  (4)             /* 2KB page device   */
#define     BML_4KB_PG                  (8)             /* 4KB page device   */

/*****************************************************************************/
/* Etc.                                                                      */
/*****************************************************************************/
#define     BML_NUM_OF_META_PGS         (3)  /* PCB meta pages except confirm */

#define     BML_VALID_BLK_MARK          (UINT16)(0xFFFF)
                         /* Valid block mark : This value should be same with */
                         /* FSR_FND_VALID_BLK_MARK and FSR_OND_VALID_BLK_MARK */

#define     BML_INVALID                 (0xFFFFFFFF)       /* invalid data  */

/**
 * @brief  Data structure for storing the info. about BMS(Block Map Sector)
 */
typedef struct
{
    UINT16     nInf;            /**< nInf field shows a cause why BMS is created*/
    UINT16     nRsv;            /**< a reserved area for future use             */
    BmlBMF     stBMF[BML_BMFS_PER_BMS]; /**< Block Map Field Array              */
} BmlBMS;

/**
 * @brief  Data structure to save device info used in the BBM for compatibility
 */
typedef struct
{
    UINT32      nNumOfPlane;        /**< number of plane (1 or 2)            */
    UINT32      nNumOfDieInDev;     /**< number of die   (1 or 2)            */
    UINT32      nNumOfBlksInDie;    /**< number of blocks in a die           */
    UINT32      nNumOfBlksInDev;    /**< number of blocks in a device        */
} BmlDevInfo;

/**
 * @brief  typedefs for Pool Control Header
 * @n      The size of PoolCtlHdr should be 512 Bytes
 */
typedef struct
{
    UINT8        aSig[BML_MAX_PCH_SIG]; /**< "LPCH" / "UPCH"                 */
    UINT16       nREFPbn;               /**< Pbn of REF block                */
    UINT16       nTPCBPbn;              /**< Pbn of TPCB block               */
    UINT16       nNumOfMLCRsvr;         /**< number of MLC reservoir         */
    UINT16       nRsv;                  /**< Reserved area (Unused)          */
    UINT32       nAge;                  /**< Age of PCH                      */
    UINT32       nGlobalAge;            /**< Global age of PCH               */
    BmlDevInfo   stDevInfo;             /**< device info for compatibility   */
    UINT8        aPad[FSR_SECTOR_SIZE - BML_MAX_PCH_SIG - 4 * 8];  
                                       /** reserved area (not used)          */
} BmlPoolCtlHdr;

/*****************************************************************************/
/* exported function prototype of BML                                        */
/*****************************************************************************/

PUBLIC VOID    _InitBBM            (BmlVolCxt     *pstVol,
                                    BmlDevCxt     *pstDev);
PUBLIC BOOL32  _IsAllocRB          (BmlReservoir  *pstRsv,
                                    UINT32         nPbn);
PUBLIC BOOL32  _CmpData            (BmlVolCxt     *pstVol,
                                    UINT8         *pSrc, 
                                    UINT8          nCmpData,
                                    BOOL32         bSpare);
PUBLIC INT32   _LLDRead            (BmlVolCxt     *pstVol,
                                    UINT32         nPDev,
                                    UINT32         nPbn,
                                    UINT32         nPgOffset,
                                    BmlReservoir  *pstRsv,
                                    UINT8         *pMBuf,
                                    FSRSpareBuf   *pSBuf,
                                    UINT32         nDataType,
                                    BOOL32         bRecovery,
                                    UINT32         nFlag);
PUBLIC INT32   _CalcRsvrSize       (BmlVolCxt     *pstVol,
                                    BmlDevCxt     *pstDev,
                                    FSRPartI      *pstPart,
                                    BOOL32         bInitFormat);
PUBLIC UINT32  _TransPbn2Sbn       (BmlReservoir  *pstRsv, 
                                    UINT32         nPbn, 
                                    BmlBMI        *pstBMI);
PUBLIC UINT16  _LookUpPartAttr     (BmlVolCxt     *pstVol,
                                    UINT32         nDieIdx,
                                    UINT16         nSbn);
PUBLIC VOID    _SetVolCxt          (BmlVolCxt     *pstVol,
                                    BmlDevCxt     *pstDev);
PUBLIC VOID    _SortBMI            (BmlReservoir  *pstRsv);
PUBLIC VOID    _ReconstructBUMap   (BmlVolCxt     *pstVol, 
                                    BmlReservoir  *pstRsv,
                                    UINT32         nDieIdx);

#if !defined(FSR_NBL2)
PUBLIC INT32   _CheckPartInfo      (BmlVolCxt     *pstVol,
                                    FSRPartI      *pstPartI,
                                    UINT32        *pnLastSLCPbn);
#endif /* FSR_NBL2 */

#ifdef __cplusplus
}
#endif
#endif  /* _FSR_BML_BBMCOMMON_H_ */
