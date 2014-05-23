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
 *  @file       FSR_STL_WBInterface.c
 *  @brief      This file contains the routine for interfaces of FSR_STL Write Buffer module
 *              such as FSR_STL_WBRead, FSR_STL_WBWrite, and etc.
 *  @author     Seunghyun Han
 *  @date       30-JUL-2008
 *  @remark
 *  REVISION HISTORY
 *  @n  30-JUL-2008 [Seunghyun Han] : first writing
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
/* Macro                                                                     */
/*****************************************************************************/
#define WB_AUTO_FLUSHING                            (1)

/*****************************************************************************/
/* Local type defines                                                        */
/*****************************************************************************/

/*****************************************************************************/
/* Global variable definitions                                               */
/*****************************************************************************/

/*****************************************************************************/
/* Local (static) variable definitions                                       */
/*****************************************************************************/

/*****************************************************************************/
/* Imported variable declarations                                            */
/*****************************************************************************/
#if defined (FSR_POR_USING_LONGJMP)
extern BOOL32          gIsPorClose;
#endif

/*****************************************************************************/
/* Imported function prototype                                               */
/*****************************************************************************/

/*****************************************************************************/
/* Local macro                                                               */
/*****************************************************************************/
#define WB_NULL_LPN                                 (0xFFFFFFFF)
#define WB_DEL_LPN                                  (0xFFFFFFFE)
#define WB_NULL_POFFSET                             (0xFFFF)
#define WB_NULL_BOFFSET                             (0xFFFF)

#define WB_SLC_ENDURANCE                            (50000)         /* FIXME: must get from LLD */
#define WB_MLC_ENDURANCE                            (10000)         /* FIXME: must get from LLD */

#define WB_HASH_SFT                                 (4)
#define WB_GET_HASH_INDEX(p)                        ((p) >> WB_HASH_SFT)

#define WB_MAX_WRITE_REQUEST                        (8)
#define WB_MAX_NUM_OF_COLLISION                     (16)
#define WB_MAX_DELLIST                              (32)

#define WB_MIN_BLKS_FOR_AUTO_FLUSHING               (3)

#define WB_PG_SIZE                                  (pstWBObj->nSctsPerPg * FSR_SECTOR_SIZE)

#define WB_SIG_HEADER                               0xA8B75748      /* 'WH' */
#define WB_SIG_DATA                                 0xBBBE4441      /* 'DA' */
#define WB_SIG_DELETE                               0xB3AB4C54      /* 'LT' */

#define WB_INVALID_BITMAP                           (0x0)

#define WB_BITMAP_NUM(p)                            ((p) >> 0x3)    /* (p) / 8 */
#define WB_BITMAP_OFF(p)                            ((p) &  0x7)    /* (p) % 8 */

#define WB_SET_BITMAP(p)        (pstWBObj->pPgBitmap[WB_BITMAP_NUM(p)] |= (1 << WB_BITMAP_OFF(p)))
#define WB_RESET_BITMAP(p)      (pstWBObj->pPgBitmap[WB_BITMAP_NUM(p)] &= ~(1 << WB_BITMAP_OFF(p)))
#define WB_GET_BITMAP(p)        (pstWBObj->pPgBitmap[WB_BITMAP_NUM(p)] & (1 << WB_BITMAP_OFF(p)))

#define WB_SET_IDXT(p,v)        (pstWBObj->pIndexT[(p)] = (POFFSET) (v))
#define WB_GET_IDXT(p)          (pstWBObj->pIndexT[(p)])

#define WB_SET_COLT(p,v)        (pstWBObj->pCollisionT[(p)] = (POFFSET) (v))
#define WB_GET_COLT(p)          (pstWBObj->pCollisionT[(p)])

#define WB_SET_PGT(p,v)         (pstWBObj->pPgMapT[(p)] = (PADDR) (v))
#define WB_GET_PGT(p)           (pstWBObj->pPgMapT[(p)])

#define WB_GET_VPN(b,p)         ((((b) + pstWBObj->nStartVbn) << pstWBObj->nPgsPerBlkSft) + (p))
#define WB_GET_MAPOFF(b,p)      (((b) << pstWBObj->nPgsPerBlkSft) + (p))
#define WB_GET_NEXT_BLK(b)      (((b) + 1) % pstWBObj->nNumTotalBlks)

#define WB_SET_DELLIST(p)       (pstWBObj->pDeletedList[pstWBObj->nDeletedListOff++] = (PADDR) (p))
#define WB_RESET_DELLIST()      (pstWBObj->nDeletedListOff = 0)
#define WB_GET_DELLIST(i)       (pstWBObj->pDeletedList[(i)])


/*****************************************************************************/
/* Local (static) function prototype                                         */
/*****************************************************************************/
#if (OP_SUPPORT_WRITE_BUFFER == 1)
PRIVATE INT32   _SetWBObj               (UINT32         nVol,
                                         UINT32         nPart);

PRIVATE INT32   _FreeWBObj              (UINT32         nVol,
                                         UINT32         nPart);

PRIVATE INT32   _AllocateNewBlk         (UINT32         nVol,
                                         UINT32         nPart);

PRIVATE INT32   _FlushBlk               (UINT32         nVol,
                                         UINT32         nPart,
                                         UINT32         nPgCnt);

PRIVATE INT32   _InsertMapItem          (STLWBObj      *pstWBObj,
                                         PADDR          nLpn,
                                         BADDR          nBlkOff,
                                         POFFSET        nPgOff);

PRIVATE INT32   _FindMapItem            (STLWBObj      *pstWBObj,
                                         PADDR          nLpn,
                                         BADDR         *pBlkOff,
                                         POFFSET       *pPgOff);

PRIVATE INT32   _DeleteMapItem          (STLWBObj      *pstWBObj,
                                         PADDR          nLpn,
                                         BOOL32        *pbDeleted);

PRIVATE INT32   _GetWBBitmap            (STLWBObj      *pstWBObj,
                                         UINT8         *pBuf);

PRIVATE INT32   _GetWBHashTable         (STLWBObj      *pstWBObj,
                                         BADDR          nBlkOff,
                                         UINT8         *pBuf);

PRIVATE INT32   _CheckSpareValidity     (UINT8         *pMainBuf,
                                         FSRSpareBuf   *pSpareBuf,
                                         UINT32         nPageSize,
                                         UINT32         nPageType);

PRIVATE INT32   _CheckWritable          (STLWBObj      *pstWBObj,
                                         UINT32         nLsn,
                                         UINT32         nNumOfScts);

PRIVATE INT32   _WriteHeaderPage        (UINT32         nVol,
                                         UINT32         nPart,
                                         BADDR          nBlkOff,
                                         POFFSET        nPgOff);

PRIVATE INT32   _WriteDelPage           (UINT32         nVol,
                                         UINT32         nPart,
                                         BADDR          nBlkOff,
                                         POFFSET        nPgOff);

PRIVATE INT32   _WriteUnalignedDataPage (UINT32         nVol,
                                         UINT32         nPart,
                                         BADDR          nDstBlkOff,
                                         POFFSET        nDstPgOff,
                                         PADDR          nLpn,
                                         UINT32         nLsnOff,
                                         UINT32         nNumOfScts,
                                         UINT8         *pBuf);

PRIVATE INT32   _WriteAlignedDataPages  (UINT32         nVol,
                                         UINT32         nPart,
                                         BADDR          nDstBlkOff,
                                         POFFSET        nDstPgOff,
                                         PADDR          nLpn,
                                         UINT32         nNumOfPgs,
                                         UINT32        *pActualWrPgs,
                                         UINT8         *pMainBuf);

PRIVATE INT32   _ReadUnalignedDataPage  (UINT32         nVol,
                                         UINT32         nPart,
                                         PADDR          nLpn,
                                         UINT32         nLsnOff,
                                         UINT32         nNumOfScts,
                                         UINT8         *pBuf);

PRIVATE INT32   _ReadAlignedDataPages   (UINT32         nVol,
                                         UINT32         nPart,
                                         PADDR          nLpn,
                                         UINT32         nNumOfPgs,
                                         UINT32        *pActualRdPgs,
                                         UINT8         *pMainBuf);
#endif  /* #if (OP_SUPPORT_WRITE_BUFFER == 1) */


/*****************************************************************************/
/* Local (static)  Function Definition                                       */
/*****************************************************************************/
#if (OP_SUPPORT_WRITE_BUFFER == 1)
/**
 * @brief       This function sets variables of STLWBObj, if WB exists.
 * @n           Called by FSR_STL_OpenWB()
 *
 * @param[in]   nVol        : Volume ID
 * @param[in]   nPart       : Partition ID offset (0~7)
 *
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_ERR_PARAM
 * @return      FSR_STL_ERR_PHYSICAL
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PRIVATE INT32
_SetWBObj(UINT32    nVol,
          UINT32    nPart)
{
    STLPartObj         *pstSTLPartObj;
    STLWBObj           *pstWBObj;
    STLClstObj         *pstClst;
    FSRVolSpec          stVolSpec;
    FSRPartEntry        stPartEntry;
    UINT32              n1stVpn;
    UINT32              nPgsPerUnit;
    UINT32              nZone;
    UINT32              nIndexTableSize;
    UINT32              nColTableSize;
    UINT32              nMapTableSize;
    UINT32              nBitmapSize;
    UINT32              nIdx;
    INT32               nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Get partition object */
    pstSTLPartObj = &(gstSTLPartObj[nVol][nPart]);

    do
    {
        /* Get Volume Information */
        nErr = FSR_BML_GetVolSpec(nVol,                         /* Volume ID                    */
                                 &stVolSpec,                    /* Volume Information           */
                                  FSR_BML_FLAG_NONE);           /* Must be FSR_BML_FLAG_NONE    */
        if (nErr != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - FSR_BML_GetVolSpec() (0x%x)\r\n"),
                __FSR_FUNC__, __LINE__, nErr));
            FSR_ASSERT(nErr != FSR_BML_INVALID_PARAM);
            break;
        }

        /* Get BML partition information */
        nErr = FSR_BML_LoadPIEntry(nVol,                        /* Volume ID                    */
                                   nPart + FSR_PARTID_STL0_WB,  /* Partition ID                 */
                                  &n1stVpn,                     /* The first VPN                */
                                  &nPgsPerUnit,                 /* The # of pages per unit      */
                                  &stPartEntry);                /* Partition Information        */
        if (nErr == FSR_BML_NO_PIENTRY)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:INF] %s() L(%d) - No write buffer exists (nVol=%d, nPart=%d)\r\n"),
                __FSR_FUNC__, __LINE__, nVol, nPart));
            break;
        }
        else if (nErr != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - FSR_BML_LoadPIEntry(nVol=%d, nPart=%d) (=0x%x)\r\n"),
                __FSR_FUNC__, __LINE__, nVol, nPart, nErr));
            FSR_ASSERT(nErr != FSR_BML_VOLUME_NOT_OPENED);
            FSR_ASSERT(nErr != FSR_BML_INVALID_PARAM);
            break;
        }

        /* The attribute of write buffer partition is restricted */
        if (stPartEntry.nAttr != (FSR_BML_PI_ATTR_RW | FSR_BML_PI_ATTR_SLC))
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - WB partition has wrong attributes! (nVol=%d, nPart=%d)\r\n"),
                __FSR_FUNC__, __LINE__, nVol, nPart));
            nErr = FSR_STL_PARTITION_ERROR;
            break;
        }

        /* Allocate memory for write buffer object */
        pstSTLPartObj->pstWBObj = (STLWBObj*) FSR_STL_MALLOC(sizeof(STLWBObj),
                                              FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_DRAM);
        if (pstSTLPartObj->pstWBObj == NULL)
        {
            nErr = FSR_STL_OUT_OF_MEMORY;
            break;
        }
        else if ((((UINT32)(pstSTLPartObj->pstWBObj)) & 0x03))
        {
            nErr = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }

        pstWBObj = pstSTLPartObj->pstWBObj;

        /* Initialize constants of write buffer */
        pstWBObj->nNumWays          = stVolSpec.nNumOfWays;
        pstWBObj->nPgsPerBlk        = nPgsPerUnit;
        pstWBObj->nPgsPerBlkSft     = FSR_STL_GetShiftBit(pstWBObj->nPgsPerBlk);
        pstWBObj->nSctsPerPg        = stVolSpec.nSctsPerPg;
        pstWBObj->nSctsPerPgSft     = FSR_STL_GetShiftBit(pstWBObj->nSctsPerPg);

        pstWBObj->nStartVbn         = stPartEntry.n1stVun;
        pstWBObj->nNumTotalBlks     = stPartEntry.nNumOfUnits;
        pstWBObj->nStartVpn         = n1stVpn;
        pstWBObj->nNumTotalPgs      = pstWBObj->nPgsPerBlk * pstWBObj->nNumTotalBlks;

        /* Calculate total sectors and blocks of the mother partition */
        pstWBObj->nNumTotalBlksM    = 0;
        pstWBObj->nNumTotalPgsM     = 0;
        pstClst = FSR_STL_GetClstObj(pstSTLPartObj->nClstID);
        for (nZone = 0; nZone < pstSTLPartObj->nNumZone; nZone++)
        {
            pstWBObj->nNumTotalBlksM += 
                pstClst->stZoneObj[pstSTLPartObj->nZoneID + nZone].pstZI->nNumUserBlks;
            pstWBObj->nNumTotalPgsM += 
                pstClst->stZoneObj[pstSTLPartObj->nZoneID + nZone].pstZI->nNumUserScts >> pstWBObj->nSctsPerPgSft;
        }

        /* These values will be set up later. */
        pstWBObj->nHeadBlkOff       = 0;
        pstWBObj->nTailBlkOff       = 0;
        pstWBObj->nHeadPgOff        = 0;
        pstWBObj->nHeaderAge        = 0;
        pstWBObj->bDirty            = FALSE32;

        /* Allocate and initialize memory for WB mapping tables */
        nIndexTableSize = sizeof(POFFSET) * (pstWBObj->nNumTotalPgsM >> WB_HASH_SFT);
        nColTableSize   = sizeof(POFFSET) * pstWBObj->nNumTotalPgs;
        nMapTableSize   = sizeof(PADDR) * pstWBObj->nNumTotalPgs;
        nBitmapSize     = pstWBObj->nNumTotalPgs >> 3;

        pstWBObj->pIndexT     = (POFFSET*) FSR_STL_MALLOC(nIndexTableSize,
                                           FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_DRAM);
        pstWBObj->pCollisionT = (POFFSET*) FSR_STL_MALLOC(nColTableSize,
                                           FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_DRAM);
        pstWBObj->pPgMapT     = (PADDR*)   FSR_STL_MALLOC(nMapTableSize,
                                           FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_DRAM);
        pstWBObj->pPgBitmap   = (UINT8*)   FSR_STL_MALLOC(nBitmapSize,
                                           FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_DRAM);
        if (pstWBObj->pIndexT       == NULL ||
            pstWBObj->pCollisionT   == NULL ||
            pstWBObj->pPgMapT       == NULL ||
            pstWBObj->pPgBitmap     == NULL)
        {
            nErr = FSR_STL_OUT_OF_MEMORY;
            break;
        }
        else if ((((UINT32)(pstWBObj->pIndexT))     & 0x03) ||
                 (((UINT32)(pstWBObj->pCollisionT)) & 0x03) ||
                 (((UINT32)(pstWBObj->pPgMapT))     & 0x03) ||
                 (((UINT32)(pstWBObj->pPgBitmap))   & 0x03))
        {
            nErr = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }
        FSR_OAM_MEMSET(pstWBObj->pIndexT,     0xFF, nIndexTableSize);
        FSR_OAM_MEMSET(pstWBObj->pCollisionT, 0xFF, nColTableSize);
        FSR_OAM_MEMSET(pstWBObj->pPgMapT,     0xFF, nMapTableSize);
        FSR_OAM_MEMSET(pstWBObj->pPgBitmap,   0x00, nBitmapSize);

        /* Allocate memory for deleted page list */
        pstWBObj->nDeletedListOff = 0;
        pstWBObj->pDeletedList    = (PADDR*)         FSR_STL_MALLOC(WB_MAX_DELLIST * sizeof(PADDR),
                                                     FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_DRAM);
        if (pstWBObj->pDeletedList == NULL)
        {
            nErr = FSR_STL_OUT_OF_MEMORY;
            break;
        }
        else if (((UINT32)(pstWBObj->pDeletedList))       & 0x03)
        {
            nErr = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }

        FSR_OAM_MEMSET(pstWBObj->pDeletedList,  0xFF, WB_MAX_DELLIST * sizeof(PADDR));

        /* Allocate memory for temporary buffers */
        pstWBObj->pMainBuf          = (UINT8*)           FSR_STL_MALLOC(WB_PG_SIZE,
                                                         FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_DRAM);
        pstWBObj->pstSpareBuf       = (FSRSpareBuf*)     FSR_STL_MALLOC(sizeof(FSRSpareBuf)     * WB_MAX_WRITE_REQUEST,
                                                         FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_DRAM);
        pstWBObj->pstSpareBufBase   = (FSRSpareBufBase*) FSR_STL_MALLOC(sizeof(FSRSpareBufBase) * WB_MAX_WRITE_REQUEST,
                                                         FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_DRAM);
        pstWBObj->pstSpareBufExt    = (FSRSpareBufExt*)  FSR_STL_MALLOC(sizeof(FSRSpareBufExt)  * WB_MAX_WRITE_REQUEST * (pstWBObj->nSctsPerPg / 4),
                                                         FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_DRAM);
        if (pstWBObj->pMainBuf        == NULL ||
            pstWBObj->pstSpareBuf     == NULL ||
            pstWBObj->pstSpareBufBase == NULL ||
            pstWBObj->pstSpareBufExt  == NULL)
        {
            nErr = FSR_STL_OUT_OF_MEMORY;
            break;
        }
        else if ((((UINT32)(pstWBObj->pMainBuf))        & 0x03) ||
                 (((UINT32)(pstWBObj->pstSpareBuf))     & 0x03) ||
                 (((UINT32)(pstWBObj->pstSpareBufBase)) & 0x03) ||
                 (((UINT32)(pstWBObj->pstSpareBufExt))  & 0x03))
        {
            nErr = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }

        FSR_OAM_MEMSET(pstWBObj->pMainBuf,        0xFF, WB_PG_SIZE);
        FSR_OAM_MEMSET(pstWBObj->pstSpareBuf,     0xFF, sizeof(FSRSpareBuf)     * WB_MAX_WRITE_REQUEST);
        FSR_OAM_MEMSET(pstWBObj->pstSpareBufBase, 0xFF, sizeof(FSRSpareBufBase) * WB_MAX_WRITE_REQUEST);
        FSR_OAM_MEMSET(pstWBObj->pstSpareBufExt,  0xFF, sizeof(FSRSpareBufExt)  * WB_MAX_WRITE_REQUEST * (pstWBObj->nSctsPerPg / 4));

        for (nIdx = 0; nIdx < WB_MAX_WRITE_REQUEST; nIdx++)
        {
            (pstWBObj->pstSpareBuf + nIdx)->nNumOfMetaExt    = (pstWBObj->nSctsPerPg / 4);
            (pstWBObj->pstSpareBuf + nIdx)->pstSpareBufBase  = pstWBObj->pstSpareBufBase + nIdx;
            (pstWBObj->pstSpareBuf + nIdx)->pstSTLMetaExt    = pstWBObj->pstSpareBufExt + nIdx * (pstWBObj->nSctsPerPg / 4);
        }

        nErr = FSR_STL_SUCCESS;

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}


/**
 * @brief       This function frees variables of STLWBObj, if WB exists.
 * @n           Called by FSR_STL_OpenWB() and FSR_STL_CloseWB()
 *
 * @param[in]   nVol        : Volume ID
 * @param[in]   nPart       : Partition ID offset (0~7)
 *
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_ERR_PARAM
 * @return      FSR_STL_ERR_PHYSICAL
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PRIVATE INT32
_FreeWBObj      (UINT32     nVol,
                 UINT32     nPart)
{
    STLWBObj           *pstWBObj;
    INT32               nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Get write buffer object */
    pstWBObj = gstSTLPartObj[nVol][nPart].pstWBObj;

    do
    {
        if (pstWBObj == NULL)
        {
            break;
        }

        if (pstWBObj->pIndexT != NULL)
        {
            FSR_OAM_Free(pstWBObj->pIndexT);
            pstWBObj->pIndexT = NULL;
        }

        if (pstWBObj->pCollisionT != NULL)
        {
            FSR_OAM_Free(pstWBObj->pCollisionT);
            pstWBObj->pCollisionT = NULL;
        }

        if (pstWBObj->pPgMapT != NULL)
        {
            FSR_OAM_Free(pstWBObj->pPgMapT);
            pstWBObj->pPgMapT = NULL;
        }

        if (pstWBObj->pPgBitmap != NULL)
        {
            FSR_OAM_Free(pstWBObj->pPgBitmap);
            pstWBObj->pPgBitmap = NULL;
        }

        if (pstWBObj->pMainBuf != NULL)
        {
            FSR_OAM_Free(pstWBObj->pMainBuf);
            pstWBObj->pMainBuf = NULL;
        }

        if (pstWBObj->pstSpareBuf != NULL)
        {
            FSR_OAM_Free(pstWBObj->pstSpareBuf);
            pstWBObj->pstSpareBuf = NULL;
        }

        if (pstWBObj->pstSpareBufBase != NULL)
        {
            FSR_OAM_Free(pstWBObj->pstSpareBufBase);
            pstWBObj->pstSpareBufBase = NULL;
        }

        if (pstWBObj->pstSpareBufExt != NULL)
        {
            FSR_OAM_Free(pstWBObj->pstSpareBufExt);
            pstWBObj->pstSpareBufExt = NULL;
        }

        if (pstWBObj != NULL)
        {
            FSR_OAM_Free(pstWBObj);
            gstSTLPartObj[nVol][nPart].pstWBObj = NULL;
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}



/**
 * @brief       This function allocates new block. (Round-robin)
 *
 * @param[in]   nVol        : Volume ID
 * @param[in]   nPart       : Partition ID offset (0~7)
 *
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_ERR_PHYSICAL
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PRIVATE INT32
_AllocateNewBlk (UINT32     nVol,
                 UINT32     nPart)
{
    STLWBObj           *pstWBObj;
    UINT32              nNewBlkOff;
    UINT32              nPgOff;
    UINT32              nMapOff;
    UINT32              nVbn;
    INT32               nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Get write buffer object */
    pstWBObj = gstSTLPartObj[nVol][nPart].pstWBObj;

    do
    {
        /* 1. Calculate next block offset */
        nNewBlkOff = WB_GET_NEXT_BLK(pstWBObj->nHeadBlkOff);

        /* 2. Flush tail block, if there is only one free block in write buffer,
              in order to avoid that the tail blk is lost. */
        if (WB_GET_NEXT_BLK(nNewBlkOff) == pstWBObj->nTailBlkOff)
        {
            nErr = _FlushBlk(nVol, nPart, 0);
            if (nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - _FlushBlk() (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nErr));
                break;
            }
        }

        /* 3. Erase new block */
        nVbn = nNewBlkOff + pstWBObj->nStartVbn;

        nErr = FSR_BML_Erase(nVol,                              /* nVol             */
                             &nVbn,                             /* nVun             */
                             1,                                 /* nNumOfUnits      */
                             FSR_BML_FLAG_NONE);                /* nFlag            */
        if (nErr != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - FSR_BML_Erase(nVbn=%d) (0x%x)\r\n"), 
                __FSR_FUNC__, __LINE__, nVbn, nErr));
            break;
        }

        /* 4. Erase mapping info */
        for (nPgOff = 0; nPgOff < pstWBObj->nPgsPerBlk; nPgOff++)
        {
            nMapOff = WB_GET_MAPOFF(nNewBlkOff, nPgOff);
            FSR_ASSERT(nMapOff < pstWBObj->nNumTotalPgs);

            if (WB_GET_BITMAP(nMapOff)  == WB_INVALID_BITMAP &&
                WB_GET_COLT(nMapOff)    == WB_NULL_POFFSET)
            {
                WB_GET_PGT(nMapOff) = WB_NULL_LPN;
            }
            else
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - Block is not empty.(nNewBlkOff=%d, nPgOff=%d)\r\n"), 
                    __FSR_FUNC__, __LINE__, nNewBlkOff, nPgOff));
                nErr = FSR_STL_CRITICAL_ERROR;
                FSR_ASSERT(FALSE32);
                break;
            }
        }

        if (nErr == FSR_STL_CRITICAL_ERROR)
        {
            break;
        }

        /* 5. Check whether the tail block offset can be moved or not */
        while (pstWBObj->nTailBlkOff != pstWBObj->nHeadBlkOff)
        {
            for (nPgOff = 0; nPgOff < pstWBObj->nPgsPerBlk; nPgOff++)
            {
                nMapOff = WB_GET_MAPOFF(pstWBObj->nTailBlkOff, nPgOff);
                FSR_ASSERT(nMapOff < pstWBObj->nNumTotalPgs);

                if (WB_GET_BITMAP(nMapOff)  != WB_INVALID_BITMAP ||
                    WB_GET_COLT(nMapOff)    != WB_NULL_POFFSET)
                {
                    break;
                }
            }

            if (nPgOff == pstWBObj->nPgsPerBlk)
            {
                pstWBObj->nTailBlkOff = WB_GET_NEXT_BLK(pstWBObj->nTailBlkOff);
            }
            else
            {
                break;
            }
        }

        /* 6. Write header data */
        nErr = _WriteHeaderPage(nVol, nPart, (BADDR) nNewBlkOff, 0);
        if (nErr != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - _WriteHeaderPage(nVol=%d, nPart=%d, nBlkOff=%d, nPgOff=0) (0x%x)\r\n"),
                __FSR_FUNC__, __LINE__, nVol, nPart, nNewBlkOff, nErr));
            break;
        }

        /* 7. Write delete page */
        nErr = _WriteDelPage(nVol, nPart, (BADDR) nNewBlkOff, 1);
        if (nErr != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - _WriteDelPage(nVol=%d, nPart=%d, nBlkOff=%d, nPgOff=1) (0x%x)\r\n"),
                __FSR_FUNC__, __LINE__, nVol, nPart, nNewBlkOff, nErr));
            break;
        }

        /* 8. Increase header age */
        pstWBObj->nHeaderAge++;

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}


/**
 * @brief       This function flushes a tail block as Round-robin. (Note! pMainBuf of WB)
 *
 * @param[in]   nVol        : Volume ID
 * @param[in]   nPart       : Partition ID offset (0~7)
 * @param[in]   nPgCnt      : # of pages to be flushed
 *
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_ERR_PHYSICAL
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PRIVATE INT32
_FlushBlk   (UINT32     nVol,
             UINT32     nPart,
             UINT32     nPgCnt)
{
    STLPartObj         *pstSTLPartObj;
    STLWBObj           *pstWBObj;
    PADDR               nLpn;
    UINT32              nLsn;
    UINT32              nPgOff;
    UINT32              nMapOff;
    UINT32              nZone;
    UINT32              nZoneLsn;
    UINT32              nVpn;
    UINT32              nFlushedPgCnt = 0;
    BOOL32              bDeleted;
    INT32               nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Get STL partition object */
    pstSTLPartObj = &(gstSTLPartObj[nVol][nPart]);

    /* Get write buffer object */
    pstWBObj = gstSTLPartObj[nVol][nPart].pstWBObj;

    do
    {
        /* Check and flush every page of tail block */
        for (nPgOff = 1; nPgOff < pstWBObj->nPgsPerBlk; nPgOff++)
        {
            /* 1. If there are valid data sectors, flush it */
            nMapOff = WB_GET_MAPOFF(pstWBObj->nTailBlkOff, nPgOff);
            FSR_ASSERT(nMapOff < pstWBObj->nNumTotalPgs);

            nLpn = WB_GET_PGT(nMapOff);
            if (nLpn == WB_NULL_LPN || 
                nLpn == WB_DEL_LPN ||
                WB_GET_BITMAP(nMapOff) == WB_INVALID_BITMAP)
            {
                FSR_ASSERT(!((nLpn == WB_NULL_LPN || nLpn == WB_DEL_LPN) &&
                           WB_GET_BITMAP(nMapOff) != WB_INVALID_BITMAP));
                continue;
            }

            /* Read data from WB (FIXME: use FSR_BML_CopyBack) */
            nVpn = WB_GET_VPN(pstWBObj->nTailBlkOff, nPgOff);

            nErr = FSR_BML_Read(nVol,                               /* nVol             */
                                nVpn,                               /* nVpn             */
                                1,                                  /* nNumOfPgs        */
                                pstWBObj->pMainBuf,                 /* pMBuf            */
                                pstWBObj->pstSpareBuf,              /* pSBuf            */
                                FSR_BML_FLAG_USE_SPAREBUF);         /* nFlag            */
            if (nErr != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - FSR_BML_Read(nVpn=%d) (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nVpn, nErr));
                break;
            }

            /* Check validity of spare data */
            nErr = _CheckSpareValidity(pstWBObj->pMainBuf,
                                       pstWBObj->pstSpareBuf,
                                       WB_PG_SIZE,
                                       WB_SIG_DATA);
            if (nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - _CheckSpareValidity(nVpn=%d) (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nVpn, nErr));
                break;
            }

            FSR_ASSERT(nLpn == pstWBObj->pstSpareBufBase->nSTLMetaBase1);

            /* Write data to zone */
            nLsn        = nLpn << pstWBObj->nSctsPerPgSft;
            nZoneLsn    = FSR_STL_GetZoneLsn(pstSTLPartObj, nLsn, &nZone);

            nErr = FSR_STL_WriteZone(pstSTLPartObj->nClstID,            /* nClstID      */
                                     pstSTLPartObj->nZoneID + nZone,    /* nZoneID      */
                                     nZoneLsn,                          /* nLsn         */
                                     pstWBObj->nSctsPerPg,              /* nNumOfScts   */
                                     pstWBObj->pMainBuf,                /* pBuf         */
                                     FSR_STL_FLAG_DEFAULT);             /* nFlag        */
            if (nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - FSR_STL_WriteZone(nPgOff=%d) (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nPgOff, nErr));
                break;
            }

            /* Delete a map item */
            nErr = _DeleteMapItem(pstWBObj,
                                  nLpn,
                                  &bDeleted);
            if (nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - _DeleteMapItem(nLpn=%d) (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nLpn, nErr));
                break;
            }

            /* Add page count (if nPgCnt == 0, flush a whole block) */
            nFlushedPgCnt++;
            if (nFlushedPgCnt == nPgCnt)
            {
                break;
            }
        }

        /* Move tail pointer */
        if (nPgOff  == pstWBObj->nPgsPerBlk && 
            nErr    == FSR_STL_SUCCESS      &&
            pstWBObj->nTailBlkOff != pstWBObj->nHeadBlkOff)
        {
            pstWBObj->nTailBlkOff = WB_GET_NEXT_BLK(pstWBObj->nTailBlkOff);
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}


/**
 * @brief       This function inserts new item to mapping (hash & bitmap).
 *
 * @param[in]   pstWBObj        : Write buffer object
 * @param[in]   nLpn            : LPN to be buffered
 * @param[in]   nBlkOff         : VBN offset of the buffer page
 * @param[in]   nPgOff          : VPN offset of the buffer page
 *
 * @return      FSR_STL_SUCCESS
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PRIVATE INT32
_InsertMapItem  (STLWBObj  *pstWBObj,
                 PADDR      nLpn,
                 BADDR      nBlkOff,
                 POFFSET    nPgOff)
{
    UINT32              nSearchCnt;
    UINT32              nIndexOff;
    UINT32              nNewOff;
    UINT32              nMapOff;
    UINT32              nPrevMapOff;
    INT32               nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    do
    {
        /* Check parameters */
        if (nLpn    != WB_DEL_LPN &&
            nLpn    >= pstWBObj->nNumTotalPgsM)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - Invalid parameter(nLpn=%d)\r\n"),
                __FSR_FUNC__, __LINE__, nLpn));
            nErr = FSR_STL_INVALID_PARAM;
            FSR_ASSERT(FALSE32);
            break;
        }

        if (nBlkOff >= pstWBObj->nNumTotalBlks ||
            nPgOff  >= pstWBObj->nPgsPerBlk)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - Invalid parameter(nBlkOff=%d, nPgOff=%d)\r\n"),
                __FSR_FUNC__, __LINE__, nBlkOff, nPgOff));
            nErr = FSR_STL_INVALID_PARAM;
            FSR_ASSERT(FALSE32);
            break;
        }

        /* Calculate offset */
        nNewOff = WB_GET_MAPOFF(nBlkOff, nPgOff);
        FSR_ASSERT(nNewOff < pstWBObj->nNumTotalPgs);

        /* Insert an item (LPN) into the mapping table */
        WB_SET_PGT(nNewOff, nLpn);

        /* If it is a delete page, return now. */
        if (nLpn == WB_DEL_LPN)
        {
            FSR_ASSERT(WB_GET_BITMAP(nNewOff) == WB_INVALID_BITMAP);
            break;
        }

        /* Set the bitmap */
        WB_SET_BITMAP(nNewOff);

        /* Delete LPN from the delete list */
        for (nSearchCnt = 0; nSearchCnt < pstWBObj->nDeletedListOff; nSearchCnt++)
        {
            if (WB_GET_DELLIST(nSearchCnt) == nLpn)
            {
                while (nSearchCnt < pstWBObj->nDeletedListOff)
                {
                    pstWBObj->pDeletedList[nSearchCnt] = pstWBObj->pDeletedList[nSearchCnt + 1];
                    nSearchCnt++;
                }
                pstWBObj->nDeletedListOff--;
                break;
            }
        }

        /* First inserted */
        nIndexOff = WB_GET_HASH_INDEX(nLpn);
        FSR_ASSERT(nIndexOff < (pstWBObj->nNumTotalPgsM >> WB_HASH_SFT));
        if (WB_GET_IDXT(nIndexOff) == WB_NULL_POFFSET)
        {
            WB_SET_IDXT(nIndexOff, nNewOff);
            WB_SET_COLT(nNewOff, WB_NULL_POFFSET);
            break;
        }

        /* Search mapping tables */
        nMapOff = WB_GET_IDXT(nIndexOff);
        nPrevMapOff = nMapOff;
        for (nSearchCnt = 0; nSearchCnt < WB_MAX_NUM_OF_COLLISION; nSearchCnt++)
        {
            FSR_ASSERT(nMapOff < pstWBObj->nNumTotalPgs);
            FSR_ASSERT(WB_GET_PGT(nMapOff) != WB_NULL_LPN);

            if (WB_GET_PGT(nMapOff)    == nLpn &&
                WB_GET_BITMAP(nMapOff) != WB_INVALID_BITMAP)
            {
                FSR_ASSERT(nMapOff != nNewOff);

                /* Already inserted */
                if (nSearchCnt == 0)
                {
                    WB_SET_IDXT(nIndexOff, nNewOff);
                }
                else
                {
                    WB_SET_COLT(nPrevMapOff, nNewOff);
                }

                WB_SET_COLT(nNewOff, WB_GET_COLT(nMapOff));
                WB_SET_COLT(nMapOff, WB_NULL_POFFSET);
                WB_RESET_BITMAP(nMapOff);

                /* From now, WB is dirty */
                pstWBObj->bDirty = TRUE32;

                break;
            }
            else
            {
                if (WB_GET_COLT(nMapOff) == NULL_POFFSET)
                {
                    /* New insert */
                    WB_SET_COLT(nMapOff, nNewOff);
                    WB_SET_COLT(nNewOff, WB_NULL_POFFSET);

                    /* From now, WB is dirty */
                    pstWBObj->bDirty = TRUE32;

                    break;
                }
                else
                {
                    nPrevMapOff = nMapOff;
                    nMapOff = WB_GET_COLT(nMapOff);
                }
            }
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}


/**
 * @brief       This function finds an item with LPN.
 *
 * @param[in]   pstWBObj        : Write buffer object
 * @param[in]   nLpn            : LPN to be buffered
 * @param[out]  pBlkOff         : VBN offset of the buffer page
 * @param[out]  pPgOff          : VPN offset of the buffer page
 *
 * @return      FSR_STL_SUCCESS
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PRIVATE INT32
_FindMapItem    (STLWBObj  *pstWBObj,
                 PADDR      nLpn,
                 BADDR     *pBlkOff,
                 POFFSET   *pPgOff)
{
    UINT32              nSearchCnt;
    UINT32              nIndexOff;
    UINT32              nMapOff;
    INT32               nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    do
    {
        /* Check parameters */
        if (nLpn    >= pstWBObj->nNumTotalPgsM)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - Invalid parameter(nLpn=%d)\r\n"),
                __FSR_FUNC__, __LINE__, nLpn));
            nErr = FSR_STL_INVALID_PARAM;
            FSR_ASSERT(FALSE32);
            break;
        }

        /* Initialize output parameters */
        *pBlkOff    = WB_NULL_BOFFSET;
        *pPgOff     = WB_NULL_POFFSET;

        /* Calculate the index table offset */
        nIndexOff = WB_GET_HASH_INDEX(nLpn);
        FSR_ASSERT(nIndexOff < (pstWBObj->nNumTotalPgsM >> WB_HASH_SFT));
        if (WB_GET_IDXT(nIndexOff) == WB_NULL_POFFSET)
        {
            /* Have not been inserted yet */
            break;
        }

        /* Search mapping tables */
        nMapOff = WB_GET_IDXT(nIndexOff);
        for (nSearchCnt = 0; nSearchCnt < WB_MAX_NUM_OF_COLLISION; nSearchCnt++)
        {
            FSR_ASSERT(nMapOff < pstWBObj->nNumTotalPgs);
            FSR_ASSERT(WB_GET_PGT(nMapOff) != WB_NULL_LPN);

            if (WB_GET_PGT(nMapOff)    == nLpn &&
                WB_GET_BITMAP(nMapOff) != WB_INVALID_BITMAP)
            {
                /* Already inserted */
                *pBlkOff    = (BADDR) (nMapOff >> pstWBObj->nPgsPerBlkSft);
                *pPgOff     = (POFFSET) (nMapOff & (pstWBObj->nPgsPerBlk - 1));
                break;
            }
            else
            {
                if (WB_GET_COLT(nMapOff) == WB_NULL_POFFSET)
                {
                    break;
                }
                else
                {
                    nMapOff = WB_GET_COLT(nMapOff);
                }
            }
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}


/**
 * @brief       This function deletes an item.
 *
 * @param[in]   pstWBObj        : Write buffer object
 * @param[in]   nLpn            : Lpn
 *
 * @return      FSR_STL_SUCCESS
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PRIVATE INT32
_DeleteMapItem  (STLWBObj  *pstWBObj,
                 PADDR      nLpn,
                 BOOL32    *pbDeleted)
{
    UINT32              nSearchCnt;
    UINT32              nIndexOff;
    UINT32              nPrevMapOff;
    UINT32              nMapOff;
    INT32               nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    do
    {
        /* Check parameters */
        if (nLpn    >= pstWBObj->nNumTotalPgsM)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - Invalid parameter(nLpn=%d)\r\n"),
                __FSR_FUNC__, __LINE__, nLpn));
            nErr = FSR_STL_INVALID_PARAM;
            FSR_ASSERT(FALSE32);
            break;
        }

        /* Initialize output parameters */
        *pbDeleted = FALSE32;

        /* Calculate the index table offset */
        nIndexOff = WB_GET_HASH_INDEX(nLpn);
        FSR_ASSERT(nIndexOff < (pstWBObj->nNumTotalPgsM >> WB_HASH_SFT));
        if (WB_GET_IDXT(nIndexOff) == WB_NULL_POFFSET)
        {
            /* Have not been inserted yet */
            break;
        }

        /* Search mapping tables */
        nMapOff = WB_GET_IDXT(nIndexOff);
        nPrevMapOff = nMapOff;
        for (nSearchCnt = 0; nSearchCnt < WB_MAX_NUM_OF_COLLISION; nSearchCnt++)
        {
            FSR_ASSERT(nMapOff < pstWBObj->nNumTotalPgs);
            FSR_ASSERT(WB_GET_PGT(nMapOff) != WB_NULL_LPN);

            if (WB_GET_PGT(nMapOff) == nLpn)
            {
                /* Delete sectors */
                if (nSearchCnt == 0)
                {
                    WB_SET_IDXT(nIndexOff, WB_GET_COLT(nMapOff));
                }
                else
                {
                    WB_SET_COLT(nPrevMapOff, WB_GET_COLT(nMapOff));
                }

                WB_SET_COLT(nMapOff, WB_NULL_POFFSET);
                WB_RESET_BITMAP(nMapOff);

                /* Insert LPN into the delete list */
                if (pstWBObj->nDeletedListOff < WB_MAX_DELLIST)
                {
                    WB_SET_DELLIST(nLpn);
                }

                /* Set output parameters */
                *pbDeleted = TRUE32;

                /* From now, WB is dirty */
                pstWBObj->bDirty = TRUE32;

                break;
            }
            else
            {
                if (WB_GET_COLT(nMapOff) == WB_NULL_POFFSET)
                {
                    /* Have not been inserted yet */
                    break;
                }
                else
                {
                    nPrevMapOff = nMapOff;
                    nMapOff = WB_GET_COLT(nMapOff);
                }
            }
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}


/**
 * @brief       This function sets a buffer with valid bitmap.
 *
 * @param[in]   pstWBObj        : Write buffer object
 * @param[out]  pBuf            : Buffer pointer
 *
 * @return      FSR_STL_SUCCESS
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PRIVATE INT32
_GetWBBitmap    (STLWBObj  *pstWBObj,
                 UINT8     *pBuf)
{
    UINT32              nBitmapSize;
    INT32               nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    do
    {
        nBitmapSize = pstWBObj->nNumTotalPgs >> 3;

        FSR_ASSERT(nBitmapSize <= WB_PG_SIZE);

        /* Copy bitmap */
        FSR_OAM_MEMSET(pBuf, 0xFF, WB_PG_SIZE);
        FSR_OAM_MEMCPY(pBuf, pstWBObj->pPgBitmap, nBitmapSize);

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}


/**
 * @brief       This function sets a buffer with virtual to logical table.
 *
 * @param[in]   pstWBObj        : Write buffer object
 * @param[in]   nBlkOff         : VBN offset
 * @param[out]  pBuf            : Buffer pointer
 *
 * @return      FSR_STL_SUCCESS
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PRIVATE INT32
_GetWBHashTable (STLWBObj  *pstWBObj,
                 BADDR      nBlkOff,
                 UINT8     *pBuf)
{
    UINT32              nBlkMapSize;
    UINT32              nMaxBlks;
    UINT32              nBlkIdx;
    UINT32              nIdx;
    INT32               nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    do
    {
        nBlkMapSize = pstWBObj->nPgsPerBlk * sizeof(PADDR);

        nMaxBlks = WB_PG_SIZE / nBlkMapSize;

        FSR_OAM_MEMSET(pBuf, 0xFF, WB_PG_SIZE);

        /* Copy map table */
        for (nIdx = 0; nIdx < nMaxBlks; nIdx++)
        {
            nBlkIdx = (pstWBObj->nNumTotalBlks + nBlkOff - nMaxBlks + nIdx) 
                      % pstWBObj->nNumTotalBlks;

            FSR_OAM_MEMCPY(pBuf + nIdx * nBlkMapSize,
                           (UINT8*) &(WB_GET_PGT(nBlkIdx << pstWBObj->nPgsPerBlkSft)),
                           nBlkMapSize);
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}


/**
 * @brief       This function checks validity of the spare data.
 *
 * @param[in]   pMainBuf        : Main area buffer
 * @param[in]   pSpareBuf       : Spare area buffer
 * @param[in]   nPageType       : Page type
 *
 * @return      FSR_STL_SUCCESS
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PRIVATE INT32
_CheckSpareValidity (UINT8         *pMainBuf,
                     FSRSpareBuf   *pSpareBuf,
                     UINT32         nPageSize,
                     UINT32         nPageType)
{
    UINT16              nTailBlkOff;
    UINT16              nInvTailBlkOff;
    UINT32              nZBC;
    FSRSpareBufBase    *pstSpareBufBase;
    FSRSpareBufExt     *pstSpareBufExt;
    INT32               nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    do
    {
        /* 0. Check pointer */
        FSR_ASSERT(pMainBuf != NULL);
        FSR_ASSERT(pSpareBuf != NULL);
        FSR_ASSERT(pSpareBuf->pstSpareBufBase != NULL);
        FSR_ASSERT(pSpareBuf->pstSTLMetaExt != NULL);

        pstSpareBufBase = pSpareBuf->pstSpareBufBase;
        pstSpareBufExt  = pSpareBuf->pstSTLMetaExt;

        /* 1. Check SData0 */
        if (pstSpareBufBase->nSTLMetaBase0 != nPageType)
        {
            nErr = FSR_STL_INVALID_PAGE;
            break;
        }

        /* 2. Check SData1/2, and Ext SData0/1 */
        switch (nPageType)
        {
        case WB_SIG_HEADER:
            if (pstSpareBufBase->nSTLMetaBase1 != ~(pstSpareBufBase->nSTLMetaBase2))
            {
                nErr = FSR_STL_INVALID_PAGE;
                break;
            }

            nTailBlkOff = (UINT16) (pstSpareBufExt->nSTLMetaExt0 & 0xFFFF);
            nInvTailBlkOff = nTailBlkOff ^ 0xFFFF;

            if (pstSpareBufExt->nSTLMetaExt0 != (((UINT32) nInvTailBlkOff << 16) | (UINT32) nTailBlkOff))
            {
                nErr = FSR_STL_INVALID_PAGE;
                break;
            }

            if (pstSpareBufExt->nSTLMetaExt1 != 0xFFFFFFFF)
            {
                nErr = FSR_STL_INVALID_PAGE;
                break;
            }
            break;

        case WB_SIG_DATA:
            if (pstSpareBufBase->nSTLMetaBase1 != ~(pstSpareBufBase->nSTLMetaBase2))
            {
                nErr = FSR_STL_INVALID_PAGE;
                break;
            }

            if (pstSpareBufExt->nSTLMetaExt0 != 0xFFFFFFFF ||
                pstSpareBufExt->nSTLMetaExt1 != 0xFFFFFFFF)
            {
                nErr = FSR_STL_INVALID_PAGE;
                break;
            }
            break;

        case WB_SIG_DELETE:
            if (pstSpareBufBase->nSTLMetaBase1 != 0xFFFFFFFF ||
                pstSpareBufBase->nSTLMetaBase2 != 0xFFFFFFFF ||
                pstSpareBufExt->nSTLMetaExt0   != 0xFFFFFFFF ||
                pstSpareBufExt->nSTLMetaExt1   != 0xFFFFFFFF)
            {
                nErr = FSR_STL_INVALID_PAGE;
                break;
            }
            break;

        default:
            nErr = FSR_STL_INVALID_PAGE;
            break;
        }

        if (nErr != FSR_STL_SUCCESS)
        {
            break;
        }

        /* 3. Check Ext SData2/3 - Zero bit count */
        if (pstSpareBufExt->nSTLMetaExt2 != ~(pstSpareBufExt->nSTLMetaExt3))
        {
            nErr = FSR_STL_INVALID_PAGE;
            break;
        }
        else
        {
            nZBC = FSR_STL_GetZBC(pMainBuf, nPageSize);

            if (pstSpareBufExt->nSTLMetaExt2 != nZBC)
            {
                nErr = FSR_STL_INVALID_PAGE;
                break;
            }
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}


/**
 * @brief       This function checks whether the write buffer can handle this request or not.
 *
 * @param[in]   pstWBObj        : Write buffer object
 * @param[in]   nLsn            : lsn
 * @param[in]   nNumOfScts      : # of sectors
 *
 * @return      FSR_STL_SUCCESS
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PRIVATE INT32
_CheckWritable      (STLWBObj  *pstWBObj,
                     UINT32     nLsn,
                     UINT32     nNumOfScts)
{
    UINT32              nCurLpn;
    UINT32              nEndLpn;
    UINT32              nNumOfPgs;
    UINT32              nCurBlkOff;
    UINT32              nCurPgOff;
    BADDR               nBlkOff;
    POFFSET             nPgOff;
    UINT32              nPgs;
    INT32               nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    do
    {
        /* Initialize local variables */
        nCurLpn     = (nLsn) >> pstWBObj->nSctsPerPgSft;
        nEndLpn     = (nLsn + nNumOfScts - 1) >> pstWBObj->nSctsPerPgSft;

        nNumOfPgs   = nEndLpn - nCurLpn + 1;

        nCurBlkOff  = pstWBObj->nHeadBlkOff;
        nCurPgOff   = pstWBObj->nHeadPgOff;

        /* 0. FIXME: Unaliged write request under 1 page should be processed, if it is in WB. 
                     Zone does not know WB. */
        if (nLsn != nCurLpn << pstWBObj->nSctsPerPgSft && 
            nNumOfScts < pstWBObj->nSctsPerPg)
        {
            /* Search mapping info for written data */
            nErr = _FindMapItem(pstWBObj,
                                nCurLpn,
                                &nBlkOff,
                                &nPgOff);
            if (nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - _FindMapItem() (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nErr));
                break;
            }

            /* Check whether the page exists in WB */
            if (nBlkOff != WB_NULL_BOFFSET && nPgOff != WB_NULL_POFFSET)
            {
                /* Note! All pages should be in WB */
                nErr = FSR_STL_SUCCESS;
                break;
            }
        }

        /* 1. Check erase count of WB blocks */
        if (pstWBObj->nHeaderAge > ((UINT32) pstWBObj->nNumTotalBlks * WB_SLC_ENDURANCE))
        {
            nErr = FSR_STL_WB_IS_WORN_OUT;
            break;
        }

        /* 2. Check free space */
        while (nNumOfPgs > 0)
        {
            if ((nCurPgOff + 1) == pstWBObj->nPgsPerBlk)
            {
                nCurBlkOff = WB_GET_NEXT_BLK(nCurBlkOff);
                nCurPgOff  = 1;

                if (WB_GET_NEXT_BLK(nCurBlkOff) == pstWBObj->nTailBlkOff)
                {
                    /* There is no space enough to write it */
                    nErr = FSR_STL_WB_IS_FULL;
                    break;
                }
            }
            else
            {
                nPgs = pstWBObj->nPgsPerBlk - (nCurPgOff + 1);
                if (nNumOfPgs > nPgs)
                {
                    nNumOfPgs -= nPgs;
                    nCurPgOff += nPgs;
                }
                else
                {
                    break;
                }
            }
        }

        if (nErr == FSR_STL_SUCCESS)
        {
            break;
        }

        /* 3. Check it is already in WB (If yes, flush forcibly and write) */
        while (nCurLpn <= nEndLpn)
        {
            /* Search mapping info for written data */
            nErr = _FindMapItem(pstWBObj,
                                nCurLpn,
                                &nBlkOff,
                                &nPgOff);
            if (nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - _FindMapItem() (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nErr));
                break;
            }

            /* Check whether the page exists in WB */
            if (nBlkOff != WB_NULL_BOFFSET && nPgOff != WB_NULL_POFFSET)
            {
                /* Note! All pages should be in WB */
                nCurLpn++;
                continue;
            }
            else
            {
                /* Note! If any of pages is not in WB, do not write it */
                nErr = FSR_STL_WB_IS_FULL;
                break;
            }
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}


/**
 * @brief       This function write a header page. (Note! pMainBuf of WB)
 *
 * @param[in]   nVol            : Volume ID
 * @param[in]   nPart           : Partition ID offset (0~7)
 * @param[in]   nBlkOff         : Block offset
 * @param[in]   nPgOff          : Page offset
 *
 * @return      FSR_STL_SUCCESS
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PRIVATE INT32
_WriteHeaderPage    (UINT32     nVol,
                     UINT32     nPart,
                     BADDR      nBlkOff,
                     POFFSET    nPgOff)
{
    STLWBObj           *pstWBObj;
    UINT32              nVpn;
    UINT32              nZBC;
    INT32               nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Get write buffer object */
    pstWBObj = gstSTLPartObj[nVol][nPart].pstWBObj;

    do
    {
        /* 1. Make header data */
        nErr = _GetWBHashTable(pstWBObj, nBlkOff, pstWBObj->pMainBuf);
        if (nErr != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - _GetWBHashTable() (0x%x)\r\n"),
                __FSR_FUNC__, __LINE__, nErr));
            break;
        }

        nVpn = WB_GET_VPN(nBlkOff, nPgOff);

        nZBC = FSR_STL_GetZBC(pstWBObj->pMainBuf, WB_PG_SIZE);

        pstWBObj->pstSpareBufBase->nSTLMetaBase0 = WB_SIG_HEADER;
        pstWBObj->pstSpareBufBase->nSTLMetaBase1 = pstWBObj->nHeaderAge;
        pstWBObj->pstSpareBufBase->nSTLMetaBase2 = ~(pstWBObj->nHeaderAge);
        pstWBObj->pstSpareBufExt->nSTLMetaExt0   = ((UINT32) (pstWBObj->nTailBlkOff ^ 0xFFFF) << 16) |
                                                   ((UINT32) (pstWBObj->nTailBlkOff));
        pstWBObj->pstSpareBufExt->nSTLMetaExt1   = 0xFFFFFFFF;
        pstWBObj->pstSpareBufExt->nSTLMetaExt2   = nZBC;
        pstWBObj->pstSpareBufExt->nSTLMetaExt3   = ~(nZBC);

        /* Note! For DDP, FlushOp should be called for interleaving */
        if (pstWBObj->nNumWays > 1)
        {
            nErr = FSR_BML_FlushOp(nVol, FSR_BML_FLAG_NORMAL_MODE);
            if (nErr != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - FSR_BML_FlushOp() (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nErr));
                break;
            }
        }

        /* 2. Write header page */
        nErr = FSR_BML_Write(nVol,                              /* nVol             */
                             nVpn,                              /* nVpn             */
                             1,                                 /* nNumOfPgs        */
                             pstWBObj->pMainBuf,                /* pMBuf            */
                             pstWBObj->pstSpareBuf,                 /* pSBuf            */
                             FSR_BML_FLAG_USE_SPAREBUF);        /* nFlag            */
        if (nErr != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - FSR_BML_Write(nVpn=%d) (0x%x)\r\n"),
                __FSR_FUNC__, __LINE__, nVpn, nErr));
            break;
        }

        nErr = FSR_STL_SUCCESS;

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}


/**
 * @brief       This function write a delete page. (Note! pMainBuf of WB)
 *
 * @param[in]   nVol            : Volume ID
 * @param[in]   nPart           : Partition ID offset (0~7)
 * @param[in]   nBlkOff         : Block offset
 * @param[in]   nPgOff          : Page offset
 *
 * @return      FSR_STL_SUCCESS
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PRIVATE INT32
_WriteDelPage       (UINT32     nVol,
                     UINT32     nPart,
                     BADDR      nBlkOff,
                     POFFSET    nPgOff)
{
    STLWBObj           *pstWBObj;
    UINT32              nVpn;
    UINT32              nZBC;
    INT32               nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Get write buffer object */
    pstWBObj = gstSTLPartObj[nVol][nPart].pstWBObj;

    do
    {
        /* 1. Make delete meta data */
        nErr = _GetWBBitmap(pstWBObj, pstWBObj->pMainBuf);
        if (nErr != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - _GetWBBitmap() (0x%x)\r\n"),
                __FSR_FUNC__, __LINE__, nErr));
            break;
        }

        nVpn = WB_GET_VPN(nBlkOff, nPgOff);

        nZBC = FSR_STL_GetZBC(pstWBObj->pMainBuf, WB_PG_SIZE);

        pstWBObj->pstSpareBufBase->nSTLMetaBase0 = WB_SIG_DELETE;
        pstWBObj->pstSpareBufBase->nSTLMetaBase1 = 0xFFFFFFFF;
        pstWBObj->pstSpareBufBase->nSTLMetaBase2 = 0xFFFFFFFF;
        pstWBObj->pstSpareBufExt->nSTLMetaExt0   = 0xFFFFFFFF;
        pstWBObj->pstSpareBufExt->nSTLMetaExt1   = 0xFFFFFFFF;
        pstWBObj->pstSpareBufExt->nSTLMetaExt2   = nZBC;
        pstWBObj->pstSpareBufExt->nSTLMetaExt3   = ~(nZBC);

        /* Note! For DDP, FlushOp should be called for interleaving */
        if (pstWBObj->nNumWays > 1)
        {
            nErr = FSR_BML_FlushOp(nVol, FSR_BML_FLAG_NORMAL_MODE);
            if (nErr != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - FSR_BML_FlushOp() (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nErr));
                break;
            }
        }

        /* 2. Write delete meta */
        nErr = FSR_BML_Write(nVol,                              /* nVol             */
                             nVpn,                              /* nVpn             */
                             1,                                 /* nNumOfPgs        */
                             pstWBObj->pMainBuf,                /* pMBuf            */
                             pstWBObj->pstSpareBuf,                 /* pSBuf            */
                             FSR_BML_FLAG_USE_SPAREBUF);        /* nFlag            */
        if (nErr != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - FSR_BML_Write(nVpn=%d) (0x%x)\r\n"),
                __FSR_FUNC__, __LINE__, nVpn, nErr));
            break;
        }

        /* 3. Insert a delete page into the mapping info */
        nErr = _InsertMapItem(pstWBObj,
                              WB_DEL_LPN,
                              nBlkOff,
                              nPgOff);
        if (nErr != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - _InsertMapItem() (0x%x)\r\n"),
                __FSR_FUNC__, __LINE__, nErr));
        }

        /* 4. Initialize deleted page list */
        WB_RESET_DELLIST();

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}


/**
 * @brief       This function writes multiple data pages. (Page-aligned)
 *
 * @param[in]   nVol            : Volume ID
 * @param[in]   nPart           : Partition ID offset (0~7)
 * @param[in]   nDstBlkOff         : Block offset
 * @param[in]   nDstPgOff          : Page offset
 * @param[in]   nLpn            : LPN of data
 * @param[in]   nNumOfPgs       : # of pages to write
 * @param[in]   pWrittenPgs     : # of pages to be written
 * @param[in]   pBuf            : Main buffer
 *
 * @return      FSR_STL_SUCCESS
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PRIVATE INT32
_WriteAlignedDataPages  (UINT32     nVol,
                         UINT32     nPart,
                         BADDR      nDstBlkOff,
                         POFFSET    nDstPgOff,
                         PADDR      nLpn,
                         UINT32     nNumOfPgs,
                         UINT32    *pActualWrPgs,
                         UINT8     *pBuf)
{
    STLWBObj           *pstWBObj;
    FSRSpareBuf        *pstSpareBuf;
    UINT32              nVpn;
    UINT32              nZBC;
    UINT32              nActualWrPgs;
    UINT32              nIdx;
    INT32               nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Get write buffer object */
    pstWBObj = gstSTLPartObj[nVol][nPart].pstWBObj;

    do
    {
        /* 1. Decide an actual number of page to write */
        nActualWrPgs = pstWBObj->nPgsPerBlk - (UINT32) nDstPgOff;

        if (nActualWrPgs > nNumOfPgs)
        {
            nActualWrPgs = nNumOfPgs;
        }

        if (nActualWrPgs > WB_MAX_WRITE_REQUEST)
        {
            nActualWrPgs = WB_MAX_WRITE_REQUEST;
        }

        /* 2. Make params */
        for (nIdx = 0; nIdx < nActualWrPgs; nIdx++)
        {
            nZBC = FSR_STL_GetZBC(pBuf + nIdx * WB_PG_SIZE, WB_PG_SIZE);

            pstSpareBuf = pstWBObj->pstSpareBuf + nIdx;

            pstSpareBuf->pstSpareBufBase->nSTLMetaBase0 = WB_SIG_DATA;
            pstSpareBuf->pstSpareBufBase->nSTLMetaBase1 = nLpn + nIdx;
            pstSpareBuf->pstSpareBufBase->nSTLMetaBase2 = ~(nLpn + nIdx);
            pstSpareBuf->pstSTLMetaExt->nSTLMetaExt0   = 0xFFFFFFFF;
            pstSpareBuf->pstSTLMetaExt->nSTLMetaExt1   = 0xFFFFFFFF;
            pstSpareBuf->pstSTLMetaExt->nSTLMetaExt2   = nZBC;
            pstSpareBuf->pstSTLMetaExt->nSTLMetaExt3   = ~(nZBC);
        }

        nVpn = WB_GET_VPN(nDstBlkOff, nDstPgOff);

        /* Note! For DDP, FlushOp should be called for interleaving */
        if (pstWBObj->nNumWays > 1)
        {
            nErr = FSR_BML_FlushOp(nVol, FSR_BML_FLAG_NORMAL_MODE);
            if (nErr != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - FSR_BML_FlushOp() (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nErr));
                break;
            }
        }

        /* 3. Write data */
        nErr = FSR_BML_Write(nVol,                              /* nVol             */
                             nVpn,                              /* nVpn             */
                             nActualWrPgs,                      /* nNumOfPgs        */
                             pBuf,                              /* pMBuf            */
                             pstWBObj->pstSpareBuf,             /* pSBuf            */
                             FSR_BML_FLAG_USE_SPAREBUF);        /* nFlag            */
        if (nErr != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - FSR_BML_Write(nVpn=%d) (0x%x)\r\n"),
                __FSR_FUNC__, __LINE__, nVpn, nErr));
            break;
        }

        /* 4. Update mapping information */
        for (nIdx = 0; nIdx < nActualWrPgs; nIdx++)
        {
            nErr = _InsertMapItem(pstWBObj,
                                  nLpn + nIdx,
                                  nDstBlkOff,
                                  nDstPgOff + (POFFSET) nIdx);
            if (nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - _InsertMapItem(nLpn=%d) (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nLpn, nErr));
                break;
            }
        }

        /* 5. Set return value */
        if (pActualWrPgs != NULL) 
        {
            *pActualWrPgs = nActualWrPgs;
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}


/**
 * @brief       This function writes (copybacks) a data page. (Note! pMainBuf of WB)
 *
 * @param[in]   nVol            : Volume ID
 * @param[in]   nPart           : Partition ID offset (0~7)
 * @param[in]   nDstBlkOff      : Block offset of destination page
 * @param[in]   nDstPgOff       : Page offset of destination page
 * @param[in]   nLpn            : LPN of data
 * @param[in]   nLsnOff         : Start LSN offset of data
 * @param[in]   nNumOfScts      : Number of sectors of data
 * @param[in]   pBuf            : Data buffer

 *
 * @return      FSR_STL_SUCCESS
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PRIVATE INT32
_WriteUnalignedDataPage (UINT32         nVol,
                         UINT32         nPart,
                         BADDR          nDstBlkOff,
                         POFFSET        nDstPgOff,
                         PADDR          nLpn,
                         UINT32         nLsnOff,
                         UINT32         nNumOfScts,
                         UINT8         *pBuf)
{
    STLPartObj         *pstSTLPartObj;
    STLWBObj           *pstWBObj;
    BADDR               nSrcBlkOff;
    POFFSET             nSrcPgOff;
    UINT32              nVpn;
    UINT32              nZone;
    UINT32              nZoneLsn;
    UINT32              nTempLsn;
    UINT32              nSTLMetaExt[2];
    UINT32              nSrcWay;
    UINT32              nDstWay;
    BOOL32              bUseCpBk;
    BMLCpBkArg         *pstBMLCpBk[FSR_MAX_WAYS];
    BMLCpBkArg          stBMLCpBk;
    BMLRndInArg         staBMLRndIn[2];
    INT32               nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Get STL partition object */
    pstSTLPartObj = &(gstSTLPartObj[nVol][nPart]);

    /* Get write buffer object */
    pstWBObj = gstSTLPartObj[nVol][nPart].pstWBObj;

    do
    {
        /* 1. Search mapping info */
        nErr = _FindMapItem(pstWBObj,
                            nLpn,
                            &nSrcBlkOff,
                            &nSrcPgOff);
        if (nErr != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] (FSR_STL_WriteWB) _FindMapItem (nErr=0x%x)\r\n"),
                __FSR_FUNC__, __LINE__, nErr));
            break;
        }

        /* Initialize read buffer */
        FSR_OAM_MEMSET(pstWBObj->pMainBuf, 0xFF, WB_PG_SIZE);

        /* 2. Read written data is in WB or Zone */
        if (nSrcBlkOff != WB_NULL_BOFFSET && nSrcPgOff != WB_NULL_POFFSET)
        {
            /* Read data from WB */
            nVpn = WB_GET_VPN(nSrcBlkOff, nSrcPgOff);

            nErr = FSR_BML_Read(nVol,                               /* nVol             */
                                nVpn,                               /* nVpn             */
                                1,                                  /* nNumOfPgs        */
                                pstWBObj->pMainBuf,                 /* pMBuf            */
                                pstWBObj->pstSpareBuf,              /* pSBuf            */
                                FSR_BML_FLAG_USE_SPAREBUF);         /* nFlag            */
            if (nErr != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR]%s() L(%d) - FSR_BML_Read(nVpn=%d) (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nVpn, nErr));
                break;
            }

            /* Check validity of spare data */
            nErr = _CheckSpareValidity(pstWBObj->pMainBuf,
                                       pstWBObj->pstSpareBuf,
                                       WB_PG_SIZE,
                                       WB_SIG_DATA);
            if (nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - _CheckSpareValidity(nVpn=%d) (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nVpn, nErr));
                break;
            }

            FSR_ASSERT(nLpn == pstWBObj->pstSpareBufBase->nSTLMetaBase1);
        }
        else    /* (nSrcBlkOff != WB_NULL_BOFFSET && nSrcPgOff != WB_NULL_POFFSET) */
        {
            /* Read data from zone */
            nTempLsn    = nLpn << pstWBObj->nSctsPerPgSft;
            nZoneLsn    = FSR_STL_GetZoneLsn(pstSTLPartObj, nTempLsn, &nZone);

            nErr = FSR_STL_ReadZone(pstSTLPartObj->nClstID,         /* nClstID      */
                                    pstSTLPartObj->nZoneID + nZone, /* nZoneID      */
                                    nZoneLsn,                       /* nLsn         */
                                    pstWBObj->nSctsPerPg,           /* nNumOfScts   */
                                    pstWBObj->pMainBuf,             /* pBuf         */
                                    FSR_STL_FLAG_DEFAULT);          /* nFlag        */
            if (nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - FSR_STL_ReadZone(nZoneLsn=%d) (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nZoneLsn, nErr));
                break;
            }
        }

        /* Copy buffer*/
        FSR_OAM_MEMCPY(pstWBObj->pMainBuf + nLsnOff * FSR_SECTOR_SIZE,
                       pBuf, 
                       nNumOfScts * FSR_SECTOR_SIZE);

        /* 3. Copyback data in WB, if possible */
        if (nSrcBlkOff != WB_NULL_BOFFSET && nSrcPgOff != WB_NULL_POFFSET)
        {
            nSrcWay = WB_GET_VPN(nSrcBlkOff, nSrcPgOff) & (pstWBObj->nNumWays - 1);
            nDstWay = WB_GET_VPN(nDstBlkOff, nDstPgOff) & (pstWBObj->nNumWays - 1);

            /* FIXME: Copyback should be decided as considering load time and data transfer time */
            /*        With async write + 50MHz, Copyback 6 sectors = 237 ~ 243 usec              */
            /*        With async write + 50MHz, Copyback 7 sectors = 264 ~ 270 usec              */
            /*        With async write + 50MHz, Write    8 sectors = 234 ~ 240 usec              */
            if (nSrcWay == nDstWay)
            {
                /* 3.1. Make params */
                nSTLMetaExt[0]  = FSR_STL_GetZBC(pstWBObj->pMainBuf, WB_PG_SIZE);
                nSTLMetaExt[1]  = ~(nSTLMetaExt[0]);

                staBMLRndIn[0].nOffset      = (UINT16) nLsnOff * FSR_SECTOR_SIZE;
                staBMLRndIn[0].nNumOfBytes  = (UINT16) nNumOfScts * FSR_SECTOR_SIZE;
                staBMLRndIn[0].pBuf         = pBuf;

                staBMLRndIn[1].nOffset      = (UINT16)(FSR_STL_CPBK_SPARE_EXT + sizeof(UINT32) * 2);
                staBMLRndIn[1].nNumOfBytes  = (UINT16)(sizeof(UINT32) * 2);
                staBMLRndIn[1].pBuf         = &(nSTLMetaExt[0]);

                stBMLCpBk.nSrcVun           = (UINT16)(nSrcBlkOff + pstWBObj->nStartVbn);
                stBMLCpBk.nSrcPgOffset      = (UINT16)(nSrcPgOff);
                stBMLCpBk.nDstVun           = (UINT16)(nDstBlkOff + pstWBObj->nStartVbn);
                stBMLCpBk.nDstPgOffset      = (UINT16)(nDstPgOff);
                stBMLCpBk.nRndInCnt         = 2;         /*   Data & spare  */
                stBMLCpBk.pstRndInArg       = staBMLRndIn;

                FSR_OAM_MEMSET(pstBMLCpBk, 0, sizeof(long) * FSR_MAX_WAYS); /* FIXME: sizeof(long)? */
                pstBMLCpBk[nSrcWay] = &stBMLCpBk;;

                if (WB_GET_VPN(nDstBlkOff, nDstPgOff) == 80)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                        (TEXT("[WBM:ERR] %s() L(%d) - FSR_BML_CopyBack(nDstVpn=%d) (nLpn=%d)\r\n"),
                        __FSR_FUNC__, __LINE__, WB_GET_VPN(nDstBlkOff, nDstPgOff), nLpn));
                }

                /* 3.2. Copyback data */
                nErr = FSR_BML_CopyBack(nVol,               /* The Volume ID for BML            */
                                        pstBMLCpBk,         /* The copyback argument for BML    */
                                        FSR_BML_FLAG_NONE); /* Must be FSR_BML_FLAG_NONE        */
                if (nErr != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                        (TEXT("[WBM:ERR] %s() L(%d) - FSR_BML_CopyBack(nSrcVun=%d nSrcPgOffset=%d nDstVun=%d nSrcPgOffset=%d) (0x%x)\r\n"),
                        __FSR_FUNC__, __LINE__, stBMLCpBk.nSrcVun, stBMLCpBk.nSrcPgOffset, stBMLCpBk.nDstVun, stBMLCpBk.nDstPgOffset, nErr));
                    break;
                }

                bUseCpBk = TRUE32;
            }
            else    /* if (nSrcWay == nDstWay) */
            {
                bUseCpBk = FALSE32;
            }
        }
        else    /* (nSrcBlkOff != WB_NULL_BOFFSET && nSrcPgOff != WB_NULL_POFFSET) */
        {
            /* FIXME: use FSR_BML_CopyBack */
            bUseCpBk = FALSE32;
        }

        /* 4. Write data into WB, if can not copyback - full sector write */
        if (bUseCpBk == FALSE32)
        {
            nErr = _WriteAlignedDataPages(nVol,
                                          nPart,
                                          nDstBlkOff,
                                          nDstPgOff,
                                          nLpn,
                                          1,
                                          NULL,
                                          pstWBObj->pMainBuf);
            if (nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - _WriteAlignedDataPages(nLpn=%d) (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nLpn, nErr));
                break;
            }
        }
        else
        {
            /* Update only the mapping information */
            nErr = _InsertMapItem(pstWBObj,
                                  nLpn,
                                  nDstBlkOff,
                                  nDstPgOff);
            if (nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - _InsertMapItem(nLpn=%d) (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nLpn, nErr));
                break;
            }
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}


/**
 * @brief       This function reads multiple data pages. (Page-aligned read)
 *
 * @param[in]   nVol            : Volume ID
 * @param[in]   nPart           : Partition ID offset (0~7)
 * @param[in]   nLpn            : LPN of data
 * @param[in]   nNumOfPgs       : # of pages to write
 * @param[in]   pActualRdPgs    : # of pages to be read
 * @param[out]  pBuf            : Main buffer
 *
 * @return      FSR_STL_SUCCESS
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PRIVATE INT32
_ReadAlignedDataPages   (UINT32     nVol,
                         UINT32     nPart,
                         PADDR      nLpn,
                         UINT32     nNumOfPgs,
                         UINT32    *pActualRdPgs,
                         UINT8     *pBuf)
{
    STLWBObj           *pstWBObj;
    BADDR               nBlkOff;
    BADDR               nNextBlkOff;
    POFFSET             nPgOff;
    POFFSET             nNextPgOff;
    UINT32              nCurLpn;
    UINT32              nEndLpn;
    UINT32              nVpn;
    UINT32              nPgNum;
    UINT32              nPgIdx;
    INT32               nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Get write buffer object */
    pstWBObj = gstSTLPartObj[nVol][nPart].pstWBObj;

    do
    {
        nCurLpn = nLpn;
        nEndLpn = nLpn + nNumOfPgs;

        while (nCurLpn < nEndLpn)
        {
            /* 1. Search first mapping info */
            nErr = _FindMapItem(pstWBObj,
                                nCurLpn,
                                &nBlkOff,
                                &nPgOff);
            if (nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - _FindMapItem() (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nErr));
                break;
            }

            if (nBlkOff == WB_NULL_BOFFSET || nPgOff == WB_NULL_POFFSET)
            {
                nCurLpn++;
                pBuf += WB_PG_SIZE;
                continue;
            }

            nVpn = WB_GET_VPN(nBlkOff, nPgOff);

            /* 2. Search next mapping info for reading multiple data pages */
            nPgNum = 1;
            while (nPgNum < WB_MAX_WRITE_REQUEST && (nCurLpn + nPgNum) < nEndLpn)
            {
                nErr = _FindMapItem(pstWBObj,
                                    nCurLpn + nPgNum,
                                    &nNextBlkOff,
                                    &nNextPgOff);
                if (nErr != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                        (TEXT("[WBM:ERR] %s() L(%d) - _FindMapItem() (0x%x)\r\n"),
                        __FSR_FUNC__, __LINE__, nErr));
                    break;
                }

                if (nNextBlkOff == WB_NULL_BOFFSET || nNextPgOff == WB_NULL_POFFSET)
                {
                    break;
                }
                else if (nNextBlkOff == nBlkOff && nNextPgOff == (nPgOff + nPgNum))
                {
                    nPgNum++;
                    continue;
                }
                else
                {
                    break;
                }
            }

            if (nErr != FSR_STL_SUCCESS)
            {
                break;
            }

            /* 3. Read multiple pages from write buffer */
            nErr = FSR_BML_Read(nVol,                               /* nVol             */
                                nVpn,                               /* nVpn             */
                                nPgNum,                             /* nNumOfPgs        */
                                pBuf,                               /* pMBuf            */
                                pstWBObj->pstSpareBuf,              /* pSBuf            */
                                FSR_BML_FLAG_USE_SPAREBUF);         /* nFlag            */
            if (nErr != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - FSR_BML_Read(nVpn=%d, nPgNum=%d) (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nVpn, nPgNum, nErr));
                break;
            }

            nErr = FSR_STL_SUCCESS;

            /* 4. Check validity of spare data */
            for (nPgIdx = 0; nPgIdx < nPgNum; nPgIdx++)
            {
                nErr = _CheckSpareValidity((pBuf + nPgIdx * WB_PG_SIZE),
                                           (pstWBObj->pstSpareBuf + nPgIdx),
                                           WB_PG_SIZE,
                                           WB_SIG_DATA);
                if (nErr != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                        (TEXT("[WBM:ERR] %s() L(%d) - _CheckSpareValidity(nVpn=%d) (0x%x)\r\n"),
                        __FSR_FUNC__, __LINE__, nVpn, nErr));
                    break;
                }

                FSR_ASSERT((nCurLpn + nPgIdx) == (pstWBObj->pstSpareBuf + nPgIdx)->pstSpareBufBase->nSTLMetaBase1);
            }

            if (nErr != FSR_STL_SUCCESS)
            {
                break;
            }

            nCurLpn += nPgNum;
            pBuf += nPgNum * WB_PG_SIZE;
        }

        /* 5. Set return value */
        if (pActualRdPgs != NULL)
        {
            if (nErr == FSR_STL_SUCCESS)
            {
                *pActualRdPgs = nNumOfPgs;
            }
            else
            {
                *pActualRdPgs = 0;
            }
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}


/**
 * @brief       This function reads several sectors of a data page. (Note! pMainBuf of WB)
 *
 * @param[in]   nVol            : Volume ID
 * @param[in]   nPart           : Partition ID offset (0~7)
 * @param[in]   nLpn            : LPN of data
 * @param[in]   nLsnOff         : Start LSN offset of data
 * @param[in]   nNumOfScts      : Number of sectors of data
 * @param[out]  pBuf            : Data buffer
 *
 * @return      FSR_STL_SUCCESS
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PRIVATE INT32
_ReadUnalignedDataPage  (UINT32         nVol,
                         UINT32         nPart,
                         PADDR          nLpn,
                         UINT32         nLsnOff,
                         UINT32         nNumOfScts,
                         UINT8         *pBuf)
{
    STLWBObj           *pstWBObj;
    BADDR               nBlkOff;
    POFFSET             nPgOff;
    UINT32              nVpn;
    INT32               nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Get write buffer object */
    pstWBObj = gstSTLPartObj[nVol][nPart].pstWBObj;

    do
    {
        /* 1. Search mapping info for written data */
        nErr = _FindMapItem(pstWBObj,
                            nLpn,
                            &nBlkOff,
                            &nPgOff);
        if (nErr != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - _FindMapItem() (0x%x)\r\n"),
                __FSR_FUNC__, __LINE__, nErr));
            break;
        }

        /* 2. Read a page from write buffer */
        if (nBlkOff != WB_NULL_BOFFSET && nPgOff != WB_NULL_POFFSET)
        {
            nVpn = WB_GET_VPN(nBlkOff, nPgOff);

            /* 2.1. BML read */
            nErr = FSR_BML_Read(nVol,                               /* nVol             */
                                nVpn,                               /* nVpn             */
                                1,                                  /* nNumOfPgs        */
                                pstWBObj->pMainBuf,                 /* pMBuf            */
                                pstWBObj->pstSpareBuf,              /* pSBuf            */
                                FSR_BML_FLAG_USE_SPAREBUF);         /* nFlag            */
            if (nErr != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - FSR_BML_Read(nVpn=%d) (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nVpn, nErr));
                break;
            }

            nErr = FSR_STL_SUCCESS;

            /* 2.2. Check validity of spare data */
            nErr = _CheckSpareValidity(pstWBObj->pMainBuf,
                                       pstWBObj->pstSpareBuf,
                                       WB_PG_SIZE,
                                       WB_SIG_DATA);
            if (nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - _CheckSpareValidity(nVpn=%d) (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nVpn, nErr));
                break;
            }

            FSR_ASSERT(nLpn == pstWBObj->pstSpareBufBase->nSTLMetaBase1);

            /* 2.3. If partial sector read, copy valid data from buf */
            FSR_OAM_MEMCPY(pBuf,
                           pstWBObj->pMainBuf + nLsnOff * FSR_SECTOR_SIZE,
                           nNumOfScts * FSR_SECTOR_SIZE);
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}
#endif  /* #if (OP_SUPPORT_WRITE_BUFFER == 1) */


/*****************************************************************************/
/* Global Function Definition                                                */
/*****************************************************************************/
#if (OP_SUPPORT_WRITE_BUFFER == 1)
/**
 * @brief       This function opens write buffer. (Note! pMainBuf of WB)
 *
 * @param[in]   nVol        : Volume ID
 * @param[in]   nPart       : Partition ID offset (0~7)
 * @param[in]   nFlag       : Flags
 *
 * @return      FSR_BML_NO_PIENTRY
 * @return      FSR_BML_SUCCESS
 * @return      FSR_STL_ERR_PHYSICAL
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PUBLIC INT32
FSR_STL_OpenWB  (UINT32     nVol,
                 UINT32     nPart,
                 UINT32     nFlag)
{
    STLWBObj           *pstWBObj;
    UINT8              *pMainBuf;
    FSRSpareBuf        *pstSpareBuf;
    FSRSpareBufBase    *pstSpareBufBase;
    FSRSpareBufExt     *pstSpareBufExt;
    UINT32              nBlkIdx;
    UINT32              nPrevBlkIdx;
    UINT32              nFirstValidBlkIdx;
    UINT32              nMaxBlksInHeaderPg;
    UINT32              nBlkOffInHeaderPg;
    UINT32              nTempAge;
    BOOL32              bHeaderFound;
    BOOL32              bNearHead;
    BADDR               nBlkOff;
    POFFSET             nPgOff;
    UINT32              nMaxPgOff;
    UINT32              nPgMapOff;
    UINT32              nLpn;
    UINT32              nVpn;
    UINT32              nBitmap;
    UINT8              *pTempBitmap;
    BOOL32              bDeleted;
    INT32               nBMLErr     = FSR_BML_SUCCESS;
    INT32               nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    do
    {
        /* 1. Set global variables of write buffer */
        nErr = _SetWBObj(nVol, nPart);
        if (nErr == FSR_BML_NO_PIENTRY)
        {
            break;
        }
        else if (nErr != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - _SetWBObj(nVol=%d, nPart=%d) (0x%x)\r\n"),
                __FSR_FUNC__, __LINE__, nVol, nPart, nErr));

            /* Free memory might be allocated */
            nErr = _FreeWBObj(nVol, nPart);
            if (nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - _FreeWBObj() (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nErr));
            }

            nErr = FSR_STL_CRITICAL_ERROR;

            break;
        }

        /* 2. Get write buffer object & Initialize local variables */
        pstWBObj = gstSTLPartObj[nVol][nPart].pstWBObj;

        pMainBuf            = pstWBObj->pMainBuf;
        pstSpareBuf         = pstWBObj->pstSpareBuf;
        pstSpareBufBase     = pstWBObj->pstSpareBufBase;
        pstSpareBufExt      = pstWBObj->pstSpareBufExt;

        nMaxBlksInHeaderPg  = (BADDR) (WB_PG_SIZE / (pstWBObj->nPgsPerBlk * sizeof(PADDR)));

        nBlkIdx             = 0;
        nPrevBlkIdx         = 0;
        nFirstValidBlkIdx   = 0;
        nTempAge            = 0;
        bHeaderFound        = FALSE32;
        bNearHead           = FALSE32;

        /* 3. Forward searching */
        while (nBlkIdx < pstWBObj->nNumTotalBlks)
        {
            /* 3.1. Read header page */
            nVpn = WB_GET_VPN(nBlkIdx, 0);

            nBMLErr = FSR_BML_Read(nVol,                            /* nVol             */
                                   nVpn,                            /* nVpn             */
                                   1,                               /* nNumOfPgs        */
                                   pMainBuf,                        /* pMBuf            */
                                   pstSpareBuf,                     /* pSBuf            */
                                   FSR_BML_FLAG_USE_SPAREBUF);      /* nFlag            */

            nErr = _CheckSpareValidity(pMainBuf,
                                       pstSpareBuf,
                                       WB_PG_SIZE,
                                       WB_SIG_HEADER);

            if (nBMLErr != FSR_BML_SUCCESS || nErr != FSR_STL_SUCCESS)
            {
                /* Invalid header means it is a clean page */
                if (bNearHead == TRUE32)
                {
                    /* If it was near head blk, it means we found latest head blk */
                    break;
                }
                else
                {
                    if (bHeaderFound == FALSE32)
                    {
                        nBlkIdx++;
                        nFirstValidBlkIdx++;
                    }
                    else
                    {
                        nBlkIdx = nPrevBlkIdx + 1;
                        bNearHead = TRUE32;
                    }
                    continue;
                }
            }

            bHeaderFound = TRUE32;

            /* 3.2. Process header */
            nTempAge = pstSpareBufBase->nSTLMetaBase1;
            if (pstWBObj->nHeaderAge < nTempAge)
            {
                /* It is a newer header */
                pstWBObj->nHeaderAge = nTempAge;
                pstWBObj->nHeadBlkOff = (BADDR) nBlkIdx;
                pstWBObj->nTailBlkOff = (BADDR) (pstSpareBufExt->nSTLMetaExt0 & 0xFFFF);

                /* Read mapping information */
                nBlkOffInHeaderPg = 0;
                nBlkOff = (BADDR) (nBlkIdx >= nMaxBlksInHeaderPg ? 
                                   nBlkIdx -  nMaxBlksInHeaderPg :
                                   pstWBObj->nNumTotalBlks + nBlkIdx - nMaxBlksInHeaderPg);
                while (nBlkOffInHeaderPg < nMaxBlksInHeaderPg)
                {
                    for (nPgOff = 1; nPgOff < pstWBObj->nPgsPerBlk; nPgOff++)
                    {
                        nPgMapOff = WB_GET_MAPOFF(nBlkOff, nPgOff);
                        FSR_ASSERT(nPgMapOff < pstWBObj->nNumTotalPgs);

                        nLpn      = *(((PADDR*) pstWBObj->pMainBuf) + 
                                    nBlkOffInHeaderPg * pstWBObj->nPgsPerBlk + nPgOff);

                        WB_SET_PGT(nPgMapOff, nLpn);
                    }

                    nBlkOffInHeaderPg++;
                    nBlkOff = WB_GET_NEXT_BLK(nBlkOff);
                }

                /* Calculate the next block index to read */
                if (bNearHead == TRUE32)
                {
                    nBlkIdx++;
                }
                else
                {
                    nPrevBlkIdx = nBlkIdx;
                    nBlkIdx = (nBlkIdx + nMaxBlksInHeaderPg) < pstWBObj->nNumTotalBlks ?
                              (nBlkIdx + nMaxBlksInHeaderPg) : (pstWBObj->nNumTotalBlks - 1);
                }
            }
            else
            {
                if (bNearHead == TRUE32)
                {
                    /* If it was near head blk, it means we found latest head blk */
                    break;
                }
                else
                {
                    nBlkIdx = nPrevBlkIdx + 1;
                    bNearHead = TRUE32;
                    continue;
                }
            }
        }

        /* If there is no header page, the partition might be unformatted */
        if (bHeaderFound == FALSE32)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - WB is not formatted(nVol=%d, nPart=%d) (0x%x)\r\n"),
                __FSR_FUNC__, __LINE__, nVol, nPart, nErr));
            nErr = FSR_STL_UNFORMATTED;
            break;
        }
        else
        {
            nBMLErr = FSR_BML_SUCCESS;
            nErr    = FSR_STL_SUCCESS;
        }

        /* 4. Backward searching */
        if (nFirstValidBlkIdx == 0 &&
            pstWBObj->nHeadBlkOff < pstWBObj->nTailBlkOff)
        {
            /* Calculate a block index to read for backward searching */
            nBlkIdx = pstWBObj->nNumTotalBlks - nMaxBlksInHeaderPg;
        }
        else
        {
            nBlkIdx = 0;
        }

        while (nBlkIdx > pstWBObj->nTailBlkOff)
        {
            /* 4.1. Read header page */
            nVpn = WB_GET_VPN(nBlkIdx, 0);

            nBMLErr = FSR_BML_Read(nVol,                            /* nVol             */
                                   nVpn,                            /* nVpn             */
                                   1,                               /* nNumOfPgs        */
                                   pMainBuf,                        /* pMBuf            */
                                   pstSpareBuf,                     /* pSBuf            */
                                   FSR_BML_FLAG_USE_SPAREBUF);      /* nFlag            */

            nErr = _CheckSpareValidity(pMainBuf,
                                       pstSpareBuf,
                                       WB_PG_SIZE,
                                       WB_SIG_HEADER);

            if (nBMLErr != FSR_BML_SUCCESS || nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - Meta data is broken (Read error) (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nErr));
                nErr = FSR_STL_META_BROKEN;
                break;
            }

            /* 4.2. Process header */
            nTempAge = pstSpareBufBase->nSTLMetaBase1;
            if (nTempAge < (pstWBObj->nHeaderAge - pstWBObj->nHeadBlkOff))
            {
                /* Read mapping information */
                if (nBlkIdx >= nMaxBlksInHeaderPg)
                {
                    nBlkOffInHeaderPg = 0;
                    nBlkOff = (BADDR) (nBlkIdx -  nMaxBlksInHeaderPg);
                }
                else
                {
                    nBlkOffInHeaderPg = nMaxBlksInHeaderPg - nBlkIdx;
                    nBlkOff = 0;
                }

                while (nBlkOffInHeaderPg < nMaxBlksInHeaderPg)
                {
                    if (nBlkOff >= pstWBObj->nTailBlkOff)
                    {
                        for (nPgOff = 1; nPgOff < pstWBObj->nPgsPerBlk; nPgOff++)
                        {
                            nPgMapOff = WB_GET_MAPOFF(nBlkOff, nPgOff);
                            FSR_ASSERT(nPgMapOff < pstWBObj->nNumTotalPgs);

                            nLpn      = *(((PADDR*) pstWBObj->pMainBuf) + 
                                        nBlkOffInHeaderPg * pstWBObj->nPgsPerBlk + nPgOff);

                            WB_SET_PGT(nPgMapOff, nLpn);
                        }
                    }

                    nBlkOffInHeaderPg++;
                    nBlkOff = WB_GET_NEXT_BLK(nBlkOff);
                }

                /* Calculate the next block index to read */
                nBlkIdx = (nBlkIdx > nMaxBlksInHeaderPg) ?
                          (nBlkIdx - nMaxBlksInHeaderPg): 0;
            }
            else
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - Meta data is broken (Wrong age)\r\n"),
                    __FSR_FUNC__, __LINE__));
                nErr = FSR_STL_META_BROKEN;
                break;
            }
        }

        if (nErr != FSR_STL_SUCCESS)
        {
            break;
        }

        /* 5. Read delete page of the head block */
        while (TRUE32)
        {
            /* 5.1. Read page - the page 1 is always a delete page */
            nVpn = WB_GET_VPN(pstWBObj->nHeadBlkOff, 1);

            nBMLErr = FSR_BML_Read(nVol,                            /* nVol             */
                                   nVpn,                            /* nVpn             */
                                   1,                               /* nNumOfPgs        */
                                   pMainBuf,                        /* pMBuf            */
                                   pstSpareBuf,                     /* pSBuf            */
                                   FSR_BML_FLAG_USE_SPAREBUF);      /* nFlag            */

            /* 5.2. Check whether it is a delete page or not */
            nErr = _CheckSpareValidity(pMainBuf,
                                       pstSpareBuf,
                                       WB_PG_SIZE,
                                       WB_SIG_DELETE);

            if (nBMLErr != FSR_BML_SUCCESS || nErr != FSR_STL_SUCCESS)
            {
                if (pstWBObj->nHeadBlkOff == pstWBObj->nTailBlkOff)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                        (TEXT("[WBM:ERR] %s() L(%d) - Meta data is broken (Wrong blk offset)\r\n"),
                        __FSR_FUNC__, __LINE__));
                    nErr = FSR_STL_META_BROKEN;
                    break;
                }

                /* Move head block offset backward */
                pstWBObj->nHeadBlkOff = (pstWBObj->nHeadBlkOff > 0) ?
                                        (pstWBObj->nHeadBlkOff - 1) : (pstWBObj->nNumTotalBlks - 1);
            }
            else
            {
                /* Successfully read a delete page. 
                   Copy it to the valid sector bitmap. */
                FSR_OAM_MEMSET(pstWBObj->pPgBitmap, 0x00, pstWBObj->nNumTotalPgs >> 3);
                FSR_OAM_MEMCPY(pstWBObj->pPgBitmap,
                               pMainBuf,
                               pstWBObj->nNumTotalPgs >> 3);
                break;
            }
        }

        if (nErr != FSR_STL_SUCCESS)
        {
            break;
        }

        FSR_DBZ_RTLMOUT(FSR_DBZ_INF | FSR_DBZ_STL_WBM,
            (TEXT("[WBM:INF] %s() L(%d) - Previous Head Block Offset (nBlkOff=%d) (0x%x)\r\n"),
            __FSR_FUNC__, __LINE__, pstWBObj->nHeadBlkOff, nErr));

        /* 6. Delete map items wrongly inserted                     */
        /*    Note! If head = tail, all map items should be deleted */
        nBlkOff = pstWBObj->nHeadBlkOff;
        do
        {
            for (nPgOff = 0; nPgOff < pstWBObj->nPgsPerBlk; nPgOff++)
            {
                nPgMapOff = WB_GET_MAPOFF(nBlkOff, nPgOff);
                FSR_ASSERT(nPgMapOff < pstWBObj->nNumTotalPgs);

                WB_SET_PGT(nPgMapOff, WB_NULL_LPN);
                WB_SET_COLT(nPgMapOff, WB_NULL_POFFSET);
                WB_RESET_BITMAP(nPgMapOff);
            }

            nBlkOff = WB_GET_NEXT_BLK(nBlkOff);
        } while (nBlkOff != pstWBObj->nTailBlkOff);

        /* 7. Insert map items */
        nBlkOff = pstWBObj->nTailBlkOff;
        while (nBlkOff != pstWBObj->nHeadBlkOff)
        {
            for (nPgOff = 0; nPgOff < pstWBObj->nPgsPerBlk; nPgOff++)
            {
                nPgMapOff = WB_GET_MAPOFF(nBlkOff, nPgOff);
                FSR_ASSERT(nPgMapOff < pstWBObj->nNumTotalPgs);

                nLpn      = WB_GET_PGT(nPgMapOff);
                nBitmap   = WB_GET_BITMAP(nPgMapOff);

                if ((nBitmap != WB_INVALID_BITMAP) &&
                    (nLpn == WB_NULL_LPN || nLpn == WB_DEL_LPN))
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                        (TEXT("[WBM:ERR] %s() L(%d) - Meta data is broken (Wrong bitmap)\r\n"),
                        __FSR_FUNC__, __LINE__));
                    nErr = FSR_STL_META_BROKEN;
                    break;
                }
                else if ((nBitmap != WB_INVALID_BITMAP) ||
                         (nLpn == WB_DEL_LPN  && nBitmap == WB_INVALID_BITMAP))
                {
                    nErr = _InsertMapItem(pstWBObj,
                                          nLpn,
                                          nBlkOff,
                                          nPgOff);
                    if (nErr != FSR_STL_SUCCESS)
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                            (TEXT("[WBM:ERR] %s() L(%d) - _InsertMapItem() (0x%x)\r\n"),
                            __FSR_FUNC__, __LINE__, nErr));
                        break;
                    }
                }
            }

            if (nErr != FSR_STL_SUCCESS)
            {
                break;
            }

            /* Go to the next block */
            nBlkOff = WB_GET_NEXT_BLK(nBlkOff);
        }

        if (nErr != FSR_STL_SUCCESS)
        {
            break;
        }

        /* 7.1. Insert a delete page of head block (Page offset = 1)*/
        nErr = _InsertMapItem(pstWBObj,
                              WB_DEL_LPN,
                              pstWBObj->nHeadBlkOff,
                              1);
        if (nErr != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - _InsertMapItem() (0x%x)\r\n"),
                __FSR_FUNC__, __LINE__, nErr));
            break;
        }

        /* 8. Head block scanning */
        pstWBObj->nHeadPgOff = 2;
        while (pstWBObj->nHeadPgOff < pstWBObj->nPgsPerBlk)
        {
            /* 8.1. Read page */
            nVpn = WB_GET_VPN(pstWBObj->nHeadBlkOff, pstWBObj->nHeadPgOff);

            nBMLErr = FSR_BML_Read(nVol,                            /* nVol             */
                                   nVpn,                            /* nVpn             */
                                   1,                               /* nNumOfPgs        */
                                   pMainBuf,                        /* pMBuf            */
                                   pstSpareBuf,                     /* pSBuf            */
                                   FSR_BML_FLAG_USE_SPAREBUF);      /* nFlag            */

            if (nBMLErr != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_INF | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:INF] %s() L(%d) - FSR_BML_Read error(nPgOff=%d) (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, pstWBObj->nHeadPgOff, nErr));
                nErr = FSR_STL_SUCCESS;
                break;
            }

            /* 8.2. Check whether it is data page */
            nErr = _CheckSpareValidity(pMainBuf,
                                       pstSpareBuf,
                                       WB_PG_SIZE,
                                       WB_SIG_DATA);
            if (nErr == FSR_STL_SUCCESS)
            {
                /* It is a data page */
                nLpn = pstSpareBufBase->nSTLMetaBase1;
                nErr = _InsertMapItem(pstWBObj,
                                      nLpn,
                                      pstWBObj->nHeadBlkOff,
                                      pstWBObj->nHeadPgOff);
                if (nErr != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                        (TEXT("[WBM:ERR] %s() L(%d) - _InsertMapItem(nLpn=%d) (0x%x)\r\n"),
                        __FSR_FUNC__, __LINE__, nLpn, nErr));
                    break;
                }
            }
            else
            {
                /* 8.3. Check whether it is delete page */
                nErr = _CheckSpareValidity(pMainBuf,
                                           pstSpareBuf,
                                           WB_PG_SIZE,
                                           WB_SIG_DELETE);
                if (nErr == FSR_STL_SUCCESS)
                {
                    /* It is a delete page. */
                    pTempBitmap = (UINT8*) pMainBuf;

                    nBlkOff = pstWBObj->nTailBlkOff;
                    while (nBlkOff != WB_GET_NEXT_BLK(pstWBObj->nHeadBlkOff))
                    {
                        nMaxPgOff = (nBlkOff == pstWBObj->nHeadBlkOff) ?
                                    pstWBObj->nHeadPgOff : pstWBObj->nPgsPerBlk;

                        for (nPgOff = 0; nPgOff < nMaxPgOff; nPgOff++)
                        {
                            nPgMapOff = WB_GET_MAPOFF(nBlkOff, nPgOff);
                            FSR_ASSERT(nPgMapOff < pstWBObj->nNumTotalPgs);

                            nLpn      = WB_GET_PGT(nPgMapOff);
                            nBitmap   = WB_GET_BITMAP(nPgMapOff);

                            /* Delete mapping info */
                            if (nLpn != WB_NULL_LPN && nBitmap != WB_INVALID_BITMAP)
                            {
                                nBitmap = pTempBitmap[WB_BITMAP_NUM(nPgMapOff)] & (1 << WB_BITMAP_OFF(nPgMapOff));

                                if (nBitmap == WB_INVALID_BITMAP)
                                {
                                    nErr = _DeleteMapItem(pstWBObj,
                                                          nLpn,
                                                          &bDeleted);
                                    if (nErr != FSR_STL_SUCCESS)
                                    {
                                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                                            (TEXT("[WBM:ERR] %s() L(%d) - _DeleteMapItem(nLpn=%d) (0x%x)\r\n"),
                                            __FSR_FUNC__, __LINE__, nLpn, nErr));
                                        break;
                                    }
                                }
                            }
                        }

                        if (nErr != FSR_STL_SUCCESS)
                        {
                            break;
                        }

                        /* Go to the next block */
                        nBlkOff = WB_GET_NEXT_BLK(nBlkOff);
                    }

                    if (nErr != FSR_STL_SUCCESS)
                    {
                        break;
                    }
                }
                else
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_INF | FSR_DBZ_STL_WBM,
                        (TEXT("[WBM:INF] %s() L(%d) - Invalid page (nPgOff=%d) (0x%x)\r\n"),
                        __FSR_FUNC__, __LINE__, pstWBObj->nHeadPgOff, nErr));
                    nErr = FSR_STL_SUCCESS;
                    break;
                }
            }

            pstWBObj->nHeadPgOff++;
        }

        pstWBObj->nHeadPgOff--; /* Note! This is not necessary. Just for debugging message */

        if (nErr != FSR_STL_SUCCESS)
        {
            break;
        }

        FSR_DBZ_RTLMOUT(FSR_DBZ_INF | FSR_DBZ_STL_WBM,
            (TEXT("[WBM:INF] %s() L(%d) - Previous Head Page Offset (nPgOff=%d) (0x%x)\r\n"),
            __FSR_FUNC__, __LINE__, pstWBObj->nHeadPgOff, nErr));

        /* To avoid the soft program issue, move page offset to the last page of a block*/
        pstWBObj->nHeadPgOff = (POFFSET) pstWBObj->nPgsPerBlk - 1;

        /* Increase header age for the next block */
        pstWBObj->nHeaderAge++;

        nErr = FSR_STL_SUCCESS;

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}


/**
 * @brief       This function closes write buffer.
 *
 * @param[in]   nVol        : Volume ID
 * @param[in]   nPart       : Partition ID offset (0~7)
 *
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_ERR_PARAM
 * @return      FSR_STL_ERR_PHYSICAL
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PUBLIC INT32
FSR_STL_CloseWB  (UINT32    nVol,
                  UINT32    nPart)
{
    STLWBObj       *pstWBObj;
    INT32           nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Get write buffer object */
    pstWBObj = gstSTLPartObj[nVol][nPart].pstWBObj;

    do
    {
#if defined (FSR_POR_USING_LONGJMP)
        if (gIsPorClose != TRUE32)
#endif

#if (OP_SUPPORT_CLOSE_TIME_UPDATE_FOR_FASTER_OPEN == 1)
        {
            /* Allocate a new block & Write meta data for fast re-open */
            if (pstWBObj->nHeadPgOff > 1 && pstWBObj->bDirty == TRUE32)
            {
                nErr = _AllocateNewBlk(nVol, nPart);
                if (nErr != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                        (TEXT("[WBM:ERR] %s() L(%d) - _AllocateNewBlk() (0x%x)\r\n"),
                        __FSR_FUNC__, __LINE__, nErr));
                    break;
                }
            }
        }
#endif

        nErr = _FreeWBObj(nVol, nPart);
        if (nErr != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - _FreeWBObj() (0x%x)\r\n"),
                __FSR_FUNC__, __LINE__, nErr));
            break;
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}


/**
 * @brief       This function formats write buffer.
 *
 * @param[in]   nVol        : Volume ID
 * @param[in]   nPart       : Partition ID offset (0~7)
 *
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_ERR_PARAM
 * @return      FSR_STL_ERR_PHYSICAL
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PUBLIC INT32
FSR_STL_FormatWB (UINT32    nVol,
                  UINT32    nPart)
{
    FSRVolSpec          stVolSpec;
    FSRPartEntry        stPartEntry;
    UINT32              n1stVpn;
    UINT32              nSctsPerPgSft;
    UINT32              nPgsPerUnit;
    UINT32              nVbn;
    UINT8              *pMainBuf    = NULL;
    FSRSpareBuf         stSpareBuf;
    FSRSpareBufBase     stSpareBufBase;
    FSRSpareBufExt      stSpareBufExt;
    UINT32              nZBC;
    INT32               nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    do
    {
        /* Temporary buffer allocation */
        pMainBuf = (UINT8*) FSR_OAM_Malloc(32 * FSR_SECTOR_SIZE);
        if (pMainBuf == NULL)
        {
            nErr = FSR_STL_OUT_OF_MEMORY;
            break;
        }
        else if ((((UINT32)(pMainBuf)) & 0x03))
        {
            nErr = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }

        /* 1. Get Volume Information */
        nErr = FSR_BML_GetVolSpec(nVol,                 /* Volume ID                    */
                                 &stVolSpec,            /* Volume Information           */
                                  FSR_BML_FLAG_NONE);   /* Must be FSR_BML_FLAG_NONE    */
        if (nErr != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - FSR_BML_GetVolSpec() (0x%x)\r\n"),
                __FSR_FUNC__, __LINE__, nErr));
            FSR_ASSERT(nErr != FSR_BML_INVALID_PARAM);
            break;
        }

        nSctsPerPgSft = FSR_STL_GetShiftBit(stVolSpec.nSctsPerPg);

        /* 2. Get BML partition information */
        nErr = FSR_BML_LoadPIEntry(nVol,                        /* Volume ID                */
                                   nPart + FSR_PARTID_STL0_WB,  /* Partition ID             */
                                  &n1stVpn,                     /* The first VPN            */
                                  &nPgsPerUnit,                 /* The # of pages per unit  */
                                  &stPartEntry);                /* Partition Information    */
        if (nErr == FSR_BML_NO_PIENTRY)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:INF]  %s() L(%d) - No write buffer exists (nVol=%d, nPart=%d)\r\n"),
                __FSR_FUNC__, __LINE__, nVol, nPart));
            break;
        }
        else if (nErr != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR]  %s() L(%d) - FSR_BML_LoadPIEntry(nVol=%d, nPart=%d) (0x%x)\r\n"),
                __FSR_FUNC__, __LINE__, nVol, nPart, nErr));
            FSR_ASSERT(nErr != FSR_BML_VOLUME_NOT_OPENED);
            FSR_ASSERT(nErr != FSR_BML_INVALID_PARAM);
            break;
        }

        /* 2.1. The attribute of write buffer partition is restricted */
        if (stPartEntry.nAttr != (FSR_BML_PI_ATTR_RW | FSR_BML_PI_ATTR_SLC))
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - WB partition has wrong attributes! (nVol=%d, nPart=%d)\r\n"),
                __FSR_FUNC__, __LINE__, nVol, nPart));
            nErr = FSR_STL_PARTITION_ERROR;
            break;
        }

        /* 2.2. The size of write buffer partition is restricted.
                (# of pages in write buffer should be smaller than # of bits in one page
                and also larger than # of 4-bytes (PADDR) in one page.) */
        if (((UINT32)stPartEntry.nNumOfUnits * nPgsPerUnit) > 
            (((UINT32)FSR_SECTOR_SIZE << nSctsPerPgSft) << 3))
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - WB partition is too big! (nVol=%d, nPart=%d)\r\n"),
                __FSR_FUNC__, __LINE__, nVol, nPart));
            nErr = FSR_STL_PARTITION_ERROR;
            break;
        }
        else if (((UINT32)stPartEntry.nNumOfUnits * nPgsPerUnit * sizeof(PADDR)) < 
                 ((UINT32)FSR_SECTOR_SIZE << nSctsPerPgSft))
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - WB partition is too small! (nVol=%d, nPart=%d)\r\n"),
                __FSR_FUNC__, __LINE__, nVol, nPart));
            nErr = FSR_STL_PARTITION_ERROR;
            break;
        }

        /* 3. Erase all WB blocks */
        for (nVbn = stPartEntry.n1stVun; 
             nVbn < stPartEntry.n1stVun + stPartEntry.nNumOfUnits; nVbn++)
        {
            nErr = FSR_BML_Erase(nVol,                  /* Volume ID for BML        */
                                 &nVbn,                 /* Virtual Unit Number      */
                                 1,                     /* The number of VUN        */
                                 FSR_BML_FLAG_NONE);    /* Flags for erase          */
            if (nErr != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - FSR_BML_Erase(nVbn=%d) (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nVbn, nErr));
                FSR_ASSERT(nErr != FSR_BML_VOLUME_NOT_OPENED);
                FSR_ASSERT(nErr != FSR_BML_WR_PROTECT_ERROR);
                break;
            }
        }
        
        if (nVbn != (stPartEntry.n1stVun + stPartEntry.nNumOfUnits))
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - All blocks are not erased (nVbn=%d) (0x%x)\r\n"),
                __FSR_FUNC__, __LINE__, nVbn, nErr));
            break;
        }

        /* 4. Write a dummy header (for open) */
        FSR_OAM_MEMSET(pMainBuf, 0xFF, 32 * FSR_SECTOR_SIZE);

        stSpareBuf.pstSpareBufBase      = &stSpareBufBase;
        stSpareBuf.pstSTLMetaExt        = &stSpareBufExt;
        stSpareBuf.nNumOfMetaExt        = 1;
        stSpareBufBase.nSTLMetaBase0    = WB_SIG_HEADER;        /* Signature        */
        stSpareBufBase.nSTLMetaBase1    = 0x00000001;           /* Age              */
        stSpareBufBase.nSTLMetaBase2    = 0xFFFFFFFE;           /* Age'             */
        stSpareBufExt.nSTLMetaExt0      = 0xFFFF0000;           /* TailBlkOff | TailBlkOff' */
        stSpareBufExt.nSTLMetaExt1      = 0xFFFFFFFF;           /* 0xFFFFFFFF       */
        stSpareBufExt.nSTLMetaExt2      = 0x00000000;           /* ZBC              */
        stSpareBufExt.nSTLMetaExt3      = 0xFFFFFFFF;           /* ZBC'             */

        nErr = FSR_BML_Write(nVol,                              /* nVol             */
                             n1stVpn,                           /* nVpn             */
                             1,                                 /* nNumOfPgs        */
                             pMainBuf,                          /* pMBuf            */
                             &stSpareBuf,                       /* pSBuf            */
                             FSR_BML_FLAG_USE_SPAREBUF);        /* nFlag            */
        if (nErr != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - FSR_BML_Write(nVpn=%d) (0x%x)\r\n"), 
                __FSR_FUNC__, __LINE__, n1stVpn, nErr));
            break;
        }

        /* 5. Write a dummy delete page */
        FSR_OAM_MEMSET(pMainBuf, 0x00, (stPartEntry.nNumOfUnits * nPgsPerUnit) >> 3);

        nZBC = FSR_STL_GetZBC(pMainBuf, (FSR_SECTOR_SIZE << nSctsPerPgSft));

        stSpareBuf.pstSpareBufBase      = &stSpareBufBase;
        stSpareBuf.pstSTLMetaExt        = &stSpareBufExt;
        stSpareBuf.nNumOfMetaExt        = 1;
        stSpareBufBase.nSTLMetaBase0    = WB_SIG_DELETE;        /* Signature        */
        stSpareBufBase.nSTLMetaBase1    = 0xFFFFFFFF;           /* 0xFFFFFFFF       */
        stSpareBufBase.nSTLMetaBase2    = 0xFFFFFFFF;           /* 0xFFFFFFFF       */
        stSpareBufExt.nSTLMetaExt0      = 0xFFFFFFFF;           /* 0xFFFFFFFF       */
        stSpareBufExt.nSTLMetaExt1      = 0xFFFFFFFF;           /* 0xFFFFFFFF       */
        stSpareBufExt.nSTLMetaExt2      = nZBC;                 /* ZBC              */
        stSpareBufExt.nSTLMetaExt3      = ~(nZBC);              /* ZBC'             */

        nErr = FSR_BML_Write(nVol,                              /* nVol             */
                             n1stVpn + 1,                       /* nVpn             */
                             1,                                 /* nNumOfPgs        */
                             pMainBuf,                          /* pMBuf            */
                             &stSpareBuf,                       /* pSBuf            */
                             FSR_BML_FLAG_USE_SPAREBUF);        /* nFlag            */
        if (nErr != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:ERR] %s() L(%d) - FSR_BML_Write(nVpn=%d) (0x%x)\r\n"),
                __FSR_FUNC__, __LINE__, n1stVpn + 1, nErr));
            break;
        }

        nErr = FSR_STL_SUCCESS;

    } while (0);
    
    if (pMainBuf != NULL)
    {
        FSR_OAM_Free(pMainBuf);
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}


/**
 * @brief       This function reads valid sectors from write buffer.
 *
 * @param[in]   nVol        : Volume ID
 * @param[in]   nPart       : Partition ID offset (0~7)
 * @param[in]   nLsn        : Start logical sector number
 * @param[in]   nNumOfScts  : Number of sectors to read (under one page)
 * @param[out]  pBuf        : User buffer pointer
 *
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_ERR_PARAM
 * @return      FSR_STL_ERR_PHYSICAL
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PUBLIC INT32
FSR_STL_ReadWB  (UINT32     nVol,
                 UINT32     nPart,
                 UINT32     nLsn,
                 UINT32     nNumOfScts,
                 UINT8     *pBuf,
                 UINT32     nFlag)
{
    STLWBObj           *pstWBObj;
    PADDR               nCurLpn;
    UINT32              nCurLsnOff;
    UINT32              nRemainingScts;
    UINT32              nRdPgs;
    UINT32              nRdScts;
    UINT32              nActualRdPgs;
    UINT32              nActualRdScts;
    INT32               nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Get write buffer object */
    pstWBObj = gstSTLPartObj[nVol][nPart].pstWBObj;

    do
    {
        /* Initialize local variables, Pgs and Scts */
        nCurLpn        = nLsn >> pstWBObj->nSctsPerPgSft;
        nCurLsnOff     = nLsn & (pstWBObj->nSctsPerPg - 1);
        nRemainingScts = nNumOfScts;

        while (nRemainingScts > 0)
        {
            /* Read page */
            if (nCurLsnOff != 0 || nRemainingScts < pstWBObj->nSctsPerPg)
            {
                /* Unaligned read - calculate # of sectors */
                nRdPgs = 1;
                nRdScts = (pstWBObj->nSctsPerPg - nCurLsnOff) < nRemainingScts ? 
                          (pstWBObj->nSctsPerPg - nCurLsnOff) : nRemainingScts;

                /* Read a single page - partial sector read */
                nErr = _ReadUnalignedDataPage(nVol,
                                              nPart,
                                              nCurLpn,
                                              nCurLsnOff,
                                              nRdScts,
                                              pBuf);
                if (nErr != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                        (TEXT("[WBM:ERR] %s() L(%d) - _ReadUnalignedDataPage(nLpn=%d) (0x%x)\r\n"),
                        __FSR_FUNC__, __LINE__, nCurLpn, nErr));
                    break;
                }

                /* Read results */
                nActualRdPgs  = nRdPgs;
                nActualRdScts = nRdScts;
            }
            else
            {
                /* Aligned read - calculate # of pages */
                nRdPgs  = nRemainingScts >> pstWBObj->nSctsPerPgSft;
                nRdScts = nRdPgs << pstWBObj->nSctsPerPgSft;

                /* Read multiple pages - full sector read only */
                nErr = _ReadAlignedDataPages(nVol,
                                             nPart,
                                             nCurLpn,
                                             nRdPgs,
                                             &nActualRdPgs,
                                             pBuf);
                if (nErr != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                        (TEXT("[WBM:ERR] %s() L(%d) - _ReadAlignedDataPages(nLpn=%d) (0x%x)\r\n"),
                        __FSR_FUNC__, __LINE__, nCurLpn, nErr));
                    break;
                }

                /* Read results - nActualRdPgs is set by _ReadAlignedDataPages() */
                nActualRdScts = nActualRdPgs << pstWBObj->nSctsPerPgSft;
            }

            /* Set variables */
            nCurLpn        += nActualRdPgs;
            nCurLsnOff      = 0;
            nRemainingScts -= nActualRdScts;
            pBuf           += nActualRdScts * FSR_SECTOR_SIZE;
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}


/**
 * @brief       This function writes sectors to write buffer.
 *
 * @param[in]   nVol        : Volume ID
 * @param[in]   nPart       : Partition ID offset (0~7)
 * @param[in]   nLsn        : Start logical sector number
 * @param[in]   nNumOfScts  : Number of sectors to write
 * @param[in]   pBuf        : User buffer pointer 
 * @param[in]   nFlag       : Flags
 *
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_ERR_PARAM
 * @return      FSR_STL_ERR_PHYSICAL
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PUBLIC INT32
FSR_STL_WriteWB (UINT32     nVol,
                 UINT32     nPart,
                 UINT32     nLsn,
                 UINT32     nNumOfScts,
                 UINT8     *pBuf,
                 UINT32     nFlag)
{
    STLWBObj           *pstWBObj;
    PADDR               nCurLpn;
    UINT32              nCurLsnOff;
    UINT32              nRemainingScts;
    UINT32              nWrPgs;
    UINT32              nWrScts;
    UINT32              nActualWrPgs;
    UINT32              nActualWrScts;
    INT32               nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Get write buffer object */
    pstWBObj = gstSTLPartObj[nVol][nPart].pstWBObj;

    do
    {
        /* Check whether write buffer can handle this request or not */
        nErr = _CheckWritable(pstWBObj, nLsn, nNumOfScts);
        if (nErr != FSR_STL_SUCCESS)
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_INF | FSR_DBZ_STL_WBM,
                (TEXT("[WBM:INF] %s() L(%d) - Not writable (nLsn=%d, nNumOfScts=%d)\r\n"),
                __FSR_FUNC__, __LINE__, nLsn, nNumOfScts));
            break;
        }

        /* Initialize local variables, Pgs and Scts */
        nCurLpn        = nLsn >> pstWBObj->nSctsPerPgSft;
        nCurLsnOff     = nLsn - (nCurLpn << pstWBObj->nSctsPerPgSft);
        nRemainingScts = nNumOfScts;

        while (nRemainingScts > 0)
        {
            /* 1. Move head pointer */
            if (((UINT32) pstWBObj->nHeadPgOff + 1) == pstWBObj->nPgsPerBlk)
            {
                /* If it is the end of block, allocate new block */
                nErr = _AllocateNewBlk(nVol, nPart);
                if (nErr != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                        (TEXT("[WBM:ERR] %s() L(%d) - _AllocateNewBlk() (0x%x)\r\n"),
                        __FSR_FUNC__, __LINE__, nErr));
                    break;
                }

                pstWBObj->nHeadBlkOff = WB_GET_NEXT_BLK(pstWBObj->nHeadBlkOff);
                pstWBObj->nHeadPgOff = 1;
            }

            /* 2. Write page */
            if (nCurLsnOff != 0 || nRemainingScts < pstWBObj->nSctsPerPg)
            {
                /* Unaligned write - calculate # of sectors */
                nWrPgs  = 1;
                nWrScts = (pstWBObj->nSctsPerPg - nCurLsnOff) < nRemainingScts ?
                          (pstWBObj->nSctsPerPg - nCurLsnOff) : nRemainingScts;

                /* Write a single page - partial sector write */
                nErr = _WriteUnalignedDataPage(nVol,
                                               nPart,
                                               pstWBObj->nHeadBlkOff,
                                               pstWBObj->nHeadPgOff + 1,
                                               nCurLpn,
                                               nCurLsnOff,
                                               nWrScts,
                                               pBuf);
                if (nErr != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                        (TEXT("[WBM:ERR] %s() L(%d) - _WriteUnalignedDataPage(nLpn=%d) (0x%x)\r\n"),
                        __FSR_FUNC__, __LINE__, nCurLpn, nErr));
                    break;
                }

                /* Write results */
                nActualWrPgs  = nWrPgs;
                nActualWrScts = nWrScts;
            }
            else
            {
                /* Aligned write - calculate # of pages */
                nWrPgs  = nRemainingScts >> pstWBObj->nSctsPerPgSft;
                nWrScts = nWrPgs << pstWBObj->nSctsPerPgSft;

                /* Write multiple pages - full sector write only */
                nErr = _WriteAlignedDataPages(nVol,
                                              nPart,
                                              pstWBObj->nHeadBlkOff,
                                              pstWBObj->nHeadPgOff + 1,
                                              nCurLpn,
                                              nWrPgs,
                                              &nActualWrPgs,
                                              pBuf);
                if (nErr != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                        (TEXT("[WBM:ERR] %s() L(%d) - _WriteAlignedDataPages(nLpn=%d) (0x%x)\r\n"),
                        __FSR_FUNC__, __LINE__, nCurLpn, nErr));
                    break;
                }

                /* Write results - nActualWrPgs is set by _WriteAlignedDataPages() */
                nActualWrScts = nActualWrPgs << pstWBObj->nSctsPerPgSft;
            }

            /* 3. Set variables */
            nCurLpn        += nActualWrPgs;
            nCurLsnOff      = 0;
            nRemainingScts -= nActualWrScts;
            pBuf           += nActualWrScts * FSR_SECTOR_SIZE;

            pstWBObj->nHeadPgOff = pstWBObj->nHeadPgOff + (POFFSET) nActualWrPgs;
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}


/**
 * @brief       This function deletes sectors from write buffer. (Note! pMainBuf of WB)
 *
 * @param[in]   nVol        : Volume ID
 * @param[in]   nPart       : Partition ID offset (0~7)
 * @param[in]   nLsn        : Start logical sector number
 * @param[in]   nNumOfScts  : Number of sectors to write
 * @param[in]   nFlag       : Flags
 *
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_ERR_PARAM
 * @return      FSR_STL_ERR_PHYSICAL
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PUBLIC INT32
FSR_STL_DeleteWB(UINT32     nVol,
                 UINT32     nPart,
                 UINT32     nLsn,
                 UINT32     nNumOfScts,
                 BOOL32     bSyncMode,
                 UINT32     nFlag)
{
    STLWBObj           *pstWBObj;
    UINT32              nCurLpn;
    UINT32              nCurLsnOff;
    UINT32              nRemainingScts;
    UINT32              nDeleteScts;
    BADDR               nBlkOff;
    POFFSET             nPgOff;
    PADDR               nVpn;
    BOOL32              bDeleted;
    BOOL32              bWriteDelPage;
    UINT32              nIdx;
    INT32               nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Get write buffer object */
    pstWBObj = gstSTLPartObj[nVol][nPart].pstWBObj;

    do
    {
        /* Initialize local variables, LPNs and Scts */
        nCurLpn        = nLsn >> pstWBObj->nSctsPerPgSft;
        nCurLsnOff     = nLsn - (nCurLpn << pstWBObj->nSctsPerPgSft);
        nRemainingScts = nNumOfScts;

        bWriteDelPage  = FALSE32;

        /* 1. Reset valid bitmaps for deleted pages */
        while (nRemainingScts > 0)
        {
            nDeleteScts = (pstWBObj->nSctsPerPg - nCurLsnOff) < nRemainingScts ?
                          (pstWBObj->nSctsPerPg - nCurLsnOff) : nRemainingScts;

            if (nDeleteScts < pstWBObj->nSctsPerPg)
            {
                /* If delete sync, read and overwrite partial page */
                if (bSyncMode == TRUE32)
                {
                    /* Search mapping info for written data */
                    nErr = _FindMapItem(pstWBObj,
                                        nCurLpn,
                                        &nBlkOff,
                                        &nPgOff);
                    if (nErr != FSR_STL_SUCCESS)
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                            (TEXT("[WBM:ERR] %s() L(%d) - _FindMapItem() (0x%x)\r\n"),
                            __FSR_FUNC__, __LINE__, nErr));
                        break;
                    }

                    /* Read a page from WB, if it exists */
                    if (nBlkOff != WB_NULL_BOFFSET && nPgOff != WB_NULL_POFFSET)
                    {
                        nVpn = WB_GET_VPN(nBlkOff, nPgOff);

                        /* BML read */
                        nErr = FSR_BML_Read(nVol,                           /* nVol             */
                                            nVpn,                           /* nVpn             */
                                            1,                              /* nNumOfPgs        */
                                            pstWBObj->pMainBuf,             /* pMBuf            */
                                            pstWBObj->pstSpareBuf,          /* pSBuf            */
                                            FSR_BML_FLAG_USE_SPAREBUF);     /* nFlag            */
                        if (nErr != FSR_BML_SUCCESS)
                        {
                            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                                (TEXT("[WBM:ERR] %s() L(%d) - FSR_BML_Read(nVpn=%d) (0x%x)\r\n"),
                                __FSR_FUNC__, __LINE__, nVpn, nErr));
                            break;
                        }

                        /* Check validity of spare data */
                        nErr = _CheckSpareValidity(pstWBObj->pMainBuf,
                                                   pstWBObj->pstSpareBuf,
                                                   WB_PG_SIZE,
                                                   WB_SIG_DATA);
                        if (nErr != FSR_STL_SUCCESS)
                        {
                            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                                (TEXT("[WBM:ERR] %s() L(%d) - _CheckSpareValidity(nVpn=%d) (0x%x)\r\n"),
                                __FSR_FUNC__, __LINE__, nVpn, nErr));
                            break;
                        }

                        FSR_ASSERT(nCurLpn == pstWBObj->pstSpareBufBase->nSTLMetaBase1);

                        /* Overwrite sectors with 0xFF */
                        FSR_OAM_MEMSET(pstWBObj->pMainBuf + nCurLsnOff * FSR_SECTOR_SIZE,
                                       0xFF, 
                                       nDeleteScts * FSR_SECTOR_SIZE);

                        /* Move head pointer */
                        if (((UINT32) pstWBObj->nHeadPgOff + 1) == pstWBObj->nPgsPerBlk)
                        {
                            /* Allocate new block */
                            nErr = _AllocateNewBlk(nVol, nPart);
                            if (nErr != FSR_STL_SUCCESS)
                            {
                                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                                    (TEXT("[WBM:ERR] %s() L(%d) - _AllocateNewBlk() (0x%x)\r\n"),
                                    __FSR_FUNC__, __LINE__, nErr));
                                break;
                            }

                            pstWBObj->nHeadBlkOff = (BADDR) WB_GET_NEXT_BLK(pstWBObj->nHeadBlkOff);
                            pstWBObj->nHeadPgOff = 1;
                        }

                        /* Write the page */
                        nErr = _WriteAlignedDataPages(nVol,
                                                      nPart,
                                                      pstWBObj->nHeadBlkOff,
                                                      pstWBObj->nHeadPgOff + 1,
                                                      nCurLpn,
                                                      1,
                                                      NULL,
                                                      pstWBObj->pMainBuf);
                        if (nErr != FSR_STL_SUCCESS)
                        {
                            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                                (TEXT("[WBM:ERR] %s() L(%d) - _WriteAlignedDataPages(nLpn=%d) (0x%x)\r\n"),
                                __FSR_FUNC__, __LINE__, nCurLpn, nErr));
                            break;
                        }

                        /* Increase head page offset */
                        pstWBObj->nHeadPgOff += 1;
                    }
                }
                else
                {
                    /* FIXME: Unaligned sector deletes must be gathered */
                    /* Now Ignore delete operation for unaligned sectors, if async delete */
                    FSR_DBZ_RTLMOUT(FSR_DBZ_INF | FSR_DBZ_STL_WBM,
                        (TEXT("[WBM:ERR] %s() L(%d) - Unaligned sector will be ignored.\r\n"),
                        __FSR_FUNC__, __LINE__));
                }
            }
            else
            {
                /* Delete mapping info */
                nErr = _DeleteMapItem(pstWBObj,
                                      nCurLpn,
                                      &bDeleted);
                if (nErr != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                        (TEXT("[WBM:ERR] %s() L(%d) - _DeleteMapItem() (0x%x)\r\n"),
                        __FSR_FUNC__, __LINE__, nErr));
                    break;
                }

                /* Decide to write delete page or just add to delete list */
                if (bDeleted == TRUE32)
                {
                    bWriteDelPage = TRUE32;
                }
                else
                {
                    /* Check deleted list */
                    if (pstWBObj->nDeletedListOff == WB_MAX_DELLIST)
                    {
                        /* Too many deleted pages. Write del page. */
                        bWriteDelPage = TRUE32;
                    }
                    else
                    {
                        for (nIdx = 0; nIdx < pstWBObj->nDeletedListOff; nIdx++)
                        {
                            if (WB_GET_DELLIST(nIdx) == nCurLpn)
                            {
                                /* LPN has been already deleted during flush */
                                bWriteDelPage = TRUE32;
                                break;
                            }
                        }
                    }
                }
            }

            /* Set variables */
            nCurLpn++;
            nCurLsnOff = 0;
            nRemainingScts -= nDeleteScts;

            nErr = FSR_STL_SUCCESS;
        }

        if (nErr != FSR_STL_SUCCESS)
        {
            break;
        }

        /* 2. If delete sync, write delete page */
        if (bSyncMode == TRUE32 && bWriteDelPage == TRUE32)
        {
            /* Move head pointer & write delete page*/
            if (((UINT32) pstWBObj->nHeadPgOff + 1) == pstWBObj->nPgsPerBlk)
            {
                /* Allocate new block */
                nErr = _AllocateNewBlk(nVol, nPart);
                if (nErr != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                        (TEXT("[WBM:ERR] %s() L(%d) - _AllocateNewBlk() (0x%x)\r\n"),
                        __FSR_FUNC__, __LINE__, nErr));
                    break;
                }

                /* In _AllocateNewBlk(), the delete page has already been written.
                   We do not need to re-write delete page */
                pstWBObj->nHeadBlkOff = (BADDR) WB_GET_NEXT_BLK(pstWBObj->nHeadBlkOff);
                pstWBObj->nHeadPgOff = 1;
            }
            else
            {
                /* Write delete page */
                nErr = _WriteDelPage(nVol, 
                                     nPart, 
                                     pstWBObj->nHeadBlkOff, 
                                     pstWBObj->nHeadPgOff + 1);
                if (nErr != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                        (TEXT("[WBM:ERR] %s() L(%d) - _WriteDelPage(nVol=%d, nPart=%d, nBlkOff=%d, nPgOff=%d) (0x%x)\r\n"),
                        __FSR_FUNC__, __LINE__, nVol, nPart, pstWBObj->nHeadBlkOff, pstWBObj->nHeadPgOff, nErr));
                    break;
                }

                /* Increase head page offset */
                pstWBObj->nHeadPgOff += 1;
            }
        }
#ifdef WB_AUTO_FLUSHING
        /* Auto flushing - flush one page before WB is full*/
        else
        {
            if (((pstWBObj->nHeadBlkOff > pstWBObj->nTailBlkOff) &&
                 (pstWBObj->nHeadBlkOff - pstWBObj->nTailBlkOff) > (pstWBObj->nNumTotalBlks - WB_MIN_BLKS_FOR_AUTO_FLUSHING)) ||
                ((pstWBObj->nHeadBlkOff < pstWBObj->nTailBlkOff) &&
                 (pstWBObj->nTailBlkOff - pstWBObj->nHeadBlkOff) < (WB_MIN_BLKS_FOR_AUTO_FLUSHING)))
            {
                nErr = _FlushBlk(nVol, nPart, 1);
                if (nErr != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                        (TEXT("[WBM:ERR] %s() L(%d) - _FlushBlk(nVol=%d, nPart=%d) (0x%x)\r\n"),
                        __FSR_FUNC__, __LINE__, nVol, nPart, nErr));
                    break;
                }
            }
        }
#endif

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}


/**
 * @brief       This function counts valid sectors in write buffer.
 *
 * @param[in]   nVol            : Volume ID
 * @param[in]   nPart           : Partition ID offset (0~7)
 * @param[in]   nLsn            : Start logical sector number
 * @param[in]   nNumOfScts      : # of sectors
 * @param[in]   pnNumOfSctsInWB : # of sectors exists in write buffer
 *
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_ERR_PARAM
 * @return      FSR_STL_ERR_PHYSICAL
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PUBLIC INT32
FSR_STL_IsInWB  (UINT32         nVol,
                 UINT32         nPart,
                 UINT32         nLsn,
                 UINT32         nNumOfScts,
                 UINT32        *pnNumOfSctsInWB)
{
    STLWBObj           *pstWBObj;
    PADDR               nCurLpn;
    UINT32              nCurLsnOff;
    UINT32              nRemainingScts;
    UINT32              nCheckScts;
    BADDR               nBlkOff;
    POFFSET             nPgOff;
    INT32               nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Get write buffer object */
    pstWBObj = gstSTLPartObj[nVol][nPart].pstWBObj;

    do
    {
        /* Initialize output parameter */
        *pnNumOfSctsInWB = 0;

        /* Initialize local variables, Pgs and Scts */
        nCurLpn        = nLsn >> pstWBObj->nSctsPerPgSft;
        nCurLsnOff     = nLsn - (nCurLpn << pstWBObj->nSctsPerPgSft);
        nRemainingScts = nNumOfScts;

        while (nRemainingScts > 0)
        {
            nCheckScts = (pstWBObj->nSctsPerPg - nCurLsnOff) < nRemainingScts ? 
                         (pstWBObj->nSctsPerPg - nCurLsnOff) : nRemainingScts;

            /* Search mapping info for written data */
            nErr = _FindMapItem(pstWBObj,
                                nCurLpn,
                                &nBlkOff,
                                &nPgOff);
            if (nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - _FindMapItem() (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nErr));
                break;
            }

            /* Check whether the sectors exist in WB */
            if (nBlkOff != WB_NULL_BOFFSET && nPgOff != WB_NULL_POFFSET)
            {
                *pnNumOfSctsInWB += nCheckScts;
            }

            /* Set variables */
            nCurLpn++;
            nCurLsnOff = 0;
            nRemainingScts -= nCheckScts;
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}


/**
 * @brief       This function flushes sectors from write buffer.
 *
 * @param[in]   nVol        : Volume ID
 * @param[in]   nPart       : Partition ID offset (0~7)
 * @param[in]   nFreeRatio  : Flushing ratio (If zero, flush one page.)
 * @param[in]   nFlag       : Flags
 *
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_ERR_PARAM
 * @return      FSR_STL_ERR_PHYSICAL
 * @author      Seunghyun Han
 * @version     1.1.0
 *
 */
PUBLIC INT32
FSR_STL_FlushWB (UINT32     nVol,
                 UINT32     nPart,
                 UINT32     nFreeRatio,
                 UINT32     nFlag)
{
    STLWBObj           *pstWBObj;
    UINT32              nCurNumFreeBlk;
    UINT32              nTargetNumFreeBlk;
    UINT32              nIdx;
    INT32               nErr        = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* Get write buffer object */
    pstWBObj = gstSTLPartObj[nVol][nPart].pstWBObj;

    do
    {
        /* If nFreeRatio == 0, it means one page flush */
        if (nFreeRatio == 0)
        {
            nErr = _FlushBlk(nVol, nPart, 1);
            if (nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                    (TEXT("[WBM:ERR] %s() L(%d) - _FlushBlk(nVol=%d, nPart=%d) (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nVol, nPart, nErr));
                break;
            }

            break;
        }
        else
        {
            /* Calculate the number of free blocks */
            nCurNumFreeBlk = pstWBObj->nTailBlkOff > pstWBObj->nHeadBlkOff ?
                             pstWBObj->nTailBlkOff - pstWBObj->nHeadBlkOff - 1 :
                             pstWBObj->nNumTotalBlks + pstWBObj->nTailBlkOff - pstWBObj->nHeadBlkOff - 1;
            nTargetNumFreeBlk = pstWBObj->nNumTotalBlks * 100 / nFreeRatio;

            /* Flush blocks */
            if (nTargetNumFreeBlk > nCurNumFreeBlk)
            {
                for (nIdx = 0; nIdx < (nTargetNumFreeBlk - nCurNumFreeBlk); nIdx++)
                {
                    nErr = _FlushBlk(nVol, nPart, 0);
                    if (nErr != FSR_STL_SUCCESS)
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                            (TEXT("[WBM:ERR] %s() L(%d) - _FlushBlk() (0x%x)\r\n"),
                            __FSR_FUNC__, __LINE__, nErr));
                        break;
                    }
                }
            }
        }

        nErr = FSR_STL_SUCCESS;

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_STL_WBM,
        (TEXT("[WBM:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return nErr;
}

#endif  /* if (OP_SUPPORT_WRITE_BUFFER == 1) */

