/**
 *   @mainpage   Flex Sector Remapper : RFS_1.3.1_b046-LinuStoreIII_1.1.0_b016-FSR_1.1.1_b109_Houdini
 *
 *   @section Intro
 *       Flash Translation Layer for Flex-OneNAND and OneNAND
 *    
 *    @section  Copyright
 *            COPYRIGHT. 2003-2009 SAMSUNG ELECTRONICS CO., LTD.               
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
#ifndef SAMSUNG_FLASH_H
#define SAMSUNG_FLASH_H

/*===========================================================================
INCLUDE FILES
===========================================================================*/
#include "FSR.h"
/*===========================================================================
PUBLIC DATA DECLARATIONS
===========================================================================*/
#define FS_REBUILD_UNIT                 (0)
#define FS_REBUILD_MAGIC1               (0xA5A5A5A5)
#define FS_REBUILD_MAGIC2               (0x5A5A5A5A)
#define FS_REBUILD_FLAG                 (0xA5A55A5A)
#define FS_REBUILD_FLAG_RESET           (0x5A5A5A5A)
#define FS_REBUILD_FLAG_POS             (0x01FFFFFC)

/*===========================================================================
PUBLIC FUNCTION DECLARATIONS
===========================================================================*/
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

INT32
samsung_ftl_init(void);

INT32
samsung_bml_format(FSRPartI* pstFSRPartI);

INT32
samsung_stl_init(UINT32 nPartID);

INT32
samsung_stl_format(UINT32 nPartID);

INT32
samsung_stl_write(UINT32 nImgSize, UINT8* pBuf, UINT32 nPartID);

INT32
samsung_bml_write(UINT32 nStartSct, UINT32 nNumOfScts, UINT8* pMBuf, FSRSpareBuf *pSBuf);

INT32
samsung_bml_erase(UINT32 nVUN, UINT32 nNumOfUnits);

INT32
samsung_is_page_erased(UINT32 nPage);

INT32
samsung_bml_read(UINT32 nStartScts, UINT32 nNumOfScts, UINT8* pMBuf, FSRSpareBuf *pSBuf);

INT32
samsung_bml_lock_op (UINT32 nStartUnit, UINT32 nNumOfUnits, UINT32 nOpcode);

UINT32
samsung_retrieve_ID(void);

INT32
samsung_set_fs_rebuild(void);

INT32
samsung_init_fs(void);

INT32
samsung_lld_write(UINT32 nPbn, UINT32 nPageOffset, UINT32 nSize, UINT8* pMBuf);

INT32
samsung_lld_read(UINT32 nPbn, UINT32 nPageOffset, UINT32 nSize, UINT8* pMBuf);

INT32
samsung_bml_get_vir_unit_info(UINT32 nVun, UINT32 *pn1stVpn, UINT32 *pnPgsPerUnit);

INT32
samsung_dump_bootimg(UINT32 nID, UINT8* pBuf, UINT32* pDumpSize);

VOID
samsung_smem_init(void);

INT32
samsung_block_count(void);

INT32
samsung_block_size(void);

INT32
samsung_page_size(void);

#if !defined(QCSBL)
VOID
samsung_otp_read(UINT32 page, UINT8 *page_buf);
#endif

VOID
samsung_smem_flash_spin_lock(UINT32 nIdx, UINT32 nBaseAddr);

VOID
samsung_smem_flash_spin_unlock(UINT32 nIdx, UINT32 nBaseAddr);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* SAMSUNG_FLASH_H */

