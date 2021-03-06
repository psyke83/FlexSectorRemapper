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
 * @file      FSR_LLD_FNDShared.c
 * @brief     This file declares the shared variable for supporting the linux
 * @author    NamOh Hwang
 * @date      1-Aug-2007
 * @remark
 * REVISION HISTORY
 * @n  1-Aug-2007 [NamOh Hwang] : first writing
 *
 */

/******************************************************************************/
/* Header file inclusions                                                     */
/******************************************************************************/
#define     FSR_NO_INCLUDE_BML_HEADER
#define     FSR_NO_INCLUDE_STL_HEADER

#include    "FSR.h"
#include    "FSR_LLD_FlexOND.h"

#define     FSR_LLD_LOGGING_HISTORY

#if defined(FSR_ONENAND_EMULATOR)
#define     FSR_LLD_LOGGING_HISTORY
#endif

#if defined(FSR_NBL2)
#undef      FSR_LLD_LOGGING_HISTORY
#endif

/******************************************************************************/
/* Global variable definitions                                                */
/******************************************************************************/
#if defined(TINY_FSR)
volatile FlexONDSharedCxt gstFNDSharedCxt[FSR_MAX_DEVS] =
{
    {{0, 0}, {FSR_LLD_SUCCESS, FSR_LLD_SUCCESS}, {FSR_FND_PREOP_NONE, FSR_FND_PREOP_NONE}, {FSR_FND_PREOP_ADDRESS_NONE, FSR_FND_PREOP_ADDRESS_NONE}, {FSR_FND_PREOP_ADDRESS_NONE, FSR_FND_PREOP_ADDRESS_NONE}, {FSR_LLD_FLAG_NONE, FSR_LLD_FLAG_NONE}, {0, 0},{{0,},}, {{0,},}},
    {{0, 0}, {FSR_LLD_SUCCESS, FSR_LLD_SUCCESS}, {FSR_FND_PREOP_NONE, FSR_FND_PREOP_NONE}, {FSR_FND_PREOP_ADDRESS_NONE, FSR_FND_PREOP_ADDRESS_NONE}, {FSR_FND_PREOP_ADDRESS_NONE, FSR_FND_PREOP_ADDRESS_NONE}, {FSR_LLD_FLAG_NONE, FSR_LLD_FLAG_NONE}, {0, 0},{{0,},}, {{0,},}},
    {{0, 0}, {FSR_LLD_SUCCESS, FSR_LLD_SUCCESS}, {FSR_FND_PREOP_NONE, FSR_FND_PREOP_NONE}, {FSR_FND_PREOP_ADDRESS_NONE, FSR_FND_PREOP_ADDRESS_NONE}, {FSR_FND_PREOP_ADDRESS_NONE, FSR_FND_PREOP_ADDRESS_NONE}, {FSR_LLD_FLAG_NONE, FSR_LLD_FLAG_NONE}, {0, 0},{{0,},}, {{0,},}},
    {{0, 0}, {FSR_LLD_SUCCESS, FSR_LLD_SUCCESS}, {FSR_FND_PREOP_NONE, FSR_FND_PREOP_NONE}, {FSR_FND_PREOP_ADDRESS_NONE, FSR_FND_PREOP_ADDRESS_NONE}, {FSR_FND_PREOP_ADDRESS_NONE, FSR_FND_PREOP_ADDRESS_NONE}, {FSR_LLD_FLAG_NONE, FSR_LLD_FLAG_NONE}, {0, 0},{{0,},}, {{0,},}}
};

#if defined(FSR_LINUX_OAM)
EXPORT_SYMBOL(gstFNDSharedCxt);
#endif

#endif /* defined(TINY_FSR)*/

#if defined(FSR_LLD_LOGGING_HISTORY)

#if defined(TINY_FSR) || !defined(FSR_LLD_HANDSHAKE_ERR_INF)

volatile FlexONDOpLog gstFNDOpLog[FSR_MAX_DEVS] =
{
    {0, {0,}, {0,}, {0,},{0,}},
    {0, {0,}, {0,}, {0,},{0,}},
    {0, {0,}, {0,}, {0,},{0,}},
    {0, {0,}, {0,}, {0,},{0,}}
};

#if defined(FSR_LINUX_OAM)
EXPORT_SYMBOL(gstFNDOpLog);
#endif

#endif

#endif  /* #if defined(FSR_LLD_LOGGING_HISTORY) */
