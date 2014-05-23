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
 *  @file       FSR_STL_Interface.c
 *  @brief      This file contains the routine for FSR_STL Interface
 *              such as FSR_STL_Read, FSR_STL_Write etc.
 *  @author     SongHo Yoon
 *  @date       10-MAY-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  10-MAY-2007 [SongHo Yoon] : first writing
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
/* Local type defines                                                        */
/*****************************************************************************/

/*****************************************************************************/
/* Global variable definitions                                               */
/*****************************************************************************/
PUBLIC STLPartObj      gstSTLPartObj[FSR_MAX_VOLS][FSR_MAX_STL_PARTITIONS];

/*****************************************************************************/
/* the static variable definitions                                           */
/*****************************************************************************/
PRIVATE BOOL32          gbSTLInitFlag = FALSE32;
PRIVATE UINT32          gnaSTLOpenCnt[FSR_MAX_VOLS];

/*****************************************************************************/
/* Imported variable declarations                                            */
/*****************************************************************************/

/*****************************************************************************/
/* Imported function prototype                                               */
/*****************************************************************************/

/*****************************************************************************/
/* Local macro                                                               */
/*****************************************************************************/
#define CHECK_INIT_STATE()                          \
        if (gbSTLInitFlag != TRUE32)                \
        {                                           \
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,   \
                (TEXT("[SIF:ERR] FSR_STL_Init() Not be called\r\n")));  \
            nErr = FSR_STL_CRITICAL_ERROR;          \
            break;                                  \
        }

#define CHECK_VOLUME_ID(nVol)                       \
        if (nVol >= FSR_MAX_VOLS)                   \
        {                                           \
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,   \
                (TEXT("[SIF:ERR] Invalid volume (Vol %d) - FSR_STL_MAX_VOLUME (%d)\r\n"), nVol, FSR_MAX_VOLS)); \
            nErr = FSR_STL_INVALID_VOLUME_ID;       \
            break;                                  \
        }

#define CHECK_PARTITION_ID(nPart)                   \
        if (nPart < FSR_PARTID_STL0 || nPart > FSR_PARTID_STL7)         \
        {                                           \
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,   \
                (TEXT("[SIF:ERR] Invalid Partition ID (PartID %d) - FSR_STL_PARTID (%d-%d)\r\n"), nPart, FSR_PARTID_STL0, FSR_PARTID_STL7)); \
            nErr = FSR_STL_INVALID_PARTITION_ID;    \
            break;                                  \
        }

#define CHECK_PARTITION_OPEN(pstSTLPartObj, nPart)  \
        if (pstSTLPartObj->nOpenCnt == 0)           \
        {                                           \
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,   \
                (TEXT("[SIF:ERR] The partition (PartID %d) is not opened\r\n"), nPart)); \
            nErr = FSR_STL_PARTITION_NOT_OPENED;    \
            break;                                  \
        }

#define CHECK_READ_ONLY_PARTITION(pstSTLPart, nPart)    \
        if ((pstSTLPart->pst1stPart->nOpenFlag & FSR_STL_FLAG_RO_PARTITION) != 0)   \
        {                                           \
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG, \
                (TEXT("[SIF:ERR] The partition (PartID %d) can read only\r\nClose all partition and reopen in order to change partition attribute\r\n"), nPart)); \
            nErr = FSR_STL_PARTITION_READ_ONLY;     \
            break;                                  \
        }

#define CHECK_LOCKED_PARTITION(pstSTLPart, nPart)   \
        if ((pstSTLPart->pst1stPart->nOpenFlag & FSR_STL_FLAG_LOCK_PARTITION) != 0)   \
        {                                           \
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG, \
                (TEXT("[SIF:ERR] The partition (PartID %d) is locked by STL\r\n"), nPart)); \
            nErr = FSR_STL_PARTITION_LOCKED;        \
            break;                                  \
        }

#define CHECK_BUFFER_NULL(pBuf)                     \
        if (pBuf == NULL)                           \
        {                                           \
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG, \
                (TEXT("[SIF:ERR] Invalid argument (pBuf %x)\r\n"), pBuf)); \
            nErr = FSR_STL_INVALID_PARAM;           \
            break;                                  \
        }


/*****************************************************************************/
/* Wear-leveling trigger point                                               */
/*****************************************************************************/
/* We will change this constant to some calculation result according to 
   the endurance of each device */
#define FSR_STL_WEARLEVEL_THRESHOLD     (2000)


/*****************************************************************************/
/* Global Wear-leveling                                                      */
/*****************************************************************************/
#define GLOBAL_WL_MAX_GRP               (2)
#define GLOBAL_WL_GRP_MASK              (FSR_BML_PI_ATTR_STL_GLOBAL_WL_GRP0 | FSR_BML_PI_ATTR_STL_GLOBAL_WL_GRP1)
#define GET_GLOBAL_WL_GRP(nBMLAttr)     (((nBMLAttr) & GLOBAL_WL_GRP_MASK) >> 6)


/*****************************************************************************/
/* Local constant definitions                                                */
/*****************************************************************************/

/*****************************************************************************/
/* Local (static) function prototype                                         */
/*****************************************************************************/
PRIVATE INT32   _SetPartInfo        (STLPartObj     *pstSTLPart,
                                     const UINT32    nSctsPerPg);
PRIVATE INT32   _SetSTLEnv          (const UINT32    nVol,
                                     UINT32          nPartID);

/*****************************************************************************/
/* Local (static)  Function Definition                                       */
/*****************************************************************************/
/**
 * @brief           This function sets STL partition information
   @n               by BML partition information
 *
 * @param[in]       pstSTLPart      : STL partition object
 * @param[in]       nSctsPerPg      : The # of sectors per page
 *
 * @return          FSR_BML_SUCCESS
 *
 * @author          SongHo Yoon
 * @version         1.0.0
 *
 */
PRIVATE INT32
_SetPartInfo   (STLPartObj     *pstSTLPart,
                const UINT32    nSctsPerPg)
{
    UINT32  nMaxBlks    = MAX_BLKS_PER_ZONE_2KF;
    UINT32  nNumZone;
    UINT32  nBitZone;
    UINT32  nBitCnt;
    INT32   nErr = FSR_BML_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    do 
    {
        /* Set up STL Partition Information */
        switch (nSctsPerPg)
        {
        case 4:         /* 2KB page */
            nMaxBlks = MAX_BLKS_PER_ZONE_2KF;
            break;
        case 8:         /* 4KB page */
            nMaxBlks = MAX_BLKS_PER_ZONE_4KF;
            break;
        case 16:        /* 8KB page */
            nMaxBlks = MAX_BLKS_PER_ZONE_8KF;
            break;
        case 32:        /* 16KB page */
            nMaxBlks = MAX_BLKS_PER_ZONE_16KF;
            break;
        default:
            nErr = FSR_STL_PARTITION_ERROR;
        }
        if (nErr != FSR_BML_SUCCESS)
        {
            break;
        }

        /* Adjust the number of zone to power of 2 */
        nNumZone = ((pstSTLPart->nMaxUnit - 1) / nMaxBlks);
        FSR_ASSERT(nNumZone < MAX_ZONE);
        nBitCnt  = 0x09;    /* 256 */
        nBitZone = 0x0100;  /* 256 */
        while (nBitCnt > 0)
        {
            if ((nNumZone & nBitZone) != 0)
            {
                break;
            }
            nBitCnt--;
            nBitZone >>= 1;
        }

        nBitZone = (nBitZone == 0) ? 1 : nBitZone << 1;

        pstSTLPart->nNumZone = nBitZone;
        pstSTLPart->nZoneMsk = nBitZone - 1;
        pstSTLPart->nZoneSft = nBitCnt;

        FSR_ASSERT(pstSTLPart->nNumZone <= MAX_ZONE);
        if (pstSTLPart->nNumZone > MAX_ZONE)
        {
            nErr = FSR_STL_PARTITION_ERROR;
            break;
        }

    } while (0);
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nErr));
    return nErr;
}


/**
 * @brief           This function sets STL environment by BML partition information
 *
 * @param[in]       nVol       : BML volume number
 *
 * @return          NONE
 *
 * @author          SongHo Yoon
 * @version         1.0.0
 *
 */
PRIVATE INT32
_SetSTLEnv     (const UINT32    nVol,UINT32      nPartID)
{
    RBWDevInfo         *pstDev;
    STLPartObj         *pstSTLPart;
    STLPartObj         *pst1stPart      = NULL;
    FSRVolSpec          stVolSpec;
    FSRPartEntry        staPartEntry[FSR_MAX_STL_PARTITIONS];
    UINT32              na1stVpn    [FSR_MAX_STL_PARTITIONS];
    UINT32              naPgsPerUnit[FSR_MAX_STL_PARTITIONS];
    UINT32              naGWGrpID   [FSR_MAX_STL_PARTITIONS];
    UINT32              nClstID;
    UINT32              nZoneID;
    UINT32              nPart;
    UINT32              nPartIdx;
    UINT32              nStartPart;
    UINT32              nNumPart;
    UINT32              nAdjUnit;
    BOOL32              bGlobalWL;
    BOOL32              baGlobalWL[GLOBAL_WL_MAX_GRP];
    BOOL32              bNextClst;
    UINT32              nGWGrp;
    UINT32              nCurGWGrp;
    INT32               nErr            = FSR_BML_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%d)\r\n"), __FSR_FUNC__, nVol));

    do
    {
        if (gnaSTLOpenCnt[nVol] != 0)
        {
            /* Update the recent partition attribute */
            nPart = nPartID - FSR_PARTID_STL0;            
            pstSTLPart = &(gstSTLPartObj[nVol][nPartID - FSR_PARTID_STL0]);

            nErr = FSR_BML_LoadPIEntry(nVol,                    /* Volume ID                    */
                                       nPartID,                 /* Partition ID                 */
                                      &(na1stVpn    [nPart]),   /* The first VPN                */
                                      &(naPgsPerUnit[nPart]),   /* the number of pages per unit */
                                      &(staPartEntry[nPart]));  /* Partition Information        */
            if (nErr != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                    (TEXT("[SIF:ERR] FSR_BML_LoadPIEntry error (nBMLRe=0x%x)\r\n"), nErr));
                FSR_ASSERT(nErr != FSR_BML_VOLUME_NOT_OPENED);
                FSR_ASSERT(nErr != FSR_BML_INVALID_PARAM);
                nErr = FSR_STL_INVALID_PARTITION_ID;
                break;
            }

            if (((staPartEntry[nPart].nAttr) &
                (FSR_BML_PI_ATTR_RO | FSR_BML_PI_ATTR_LOCK | FSR_BML_PI_ATTR_LOCKTIGHTEN)) != 0)
            {
                pstSTLPart->pst1stPart->nOpenFlag = FSR_STL_FLAG_RO_PARTITION;
            }
            else
            {
                pstSTLPart->pst1stPart->nOpenFlag = FSR_STL_FLAG_RW_PARTITION;
            }
            break;
        }

        /* 1. Get Volume Information */
        nErr = FSR_BML_GetVolSpec(nVol,                 /* Volume ID                    */
                                 &stVolSpec,            /* Volume Information           */
                                  FSR_BML_FLAG_NONE);   /* Must be FSR_BML_FLAG_NONE    */
        if (nErr != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                (TEXT("[SIF:ERR] FSR_BML_GetVolSpec error (nBMLRe=0x%x)\r\n"), nErr));
            FSR_ASSERT(nErr != FSR_BML_INVALID_PARAM);
            break;
        }

        /* 2. Get all STL partition information */
        FSR_OAM_MEMSET(naGWGrpID, 0xFF, sizeof (naGWGrpID));
        for (nPart = 0; nPart < FSR_MAX_STL_PARTITIONS; nPart++)
        {
            /* Get BML partition information */
            nErr = FSR_BML_LoadPIEntry(nVol,                    /* Volume ID                    */
                                       nPart + FSR_PARTID_STL0, /* Partition ID                 */
                                      &(na1stVpn    [nPart]),   /* The first VPN                */
                                      &(naPgsPerUnit[nPart]),   /* the number of pages per unit */
                                      &(staPartEntry[nPart]));  /* Partition Information        */
            if (nErr == FSR_BML_NO_PIENTRY)
            {
                continue;
            }
            if (nErr != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                    (TEXT("[SIF:ERR] FSR_BML_LoadPIEntry error (nBMLRe=0x%x)\r\n"), nErr));
                FSR_ASSERT(nErr != FSR_BML_VOLUME_NOT_OPENED);
                FSR_ASSERT(nErr != FSR_BML_INVALID_PARAM);
                break;
            }

            /* Get global wear-leveling group ID */
            nGWGrp = GET_GLOBAL_WL_GRP(staPartEntry[nPart].nAttr);
            if (nGWGrp > GLOBAL_WL_MAX_GRP)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                    (TEXT("[SIF:ERR] Wrong Global Wear-leveling Group ID (BML Attribute 0x%x)\r\n"), staPartEntry[nPart].nAttr));
                nErr = FSR_BML_INVALID_PARTITION | FSR_BML_INVALID_GLOBAL_WL;
                break;
            }

            /* The partition which participate the global wear-leveling must have write capability */
            if (nGWGrp > 0)
            {
                if (((staPartEntry[nPart].nAttr) &
                     (FSR_BML_PI_ATTR_RO | FSR_BML_PI_ATTR_LOCK | FSR_BML_PI_ATTR_LOCKTIGHTEN))
                    != 0)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                        (TEXT("[SIF:ERR] stPartEntry.nAttr has read only (nAttr=0x%x)\r\n"), staPartEntry[nPart].nAttr));
                    nErr = FSR_BML_INVALID_PARTITION | FSR_BML_INVALID_GLOBAL_WL;
                    break;
                }
            }

            /* 
               The partition attribute need be set FSR_BML_PI_ATTR_STL for STL 
               Otherwise, Warning message is given to user.
            */
            if (!((staPartEntry[nPart].nAttr & FSR_BML_PI_ATTR_STL) == FSR_BML_PI_ATTR_STL))
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_WARN | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                    (TEXT("[SIF:INF] stPartEntry.nAttr has no FSR_BML_PI_ATTR_STL\r\n")));
                continue;
            }

            FSR_ASSERT(staPartEntry[nPart].nID == nPart + FSR_PARTID_STL0);
            naGWGrpID[nPart] = nGWGrp;
        }

        /* If there is any error, return right now */
        if ((nPart != FSR_MAX_STL_PARTITIONS) && (nErr != FSR_BML_SUCCESS))
        {
            break;
        }

        /* Initialize boolean flags */
        FSR_OAM_MEMSET(baGlobalWL, 0, sizeof (baGlobalWL));
        bGlobalWL = FALSE32;
        bNextClst = FALSE32;

        /* 3. Set STL partition object and device information object */
        nClstID    = nVol * FSR_MAX_STL_PARTITIONS;
        nZoneID    = 0;
        nPart      = 0;
        nStartPart = 0;
        while(nPart < FSR_MAX_STL_PARTITIONS)
        {
            /* Get global wear-leveling group ID */
            nGWGrp = naGWGrpID[nPart];
            if (nGWGrp == (UINT32)(-1))
            {
                nPart++;
                nErr = FSR_BML_SUCCESS;
                continue;
            }

            /* The partition attribute has global wear-leveling feature */
            if (nGWGrp > 0)
            {
                nCurGWGrp = nGWGrp - 1;

                /* There is the duplicated global wear-leveling ID */
                if (bGlobalWL == FALSE32 && baGlobalWL[nCurGWGrp] == TRUE32)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                        (TEXT("[SIF:ERR] Wrong Global Wear-leveling Group ID (BML Attribute 0x%x)\r\n"), staPartEntry[nPart].nAttr));
                    nErr = FSR_BML_INVALID_PARTITION | FSR_BML_INVALID_GLOBAL_WL;
                    break;
                }
                else if (bGlobalWL == TRUE32 && baGlobalWL[nCurGWGrp] == TRUE32)
                {
                    /* Chain of global wear-leveling group.
                       Nothing to do. Just keep going */
                }
                else
                {
                    /* New start of global wear-leveling group */
                    baGlobalWL[nCurGWGrp] = TRUE32;
                    bGlobalWL = TRUE32;

                    bNextClst = TRUE32;
                }
            }
            else
            {
                /* Set global wear-leveling flags */
                bGlobalWL = FALSE32;

                bNextClst = TRUE32;
            }

            if (bNextClst == TRUE32)
            {
                /* Get the partition object of the 1st zone in the same cluster */
                nStartPart = nPart;
                pst1stPart = &(gstSTLPartObj[nVol][nStartPart]);

                /* Set the next nClstID and nZoneID */
                if (nPart != 0) 
                {
                    nClstID++;
                    nZoneID = 0;
                }

                bNextClst = FALSE32;
            }

            /* Get STL partition object */
            pstSTLPart = &(gstSTLPartObj[nVol][nPart]);

            /* Set up STL partition information */
            pstSTLPart->nVolID                      = nVol;
            pstSTLPart->nPart                       = nPart;
            pstSTLPart->nClstID                     = nClstID;
            pstSTLPart->nZoneID                     = nZoneID;
            pstSTLPart->pst1stPart                  = pst1stPart;
            pstSTLPart->nSM                         = (UINT32) -1;

            if (((staPartEntry[nStartPart].nAttr) &
                 (FSR_BML_PI_ATTR_RO | FSR_BML_PI_ATTR_LOCK | FSR_BML_PI_ATTR_LOCKTIGHTEN))
                != 0)
            {
                pstSTLPart->nOpenFlag               = FSR_STL_FLAG_RO_PARTITION;
            }
            else
            {
                pstSTLPart->nOpenFlag               = FSR_STL_FLAG_RW_PARTITION;
            }

            pstSTLPart->stClstEV.nStartVbn          = staPartEntry[nStartPart].n1stVun;
            pstSTLPart->stClstEV.nStartVpn          = na1stVpn    [nStartPart];
            pstSTLPart->stClstEV.nECDiffThreshold   = stVolSpec.nMLCPECycle;

#if (OP_SUPPORT_WRITE_BUFFER == 1)
            pstSTLPart->pstWBObj                    = NULL;
#endif

#if (OP_SUPPORT_HOT_DATA_DETECTION == 1)
            pstSTLPart->pLRUT                       = NULL;
            pstSTLPart->nLRUTSize                   = 0;
            pstSTLPart->nDecayCnt                   = 0;
            pstSTLPart->nWriteCnt                   = 0;
            pstSTLPart->nHitCnt                     = 0;
            pstSTLPart->nLSBPos                     = 0;
#endif

#if (OP_SUPPORT_STATISTICS_INFO == 1)
            pstSTLPart->nSTLLastECnt                = 0;
            pstSTLPart->nSTLRdScts                  = 0;
            pstSTLPart->nSTLWrScts                  = 0;
            pstSTLPart->nSTLDelScts                 = 0;
#endif

            /* Temporarily set variables. Will be fixed up with accurate values */
            pstSTLPart->nMaxUnit                    = staPartEntry[nPart].nNumOfUnits;
            pstSTLPart->nNumPart                    = 1;

            /* Set up the number of zones of the partition */
            nErr = _SetPartInfo(pstSTLPart, stVolSpec.nSctsPerPg);
            if (nErr != FSR_BML_SUCCESS)
            {
                break;
            }

            nZoneID += pstSTLPart->nNumZone;
            if (nZoneID > MAX_ZONE)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                    (TEXT("[SIF:ERR] Partition is too big!\r\n")));
                nErr = FSR_STL_PARTITION_ERROR;
                break;
            }

            /* Get device information object */
            pstDev = &(pstSTLPart->stClstEV.stDevInfo);

            /* Set device information object */
            if ((staPartEntry[nStartPart].nAttr & FSR_BML_PI_ATTR_SLC) == FSR_BML_PI_ATTR_SLC)
            {
                pstDev->nDeviceType         = RBW_DEVTYPE_SLC;
            }
            else if ((staPartEntry[nStartPart].nAttr & FSR_BML_PI_ATTR_MLC) == FSR_BML_PI_ATTR_MLC)
            {
                pstDev->nDeviceType         = RBW_DEVTYPE_MLC;

                /* CAUTION : The value depends on device.
                   For example, The value of 4Gb and 8Gb/16Gb MLC are different */
                pstDev->nLSBModeBranchPgOffset  = stVolSpec.nNumOfWays << 2;    /* multiple 4 */
            }

            if (stVolSpec.nNANDType == FSR_LLD_SLC_ONENAND)
            {
                pstDev->nDeviceType         = RBW_DEVTYPE_ONENAND;
            }

            pstDev->nNumWays                = stVolSpec.nNumOfWays;
            pstDev->nNumWaysShift           = FSR_STL_GetShiftBit(pstDev->nNumWays);

            pstDev->nSecPerVPg              = stVolSpec.nSctsPerPg;     /*  2-plane considered */
            pstDev->nSecPerVPgShift         = FSR_STL_GetShiftBit(pstDev->nSecPerVPg);

            pstDev->nPagesPerSBlk           = naPgsPerUnit[nStartPart]; /*  SBLK in ATL = Virtual Unit in BML */
            pstDev->nPagesPerSbShift        = FSR_STL_GetShiftBit(pstDev->nPagesPerSBlk);

            pstDev->nSecPerSBlk             = 1 << (pstDev->nPagesPerSbShift + pstDev->nSecPerVPgShift);
            pstDev->nBytesPerVPg            = 1 << (BYTES_SECTOR_SHIFT       + pstDev->nSecPerVPgShift);

            if (pstDev->nSecPerVPg == 32)
            {
                pstDev->nFullSBitmapPerVPg  = 0xFFFFFFFF;
            }
            else if (pstDev->nSecPerVPg < 32)
            {
                pstDev->nFullSBitmapPerVPg  = (1 << pstDev->nSecPerVPg) - 1;
            }
            else
            {
                nErr = FSR_STL_CRITICAL_ERROR;
                break;
            }

            /* Go to the next partition */
            nPart++;

            /* Set up the rest of STL partition information in the end of the loop */
            if ((nPart == FSR_MAX_STL_PARTITIONS) ||
                (bGlobalWL == FALSE32) ||
                (bGlobalWL == TRUE32 && nPart < FSR_MAX_STL_PARTITIONS && nGWGrp != naGWGrpID[nPart]))
            {
                /* Adjust the max unit in each zone for Root Unit (RU) */
                FSR_ASSERT(MAX_ROOT_BLKS > 0);
                nNumPart   = nPart - nStartPart;
                nAdjUnit   = (UINT32)((MAX_ROOT_BLKS - 1) / nNumPart);
                pstSTLPart = pst1stPart;
                for (nPartIdx = 0; nPartIdx < nNumPart; nPartIdx++)
                {
                    pstSTLPart->nNumPart  = nNumPart;
                    pstSTLPart->nMaxUnit -= nAdjUnit;
                    pstSTLPart++;
                }
                pst1stPart->nMaxUnit += (nAdjUnit * nNumPart);
            }
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nErr));
    return nErr;
}


/*****************************************************************************/
/* Global Function Definition                                                */
/*****************************************************************************/

/**
 * @brief       This function sets STL environment by BML partition information
 *
 * @param[in]   pstSTLPart  : STL partition object
 * @param[in]   nLsn        : Logical Sector Number
 * @param[out]  nZoneIdx    : Zone index
 *
 * @return      Lsn of the zone
 *
 * @author      SongHo Yoon & Seung-hyun Han
 * @version     1.1.0
 *
 */
PUBLIC UINT32
FSR_STL_GetZoneLsn    (STLPartObj     *pstSTLPart,
                       const UINT32    nLsn,
                             UINT32   *nZoneIdx)
{
    UINT32      nLbn;
    UINT32      nZoneLsn   = nLsn;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    FSR_ASSERT(pstSTLPart           != NULL);
    FSR_ASSERT(pstSTLPart->nOpenCnt >  0   );
    FSR_ASSERT(pstSTLPart->nNumZone != 0   );

    do
    {
        if (pstSTLPart->nNumZone == 1)
        {
            (*nZoneIdx) = 0;
            break;
        }

        nLbn        = nLsn >> pstSTLPart->nBlkSft;
        (*nZoneIdx) = nLbn & pstSTLPart->nZoneMsk;
        nZoneLsn = ((nLbn >> pstSTLPart->nZoneSft) << pstSTLPart->nBlkSft) | (nLsn & pstSTLPart->nBlkMsk);

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nZoneLsn));
    return nZoneLsn;
}


/**
 * @brief       This function initializes the data structure of STL and calls FSR_BML_Init
 * 
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_ALREADY_INITIALIZED
 * @return      FSR_BML_ALREADY_INITIALIZED
 * @return      FSR_LLD_ERROR
 *
 * @author      SongHo Yoon
 * @version     1.0.0
 *
 */
PUBLIC INT32
FSR_STL_Init (VOID)
{
    /* local variable definitions */
    STLPartObj *pstSTLPart;
    UINT32      nVolIdx;
    UINT32      nPartIdx;
    INT32       nErr        = FSR_STL_SUCCESS;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    do
    {
        /* Check FSR_STL_Init() is first called */
        if (gbSTLInitFlag == TRUE32)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_WARN | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                (TEXT("[SIF:   ] FSR_STL_Init is already called\r\n")));
            nErr = FSR_STL_ALREADY_INITIALIZED;
            break;
        }

        /* Check data structure size
           sizeof (STLBMTEntry) must 2 bytes */
        if (sizeof (STLBMTEntry) != 2)
        {
            nErr = FSR_STL_CRITICAL_ERROR;
            break;
        }

        /* BML Initialization*/
        nErr = FSR_BML_Init(FSR_BML_FLAG_NONE);
        if ((nErr != FSR_BML_SUCCESS) && (nErr != FSR_BML_ALREADY_INITIALIZED))
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                (TEXT("[SIF:ERR] FSR_BML_Init Fail(%x)\r\n"), nErr));
            break;
        }

        /* Initialize global data */
        FSR_OAM_MEMSET(gstSTLPartObj, 0xFF, sizeof (gstSTLPartObj));
        for (nVolIdx = 0; nVolIdx < FSR_MAX_VOLS; nVolIdx++)
        {
            gnaSTLOpenCnt[nVolIdx] = 0;
            for (nPartIdx = 0; nPartIdx < FSR_MAX_STL_PARTITIONS; nPartIdx++)
            {
                pstSTLPart = &(gstSTLPartObj[nVolIdx][nPartIdx]);
                pstSTLPart->nOpenCnt   = 0;
                pstSTLPart->nSM        = (UINT32)-1;
                pstSTLPart->pst1stPart = NULL;
            }
        }

        /* STL_Init Success */
        gbSTLInitFlag = TRUE32;
        nErr = FSR_STL_SUCCESS;

        /* function implementation end */
    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nErr));
    return nErr;
}


/**
 * @brief       This function formats the partition for STL
 * 
 * @param[in]   nVol       : Volume number to open
 * @param[in]   nPartID    : Partition ID number to open, 
 * @n                        which must be from FSR_PARTID_STL0 to FSR_PARTID_STL7
 * @param[in]   pstStlFmt  : FSR_STL_FORMAT_HOT_SPOT      - Enabling Hot-spot static mapping
 * @n                        FSR_STL_FORMAT_REMEMBER_ECNT - Keep Erase Count
 *
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_INVALID_PARAM
 * @return      FSR_STL_INVALID_VOLUME_ID
 * @return      FSR_STL_INVALID_PARTITION_ID
 * @return      FSR_STL_PARTITION_ALREADY_OPENED
 * @return      FSR_STL_PARTITION_TOO_MANY_OPENED
 * @return      FSR_STL_CRITICAL_ERROR 
 * @return      FSR_STL_UNFORMATTED
 * @return      FSR_BML_VOLUME_NOT_OPENED
 * @return      FSR_BML_INVALID_PARAM
 * @return      FSR_BML_WR_PROTECT_ERROR
 * @return      FSR_BML_WRITE_ERROR
 * @return      FSR_BML_ERASE_ERROR
 * @return      FSR_BML_ACQUIRE_SM_ERROR
 * @return      FSR_BML_RELEASE_SM_ERROR
 * @return      FSR_BML_CRITICAL_ERROR
 *
 * @author      SongHo Yoon & Seung-hyun Han
 * @version     1.1.0
 *
 */
PUBLIC INT32
FSR_STL_Format (UINT32          nVol,
                UINT32          nPartID,
                FSRStlFmtInfo  *pstStlFmt)
{
    STLPartObj         *pstSTLPartObj;
    STLPartObj         *pstTmpSTLPart;
    STLClstObj         *pstClst;
    BOOL32              bBMLOpened  = FALSE32;
    UINT16              nZoneBlks[MAX_ZONE];
    UINT32              nClst;
    UINT32              nZone;
    UINT32              nZoneIdx;
    UINT32              nPart;
    UINT32              nNumPart;
    UINT32              nZoneUnit;
    UINT32              nAdjUnit;
    UINT32              nNumRoot;
    STLFmtInfo          stFmt;
    INT32               nBMLErr;
    INT32               nErr        = FSR_STL_CRITICAL_ERROR;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    do
    {
        /* Check validity of arguments          */
        CHECK_INIT_STATE();
        /* Check the boundary of Volume ID      */
        CHECK_VOLUME_ID(nVol);
        /* Check the boundary of Partition ID   */
        CHECK_PARTITION_ID(nPartID);
        /* Check validity of pstSTLInfo         */
        CHECK_BUFFER_NULL(pstStlFmt);

        /* Open BML */
        nErr = FSR_BML_Open(nVol, FSR_BML_FLAG_NONE);
        if (nErr != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                (TEXT("[SIF:ERR] BML_Open Fail (Vol %d) : Err(%x)\r\n"), nVol, nErr));
            FSR_ASSERT(nErr != FSR_BML_INVALID_PARAM);
            break;
        }
        bBMLOpened  = TRUE32;

        /* Get volume and all partition information */
        nErr = _SetSTLEnv(nVol,nPartID);
        if (nErr != FSR_BML_SUCCESS)
        {
            break;
        }

        /* Check the partition information */
        pstSTLPartObj   = &(gstSTLPartObj[nVol][nPartID - FSR_PARTID_STL0]);
        if (pstSTLPartObj->nZoneID == (UINT32)(-1))
        {
            FSR_ASSERT(pstSTLPartObj->nClstID == (UINT32)(-1));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                (TEXT("[SIF:ERR] Invalid Partition ID (PartID %d)\r\n"), nPartID));
            nErr = FSR_STL_INVALID_PARTITION_ID;
            break;
        }

        pstTmpSTLPart = pstSTLPartObj->pst1stPart;

        if (pstTmpSTLPart != NULL)
        {
            /* Check whether the partition is master zone */
            if (pstSTLPartObj != pstTmpSTLPart)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                    (TEXT("[SIF:ERR] To format the partition[%d], format using the first partition in the same Global Wear-Leveling group\r\n"), nPartID));
                nErr = FSR_STL_PROHIBIT_FORMAT_BY_GWL;
                break;
            }

            /* Find the open partition */
            pstTmpSTLPart = pstSTLPartObj->pst1stPart;
            nNumPart      = pstTmpSTLPart->nNumPart;
            for (nPart = 0; nPart < nNumPart; nPart++)
            {
                FSR_ASSERT(pstTmpSTLPart->nClstID == pstSTLPartObj->nClstID);
                FSR_ASSERT(pstTmpSTLPart->nNumZone > 0);

                if (pstTmpSTLPart->nOpenCnt > 0)
                {
                    break;
                }
                pstTmpSTLPart++;
            }

            /* Check whether the partition is opened */
            if (nPart < nNumPart)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                    (TEXT("[SIF:ERR] Some partition is opened.\r\n           To format the partition[%d], close all partition in the same Global Wear-Leveling group\r\n"), nPartID));
                nErr = FSR_STL_PARTITION_ALREADY_OPENED;
                break;
            }
        }

        /* Check whether the partition is read only */
        CHECK_READ_ONLY_PARTITION(pstSTLPartObj, nPartID);

        /* Set STL format information */
        if ((pstStlFmt->nOpt & FSR_STL_FORMAT_USE_ECNT) != 0)           /* remember count */
        {
            stFmt.nAvgECnt = pstStlFmt->nAvgECnt;
            stFmt.nNumECnt = pstStlFmt->nNumOfECnt;
            stFmt.pnECnt   = pstStlFmt->pnECnt;
        }
        else if ((pstStlFmt->nOpt & FSR_STL_FORMAT_REMEMBER_ECNT) != 0) /* average count */
        {
            stFmt.nAvgECnt = pstStlFmt->nAvgECnt;
            stFmt.nNumECnt = 0;
            stFmt.pnECnt   = NULL;
        }
        else
        {
            stFmt.nAvgECnt = 0;
            stFmt.nNumECnt = 0;
            stFmt.pnECnt   = NULL;
        }

        /* Initialize the cluster which belongs to the partition */
        nErr    = FSR_STL_InitCluster(pstSTLPartObj->nClstID,   /* Cluster ID               */
                                      nVol,                     /* Volume ID                */
                                      nPartID - FSR_PARTID_STL0,/* Partition index          */
                                      RBW_OPMODE_SYNC,          /* Must be RBW_OPMODE_SYNC  */
                                      NULL);                    /* Must be NULL             */
        if (nErr != FSR_STL_SUCCESS)
        {
            break;
        }

        /* Set Cluster environment variable */
        pstClst = FSR_STL_GetClstObj(pstSTLPartObj->nClstID);
        pstClst->pstEnvVar = &(pstSTLPartObj->stClstEV);

        FSR_ASSERT(pstSTLPartObj->pst1stPart->nClstID == pstSTLPartObj->nClstID);
        FSR_ASSERT(pstSTLPartObj->pst1stPart->nZoneID == 0);

        /* Set the number of units of each zone in the cluster */
        nClst         = pstSTLPartObj->nClstID;
        nZone         = 0;
        pstTmpSTLPart = pstSTLPartObj->pst1stPart;
        nNumPart      = pstSTLPartObj->nNumPart;

        /* Adjust the max unit in each zone for Root Unit (RU) */
        FSR_ASSERT(MAX_ROOT_BLKS > 0);
        nNumRoot = MAX_ROOT_BLKS;
        nZone    = 0;
        for (nPart = 0; nPart < nNumPart; nPart++)
        {
            FSR_ASSERT(pstTmpSTLPart->nClstID == nClst);
            FSR_ASSERT(pstTmpSTLPart->nNumZone > 0);

            nZoneUnit = (UINT32)((pstTmpSTLPart->nMaxUnit - nNumRoot) >> pstTmpSTLPart->nZoneSft);
            nAdjUnit  = (UINT32)((pstTmpSTLPart->nMaxUnit - nNumRoot) &  pstTmpSTLPart->nZoneMsk);
            for (nZoneIdx = 0; nZoneIdx < pstTmpSTLPart->nNumZone; nZoneIdx++)
            {
                nZoneBlks[nZone] = (UINT16)(nZoneUnit + nNumRoot);
                nNumRoot         = 0;
                if (nAdjUnit > 0)
                {
                    nZoneBlks[nZone]++;
                    nAdjUnit--;
                }
                nZone++;
            }
            FSR_ASSERT(nAdjUnit == 0);

            pstTmpSTLPart++;
        }
        FSR_ASSERT(nZone > 0);

        /* Format the cluster */
        nErr    = FSR_STL_FormatCluster(nClst,      /* Cluster ID           */
                                        0,          /* The start VBN        */
                                        nZone,      /* The number of Zone   */
                                        nZoneBlks,  /* The number of blks   */
                                       &stFmt);     /* Format Information   */

        /* We must close the cluster to free memory */
        FSR_STL_CloseCluster(nClst);
        if (nErr != FSR_STL_SUCCESS)
        {
            break;
        }

#if (OP_SUPPORT_WRITE_BUFFER == 1)
        /* Format write buffers, if exist */
        pstTmpSTLPart = pstSTLPartObj->pst1stPart;
        for (nPart = 0; nPart < nNumPart; nPart++)
        {
            nErr = FSR_STL_FormatWB(pstTmpSTLPart->nVolID, pstTmpSTLPart->nPart);
            if (nErr != FSR_BML_NO_PIENTRY && nErr != FSR_STL_SUCCESS)
            {
                /* Format is failed. */
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF,
                        (TEXT("[SIF:ERR] FSR_STL_FormatWB (nVol=%d, nPart=%d) fail! : (0x%x)\r\n"),
                        pstTmpSTLPart->nVolID, pstTmpSTLPart->nPart, nErr));
                break;
            }
            pstTmpSTLPart++;
        }

        if (nPart != nNumPart)
        {
            break;
        }
#endif

        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
            (TEXT("[SIF:INF] Partition[%d"), nPartID));
        for (nPart = 1; nPart < nNumPart; nPart++)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                (TEXT(", %d"), nPartID + nPart));
        }
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
            (TEXT("] format is success%s\r\n"), (nNumPart > 1) ? " with Global Wear-Leveling." : "."));

        nErr = FSR_STL_SUCCESS;
    } while (0);

    if (bBMLOpened == TRUE32)
    {
        /* Close BML to exit */
        nBMLErr = FSR_BML_Close(nVol, FSR_BML_FLAG_NONE);
        if (nBMLErr != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                (TEXT("[SIF:ERR] FSR_BML_Close (Vol %d) fail! : %x\r\n"), nVol, nErr));
            FSR_ASSERT(nErr != FSR_BML_VOLUME_NOT_OPENED);
            FSR_ASSERT(nErr != FSR_BML_INVALID_PARAM);
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nErr));
    return nErr;
}


/**
 * @brief       This function opens a partition in volume for access
 *
 * @param[in]     nVol       : Volume number to open
 * @param[in]     nPartID    : Partition ID number to open
 * @param[in,out] pstSTLInfo : STL related information return structure
 * @param[in]     nFlag      : FSR_STL_FLAG_DEFAULT or FSR_STL_FLAG_OPEN_READONLY
 *
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_INVALID_PARAM
 * @return      FSR_STL_INVALID_VOLUME_ID
 * @return      FSR_STL_INVALID_PARTITION_ID
 * @return      FSR_STL_PARTITION_ALREADY_OPENED
 * @return      FSR_STL_PARTITION_TOO_MANY_OPENED
 * @return      FSR_STL_CRITICAL_ERROR 
 * @return      FSR_STL_UNFORMATTED
 * @return      FSR_BML_VOLUME_NOT_OPENED
 * @return      FSR_BML_INVALID_PARAM
 * @return      FSR_BML_UNFORMATTED
 *
 * @author      SongHo Yoon & Seung-hyun Han
 * @version     1.1.0
 *
 */
PUBLIC INT32
FSR_STL_Open   (UINT32      nVol,
                UINT32      nPartID,
                FSRStlInfo *pstSTLInfo,
                UINT32      nFlag)
{
    STLPartObj         *pstSTLPartObj;
    STLPartObj         *pstTmpSTLPart;
    STLClstObj         *pstClst;
    RBWDevInfo         *pstDI;
    UINT32              nPart;
    UINT32              nNumPart;
    UINT32              nZone;
    UINT32              nNumZone;
    BOOL32              bBMLOpened  = FALSE32;
    BOOL32              bAlreadyOpened = FALSE32;
    BOOL32              bRet;
    INT32               nBMLErr;
    INT32               nErr        = FSR_STL_CRITICAL_ERROR;
    UINT32              nTmpOpenFlag=0; /* Used to keep the original open flag */    
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    do
    {
        /* Check validity of arguments          */
        CHECK_INIT_STATE();
        /* Check the boundary of Volume ID      */
        CHECK_VOLUME_ID(nVol);
        /* Check the boundary of Partition ID   */
        CHECK_PARTITION_ID(nPartID);
        /* Check validity of pstSTLInfo         */
        CHECK_BUFFER_NULL(pstSTLInfo);

        /* Open BML */
        nErr = FSR_BML_Open(nVol, FSR_BML_FLAG_NONE);
        if (nErr != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                (TEXT("[SIF:ERR] BML_Open Fail (Vol %d) : Err(%x)\r\n"), nVol, nErr));
            FSR_ASSERT(nErr != FSR_BML_INVALID_PARAM);
            break;
        }
        bBMLOpened = TRUE32;

        /* Get volume and partitions information */
        nErr= _SetSTLEnv(nVol,nPartID);
        if (nErr != FSR_BML_SUCCESS)
        {
            break;
        }

        /* Get STL partition object */
        pstSTLPartObj   = &(gstSTLPartObj[nVol][nPartID - FSR_PARTID_STL0]);
        if (pstSTLPartObj->nZoneID == (UINT32)(-1))
        {
            FSR_ASSERT(pstSTLPartObj->nClstID == (UINT32)(-1));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                (TEXT("[SIF:ERR] Invalid Partition ID (PartID %d)\r\n"), nPartID));
            nErr = FSR_STL_INVALID_PARTITION_ID;
            break;
        }

        /* 
        * The attribute of partition to be changed should be FSR_STL_FLAG_OPEN_READWRITE or
        * FSR_STL_FLAG_OPEN_READONLY or FSR_STL_FLAG_DEFAULT.
        */
        if ((nFlag != FSR_STL_FLAG_OPEN_READONLY) && 
            (nFlag != FSR_STL_FLAG_OPEN_READWRITE) &&
            (nFlag != FSR_STL_FLAG_DEFAULT))
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                (TEXT("[SIF:ERR] Invalid argument (OpenFlag %x)\r\n"),nFlag));
            nErr = FSR_STL_INVALID_PARAM;
            break;
        }

        if ((nFlag == FSR_STL_FLAG_OPEN_READONLY) && 
            (pstSTLPartObj->nNumPart > 1))
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                (TEXT("[SIF:ERR] A global-wear-leveling partition can not be opened as Read Only\r\n"),
                nFlag));
            nErr = FSR_STL_INVALID_PARAM;
            break;
        }

        /* Find the open partition and the number of zone, nNumZone */
        pstTmpSTLPart = pstSTLPartObj->pst1stPart;
        nNumPart      = pstTmpSTLPart->nNumPart;
        nNumZone      = 0;
        bAlreadyOpened = FALSE32;
        for (nPart = 0; nPart < nNumPart; nPart++)
        {
            FSR_ASSERT(pstTmpSTLPart->nClstID == pstSTLPartObj->nClstID);
            FSR_ASSERT(pstTmpSTLPart->nNumZone > 0);

            if (pstTmpSTLPart->nOpenCnt > 0)
            {
                bAlreadyOpened = TRUE32;
            }
            nNumZone += pstTmpSTLPart->nNumZone;
            pstTmpSTLPart++;
        }

        /* Partition is already opened */
        if (bAlreadyOpened == TRUE32)
        {
            /* Get the partition object of the 1st zone in the same cluster */
            pstTmpSTLPart = pstSTLPartObj->pst1stPart;
            FSR_ASSERT(pstTmpSTLPart->nSM != (UINT32)(-1));

            /* Get cluster object */
            pstClst = FSR_STL_GetClstObj(pstSTLPartObj->nClstID);
            FSR_ASSERT(pstClst != NULL);

            /* Set partition information */
            pstDI = pstClst->pstDevInfo;
            pstSTLInfo->nLogSctsPerPage = pstDI->nSecPerVPg;
            pstSTLInfo->nLogSctsPerUnit = pstDI->nSecPerVPg << pstDI->nPagesPerSbShift;
            pstSTLInfo->nTotalLogScts   = 0;
            pstSTLInfo->nTotalLogUnits  = 0;
            for (nZone = 0; nZone < pstSTLPartObj->nNumZone; nZone++)
            {
                pstSTLInfo->nTotalLogScts  += pstClst->stZoneObj[pstSTLPartObj->nZoneID + nZone].pstZI->nNumUserScts;
                pstSTLInfo->nTotalLogUnits += pstClst->stZoneObj[pstSTLPartObj->nZoneID + nZone].pstZI->nNumUserBlks;
            }

            if (pstSTLPartObj->nOpenCnt == 0)
            {
                /* Increase open counter */
                gnaSTLOpenCnt[nVol]++;

                /* the partition open is success */
                pstSTLPartObj->nOpenCnt++;
                nErr = FSR_STL_SUCCESS;
            }
            else
            {
                nErr = FSR_STL_PARTITION_ALREADY_OPENED;
            }

            break;
        }

        /* Create the semaphore handle for nDevIdx */
        bRet = FSR_OAM_CreateSM(&(pstSTLPartObj->pst1stPart->nSM), FSR_OAM_SM_TYPE_STL);
        if (bRet == FALSE32)
        {
            nErr = FSR_STL_SEMAPHORE_ERROR;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                (TEXT("[BIF:ERR]  FSR_OAM_CreateSM Error\r\n")));
            break;
        }

        /* Initialize cluster */
        nErr    = FSR_STL_InitCluster(pstSTLPartObj->nClstID,   /* Cluster ID               */
                                      nVol,                     /* Volume ID                */
                                      nPartID - FSR_PARTID_STL0,/* Partition index          */
                                      RBW_OPMODE_SYNC,          /* Must be RBW_OPMODE_SYNC  */
                                      NULL);                    /* Must be NULL             */
        if (nErr != FSR_STL_SUCCESS)
        {
            break;
        }

        /* Set Cluster environment variable */
        pstClst = FSR_STL_GetClstObj(pstSTLPartObj->nClstID);

#if (OP_SUPPORT_MSB_PAGE_WAIT == 1)
        FSR_OAM_MEMSET(pstClst->baMSBProg, FALSE32, sizeof(pstClst->baMSBProg));
#endif

        pstTmpSTLPart = pstSTLPartObj->pst1stPart;
        pstClst->pstEnvVar = &(pstSTLPartObj->stClstEV);
        pstClst->pstEnvVar->nECDiffThreshold = (FSR_STL_WEARLEVEL_THRESHOLD >> 1); /* FIXME */
        pstDI = pstClst->pstDevInfo;

        /* Set the initial flag of partitino attribute to be recovered before returning */
        if ((nFlag & FSR_STL_FLAG_OPEN_READWRITE) != 0)
        {
            nTmpOpenFlag  |= FSR_STL_FLAG_RW_PARTITION;
        }
        else if ((nFlag & FSR_STL_FLAG_OPEN_READONLY) != 0)
        {
            nTmpOpenFlag  |= FSR_STL_FLAG_RO_PARTITION;
        }
        else
        {
            nTmpOpenFlag  = pstTmpSTLPart->nOpenFlag;
        }

        /* Keep the initial flag of the partition */
        if (pstTmpSTLPart->nOpenCnt == 0)
        {
            pstTmpSTLPart->nInitOpenFlag = pstTmpSTLPart->nOpenFlag;
        }

        /* To make the partition writable, change the partition as BML RW */
        if ((pstTmpSTLPart->nOpenFlag & FSR_STL_FLAG_RW_PARTITION) == 0)
        {
            nErr = FSR_STL_ChangePartAttr(nVol,nPartID,FSR_STL_FLAG_OPEN_READWRITE);
            if (nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
                               (TEXT("[SIF:INF] --%s() L(%d): Unable to change the Partition attribute\r\n"),
                               __FSR_FUNC__, __LINE__, nErr));
            }
        }

        /* Open the all zone in the cluster */
        nErr = FSR_STL_OpenCluster(pstSTLPartObj->nClstID,
                                   nNumZone,
                                   pstSTLPartObj->nOpenFlag);
        if (nErr != FSR_STL_SUCCESS)
        {
            /* for error handling */
            FSR_STL_CloseCluster(pstSTLPartObj->nClstID);

            /* Destroy the semaphore handle for nVDevNo */
            FSR_OAM_DestroySM(pstSTLPartObj->pst1stPart->nSM, FSR_OAM_SM_TYPE_STL);
            pstSTLPartObj->pst1stPart->nSM = (UINT32)-1;
            break;
        }

        /* 
        * If the original OpenFlag has FSR_STL_FLAG_OPEN_READONLY,
        * then changes the attribute to RO.
        * The current state of the partition is FSR_STL_FLAG_OPEN_READWRITE.
        */
        if ((nTmpOpenFlag & FSR_STL_FLAG_RO_PARTITION) != 0)
        {
            nErr = FSR_STL_ChangePartAttr(nVol,nPartID,FSR_STL_FLAG_OPEN_READONLY);
            if (nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
                               (TEXT("[SIF:INF] --%s() L(%d): Unable to change the Partition attribute\r\n"),
                               __FSR_FUNC__, __LINE__, nErr));
            }
            pstTmpSTLPart->nOpenFlag = nTmpOpenFlag;
        }

        /* Disable to change STL partition attribute */
        FSR_BML_SetPartAttrChg(nVol,nPartID,TRUE32,FSR_STL_FLAG_DEFAULT);

        /* Get STL information */
        pstTmpSTLPart = pstSTLPartObj->pst1stPart;
        for (nPart = 0; nPart < nNumPart; nPart++)
        {
            pstTmpSTLPart->nBlkSft = pstDI->nPagesPerSbShift + pstDI->nSecPerVPgShift;
            pstTmpSTLPart->nBlkMsk = (1 << pstTmpSTLPart->nBlkSft) - 1;
            pstTmpSTLPart++;
        }

        pstSTLInfo->nLogSctsPerPage = pstDI->nSecPerVPg;
        pstSTLInfo->nLogSctsPerUnit = pstDI->nSecPerVPg << pstDI->nPagesPerSbShift;
        pstSTLInfo->nTotalLogScts   = 0;
        pstSTLInfo->nTotalLogUnits  = 0;
        for (nZone = 0; nZone < pstSTLPartObj->nNumZone; nZone++)
        {
            pstSTLInfo->nTotalLogScts  += pstClst->stZoneObj[pstSTLPartObj->nZoneID + nZone].pstZI->nNumUserScts;
            pstSTLInfo->nTotalLogUnits += pstClst->stZoneObj[pstSTLPartObj->nZoneID + nZone].pstZI->nNumUserBlks;
        }

        /* Increase open counter */
        gnaSTLOpenCnt[nVol]++;

        /* the partition open is success */
        pstSTLPartObj->nOpenCnt++;

#if (OP_SUPPORT_WRITE_BUFFER == 1)
        /* Open write buffers */
        pstTmpSTLPart = pstSTLPartObj->pst1stPart;
        for (nPart = 0; nPart < nNumPart; nPart++)
        {
            nErr = FSR_STL_OpenWB(pstTmpSTLPart->nVolID, pstTmpSTLPart->nPart, nFlag);
            if (nErr != FSR_BML_NO_PIENTRY && nErr != FSR_STL_SUCCESS)
            {
                pstTmpSTLPart->pstWBObj = NULL;
                break;
            }

#if (OP_SUPPORT_HOT_DATA_DETECTION == 1)
            if (nErr == FSR_STL_SUCCESS)
            {
                pstTmpSTLPart->nDecayCnt = DEFAULT_DECAY_PERIOD;
                pstTmpSTLPart->nWriteCnt = 0;
                pstTmpSTLPart->nHitCnt   = 0;
                pstTmpSTLPart->nLSBPos   = 0;
                pstTmpSTLPart->nLRUTSize = ((pstTmpSTLPart->pstWBObj->nNumTotalPgsM & 0x1F) != 0) ?
                                            (pstTmpSTLPart->pstWBObj->nNumTotalPgsM >> 5) * 2 + 2 :
                                            (pstTmpSTLPart->pstWBObj->nNumTotalPgsM >> 4);
                pstTmpSTLPart->pLRUT     = (UINT32*) FSR_STL_MALLOC(pstTmpSTLPart->nLRUTSize * sizeof(UINT32),
                                                     FSR_STL_MEM_CACHEABLE, FSR_STL_MEM_DRAM);
                if (pstTmpSTLPart->pLRUT == NULL)
                {
                    nErr = FSR_STL_OUT_OF_MEMORY;
                    break;
                }
                else if (((UINT32)(pstTmpSTLPart->pLRUT))       & 0x03)
                {
                    nErr = FSR_OAM_NOT_ALIGNED_MEMPTR;
                    break;
                }

                FSR_OAM_MEMSET((UINT8*) pstTmpSTLPart->pLRUT, 0x00, 
                               pstTmpSTLPart->nLRUTSize * sizeof(UINT32));
            }
#endif
            pstTmpSTLPart++;
        }

        if (nErr != FSR_BML_NO_PIENTRY && nErr != FSR_STL_SUCCESS)
        {
            break;
        }
#endif

        nErr = FSR_STL_SUCCESS;
    } while (0);

    if (nErr == FSR_STL_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                       (TEXT("[SIF:INF] Partition [%d, %d] open is success\r\n"),
                nVol, nPartID));
    }
    else
    {
        if (bBMLOpened == TRUE32)
        {
            /* Close BML to exit */
            nBMLErr = FSR_BML_Close(nVol, FSR_BML_FLAG_NONE);
            if (nBMLErr != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                               (TEXT("[SIF:ERR] FSR_BML_Close (Vol %d) fail! : %x\r\n"), nVol, nErr));
                FSR_ASSERT(nErr != FSR_BML_VOLUME_NOT_OPENED);
                FSR_ASSERT(nErr != FSR_BML_INVALID_PARAM);
            }
        }

        if (nErr != FSR_STL_PARTITION_ALREADY_OPENED)
        {
            FSR_MAMMOTH_POWEROFF();
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nErr));
    return  nErr;
}


/**
 * @brief       This function closes the partition in volume
 *
 * @param[in]   nVol       : Volume number to close
 * @param[in]   nPartID    : Partition ID number to close
 *
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_INVALID_PARAM
 * @return      FSR_STL_INVALID_VOLUME_ID
 * @return      FSR_STL_INVALID_PARTITION_ID
 * @return      FSR_STL_PARTITION_NOT_OPENED
 * @return      FSR_STL_CRITICAL_ERROR 
 * @return      FSR_BML_VOLUME_NOT_OPENED
 * @return      FSR_BML_INVALID_PARAM 
 * @return      FSR_BML_CRITICAL_ERROR
 *
 * @author      SongHo Yoon & Seung-hyun Han
 * @version     1.1.0
 *
 */
PUBLIC INT32
FSR_STL_Close  (UINT32          nVol,
                UINT32          nPartID)
{
    STLPartObj         *pstSTLPartObj;
    STLPartObj         *pstTmpPartObj;

    UINT32              nZone;
    UINT32              nPart;
    UINT32              nNumPart;
    BOOL32              bRet;
    INT32               nErr        = FSR_STL_CRITICAL_ERROR;

    INT32               nBMLErr;
    FSRChangePA         stChangePA;   /* Used to call the BML_IOCTL */
    UINT32              nInitOpenFlag=0; /* Used to keep the Initial open flag */

    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    do
    {
        /* Check validity of arguments          */
        CHECK_INIT_STATE();
        /* Check the boundary of Volume ID      */
        CHECK_VOLUME_ID(nVol);
        /* Check the boundary of Partition ID   */
        CHECK_PARTITION_ID(nPartID);

        /* Get STL Partition Object             */
        pstSTLPartObj   = &(gstSTLPartObj[nVol][nPartID - FSR_PARTID_STL0]);

        /* Mark close flag of the partition object */
#if defined (FSR_POR_USING_LONGJMP)
        if (pstSTLPartObj->nOpenCnt > 0)
        {
            pstSTLPartObj->nOpenCnt--;
        }
#else
        /* Check which the partition is opened  */
        CHECK_PARTITION_OPEN(pstSTLPartObj, nPartID);
        pstSTLPartObj->nOpenCnt--;
#endif

        /* Check whether the all zone is closed */
        pstTmpPartObj = pstSTLPartObj->pst1stPart;
        nInitOpenFlag = pstTmpPartObj->nInitOpenFlag;
        nNumPart      = pstTmpPartObj->nNumPart;

        for (nPart = 0; nPart < nNumPart; nPart++)
        {
            if (pstTmpPartObj->nOpenCnt > 0)
            {
                break;
            }
            pstTmpPartObj++;
        }

        /* All zone is closed, so, close cluster */
        if (nPart == nNumPart)
        {
            /* Acquire semaphore */
            bRet = FSR_OAM_AcquireSM(pstSTLPartObj->pst1stPart->nSM, FSR_OAM_SM_TYPE_STL);
            if (bRet == FALSE32)
            {
                nErr = FSR_STL_ACQUIRE_SM_ERROR;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  Acquiring semaphore is failed.\r\n")));
            }

#if (OP_SUPPORT_WRITE_BUFFER == 1)
            /* Close write buffers */
            pstTmpPartObj = pstSTLPartObj->pst1stPart;
            for (nPart = 0; nPart < nNumPart; nPart++)
            {
                /* Close write buffer */
                if (pstTmpPartObj->pstWBObj != NULL)
                {
                    pstTmpPartObj->nOpenCnt++;  /* To write in FSR_STL_CloseWB */
                    FSR_STL_CloseWB(pstTmpPartObj->nVolID, pstTmpPartObj->nPart);
                    pstTmpPartObj->pstWBObj = NULL;
                    pstTmpPartObj->nOpenCnt--;
                }

#if (OP_SUPPORT_HOT_DATA_DETECTION == 1)
                /* Free memory for LRU table */
                if (pstTmpPartObj->pLRUT != NULL)
                {
                    FSR_OAM_Free(pstTmpPartObj->pLRUT);
                    pstTmpPartObj->pLRUT = NULL;
                }
#endif
                pstTmpPartObj++;
            }
#endif
            
            /* Enable to change STL partition attribute */
            FSR_BML_SetPartAttrChg(nVol,nPartID,FALSE32,FSR_STL_FLAG_DEFAULT);

            pstTmpPartObj = pstSTLPartObj->pst1stPart;

             /* 
             * To make the partition writable, change the partition as BML RW 
             * It doesn't iterate to consider the global wear-leveling partition,
             * since the attribute of every global-wear-leveling partition is BML RW.
             * So, we just changes the partition attribute of the first partition.
             */
            if ((pstTmpPartObj->nOpenFlag & FSR_STL_FLAG_RW_PARTITION) == 0)
            {
                /* To make the partition writable, change the partition as BML RW */
                nErr = FSR_STL_ChangePartAttr(nVol,nPartID,FSR_STL_FLAG_OPEN_READWRITE);
                if (nErr != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]   %s(nVol: %d, nCode: FSR_STL_IOCTL_CHANGE_PART_ATTR, nRe:0x%x)\r\n"),
                    __FSR_FUNC__, nVol, bRet));
                }
            }

            /* Close all zones */
            for (nPart = 0; nPart < nNumPart; nPart++)
            {
                for (nZone = 0; nZone < pstTmpPartObj->nNumZone; nZone++)
                {
                    /* Close the zone */
                    nErr = FSR_STL_CloseZone(pstTmpPartObj->nClstID,            /* Cluster ID   */
                                             pstTmpPartObj->nZoneID + nZone);   /* Zone ID      */
                    if (nErr != FSR_STL_SUCCESS)
                    {
                        break;
                    }
                }

                if (nErr != FSR_STL_SUCCESS)
                {
                    break;
                }
                pstTmpPartObj++;
            }

            if (nErr != FSR_STL_SUCCESS)
            {
                break;
            }

            /* Close cluster */
            nErr = FSR_STL_CloseCluster(pstSTLPartObj->nClstID);
            if (nErr != FSR_STL_SUCCESS)
            {
                break;
            }

            pstTmpPartObj = pstSTLPartObj->pst1stPart;

            /* 
            * If the original OpenFlag has FSR_STL_FLAG_OPEN_READONLY,
            * then changes the attribute to RO.
            * The current state of the partition is FSR_STL_FLAG_OPEN_READWRITE.
            */
            if ((nInitOpenFlag & FSR_STL_FLAG_RO_PARTITION) != 0)
            {
                /** Step 1. Check if the partition has only FSR_STL_FLAG_RW_PARTITION */
                FSR_ASSERT((pstTmpPartObj->nOpenFlag & FSR_STL_FLAG_RW_PARTITION) !=0);
                FSR_ASSERT((pstTmpPartObj->nOpenFlag & FSR_STL_FLAG_RO_PARTITION) ==0);

                /**
                 * Step 2. If the partition is not FSR_STL_FLAG_RO_PARTITION, 
                 * then call BML_IOCTL to set the BML partition info as BML RO
                 */
                stChangePA.nPartID  = nPartID;
                stChangePA.nNewAttr = FSR_BML_PI_ATTR_RO | FSR_BML_PI_ATTR_LOCK;

                nBMLErr = FSR_BML_IOCtl(nVol,
                               FSR_BML_IOCTL_CHANGE_PART_ATTR,
                               (UINT8 *) &stChangePA,
                               sizeof(stChangePA),
                               NULL,
                               0,
                               NULL);
                if (nBMLErr != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR] --%s() L(%d): FSR_BML_IOCTL_CHANGE_PART_ATTR 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nErr));
                }
            }

            /* Step 3. Set nOpenFlag as FSR_STL_FLAG_RO_PARTITION */
            pstTmpPartObj->nOpenFlag = nInitOpenFlag;

            /* Destroy the semaphore handle for nVDevNo */
            FSR_OAM_DestroySM(pstTmpPartObj->nSM, FSR_OAM_SM_TYPE_STL);
            pstTmpPartObj->nSM = (UINT32)-1;
        }

        /* Decrease open counter */
#if defined (FSR_POR_USING_LONGJMP)
        if (gnaSTLOpenCnt[nVol] > 0)
        {
            gnaSTLOpenCnt[nVol]--;
        }
#else
        FSR_ASSERT(gnaSTLOpenCnt[nVol] > 0);
        gnaSTLOpenCnt[nVol]--;
#endif

        /* Close BML to exit */
        nErr = FSR_BML_Close(nVol, FSR_BML_FLAG_NONE);
        if (nErr != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                (TEXT("[SIF:ERR] FSR_BML_Close (Vol %d) fail! : %x\r\n"), nVol, nErr));
            FSR_ASSERT(nErr != FSR_BML_INVALID_PARAM);
            break;
        }

        nErr = FSR_STL_SUCCESS;
    } while (0);

#if defined (FSR_POR_USING_LONGJMP)
    
    if (nErr == FSR_STL_PARTITION_NOT_OPENED)
    {
        FSR_BML_Close(nVol, FSR_BML_FLAG_NONE);        
    }

#endif

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nErr));
    return  nErr;
}

/**
 * @brief       This function reads sectors
 * 
 * @param[in]   nVol       : Volume number
 * @param[in]   nPartID    : Partition ID number
 * @param[in]   nLsn       : Start Lsn for reading
 * @param[in]   nNumOfScts : The number of sectors to read
 * @param[in]   pBuf       : Buffer for reading
 * @param[in]   nFlag      : must be FSR_STL_FLAG_DEFAULT
 * 
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_INVALID_PARAM
 * @return      FSR_STL_INVALID_VOLUME_ID
 * @return      FSR_STL_INVALID_PARTITION_ID
 * @return      FSR_STL_PARTITION_NOT_OPENED
 * @return      FSR_STL_USERDATA_ERROR
 * @return      FSR_STL_CRITICAL_ERROR 
 * @return      FSR_BML_VOLUME_NOT_OPENED
 * @return      FSR_BML_INVALID_PARAM
 * @return      FSR_BML_READ_ERROR
 * @return      FSR_BML_CRITICAL_ERROR
 * @return      FSR_BML_ACQUIRE_SM_ERROR
 * @return      FSR_BML_RELEASE_SM_ERROR
 *
 * @author      SongHo Yoon & Seung-hyun Han
 * @version     1.1.0
 *
 */
PUBLIC INT32
FSR_STL_Read   (UINT32          nVol,
                UINT32          nPartID,
                UINT32          nLsn,
                UINT32          nNumOfScts,
                UINT8          *pBuf,
                UINT32          nFlag)
{
    STLPartObj         *pstSTLPartObj;
    SM32                nSM;
    BOOL32              bRet;
    UINT32              nZoneLsn;
    UINT32              nZone;
    UINT32              nScts;
#if (OP_SUPPORT_WRITE_BUFFER == 1)
    UINT32              nNumOfSctsInWB;
    UINT32              nOriLsn;
    UINT32              nOriNumOfScts;
    UINT8              *pOriBuf;
#endif
    INT32               nErr        = FSR_STL_INVALID_PARAM;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%d,%d,%d,%d,%x,%x)\r\n"),
            __FSR_FUNC__, nVol, nPartID, nLsn, nNumOfScts, pBuf, nFlag));

    do
    {
        /* Check validity of arguments          */
        CHECK_INIT_STATE();
        /* Check the boundary of Volume ID      */
        CHECK_VOLUME_ID(nVol);
        /* Check the boundary of Partition ID   */
        CHECK_PARTITION_ID(nPartID);
        /* Check validity of pBuf               */
        CHECK_BUFFER_NULL(pBuf);

        /* Get STL Partition Object             */
        pstSTLPartObj   = &(gstSTLPartObj[nVol][nPartID - FSR_PARTID_STL0]);

        /* Check which the partition is opened  */
        CHECK_PARTITION_OPEN(pstSTLPartObj, nPartID);

        nSM  = pstSTLPartObj->pst1stPart->nSM;

        /* Acquire a semaphore */
        if ((nFlag & FSR_STL_FLAG_USE_SM) != 0)
        {
            nSM  = pstSTLPartObj->pst1stPart->nSM;
            bRet = FSR_OAM_AcquireSM(nSM, FSR_OAM_SM_TYPE_STL);
            if (bRet == FALSE32)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  Acquiring semaphore is failed.\r\n")));
                nErr = FSR_STL_ACQUIRE_SM_ERROR;
                break;
            }
        }

#if (OP_SUPPORT_STATISTICS_INFO == 1)
        pstSTLPartObj->nSTLRdScts += nNumOfScts;
#endif

#if (OP_SUPPORT_WRITE_BUFFER == 1)
        /* Check how many sectors are in WB */
        nNumOfSctsInWB = 0;
        nOriLsn         = nLsn;
        nOriNumOfScts   = nNumOfScts;
        pOriBuf         = pBuf;

        if (pstSTLPartObj->pstWBObj != NULL)
        {
            nErr = FSR_STL_IsInWB(pstSTLPartObj->nVolID,
                                  pstSTLPartObj->nPart,
                                  nLsn,
                                  nNumOfScts,
                                  &nNumOfSctsInWB);
            if (nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] %s() L(%d) - FSR_STL_IsInWB(nLsn=%d, nNumOfScts=%d) (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nLsn, nNumOfScts, nErr));
                nNumOfSctsInWB = 0;
                //break;                    /* Be careful! Semaphore is locked */
            }
        }

        /* Read data from Zone */
        if ((pstSTLPartObj->pstWBObj == NULL) ||
            (pstSTLPartObj->pstWBObj != NULL && 
             nErr                    == FSR_STL_SUCCESS && 
             nNumOfSctsInWB          != nNumOfScts))
#endif
        {
            nScts = (pstSTLPartObj->nBlkMsk + 1) - (nLsn & pstSTLPartObj->nBlkMsk);
            while (nNumOfScts > 0)
            {
                if (nScts > nNumOfScts)
                {
                    nScts = nNumOfScts;
                }

                nZoneLsn = FSR_STL_GetZoneLsn(pstSTLPartObj, nLsn, &nZone);

                nErr = FSR_STL_ReadZone(pstSTLPartObj->nClstID,         /* Cluster ID               */
                                        pstSTLPartObj->nZoneID + nZone, /* Zone ID                  */
                                        nZoneLsn,               /* Start LSN for reading            */
                                        nScts,                  /* The number of sectors to read    */
                                        pBuf,                   /* Buffer for reading               */
                                        nFlag);                 /* must be FSR_STL_FLAG_DEFAULT     */
                if (nErr != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR] %s() L(%d) - FSR_STL_ReadZone(nZoneLsn=%d, nScts=%d) (0x%x)\r\n"),
                        __FSR_FUNC__, __LINE__, nZoneLsn, nScts, nErr));
                    break;
                }

                nLsn       += nScts;
                nNumOfScts -= nScts;
                pBuf       += (nScts << BYTES_SECTOR_SHIFT);

                nScts       = (pstSTLPartObj->nBlkMsk + 1);
            }
        }

#if (OP_SUPPORT_WRITE_BUFFER == 1)
        /* Read data from WB */
        if (pstSTLPartObj->pstWBObj != NULL && 
            nErr                    == FSR_STL_SUCCESS && 
            nNumOfSctsInWB          != 0)
        {
            nErr = FSR_STL_ReadWB(pstSTLPartObj->nVolID,
                                  pstSTLPartObj->nPart,
                                  nOriLsn,
                                  nOriNumOfScts,
                                  pOriBuf,
                                  nFlag);
            if (nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] %s() L(%d) - FSR_STL_ReadWB(nLsn=%d, nNumOfScts=%d) (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nLsn, nNumOfScts, nErr));
                //break;                    /* Be careful! Semaphore is locked */
            }
        }
#endif

        /* Release a semaphore */
        if ((nFlag & FSR_STL_FLAG_USE_SM) != 0)
        {
            bRet = FSR_OAM_ReleaseSM(nSM, FSR_OAM_SM_TYPE_STL);
            if (bRet == FALSE32)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  Releasing semaphore is failed.\r\n")));
                if (nErr == FSR_STL_SUCCESS)
                {
                    nErr = FSR_STL_RELEASE_SM_ERROR;
                    break;
                }
            }
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nErr));
    return  nErr;
}


/**
 * @brief       This function writes sectors
 * 
 * @param[in]   nVol       : Volume number
 * @param[in]   nPartID    : Partition ID number
 * @param[in]   nLsn       : Start Lsn for writing
 * @param[in]   nNumOfScts : The number of sectors to write
 * @param[in]   pBuf       : Buffer for writing
 * @param[in]   nFlag      : FSR_STL_FLAG_DEFAULT, FSR_STL_FLAG_WRITE_NO_LSB_BACKUP, 
 * @n                        or FSR_STL_FLAG_WRITE_NO_CONFIRM
 * 
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_INVALID_PARAM
 * @return      FSR_STL_INVALID_VOLUME_ID
 * @return      FSR_STL_INVALID_PARTITION_ID
 * @return      FSR_STL_PARTITION_NOT_OPENED
 * @return      FSR_STL_CRITICAL_ERROR 
 * @return      FSR_BML_VOLUME_NOT_OPENED
 * @return      FSR_BML_INVALID_PARAM
 * @return      FSR_BML_WR_PROTECT_ERROR
 * @return      FSR_BML_CRITICAL_ERROR
 * @return      FSR_BML_ACQUIRE_SM_ERROR
 * @return      FSR_BML_RELEASE_SM_ERROR
 *
 * @author      SongHo Yoon & Seung-hyun Han
 * @version     1.1.0
 *
 */
PUBLIC INT32
FSR_STL_Write  (UINT32          nVol,
                UINT32          nPartID,
                UINT32          nLsn,
                UINT32          nNumOfScts,
                UINT8          *pBuf,
                UINT32          nFlag)
{
    STLPartObj         *pstSTLPartObj;
    SM32                nSM;
    BOOL32              bRet;
    UINT32              nZoneLsn;
    UINT32              nZone;
    UINT32              nScts;
#if (OP_SUPPORT_WRITE_BUFFER == 1)
    UINT32              nTempLsn  = 0;
    UINT32              nTempScts = 0;
    UINT32              nSecPerPg;
    UINT32              nSecPerPgSft;
    UINT32              nNumOfSctsInWB;
    UINT32              nNumOfColdPgs;
#if (OP_SUPPORT_HOT_DATA_DETECTION == 1)
    UINT32              nCurLpn;
    UINT32              nEndLpn;
    UINT32             *pLRUT;
    UINT32              nMSBPos;
    UINT32              nLSBPos;
    UINT32              nMSB;
    UINT32              nLSB;
    UINT32              nIdx;
#endif
#endif
    INT32               nErr        = FSR_STL_INVALID_PARAM;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%d, %d, %d, %d, %x, %x)\r\n"),
            __FSR_FUNC__, nVol, nPartID, nLsn, nNumOfScts, pBuf, nFlag));

    do
    {
        /* Check validity of arguments          */
        CHECK_INIT_STATE();
        /* Check the boundary of Volume ID      */
        CHECK_VOLUME_ID(nVol);
        /* Check the boundary of Partition ID   */
        CHECK_PARTITION_ID(nPartID);
        /* Check validity of pBuf               */
        CHECK_BUFFER_NULL(pBuf);

        /* Get STL Partition Object             */
        pstSTLPartObj   = &(gstSTLPartObj[nVol][nPartID - FSR_PARTID_STL0]);

        /* Check which the partition is opened  */
        CHECK_PARTITION_OPEN(pstSTLPartObj, nPartID);

        /* Check whether the partition is read only */
        CHECK_READ_ONLY_PARTITION(pstSTLPartObj, nPartID);

        /* Check whether the partition is locked by STL */
        CHECK_LOCKED_PARTITION(pstSTLPartObj, nPartID);

        /* Check a flag for write options */
        if ((nFlag & FSR_STL_FLAG_WRITE_HOT_DATA) != 0 &&
            (nFlag & FSR_STL_FLAG_WRITE_COLD_DATA) != 0)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                (TEXT("[SIF:ERR] Flags for HOT and COLD data can not be set simultaneously.\r\n")));
            nErr = FSR_STL_INVALID_PARAM_FLAG;
            break;
        }

        nSM  = pstSTLPartObj->pst1stPart->nSM;

        /* Acquire a semaphore */
        if ((nFlag & FSR_STL_FLAG_USE_SM) != 0)
        {
            bRet = FSR_OAM_AcquireSM(nSM, FSR_OAM_SM_TYPE_STL);
            if (bRet == FALSE32)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  Acquiring semaphore is failed.\r\n")));
                nErr = FSR_STL_ACQUIRE_SM_ERROR;
                break;
            }
        }

#if (OP_SUPPORT_STATISTICS_INFO == 1)
        pstSTLPartObj->nSTLWrScts += nNumOfScts;
#endif

#if (OP_SUPPORT_WRITE_BUFFER == 1)
        if (pstSTLPartObj->pstWBObj != NULL)
        {
            nSecPerPg       = pstSTLPartObj->pstWBObj->nSctsPerPg;
            nSecPerPgSft    = pstSTLPartObj->pstWBObj->nSctsPerPgSft;

#if (OP_SUPPORT_HOT_DATA_DETECTION == 1)
            /* Run LRU for detecting hot */
            if ((nFlag & FSR_STL_FLAG_WRITE_COLD_DATA) == 0 &&
                (pstSTLPartObj->pLRUT != NULL))
            {
                nCurLpn = nLsn >> nSecPerPgSft;
                nEndLpn = (nLsn + nNumOfScts - 1) >> nSecPerPgSft;

                pLRUT   = pstSTLPartObj->pLRUT;
                nLSBPos = pstSTLPartObj->nLSBPos;
                nMSBPos = (nLSBPos == 0) ? 1 : 0;

                nNumOfColdPgs = 0;

                /* Mark referenced LPNs */
                while (nCurLpn <= nEndLpn)
                {
                    nLSB = pLRUT[((nCurLpn >> 5) * 2) + nLSBPos] & (1 << (nCurLpn & 0x1f));
                    nMSB = pLRUT[((nCurLpn >> 5) * 2) + nMSBPos] & (1 << (nCurLpn & 0x1f));

                    if (nLSB == 0 && nMSB == 0)
                    {
                        /* 00 -> 01 */
                        pLRUT[((nCurLpn >> 5) * 2) + nLSBPos] |= (1 << (nCurLpn & 0x1f));

                        /* This page has not been referenced before. */
                        nNumOfColdPgs++;
                    }
                    else
                    {
                        if (nLSB != 0 && nMSB == 0)
                        {
                            /* 01 -> 10 */
                            pLRUT[((nCurLpn >> 5) * 2) + nLSBPos] &= ~(1 << (nCurLpn & 0x1f));
                            pLRUT[((nCurLpn >> 5) * 2) + nMSBPos] |=  (1 << (nCurLpn & 0x1f));
                        }
                        else if (nLSB == 0 && nMSB != 0)
                        {
                            /* 10 -> 11 */
                            pLRUT[((nCurLpn >> 5) * 2) + nLSBPos] |=  (1 << (nCurLpn & 0x1f));
                        }
                        else
                        {
                            /* 11 -> Do nothing */
                        }

                        /* This page has been referenced before */
                        pstSTLPartObj->nHitCnt++;
                    }

                    nCurLpn++;
                    pstSTLPartObj->nWriteCnt++;
                }

                /* Check whether it is hot or cold */
                if (nNumOfColdPgs == 0)
                {
                    /* Note! If all pages are hot, then it is treated as hot data */
                    nFlag |= FSR_STL_FLAG_WRITE_HOT_DATA;
                }

                /* Decay */
                if (pstSTLPartObj->nWriteCnt > pstSTLPartObj->nDecayCnt)
                {
                    for (nIdx = nLSBPos; nIdx < pstSTLPartObj->nLRUTSize; nIdx += 2)
                    {
                        pLRUT[nIdx] = 0;
                    }

                    /* FIXME: Decay period should be decided more reasonablely */
                    if (pstSTLPartObj->nHitCnt < (pstSTLPartObj->pstWBObj->nNumTotalPgs >> 4))
                    {
                        pstSTLPartObj->nDecayCnt *= 2;
                    }
                    else if(pstSTLPartObj->nHitCnt < (pstSTLPartObj->pstWBObj->nNumTotalPgs >> 3))
                    {
                        pstSTLPartObj->nDecayCnt += DEFAULT_DECAY_PERIOD;
                    }
                    else
                    {
                        pstSTLPartObj->nDecayCnt /= 2;
                    }

                    if (pstSTLPartObj->nDecayCnt > pstSTLPartObj->pstWBObj->nNumTotalPgs)
                    {
                        pstSTLPartObj->nDecayCnt = pstSTLPartObj->pstWBObj->nNumTotalPgs;
                    }

                    pstSTLPartObj->nWriteCnt    = 0;
                    pstSTLPartObj->nHitCnt      = 0;
                    pstSTLPartObj->nLSBPos      = (nLSBPos == 0) ? 1 : 0;

                    /* Just for debugging */
                    /*for (nIdx = 0; nIdx < pstSTLPartObj->pstWBObj->nNumTotalPgsM; nIdx++)
                    {
                        if ((pLRUT[((nIdx >> 5) * 2) + nMSBPos] & (1 << (nIdx & 0x1f))) != 0)
                        {
                            FSR_DBZ_RTLMOUT(FSR_DBZ_DEFAULT,
                                (TEXT("[SIF:   ] Hot data => LSN = %6d (LPN = %6d)\r\n"),
                                nIdx << nSecPerPgSft, nIdx));
                        }
                    }
                    FSR_DBZ_RTLMOUT(FSR_DBZ_DEFAULT, (TEXT("[SIF:   ] Decay count => %6d\r\n"),
                        pstSTLPartObj->nDecayCnt));
                    */
                }
            }
#endif  /* #if (OP_SUPPORT_HOT_DATA_DETECTION == 1) */

            /* Though it is not hot, if it has been written in WB, write it into WB again */
            if ((nFlag & FSR_STL_FLAG_WRITE_COLD_DATA) == 0 &&
                (nFlag & FSR_STL_FLAG_WRITE_HOT_DATA) == 0)
            {
                nErr = FSR_STL_IsInWB(pstSTLPartObj->nVolID,
                                  pstSTLPartObj->nPart,
                                  nLsn,
                                  nNumOfScts,
                                  &nNumOfSctsInWB);
                if (nErr != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR] %s() L(%d) - FSR_STL_IsInWB(nLsn=%d, nNumOfScts=%d) (0x%x)\r\n"),
                        __FSR_FUNC__, __LINE__, nLsn, nNumOfScts, nErr));
                    nNumOfSctsInWB = 0;
                    //break;                    /* Be careful! Semaphore is locked */
                }
                else
                {
                    /* Note! If all sectors are in WB, then it is treated as hot data */
                    if (nNumOfSctsInWB == nNumOfScts)
                    {
                        nFlag |= FSR_STL_FLAG_WRITE_HOT_DATA;
                    }
                }
            }

            /* Write data to WB, if it has a flag */
            if ((nFlag & FSR_STL_FLAG_WRITE_COLD_DATA) == 0 && 
                (nFlag & FSR_STL_FLAG_WRITE_HOT_DATA) != 0)
            {
                nErr = FSR_STL_WriteWB(pstSTLPartObj->nVolID,
                                       pstSTLPartObj->nPart,
                                       nLsn,
                                       nNumOfScts,
                                       pBuf,
                                       nFlag);
                if (nErr == FSR_STL_SUCCESS)
                {
                    nLsn       += nNumOfScts;
                    nNumOfScts -= nNumOfScts;   /* nNumOfScts will be zero */
                    pBuf       += (nNumOfScts << BYTES_SECTOR_SHIFT);
                }
            }

            /* Write data to WB (Front unaligned sectors) */
            if ((nFlag & FSR_STL_FLAG_WRITE_COLD_DATA) == 0 && 
                nNumOfScts > 0 && (nLsn & (nSecPerPg - 1)) != 0)
            {
                nTempScts = (nSecPerPg - (nLsn & (nSecPerPg - 1))) < nNumOfScts ?
                            (nSecPerPg - (nLsn & (nSecPerPg - 1))) : nNumOfScts;
                nTempLsn  = nLsn;

                nErr = FSR_STL_WriteWB(pstSTLPartObj->nVolID,
                                       pstSTLPartObj->nPart,
                                       nTempLsn,
                                       nTempScts,
                                       pBuf,
                                       nFlag);
                if (nErr == FSR_STL_SUCCESS)
                {
                    nLsn       += nTempScts;
                    nNumOfScts -= nTempScts;
                    pBuf       += (nTempScts << BYTES_SECTOR_SHIFT);
                }
            }

            /* Write data to WB (Rear unaligned sectors) */
            if ((nFlag & FSR_STL_FLAG_WRITE_COLD_DATA) == 0 && 
                nNumOfScts > 0 && ((nLsn + nNumOfScts) & (nSecPerPg - 1)) != 0)
            {
                nTempScts = (nLsn + nNumOfScts) & (nSecPerPg - 1);
                nTempLsn  = (nLsn + nNumOfScts) - nTempScts;

                nErr = FSR_STL_WriteWB(pstSTLPartObj->nVolID,
                                       pstSTLPartObj->nPart,
                                       nTempLsn,
                                       nTempScts,
                                       pBuf + ((nTempLsn - nLsn) << BYTES_SECTOR_SHIFT),
                                       nFlag);
                if (nErr == FSR_STL_SUCCESS)
                {
                    nNumOfScts -= nTempScts;
                }
            }

            /* Backup lsn and numofscts */
            nTempLsn    = nLsn;
            nTempScts   = nNumOfScts;
        }
#endif  /* #if (OP_SUPPORT_WRITE_BUFFER == 1) */

        /* Write data to Zone */
        nScts = (pstSTLPartObj->nBlkMsk + 1) - (nLsn & pstSTLPartObj->nBlkMsk);
        while (nNumOfScts > 0)
        {
            if (nScts > nNumOfScts)
            {
                nScts = nNumOfScts;
            }

            nZoneLsn = FSR_STL_GetZoneLsn(pstSTLPartObj, nLsn, &nZone);

            nErr = FSR_STL_WriteZone(pstSTLPartObj->nClstID,            /* Cluster ID   */
                                     pstSTLPartObj->nZoneID + nZone,    /* Zone ID      */
                                     nZoneLsn,          /* Start LSN for writing        */
                                     nScts,             /* The number of sectors to write   */
                                     pBuf,              /* Buffer for writing           */
                                     nFlag);            /* must be FSR_STL_FLAG_DEFAULT */
            if (nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] %s() L(%d) - FSR_STL_WriteZone(nLsn=%d, nNumOfScts=%d) (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nLsn, nScts, nErr));
                break;
            }

            nLsn       += nScts;
            nNumOfScts -= nScts;
            pBuf       += (nScts << BYTES_SECTOR_SHIFT);

            nScts       = (pstSTLPartObj->nBlkMsk + 1);
        }

#if (OP_SUPPORT_WRITE_BUFFER == 1)
        if (nTempScts > 0 && pstSTLPartObj->pstWBObj != NULL)
        {
            /* Delete the sectors newly written in Zone from WB */
            nErr = FSR_STL_DeleteWB(pstSTLPartObj->nVolID,
                                    pstSTLPartObj->nPart,
                                    nTempLsn,
                                    nTempScts,
                                    TRUE32,
                                    nFlag);
            if (nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] %s() L(%d) - FSR_STL_DeleteWB(nLsn=%d, nNumOfScts=%d) (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nLsn, nScts, nErr));
                break;
            }
        }
#endif  /* #if (OP_SUPPORT_WRITE_BUFFER == 1) */

        /* Release a semaphore */
        if ((nFlag & FSR_STL_FLAG_USE_SM) != 0)
        {
            bRet = FSR_OAM_ReleaseSM(nSM, FSR_OAM_SM_TYPE_STL);
            if (bRet == FALSE32)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  Releasing semaphore is failed.\r\n")));
                if (nErr == FSR_STL_SUCCESS)
                {
                    nErr = FSR_STL_RELEASE_SM_ERROR;
                    break;
                }
            }
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nErr));
    return  nErr;
}


/**
 * @brief       This function deletes sectors.
 * 
 * @param[in]   nVol       : Volume number
 * @param[in]   nPartID    : Partition ID number
 * @param[in]   nLsn       : Start Lsn for deletion
 * @param[in]   nNumOfScts : The number of sectors to delete
 * @param[in]   nFlag      : must be FSR_STL_FLAG_DEFAULT
 * 
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_INVALID_PARAM
 * @return      FSR_STL_INVALID_VOLUME_ID
 * @return      FSR_STL_INVALID_PARTITION_ID
 * @return      FSR_STL_PARTITION_NOT_OPENED
 * @return      FSR_STL_CRITICAL_ERROR 
 *
 * @author      SongHo Yoon & Seung-hyun Han
 * @version     1.1.0
 *
 */
PUBLIC INT32
FSR_STL_Delete (UINT32          nVol,
                UINT32          nPartID,
                UINT32          nLsn,
                UINT32          nNumOfScts,
                UINT32          nFlag)
{
    STLPartObj         *pstSTLPartObj;
    SM32                nSM;
    BOOL32              bRet;
    UINT32              nZoneLsn;
    UINT32              nZone;
    UINT32              nScts;
    INT32               nErr        = FSR_STL_INVALID_PARAM;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%d, %d, %d, %d, %x)\r\n"),
            __FSR_FUNC__, nVol, nPartID, nLsn, nNumOfScts, nFlag));

    do
    {
        /* Check validity of arguments          */
        CHECK_INIT_STATE();
        /* Check the boundary of Volume ID      */
        CHECK_VOLUME_ID(nVol);
        /* Check the boundary of Partition ID   */
        CHECK_PARTITION_ID(nPartID);

        /* Get STL Partition Object             */
        pstSTLPartObj   = &(gstSTLPartObj[nVol][nPartID - FSR_PARTID_STL0]);

        /* Check whether the partition is opened  */
        CHECK_PARTITION_OPEN(pstSTLPartObj, nPartID);

        /* Check whether the partition is read only */
        CHECK_READ_ONLY_PARTITION(pstSTLPartObj, nPartID);

        /* Check whether the partition is locked by STL */
        CHECK_LOCKED_PARTITION(pstSTLPartObj, nPartID);

        nSM  = pstSTLPartObj->pst1stPart->nSM;

        /* Acquire a semaphore */
        if ((nFlag & FSR_STL_FLAG_USE_SM) != 0)
        {
            bRet = FSR_OAM_AcquireSM(nSM, FSR_OAM_SM_TYPE_STL);
            if (bRet == FALSE32)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  Acquiring semaphore is failed.\r\n")));
                nErr = FSR_STL_ACQUIRE_SM_ERROR;
                break;
            }
        }

#if (OP_SUPPORT_STATISTICS_INFO == 1)
        pstSTLPartObj->nSTLDelScts += nNumOfScts;
#endif
        /* Delete sectors */
        nScts = (pstSTLPartObj->nBlkMsk + 1) - (nLsn & pstSTLPartObj->nBlkMsk);
        while (nNumOfScts > 0)
        {
            if (nScts > nNumOfScts)
            {
                nScts = nNumOfScts;
            }

            nZoneLsn = FSR_STL_GetZoneLsn(pstSTLPartObj, nLsn, &nZone);

            nErr = FSR_STL_DeleteZone(pstSTLPartObj->nClstID,           /* Cluster ID   */
                                      pstSTLPartObj->nZoneID + nZone,   /* Zone ID      */
                                      nZoneLsn,          /* Start LSN for deleting      */
                                      nScts,             /* The number of sectors to delete */
                                      nFlag);            /* must be FSR_STL_FLAG_DEFAULT    */
            if (nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] %s() L(%d) - FSR_STL_DeleteZone(nLsn=%d, nNumOfScts=%d) (0x%x)\r\n"),
                    __FSR_FUNC__, __LINE__, nLsn, nScts, nErr));
                break;
            }

#if (OP_SUPPORT_WRITE_BUFFER == 1)
            if (pstSTLPartObj->pstWBObj != NULL)
            {
                /* Delete the sectors from WB */
                nErr = FSR_STL_DeleteWB(pstSTLPartObj->nVolID,
                                        pstSTLPartObj->nPart,
                                        nLsn,
                                        nScts,
                                        FALSE32,
                                        nFlag);
                if (nErr != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                        (TEXT("[SIF:ERR] %s() L(%d) - FSR_STL_DeleteWB(nLsn=%d, nNumOfScts=%d) (0x%x)\r\n"),
                        __FSR_FUNC__, __LINE__, nLsn, nScts, nErr));
                    break;
                }
            }
#endif

            nLsn       += nScts;
            nNumOfScts -= nScts;

            nScts       = (pstSTLPartObj->nBlkMsk + 1);
        }

        /* Release a semaphore */
        if ((nFlag & FSR_STL_FLAG_USE_SM) != 0)
        {
            bRet = FSR_OAM_ReleaseSM(nSM, FSR_OAM_SM_TYPE_STL);
            if (bRet == FALSE32)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  Releasing semaphore is failed.\r\n")));
                if (nErr == FSR_STL_SUCCESS)
                {
                    nErr = FSR_STL_RELEASE_SM_ERROR;
                    break;
                }
            }
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nErr));
    return  nErr;
}


/**
 * @brief       This function provides a generic I/O control code (IOCTL) for getting
 *
 * @param[in]   nVol          : Volume number
 * @param[in]   nPartID       : Partition ID number
 * @param[in]   nCode         : IO control code, which can be pre-defined
 * @param[in]  *pBufIn        : Pointer to the input buffer supplied by the caller
 * @param[in]   nLenIn        : Size in bytes of pBufIn
 * @param[in]  *pBufOut       : Pointer to the output buffer supplied by the caller
 * @param[in]   nLenOut       : Size in bytes of pBufOut
 * @param[in]  *pBytesReturned: Number of bytes returned in lpOutBuf
 *
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_INVALID_PARAM
 * @return      FSR_STL_INVALID_VOLUME_ID
 * @return      FSR_STL_INVALID_PARTITION_ID
 * @return      FSR_STL_PARTITION_NOT_OPENED
 * @return      FSR_STL_CRITICAL_ERROR 
 *
 * @author      SongHo Yoon & Seung-hyun Han
 * @version     1.1.0
 *
 */
PUBLIC INT32
FSR_STL_IOCtl  (UINT32          nVol,
                UINT32          nPartID,
                UINT32          nCode,
                VOID           *pBufIn,
                UINT32          nLenIn,
                VOID           *pBufOut,
                UINT32          nLenOut,
                UINT32         *pBytesReturned)
{
    /* local variable definitions */
    STLPartObj         *pstSTLPartObj;
    STLClstObj         *pstSTLClstObj;
    STLZoneObj         *pstZone;
    UINT32              nPartAttr;
    UINT32              nPart;
    UINT32              nNumPart;
    UINT32              nZone;
    UINT32              nNumZone;
    UINT32              nScts;
    UINT32              nBlkNum;
    UINT32              nSctsPerPg;
#if (OP_SUPPORT_STATISTICS_INFO == 1)
    STLPartObj         *pstTmpSTLPartObj;
    UINT32              nRootBlksECNT;
    UINT32              nZoneBlksECNT;
    UINT32              nTotalECNT;
    FSRStlStats        *pstStats;
#endif 
    SM32                nSM;
    BOOL32              bRet;
    INT32               nErr        = FSR_STL_INVALID_PARAM;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF: IN]  ++%s(%d, %d, %x, %x, %d, %x, %d, %x)\r\n"),
            __FSR_FUNC__, nVol, nPartID, nCode, pBufIn, nLenIn, pBufOut, nLenOut, pBytesReturned));

    do
    {
        /* Check validity of arguments          */
        CHECK_INIT_STATE();
        /* Check the boundary of Volume ID      */
        CHECK_VOLUME_ID(nVol);
        /* Check the boundary of Partition ID   */
        CHECK_PARTITION_ID(nPartID);

        /* Get STL Partition Object             */
        pstSTLPartObj   = &(gstSTLPartObj[nVol][nPartID - FSR_PARTID_STL0]);

        /* Check which the partition is opened  */
        CHECK_PARTITION_OPEN(pstSTLPartObj, nPartID);

        /* Get semaphore */
        nSM  = pstSTLPartObj->pst1stPart->nSM;
        bRet = FSR_OAM_AcquireSM(nSM, FSR_OAM_SM_TYPE_STL);
        if (bRet == FALSE32)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR]  Acquiring semaphore is failed.\r\n")));
            nErr = FSR_STL_ACQUIRE_SM_ERROR;
            break;
        }

        switch (nCode)
        {
            case FSR_STL_IOCTL_GET_WL_THRESHOLD:
            {
                /* input & output parameter check */
                if ((pBufOut == NULL) || (nLenOut < sizeof(UINT32)) ||
                    (pBytesReturned == NULL))
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                        (TEXT("[SIF:ERR] Invalid argument (pBufOut %x), (nLenOut %d), (pBytesReturned %x)\r\n"),
                            pBufOut, nLenOut, pBytesReturned));
                    nErr = FSR_STL_INVALID_PARAM;
                    break;
                }

                pstSTLClstObj = FSR_STL_GetClstObj(pstSTLPartObj->nClstID);

                /* For meta-unit wear-leveling, we exchange the meta-unit 
                 * of which erase count is the maximum among meta-units with
                 * free unit of which erase count may be the maximum among
                 * data-units.Therefore, our wear-level threshold doubles
                 */
                *((UINT32 *)pBufOut) = (pstSTLClstObj->pstEnvVar->nECDiffThreshold) << 1;

                /* output byte */
                *pBytesReturned = sizeof(UINT32);

                nErr = FSR_STL_SUCCESS;
                break;
            }

            case FSR_STL_IOCTL_GET_ECNT_BUFFER_SIZE:
            {
                /* input & output parameter check */
                if ((pBufOut == NULL) || (nLenOut < sizeof(UINT32)) ||
                    (pBytesReturned == NULL))
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                        (TEXT("[SIF:ERR] Invalid argument (pBufOut %x), (nLenOut %d), (pBytesReturned %x)\r\n"),
                            pBufOut, nLenOut, pBytesReturned));
                    nErr = FSR_STL_INVALID_PARAM;
                    break;
                }

                pstSTLClstObj = FSR_STL_GetClstObj(pstSTLPartObj->nClstID);

                /* out total logical sectors */
                nBlkNum  = 0;

                pstSTLPartObj = pstSTLPartObj->pst1stPart;
                nNumPart      = pstSTLPartObj->nNumPart;
                for (nPart = 0; nPart < nNumPart; nPart++)
                {
                    nNumZone = pstSTLPartObj->nNumZone;
                    for (nZone = 0; nZone < nNumZone; nZone++)
                    {
                        nBlkNum += pstSTLClstObj->stZoneObj[pstSTLPartObj->nZoneID + nZone].pstZI->nNumTotalBlks;
                    }

                    pstSTLPartObj++;
                }

                *((UINT32 *)pBufOut) = (nBlkNum + MAX_ROOT_BLKS);

                /* output byte */
                *pBytesReturned = sizeof(UINT32);

                nErr = FSR_STL_SUCCESS;
                break;
            }

            case FSR_STL_IOCTL_READ_ECNT:
            {
                /* input & output parameter check */
                if ((pBufOut == NULL) || (nLenOut < sizeof(UINT32)) ||
                    (pBytesReturned == NULL))
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                        (TEXT("[SIF:ERR] Invalid argument (pBufIn %x), (nLenIn %d), (pBufOut %x), (nLenOut %d), (pBytesReturned %x)\r\n"),
                            pBufIn, nLenIn, pBufOut, nLenOut, pBytesReturned));
                    nErr = FSR_STL_INVALID_PARAM;
                    break;
                }
                nBlkNum = nLenOut >> 2;     /* divide by sizeof (UINT32) */

                pstSTLClstObj = FSR_STL_GetClstObj(pstSTLPartObj->nClstID);

                /*initialize EC array*/
                FSR_OAM_MEMSET(pBufOut, 0xFF, nLenOut);

                /* get erase count of all units in the cluster */
                pstSTLPartObj = pstSTLPartObj->pst1stPart;
                nNumPart      = pstSTLPartObj->nNumPart;
                for (nPart = 0; nPart < nNumPart; nPart++)
                {
                    nNumZone = pstSTLPartObj->nNumZone;
                    for (nZone = 0; nZone < nNumZone; nZone++)
                    {
                        pstZone = &(pstSTLClstObj->stZoneObj[pstSTLPartObj->nZoneID + nZone]);
#if (OP_SUPPORT_PAGE_DELETE == 1)
                        /* Reserve meta page */
                        nErr = FSR_STL_ReserveMetaPgs(pstZone, 1, TRUE32);
                        if (nErr != FSR_STL_SUCCESS)
                        {
                            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                                    __FSR_FUNC__, __LINE__, nErr));
                            return nErr;
                        }
                        /* flush lazy delete operation in all zones */
                        nErr = FSR_STL_StoreDeletedInfo(pstZone);
                        if (nErr != FSR_STL_SUCCESS){
                            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                                    __FSR_FUNC__, __LINE__, nErr));
                            return nErr;
                        }
#endif
                        nErr = FSR_STL_GetAllBlksEC(pstZone, (UINT32*)pBufOut, NULL, NULL, nBlkNum);
                    }

                    pstSTLPartObj++;
                }

                /* output byte */
                *pBytesReturned = nBlkNum << 2;     /* multiply by sizeof(UINT32) */

                break;
            }

            case FSR_STL_IOCTL_LOG_SECTS:
            {
                /* input & output parameter check */
                if ((pBufOut == NULL) || (nLenOut < sizeof(UINT32)) ||
                    (pBytesReturned == NULL))
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                        (TEXT("[SIF:ERR] Invalid argument (pBufOut %x), (nLenOut %d), (pBytesReturned %x)\r\n"),
                            pBufOut, nLenOut, pBytesReturned));
                    nErr = FSR_STL_INVALID_PARAM;
                    break;
                }

                pstSTLClstObj = FSR_STL_GetClstObj(pstSTLPartObj->nClstID);

                /* out total logical sectors */
                nScts    = 0;
                nZone    = pstSTLPartObj->nZoneID;
                nNumZone = nZone + pstSTLPartObj->nNumZone;
                while (nZone < nNumZone)
                {
                    nScts += pstSTLClstObj->stZoneObj[nZone].pstZI->nNumUserScts;
                    nZone++;
                }
                *((UINT32 *)pBufOut) = nScts;

                /* output byte */
                *pBytesReturned = sizeof(UINT32);

                nErr = FSR_STL_SUCCESS;
                break;
            }

            case FSR_STL_IOCTL_PAGE_SIZE:
            {
                /* input & output parameter check */
                if ((pBufOut == NULL) || (nLenOut < sizeof(UINT32)) ||
                    (pBytesReturned == NULL))
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                        (TEXT("[SIF:ERR] Invalid argument (pBufOut %x), (nLenOut %d), (pBytesReturned %x)\r\n"),
                            pBufOut, nLenOut, pBytesReturned));
                    nErr = FSR_STL_INVALID_PARAM;
                    break;
                }

                pstSTLClstObj = FSR_STL_GetClstObj(pstSTLPartObj->nClstID);
                nSctsPerPg    = pstSTLClstObj->pstDevInfo->nSecPerVPg;

                /* out page size */
                *((UINT32 *)pBufOut) = nSctsPerPg * FSR_SECTOR_SIZE;

                /* output byte */
                *pBytesReturned = sizeof(UINT32);

                nErr = FSR_STL_SUCCESS;
                break;
            }

            case FSR_STL_IOCTL_CHANGE_PART_ATTR:
            {                
                /* input parameter check */
                if ((pBufIn == NULL) || (nLenIn != sizeof(UINT32)))
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                        (TEXT("[SIF:ERR] Invalid argument (pBufIn %x), (nLenIn %d)\r\n"),
                            pBufIn, nLenIn));
                    nErr = FSR_STL_INVALID_PARAM;
                    break;
                }

                /* Set nPart atrribute */
                nPartAttr = *((UINT32*)pBufIn);

                /* 
                * The attribute of partition to be changed should be FSR_STL_FLAG_OPEN_READWRITE or
                * FSR_STL_FLAG_OPEN_READONLY
                */
                if ((nPartAttr != FSR_STL_FLAG_OPEN_READONLY) && (nPartAttr != FSR_STL_FLAG_OPEN_READWRITE))
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                        (TEXT("[SIF:ERR] Invalid argument (pBufIn %x), (nLenIn %d)\r\n"),
                            pBufIn, nLenIn));
                    nErr = FSR_STL_INVALID_PARAM;
                    break;
                }

                FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG, 
                    (TEXT("[SIF:   ]  nPartID : 0x%x\r\n"), nPartID));
                FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG, 
                    (TEXT("[SIF:   ]  nPartAttr : %x\r\n"), nPartAttr));
                
                nErr = FSR_STL_ChangePartAttr(nVol,nPartID,nPartAttr);
                if (nErr != FSR_STL_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[SIF:ERR]   %s(nVol: %d, nCode: FSR_STL_IOCTL_CHANGE_PART_ATTR, nRe:0x%x)\r\n"),
                                                    __FSR_FUNC__, nVol, bRet));
                    break;
                }

                /* output byte */
                if (pBytesReturned != NULL)
                {
                    *pBytesReturned = 0;
                }

                nErr = FSR_STL_SUCCESS;
                break;
            }

            case FSR_STL_IOCTL_RESET_STATS:
            {
#if (OP_SUPPORT_STATISTICS_INFO == 1)
                pstSTLClstObj = FSR_STL_GetClstObj(pstSTLPartObj->nClstID);

                /* Get nBlkNum */
                nBlkNum = 0;
                pstTmpSTLPartObj = pstSTLPartObj->pst1stPart;
                nNumPart         = pstTmpSTLPartObj->nNumPart;
                for (nPart = 0; nPart < nNumPart; nPart++)
                {
                    nNumZone = pstTmpSTLPartObj->nNumZone;
                    for (nZone = 0; nZone < nNumZone; nZone++)
                    {
                        pstZone = &(pstSTLClstObj->stZoneObj[pstTmpSTLPartObj->nZoneID + nZone]);

                        nBlkNum += pstZone->pstZI->nNumTotalBlks;
                    }
                    pstTmpSTLPartObj++;
                }
                nBlkNum += MAX_ROOT_BLKS;

                nTotalECNT = 0;

                /* Reset STL status */
                pstTmpSTLPartObj = pstSTLPartObj->pst1stPart;
                nNumPart         = pstTmpSTLPartObj->nNumPart;
                for (nPart = 0; nPart < nNumPart; nPart++)
                {
                    nNumZone = pstTmpSTLPartObj->nNumZone;
                    for (nZone = 0; nZone < nNumZone; nZone++)
                    {
                        pstZone = &(pstSTLClstObj->stZoneObj[pstTmpSTLPartObj->nZoneID + nZone]);

                        pstZone->pstStats->nCompactionCnt = 0;         /**< compaction count                   */
                        pstZone->pstStats->nActCompactCnt = 0;         /**< active compaction count            */
                        pstZone->pstStats->nTotalLogBlkCnt = 0;        /**< total allocated log block count    */
                        pstZone->pstStats->nTotalLogPgmCnt = 0;        /**< total log block program count      */
                        pstZone->pstStats->nCtxPgmCnt = 0;             /**< total meta page program count      */

#if (OP_SUPPORT_PAGE_DELETE == 1)
                        /* Reserve meta page */
                        nErr = FSR_STL_ReserveMetaPgs(pstZone, 1, TRUE32);
                        if (nErr != FSR_STL_SUCCESS)
                        {
                            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                                    __FSR_FUNC__, __LINE__, nErr));
                            return nErr;
                        }

                        /* flush lazy delete operation in all zones */
                        nErr = FSR_STL_StoreDeletedInfo(pstZone);
                        if (nErr != FSR_STL_SUCCESS)
                        {
                            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                                    __FSR_FUNC__, __LINE__, nErr));
                            return nErr;
                        }
#endif
                        nErr = FSR_STL_GetAllBlksEC(pstZone,
                                                    NULL,
                                                    &nRootBlksECNT,
                                                    &nZoneBlksECNT,
                                                    nBlkNum);

                        if (nPart == 0 && nZone == 0)
                        {
                            nTotalECNT += (nRootBlksECNT + nZoneBlksECNT);
                        }
                        else
                        {
                            nTotalECNT += nZoneBlksECNT;
                        }
                    }

                    /* Initialize variable in each partition for collecting statistics*/
                    pstTmpSTLPartObj->nSTLWrScts   = 0;
                    pstTmpSTLPartObj->nSTLRdScts    = 0;
                    pstTmpSTLPartObj->nSTLDelScts  = 0;

                    pstTmpSTLPartObj++;
                }

                /* Store last erase count only at 1st partition in cluster */
                pstSTLPartObj->pst1stPart->nSTLLastECnt = nTotalECNT;

                nErr = FSR_STL_SUCCESS;
#else
                nErr = FSR_STL_ERROR;
#endif
                break;
            }

            case FSR_STL_IOCTL_GET_STATS:
            {
#if (OP_SUPPORT_STATISTICS_INFO == 1)
                /* input & output parameter check */
                if ((pBufOut == NULL) || (nLenOut < sizeof(FSRStlStats)) ||
                    (pBytesReturned == NULL))
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                        (TEXT("[SIF:ERR] Invalid argument (pBufOut %x), (nLenOut %d), (pBytesReturned %x)\r\n"),
                            pBufOut, nLenOut, pBytesReturned));
                    nErr = FSR_STL_INVALID_PARAM;
                    break;
                }

                pstSTLClstObj = FSR_STL_GetClstObj(pstSTLPartObj->nClstID);
                
                /* Initialize output buffer */
                FSR_OAM_MEMSET(pBufOut, 0, nLenOut);
                pstStats      = pBufOut;

                /* Get nBlkNum */
                nBlkNum = 0;
                pstTmpSTLPartObj = pstSTLPartObj->pst1stPart;
                nNumPart         = pstTmpSTLPartObj->nNumPart;
                for (nPart = 0; nPart < nNumPart; nPart++)
                {
                    nNumZone = pstTmpSTLPartObj->nNumZone;
                    for (nZone = 0; nZone < nNumZone; nZone++)
                    {
                        pstZone = &(pstSTLClstObj->stZoneObj[pstTmpSTLPartObj->nZoneID + nZone]);

                        nBlkNum += pstZone->pstZI->nNumTotalBlks;
                    }
                    pstTmpSTLPartObj++;
                }
                nBlkNum += MAX_ROOT_BLKS;

                nTotalECNT = 0;

                /* Get STL status */
                pstTmpSTLPartObj = pstSTLPartObj->pst1stPart;
                nNumPart         = pstTmpSTLPartObj->nNumPart;
                for (nPart = 0; nPart < nNumPart; nPart++)
                {
                    nNumZone = pstTmpSTLPartObj->nNumZone;
                    for (nZone = 0; nZone < nNumZone; nZone++)
                    {
                        pstZone = &(pstSTLClstObj->stZoneObj[pstTmpSTLPartObj->nZoneID + nZone]);

                        pstStats->nCompactionCnt += pstZone->pstStats->nCompactionCnt;
                        pstStats->nActCompactCnt += pstZone->pstStats->nActCompactCnt;
                        pstStats->nTotalLogBlkCnt += pstZone->pstStats->nTotalLogBlkCnt;
                        pstStats->nTotalLogPgmCnt += pstZone->pstStats->nTotalLogPgmCnt;
                        pstStats->nCtxPgmCnt += pstZone->pstStats->nCtxPgmCnt;

#if (OP_SUPPORT_PAGE_DELETE == 1)
                        /* Reserve meta page */
                        nErr = FSR_STL_ReserveMetaPgs(pstZone, 1, TRUE32);
                        if (nErr != FSR_STL_SUCCESS)
                        {
                            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                                    __FSR_FUNC__, __LINE__, nErr));
                            return nErr;
                        }
                        /* flush lazy delete operation in all zones */
                        nErr = FSR_STL_StoreDeletedInfo(pstZone);
                        if (nErr != FSR_STL_SUCCESS){
                            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                                    __FSR_FUNC__, __LINE__, nErr));
                            return nErr;
                        }
#endif
                        nErr = FSR_STL_GetAllBlksEC(pstZone,
                                                    NULL,
                                                    &nRootBlksECNT,
                                                    &nZoneBlksECNT,
                                                    nBlkNum);

                        if (nPart == 0 && nZone == 0)
                        {
                            nTotalECNT += (nRootBlksECNT + nZoneBlksECNT);
                        }
                        else
                        {
                            nTotalECNT += nZoneBlksECNT;
                        }
                    }

                    pstStats->nSTLWrScts += pstTmpSTLPartObj->nSTLWrScts;
                    pstStats->nSTLRdScts += pstTmpSTLPartObj->nSTLRdScts;
                    pstStats->nSTLDelScts += pstTmpSTLPartObj->nSTLDelScts;

                    pstTmpSTLPartObj++;
                }

                pstStats->nNumOfUnits = nBlkNum;
                pstStats->nSctsPerUnit = pstSTLClstObj->pstDevInfo->nSecPerSBlk;

                pstStats->nECnt = nTotalECNT - pstSTLPartObj->pst1stPart->nSTLLastECnt;

                /* output byte */
                *pBytesReturned = sizeof(FSRStlStats);

                nErr = FSR_STL_SUCCESS;
#else
                nErr = FSR_STL_ERROR;
#endif
                break;
            }

            case FSR_STL_IOCTL_FLUSH_WB:
            {
#if (OP_SUPPORT_WRITE_BUFFER == 1)
                /* input parameter check */
                if ((pBufIn == NULL) || (nLenIn < sizeof(UINT32)))
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                        (TEXT("[SIF:ERR] Invalid argument (pBufIn %x), (nLenIn %d)\r\n"),
                            pBufIn, nLenIn));
                    nErr = FSR_STL_INVALID_PARAM;
                    break;
                }

                if (pstSTLPartObj->pstWBObj != NULL)
                {
                    /* Flush write buffer */
                    nErr = FSR_STL_FlushWB(pstSTLPartObj->nVolID,
                                           pstSTLPartObj->nPart,
                                           *((UINT32*) pBufIn),
                                           FSR_STL_FLAG_DEFAULT);
                    if (nErr != FSR_STL_SUCCESS)
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_WBM,
                            (TEXT("[WBM:ERR] %s() L(%d) - FSR_STL_FlushWB(nVol=%d, nPart=%d) (0x%x)\r\n"),
                            __FSR_FUNC__, __LINE__, pstSTLPartObj->nVolID, pstSTLPartObj->nPart, nErr));
                        break;
                    }
                }
                else
                {
                    nErr = FSR_STL_ERROR;
                    break;
                }

                /* output byte */
                if (pBytesReturned != NULL)
                {
                    *pBytesReturned = 0;
                }

                nErr = FSR_STL_SUCCESS;
#else
                nErr = FSR_STL_ERROR;
#endif
                break;
            }

            default:
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                    (TEXT("[SIF:ERR] Invalid function code (Vol %d, Part %d, code %x)\r\n"),
                        nVol, nPartID, nCode));
                break; 
            }
        }

        /* Release a semaphore */
        bRet = FSR_OAM_ReleaseSM(nSM, FSR_OAM_SM_TYPE_STL);
        if (bRet == FALSE32)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR]  Releasing semaphore is failed.\r\n")));
            if (nErr == FSR_STL_SUCCESS)
            {
                nErr = FSR_STL_RELEASE_SM_ERROR;
                break;
            }
        }

        /* function implementation end */
    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"),
            __FSR_FUNC__, nErr));
    return nErr;
}

/**
 * @brief       This function locks the given cluster to prohibit write
 * 
 * @param[in]   nClstID        : The cluster ID
 * 
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_INVALID_PARAM
 *
 * @author      SongHo Yoon
 * @version     1.0.0
 *
 */
PUBLIC INT32
FSR_STL_LockPartition  (const   UINT32  nClstID)
{
    STLPartObj         *pstSTLPartObj   = NULL;
    STLClstObj         *pstSTLClstObj;
    UINT32              nPart;
    UINT32              nNumPart;
    INT32               nErr;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF: IN]  ++%s(%d)\r\n"),
            __FSR_FUNC__, nClstID));

    do
    {
        /* Check parameter */
        if (nClstID >= MAX_NUM_CLUSTERS)
        {
            nErr = FSR_STL_INVALID_PARAM;
            break;
        }

        /* Get cluster object */
        pstSTLClstObj = FSR_STL_GetClstObj(nClstID);
        if (pstSTLClstObj == NULL)
        {
            nErr = FSR_STL_INVALID_PARAM;
            break;
        }

        /* Find STL partition object from nClstID */
        for (nPart = 0; nPart < FSR_MAX_STL_PARTITIONS; nPart++)
        {
            pstSTLPartObj = &(gstSTLPartObj[pstSTLClstObj->nVolID][nPart]);
            if (pstSTLPartObj->nClstID == nClstID)
            {
                break;
            }
        }

        if (nPart == FSR_MAX_STL_PARTITIONS)
        {
            nErr = FSR_STL_INVALID_PARTITION_ID;
            break;
        }

        /* Set the all partitions in the cluster to Lock*/
        pstSTLPartObj = pstSTLPartObj->pst1stPart;
        nNumPart      = pstSTLPartObj->nNumPart;
        for (nPart = 0; nPart < nNumPart; nPart++)
        {
            FSR_ASSERT(pstSTLPartObj->nClstID == nClstID);
            FSR_ASSERT(pstSTLPartObj->nNumZone > 0);

            pstSTLPartObj->nOpenFlag |= FSR_STL_FLAG_LOCK_PARTITION;
            pstSTLPartObj++;
        }

        nErr = FSR_STL_SUCCESS;
        FSR_MAMMOTH_POWEROFF();
    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"),
            __FSR_FUNC__, nErr));
    return nErr;
}

/**
 * @brief       This function changes partition attribute 
 * 
 * @param[in]   nVol          : Volume Id
 * @param[in]   nPartID       : Partition Id
 * @param[in]   nAttr         : R/W Flag 
 *                              FSR_STL_FLAG_OPEN_READWRITE : RW
 *                              FSR_STL_FLAG_OPEN_READONLY  : RO
 * 
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_INVALID_PARAM
 * @return      FSR_STL_INVALID_PARTITION_ID
 * @return      FSR_STL_PARTITION_ERROR 
 * @return      FSR_STL_PARTITION_LOCKED
 *
 * @author      SangHoon Choi
 * @version     1.1.0
 *
 */
PUBLIC INT32
FSR_STL_ChangePartAttr(UINT32       nVol,
                       UINT32       nPartID,
                       UINT32       nAttr)
{
    STLPartObj         *pstSTLPartObj;
    STLPartObj         *pstTmpSTLPart;
    STLZoneObj         *pstZone;
    FSRChangePA         stChangePA;
    BOOL32              bEnPartAttrChg = FALSE32;
    INT32               nBMLErr        = FSR_BML_SUCCESS;
    INT32               nErr           = FSR_STL_SUCCESS;

    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    do
    {
        /* Remember the initial state of enability to change partition attribute */
        bEnPartAttrChg = FSR_BML_GetPartAttrChg (nVol, nPartID, FSR_BML_FLAG_NONE);
        if (bEnPartAttrChg)
        {
            /* Enable to change STL partition attribute */
            FSR_BML_SetPartAttrChg(nVol,nPartID,FALSE32,FSR_STL_FLAG_DEFAULT);
        }

        /* Get STL Partition Object */
        pstSTLPartObj   = &(gstSTLPartObj[nVol][nPartID - FSR_PARTID_STL0]);
        if (pstSTLPartObj->nZoneID == (UINT32)(-1))
        {
            FSR_ASSERT(pstSTLPartObj->nClstID == (UINT32)(-1));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                (TEXT("[SIF:ERR] Invalid Partition ID (PartID %d)\r\n"), nPartID));
            nErr = FSR_STL_INVALID_PARTITION_ID;
            break;
        }

        /* Check if the partition is a global wear leveling partition */
        if (pstSTLPartObj->nNumPart != 1)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                (TEXT("[SIF:ERR] Global Wear Leveling partition is not supported (PartID %d)\r\n"), nPartID));
            nErr = FSR_STL_PARTITION_ERROR;
            break;
        }

        /* Get the partition info */
        pstTmpSTLPart = pstSTLPartObj->pst1stPart;
        pstZone = &(gpstSTLClstObj[pstSTLPartObj->nClstID]->stZoneObj[pstSTLPartObj->nZoneID]);

        /* If the partition is locked, changing the partition attribute is not available */
        if ((pstTmpSTLPart->nOpenFlag & FSR_STL_FLAG_LOCK_PARTITION) != 0)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR | FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
                (TEXT("[SIF:ERR] The partition (PartID %d) is locked by STL\r\n"), nPartID));
            nErr = FSR_STL_PARTITION_LOCKED;
            break;
        }

        /* 
        * This should be executed only when the user requests
        * to change to STL RO. Because it is possible that there is a 
        * unflushed delete object.
        */
        if (((nAttr & FSR_STL_FLAG_OPEN_READONLY) != 0) &&
            ((pstTmpSTLPart->nOpenFlag & FSR_STL_FLAG_RO_PARTITION) == 0))
        {
#if (OP_SUPPORT_PAGE_DELETE == 1)
            /* Flush Delete Object  */
            /* store previous deleted page info */
            nErr = FSR_STL_StoreDeletedInfo(pstZone);
            if (nErr != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nErr));
                nErr = FSR_STL_ERROR;
                break;
            }
#endif /* (OP_SUPPORT_PAGE_DELETE == 1) */
        }

        stChangePA.nPartID  = nPartID;

        /* If the current state and the new state is same, then return success */
        if ((((nAttr & FSR_STL_FLAG_OPEN_READONLY) != 0) && 
            ((pstTmpSTLPart->nOpenFlag & FSR_STL_FLAG_RO_PARTITION) != 0)) ||
            (((nAttr & FSR_STL_FLAG_OPEN_READWRITE) != 0) &&
            ((pstTmpSTLPart->nOpenFlag & FSR_STL_FLAG_RW_PARTITION) != 0)))
        {
            nErr = FSR_STL_SUCCESS;
            break;
        }

        /* Set the partition attribute */
        if (((nAttr & FSR_STL_FLAG_OPEN_READONLY) != 0) &&
            ((pstTmpSTLPart->nOpenFlag & FSR_STL_FLAG_RO_PARTITION) == 0))
        {
            stChangePA.nNewAttr = FSR_BML_PI_ATTR_RO | FSR_BML_PI_ATTR_LOCK;
        }
        else if(((nAttr & FSR_STL_FLAG_OPEN_READWRITE) != 0) &&
            ((pstTmpSTLPart->nOpenFlag & FSR_STL_FLAG_RW_PARTITION) == 0))
        {
            stChangePA.nNewAttr = FSR_BML_PI_ATTR_RW;
        }

        nBMLErr = FSR_BML_IOCtl(nVol,
                       FSR_BML_IOCTL_CHANGE_PART_ATTR,
                       (UINT8 *) &stChangePA,
                       sizeof(stChangePA),
                       NULL,
                       0,
                       NULL);
        if (nBMLErr != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nErr));
            nErr = FSR_STL_ERROR;
            break;
        }

        /* Set flag When to change to RO */
        if (((nAttr & FSR_STL_FLAG_OPEN_READONLY) != 0) &&
            ((pstTmpSTLPart->nOpenFlag & FSR_STL_FLAG_RO_PARTITION) == 0))
        {
            pstTmpSTLPart->nOpenFlag |= FSR_STL_FLAG_RO_PARTITION;
            pstTmpSTLPart->nOpenFlag &= ~FSR_STL_FLAG_RW_PARTITION;
        }
        /* Set flag when to change to RW */
        else if(((nAttr & FSR_STL_FLAG_OPEN_READWRITE) != 0) &&
            ((pstTmpSTLPart->nOpenFlag & FSR_STL_FLAG_RW_PARTITION) == 0))
        {
            pstTmpSTLPart->nOpenFlag |= FSR_STL_FLAG_RW_PARTITION;
            pstTmpSTLPart->nOpenFlag &= ~FSR_STL_FLAG_RO_PARTITION;
        }
    }while(0);

    /* Recover to the initial state of enability to change partition attribute */
    if (bEnPartAttrChg)
    {
        /* Disable to to change STL partition attribute */
        FSR_BML_SetPartAttrChg(nVol,nPartID,TRUE32,FSR_STL_FLAG_DEFAULT);
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF, (TEXT("[SIF:OUT] --%s\r\n"), __FSR_FUNC__));
    return nErr;
}


#if defined (FSR_STL_FOR_PRE_PROGRAMMING)

PUBLIC INT32    FSR_STL_DefragmentCluster  (STLClstObj     *pstClst,
                                            const UINT32    nMaxBlks,
                                            FSRStlDfrg     *pstSTLDfra,
                                            FSRStlPrgFp     pfProgress);

/**
 * @brief       This function defragments for pre-programming
 * 
 * @param[in]   nVol       : Volume number
 * @param[in]   nPartID    : Partition ID number
 * @param[out]  pstSTLDfra : the information of valid units
 * @param[in]   pfProgress : callback function to report progress
 * @param[in]   nFlag      : must be FSR_STL_FLAG_DEFAULT
 * 
 * @return      FSR_STL_SUCCESS
 * @return      FSR_STL_INVALID_PARAM
 * @return      FSR_STL_INVALID_VOLUME_ID
 * @return      FSR_STL_INVALID_PARTITION_ID
 * @return      FSR_STL_PARTITION_NOT_OPENED
 * @return      FSR_STL_CRITICAL_ERROR
 *
 * @author      SongHo Yoon
 * @version     1.0.0
 *
 */
PUBLIC INT32
FSR_STL_Defragment     (UINT32          nVol,
                        UINT32          nPartID,
                        FSRStlDfrg     *pstSTLDfra,
                        FSRStlPrgFp     pfProgress,
                        UINT32          nFlag)
{
    STLPartObj         *pstSTLPartObj;
    STLClstObj         *pstSTLClstObj;
    UINT32              nNumPart;
    UINT32              nPart;
    UINT32              nNumZone;
    UINT32              nZone;
    UINT32              nBlkNum;
    UINT32              nStartVun;
    SM32                nSM;
    BOOL32              bRet;
    INT32               nErr        = FSR_STL_INVALID_PARAM;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s(%d, %d, %x)\r\n"),
            __FSR_FUNC__, nVol, nPartID, nFlag));

    do
    {
        /* Check validity of arguments          */
        CHECK_INIT_STATE();
        /* Check the boundary of Volume ID      */
        CHECK_VOLUME_ID(nVol);
        /* Check the boundary of Partition ID   */
        CHECK_PARTITION_ID(nPartID);

        /* Get STL Partition Object             */
        pstSTLPartObj   = &(gstSTLPartObj[nVol][nPartID - FSR_PARTID_STL0]);

        /* Check whether the partition is opened  */
        CHECK_PARTITION_OPEN(pstSTLPartObj, nPartID);

        if (pstSTLPartObj != pstSTLPartObj->pst1stPart)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                (TEXT("[SIF:INF]  The defragmentation is only allowed the 1st partition in Global wear-leveling group.\r\n")));
            nErr = FSR_STL_ALREADY_DEFRAGMENTED;
            break;
        }

        nSM  = pstSTLPartObj->pst1stPart->nSM;
        if ((nFlag & FSR_STL_FLAG_USE_SM) != 0)
        {
            /* Get semaphore */
            bRet = FSR_OAM_AcquireSM(nSM, FSR_OAM_SM_TYPE_STL);
            if (bRet == FALSE32)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  Acquiring semaphore is failed.\r\n")));
                nErr = FSR_STL_ACQUIRE_SM_ERROR;
                break;
            }
        }

        pstSTLClstObj = FSR_STL_GetClstObj(pstSTLPartObj->nClstID);
        nBlkNum  = 0;

        pstSTLPartObj = pstSTLPartObj->pst1stPart;
        nStartVun     = pstSTLPartObj->stClstEV.nStartVbn;
        nNumPart      = pstSTLPartObj->nNumPart;
        for (nPart = 0; nPart < nNumPart; nPart++)
        {
            nNumZone = pstSTLPartObj->nNumZone;
            for (nZone = 0; nZone < nNumZone; nZone++)
            {
                nBlkNum += pstSTLClstObj->stZoneObj[pstSTLPartObj->nZoneID + nZone].pstZI->nNumTotalBlks;
            }

            pstSTLPartObj++;
        }

        nErr = FSR_STL_DefragmentCluster(pstSTLClstObj,
                                         nBlkNum + MAX_ROOT_BLKS,
                                         pstSTLDfra,
                                         pfProgress);
        if (nErr == FSR_STL_SUCCESS)
        {
            FSR_ASSERT(pstSTLDfra->nNumGroup <= FSR_MAX_STL_DFRG_GROUP);
            for (nZone = 0; nZone < pstSTLDfra->nNumGroup; nZone++)
            {
                pstSTLDfra->staGroup[nZone].nStartVun += nStartVun;
            }
        }

        if ((nFlag & FSR_STL_FLAG_USE_SM) != 0)
        {
            /* Release a semaphore */
            bRet = FSR_OAM_ReleaseSM(nSM, FSR_OAM_SM_TYPE_STL);
            if (bRet == FALSE32)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                    (TEXT("[SIF:ERR]  Releasing semaphore is failed.\r\n")));
                if (nErr == FSR_STL_SUCCESS)
                {
                    nErr = FSR_STL_RELEASE_SM_ERROR;
                    break;
                }
            }
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_IF | FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nErr));
    return  nErr;
}
#endif  /* defined (FSR_STL_FOR_PRE_PROGRAMMING) */

