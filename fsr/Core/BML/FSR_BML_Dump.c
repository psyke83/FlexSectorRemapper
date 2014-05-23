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
 *  @file       FSR_BML_Dump.c
 *  @brief      This file consists of common FSR_BML functions
 *  @author     SuRuyn Lee
 *  @date       18-JUN-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  18-JUN-2007 [SuRyun Lee] : first writing
 *
 */

/**
 *  @brief  Header file inclusions
 */

#define  FSR_NO_INCLUDE_STL_HEADER
#include "FSR.h"

#include "FSR_BML_Config.h"
#include "FSR_BML_Types.h"
#include "FSR_BML_BIFCommon.h"
#include "FSR_BML_BadBlkMgr.h"
#include "FSR_BML_BBMCommon.h"
#include "FSR_BML_NonBlkMgr.h"
#include "FSR_BML_Dump.h"

#if !defined(FSR_NBL2)
PRIVATE FSRVolDumpHdr       *gpstVolDumpHdr[FSR_MAX_VOLS];
#endif /* FSR_NBL2 */
/**
 *  @brief  Code Implementation
 */

#if !defined(FSR_NBL2)

#if !defined(FSR_OAM_RTLMSG_DISABLE)
PRIVATE const char *gpFSRDumpSeqOrderStr[] = {
    "BML_DUMP_OTPBLK",
    "BML_DUMP_PIBLK",
    "BML_DUMP_DATABLK",
    "BML_DUMP_LSBPG",
    "BML_DUMP_CHK_LASTIMAGE"};

PRIVATE const char *gpFSRDumpDataType[] = {"SLC", "MLC"};
#endif

/**
 *  @brief      This function prints dump status
 *
 *  @param [in]   nDev           : device number
 *  @param [in]   *pstDumpLog    : pointer to BmlDumpLog data structure
 *  @param [in]   *pstVolDumpHdr : pointer to FSRVolDumpHdr data structure
 *  @param [in]   *pstDieDumpHdr : pointer to FSRDieDumpHdr data structure
 *
 *  @author     SongHo Yoon
 *  @version    1.0.0
 *
 */

PRIVATE VOID
_PrintDumpStat(UINT32          nDev,
               BmlDumpLog     *pstDumpLog,
               FSRVolDumpHdr  *pstVolDumpHdr,
               FSRDieDumpHdr  *pstDieDumpHdr)
{
    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   nSeqOrder                            : %s\r\n"), gpFSRDumpSeqOrderStr[pstDumpLog->nSeqOrder]));
    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   nDevNum                              : %d\r\n"), nDev));
    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   nDieNum                              : %d\r\n"), pstDumpLog->nDieIdx));
    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   nPbn                                 : %d\r\n"), pstDumpLog->nPbn));
    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   nPEntryNum                           : %d\r\n"), pstDumpLog->nPEntryNum));
    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   pstVolDumpHdr->nBlksForSLCArea[0][0] : %d\r\n"), pstVolDumpHdr->nBlksForSLCArea[0][0]));
    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   pstVolDumpHdr->nBlksForSLCArea[0][1] : %d\r\n"), pstVolDumpHdr->nBlksForSLCArea[0][1]));
    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   pstVolDumpHdr->nBlksForSLCArea[1][0] : %d\r\n"), pstVolDumpHdr->nBlksForSLCArea[1][0]));
    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   pstVolDumpHdr->nBlksForSLCArea[1][1] : %d\r\n"), pstVolDumpHdr->nBlksForSLCArea[1][1]));
    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   pstVolDumpHdr->nDevCxtSize           : %d\r\n"), pstVolDumpHdr->nDevCxtSize));
    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   pstVolDumpHdr->nHeaderSig            : 0x%x\r\n"), pstVolDumpHdr->nHeaderSig));
    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   pstVolDumpHdr->nNumOfDevs            : %d\r\n"), pstVolDumpHdr->nNumOfDevs));
    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   pstDieDumpHdr->nDataAreaOffset       : %d\r\n"), pstDieDumpHdr->nDataAreaOffset));
    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   pstDieDumpHdr->nDataAreaSize         : %d\r\n"), pstDieDumpHdr->nDataAreaSize));
    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   pstDieDumpHdr->nDieOffset            : %d\r\n"), pstDieDumpHdr->nDieOffset));
    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   pstDieDumpHdr->nFlagCellOffset       : %d\r\n"), pstDieDumpHdr->nFlagCellOffset));
    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   pstDieDumpHdr->nFlagCellSize         : %d\r\n"), pstDieDumpHdr->nFlagCellSize));
    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   pstDieDumpHdr->nNumOfPEntry          : %d\r\n"), pstDieDumpHdr->nNumOfPEntry));
    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   pstDieDumpHdr->nOTPBlkOffset         : %d\r\n"), pstDieDumpHdr->nOTPBlkOffset));
    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   pstDieDumpHdr->nOTPBlkSize           : %d\r\n"), pstDieDumpHdr->nOTPBlkSize));
    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   pstDieDumpHdr->nPIBlkOffset          : %d\r\n"), pstDieDumpHdr->nPIBlkOffset));
    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   pstDieDumpHdr->nPIBlkSize            : %d\r\n"), pstDieDumpHdr->nPIBlkSize));
    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   pstDieDumpHdr->stPEntry[%2d].nBlkType : %s\r\n"), 
        pstDumpLog->nPEntryNum, 
        gpFSRDumpDataType[pstDieDumpHdr->stPEntry[pstDumpLog->nPEntryNum].nBlkType]));
}

/**
 *  @brief      This function gets pointer to FSRVolDumpHdr data structure.
 *
 *  @param [in]  nVol           : Volume number
 *
 *  @return     pointer to FSRVolDumpHdr data structure
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PRIVATE FSRVolDumpHdr*
_GetVolDumpHdr(UINT32     nVol)
{
    return gpstVolDumpHdr[nVol];
}
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function sets die dump header
 *
 *  @param [in]  nVol           : Volume number
 *  @param [in]  nDumpType      : FSR_DUMP_VOLUME
 *  @n                            FSR_DUMP_PARTITION
 *  @n                            FSR_DUMP_RESERVOIR_ONLY
 *  @param [in]  pstPEList      : Pointer to FSRPEList structure
 *  @param [out] pstDieDumpHdr  : Pointer to FSRDieDumpHdr structure
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     some LLD error returns
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PRIVATE INT32
_SetDieDumpHdr(UINT32         nVol,
               UINT32         nDumpType,
               FSRDumpPEList *pstPEList,
               FSRDieDumpHdr *pstDieDumpHdr)
{
    UINT32          nPDev       = 0;        /* Physical device number       */
    UINT32          nDieIdx     = 0;        /* Die Index                    */
    UINT32          nIdx        = 0;        /* Temporary Index              */
    UINT32          nByteRet    = 0;        /* # of bytes returned          */
    UINT32          nBlkType    = 0;        /* Block type (SLC/MLC)         */
    UINT32          nNumOfBlksInRsvr = 0;   /* # of reservoir blocks in die */
    UINT32          nNumOfIntvBlks   = 0;   /* # of interval blocks
                                                     to support hybrid Rsv. */
    UINT32          nLastPENum  = 0;        /* Last PEntry number           */
    UINT32          n1stSbn     = 0;        /* 1st Sbn in SLC/MLC area      */
    UINT32          nLastSbn    = 0;        /* Last Sbn in SLC/MLC area     */
    UINT32          nBMFIdx     = 0;        /* # of RCBs                    */
    INT32           nLLDRe  = FSR_LLD_SUCCESS;
    INT32           nRe     = FSR_BML_SUCCESS;
    BmlVolCxt      *pstVol;
    BmlDevCxt      *pstDev;
    BmlDieCxt      *pstDie;
    BmlBMI         *pstBMI;
    BmlBMF          stBMF[FSR_MAX_BAD_BLKS/2];
    LLDPIArg        stLLDPIArg;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, nDumpType: 0x%x)\r\n"),
                                    __FSR_FUNC__, nVol, nDumpType));

#if defined(BML_CHK_PARAMETER_VALIDATION)
    /* check whether pstDieDumpHdr is NULL or not */
    if (pstDieDumpHdr == NULL)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, pstDieDumpHdr:0x%x, nRe:FSR_BML_INVALID_PARAM)\r\n"),
                                        __FSR_FUNC__, nVol, pstDieDumpHdr));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, FSR_BML_INVALID_PARAM));
        return FSR_BML_INVALID_PARAM;
    }
#endif /* BML_CHK_PARAMETER_VALIDATION */

    /* Get pointer to BmlVolCxt structure */
    pstVol  = _GetVolCxt(nVol);

    /* Get nPDev, nDieIdx */
    nPDev   = pstVol->pstDumpLog->nPDev;
    nDieIdx = pstVol->pstDumpLog->nDieIdx;

    /* Initialize nPEntryNum */
    pstVol->pstDumpLog->nPEntryNum  = 0;

    /* Get pstDev, pstDie, n1stPbnOfRsvr */
    pstDev          = _GetDevCxt(nPDev);
    pstDie          = pstDev->pstDie[nDieIdx];

    /* Initialize LLDPIArg */
    FSR_OAM_MEMSET(&stLLDPIArg, 0x00, sizeof(LLDPIArg));

    /* Initialize BmlBMF */
    FSR_OAM_MEMSET(&stBMF, 0x00, sizeof(BmlBMF) * FSR_MAX_BAD_BLKS / 2);

    switch(nDumpType)
    {
    case FSR_DUMP_VOLUME:
        {
            /* Initialize nNumOfPEntry */
            pstDieDumpHdr->nNumOfPEntry = 1;

            if (pstVol->nNANDType == FSR_LLD_SLC_ONENAND)
            {
                pstDieDumpHdr->stPEntry[0].nBlkType = FSR_DUMP_SLC_BLOCK;
                pstDieDumpHdr->stPEntry[0].n1stPbn  = nDieIdx * pstVol->nNumOfBlksInDie;
                pstDieDumpHdr->stPEntry[0].nNumOfPbn= pstVol->nNumOfBlksInDie;
            }
            else /* Flex-OneNAND */
            {
                /* Read PI */
                nLLDRe = pstVol->LLD_IOCtl(nPDev,
                                           FSR_LLD_IOCTL_PI_READ,
                                           (UINT8 *)&nDieIdx,
                                           sizeof(nDieIdx),
                                           (UINT8 *)&(stLLDPIArg),
                                           sizeof(stLLDPIArg),
                                           &nByteRet);
                if (nLLDRe != FSR_LLD_SUCCESS)
                {
                    nRe = nLLDRe;
                    /* message out */
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nDumpType:0x%x)\r\n"),
                                                    __FSR_FUNC__, nVol, nDumpType));
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_IOCtl(nPDev:%d, nCode:FSR_LLD_IOCTL_PI_READ, nDieIdx:%d, nRe:0x%x)\r\n"),
                                                    nPDev, nDieIdx, nLLDRe));
                    break;
                }

                /* Set stPEntry of SLC Block */
                pstDieDumpHdr->stPEntry[0].nBlkType     = FSR_DUMP_SLC_BLOCK;
                pstDieDumpHdr->stPEntry[0].n1stPbn      = nDieIdx * pstVol->nNumOfBlksInDie;
                pstDieDumpHdr->stPEntry[0].nNumOfPbn    = (stLLDPIArg.nEndOfSLC + 1) -
                                                          (nDieIdx * pstVol->nNumOfBlksInDie);

                /* MLC Block */
                if (pstVol->nNumOfBlksInDie != pstDieDumpHdr->stPEntry[0].nNumOfPbn)
                {
                    /* Increase # of partition entries */
                    pstDieDumpHdr->nNumOfPEntry             ++;

                    /* Set stPEntry of MLC Block */
                    pstDieDumpHdr->stPEntry[1].nBlkType     = FSR_DUMP_MLC_BLOCK;
                    pstDieDumpHdr->stPEntry[1].n1stPbn      = stLLDPIArg.nEndOfSLC + 1;
                    pstDieDumpHdr->stPEntry[1].nNumOfPbn    = pstVol->nNumOfBlksInDie - pstDieDumpHdr->stPEntry[0].nNumOfPbn;
                }
            }
        }
        break;

    case FSR_DUMP_PARTITION:
        {
            pstDieDumpHdr->nNumOfPEntry = pstPEList->nNumOfPEntry;

            /* Set # of blocks in reservoir */
            nNumOfBlksInRsvr            = pstDie->pstRsv->nNumOfRsvrInSLC +
                                          pstDie->pstRsv->nNumOfRsvrInMLC;

            /* User data area */
            for (nIdx = 0; nIdx < pstPEList->nNumOfPEntry; nIdx++)
            {
                nNumOfIntvBlks = 0;
                if (pstPEList->stPEntry[nIdx].nAttr & FSR_BML_PI_ATTR_SLC)
                {
                    nBlkType = FSR_DUMP_SLC_BLOCK;
                }
                else
                {
                    nBlkType = FSR_DUMP_MLC_BLOCK;
                    /* Calculate # of reserved blocks by reservoir type */
                    if (pstDie->pstRsv->nRsvrType == BML_HYBRID_RESERVOIR)
                    {
                        nNumOfIntvBlks = pstDie->pstRsv->nLastSbnOfRsvr - pstDie->pstRsv->n1stSbnOfRsvr + 1;
                    }
                }

                pstDieDumpHdr->stPEntry[nIdx].nBlkType  = nBlkType;
                pstDieDumpHdr->stPEntry[nIdx].n1stPbn   = pstPEList->stPEntry[nIdx].n1stVun +
                                                          (nDieIdx * pstVol->nNumOfBlksInDie)+
                                                          nNumOfIntvBlks;
                pstDieDumpHdr->stPEntry[nIdx].nNumOfPbn = pstPEList->stPEntry[nIdx].nNumOfUnits *
                                                          pstVol->nNumOfPlane;

                /* Get pointer to BmlBMI structure */
                pstBMI = pstDie->pstRsv->pstBMI;

                if ((pstVol->nNumOfPlane != 1) &&
                    (pstBMI->nNumOfBMFs > nNumOfBlksInRsvr))
                {
                    FSR_OAM_MEMCPY(&stBMF, pstDie->pstRsv->pstBMF, sizeof(BmlBMF) * pstBMI->nNumOfBMFs);

                    /* Check reservoir candidate block(RCB) in die */
                    n1stSbn     = pstDieDumpHdr->stPEntry[nIdx].n1stPbn;
                    nLastSbn    = n1stSbn + pstDieDumpHdr->stPEntry[nIdx].nNumOfPbn;

                    for (nBMFIdx = nNumOfBlksInRsvr; nBMFIdx < pstBMI->nNumOfBMFs; nBMFIdx++)
                    {
                        if ((stBMF[nBMFIdx].nRbn >= n1stSbn)    &&
                            (stBMF[nBMFIdx].nRbn <= nLastSbn))
                        {
                            /* Set new FSRDumpPEntry for RCB */
                            pstDieDumpHdr->nNumOfPEntry++;
                            pstDieDumpHdr->stPEntry[pstDieDumpHdr->nNumOfPEntry-1].nBlkType = nBlkType;
                            pstDieDumpHdr->stPEntry[pstDieDumpHdr->nNumOfPEntry-1].n1stPbn  = stBMF[nBMFIdx].nRbn;
                            pstDieDumpHdr->stPEntry[pstDieDumpHdr->nNumOfPEntry-1].nNumOfPbn= 1;
                        }
                    }
                }/* End of "if (pstVol->nNumOfPlane != 1)" */

            } /* End of "for (nIdx = 0; nIdx < pstPEList->nNumOfPEntry; nIdx++)" */

            pstDieDumpHdr->nNumOfPEntry ++;
            nLastPENum                  = pstDieDumpHdr->nNumOfPEntry - 1;

            /* Set # of blocks in reservoir */
            nNumOfBlksInRsvr    = pstDie->pstRsv->nNumOfRsvrInSLC +
                                  pstDie->pstRsv->nNumOfRsvrInMLC;

            /* Set block type */
            if (pstDie->pstRsv->nRsvrType == BML_MLC_RESERVOIR)
            {
                nBlkType = FSR_DUMP_MLC_BLOCK;
            }
            else /* BML_MLC_RESERVOIR or BML_HYBRID_RESERVOIR */
            {
                nBlkType = FSR_DUMP_SLC_BLOCK;
            }

            /* Reservoir area */
            pstDieDumpHdr->stPEntry[nLastPENum].nBlkType     = nBlkType;
            pstDieDumpHdr->stPEntry[nLastPENum].n1stPbn      = pstDie->pstRsv->n1stSbnOfRsvr;
            pstDieDumpHdr->stPEntry[nLastPENum].nNumOfPbn    = nNumOfBlksInRsvr + BML_RSV_META_BLKS;
            if (pstDie->pstRsv->nRsvrType == BML_HYBRID_RESERVOIR)
            {
                pstDieDumpHdr->nNumOfPEntry ++;
                pstDieDumpHdr->stPEntry[nLastPENum].nNumOfPbn   -= (pstDie->pstRsv->nNumOfRsvrInMLC +
                                                                    BML_RSV_REF_BLKS);

                pstDieDumpHdr->stPEntry[nLastPENum+1].nBlkType     = FSR_DUMP_MLC_BLOCK;
                pstDieDumpHdr->stPEntry[nLastPENum+1].n1stPbn      = pstDie->pstRsv->n1stSbnOfRsvr +
                                                                     pstDie->pstRsv->nNumOfRsvrInSLC + 
                                                                     BML_RSV_PCB_BLKS;
                pstDieDumpHdr->stPEntry[nLastPENum+1].nNumOfPbn    = pstDie->pstRsv->nNumOfRsvrInMLC +
                                                                    BML_RSV_REF_BLKS;
            }
        }
        break;

    case FSR_DUMP_RESERVOIR_ONLY:
        {
            /* Set # of blocks in reservoir */
            nNumOfBlksInRsvr    = pstDie->pstRsv->nNumOfRsvrInSLC +
                                  pstDie->pstRsv->nNumOfRsvrInMLC;

            /* Initialize nNumOfPEntry */
            pstDieDumpHdr->nNumOfPEntry             = 1;

            /* Set block type */
            if (pstDie->pstRsv->nRsvrType == BML_MLC_RESERVOIR)
            {
                nBlkType = FSR_DUMP_MLC_BLOCK;
            }
            else /* BML_MLC_RESERVOIR or BML_HYBRID_RESERVOIR */
            {
                nBlkType = FSR_DUMP_SLC_BLOCK;
            }

            /* Reservoir area */
            pstDieDumpHdr->stPEntry[nLastPENum].nBlkType     = nBlkType;
            pstDieDumpHdr->stPEntry[nLastPENum].n1stPbn      = pstDie->pstRsv->n1stSbnOfRsvr;
            pstDieDumpHdr->stPEntry[nLastPENum].nNumOfPbn    = nNumOfBlksInRsvr + BML_RSV_META_BLKS;
            if (pstDie->pstRsv->nRsvrType == BML_HYBRID_RESERVOIR)
            {
                pstDieDumpHdr->nNumOfPEntry ++;
                pstDieDumpHdr->stPEntry[nLastPENum].nNumOfPbn   -= (pstDie->pstRsv->nNumOfRsvrInMLC +
                                                                    BML_RSV_REF_BLKS);

                pstDieDumpHdr->stPEntry[nLastPENum+1].nBlkType     = FSR_DUMP_MLC_BLOCK;
                pstDieDumpHdr->stPEntry[nLastPENum+1].n1stPbn      = pstDie->pstRsv->n1stSbnOfRsvr +
                                                                     pstDie->pstRsv->nNumOfRsvrInSLC + 
                                                                     BML_RSV_PCB_BLKS;
                pstDieDumpHdr->stPEntry[nLastPENum+1].nNumOfPbn    = pstDie->pstRsv->nNumOfRsvrInMLC +
                                                                    BML_RSV_REF_BLKS;
            }

            /* Get pointer to BmlBMI structure */
            pstBMI = pstDie->pstRsv->pstBMI;

            if ((pstVol->nNumOfPlane != 1) &&
                (pstBMI->nNumOfBMFs > nNumOfBlksInRsvr))
            {
                FSR_OAM_MEMCPY(&stBMF, pstDie->pstRsv->pstBMF, sizeof(BmlBMF) * pstBMI->nNumOfBMFs);

                /* Check reservoir candidate block(RCB) of current partition */
                if (pstDie->pstRsv->nRsvrType == BML_HYBRID_RESERVOIR)
                {
                    /* SLC RCB */
                    n1stSbn     = nDieIdx * pstVol->nNumOfBlksInDie;
                    nLastSbn    = n1stSbn + pstVol->nNumOfSLCUnit * pstVol->nNumOfPlane;

                    for (nBMFIdx = nNumOfBlksInRsvr; nBMFIdx < pstBMI->nNumOfBMFs; nBMFIdx++)
                    {
                        if ((stBMF[nBMFIdx].nRbn >= n1stSbn)    &&
                            (stBMF[nBMFIdx].nRbn <= nLastSbn))
                        {
                            /* Set new FSRDumpPEntry for RCB */
                            pstDieDumpHdr->nNumOfPEntry++;
                            pstDieDumpHdr->stPEntry[pstDieDumpHdr->nNumOfPEntry-1].nBlkType = FSR_DUMP_SLC_BLOCK;
                            pstDieDumpHdr->stPEntry[pstDieDumpHdr->nNumOfPEntry-1].n1stPbn  = stBMF[nBMFIdx].nRbn;
                            pstDieDumpHdr->stPEntry[pstDieDumpHdr->nNumOfPEntry-1].nNumOfPbn= 1;
                        }
                    }

                    /* MLC RCB */
                    n1stSbn     = pstDie->pstRsv->nLastSbnOfRsvr + 1;
                    nLastSbn    = n1stSbn +
                                 (pstVol->nLastUnit - pstVol->nNumOfSLCUnit + 1) * pstVol->nNumOfPlane;

                    for (nBMFIdx = nNumOfBlksInRsvr; nBMFIdx < pstBMI->nNumOfBMFs; nBMFIdx++)
                    {
                        if ((stBMF[nBMFIdx].nRbn >= n1stSbn)    &&
                            (stBMF[nBMFIdx].nRbn <= nLastSbn))
                        {
                            /* Set new FSRDumpPEntry for RCB */
                            pstDieDumpHdr->nNumOfPEntry++;
                            pstDieDumpHdr->stPEntry[pstDieDumpHdr->nNumOfPEntry-1].nBlkType = FSR_DUMP_MLC_BLOCK;
                            pstDieDumpHdr->stPEntry[pstDieDumpHdr->nNumOfPEntry-1].n1stPbn  = stBMF[nBMFIdx].nRbn;
                            pstDieDumpHdr->stPEntry[pstDieDumpHdr->nNumOfPEntry-1].nNumOfPbn= 1;
                        }
                    }
                }
                else /* BML_SLC_RESERVOIR or BML_MLC_RESERVOIR */
                {
                    if (pstDie->pstRsv->nRsvrType == BML_SLC_RESERVOIR)
                    {
                        nBlkType = FSR_DUMP_SLC_BLOCK;
                    }
                    else /* BML_MLC_RESERVOIR */
                    {
                        nBlkType = FSR_DUMP_MLC_BLOCK;
                    }

                    n1stSbn     = nDieIdx * pstVol->nNumOfBlksInDie;
                    nLastSbn    = n1stSbn +
                                  pstVol->nNumOfBlksInDie -
                                  (pstDie->pstRsv->nNumOfRsvrInMLC + pstDie->pstRsv->nNumOfRsvrInSLC);;

                    for (nBMFIdx = nNumOfBlksInRsvr; nBMFIdx < pstBMI->nNumOfBMFs; nBMFIdx++)
                    {
                        if ((stBMF[nBMFIdx].nRbn >= n1stSbn)    &&
                            (stBMF[nBMFIdx].nRbn <= nLastSbn))
                        {
                            /* Set new FSRDumpPEntry for RCB */
                            pstDieDumpHdr->nNumOfPEntry++;
                            pstDieDumpHdr->stPEntry[pstDieDumpHdr->nNumOfPEntry-1].nBlkType = nBlkType;
                            pstDieDumpHdr->stPEntry[pstDieDumpHdr->nNumOfPEntry-1].n1stPbn  = stBMF[nBMFIdx].nRbn;
                            pstDieDumpHdr->stPEntry[pstDieDumpHdr->nNumOfPEntry-1].nNumOfPbn= 1;
                        }
                    }
                } /* End of "if (pstDie->pstRsv->nRsvrType == BML_HYBRID_RESERVOIR)" */
            }/* End of "if (pstVol->nNumOfPlane != 1)" */
        }
        break;

    default:
        /* if code reaches this line, abnormal case.. */
        nRe = FSR_BML_CRITICAL_ERROR;
        break;
    }

    /* Set nFlagCellOffset, nFlagCellSize */
    if (nRe == FSR_BML_SUCCESS)
    {
        pstDieDumpHdr->nFlagCellOffset  = pstDieDumpHdr->nDieOffset;
        pstDieDumpHdr->nFlagCellSize    = 0;
        pstDieDumpHdr->nOTPBlkOffset    = pstDieDumpHdr->nDieOffset;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, FSR_BML_SUCCESS));

    return nRe;
}
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function sets volume dump header
 *
 *  @param [in]  nVol           : Volume number
 *  @param [in]  nDumpType      : FSR_DUMP_VOLUME
 *  @n                            FSR_DUMP_PARTITION
 *  @n                            FSR_DUMP_RESERVOIR_ONLY
 *  @param [in]  pstPEList      : Pointer to FSRPEList structure
 *  @param [out] pstVolDumpHdr  : Pointer to FSRVolDumpHdr structure
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     some LLD error returns
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PRIVATE INT32
_SetVolDumpHdr(UINT32         nVol,
               UINT32         nDumpType,
               FSRDumpPEList *pstPEList,
               FSRVolDumpHdr *pstVolDumpHdr)
{
    UINT32          nPDev   = 0;    /* Physical device number   */
    UINT32          nVersion;       /* Version name             */
    UINT32          nDevIdx = 0;    /* Device Index             */
    INT32           nLLDRe  = FSR_LLD_SUCCESS;
    INT32           nPAMRe  = FSR_PAM_SUCCESS;
    INT32           nRe     = FSR_BML_SUCCESS;
    FsrVolParm      stFsrVolParm[FSR_MAX_VOLS];
    BmlVolCxt      *pstVol;
    FSRDevSpec      stLLDSpec;
    FSRDumpDevCxt   stDevCxt;
    FSRDieDumpHdr  *pstDieDumpHdr;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, nDumpType: 0x%x)\r\n"),
                                    __FSR_FUNC__, nVol, nDumpType));

    /* Get pointer to BmlVolCxt structure */
    pstVol = _GetVolCxt(nVol);

    /* Initialize stLLDSpec */
    FSR_OAM_MEMSET(&stLLDSpec, 0x00, sizeof(FSRDevSpec));

    do
    {
        nPAMRe = FSR_PAM_GetPAParm(stFsrVolParm);
        if (nPAMRe != FSR_PAM_SUCCESS)
        {
            nRe = nPAMRe;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                (TEXT("[BIF:ERR]   FSR_PAM_GetPAParm() is failed / %s / line %d\r\n"), __FUNCTION__, __LINE__));
            break;
        }

        for (nDevIdx = 0; nDevIdx < pstVol->nNumOfDev; nDevIdx++)
        {
            /* Set all nPDev in volume */
            nPDev = (nVol * DEVS_PER_VOL) + nDevIdx;

            /* Get NAND device specification using LLD_GetDevSpec() */
            nLLDRe = pstVol->LLD_GetDevSpec(nPDev,
                                            &stLLDSpec,
                                            FSR_LLD_FLAG_NONE);
            if (nLLDRe != FSR_LLD_SUCCESS)
            {
                nRe = nLLDRe;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nDumpType: 0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nDumpType));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_GetDevSpec(nPDev:%d,nRe: 0x%x)\r\n"),
                                                nPDev, nLLDRe));
                break;
            }

            /* Set nBlksForSLCArea for each device */
            pstVolDumpHdr->nBlksForSLCArea[nDevIdx][0]  = stLLDSpec.nBlksForSLCArea[0];
            pstVolDumpHdr->nBlksForSLCArea[nDevIdx][1]  = stLLDSpec.nBlksForSLCArea[1];

            /* Set nBaseAddr for each device */
            pstVolDumpHdr->stVolParm.nBaseAddr[nDevIdx] = stFsrVolParm[nVol].nBaseAddr[nDevIdx];
        }

        /* Initialize stDevCxt */
        FSR_OAM_MEMSET(&stDevCxt, 0x00, sizeof(FSRDumpDevCxt));

        /* Set pstDevCxt*/
        stDevCxt.nNumOfBlks         = stLLDSpec.nNumOfBlks;
        stDevCxt.nNumOfPlanes       = stLLDSpec.nNumOfPlanes;
        stDevCxt.nSparePerSct       = stLLDSpec.nSparePerSct;
        stDevCxt.nSctsPerPG         = stLLDSpec.nSctsPerPG;
        stDevCxt.nPgsPerBlkForSLC   = stLLDSpec.nPgsPerBlkForSLC;
        stDevCxt.nPgsPerBlkForMLC   = stLLDSpec.nPgsPerBlkForMLC;
        stDevCxt.nNumOfDies         = stLLDSpec.nNumOfDies;
        stDevCxt.nNANDType          = stLLDSpec.nNANDType;
        stDevCxt.nDID               = stLLDSpec.nDID;

        /* Set the FSRVolDumpHdr */
        FSR_OAM_MEMCPY(pstVolDumpHdr->nHeaderSig, FSR_DUMP_HDR_SIG, FSR_DUMP_HDR_SIG_SIZE);
        pstVolDumpHdr->nHeaderVerCode               = FSR_DUMP_HDR_VER;
        pstVolDumpHdr->nFSRVerCode                  = (UINT32) FSR_Version(&nVersion);
        pstVolDumpHdr->nNumOfDevs                   = pstVol->nNumOfDev;
        pstVolDumpHdr->nDiesPerDev                  = pstVol->nNumOfDieInDev;

        /* Set FSRDumpVolParm structure in FSRVolDumpHdr */
        pstVolDumpHdr->stVolParm.nEndianType        = 0x00000000; /* Initialize endian type */
        pstVolDumpHdr->stVolParm.nDevsInVol         = stFsrVolParm[nVol].nDevsInVol;
        pstVolDumpHdr->stVolParm.pExInfo            = stFsrVolParm[nVol].pExInfo;

        FSR_OAM_MEMCPY(&pstVolDumpHdr->stDevCxt, &stDevCxt, sizeof(FSRDumpDevCxt));
        pstVolDumpHdr->nDevCxtSize      = sizeof(FSRDumpDevCxt);

        /* Set nDieOffset of FSRDieDumpHdr */        
        pstDieDumpHdr = &(pstVolDumpHdr->stDieDumpHdr[0][0]);

        /* Initialize pstDieDumpHdr */
        FSR_OAM_MEMSET(pstDieDumpHdr, 0x00, sizeof(FSRDieDumpHdr));
        pstDieDumpHdr->nDieOffset       = sizeof(FSRVolDumpHdr);

        /* Set Die Dump header */
        nRe = _SetDieDumpHdr(nVol,
                             nDumpType,
                             pstPEList,
                             pstDieDumpHdr);
        if (nRe != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nDumpType: 0x%x)\r\n"),
                                            __FSR_FUNC__, nVol, nDumpType));
            break;
        }
    } while(0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, nRe));

    return nRe;
}
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function starts the dump sequence.
 *
 *  @param [in]  nVol           : Volume number
 *  @param [in]  nDumpType      : FSR_DUMP_VOLUME
 *  @n                            FSR_DUMP_PARTITION
 *  @n                            FSR_DUMP_RESERVOIR_ONLY
 *  @param [in]  pstPEList      : Pointer to FSRPEList structure
 *  @param [out] pBuf           : Pointer to page buffer for dump
 *  @param [out] pBytesReturned : Number of bytes returned in pBuf
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     some error returns
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PRIVATE INT32
_StartDump(UINT32         nVol,
           UINT32         nDumpType,
           FSRDumpPEList *pstPEList,
           UINT8         *pBuf,
           UINT32        *pBytesReturned)
{
    INT32           nRe     = FSR_BML_SUCCESS;
    BmlVolCxt      *pstVol;
    FSRVolDumpHdr  *pstVolDumpHdr;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, nDumpType: 0x%x)\r\n"),
                                    __FSR_FUNC__, nVol, nDumpType));

    /* Check the volume range */
    CHK_VOL_RANGE(nVol);

    /* Get the pointer of volume context */
    pstVol = _GetVolCxt(nVol);

    /* 
     * << First procedure for volume dump >>
     * STEP1. Open the volume if FSR_BML_Open is not called
     * STEP2. Initialize dump log of BmlVolCxt
     * STEP3. Set BML Volume Dump header
     */

    do
    {
        /* STEP1. Open the volume if FSR_BML_Open is not called */
        if (pstVol->bVolOpen != TRUE32)
        {
            /* Open a volume */
            nRe = _Open(nVol);
            if (nRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nDumpType: 0x%x, nRe:0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nDumpType, nRe));
                break;
            }
        }

        /* STEP2. Initialize dump log of BmlVolCxt */
        pstVol->pstDumpLog->nPDev       = (UINT16)nVol * DEVS_PER_VOL;
        pstVol->pstDumpLog->nDieIdx     = 0;
        pstVol->pstDumpLog->bDataBlk    = TRUE32;
        pstVol->pstDumpLog->nPbn        = 0;
        pstVol->pstDumpLog->nSeqOrder   = 0;

        pstVol->pstDumpLog->bOTPBlk     = FALSE32;
        pstVol->pstDumpLog->bPIBlk      = FALSE32;
        if ((pstVol->nNANDType == FSR_LLD_FLEX_ONENAND) ||
            (pstVol->nNANDType == FSR_LLD_SLC_ONENAND))
        {
            pstVol->pstDumpLog->bOTPBlk     = TRUE32;
            if (pstVol->nNANDType == FSR_LLD_FLEX_ONENAND)
            {
                pstVol->pstDumpLog->bPIBlk  = TRUE32;
            }
        }

        /* Create Volume Header Context */
        gpstVolDumpHdr[nVol] = (FSRVolDumpHdr *) FSR_OAM_Malloc(sizeof(FSRVolDumpHdr));
        if (gpstVolDumpHdr[nVol] == NULL)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nDumpType: 0x%x, nRe:FSR_BML_OAM_ACCESS_ERROR)\r\n"),
                                            __FSR_FUNC__, nVol, nDumpType));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]  FSR_OAM_Malloc Error: gpstVolDumpHdr[%d]\r\n"), nVol));
            nRe = FSR_BML_OAM_ACCESS_ERROR;
            break;
        }

        /* Check mis-aligned pointer */
        if (((UINT32)(gpstVolDumpHdr[nVol]) & 0x3) != 0)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nDumpType: 0x%x, nRe:FSR_OAM_NOT_ALIGNED_MEMPTR)\r\n"),
                                            __FSR_FUNC__, nVol, nDumpType));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]  FSR_OAM_Malloc Error: gpstDumpLsbPg\r\n")));
            nRe = FSR_OAM_NOT_ALIGNED_MEMPTR;
            break;
        }

        /* Initialize gpstVolDumpHdr[nVol] */
        FSR_OAM_MEMSET(gpstVolDumpHdr[nVol], 0x00, sizeof(FSRVolDumpHdr));

        /* Get pointer to FSRVolDumpHdr structure */
        pstVolDumpHdr = gpstVolDumpHdr[nVol];

        /* STEP3. Set BML Volume Dump header */
        nRe = _SetVolDumpHdr(nVol,
                             nDumpType,
                             pstPEList,
                             pstVolDumpHdr);
        if (nRe != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nDumpType: 0x%x, nRe:0x%x)\r\n"),
                                            __FSR_FUNC__, nVol, nDumpType, nRe));
            break;
        }

        /* Set pBuf, pBytesReturned */
        FSR_OAM_MEMCPY(pBuf, pstVolDumpHdr, sizeof(FSRVolDumpHdr));
        *pBytesReturned = sizeof(FSRVolDumpHdr);

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, nRe));

    return nRe;
}
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function dumps data from the given Pbn.
 *
 *  @param [in]  nVol           : Volume number
 *  @param [in]  nPDev          : Physical device number
 *  @param [in]  nPbn           : Physical block number
 *  @param [in]  nNumOfPgs      : The number of pages to read
 *  @param [out] *pBuf          : Buffer pointer for dump image
 *  @param [out] *pBytesReturned: Size of dump image
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     some error returns
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PRIVATE INT32
_ReadBlk(UINT32     nVol,
         UINT32     nPDev,
         UINT32     nPbn,
         UINT32     nNumOfPgs,
         UINT8     *pBuf,
         UINT32    *pBytesReturned)
{
    UINT32          nPgIdx  = 0;    /* Page index           */
    INT32           nLLDRe  = FSR_LLD_SUCCESS;
    INT32           nRe     = FSR_BML_SUCCESS;
    UINT8          *pMBuf;
    UINT8          *pSBuf;
    BmlVolCxt      *pstVol;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, nPDev: %d, nPbn: %d, nNumOfPgs: %d)\r\n"),
                                    __FSR_FUNC__, nVol, nPDev, nPbn, nNumOfPgs));

    /* Get pointer to BmlVolCxt structure */
    pstVol = _GetVolCxt(nVol);

    /* Initialize pMBuf, pSBuf */
    pMBuf   = pBuf;
    pSBuf   = pBuf + pstVol->nSizeOfPage;

    /* <Block Dump>
     * This API does not check ECC of a page.
     * So, uncorrectable read error or read disturbance error do not occur.
     */
    for (nPgIdx = 0; nPgIdx < nNumOfPgs; nPgIdx++)
    {
        /* Call LLD_Read() for "load" operation */
        nLLDRe = pstVol->LLD_Read(nPDev,
                                  nPbn,
                                  nPgIdx,
                                  pMBuf,
                                  (FSRSpareBuf *) pSBuf,
                                  FSR_LLD_FLAG_DUMP_ON      |
                                  FSR_LLD_FLAG_USE_SPAREBUF |
                                  FSR_LLD_FLAG_ECC_OFF      |
                                  FSR_LLD_FLAG_1X_LOAD);

        /* Call LLD_Read() for "transfer" operation */
        nLLDRe = pstVol->LLD_Read(nPDev,
                                  nPbn,
                                  nPgIdx,
                                  pMBuf,
                                  (FSRSpareBuf *) pSBuf,
                                  FSR_LLD_FLAG_DUMP_ON      |
                                  FSR_LLD_FLAG_USE_SPAREBUF |
                                  FSR_LLD_FLAG_ECC_OFF      |
                                  FSR_LLD_FLAG_TRANSFER);
        if (nLLDRe == FSR_LLD_INVALID_PARAM)
        {
            nRe = nLLDRe;
            /* message out */
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_Read(nPDev:%d, nPbn:%d, nPgIdx:%d, nRe:0x%x)\r\n"),
                                            nPDev, nPbn, nPgIdx, nLLDRe));
            break;
        }

        /* Calculate pBytesReturned */
        *pBytesReturned += pstVol->nSctsPerPg * (pstVol->nSparePerSct + FSR_SECTOR_SIZE);

        /* Increase pMBuf, pSBuf*/
        pMBuf   = pSBuf + pstVol->nSparePerSct * pstVol->nSctsPerPg;
        pSBuf   = pMBuf + pstVol->nSizeOfPage;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, nRe));

    return nRe;
}
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function dumps data from OTP block.
 *
 *  @param [in]   nVol          : Volume number
 *  @param [out] *pBuf          : Buffer pointer for dump image
 *  @param [out] *pBytesReturned: Size of dump image
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_PAM_ACCESS_ERROR
 *  @return     some error returns
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PRIVATE INT32
_ReadOTPBlk(UINT32      nVol,
            UINT8      *pBuf,
            UINT32     *pBytesReturned)
{
    UINT32          nPDev   = 0;    /* Physical device number   */
    UINT32          nDevIdx = 0;    /* Device Index             */
    UINT32          nDieIdx = 0;    /* Die Index                */
    UINT32          nByteRet= 0;    /* # of bytes of return     */
    INT32           nRe     = FSR_BML_SUCCESS;
    INT32           nLLDRe  = FSR_LLD_SUCCESS;
    BmlVolCxt      *pstVol;
    BmlDumpLog     *pstDumpLog;
    FSRVolDumpHdr  *pstVolDumpHdr;
    FSRDieDumpHdr  *pstDieDumpHdr;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));

    /* Get pointer to VolCxt structure */
    pstVol = _GetVolCxt(nVol);

    /* Get pointer to BmlDumpLog structure */
    pstDumpLog  = pstVol->pstDumpLog;

    /* Set nPDev, nDieIdx by pstDumpLog */
    nPDev       = pstVol->pstDumpLog->nPDev;
    nDieIdx     = pstVol->pstDumpLog->nDieIdx;
    nDevIdx     = nPDev - (nVol * DEVS_PER_VOL);

    /* Get pointer to FSRVolDumpHdr, FSRDieDumpHdr structure */
    pstVolDumpHdr = _GetVolDumpHdr(nVol);
    pstDieDumpHdr = &(pstVolDumpHdr->stDieDumpHdr[nDevIdx][nDieIdx]);

    _PrintDumpStat( nDevIdx,
                    pstDumpLog,
                    pstVolDumpHdr,
                    pstDieDumpHdr);

    /* NO OTP Block */
    if (pstVol->pstDumpLog->bOTPBlk == FALSE32)
    {
        /* Set pBytesReturned, nOTPBlkSize, nPIBlkOffset of FSRDieDumpHdr */
        *pBytesReturned             = 0;
        pstDieDumpHdr->nOTPBlkSize  = 0;
        pstDieDumpHdr->nPIBlkOffset = pstDieDumpHdr->nOTPBlkOffset;

        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));
        return nRe;
    }

    /************************************************************/
    /* <Procedure of _ReadOTPBlk()                              */
    /* STEP1. Set OTP Mode                                      */
    /* STEP2. Dump data form OTP block                          */
    /* STEP3. Set NAND Core reset mode                          */
    /************************************************************/
    do
    {
        /************************************************/
        /* STEP1. Set OTP Mode by FSR_LLD_IOCTL()       */
        /************************************************/
        nLLDRe = pstVol->LLD_IOCtl(nPDev,
                                   FSR_LLD_IOCTL_OTP_ACCESS,
                                   (UINT8 *) &nDieIdx,
                                   sizeof(nDieIdx),
                                   NULL,
                                   0,
                                   &nByteRet);
        if (nLLDRe != FSR_LLD_SUCCESS)
        {
            nRe = nLLDRe;
            /* message out */
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_IOCtl(nPDev:%d, nCode: FSR_LLD_IOCTL_OTP_ACCESS, nDieIdx:%d, nRe:0x%x)\r\n"),
                                            nPDev, nDieIdx, nLLDRe));
            break;
        }

        /********************************************/
        /* STEP2. Dump data form OTP block          */
        /********************************************/
        nRe = _ReadBlk(nVol,
                       nPDev,
                       nDieIdx * pstVol->nNumOfBlksInDie,
                       pstVol->nNumOfPgsInSLCBlk,
                       pBuf,
                       pBytesReturned);
        if (nRe != FSR_BML_SUCCESS)
        {
            /* Message out */
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:0x%x)\r\n"), __FSR_FUNC__, nVol, nRe));
            break;
        }

        /* Set nOTPBlkSize, nPIBlkOffset of FSRDieDumpHdr */
        pstDieDumpHdr->nOTPBlkSize  = *pBytesReturned;
        pstDieDumpHdr->nPIBlkOffset = pstDieDumpHdr->nOTPBlkOffset + 
                                      pstDieDumpHdr->nOTPBlkSize;

        /************************************************/
        /* STEP3. Set NAND Core reset mode              */
        /************************************************/
        nLLDRe = pstVol->LLD_IOCtl(nPDev,
                                   FSR_LLD_IOCTL_CORE_RESET,
                                   NULL,
                                   0,
                                   NULL,
                                   0,
                                   &nByteRet);
        if (nLLDRe != FSR_LLD_SUCCESS)
        {
            nRe = nLLDRe;
            /* message out */
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_IOCtl(nPDev:%d, nCode: FSR_LLD_IOCTL_CORE_RESET, nRe:0x%x)\r\n"),
                                            nPDev, nLLDRe));
            break;
        }

    } while(0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));

    return nRe;
}
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function dumps data from PI block.
 *
 *  @param [in]   nVol          : Volume number
 *  @param [out] *pBuf          : Buffer pointer for dump image
 *  @param [out] *pBytesReturned: Size of dump image
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_PAM_ACCESS_ERROR
 *  @return     some error returns
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PRIVATE INT32
_ReadPIBlk(UINT32      nVol,
           UINT8      *pBuf,
           UINT32     *pBytesReturned)
{
    UINT32         nPDev   = 0;    /* Physical device number   */
    UINT32         nDevIdx = 0;    /* Device Index             */
    UINT32         nDieIdx = 0;    /* Die Index                */
    UINT32         nByteRet= 0;    /* # of bytes of return     */
    INT32          nRe     = FSR_BML_SUCCESS;
    INT32          nLLDRe  = FSR_LLD_SUCCESS;
    BmlVolCxt      *pstVol;
    BmlDumpLog     *pstDumpLog;
    FSRVolDumpHdr  *pstVolDumpHdr;
    FSRDieDumpHdr  *pstDieDumpHdr;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));

    /* Get pointer to VolCxt structure */
    pstVol = _GetVolCxt(nVol);

    /* Get pointer to BmlDumpLog structure */
    pstDumpLog  = pstVol->pstDumpLog;

    /* Set nPDev, nDieIdx by pstDumpLog */
    nPDev       = pstVol->pstDumpLog->nPDev;
    nDevIdx     = nPDev - (nVol * DEVS_PER_VOL);
    nDieIdx     = pstVol->pstDumpLog->nDieIdx;

    /* Get pointer to FSRVolDumpHdr, FSRDieDumpHdr structure */
    pstVolDumpHdr = _GetVolDumpHdr(nVol);
    pstDieDumpHdr = &(pstVolDumpHdr->stDieDumpHdr[nDevIdx][nDieIdx]);

    /* NO PI Block */
    if (pstVol->pstDumpLog->bPIBlk == FALSE32)
    {
        /* Set pBytesReturned, nPIBlkSize, nDataAreaOffset, nPEOffset, of FSRDieDumpHdr */
        *pBytesReturned                     = 0;
        pstDieDumpHdr->nPIBlkSize           = 0;
        pstDieDumpHdr->nDataAreaOffset      = pstDieDumpHdr->nPIBlkOffset;
        pstDieDumpHdr->stPEntry[0].nPEOffset= pstDieDumpHdr->nDataAreaOffset;
        pstVol->pstDumpLog->nPEntryNum      = 0;
        pstVol->pstDumpLog->nPbn            = pstDieDumpHdr->stPEntry[0].n1stPbn;

        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));
        return nRe;
    }

    _PrintDumpStat( nDevIdx,
                    pstDumpLog,
                    pstVolDumpHdr,
                    pstDieDumpHdr);

    /************************************************************/
    /* <Procedure of _ReadPIBlk()                               */
    /* STEP1. Set PI Mode                                       */
    /* STEP2. Read data form PI block                           */
    /* STEP3. Set NAND Core reset mode                          */
    /************************************************************/
    do
    {
        /************************************************/
        /* STEP1. Set OTP Mode by FSR_LLD_IOCTL()       */
        /************************************************/
        nLLDRe = pstVol->LLD_IOCtl(nPDev,
                                   FSR_LLD_IOCTL_PI_ACCESS,
                                   (UINT8 *) &nDieIdx,
                                   sizeof(nDieIdx),
                                   NULL,
                                   0,
                                   &nByteRet);
        if (nLLDRe != FSR_LLD_SUCCESS)
        {
            nRe = nLLDRe;
            /* message out */
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_IOCtl(nPDev:%d, nCode: FSR_LLD_IOCTL_PI_ACCESS, nDieIdx:%d, nRe:0x%x)\r\n"),
                                            nPDev, nDieIdx, nLLDRe));
            break;
        }

        /****************************************/
        /* STEP2. Dump data form PI block       */
        /****************************************/
        nRe = _ReadBlk(nVol,
                       nPDev,
                       nDieIdx * pstVol->nNumOfBlksInDie,
                       pstVol->nNumOfPgsInSLCBlk,
                       pBuf,
                       pBytesReturned);
        if (nRe != FSR_BML_SUCCESS)
        {
            /* Message out */
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nRe:0x%x)\r\n"),
                                            __FSR_FUNC__, nVol, nRe));
            break;
        }

        /* Set nPIBlkSize, nSLCAreaOffset, nMLCAreaOffset of FSRDieDumpHdr */
        pstDieDumpHdr->nPIBlkSize           = *pBytesReturned;
        pstDieDumpHdr->nDataAreaOffset      = pstDieDumpHdr->nPIBlkOffset +
                                              pstDieDumpHdr->nPIBlkSize;
        pstDieDumpHdr->stPEntry[0].nPEOffset= pstDieDumpHdr->nDataAreaOffset;
        pstVol->pstDumpLog->nPEntryNum      = 0;
        pstVol->pstDumpLog->nPbn            = pstDieDumpHdr->stPEntry[0].n1stPbn;

        /************************************************/
        /* STEP3. Set NAND Core reset mode              */
        /************************************************/
        nLLDRe = pstVol->LLD_IOCtl(nPDev,
                                   FSR_LLD_IOCTL_CORE_RESET,
                                   NULL,
                                   0,
                                   NULL,
                                   0,
                                   &nByteRet);
        if (nLLDRe != FSR_LLD_SUCCESS)
        {
            nRe = nLLDRe;
            /* message out */
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_IOCtl(nPDev:%d, nCode: FSR_LLD_IOCTL_CORE_RESET, nRe:0x%x)\r\n"),
                                            nPDev, nLLDRe));
            break;
        }

    } while(0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));

    return nRe;
}
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function dumps data from data block.
 *
 *  @param [in]   nVol          : Volume number
 *  @param [out] *pBuf          : Buffer pointer for dump image
 *  @param [out] *pBytesReturned: Size of dump image
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_PAM_ACCESS_ERROR
 *  @return     some error returns
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PRIVATE INT32
_ReadDataBlk(UINT32         nVol,
             UINT8         *pBuf,
             UINT32        *pBytesReturned)
{
    UINT32          nPEIdx      = 0;    /* Dump Partition Entry Index   */
    UINT32          nDieIdx     = 0;    /* Die Index                    */
    UINT32          nDevIdx     = 0;    /* Device Index                 */
    UINT32          nPDev       = 0;    /* Physical device number       */
    UINT32          nPbn        = 0;    /* Physical Block number        */
    UINT32          nNumOfPgs   = 0;    /* # of pages(SLC / MLC)        */
    INT32           nRe = FSR_BML_SUCCESS;
    BmlDumpLog     *pstDumpLog;
    BmlVolCxt      *pstVol;
    FSRVolDumpHdr  *pstVolDumpHdr;
    FSRDieDumpHdr  *pstDieDumpHdr;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));

    /* Get pointer to BmlVolCxt structure */
    pstVol = _GetVolCxt(nVol);

    /* Get pointer to BmlDumpLog structure */
    pstDumpLog  = pstVol->pstDumpLog;

    /* Set nPDev, nPbn, nDevIdx, nDieIdx, nPEIdx */
    nPDev           = pstDumpLog->nPDev;
    nPbn            = pstDumpLog->nPbn;
    nDevIdx         = nPDev - (nVol * DEVS_PER_VOL);
    nDieIdx         = pstDumpLog->nDieIdx;
    nPEIdx          = pstDumpLog->nPEntryNum;

    /* Get pointer to FSRVolDumpHdr, FSRDieDumpHdr structure */
    pstVolDumpHdr   = _GetVolDumpHdr(nVol);
    pstDieDumpHdr   = &(pstVolDumpHdr->stDieDumpHdr[nDevIdx][nDieIdx]);

    /* Set nNumOfPgs */
    nNumOfPgs       = pstVol->nNumOfPgsInSLCBlk;
    if (pstDieDumpHdr->stPEntry[nPEIdx].nBlkType == FSR_DUMP_MLC_BLOCK)
    {
        nNumOfPgs   = pstVol->nNumOfPgsInMLCBlk;
    }

    _PrintDumpStat( nDevIdx,
                    pstDumpLog,
                    pstVolDumpHdr,
                    pstDieDumpHdr);

    /************************************************************/
    /* <Procedure of _ReadDataBlk()                             */
    /* STEP1. Dump data from current Pbn                        */
    /* STEP2. Increase data area size                           */
    /* STEP3. Set the next nPbn to read                         */
    /************************************************************/

    /********************************/
    /* STEP1. Read data block       */
    /********************************/
    nRe = _ReadBlk(nVol,
                   nPDev,
                   nPbn,
                   nNumOfPgs,
                   pBuf,
                   pBytesReturned);
    if (nRe != FSR_BML_SUCCESS)
    {
        /* Message out */
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nRe:0x%x)\r\n"),
                                        __FSR_FUNC__, nVol, nRe));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));
        return nRe;
    }

    /* STEP2. Increase data area size &   */
    pstDieDumpHdr->nDataAreaSize            += *pBytesReturned;
    pstDieDumpHdr->stPEntry[nPEIdx].nPESize += *pBytesReturned;

    /* STEP3. Set the next nPbn to read */
    nPbn    ++;
    if (nPbn < (pstDieDumpHdr->stPEntry[nPEIdx].n1stPbn + pstDieDumpHdr->stPEntry[nPEIdx].nNumOfPbn))
    {
        pstVol->pstDumpLog->nPbn    = nPbn;
    }
    else
    {
        nPEIdx ++;
        if (nPEIdx < pstDieDumpHdr->nNumOfPEntry)
        {
            nPbn                                        = pstDieDumpHdr->stPEntry[nPEIdx].n1stPbn;
            pstVol->pstDumpLog->nPbn                    = nPbn;
            pstVol->pstDumpLog->nPEntryNum              = nPEIdx;
            pstDieDumpHdr->stPEntry[nPEIdx].nPEOffset   = pstDieDumpHdr->stPEntry[nPEIdx-1].nPEOffset +
                                                          pstDieDumpHdr->stPEntry[nPEIdx-1].nPESize;
        }
        else /* The End of data block to read */
        {
            pstVol->pstDumpLog->nPbn                    = pstDieDumpHdr->stPEntry[0].n1stPbn;
            pstVol->pstDumpLog->nPEntryNum              = 0;
            pstVol->pstDumpLog->nSeqOrder               = BML_DUMP_CHK_LASTIMAGE;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));

    return FSR_BML_SUCCESS;
}
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function checks whether this device and die are last one or not.
 *
 *  @param [in]  nVol           : Volume number
 *  @param [in]  nDumpType      : FSR_DUMP_VOLUME
 *  @n                            FSR_DUMP_PARTITION
 *  @n                            FSR_DUMP_RESERVOIR_ONLY
 *  @param [in]  pstPEList      : Pointer to FSRPEList structure
 *  @param [out] pBuf           : Pointer to page buffer for dump
 *  @param [out] pBytesReturned : Number of bytes returned in pBuf
 *
 *  @return     FSR_BML_DUMP_COMPLETE
 *  @return     FSR_BML_DUMP_INCOMPLETE
 *  @return     some BML error returns
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PRIVATE INT32
_ChkLastImage(UINT32         nVol,
              UINT32         nDumpType,
              FSRDumpPEList *pstPEList,
              UINT8         *pBuf,
              UINT32        *pBytesReturned)
{
    UINT32          nDevIdx         = 0;    /* Device index             */
    UINT32          nDieIdx         = 0;    /* Die index                */
    UINT32          nNextDieOffset  = 0;    /* Die offset for next die  */
    INT32           nRe             = FSR_BML_SUCCESS;
    BmlVolCxt      *pstVol;
    BmlDumpLog     *pstDumpLog;
    FSRVolDumpHdr  *pstVolDumpHdr;
    FSRDieDumpHdr  *pstDieDumpHdr;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, nDumpType: 0x%x)\r\n"),
                                    __FSR_FUNC__, nVol, nDumpType));

    /* Get pointer to BmlVolCxt structure */
    pstVol          = _GetVolCxt(nVol);

    /* Get pointer to BmlDumpLog structure */
    pstDumpLog      = pstVol->pstDumpLog;

    /* Get pointer to FSRDieDumpHdr structure */
    pstVolDumpHdr   = _GetVolDumpHdr(nVol);

    /* Initialize pBytesReturned */
    *pBytesReturned = 0;

    /* Get nDevIdx, nDieIdx, pstDieDumpHdr */
    nDevIdx         = pstDumpLog->nPDev - (nVol * DEVS_PER_VOL);
    nDieIdx         = pstDumpLog->nDieIdx;
    pstDieDumpHdr   = &(pstVolDumpHdr->stDieDumpHdr[nDevIdx][nDieIdx]);

    /* Calculate next die offset */
    nNextDieOffset  = pstDieDumpHdr->nDataAreaOffset +
                      pstDieDumpHdr->nDataAreaSize;

    _PrintDumpStat( nDevIdx,
                    pstVol->pstDumpLog,
                    pstVolDumpHdr,
                    pstDieDumpHdr);

    /*
     * << Sequence for checking last die >>
     * STEP1. check last die of last device of volume
     * STEP2-1. If it is last one, rewrite volume dump header.
     *          Free gpstVolDumpHdr of FSRVolDumpHdr
     *          Free gpstDumpLsbPg of FSRDumpRcvdLsbPage
     * STEP2-2. Otherwise, set Die Dump Header for next die
     */

    /****************************************************/
    /* STEP1. check last die of last device of volume   */
    /****************************************************/
    pstDumpLog->nDieIdx++;
    if (pstDumpLog->nDieIdx >= pstVol->nNumOfDieInDev)
    {
        /* Initialize nDieIdx */
        pstDumpLog->nDieIdx = 0;

        pstDumpLog->nPDev++;
        nDevIdx = pstDumpLog->nPDev - (nVol * DEVS_PER_VOL);
        if (nDevIdx >= pstVol->nNumOfDev)
        {
            /* Initialize nPDev */
            pstDumpLog->nPDev   = 0;

            /* In case that FSR_BML_Open is not called */
            if (pstVol->bVolOpen != TRUE32)
            {
                /* Close a volume */
                nRe = _Close(nVol);
                if (nRe != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nRe:0x%x)\r\n"),
                                                    __FSR_FUNC__, nVol, nRe));
                    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));
                    return nRe;
                }
            }

            /************************************************************/
            /* STEP2-1. If it is last one, rewrite volume dump header   */
            /************************************************************/
            FSR_OAM_MEMCPY(pBuf, pstVolDumpHdr, sizeof(FSRVolDumpHdr));
            *pBytesReturned = sizeof(FSRVolDumpHdr);

            /* Free gpstVolDumpHdr of FSRVolDumpHdr*/
            if (gpstVolDumpHdr[nVol] != NULL)
            {
                FSR_OAM_Free(gpstVolDumpHdr[nVol]);
                gpstVolDumpHdr[nVol] = NULL;
            }

             FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));
            return FSR_BML_DUMP_COMPLETE;
        } /* End of "if (nDevIdx >= pstVol->nNumOfDev)" */
    } /* End of "if (pstDumpLog->nDieIdx...)" */

    /************************************************/
    /* STEP2-2. set Die Dump Header for next die    */
    /************************************************/
    /* Initialize nSeqOrder */
    pstDumpLog->nSeqOrder   = BML_DUMP_OTPBLK;

    /* Get nDevIdx, nDieIdx, pstDieDumpHdr */
    nDevIdx         = pstDumpLog->nPDev - (nVol * DEVS_PER_VOL);
    nDieIdx         = pstDumpLog->nDieIdx;
    pstDieDumpHdr   = &(pstVolDumpHdr->stDieDumpHdr[nDevIdx][nDieIdx]);

    /* Initialize pstDieDumpHdr */
    FSR_OAM_MEMSET(pstDieDumpHdr, 0x00, sizeof(FSRDieDumpHdr));

    /* Set DieOffset */
    pstDieDumpHdr->nDieOffset = nNextDieOffset;

    /* Set DieDumpHdr for next die */
    nRe = _SetDieDumpHdr(nVol,
                         nDumpType,
                         pstPEList,
                         pstDieDumpHdr);
    if (nRe != FSR_BML_SUCCESS)
    {
        /* Message out */
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nDumpType:0x%x, nRe:0x%x)\r\n"),
                                        __FSR_FUNC__, nVol, nDumpType, nRe));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));
        return nRe;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));

    return FSR_BML_DUMP_INCOMPLETE;
}
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function dumps data from physical device.
 *
 *  @param [in]  nVol           : Volume number
 *  @param [in]  nDumpType      : FSR_DUMP_VOLUME
 *  @n                            FSR_DUMP_PARTITION
 *  @n                            FSR_DUMP_RESERVOIR_ONLY
 *  @param [in]  nDumpOrder     : FSR_BML_FLAG_DUMP_FIRST
 *  @n                            FSR_BML_FLAG_DUMP_CONTINUE
 *  @param [in]  pstPEList      : Pointer to FSRPEList structure
 *  @param [out] pBuf           : Pointer to page buffer for dump
 *  @param [out] pBytesReturned : Number of bytes returned in pBuf
 *
 *  @return     FSR_BML_DUMP_COMPLETE
 *  @return     FSR_BML_DUMP_INCOMPLETE
 *  @return     FSR_BML_INVALID_PARAM
 *  @return     some error returns
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
FSR_BML_Dump(UINT32         nVol,
             UINT32         nDumpType,
             UINT32         nDumpOrder,
             FSRDumpPEList *pstPEList,
             UINT8         *pBuf,
             UINT32        *pBytesReturned)
{
    INT32           nRe     = FSR_BML_SUCCESS;
    INT32           nBMLRe  = FSR_BML_DUMP_INCOMPLETE;
    BmlVolCxt      *pstVol;
    BmlDumpLog     *pstDumpLog;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, nDumpType: 0x%x, nDumpOrder: 0x%x)\r\n"),
                                    __FSR_FUNC__, nVol, nDumpType, nDumpOrder));

    /* Check input parameter */
#if defined(BML_CHK_PARAMETER_VALIDATION)
    if ((nDumpType != FSR_DUMP_VOLUME)      &&
        (nDumpType != FSR_DUMP_PARTITION)   &&
        (nDumpType != FSR_DUMP_RESERVOIR_ONLY))
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nDumpType:0x%x, nRe:FSR_BML_INVALID_PARAM)\r\n"),
                                        __FSR_FUNC__, nVol, nDumpType));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nBMLRe: 0x%x)\r\n"),
                                        __FSR_FUNC__, FSR_BML_INVALID_PARAM));
        return FSR_BML_INVALID_PARAM;
    }

    if ((nDumpOrder != FSR_BML_FLAG_DUMP_FIRST) &&
        (nDumpOrder != FSR_BML_FLAG_DUMP_CONTINUE))
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nDumpOrder:0x%x, nRe:FSR_BML_INVALID_PARAM)\r\n"),
                                        __FSR_FUNC__, nVol, nDumpOrder));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nBMLRe: 0x%x)\r\n"),
                                        __FSR_FUNC__, FSR_BML_INVALID_PARAM));
        return FSR_BML_INVALID_PARAM;
    }

    if ((pstPEList  == NULL)            &&
        (nDumpType  == FSR_DUMP_PARTITION))
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, pstPEList:0x%x, nDumpType:0x%x, nRe:FSR_BML_INVALID_PARAM)\r\n"),
                                        __FSR_FUNC__, nVol, pstPEList, nDumpType));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nBMLRe: 0x%x)\r\n"),
                                        __FSR_FUNC__, FSR_BML_INVALID_PARAM));
        return FSR_BML_INVALID_PARAM;
    }

    if ((pBuf == NULL) || (pBytesReturned == NULL))
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, pBuf:0x%x, pBytesReturned:0x%x, nRe:FSR_BML_INVALID_PARAM)\r\n"),
                                        __FSR_FUNC__, nVol, pBuf, pBytesReturned));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nBMLRe: 0x%x)\r\n"),
                                        __FSR_FUNC__, FSR_BML_INVALID_PARAM));
        return FSR_BML_INVALID_PARAM;
    }
#endif /* BML_CHK_PARAMETER_VALIDATION */

    /* Get pointer to BmlVolCxt structure */
    pstVol = _GetVolCxt(nVol);

    /* Check the pointer to volume context */
    CHK_VOL_POINTER(pstVol);

    /* Get pointer to BmlDumpLog */
    pstDumpLog  = pstVol->pstDumpLog;

    /*
     * << Sequence of image dump>>
     * STEP1. Set dump header (volume/die dump header)
     * STEP2. Dump OTP block
     * STEP3. Dump PI block
     * STEP4. Dump data block
     * STEP5. Dump recovered LSB page in MLC block
     * STEP6. Check whether this die is last one or not.
     *        If this die is last one, rewrite dump header.
     *        Otherwise, reiterate STEP2 ~ STEP6.
     */

    if (nDumpOrder == FSR_BML_FLAG_DUMP_FIRST)
    {
        /* STEP1. Set dump header (volume/die dump header) */
        nRe = _StartDump(nVol,
                         nDumpType,
                         pstPEList,
                         pBuf,
                         pBytesReturned);
        if (nRe != FSR_BML_SUCCESS)
        {
            nBMLRe = nRe;
            /* Message out */
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nDumpType:0x%x, nDumpOrder:0x%x, nRe:0x%x)\r\n"),
                                            __FSR_FUNC__, nVol, nDumpType, nDumpOrder, nRe));
        }
    }
    else
    {
        switch(pstDumpLog->nSeqOrder)
        {
        case BML_DUMP_OTPBLK:
            /* Indicate next step(PI Block) */
            pstVol->pstDumpLog->nSeqOrder = BML_DUMP_PIBLK;

            /* STEP2. Dump OTP block */
            nRe = _ReadOTPBlk(nVol,
                              pBuf,
                              pBytesReturned);
            if (nRe != FSR_BML_SUCCESS)
            {
                /* Retry this step(OTP Block) */
                pstVol->pstDumpLog->nSeqOrder = BML_DUMP_OTPBLK;

                nBMLRe = nRe;
                /* Message out */
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nDumpType:0x%x, nDumpOrder:0x%x, nRe:0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nDumpType, nDumpOrder, nRe));
            }
            break;

        case BML_DUMP_PIBLK:
            /* Indicate next step(data Block) */
            pstVol->pstDumpLog->nSeqOrder = BML_DUMP_DATABLK;

            /* STEP3. Dump PI block */
            nRe = _ReadPIBlk(nVol,
                             pBuf,
                             pBytesReturned);
            if (nRe != FSR_BML_SUCCESS)
            {
                /* Retry this step(PI Block) */
                pstVol->pstDumpLog->nSeqOrder = BML_DUMP_PIBLK;

                nBMLRe = nRe;
                /* Message out */
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nDumpType:0x%x, nDumpOrder:0x%x, nRe:0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nDumpType, nDumpOrder, nRe));
            }
            break;

        case BML_DUMP_DATABLK:
            /* STEP4. Dump data block */
            nRe = _ReadDataBlk(nVol,
                               pBuf,
                               pBytesReturned);
            if (nRe != FSR_BML_SUCCESS)
            {
                nBMLRe = nRe;
                /* Message out */
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nDumpType:0x%x, nDumpOrder:0x%x, nRe:0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nDumpType, nDumpOrder, nRe));
            }
            break;

        case BML_DUMP_CHK_LASTIMAGE:
            /* STEP6. Check whether this die is last one or not. */
            nBMLRe = _ChkLastImage(nVol,
                                   nDumpType,
                                   pstPEList,
                                   pBuf,
                                   pBytesReturned);
            if ((nBMLRe != FSR_BML_DUMP_COMPLETE) &&
                (nBMLRe != FSR_BML_DUMP_INCOMPLETE))
            {
                /* Message out */
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nDumpType:0x%x, nDumpOrder:0x%x, nRe:0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nDumpType, nDumpOrder, nBMLRe));
            }
            break;

        default:
            /* abnormal case */
            nBMLRe = FSR_BML_CRITICAL_ERROR;
            break;

        } /* End of "switch(pstDumpLog->nSeqOrder)" */

    } /* End of "if (nDumpOrder == FSR_BML_FLAG_DUMP_FIRST)" */

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));

    return nBMLRe;
}
#endif /* FSR_NBL2 */
