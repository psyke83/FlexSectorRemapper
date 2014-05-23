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
 *  @file       FSR_STL_MappingMgr.c
 *  @brief      PMT, Log Group, Log, Active List Management.
 *  @author     Wonmoon Cheon
 *  @date       02-MAR-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  02-MAR-2007 [Wonmoon Cheon] : first writing
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
PRIVATE PADDR   _GetVpnFromPMT         (RBWDevInfo     *pstDev,
                                        STLLogGrpHdl   *pstLogGrp,
                                        POFFSET         nLPOffs);
PRIVATE PADDR   _FindVpnInLogGrpList   (STLZoneObj     *pstZone,
                                        STLLogGrpList  *pstLogGrpList,
                                        BADDR           nDgn,
                                        POFFSET         nLPOffs);

/*****************************************************************************/
/* Local (static)  Function Definition                                       */
/*****************************************************************************/
/**
 * @brief       This function returns VPN by the specified log groups' PMT
 *
 * @param[in]   pstDev      : device info pointer
 * @param[in]   pstLogGrp   : log group 
 * @param[in]   nLPOffs     : logical page offset in a group to find
 *
 * @return      VPN of the logical page offset
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PRIVATE PADDR
_GetVpnFromPMT (RBWDevInfo     *pstDev,
                STLLogGrpHdl   *pstLogGrp,
                POFFSET         nLPOffs)
{
    POFFSET     nGrpPOffs;
    POFFSET     nLogPOffs;
    INT32       nLogIdx;
    STLLog     *pstLog;
    PADDR       nResultVpn;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    pstLog      = NULL;
    nResultVpn  = NULL_VPN;
    
    /*  when it is in the active log group, refer the PMT */
    nGrpPOffs = *(pstLogGrp->pMapTbl + nLPOffs);
    
    /*  when nGrpPOffs == NULL_POFFSET, the logical page is mapped to data block */
    if (nGrpPOffs != NULL_POFFSET)
    {
        /* get log index */
        nLogIdx = nGrpPOffs >> pstDev->nPagesPerSbShift;

        /* get physical page offset in the log */
        /* nLogPOffs = (POFFSET)(nGrpPOffs % pstDev->nPagesPerSBlk); */
        nLogPOffs = (POFFSET)(nGrpPOffs & (pstDev->nPagesPerSBlk - 1));

        /* get log object pointer */
        pstLog = pstLogGrp->pstLogList + nLogIdx;
        
        FSR_ASSERT(pstLog->nVbn != NULL_VBN);
        FSR_ASSERT(pstLog->nState != LS_FREE);

        /* get physical page address */
        nResultVpn = (pstLog->nVbn << pstDev->nPagesPerSbShift) + nLogPOffs;
    }
    
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : %d\r\n"), __FSR_FUNC__, nResultVpn));
    return nResultVpn;
}

/**
 * @brief        This function finds the specified logical page offset in the
 * @n            log group
 *
 * @param[in]   pstZone         : zone object
 * @param[in]   pstLogGrpList   : log group list
 * @param[in]   nDgn            : DGN of log group
 * @param[in]   nLPOffs         : logical page offset in a group to find
 *
 * @return      VPN of the logical page offset
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PRIVATE PADDR
_FindVpnInLogGrpList   (STLZoneObj     *pstZone,
                        STLLogGrpList  *pstLogGrpList,
                        BADDR           nDgn,
                        POFFSET         nLPOffs)
{
    STLLogGrpHdl   *pstLogGrp;
    RBWDevInfo     *pstDev;
    PADDR           nResultVpn;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));
    
    /*  get device info pointer */
    pstDev = pstZone->pstDevInfo;

    /* set default VPN */
    nResultVpn  = NULL_VPN;

    /* search the DGN */
    pstLogGrp = FSR_STL_SearchLogGrp(pstLogGrpList, nDgn);
    if (pstLogGrp != NULL)
    {
        nResultVpn = _GetVpnFromPMT(pstDev, pstLogGrp, nLPOffs);
    }
    
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : %d\r\n"), __FSR_FUNC__, nResultVpn));
    return nResultVpn;
}


/*****************************************************************************/
/* Global Function Definition                                                */
/*****************************************************************************/

/**
 * @brief       This function returns the actual clean page offset
 * @n           in log block. The actual clean page is aligned to number of 
 * @n           interleaving ways. For interleaving copyback in merge operation, 
 * @n           instead of current nCPOffset in log block, this functions' 
 * @n           return value should be used.
 *
 * @param[in]   pstDev          : device info object
 * @param[in]   nLogCPOffset    : clean page offset in the log block
 * @param[in]   nLpn            : logical page number
 *
 * @return      Clean page offset by way organization
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC POFFSET
FSR_STL_GetWayCPOffset(RBWDevInfo *pstDev,
                POFFSET     nLogCPOffset,
                PADDR       nLpn)
{
    POFFSET     nDiff;
    POFFSET     nCPWay;
    POFFSET     nLPWay;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /*  For better merge operation time, we need to adjust way alignment. */
    /*  A few pages can be skipped without programming to keep the way offset. */

    /*  logical page way */
    /* nLPWay = (POFFSET)(nLpn         % pstDev->nNumWays); */
    nLPWay = (POFFSET)(nLpn         & (pstDev->nNumWays - 1));

    /*  clean page way */
    /* nCPWay = (POFFSET)(nLogCPOffset % pstDev->nNumWays); */
    nCPWay = (POFFSET)(nLogCPOffset & (pstDev->nNumWays - 1));

    if (nLPWay >= nCPWay)
    {
        nDiff = nLPWay - nCPWay;
    }
    else
    {
        nDiff = (POFFSET)(pstDev->nNumWays - (nCPWay - nLPWay));
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : %d\r\n"), __FSR_FUNC__, nLogCPOffset + nDiff));
    return (nLogCPOffset + nDiff);
}

/**
 * @brief       This function searches PMT directory to find out meta position of nDgn.
 *
 * @param[in]   pstZone         : zone object pointer
 * @param[in]   nDgn            : search target data group number
 * @param[out]  pnMetaPOffs     : meta blocks' page offset (search result)
 *                                when it fails to find, this value will be NULL_POFFSET
 *
 * @return      entry index in PMT directory (-1 : search fail)
 *
 * @author      Wonmoon Cheon
 * @version     1.1.0
 *
 */
PUBLIC INT32
FSR_STL_SearchPMTDir   (STLZoneObj     *pstZone,
                        BADDR           nDgn,
                        POFFSET        *pnMetaPOffs)
{
    STLDirHdrHdl       *pstDH;
    STLPMTDirEntry     *pstDE;
    INT32               nDirIdx;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get directory header object pointer */
    pstDH = pstZone->pstDirHdrHdl;

    /* default output */
    /* set directory entry index */
    nDirIdx = -1;
    *pnMetaPOffs = NULL_POFFSET;

    /* when PMT ratio is 100%, nDgn equals to the index of PMT directory. 
     * we don't need to search it.
     */
    pstDE           = pstDH->pstPMTDir + nDgn;
    *pnMetaPOffs    = pstDE->nPOffs;
    if (pstDE->nPOffs != NULL_POFFSET)
    {
        nDirIdx = nDgn;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nDirIdx));
    return nDirIdx;
}

/**
 * @brief       This function searches meta page location of a set of 
 *              data group groups.
 *
 * @param[in]   pstZone         : zone object pointer
 * @param[in]   nDgn            : search target data group number
 * @param[out]  pnMetaPOffs     : meta blocks' page offset (search result)
 *                                when it fails to find, this value will be NULL_POFFSET
 *
 * @return      entry index in PMT directory (-1 : search fail)
 *
 * @author      Wonmoon Cheon
 * @version     1.1.0
 *
 */
PUBLIC INT32
FSR_STL_SearchMetaPg   (STLZoneObj     *pstZone,
                        BADDR           nDgn,
                        POFFSET        *pnMetaPOffs)
{
    STLDirHdrHdl       *pstDH;
    STLPMTDirEntry     *pstDE;
    INT32               nDirIdx;
    INT32               nIdx;
    BADDR               nStartDgn;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get directory header object pointer */
    pstDH = pstZone->pstDirHdrHdl;

    /* default output */
    /* set directory entry index */
    nDirIdx = -1;
    *pnMetaPOffs = NULL_POFFSET;

    /* we have to search all data groups inside one meta page */
    nStartDgn  = nDgn / pstZone->pstZI->nNumLogGrpPerPMT;
    nStartDgn  = nStartDgn * pstZone->pstZI->nNumLogGrpPerPMT;
    
    /* when PMT ratio is 100%, nDgn equals to the index of PMT directory. */
    for (nIdx = nStartDgn; nIdx < nStartDgn + pstZone->pstZI->nNumLogGrpPerPMT; nIdx++)
    {
        pstDE = pstDH->pstPMTDir + nIdx;
        if (pstDE->nPOffs != NULL_POFFSET)
        {
            /* when there is a meta page */
            *pnMetaPOffs    = pstDE->nPOffs;
            nDirIdx         = nDgn;
            break;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nDirIdx));
    return nDirIdx;
}


/**
 * @brief       This function updates minimally erased log group location info.
 *
 * @param[in]   pstZone          : zone object pointer
 * @param[in]   pstDH            : directory header pointer
 *
 * @return      none
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 */
PUBLIC VOID     
FSR_STL_UpdatePMTDirMinECGrp   (STLZoneObj     *pstZone,
                                STLDirHdrHdl   *pstDH)
{
    UINT16          nIdx;
    UINT16          nMinIdx;
    UINT32          nWLGrpCnt;
    STLPMTWLGrp    *pstWLGrp;
    UINT32          nEC;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    nMinIdx     = 0;
    nEC         = 0xFFFFFFFF;
    nWLGrpCnt   = ((pstZone->pstZI->nMaxPMTDirEntry - 1) >> DEFAULT_PMT_EC_GRP_SIZE_SHIFT) + 1;
    pstWLGrp    = pstDH->pstPMTWLGrp;
        
    /* find the minimum */
    for (nIdx = 0; nIdx < nWLGrpCnt; nIdx++)
    {
        if (pstWLGrp->nMinEC < nEC)
        {
            nEC     = pstWLGrp->nMinEC;
            nMinIdx = nIdx;
        }
            
        /* move next group */
        pstWLGrp++;
    }

    /* update index */
    pstDH->pstFm->nMinECPMTIdx = nMinIdx;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}


/**
 * @brief       This function adds new PMT entry when it fails to find the nDgn.
 * @n           If there is the nDgn, it sorts the entry list by LRU policy.
 *
 * @param[in]   pstZone          : zone object pointer
 * @param[in]   nDgn             : search target data group number
 * @param[in]   nMetaPOffs       : meta blocks' page offset (only if new group)
 * @param[in]   nMinEC           : minimum erase count of log group
 * @param[in]   nMinVbn          : log block VBN which has the minimum EC
 *
 * @return      none
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC VOID
FSR_STL_UpdatePMTDir   (STLZoneObj     *pstZone,
                        BADDR           nDgn,
                        POFFSET         nMetaPOffs,
                        UINT32          nMinEC,
                        BADDR           nMinVbn,
                        BOOL32          bOpenFlag)
{
    INT32               nIdx;
    STLDirHdrHdl       *pstDH;
    STLCtxInfoHdl      *pstCtx;
    STLPMTDirEntry     *pstDE;
    INT32               nFoundIdx;
    POFFSET             nFoundMetaPOffs;
    BADDR               nStartDgn;
#if (OP_SUPPORT_DATA_WEAR_LEVELING == 1)
    UINT16              nECGrpIdx;
    STLPMTWLGrp        *pstWLGrp;
#endif  /* (OP_SUPPORT_DATA_WEAR_LEVELING == 1) */
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get directory header object handler */
    pstDH   = pstZone->pstDirHdrHdl;
    pstCtx  = pstZone->pstCtxHdl;

    /* search nDgn on PMT directory */
    nFoundIdx = FSR_STL_SearchPMTDir(pstZone, nDgn, &nFoundMetaPOffs);

    if (nFoundMetaPOffs != NULL_POFFSET)
    {
        FSR_ASSERT(nFoundIdx >= 0);

        /* get directory entry pointer */
        pstDE = pstDH->pstPMTDir + nFoundIdx;
        
        /*  when the target group already exists */
        if (nMetaPOffs != NULL_POFFSET)
        {
            /* update new meta page address */
            pstDE->nPOffs = nMetaPOffs;
        }
    }
    else    /* when there is no corresponding log group on nDgn */
    {
        /*  add new entry at tail position */
        nFoundIdx = nDgn;

        /* get new directory entry pointer */
        pstDE = pstDH->pstPMTDir + nFoundIdx;

        /* set default compaction cost : 0 */
        pstDE->nCost    = (UINT16)(-1);
        pstDE->nPOffs   = nMetaPOffs;

        /* increase the number of entries in PMT directory */
        pstDH->pstFm->nPMTEntryCnt++;
    }

    /* we have to search all data groups inside one meta page */
    nStartDgn  = nDgn / pstZone->pstZI->nNumLogGrpPerPMT;
    nStartDgn  = nStartDgn * pstZone->pstZI->nNumLogGrpPerPMT;

    /* when PMT ratio is 100%, nDgn equals to the index of PMT directory. */
    for (nIdx = nStartDgn; nIdx < nStartDgn + pstZone->pstZI->nNumLogGrpPerPMT; nIdx++)
    {
        pstDE = pstDH->pstPMTDir + nIdx;

        if (pstDE->nPOffs != NULL_POFFSET)
        {
            /* update meta page offset */
            pstDE->nPOffs = nMetaPOffs;
        }
    }

#if (OP_SUPPORT_DATA_WEAR_LEVELING == 1)
    /* get PMT wear-leveling group */
    FSR_ASSERT(nFoundIdx >= 0);
    nECGrpIdx = (UINT16)nFoundIdx >> DEFAULT_PMT_EC_GRP_SIZE_SHIFT;
    pstWLGrp = pstDH->pstPMTWLGrp + nECGrpIdx;
    
    /* update minimum erase count in the group */
    if ((bOpenFlag        == FALSE32) &&
        (pstWLGrp->nMinEC >  nMinEC ))
    {
        pstWLGrp->nDgn      = nDgn;
        pstWLGrp->nMinEC    = nMinEC;
        pstWLGrp->nMinVbn   = nMinVbn;

        /* scan every group minEC & update the minimum in all log groups */
        FSR_STL_UpdatePMTDirMinECGrp(pstZone, pstDH);
        
        /* Update Context */
        pstCtx->pstFm->nMinECPMTIdx                 = pstDH->pstFm->nMinECPMTIdx;
        pstCtx->pstFm->nUpdatedPMTWLGrpIdx          = nECGrpIdx;
        pstCtx->pstFm->stUpdatedPMTWLGrp.nDgn       = pstDH->pstPMTWLGrp[nECGrpIdx].nDgn;
        pstCtx->pstFm->stUpdatedPMTWLGrp.nMinVbn    = pstDH->pstPMTWLGrp[nECGrpIdx].nMinVbn;
        pstCtx->pstFm->stUpdatedPMTWLGrp.nMinEC     = pstDH->pstPMTWLGrp[nECGrpIdx].nMinEC;
    }
#endif

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}


/**
 * @brief       This function checks if the log group can be a merge victim or not.
 *
 * @param[in]   pstZone     : zone object pointer
 * @param[in]   pstLogGrp   : log group to check if it can be a merge victim or not
 * @param[in]   bOpenFlag   : flag for open mode or not
 * @param[out]  pbVictim    : TRUE32  - the specified group can be a merge victim
 *                            FALSE32 - the specified group can not be a merge victim
 *
 * @return      FSR_STL_SUCCESS
 *
 * @author      Wonmoon Cheon
 * @version     1.1.0
 *
 */
PUBLIC INT32
FSR_STL_CheckMergeVictimGrp    (STLZoneObj     *pstZone,
                                STLLogGrpHdl   *pstLogGrp,
                                BOOL32          bOpenFlag,
                                BOOL32         *pbVictim)
{
    STLRootInfo    *pstRI;
    STLZoneInfo    *pstZI;
    INT32           nRet;
    INT32           nIdx;
    INT32           nDBlkIdx;
    INT32           nLogIdx;
    UINT16          nDBlks;
    STLLog         *pstLog;
    BADDR          *pLbnList;
    BADDR           nLbn;
    BADDR           nLan;
    BADDR           nOrgLan;
    STLBMTHdl      *pstBMT;
    STLBMTEntry    *pstBE;
    POFFSET         nLbOffs;
    BOOL32          bMergeVictimGrp;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get root info object pointer */
    pstRI = pstZone->pstRI;

    /* get zone info object pointer */
    pstZI = pstZone->pstZI;

    /* set default output */
    nRet = FSR_STL_SUCCESS;
    bMergeVictimGrp = FALSE32;

    /* Possible condition to be a victim */
    /*  case1) if the number of log blocks in the group is greater than 'N' */
    /*  case2) when data block is not NULL and log block exists */

    do
    {
        /* case1 : when there are much logs */
        if (pstLogGrp->pstFm->nNumLogs > NUM_DBLKS_PER_GRP)
        {
            bMergeVictimGrp = TRUE32;
            break;
        }

        /* get BMT object pointer */
        pstBMT = pstZone->pstBMTHdl;

        /* we need to keep original LAN before checking victim */
        if (bOpenFlag == TRUE32)
        {
            nOrgLan = NULL_DGN;
            pstBMT->pstFm->nLan = NULL_DGN;
        }
        else
        {
            nOrgLan = pstBMT->pstFm->nLan;
        }

        /* get LAN of pstLogGrp */
        nLan = (BADDR)((pstLogGrp->pstFm->nDgn << pstRI->nNShift)
                        >> pstZI->nDBlksPerLAShift);
        
        /* if the pstLogGrp's BMT is not loaded in current BMT buffer */
        if (nOrgLan != nLan)
        {
            nRet = FSR_STL_LoadBMT(pstZone, nLan, bOpenFlag);
            if (nRet != FSR_STL_SUCCESS)
            {
                break;
            }
        }

        /* check every logs in this log group */
        nLogIdx = pstLogGrp->pstFm->nHeadIdx;
        for (nIdx = 0; nIdx < pstLogGrp->pstFm->nNumLogs; nIdx++)
        {
            /* get log object pointer */
            pstLog = pstLogGrp->pstLogList + nLogIdx;

            /* get log block LBN list info */
            nDBlks = *(pstLogGrp->pNumDBlks + nLogIdx);
            pLbnList = pstLogGrp->pLbnList + (nLogIdx << pstRI->nNShift);

            for (nDBlkIdx = 0; nDBlkIdx < nDBlks; nDBlkIdx++)
            {
                /* get LBN and its offset on BMT */
                nLbn    = *(pLbnList + nDBlkIdx);
                /* nLbOffs = (POFFSET)(nLbn % pstZI->nDBlksPerLA); */
                nLbOffs = (POFFSET)(nLbn & (pstZI->nDBlksPerLA - 1));

                /* get BMT entry pointer */
                pstBE = pstBMT->pMapTbl + nLbOffs;

                /* check if it is idle block or not */
                if ((pstBE->nVbn != NULL_VBN) && 
                    !(pstBMT->pGBlkFlags[nLbOffs >> 3] & (1 << (nLbOffs & 7))) )
                {
                    bMergeVictimGrp = TRUE32;
                    break;
                }
            }

            if (bMergeVictimGrp == TRUE32)
            {
                break;
            }

            /* move to next log */
            nLogIdx = pstLog->nNextIdx;
        }
        
        /* recover original BMT */
        if ((nOrgLan != nLan) &&
            (bOpenFlag == FALSE32))
        {
            nRet = FSR_STL_LoadBMT(pstZone, nOrgLan, bOpenFlag);
        }
    } while (0);
    
    /* set result flag */
    *pbVictim = bMergeVictimGrp;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}

/**
 * @brief        This function returns merge victim status of the log group.
 *
 * @param[in]    pstZone     : zone object
 * @param[in]    pstLogGrp   : log group object handler
 *
 * @return       TRUE32      
 * @return       FALSE32
 *
 * @author       Wonmoon Cheon
 * @version      1.1.0
 *
 */
PUBLIC BOOL32    
FSR_STL_IsMergeVictimGrp(STLZoneObj    *pstZone,
                         STLLogGrpHdl  *pstLogGrp)
{
    INT32           nFoundIdx;
    INT32           nByteOffs;
    INT32           nBitOffs;
    POFFSET         nMetaPOffs;
    STLDirHdrHdl   *pstDH;
    BOOL32          bVictim;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* set default output */
    bVictim = FALSE32;

    /* get directory header object pointer */
    pstDH = pstZone->pstDirHdrHdl;

    /* search nDgn in directory */
    nFoundIdx = FSR_STL_SearchPMTDir(pstZone, pstLogGrp->pstFm->nDgn, &nMetaPOffs);

    if (nFoundIdx >= 0)   /* found */
    {
        /* get byte and bit offset of that entry */
        nByteOffs = nFoundIdx >> 3;
        nBitOffs = 7 - (nFoundIdx & 0x07);

        /* look up merge victim flag bitstring */
        if (*(pstDH->pPMTMergeFlags + nByteOffs) & (1 << nBitOffs))
        {
            bVictim = TRUE32;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, bVictim));
    return bVictim;
}

/**
 * @brief       This function sets the victim status bit flag in merge victim bitstream.
 *
 * @param[in]   pstZone     : zone object pointer
 * @param[in]   nDgn        : data group number
 * @param[in]   bSet        : true or false value to set 
 *
 * @return      none
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC VOID
FSR_STL_SetMergeVictimGrp(STLZoneObj *pstZone,
                          BADDR       nDgn,
                          BOOL32      bSet)
{
    INT32           nFoundIdx;
    STLDirHdrHdl   *pstDH;
    STLCtxInfoHdl  *pstCI;
    POFFSET         nMetaPOffs;
    INT32           nByteOffs;
    INT32           nBitOffs;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get directory header object pointer */
    pstDH = pstZone->pstDirHdrHdl;

    /* get context info object pointer */
    pstCI = pstZone->pstCtxHdl;

    /* search nDgn in directory */
    nFoundIdx = FSR_STL_SearchPMTDir(pstZone, nDgn, &nMetaPOffs);

    if (nFoundIdx >= 0)   /* found */
    {
        /* get byte and bit offset of that entry */
        nByteOffs = nFoundIdx >> 3;
        nBitOffs  = 7 - (nFoundIdx & 0x07);

        /* set victim flag bit */
        if (bSet == TRUE32)
        {
            *(pstDH->pPMTMergeFlags + nByteOffs) |= (1 << nBitOffs);
            pstCI->pstFm->nMergeVictimFlag = 1;
        }
        else
        {
            *(pstDH->pPMTMergeFlags + nByteOffs) &= ~(1 << nBitOffs);
            pstCI->pstFm->nMergeVictimFlag = 0;
        }

        /* set merge victim group for sudden power off */
        pstCI->pstFm->nMergeVictimDgn = nDgn;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}

/**
 * @brief           This function searches merge victim log group.
 *
 * @param[in]       pstZone     : zone object pointer
 * @param[in,out]   pnDgn       : search result of data group number
 * @param[out]      pnDirIdx    : PMT directory index which is found
 *
 * @return          TRUE32
 * @return          FALSE32
 *
 * @author          Wonmoon Cheon
 * @version         1.0.0
 *
 */
PUBLIC BOOL32
FSR_STL_SearchMergeVictimGrp   (STLZoneObj     *pstZone,
                                BADDR          *pnDgn,
                                UINT32         *pnDirIdx)
{
    /* get directory header object pointer */
    STLDirHdrHdl       *pstDH           = pstZone->pstDirHdrHdl;
    /* merge victim flags bitstream */
    UINT8              *pMergeFlags;
    STLPMTDirEntry     *pstDE;
    INT32               nByteOffs;
    INT32               nBitOffs;
    BOOL32              bFound          = FALSE32;
    UINT32              nEntryCnt;
    UINT32              nIdx;
    UINT16              nMinCost;
    STLLogGrpHdl       *pstLogGrp;
    UINT8               aBackupFlags[(ACTIVE_LOG_GRP_POOL_SIZE + 7) >> 3];
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get number of entries in PMT directory */
    nEntryCnt = pstDH->pstFm->nPMTEntryCnt;

    if (nEntryCnt == 0)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, bFound));
        return bFound;
    }

    /* set default value */
    pMergeFlags = pstDH->pPMTMergeFlags;
    *pnDgn      = NULL_DGN;
    *pnDirIdx   = 0xFFFF;
    nMinCost    = 0xFFFF;
    nEntryCnt   = pstZone->pstZI->nMaxPMTDirEntry;

    /* get first directory entry */
    pstDE = pstDH->pstPMTDir;

    /* to search inactive groups only, we turn off the victim flag temporarily */
    /* we have to keep original flags */
    FSR_OAM_MEMSET(aBackupFlags, 0x00, ACTIVE_LOG_GRP_POOL_SIZE >> 3);

    /* LRU based victim selection */
    if (pstZone->pstActLogGrpList != NULL)
    {
        pstLogGrp = pstZone->pstActLogGrpList->pstHead;
        nIdx = 0;

        while (pstLogGrp != NULL)
        {
            nByteOffs = pstLogGrp->pstFm->nDgn >> 3;
            nBitOffs = 7 - (pstLogGrp->pstFm->nDgn & 0x07);

            /* if current active group is a merge victim */
            if (*(pMergeFlags + nByteOffs) & (1 << nBitOffs))
            {
                /* backup original one */
                aBackupFlags[nIdx >> 3] |= (1 << (7 - (nIdx & 0x07)));

                /* turn it off */
                *(pMergeFlags + nByteOffs) &= ~(1 << nBitOffs);
            }

            pstLogGrp = pstLogGrp->pNext;
            nIdx++;
        }
    }

    /* search from the first log group */
    for (nIdx = 0; nIdx < nEntryCnt; nIdx++)
    {
        nByteOffs = nIdx >> 3;
        nBitOffs  = 7 - (nIdx & 0x07);

        if (*(pMergeFlags + nByteOffs) & (1 << nBitOffs))
        {
            /* find the minimum cost */
            if (pstDE->nCost < nMinCost)
            {
                nMinCost = pstDE->nCost;

                /* set result DGN */
                bFound      = TRUE32;
                *pnDgn      = (BADDR)nIdx;
                *pnDirIdx   = nIdx;
            }
        }

        pstDE++;
    }

    /* we have to recover the original victim flags */
    if (pstZone->pstActLogGrpList != NULL)
    {
        pstLogGrp = pstZone->pstActLogGrpList->pstHead;
        nIdx = 0;

        while (pstLogGrp != NULL)
        {
            nByteOffs = pstLogGrp->pstFm->nDgn >> 3;
            nBitOffs  = 7 - (pstLogGrp->pstFm->nDgn & 0x07);

            /* if current active group is a merge victim */
            if (aBackupFlags[nIdx >> 3] & (1 << (7 - (nIdx & 0x07))))
            {
                /* recover original one, turn it on */
                *(pMergeFlags + nByteOffs) |= (1 << nBitOffs);
            }

            pstLogGrp = pstLogGrp->pNext;
            nIdx++;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, bFound));
    return bFound;
}

/**
 * @brief       This function searches log group in the specified list.
 *
 * @param[in]   pstLogGrpList    : log group list
 * @param[in]   nDgn             : search target data group number
 *
 * @return      Search result of log group object pointer
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC STLLogGrpHdl*
FSR_STL_SearchLogGrp   (STLLogGrpList  *pstLogGrpList,
                        BADDR           nDgn)
{
    STLLogGrpHdl   *pstTempLogGrp;
    STLLogGrpHdl   *pstLogGrp;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* set default output */
    pstLogGrp = NULL;
    
    if (pstLogGrpList != NULL)
    {
        pstTempLogGrp = pstLogGrpList->pstHead;

        /*  sequential search */
        while (pstTempLogGrp != NULL)
        {
            if (pstTempLogGrp->pstFm->nDgn == nDgn)
            {
                pstLogGrp = pstTempLogGrp;
                break;
            }

            pstTempLogGrp = pstTempLogGrp->pNext;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return pstLogGrp;
}

/**
 * @brief       This function allocates new log group from the specified log group list.
 *
 * @param[in]   pstZone          : zone object
 * @param[in]   pstLogGrpList    : log group list
 *
 * @return      Allocated log group object pointer
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC STLLogGrpHdl*
FSR_STL_AllocNewLogGrp (STLZoneObj     *pstZone,
                        STLLogGrpList  *pstLogGrpList)
{
    INT32           nIdx;
    INT32           nGrpPoolSize    = 0;
    STLLogGrpHdl   *pstLogGrp;
    STLLogGrpHdl   *pstTempLogGrp   = NULL;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    pstLogGrp = NULL;

    if (pstLogGrpList == pstZone->pstActLogGrpList)
    {
        pstTempLogGrp = pstZone->pstActLogGrpPool;
        nGrpPoolSize = ACTIVE_LOG_GRP_POOL_SIZE; 
    }
    else if (pstLogGrpList == pstZone->pstInaLogGrpCache)
    {
        pstTempLogGrp = pstZone->pstInaLogGrpPool;
        nGrpPoolSize = INACTIVE_LOG_GRP_POOL_SIZE;
    }
    else
    {
        FSR_ASSERT(FALSE32);
    }

    for (nIdx = nGrpPoolSize; nIdx > 0; nIdx--)
    {
        if (pstTempLogGrp->pstFm->nDgn == NULL_VBN)
        {
            /*  success to find available log group */
            pstLogGrp = pstTempLogGrp;
            break;
        }
        pstTempLogGrp++;
    }

    if (pstLogGrp == NULL)
    {
        /*  there is no available log group in the pool */
        return NULL;
    }

    /* initialize pstLogGrp object */
    FSR_STL_InitLogGrp(pstZone, pstLogGrp);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return pstLogGrp;
}

/**
 * @brief       This function adds new log group to the specified log group list.
 *
 * @param[in]   pstLogGrpList   : log group list
 * @param[in]   pstLogGrp       : log group object to add.
 *
 * @return      none
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC VOID
FSR_STL_AddLogGrp  (STLLogGrpList  *pstLogGrpList,
                    STLLogGrpHdl   *pstLogGrp)
{
    STLLogGrpHdl   *pstCurHead;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get current head pointer of log group list */
    pstCurHead = pstLogGrpList->pstHead;

    /* set link pointers to null */
    pstLogGrp->pPrev = NULL;
    pstLogGrp->pNext = NULL;

    if (pstLogGrpList->pstTail == NULL)
    {
        /* when there is no entries, this is the first time to add */
        pstLogGrpList->pstTail = pstLogGrp;
    }

    pstLogGrp->pNext = pstCurHead;
    if (pstCurHead != NULL)
    {
        /* when there is no entries, this is the first time to add */
        pstCurHead->pPrev = pstLogGrp;
    }
    pstLogGrp->pPrev = NULL;

    /* always, insert at head */
    pstLogGrpList->pstHead = pstLogGrp;

    /* increase number of log groups in this list */
    pstLogGrpList->nNumLogGrps++;

    FSR_ASSERT(pstLogGrpList->pstTail != NULL);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}

/**
 * @brief       This function moves the specified log group to head node of the list.
 *
 * @param[in]   pstLogGrpList   : log group list
 * @param[in]   pstLogGrp       : log group object to move
 *
 * @return      none
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC VOID
FSR_STL_MoveLogGrp2Head(STLLogGrpList  *pstLogGrpList,
                        STLLogGrpHdl   *pstLogGrp)
{
    STLLogGrpHdl   *pstCurHead;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    FSR_ASSERT(pstLogGrpList != NULL);
    FSR_ASSERT(pstLogGrp != NULL);

    pstCurHead = pstLogGrpList->pstHead;

    if (pstCurHead == pstLogGrp)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
            (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
        return;
    }

    /*  node removal */
    if (pstLogGrp->pPrev != NULL)
    {
        pstLogGrp->pPrev->pNext = pstLogGrp->pNext;
    }
    if (pstLogGrp->pNext != NULL)
    {
        pstLogGrp->pNext->pPrev = pstLogGrp->pPrev;
    }

    if (pstLogGrpList->pstTail == pstLogGrp)
    {
        pstLogGrpList->pstTail = pstLogGrp->pPrev;
    }
    
    /*  insert at head */
    pstLogGrp->pNext = pstCurHead;
    if (pstCurHead != NULL)
    {
        pstCurHead->pPrev = pstLogGrp;
    }
    pstLogGrp->pPrev = NULL;
    
    pstLogGrpList->pstHead = pstLogGrp;

#if (OP_STL_DEBUG_CODE == 1)
    FSR_ASSERT(pstLogGrpList->pstTail != NULL);
    if (pstLogGrpList->nNumLogGrps > 1)
    {
        FSR_ASSERT(pstLogGrpList->pstTail->pPrev != NULL);
    }
#endif

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}

/**
 * @brief       This function removes the specified log group from list
 *
 * @param[in]   pstZone         : zone object pointer
 * @param[in]   pstLogGrpList   : log group list
 * @param[in]   pstLogGrp       : log group object to remove
 *
 * @return      none
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC VOID
FSR_STL_RemoveLogGrp   (STLZoneObj     *pstZone,
                        STLLogGrpList  *pstLogGrpList,
                        STLLogGrpHdl   *pstLogGrp)
{
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    FSR_ASSERT(pstLogGrpList != NULL);
    FSR_ASSERT(pstLogGrp != NULL);
    
    /* node removal */
    if (pstLogGrp->pPrev != NULL)
    {
        pstLogGrp->pPrev->pNext = pstLogGrp->pNext;
    }
    if (pstLogGrp->pNext != NULL)
    {
        pstLogGrp->pNext->pPrev = pstLogGrp->pPrev;
    }

    if (pstLogGrpList->pstHead == pstLogGrp)
    {
        pstLogGrpList->pstHead = pstLogGrp->pNext;
    }

    if (pstLogGrpList->pstTail == pstLogGrp)
    {
        pstLogGrpList->pstTail = pstLogGrp->pPrev;
    }

    /* decrease number of log groups in this list */
    pstLogGrpList->nNumLogGrps--;

    /* initialize pstLogGrp object */
    FSR_STL_InitLogGrp(pstZone, pstLogGrp);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}

/**
 * @brief       This function selects a victim log group to be replaced
 *
 * @param[in]   pstLogGrpList   : log group list
 * @param[in]   nSelfDgn        : self log group object not to be victim
 *
 * @return      Victim target log group
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC STLLogGrpHdl*
FSR_STL_SelectVictimLogGrp (STLLogGrpList  *pstLogGrpList,
                            BADDR           nSelfDgn)
{
    STLLogGrpHdl   *pstLogGrp;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    FSR_ASSERT(pstLogGrpList->nNumLogGrps > 0);
    FSR_ASSERT(pstLogGrpList->pstTail != NULL);

    /* set default output */
    pstLogGrp = NULL;

    if (pstLogGrpList->nNumLogGrps == 1)
    {
        nSelfDgn = NULL_DGN;
    }

    /* tail item is the oldest */
    pstLogGrp = pstLogGrpList->pstTail;

    /* LRU based victim selection excluding self data group */
    while ((pstLogGrp != NULL) && (pstLogGrp->pstFm->nDgn == nSelfDgn))
    {
        /* move to previous log group */
        pstLogGrp = pstLogGrp->pPrev;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return pstLogGrp;
}

#if (OP_SUPPORT_CLOSE_TIME_UPDATE_FOR_FASTER_OPEN == 1)
/**
 * @brief       This function flushes all meta info such as PMT Ctx info.
 *              For faster open, new meta block is allocated, and BU
 *              block is erased.
 *
 * @param[in]   pstZone      : Zone object
 *
 * @return      
 *
 * @author      SangHoon Choi
 * @version     1.1.0
 *
 */
PUBLIC VOID
FSR_STL_FlushAllMetaInfo     (STLZoneObj     *pstZone)
{
    STLLog         *pstLog = NULL;
    STLLogGrpHdl   *pstLogGrp;
    STLLogGrpHdl   *pstPrevLogGrp;
    STLLogGrpList  *pstLogGrpList;    
    INT32           nRet                = FSR_STL_SUCCESS;
    BOOL32          bMergeVictimGrp;
    BOOL32          bInactivated        = FALSE32;
    BOOL32          bMetaWL         = FALSE32;

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    STLCtxInfoHdl  *pstCI               = pstZone->pstCtxHdl;
    STLCtxInfoFm   *pstCIFm             = pstCI->pstFm;
    RBWDevInfo     *pstDVI              = NULL;
    STLBUCtxObj    *pstBUCtx;
    BADDR           nBBlkVbn;
    UINT32          nBBlkEC;
    UINT32		    nPgsPerSBlk;
    BOOL32          nIsFlushed          = FALSE32;
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1) */
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    pstBUCtx        = pstZone->pstBUCtxObj;
    pstDVI          = pstZone->pstDevInfo;
    nPgsPerSBlk     = pstDVI->nPagesPerSBlk;

    if (pstDVI->nDeviceType == RBW_DEVTYPE_MLC)
    {
        nPgsPerSBlk >>= 1;
    }
#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRIT */


#if (OP_SUPPORT_RUNTIME_PMT_BUILDING == 1)
    /* 
    * Update Mapping information 
    * If the active log block is not scanned,
    * invalid information will be stored on the meta block.
    * So, when OP_SUPPORT_RUNTIME_PMT_BUILDING is activated,
    * scanning all the active log group should be done.
    */
    /* Set active log group list */
    pstLogGrpList = pstZone->pstActLogGrpList;

    /* tail item is the oldest */
    pstLogGrp = pstLogGrpList->pstTail;

    while (pstLogGrp != NULL)
    {        
        /* Execute Run time scanning */
        nRet = FSR_STL_ScanActLogBlock(pstZone,pstLogGrp->pstFm->nDgn);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR, 
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
            break;
        }
        pstLogGrp = pstLogGrp->pPrev;
    }
#endif /*OP_SUPPORT_RUNTIME_PMT_BUILDING*/

    /*
    * Buffer flushing should be done after the active log scanning is completed.
    * If not, it can write buffer page on the wrong page
    */
#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1 && OP_SUPPORT_BU_DELAYED_FLUSH == 1)
    /* write the previous misaligned sectors into log block */
    if (pstBUCtx->nBufState    != STL_BUFSTATE_CLEAN)
    {
        /* flush currently buffered single page into log block */
        nRet = FSR_STL_FlushBufferPage(pstZone);
        if (nRet != FSR_STL_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return ;
        }
        /* If the BU block is flushed, then it must erase the BU block to decrease the OPEN time */
        nIsFlushed = TRUE32;
    }

    /* 
    * If the state of BU is CLEAN and the CPOffset is more than 0 and less than 64,
    * it means the BU object has invalid page in the block. 
    * In that case, we erase the BU block to decrease the buffer block scanning time.
    */ 
    if ((nIsFlushed == TRUE32) ||
        ((pstBUCtx->nBBlkCPOffset > 0) && (pstBUCtx->nBBlkCPOffset < nPgsPerSBlk)
        && (pstBUCtx->nBufState == STL_BUFSTATE_CLEAN)))
    {
        FSR_ASSERT(pstCIFm->nBBlkVbn != NULL_VBN);
        if (pstCIFm->nBBlkEC > pstCI->pFBlksEC [pstCIFm->nFreeListHead])
        {
            /* Swap VBNs & ECs */
            nBBlkVbn = pstCI->pFreeList[pstCIFm->nFreeListHead];
            nBBlkEC  = pstCI->pFBlksEC [pstCIFm->nFreeListHead] + 1;

            pstCI->pFreeList[pstCIFm->nFreeListHead] = pstCIFm->nBBlkVbn;
            pstCI->pFBlksEC[pstCIFm->nFreeListHead]  = pstCIFm->nBBlkEC;

            pstCIFm->nBBlkVbn = nBBlkVbn;
            pstCIFm->nBBlkEC  = nBBlkEC;
        }
        else
        {
            pstCIFm->nBBlkEC++;
            nBBlkVbn = pstCIFm->nBBlkVbn;
            nBBlkEC  = pstCIFm->nBBlkEC;
        }
        FSR_ASSERT(nBBlkVbn != NULL_VBN);

        /* Get buffer blocks' VBN */
        nRet = FSR_STL_FlashErase(pstZone, nBBlkVbn);
        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:ERR] --%s() L(%d): 0x%08x\r\n"),
                    __FSR_FUNC__, __LINE__, nRet));
            return ;
        }

        /* initialize BU Object */
        pstBUCtx->nBBlkCPOffset     = 0;    /* 1 plus index of the last page of the BU block */
        pstBUCtx->nBufferedLpn      = NULL_VPN;
        pstBUCtx->nBufferedDgn      = NULL_DGN;
        pstBUCtx->nBufferedVpn      = NULL_VPN;
        pstBUCtx->nBufState         = STL_BUFSTATE_CLEAN;
        pstBUCtx->nValidSBitmap     = 0x00;
    }


#endif  /* (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1 && OP_SUPPORT_BU_DELAYED_FLUSH == 1) */

    /* Set active log group list */
    pstLogGrpList = pstZone->pstActLogGrpList;

    /* tail item is the oldest */
    pstLogGrp = pstLogGrpList->pstTail;

    while (pstLogGrp != NULL)
    {
        bInactivated = FALSE32;
        /* Set the pointer of previous active log group */
        pstPrevLogGrp = pstLogGrp->pPrev;

        /* 
         * If the meta block has two or more mappig pages, then
         * we allocated another block and write the first page with the mapping information.
         * The mapping information written here is for the last log group information.
         * We can know if the log group information is last by checking the value pstPrevLogGrp.
         */
        if ((pstPrevLogGrp == NULL) && 
            (pstZone->pstDirHdrHdl->pstFm->nLatestPOffs > 
             pstZone->pstZI->nNumDirHdrPgs + MAX_META_PAGES_FOR_NEW_ALLOCATION))
        {
            pstZone->pstDirHdrHdl->pstFm->nCPOffs = 0;
            nRet = FSR_STL_StoreDirHeader(pstZone,
                                          FALSE32,
                                          &bMetaWL);
        }
        else if(pstPrevLogGrp != NULL)
        {
            /* 
             * If the last active log group doesn't have active log block,
             * then we don't inactivate log blocks in the active log group.
             * This means storing mapping information is not needed in that case.
             * But, we still need to allocate new meta block and wriate at least
             * one mapping information in the allocated new block. 
             * To allocate and write mapping information, when the last log group doesn't have
             * any active log block, we check the previous log block of pstPrevLogGrp
             * to check if the number of log blocks of the last log group is 0.
             * If the number of log block of the last log group is 0
             * we allocate new meta block and write mapping information.
             */
            if (pstPrevLogGrp->pPrev != NULL)
            {
                if ((pstPrevLogGrp->pPrev->pstFm->nNumLogs == 0) && 
                    (pstZone->pstDirHdrHdl->pstFm->nLatestPOffs > 
                     pstZone->pstZI->nNumDirHdrPgs + MAX_META_PAGES_FOR_NEW_ALLOCATION))
                {
                    pstZone->pstDirHdrHdl->pstFm->nCPOffs = 0;
                    nRet = FSR_STL_StoreDirHeader(pstZone,
                                                  FALSE32,
                                                  &bMetaWL);
                }
            }
        }

        /* 
        * If there isn't any log block in the active log group,
        * we don't try to inactivate log blocks in the active log group.
        */
        if(pstLogGrp->pstFm->nNumLogs != 0)
        {            
            /** 
            * In this case, pstVictimLogGrp always exists in the PMTDir[].
            * check if the victim group can be merge victim or not.
            */
            while (TRUE32)
            {                
                /* 2) select victim log to be inactive */
                pstLog = FSR_STL_SelectVictimLog(pstLogGrp, TRUE32);
                if (pstLog == NULL)
                {
                    break;
                } 

                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_INF,
                               (TEXT("[SIF:INF] INACTIVATED LOG BLOCKS nDgn(%5d) nVbn(%5d) nCPOffs(%3d)\r\n"),
                                pstLogGrp->pstFm->nDgn, pstLog->nVbn, pstLog->nCPOffs));
                FSR_STL_RemoveActLogList(pstZone, pstLogGrp->pstFm->nDgn, pstLog);                
                bInactivated = TRUE32;
            }

            /** 
            * In this case, pstVictimLogGrp always exists in the PMTDir[].
            * check if the victim group can be merge victim or not.
            */
            nRet = FSR_STL_CheckMergeVictimGrp(pstZone, pstLogGrp, FALSE32, &bMergeVictimGrp);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
                    (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
                return ;
            }

            if (bMergeVictimGrp == TRUE32)
            {
                FSR_STL_SetMergeVictimGrp(pstZone, pstLogGrp->pstFm->nDgn, TRUE32);
            }
        }
       
        /* Inactive only when the log block has at least one log block */
        if (bInactivated == TRUE32)
        {
            /* Reserve meta page */
            nRet = FSR_STL_ReserveMetaPgs(pstZone, 1, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                __FSR_FUNC__, __LINE__, nRet));
                break;
            }    

            /* store PMT+CXT, update PMTDir */
            nRet = FSR_STL_StorePMTCtx(pstZone, pstLogGrp, FALSE32);

            if (nRet != FSR_STL_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_STL_LOG | FSR_DBZ_ERROR,
                    (TEXT("[SIF:OUT]  --%s() L(%d) : 0x%08x\r\n"),
                        __FSR_FUNC__, __LINE__, nRet));
                return;
            }

            /* check if all logs in the log group are inactive or not */
            FSR_ASSERT(FSR_STL_CheckFullInaLogs(pstLogGrp) == TRUE32);
        }       

        FSR_STL_RemoveLogGrp(pstZone, pstZone->pstActLogGrpList, pstLogGrp);
        pstLogGrp = pstPrevLogGrp;
    }
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG|FSR_DBZ_ERROR,
                   (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return ;
}
#endif /*OP_SUPPORT_CLOSE_TIME_UPDATE_FOR_FASTER_OPEN*/

/**
 * @brief       This function adds new log block into the specified log group
 *
 * @param[in]   pstZone      : zone object
 * @param[in]   pstLogGrp    : log group 
 * @param[in]   nLbn         : logical block number to write in this log
 * @n                          Note that this parameter is necessary for POR.
 * @param[in]   nVbn         : log blocks' VBN to add
 * @param[in]   nEC          : erase count 
 *
 * @return      added log pointer
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC STLLog*
FSR_STL_AddNewLog  (STLZoneObj     *pstZone,
                    STLLogGrpHdl   *pstLogGrp,
                    BADDR           nLbn,
                    BADDR           nVbn,
                    UINT32          nEC)
{
    const RBWDevInfo   *pstDev      = pstZone->pstDevInfo;
    STLCtxInfoHdl      *pstCtx      = pstZone->pstCtxHdl;
    /* get root info object */
    STLRootInfo        *pstRI       = pstZone->pstRI;
    UINT32              nIdx;
    INT32               i;
    STLLog             *pstLog      = NULL;
    STLLog             *pstTempLog;
    UINT16              nNumDBlks;
    BADDR              *pLbnList;
    UINT16             *pLogVPgCnt;
    UINT32              nVPgCnt;
#if (OP_SUPPORT_DATA_WEAR_LEVELING == 1)
#if (OP_STL_DEBUG_CODE == 1)
    INT32               nDirIdx;
#endif
    BADDR               nWLGrpIdx;
    BADDR               nMetaPOffs;
    STLDirHdrHdl       *pstDH;
#endif  /* (OP_SUPPORT_DATA_WEAR_LEVELING == 1) */
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /*  search empty log memory in log memory pool in the log group */
    for (nIdx = 0; nIdx < pstRI->nK; nIdx++)
    {
        pstLog = pstLogGrp->pstLogList + nIdx;

        if (pstLog->nState == LS_FREE)
        {
            pstLog->nVbn    = nVbn;
            pstLog->nState  = LS_ALLOC;
            break;
        }
    }

    FSR_ASSERT(nIdx == pstLog->nSelfIdx);
    FSR_ASSERT(nIdx <  pstRI->nK);

    do
    {
        /*  add new LBN  */
        nNumDBlks = *(pstLogGrp->pNumDBlks + nIdx);
        pLbnList  = pstLogGrp->pLbnList + (nIdx << pstRI->nNShift);
        for (i = 0; i < nNumDBlks; i++)
        {
            if (*(pLbnList + i) == nLbn)
            {
                break;
            }
        }

        /*  fail to find nLbn */
        if (i == nNumDBlks)
        {
            *(pLbnList + nNumDBlks++) = nLbn;
            *(pstLogGrp->pNumDBlks + nIdx) = nNumDBlks;
        }

        if (pstLogGrp->pstFm->nTailIdx == NULL_LOGIDX)
        {
            pstLogGrp->pstFm->nTailIdx = (UINT8)(nIdx);
        }

#if (OP_SUPPORT_DATA_WEAR_LEVELING == 1)
        pstLog->nEC = nEC;

        /* update minimum erase count (Round 1) */
        if (pstLogGrp->pstFm->nMinEC > nEC)
        {
            pstLogGrp->pstFm->nMinEC  = nEC;
            pstLogGrp->pstFm->nMinVbn = nVbn;
            pstLogGrp->pstFm->nMinIdx = pstLog->nSelfIdx;

            /* get directory header pointer */
            pstDH = pstZone->pstDirHdrHdl;

            /* directory offset */
#if (OP_STL_DEBUG_CODE == 1)
            nDirIdx = FSR_STL_SearchPMTDir(pstZone, pstLogGrp->pstFm->nDgn, &nMetaPOffs);
#else
            FSR_STL_SearchPMTDir(pstZone, pstLogGrp->pstFm->nDgn, &nMetaPOffs);
#endif
            
            /* search success */
            {
                /* find the WL group index */
                nWLGrpIdx   = (UINT16)(pstLogGrp->pstFm->nDgn >> DEFAULT_PMT_EC_GRP_SIZE_SHIFT);

                /* if the current log is the minimum EC block in its WL group (Round 2) */
                if (pstDH->pstPMTWLGrp[nWLGrpIdx].nMinEC > nEC)
                {
                    pstDH->pstPMTWLGrp[nWLGrpIdx].nDgn      = pstLogGrp->pstFm->nDgn;
                    pstDH->pstPMTWLGrp[nWLGrpIdx].nMinEC    = nEC;
                    pstDH->pstPMTWLGrp[nWLGrpIdx].nMinVbn   = nVbn;

                    /* if the current log is the globally minimum EC block (Round 3) */
                    if (pstDH->pstPMTWLGrp[pstDH->pstFm->nMinECPMTIdx].nMinEC > nEC)
                    {
                        FSR_STL_UpdatePMTDirMinECGrp(pstZone, pstDH);
                    }

                    /* Update Context */
                    pstCtx->pstFm->nMinECPMTIdx                 = pstDH->pstFm->nMinECPMTIdx;
                    pstCtx->pstFm->nUpdatedPMTWLGrpIdx          = nWLGrpIdx;
                    pstCtx->pstFm->stUpdatedPMTWLGrp.nDgn       = pstDH->pstPMTWLGrp[nWLGrpIdx].nDgn;
                    pstCtx->pstFm->stUpdatedPMTWLGrp.nMinVbn    = pstDH->pstPMTWLGrp[nWLGrpIdx].nMinVbn;
                    pstCtx->pstFm->stUpdatedPMTWLGrp.nMinEC     = pstDH->pstPMTWLGrp[nWLGrpIdx].nMinEC;
                }
            }
        }
#endif

        /* initial state : when first log is added */
        if (pstLogGrp->pstFm->nNumLogs == 0)
        {
            pstLogGrp->pstFm->nMinVPgLogIdx = pstLog->nSelfIdx;
        }
        
        /*  insert the log in the linked list */
        pstLog->nNextIdx = pstLogGrp->pstFm->nHeadIdx;
        if (pstLogGrp->pstFm->nHeadIdx != NULL_LOGIDX)
        {
            pstTempLog = pstLogGrp->pstLogList + pstLogGrp->pstFm->nHeadIdx;
            pstTempLog->nPrevIdx = (UINT8)(nIdx);
        }
        pstLog->nPrevIdx = NULL_LOGIDX;

        pstLogGrp->pstFm->nHeadIdx = (UINT8)(nIdx);
        pstLogGrp->pstFm->nNumLogs++;

    } while (0);

    /* Update the index of log blk which has the minimum valid pages */
    pLogVPgCnt = pstLogGrp->pLogVPgCnt
               + ((pstLogGrp->pstFm->nMinVPgLogIdx) << pstDev->nNumWaysShift);
    nVPgCnt    = 0;
    for (nIdx = 0; nIdx < pstDev->nNumWays; nIdx++)
    {
        nVPgCnt += pLogVPgCnt[nIdx];
    }
    if (nVPgCnt > 0)
    {
        pstLogGrp->pstFm->nMinVPgLogIdx = pstLog->nSelfIdx;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return pstLog;
}

/**
 * @brief       This function searches whether the specified VBN exists in the group.
 *
 * @param[in]   pstLogGrp       : log group 
 * @param[in]   nVbn            : log block VBN to search in the group
 * @param[in]   pstLog          : log object
 *
 * @return      TRUE32          : search found
 * @return      FALSE32         : search fail
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC BOOL32   
FSR_STL_SearchLog  (STLLogGrpHdl   *pstLogGrp,
                    BADDR           nVbn,
                    STLLog        **pstLog)
{
    STLLog     *pstLogs;
    STLLog     *pstTempLog;
    UINT8       nLogIdx;
    BOOL32      bFound;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get log list */
    pstLogs = pstLogGrp->pstLogList;

    /* set default output */
    *pstLog = NULL;
    bFound  = FALSE32;

    /* get head index of list */
    nLogIdx = pstLogGrp->pstFm->nHeadIdx;

    while (nLogIdx != NULL_LOGIDX)
    {
        pstTempLog = pstLogs + nLogIdx;

        if (pstTempLog->nVbn == nVbn)
        {
            /* found */
            *pstLog = pstTempLog;
            bFound  = TRUE32;
            break;
        }

        /* move to the next log */
        nLogIdx = pstTempLog->nNextIdx;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, bFound));
    return bFound;
}

/**
 * @brief       This function removes the specified log from log group
 *
 * @param[in]   pstLogGrp       : log group 
 * @param[in]   pstLog          : log object
 *
 * @return      none
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC VOID
FSR_STL_RemoveLog  (STLZoneObj     *pstZone,
                    STLLogGrpHdl   *pstLogGrp,
                    STLLog         *pstLog)
{
    STLLogGrpFm        *pstLogGrpFm     = pstLogGrp->pstFm;
    STLLog             *pstLogs         = pstLogGrp->pstLogList;
    STLLog             *pstTempLog;
    STLLog             *pstHeadLog;
    STLLog             *pstTailLog;
    STLLog             *pstPrevLog;
    STLLog             *pstNextLog;
    UINT32              nTempIdx;
#if (OP_SUPPORT_DATA_WEAR_LEVELING == 1)
    BADDR               nRemovalVbn     = pstLog->nVbn;
#endif  /* (OP_SUPPORT_DATA_WEAR_LEVELING == 1) */
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get head log */
    pstHeadLog  = pstLogs + pstLogGrpFm->nHeadIdx;

    /* get tail log */
    pstTailLog  = pstLogs + pstLogGrpFm->nTailIdx;

    /* Remove log from log list */
    nTempIdx    = pstLogGrpFm->nHeadIdx;
    while (nTempIdx != NULL_LOGIDX)
    {
        pstTempLog = pstLogs + nTempIdx;
        if (pstTempLog == pstLog)
        {
            if (pstHeadLog == pstLog)
            {
                /* change head node */
                pstLogGrpFm->nHeadIdx = pstLog->nNextIdx;
            }
            
            if (pstTailLog == pstLog)
            {
                /* change tail node */
                pstLogGrpFm->nTailIdx = pstLog->nPrevIdx;
            }

            /*  node removal */
            if (pstLog->nPrevIdx != NULL_LOGIDX)
            {
                pstPrevLog = pstLogs + pstLog->nPrevIdx;
                pstPrevLog->nNextIdx = pstLog->nNextIdx;
            }
            if (pstLog->nNextIdx != NULL_LOGIDX)
            {
                pstNextLog = pstLogs + pstLog->nNextIdx;
                pstNextLog->nPrevIdx = pstLog->nPrevIdx;
            }

            /* initialize pstLog object */
            FSR_STL_InitLog(pstZone, pstLogGrp, pstLog);

            pstLogGrpFm->nNumLogs--;
            break;
        }

        nTempIdx = pstTempLog->nNextIdx;
    }

    /* Update the minimum valid page index */
    FSR_STL_UpdatePMTCost(pstZone, pstLogGrp);

#if (OP_STL_DEBUG_CODE == 1)
    if (pstLogGrpFm->nNumLogs > 0)
    {
        FSR_ASSERT(pstLogGrpFm->nMinVPgLogIdx < pstZone->pstRI->nK);
        pstTempLog = &(pstLogGrp->pstLogList[pstLogGrpFm->nMinVPgLogIdx]);
        FSR_ASSERT(pstTempLog->nVbn != NULL_VBN);
    }
    else
    {
        FSR_ASSERT(pstLogGrpFm->nMinVPgLogIdx == NULL_LOGIDX);
    }
#endif  /* (OP_STL_DEBUG_CODE == 1) */

#if (OP_SUPPORT_DATA_WEAR_LEVELING == 1)
    /*  After removing the log, we need to update minimum erase count */
    FSR_STL_UpdatePMTMinEC(pstZone, pstLogGrp, nRemovalVbn);
#endif

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}

/**
 * @brief       This function selects a victim log to be replaced
 *
 * @param[in]   pstLogGrp       : log group 
 * @param[in]   bActiveLog      : flag to select active or inactive log (TRUE32 : active)
 *
 * @return      Victim target log
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC STLLog*
FSR_STL_SelectVictimLog    (STLLogGrpHdl   *pstLogGrp,
                            BOOL32          bActiveLog)
{
    STLLog         *pstVictim;
    STLLog         *pstLogs;
    BOOL32          bState;
    STLLogGrpFm    *pstFm;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get log list */
    pstLogs    = pstLogGrp->pstLogList;

    /* get fixed size member pointer of log group */
    pstFm      = pstLogGrp->pstFm;

    FSR_ASSERT(pstFm->nNumLogs > 0);
    FSR_ASSERT(pstFm->nTailIdx != NULL_LOGIDX);

    pstVictim = pstLogs + pstFm->nTailIdx;
    bState    = ((pstVictim->nState) & (1 << LS_ACTIVE_SHIFT)) ? TRUE32 : FALSE32;

    while (bState != bActiveLog)
    {
        if (pstVictim->nPrevIdx != NULL_LOGIDX)
        {
            pstVictim = pstLogs + pstVictim->nPrevIdx;
            bState    = ((pstVictim->nState) & (1 << LS_ACTIVE_SHIFT)) ? TRUE32 : FALSE32;
        }
        else
        {
            pstVictim = NULL;
            break;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return pstVictim;
}

/**
 * @brief       This function changes log state.
 *
 * @param[in]       pstZone     : zone object
 * @param[in,out]   pstLogGrp   : log group object
 * @param[in,out]   pstLog      : log block to change state
 * @param[in]       nLpn        : write request
 *
 * @return          none
 *
 * @author          Wonmoon Cheon
 * @version         1.0.0
 *
 */

PUBLIC VOID
FSR_STL_ChangeLogState (STLZoneObj     *pstZone,
                        STLLogGrpHdl   *pstLogGrp,
                        STLLog         *pstLog,
                        PADDR           nLpn)
{
    RBWDevInfo     *pstDev;
    UINT32          nPgsPerGrp;
    POFFSET         nLPOffs;
    UINT8           nCurState;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get device object pointer */
    pstDev      = pstZone->pstDevInfo;

    /* get number of pages per log group mapping table */
    nPgsPerGrp  = pstZone->pstML->nPagesPerLGMT;
    nLPOffs     = (POFFSET)(nLpn & (nPgsPerGrp - 1));

    nCurState = (UINT8)(pstLog->nState & LS_STATES_BIT_MASK);

    do
    {
        if (nCurState == LS_RANDOM)
        {
            pstLog->nLastLpo = nLPOffs;
            break;
        }
        
        if (nCurState == LS_ALLOC)
        {
            /* if ((nLpn % pstDev->nPagesPerSBlk) != 0) */
            if ((nLpn & (pstDev->nPagesPerSBlk - 1)) != 0)
            {
                nCurState = LS_RANDOM;
            }
            else
            {
                nCurState = LS_SEQ;
            }
        }
        else if (nCurState == LS_SEQ)
        {
            if ((pstLog->nLastLpo + 1) != nLPOffs)
            {
                nCurState = LS_RANDOM;
            }
    
            if (*(pstLogGrp->pNumDBlks + pstLog->nSelfIdx) > 1)
            {
                nCurState = LS_RANDOM;
            }
        }
        
        pstLog->nState  &= ~(LS_STATES_BIT_MASK);
        pstLog->nState  |= nCurState;
        pstLog->nLastLpo = nLPOffs;
        
    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}

/**
 * @brief       This function finds available log block to write.
 *
 * @param[in]   pstZone      : zone object
 * @param[in]   pstLogGrp    : log group object
 * @param[in]   nLpn         : write request
 * @param[out]  pnNumAvailPgs: number of available clean pages
 *
 * @return      log object that can use to write
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC STLLog*
FSR_STL_FindAvailLog   (STLZoneObj     *pstZone,
                        STLLogGrpHdl   *pstLogGrp,
                        PADDR           nLpn,
                        UINT32         *pnNumAvailPgs)
{
    RBWDevInfo     *pstDev;
    STLLogGrpFm    *pstFm;
    STLLog         *pstLogs;
    STLLog         *pstTempLog;
    STLLog         *pstLog;
    UINT32          nPgsPerSBlk;
    POFFSET         nCPOffs;
    POFFSET         nWayPOffs;
    UINT8           nTempIdx;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* get device info pointer */
    pstDev  = pstZone->pstDevInfo;

    /* get log list */
    pstLogs = pstLogGrp->pstLogList;

    /* get fixed size member pointer of log group */
    pstFm   = pstLogGrp->pstFm;

    /* set default output */
    pstLog          = NULL;
    *pnNumAvailPgs  = 0;

    /* we need to search from head node, because head side logs are recent ones. */
    nTempIdx = pstFm->nHeadIdx;
    while (nTempIdx != NULL_LOGIDX)
    {
        /* get a log in list */
        pstTempLog = pstLogs + nTempIdx;

        /* nPgsPerSBlk can be changed if (OP_SUPPORT_MLC_LSB_ONLY == 1) */
        nPgsPerSBlk = pstDev->nPagesPerSBlk;
        
        /* get its clean page offset */
        nCPOffs = pstTempLog->nCPOffs;
        
#if (OP_SUPPORT_MLC_LSB_ONLY == 1)
        if (pstTempLog->nState & (1 << LS_MLC_FAST_MODE_SHIFT))
        {
            nCPOffs = pstTempLog->nLSBOffs;
        }
#endif

        /*  get actual clean page offset considering way interleaving */
        nWayPOffs = FSR_STL_GetWayCPOffset(pstDev, nCPOffs, nLpn);

#if (OP_SUPPORT_MLC_LSB_ONLY == 1)
        if (pstTempLog->nState & (1 << LS_MLC_FAST_MODE_SHIFT))
        {
            nWayPOffs = pstDev->aLSBPgTblSBlk[nWayPOffs];
            
            /*  in case of MLC, only LSB pages are used. (half of total pages) */
            nPgsPerSBlk >>= 1;        /*  MLC LSB Half Pages Only */
        }
#endif

        if (nWayPOffs < nPgsPerSBlk)
        {
            /* found */
            pstLog          = pstTempLog;
            *pnNumAvailPgs  = nPgsPerSBlk - nWayPOffs;
            break;
        }
        
        /*  move to next log */
        nTempIdx = pstTempLog->nNextIdx;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
    return pstLog;
}

/**
 * @brief       This function adds the specified log into active log list in context.
 *
 * @param[in]       pstZone     : zone object
 * @param[in]       nDgn        : DGN of log block
 * @param[in,out]   pstLog      : log object
 *
 * @return          none
 *
 * @author          Wonmoon Cheon
 * @version         1.0.0
 *
 */
PUBLIC VOID
FSR_STL_AddActLogList(STLZoneObj *pstZone,
                      BADDR       nDgn,
                      STLLog     *pstLog)
{
    STLCtxInfoHdl  *pstCtx;
    STLCtxInfoFm   *pstFm;
    STLActLogEntry *pstALE;
    INT32           nIdx;
    STLLogGrpHdl   *pstCachedInaLogGrp;
#if (OP_STL_DEBUG_CODE == 1)
    STLActLogEntry *pstActLog2;
    BADDR           nVbnI;
    BADDR           nVbnJ;
    INT32           j;
#endif
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* set log state to active */
    pstLog->nState |= (1 << LS_ACTIVE_SHIFT);

    /* get context info object handler */
    pstCtx  = pstZone->pstCtxHdl;

    /* get fixed size members pointer */
    pstFm   = pstCtx->pstFm;

    FSR_ASSERT(pstFm->nNumLBlks < MAX_ACTIVE_LBLKS);

    /* add at tail position of active log list */
    nIdx        = pstFm->nNumLBlks;
    pstALE      = pstCtx->pstActLogList + nIdx;

    /* set active log info */
    pstALE->nDgn = nDgn;
    pstALE->nVbn = pstLog->nVbn;

    /* increase the number of active logs */
    pstFm->nNumLBlks++;

    /* cache consistency */
    /* we need to remove inactive group cahce, if the group exists */
    pstCachedInaLogGrp = FSR_STL_SearchLogGrp(pstZone->pstInaLogGrpCache, nDgn);
    if (pstCachedInaLogGrp != NULL)
    {
        FSR_STL_RemoveLogGrp(pstZone, pstZone->pstInaLogGrpCache, pstCachedInaLogGrp);
    }

#if (OP_STL_DEBUG_CODE == 1)
    /*  debug code */
    for (nIdx = 0; nIdx < pstFm->nNumLBlks; nIdx++)
    {
        pstALE      = pstCtx->pstActLogList + nIdx;
        nVbnI       = pstALE->nVbn;
        
        for (j = 0; j < pstFm->nNumLBlks; j++)
        {
            pstActLog2  = pstCtx->pstActLogList + j;
            nVbnJ       = pstActLog2->nVbn;

            if (nIdx != j && nVbnI == nVbnJ)
            {
                FSR_ASSERT(FALSE32);
            }
        }
    }
#endif

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}

/**
 * @brief       This function removes the log block in the active log list.
 * @n           After this function is called, context saving is required.
 *
 * @param[in]       pstZone     : zone object
 * @param[in]       nDgn        : DGN of log block
 * @param[in,out]   pstLog      : log object
 *
 * @return          none
 *
 * @author          Wonmoon Cheon
 * @version         1.0.0
 *
 */
PUBLIC VOID
FSR_STL_RemoveActLogList(STLZoneObj *pstZone,
                         BADDR       nDgn, 
                         STLLog     *pstLog)
{
    STLCtxInfoHdl  *pstCtx;
    STLCtxInfoFm   *pstFm;
    STLActLogEntry *pstALE;
    STLActLogEntry *pstALELeft;
    STLActLogEntry *pstALERight;
    INT32           nIdx;
    INT32           nFoundIdx;
    UINT16          nNumLBlks;
#if (OP_STL_DEBUG_CODE == 1)
    STLActLogEntry *pstActLog2;
    BOOL32          bFound;
    BADDR           nVbnI;
    BADDR           nVbnJ;
    INT32           j;
#endif
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /* set log state to inactive */
    pstLog->nState &= ~(1 << LS_ACTIVE_SHIFT);

    /* get context info object handler */
    pstCtx  = pstZone->pstCtxHdl;

    /* get fixed size members pointer */
    pstFm   = pstCtx->pstFm;

    nNumLBlks = pstFm->nNumLBlks;
    nFoundIdx = 0;
#if (OP_STL_DEBUG_CODE == 1)
    bFound    = FALSE32;
#endif

    FSR_ASSERT(nNumLBlks >= 1);

    /**
     * In the pstActLogList[] array, ordering is important. the first comer is 
     * placed at lower index.
     */
    for (nIdx = 0; nIdx < nNumLBlks; nIdx++)
    {
        pstALE   = pstCtx->pstActLogList + nIdx;
        
        if ((pstALE->nVbn == pstLog->nVbn) &&
            (pstALE->nDgn == nDgn))
        {
#if (OP_STL_DEBUG_CODE == 1)
            bFound    = TRUE32;
#endif
            nFoundIdx = nIdx;
            break;
        }
    }

#if (OP_STL_DEBUG_CODE == 1)
    FSR_ASSERT(bFound == TRUE32);
#endif

    /*  move the logs left to one step left shift */
    pstALELeft = pstCtx->pstActLogList + nFoundIdx;
    pstALERight = pstALELeft + 1;
    for (nIdx = nFoundIdx; nIdx < nNumLBlks - 1; nIdx++)
    {
        pstALELeft->nDgn = pstALERight->nDgn;
        pstALELeft->nVbn = pstALERight->nVbn;

        pstALELeft++;
        pstALERight++;
    }

    /*  clear the last item */
    pstALELeft->nDgn = NULL_VBN;
    pstALELeft->nVbn = NULL_VBN;

    /* decrease the number of active logs */
    pstFm->nNumLBlks--;

#if (OP_STL_DEBUG_CODE == 1)
    /*  debug code */
    for (nIdx = 0; nIdx < pstFm->nNumLBlks; nIdx++)
    {
        pstALE      = pstCtx->pstActLogList + nIdx;
        nVbnI       = pstALE->nVbn;

        for (j = 0; j < pstFm->nNumLBlks; j++)
        {
            pstActLog2  = pstCtx->pstActLogList + j;
            nVbnJ       = pstActLog2->nVbn;

            if (nIdx != j && nVbnI == nVbnJ)
            {
                FSR_ASSERT(FALSE32);
            }
        }
    }
#endif

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}

/**
 * @brief       This function refers BMT and PMT to translate LPN to VPN, 
 * @n           and returns the VPN to read
 *
 * @param[in]   pstZone         : zone object
 * @param[in]   nLpn            : LPN address to translate
 * @param[out]  pnVpn           : result VPN value
 * @param[out]  pbDeleted       : page deleted flag 
 *
 * @return      FSR_STL_SUCCESS
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC INT32
FSR_STL_GetLpn2Vpn (STLZoneObj     *pstZone,
                    PADDR           nLpn,
                    PADDR          *pnVpn,
                    BOOL32         *pbDeleted)
{
    RBWDevInfo     *pstDev;
    BADDR           nLbn;
    BADDR           nDgn;
    POFFSET         nLPOffs;
    BOOL32          bExistBMT;
    PADDR           nResultVpn;
    BADDR           nLan;
    INT32           nRet;
    STLBMTHdl      *pstBMT;
    POFFSET         nDBOffs;        /*< data block offset in local area */
    POFFSET         nMetaPOffs;
    STLLogGrpHdl   *pstTempLogGrp;
    INT32           nFoundIdx;
    STLZoneInfo    *pstZI;
    STLBMTEntry    *pBE;
    BADDR           nRndVbn;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /*  get device info pointer */
    pstDev = pstZone->pstDevInfo;

    /* get root info object pointer */
    pstZI  = pstZone->pstZI;

    nRet            = FSR_STL_SUCCESS;
    nResultVpn      = NULL_VPN;
    pstBMT          = NULL;
    pstTempLogGrp   = NULL;

    /*  set deleted mark to FALSE32 */
    *pbDeleted      = FALSE32;

    /* get LBN, DGN, LPO from nLpn */
    nLbn            = (BADDR)(nLpn >> pstDev->nPagesPerSbShift);
    nDgn            = nLbn >> pstZone->pstRI->nNShift;
    nLPOffs         = (POFFSET)(nLpn & (pstZone->pstML->nPagesPerLGMT - 1));

    /* default : the nLpn page exists in PMT */
    bExistBMT       = FALSE32;

    do
    {
        if (bExistBMT == FALSE32)
        {
            /*  check if the LPN exists in the active log group list */
            nResultVpn = _FindVpnInLogGrpList(pstZone,
                                              pstZone->pstActLogGrpList,
                                              nDgn, nLPOffs);
            if (nResultVpn != NULL_VPN)
            {
                /* success to find in active log */
                *pnVpn = nResultVpn;
                break;
            }
            else
            {
                /*  check if the LPN exists in the inactive log group cache */
                nResultVpn = _FindVpnInLogGrpList(pstZone,
                                                  pstZone->pstInaLogGrpCache,
                                                  nDgn, nLPOffs);
                if (nResultVpn != NULL_VPN)
                {
                    /* success to find in inactive log cache */
                    *pnVpn = nResultVpn;
                    break;
                }
                else
                {
                    /*  otherwise, check the PMT directory */
                    nFoundIdx = FSR_STL_SearchPMTDir(pstZone, nDgn, &nMetaPOffs);
                    if (nFoundIdx >= 0)
                    {
                        /* check if the group exists in meta page buffer */
                        pstTempLogGrp = &(pstZone->pstPMTHdl->astLogGrps[nDgn % pstZI->nNumLogGrpPerPMT]);
                        nRet = FSR_STL_LoadPMT(pstZone, nDgn, nMetaPOffs, &pstTempLogGrp, FALSE32);
                        if (nRet != FSR_STL_SUCCESS)
                        {
                            break;
                        }

                        /* look up log page mapping table to get VPN */
                        nResultVpn = _GetVpnFromPMT(pstDev, pstTempLogGrp, nLPOffs);

                        if (nResultVpn != NULL_VPN)
                        {
                            /* success to find */
                            *pnVpn = nResultVpn;
                            break;
                        }
                    }
                }
            }
        }

        /* BMT object handler */
        pstBMT = pstZone->pstBMTHdl;

        /*  load the BMT */
        nLan    = (BADDR)  (nLbn >> pstZI->nDBlksPerLAShift);
        /* nDBOffs = (POFFSET)(nLbn % pstZI->nDBlksPerLA); */
        nDBOffs = (POFFSET)(nLbn & (pstZI->nDBlksPerLA - 1));
        /* nLPOffs = (POFFSET)(nLpn % pstDev->nPagesPerSBlk); */
        nLPOffs = (POFFSET)(nLpn & (pstDev->nPagesPerSBlk -1));

        if (nLan != pstBMT->pstFm->nLan)
        {
            nRet = FSR_STL_LoadBMT(pstZone, nLan, FALSE32);
            if (nRet != FSR_STL_SUCCESS)
            {
                break;
            }
        }

        /* get BMT entry pointer */
        pBE = pstBMT->pMapTbl + nDBOffs;
        
        if (pBE->nVbn == NULL_VBN)
        {
            /* deleted page */
            *pbDeleted = TRUE32;
            
            /* when there is not data block, read a random page */
            /* let's read current free blocks' first page */
            /* When we test on Prism, this nRndVbn must be not a meta block */
            nRndVbn = pstZone->pstCtxHdl->pFreeList[pstZone->pstCtxHdl->pstFm->nFreeListHead];
            nResultVpn = (nRndVbn << pstDev->nPagesPerSbShift);
        }
        else
        {
            nResultVpn = (pBE->nVbn << pstDev->nPagesPerSbShift);

            /* even though there is data block in BMT, that data block
               could be garbage block */
            if (pstBMT->pGBlkFlags[nDBOffs >> 3] & (1 << (nDBOffs & 0x07)))
            {
                *pbDeleted = TRUE32;
            }
        }

        nResultVpn += nLPOffs;
    } while (0);

    /* set output */
    *pnVpn = nResultVpn;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : 0x%08x\r\n"), __FSR_FUNC__, nRet));
    return nRet; 
}

/**
 * @brief       This function returns the VPN to write in the specified log block.
 *
 * @param[in]       pstZone         : zone object
 * @param[in,out]   pstLog          : pointer to Log object
 * @param[in]       nLpn            : logical page number
 *
 * @return          Clean page number to write in the specified log block 
 *
 * @author          Wonmoon Cheon
 * @version         1.0.0
 *
 */
PUBLIC PADDR
FSR_STL_GetLogCPN(STLZoneObj *pstZone,
                  STLLog     *pstLog,
                  PADDR       nLpn)
{
    POFFSET     nCPOffs;
    POFFSET     nWayCPOffs;
    RBWDevInfo *pstDev;
    PADDR       nVpn;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /*  get device info pointer */
    pstDev = pstZone->pstDevInfo;

    /* get current clean page offset */
    nCPOffs = pstLog->nCPOffs;
    
#if (OP_SUPPORT_MLC_LSB_ONLY == 1)
    if (pstLog->nState & (1 << LS_MLC_FAST_MODE_SHIFT))
    {
        nCPOffs = pstLog->nLSBOffs;
    }
#endif

    /*  get actual clean page offset considering way interleaving */
    nWayCPOffs = FSR_STL_GetWayCPOffset(pstDev, nCPOffs, nLpn);

#if (OP_SUPPORT_MLC_LSB_ONLY == 1)
    if (pstLog->nState & (1 << LS_MLC_FAST_MODE_SHIFT))
    {
        pstLog->nLSBOffset += (nWayCPOffs - nCPOffs);

        FSR_ASSERT(pstLog->nLSBOffs >= 0 && 
                   pstLog->nLSBOffs < (pstDev->nPagesPerSBlk >> 1));

        nWayCPOffs = pstDev->aLSBPgTblSBlk[nWayCPOffs];
    }
    else
    {
        pstLog->nLSBOffs = nWayCPOffs;
    }
#endif

    FSR_ASSERT(nWayCPOffs < pstDev->nPagesPerSBlk);

    /*  update the final clean offset */
    pstLog->nCPOffs = nWayCPOffs;

    /* get VPN */
    nVpn = (pstLog->nVbn << pstDev->nPagesPerSbShift) + pstLog->nCPOffs;
    
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s() : %d\r\n"), __FSR_FUNC__, nVpn));
    return nVpn;
}


/**
 * @brief           This function updates the PMT with new page write
 *
 * @param[in]       pstZone         : zone object
 * @param[in,out]   pstLogGrp       : pointer to log group
 * @param[in]       pstLog          : pointer to log object
 * @param[in]       nLpn            : logical page number
 * @param[in]       nVpn            : virtual page number
 *
 * @return          none 
 *
 * @author          Wonmoon Cheon
 * @version         1.0.0
 *
 */
PUBLIC VOID
FSR_STL_UpdatePMT  (STLZoneObj     *pstZone,
                    STLLogGrpHdl   *pstLogGrp,
                    STLLog         *pstLog,
                    PADDR           nLpn,
                    PADDR           nVpn)
{
    /* get device info pointer */
    const RBWDevInfo   *pstDev      = pstZone->pstDevInfo;
    BADDR               nLbn;
    POFFSET             nLPOffs;
    POFFSET             nGrpPOffs;
    UINT32              nIdx;
    UINT32              nNumDBlks;
    UINT32              nStartIdx;
    UINT32              nLogIdx;
    UINT16             *pLogVPgCnt;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    FSR_ASSERT(pstLog->nVbn == (nVpn >> pstDev->nPagesPerSbShift));

    /* logical offset */
    nLPOffs         = (POFFSET)(nLpn & (pstZone->pstML->nPagesPerLGMT - 1));

    /* physical offset in that group */
    nGrpPOffs       = (POFFSET)((pstLog->nSelfIdx << pstDev->nPagesPerSbShift) + 
                                (nVpn & (pstDev->nPagesPerSBlk - 1)));

    if (pstLogGrp->pMapTbl[nLPOffs] != NULL_POFFSET)
    {
        /* get current log index */
        nLogIdx = (UINT8)((pstLogGrp->pMapTbl[nLPOffs]) >> pstDev->nPagesPerSbShift);
        pLogVPgCnt = pstLogGrp->pLogVPgCnt
                   + (nLogIdx << pstDev->nNumWaysShift)
                   + (nLPOffs & (pstDev->nNumWays - 1));
        FSR_ASSERT(pLogVPgCnt > 0);

        /* decrease num. of valid pages in that log */
        (*pLogVPgCnt)--;
    }

    /* update mapping info */
    pstLogGrp->pMapTbl[nLPOffs] = nGrpPOffs;

    /* increase num. of valid pages in this log */
    pLogVPgCnt = pstLogGrp->pLogVPgCnt
               + (pstLog->nSelfIdx << pstDev->nNumWaysShift)
               + (nLPOffs & (pstDev->nNumWays - 1));
    (*pLogVPgCnt)++;


    nLbn = (BADDR)(nLpn >> pstDev->nPagesPerSbShift);

    /* get current number of data blocks in the log */
    nNumDBlks = *(pstLogGrp->pNumDBlks + pstLog->nSelfIdx);

    /* check if the LBN already exists or not */
    nStartIdx = (pstLog->nSelfIdx << pstZone->pstRI->nNShift);
    for (nIdx = nStartIdx; nIdx < (nStartIdx + nNumDBlks); nIdx++)
    {
        if (pstLogGrp->pLbnList[nIdx] == nLbn)
        {
            break;
        }
    }

    /*  fail to find nLbn */
    if (nIdx == (nStartIdx + nNumDBlks))
    {
        /* add new LBN in the list */
        pstLogGrp->pLbnList[nIdx] = nLbn;

        /* increase the number of data blocks in the log */
        pstLogGrp->pNumDBlks[pstLog->nSelfIdx] = (UINT16)(nNumDBlks + 1);
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}

/**
 * @brief           This function updates the compaction cost of PMT with new page write
 *
 * @param[in]       pstZone         : zone object
 * @param[in,out]   pstLogGrp       : pointer to log group
 *
 * @return          none
 *
 * @author          Wonmoon Cheon
 * @version         1.0.0
 *
 */
PUBLIC VOID
FSR_STL_UpdatePMTCost  (STLZoneObj     *pstZone,
                        STLLogGrpHdl   *pstLogGrp)
{
    /* get device info pointer */
    const RBWDevInfo   *pstDev      = pstZone->pstDevInfo;
    STLCtxInfoFm       *pstCIFm     = pstZone->pstCtxHdl->pstFm;
    STLLogGrpFm        *pstLogGrpFm = pstLogGrp->pstFm;
    STLLog             *pstTempLog;
    UINT32              nIdx;
    UINT32              nTempIdx;
    UINT32              nVPgCnt;
    UINT16             *pLogVPgCnt;
    UINT32              nMinVPgCnt  = (UINT32)(-1);
    STLPMTDirEntry     *pstDE;
    UINT32              nNumCPg;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));


    /* if the minimum log is the old block, we need to update the min log index */
    if (pstLogGrpFm->nNumLogs == 0)
    {
        pstLogGrpFm->nMinVPgLogIdx = NULL_LOGIDX;
        nMinVPgCnt = (UINT16)(-1);
    }
    else if (pstLogGrpFm->nNumLogs == 1)
    {
        FSR_ASSERT(pstLogGrpFm->nHeadIdx < pstZone->pstRI->nK);

        /* When the first page is written */
        pstLogGrpFm->nMinVPgLogIdx = pstLogGrpFm->nHeadIdx;
        nMinVPgCnt = 0x8000;
    }
    else
    {
        /* Find the minimum valid pages */
        nTempIdx   = pstLogGrpFm->nTailIdx;
        nMinVPgCnt = (UINT16)(-1);
        while (nTempIdx != pstLogGrpFm->nHeadIdx)
        {
            FSR_ASSERT(nTempIdx < pstZone->pstRI->nK);

            pstTempLog = pstLogGrp->pstLogList + nTempIdx;
            pLogVPgCnt = pstLogGrp->pLogVPgCnt + (nTempIdx << pstDev->nNumWaysShift);

            nVPgCnt = 0;
            for (nIdx = 0; nIdx < pstDev->nNumWays; nIdx++)
            {
                nVPgCnt += pLogVPgCnt[nIdx];
            }

            if (nVPgCnt < nMinVPgCnt)
            {
                nMinVPgCnt = (UINT16)nVPgCnt;
                pstLogGrpFm->nMinVPgLogIdx = (UINT16)nTempIdx;
            }
            nTempIdx = pstTempLog->nPrevIdx;
        }
        FSR_ASSERT(pstLogGrpFm->nMinVPgLogIdx < pstZone->pstRI->nK);
        FSR_ASSERT(nMinVPgCnt != (UINT16)(-1));

        /* update log group compaction cost */
        pstTempLog = pstLogGrp->pstLogList + pstLogGrpFm->nHeadIdx;
        nNumCPg    = pstDev->nPagesPerSBlk - pstTempLog->nCPOffs;
        if (nNumCPg < nMinVPgCnt)
        {
            nMinVPgCnt = (pstDev->nPagesPerSBlk << 1) / pstLogGrpFm->nNumLogs;
        }
        nMinVPgCnt = (nMinVPgCnt + pstDev->nPagesPerSBlk) - pstLogGrpFm->nNumLogs;
        FSR_ASSERT(nMinVPgCnt < 0x8000);
    }

    pstDE = pstZone->pstDirHdrHdl->pstPMTDir + pstLogGrpFm->nDgn;
    pstDE->nCost = (UINT16)nMinVPgCnt;

    pstCIFm->nModifiedDgn  = pstLogGrpFm->nDgn;
    pstCIFm->nModifiedCost = pstDE->nCost;

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}

#if (OP_SUPPORT_MLC_LSB_ONLY == 1)

/**
 * @brief       This function checks LSB page offset according to the
 * @n           current clean page offset value at pstLog->nCPOffs.
 *
 * @param[in]   pstZone         : zone object
 * @param[in]   pstLog          : pointer to log object
 *
 * @return      none 
 *
 * @author      Wonmoon Cheon
 * @version     1.0.0
 *
 */
PUBLIC VOID
FSR_STL_CheckMLCFastMode(STLZoneObj *pstZone,
                         STLLog     *pstLog)
{
    RBWDevInfo *pstDev      = NULL;
    UINT32      nIdx        = 0;
    UINT32      nPgsPerSBlk;
    FSR_STACK_VAR;
    FSR_STACK_END;
    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:IN ]  ++%s()\r\n"), __FSR_FUNC__));

    /*  get device info pointer */
    pstDev = pstZone->pstDevInfo;

    do
    {
        if (pstDev->nDeviceType != RBW_DEVTYPE_MLC)
        {
            /* there is no work to do when SLC case */
            break;
        }

        if (((pstLog->nState >> LS_MLC_FAST_MODE_SHIFT) == 0) &&
             (pstLog->nCPOffs < pstDev->nLSBModeBranchPgOffset))
        {
            if ((pstLog->nState & LS_STATES_BIT_MASK) == LS_RANDOM)
            {
                /* Only if current state is random, we can enter into LSB only mode. 
                   Because we still want to maximize the ratio of swap merge. */
                pstLog->nState |= (1 << LS_MLC_FAST_MODE_SHIFT);
            }
        }
        
        nPgsPerSBlk = (pstDev->nPagesPerSBlk >> 1);
        for (nIdx = (pstLog->nCPOffs >> 1); nIdx < nPgsPerSBlk; nIdx++)
        {
            if (pstLog->nCPOffs <= pstDev->aLSBPgTblSBlk[nIdx])
            {
                pstLog->nLSBOffs = nIdx;
                break;
            }
        }    
        
    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_STL_LOG,
        (TEXT("[SIF:OUT]  --%s()\r\n"), __FSR_FUNC__));
}

#endif /* (OP_SUPPORT_MLC_LSB_ONLY == 1) */
