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
 *  @file       FSR_STL_Common.c
 *  @brief      This file contains global data and utility functions for STL.
 *  @author     Wonmoon Cheon
 *  @date       02-MAR-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  02-MAR-2007  [Wonmoon Cheon] : first writing
 *  @n  27-JUNE-2007 [Jongtae Park] : re-writing for orange V1.2.0
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
#if (OP_STL_DEBUG_CODE == 1)
#include <stdio.h>
#endif
/*****************************************************************************/
/* Global variable definitions                                               */
/*****************************************************************************/
STLClstObj     *gpstSTLClstObj[MAX_NUM_CLUSTERS] = {NULL,};
 
/**************************************************
 * 8bits Zero Count Table
 **************************************************/
PRIVATE const UINT8 aZBCTable8[256] =
{
    8, 7, 7, 6, 7, 6, 6, 5, 7, 6, 6, 5, 6, 5, 5, 4,
    7, 6, 6, 5, 6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3,
    7, 6, 6, 5, 6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3,
    6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2,
    7, 6, 6, 5, 6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3,
    6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2,
    6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2,
    5, 4, 4, 3, 4, 3, 3, 2, 4, 3, 3, 2, 3, 2, 2, 1,
    7, 6, 6, 5, 6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3,
    6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2,
    6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2,
    5, 4, 4, 3, 4, 3, 3, 2, 4, 3, 3, 2, 3, 2, 2, 1,
    6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2,
    5, 4, 4, 3, 4, 3, 3, 2, 4, 3, 3, 2, 3, 2, 2, 1,
    5, 4, 4, 3, 4, 3, 3, 2, 4, 3, 3, 2, 3, 2, 2, 1,
    4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0
};

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
 *  @brief      This function generates CRC32 code for user data confirmation
 *
 *  @param[in]      pBuf    : buffer pointer
 *  @param[in]      size    : number of sectors
 *
 *  @return         CRC     : crc32 code
 *
 *  @author         Jeonguk kang, Jongtae Park
 *  @version        1.2.0
 */
PUBLIC UINT32
FSR_STL_CalcCRC32(const UINT8 *pBuf, UINT32 nSize)
{
    UINT32 *p4Buf;
    UINT32  nOff;
    UINT32  nInput;
    UINT32  nCRC    = 0xFFFFFFFF;
    UINT32  nCRC0, nCRC1, nCRC2, nCRC3;
    UINT32  nIdx;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    nOff = ((UINT32)(pBuf) & (sizeof(UINT32) - 1));
    if (nOff != 0)
    {
        nOff = sizeof(UINT32) - nOff;
        for (nIdx = 0; nIdx < nOff; nIdx++)
        {
            nInput = nCRC ^ *(pBuf++);
            nCRC0  = CRC32_Table[0][nInput & 0xFF];
            nCRC   = (nCRC >> 8) ^ nCRC0;
        }
        nSize -= nOff;
    }

    p4Buf = (UINT32*)(pBuf);
    while(nSize > 3)
    {
        nInput =nCRC ^ *(p4Buf++);

        nCRC0 = CRC32_Table[0][(nInput >> 24) & 0xFF];
        nCRC1 = CRC32_Table[1][(nInput >> 16) & 0xFF];
        nCRC2 = CRC32_Table[2][(nInput >>  8) & 0xFF];
        nCRC3 = CRC32_Table[3][(nInput      ) & 0xFF];

        nCRC =  (nCRC0)^(nCRC1)^(nCRC2)^(nCRC3);

        nSize -= 4;
    }

    pBuf = (UINT8*)(p4Buf);
    for (nIdx = 0 ; nIdx < nSize; nIdx++)
    {
        nInput = nCRC ^ *(pBuf++);
        nCRC0  = CRC32_Table[0][nInput & 0xFF];
        nCRC   = (nCRC >> 8) ^ nCRC0;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, ~nCRC));
    return ~nCRC;
}

/**
 *  @brief      This function allocates VFL parameter from pool.
 *
 *  @param[in]  pstZone : zone object
 *
 *  @return     VFL parameter object pointer
 *
 *  @author     Wonmoon Cheon
 *  @version    1.2.0
 */
PUBLIC VFLParam*    
FSR_STL_AllocVFLParam(STLZoneObj *pstZone)
{
    STLClstObj  *pstClst;
    VFLParam    *pstParam = NULL;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF, 
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Get cluster object */
    pstClst = gpstSTLClstObj[pstZone->nClstID];

    FSR_ASSERT(pstClst->nVFLParamCnt < MAX_VFL_PARAMS);
    pstParam = &(pstClst->astParamPool[pstClst->nVFLParamCnt]);
    pstClst->nVFLParamCnt++;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    FSR_ASSERT(pstParam != NULL);
    return pstParam;
}

/**
 *  @brief      This function frees allocated VFL parameter.
 *
 *  @param[in]  pstZone : zone object
 *
 *  @return     none
 *
 *  @author     Wonmoon Cheon
 *  @version    1.2.0
 */
PUBLIC VOID
FSR_STL_FreeVFLParam(STLZoneObj *pstZone,
                     VFLParam   *pstVFLParam)
{
    STLClstObj  *pstClst;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Get cluster object */
    pstClst = gpstSTLClstObj[pstZone->nClstID];

    FSR_ASSERT(pstClst->nVFLParamCnt > 0);
    FSR_ASSERT(&(pstClst->astParamPool[pstClst->nVFLParamCnt - 1]) == pstVFLParam);
    pstClst->nVFLParamCnt--;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}

/**
 *  @brief      This function allocates extended VFL parameter from pool.
 *
 *  @param[in]  pstZone : zone object
 *
 *  @return     VFL parameter object pointer
 *
 *  @author     Wonmoon Cheon
 *  @version    1.2.0
 */
PUBLIC VFLExtParam* 
FSR_STL_AllocVFLExtParam(STLZoneObj *pstZone)
{
    STLClstObj     *pstClst;
    VFLExtParam    *pstExtParam = NULL;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Get cluster object */
    pstClst = gpstSTLClstObj[pstZone->nClstID];

    FSR_ASSERT(pstClst->nVFLExtCnt < MAX_VFLEXT_PARAMS);
    pstExtParam = &(pstClst->astExtParamPool[pstClst->nVFLExtCnt]);
    pstClst->nVFLExtCnt++;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    FSR_ASSERT(pstExtParam != NULL);
    return pstExtParam;
}

/**
 *  @brief      This function frees allocated extended VFL parameter.
 *
 *  @param[in]  pstZone : zone object
 *
 *  @return     none
 *
 *  @author     Wonmoon Cheon
 *  @version    1.2.0
 */
PUBLIC VOID
FSR_STL_FreeVFLExtParam(STLZoneObj  *pstZone,
                        VFLExtParam *pstVFLExtParam)
{
    STLClstObj     *pstClst;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Get cluster object */
    pstClst = gpstSTLClstObj[pstZone->nClstID];

    FSR_ASSERT(pstClst->nVFLExtCnt > 0);
    FSR_ASSERT(&(pstClst->astExtParamPool[pstClst->nVFLExtCnt - 1]) == pstVFLExtParam);
    pstClst->nVFLExtCnt--;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}


/**
 *  @brief      FSR_STL_MarkSBitmap() function make a bitmap 
 *
 *  @param[in]  nSrcBitmap  : source bitmap
 *
 *  @return     SBITMAP     : Sector bitmap data
 *
 *  @author     Wonmoon Cheon
 *  @version    1.2.0
*/
PUBLIC SBITMAP
FSR_STL_MarkSBitmap(SBITMAP nSrcBitmap)
{
    SBITMAP     nSBitmap = 0;
    INT32       i;
    UINT32      nMaxIdx;
    BOOL32      bMSOneFound = FALSE32;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* the number of bits in the bitmap structure */
    nMaxIdx = (sizeof(SBITMAP) << 3) - 1; /* i.e., * 8 bits */

    for (i = nMaxIdx; i >= 0; i--)
    {
        if((!bMSOneFound) &&
            (nSrcBitmap & (1 << i)))
        {
            bMSOneFound = TRUE32;
        }

        if((bMSOneFound) &&
            !(nSrcBitmap & (1 << i)) )
        {
            nSBitmap = nSBitmap | (1 << i);
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nSBitmap));
    return nSBitmap;
}



/**
 *  @brief  This function sets the sector bitmap with the specified
 *  @n      nLsn and nSectors. The specified number of sectors can not
 *  @n      exceed the page boundary.
 *
 *  @param[in]      nLsn        : logical starting sector number 
 *  @param[in]      nSectors    : number of sectors
 *  @param[in]      nSecPerVPg  : number of sectors per virtual page
 *
 *  @return         SBITMAP     : Sector bitmap data
 *
 *  @author         Wonmoon Cheon
 *  @version        1.1.0
 */
PUBLIC SBITMAP
FSR_STL_SetSBitmap (UINT32 nLsn,
                    UINT32 nSectors,
                    UINT32 nSecPerVPg)
{
    SBITMAP     nSBitmap = 0;
    UINT32      nOffset;
    UINT32      i, j;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* nOffset = nLsn % nSecPerVpg */
    nOffset = nLsn & (nSecPerVPg - 1);

    for (i = nSectors, j = 0; i > 0; i--)
    {
        nSBitmap |= 1 << (nOffset + j++);
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nSBitmap));
    return nSBitmap;
}


/**
 *  @brief   This function verifies the input parameters of 
 *  @n       FSR_STL_ReadZone() and FSR_STL_WriteZone() function.
 *
 *  @param[in]      nClstID     : clust id
 *  @param[in]      nZoneID     : zone id
 *  @param[in]      nLsn        : logical starting sector number 
 *  @param[in]      nNumOfScts  : number of sectors
 *  @param[in]      pBuf        : buffer memory pointer to read or write
 *
 *  @return         FSR_STL_SUCCESS
 *  @return         FSR_STL_INVALID_VOLUME_ID
 *  @return         FSR_STL_INVALID_PARTITION_ID
 *  @return         FSR_STL_INVALID_PARAM
 *
 *  @author         Wonmoon Cheon
 *  @version        1.2.0
 */
PUBLIC INT32
FSR_STL_CheckArguments (UINT32 nClstID,
                        UINT32 nZoneID,
                        UINT32 nLsn,
                        UINT32 nNumOfScts,
                        UINT8* pBuf)
{
    STLClstObj       *pstClst = NULL;
    STLZoneObj       *pstZone = NULL;
    STLZoneInfo      *pstZI  = NULL;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* volume id check */
    if (nClstID >= MAX_NUM_CLUSTERS)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, FSR_STL_INVALID_VOLUME_ID));
        return FSR_STL_INVALID_VOLUME_ID;
    }

    /* Get volume object pointer */
    pstClst = gpstSTLClstObj[nClstID];

    /* partition id check */
    if (nZoneID >= pstClst->stRootInfoBuf.stRootInfo.nNumZone)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, FSR_STL_INVALID_PARTITION_ID));
        return FSR_STL_INVALID_PARTITION_ID;
    }

    /* Get partition object pointer */
    pstZone = &(pstClst->stZoneObj[nZoneID]);
    pstZI   = pstZone->pstZI;
    
    /* input parameter check */
    if (pBuf == NULL)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR]      Invalid Parameter\r\n")));
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, FSR_STL_INVALID_PARAM));
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR]      pBuf == NULL!\r\n")));
        return FSR_STL_INVALID_PARAM;
    }

    /* Check request sector range */
    if ((nNumOfScts == 0) || 
        (nLsn >= pstZI->nNumUserScts) ||
        ((nLsn + nNumOfScts) > pstZI->nNumUserScts))
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR]      Invalid Parameter\r\n")));
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, FSR_STL_INVALID_PARAM));
        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR]    Invalid address error! (nLsn:%d, nNumOfScts:%d)\r\n"), nLsn, nNumOfScts));
        return FSR_STL_INVALID_PARAM;
    }

    if (pstZone->bOpened != TRUE32)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
            (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, FSR_STL_PARTITION_NOT_OPENED));
        return FSR_STL_PARTITION_NOT_OPENED;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s\r\n"), __FSR_FUNC__));
    return FSR_STL_SUCCESS;
}

/**
*  @brief   This function counts zero bits of pBuf
*
*  @param[in]      pBuf         : buffer pointer 
*  @param[in]      nCntSize     : count size
*
*  @return         the number of zero bits in pBuf
*
*  @author         Jongtae Park
*  @version        1.2.0
*/
PUBLIC UINT32
FSR_STL_GetZBC     (UINT8* pBuf, 
                    UINT32 nCntSize)
{
    UINT32  nSumZeroBits    = 0;
    UINT32  nSumOneBits     = 0;
    UINT32  i;
    UINT32  nLoopCnt        = nCntSize >> 2;
    UINT32  nMaskBit        = 0x00000003;
    UINT32  nBufMask        = 0x00000000;
    UINT32  pTempData;
    UINT32  nRemainedBytes  = 0;
    UINT32 *pTemp32         = NULL;
    BOOL32  bMisAlign       = FALSE32;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    pTempData = (UINT32)(pBuf);

    nBufMask = pTempData & nMaskBit;

    /* Check buffer align first address */
    if (nBufMask != 0x00)
    {
        nLoopCnt -= 1;
        for (i = 0; i < nBufMask; i++)
        {
            /* count number of 0 bits in misaligned area */
            nSumZeroBits += aZBCTable8[*(pBuf + i)];
        }
    }

    nRemainedBytes = nCntSize - nBufMask - (nLoopCnt << 2);

    /* Check misaligned Case */
    if (nRemainedBytes != 0)
    {
        bMisAlign = TRUE32;
    }
    
    pTemp32 = (UINT32 *)(pBuf + nBufMask);

    /* Count the number of 0's */
    if (pBuf != NULL)
    {
        UINT32  n;

        /* Count the number of 1 bits */
        for (i = 0; i < nLoopCnt; i++)
        {
            n = *(pTemp32 + i);
                
            n = (n & 0x55555555) + ((n & 0xAAAAAAAA) >> 1);
            n = (n & 0x33333333) + ((n & 0xCCCCCCCC) >> 2);
            n = (n + (n >> 4)) & 0x0F0F0F0F;
            n = (n + (n >> 16));
            n = (n + (n >> 8)) & 0x3F;

            nSumOneBits += n;
        }

        /* Convert the number of 1 bits into number of 0 bits */
        nSumZeroBits += (nLoopCnt << 5) - nSumOneBits;
    }
        
    if (bMisAlign == TRUE32)
    {
        /* Check buffer align last address */

        for (i = 0; i < nRemainedBytes; i++)
        {
            /* count number of 0 bits in misaligned area */
            nSumZeroBits += aZBCTable8[*(pBuf + nBufMask + (nLoopCnt << 2) + i)];
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : %4d\r\n"), __FSR_FUNC__, nSumZeroBits));
    return nSumZeroBits;
}

/**
 *  @brief       This function function sets PTF field
 *
 *  @param[in]      pstZone              : pointer to atl Zone object
 *  @param[in]      nTypeField   : page type field
 *  @param[in]      nMetaType    : meta page type
 *  @param[in]      nLSBOrMSB    : LSB or MSB, MID or END
 *  @param[in]      nSctCnt      : sector count
 *
 *  @return                 PTF field
 *
 *  @author                 Jongtae Park
 *  @version        1.2.0
 */

/**
 * ----------------------------------------------
 * PTF (16bits)
 * bit Data/Log           BU            Meta
 * ----------------------------------------------(LSB)
 * 0                  "zoneidx"
 * 1
 * 2
 * 3
 * 4                   Data/Log
 * 5                    BU
 * 6                    Meta
 * 7                    N/A
 * ----------------------------------------------
 * 8   MID(LSB)     "SectorCount"    ROOT  (1)
 * 9   END(LSB)                       DH   (2)
 * 10  MID(MSB)                       BMT  (4)
 * 11  END(MSB)                       PMT  (8)
 * 12                                 TU   (16)
 * 13
 * 14
 * 15
 * ----------------------------------------------(MSB)
 */
PUBLIC UINT16
FSR_STL_SetPTF (STLZoneObj *pstZone,
                UINT8       nTypeField,
                UINT8       nMetaType,
                UINT8       nLSBOrMSB,
                UINT8       nSctCnt)
{
    UINT16 nPTF = 0x0000;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    nPTF |= pstZone->nZoneID;   /* zone index */

    switch (nTypeField)
   {
       case TF_LOG :            /* data or log page */
           nPTF |= (1 << 4);
           nPTF |= (nLSBOrMSB << 8);
           break;
       case TF_BU :             /* buffer page */
           nPTF |= (1 << 5);
           nPTF |= (nSctCnt << 8);
           break;
       case TF_META :           /* meta page */
           nPTF |= (1 << 6);
           nPTF |= (nMetaType << 8);
           break;
       default :
           return 0xFFFF;
   }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nPTF));
    return nPTF;
}

/**
*  @brief       This function function set Random In field
*
*  @param[in]       pExtParam   : pointer to extended parameter
*  @param[in]       nOffset     : offset to buffer
*  @param[in]       nNumOfBytes : bytes to program
*  @param[in]       pBuf        : pointer to buffer
*  @param[in]       nIdx        : index to random-in argument
*
*  @return          VOID
*
*  @author          Jongtae Park
*  @version         1.2.0
*/
PUBLIC VOID
FSR_STL_SetRndInArg(VFLExtParam    *pExtParam,
                    UINT16          nOffset,
                    UINT16          nNumOfBytes,
                    VOID           *pBuf,
                    UINT16          nIdx)
{
    VFLRndInArg     *RndInArg;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    RndInArg                = &(pExtParam->astRndIn[nIdx]);

    RndInArg->pBuf          = pBuf;
    RndInArg->nOffset       = nOffset;
    RndInArg->nNumOfBytes   = nNumOfBytes;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s\r\n"), __FSR_FUNC__));
}

#if (OP_STL_DEBUG_CODE == 1)

/**
*  @brief          This function check the validity of pstPart
*
*  @param[in]      pstZone      : STL zone object pointer 
*
*  @return         none
*
*  @author         Wonmoon Cheon, Jongtae Park
*  @version        1.2.0
*/
PUBLIC VOID 
FSR_STL_DBG_CheckValidity(STLZoneObj *pstZone)
{
    const   UINT32  nMaxFBlks   = pstZone->pstML->nMaxFreeSlots;
    UINT32          nFBIdx;
    BADDR           nVbn;
    UINT32          nIdx;
    UINT32          i;
    STLZoneInfo    *pstZI       = NULL;
    STLLogGrpList  *pstAGrpLst  = NULL;
    STLLogGrpHdl   *pstAGrp     = NULL;
    STLCtxInfoHdl  *pstCI       = NULL;
    BOOL32          bFound      = FALSE32;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    pstCI         = pstZone->pstCtxHdl;
    pstZI         = pstZone->pstZI;
    pstAGrpLst    = pstZone->pstActLogGrpList;
    
    for (nIdx = 0; nIdx < pstCI->pstFm->nNumFBlks; nIdx++)
    {
        nFBIdx = pstCI->pstFm->nFreeListHead + nIdx;
        if (nFBIdx >= nMaxFBlks)
        {
            nFBIdx -= nMaxFBlks;
        }
        FSR_ASSERT(nFBIdx < nMaxFBlks);

        nVbn = pstCI->pFreeList[nFBIdx];

        for (i = 0; i < pstZI->nNumMetaBlks; i++)
        {
            FSR_ASSERT(nVbn != pstZI->aMetaVbnList[i]);
        }
        
    }

    FSR_ASSERT(pstCI->pstFm->nNumFBlks >= 1);

    FSR_ASSERT(pstAGrpLst->nNumLogGrps <= pstCI->pstFm->nNumLBlks);

    pstAGrp = pstAGrpLst->pstHead;
    for (nIdx = 0; nIdx < pstAGrpLst->nNumLogGrps; nIdx++)
    {
        bFound = FALSE32;

        for (i = 0; i < pstCI->pstFm->nNumLBlks; i++)
        {
            if (pstAGrp->pstFm->nDgn == pstCI->pstActLogList[i].nDgn)
            {
                bFound = TRUE32;
                break;
            }
        }

        FSR_ASSERT(bFound == TRUE32);
        pstAGrp = pstAGrp->pNext;
    }

    for (nIdx = 0; nIdx < pstCI->pstFm->nNumFBlks; nIdx++)
    {
        bFound = FALSE32;

        nFBIdx = pstCI->pstFm->nFreeListHead + nIdx;
        if (nFBIdx >= nMaxFBlks)
        {
            nFBIdx -= nMaxFBlks;
        }
        FSR_ASSERT(nFBIdx < nMaxFBlks);

        nVbn = pstCI->pFreeList[nFBIdx];

        for (i = 0; i < pstCI->pstFm->nNumLBlks; i++)
        {
            if (nVbn == pstCI->pstActLogList[i].nVbn)
            {
                bFound = TRUE32;
                break;
            }
        }
        
        FSR_ASSERT(bFound == FALSE32);
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s\r\n"), __FSR_FUNC__));
}

/**
 *  @brief          This function check the validity of pstPart
 *
 *  @param[in]      pstZone          : pointer to atl Zone object
 *
 *  @return         none
 *
 *  @author         Jongtae Park
 *  @version        1.2.0
 */
PUBLIC VOID 
FSR_STL_DBG_CheckValidPgCnt(STLZoneObj *pstZone)
{
    const   UINT32  nPgsPerSBlk     = pstZone->pstDevInfo->nPagesPerSBlk;
    const   UINT32  nPgsSft         = pstZone->pstDevInfo->nPagesPerSbShift;
    UINT32          nValidIdx;
    UINT32          nLoopIdx;
    UINT32          nIdx, i;
    UINT32          nValidPgCnt;
    BOOL32          bBMTFound;
    POFFSET         nMetaPOffs;
    STLDirHdrHdl   *pstDH;
    STLZoneInfo    *pstZI;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    pstDH       = pstZone->pstDirHdrHdl;
    pstZI       = pstZone->pstZI;

    /* Compare the number of ValidPgCnt & aBMTDir, aPMTDir  */
    for (nValidIdx = 0; nValidIdx < pstZI->nNumMetaBlks; nValidIdx++)
    {
        nValidPgCnt = 0;
        for (nIdx = pstZI->nNumDirHdrPgs; nIdx < nPgsPerSBlk; nIdx++)
        {
            bBMTFound = FALSE32;
            /* nMetaPOffs = (POFFSET)((nValidIdx * nPgsPerSBlk) + (nIdx % nPgsPerSBlk)); */
            nMetaPOffs = (POFFSET)((nValidIdx << nPgsSft) + (nIdx & (nPgsPerSBlk - 1)));
            for (nLoopIdx = 0; nLoopIdx < pstZI->nNumLA; nLoopIdx++)
            {
                if (pstDH->pBMTDir[nLoopIdx] == nMetaPOffs)
                {
                    bBMTFound = TRUE32;
                    nValidPgCnt++;
                }
            }
            if (bBMTFound == TRUE32)
            {
                continue;
            }
            for (nLoopIdx = 0; nLoopIdx < pstZI->nMaxPMTDirEntry; nLoopIdx+=pstZI->nNumLogGrpPerPMT)
            {
                for (i=0; i < pstZI->nNumLogGrpPerPMT; i++)
                {
                    if ((nLoopIdx + i) < pstZI->nMaxPMTDirEntry )
                    {
                        if (pstDH->pstPMTDir[nLoopIdx + i].nPOffs == nMetaPOffs)
                        {
                            nValidPgCnt++;
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }
        FSR_ASSERT(pstDH->pValidPgCnt[nValidIdx] == nValidPgCnt);
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s\r\n"), __FSR_FUNC__));
}

/**
 *  @brief          This function checks the validity of pstPart
 *
 *  @param[in]      pstZone          : pointer to atl Zone object
 *
 *  @return         none
 *
 *  @author         Jongtae Park
 *  @version        1.2.0
 */
PUBLIC VOID 
FSR_STL_DBG_CheckMergeVictim(STLZoneObj *pstZone)
{
    INT32           nIdx, i, j;
    INT32           nDBlkIdx;
    INT32           nLogIdx;
    BADDR           nLbn;
    BADDR           nLan;
    INT32           nRet;
    POFFSET         nLbOffs;
    INT32           nByteOffs;
    INT32           nBitOffs;
    UINT32          nCxtVpn;
    UINT32          nCxtVbn;
    const   UINT32  nPgPerSBlk      = pstZone->pstDevInfo->nPagesPerSBlk;
    const   UINT32  nPgSft          = pstZone->pstDevInfo->nPagesPerSbShift;
    POFFSET         bFoundIdx       = NULL_POFFSET;
    BOOL32          bMergeVictimGrp = FALSE32;
    STLRootInfo    *pstRI           = NULL;
    STLDirHdrHdl   *pstDH           = NULL;
    UINT8          *pMPgBF          = NULL;
    UINT8          *pTmpMPgBF       = NULL;
    STLLogGrpHdl   *pstLogGrp       = NULL;
    STLLogGrpHdl   *pstTempLogGrp   = NULL;
    STLZoneInfo    *pstZI           = NULL;
    STLMetaLayout  *pstML           = NULL;
    STLBMTHdl      *pstBMT          = NULL;
    STLPMTHdl      *pstPMT          = NULL;
    STLLog         *pstLog          = NULL;
    RBWDevInfo     *pstDVI          = NULL;
    VFLParam        stVFLParam;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    pstRI        = pstZone->pstRI;
    pstDVI       = pstZone->pstDevInfo;
    pstDH        = pstZone->pstDirHdrHdl;
    pstBMT       = pstZone->pstBMTHdl;
    pstPMT       = pstZone->pstPMTHdl;
    pMPgBF       = pstZone->pMetaPgBuf;
    pTmpMPgBF    = pstZone->pTempMetaPgBuf;
    pstZI        = pstZone->pstZI;
    pstML        = pstZone->pstML;

    FSR_OAM_MEMCPY(pTmpMPgBF, pMPgBF, pstML->nBMTBufSize +
                    pstML->nCtxBufSize + pstML->nPMTBufSize);

    stVFLParam.pData      = (UINT8*)pstPMT->pBuf;
    stVFLParam.bPgSizeBuf = FALSE32;
    stVFLParam.bUserData  = FALSE32;
    stVFLParam.bSpare     = TRUE32;
    stVFLParam.nBitmap    =  pstML->nPMTSBM;
    stVFLParam.nNumOfPgs  = 1;
    stVFLParam.pExtParam  = NULL;

    for (i = 0; i < pstZI->nMaxPMTDirEntry; i+=pstZI->nNumLogGrpPerPMT)
    {
        bFoundIdx = NULL_POFFSET;
        for (j = 0; j < pstZI->nNumLogGrpPerPMT; j++)
        {
            if (pstDH->pstPMTDir[i + j].nPOffs != 0xFFFF)
            {
                bFoundIdx = (POFFSET)(i + j);
                break;
            }

            if (j == (pstZI->nNumLogGrpPerPMT - 1))
            {
                bFoundIdx = NULL_POFFSET;
            }
        }

        if (bFoundIdx == NULL_POFFSET)
        {
            continue;
        }

        /*
        nCxtVpn = pstZI->aMetaVbnList[pstDH->pstPMTDir[bFoundIdx].nPOffs / nPgPerSBlk] * 
                        nPgPerSBlk + pstDH->pstPMTDir[bFoundIdx].nPOffs % nPgPerSBlk;
        */
        nCxtVbn = pstZI->aMetaVbnList[(pstDH->pstPMTDir[bFoundIdx].nPOffs) >> nPgSft];
        nCxtVpn = (nCxtVbn << nPgSft) + (pstDH->pstPMTDir[bFoundIdx].nPOffs & (nPgPerSBlk - 1));

        nRet = FSR_STL_FlashRead(pstZone, nCxtVpn, &stVFLParam, FALSE32);
        if (nRet == FSR_BML_READ_ERROR)
        {
            return;
        }
        else if (nRet != FSR_BML_SUCCESS)
        {
            return;
        }

        for (j = 0; j < pstZI->nNumLogGrpPerPMT; j++)
        {
            bMergeVictimGrp = FALSE32;

            if ((i+j) >= pstZI->nMaxPMTDirEntry)
            {
                break;
            }

            if (pstDH->pstPMTDir[i + j].nPOffs == 0xFFFF)
            {
                continue;
            }

            pstLogGrp    = &(pstPMT->astLogGrps[j]);

            FSR_ASSERT(pstLogGrp->pstFm->nDgn == (i + j));

            /*  case1) if the number of log blocks in the group is greater than 'N'  */
            if (pstLogGrp->pstFm->nNumLogs > pstRI->nN)
            {
                bMergeVictimGrp = TRUE32;
            }
            else
            {
                /*  case2) when data block is not NULL and log block exists */
                nLan = (BADDR)((pstLogGrp->pstFm->nDgn * pstRI->nN) >> pstZI->nDBlksPerLAShift);

                nRet = FSR_STL_LoadBMT(pstZone, nLan, FALSE32);
                if (nRet != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                            __FSR_FUNC__, __LINE__, nRet));
                }

                nLogIdx = pstLogGrp->pstFm->nHeadIdx;
                for (nIdx = 0; nIdx < pstLogGrp->pstFm->nNumLogs; nIdx++)
                {
                    pstLog = &(pstLogGrp->pstLogList[nLogIdx]);

                    FSR_ASSERT(*(pstLogGrp->pNumDBlks + pstLog->nSelfIdx) < 2);

                    for (nDBlkIdx = 0; nDBlkIdx < *(pstLogGrp->pNumDBlks + pstLog->nSelfIdx); nDBlkIdx++)
                    {
                        nLbn    = pstLogGrp->pLbnList[(pstLog->nSelfIdx << pstRI->nNShift) + nDBlkIdx];
                        nLbOffs = (POFFSET)(nLbn & (pstZI->nDBlksPerLA - 1));


                        if (((pstBMT->pGBlkFlags[nLbOffs >> 3] & (1 << (nLbOffs & 7))) == 0) &&
                            (pstBMT->pMapTbl[nLbOffs].nVbn != NULL_VBN))
                        {
                            bMergeVictimGrp = TRUE32;
                            break;
                        }
                    }

                    if (bMergeVictimGrp == TRUE32)
                    {
                        break;
                    }

                    nLogIdx = pstLog->nNextIdx;
                }
            }

            /*check MergeVictimFlag*/
            nByteOffs = (i + j) / 8;
            nBitOffs = 7 - ((i + j) % 8);

            pstTempLogGrp = FSR_STL_SearchLogGrp(pstZone->pstActLogGrpList, pstLogGrp->pstFm->nDgn);

            if (pstTempLogGrp == NULL)
            {
                if (pstDH->pPMTMergeFlags[nByteOffs] & (1 << nBitOffs))
                {
                    FSR_ASSERT(bMergeVictimGrp == TRUE32);
                }
                else
                {
                    FSR_ASSERT(bMergeVictimGrp == FALSE32);
                }
            }
        }
    }
    
    FSR_OAM_MEMCPY(pMPgBF, pTmpMPgBF, pstML->nBMTBufSize +
                    pstML->nCtxBufSize + pstML->nPMTBufSize);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s\r\n"), __FSR_FUNC__));
}

/**
 *  @brief          This function identify the history of PMT
 *
 *  @param[in]      pstZone          : pointer to stl Zone object
 *  @param[in]      nDgn             : data group number
 *
 *  @return         none
 *
 *  @author         Jongtae Park
 *  @version        1.2.0
 */
PUBLIC VOID 
FSR_STL_DBG_PMTHistoryLog(STLZoneObj *pstZone, BADDR nDgn)
{
    UINT32          nIdx,i,j;
    UINT16          nTemp;
    UINT32          nTemp1;
    UINT16          nMetaType;
    BOOL32          bValid = FALSE32;
    UINT32          nZBCHeader;
    INT32           nRet;
    BADDR           nVbn;
    UINT32          nVpn;
    UINT8           *pMPgBF;
    UINT8           *pTmpMPgBF;
    UINT8           *pBpMPgBF;
    STLPMTHdl       *pstPMT         = pstZone->pstPMTHdl;
    STLZoneInfo     *pstZI          = pstZone->pstZI;
    STLMetaLayout   *pstML          = pstZone->pstML;
    RBWDevInfo      *pstDVI         = NULL;
    UINT32          nPgPerSBlk      = pstZone->pstDevInfo->nPagesPerSBlk;
    const   UINT32          nPgSft          = pstZone->pstDevInfo->nPagesPerSbShift;
    STLLogGrpHdl    *pstLogGrp;
    VFLParam        *pstVFLParam;
    UINT32          nMetaAge[32];
    UINT16          nOrder[32];
    POFFSET         *pMapTbl;

    pstDVI          = pstZone->pstDevInfo;
    pMPgBF          = pstZone->pMetaPgBuf;
    pTmpMPgBF       = pstZone->pTempMetaPgBuf;
    pstVFLParam     = FSR_STL_AllocVFLParam(pstZone);

    pBpMPgBF        = (UINT8*)FSR_STL_MALLOC(pstML->nBMTBufSize + pstML->nCtxBufSize + pstML->nPMTBufSize, 
                                            FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_SRAM);

    pMapTbl         = (POFFSET*)FSR_STL_MALLOC(pstML->nPagesPerLGMT * sizeof(POFFSET),
                                            FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_SRAM);

    FSR_OAM_MEMCPY(pBpMPgBF, pMPgBF, pstML->nBMTBufSize + pstML->nCtxBufSize + pstML->nPMTBufSize);

    if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
    {
        nPgPerSBlk >>= 1;
    }

    /* initialize nOrder, nMetaAge array */
    for (nIdx = 0; nIdx < 32; nIdx++)
    {
        nOrder[nIdx] = 0xFFFF;
        nMetaAge[nIdx] = 0xFFFFFFFF;
    }

    FSR_OAM_MEMSET(pMapTbl, 0xFF, sizeof(POFFSET) * pstML->nPagesPerLGMT);

    /* Set VFL Parameter */
    pstVFLParam->pData      = (UINT8*)pTmpMPgBF;
    pstVFLParam->bPgSizeBuf = FALSE32;
    pstVFLParam->bUserData  = FALSE32;
    pstVFLParam->bSpare     = TRUE32;
    pstVFLParam->nBitmap    = pstDVI->nFullSBitmapPerVPg;
    pstVFLParam->nNumOfPgs  = 1;
    pstVFLParam->pExtParam  = FSR_STL_AllocVFLExtParam(pstZone);
    
    /*
     * 1. Scan header page for storing meta blk's age
     */
    for (nIdx = 0; nIdx < pstZI->nNumMetaBlks; nIdx++)
    {
        nVbn = *(pstZI->aMetaVbnList + nIdx);

        /* get VPN address of header page of each meta blocks */
        if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
        {
            nVpn = (nVbn << nPgSft) 
                            + FSR_BML_GetVPgOffOfLSBPg(pstZone->nVolID, pstZI->nNumDirHdrPgs - 1);
        }
        else
        {
            nVpn = (nVbn << nPgSft)
                 + (pstZI->nNumDirHdrPgs - 1);
        }

        /* first, read main & spare data to check up age and signature */
        nRet = FSR_STL_FlashRead(pstZone, nVpn, pstVFLParam, TRUE32);

        /* calculate zero bit count for header page confirm */
        nZBCHeader = FSR_STL_GetZBC(pstVFLParam->pData, pstDVI->nBytesPerVPg);

        if (nRet == FSR_STL_CLEAN_PAGE)
        {
             continue;
        }
        else if (nRet == FSR_BML_READ_ERROR)
        {
            /* sudden power off case */
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
                (TEXT("[SIF:DBG] CxtHeader : U_ECC (nVpn = %d)\r\n"), nVpn));
            continue;
        }
        else if ((pstVFLParam->nSData1 != nZBCHeader) ||
                 ((pstVFLParam->nSData2 ^ 0xFFFFFFFF) != 
                   pstVFLParam->nSData1))
        {
            /* sudden power off case */
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
                (TEXT("[SIF:DBG] CxtHeader : Meta Broken (nVpn = %d)\r\n"), nVpn));
            continue;
        }
        else if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
                (TEXT("[SIF:DBG] CxtHeader : VFL Error (nVpn = %d)\r\n"), nVpn));
            FSR_ASSERT(FALSE32);
        }
        else
        {
            bValid = TRUE32;
        }


        /* check header page validity */
        if (bValid == FALSE32)
        {
            continue;
        }

        nMetaAge[nIdx] = pstVFLParam->nSData1;
    }

    for (i=0; i< pstZI->nNumMetaBlks; i++)
    {
        nOrder[i] = (UINT16)i;

        if (nMetaAge[i] == 0xFFFFFFFF)
        {
            nOrder[i] = 0xFFFF;
        }
    }

    /*
     * 2. Sort meta block
     */
    for(i = 0; i < (UINT32)(pstZI->nNumMetaBlks - 1); i++)
    {
        for(j = i+1; j < pstZI->nNumMetaBlks; j++) 
        {
            if(nMetaAge[i] > nMetaAge[j]) 
            {
                nTemp = nOrder[i];
                nOrder[i] = nOrder[j];
                nOrder[j] = nTemp;
                
                nTemp1 = nMetaAge[i];
                nMetaAge[i] = nMetaAge[j];
                nMetaAge[j] = nTemp1;
                
            }
        }
    }

    /*
     * 3. Print PMT
     */
    for (i = (pstZI->nNumMetaBlks - 1); i > 0; i--)
    {
        if (nOrder[i] != 0xFFFF)
        {
            nVbn = *(pstZI->aMetaVbnList + nOrder[i]);

            for (j = pstZI->nNumDirHdrPgs; j < nPgPerSBlk; j++)
            {
                /* get VPN address of header page of each meta blocks */
                if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
                {

                    nVpn = (nVbn << nPgSft) 
                                    + FSR_BML_GetVPgOffOfLSBPg(pstZone->nVolID, j & (nPgPerSBlk - 1));
                }
                else
                {
                    nVpn = (nVbn << nPgSft) + (j & (nPgPerSBlk - 1));
                }

                pstLogGrp = &(pstPMT->astLogGrps[nDgn % pstZI->nNumLogGrpPerPMT]);

                nRet = FSR_STL_FlashRead(pstZone, nVpn, pstVFLParam, FALSE32);
                if (nRet == FSR_STL_CLEAN_PAGE)
                {
                    continue;
                }
                else if (nRet == FSR_BML_READ_ERROR)
                {
                    continue;
                }
                else if (nRet != FSR_BML_SUCCESS)
                {
                    continue;
                }

                nMetaType = (UINT16)(pstVFLParam->nSData1 >> 16);

                if (nMetaType == 0xFFFF)
                {
                    continue;
                }

                if (nMetaType == MT_PMT)
                {
                    FSR_OAM_MEMCPY((pMPgBF + pstML->nBMTBufSize), pTmpMPgBF, 
                                    pstML->nCtxBufSize + pstML->nPMTBufSize);

                    if (pstLogGrp->pstFm->nDgn == nDgn)
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG,
                            (TEXT("[SIF:INF] Page Mapping Table : BlkIdx(%d) PgIdx(%d)\r\n")
                            ,nOrder[i], j));

                        for (nIdx = 0; nIdx < pstML->nPagesPerLGMT; nIdx++)
                        {
                            if (pMapTbl[nIdx] != pstLogGrp->pMapTbl[nIdx])
                            {
                                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG,
                                                (TEXT("[SIF:INF]  [%d] : %d \r\n"),
                                                nIdx, pstLogGrp->pMapTbl[nIdx]));
                            }
                            pMapTbl[nIdx] = pstLogGrp->pMapTbl[nIdx];
                        }
                    }
                }
            }
        }
        
        if (i == 0)
        {
            break;
        }
        
    }

    FSR_OAM_MEMCPY(pMPgBF, pBpMPgBF, pstML->nBMTBufSize + pstML->nCtxBufSize + pstML->nPMTBufSize);

    if (pBpMPgBF != NULL)
    {
        FSR_OAM_Free(pBpMPgBF);
        pBpMPgBF = NULL;
    }

    if (pMapTbl != NULL)
    {
        FSR_OAM_Free(pMapTbl);
        pMapTbl = NULL;
    }
}

PUBLIC VOID
FSR_STL_GetPTF (STLZoneObj *pstZone,
                UINT32      nVbn)
{
    UINT32      nIdx;
    UINT32      nVpn;
    INT32       nRet;
    UINT8       *pTmpMPgBF;
    FILE *      m_Fp; 
    const       UINT32  nPgsPerSBlk     = pstZone->pstDevInfo->nPagesPerSBlk;
    const       UINT32  nPgsSft         = pstZone->pstDevInfo->nPagesPerSbShift;
    VFLParam    stVFLParam;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    pTmpMPgBF    = pstZone->pTempMetaPgBuf;

    stVFLParam.pData      = (UINT8*)pTmpMPgBF;
    stVFLParam.bPgSizeBuf = FALSE32;
    stVFLParam.bUserData  = FALSE32;
    stVFLParam.bSpare     = TRUE32;
    stVFLParam.nBitmap    =  0xFF;
    stVFLParam.nNumOfPgs  = 1;
    stVFLParam.pExtParam  = NULL;

    m_Fp = fopen((char *) "nSData123.txt", "wt");

    for (nIdx = 0; nIdx < nPgsPerSBlk; nIdx++)
    {
        nVpn = (nVbn << nPgsSft) + nIdx;
        nRet = FSR_STL_FlashRead(pstZone, nVpn, &stVFLParam, FALSE32);

        fprintf(m_Fp, "%8x %8x %8x\r\n", 
            (const char *) stVFLParam.nSData1,
            (const char *) stVFLParam.nSData2,
            (const char *) stVFLParam.nSData3);

    }

    fclose(m_Fp);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__));

}

PUBLIC BOOL32
FSR_STL_GetVbn (STLZoneObj *pstZone,
                UINT32      nVbn,
                UINT32     *pnBlkCnt)
{
    UINT32          nIdx, i, j;
    UINT32          nVpn;
    BADDR           nMVbn;
    INT32           nRet;
    UINT16          nFBIdx;
    UINT16          nLogIdx;
    BOOL32          bFound;
    BOOL32          bFoundVbn = FALSE32;
    UINT16          nDgn    = NULL_DGN;
    UINT16          nMPOffs = NULL_POFFSET;
    UINT8           *pMPgBF;
    UINT8           *pTmpMPgBF;
    STLZoneInfo     *pstZI;
    STLCtxInfoHdl   *pstCI;
    STLDirHdrHdl    *pstDI;
    STLMetaLayout   *pstML;
    STLPMTHdl       *pstPMT;
    STLBMTHdl       *pstBMT;
    STLLog          *pstLog;
    const       UINT32  nMaxFBlks       = pstZone->pstML->nMaxFreeSlots;
    const       UINT32  nPgsPerSBlk     = pstZone->pstDevInfo->nPagesPerSBlk;
    const       UINT32  nPgsSft         = pstZone->pstDevInfo->nPagesPerSbShift;
    VFLParam    stVFLParam;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    pstZI       = pstZone->pstZI;
    pstCI       = pstZone->pstCtxHdl;
    pstDI       = pstZone->pstDirHdrHdl;
    pstML       = pstZone->pstML;
    pstPMT      = pstZone->pstPMTHdl;
    pstBMT      = pstZone->pstBMTHdl;
    pMPgBF      = pstZone->pMetaPgBuf;
    pTmpMPgBF   = pstZone->pTempMetaPgBuf;

    FSR_OAM_MEMCPY(pTmpMPgBF, pMPgBF, pstML->nBMTBufSize + pstML->nCtxBufSize + pstML->nPMTBufSize);

    /* 1 search nVbn in meta blocks */
    for(nIdx = 0; nIdx < pstZI->nNumMetaBlks; nIdx++)
    {
        if (nVbn == *(pstZI->aMetaVbnList + nIdx))
        {
            bFoundVbn = TRUE32;
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
                (TEXT("[SIF:INF] ZoneID(%d) nVbn (%4d) nMetaIdx(%2d) : (%4d)\r\n")
                            ,pstZone->nZoneID, nVbn, nIdx, *(pstZI->aMetaVbnList + nIdx)));

            pnBlkCnt[pstZone->nZoneID]++;
        }
    }

    /* 2 search nVbn in BU blocks */
    if (nVbn == pstCI->pstFm->nBBlkVbn)
    {
        if(bFoundVbn == TRUE32)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[-------] VBN[%d] is duplicated\r\n"),
                    nVbn));
            FSR_ASSERT(FALSE32);
        }

        bFoundVbn = TRUE32;

        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
            (TEXT("[SIF:INF] ZoneID(%d) nVbn (%4d) BUBlk : (%4d)\r\n")
                        ,pstZone->nZoneID, nVbn, nIdx, *(pstZI->aMetaVbnList + nIdx)));

        pnBlkCnt[pstZone->nZoneID]++;
    }

    /* 3 search nVbn in free blocks */
    for (nIdx = 0; nIdx < pstCI->pstFm->nNumFBlks; nIdx++)
    {
        nFBIdx = (UINT16)(pstCI->pstFm->nFreeListHead + nIdx);
        if (nFBIdx >= nMaxFBlks)
        {
            nFBIdx = (UINT16)(nFBIdx - nMaxFBlks);
        }
        FSR_ASSERT(nFBIdx < nMaxFBlks);

        if (nVbn == pstCI->pFreeList[nFBIdx])
        {
            if(bFoundVbn == TRUE32)
            {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[-------] VBN[%d] is duplicated\r\n"),
                        nVbn));
                FSR_ASSERT(FALSE32);
            }
            bFoundVbn = TRUE32;
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
                (TEXT("[SIF:INF] ZoneID(%d) nVbn (%4d) nFreeIdx(%2d) : (%4d)\r\n")
                            ,pstZone->nZoneID, nVbn, nFBIdx, pstCI->pFreeList[nFBIdx]));

            pnBlkCnt[pstZone->nZoneID]++;
        }
    }

#if 0
    /* 3. search nVbn in Active log group */
    for (nIdx = 0; nIdx < pstCI->pstFm->nNumLBlks; nIdx++)
    {
        if ( nVbn == pstCI->pstActLogList[nIdx].nVbn )
        {
            if(bFoundVbn == TRUE32)
            {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[-------] VBN[%d] is duplicated\r\n"),
                        nVbn));
                FSR_ASSERT(FALSE32);
        }
            bFoundVbn = TRUE32;
            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
                (TEXT("[SIF:INF] ZoneID(%d) nVbn (%4d) nActiveIdx(%2d) : (%4d)\r\n")
                            ,pstZone->nZoneID, nVbn, nIdx, pstCI->pstActLogList[nIdx].nVbn));

            pnBlkCnt[pstZone->nZoneID]++;
    }
    }
#endif

    /* 4 search nVbn in PMT */
    stVFLParam.pData      = (UINT8*)pstPMT->pBuf;
    stVFLParam.bPgSizeBuf = FALSE32;
    stVFLParam.bUserData  = FALSE32;
    stVFLParam.bSpare     = TRUE32;
    stVFLParam.nBitmap    =  pstML->nPMTSBM;
    stVFLParam.nNumOfPgs  = 1;
    stVFLParam.pExtParam  = NULL;

    for (nIdx = 0; nIdx < pstZI->nMaxPMTDirEntry; nIdx+=pstZI->nNumLogGrpPerPMT)
    {
        bFound = FALSE32;
        for (i=0; i<pstZI->nNumLogGrpPerPMT; i++)
        {
            if (pstDI->pstPMTDir[nIdx + i].nPOffs != NULL_POFFSET)
            {
                nMPOffs = pstDI->pstPMTDir[nIdx + i].nPOffs;
                bFound = TRUE32;
                break;
            }
        }

        if (bFound == TRUE32)
        {
        nMVbn = *(pstZI->aMetaVbnList + (nMPOffs >> nPgsSft));
        nVpn = (nMVbn << nPgsSft) + (nMPOffs & (nPgsPerSBlk - 1));
        nRet = FSR_STL_FlashRead(pstZone, nVpn, &stVFLParam, FALSE32);
        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_ASSERT(FALSE32);
        }
        
        for (i=0; i<pstZI->nNumLogGrpPerPMT; i++)
        {
            nDgn = pstPMT->astLogGrps[i].pstFm->nDgn;
            if (nDgn != NULL_DGN)
            {
                nLogIdx = pstPMT->astLogGrps[i].pstFm->nHeadIdx;

                for (j=0; j<pstPMT->astLogGrps[i].pstFm->nNumLogs; j++)
                {
                    pstLog = &(pstPMT->astLogGrps[i].pstLogList[nLogIdx]);

                    if (nVbn == pstLog->nVbn)
                    {
                            if(bFoundVbn == TRUE32)
                            {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                                    (TEXT("[-------] VBN[%d] is duplicated\r\n"),
                                        nVbn));
                                FSR_ASSERT(FALSE32);
                            }
                            bFoundVbn = TRUE32;
                            FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
                                (TEXT("[SIF:INF] ZoneID(%d) nVbn (%4d) nDgn(%4d) LogIdx(%2d) nCPOffs(%3d)\r\n")
                                            ,pstZone->nZoneID, nVbn, nDgn, nLogIdx, pstLog->nCPOffs));

                            pnBlkCnt[pstZone->nZoneID]++;
                    }

                    nLogIdx = pstLog->nNextIdx;
                }
            }
        }
    }
    }

    /* 5 search nVbn in BMT */
    stVFLParam.pData      = (UINT8*)pstBMT->pBuf;
    stVFLParam.nBitmap    =  pstML->nBMTSBM;;
    for (nIdx = 0; nIdx < pstZI->nNumLA; nIdx++)
    {
        nMPOffs = pstDI->pBMTDir[nIdx];
        nMVbn    = *(pstZI->aMetaVbnList + (nMPOffs >> nPgsSft));
        nVpn = (nMVbn << nPgsSft) + (nMPOffs & (nPgsPerSBlk - 1));
        nRet = FSR_STL_FlashRead(pstZone, nVpn, &stVFLParam, FALSE32);
        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_ASSERT(FALSE32);
        }

        for (i=0; i<pstZI->nDBlksPerLA; i++)
        {
            if (nVbn == pstBMT->pMapTbl[i].nVbn)
            {
                if(bFoundVbn == TRUE32)
                {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[-------] VBN[%d] is duplicated\r\n"),
                            nVbn));
                    FSR_ASSERT(FALSE32);
                }
                bFoundVbn = TRUE32;
                FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
                            (TEXT("[SIF:INF] ZoneID(%d) nVbn (%4d) BMTIdx(%2d) nVbn(%4d)\r\n")
                                        ,pstZone->nZoneID, nVbn, i, pstBMT->pMapTbl[i].nVbn));

                pnBlkCnt[pstZone->nZoneID]++;
            }
        }
    }

    FSR_OAM_MEMCPY(pMPgBF, pTmpMPgBF, pstML->nBMTBufSize + pstML->nCtxBufSize + pstML->nPMTBufSize);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__));
    return bFoundVbn;
}

PUBLIC VOID 
FSR_STL_DBG_CheckWLGrp(STLZoneObj *pstZone)
{
    STLMetaLayout  *pstML           = pstZone->pstML;
    STLZoneInfo    *pstZI           = pstZone->pstZI;
    STLDirHdrHdl   *pstDH           = pstZone->pstDirHdrHdl;
    STLPMTHdl      *pstPMT          = pstZone->pstPMTHdl;
    UINT8          *pMPgBF          = pstZone->pMetaPgBuf;
    UINT8          *pTmpMPgBF       = pstZone->pTempMetaPgBuf;
    STLLogGrpHdl   *pstLogGrp;
    STLPMTWLGrp    *pstCompPMTWLGrp;
    STLPMTWLGrp    *pstPMTWLGrp;
    UINT32          nSDgnInWLGrp;
    UINT32          nNumPMTInWLGrp;
    BADDR           nTmpDgn;
    VFLParam        stVFLParam;
    const   UINT32  nPgsPerSBlk     = pstZone->pstDevInfo->nPagesPerSBlk;
    const   UINT32  nPgsSft         = pstZone->pstDevInfo->nPagesPerSbShift;
    UINT32          nIdx, i;
    UINT32          nVpn;
    BADDR           nVbn;
    UINT32          nWLGrpCnt;
    INT32           nRet;
    POFFSET         nMPOffs;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    FSR_OAM_MEMCPY(pTmpMPgBF, pMPgBF, pstML->nBMTBufSize + pstML->nCtxBufSize + pstML->nPMTBufSize);

    stVFLParam.pData      = (UINT8*)pstPMT->pBuf;
    stVFLParam.bPgSizeBuf = FALSE32;
    stVFLParam.bUserData  = FALSE32;
    stVFLParam.bSpare     = TRUE32;
    stVFLParam.nBitmap    =  pstML->nPMTSBM;
    stVFLParam.nNumOfPgs  = 1;
    stVFLParam.pExtParam  = NULL;

    nWLGrpCnt   = ((pstZone->pstZI->nMaxPMTDirEntry - 1) >> DEFAULT_PMT_EC_GRP_SIZE_SHIFT) + 1;

    for (nIdx = 0; nIdx < nWLGrpCnt; nIdx++)
    {
        pstCompPMTWLGrp = &(pstDH->pstPMTWLGrp[nIdx]);

        for (i = 0; i < nWLGrpCnt; i++)
        {
            pstPMTWLGrp = &(pstDH->pstPMTWLGrp[i]);

            if ( (nIdx != i) &&
                 (pstCompPMTWLGrp->nMinVbn != NULL_VBN) &&
                 (pstPMTWLGrp->nMinVbn != NULL_VBN) )
            {
                FSR_ASSERT(pstCompPMTWLGrp->nMinVbn != pstPMTWLGrp->nMinVbn);
            }
             
        }
    }

    for (nIdx = 0; nIdx < nWLGrpCnt; nIdx++)
    {
        nSDgnInWLGrp   = (nIdx) << DEFAULT_PMT_EC_GRP_SIZE_SHIFT;
        nNumPMTInWLGrp = DEFAULT_PMT_EC_GRP_SIZE;
        if (nSDgnInWLGrp + nNumPMTInWLGrp >= (UINT32)(pstZI->nNumUserBlks >> pstZone->pstRI->nNShift))
        {
            nNumPMTInWLGrp = (pstZI->nNumUserBlks >> pstZone->pstRI->nNShift) - nSDgnInWLGrp;
        }

        pstPMTWLGrp = &(pstDH->pstPMTWLGrp[nIdx]);
        for (i = 0; i < nNumPMTInWLGrp; i++)
        {
            nTmpDgn   = (BADDR)(nSDgnInWLGrp + i);
            pstLogGrp = pstPMT->astLogGrps + (nTmpDgn % pstZI->nNumLogGrpPerPMT);
            if (pstLogGrp->pstFm->nDgn != nTmpDgn)
            {
                FSR_STL_SearchMetaPg(pstZone, nTmpDgn, &nMPOffs);
                if (nMPOffs == NULL_POFFSET)
                {
                    FSR_ASSERT(nTmpDgn != pstPMTWLGrp->nDgn);
                    continue;
                }
                nVbn = pstZI->aMetaVbnList[nMPOffs >> nPgsSft];
                nVpn = (nVbn << nPgsSft) + (nMPOffs & (nPgsPerSBlk - 1));
                nRet = FSR_STL_FlashRead(pstZone, nVpn, &stVFLParam, FALSE32);
                if (nRet != FSR_BML_SUCCESS)
                {
                    FSR_ASSERT(FALSE32);
                }
            }

            if (nTmpDgn == pstPMTWLGrp->nDgn)
            {
                /* 1. compare min VBN of self set */
                FSR_ASSERT(pstPMTWLGrp->nMinVbn == pstLogGrp->pstFm->nMinVbn);
                FSR_ASSERT(pstPMTWLGrp->nMinEC  == pstLogGrp->pstFm->nMinEC);
            }
            else
            {
                /* 2. compare min VBN of another set */
                if (pstPMTWLGrp->nDgn == NULL_DGN)
                {
                    FSR_ASSERT(pstPMTWLGrp->nMinVbn == NULL_VBN);;
                    FSR_ASSERT(pstPMTWLGrp->nMinEC  == 0xFFFFFFFF);
                    FSR_ASSERT(pstPMTWLGrp->nMinVbn == pstLogGrp->pstFm->nMinVbn);
                    FSR_ASSERT(pstPMTWLGrp->nMinEC  == pstLogGrp->pstFm->nMinEC);
                }
                else
                {
                    FSR_ASSERT(pstPMTWLGrp->nMinVbn != pstLogGrp->pstFm->nMinVbn);
                    FSR_ASSERT(pstPMTWLGrp->nMinEC  <= pstLogGrp->pstFm->nMinEC);
                }
            }
        }
    }

    FSR_OAM_MEMCPY(pMPgBF, pTmpMPgBF, pstML->nBMTBufSize + pstML->nCtxBufSize + pstML->nPMTBufSize);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__));
    

}

#endif

/**
 *  @brief          This function returns the shift bit of input parameter
 *
 *  @param[in]      nVal      : An integer 
 *
 *  @return         UINT32
 *
 *  @author         Wonmoon Cheon
 *  @version        1.1.0
 */
PUBLIC UINT32
FSR_STL_GetShiftBit(UINT32 nVal)
{
    UINT32  nShiftBits = 0;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    while ( (nVal >> 1) != 0 )
    {
        nShiftBits++;
        nVal >>= 1;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
        (TEXT("[SIF:OUT]  --%s() : %4d\r\n"), __FSR_FUNC__, nShiftBits));
    return nShiftBits;
}

