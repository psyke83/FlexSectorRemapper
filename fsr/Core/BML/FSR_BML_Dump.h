/**
 *   @mainpage   Flex Sector Remapper : LinuStoreIII_1.2.0_b038-FSR_1.2.1p1_b139_RTM
 *
 *   @section Intro Intro
 *       Flash Translation Layer for Flex-OneNAND and OneNAND
 *   
 *     @MULTI_BEGIN@ @COPYRIGHT_DEFAULT
 *     @section Copyright COPYRIGHT_DEFAULT
 *            COPYRIGHT. SAMSUNG ELECTRONICS CO., LTD.
 *                                    ALL RIGHTS RESERVED
 *     Permission is hereby granted to licensees of Samsung Electronics Co., Ltd. products
 *     to use this computer program only in accordance 
 *     with the terms of the SAMSUNG FLASH MEMORY DRIVER SOFTWARE LICENSE AGREEMENT.
 *     @MULTI_END@
 *
 *      
 *
 *     @section Description
 *
 */

/**
 *  @file       FSR_BML_Dump.h
 *  @brief      BML Volume dump header file
 *  @author     SuRuyn Lee
 *  @date       18-JUN-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  18-JUN-2007 [SuRyun Lee] : first writing
 *
 */

#ifndef _FSR_BML_DUMP_H_
#define _FSR_BML_DUMP_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/****************************************************************************/
/*Common Constant definitions                                               */
/****************************************************************************/
#define     BML_DUMP_OTPBLK                         (0x00000000)
#define     BML_DUMP_PIBLK                          (0x00000001)
#define     BML_DUMP_DATABLK                        (0x00000002)
#define     BML_DUMP_CHK_LASTIMAGE                  (0x00000004)

/**
 *  @brief      Static function prototypes
 */
#if !defined(FSR_NBL2)
PRIVATE INT32   _SetDieDumpHdr(UINT32           nVol,
                               UINT32           nDumpType,
                               FSRDumpPEList   *pstPEList,
                               FSRVolDumpHdr   *pstVolDumpHdr);
PRIVATE INT32   _SetVolDumpHdr  (UINT32         nVol,
                                 UINT32         nDumpType,
                                 FSRDumpPEList *pstPEList,
                                 FSRVolDumpHdr *pstVolDumpHdr);
PRIVATE INT32   _StartDump      (UINT32         nVol,
                                 UINT32         nDumpType,
                                 FSRDumpPEList *pstPEList,
                                 UINT8         *pBuf,
                                 UINT32        *pBytesReturned);
#if 0 //This function is not used now but it maybe used next feature.
PRIVATE VOID    _SearchMLCBlk   (UINT32         nVol);
#endif
PRIVATE INT32   _ReadBlk        (UINT32         nVol,
                                 UINT32         nPDev,
                                 UINT32         nPbn,
                                 UINT32         nNumOfPgs,
                                 UINT8         *pBuf,
                                 UINT32        *pBytesReturned);
PRIVATE INT32   _ReadOTPBlk     (UINT32         nVol,
                                 UINT8         *pBuf,
                                 UINT32        *pBytesReturned);
PRIVATE INT32   _ReadPIBlk      (UINT32         nVol,
                                 UINT8         *pBuf,
                                 UINT32        *pBytesReturned);
PRIVATE INT32   _ReadDataBlk    (UINT32         nVol,
                                 UINT8         *pBuf,
                                 UINT32        *pBytesReturned);
PRIVATE INT32   _ChkLastImage   (UINT32         nVol,
                                 UINT32         nDumpType,
                                 FSRDumpPEList *pstPEList,
                                 UINT8         *pBuf,
                                 UINT32        *pBytesReturned);
PRIVATE VOID    _CalcDumpSize   (UINT32         nVol,
                                 FSRVolDumpHdr *pstVolDumpHdr);

PRIVATE FSRVolDumpHdr* _GetVolDumpHdr (UINT32     nVol);

#if defined(FSR_SUPPORT_BE_IMAGE_BE_PLATFORM)
PRIVATE VOID           _ChangeByteOrderFSRVolDumpHdr (FSRVolDumpHdr *pstVolDumpHdr);
PRIVATE VOID           _ChangeByteOrderPIBlk         (UINT8         *pPIBlk);
#endif

#if defined(FSR_SUPPORT_BE_IMAGE_BE_PLATFORM)
#define FSR_CHANGE_BYTE_ORDER_FSRVOLDUMPHDR(pSrc) _ChangeByteOrderFSRVolDumpHdr(pSrc)
#define FSR_CHANGE_BYTE_ORDER_PIBLK(pSrc)         _ChangeByteOrderPIBlk(pSrc)
#else
#define FSR_CHANGE_BYTE_ORDER_FSRVOLDUMPHDR(pSrc)
#define FSR_CHANGE_BYTE_ORDER_PIBLK(pSrc)
#endif 

#endif /* FSR_NBL2 */
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _FSR_BML_BIFCOMMON_H_ */
