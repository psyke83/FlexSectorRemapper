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
 * @file      FSR_BML_BadBlkMgr.c
 * @brief     This file contains the routine for managing Reservoir
 * @author    MinYoung Kim
 * @date      19-JAN-2007
 * @remark
 * REVISION HISTORY
 * @n  19-JAN-2007 [MinYoung Kim] : first writing
 * @n  01-FEB-2008 [SangHoon Choi] : checks the attribute of partition in FSR_BBM_Repartition
 *
 */
 
/*****************************************************************************/
/* Header file inclusions                                                    */
/*****************************************************************************/

#define  FSR_NO_INCLUDE_STL_HEADER
#include "FSR.h"

#include "FSR_BML_Config.h"
#include "FSR_BML_Types.h"
#include "FSR_BML_BadBlkMgr.h"
#include "FSR_BML_BBMCommon.h"
#include "FSR_BML_BBMMount.h"

/*****************************************************************************/
/* nUpdateType                                                               */
/*****************************************************************************/
#define     BML_TYPE_INIT_CREATE        (UINT16) (0x0000)
#define     BML_TYPE_WRITE_ERR          (UINT16) (0x0011)
#define     BML_TYPE_ERASE_ERR          (UINT16) (0x0022)
#define     BML_TYPE_LOCK_ERR           (UINT16) (0x0044)
#define     BML_TYPE_UPDATE_PIEXT       (UINT16) (0x1100)
#define     BML_TYPE_UPDATE_ERL         (UINT16) (0x2200)

/*****************************************************************************/
/* BAD mark value                                                            */
/*****************************************************************************/
#define     BML_BADMARKV_ERASEERR       (UINT16) (0x2222)
#define     BML_BADMARKV_WRITEERR       (UINT16) (0x4444)

/*****************************************************************************/
/* Cell type                                                                 */
/*****************************************************************************/
#define     BML_SLC_BLK                 (0x00000001)
#define     BML_MLC_BLK                 (0x00000002)

/*****************************************************************************/
/* progress info for ERL                                                     */
/*****************************************************************************/
#define     BML_PROGINFO_UPDATED        (0x0001)
#define     BML_PROGINFO_COPIED         (0x0002)

/*****************************************************************************/
/* progress info for ERL                                                     */
/*****************************************************************************/
#define     BML_NEXT_PREV_PG_OFFSET     (0x0001)
#define     BML_PREV_PG_OFFSET          (0x0002)
#define     BML_DUMMY_PG_OFFSET         (0x0003)

/*****************************************************************************/
/* plane index for multi-plane                                               */
/*****************************************************************************/
#define     BML_1ST_PLN_IDX             (0x0000)
#define     BML_2ND_PLN_IDX             (0x0001)

/*****************************************************************************/
/* Local variables definitions                                               */
/*****************************************************************************/
PRIVATE UINT32 gnWaitMSecForErError = 0;

/*****************************************************************************/
/* Function prototype                                                        */
/*****************************************************************************/
#if !defined(FSR_NBL2)
PRIVATE INT32   _Format             (BmlVolCxt     *pstVol, 
                                     BmlDevCxt     *pstDev, 
                                     FSRPartI      *pstPart,
                                     UINT32         nDieIdx);
PRIVATE BOOL32  _ConstructBMF       (BmlDevCxt     *pstDev, 
                                     BmlVolCxt     *pstVol, 
                                     UINT32         nDieIdx,
                                     UINT32        *pnNumOfSLCInitBad,
                                     UINT32        *pnNumOfMLCInitBad);
PRIVATE INT32   _WritePI            (BmlVolCxt     *pstVol,
                                     BmlDevCxt     *pstDev,
                                     UINT32         nDieIdx);
PRIVATE INT32   _MakeNewPCB         (BmlDevCxt     *pstDev, 
                                     BmlVolCxt     *pstVol, 
                                     UINT32         nDieIdx, 
                                     FSRPartI      *pstPart, 
                                     BOOL32         bInitFormat);
PRIVATE BOOL32  _LLDVerify          (BmlVolCxt     *pstVol, 
                                     BmlDevCxt     *pstDev, 
                                     UINT32         nDieIdx);
PRIVATE BOOL32 _MoveRsvBoundary     (BmlDevCxt     *pstDev,
                                     BmlReservoir  *pstRsv,
                                     UINT32         nDieIdx,
                                     UINT32         nBlkType);
PRIVATE INT32 _CheckRangeOfPartition(BmlVolCxt     *pstVol,
                                     FSRPartI      *pstPrevPartI,
                                     FSRPartI      *pstNewPartI);
PRIVATE FSRPartEntry* _GetFSRPartEntry(FSRPartI    *pstPartI,
                                     UINT32   nVun);
#endif /* FSR_NBL2 */

PRIVATE VOID    _SetAllocRB         (BmlReservoir  *pstRsv, 
                                     UINT32         nPbn);
PRIVATE BOOL32  _GetFreeRB          (BmlReservoir  *pstRsv,
                                     UINT32         nNumOfPlane,
                                     UINT32         nBlkType,
                                     UINT32         nDataType,
                                     UINT32         nNumOfRepBlks,
                                     UINT32        *pFreePbn,           
                                     BOOL32        *pbPaired);
PRIVATE INT32   _UpdateUPCB         (BmlDevCxt     *pstDev, 
                                     BmlVolCxt     *pstVol, 
                                     FSRPIExt      *pstPExt, 
                                     UINT32         nDieIdx, 
                                     UINT16         nUpdateType);
PRIVATE INT32   _UpdateLPCB         (BmlDevCxt     *pstDev, 
                                     BmlVolCxt     *pstVol, 
                                     FSRPartI      *pstPI, 
                                     UINT32         nDieIdx, 
                                     UINT16         nUpdateType);
PRIVATE INT32   _MakeUPCB           (BmlDevCxt     *pstDev, 
                                     BmlVolCxt     *pstVol, 
                                     FSRPIExt      *pstPExt, 
                                     UINT32         nDieIdx, 
                                     UINT32         nPCBIdx, 
                                     UINT16         nUpdateType);
PRIVATE INT32   _MakeLPCB           (BmlDevCxt     *pstDev, 
                                     BmlVolCxt     *pstVol, 
                                     FSRPartI      *pstPI, 
                                     UINT32         nDieIdx, 
                                     UINT32         nPCBIdx, 
                                     UINT16         nUpdateType);
PRIVATE VOID    _MakePCH            (BmlVolCxt     *pstVol,
                                     BmlDieCxt     *pstDie, 
                                     UINT8         *pBuf, 
                                     UINT16         nType);
PRIVATE VOID    _MakePIA            (UINT8         *pBuf, 
                                     VOID          *pstPI, 
                                     UINT16         nPCBType);
PRIVATE VOID    _MakeBMS            (BmlVolCxt     *pstVol,
                                     BmlReservoir  *pstRsv,
                                     UINT8         *pBuf,
                                     UINT32        *pBMFIdx, 
                                     UINT32         nPCBType, 
                                     UINT16         nUpdateType);
PRIVATE VOID    _MakeRCBS           (BmlReservoir  *pstRsv,
                                     UINT8         *pBuf,
                                     UINT32        *pRCBFIdx);
PRIVATE INT32   _LLDWrite           (BmlVolCxt     *pstVol,
                                     BmlDevCxt     *pstDev,
                                     UINT32         nDieIdx, 
                                     UINT32         nPbn,
                                     UINT32         nPgOffset,
                                     UINT8         *pMBuf, 
                                     FSRSpareBuf   *pSBuf,
                                     UINT32         nDataType,
                                     UINT32         nFlag);
PRIVATE INT32   _LLDErase           (BmlVolCxt     *pstVol,
                                     UINT32         nPDev,
                                     UINT32        *pnPbn,
                                     UINT32         nFlag);
PRIVATE INT32   _UpdateMetaData     (BmlDevCxt     *pstDev, 
                                     BmlVolCxt     *pstVol, 
                                     VOID          *pstPI,
                                     UINT32         nDieIdx, 
                                     UINT32         nPCBIdx, 
                                     UINT16         nUpdateType,
                                     UINT16         nPCBType);
PRIVATE INT32   _AllocPCB           (BmlDieCxt     *pstDie,
                                     BmlVolCxt     *pstVol, 
                                     UINT32         nType);
PRIVATE BOOL32  _RegBMF             (BmlVolCxt     *pstVol,
                                     BmlBMF        *pstBMF, 
                                     BmlReservoir  *pstRsv,
                                     UINT32         nCandidate);
PRIVATE VOID    _DeleteRCB          (BmlReservoir  *pstRsv,
                                     UINT32         nSbn);
PRIVATE VOID    _MakeBadBlk         (UINT32         nBadPbn, 
                                     UINT16         nMarkValue, 
                                     UINT32         nDieIdx,
                                     BmlDevCxt     *pstDev, 
                                     BmlVolCxt     *pstVol);
PRIVATE VOID    _DeletePcb          (UINT32         nPcb, 
                                     UINT32         nDieIdx, 
                                     BmlDevCxt     *pstDev, 
                                     BmlVolCxt     *pstVol);
PRIVATE UINT32  _TransSbn2Pbn       (UINT32         nSbn,
                                     BmlReservoir  *pstRsv);
PRIVATE INT32   _CopyBlk            (BmlVolCxt     *pstVol,
                                     BmlDevCxt     *pstDev,
                                     UINT32         nDieIdx,
                                     UINT32         nSrcPbn, 
                                     UINT32         nPgOff,
                                     UINT32         nDstPbn,
                                     BOOL32         bRecoverPrevErr);
PRIVATE INT32   _CopyBlkRefresh     (BmlVolCxt     *pstVol,
                                     BmlDevCxt     *pstDev,
                                     UINT32         nDieIdx,
                                     UINT32         nSrcPbn,
                                     UINT32         nDstPbn,
                                     BOOL32        *bRDFlag,
                                     BOOL32        *bUCReadErr,
                                     UINT32         nFlag);
PRIVATE VOID    _FreeRB             (BmlReservoir  *pstRsv, 
                                     UINT32         nPbn);
PRIVATE BOOL32  _GetNewREFBlk       (BmlDieCxt     *pstDie,
                                     UINT16        *pFreePbn);
PRIVATE BOOL32  _GetRepBlk          (BmlVolCxt     *pstVol,
                                     BmlDevCxt     *pstDev,
                                     UINT32         nBadSbn, 
                                     UINT32         nNumOfBadBlks,
                                     UINT32        *pnFreeBlk,
                                     BOOL32        *pbPaired);

/*
 * @brief           This function updates Erase Refresh List in UPCB block
 *
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]      *pstDev      : device context pointer
 * @param[in]       nDieIdx     : die index
 * @param[in]       nOrgSbn     : Sbn - block has read disturbance error
 * @param[in]       nFlag       : BML_FLAG_ERL_UPDATE / BML_FLAG_ERL_PROGRAM
 * @n                             BML_FLAG_ERL_DELETE
 *
 * @return          FSR_BML_SUCCESS  
 * @return          FSR_BML_NO_RSV_BLK_POOL
 * @return          FSR_BML_CRITICAL_ERROR  
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PUBLIC INT32
FSR_BBM_UpdateERL(BmlVolCxt    *pstVol,
                  BmlDevCxt    *pstDev,
                  UINT32        nDieIdx,
                  UINT32        nOrgSbn,
                  UINT32        nFlag)
{
    BmlReservoir   *pstRsv;
    BmlReservoirSh *pstRsvSh;
    BmlERL         *pstCurERL;

    INT32           nRet        = FSR_BML_SUCCESS;
    UINT32          nIdx        = 0;
    UINT32          nIdx2       = 0;
    UINT32          nLastERL    = 0;
    UINT32          nMinER      = 0;
    BOOL32          bExist;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(PDev: %d, nDieIdx: %d, nOrgSbn: %d, nFlag: 0x%x)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nDieIdx, nOrgSbn, nFlag));
    
    FSR_ASSERT((pstVol != NULL) && (pstDev != NULL));

    do
    {
        pstRsv    = pstDev->pstDie[nDieIdx]->pstRsv;
        pstRsvSh  = pstDev->pstDie[nDieIdx]->pstRsvSh;
        pstCurERL = pstRsv->pstERL;

#if defined (FSR_BML_DEBUG_CODE)
        /* ERL cheking code by ham 090413 */
        {
            UINT32   nValidCnt;

            nValidCnt = 0;

            for (nIdx = 0; nIdx < BML_MAX_ERL_ITEM; nIdx++)
            {
                if (pstCurERL->astERBlkInfo[nIdx].nSbn != 0xFFFF)
                {
                    nValidCnt++;
                }
            }
            FSR_ASSERT(pstCurERL->nCnt == nValidCnt);
        }
        /* ERL cheking code by ham 090413 */
#endif

        /* update ERL list in RAM */
        if (nFlag == BML_FLAG_ERL_UPDATE)
        {
            bExist = FALSE32;

            if (pstCurERL->nCnt != 0)
            {
                nMinER = pstCurERL->nCnt - 1;
                /* find list info of ERL list */
                for (nIdx = (BML_MAX_ERL_ITEM - 1); nIdx >= nMinER; nIdx--)
                {
                    if (pstCurERL->astERBlkInfo[nIdx].nSbn != 0xFFFF)
                    {
                        nLastERL = nIdx + 1;
                        break;
                    }
                }
            }

            /* scan all list */
            for (nIdx = 0; nIdx <= nLastERL; nIdx++)
            {
                /* check if the block is already in ERL list */
                if (pstCurERL->astERBlkInfo[nIdx].nSbn == nOrgSbn)
                {
                    bExist = TRUE32;
                    break;
                }
            }

            /* list is not updated in this case */
            if (bExist == TRUE32)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:INF]  Sbn %d already exists in Erase Refresh List (nDev=%d)\r\n"), nOrgSbn, pstDev->nDevNo));
                break;
            }

            /* if ERL block list is full */  
            if (pstCurERL->nCnt >= BML_MAX_ERL_ITEM)
            {
                /* print message but error is not returned */
                FSR_DBZ_RTLMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:INF] ERL block is full. Skip registering new sbn(%d)\r\n"), nOrgSbn));             
                break;
            }

            FSR_DBZ_RTLMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:INF]  Sbn %d is added into Erase Refresh List (nDev=%d)\r\n"), nOrgSbn, pstDev->nDevNo));

            pstCurERL->astERBlkInfo[nLastERL].nSbn      = (UINT16) nOrgSbn;
            pstCurERL->astERBlkInfo[nLastERL].nProgInfo = BML_PROGINFO_UPDATED;

            pstCurERL->nCnt++;
            
            /* just update ERL in RAM */
            pstRsvSh->nERLCntInRAM++;
        }
        /* Program ERL list into NAND */
        else if (nFlag == BML_FLAG_ERL_PROGRAM)
        {
            if (pstRsvSh->nERLCntInRAM > 0)
            {
                /* Update UPCB to write ERL list */
                nRet = _UpdateUPCB(pstDev, pstVol, pstVol->pPIExt, nDieIdx, BML_TYPE_UPDATE_ERL);
                if (nRet != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _UpdateUPCB() is failed\r\n")));
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] nERLFlag: 0x%x, nDieIdx: %d, nUpdateType: %d\r\n"), 
                                                    nFlag, nDieIdx, BML_TYPE_UPDATE_ERL));
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] <<CRITICAL ERROR>>\r\n")));                    
                    break;
                }

                /* reset number of ERL in RAM */
                pstRsvSh->nERLCntInRAM = 0;
            }
        }
        /* program ERL list into NAND even if there is no block in ERL */
        else if (nFlag == BML_FLAG_ERL_FORCED_PROGRAM)
        {
            /*
             * If UPCB has a read disturbance error, BBM writes new meta-data into TPCB by force
             * without updating ERL. This flag is only used at mount time to remove
             * the read dirturbance error from UPCB without copy operation.
             * This policy moves current meta-data into TPCB to avoid using of current UPCB
             * because meta-data can be lost if power is suddenly down during copy operation of meta-data.
             */

            /* 
             * Change a flag to exchange UPCB for TPCB 
             * even if current UPCB has no corrupted data 
             */
            pstRsvSh->bKeepUPCB = FALSE32;

            /* Update UPCB to write ERL list */
            nRet = _UpdateUPCB(pstDev, pstVol, pstVol->pPIExt, nDieIdx, BML_TYPE_UPDATE_ERL);
            if (nRet != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _UpdateUPCB() is failed\r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] nERLFlag: 0x%x, nDieIdx: %d, nUpdateType: %d\r\n"), 
                                                nFlag, nDieIdx, BML_TYPE_UPDATE_ERL));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] <<CRITICAL ERROR>>\r\n")));
                break;
            }

            /* reset number of ERL in RAM */
            pstRsvSh->nERLCntInRAM = 0;
        }
        /* delete a block in ERL list */
        else if (nFlag == BML_FLAG_ERL_DELETE)
        {
            bExist = FALSE32;

            /* scan all list */
            for (nIdx = 0; nIdx < pstCurERL->nCnt; nIdx++)
            {
                /* check if the block resides in ERL list */
                if (pstCurERL->astERBlkInfo[nIdx].nSbn == nOrgSbn)
                {
                    bExist = TRUE32;
                    break;
                }
            }
            
            /* the block should be deleted in ERL list because the block will be erased */
            if (bExist == TRUE32)
            {
                /* 
                 * delete the block from the list 
                 * However, pstRsv->nERLCntInRAM is not decreased
                 */
                pstCurERL->astERBlkInfo[nIdx].nSbn      = 0xFFFF;
                pstCurERL->astERBlkInfo[nIdx].nProgInfo = 0xFFFF;
                pstCurERL->nCnt--;
            }

            if (pstCurERL->nCnt != 0)
            {
                nMinER = pstCurERL->nCnt - 1;
                /* find list info of ERL list */
                for (nIdx = (BML_MAX_ERL_ITEM - 1); nIdx >= nMinER; nIdx--)
                {
                    if (pstCurERL->astERBlkInfo[nIdx].nSbn != 0xFFFF)
                    {
                        nLastERL = nIdx +1;
                        break;
                    }
                }

                /* Remove empty info of ERL list */
                nIdx = 0;
                do
                {
                    if (pstCurERL->astERBlkInfo[nIdx].nSbn == 0xFFFF)
                    {
                        for (nIdx2 = nIdx; nIdx2 < nLastERL; nIdx2++)
                        {
                            if (pstCurERL->astERBlkInfo[nIdx2].nSbn != 0xFFFF)
                            {
                                pstCurERL->astERBlkInfo[nIdx].nSbn       = 
                                                     pstCurERL->astERBlkInfo[nIdx2].nSbn;
                                pstCurERL->astERBlkInfo[nIdx].nProgInfo  = 
                                                pstCurERL->astERBlkInfo[nIdx2].nProgInfo;
                                pstCurERL->astERBlkInfo[nIdx2].nSbn      = 0xFFFF;
                                pstCurERL->astERBlkInfo[nIdx2].nProgInfo = 0xFFFF;
                                break;
                            }
                        }
                    }
                    nIdx++;
                } while (nIdx != nLastERL);
            }
        }
        /* delete a block in ERL list at UPCB */
        else if (nFlag == BML_FLAG_ERL_UPCB_DELETE)
        {
            bExist = FALSE32;

            /* scan all list */
            for (nIdx = 0; nIdx < pstCurERL->nCnt; nIdx++)
            {
                /* check if the block resides in ERL list */
                if (pstCurERL->astERBlkInfo[nIdx].nSbn == nOrgSbn)
                {
                    bExist = TRUE32;
                    break;
                }
            }
            
            /* the block should be deleted in ERL list because the block will be erased */
            if (bExist == TRUE32)
            {
                /* 
                 * delete the block from the list 
                 * However, pstRsv->nERLCntInRAM is not decreased
                 */
                pstCurERL->astERBlkInfo[nIdx].nSbn      = 0xFFFF;
                pstCurERL->astERBlkInfo[nIdx].nProgInfo = 0xFFFF;
                pstCurERL->nCnt--;
            }

        }
        /* invalid flag */
        else
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Invlalid flag(0x%x)\r\n"), nFlag));
            nRet = FSR_BML_CRITICAL_ERROR;
            break;
        }

    } while(0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nRet));

    return nRet;
}

/*
 * @brief           This function copies the given nSrcPbn to the given nDstPbn
 *
 * @param[in]      *pstVol        : BmlVolCxt structure pointer
 * @param[in]      *pstDev        : BmlDevCxt structure pointer
 * @param[in]       nBadSbn       : bad block sbn
 * @param[in]       nPgOff        : page offset
 * @param[in]       nNumOfBadBlks : number of bad blocks ( 1 <= n <= number of plane)
 * @param[in]       nFlag         : BML_HANDLE_WRITE_ERROR, BML_HANDLE_ERASE_ERROR,
 * @n                               
 *
 * @return          FSR_BML_SUCCESS
 * @return          FSR_BML_NO_RSV_BLK_POOL
 * @return          FSR_BML_CRITICAL_ERROR
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PUBLIC INT32
FSR_BBM_HandleBadBlk(BmlVolCxt *pstVol,
                     BmlDevCxt *pstDev,
                     UINT32     nBadSbn,
                     UINT32     nPgOff,
                     UINT32     nNumOfBadBlks,
                     UINT32     nFlag)
{
    LLDProtectionArg    stLLDPrtArg;
    BmlBMF              stmBmf;
    BmlReservoir       *pstRsv;
    UINT32              nDieIdx;
    UINT32              nFreeBlk[FSR_MAX_PLANES];
    UINT32              nBadPbn[FSR_MAX_PLANES];
    UINT32              nSbn[FSR_MAX_PLANES];
    UINT32              nByteReturned;
    UINT32              nErrPbn;
    UINT32              nPlnIdx;
    UINT32              nNumOfCopiedBlks = 0;
    UINT32              nCnt;
    BOOL32              bPaired;
    BOOL32              bRet   = TRUE32;
    BOOL32              bRecoverPrevErr = FALSE32;
    INT32               nBBMRe = FSR_BML_SUCCESS;
    INT32               nLLDRe = FSR_LLD_SUCCESS;
    UINT16              nUpdateType = 0xFFFF;
    UINT16              nLockState;
    UINT16              nBadMarkV   = 0xFFFF;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(PDev: %d, nBadSbn: %d, nPgOff: %d, nNumOfBadBlk: %d, nFlag: 0x%x)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nBadSbn, nPgOff, nNumOfBadBlks, nFlag));

    FSR_ASSERT((pstVol != NULL) && (pstDev != NULL));

    /* calculate die number */
    nDieIdx = (nBadSbn >> pstVol->nSftNumOfBlksInDie) & 0x1;

    /* get pointer of Reservoir and BMI */
    pstRsv = pstDev->pstDie[nDieIdx]->pstRsv;

    /* 
     * If Flex-OneNAND has only one SLC block (block #0),
     * the blocks can not be bad block becuase reservoirz is MLC reservoir 
     * and it has no SLC reservoir
     */
    if ((pstVol->nNANDType == FSR_LLD_FLEX_ONENAND) && 
        (pstDev->nNumOfSLCBlksInDie[nDieIdx] == pstVol->nNumOfPlane) &&
        (nBadSbn - (pstVol->nNumOfBlksInDie * nDieIdx) < pstVol->nNumOfPlane))
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] First block(%d) in die(%d) can not be bad block\r\n"), nBadSbn, nDieIdx));
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] number of SLC in a die(%d), number of plane(%d)\r\n"), pstDev->nNumOfSLCBlksInDie[nDieIdx], pstVol->nNumOfPlane));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FSR_BML_CRITICAL_ERROR));
        return FSR_BML_CRITICAL_ERROR;
    }
    
    /* loop up attribute of partition to know lock state of the block */
    nLockState = _LookUpPartAttr(pstVol, nDieIdx, (UINT16) nBadSbn);
    
    /* 
     * if nBadSbn is included in the locked area and pre-program flag is FALSE32. 
     * This is during refresh by erase operation. we should unlock blocks         
     */              
    if ((nLockState == FSR_LLD_BLK_STAT_LOCKED) & (pstVol->bPreProgram == FALSE32))
    {
        /* unlock bad Pbn (valid block is not included) */
       for (nCnt = 0; nCnt < nNumOfBadBlks; nCnt++)
       {
            nBadPbn[nCnt] = _TransSbn2Pbn(nBadSbn + nCnt, pstRsv);

           /* Set the parameter of LLD_IOCtl() */
            stLLDPrtArg.nStartBlk = nBadPbn[nCnt];
            stLLDPrtArg.nBlks     = 1;

            /* 
             * Result of IOCtl() is ignored. 
             * If unlock operation is failed, bad block handling procedure is continued
             */
            pstVol->LLD_IOCtl(pstDev->nDevNo,
                              FSR_LLD_IOCTL_UNLOCK_BLOCK,
                              (UINT8 *) &stLLDPrtArg,
                              sizeof(stLLDPrtArg),
                              (UINT8 *) &nErrPbn,
                              sizeof(nErrPbn),
                              &nByteReturned);
       }
    }

    /*
     *                [Available cases in multi plane device]
     * 
     *    Sbn#   n-1  n               n-1  n                   n-1  n
     *          +---+---+            +---+---+                +---+---+
     *          | B | V |            | V | B |                | B | B |
     *          +---+---+            +---+---+                +---+---+
     *      (one bad in plane0) (one bad in plane1)  (two bad in plane0 and plane1)
     *        
     * nBadSbn     n-1                   n                       n-1
     * # of blks    1                    1                        2
     *
     */
        
    /****************************************************
    [NOTE] DO NOT USE RETURN CODE within do-while loop
    ****************************************************/
    do 
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:   ] Bad block replace for dev(%d) and die(%d)\r\n"), pstDev->nDevNo, nDieIdx));
        /************************************************************************/
        /* 1. Get the free block number to replace nBadSbn                      */
        /************************************************************************/
        FSR_DBZ_RTLMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:   ] 1. Get FreePbn for nBadSbn(%d)\r\n"), nBadSbn));
        
        bRet = _GetRepBlk(pstVol, pstDev, nBadSbn, nNumOfBadBlks, &nFreeBlk[0], &bPaired);
        if (bRet == FALSE32)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:   ]    Can't get FreePbn\r\n")));
            nBBMRe = FSR_BML_NO_RSV_BLK_POOL;
            break;
        }

        /* paired replacement case */
        if ((bPaired == TRUE32) && (pstVol->nNumOfPlane > 1))
        {
            /* 
             * bad block and its adjacent block are copied into newly allocated reservoir blocks 
             * valid block can be replaced in paired replacement
             */
            nNumOfCopiedBlks = pstVol->nNumOfPlane;
        }
        /* unpaired replacement case */
        else
        {
            /* each bad blocks are copied into newly allocated reservoir blocks */
            nNumOfCopiedBlks = nNumOfBadBlks;
        }

        for (nCnt = 0; nCnt < nNumOfCopiedBlks; nCnt++)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:   ]    FreePbn : %d, bPaired : %d\r\n"), 
                                             nFreeBlk[nCnt], bPaired));

            /* paired replacement case */
            if (bPaired == TRUE32)
            {
                /* calculate bad sbn and its adjacent sbn using bad sbn */
                nSbn[nCnt] = ((nBadSbn / pstVol->nNumOfPlane) * pstVol->nNumOfPlane) + nCnt;
            }
            else
            {
                nSbn[nCnt] = nBadSbn + nCnt;
            }

            /************************************************************************/
            /* 2. Translates nBadSbn to nBadPbn                                     */
            /************************************************************************/
            nBadPbn[nCnt] = _TransSbn2Pbn(nSbn[nCnt], pstRsv);

            FSR_DBZ_RTLMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:   ] 2-%d. Translate nBadSbn(%d) to nBadPbn(%d)\r\n"), nCnt + 1, nSbn[nCnt], nBadPbn[nCnt]));

            /* if the type of error is write, copy data in errored block to good block */
            if ((nFlag & BML_HANDLE_WRITE_ERROR) == BML_HANDLE_WRITE_ERROR)
            {
                /************************************************************************/
                /* 3. Copy nBadPbn to Free Block in Reserved Block Pool                 */
                /************************************************************************/
                FSR_DBZ_RTLMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:   ] 3-%d. Copy nBadSbn(%d) to FreePbn(%d) (pg#0 - pg#%d)\r\n"), 
                                                 nCnt + 1, nBadPbn[nCnt], nFreeBlk[nCnt], nPgOff - 1));

                /* If both next previous operation and previous operation are failed (write error case) */
                if ((nFlag & BML_RECOVER_PREV_ERROR) == BML_RECOVER_PREV_ERROR)
                {
                    bRecoverPrevErr = TRUE32;
                }

                nLLDRe = _CopyBlk(pstVol, pstDev, nDieIdx, nBadPbn[nCnt], nPgOff, nFreeBlk[nCnt], bRecoverPrevErr);            
                if (nLLDRe != FSR_LLD_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR]    CopyBlk is failed(nBadPbn: %d, nPgOff: %d, nFreeBlk: %d, nPDev: %d, nDieIdx: %d, bRecoverPrevErr: %d\r\n"), 
                                                     nBadPbn[nCnt], nPgOff, nFreeBlk[nCnt], pstDev->nDevNo, nDieIdx, bRecoverPrevErr));
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR]    Retry to get the FreePbn\r\n")));

                    /* mark the given nBadSbn as bad block */                    
                    if (nLLDRe == FSR_LLD_PREV_ERASE_ERROR)
                    {
                        nBadMarkV = BML_BADMARKV_ERASEERR;
                    }
                    else
                    {
                        nBadMarkV = BML_BADMARKV_WRITEERR;
                    }

                    _MakeBadBlk(nFreeBlk[nCnt], nBadMarkV, nDieIdx, pstDev, pstVol);

                    
                    if (pstVol->nNumOfPlane > 1)
                    {
                        /* paired replacement case */
                        if (bPaired == TRUE32)
                        {
                            nPlnIdx = (nCnt + 1) & 0x1;
                            _FreeRB(pstRsv, nFreeBlk[nPlnIdx]);
                        }
                        /* unpaired replacement case */
                        else
                        {
                            /* delete allocated RCB data from the RCB queue */
                            _DeleteRCB(pstRsv, nFreeBlk[nCnt]);
                        }
                    }
                    
                    break;
                }

                /* 
                 * If the bad block has read disturbance, 
                 * the block is removed from ERL 
                 * because the block will not be accessed any more 
                 */
                nBBMRe = FSR_BBM_UpdateERL(pstVol,
                                           pstDev,
                                           nDieIdx,
                                           nBadPbn[nCnt],
                                           BML_FLAG_ERL_DELETE);
                if (nBBMRe != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] FSR_BBM_UpdateERL() is failed during Bad block replacement\r\n")));
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] nPDev: %d, nDieIdx: %d, nBadPbn: %d, nFlag: 0x%x\r\n"),
                                                   pstDev->nDevNo, nDieIdx, nBadPbn[nCnt], BML_FLAG_ERL_DELETE));
                    break;
                }

            }
        }

        /* if error occurs during copy operation in _CopyBlk() */
        if (nLLDRe != FSR_LLD_SUCCESS)
        {
            /* restart replace operation */
            continue;
        }

        nPlnIdx = 0xFFFFFFFF;

        /*************************************************************/
        /* 4. Register BMF into the free BMF entry of BMI            */
        /*************************************************************/

        FSR_DBZ_RTLMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:   ] 4. Register BMF into the free BMF Entry\r\n")));

        for (nCnt = 0; nCnt < nNumOfCopiedBlks; nCnt++)
        {
            stmBmf.nSbn       = (UINT16) nSbn[nCnt];
            stmBmf.nRbn       = (UINT16) nFreeBlk[nCnt];

            FSR_DBZ_RTLMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:   ]    stmBmf.nSbn  = %d\r\n"), stmBmf.nSbn));
            FSR_DBZ_RTLMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:   ]    stmBmf.nRbn  = %d\r\n"), stmBmf.nRbn));
            
            /* if a replaced block is valid for paired replacement case */
            if ((bPaired == TRUE32) &&
                (pstVol->nNumOfPlane > 1) &&
                (nNumOfBadBlks < pstVol->nNumOfPlane) &&
                (nBadSbn != nSbn[nCnt]))
            {
                /* If reservoir candidate should be registered */
                bRet = _RegBMF(pstVol, &stmBmf, pstRsv, nBadPbn[nCnt]);

                /* save plane index of valid block */
                nPlnIdx = nCnt;
            }
            /* if the block is bad */
            else
            {
                /* If there is nor reservoiar candidate block */
                bRet = _RegBMF(pstVol, &stmBmf, pstRsv, BML_INVALID);
            }
            
            if (bRet == FALSE32)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _RegBMF is failed\r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR]   <<CRITICAL ERROR>>\r\n")));
                nBBMRe = FSR_BML_CRITICAL_ERROR;
                break;
            }
        }

        /*************************************************************/
        /* 5. Register BMI into BMSG                                 */
        /*************************************************************/
        FSR_DBZ_RTLMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:   ] 5. Register BMI into BMSG\r\n")));

        if ((nFlag & BML_HANDLE_WRITE_ERROR) == BML_HANDLE_WRITE_ERROR)
        {
            nUpdateType =  BML_TYPE_WRITE_ERR;
        }
        else if ((nFlag & BML_HANDLE_ERASE_ERROR) == BML_HANDLE_ERASE_ERROR)
        {
            nUpdateType =  BML_TYPE_ERASE_ERR;
        }

        if (nLockState != FSR_LLD_BLK_STAT_LOCKED_TIGHT)
        {
            nBBMRe = _UpdateUPCB(pstDev, pstVol, pstVol->pPIExt, nDieIdx, nUpdateType); 
            if (nBBMRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _UpdateUPCB is failed\r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] nPdev: %d, nDieIdx: %d, nUpdateType: %d, nRet: 0x%x\r\n"), 
                                               pstDev->nDevNo, nDieIdx, nUpdateType, nBBMRe));
                break;
            }
        }
        else
        {
            nBBMRe = _UpdateLPCB(pstDev, pstVol, pstVol->pstPartI, nDieIdx, nUpdateType); 
            if (nBBMRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _UpdateLPCB is failed\r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] nPdev: %d, nDieIdx: %d, nUpdateType: %d, nRet: 0x%x\r\n"), 
                                               pstDev->nDevNo, nDieIdx, nUpdateType, nBBMRe));                
                break;
            }
        }
                
        /* reconstruct Bad Unit Map */
        _ReconstructBUMap(pstVol, pstRsv, nDieIdx);

        /* mark the given nBadPbn as bad block */
        if ((nFlag & BML_HANDLE_ERASE_ERROR) != 0)
        {
            nBadMarkV = BML_BADMARKV_ERASEERR;
        }
        else if ((nFlag & BML_HANDLE_WRITE_ERROR) != 0)
        {
            nBadMarkV = BML_BADMARKV_WRITEERR;
        }
        
        for (nCnt = 0; nCnt < nNumOfBadBlks; nCnt++)
        {
            /* 
             * paired replacement case in multi plane device 
             * (one bad block and one valid block)
             */
            if ((bPaired == TRUE32) &&
                (pstVol->nNumOfPlane > 1) &&
                (nNumOfBadBlks < pstVol->nNumOfPlane))
            {
                /* calculated plane index of bad block using plane index of valid block */
                nPlnIdx = (nPlnIdx + 1) & 0x1;

                /* write bad mark to bad block (Do not write bad mark to valid block) */
                _MakeBadBlk(nBadPbn[nPlnIdx], nBadMarkV, nDieIdx, pstDev, pstVol);
            }
            else
            {
                /* write bad mark to bad block */
                _MakeBadBlk(nBadPbn[nCnt], nBadMarkV, nDieIdx, pstDev, pstVol);
            }
        }


        for (nCnt = 0; nCnt < nNumOfCopiedBlks; nCnt++)
        {

            /* if nBadSbn is included in the locked area and pre-program flag is FALSE32. */
            /* This is during refresh by erase operation. we should lock the block again  */                  
            if ((nLockState == FSR_LLD_BLK_STAT_LOCKED) & (pstVol->bPreProgram == FALSE32))
            {
                /* Set the parameter of LLD_IOCtl() */
                stLLDPrtArg.nStartBlk = nBadPbn[nCnt];
                stLLDPrtArg.nBlks     = 1;

                /* 
                 * Result of IOCtl() is ignored because the block is already mapped out. 
                 * Even though lock operation is failed, this function never returns an error.
                 */
                pstVol->LLD_IOCtl(pstDev->nDevNo,
                                  FSR_LLD_IOCTL_LOCK_BLOCK,
                                  (UINT8 *) &stLLDPrtArg,
                                  sizeof(stLLDPrtArg),
                                  (UINT8 *) &nErrPbn,
                                  sizeof(nErrPbn),
                                  &nByteReturned);

                /* Set the parameter of LLD_IOCtl() to lock the allocated block in Reservoir */
                stLLDPrtArg.nStartBlk = nFreeBlk[nCnt];
                stLLDPrtArg.nBlks     = 1;

                nLLDRe = pstVol->LLD_IOCtl(pstDev->nDevNo,
                                           FSR_LLD_IOCTL_LOCK_BLOCK,
                                           (UINT8 *) &stLLDPrtArg,
                                           sizeof(stLLDPrtArg),
                                           (UINT8 *) &nErrPbn,
                                           sizeof(nErrPbn),
                                           &nByteReturned);
                
                if (nLLDRe != FSR_LLD_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Can't lock replaced block\r\n")));
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] nPDev: %d, nStartBlk: %d, number of blocks: 1\r\n"), 
                                                   pstDev->nDevNo, nFreeBlk));                    
                    break;
                }
            }
        }

        /* 
         * If lock operation of the reservoir block is failed, 
         * restart the bad block handling procedure
         */
        if (nLLDRe != FSR_LLD_SUCCESS)
        {
            continue;
        }

        /* If all operations are completed, exit do-while loop using break */
        break;

    /* 
     * DO NOT CHANGE while(1) to while(0) 
     * loop until bad block handling procedure is completed successfully
     */       

    } while (1);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nBBMRe));

    return nBBMRe;
}


/**
 * @brief          this function writes data to a page
 *
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]      *pstDev      : device context pointer
 * @param[in]       nDieIdx     : die index
 * @param[in]       nPbn        : physical block number
 * @param[in]       nPgOffset   : physical page offset 
 * @param[in]      *pMBuf       : main buffer pointer
 * @param[in]      *pSBuf       : spare buffer pointer
 * @param[in]       nDataType   : type of data to be written 
 * @param[in]       nFlag       : flag for write operation
 *
 * @return          FSR_LLD_SUCCESS  
 * @return          Some LLD errors
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE INT32
_LLDWrite(BmlVolCxt     *pstVol,
          BmlDevCxt     *pstDev,
          UINT32         nDieIdx, 
          UINT32         nPbn,
          UINT32         nPgOffset,
          UINT8         *pMBuf, 
          FSRSpareBuf   *pSBuf,
          UINT32         nDataType,
          UINT32         nFlag)
{   
    UINT32          nPDev;
    INT32           nRet = FSR_LLD_SUCCESS;
    BmlReservoir   *pstRsv;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(PDev: %d, nPbn: %d, nPgOff: %d, nDieIdx: %d, nDataType: 0x%x, nFlag: 0x%x)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nPbn, nPgOffset, nDieIdx, nDataType, nFlag));
    
    nPDev  = pstDev->nDevNo;
    pstRsv = pstDev->pstDie[nDieIdx]->pstRsv;

    /* BBM write meta data based on SLC page offset */

    /* when BBM writes meta data to the MLC area (for MLC Reservoir) */
    if ((nDataType == BML_META_DATA) && 
        (pstRsv->nRsvrType == BML_MLC_RESERVOIR))
    {
        /* convert SLC page offset to LSB page offset in MLC */
        nPgOffset = pstVol->pLSBPgMap[nPgOffset];
    }

    if (nDataType == BML_META_DATA)
    {
        /* 
         * If value of the flag is FSR_LLD_FLAG_BBM_META_BLOCK,
         * LLD write FSR_LLD_BBM_META_MARK in spare automatically 
         *
         * stSBuf.nBMLMetaBase0 = FSR_LLD_BBM_META_MARK; <- this statement
         * can be removed because upper layer of LLD does not need to set the value
         *
         * FSR_LLD_FLAG_USE_SPAREBUF : LLD write data in spare buffer
         */
        nFlag |= (FSR_LLD_FLAG_BBM_META_BLOCK | FSR_LLD_FLAG_USE_SPAREBUF);

        pSBuf->pstSpareBufBase->nBMLMetaBase0 = FSR_LLD_BBM_META_MARK;
    }

    /* If badmark flag is set, use not FSR_LLD_FLAG_USE_SPAREBUF*/
    if ((nDataType != BML_META_DATA) && (pSBuf != NULL))
    {
        if(((nFlag & FSR_LLD_FLAG_WR_EBADMARK) != FSR_LLD_FLAG_WR_EBADMARK) &&
            ((nFlag & FSR_LLD_FLAG_WR_EBADMARK) != FSR_LLD_FLAG_WR_EBADMARK))
        {
            /* LLD write data in spare buffer */
            nFlag |= FSR_LLD_FLAG_USE_SPAREBUF;
        }
    }

     /* LLD write */
    nRet = pstVol->LLD_Write(nPDev, nPbn, nPgOffset, pMBuf, pSBuf, nFlag);
    if (nRet != FSR_LLD_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] LLD_Write(nPdev: %d, nPbn: %d, nPgOffset: %d, nFlag: 0x%x, nDataType: %d) is failed\r\n"),
                                       nPDev, nPbn, nPgOffset, nFlag, nDataType));
        return nRet;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nRet));

    return pstVol->LLD_FlushOp(nPDev, nDieIdx, FSR_LLD_FLAG_NONE);
}

/**
 * @brief          this function erases a block
 *
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]       nPDev       : physical device number
 * @param[in]      *pnPbn       : pointer of physical block number list 
 * @param[in]       nFlag       : flag for erase operation
 *
 * @return          FSR_LLD_SUCCESS  
 * @return          Some LLD errors  
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE INT32
_LLDErase(BmlVolCxt *pstVol,
          UINT32     nPDev,
          UINT32    *pnPbn,
          UINT32     nFlag)
{
    UINT32  nDieIdx;
    INT32   nRet = FSR_LLD_SUCCESS;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(nPbn: %d, nFlag: 0x%x)\r\n"),
                                     __FSR_FUNC__, *pnPbn, nFlag));

    /* Erase blocks */
    nRet = pstVol->LLD_Erase(nPDev, pnPbn, 1, nFlag);
    if (nRet != FSR_LLD_SUCCESS)
    {
        FSR_BBM_WaitUntilPowerDn();
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] LLD_Erase(nPdev: %d, nPbn:%d) is failed\r\n"), nPDev, *pnPbn));
        return nRet;
    }
   
    /* Get an index of die */
    nDieIdx = *pnPbn >> pstVol->nSftNumOfBlksInDie;

    /* Flush a previous operation for a die */
    nRet = pstVol->LLD_FlushOp(nPDev, nDieIdx, FSR_LLD_FLAG_NONE);
    if (nRet != FSR_LLD_SUCCESS)
    {
        FSR_BBM_WaitUntilPowerDn();
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] LLD_FlushOp(nDev:%d, nDie:%d) is failed\r\n"), nPDev, nDieIdx));
        return nRet;            
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nRet));

    return nRet;
}

/**
 * @brief           This function sets timer value for erase error
 *
 * @param[in]       nNMSec   : timer value to be stored
 *
 * @return          none
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PUBLIC VOID  
FSR_BBM_SetWaitTimeForErError(UINT32 nNMSec)
{
    gnWaitMSecForErError = nNMSec;
}

#if !defined(FSR_NBL2)
/**
 * @brief           This function prints block mapping information
 *
 * @param[in]       *pstVol      : volume context pointer
 * @param[in]       *pstDev      : device context pointer
 *
 * @return          none
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PUBLIC VOID  
FSR_BBM_PrintBMI(BmlVolCxt *pstVol,
                 BmlDevCxt *pstDev)
{
    BmlReservoir   *pstRsv;
    BmlBMI         *pstBMI;
    FSRSpareBuf     stSBuf;
    UINT32          nDieIdx;
    UINT32          nBMFIdx;
    UINT32          nIdx;
    UINT32          nPbn;
    UINT32          nBadCnt;
    UINT16          nBadMark = 0xFFFF;
    UINT16          nLockState;
    BOOL32          bValid;
    
    FSR_STACK_VAR;

    FSR_STACK_END;

    nBadCnt = 0;

    for (nDieIdx = 0; nDieIdx < pstVol->nNumOfDieInDev; nDieIdx++)
    {
        pstRsv = pstDev->pstDie[nDieIdx]->pstRsv;
        pstBMI = pstRsv->pstBMI;

        FSR_DBZ_RTLMOUT(FSR_DBZ_BML_INF,  (TEXT("[BBM:   ]  << DevNO:%d, DieNO: %d MAPPING INFORMATION >>\r\n"), 
                                          pstDev->nDevNo, nDieIdx));

        FSR_DBZ_RTLMOUT(FSR_DBZ_BML_INF,  (TEXT("[BBM:   ]   Bad Mark Information\r\n")));
        FSR_DBZ_RTLMOUT(FSR_DBZ_BML_INF,  (TEXT("[BBM:   ]      - Bad Mark (0x%04x) by erase error\r\n"),     BML_BADMARKV_ERASEERR));
        FSR_DBZ_RTLMOUT(FSR_DBZ_BML_INF,  (TEXT("[BBM:   ]      - Bad Mark (0x%04x) by write error\r\n"),     BML_BADMARKV_WRITEERR));                

        for (nBMFIdx = 0; nBMFIdx < pstBMI->nNumOfBMFs; nBMFIdx++)
        {
            nPbn = pstRsv->pstBMF[nBMFIdx].nSbn;

            /* multi plane device */
            if (pstVol->nNumOfPlane > 1)
            {
                bValid = FALSE32;

                /* valid block can be replaced for paired replacement */
                
                for (nIdx = 0; nIdx < pstBMI->nNumOfRCBs; nIdx++)
                {
                    /* if the block is in the RCB queue, the block is valid */
                    if (nPbn == pstRsv->pstRCB[nIdx])
                    {
                        bValid = TRUE32;
                        break;
                    }
                }

                /* If the block is a valid block, do not print mapping info */
                if (bValid == TRUE32)
                {
                    continue;
                }

                for (nIdx = 0; nIdx < pstBMI->nNumOfBMFs; nIdx++)
                {
                    /* if the block is used as Rbn for bad replacement, the block is valid */
                    if (nPbn == pstRsv->pstBMF[nIdx].nRbn)
                    {
                        bValid = TRUE32;
                        break;
                    }
                }

                /* If the block is a valid block, do not print mapping info */
                if (bValid == TRUE32)
                {
                    continue;
                }

                /* otherwise, status of the block is locked or locked tightly */
            }

            FSR_OAM_MEMCPY(&stSBuf, pstRsv->pSBuf, sizeof(FSRSpareBuf));

            if (stSBuf.nNumOfMetaExt != 0)
            {
                stSBuf.nNumOfMetaExt = pstVol->nSizeOfPage / FSR_PAGE_SIZE_PER_SPARE_BUF_EXT;
            }

            /* Read page to get the bad mark in spare */
            _LLDRead(pstVol,
                     pstDev->nDevNo,
                     nPbn,
                     0,
                     pstRsv,
                     pstRsv->pMBuf,
                     &stSBuf,
                     BML_USER_DATA,
                     FALSE32,
                     FSR_LLD_FLAG_ECC_OFF);

            nBadMark = stSBuf.pstSpareBufBase->nBadMark;

            /* If the registered block is not valid block (bad block) */

            /* Get lock state of the block */
            nLockState = _LookUpPartAttr(pstVol, nDieIdx, (UINT16) nPbn);

            FSR_DBZ_RTLMOUT(FSR_DBZ_BML_INF,  (TEXT("[BBM:   ]   %04d: Sbn[%5d]==>Rbn[%5d] / BadMark: 0x%04x / "),
                                              nBadCnt,
                                              nPbn,
                                              pstRsv->pstBMF[nBMFIdx].nRbn,
                                              nBadMark));

            /* the block is SLC block */
            if (nPbn < pstRsv->n1stSbnOfMLC)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_BML_INF,  (TEXT("%s / "), "SLC"));
            }
            /* the block is MLC block */
            else
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_BML_INF,  (TEXT("%s / "), "MLC"));
            }

            /* lock state */
            if (nLockState == FSR_LLD_BLK_STAT_UNLOCKED)
            {   
                FSR_DBZ_RTLMOUT(FSR_DBZ_BML_INF,  (TEXT("Lock: UL\r\n")));
            }
            else if (nLockState == FSR_LLD_BLK_STAT_LOCKED)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_BML_INF,  (TEXT("Lock:  L\r\n")));
            }
            else
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_BML_INF,  (TEXT("Lock: LT\r\n")));
            }

            /* increase number of bad blocks */
            nBadCnt++;
        }

        FSR_DBZ_RTLMOUT(FSR_DBZ_BML_INF,  (TEXT("[BBM:   ]   << Total : %d BAD-MAPPING INFORMATION >>\r\n"), nBadCnt));

    }
}
#endif /* !defined(FSR_NBL2) */

#if !defined(FSR_NBL2)
/**
 * @brief           This function returns valid BMI
 *
 * @param[in]       *pstVol      : volume context pointer
 * @param[in]       *pstDev      : device context pointer
 * @param[in]        nDieIdx     : die index
 * @param[in]       *pstBMF      : pointer of buffer for copying bad block mapping info
 * @param[in]       *pnNumOfBMF  : pointer of number of bad block mapping to be copied
 *
 * @return          none
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PUBLIC VOID
FSR_BBM_GetBMI(BmlVolCxt *pstVol,
               BmlDevCxt *pstDev,
               UINT32     nDieIdx,
               BmlBMF    *pstBMF,
               UINT32    *pnNumOfBMF)
{
    BmlReservoir   *pstRsv;
    BmlBMI         *pstBMI;
    UINT32          nBMFIdx;
    UINT32          nIdx;
    UINT32          nPbn;
    UINT32          nBadCnt;
    BOOL32          bValid;
    
    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(nDev: %d, nDieIdx: %d)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nDieIdx));

    nBadCnt = 0;

    pstRsv = pstDev->pstDie[nDieIdx]->pstRsv;
    pstBMI = pstRsv->pstBMI;

    for (nBMFIdx = 0; nBMFIdx < pstBMI->nNumOfBMFs; nBMFIdx++)
    {
        nPbn = pstRsv->pstBMF[nBMFIdx].nSbn;

        /* multi plane device */
        if (pstVol->nNumOfPlane > 1)
        {
            bValid = FALSE32;

            /* valid block can be replaced for paired replacement */
            
            for (nIdx = 0; nIdx < pstBMI->nNumOfRCBs; nIdx++)
            {
                /* if the block is in the RCB queue, the block is valid */
                if (nPbn == pstRsv->pstRCB[nIdx])
                {
                    bValid = TRUE32;
                    break;
                }
            }

            /* If the block is a valid block, do not print mapping info */
            if (bValid == TRUE32)
            {
                continue;
            }

            for (nIdx = 0; nIdx < pstBMI->nNumOfBMFs; nIdx++)
            {
                /* if the block is used as Rbn for bad replacement, the block is valid */
                if (nPbn == pstRsv->pstBMF[nIdx].nRbn)
                {
                    bValid = TRUE32;
                    break;
                }
            }

            /* If the block is a valid block, do not print mapping info */
            if (bValid == TRUE32)
            {
                continue;
            }

            /* otherwise, status of the block is locked or locked tightly */
        }

        pstBMF[nBadCnt].nSbn = (UINT16) nPbn;
        pstBMF[nBadCnt].nRbn = pstRsv->pstBMF[nBMFIdx].nRbn;

        /* increase number of bad blocks */
        nBadCnt++;
    }

    *pnNumOfBMF = nBadCnt;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s()\r\n"), __FSR_FUNC__));
}
#endif /* !defined(FSR_NBL2) */

/**
 * @brief          This function refreshs the block which has read disturbance error
 *
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]      *pstDev      : device context pointer
 * @param[in]       nDieIdx     : die index number
 * @param[in]       nFlag       : flag decides whether all blocks are refreshed at a time
 *
 * @return          FSR_BML_SUCCESS
 * @return          FSR_BML_NO_RSV_BLK_POOL
 * @return          FSR_BML_ERASE_REFRESH_FAILURE  
 * @return          FSR_BML_CANT_LOCK_BLOCK  
 * @return          FSR_BML_CANT_UNLOCK_BLOCK
 * @return          FSR_BML_CRITICAL_ERROR  
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PUBLIC INT32 
FSR_BBM_RefreshByErase(BmlVolCxt   *pstVol,
                       BmlDevCxt   *pstDev,
                       UINT32       nDieIdx,
                       UINT32       nFlag)
{
    BmlReservoir       *pstRsv;
    BmlReservoirSh     *pstRsvSh;
    BmlERL             *pstERL;
    LLDProtectionArg    stLLDPrtArg;
    BOOL32              bReadFlag   = TRUE32;
    BOOL32              bUCReadErr  = FALSE32;
    BOOL32              bFixedBBMRe = FALSE32;
    INT32               nBBMRe      = FSR_BML_SUCCESS;
    INT32               nLLDRe      = FSR_LLD_SUCCESS;
    INT32               nRet        = FSR_LLD_SUCCESS;
    UINT32              nLockStat   = FSR_LLD_BLK_STAT_UNLOCKED;
    UINT32              nCnt;
    UINT32              nIdx;
    UINT32              nOrgSbn;
    UINT32              nOrgPbn;
    UINT32              nREFPbn;
    UINT32              nByteRet;
    UINT32              nReadFlag   = FALSE32;
    UINT32              nProcessedCnt;
    UINT32              nErrPbn;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(PDev: %d, nDieIdx: %d, nFlag: 0x%x)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nDieIdx, nFlag));

    pstRsv     = pstDev->pstDie[nDieIdx]->pstRsv;
    pstRsvSh   = pstDev->pstDie[nDieIdx]->pstRsvSh;
    pstERL     = pstRsv->pstERL;

    /* get number of blocks in the ERL */
    nCnt = pstERL->nCnt;

    /* 
     * If the flag is BML_FLAG_REFRESH_PARTIAL, blocks are refreshed up to 16 at a time.
     * However the flag is BBM_FLAG_REFRESH_ALL, all blocks are refreshed.
     */
    if (((nFlag & BML_FLAG_REFRESH_PARTIAL) == BML_FLAG_REFRESH_PARTIAL) && 
        (nCnt > FSR_BML_MAX_PROCESSABLE_ERL_CNT))
    {
        /* max 16 blocks can be refreshed at a time */
        nCnt = FSR_BML_MAX_PROCESSABLE_ERL_CNT;
    }
    else if ((nFlag & BML_FLAG_REFRESH_USER) == BML_FLAG_REFRESH_USER)
    {
        nCnt = nFlag >> 16;
    }

    for (nIdx = 0, nProcessedCnt = 0; (nProcessedCnt < nCnt) && (nIdx < BML_MAX_ERL_ITEM); nIdx++)
    {                
        /* Free slot is skipped */
        if ((pstERL->astERBlkInfo[nIdx].nSbn == 0xFFFF) &&
            (pstERL->astERBlkInfo[nIdx].nProgInfo == 0xFFFF))
        {
            /* move to next block */
            continue;
        }
        
        nOrgSbn = pstERL->astERBlkInfo[nIdx].nSbn;
        nOrgPbn = _TransSbn2Pbn(nOrgSbn, pstRsv);

        /* get lock state of orginal block */
        nLLDRe = pstVol->LLD_IOCtl(pstDev->nDevNo,
                                   FSR_LLD_IOCTL_GET_LOCK_STAT,
                                   (UINT8 *) &nOrgPbn,
                                   sizeof(nOrgPbn),
                                   (UINT8*) &nLockStat,
                                   sizeof(nLockStat),
                                   &nByteRet);
        if (nLLDRe != FSR_LLD_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BBM:ERR] Can not get lock status of original block\r\n")));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BBM:ERR] nPDev: %d, nOrgPbn: %d, nLLDRe: 0x%x\r\n"), pstDev->nDevNo, nOrgPbn, nLLDRe));
            
            /* remove this block from ERL */
            pstERL->astERBlkInfo[nIdx].nSbn      = 0xFFFF;
            pstERL->astERBlkInfo[nIdx].nProgInfo = 0xFFFF;
            pstERL->nCnt--;
            
            /* Update UPCB to write ERL list (return value is not checked) */
            _UpdateUPCB(pstDev, pstVol, pstVol->pPIExt, nDieIdx, BML_TYPE_UPDATE_ERL);

            /* the block is skipped without returning error */
            continue;
        }
        
        /* if the block is locked */
        if (nLockStat == FSR_LLD_BLK_STAT_LOCKED)
        {
            /* Set the parameter of LLD_IOCtl() */
            stLLDPrtArg.nStartBlk = nOrgPbn;
            stLLDPrtArg.nBlks     = 1;

            nLLDRe = pstVol->LLD_IOCtl(pstDev->nDevNo,
                                       FSR_LLD_IOCTL_UNLOCK_BLOCK,
                                       (UINT8 *) &stLLDPrtArg,
                                       sizeof(stLLDPrtArg),
                                       (UINT8 *) &nErrPbn,
                                       sizeof(nErrPbn),
                                       &nByteRet);
            /* If unlock operation is failed, replaced orginal pbn */
            if (nLLDRe != FSR_LLD_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BBM:ERR] Unlocking some area is failed\r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BBM:ERR] nPDev: %d, nOrgPbn: %d, nLLDRe: 0x%x\r\n"), pstDev->nDevNo, nOrgPbn, nLLDRe));

                /* remove this block from ERL */
                pstERL->astERBlkInfo[nIdx].nSbn      = 0xFFFF;
                pstERL->astERBlkInfo[nIdx].nProgInfo = 0xFFFF;
                pstERL->nCnt--;
                
                /* Update UPCB to write ERL list (return value is not checked) */
                _UpdateUPCB(pstDev, pstVol, pstVol->pPIExt, nDieIdx, BML_TYPE_UPDATE_ERL);

                nBBMRe = FSR_BML_CANT_UNLOCK_BLOCK;

                /* the block is skipped without returning error */
                continue;
            }

        }
        else if (nLockStat == FSR_LLD_BLK_STAT_LOCKED_TIGHT)
        {
            /* 
             * If lock state of the block is lock tight,
             * remove this block from the ERL list
             */
            pstERL->astERBlkInfo[nIdx].nSbn      = 0xFFFF;
            pstERL->astERBlkInfo[nIdx].nProgInfo = 0xFFFF;
            pstERL->nCnt--;

            /* Update UPCB to write ERL list (return value is not checked) */
            _UpdateUPCB(pstDev, pstVol, pstVol->pPIExt, nDieIdx, BML_TYPE_UPDATE_ERL);

            continue;
        }

        do
        {
            /*
             * Step1: Erase REF block
             */
            nREFPbn = pstRsvSh->nREFSbn;

            nLLDRe = _LLDErase(pstVol, pstDev->nDevNo, &nREFPbn, FSR_LLD_FLAG_1X_ERASE);

            while (nLLDRe != FSR_LLD_SUCCESS)
            {
                nRet = FSR_BBM_HandleBadBlk(pstVol, 
                                            pstDev, 
                                            pstRsvSh->nREFSbn, 
                                            0, 
                                            1,
                                            BML_HANDLE_ERASE_ERROR);
                if (nRet == FSR_BML_NO_RSV_BLK_POOL)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Bad block handling is caused by no reservoir pool \r\n")));                    
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] FSR_BBM_HandleBadBlk(nPDev: %d, nOrgSbn: %d, nFlag: 0x%x) is failed\r\n"),
                                                   pstDev->nDevNo, nOrgSbn, BML_HANDLE_ERASE_ERROR));
                    nBBMRe = nRet;
                    break;
                }
                else if (nRet != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Bad block replacement for REF block is failed(at step6)\r\n")));
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] nPDev: %d, nREFPbn: %d, nUpdateType: 0x%x\r\n"),
                                                     pstDev->nDevNo, pstRsvSh->nREFSbn, BML_HANDLE_ERASE_ERROR));
                    nBBMRe = FSR_BML_ERASE_REFRESH_FAILURE;
                    break;
                }

                nREFPbn = _TransSbn2Pbn(pstRsvSh->nREFSbn, pstRsv);
                pstRsvSh->nREFSbn = (UINT16) nREFPbn;

                nLLDRe = _LLDErase(pstVol, pstDev->nDevNo, &nREFPbn, FSR_LLD_FLAG_1X_ERASE);
            }

            if (nBBMRe != FSR_BML_SUCCESS)
            {
                break;
            }

            pstRsvSh->nNextREFPgOff = 0;

            /*
             * Step2: copy data in original block to REF block
             */

            /* copy original data to REF block */
            nRet = _CopyBlkRefresh(pstVol, pstDev, nDieIdx, nOrgPbn, (UINT32)pstRsvSh->nREFSbn, &bReadFlag, &bUCReadErr, nFlag);

            if (nRet != FSR_LLD_SUCCESS)
            {
                /* write error case */
                if (nRet == FSR_LLD_PREV_WRITE_ERROR)
                {
                    /* 
                     * repeat the bad block handling 
                     * until replacement is successfully completed 
                     */
                    do
                    {

                        nRet = FSR_BBM_HandleBadBlk(pstVol, 
                                                    pstDev, 
                                                    pstRsvSh->nREFSbn, 
                                                    0,
                                                    1,
                                                    BML_HANDLE_WRITE_ERROR);

                        if (nRet == FSR_BML_NO_RSV_BLK_POOL)
                        {
                            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Bad block handling is caused by no reservoir pool \r\n")));                    
                            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] FSR_BBM_HandleBadBlk(nPDev: %d, nOrgSbn: %d, nFlag: 0x%x) is failed\r\n"),
                                                           pstDev->nDevNo, nOrgSbn, BML_HANDLE_WRITE_ERROR));
                            nBBMRe = nRet;
                            break;
                        }
                        else if (nRet != FSR_BML_SUCCESS)
                        {
                            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BBM:ERR] Bad block replacement for REF block is failed(at Step1)\r\n")));
                            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BBM:ERR] nPDev: %d, nREFSbn: %d, nFlag: 0x%x\r\n"), 
                                                            pstDev->nDevNo, pstRsvSh->nREFSbn, BML_HANDLE_WRITE_ERROR));
                            nBBMRe = FSR_BML_ERASE_REFRESH_FAILURE;
                            break;
                        }

                        /* get newly allocated block address */
                        nREFPbn = _TransSbn2Pbn(pstRsvSh->nREFSbn, pstRsv);

                        /* update REF Sbn to replaced block */
                        pstRsvSh->nREFSbn = (UINT16) nREFPbn;

                        /* Erase REF block */
                        nLLDRe = _LLDErase(pstVol, pstDev->nDevNo, &nREFPbn, FSR_LLD_FLAG_1X_ERASE);

                    } while (nLLDRe != FSR_LLD_SUCCESS);

                    if (nBBMRe != FSR_BML_SUCCESS)
                    {
                        break;
                    }
                    else
                    {
                        /* restart copy operation */
                        continue;
                    }
                }
                /* other  error case */
                else
                {
                    nBBMRe = FSR_BML_ERASE_REFRESH_FAILURE;
                    break;
                }
            }
        } while (nRet != FSR_LLD_SUCCESS);

        if (nBBMRe != FSR_BML_SUCCESS)
        {
            /* if some error occurs, skip this block and move to next block */
            continue;
        }

        /* 
         * if read disturbance error does not occur during copy operation or 
         * if uncorrectable read error occurs
         */
        if (bUCReadErr == TRUE32)
        {
            /* 
             * remove this block from the ERL list 
             * because no read disturbance error appears during copy operation 
             * or uncorrectable read errror occurs
             */
            pstERL->astERBlkInfo[nIdx].nSbn      = 0xFFFF;
            pstERL->astERBlkInfo[nIdx].nProgInfo = 0xFFFF;
            pstERL->nCnt--;

            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BBM:INF]   %02d: Pbn [%5d] isn't refreshed / no disturb error / REFPbn: %d\r\n"), 
                                            nProcessedCnt, nOrgPbn, pstRsvSh->nREFSbn));

            /* 
             * Run time Erase Refresh case:
             * if BML_FLAG_NOTICE_READ_ERROR is set,
             * uncorrectable read error should be returned to caller 
             */
            if ((nFlag & BML_FLAG_NOTICE_READ_ERROR) == BML_FLAG_NOTICE_READ_ERROR)
            {
                /* BBM notice caller that unccorectable read error occurs */
                bFixedBBMRe = TRUE32;
            }
        }
        else
        {
             /* update progress info to keep the current status */
            pstERL->astERBlkInfo[nIdx].nProgInfo = BML_PROGINFO_COPIED;

            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BBM:INF]   %02d: Pbn [%5d] is refreshed / REFPbn: %d\r\n"), 
                                            nProcessedCnt, nOrgPbn, pstRsvSh->nREFSbn));

            /*
             * Step3: Update ERL list
             */

            /* Update UPCB to write ERL list */
            nBBMRe = _UpdateUPCB(pstDev, pstVol, pstVol->pPIExt, nDieIdx, BML_TYPE_UPDATE_ERL);
            if (nBBMRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _UpdateUPCB is failed during erase refresh operation\r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] nPDev: %d, nDieIdx: %d, nUpdateType: 0x%x, nRet: 0x%x\r\n"), 
                                                pstDev->nDevNo, nDieIdx, BML_TYPE_UPDATE_ERL, nBBMRe));
                /* move to next block */
                continue;
            }

            /*
             * Step4: Erase original block
             */

            nRet = _LLDErase(pstVol, pstDev->nDevNo, &nOrgPbn, FSR_LLD_FLAG_1X_ERASE);

            while (nRet != FSR_LLD_SUCCESS)
            {   
                nRet = FSR_BBM_HandleBadBlk(pstVol, 
                                           pstDev, 
                                           nOrgSbn, 
                                           0,
                                           1,
                                           BML_HANDLE_ERASE_ERROR);

                if (nRet == FSR_BML_NO_RSV_BLK_POOL)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Bad block handling is caused by no reservoir pool \r\n")));                    
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] FSR_BBM_HandleBadBlk(nPDev: %d, nOrgSbn: %d, nFlag: 0x%x) is failed\r\n"),
                                                   pstDev->nDevNo, nOrgSbn, BML_HANDLE_ERASE_ERROR));
                    nBBMRe = nRet;
                    break;
                }
                else if (nRet != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Bad block replacement for original block is failed(at step3)\r\n")));
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] nPDev: %d, nOrgSbn: %d, nUpdateType: 0x%x\r\n"), 
                                                   pstDev->nDevNo, nOrgSbn, BML_HANDLE_ERASE_ERROR));
                    nBBMRe = FSR_BML_ERASE_REFRESH_FAILURE;
                    break;
                }   

                nOrgPbn = _TransSbn2Pbn(nOrgSbn, pstRsv);
                nRet = _LLDErase(pstVol, pstDev->nDevNo, &nOrgPbn, FSR_LLD_FLAG_1X_ERASE);
            }

            if (nBBMRe != FSR_BML_SUCCESS)
            {
                /* move to next block */
                continue;
            }

            /*
             * Step5: copy data in REF block to original block
             */

            do
            {
                nRet = _CopyBlkRefresh(pstVol, pstDev, nDieIdx, (UINT32)pstRsvSh->nREFSbn, nOrgPbn, &nReadFlag, &bUCReadErr, nFlag);

                if (nRet != FSR_LLD_SUCCESS)
                {
                    /* write error case */
                    if (nRet == FSR_LLD_PREV_WRITE_ERROR)
                    {
                        do
                        {
                            nRet = FSR_BBM_HandleBadBlk(pstVol,
                                                        pstDev,
                                                        nOrgPbn,
                                                        0,
                                                        1,
                                                        BML_HANDLE_WRITE_ERROR);
                            if (nRet == FSR_BML_NO_RSV_BLK_POOL)
                            {
                                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Bad block handling is caused by no reservoir pool \r\n")));                    
                                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] FSR_BBM_HandleBadBlk(nPDev: %d, nOrgSbn: %d, nFlag: 0x%x) is failed\r\n"),
                                                               pstDev->nDevNo, nOrgSbn, BML_HANDLE_WRITE_ERROR));
                                nBBMRe = nRet;
                                break;
                            }
                            else if (nRet != FSR_BML_SUCCESS)
                            {
                                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Bad block replacement for original block is failed(at step4)\r\n")));
                                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] nPDev: %d, nOrgPbn: %d, nUpdateType: 0x%x\r\n"),
                                                               pstDev->nDevNo, nOrgPbn, BML_HANDLE_WRITE_ERROR));
                                nBBMRe = FSR_BML_ERASE_REFRESH_FAILURE;
                                break;
                            }

                            nOrgPbn = _TransSbn2Pbn(nOrgPbn, pstRsv);

                            nLLDRe = _LLDErase(pstVol, pstDev->nDevNo, &nOrgPbn, FSR_LLD_FLAG_1X_ERASE);

                        } while (nLLDRe != FSR_LLD_SUCCESS);
                    }                
                    /* other error case */
                    else
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Error occurs during erase refresh operation\r\n")));
                        nBBMRe = FSR_BML_ERASE_REFRESH_FAILURE;
                    }
                }
                
                if (nBBMRe != FSR_BML_SUCCESS)
                {
                    break;
                }

            } while (nRet != FSR_LLD_SUCCESS);

            if (nBBMRe != FSR_BML_SUCCESS)
            {
                /* move to next block */
                continue;
            }

            /*
             * Step6: update ERL list (delete list)
             */

            /* 
             * remove this block from the ERL list after copy operation is completed
             */
            pstERL->astERBlkInfo[nIdx].nSbn      = 0xFFFF;
            pstERL->astERBlkInfo[nIdx].nProgInfo = 0xFFFF;
            pstERL->nCnt--;

        }

        /* increase number of processed blocks */
        nProcessedCnt++;
        
        if (nBBMRe != FSR_BML_SUCCESS)
        {
            /* ignore return value */
            _UpdateUPCB(pstDev, pstVol, pstVol->pPIExt, nDieIdx, BML_TYPE_UPDATE_ERL);
            continue;
        }
        else
        {
            /* Update UPCB to write ERL list */
            nBBMRe = _UpdateUPCB(pstDev, pstVol, pstVol->pPIExt, nDieIdx, BML_TYPE_UPDATE_ERL);
            if (nBBMRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _UpdateUPCB is failed during erase refresh operation\r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] nPDev: %d, nDieIdx: %d, nUpdateType: 0x%x, nRet: 0x%x\r\n"), 
                                               pstDev->nDevNo, nDieIdx, BML_TYPE_UPDATE_ERL, nBBMRe));
                /* move to next block */
                continue;
            }
        }

        /* original lock state of the block is locked */
        if (nLockStat == FSR_LLD_BLK_STAT_LOCKED)
        {
            /* Set the parameter of LLD_IOCtl() */
            stLLDPrtArg.nStartBlk = nOrgPbn;
            stLLDPrtArg.nBlks     = 1;

            nLLDRe = pstVol->LLD_IOCtl(pstDev->nDevNo,
                                       FSR_LLD_IOCTL_LOCK_BLOCK,
                                       (UINT8 *) &stLLDPrtArg,
                                       sizeof(stLLDPrtArg),
                                       (UINT8 *) &nErrPbn,
                                       sizeof(nErrPbn),
                                       &nByteRet);

            /* If unlock operation is failed, replaced orginal pbn */
            if (nLLDRe != FSR_LLD_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BBM:ERR]  Locking original block is failed\r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BBM:ERR]  nPDev: %d, nOrgPbn: %d\r\n"), pstDev->nDevNo, nOrgPbn));
                nBBMRe = FSR_BML_CANT_LOCK_BLOCK;
                
                /* move to next block */
                continue;
            }

            /* If error occurs during lock operation, it will be ignored */
        }

    } /* end of main for loop */

    /* REF block is refreshed even if nBBMRe is not FSR_BML_SUCCESS */
    if (nProcessedCnt > 0)
    {
        /*
         * Step7: Erase REF block before using for back-up previous data
         */
        nREFPbn = pstRsvSh->nREFSbn;

        nLLDRe = _LLDErase(pstVol, pstDev->nDevNo, &nREFPbn, FSR_LLD_FLAG_1X_ERASE);

        while (nLLDRe != FSR_LLD_SUCCESS)
        {
            nRet = FSR_BBM_HandleBadBlk(pstVol, 
                                        pstDev, 
                                        pstRsvSh->nREFSbn, 
                                        0, 
                                        1,
                                        BML_HANDLE_ERASE_ERROR);
            if (nRet == FSR_BML_NO_RSV_BLK_POOL)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Bad block handling is caused by no reservoir pool \r\n")));                    
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] FSR_BBM_HandleBadBlk(nPDev: %d, nOrgSbn: %d, nFlag: 0x%x) is failed\r\n"),
                                               pstDev->nDevNo, pstRsvSh->nREFSbn, BML_HANDLE_WRITE_ERROR));
                nBBMRe = nRet;
                break;
            }
            else if (nRet != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Bad block replacement for REF block is failed(at step6)\r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] nPDev: %d, nREFPbn: %d, nUpdateType: 0x%x\r\n"),
                                                 pstDev->nDevNo, pstRsvSh->nREFSbn, BML_HANDLE_ERASE_ERROR));
                nBBMRe = FSR_BML_ERASE_REFRESH_FAILURE;
                break;
            }   

            nREFPbn = _TransSbn2Pbn(pstRsvSh->nREFSbn, pstRsv);
            pstRsvSh->nREFSbn = (UINT16) nREFPbn;

            nLLDRe = _LLDErase(pstVol, pstDev->nDevNo, &nREFPbn, FSR_LLD_FLAG_1X_ERASE);
        }

        pstRsvSh->nNextREFPgOff = 0;
    }

    /* 
     * if BML_FLAG_NOTICE_READ_ERROR is set and 
     * uncorrectable read error occurs during copy operation,
     * BBM always returns FSR_BML_READ_ERROR.
     * In that case, other errors should be ignored.
     *
     */
    if (bFixedBBMRe == TRUE32)
    {
        nBBMRe = FSR_BML_READ_ERROR;
    }

    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:INF]   The registered blocks in ERL are handled\r\n")));
    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:INF]   (number of blocks: %d)\r\n"), nProcessedCnt));
    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nBBMRe));

    /* 
     * 1) Return value of this function is ignored at open time
     * 2) When this function is called at run time, 
     *    only FSR_BML_READ_ERROR is valid and other errors are also ignored.
     * 
     */

    return nBBMRe;
}

/**
 * @brief          This function checks status of previous refresh operation 
 * @n              and continues refresh operation 
 *
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]      *pstDev      : device context pointer
 * @param[in]       nDieIdx     : die index number
 *
 * @return          FSR_BML_SUCCESS
 * @return          FSR_BML_NO_RSV_BLK_POOL
 * @return          FSR_BML_CANT_LOCK_BLOCK
 * @return          FSR_BML_CRITICAL_ERROR 
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PUBLIC INT32 
FSR_BBM_ChkRefreshBlk(BmlVolCxt    *pstVol,
                      BmlDevCxt    *pstDev,
                      UINT32        nDieIdx)
{
    BmlReservoir       *pstRsv;
    BmlReservoirSh     *pstRsvSh;
    BmlERL             *pstERL;
    BmlBMI             *pstBMI;
    LLDProtectionArg    stLLDPrtArg;
    INT32               nBBMRe = FSR_BML_SUCCESS;
    INT32               nLLDRe = FSR_LLD_SUCCESS;
    INT32               nRet   = FSR_LLD_SUCCESS; 
    UINT32              nREFSbn;
    UINT32              nREFPbn;
    UINT32              nIdx;
    UINT32              nOrgPbn = BML_INVALID;
    UINT32              nOrgSbn;
    UINT32              nLockStat;
    UINT32              nByteRet;
    UINT32              nErrPbn;
    BOOL32              bValidREF   = FALSE32;
    BOOL32              bRDFlag     = FALSE32;
    BOOL32              bUCReadErr  = FALSE32;

    UINT32              nFlag = FSR_BML_FLAG_NONE;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(PDev: %d, nDieIdx: %d)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nDieIdx));

    do
    {
        pstRsv  = pstDev->pstDie[nDieIdx]->pstRsv;
        pstRsvSh    = pstDev->pstDie[nDieIdx]->pstRsvSh;

        pstERL  = pstRsv->pstERL;
        pstBMI  = pstRsv->pstBMI;
        nREFSbn = pstRsvSh->nREFSbn;

        /* look up all ERL list to check current status */
        for (nIdx = 0; nIdx < BML_MAX_ERL_ITEM; nIdx++)
        {
            /* original data is already copied to REF block */
            if ((pstERL->astERBlkInfo[nIdx].nProgInfo == BML_PROGINFO_COPIED) &&
                (pstERL->astERBlkInfo[nIdx].nSbn != 0xFFFF))
            {
                nOrgPbn   = pstERL->astERBlkInfo[nIdx].nSbn;
                bValidREF = TRUE32;
                break;
            }
        }

        /* REF block has valid data */
        if (bValidREF == TRUE32)
        {        
            /* get Sbn */
            nOrgSbn = _TransPbn2Sbn(pstRsv, nOrgPbn, pstBMI);

            /* get pbn (it is needed when block is replaced during refresh operation) */
            nOrgPbn = _TransSbn2Pbn(nOrgSbn, pstRsv);

            /* get lock state of orginal block */
            nLLDRe = pstVol->LLD_IOCtl(pstDev->nDevNo,
                                       FSR_LLD_IOCTL_GET_LOCK_STAT,
                                       (UINT8 *) &nOrgSbn,
                                       sizeof(nOrgSbn),
                                       (UINT8*) &nLockStat,
                                       sizeof(nLockStat),
                                       &nByteRet);
            if (nLLDRe != FSR_LLD_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BBM:ERR] Can not get lock status of original block\r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BBM:ERR] nPDev: %d, nOrgSbn: %d, nLLDRe: 0x%x\r\n"), pstDev->nDevNo, nOrgSbn, nLLDRe));
                nBBMRe = FSR_BML_CRITICAL_ERROR;
                break;
            }

            /* 
             * if attribute of the block is lock tighten,
             * the block can not be refreshed by erase operation
             * and its progress info can not be PROGINFO_COPIED
             */
            FSR_ASSERT(nLockStat != FSR_LLD_BLK_STAT_LOCKED_TIGHT);
            
            /* if the block is locked */
            if (nLockStat == FSR_LLD_BLK_STAT_LOCKED)
            {
                /* Set the parameter of LLD_IOCtl() */
                stLLDPrtArg.nStartBlk = nOrgPbn;
                stLLDPrtArg.nBlks     = 1;

                nLLDRe = pstVol->LLD_IOCtl(pstDev->nDevNo,
                                           FSR_LLD_IOCTL_UNLOCK_BLOCK,
                                           (UINT8 *) &stLLDPrtArg,
                                           sizeof(stLLDPrtArg),
                                           (UINT8 *) &nErrPbn,
                                           sizeof(nErrPbn),
                                           &nByteRet);

                /* If unlock operation is failed, replaced orginal pbn */
                if (nLLDRe != FSR_LLD_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BBM:ERR]  Unlocking some area is failed\r\n")));    
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BBM:ERR] nPDev: %d, nOrgPbn: %d, nLLDRe: 0x%x\r\n"), pstDev->nDevNo, nOrgPbn, nLLDRe));
                    nBBMRe = FSR_BML_CANT_UNLOCK_BLOCK;
                    break;
                }
            }

            /* copy data from REF block to original data */
            
            /* erase original block */
            nRet = _LLDErase(pstVol, pstDev->nDevNo, &nOrgPbn, FSR_LLD_FLAG_1X_ERASE);

            while (nRet != FSR_LLD_SUCCESS)
            {
                nRet = FSR_BBM_HandleBadBlk(pstVol, 
                                            pstDev, 
                                            nOrgSbn, 
                                            0,
                                            1,
                                            BML_HANDLE_ERASE_ERROR);
                if (nRet == FSR_BML_NO_RSV_BLK_POOL)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Bad block handling is caused by no reservoir pool \r\n")));                    
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] FSR_BBM_HandleBadBlk(nPDev: %d, nOrgSbn: %d, nFlag: 0x%x) is failed\r\n"),
                                                   pstDev->nDevNo, nOrgSbn, BML_HANDLE_ERASE_ERROR));
                    nBBMRe = nRet;
                    break;
                }
                else if (nRet != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Bad block handling is caused by failure of erasing the original block\r\n")));                    
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] FSR_BBM_HandleBadBlk(nPDev: %d, nOrgSbn: %d, nFlag: 0x%x) is failed\r\n"),
                                                   pstDev->nDevNo, nOrgSbn, BML_HANDLE_ERASE_ERROR));
                    nBBMRe = FSR_BML_CRITICAL_ERROR;
                    break;
                }

                nOrgPbn = _TransSbn2Pbn(nOrgSbn, pstRsv);
                nRet = _LLDErase(pstVol, pstDev->nDevNo, &nOrgPbn, FSR_LLD_FLAG_1X_ERASE);
            }

            if (nBBMRe != FSR_BML_SUCCESS)
            {
                break;
            }

            do
            {
                /* 
                 * copy REF block to original data 
                 * bRDFlag and bUCReadErr are not used in this function 
                 */        
                nRet = _CopyBlkRefresh(pstVol, pstDev, nDieIdx, pstRsvSh->nREFSbn, nOrgPbn, &bRDFlag, &bUCReadErr, nFlag); 

                if (nRet != FSR_LLD_SUCCESS)
                {
                    /* write error case */
                    if (nRet == FSR_LLD_PREV_WRITE_ERROR)
                    {
                        /* 
                         * repeat the bad block handling 
                         * until replacement is successfully completed 
                         */
                        do
                        {
                            nRet = FSR_BBM_HandleBadBlk(pstVol, 
                                                        pstDev, 
                                                        nOrgSbn, 
                                                        0,
                                                        1,
                                                        BML_HANDLE_WRITE_ERROR);
                            if (nRet == FSR_BML_NO_RSV_BLK_POOL)
                            {
                                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Bad block handling is caused by no reservoir pool \r\n")));                    
                                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] FSR_BBM_HandleBadBlk(nPDev: %d, nOrgSbn: %d, nFlag: 0x%x) is failed\r\n"),
                                                               pstDev->nDevNo, nOrgSbn, BML_HANDLE_ERASE_ERROR));
                                nBBMRe = nRet;
                                break;
                            }
                            else if (nRet != FSR_BML_SUCCESS)
                            {
                                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Bad block handling is caused by failure of coping to original block\r\n")));                                        
                                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] FSR_BBM_HandleBadBlk(nPDev: %d, nOrgSbn: %d, nFlag: 0x%x) is failed\r\n"),
                                                                pstDev->nDevNo, nOrgSbn, BML_HANDLE_WRITE_ERROR));                                
                                nBBMRe = FSR_BML_CRITICAL_ERROR;
                                break;
                            }

                            nOrgPbn = _TransSbn2Pbn(nOrgSbn, pstRsv);

                            nLLDRe = _LLDErase(pstVol, pstDev->nDevNo, &nOrgPbn, FSR_LLD_FLAG_1X_ERASE);

                        } while (nLLDRe != FSR_LLD_SUCCESS);

                        if (nBBMRe != FSR_BML_SUCCESS)
                        {
                            break;
                        }
                        else
                        {
                            /* restart copy operation */
                            continue;
                        }
                    }
                    /* other  error case */
                    else
                    {
                        nBBMRe = FSR_BML_CRITICAL_ERROR;
                        break;
                    }
                }

            } while (nRet != FSR_LLD_SUCCESS);

            if (nBBMRe != FSR_BML_SUCCESS)
            {
                break;
            }

            /* 
             * remove this block from the ERL list after copy operation is completed
             */
            pstERL->astERBlkInfo[nIdx].nSbn      = 0xFFFF;
            pstERL->astERBlkInfo[nIdx].nProgInfo = 0xFFFF;
            pstERL->nCnt--;            

            /* Update UPCB to write ERL list */
            nBBMRe = _UpdateUPCB(pstDev, pstVol, pstVol->pPIExt, nDieIdx, BML_TYPE_UPDATE_ERL);
            if (nBBMRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _UpdateUPCB is failed during erase refresh operation\r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] nPDev: %d, nDieIdx: %d, nUpdateType: 0x%x, nRet: 0x%x\r\n"), 
                                               pstDev->nDevNo, nDieIdx, BML_TYPE_UPDATE_ERL, nBBMRe));
                break;
            }

            /* get pbn */
            nREFPbn = _TransSbn2Pbn(nREFSbn, pstRsv);

            nLLDRe = _LLDErase(pstVol, pstDev->nDevNo, &nREFPbn, FSR_LLD_FLAG_1X_ERASE);

            while (nLLDRe != FSR_LLD_SUCCESS)
            {
                nRet = FSR_BBM_HandleBadBlk(pstVol, 
                                            pstDev, 
                                            pstRsvSh->nREFSbn, 
                                            0,
                                            1,
                                            BML_HANDLE_ERASE_ERROR);
                if (nRet != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Bad block handling is caused by failure of Erasing REF block\r\n")));                                        
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Bad block replacement(nPDev: %d, nREFSbn: %d, nFlag: 0x%x) for REF block is failed\r\n"),
                                                   pstDev->nDevNo, pstRsvSh->nREFSbn, BML_HANDLE_ERASE_ERROR));
                    nBBMRe = FSR_BML_CRITICAL_ERROR;
                    break;
                }

                nREFPbn = _TransSbn2Pbn((UINT16) pstRsvSh->nREFSbn, pstRsv);
                pstRsvSh->nREFSbn = (UINT16) nREFPbn;

                nLLDRe = _LLDErase(pstVol, pstDev->nDevNo, &nREFPbn, FSR_LLD_FLAG_1X_ERASE);
            }

            pstRsvSh->nNextREFPgOff = 0;

            /* original lock state of the block is locked */
            if (nLockStat == FSR_LLD_BLK_STAT_LOCKED)
            {
                /* Set the parameter of LLD_IOCtl() */
                stLLDPrtArg.nStartBlk = nOrgPbn;
                stLLDPrtArg.nBlks     = 1;

                nLLDRe = pstVol->LLD_IOCtl(pstDev->nDevNo,
                                           FSR_LLD_IOCTL_LOCK_BLOCK,
                                           (UINT8 *) &stLLDPrtArg,
                                           sizeof(stLLDPrtArg),
                                           (UINT8 *) &nErrPbn,
                                           sizeof(nErrPbn),
                                           &nByteRet);

                /* If unlock operation is failed, replaced orginal pbn */
                if (nLLDRe != FSR_LLD_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BBM:ERR]  Locking original block is failed\r\n")));
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BBM:ERR]  nPDev: %d, nOrgPbn: %d\r\n"), pstDev->nDevNo, nOrgPbn));
                    nBBMRe = FSR_BML_CANT_LOCK_BLOCK;
                    break;
                }
            }
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nBBMRe));

    return nBBMRe;
}

/**
 * @brief          this function locks PI by IOCtl
 *
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]       nPDev       : physical device number
 * @param[in]       nDieIdx     : die index
 *
 * @return          FSR_LLD_SUCCESS  
 * @return          Some LLD errors
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PUBLIC INT32
FSR_BBM_FixSLCBoundary(BmlVolCxt   *pstVol,
                       UINT32       nPDev,
                       UINT32       nDieIdx)
{
    LLDPIArg    stLLDPIArg;
    INT32       nRet = FSR_BML_SUCCESS;
    UINT32      nLastSLCPbnInDie;
    UINT32      nByteRet;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(PDev: %d, nDieIdx: %d)\r\n"),
                                     __FSR_FUNC__, nPDev, nDieIdx));

    /* this function can be used only in Flex-OneNAND */

    nRet = pstVol->LLD_IOCtl(nPDev,
                             FSR_LLD_IOCTL_PI_READ,
                             (UINT8 *)&nDieIdx,
                             sizeof(nDieIdx),
                             (UINT8 *)&(stLLDPIArg),
                             sizeof(stLLDPIArg),
                             &nByteRet);
    
    if (nRet != FSR_LLD_SUCCESS)
    {
       FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] LLD_IOCtl(nCode:FSR_LLD_IOCTL_PI_READ) fails!(Dev:%d, Die:%d)\r\n"), 
                                       nPDev, nDieIdx));
       FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nRet));
       return nRet;
    }

    nLastSLCPbnInDie = stLLDPIArg.nEndOfSLC;

    /* set the parameter of LLD_IOCtl() */
    stLLDPIArg.nEndOfSLC    = (UINT16) nLastSLCPbnInDie;
    stLLDPIArg.nPILockValue = FSR_LLD_IOCTL_LOCK_PI;

    /* write PI data */
    nRet = pstVol->LLD_IOCtl(nPDev, 
                             FSR_LLD_IOCTL_PI_WRITE,
                             (UINT8 *)&stLLDPIArg,
                             sizeof(stLLDPIArg),
                             NULL,
                             0,
                             &nByteRet);

    if (nRet != FSR_LLD_SUCCESS)
    {   
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] LLD_IOCtl(nCode:FSR_LLD_IOCTL_PI_WRITE) fails!(Dev:%d, Die:%d, last SLc: %d, PI lock value: 0x%x)\r\n"), 
                                       nPDev, nDieIdx, nLastSLCPbnInDie, FSR_LLD_IOCTL_LOCK_PI));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nRet));
        return nRet;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nRet));
    return nRet;
}

/**
 * @brief          this function do lock tight operation for Reservoir
 *
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]      *pstDev      : device context pointer
 * @param[in]       nDieIdx     : die index
 *
 * @return          FSR_BML_SUCCESS  
 * @return          FSR_BML_CANT_LOCK_FOREVER  
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PUBLIC INT32
FSR_BBM_LockTight(BmlVolCxt     *pstVol,
                  BmlDevCxt     *pstDev,
                  UINT32         nDieIdx)
{
    LLDProtectionArg    stLLDPrtArg;
    BmlReservoirSh     *pstRsvSh;
    UINT32              nPbn;
    UINT32              nByteReturned;
    UINT32              nLockState;
    UINT32              nErrPbn;
    INT32               nLLDRe;
    
    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(PDev: %d, nDieIdx: %d)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nDieIdx));

    /* get Reservoir pointer of thie die */
    pstRsvSh    = pstDev->pstDie[nDieIdx]->pstRsvSh;

    /* change lock state of PCB */

    /* Set the parameter of LLD_IOCtl() */
    stLLDPrtArg.nStartBlk = pstRsvSh->nLPCBSbn;
    stLLDPrtArg.nBlks = 1;

    nPbn = pstRsvSh->nLPCBSbn;

    /* Check the state of the current block */
    nLLDRe = pstVol->LLD_IOCtl(pstDev->nDevNo,
                               FSR_LLD_IOCTL_GET_LOCK_STAT,
                               (UINT8 *) &nPbn,
                               sizeof(nPbn),
                               (UINT8 *) &nLockState,
                               sizeof(nLockState),
                               &nByteReturned);
    if (nLLDRe != FSR_LLD_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Can't get lock state(nPDev: %d, LPCB: %d)\r\n"),
                                       pstDev->nDevNo, nPbn));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FSR_BML_CANT_LOCK_FOREVER));
        return FSR_BML_CANT_LOCK_FOREVER;
    }

    if (nLockState == FSR_LLD_BLK_STAT_UNLOCKED)
    {
        nLLDRe = pstVol->LLD_IOCtl(pstDev->nDevNo,
                                   FSR_LLD_IOCTL_LOCK_BLOCK,
                                   (UINT8 *) &stLLDPrtArg,
                                   sizeof(stLLDPrtArg),
                                   (UINT8 *) &nErrPbn,
                                   sizeof(nErrPbn),
                                   &nByteReturned);
        if (nLLDRe != FSR_LLD_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Can't lock Reservoir blocks(nPDev: %d, LPCB: %d)\r\n"), 
                                           pstDev->nDevNo, stLLDPrtArg.nStartBlk));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FSR_BML_CANT_LOCK_FOREVER));
            return FSR_BML_CANT_LOCK_FOREVER;
        }
    }

    nLLDRe = pstVol->LLD_IOCtl(pstDev->nDevNo,
                               FSR_LLD_IOCTL_LOCK_TIGHT,
                               (UINT8 *) &stLLDPrtArg,
                               sizeof(stLLDPrtArg),
                               (UINT8 *) &nErrPbn,
                               sizeof(nErrPbn),
                               &nByteReturned);
    if (nLLDRe != FSR_LLD_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Can't lock Reservoir blocks(lock-tight, nPDev: %d, LPCB: %d)\r\n"),
                                       pstDev->nDevNo, stLLDPrtArg.nStartBlk));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FSR_BML_CANT_LOCK_FOREVER));
        return FSR_BML_CANT_LOCK_FOREVER;
    }         

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FSR_BML_SUCCESS));

    return FSR_BML_SUCCESS;
}


/**
 * @brief           This function makes the allocation status of the 
 * @n               given nPbn to the allocated status
 *
 * @param[in]      *pstRsv      : Reservoir structure pointer
 * @param[in]       nPbn        : physical block number
 *
 * @return          none
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE VOID
_SetAllocRB(BmlReservoir *pstRsv, 
            UINT32 nPbn)
{
    UINT32  nRBIdx;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(nPbn: %d)\r\n"),
                                     __FSR_FUNC__, nPbn));

    /* get reserved block index */
    nRBIdx = nPbn - pstRsv->n1stSbnOfRsvr;

    /* set the bit of Block Allocation BitMap as 1 */
    pstRsv->pBABitMap[nRBIdx / 8] |= (UINT8) (0x80 >> (nRBIdx % 8));

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s()\r\n"), __FSR_FUNC__));
}

/**
 * @brief           This function reset the allocation status of the 
 * @n               given nPbn
 *
 * @param[in]      *pstRsv      : Reservoir structure pointer
 * @param[in]       nPbn        : physical block number
 *
 * @return          none
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE VOID
_FreeRB(BmlReservoir *pstRsv, 
        UINT32 nPbn)
{
    UINT32  nRBIdx;
    UINT8   nMask;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(nPbn: %d)\r\n"),
                                     __FSR_FUNC__, nPbn));

    /* get reserved block index */
    nRBIdx = nPbn - pstRsv->n1stSbnOfRsvr;

    /* calculate mask value to reset a bit */
    nMask = (UINT8) ~(0x01 << (7 - (nRBIdx % 8)));

    /* reset the specific bit for Block Allocation BitMap */
    pstRsv->pBABitMap[nRBIdx / 8] &= nMask;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s()\r\n"), __FSR_FUNC__));
}

/*
 * @brief           This function gets the free reserved block from Reserved Block Pool
 *
 * @param[in]       pstRsv       : pointer of Reservoir structure
 * @param[in]       nNumOfPlane  : number of plane in device
 * @param[in]       nBlkType     : type of block (SLC or MLC)
 * @param[in]       nDataType    : type of data  (BBM meta or user data)
 * @param[in]       nNumOfRepBlks: number of blocks to be replaced (1 - number of plane)
 * @param[out]     *pFreePbn     : variable pointer that free physical block number is store
 * @param[out]     *pbPaired     : paired replacement occurs or not
 *
 * @return          TRUE32
 * @return          FALSE32
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE BOOL32
_GetFreeRB(BmlReservoir    *pstRsv,
           UINT32           nNumOfPlane,
           UINT32           nBlkType,
           UINT32           nDataType,
           UINT32           nNumOfRepBlks,
           UINT32          *pFreePbn,
           BOOL32          *pbPaired)
{
    UINT32      nBlkIdx;
    UINT32      nStartBlkIdx;
    UINT32      nEndBlkIdx;
    UINT32      nPlnStartBlkIdx;
    UINT32      nPlnEndBlkIdx;
    UINT32      nPlnIdx;
    UINT32      nCnt;
    UINT32      nSizeOfRCB;
    UINT32      nRCBIdx;
    BOOL32      bFound   = FALSE32;
    BOOL32      bRetPln0 = TRUE32;
    BOOL32      bRetPln1 = TRUE32;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(nNumOfPlane: %d, nBlkType: %d, nDataType: %d)\r\n"),
                                     __FSR_FUNC__, nNumOfPlane, nBlkType, nDataType));

    do
    {
        /* number of blocks to be replaced should be larger than 0 */
        FSR_ASSERT((nNumOfRepBlks != 0));

        /* Reservoir for hybrid cell type(SLC+MLC) Flex-OneNAND */
        if (pstRsv->nRsvrType == BML_HYBRID_RESERVOIR)
        {
            /* SLC block */
            if (nBlkType == BML_SLC_BLK)
            {
                nStartBlkIdx = pstRsv->n1stSbnOfRsvr;
                nEndBlkIdx   = pstRsv->n1stSbnOfMLC - 1;
            }
            /* MLC block */
            else
            {
                nStartBlkIdx = pstRsv->n1stSbnOfMLC;
                nEndBlkIdx   = pstRsv->nLastSbnOfRsvr;
            }
        }
        /* other types */
        else
        {
            nStartBlkIdx = pstRsv->n1stSbnOfRsvr;
            nEndBlkIdx   = pstRsv->nLastSbnOfRsvr;
        }

        /* 
         *  < bad block replacement for 2 plane device >
         *
         *    SLC Reservoir  MLC Reservoir
         *          |<--->|<--------------->|        
         * +-----------------------------------------+
         * |p0|p1|p0|p1|p0|p1|p0|p1|p0|p1|p0|p1|p0|p1|
         * |  |  |  |  |  |  |  |  |**|**|  |  |B |X |
         * +-----------------------------------------+
         *                          ^  replace  | 
         *                          +-----------+
         *                   |<--------->|
         *     These blocks can be used the bad block replacement for 2 plane     
         *
         */

        /* 
         * Allocation direction of hybrid type reservoir
         * +-----------------------------------------+
         * |  SLC Reservoir     |   MLC Reservoir    |
         * +-----------------------------------------+
         * --------->                      <----------
         * ascending order            descending order
         * Note) Blocks in the side of reservoir should be
         * allocated first becuase PI value is not written yet at format time
         *
         * Allocation direction of plane type reservoir
         * +-----------------------------------------+
         * |  SLC only or MLC only Reservoir         |
         * +-----------------------------------------+
         *                <---------------------------
         *                            descending order
         */

        /*
         * Multi-plane case
         * step1) 2X replacement (paired replacement) 
         * step2) if step1 is impossible, try to replace one block using free slot of Reservoir
         * step3) if step2 is impossible, try to replace one block using free slot of user area
         */

        /* [step1] paired replacement */
        if ((nNumOfPlane > 1) && (nDataType != BML_META_DATA))
        {
            /* first block index which is aligned with plane */
            if ((nStartBlkIdx + 1) % nNumOfPlane)
            {
                nPlnStartBlkIdx =  nStartBlkIdx;
            }
            else
            {
                nPlnStartBlkIdx = nStartBlkIdx + 1;
            }

            /* last block index which is aligned with plane */
            if ((nEndBlkIdx + 1) % nNumOfPlane)
            {
                nPlnEndBlkIdx = nEndBlkIdx - 1;
            }
            else
            {
                nPlnEndBlkIdx = nEndBlkIdx;
            }

            /* SLC area of hybird Reservoir allocates free blocks by ascending order */
            if ((pstRsv->nRsvrType == BML_HYBRID_RESERVOIR) && (nBlkType == BML_SLC_BLK))
            {
                /* find free blocks to replace */
                for (nBlkIdx = nPlnStartBlkIdx; nBlkIdx <= nPlnEndBlkIdx; nBlkIdx += nNumOfPlane)
                {   
                    /* if the block or adjacent block is already allocated, skip */
                    bRetPln0 = _IsAllocRB(pstRsv, nBlkIdx);
                    bRetPln1 = _IsAllocRB(pstRsv, nBlkIdx + 1);
                    
                    if (bRetPln0 || bRetPln1)
                    {
                        continue;        
                    }

                    nPlnIdx = nNumOfPlane;

                    do
                    {
                        nPlnIdx--;
                        
                        /* 
                         * plane 0: nBlkIdx , plane 1: nBlkIdx + 1
                         * *pFreePbn[nPlnIdx] = (nBlkIdx - (nPlnIdx + 1 % nNumOfPlane));
                         */
                        pFreePbn[nPlnIdx] = (nBlkIdx + nPlnIdx);
                        
                        /* allocate a free block */    
                        _SetAllocRB(pstRsv, (nBlkIdx + nPlnIdx));
                    } while (nPlnIdx > 0);

                    *pbPaired = TRUE32;
                    bFound = TRUE32;
                    break;
                }
            }
            else
            {
                /* find free blocks to replace */
                for (nBlkIdx = nPlnEndBlkIdx; nBlkIdx >= nPlnStartBlkIdx; nBlkIdx -= nNumOfPlane)
                {   
                    bRetPln1 = _IsAllocRB(pstRsv, nBlkIdx);
                    bRetPln0 = _IsAllocRB(pstRsv, nBlkIdx - 1);

                    /* if the block or adjacent block is already allocated, skip */
                    if (bRetPln0 || bRetPln1)
                    {
                        continue;
                    }

                    nPlnIdx = nNumOfPlane;

                    do
                    {
                        nPlnIdx--;

                        /* 
                         * plane 1: nBlkIdx , plane 0: nBlkIdx - 1
                         * *pFreePbn[nPlnIdx] = (nBlkIdx - (nPlnIdx + 1 % nNumOfPlane));
                         */
                        pFreePbn[nPlnIdx] = (nBlkIdx - (~nPlnIdx & 0x1));
                        
                        /* allocate a free block */    
                        _SetAllocRB(pstRsv, nBlkIdx - (~nPlnIdx & 0x1));
                    } while (nPlnIdx > 0);

                    *pbPaired = TRUE32;
                    bFound = TRUE32;
                    break;
                }
            }

            if (bFound == TRUE32)   
            {
                break;
            }
        }

        nCnt = 0;

        /* Reservoir allocation by below methods is not the paired replacement */
        *pbPaired = FALSE32;

        /* 
         * [step2] if step1 is impossible, try to replace one block using free slot of Reservoir
         *
         * Scan the Reservoir to find a free block 
         * case1) 1 plane device
         * case2) Paired replacement for 2 plane is impossible (out of space)
         * case3) BBM meta blocks or locked lock
         *
         */

        /* SLC area of hybird Reservoir allocates free blocks by ascending order */
        if ((pstRsv->nRsvrType == BML_HYBRID_RESERVOIR) && (nBlkType == BML_SLC_BLK))
        {
            /* scan by bottom-up fashion */
            for (nBlkIdx = nStartBlkIdx; nBlkIdx <= nEndBlkIdx; nBlkIdx++)
            {
                bRetPln0 = _IsAllocRB(pstRsv, nBlkIdx);
                if (bRetPln0 == TRUE32)
                {
                    continue;
                }

                pFreePbn[nCnt++] = nBlkIdx;

                /* allocate a free block */
                _SetAllocRB(pstRsv, nBlkIdx);

                nNumOfRepBlks--;

                /* 1X replacement is continued until all bad blocks are treated */
                if (nNumOfRepBlks == 0)
                {
                    bFound = TRUE32;
                    break;
                }
            }
        }
        /* other Reservoirs allocate free blocks be descending order */
        else
        {
            /* scan by top-down fashion */
            for (nBlkIdx = nEndBlkIdx; nBlkIdx >= nStartBlkIdx; nBlkIdx--)
            {
                bRetPln0 = _IsAllocRB(pstRsv, nBlkIdx);
                if (bRetPln0 == TRUE32)
                {
                    continue;
                }

                pFreePbn[nCnt++] = nBlkIdx;

                /* allocate a free block */
                _SetAllocRB(pstRsv, nBlkIdx);

                nNumOfRepBlks--;
                
                /* 1X replacement is continued until all bad blocks are treated */
                if (nNumOfRepBlks == 0)
                {
                    bFound = TRUE32;
                    break;
                }
            }
        }

        if (bFound == TRUE32)
        {   
            break;
        }

        nCnt = 0;

        /* 
         * [step3] if step2 is impossible, try to replace one block using free slot of user area
         * (use reservoir candidate blocks)
         */       

        /* 
         * meta block can not be replaced to user area and
         * the device has multiple planes to support reservoir candidate scheme 
         */
        if ((nDataType != BML_META_DATA) && (nNumOfPlane > 1))
        {
            nSizeOfRCB = (pstRsv->nNumOfRsvrInSLC + pstRsv->nNumOfRsvrInMLC) / 2;

            /* unassigned area of RCB should be scanned to find reservoir candidate block */
            for (nRCBIdx = 0; nRCBIdx < nSizeOfRCB; nRCBIdx++)
            {

                /* Find free reservoir candidate block info */
                if (pstRsv->pstRCB[nRCBIdx] != 0xFFFF)
                {
                    /* 
                     * If cell type of the free reservoir candidate is equal to 
                     * cell type of the replaced block
                     */                    
                    if (((nBlkType == BML_SLC_BLK) && (pstRsv->pstRCB[nRCBIdx] < pstRsv->n1stSbnOfMLC)) ||
                        ((nBlkType == BML_MLC_BLK) && (pstRsv->pstRCB[nRCBIdx] >= pstRsv->n1stSbnOfMLC)))
                    {
                        pFreePbn[nCnt++] = (UINT32) pstRsv->pstRCB[nRCBIdx];

                        nNumOfRepBlks--;

                        /* 1X replacement is continued until all bad blocks are treated */
                        if (nNumOfRepBlks == 0)
                        {
                            bFound = TRUE32;
                            break;
                        }
                    }
                } /* end of if */
            }  /* end of for */

            /* can not find free reservoir candidate */
            if (bFound != TRUE32)
            {
                break;
            }
        }
        /* free block can not be allocated */
        else
        {  
            break; 
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, bFound));

    return bFound;
}

/*
 * @brief           This function gets the free reserved block for REF block
 *
 * @param[in]       pstRsv       : pointer of Reservoir structure
 * @param[out]     *pFreePbn     : variable pointer that free physical block number is store
 *
 * @return          TRUE32  
 * @return          FALSE32  
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE BOOL32
_GetNewREFBlk(BmlDieCxt    *pstDie,
              UINT16       *pFreePbn)
{
    BmlReservoir   *pstRsv;
    BmlReservoirSh *pstRsvSh;
    UINT32          nBlkIdx;
    UINT32          nStartBlkIdx;
    UINT32          nEndBlkIdx;
    BOOL32          bFound = FALSE32;
    BOOL32          bRet   = TRUE32;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s()\r\n"), __FSR_FUNC__));

    do
    {
        pstRsv      = pstDie->pstRsv;
        pstRsvSh    = pstDie->pstRsvSh;

        nEndBlkIdx = pstRsvSh->nREFSbn;

        /* Reservoir for hybrid cell type (SLC+MLC) Flex-OneNAND */
        if (pstRsv->nRsvrType == BML_HYBRID_RESERVOIR)
        {
            nStartBlkIdx = pstRsv->n1stSbnOfMLC;
        }
        /* SLC only or MLC only */
        else
        {
            nStartBlkIdx = pstRsv->n1stSbnOfRsvr;
        }

        /* scan from REF block to first Reservoir block */
        for (nBlkIdx = nEndBlkIdx; nBlkIdx >= nStartBlkIdx; nBlkIdx--)
        {
            bRet = _IsAllocRB(pstRsv, nBlkIdx);
            if (bRet == TRUE32)
            {
                continue;
            }

            *pFreePbn = (UINT16) nBlkIdx;
            bFound    = TRUE32;

            /* allocate a free block */
            _SetAllocRB(pstRsv, nBlkIdx);

            break;
        }

        /* If candidate is not found ahead of REF block */
        if (bFound == FALSE32)
        {
            nEndBlkIdx   = pstRsv->nLastSbnOfRsvr;
            nStartBlkIdx = pstRsvSh->nREFSbn;

            /* scan from last Reservoir block to REF block */
            for (nBlkIdx = nEndBlkIdx; nBlkIdx >= nStartBlkIdx; nBlkIdx--)
            {
                bRet = _IsAllocRB(pstRsv, nBlkIdx);
                if (bRet == TRUE32)
                {
                    continue;
                }

                *pFreePbn = (UINT16) nBlkIdx;
                bFound    = TRUE32;

                /* allocate a free block */
                _SetAllocRB(pstRsv, nBlkIdx);

                break;
            }
        }

    }
    while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, bFound));

    return bFound;
}

#if !defined(FSR_NBL2)
/*
 * @brief           This function makes new PCB
 *                  Internally, this function writes LPCB and UPCB
 *
 * @param[in]      *pstDev      : device context pointer
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]      *pstPart     : partition information pointer
 * @param[in]       bInitFormat : flag for initial format
 * @param[in]       nDieIdx     : index of die
 *
 * @return          FSR_BML_SUCCESS  
 * @return          FSR_BML_NO_RSV_BLK_POOL  
 * @return          FSR_BML_MAKE_NEW_PCB_FAILURE
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE INT32
_MakeNewPCB(BmlDevCxt  *pstDev, 
            BmlVolCxt  *pstVol, 
            UINT32      nDieIdx, 
            FSRPartI   *pstPart, 
            BOOL32      bInitFormat)
{
    INT32           nRet   = FSR_BML_SUCCESS;
    BmlReservoirSh *pstRsvSh;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(nPDev, %d, nDieIdx: %d, bInitFormat: %d)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nDieIdx, bInitFormat));

    pstRsvSh    = pstDev->pstDie[nDieIdx]->pstRsvSh;

    do
    {
        /* page offset in LPCB */
        pstRsvSh->nNextLPCBPgOff = 0;

        nRet = _UpdateLPCB(pstDev, pstVol, pstPart, nDieIdx, BML_TYPE_INIT_CREATE);
        if (nRet != FSR_BML_SUCCESS)
        {
            nRet = FSR_BML_MAKE_NEW_PCB_FAILURE;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] update of LPCB is failed(nPDev: %d, nDieIdx: %d, LPCB: %d, BML_TYPE_INIT_CREATE)\r\n"), 
                                           pstDev->nDevNo, nDieIdx, pstRsvSh->nLPCBSbn));
            break;
        }

        if(bInitFormat == TRUE32)
        {
            /* page offset in UPCB */
            pstRsvSh->nNextUPCBPgOff = 0; 

            /* pointer to PIExt should be NULL at initial format */
            nRet = _UpdateUPCB(pstDev, pstVol, NULL, nDieIdx, BML_TYPE_INIT_CREATE);
            if (nRet != FSR_BML_SUCCESS)
            {
                nRet = FSR_BML_MAKE_NEW_PCB_FAILURE;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] update of UPCB is failed(nPDev: %d, nDieIdx: %d, UPCB: %d, BML_TYPE_INIT_CREATE)\r\n"),
                                               pstDev->nDevNo, nDieIdx, pstRsvSh->nUPCBSbn));
                break;
            }
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nRet));

    return nRet;
}
#endif /* !defined(FSR_NBL2) */

/*
 * @brief           This function updates UPCB (Unlockable Pool Control Block)
 *
 * @param[in]      *pstDev      : device context pointer
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]      *pstPExt     : FSRPIExt structure pointer 
 * @param[in]       nDieIdx     : index of die
 * @param[in]       nUpdateType : BML_TYPE_INIT_CREATE / BML_TYPE_UPDATE_PIEXT / 
 * @n                             BML_TYPE_WRITE_ERR /BML_TYPE_ERASE_ERR/ BML_TYPE_UPDATE_ERL 
 *
 * @return          FSR_BML_SUCCESS
 * @return          FSR_BML_NO_RSV_BLK_POOL
 * @return          FSR_BML_CRITICAL_ERROR
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE INT32
_UpdateUPCB(BmlDevCxt  *pstDev,
            BmlVolCxt  *pstVol,
            FSRPIExt   *pstPExt,
            UINT32      nDieIdx,
            UINT16      nUpdateType)
{
    BmlReservoir   *pstRsv;
    BmlReservoirSh *pstRsvSh;
    BmlBMF          stBmf;

    INT32           nLLDRe = FSR_LLD_SUCCESS;
    INT32           nPreLLDRe = FSR_LLD_SUCCESS;
    INT32           nBBMRe = FSR_BML_SUCCESS;
    BOOL32          bWrComplete;
    BOOL32          bRet = TRUE32;
    UINT32          nNumOfMetaPg;
    UINT32          nPgsPerPCB;
    UINT32          nOldPcb;
    UINT32          nCurPCBSbn;
    UINT32          nPcbType;
    UINT32          nTmpPcbSbn;
    UINT16          nBadMarkV;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(nPDev: %d, nDieIdx: %d, nUpdateType: %d)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nDieIdx, nUpdateType));

    pstRsv       = pstDev->pstDie[nDieIdx]->pstRsv;
    pstRsvSh     = pstDev->pstDie[nDieIdx]->pstRsvSh;

    /* 
     * number of available pages per PCB 
     * its value is calculated based on number of pages in SLC block
     */
    nPgsPerPCB    = pstVol->nNumOfPgsInSLCBlk;

    do
    {
        /* Writing BMS is completed ? */
        bWrComplete   = FALSE32;

        /* number of meta pages (meta data + confirm) */
        nNumOfMetaPg = BML_NUM_OF_META_PGS + 1;

        /* check initial flag for softprogram. If updatePCB is first, swap UPCB to TPCB */
        if(pstRsvSh->nFirstUpdateUPCB == 0)  
        {
            pstRsvSh->nFirstUpdateUPCB++;
            pstRsvSh->bKeepUPCB = FALSE32;
        }

#if defined (FSR_BML_DEBUG_CODE)
        /* ERL cheking code by ham 090413 */
        {
            UINT32   nValidCnt;
            UINT32   nIdx;
            BmlERL  *pstCurERL;

            pstCurERL = pstRsv->pstERL;

            nValidCnt = 0;

            for (nIdx = 0; nIdx < BML_MAX_ERL_ITEM; nIdx++)
            {
                if (pstCurERL->astERBlkInfo[nIdx].nSbn != 0xFFFF)
                {
                    nValidCnt++;
                }
            }
            FSR_ASSERT(pstCurERL->nCnt == nValidCnt);
        }
        /* ERL cheking code by ham 090413 */
#endif

        /* 
         * If PCB has free pages and the the latest meta pages has valid data, 
         * current PCB is updated (not initial format) 
         *
         * If value of bLatestUPCB is FALSE32, It means power was suddenly down 
         * during PCB update operation. In that case, BBM meta block should be programmed
         * to other block (not current PCB) because of soft-program.         
         */
        if ((pstRsvSh->nNextUPCBPgOff < nPgsPerPCB) &&
            ((nPgsPerPCB - pstRsvSh->nNextUPCBPgOff) >= nNumOfMetaPg) &&
            (pstRsvSh->nNextUPCBPgOff > 0) &&
            (pstRsvSh->bKeepUPCB == TRUE32))
        {
            /* if free pages are available, write meta data */
            nLLDRe = _UpdateMetaData(pstDev, 
                                     pstVol, 
                                     pstPExt, 
                                     nDieIdx, 
                                     pstRsvSh->nUPCBSbn,
                                     nUpdateType,
                                     BML_TYPE_UPCB);
            if (nLLDRe == FSR_LLD_SUCCESS)
            {
                /* meta pages are written successfully */
                bWrComplete = TRUE32;
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:INF] BBM meta update is completed\r\n")));
                break;
            } 

            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:INF] _UpdateMetaData(nPDev: %d, nDieIdx: %d, UPCB: %d, nUpdateType: 0x%x) is failed\r\n"),
                                           pstDev->nDevNo, nDieIdx, pstRsvSh->nUPCBSbn, nUpdateType));

            /* for make present PCB to bad block after following _MakeUPCB  */
            nPreLLDRe = nLLDRe;

            do
            {
                nOldPcb = pstRsvSh->nUPCBSbn;

                /* allocates new PCB into current PCB-index slot */
                nBBMRe = _AllocPCB(pstDev->pstDie[nDieIdx],
                                   pstVol,
                                   BML_TYPE_UPCB);
                if (nBBMRe != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Allocating PCB(UPCBSbn:%d) is failed\r\n"), 
                                                   pstRsvSh->nUPCBSbn));
                    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nBBMRe));
                    /* if allocating PCB is failed, it is critical error */
                    return nBBMRe;
                }

                stBmf.nSbn = (UINT16) nOldPcb;
                stBmf.nRbn = (UINT16) pstRsvSh->nUPCBSbn;

                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:INF] register nOldPcb(%d) into BMI\r\n"), nOldPcb));
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:INF] stBmf.nSbn  = %d\r\n"), stBmf.nSbn));
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:INF] stBmf.nRbn  = %d\r\n"), stBmf.nRbn));   

                bRet = _RegBMF(pstVol, &stBmf, pstRsv, BML_INVALID);
                if (bRet == FALSE32)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _RegBMF is failed(nSbn: %d, nRbn: %d)\r\n"), 
                                                   stBmf.nSbn, stBmf.nRbn));
                    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FSR_BML_CRITICAL_ERROR));
                    return FSR_BML_CRITICAL_ERROR;
                }

                /* make UPCB at the newly allocated PCB index */
                nLLDRe = _MakeUPCB(pstDev,
                                   pstVol,
                                   pstPExt,
                                   nDieIdx,
                                   pstRsvSh->nUPCBSbn,
                                   nUpdateType);

                /* decide nBadMarkV for previous PCB*/
                if (nPreLLDRe == FSR_LLD_PREV_ERASE_ERROR)
                {
                    nBadMarkV = BML_BADMARKV_ERASEERR;
                }
                else
                {
                    nBadMarkV = BML_BADMARKV_WRITEERR;
                }
                
                /* if making UPCB is failed, mark the old PCB as bad */
                _MakeBadBlk(nOldPcb, nBadMarkV, nDieIdx, pstDev, pstVol);

                /* delete nOldPCB */
                _DeletePcb(nOldPcb, nDieIdx, pstDev, pstVol);

                if (nLLDRe == FSR_LLD_SUCCESS)
                {
                    /* if making new PCB is completed exit do-while loop */
                    bWrComplete = TRUE32;
 
                    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:   ]   Making UPCB is passed\r\n"))); 

                    /* if making new PCB is completed exit do-while loop */
                    break;
                }
                nPreLLDRe = nLLDRe;

            } while (1);
        }

        /* if writing PCB is completed, exit do-while loop */
        if (bWrComplete == TRUE32)
        {
            break;
        }

        /* 
         * Available case
         * 1) PCB has no free pages 
         * 2) PCB is written for the first time
         * 3) PCB is soft-prgrammed (PCB block should be changed)
         */

        /* case 1 or 3 : not first writing */
        if (pstRsvSh->nNextUPCBPgOff != 0)
        {
            /* reset page offset */
            pstRsvSh->nNextUPCBPgOff = 0;

            FSR_ASSERT(pstRsvSh->nTPCBSbn != 0);

            /* use TPCB instead of UPCB */
            nCurPCBSbn = pstRsvSh->nTPCBSbn;
            
            /* UPCB is swapped with TPCB */
            nTmpPcbSbn       = pstRsvSh->nUPCBSbn;
            pstRsvSh->nUPCBSbn = (UINT16) nCurPCBSbn;
            pstRsvSh->nTPCBSbn = (UINT16) nTmpPcbSbn;
        }
        /* first writing case */
        else
        {
            /* use UPCB */
            nCurPCBSbn = pstRsvSh->nUPCBSbn;
        }

        nPcbType   = BML_TYPE_UPCB;

        /* make UPCB */
        nLLDRe = _MakeUPCB(pstDev,
                           pstVol,
                           pstPExt,
                           nDieIdx,
                           nCurPCBSbn,
                           nUpdateType);

        if (nLLDRe == FSR_LLD_SUCCESS)
        {
            /* making UPCB is completed */
            bWrComplete          = TRUE32;

            /* the latest pages of UPCB has valid data */
            pstRsvSh->bKeepUPCB = TRUE32;

            /* exit do-while loop */
            break;
        }

        /* for make present PCB to bad block after following _MakeUPCB  */
        nPreLLDRe = nLLDRe;

        /* if making UPCB is failed, ... */
        do
        {
            nOldPcb = nCurPCBSbn;

            /* allocates new PCB into current PCB-index slot */
            nBBMRe = _AllocPCB(pstDev->pstDie[nDieIdx],
                               pstVol,
                               nPcbType);
            if (nBBMRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Allocating PCB(nPcbType: 0x%x) is failed\r\n"), 
                                                nPcbType));
                /* if allocating PCB is failed, it is critical error */
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nBBMRe));
                return nBBMRe;
            }

            stBmf.nSbn = (UINT16) nOldPcb;
            stBmf.nRbn = (UINT16) pstRsvSh->nUPCBSbn;

            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:INF] register nOldPcb(%d) into BMI\r\n"), nOldPcb));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:INF] stBmf.nSbn  = %d\r\n"), stBmf.nSbn));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:INF] stBmf.nRbn  = %d\r\n"), stBmf.nRbn));

            /* register replaced block to BMF */
            bRet = _RegBMF(pstVol, &stBmf, pstRsv, BML_INVALID);
            if (bRet == FALSE32)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _RegBMF is failed(nSbn: %d, nRbn: %d)\r\n"), 
                                               stBmf.nSbn, stBmf.nRbn));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] <<CRITICAL ERROR>>\r\n")));
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FSR_BML_CRITICAL_ERROR));
                return FSR_BML_CRITICAL_ERROR;
            }

            /* make UPCB at the newly allocated PCB index */
            nLLDRe = _MakeUPCB(pstDev,
                               pstVol,
                               pstPExt,
                               nDieIdx,
                               stBmf.nRbn,
                               nUpdateType);

            /* decide nBadMarkV */
            if (nPreLLDRe == FSR_LLD_PREV_ERASE_ERROR)
            {
                nBadMarkV = BML_BADMARKV_ERASEERR;
            }
            else
            {
                nBadMarkV = BML_BADMARKV_WRITEERR;
            }
            
            /* if making UPCB is failed, mark the old PCB as bad */
            _MakeBadBlk(nOldPcb, nBadMarkV, nDieIdx, pstDev, pstVol);

            /* delete nOldPCB */
            _DeletePcb(nOldPcb, nDieIdx, pstDev, pstVol);

            if (nLLDRe == FSR_LLD_SUCCESS)
            {
                /* if making new PCB is completed exit do-while loop */
                bWrComplete = TRUE32;

                /* the latest pages of UPCB has valid data */
                pstRsvSh->bKeepUPCB = TRUE32;

                /* if making new PCB is completed exit do-while loop */
                break;
            }
            nPreLLDRe = nLLDRe;

        } while (1);

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FSR_BML_SUCCESS));

    return FSR_BML_SUCCESS;
}

/*
 * @brief           This function updates LPCB (tightly Lockable Pool Control Block)
 *
 * @param[in]      *pstDev      : device context pointer
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]      *pstPartI    : partition info structure pointer 
 * @param[in]       nDieIdx     : index of die
 * @param[in]       nUpdateType : BML_TYPE_INIT_CREATE / BML_TYPE_UPDATE_PIEXT /
 * @n                             BML_TYPE_WRITE_ERR /BML_TYPE_ERASE_ERR/ BML_TYPE_UPDATE_ERL
 *
 * @return          FSR_BML_SUCCESS  
 * @return          FSR_BML_NO_RSV_BLK_POOL  
 * @return          FSR_BML_CRITICAL_ERROR  
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE INT32
_UpdateLPCB(BmlDevCxt  *pstDev,
            BmlVolCxt  *pstVol,
            FSRPartI   *pstPartI,
            UINT32      nDieIdx,
            UINT16      nUpdateType)
{
    BmlReservoir   *pstRsv;
    BmlReservoirSh *pstRsvSh;
    BmlBMF          stBmf;
    INT32           nLLDRe = FSR_LLD_SUCCESS;
    INT32           nPreLLDRe = FSR_LLD_SUCCESS;
    INT32           nBBMRe = FSR_BML_SUCCESS;
    BOOL32          bWrComplete;
    BOOL32          bRet = TRUE32;
    UINT32          nNumOfMetaPg;
    UINT32          nPgsPerPCB;
    UINT32          nOldPcb;
    UINT32          nCurPCBSbn;
    UINT32          nPcbType;
    UINT32          nTmpPcbSbn;
    UINT16          nBadMarkV;
    
    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(nPDev: %d, nDieIdx: %d, nUpdateType: %d)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nDieIdx, nUpdateType));

    pstRsv       = pstDev->pstDie[nDieIdx]->pstRsv;
    pstRsvSh     = pstDev->pstDie[nDieIdx]->pstRsvSh;

    /* 
     * number of available pages per PCB 
     * its value is calculated based on number of pages in SLC block
     */
    nPgsPerPCB    = pstVol->nNumOfPgsInSLCBlk;

    do
    {
        /* Writing BMS is completed ? */
        bWrComplete   = FALSE32;
        
        /* number of meta pages (meta data + confirm) */
        nNumOfMetaPg = BML_NUM_OF_META_PGS + 1;

        /* check initial flag for softprogram. If updatePCB is first, swap LPCB to TPCB */
        if(pstRsvSh->nFirstUpdateLPCB == 0) 
        {
            pstRsvSh->nFirstUpdateLPCB++;
            pstRsvSh->bKeepUPCB = FALSE32;
        }
        
        /* 
         * If PCB has free pages and the latest meta pages has valid data, 
         * current PCB is updated (not initial format) 
         */
        if ((pstRsvSh->nNextLPCBPgOff < nPgsPerPCB) &&
            ((nPgsPerPCB - pstRsvSh->nNextLPCBPgOff) >= nNumOfMetaPg) &&
            (pstRsvSh->nNextLPCBPgOff > 0) &&
            (pstRsvSh->bKeepLPCB == TRUE32))
        {
            /* if free pages are available, write meta data */
            nLLDRe = _UpdateMetaData(pstDev, 
                                     pstVol, 
                                     pstPartI, 
                                     nDieIdx, 
                                     pstRsvSh->nLPCBSbn, 
                                     nUpdateType,
                                     BML_TYPE_LPCB);
            if (nLLDRe == FSR_LLD_SUCCESS)
            {
                bWrComplete = TRUE32;

                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:INF] BBM meta update is completed(nPDev: %d, nDieIdx: %d, LPCB: %d, nUpdateType: 0x%x)\r\n"),
                                                 pstDev->nDevNo, nDieIdx, pstRsvSh->nLPCBSbn, nUpdateType));
                break;
            } 

            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:INF] _UpdateMetaData() is failed\r\n")));

            /* for make present PCB to bad block after following _MakeLPCB  */
            nPreLLDRe = nLLDRe;
            
            do
            {
                nOldPcb = pstRsvSh->nLPCBSbn;

                /* allocates new PCB into current PCB-index slot */
                nBBMRe = _AllocPCB(pstDev->pstDie[nDieIdx],
                                   pstVol,
                                   BML_TYPE_LPCB);
                if (nBBMRe != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Allocating PCB(LPCB) is failed\r\n")));
                    /* if allocating PCB is failed, it is critical error */
                    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nBBMRe));
                    return nBBMRe;
                }

                stBmf.nSbn = (UINT16) nOldPcb;
                stBmf.nRbn = (UINT16) pstRsvSh->nLPCBSbn;

                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:INF] register nOldPcb(%d) into BMI\r\n"), nOldPcb));
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:INF] stBmf.nSbn  = %d\r\n"), stBmf.nSbn));
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:INF] stBmf.nRbn  = %d\r\n"), stBmf.nRbn));   

                bRet = _RegBMF(pstVol, &stBmf, pstRsv, BML_INVALID);
                if (bRet == FALSE32)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _RegBMF is failed(nSbn: %d, nRbn: %d)\r\n"), 
                                                   stBmf.nSbn, stBmf.nRbn));
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] <<CRITICAL ERROR>>\r\n")));
                    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FSR_BML_CRITICAL_ERROR));
                    return FSR_BML_CRITICAL_ERROR;
                }

                /* make LPCB at the newly allocated PCB index */
                nLLDRe = _MakeLPCB(pstDev,
                                   pstVol,
                                   pstPartI,
                                   nDieIdx,
                                   pstRsvSh->nLPCBSbn,
                                   nUpdateType);

                /* decide nBadMarkV */
                if (nPreLLDRe == FSR_LLD_PREV_ERASE_ERROR)
                {
                    nBadMarkV = BML_BADMARKV_ERASEERR; 
                }
                else
                {
                    nBadMarkV = BML_BADMARKV_WRITEERR;
                }

                /* if making LPCB is failed, mark the old PCB as bad */
                _MakeBadBlk(nOldPcb, nBadMarkV, nDieIdx, pstDev, pstVol);

                /* delete nOldPCB */
                _DeletePcb(nOldPcb, nDieIdx, pstDev, pstVol);

                if (nLLDRe == FSR_LLD_SUCCESS)
                {
                    /* if making new PCB is completed exit do-while loop */
                    bWrComplete = TRUE32;

                    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:   ]   Making LPCB is passed\r\n"))); 

                    /* if making new PCB is completed exit do-while loop */
                    break;
                }
                nPreLLDRe = nLLDRe;

            } while (1);
        }

        /* if writing PCB is completed, exit do-while loop */
        if (bWrComplete == TRUE32)
        {
            break;
        }

        /* 
         * Available case
         * 1) PCB has no free pages 
         * 2) PCB is written for the first time
         * 3) PCB is soft-prgrammed (PCB block should be changed)
         */

        /* case 1 or 3 : not first writing */
        if (pstRsvSh->nNextLPCBPgOff != 0)
        {
            /* reset page offset */
            pstRsvSh->nNextLPCBPgOff = 0;

            FSR_ASSERT(pstRsvSh->nTPCBSbn != 0);

            /* use TPCB instead of LPCB */
            nCurPCBSbn = pstRsvSh->nTPCBSbn;

            nTmpPcbSbn       = pstRsvSh->nLPCBSbn;
            pstRsvSh->nLPCBSbn = (UINT16) nCurPCBSbn;
            pstRsvSh->nTPCBSbn = (UINT16) nTmpPcbSbn;
        }
        /* first writing case */
        else
        {
            /* use LPCB */
            nCurPCBSbn = pstRsvSh->nLPCBSbn;
        }

        nPcbType   = BML_TYPE_LPCB;

        /* make LPCB at the alternate PCB index */
        nLLDRe = _MakeLPCB(pstDev,
                           pstVol,
                           pstPartI,
                           nDieIdx,
                           nCurPCBSbn,
                           nUpdateType);
                           
        if (nLLDRe == FSR_LLD_SUCCESS)
        {
            /* making LPCB is completed */
            bWrComplete          = TRUE32;

            /* the latest pages of LPCB has valid data */
            pstRsvSh->bKeepLPCB = TRUE32;

            /* exit do-while loop */
            break;
        }

        /* for make present PCB to bad block after following _MakeLPCB  */
        nPreLLDRe = nLLDRe;

        /* if making LPCB is failed, ... */
        do
        {
            nOldPcb = nCurPCBSbn;

            /* allocates new PCB into current PCB-index slot */
            nBBMRe = _AllocPCB(pstDev->pstDie[nDieIdx],
                               pstVol,
                               nPcbType);
            if (nBBMRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Allocating new LPCB is failed\r\n")));
                /* if allocating PCB is failed, it is critical error */
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nBBMRe));
                return nBBMRe;
            }

            stBmf.nSbn = (UINT16) nOldPcb;
            stBmf.nRbn = (UINT16) pstRsvSh->nLPCBSbn;

            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:INF] register nOldPcb(%d) into BMI\r\n"), nOldPcb));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:INF] stBmf.nSbn  = %d\r\n"), stBmf.nSbn));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:INF] stBmf.nRbn  = %d\r\n"), stBmf.nRbn));   

            /* register replaced block to BMF */
            bRet = _RegBMF(pstVol, &stBmf, pstRsv, BML_INVALID);
            if (bRet == FALSE32)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _RegBMF is failed(nSbn: %d, nRbn: %d)\r\n"), 
                                               stBmf.nSbn, stBmf.nRbn));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] <<CRITICAL ERROR>>\r\n")));
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FSR_BML_CRITICAL_ERROR));
                return FSR_BML_CRITICAL_ERROR;
            }

            /* make LPCB at the newly allocated PCB index */        
            nLLDRe = _MakeLPCB(pstDev,
                               pstVol,
                               pstPartI,
                               nDieIdx,
                               stBmf.nRbn,
                               nUpdateType);

            /* decide nBadMarkV */
            if (nPreLLDRe == FSR_LLD_PREV_ERASE_ERROR)
            {
                nBadMarkV = BML_BADMARKV_ERASEERR;
            }
            else
            {
                nBadMarkV= BML_BADMARKV_WRITEERR;
            }
            
            /* if making LPCB is failed, mark the old PCB as bad */
            _MakeBadBlk(nOldPcb, nBadMarkV, nDieIdx, pstDev, pstVol);

            /* delete nOldPCB */
            _DeletePcb(nOldPcb, nDieIdx, pstDev, pstVol);

            if (nLLDRe == FSR_LLD_SUCCESS)
            {
                /* if making new PCB is completed exit do-while loop */
                bWrComplete = TRUE32;

                /* the latest pages of LPCB has valid data */
                pstRsvSh->bKeepLPCB = TRUE32;

                /* if making new PCB is completed exit do-while loop */
                break;
            }
            nPreLLDRe = nLLDRe;

        } while (1);

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FSR_BML_SUCCESS));

    return FSR_BML_SUCCESS;
}

/*
 * @brief           This function makes UPCB (Unlockable Pool Control Block)
 *
 * @param[in]      *pstDev      : device context pointer
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]      *pstPExt     : FSRPIExt structure pointer
 * @param[in]       nDieIdx     : index of die
 * @param[in]       nPCBIdx     : index of PCB
 * @param[in]       nUpdateType : BML_TYPE_INIT_CREATE / BML_TYPE_UPDATE_PIEXT /
 * @n                             BML_TYPE_WRITE_ERR / BML_TYPE_ERASE_ERR / BML_TYPE_UPDATE_ERL
 *
 * @return          FSR_LLD_SUCCESS
 * @return          Some LLD errors
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE INT32
_MakeUPCB(BmlDevCxt    *pstDev,
          BmlVolCxt    *pstVol,
          FSRPIExt     *pstPExt,
          UINT32        nDieIdx,
          UINT32        nPCBIdx,
          UINT16        nUpdateType)
{
    BmlReservoirSh *pstRsvSh;
    INT32           nRet = FSR_LLD_SUCCESS;
    INT32           nMajorErr;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(nPDev: %d, nDieIdx: %d, nPCBIdx: %d, nUpdateType: %d)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nDieIdx, nPCBIdx, nUpdateType));

    do
    {
        pstRsvSh    = pstDev->pstDie[nDieIdx]->pstRsvSh;

        /* ignore return value of EraseNflush() */
        nRet = _LLDErase(pstVol, 
                         pstDev->nDevNo, 
                        &nPCBIdx, 
                         FSR_LLD_FLAG_1X_ERASE);

        nMajorErr = FSR_RETURN_MAJOR(nRet);

        if (nMajorErr != FSR_LLD_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] LLDErase(nPDev: %d, nPCBIdx: %d, UPCB) is failed\r\n"), 
                                           pstDev->nDevNo, nPCBIdx));
            nRet = FSR_LLD_PREV_ERASE_ERROR;
            break;
        }

        pstRsvSh->nNextUPCBPgOff = 0;

        /* Remove UPCB from ERL list because it is already deleted */
        nRet = FSR_BBM_UpdateERL(pstVol,
                                 pstDev,
                                 nDieIdx,
                                 nPCBIdx,
                                 BML_FLAG_ERL_UPCB_DELETE);
        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] FSR_BBM_UpdateERL(nPDev: %d, nDieIdx: %d, nPCBIdx: %d, ERL_DELETE) is failed in _MakeUPCB()\r\n"),
                                           pstDev->nDevNo, nDieIdx, nPCBIdx));
            break;
        }

        /* write meta data */
        nRet = _UpdateMetaData(pstDev, 
                               pstVol, 
                               pstPExt, 
                               nDieIdx, 
                               nPCBIdx, 
                               nUpdateType,
                               BML_TYPE_UPCB);
        if (nRet != FSR_LLD_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _UpdateMetaData(nPDev: %d, nDieIdx: %d, nPCBIdx: %d, nUpdateType: 0x%x, UPCB) is failed\r\n"),
                                           pstDev->nDevNo, nDieIdx, nPCBIdx, nUpdateType));
            break;
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nRet));

    return nRet;
}

/*
 * @brief           This function makes LPCB (tightly Lockable Pool Control Block)
 *
 * @param[in]      *pstDev      : device context pointer
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]      *pstPartI    : FSRPartI structure pointer
 * @param[in]       nDieIdx     : index of die
 * @param[in]       nPCBIdx     : index of PCB
 * @param[in]       nUpdateType : BML_TYPE_INIT_CREATE / BML_TYPE_UPDATE_PIEXT /
 * @n                             BML_TYPE_WRITE_ERR / BML_TYPE_ERASE_ERR / BML_TYPE_UPDATE_ERL
 *
 * @return          FSR_LLD_SUCCESS
 * @return          FSR_LLD_PREV_ERASE_ERROR
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE INT32
_MakeLPCB(BmlDevCxt    *pstDev,
          BmlVolCxt    *pstVol,
          FSRPartI     *pstPartI,
          UINT32        nDieIdx,
          UINT32        nPCBIdx,
          UINT16        nUpdateType)
{
    BmlReservoirSh *pstRsvSh;
    INT32           nRet = FSR_LLD_SUCCESS;
    INT32           nMajorErr;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(nPDev: %d, nDieIdx: %d, nPCBIdx: %d, nUpdateType: %d)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nDieIdx, nPCBIdx, nUpdateType));

    do
    {
        pstRsvSh = pstDev->pstDie[nDieIdx]->pstRsvSh;

        /* ignore return value of EraseNflush() */
        nRet = _LLDErase(pstVol, 
                         pstDev->nDevNo,
                         &nPCBIdx,
                         FSR_LLD_FLAG_1X_ERASE);

        nMajorErr = FSR_RETURN_MAJOR(nRet);

        if (nMajorErr != FSR_LLD_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] LLDErase(nPDev: %d, nPCBIdx: %d, LPCB) is failed\r\n"), 
                                           pstDev->nDevNo, nPCBIdx));
            nRet = FSR_LLD_PREV_ERASE_ERROR;
            break;
        }

        pstRsvSh->nNextLPCBPgOff = 0;

        /* write meta data */
        nRet = _UpdateMetaData(pstDev,
                               pstVol,
                               pstPartI,
                               nDieIdx,
                               nPCBIdx,
                               nUpdateType,
                               BML_TYPE_LPCB);
        if (nRet != FSR_LLD_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _UpdateMetaData(nPDev: %d, nDieIdx: %d, nPCBIdx: %d, nUpdateType: 0x%x, LPCB) is failed\r\n"),
                                           pstDev->nDevNo, nDieIdx, nPCBIdx, nUpdateType));
            break;
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nRet));

    return nRet;
}

/*
 * @brief           This function makes PCH (Pool Control Header)
 *
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]       pstRsv      : pointer of Reservoir structure
 * @param[in]      *pBuf        : pointer of buffer
 * @param[in]       nUpdateType :BML_TYPE_LPCB / BML_TYPE_UPCB
 *
 * @return          none
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE VOID
_MakePCH(BmlVolCxt     *pstVol,
         BmlDieCxt     *pstDie,
         UINT8         *pBuf,
         UINT16         nType)
{
    BmlPoolCtlHdr     *pstPCH;
    BmlReservoirSh    *pstRsvSh;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(nType: %d)\r\n"),
                                     __FSR_FUNC__, nType));

    pstPCH = (BmlPoolCtlHdr *) pBuf;

    FSR_OAM_MEMSET(pstPCH, 0xFF, sizeof(BmlPoolCtlHdr));

    pstRsvSh = pstDie->pstRsvSh;

    /* PCH for LPCB */
    if (nType == BML_TYPE_LPCB)
    {
        /* signature for LPCH */
        FSR_OAM_MEMCPY(pstPCH->aSig, BML_LPCH_SIG, BML_MAX_PCH_SIG);

        /* increase age of LPCH */
        pstPCH->nAge    = ++pstRsvSh->nLPcbAge;
    }
    /* PCH for UPCB */
    else
    {
        /* signature for UPCH */
        FSR_OAM_MEMCPY(pstPCH->aSig, BML_UPCH_SIG, BML_MAX_PCH_SIG);

        /* increate age of UPCH */
        pstPCH->nAge    = ++pstRsvSh->nUPcbAge;
    }

    /* increase age of PCB */
    pstPCH->nGlobalAge = ++pstRsvSh->nGlobalPCBAge;

    /* pbn of TPCB block */
    pstPCH->nTPCBPbn = pstRsvSh->nTPCBSbn;

    /* pbn of REF block */
    pstPCH->nREFPbn = pstRsvSh->nREFSbn;

    /* number of MLC reservoir */
    pstPCH->nNumOfMLCRsvr = (UINT16) pstDie->pstRsv->nNumOfRsvrInMLC;

    /* device info for compatibility */
    pstPCH->stDevInfo.nNumOfPlane     = pstVol->nNumOfPlane;
    pstPCH->stDevInfo.nNumOfDieInDev  = pstVol->nNumOfDieInDev;
    pstPCH->stDevInfo.nNumOfBlksInDie = pstVol->nNumOfBlksInDie;
    pstPCH->stDevInfo.nNumOfBlksInDev = pstVol->nNumOfBlksInDev;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s()\r\n"), __FSR_FUNC__));
}

/*
 * @brief           This function makes UPCB (Unlockable Pool Control Block)
 *
 * @param[in]      *pstPExt     : FSRPIExt structure pointer 
 * @param[in]       nDieIdx     : index of die
 * @param[in]       nPCBIdx     : index of PCB 
 * @param[in]       nUpdateType : BML_TYPE_INIT_CREATE / BML_TYPE_UPDATE_PIEXT / 
 * @n                             BML_TYPE_WRITE_ERR / BML_TYPE_ERASE_ERR / BML_TYPE_UPDATE_ERL
 *
 * @return          none
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE VOID
_MakePIA(UINT8     *pBuf,
         VOID      *pstPI,
         UINT16     nPCBType)
{
    FSRPIExt    *pstPIExt;
    FSRPIExt    *pstPIExtDup;
    FSRPartI    *pPartIDup;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(nPCBType: %d)\r\n"),
                                     __FSR_FUNC__, nPCBType));

    if (nPCBType == BML_TYPE_LPCB)
    {           
        pPartIDup = (FSRPartI*) pBuf;

        /* copy partition info to main buffer */
        FSR_OAM_MEMCPY(pPartIDup, pstPI, sizeof(FSRPartI));
    }
    else /* if (nPCBType == BML_TYPE_UPCB) */
    {

        /* PiExt */
        pstPIExt    = (FSRPIExt *) pstPI;

        if (pstPIExt != NULL)
        {
            pstPIExtDup = (FSRPIExt *) pBuf;

            if (pstPIExt->nSizeOfData > FSR_BML_MAX_PIEXT_DATA)
            {
                pstPIExt->nSizeOfData = FSR_BML_MAX_PIEXT_DATA;
            }

            FSR_OAM_MEMCPY((UINT8*)pstPIExtDup, pstPIExt, sizeof(FSRPIExt));

        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s()\r\n"), __FSR_FUNC__));
}

/*
 * @brief           This function copies BMS to buffer for PCB
 *
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]      *pstRsv      : Reservoir context pointer
 * @param[in]       pBuf        : main buffer pointer
 * @param[out]     *pBMFIdx     : BMF index which is already copied
 * @param[in]       nPCBType    : BML_TYPE_LPCB / BML_TYPE_UPCB 
 * @param[in]       nUpdateType : BML_TYPE_INIT_CREATE / BML_TYPE_UPDATE_PIEXT /
 * @n                             BML_TYPE_WRITE_ERR / BML_TYPE_ERASE_ERR / BML_TYPE_UPDATE_ERL
 *
 * @return          none
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE VOID
_MakeBMS(BmlVolCxt     *pstVol,
         BmlReservoir  *pstRsv,
         UINT8         *pBuf,
         UINT32        *pBMFIdx,
         UINT32         nPCBType,
         UINT16         nUpdateType)
{
    UINT32      nNewBMFIdx;
    UINT32      nBMFIdx;
    UINT32      nSbn;
    UINT32      nDieIdx;
    UINT16      nLockState;
    BmlBMS     *pstBMS;
    BmlBMI     *pstBMI;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(nPCBType: %d, nUpdateType: %d)\r\n"),
                                     __FSR_FUNC__, nPCBType, nUpdateType));

    nBMFIdx      = *pBMFIdx;
    pstBMI       = pstRsv->pstBMI;

    pstBMS       = (BmlBMS *) pBuf;

    pstBMS->nInf = (UINT16) nUpdateType;

    do
    {
        nNewBMFIdx = 0;

        /* classify blocks in Reservoir depending on its lock state */
        do
        {
            if (nBMFIdx >= pstBMI->nNumOfBMFs)
            {
                break;
            }

            nSbn = (UINT32)(pstRsv->pstBMF[nBMFIdx].nSbn);

            nDieIdx = (nSbn >> pstVol->nSftNumOfBlksInDie) & 0x1;

            /* loop up attribute of partition to know lock state of the block */
            nLockState = _LookUpPartAttr(pstVol, nDieIdx, (UINT16) nSbn);

            if (nPCBType == BML_TYPE_LPCB)
            {
                /* 
                 * if state of this block is unlock state or lock state,
                 * skip and move to next BMF field
                 */
                if (nLockState != FSR_LLD_BLK_STAT_LOCKED_TIGHT)
                {   
                    nBMFIdx++;
                    continue;
                }
            }
            else /* if (nPCBType == BML_TYPE_UPCB) */
            {
                /* 
                 * if state of this block is lock tighten state,
                 * skip and move to next BMF field
                 */
                if (nLockState == FSR_LLD_BLK_STAT_LOCKED_TIGHT)
                {   
                    nBMFIdx++;
                    continue;
                }
            }

            pstBMS->stBMF[nNewBMFIdx].nSbn = pstRsv->pstBMF[nBMFIdx].nSbn;
            pstBMS->stBMF[nNewBMFIdx].nRbn = pstRsv->pstBMF[nBMFIdx].nRbn;

            nBMFIdx++;
            nNewBMFIdx++;

        } while (nNewBMFIdx < BML_BMFS_PER_BMS);

        pBuf += FSR_SECTOR_SIZE;
        *pBMFIdx = nBMFIdx;

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s()\r\n"), __FSR_FUNC__));
}

/*
 * @brief           This function copies RCBSbn to buffer for PCB
 * 
 * @param[in]      *pstRsv      : Reservoir context pointer
 * @param[in]       pBuf        : main buffer pointer
 * @param[out]     *pRCBFIdx    : RCBF index which is already copied
 *
 * @return          none
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE VOID
_MakeRCBS(BmlReservoir  *pstRsv,
          UINT8         *pBuf,
          UINT32        *pRCBFIdx)
{
    UINT32      nNewRCBFIdx;
    UINT32      nRCBFIdx;
    UINT32      nSbn;
    UINT32      nMaxRCBF;
    UINT16     *pRCBSbn;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s()\r\n"), __FSR_FUNC__));

    nRCBFIdx     = *pRCBFIdx;

    nMaxRCBF = (pstRsv->nNumOfRsvrInSLC + pstRsv->nNumOfRsvrInMLC) / 2;

    pRCBSbn = (UINT16 *) pBuf;

    do
    {
        nNewRCBFIdx = 0;

        do
        {
            if (nRCBFIdx >= nMaxRCBF)
            {
                break;
            }

            nSbn = pstRsv->pstRCB[nRCBFIdx];

            /* Free entries are removed */
            if (nSbn == 0xFFFF)
            {
                nRCBFIdx++;
                continue;
            }

            pRCBSbn[nNewRCBFIdx] = (UINT16) nSbn;

            nRCBFIdx++;
            nNewRCBFIdx++;

        } while (nNewRCBFIdx < BML_RCBFS_PER_RCBS);

        pBuf += FSR_SECTOR_SIZE;
        *pRCBFIdx = nRCBFIdx;
        
    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s()\r\n"), __FSR_FUNC__));
}

/*
 * @brief           This function writes meta data to PCB
 *
 * @param[in]      *pstDev      : device context pointer
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]      *pstPI       : FSRPIExt or FSRPartI structure pointer
 * @param[in]       nDieIdx     : index of die
 * @param[in]       nPCBIdx     : index of PCB 
 * @param[in]       nUpdateType : BML_TYPE_INIT_CREATE / BML_TYPE_UPDATE_PIEXT /
 * @n                             BML_TYPE_WRITE_ERR / BML_TYPE_ERASE_ERR / BML_TYPE_UPDATE_ERL
 * @param[in]       nPCBType    : BML_TYPE_UPCB / BML_TYPE_LPCB
 *
 * @return          FSR_LLD_SUCCESS  
 * @return          Some LLD errors
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE INT32
_UpdateMetaData(BmlDevCxt  *pstDev,
                BmlVolCxt  *pstVol,
                VOID       *pstPI,
                UINT32      nDieIdx,
                UINT32      nPCBIdx,
                UINT16      nUpdateType,
                UINT16      nPCBType)
{
    BmlReservoir   *pstRsv;
    BmlReservoirSh *pstRsvSh;
    FSRSpareBuf     stSBuf;
    INT32           nRet = FSR_LLD_SUCCESS;
    UINT32          nSctIdx;
    UINT32          nBMFOffset;
    UINT32          nRCBFOffset;
    UINT32          nBMSSctOffset;
    UINT32          nNumOfBMS;
    UINT32          nNumOfRCB;
    UINT32          nSizeOfMaxRsvr;
    UINT32          nSizeOfBABitMap;
    UINT32          nPgOff;
    UINT32          nFlag;
    UINT32          nExtIdx;
    UINT8          *pBufPtr;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(nPDev: %d, nDieIdx: %d, nPCBIdx, nUpdateType: %d, nPCBType: %d)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nDieIdx, nPCBIdx, nUpdateType, nPCBType));

    do
    {
        pstRsv      = pstDev->pstDie[nDieIdx]->pstRsv;
        pstRsvSh    = pstDev->pstDie[nDieIdx]->pstRsvSh;

        /* Reset buffer */
        FSR_OAM_MEMSET(pstRsv->pMBuf, 0xFF, pstVol->nSizeOfPage);

        /* initialize buffer pointer to address of MBuf */
        pBufPtr = pstRsv->pMBuf;

        nBMFOffset  = 0;
        nRCBFOffset = 0;

        /*
         *  Layout of meta data in PCB
         *
         *  2KB page              4KB page
         *  +---+---+---+---+     +---+---+---+---+---+---+---+---+
         *  |PCH|PIA|ERL|BAB|     |PCH|PIA|ERL|BAB|Rsv|Rsv|Rsv|Rsv| 
         *  +---+---+---+---+     +---+---+---+---+---+---+---+---+
         *  |BMS|BMS|BMS|BMS|     |BMS|BMS|BMS|BMS|BMS|BMS|BMS|BMS|
         *  +---+---+---+---+     +---+---+---+---+---+---+---+---+
         *  |Rsv|Rsv|Rsv|RCB|     |Rsv|Rsv|Rsv|Rsv|Rsv|Rsv|RCB|RCB| 
         *  +---+---+---+---+     +---+---+---+---+---+---+---+---+
         *  |    Confirm    |     |            Confirm            |
         *  +---+---+---+---+     +---+---+---+---+---+---+---+---+
         *       4 pages                        4 pages      
         *   4 sectors per page            8 sectors per page          
         * 
         *  (Rsv means unused sectors)
         */

        /* 2KB page: 4, 4KB page: 8 */
        nBMSSctOffset = pstVol->nSctsPerPg;

        /* default value for 1 plane device */
        nNumOfRCB = 0;

        /* 2KB page device */
        if (pstVol->nSctsPerPg == BML_2KB_PG)
        {
            /* number of BMS sectors */
            nNumOfBMS = BML_NUM_OF_BMS_2KB_PG;
            
            /* max number of reservoir blocks */
            nSizeOfMaxRsvr = ((BML_NUM_OF_BMS_2KB_PG * BML_BMFS_PER_BMS) * 2) / 3;
        }
        /* 4KB page device */
        else
        {   
            /* number of BMS sectors */
            nNumOfBMS = BML_NUM_OF_BMS_4KB_PG;

            /* max number of reservoir blocks */
            nSizeOfMaxRsvr = ((BML_NUM_OF_BMS_4KB_PG * BML_BMFS_PER_BMS) * 2) / 3;
        }

        /* multi plane device */
        if (pstVol->nNumOfPlane > 1)
        {
            /* 
             * number of entries for reservoir candidate block = 
             * max number of reservoir blocks which is supported by FSR / 2
             * 
             * max number of BMF entries = 3/2 * max number of reservoir blocks 
             *                           = nNumOfBMS * BML_BMFS_PER_BMS
             * 
             * max number of BMF entries / 3 = max number of reservoir blocks / 2
             *
             * number of sectors for reservoir candidate block = 
             *  ceiling of (number of entries for reservoir candidate block / number of RCB entries in sector) 
             *
             */
            nNumOfRCB = ((nNumOfBMS * BML_BMFS_PER_BMS) / 3);

            if (nNumOfRCB % BML_RCBFS_PER_RCBS)
            {
                nNumOfRCB = (nNumOfRCB / BML_RCBFS_PER_RCBS) + 1;
            }
            else
            {
                nNumOfRCB = (nNumOfRCB / BML_RCBFS_PER_RCBS);
            }
        }

        /* calculate size of BABitMap using nSizeOfMaxRsvr */
        if (nSizeOfMaxRsvr % BML_NUM_OF_BITS_IN_1BYTE)
        {
            nSizeOfBABitMap = (nSizeOfMaxRsvr / BML_NUM_OF_BITS_IN_1BYTE) + 1;
        }
        else
        {
            nSizeOfBABitMap = (nSizeOfMaxRsvr / BML_NUM_OF_BITS_IN_1BYTE);
        }

        FSR_OAM_MEMCPY(&stSBuf, pstRsv->pSBuf, sizeof(FSRSpareBuf));
        
        /* initialize STL meta area in spare (unused area) */
        stSBuf.pstSpareBufBase->nSTLMetaBase0 = BML_INVALID;
        stSBuf.pstSpareBufBase->nSTLMetaBase1 = BML_INVALID;
        stSBuf.pstSpareBufBase->nSTLMetaBase2 = BML_INVALID;

        if (stSBuf.nNumOfMetaExt != 0)
        {
            stSBuf.nNumOfMetaExt = pstVol->nSizeOfPage / FSR_PAGE_SIZE_PER_SPARE_BUF_EXT;

            /* initialize STL meta extension area in spare (unused area) */
            for (nExtIdx = 0; nExtIdx < stSBuf.nNumOfMetaExt; nExtIdx++)
            {
                FSR_OAM_MEMSET(stSBuf.pstSTLMetaExt + nExtIdx, 0xFF, sizeof(FSRSpareBufExt));
            }
        }

        for (nSctIdx = 0; nSctIdx <= BML_NUM_OF_META_PGS * pstVol->nSctsPerPg; nSctIdx++)
        {
            /* PCH sector offset : 0 */
            if (nSctIdx == BML_PCH_SCT_OFF)
            {
                /* copy PCH structure to main buffer */
                _MakePCH(pstVol, pstDev->pstDie[nDieIdx], pBufPtr, nPCBType); 
                pBufPtr += sizeof(BmlPoolCtlHdr);
            }
            /* PIA sector offset : 1 */
            else if (nSctIdx == BML_PIA_SCT_OFF)
            {
                /* copy partition info to main buffer */
                _MakePIA(pBufPtr, pstPI, nPCBType);
                pBufPtr += FSR_SECTOR_SIZE;
            }
            /* ERL sector offset : 2 */
            else if (nSctIdx == BML_ERL_SCT_OFF)
            {
                /* copy ERL to main buffer */
                FSR_OAM_MEMCPY(pBufPtr, pstRsv->pstERL, sizeof(BmlERL));
                pBufPtr += sizeof(BmlERL);
            }
            /* BAB sector offset : 3 */
            else if (nSctIdx == BML_BAB_SCT_OFF)
            {
                /* copy BAB(Block Allocation Bitmap) to main buffer */
                FSR_OAM_MEMCPY(pBufPtr, pstRsv->pBABitMap, nSizeOfBABitMap);
                pBufPtr += FSR_SECTOR_SIZE;
            }
            /* BMS sector */
            else if ((nBMSSctOffset <= nSctIdx) && 
                     (nSctIdx < nBMSSctOffset + nNumOfBMS))
            {
                /* 
                 * If there is no data to be written or 
                 * all data is already written, skip.
                 * If not, copy BMS data to buffer
                 */               
                if (pstRsv->pstBMI->nNumOfBMFs > nBMFOffset)
                {
                    /* copy BMS structure to main buffer */
                    _MakeBMS(pstVol, 
                             pstRsv, 
                             pBufPtr, 
                             &nBMFOffset, 
                             nPCBType, 
                             nUpdateType);
                }

                pBufPtr += FSR_SECTOR_SIZE;

            }
            /* RCBS sector */
            else if ((nSctIdx >= ((BML_NUM_OF_META_PGS * pstVol->nSctsPerPg) - nNumOfRCB)) && 
                     (nSctIdx < (BML_NUM_OF_META_PGS * pstVol->nSctsPerPg)))
            {
                /* 
                 * If there is no data to be written or 
                 * all data is already written, skip.
                 * If not, copy RCB data to buffer
                 */               
                if ((pstVol->nNumOfPlane > 1) &&
                   (((pstRsv->nNumOfRsvrInSLC + pstRsv->nNumOfRsvrInMLC) / 2) > nRCBFOffset))
                {
                    /* copy RCBS structure to main buffer */
                    _MakeRCBS(pstRsv, 
                              pBufPtr, 
                              &nRCBFOffset);
                }

                pBufPtr += FSR_SECTOR_SIZE;

            }
             /* confirm page = sector offset : 24 */
            else if (nSctIdx == (BML_NUM_OF_META_PGS * pstVol->nSctsPerPg))
            {
                /* Set buffer to zero for confirm mark */
                FSR_OAM_MEMSET(pstRsv->pMBuf, 0, pstVol->nSizeOfPage);

                pBufPtr += pstVol->nSizeOfPage;
            }
            /* unused sectors */
            else
            {
                /* skip reserved area (1 sector) */
                pBufPtr += FSR_SECTOR_SIZE;
            }

            /* if buffer is fully filled (page size) */
            if ((UINT32)(pBufPtr - pstRsv->pMBuf) >= pstVol->nSizeOfPage)
            {
                nFlag = FSR_LLD_FLAG_1X_PROGRAM | FSR_LLD_FLAG_ECC_ON;

                /* LPCB update case */
                if (nPCBType == BML_TYPE_LPCB)
                {
                    nPgOff = pstRsvSh->nNextLPCBPgOff;
                }
                /* UPCB update case */
                else
                {
                    nPgOff = pstRsvSh->nNextUPCBPgOff;
                }

                nRet = _LLDWrite(pstVol, 
                                 pstDev, 
                                 nDieIdx,
                                 nPCBIdx, 
                                 nPgOff, 
                                 pstRsv->pMBuf,
                                 &stSBuf,
                                 BML_META_DATA, 
                                 nFlag);
                if (nRet != FSR_LLD_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _LLDWrite is failed\r\n")));
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] (nPDev: %d, nDieIdx: %d, PBN:%d, nPgOff: %d, nFlag: 0x%x, nPCBTYpe: 0x%x, BBM META)\r\n"), 
                                                   pstDev->nDevNo, nDieIdx, nPCBIdx, nPgOff, nFlag, nPCBType));
                    break;
                }

                /* LPCB update case */
                if (nPCBType == BML_TYPE_LPCB)
                {
                    /* increase page offset */
                    pstRsvSh->nNextLPCBPgOff++;
                }
                /* UPCB update case */
                else
                {
                    /* increase page offset */
                    pstRsvSh->nNextUPCBPgOff++;
                }

                /* Reset buffer */
                FSR_OAM_MEMSET(pstRsv->pMBuf, 0xFF, pstVol->nSizeOfPage);
                
                /* Reset buffer pointer */
                pBufPtr = pstRsv->pMBuf;
            }

        } /* end of for */ 
        
    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nRet));

    return nRet;
}

/*
 * @brief           This function allocates new PCB block
 *
 * @param[in]      *pstRsv      : Reservoir structure pointer 
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]       nType       : PCB type (BML_TYPE_UPCB / BML_TYPE_LPCB) 
 *
 * @return          FSR_BML_SUCCESS  
 * @return          FSR_BML_NO_RSV_BLK_POOL
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE INT32
_AllocPCB(BmlDieCxt    *pstDie,
          BmlVolCxt    *pstVol,
          UINT32        nType)
{
    BmlReservoir   *pstRsv;
    BmlReservoirSh *pstRsvSh;
    UINT32          nBlkType;
    UINT32          nFreePbn;
    BOOL32          bPaired;
    BOOL32          bRet = TRUE32;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(nType: %s)\r\n"),
                                     __FSR_FUNC__, (nType == BML_TYPE_LPCB) ? "LPCB" : "UPCB"));

    pstRsv      = pstDie->pstRsv;
    pstRsvSh    = pstDie->pstRsvSh;

    /* MLC only */
    if (pstRsv->nRsvrType == BML_MLC_RESERVOIR)
    {
        nBlkType = BML_MLC_BLK;
    }
    /* SLC only or hybrid type */
    else
    {
        nBlkType = BML_SLC_BLK;
    }

    /* Allocate PCB */
    bRet = _GetFreeRB(pstRsv,
                      pstVol->nNumOfPlane,
                      nBlkType,
                      BML_META_DATA,
                      1,
                      &nFreePbn,
                      &bPaired);
    if (bRet == FALSE32)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR]  NO FreePbn(nBlkType: 0x%x, BBM META)\r\n"), nBlkType));
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Current # of BMF:%d, n1stSbnOfRsvr:%d, nLastSbnOfRsvr: %d\r\n"), 
                                        pstRsv->pstBMI->nNumOfBMFs, pstRsv->n1stSbnOfRsvr, pstRsv->nLastSbnOfRsvr));
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR]  <<CRITICAL ERROR>>\r\n")));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FSR_BML_NO_RSV_BLK_POOL));
        return FSR_BML_NO_RSV_BLK_POOL;
    }

    switch (nType)
    {
    case BML_TYPE_LPCB:
        pstRsvSh->nLPCBSbn = (UINT16) nFreePbn;
        break;
    case BML_TYPE_UPCB:
        pstRsvSh->nUPCBSbn = (UINT16) nFreePbn;
        break;
    case BML_TYPE_TPCB:
        pstRsvSh->nTPCBSbn = (UINT16) nFreePbn; 
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:INF] FreePbn = %d\r\n"), nFreePbn));

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, TRUE32));

    return FSR_BML_SUCCESS;
}

/*
 * @brief           This function registers the new BMF into the free BMF entry of BMI
 *
 * @param[in]      *pstVol      : pointer of volume context
 * @param[in]      *pstBMF      : pointer of mBMF structure that new BMF is stored
 * @param[in]      *pstRsv      : Reservoir structure pointer
 * @param[in]       nCandidate  : Pbn of reservoir candidate to be registered
 * @n                             (If the value is valid, the Pbn should be registered as reservoir candidate)
 *
 * @return          TRUE32
 * @return          FALSE32
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE BOOL32
_RegBMF(BmlVolCxt    *pstVol,
        BmlBMF       *pstBMF,
        BmlReservoir *pstRsv,
        UINT32        nCandidate)
{
    UINT32  nIdx;
    UINT32  nFreeBMFEntry;
    BOOL32  bRegisteredBMF = FALSE32;
    BOOL32  bRsvrRbn       = TRUE32;
    BOOL32  bRsvrSbn       = FALSE32;
    BOOL32  bCandidate     = FALSE32;
    BmlBMI *pstBMI;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s()\r\n"), __FSR_FUNC__));

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:   ]    Check whether nBadSbn(%d) is already registered in BMI\r\n"), 
                                     pstBMF->nSbn));

    pstBMI        = pstRsv->pstBMI;
    nFreeBMFEntry = pstBMI->nNumOfBMFs;

    /* check whether the range of pstBMI->nNumOfBMFs is out of bound */
    if (pstBMI->nNumOfBMFs >= (UINT16) ((pstRsv->nLastSbnOfRsvr - pstRsv->n1stSbnOfRsvr - BML_RSV_META_BLKS)*3/2))
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] pstBMI->nNumOfBMFs(%d) > (pstVol->nRsvrPerDev(%d) - BML_RSV_META_BLKS)\r\n"),
                                        pstBMI->nNumOfBMFs, pstRsv->n1stSbnOfRsvr - pstRsv->nLastSbnOfRsvr));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FALSE32));
        return FALSE32;
    }

    /* Check whether nBadSbn is already registered in BMI. */
    for (nIdx = 0; nIdx < pstBMI->nNumOfBMFs; nIdx++)
    {
        if (pstRsv->pstBMF[nIdx].nSbn == pstBMF->nSbn)
        {
            nFreeBMFEntry  = nIdx;
            bRegisteredBMF = TRUE32;
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:   ] nBadSbn(%d) is already registered in index #%d\r\n"), pstBMF->nSbn, nIdx));
            break;
        }
    }

    /* 
     * If Rbn is not reservoir area, the block is in the user area 
     * (reservoir candidate block is used) 
     */

    /* hybrid type (SLC + MLC) */
    if (pstRsv->nRsvrType == BML_HYBRID_RESERVOIR)
    {
        if ((pstRsv->n1stSbnOfRsvr > pstBMF->nRbn) || (pstBMF->nRbn > pstRsv->nLastSbnOfRsvr))
        {
            bRsvrRbn = FALSE32;
        }
    }
    /* SLC only or MLC only */
    else
    {
        if (pstRsv->n1stSbnOfRsvr > pstBMF->nRbn)
        {
            bRsvrRbn = FALSE32;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:   ]    nFreeBMFEntry : %d\r\n"), nFreeBMFEntry));
    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:   ]    Register new BMF into BMI\r\n")));

    if (bRegisteredBMF == FALSE32)
    {
        /* register the given Sbn and the new replaced block */
        pstRsv->pstBMF[nFreeBMFEntry].nSbn = pstBMF->nSbn;
        pstRsv->pstBMF[nFreeBMFEntry].nRbn = pstBMF->nRbn;

        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:   ]    BMF[%d]->nSbn : %d\r\n"), nFreeBMFEntry, pstBMF->nSbn));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:   ]    BMF[%d]->nRbn : %d\r\n"), nFreeBMFEntry, pstBMF->nRbn));

        /* 
         * If Rbn is in the reservoir, set allocation bit map 
         * When Rbn is reservoir candidate block in the user area, allocation bit map can not be set
         */
        if (bRsvrRbn == TRUE32)
        {
            _SetAllocRB(pstRsv, pstBMF->nRbn);
        }
    }
    else
    {
        /* Check whether nBadSbn is already registered in BMI. */
        for (nIdx = 0; nIdx < pstBMI->nNumOfBMFs; nIdx++)
        {
            /* Check if the registered block is a reservoir candidate block */
            if (pstRsv->pstBMF[nIdx].nSbn == pstRsv->pstBMF[nFreeBMFEntry].nRbn)
            {
                /* nRbn of the already registered mapping is reservoir candidate */
                bCandidate = TRUE32;
                break;
            }
        }

        /* 
         * if pstBMF->nSbn is already registered,
         * two copies of BMF is registered.     
         * one     is BMF for both the given Sbn and the new replaced block
         * another is BMF for both the bad Pbn   and the new replaced block 
         */
        pstRsv->pstBMF[nFreeBMFEntry].nSbn      = pstBMF->nSbn;

        /* If nRbn of the already registered mapping is not reservoir candidate */
        if (bCandidate != TRUE32)
        {
            pstRsv->pstBMF[pstBMI->nNumOfBMFs].nSbn = pstRsv->pstBMF[nFreeBMFEntry].nRbn;
        }

        pstRsv->pstBMF[nFreeBMFEntry].nRbn      = pstBMF->nRbn;

        /* If nRbn of the already registered mapping is not reservoir candidate */
        if (bCandidate != TRUE32)
        {
            pstRsv->pstBMF[pstBMI->nNumOfBMFs].nRbn = pstBMF->nRbn;
        }

        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:   ]    BMF[%d]->nSbn : %d\r\n"), nFreeBMFEntry, pstBMF->nSbn));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:   ]    BMF[%d]->nRbn : %d\r\n"), nFreeBMFEntry, pstBMF->nRbn));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:   ]    BMF[%d]->nSbn : %d\r\n"), pstBMI->nNumOfBMFs, pstRsv->pstBMF[pstBMI->nNumOfBMFs].nSbn));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:   ]    BMF[%d]->nRbn : %d\r\n"), pstBMI->nNumOfBMFs, pstRsv->pstBMF[pstBMI->nNumOfBMFs].nRbn));

        /* 
         * If Rbn is in the reservoir, set allocation bit map 
         * When Rbn is reservoir candidate block in the user area, allocation bit map can not be set
         */
        if (bRsvrRbn == TRUE32)
        {
            _SetAllocRB(pstRsv, pstBMF->nRbn);
        }

        /* If replaced block is in the reservoir, set allocation bit map */
        if ((pstRsv->n1stSbnOfRsvr <= pstRsv->pstBMF[pstBMI->nNumOfBMFs].nSbn) && 
            (pstRsv->pstBMF[pstBMI->nNumOfBMFs].nSbn <= pstRsv->nLastSbnOfRsvr))
        {
            _SetAllocRB(pstRsv, pstRsv->pstBMF[pstBMI->nNumOfBMFs].nSbn);
        }
    }

    if (bCandidate != TRUE32)
    {
        /* Increase the number of BMFs */
        pstBMI->nNumOfBMFs++;
    }
    
    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:   ] pstBMI->nNumOfBMFs : %d\r\n"), pstBMI->nNumOfBMFs));

    /* Sorts by ascending power of pstBMI->pstBMF[x].nSbn */
    _SortBMI(pstRsv);

    /* works for handling reservoir candidate blocks */
    if (pstVol->nNumOfPlane > 1)
    {
        if ((pstRsv->n1stSbnOfRsvr <= pstBMF->nSbn) && (pstBMF->nSbn <= pstRsv->nLastSbnOfRsvr))
        {
            bRsvrSbn = TRUE32;
        }

        /* 
         * If the Sbn can be registered as reservoir candidate 
         * (only for 2 plane device)
         * 
         * BMFIdx 0  1  2  3      RCBIdx  0  1  
         *      +--+--+--+--+--+         +--+--+--+
         * Sbn  |10|11|20|21|--|         |20|11|--|
         *      +--+--+--+--+--+         +--+--+--+
         * Rbn  |50|51|52|53|--|         reservoir candidate blocks
         *      +--+--+--+--+--+   
         *                         
         */
        if ((nCandidate != BML_INVALID) && (bRsvrSbn == FALSE32))
        {

            /* unassigned area of RCB should be scanned to allocate free slot */  

            for (nIdx = 0; nIdx < (pstRsv->nNumOfRsvrInMLC + pstRsv->nNumOfRsvrInSLC) / 2; nIdx++)
            {
                /* if free slot is found */
                if (pstRsv->pstRCB[nIdx] == 0xFFFF)
                {
                    /* register nCandidate to reservoir candidate */
                    pstRsv->pstRCB[nIdx] = (UINT16) nCandidate;

                    /* Increase the number of RCBs */
                    pstBMI->nNumOfRCBs++;

                    break;
                }
            }
        }

        /* delete allocated RCB data from the RCB queue */
        _DeleteRCB(pstRsv, pstBMF->nRbn);

    } /* end of if (pstVol->nNumOfPlane) */

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, TRUE32));

    return TRUE32;
}

/*
 * @brief           This function deletes reservoir candidate block from RCB queue
 *
 * @param[in]      *pstRsv      : Reservoir structure pointer
 * @param[in]       nSbn        : block number to be deleted
 *
 * @return          none
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE VOID
_DeleteRCB(BmlReservoir *pstRsv,
           UINT32        nSbn)
{
    UINT32  nIdx;
    BmlBMI *pstBMI;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(nSbn: %d)\r\n"), __FSR_FUNC__, nSbn));

    pstBMI        = pstRsv->pstBMI;

    /* unassigned area of RCB should be scanned to delete RCB data */
    for (nIdx = 0; nIdx < (pstRsv->nNumOfRsvrInMLC + pstRsv->nNumOfRsvrInSLC) / 2; nIdx++)
    {
        /* if free candidate block is found */
        if (pstRsv->pstRCB[nIdx] == nSbn)
        {
            /* delete reservoir candidate info */
            pstRsv->pstRCB[nIdx] = 0xFFFF;

            /* Decrease the number of RCBs */
            pstBMI->nNumOfRCBs--;

            break;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s()\r\n"), __FSR_FUNC__));

}

/*
 * @brief           This function makes the given nBadPbn as a bad block
 *
 * @param[in]       nBadPbn     : bad physical block number   
 * @param[in]       nMarkValue  : value that is written for bad mark
 * @param[in]       nDieIdx     : die index
 * @param[in]      *pstDev      : BmlDevCxt structure pointer
 * @param[in]      *pstVol      : BmlVolCxt structure pointer
 *
 * @return          none
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE VOID
_MakeBadBlk(UINT32       nBadPbn, 
            UINT16       nMarkValue, 
            UINT32       nDieIdx,
            BmlDevCxt   *pstDev, 
            BmlVolCxt   *pstVol)
{
    BmlReservoir   *pstRsv;
    FSRSpareBuf     stSBuf;
    UINT32          nPgIdx;
    UINT32          nFlag = FSR_LLD_FLAG_NONE;
    UINT32          nPairMSBPg;
    UINT32          nBlockType = FSR_LLD_MLC_BLOCK;
    UINT32          nPgsPerBlk;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(nPDev: %d, nBadPbn: %d, nMarkValue: %d, nDieIdx: %d)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nBadPbn, nMarkValue, nDieIdx));

    pstRsv = pstDev->pstDie[nDieIdx]->pstRsv;

    /* erase error case */
    if (nMarkValue == BML_BADMARKV_ERASEERR)
    {
        nFlag = FSR_LLD_FLAG_WR_EBADMARK | FSR_LLD_FLAG_1X_PROGRAM | FSR_LLD_FLAG_ECC_OFF;
    }
    /* write error case */
    else if (nMarkValue == BML_BADMARKV_WRITEERR)
    {
        nFlag = FSR_LLD_FLAG_WR_WBADMARK | FSR_LLD_FLAG_1X_PROGRAM | FSR_LLD_FLAG_ECC_OFF;
    }    

    FSR_OAM_MEMCPY(&stSBuf, pstRsv->pSBuf, sizeof(FSRSpareBuf));
    if (stSBuf.nNumOfMetaExt != 0)
    {
        stSBuf.nNumOfMetaExt = pstVol->nSizeOfPage / FSR_PAGE_SIZE_PER_SPARE_BUF_EXT;
    }

    /* Check bad block info for knowing whether SLC or BML */
    /* Although return value is failed, ignore return value because bad mark write */
    pstVol->LLD_GetBlockInfo(pstDev->nDevNo,
                             nBadPbn,
                             &nBlockType,
                             &nPgsPerBlk);

    /* Reset buffer */
    FSR_OAM_MEMSET(pstRsv->pMBuf, 0xFF, pstVol->nSizeOfPage);

    /* write bad mark to spare of page0 and page1 */
    for (nPgIdx = 0; nPgIdx < 2; nPgIdx++)
    {              
        /* ignore return value of _LLDWrite */
        _LLDWrite(pstVol, 
                  pstDev, 
                  nDieIdx,   /* index of die */
                  nBadPbn,   /* pbn of bad block */
                  nPgIdx,    /* page index */
                  pstRsv->pMBuf,      /* main buffer = 0xFF */
                  &stSBuf,   /* spare buffer */
                  BML_USER_DATA, /* meta data block */
                  nFlag);    /* flag for LLDWrite() */
    }

    /* If Bad mark is written on MLC, MSB page of 0, 1 also should be written by 0xFF*/
    if ((pstVol->nNANDType == FSR_LLD_FLEX_ONENAND) && (nBlockType == FSR_LLD_MLC_BLOCK))
    {
        for (nPgIdx = 0; nPgIdx < 2; nPgIdx++)
        {
            nPairMSBPg = pstVol->pPairedPgMap[nPgIdx];

            /* ignore return value of _LLDWrite */
            _LLDWrite(pstVol,
                      pstDev,
                      nDieIdx,   /* index of die */
                      nBadPbn,   /* pbn of bad block */
                      nPairMSBPg,    /* page index */
                      pstRsv->pMBuf,      /* main buffer = 0xFF */
                      NULL,   /* spare buffer */
                      BML_USER_DATA, /* meta data block */
                      nFlag);    /* flag for LLDWrite() */
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s()\r\n"), __FSR_FUNC__));
}

/*
 * @brief           This function deletes the given PCB
 *
 * @param[in]       nPcb        : Pool Control Block number
 * @param[in]       nDieIdx     : die index
 * @param[in]      *pstDev      : BmlDevCxt structure pointer
 * @param[in]      *pstVol      : BmlVolCxt structure pointer
 *
 * @return          none
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE VOID
_DeletePcb(UINT32       nPcb,
           UINT32       nDieIdx,
           BmlDevCxt   *pstDev,
           BmlVolCxt   *pstVol)
{
    BmlReservoir   *pstRsv;
    FSRSpareBuf     stSBuf;
    UINT32          nFlag;
    UINT32          nBlockType = FSR_LLD_MLC_BLOCK;
    UINT32          nPgsPerBlk;
    
    FSR_STACK_VAR;

    FSR_STACK_END;
 
    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(nPDev: %d, nDieIdx: %d, nPcb: %d)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nDieIdx, nPcb));

    /* get Reservoir pointer for thie die */
    pstRsv = pstDev->pstDie[nDieIdx]->pstRsv;

    /* Reset buffer */
    FSR_OAM_MEMSET(pstRsv->pMBuf, 0, pstVol->nSizeOfPage);

    FSR_OAM_MEMCPY(&stSBuf, pstRsv->pSBuf, sizeof(FSRSpareBuf));
    if (stSBuf.nNumOfMetaExt != 0)
    {
        stSBuf.nNumOfMetaExt = pstVol->nSizeOfPage / FSR_PAGE_SIZE_PER_SPARE_BUF_EXT;
    }

    /* Set BML meta mark as invalid value */
    stSBuf.pstSpareBufBase->nBMLMetaBase0 = 0;

    nFlag = FSR_LLD_FLAG_1X_PROGRAM | FSR_LLD_FLAG_ECC_OFF;

    /* Set STL meta mark as invalid value */
    stSBuf.pstSpareBufBase->nSTLMetaBase0 = 0;
    stSBuf.pstSpareBufBase->nSTLMetaBase1 = 0;
    stSBuf.pstSpareBufBase->nSTLMetaBase2 = 0;

    /* Check bad block info for knowing whether SLC or BML */
    /* Although return value is failed, ignore return value because bad mark write */
    pstVol->LLD_GetBlockInfo(pstDev->nDevNo,
                             nPcb,
                             &nBlockType,
                             &nPgsPerBlk);

    /* ignore return value of _LLDWrite */
    _LLDWrite(pstVol, 
              pstDev, 
              nDieIdx,   /* index of die */
              nPcb,      /* pbn to be deleted */
              0,         /* page index */
              pstRsv->pMBuf,      /* main buffer = NULL */
              &stSBuf,   /* spare buffer */
              BML_USER_DATA, /* meta data block */
              nFlag);    /* flag for LLDWrite() */ 

    /* If Bad mark is written on MLC, MSB page of 0 also should be written by 0xFF*/
    if ((pstVol->nNANDType == FSR_LLD_FLEX_ONENAND) && (nBlockType == FSR_LLD_MLC_BLOCK))
    {
            /* ignore return value of _LLDWrite */
            _LLDWrite(pstVol, 
                      pstDev, 
                      nDieIdx,   /* index of die */
                      nPcb,   /* pbn of bad block */
                      4,    /* page index */
                      pstRsv->pMBuf,      /* main buffer = 0xFF */
                      NULL,   /* spare buffer */
                      BML_USER_DATA, /* meta data block */
                      nFlag);    /* flag for LLDWrite() */
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s()\r\n"), __FSR_FUNC__));
}

/*
 * @brief           This function translates semi-physical block number to physical block number
 *        
 * @param[in]       nSbn        : semi physical block number   
 * @param[in]       pstBMI      : pointer of BMI structure that current BMI is stored
 *
 * @return          physical block number
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE UINT32
_TransSbn2Pbn(UINT32        nSbn,
              BmlReservoir *pstRsv)
{
    UINT32 nPbn;
    UINT32 nIdx;

    FSR_STACK_VAR;

    FSR_STACK_END;

    nPbn = nSbn;
    for (nIdx = 0; nIdx < pstRsv->pstBMI->nNumOfBMFs; nIdx++)
    {
        if (pstRsv->pstBMF[nIdx].nSbn == nSbn)
        {
            nPbn = pstRsv->pstBMF[nIdx].nRbn;
            break;
        }
    }

    return nPbn;
}

/*
 * @brief           This function registers the current BMF into the free BMP of BMSG
 *
 * @param[in]      *pstVol          : BmlVolCxt structure pointer
 * @param[in]      *pstDev          : BmlDevCxt structure pointer 
 * @param[in]       nBadSbn         : semi physical block number
 * @param[in]       nNumOfBadBlks   : number of bad blocks to be replaced simultaneously
 * @param[out]     *pnFreeBlk       : replaced block Pbn
 * @param[out]     *pbPaired        : paired replacement occurs or not
 *
 * @return          TRUE32
 * @return          FALSE32
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE BOOL32
_GetRepBlk(BmlVolCxt   *pstVol,
           BmlDevCxt   *pstDev,
           UINT32       nBadSbn,
           UINT32       nNumOfBadBlks,
           UINT32      *pnFreeBlk,
           BOOL32      *pbPaired)
{
    BmlReservoir   *pstRsv;    
    BOOL32          bRet = TRUE32;
    UINT32          nBlkType;
    UINT32          nDieIdx;    

    FSR_STACK_VAR;

    FSR_STACK_END;
 
    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(nPDev: %d, nBadSbn: %d, nNumOfBadBlks: %d)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nBadSbn, nNumOfBadBlks));

    
    /* calculate die index using Sbn */
    nDieIdx = (nBadSbn >> pstVol->nSftNumOfBlksInDie) & 0x1;

    /* get a pointer of Reservoir for thie die */
    pstRsv  = pstDev->pstDie[nDieIdx]->pstRsv;

    do
    {
        /* MLC bad block */
        if ((UINT32)(nBadSbn - (pstVol->nNumOfBlksInDie * nDieIdx)) >= pstDev->nNumOfSLCBlksInDie[nDieIdx])
        {
            nBlkType = BML_MLC_BLK;
            FSR_DBZ_RTLMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:   ] BadSbn(%d) is included in MLC AREA\r\n"), nBadSbn));
        }
        /* SLC bad block */
        else
        {
            nBlkType = BML_SLC_BLK;
            FSR_DBZ_RTLMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:   ] BadSbn(%d) is included in SLC AREA\r\n"), nBadSbn));
        }

        /* find free reservoir block for an initial bad block */
        bRet = _GetFreeRB(pstRsv,
                          pstVol->nNumOfPlane,
                          nBlkType,
                          BML_USER_DATA,
                          nNumOfBadBlks,
                          pnFreeBlk,
                          pbPaired);
        if (bRet == FALSE32)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR]  No free blk in Reservoir(nBlkType: %d, USER DATA)\r\n"), nBlkType));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR]  # of bad blocks:%d, Current # of BMF:%d, n1stSbnOfRsvr:%d, nLastSbnOfRsvr: %d\r\n"), 
                                           nNumOfBadBlks, pstRsv->pstBMI->nNumOfBMFs, pstRsv->n1stSbnOfRsvr, pstRsv->nLastSbnOfRsvr));
            break;
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, bRet));

    return bRet;
}

/*
 * @brief           This function copies the given nSrcPbn to the given nDstPbn
 *
 * @param[in]      *pstVol          : BmlVolCxt structure pointer
 * @param[in]      *pstDev          : BmlDevCxt structure pointer
 * @param[in]       nDieIdx         : Die index
 * @param[in]       nSrcSbn         : semi physical block number of source
 * @param[in]       nPgOff          : page offset
 * @param[in]       nDstPbn         : semi physical block number of destination 
 * @param[in]       bRecoverPrevErr : Flag to recover corrupted LSB paired page for previous write error
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @return          FSR_LLD_SUCCESS
 * @return          FSR_LLD_PREV_ERASE_ERROR
 * @return          FSR_LLD_PREV_WRITE_ERROR
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE INT32
_CopyBlk(BmlVolCxt     *pstVol,
         BmlDevCxt     *pstDev,
         UINT32         nDieIdx,
         UINT32         nSrcPbn, 
         UINT32         nPgOff,
         UINT32         nDstPbn, 
         BOOL32         bRecoverPrevErr)
{
    BmlReservoir   *pstRsv;
    FSRSpareBuf     stSBuf;
    INT32           nLLDRe = FSR_LLD_SUCCESS;
    INT32           nMajorErr;
    UINT32          nNumOfPages;
    UINT32          nPDev;
    UINT32          nPgIdx;
    UINT32          nFlag;
    UINT32          nPairLSBPg;
    UINT32          nNextPairLSBPg;
    BOOL32          bRecovery = FALSE32;
    BOOL32          bFreeMBuf = FALSE32;
    BOOL32          bFreeSBuf = FALSE32;
    UINT8          *pMBuf;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(nPDev: %d, nDieIdx: %d, nSrcPbn: %d, nPgOff, nDstPbn: %d)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nDieIdx, nSrcPbn, nPgOff, nDstPbn));

    pstRsv = pstDev->pstDie[nDieIdx]->pstRsv;
    nPDev  = pstDev->nDevNo;

    /* erase destination block */
    nLLDRe = _LLDErase(pstVol, nPDev, &nDstPbn, FSR_LLD_FLAG_1X_ERASE);
    if (nLLDRe != FSR_LLD_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Erase Failure in _CopyBlk(nPDev: %d, nPbn: %d)\r\n"), nPDev, nDstPbn));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FSR_LLD_PREV_ERASE_ERROR));
        return FSR_LLD_PREV_ERASE_ERROR;
    }

    pMBuf    = (UINT8 *)       pstRsv->pMBuf;

    /* source pbn and destination pbn should be in the same cell area */
    FSR_ASSERT(((nSrcPbn - (pstVol->nNumOfBlksInDie * nDieIdx) >= pstDev->nNumOfSLCBlksInDie[nDieIdx]) &&
                (nDstPbn - (pstVol->nNumOfBlksInDie * nDieIdx) >= pstDev->nNumOfSLCBlksInDie[nDieIdx])) ||
               ((nSrcPbn - (pstVol->nNumOfBlksInDie * nDieIdx) <  pstDev->nNumOfSLCBlksInDie[nDieIdx]) &&
                (nDstPbn - (pstVol->nNumOfBlksInDie * nDieIdx) <  pstDev->nNumOfSLCBlksInDie[nDieIdx])));

    /* initialize LSB page offset */
    nPairLSBPg     = BML_INVALID;
    nNextPairLSBPg = BML_INVALID;

    /* MLC block */
    if ((UINT32)(nSrcPbn - (pstVol->nNumOfBlksInDie * nDieIdx)) >= pstDev->nNumOfSLCBlksInDie[nDieIdx])
    {
        nNumOfPages = pstVol->nNumOfPgsInMLCBlk;

        /* 
         * If the device is the Flex-OneNAND and error page is MSB,
         * recover corrupted pair LSB page using recovery load mode
         */
        if (pstVol->nNANDType == FSR_LLD_FLEX_ONENAND)
        {
            if (nPgOff > pstVol->pPairedPgMap[nPgOff])
            {
                /* find pair LSB page */
                nPairLSBPg = pstVol->pPairedPgMap[nPgOff];
            }

            /* When paired LSB page of previous write error should be also recovered */
            if ((bRecoverPrevErr == TRUE32) &&
                (nPgOff + 1 > pstVol->pPairedPgMap[nPgOff + 1]))
            {
                /* find pair LSB page */    
                nNextPairLSBPg = pstVol->pPairedPgMap[nPgOff + 1];
            }
        }
    }
    /* SLC block */
    else
    {
        nNumOfPages = pstVol->nNumOfPgsInSLCBlk;
    }

    for (nPgIdx = 0; nPgIdx < nNumOfPages; nPgIdx++)
    {
        /* 
         * If current page offset is equal or larger than 
         * page offset which has an error, copy operation should be skipped. 
         * If not, sequential programming rule or NOP can be broken when
         * the device can support a cached program feature
         */
        if (nPgIdx >= nPgOff)
        {
            continue;
        }

        /* If the page should be recovered */
        if ((nPgIdx == nPairLSBPg) || (nPgIdx == nNextPairLSBPg))
        {
            /*Since user can use FSR in a multi-processor environment, which shares FSRSpareBuf variable, 
            the size of spare area should be maximum value considering the case that _CopyBlk is used by STL module.
            So nNumOfMetaExt should be assigned everytime _CopyBlk is called*/
            FSR_OAM_MEMCPY(&stSBuf, pstRsv->pSBuf, sizeof(FSRSpareBuf));
            stSBuf.nNumOfMetaExt = pstVol->nSizeOfPage / FSR_PAGE_SIZE_PER_SPARE_BUF_EXT;

            bRecovery = FALSE32;

            /* read one page from source block */
            nLLDRe = _LLDRead(pstVol,               /* Volume Number        */
                              nPDev,                /* PDev Number          */
                              nSrcPbn,              /* source pbn           */
                              nPgIdx,               /* page offset          */
                              pstRsv,               /* Reservoir pointer    */
                              pstRsv->pMBuf,        /* main buffer pointer  */
                              &stSBuf,              /* spare buffer pointer */
                              BML_USER_DATA,        /* data type            */
                              bRecovery,            /* recovery load or nomal load */
                              FSR_LLD_FLAG_ECC_ON); /* Operation Flag       */

            nMajorErr = FSR_RETURN_MAJOR(nLLDRe);

            /* if uncorrectable read error is not returned, do normal read */
            if (nMajorErr != FSR_LLD_PREV_READ_ERROR)
            {
                bRecovery = FALSE32;
            }
            /* if uncorrectable read error is returned, do recovery read */
            else
            {
                bRecovery = TRUE32;
            }
        }
        /* normal read */
        else
        {
            bRecovery = FALSE32;
        }

        /*Since user can use FSR in a multi-processor environment, which shares FSRSpareBuf variable, 
        the size of spare area should be maximum value considering the case that _CopyBlk is used by STL module.
        So nNumOfMetaExt should be assigned everytime _CopyBlk is called*/
        FSR_OAM_MEMCPY(&stSBuf, pstRsv->pSBuf, sizeof(FSRSpareBuf));
        stSBuf.nNumOfMetaExt = pstVol->nSizeOfPage / FSR_PAGE_SIZE_PER_SPARE_BUF_EXT;

        /* read one page from source block */
        nLLDRe = _LLDRead(pstVol,               /* Volume Number        */
                          nPDev,                /* PDev Number          */
                          nSrcPbn,              /* source pbn           */
                          nPgIdx,               /* page offset          */
                          pstRsv,               /* Reservoir pointer    */
                          pstRsv->pMBuf,        /* main buffer pointer  */
                          &stSBuf,              /* spare buffer pointer */
                          BML_USER_DATA,        /* data type            */
                          bRecovery,            /* recovery load or nomal load */
                          FSR_LLD_FLAG_ECC_ON); /* Operation Flag       */

        bFreeMBuf = _CmpData(pstVol, pMBuf, 0xFF, FALSE32);
        bFreeSBuf = _CmpData(pstVol, (UINT8 *) &stSBuf, 0xFF, TRUE32);

        /* if the page is free, skip it */
        if ((bFreeMBuf == TRUE32) && (bFreeSBuf == TRUE32))
        {
            continue;
        }

        nMajorErr = FSR_RETURN_MAJOR(nLLDRe);

        /* read disturbance is ignored because data is already copied into new block */
        if ((nMajorErr != FSR_LLD_SUCCESS) &&
            (nMajorErr != FSR_LLD_PREV_READ_ERROR) &&
            (nMajorErr != FSR_LLD_PREV_READ_DISTURBANCE))
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Read failure in _CopyBlk\r\n")));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] (nPDev: %d, nDieIdx: %d, Pbn: %d, nPgIdx: %d, bRecovery: %d, USER DATA)\r\n"), 
                                           pstDev->nDevNo, nDieIdx, nDstPbn, nPgIdx, bRecovery));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nLLDRe));
            return nLLDRe;
        }

        /* 
         * If uncorrectable read error is found, 
         *  do read and write operation with ECC OFF flag 
         */
        if ((nPgIdx != nPairLSBPg) && 
            (nPgIdx != nNextPairLSBPg) &&
            (nMajorErr == FSR_LLD_PREV_READ_ERROR))
        {
            FSR_OAM_MEMCPY(&stSBuf, pstRsv->pSBuf, sizeof(FSRSpareBuf));
            stSBuf.nNumOfMetaExt = pstVol->nSizeOfPage / FSR_PAGE_SIZE_PER_SPARE_BUF_EXT;

            /* Re-read one page with ECC OFF (ignore return value) */
            _LLDRead(pstVol,                /* Volume Number        */
                     nPDev,                 /* PDev Number          */
                     nSrcPbn,               /* source pbn           */
                     nPgIdx,                /* page offset          */
                     pstRsv,                /* Reservoir pointer    */
                     pstRsv->pMBuf,         /* main buffer pointer  */
                     &stSBuf,               /* spare buffer pointer */  
                     BML_USER_DATA,         /* data type            */
                     FALSE32,               /* nomal load           */
                     FSR_LLD_FLAG_ECC_OFF); /* Operation Flag       */

            /* set write operation flag (ECC OFF) */
            nFlag = FSR_LLD_FLAG_1X_PROGRAM | FSR_LLD_FLAG_ECC_OFF;
        }
        else
        {
            /* set write operation flag (default: ECC ON) */
            nFlag = FSR_LLD_FLAG_1X_PROGRAM | FSR_LLD_FLAG_ECC_ON;
        }

        /* write data to destination block */
        nLLDRe = _LLDWrite(pstVol,
                           pstDev,
                           nDieIdx,
                           nDstPbn,
                           nPgIdx,
                           pstRsv->pMBuf,
                           &stSBuf,
                           BML_USER_DATA,
                           nFlag);

        if (nLLDRe != FSR_LLD_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Write Failure in _CopyBlk\r\n")));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] (nPDev: %d, nDieIdx: %d, Pbn: %d, nPgIdx: %d, nFlag: 0x%x, USER DATA)\r\n"), 
                                           pstDev->nDevNo, nDieIdx, nDstPbn, nPgIdx, nFlag));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FSR_LLD_PREV_WRITE_ERROR));
            return FSR_LLD_PREV_WRITE_ERROR;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FSR_LLD_SUCCESS));

    return FSR_LLD_SUCCESS;
}

/*
 * @brief           This function copies the given nSrcPbn to the given nDstPbn
 *
 * @param[in]      *pstVol      : BmlVolCxt structure pointer
 * @param[in]      *pstDev      : BmlDevCxt structure pointer
 * @param[in]       nDieIdx     : Die index
 * @param[in]       nSrcSbn     : semi physical block number of source
 * @param[in]       nDstPbn     : semi physical block number of destination
 * @param[out]     *bReadFlag   : flag for read operation during copy operation
 * @param[out]     *bUCReadErr  : flag means whether uncorrectable read error occurs or not
 *
 * @return          FSR_LLD_SUCCESS
 * @return          FSR_LLD_PREV_WRITE_ERROR
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE INT32
_CopyBlkRefresh(BmlVolCxt  *pstVol,
                BmlDevCxt  *pstDev,
                UINT32      nDieIdx,
                UINT32      nSrcPbn,
                UINT32      nDstPbn,
                BOOL32     *bReadFlag,
                BOOL32     *bUCReadErr,
                UINT32      nFlag)
{
    BmlReservoir   *pstRsv;
    FSRSpareBuf     stSBuf;
    INT32           nLLDRe;
    INT32           nMajorErr;
    BOOL32          bFreeMBuf = FALSE32;
    BOOL32          bFreeSBuf = FALSE32;
    UINT32          nNumOfPages;
    UINT32          nPDev;
    UINT32          nPgIdx;
    UINT32          nLLDFlag;
    UINT32          nSrcBlkType;
    UINT32          nDstBlkType;
    UINT8          *pMBuf;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(nPDev: %d, nDieIdx: %d, nSrcPbn: %d, nDstPbn: %d)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nDieIdx, nSrcPbn, nDstPbn));

    pstRsv = pstDev->pstDie[nDieIdx]->pstRsv;
    nPDev  = pstDev->nDevNo;

    pMBuf    = (UINT8 *)       pstRsv->pMBuf;

    nNumOfPages = pstVol->nNumOfPgsInMLCBlk;

    /* Check source block */

    /* MLC block */
    if ((UINT32)(nSrcPbn - (pstVol->nNumOfBlksInDie * nDieIdx)) >= pstDev->nNumOfSLCBlksInDie[nDieIdx])
    {
        nSrcBlkType = BML_MLC_BLK;
    }
    /* SLC block */
    else
    {
        nSrcBlkType = BML_SLC_BLK;
    }

    /* Check destination block */

    /* MLC block */
    if ((UINT32)(nDstPbn - (pstVol->nNumOfBlksInDie * nDieIdx)) >= pstDev->nNumOfSLCBlksInDie[nDieIdx])
    {
        nDstBlkType = BML_MLC_BLK;
    }
    /* SLC block */
    else
    {
        nDstBlkType = BML_SLC_BLK;
    }

    /* 
     * If source or destination block is SLC block,
     * page offset to be copied is set to number of page of SLC block
     */
    if ((nSrcBlkType == BML_SLC_BLK) || (nDstBlkType == BML_SLC_BLK))
    {
        nNumOfPages = pstVol->nNumOfPgsInSLCBlk;
    }

    *bReadFlag  = FALSE32;
    *bUCReadErr = FALSE32;

    FSR_OAM_MEMCPY(&stSBuf, pstRsv->pSBuf, sizeof(FSRSpareBuf));
    stSBuf.nNumOfMetaExt = pstVol->nSizeOfPage / FSR_PAGE_SIZE_PER_SPARE_BUF_EXT;

    for (nPgIdx = 0; nPgIdx < nNumOfPages; nPgIdx++)
    {
        /* read one page from source block */
        nLLDRe = _LLDRead(pstVol,               /* Volume Number        */
                          nPDev,                /* PDev Number          */
                          nSrcPbn,              /* source pbn           */
                          nPgIdx,               /* page offset          */
                          pstRsv,               /* Reservoir pointer    */
                          pstRsv->pMBuf,        /* main buffer pointer  */
                          &stSBuf,              /* spare buffer pointer */
                          BML_USER_DATA,        /* data type            */
                          FALSE32,              /* nomal load           */
                          FSR_LLD_FLAG_ECC_ON); /* Operation Flag       */

        bFreeMBuf = _CmpData(pstVol, pMBuf, 0xFF, FALSE32);
        bFreeSBuf = _CmpData(pstVol, (UINT8 *) &stSBuf, 0xFF, TRUE32);

        /* if the page is free, skip it */
        if ((bFreeMBuf == TRUE32) && (bFreeSBuf == TRUE32))
        {
            continue;
        }

        nMajorErr = FSR_RETURN_MAJOR(nLLDRe);

        if ((nMajorErr != FSR_LLD_SUCCESS) &&
            (nMajorErr != FSR_LLD_PREV_READ_ERROR) &&
            (nMajorErr != FSR_LLD_PREV_READ_DISTURBANCE))
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Read failure in _CopyBlkRefresh\r\n")));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] (nPDev: %d, nDieIdx: %d, Pbn: %d, nPgIdx: %d, USER DATA)\r\n"), 
                                           pstDev->nDevNo, nDieIdx, nSrcPbn, nPgIdx));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nLLDRe));
            return nLLDRe;
        }

        /* if uncorrectable read error occurs, copy operation is stopped */
        if (nMajorErr == FSR_LLD_PREV_READ_ERROR)
        {
            /*
             * Assumption
             * - Uncorrectable read error may occur only when original block
             *   is copied to REF block. Uncorrectable read error will not occur
             *   when REF block is copied to original block.
             */

            /* if uncorrectable read error occurs, flag is set */
            *bUCReadErr = TRUE32;

            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Uncorrectable read error in _CopyBlkRefresh\r\n")));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] (nPDev: %d, nDieIdx: %d, Pbn: %d, nPgIdx: %d, bReadFlag: %d, bUCReadErr: %d)\r\n"), 
                                           pstDev->nDevNo, nDieIdx, nSrcPbn, nPgIdx, *bReadFlag, *bUCReadErr));

            /* Skip CopyBlkRefresh when uncorrectable read error during run time */
            if ((nFlag & BML_FLAG_NOTICE_READ_ERROR) == BML_FLAG_NOTICE_READ_ERROR) 
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Notice read error in _CopyBlkRefresh\r\n")));
                break;
            }
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] read error occurs in _CopyBlkRefresh. But, continue Erase refresh\r\n")));
        }

        /* if read disturbance occurs, flag is set */
        if ((*bReadFlag == FALSE32) && (nMajorErr == FSR_LLD_PREV_READ_DISTURBANCE))
        {
            *bReadFlag = TRUE32;
        }

        /* set write operation flag */
        nLLDFlag = FSR_LLD_FLAG_1X_PROGRAM | FSR_LLD_FLAG_ECC_ON;

        /* if UC read error occurs, flag set ECC off */
        if (*bUCReadErr == TRUE32)
        {
            nLLDFlag = FSR_LLD_FLAG_1X_PROGRAM | FSR_LLD_FLAG_ECC_OFF;
        }

        /* write data to destination block */
        nLLDRe = _LLDWrite(pstVol,
                           pstDev,
                           nDieIdx,
                           nDstPbn,
                           nPgIdx,
                           pstRsv->pMBuf,
                           &stSBuf,
                           BML_USER_DATA,
                           nLLDFlag);

        if (nLLDRe != FSR_LLD_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Write Failure using _CopyBlkRefresh\r\n")));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] (nPDev: %d, nDieIdx: %d, Pbn: %d, nPgIdx: %d, nFlag: 0x%x, USER DATA)\r\n"), 
                                           pstDev->nDevNo, nDieIdx, nDstPbn, nPgIdx, nFlag));
            return FSR_LLD_PREV_WRITE_ERROR;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FSR_LLD_SUCCESS));

    return FSR_LLD_SUCCESS;
}

/**
 * @brief          This function waits predefined time before power is down
 *
 * @param[in]       none
 *
 * @return          none
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PUBLIC VOID  
FSR_BBM_WaitUntilPowerDn(VOID)
{
    FSR_OAM_WaitNMSec(gnWaitMSecForErError);
}

#if !defined(FSR_NBL2)
/*
 * @brief           This function makes the Reservoir depending on type of device
 *
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]      *pstDev      : device context pointer
 * @param[in]      *pstPart     : pointer for partition info
 *
 * @return          FSR_BML_SUCCESS
 * @return          FSR_BML_CRITICAL_ERROR
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PUBLIC INT32
FSR_BBM_Format(BmlVolCxt   *pstVol,
               BmlDevCxt   *pstDev,
               FSRPartI    *pstPart)
{
    INT32       nRet = FSR_BML_SUCCESS;
    UINT32      nDieIdx;

    FSR_STACK_VAR;

    FSR_STACK_END;
    
    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(PDev: %d)\r\n"),
                                    __FSR_FUNC__, pstDev->nDevNo));

    FSR_ASSERT((pstVol != NULL) && (pstDev != NULL));

    do
    {
        /* sort partition entries and calculate SLC boundary before initial format */
        nRet = _CalcRsvrSize(pstVol, pstDev, pstPart, TRUE32);
        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _CalcRsvrSize(nFlag: 0x%x) is failed\r\n"), TRUE32));
            break;
        }

        /* initialize memory and set variables for Reservoir */
        _InitBBM(pstVol, pstDev);

        /* In case of DDP, do BBM format for each die */
        for (nDieIdx = 0; nDieIdx < pstVol->nNumOfDieInDev; nDieIdx++)
        {
            /* do format for each die */
            nRet = _Format(pstVol, pstDev, pstPart, nDieIdx);
            if (nRet != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _Format(nPDev: %d, nDie:%d) is failed\r\n"), 
                                                 pstDev->nDevNo, nDieIdx));
                break;
            }
        }

        if (nRet != FSR_BML_SUCCESS)
        {
            break;
        }

        /* Set volume context */
        _SetVolCxt(pstVol, pstDev);

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nRet));

    return nRet;
}
#endif /* !defined(FSR_NBL2) */

#if !defined(FSR_NBL2)
/*
 * @brief           This function makes repartitioning
 *
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]      *pstDev      : device context pointer
 * @param[in]      *pstPart     : pointer for partition info
 *
 * @return          FSR_BML_SUCCESS
 * @return          FSR_BML_CRITICAL_ERROR
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PUBLIC INT32
FSR_BBM_Repartition(BmlVolCxt  *pstVol,
                    BmlDevCxt  *pstDev,
                    FSRPartI   *pstPart)
{    
    UINT32      nDieIdx;
    UINT32      nOldLastSLCPbn;
    UINT32      nNewLastSLCPbn;
    INT32       nBBMRe = FSR_BML_SUCCESS;
    INT32       nRet;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(PDev: %d)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo));

    FSR_ASSERT((pstVol != NULL) && (pstDev != NULL));

    do
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_INF, (TEXT("[BBM:   ]   Repartition\r\n")));

        /* check whether the type of each partition of both old and new partition information is different */
        /* The attribute of changed partition can not be changed by re-partition */
        nRet  = _CheckRangeOfPartition(pstVol,pstVol->pstPartI,pstPart);
        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _CheckRangeOfPartition() fails!\r\n")));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nRet));
            return nRet;
        }

        /* 
         * repartition should compare new partition info 
         * with previous partition info for Flex-OneNAND
         * If last SLC pbn of new partition info is different with 
         * current last SLC pbn, it occurs error.
         * because PI info can not be changed by repartition operation
         */
        if (pstVol->nNANDType == FSR_LLD_FLEX_ONENAND)
        {
            /* sort partition info and calculate the last SLC pbn of previous partition info */
            nRet = _CheckPartInfo(pstVol, pstVol->pstPartI, &nOldLastSLCPbn);
            if (nRet != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _CheckPartInfo() fails!\r\n")));    
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nRet));
                return nRet;
            }

            /* sort partition info and calculate the last SLC pbn of new partition info */
            nRet = _CheckPartInfo(pstVol, pstPart, &nNewLastSLCPbn);
            if (nRet != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _CheckPartInfo() fails!\r\n")));    
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nRet));
                return nRet;
            }

            if (nOldLastSLCPbn != nNewLastSLCPbn)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] last SLC pbn of new partition info(%d) is different with\r\n"),
                                               nOldLastSLCPbn));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] previous last SLC pbn(%d) of current partition info\r\n"),
                                               nNewLastSLCPbn));
                nBBMRe = FSR_BML_CRITICAL_ERROR;
                break;
            }
        }

        /* In case of DDP, do BBM repartition for each die */
        for (nDieIdx = 0; nDieIdx < pstVol->nNumOfDieInDev; nDieIdx++)
        {
            /* UnLock Reservoir Area */
            nRet = FSR_BBM_UnlockRsvr(pstVol, pstDev, nDieIdx);
            if (nRet != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   FSR_BBM_UnlockRsvr(nPDev: %d, nDieIdx: %d)\r\n"),
                                                pstDev->nDevNo, nDieIdx));
                nBBMRe = FSR_BML_CANT_UNLOCK_BLOCK;
                break ;
            }

            nRet = _MakeNewPCB(pstDev, pstVol, nDieIdx, pstPart, FALSE32);
            if (nRet != FSR_BML_SUCCESS)
            {
                nBBMRe = nRet;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] making new PCB for repartition is failed\r\n")));
                break;
            }
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nBBMRe));

    return nBBMRe;
}
#endif /* !defined(FSR_NBL2) */

#if !defined(FSR_NBL2)
/**
 * @brief          This function updates PIExt of PIA in UPCB
 *
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]      *pstDev      : device context pointer
 * @param[in]      *pstPExt     : PI extension pointer
 *
 * @return          FSR_BML_SUCCESS  
 * @return          FSR_BML_UPDATE_PIEXT_FAILURE
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PUBLIC INT32 
FSR_BBM_UpdatePIExt(BmlVolCxt   *pstVol,
                    BmlDevCxt   *pstDev,
                    FSRPIExt    *pstPExt)
{
    UINT32        nDieIdx;
    INT32         nRet   = FSR_BML_SUCCESS;
    INT32         nBBMRe = FSR_BML_SUCCESS;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_ASSERT((pstDev != NULL) && (pstVol != NULL));

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(PDev: %d)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo));

    /* update data to die 0 (only first die in a device) */
    nDieIdx = 0;

    nRet = _UpdateUPCB(pstDev, pstVol, pstPExt, nDieIdx, BML_TYPE_UPDATE_PIEXT);
    if (nRet != FSR_BML_SUCCESS)
    {
        nBBMRe = FSR_BML_UPDATE_PIEXT_FAILURE;
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _UpdateUPCB(nDev:%d, nDie:%d, nUpdateType: 0x%x) is failed\r\n"), 
                                       pstDev->nDevNo, nDieIdx, BML_TYPE_UPDATE_PIEXT));
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nBBMRe));

    return nBBMRe;
}
#endif /* !defined(FSR_NBL2) */

#if !defined(FSR_NBL2)
/*
 * @brief           This function makes Pool Control Area to managing bad blocks.
 * @n               Internally,
 * @n               1. constructs BMF based on initial bad blocks
 * @n               2. writes BMI with PI into PCB
 *
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]      *pstDev      : device context pointer
 * @param[in]      *pstPart     : pointer for partition info
 * @param[in]       nDieIdx     : index of die
 * @param[in]       nNANDType : flag to identify the Flex-OneNAND
 *
 * @return          FSR_BML_SUCCESS  
 * @return          FSR_BML_NO_RSV_BLK_POOL
 * @return          FSR_BML_MAKE_NEW_PCB_FAILURE
 * @return          FSR_BML_CRITICAL_ERROR  
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE INT32
_Format(BmlVolCxt      *pstVol,
        BmlDevCxt      *pstDev,
        FSRPartI       *pstPart,
        UINT32          nDieIdx)
{
    UINT32          nPbn;
    UINT32          nErrPbn         = 0;
    UINT32          nBlkType;
    UINT32          nNumOfSLCInitBad = 0;
    UINT32          nNumOfMLCInitBad = 0;
    UINT32          nNumOfSLCInitBadInRsv = 0;
    UINT32          nNumOfMLCInitBadInRsv = 0;
    UINT32          nNumOfMinRunBadHand = 0;
    UINT32          nNumOfCurRunBadHand = 0;
    UINT32          nNumOfMinSLCRunBadHand = 0;
    UINT32          nNumOfCurSLCRunBadHand = 0;
    UINT32          nNumOfMinMLCRunBadHand = 0;
    UINT32          nNumOfCurMLCRunBadHand = 0;
    UINT32          nAdditionalRsv = 0;
    UINT32          nUsableBlks    = 0;
    UINT32          nNumOfRsvBlksInDie;
    UINT32          nByteRet;
    UINT32          nShiftCnt;
    UINT32          nFreePbn;
    BOOL32          bPaired;
    BOOL32          bRet   = TRUE32;
    BOOL32          bMoved = FALSE32;
    INT32           nRet = FSR_BML_SUCCESS;
    INT32           nLLDRe;
    INT32           nMajorRe;
    BmlReservoir   *pstRsv;
    BmlReservoirSh *pstRsvSh;
    LLDPIArg        stLLDPIArg;
    LLDProtectionArg stLLDIO;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(PDev: %d, nDieIdx: %d)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nDieIdx));

    do
    {
        pstRsv      = pstDev->pstDie[nDieIdx]->pstRsv;
        pstRsvSh    = pstDev->pstDie[nDieIdx]->pstRsvSh;

        /* all blocks in Reservoir should be erased except initial bad blocks */
        for (nPbn = pstRsv->n1stSbnOfRsvr; nPbn <= pstRsv->nLastSbnOfRsvr; nPbn++)
        {           
            /* if this block is initial bad block, skip */
            nLLDRe = pstVol->LLD_ChkBadBlk(pstDev->nDevNo, nPbn, FSR_LLD_FLAG_1X_CHK_BADBLOCK);
            
            /* major return : good or bad block */
            nMajorRe = FSR_RETURN_MAJOR(nLLDRe);

            if (nMajorRe == FSR_LLD_INIT_BADBLOCK)
            {
                /* SLC + MLC reservoir */
                if (pstRsv->nRsvrType == BML_HYBRID_RESERVOIR)
                {
                    /* increase number of init bad blocks in SLC reservoir */
                    if (nPbn < pstRsv->n1stSbnOfMLC)
                    {
                        nNumOfSLCInitBadInRsv++;
                    }
                    /* increase number of init bad blocks in MLC reservoir */
                    else
                    {
                        nNumOfMLCInitBadInRsv++;
                    }
                }

                /* construct pBABitMap using initial bad blocks */
                _SetAllocRB(pstRsv, nPbn);
                continue;
            }
            else // unlock if the block is not initial-bad-block
            {
                /* Set the elements of stLLDIO*/
                stLLDIO.nStartBlk = nPbn;
                stLLDIO.nBlks     = 1;

                nLLDRe = pstVol->LLD_IOCtl(pstDev->nDevNo,
                                           FSR_LLD_IOCTL_UNLOCK_BLOCK,
                                           (UINT8 *) &stLLDIO,
                                           sizeof(stLLDIO),
                                           (UINT8 *) &nErrPbn,
                                           sizeof(nErrPbn),
                                           &nByteRet);
                if (nLLDRe != FSR_LLD_SUCCESS)
                {
                    nRet = FSR_BML_CANT_UNLOCK_BLOCK;
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] unlock block failed (nPbn: %d)\r\n"), nPbn));
                    break;
                }
            }

            nLLDRe = _LLDErase(pstVol, pstDev->nDevNo, &nPbn, FSR_LLD_FLAG_1X_ERASE);
            if (nLLDRe != FSR_LLD_SUCCESS)
            {
                /* SLC + MLC reservoir */
                if (pstRsv->nRsvrType == BML_HYBRID_RESERVOIR)
                {
                    /* increase number of init bad blocks in SLC reservoir */
                    if (nPbn < pstRsv->n1stSbnOfMLC)
                    {
                        nNumOfSLCInitBadInRsv++;
                    }
                    /* increase number of init bad blocks in MLC reservoir */
                    else
                    {
                        nNumOfMLCInitBadInRsv++;
                    }
                }

                /* 
                 * construct pBABitMap for run time bad blocks 
                 * this run time bad block is regarded as initial bad block
                 */
                _SetAllocRB(pstRsv, nPbn);
                continue;
            }
        }

        if (nRet != FSR_BML_SUCCESS)
        {
            break;
        }

        /* numner of Reservoir blocks in a die */
        nNumOfRsvBlksInDie = pstVol->nNumOfRsvrBlks >> pstVol->nSftDDP;

        /* Allocate BBM meta blocks except TPCB */

        /* Set type of REF block depending of Reservoir type */
        if ((pstRsv->nRsvrType & BML_MLC_RESERVOIR) == BML_MLC_RESERVOIR)
        {
            /* MLC only or Hybrid (SLC+MLC) Reservoir */
            nBlkType = BML_MLC_BLK;
        }
        else
        {
            /* SLC only Reservoir */
            nBlkType = BML_SLC_BLK;
        }

        do
        {
            /* Allocate REF block */
            bRet = _GetFreeRB(pstRsv,  
                              pstVol->nNumOfPlane,
                              nBlkType,
                              BML_META_DATA,
                              1,
                              (UINT32 *) &nFreePbn,
                              &bPaired);

            /* 
             * If allocation is failed in hybrid reservoir, 
             * move boundary value to allocate an unsed block 
             */
            if ((bRet != TRUE32) && (pstRsv->nRsvrType == BML_HYBRID_RESERVOIR))
            {
                bMoved = _MoveRsvBoundary(pstDev, pstRsv, nDieIdx, nBlkType);
                
                /* can not shift boundary value between SLC and MLC */
                if (bMoved != TRUE32)
                {
                    break;
                }
            }
            /* allocation is completed successfully or not BML_HYBRID_RESERVOIR */
            else
            {
                break;
            }
            
        } while ((bRet != TRUE32) && (pstRsv->nRsvrType == BML_HYBRID_RESERVOIR));

        /* REF allocation failure */
        if (bRet == FALSE32)
        {
            nRet = FSR_BML_NO_RSV_BLK_POOL;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] REF block allocation is failed(nBlkType: %d)\r\n"), nBlkType));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Current # of BMF:%d, n1stSbnOfRsvr:%d, nLastSbnOfRsvr: %d\r\n"), 
                                           pstRsv->pstBMI->nNumOfBMFs, pstRsv->n1stSbnOfRsvr, pstRsv->nLastSbnOfRsvr));
            break;
        }

        pstRsvSh->nREFSbn = (UINT16) nFreePbn;

        /* Set type of PCB block depending of Reservoir type */
        if (pstRsv->nRsvrType == BML_MLC_RESERVOIR)
        {
            /* MLC only Reservoir */
            nBlkType = BML_MLC_BLK;
        }
        else
        {
            /* SLC only Reservoir or Hybrid (SLC+MLC) Reservoir */
            nBlkType = BML_SLC_BLK;
        }

        do
        {
            /* Allocate LPCB */
            bRet = _GetFreeRB(pstRsv,
                              pstVol->nNumOfPlane,
                              nBlkType,
                              BML_META_DATA,
                              1,
                              (UINT32 *) &nFreePbn,
                              &bPaired);
            /* 
             * If allocation is failed in hybrid reservoir, 
             * move boundary value to allocate an unsed block 
             */
            if ((bRet != TRUE32) && (pstRsv->nRsvrType == BML_HYBRID_RESERVOIR))
            {
                bMoved = _MoveRsvBoundary(pstDev, pstRsv, nDieIdx, nBlkType);
                
                /* can not shift boundary value between SLC and MLC */
                if (bMoved != TRUE32)
                {
                    break;
                }
            }
            /* allocation is completed successfully or not BML_HYBRID_RESERVOIR */
            else
            {
                break;
            }

        } while ((bRet != TRUE32) && (pstRsv->nRsvrType == BML_HYBRID_RESERVOIR));

        /* LPCB allocation failure */
        if (bRet == FALSE32)
        {
            nRet = FSR_BML_NO_RSV_BLK_POOL;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] LPCB block allocation is failed(nBlkType: %d)\r\n"), nBlkType));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Current # of BMF:%d, n1stSbnOfRsvr:%d, nLastSbnOfRsvr: %d\r\n"), 
                                           pstRsv->pstBMI->nNumOfBMFs, pstRsv->n1stSbnOfRsvr, pstRsv->nLastSbnOfRsvr));
            break;
        }

        pstRsvSh->nLPCBSbn = (UINT16) nFreePbn;

        do
        {
            /* Allocate UPCB */
            bRet = _GetFreeRB(pstRsv,
                              pstVol->nNumOfPlane,
                              nBlkType,
                              BML_META_DATA,
                              1,
                              (UINT32 *) &nFreePbn,
                              &bPaired);

            /* 
             * If allocation is failed in hybrid reservoir, 
             * move boundary value to allocate an unsed block 
             */
            if ((bRet != TRUE32) && (pstRsv->nRsvrType == BML_HYBRID_RESERVOIR))
            {
                bMoved = _MoveRsvBoundary(pstDev, pstRsv, nDieIdx, nBlkType);

                /* can not shift boundary value between SLC and MLC */
                if (bMoved != TRUE32)
                {
                    break;
                }
            }
            /* allocation is completed successfully or not BML_HYBRID_RESERVOIR */
            else
            {
                break;
            }

        } while ((bRet != TRUE32) && (pstRsv->nRsvrType == BML_HYBRID_RESERVOIR));

        /* UPCB allocation failure */
        if (bRet == FALSE32)
        {
            nRet = FSR_BML_NO_RSV_BLK_POOL;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] UPCB block allocation is failed(nBlkType: %d)\r\n"), nBlkType));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Current # of BMF:%d, n1stSbnOfRsvr:%d, nLastSbnOfRsvr: %d\r\n"), 
                                           pstRsv->pstBMI->nNumOfBMFs, pstRsv->n1stSbnOfRsvr, pstRsv->nLastSbnOfRsvr));
            break;
        }

        pstRsvSh->nUPCBSbn = (UINT16) nFreePbn;

        do
        {
            /* Allocate TPCB */
            bRet = _GetFreeRB(pstRsv,
                              pstVol->nNumOfPlane,
                              nBlkType,
                              BML_META_DATA,
                              1,
                              (UINT32 *) &nFreePbn,
                              &bPaired);


            /* 
             * If allocation is failed in hybrid reservoir, 
             * move boundary value to allocate an unsed block 
             */
            if ((bRet != TRUE32) && (pstRsv->nRsvrType == BML_HYBRID_RESERVOIR))
            {
                bMoved = _MoveRsvBoundary(pstDev, pstRsv, nDieIdx, nBlkType);

                /* can not shift boundary value between SLC and MLC */
                if (bMoved != TRUE32)
                {
                    break;
                }
            }
            /* allocation is completed successfully or not BML_HYBRID_RESERVOIR */
            else
            {
                break;
            }

        } while ((bRet != TRUE32) && (pstRsv->nRsvrType == BML_HYBRID_RESERVOIR));

        /* TPCB allocation failure */
        if (bRet == FALSE32)
        {
            nRet = FSR_BML_NO_RSV_BLK_POOL;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] TPCB block allocation is failed(nBlkType: %d)\r\n"), nBlkType));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Current # of BMF:%d, n1stSbnOfRsvr:%d, nLastSbnOfRsvr: %d\r\n"), 
                                           pstRsv->pstBMI->nNumOfBMFs, pstRsv->n1stSbnOfRsvr, pstRsv->nLastSbnOfRsvr));
            break;
        }

        pstRsvSh->nTPCBSbn = (UINT16) nFreePbn;

        /* construct block map field based on initial bad block */
        bRet= _ConstructBMF(pstDev, pstVol, nDieIdx, &nNumOfSLCInitBad, &nNumOfMLCInitBad);
        if (bRet == FALSE32)
        {
            nRet = FSR_BML_CONSTRUCT_BMF_FAILURE;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] BMF construction is failed(nPDev: %d, nDieIdx: %d)\r\n"), 
                                           pstDev->nDevNo, nDieIdx));
            break;
        }

        /* number of initial bad blocks in a die > 2% of number of blocks in a die : error case */
        if ((nNumOfSLCInitBadInRsv + nNumOfMLCInitBadInRsv + nNumOfSLCInitBad + nNumOfMLCInitBad) > (pstVol->nNumOfBlksInDie / 50))
        {
            nRet = FSR_BML_TOO_MUCH_INIT_BAD_DEVICE;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Number of initial bad blocks (%d) in a die is larger than %d\n"),
                                           (nNumOfSLCInitBad + nNumOfMLCInitBad), (pstVol->nNumOfBlksInDie / 50)));
            break;
        }

        /* If the device is flex-OneNAND, recalculate size of Reservoir */
        if (pstVol->nNANDType == FSR_LLD_FLEX_ONENAND)
        {
            /* adjust size of Reservoir after scanning initial bad blocks */
            
            FSR_ASSERT((nNumOfSLCInitBad <= pstRsv->nNumOfRsvrInSLC) || 
                       (nNumOfMLCInitBad <= pstRsv->nNumOfRsvrInMLC));

            if (pstRsv->nRsvrType == BML_HYBRID_RESERVOIR)
            {
                FSR_ASSERT((pstRsv->nNumOfRsvrInSLC != 0) && 
                           (pstRsv->nNumOfRsvrInMLC != 0));

                /* Check to have enough space for RunTimeBadBlkHandling */
                if (pstVol->nNumOfBlksInDie % 200 == 0)
                {
                    nNumOfMinRunBadHand = pstVol->nNumOfBlksInDie / 200;
                }
                else
                {
                    nNumOfMinRunBadHand = (pstVol->nNumOfBlksInDie / 200) + 1;
                }

                nNumOfCurRunBadHand = (pstRsv->nNumOfRsvrInMLC - (nNumOfMLCInitBad + nNumOfMLCInitBadInRsv)) +
                                        (pstRsv->nNumOfRsvrInSLC - (nNumOfSLCInitBad + nNumOfSLCInitBadInRsv));

                if (nNumOfCurRunBadHand < nNumOfMinRunBadHand)
                {
                    nRet = FSR_BML_NO_RSV_BLK_POOL;
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Allocating additional Reservoir for run time bad is failed\r\n")));
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] nNumOfCurRunBadHand :%d, nNumOfMinRunBadHand:%d\r\n"), 
                                   nNumOfCurRunBadHand, pstRsv->nNumOfRsvrInMLC, nNumOfMinRunBadHand ));
                    break;
                }

                /* If SLC Reservoir for RunTimeBadBlk is not enough  */
                nUsableBlks = pstDev->nNumOfSLCBlksInDie[nDieIdx] - pstRsv->nNumOfRsvrInSLC - BML_RSV_PCB_BLKS;

                /* 0.5% of SLC User Block   */
                if (nUsableBlks % 200 == 0)
                {
                    nNumOfMinSLCRunBadHand = nUsableBlks / 200;
                }
                else
                {
                    nNumOfMinSLCRunBadHand = (nUsableBlks / 200) + 1;
                }

                /* number of block alocated for runtime bad block handling of SLC  */
                nNumOfCurSLCRunBadHand = pstRsv->nNumOfRsvrInSLC - (nNumOfSLCInitBad + nNumOfSLCInitBadInRsv);

                /* compare number of corrent block with minimum of block for runtime bad block handling */
                if (nNumOfCurSLCRunBadHand < nNumOfMinSLCRunBadHand)
                {
                    nShiftCnt     = 0;

                    /* scanning from last SLC block in Reservoir */
                    nPbn = pstRsv->n1stSbnOfMLC;

                    nAdditionalRsv = nNumOfMinSLCRunBadHand - nNumOfCurSLCRunBadHand;

                    while (nAdditionalRsv > 0)
                    {
                        bRet = _IsAllocRB(pstRsv,nPbn);
                        if (bRet == FALSE32)
                        {
                            nAdditionalRsv--;
                        }

                        /* shift to left */
                        nShiftCnt++;

                        /* move to previous block */
                        nPbn++;

                    }

                    /* SLC Reservoir takes free blocks away from MLC Reservoir */
                    pstRsv->nNumOfRsvrInSLC += nShiftCnt;
                    pstRsv->nNumOfRsvrInMLC = nNumOfRsvBlksInDie - pstRsv->nNumOfRsvrInSLC;

                    /* add newly allocated SLC blocks */
                    pstDev->nNumOfSLCBlksInDie[nDieIdx] += nShiftCnt;
                }


                /* If MLC Reservoir for RunTimeBadBlk is not enough  */
                nUsableBlks = pstVol->nNumOfBlksInDie - pstDev->nNumOfSLCBlksInDie[nDieIdx] - pstRsv->nNumOfRsvrInMLC - BML_RSV_REF_BLKS;

                if (nUsableBlks % 200 == 0)
                {
                    nNumOfMinMLCRunBadHand = nUsableBlks / 200;
                }
                else
                {
                    nNumOfMinMLCRunBadHand = (nUsableBlks / 200) + 1;
                }

                nNumOfCurMLCRunBadHand = pstRsv->nNumOfRsvrInMLC - (nNumOfMLCInitBad + nNumOfMLCInitBadInRsv);

                if (nNumOfCurMLCRunBadHand < nNumOfMinMLCRunBadHand)
                {
                    nShiftCnt     = 0;

                    /* scanning from last SLC block in Reservoir */
                    nPbn = pstRsv->n1stSbnOfMLC - 1;
                    
                    nAdditionalRsv = nNumOfMinMLCRunBadHand - nNumOfCurMLCRunBadHand;

                    while (nAdditionalRsv > 0)
                    {
                        bRet = _IsAllocRB(pstRsv,nPbn);
                        if (bRet == FALSE32)
                        {
                            nAdditionalRsv--;
                        }

                        /* shift to left */
                        nShiftCnt++;

                        /* move to previous block */
                        nPbn--;

                    }

                    /* MLC Reservoir takes free blocks away from SLC Reservoir */
                    pstRsv->nNumOfRsvrInMLC += nShiftCnt;
                    pstRsv->nNumOfRsvrInSLC = nNumOfRsvBlksInDie - pstRsv->nNumOfRsvrInMLC;

                    /* new numbers of SLC block */
                    pstDev->nNumOfSLCBlksInDie[nDieIdx] -= nShiftCnt;
                }
            }

            /* read PI value to check its lock state */
            nLLDRe = pstVol->LLD_IOCtl(pstDev->nDevNo,
                                       FSR_LLD_IOCTL_PI_READ,
                                       (UINT8 *)&nDieIdx,
                                       sizeof(nDieIdx),
                                       (UINT8 *)&(stLLDPIArg),
                                       sizeof(stLLDPIArg),
                                       &nByteRet);
            if (nLLDRe != FSR_LLD_SUCCESS)
            {
               nRet = FSR_BML_CRITICAL_ERROR;
               FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] LLD_IOCtl(nCode:FSR_LLD_IOCTL_PI_READ) fails!(Dev:%d, Die:%d)\r\n"), 
                                                pstDev->nDevNo, nDieIdx));
               FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nRet));
               return nRet;
            }

            /* If PI is locked and new PI value is different with previous one */
            if (stLLDPIArg.nPILockValue == FSR_LLD_IOCTL_LOCK_PI)
            {
                if (stLLDPIArg.nEndOfSLC != (pstDev->nNumOfSLCBlksInDie[nDieIdx] - 1 + (nDieIdx * pstVol->nNumOfBlksInDie)))
                {
                    nRet = FSR_BML_PI_ALREADY_LOCKED;
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] New PI value(%d) is different with previous PI value(%d)(nPDev: %d, nDieIdx: %d)\r\n"),                                                     
                                                   pstDev->nNumOfSLCBlksInDie[nDieIdx] - 1 + (nDieIdx * pstVol->nNumOfBlksInDie),
                                                   stLLDPIArg.nEndOfSLC,
                                                   pstDev->nDevNo, 
                                                   nDieIdx));
                    break;
                }
            }
            /* If PI is not locked */
            else
            {
                /* Write last SLC pbn to PI */
                nRet = _WritePI(pstVol, pstDev, nDieIdx);
                if (nRet != FSR_BML_SUCCESS)
                {   
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _WritePI(nPDev: %d, nDieIdx: %d) is failed\r\n"), 
                                                   pstDev->nDevNo, nDieIdx));
                    break;
                }
            }
        }

        /* ignore its return value */
        _LLDVerify(pstVol, pstDev, nDieIdx);

        /* 
         * partition info should be copied to reference the partition info
         * without additional parameter passing
         */
        FSR_OAM_MEMCPY(pstVol->pstPartI, pstPart, sizeof(FSRPartI));

        nRet = _MakeNewPCB(pstDev, pstVol, nDieIdx, pstPart, TRUE32);
        if (nRet != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] PCB block creation is failed(nPDev: %d, nDieIdx: %d, LPCB: %d, UPCB: %d)\r\n"),
                                           pstDev->nDevNo, nDieIdx, pstRsvSh->nLPCBSbn, pstRsvSh->nUPCBSbn));
            break;
        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nRet));

    return nRet;
}
#endif /* !defined(FSR_NBL2) */

#if !defined(FSR_NBL2)
/*
 * @brief           This function constructs BMF based on initial bad blocks
 *
 * @param[in]      *pstDev            : device context pointer
 * @param[in]      *pstVol            : volume context pointer
 * @param[in]       nDieIdx           : index of die
 * @param[out]     *pnNumOfSLCInitBad : number of initial bad blocks in SLC area
 * @param[out]     *pnNumOfSLCInitBad : number of initial bad blocks in MLC area
 *
 * @return          TRUE32
 * @return          FALSE32
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE BOOL32
_ConstructBMF(BmlDevCxt    *pstDev,
              BmlVolCxt    *pstVol,
              UINT32        nDieIdx,
              UINT32       *pnNumOfSLCInitBad,
              UINT32       *pnNumOfMLCInitBad)
{
    BmlReservoir   *pstRsv;
    BmlBMF          stBMF;
    INT32           nLLDRe;
    INT32           nMajorRe;
    INT32           nMinorRe;
    UINT32          nPbn;
    UINT32          nBMFIdx;
    UINT32          nNumOfBlksInRsv;
    UINT32          nNumOfPlane;
    UINT32          n1stSLCBlk  = BML_INVALID;
    UINT32          nLastSLCBlk = BML_INVALID;
    UINT32          n1stMLCBlk  = BML_INVALID;
    UINT32          nLastMLCBlk = BML_INVALID;
    UINT32          n1stUsrBlk;
    UINT32          nLastUsrBlk;
    UINT32          nLLDFlag;
    UINT32          nNumOfInitBadBlks;
    UINT32          nOrgMinorRe;
    UINT32          nBlkType;
    UINT32          nCnt;
    UINT32          nReplacedPbn;
    UINT32          nPlnIdx;
    UINT32          nFreePbn[FSR_MAX_PLANES];
    UINT32          nCurrentRsvr;
    UINT32          nCandidate = BML_INVALID;
    BOOL32          bAligned1stBlk;
    BOOL32          bAlignedLastBlk;
    BOOL32          bPaired;
    BOOL32          bRet   = TRUE32;
    BOOL32          bMoved = FALSE32;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(nPDev: %d, nDieIdx: %d)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nDieIdx));

    pstRsv      = pstDev->pstDie[nDieIdx]->pstRsv;
    nNumOfPlane = pstVol->nNumOfPlane;

    /* set number of blocks in Reservoir (except meta blocks) */
    nNumOfBlksInRsv = pstRsv->nNumOfRsvrInSLC + pstRsv->nNumOfRsvrInMLC;

    /* set range of SLC and MLC area before scanning */

    /* SLC+MLC Flex-OneNAND */
    if (pstRsv->nRsvrType == BML_HYBRID_RESERVOIR)
    {
        n1stSLCBlk  = pstVol->nNumOfBlksInDie * nDieIdx;
        nLastSLCBlk = n1stSLCBlk + pstDev->nNumOfSLCBlksInDie[nDieIdx] - pstRsv->nNumOfRsvrInSLC - BML_RSV_PCB_BLKS - 1;
        n1stMLCBlk  = n1stSLCBlk + pstDev->nNumOfSLCBlksInDie[nDieIdx] + pstRsv->nNumOfRsvrInMLC + BML_RSV_REF_BLKS;
        nLastMLCBlk = pstVol->nNumOfBlksInDie * (nDieIdx + 1) - 1;
    }
    /* SLC device or SLC only Flex-OneNAND */
    else if (pstRsv->nRsvrType == BML_SLC_RESERVOIR)
    {
        n1stSLCBlk  = pstVol->nNumOfBlksInDie * nDieIdx;
        nLastSLCBlk = pstVol->nNumOfBlksInDie * (nDieIdx + 1) - nNumOfBlksInRsv - BML_RSV_META_BLKS - 1;
    }
    /* MLC device or MLC only Flex-OneNAND */
    else
    {
        /* 
         * Flex-OneNAND has only one SLC block 
         * MLC device has no SLC block
         */
        n1stMLCBlk  = pstDev->nNumOfSLCBlksInDie[nDieIdx] + pstVol->nNumOfBlksInDie * nDieIdx;
        nLastMLCBlk = pstVol->nNumOfBlksInDie * (nDieIdx + 1)  - nNumOfBlksInRsv - BML_RSV_META_BLKS - 1;
    }

    *pnNumOfSLCInitBad = 0;
    *pnNumOfMLCInitBad = 0;
    nBMFIdx            = 0;
    
    /* observe SLC reservoir and MLC reservoir sequentially */
    for (nCurrentRsvr = BML_SLC_RESERVOIR; nCurrentRsvr <= BML_MLC_RESERVOIR; nCurrentRsvr++)
    {
        nLLDFlag        = FSR_LLD_FLAG_1X_CHK_BADBLOCK;

        bAligned1stBlk  = TRUE32;
        bAlignedLastBlk = TRUE32;

        /* If the reservoir has identical type reservoir with current checking type */
        if (pstRsv->nRsvrType & nCurrentRsvr)
        {
            /* SLC device or hybrid type Flex-OneNAND: Reservoir block type = SLC */
            if ((nCurrentRsvr & BML_SLC_RESERVOIR) == BML_SLC_BLK)
            {
                n1stUsrBlk  = n1stSLCBlk;
                nLastUsrBlk = nLastSLCBlk;
                nBlkType    = BML_SLC_BLK;
            }
            /* MLC device or hybrid type Flex-OneNAND: Reservoir block type = MLC */            
            else
            {
                n1stUsrBlk  = n1stMLCBlk;
                nLastUsrBlk = nLastMLCBlk;
                nBlkType    = BML_MLC_BLK;
            }

            /* multi-plane device */
            if (pstVol->nNumOfPlane > 1)
            {
                /* first block is not in plane0 */
                 if (n1stUsrBlk & 0x1)
                 {
                     /* plane0 block is included */
                     n1stUsrBlk--;
                     bAligned1stBlk = FALSE32;
                 }

                 /* last block is not in plane1 */
                 if (!(nLastUsrBlk & 0x1))
                 {
                    /* plane1 block is included */
                    nLastUsrBlk++;
                    bAlignedLastBlk = FALSE32;
                 }

                 nLLDFlag = FSR_LLD_FLAG_2X_CHK_BADBLOCK;
            }

            for (nPbn = n1stUsrBlk; nPbn <= nLastUsrBlk; nPbn += nNumOfPlane)
            {
                /* check each blocks in the Reservoir to check initial bad block */
                nLLDRe = pstVol->LLD_ChkBadBlk(pstDev->nDevNo, nPbn, nLLDFlag);
                
                /* major return : good or bad block */
                nMajorRe = FSR_RETURN_MAJOR(nLLDRe);

                if (nMajorRe == FSR_LLD_INIT_GOODBLOCK)
                {
                    continue;
                }

                /* minor return : bad block position (plane0 or plane1) */
                nMinorRe = FSR_RETURN_MINOR(nLLDRe);
                
                nNumOfInitBadBlks = 0;
                nOrgMinorRe       = nMinorRe;
                nNumOfPlane       = pstVol->nNumOfPlane;

                for (nCnt = 0; nCnt < nNumOfPlane; nCnt++)
                {
                    if (nMinorRe & 0x1)
                    {
                        /* 
                         * unaligned first block and last block case 
                         * In case of first block, return value of plane0 is discarded
                         * In case of Last block, return value of plane1 is discarded
                         * 
                         *    +-+-+-+-+-+-+
                         * p0 | |*|R|R|R| |   R : reservoir blocks
                         *    +-+-+-+-+-+-+
                         * p1 | |R|R|R|*| |   * : unaligned blocks 
                         *    +-+-+-+-+-+-+
                         * 
                         * Unaligned block and reservoir block is simultaneously checked.
                         * However, result of reservoir block is ignored
                         */
                        if (((nPbn == n1stUsrBlk) && (bAligned1stBlk == FALSE32) && (nCnt == 0)) ||
                            ((nPbn == (nLastUsrBlk - 1)) && (bAlignedLastBlk == FALSE32) && (nCnt == nNumOfPlane - 1)))
                        {
                            /* To avoid replacement for 2 plane in case of misaligned block */
                            nNumOfPlane = 1;
                                  
                            /* move to return value of next plane */
                            nMinorRe >>= 1;
                            continue;
                        }

                        /* increase number of bad blocks */
                        nNumOfInitBadBlks++;
                    }

                    /* move to return value of next plane */
                    nMinorRe >>= 1;
                }

                /* If there is no bad block */
                if (nNumOfInitBadBlks < 1)
                {
                    continue;
                }

                do
                {
                    /* Find free Reservoir block for an initial bad block */
                    bRet = _GetFreeRB(pstRsv, 
                                      nNumOfPlane, 
                                      nBlkType,
                                      BML_USER_DATA,
                                      nNumOfInitBadBlks,
                                      (UINT32 *) &nFreePbn,
                                      &bPaired);

                    /* 
                     * If allocation is failed in hybrid reservoir, 
                     * move boundary value to allocate an unsed block 
                     */
                    if ((bRet != TRUE32) && (pstRsv->nRsvrType == BML_HYBRID_RESERVOIR))
                    {
                        bMoved = _MoveRsvBoundary(pstDev, pstRsv, nDieIdx, nBlkType);
                        
                        /* can not shift boundary value between SLC and MLC */
                        if (bMoved != TRUE32)
                        {
                            break;
                        }
                    }
                    /* allocation is completed successfully or not BML_HYBRID_RESERVOIR */
                    else
                    {
                        break;
                    }

                } while ((bRet != TRUE32) && (pstRsv->nRsvrType == BML_HYBRID_RESERVOIR));

                /* allocation is failed */
                if (bRet == FALSE32)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] can not find free Reservoir block(nBlkType: 0x%x, user data)\r\n"), nBlkType));
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Current # of BMF:%d, n1stSbnOfRsvr:%d, nLastSbnOfRsvr: %d, n1stSbnOfMLC: %d\r\n"), 
                                                   pstRsv->pstBMI->nNumOfBMFs, pstRsv->n1stSbnOfRsvr, pstRsv->nLastSbnOfRsvr, pstRsv->n1stSbnOfMLC));
                    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FALSE32));
                    return FALSE32;
                }

                /* Bad pbn is in the plane 0 (default) */
                nReplacedPbn = nPbn;
                nPlnIdx = BML_1ST_PLN_IDX;

                /* 
                 * Bad pbn is in the plane 1 
                 * condition
                 * 1) multi-plane 
                 * 2) number of initial bad is one 
                 * 3) a block in plane1 is bad block 
                 * -> current pbn(p0) should be moved to the block in plane 1
                 *
                 *  p0  p1
                 * +-----+
                 * |  |B |
                 * +-----+
                 */
                if ((pstVol->nNumOfPlane > 1) &&
                    (bPaired != TRUE32) &&
                    (nNumOfInitBadBlks < pstVol->nNumOfPlane) &&
                    (nOrgMinorRe & FSR_LLD_BAD_BLK_2NDPLN))
                {
                    nReplacedPbn++;
                    nPlnIdx = BML_2ND_PLN_IDX;
                }

                if (nBMFIdx >= nNumOfBlksInRsv)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] invalid BMF index(nBMFIdx: %d, nNumOfBlksInRsv)\r\n"), 
                                                   nBMFIdx, nNumOfBlksInRsv));
                    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FALSE32));
                    return FALSE32;
                }

                nMinorRe = nOrgMinorRe;

                /* Set BMF for replaced block */

                /* Paired replacement for initial bad */
                if ((bPaired == TRUE32) && (pstVol->nNumOfPlane > 1))
                {
                    nCandidate = BML_INVALID;

                    for (nPlnIdx = 0; nPlnIdx < pstVol->nNumOfPlane; nPlnIdx++)
                    {
                        stBMF.nSbn = (UINT16) nReplacedPbn;
                        stBMF.nRbn = (UINT16) nFreePbn[nPlnIdx];

                        /* If the pair consists of one bad block and one valid block */
                        if ((nNumOfInitBadBlks != pstVol->nNumOfPlane) &&
                            !(nMinorRe & 0x1))
                        {
                            /* valid block can be used as reservoir candidate */
                            nCandidate = nReplacedPbn;
                        }

                        /* register replaced block mapping info */
                        bRet = _RegBMF(pstVol, &stBMF, pstRsv, nCandidate);
                        if (bRet != TRUE32)                     
                        {
                            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _RegBMF is failed\r\n")));
                            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FALSE32));
                            return FALSE32;                        
                        }

                        nReplacedPbn++;

                        /* move to return value of next plane */
                        nMinorRe >>= 1;
                        
                        /* reset the value to check next block */
                        nCandidate = BML_INVALID;
                    }
                }
                /* unpaired replacement for initial bad */
                else
                {
                   for (nCnt = 0; nCnt < nNumOfInitBadBlks; nCnt++)
                   {
                        stBMF.nSbn = (UINT16) nReplacedPbn;
                        stBMF.nRbn = (UINT16) nFreePbn[nCnt];

                        /* 
                         * register replaced block mapping info 
                         * In case of unpaired replacement, no reservoir candidate block is created
                         */
                        bRet = _RegBMF(pstVol, &stBMF, pstRsv, BML_INVALID);
                        if (bRet != TRUE32)                     
                        {
                            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _RegBMF is failed\r\n")));
                            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FALSE32));
                            return FALSE32;
                        }

                        nReplacedPbn++;
                    }
                }

                /* 
                 * increase number initial bad blocks in each area
                 * This value is used to judge how many SLC or MLC reservoir blocks 
                 * are exhausted by initial bad blocks
                 * before determining PI value
                 */

                /* If the block is SLC type */
                if (nBlkType & BML_SLC_BLK)
                {
                    (*pnNumOfSLCInitBad) += nNumOfInitBadBlks;
                }
                /* If the block is MLC type */
                else if (nBlkType & BML_MLC_BLK)
                {
                    (*pnNumOfMLCInitBad) += nNumOfInitBadBlks;
                }

            } /* end of for (nPbn) */
        } /* end of if (pstRsv->nRsvrType & nCurrentRsvr) */
    } /* end of for (nCurrentRsvr) */
    

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, TRUE32));

    return TRUE32;
}
#endif /* !defined(FSR_NBL2) */

#if !defined(FSR_NBL2)
/**
 * @brief          this function writes PI value
 *
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]      *pstDev      : device context pointer
 * @param[in]       nDieIdx     : die index
 *
 * @return          FSR_BML_SUCCESS  
 * @return          FSR_BML_MAKE_RSVR_ERROR  
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE INT32
_WritePI(BmlVolCxt *pstVol,
         BmlDevCxt *pstDev,
         UINT32     nDieIdx)
{
    LLDPIArg    stLLDPIArg;
    INT32       nRet = FSR_BML_SUCCESS;
    UINT32      nLastSLCPbnInDie;
    UINT32      nByteRet;

    FSR_STACK_VAR;

    FSR_STACK_END; 

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(PDev: %d, nDieIdx: %d)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nDieIdx));

    /* calculate pbn for each die */
    nLastSLCPbnInDie = pstDev->nNumOfSLCBlksInDie[nDieIdx] - 1 + (nDieIdx * pstVol->nNumOfBlksInDie);

    /* set the parameter of LLD_IOCtl() */
    stLLDPIArg.nEndOfSLC    = (UINT16) nLastSLCPbnInDie;
    stLLDPIArg.nPILockValue = FSR_LLD_IOCTL_UNLOCK_PI;

    /* write PI data */ 
    nRet = pstVol->LLD_IOCtl(pstDev->nDevNo, 
                             FSR_LLD_IOCTL_PI_WRITE,
                             (UINT8 *)&stLLDPIArg,
                             sizeof(stLLDPIArg),
                             NULL,
                             0,
                            &nByteRet);
    if (nRet != FSR_LLD_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] LLD_IOCtl(nCode:FSR_LLD_IOCTL_PI_WRITE) fails!(Dev:%d, Die:%d, Last SLC: %d, PI Lock: 0x%x)\r\n"), 
                                       pstDev->nDevNo, nDieIdx, nLastSLCPbnInDie, FSR_LLD_IOCTL_UNLOCK_PI));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nRet));
        return nRet;
    }
    else
    {
        nRet = FSR_BML_SUCCESS;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nRet));

    return nRet;
}
#endif /* !defined(FSR_NBL2) */

/**
 * @brief          this function writes previous operation data to REF block
 * @n              to save the data for error handling
 *
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]      *pstDev      : device context pointer
 * @param[in]       nDieIdx     : die index
 * @param[in]      *pMBuf       : main buffer pointer
 * @param[in]      *pSBuf       : spare buffer pointer
 * @param[in]       nFlag       : BML_FLAG_NEXT_PREV_DATA or BML_FLAG_PREV_DATA
 *
 * @return          FSR_BML_SUCCESS  
 * @return          FSR_BML_CRITICAL_ERROR  
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PUBLIC INT32
FSR_BBM_BackupPrevData(BmlVolCxt   *pstVol,
                       BmlDevCxt   *pstDev,
                       UINT32       nDieIdx,
                       UINT8       *pMBuf,
                       FSRSpareBuf *pSBuf,
                       UINT32       nFlag)
{
    BmlReservoir   *pstRsv;
    BmlReservoirSh *pstRsvSh;
    UINT32          nREFPbn;
    UINT32          nLLDFlag;
    UINT32          nPlnIdx;
    INT32           nLLDRe = FSR_LLD_SUCCESS;
    INT32           nBBMRe = FSR_BML_SUCCESS;
    INT32           nRet   = FSR_BML_SUCCESS;
    BOOL32          bRet   = TRUE32;
    BOOL32          bPgmErr = FALSE32;
    UINT16          nNewREFSbn;
    FSRSpareBuf     stSBuf;
    FSRSpareBuf    *pTmpSBuf;

    FSR_STACK_VAR;

    FSR_STACK_END; 

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(PDev: %d, nDieIdx: %d, nFlag: 0x%x)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nDieIdx, nFlag));
    do
    {
        pstRsv      = pstDev->pstDie[nDieIdx]->pstRsv;
        pstRsvSh    = pstDev->pstDie[nDieIdx]->pstRsvSh;

        nREFPbn = (UINT32)pstRsvSh->nREFSbn;

        /* erase a REF block before programming previous data */
        if (nFlag == BML_FLAG_PREV_DATA)
        {
            /* Erase REF block before writing the data */
            nLLDRe = _LLDErase(pstVol, pstDev->nDevNo, &nREFPbn, FSR_LLD_FLAG_1X_ERASE);
            
            if (nLLDRe != FSR_LLD_SUCCESS)
            {
                do
                {

                    /* 
                     * Allocate new REF block to rotate the REF block in Reservoir 
                     * if new REF block is not allocated, keep current REF block
                     */
                    bRet = _GetNewREFBlk(pstDev->pstDie[nDieIdx], (UINT16 *) &(nNewREFSbn));
                    if (bRet == TRUE32)
                    {
                        /* allocate new REF block */
                        pstRsvSh->nREFSbn = nNewREFSbn;
                    }
                    /* no free reservoir block */
                    else
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BBM:ERR] Can't allocate new REF block\r\n")));
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BBM:ERR] Can't save previous data(nPDev: %d, nDieIdx: %d, nREFSbn: %d, nFlag: 0x%x)\r\n"), 
                                                        pstDev->nDevNo, nDieIdx, pstRsvSh->nREFSbn, BML_HANDLE_WRITE_ERROR));
                        nBBMRe = FSR_BML_NO_RSV_BLK_POOL;
                        break;
                    }

                    nREFPbn = (UINT32)pstRsvSh->nREFSbn;

                    /* Erase REF block before writing the data */
                    nLLDRe = _LLDErase(pstVol, pstDev->nDevNo, &nREFPbn, FSR_LLD_FLAG_1X_ERASE);

                } while (nLLDRe != FSR_LLD_SUCCESS);
            }

            if (nBBMRe != FSR_BML_SUCCESS)
            {
                break;
            }

            /* reset page offset */
            pstRsvSh->nNextREFPgOff = 0;
        }

        /* Set flag for _LLDWrite() */
        nLLDFlag = FSR_LLD_FLAG_1X_PROGRAM | FSR_LLD_FLAG_ECC_ON;

        /* 
         * When previous operation data is saved, pointer of pMBuf and pSBuf are null
         */
        if ((pMBuf == NULL) && (pSBuf == NULL))
        {
            nLLDFlag |= FSR_LLD_FLAG_BACKUP_DATA;
        }

        /* BML_FLAG_PREV_DATA */
        if (pSBuf == NULL)
        {
            pTmpSBuf = pSBuf;
        }
        /* BML_FLAG_NEXT_PREV_DATA */
        else
        {
            /* Set number of meta extension value before writing */

            FSR_OAM_MEMCPY(&stSBuf, pSBuf, sizeof(FSRSpareBuf));
            if (stSBuf.nNumOfMetaExt != 0)
            {
                stSBuf.nNumOfMetaExt = pstVol->nSizeOfPage / FSR_PAGE_SIZE_PER_SPARE_BUF_EXT;
            }
            pTmpSBuf  = &stSBuf;
        }

        bPgmErr = FALSE32;

        for (nPlnIdx = 0; nPlnIdx < pstVol->nNumOfPlane; nPlnIdx++)
        {
            /* write data to the page offset in REF block */
            nLLDRe = _LLDWrite(pstVol,
                               pstDev,
                               nDieIdx,
                               nREFPbn,
                               pstRsvSh->nNextREFPgOff,
                               pMBuf,
                               pTmpSBuf,
                               BML_USER_DATA,
                               nLLDFlag);

            if (nLLDRe != FSR_LLD_SUCCESS)
            {
                /* 
                 * program error occurs 
                 * program error should be handled after all back up program
                 * (In order to reset the position of dataRAM for back-up)
                 */
                bPgmErr = TRUE32;       
            }
            else
            {
                bPgmErr = FALSE32;
            }

            /* 
             * In case of next previous data, 
             * increase buffer pointer for next plane 
             * (pointer of pSBuf is not increased.)
             */
            if ((nFlag == BML_FLAG_NEXT_PREV_DATA) && 
                ((pMBuf != NULL) && (pSBuf != NULL)))
            {
                pMBuf += pstVol->nSizeOfPage;
                pTmpSBuf->pstSTLMetaExt++;
            }

            /* move to next page offset */
            pstRsvSh->nNextREFPgOff++;

        } /* end of for */
        
        /* if program error has occured */
        if (bPgmErr == TRUE32)
        {
            if (nFlag == BML_FLAG_NEXT_PREV_DATA)
            { 
                /* replace the bad block because data is not in the data RAM */
                nRet = FSR_BBM_HandleBadBlk(pstVol, 
                                            pstDev, 
                                            nREFPbn, 
                                            pstRsvSh->nNextREFPgOff,
                                            1,
                                            BML_HANDLE_WRITE_ERROR);
                if (nRet != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BBM:ERR] Bad block replacement for REF block is failed\r\n")));
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BBM:ERR] Can't save next previous data(nPDev: %d, nDieIdx: %d, nREFSbn: %d, nFlag: 0x%x)\r\n"), 
                                                    pstDev->nDevNo, nDieIdx, nREFPbn, BML_HANDLE_WRITE_ERROR));
                    nBBMRe = FSR_BML_CRITICAL_ERROR;
                    break;
                }

                /* get newly allocated block address */
                nREFPbn = _TransSbn2Pbn(pstRsvSh->nREFSbn, pstRsv);

                /* update REF Sbn to replaced block */
                pstRsvSh->nREFSbn = (UINT16) nREFPbn;

                /* rewrite the data */
                continue;

            }
            /* 
             * BML_FLAG_PREV_DATA : data is in data RAM 
             * (not BML_FLAG_DUMMY_DATA - LLDWrite() for dummy always returns FSR_LLD_SUCCESS) 
             */
            else
            {
                /*
                 * Write error case when data to be written is in data RAM                 
                 * However, Bad block replace operation can not be executed 
                 * because data should be protected until the data is written in the REF block successfully
                 * In this case, allocate new REF block then just try to write data in RAM
                 */

                do
                {
                    /* Allocate new REF block to rotate the REF block in Reservoir */
                    bRet = _GetNewREFBlk(pstDev->pstDie[nDieIdx], (UINT16 *) &(nNewREFSbn));
                    if (bRet == TRUE32)
                    {
                        /* allocate new REF block */
                        pstRsvSh->nREFSbn = nNewREFSbn;
                    }
                    /* no free reservoir block */
                    else
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BBM:ERR] Can't allocate new REF block\r\n")));
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BBM:ERR] Can't save previous data(nPDev: %d, nDieIdx: %d, nREFSbn: %d, nFlag: 0x%x)\r\n"), 
                                                        pstDev->nDevNo, nDieIdx, pstRsvSh->nREFSbn, BML_HANDLE_WRITE_ERROR));
                        nBBMRe = FSR_BML_NO_RSV_BLK_POOL;
                        break;
                    }

                    nREFPbn = (UINT32)pstRsvSh->nREFSbn;

                    /* Erase REF block before writing the data */
                    nLLDRe = _LLDErase(pstVol, pstDev->nDevNo, &nREFPbn, FSR_LLD_FLAG_1X_ERASE);

                } while (nLLDRe != FSR_LLD_SUCCESS);

                /* reset page offset */
                pstRsvSh->nNextREFPgOff = 0;

                /* rewrite the data */
                continue ;

            } /* end of else */

        } /* end of if (bPgmErr) */

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nBBMRe));

    return nBBMRe;
}

/**
 * @brief          this function reads previous operation data from REF block
 * @n              to restore the data for error handling
 *
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]      *pstDev      : device context pointer
 * @param[in]       nDieIdx     : die index
 * @param[in]      *pMBuf       : main buffer pointer
 * @param[in]      *pSBuf       : spare buffer pointer
 * @param[in]       nFlag       : BML_FLAG_NEXT_PREV_DATA or BML_FLAG_PREV_DATA
 *
 * @return          FSR_BML_SUCCESS  
 * @return          FSR_BML_CRITICAL_ERROR  
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PUBLIC INT32  
FSR_BBM_RestorePrevData(BmlVolCxt   *pstVol,
                        BmlDevCxt   *pstDev,
                        UINT32       nDieIdx,
                        UINT8       *pMBuf,
                        FSRSpareBuf *pSBuf,
                        UINT32       nFlag)
{
    BmlReservoir   *pstRsv;
    BmlReservoirSh *pstRsvSh;
    UINT32          nPgOff;
    UINT32          nNumOfPgs;
    UINT32          nPlnIdx;
    INT32           nBBMRe = FSR_BML_SUCCESS;
    INT32           nLLDRe;
    INT32           nMajorErr;
    FSRSpareBuf     stSBuf;

    FSR_STACK_VAR;

    FSR_STACK_END; 

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(PDev: %d, nDieIdx: %d, nFlag: 0x%x)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nDieIdx, nFlag));

    do
    {
        pstRsv      = pstDev->pstDie[nDieIdx]->pstRsv;
        pstRsvSh    = pstDev->pstDie[nDieIdx]->pstRsvSh;

        /* 
         * Currently, this function supports only 1 plane device
         * This function should be modified to support 2 plane device
         *
         * Data for previous operation is always written ahead of
         * data for next previous operation.
         * This rule should obeyed to restore data successfully
         */

        if (pstRsvSh->nNextREFPgOff < BML_PREV_PG_OFFSET)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] REF block does not have previous operation data\r\n")));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] (nPDev: %d, nDieIdx: %d, PBN:%d, nNextREFPgOff: %d, nFlag: 0x%x\r\n)"), 
                                           pstDev->nDevNo, nDieIdx, pstRsvSh->nREFSbn, pstRsvSh->nNextREFPgOff, nFlag));
            nBBMRe = FSR_BML_CRITICAL_ERROR;
            break;
        }

        /* data for previous operation */
        if (nFlag == BML_FLAG_PREV_DATA)
        {
            /* move to page offset which has previous data */
            nPgOff = pstRsvSh->nNextREFPgOff - (BML_PREV_PG_OFFSET * pstVol->nNumOfPlane);
        }
        /* BML_FLAG_NEXT_PREV_DATA : data for next previous operation */
        else
        {
            /* move to page offset which has next previous data */
            nPgOff = pstRsvSh->nNextREFPgOff - (BML_NEXT_PREV_PG_OFFSET * pstVol->nNumOfPlane);
        }

        /* hybrid reservoir or MLC reservoir */
        if ((pstRsv->nRsvrType == BML_HYBRID_RESERVOIR) ||
            (pstRsv->nRsvrType == BML_MLC_RESERVOIR))
        {            
            nNumOfPgs = pstVol->nNumOfPgsInMLCBlk;
        }
        /* BML_SLC_RESERVOIR */
        else
        {
            nNumOfPgs = pstVol->nNumOfPgsInSLCBlk;
        }

        FSR_OAM_MEMCPY(&stSBuf, pSBuf, sizeof(FSRSpareBuf));
        if (stSBuf.nNumOfMetaExt != 0)
        {
            stSBuf.nNumOfMetaExt = pstVol->nSizeOfPage / FSR_PAGE_SIZE_PER_SPARE_BUF_EXT;
        }

        for (nPlnIdx = 0; nPlnIdx < pstVol->nNumOfPlane; nPlnIdx++)
        {

            /* If BML interface tries to read unwritten data */
            if (nPgOff >= nNumOfPgs)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Invalid page offset to read previous data for error handling\r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] (nPDev: %d, nDieIdx: %d, PBN:%d, nPgOff: %d, nFlag: 0x%x\r\n)"), 
                                               pstDev->nDevNo, nDieIdx, pstRsvSh->nREFSbn, nPgOff, nFlag));
                nBBMRe = FSR_BML_CRITICAL_ERROR;
                break;
            }

            /* read one page from source block */
            nLLDRe = _LLDRead(pstVol,               /* Volume Number        */
                              pstDev->nDevNo,       /* PDev Number          */
                              pstRsvSh->nREFSbn,    /* source pbn           */
                              nPgOff,               /* page offset          */
                              pstRsv,               /* Reservoir pointer    */
                              pMBuf,                /* main buffer pointer  */
                              &stSBuf,              /* spare buffer pointer */
                              BML_USER_DATA,        /* data type            */
                              FALSE32,              /* nomal load           */
                              FSR_LLD_FLAG_ECC_ON); /* Operation Flag       */

            nMajorErr = FSR_RETURN_MAJOR(nLLDRe);

            /* ignore read disturbance and uncorrectable read error */
            if ((nLLDRe    != FSR_LLD_SUCCESS) &&
                (nMajorErr != FSR_LLD_PREV_READ_DISTURBANCE) &&
                (nMajorErr != FSR_LLD_PREV_READ_ERROR))
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] _LLDRead is failed (nLLDRe: 0x%x)\r\n"), nLLDRe));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] (nPDev: %d, nDieIdx: %d, PBN:%d, nPgOff: %d, nFlag: 0x%x, REF, User data\r\n)"), 
                                               pstDev->nDevNo, nDieIdx, pstRsvSh->nREFSbn, nPgOff, FSR_LLD_FLAG_ECC_ON));
                nBBMRe = FSR_BML_CRITICAL_ERROR;
                break;
            }

            /* move pointer for the next page */
            pMBuf += pstVol->nSizeOfPage;
            stSBuf.pstSTLMetaExt++;

            nPgOff++;

            /* 
             * data in spare buffer is copied when current plane is plane 0
             * (paired case: plane0 and plan1 has same data, 
             *  unpaired: valid data is only in the data ram of the latest used plane)
             */
            if (nPlnIdx == 0)
            {
                FSR_OAM_MEMCPY(pSBuf->pstSpareBufBase, stSBuf.pstSpareBufBase, FSR_SPARE_BUF_BASE_SIZE);
            }

        }

    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nBBMRe));

    return nBBMRe;
}

/**
 * @brief          this function erases REF block in reservoir after handling write error
 *
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]      *pstDev      : device context pointer
 * @param[in]       nDieIdx     : die index
 *
 * @return          FSR_BML_SUCCESS  
 * @return          FSR_BML_CRITICAL_ERROR  
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PUBLIC INT32
FSR_BBM_EraseREFBlk(BmlVolCxt   *pstVol,
                    BmlDevCxt   *pstDev,
                    UINT32       nDieIdx)
{
    BmlReservoir   *pstRsv;
    BmlReservoirSh *pstRsvSh;
    UINT32          nREFPbn;
    INT32           nLLDRe = FSR_LLD_SUCCESS;
    INT32           nRet   = FSR_BML_SUCCESS;
    
    FSR_STACK_VAR;

    FSR_STACK_END; 

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(PDev: %d, nDieIdx: %d)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nDieIdx));

    pstRsv      = pstDev->pstDie[nDieIdx]->pstRsv;
    pstRsvSh    = pstDev->pstDie[nDieIdx]->pstRsvSh;
    nREFPbn     = pstRsvSh->nREFSbn;

    do
    {
        /* Erase REF block for next use */
        nLLDRe = _LLDErase(pstVol, pstDev->nDevNo, &nREFPbn, FSR_LLD_FLAG_1X_ERASE);
        if (nLLDRe != FSR_LLD_SUCCESS)
        {
            nRet = FSR_BBM_HandleBadBlk(pstVol, 
                                        pstDev, 
                                        pstRsvSh->nREFSbn, 
                                        0, 
                                        1,
                                        BML_HANDLE_ERASE_ERROR);
            if (nRet != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Bad block replacement for REF block is failed(at step1)\r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] nPDev: %d, nREFPbn: %d, nUpdateType: 0x%x\r\n"),
                                                 pstDev->nDevNo, pstRsvSh->nREFSbn, BML_HANDLE_ERASE_ERROR));
                break;
            }

            nREFPbn = _TransSbn2Pbn(pstRsvSh->nREFSbn, pstRsv);
            pstRsvSh->nREFSbn = (UINT16) nREFPbn;
        }

        /* reset page offset */
        pstRsvSh->nNextREFPgOff = 0;

    } while (nLLDRe != FSR_LLD_SUCCESS);

   
    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nRet));

    return nRet;
}

#if !defined(FSR_NBL2)
/**
 * @brief          this function writes and reads specfic data pattern
 * @n              to verify LLD operation before formatting
 *
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]      *pstDev      : device context pointer
 * @param[in]       nDieIdx     : die index
 *
 * @return          TRUE32  
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE BOOL32
_LLDVerify(BmlVolCxt   *pstVol, 
           BmlDevCxt   *pstDev, 
           UINT32       nDieIdx)
{
    BmlReservoir   *pstRsv;
    BmlReservoirSh *pstRsvSh;
    FSRSpareBuf     stSBuf;
    BOOL32          bRet = TRUE32;
    UINT32          nByteIdx;
    UINT32          nREFPbn;
    UINT32          nNumOfPages;
    UINT32          nFlag;
    UINT32          nPgIdx;
    INT32           nLLDRe = FSR_LLD_SUCCESS;

    FSR_STACK_VAR;

    FSR_STACK_END; 

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(PDev: %d, nDieIdx: %d)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nDieIdx));

    pstRsv      = pstDev->pstDie[nDieIdx]->pstRsv;
    pstRsvSh    = pstDev->pstDie[nDieIdx]->pstRsvSh;

    nREFPbn = pstRsvSh->nREFSbn;

    /* MLC block */
    if ((UINT32)(nREFPbn - (pstVol->nNumOfBlksInDie * nDieIdx)) >= pstDev->nNumOfSLCBlksInDie[nDieIdx])
    {
        nNumOfPages = pstVol->nNumOfPgsInMLCBlk;
    }
    /* SLC block */
    else
    {
        nNumOfPages = pstVol->nNumOfPgsInSLCBlk;
    }

    FSR_OAM_MEMCPY(&stSBuf, pstRsv->pSBuf, sizeof(FSRSpareBuf));
    if (stSBuf.nNumOfMetaExt != 0)
    {
        stSBuf.nNumOfMetaExt = pstVol->nSizeOfPage / FSR_PAGE_SIZE_PER_SPARE_BUF_EXT;
    }

    nFlag = FSR_LLD_FLAG_1X_PROGRAM | FSR_LLD_FLAG_ECC_ON;

    for (nPgIdx = 0; nPgIdx < nNumOfPages; nPgIdx++)
    {
        /* make the specific data pattern */
        for (nByteIdx = 0; nByteIdx < pstVol->nSizeOfPage; nByteIdx++)
        {
            pstRsv->pMBuf[nByteIdx] = (UINT8)((nPgIdx + nByteIdx) % 0xFF);
        }

        /* write data into REF block */
        nLLDRe = _LLDWrite(pstVol, 
                           pstDev, 
                           nDieIdx,
                           nREFPbn, 
                           nPgIdx, 
                           pstRsv->pMBuf,
                           &stSBuf,
                           BML_USER_DATA, 
                           nFlag);
        if (nLLDRe != FSR_LLD_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:INF] _LLDWrite is failed(0x%x) during format\r\n"), nLLDRe));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:INF] (nPDev: %d, nDieIdx: %d, PBN:%d, nPgOff: %d, nFlag: 0x%x, REF block)\r\n"), 
                                           pstDev->nDevNo, nDieIdx, nREFPbn, nPgIdx, nFlag));
            break;
        }

        /* Read data from REF block */
        nLLDRe = _LLDRead(pstVol,               /* Volume Number        */
                          pstDev->nDevNo,       /* PDev Number          */
                          nREFPbn,              /* source pbn           */
                          nPgIdx,               /* page offset          */
                          pstRsv,               /* Reservoir pointer    */
                          pstRsv->pMBuf,        /* main buffer pointer  */
                          &stSBuf,              /* spare buffer pointer */  
                          BML_USER_DATA,        /* data type            */
                          FALSE32,              /* recovery load or nomal load */
                          FSR_LLD_FLAG_ECC_ON); /* Operation Flag       */ 
        if ((nLLDRe != FSR_LLD_SUCCESS) && 
            (FSR_RETURN_MAJOR(nLLDRe) != FSR_LLD_PREV_READ_DISTURBANCE))
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:INF] _LLDRead is failed during format(0x%x)\r\n"), nLLDRe));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:INF] (nPDev: %d, nDieIdx: %d, PBN:%d, nPgOff: %d, nFlag: 0x%x, REF block)\r\n"), 
                                           pstDev->nDevNo, nDieIdx, nREFPbn, nPgIdx, nFlag));
        }

        /* verify the data */
        for (nByteIdx = 0; nByteIdx < pstVol->nSizeOfPage; nByteIdx++)
        {
            if (pstRsv->pMBuf[nByteIdx] != ((nPgIdx + nByteIdx) % 0xFF))
            {
                bRet = FALSE32;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:INF] LLD Verification Error during format\r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:INF] (nREFPbn: %d, nPgIdx: %d, nByteIdx: %d, OrgData: 0x%x, ReadData: 0x%x\r\n"),
                                               nREFPbn, nPgIdx, nByteIdx, ((nPgIdx + nByteIdx) % 0xFF), pstRsv->pMBuf[nByteIdx]));
                break;
            }
        }

        /* error occurs */
        if (bRet != TRUE32)
        {
            break;
        }
    }
    
    /* Erase REF block and ignore its return value */
    _LLDErase(pstVol, pstDev->nDevNo, &nREFPbn, FSR_LLD_FLAG_1X_ERASE);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(bRe: 0x%x)\r\n"), __FSR_FUNC__, bRet));

    return TRUE32;
}
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/*
 * @brief           This function moves boundary in Reservoir between SLC and MLC
 *
 * @param[in]      *pstDev            : device context pointer
 * @param[in]      *pstRsv            : reservoir context pointer
 * @param[in]       nDieIdx           : index of die
 * @param[in]       nBlkType          : SLC block or MLC block
 *
 * @return          TRUE32  
 * @return          FALSE32  
 *
 * @author          MinYoung Kim
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE BOOL32
_MoveRsvBoundary(BmlDevCxt    *pstDev,
                 BmlReservoir *pstRsv,
                 UINT32        nDieIdx,
                 UINT32        nBlkType)
{
    BmlReservoirSh *pstRsvSh;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s(nPDev: %d, nDieIdx: %d, nBlkType: %d)\r\n"),
                                     __FSR_FUNC__, pstDev->nDevNo, nDieIdx, nBlkType));

    /* shift operation of Reservoir is only available in hybrid reservoir */
    FSR_ASSERT((pstRsv->nRsvrType == BML_HYBRID_RESERVOIR));

    pstRsvSh = pstDev->pstDie[nDieIdx]->pstRsvSh;

    /* 
     * case 1) All SLC reservoir block is used up
     *
     *  SLC     MLC            SLC     MLC  
     * +-+-+-+-+-+-+          +-+-+-+-+-+-+
     * |X|X| | |X|X|          |X|X|X| |X|X| 
     * +-+-+-+-+-+-+          +-+-+-+-+-+-+
     *     ^ shift                  ^   
     *     | ---->                  | 
     *     first MLC                first MLC
     *
     * case 2) All MLC reservoir block is used up
     *
     *  SLC     MLC            SLC     MLC  
     * +-+-+-+-+-+-+          +-+-+-+-+-+-+
     * |X|X| | |X|X|          |X|X| |X|X|X| 
     * +-+-+-+-+-+-+          +-+-+-+-+-+-+
     *   shift ^                    ^   
     *    <--- |                    | 
     *         first MLC            first MLC
     *
    */
    
    /* case 1) SLC block */
    if (nBlkType == BML_SLC_BLK)
    {  
        /* Moved position is not the reservoir */
        if (pstRsv->n1stSbnOfMLC + 1 > pstRsv->nLastSbnOfRsvr)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Can not move boundary between SLC and MLC\r\n"), 
                                           pstDev->nDevNo, nDieIdx, pstRsvSh->nLPCBSbn));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] nPDev: %d, nDieIdx: %d, nBlkType: %d, n1stSbnOfMLC: %d, nLastSbnOfRsvr: %d\r\n"), 
                                           pstDev->nDevNo, nDieIdx, nBlkType, pstRsv->n1stSbnOfMLC, pstRsv->nLastSbnOfRsvr));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FALSE32));
            return FALSE32;
        }
        else
        {
            /* Shift boundary to right */
            pstRsv->n1stSbnOfMLC++;   
            
            /* number of SLC blocks is increased */
            pstDev->nNumOfSLCBlksInDie[nDieIdx]++;
            
            /* size of reservoir for each area is modified */
            pstRsv->nNumOfRsvrInSLC++;
            pstRsv->nNumOfRsvrInMLC--;     
        }

    }
    /* case 2) MLC block */
    else
    {
        /* Moved position is not the reservoir */
        if (pstRsv->n1stSbnOfMLC - 1 < pstRsv->n1stSbnOfRsvr)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Can not move boundary between SLC and MLC\r\n"), 
                                           pstDev->nDevNo, nDieIdx, pstRsvSh->nLPCBSbn));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] nPDev: %d, nDieIdx: %d, nBlkType: %d, n1stSbnOfMLC: %d, n1stSbnOfRsvr: %d\r\n"), 
                                           pstDev->nDevNo, nDieIdx, nBlkType, pstRsv->n1stSbnOfMLC, pstRsv->n1stSbnOfRsvr));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FALSE32));
            return FALSE32;
        }
        else
        {
            /* Shift boundary to left */
            pstRsv->n1stSbnOfMLC--;
            
            /* number of SLC blocks is decreased */
            pstDev->nNumOfSLCBlksInDie[nDieIdx]--;

            /* size of reservoir for each area is modified */
            pstRsv->nNumOfRsvrInSLC--;
            pstRsv->nNumOfRsvrInMLC++;

        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, TRUE32));

    return TRUE32;
}
#endif /* !defined(FSR_NBL2) */

#if !defined(FSR_NBL2)
/*
 * @brief           This function checks the attribute of each partition. The previous partition attribute and
 * new partition attribute should be same.
 *
 * @param[in]      *pstVol      : volume context pointer
 * @param[in]      *pstPrevPartI     : pointer for previous partition info
 * @param[in]      *pstNewPartI     : pointer for new partition info
 *
 * @return          TRUE32  
 * @return          FALSE32  
 *
 * @author          SangHoon Choi
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE INT32
_CheckRangeOfPartition(BmlVolCxt   *pstVol,
                 FSRPartI    *pstPrevPartI,
                 FSRPartI    *pstNewPartI)
{
    UINT32         n1stVun      = 0;    /* the 1st Vun                      */
    UINT32         nNumOfPEntry = 0;    /* # of part entry                  */

    UINT32         nPrevLockAttr = 0;   /* # of part entry                  */
    UINT32         nNewLockAttr = 0;    /* # of part entry                  */

    FSRPartEntry  *pstPEntryPrevPart;
    FSRPartEntry  *pstPEntryNewPart;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s\r\n"),
                                     __FSR_FUNC__));

    /* Get pointer of PartEntry and nNumOfPartEntry */
    nNumOfPEntry     = pstPrevPartI->nNumOfPartEntry;
    pstPEntryPrevPart = pstPrevPartI->stPEntry + (nNumOfPEntry-1);

    while (nNumOfPEntry--)
    {
        n1stVun     = pstPEntryPrevPart->n1stVun;

        pstPEntryNewPart = _GetFSRPartEntry(pstNewPartI,n1stVun);
        if (pstPEntryNewPart == NULL)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Can not get FSRPartEntry\r\n")));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] n1stVun: %d\r\n"), 
                                           n1stVun));                               
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FALSE32));            
            return FSR_BML_CRITICAL_ERROR;
        }

        nNewLockAttr = pstPEntryNewPart->nAttr & (FSR_BML_PI_ATTR_LOCK | FSR_BML_PI_ATTR_LOCKTIGHTEN);
        nPrevLockAttr = pstPEntryPrevPart->nAttr & (FSR_BML_PI_ATTR_LOCK | FSR_BML_PI_ATTR_LOCKTIGHTEN);

        if (nNewLockAttr != nPrevLockAttr)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] The lock stats in attributes of the previous and new partition is not same\r\n")));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] nNewLockAttr: %x, nPrevLockAttr: %x\r\n"), 
                                           nNewLockAttr,nPrevLockAttr));                               
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FALSE32));            
            return FSR_BML_PARTIION_RANGE_IS_NOT_SAME;
        }
        pstPEntryPrevPart--;
    }

    /* Get pointer of PartEntry and nNumOfPartEntry */
    nNumOfPEntry     = pstNewPartI->nNumOfPartEntry;
    pstPEntryNewPart        = pstNewPartI->stPEntry + (nNumOfPEntry-1);

    while (nNumOfPEntry--)
    {
        n1stVun     = pstPEntryNewPart->n1stVun;

        pstPEntryPrevPart = _GetFSRPartEntry(pstPrevPartI,n1stVun);
        if (pstPEntryPrevPart == NULL)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] Can not get FSRPartEntry\r\n")));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] n1stVun: %d\r\n"), 
                                           n1stVun));                               
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FALSE32));
            return FSR_BML_CRITICAL_ERROR;
        }

        nNewLockAttr = pstPEntryNewPart->nAttr & (FSR_BML_PI_ATTR_LOCK | FSR_BML_PI_ATTR_LOCKTIGHTEN);
        nPrevLockAttr = pstPEntryPrevPart->nAttr & (FSR_BML_PI_ATTR_LOCK | FSR_BML_PI_ATTR_LOCKTIGHTEN);
        
        if (nNewLockAttr != nPrevLockAttr)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] The lock stats in attributes of the previous and new partition is not same\r\n")));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR, (TEXT("[BBM:ERR] nNewLockAttr: %x, nPrevLockAttr: %x\r\n"), 
                                           nNewLockAttr,nPrevLockAttr));                               
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FALSE32));
            return FSR_BML_PARTIION_RANGE_IS_NOT_SAME;
        }
        pstPEntryNewPart--;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, TRUE32));
    return FSR_BML_SUCCESS;
}
#endif /* !defined(FSR_NBL2) */

#if !defined (FSR_NBL2)
/*
 * @brief           This function returns FSRPartEntry with nVun and FSRPartI object.
 *
 * @param[in]      *pstPartI     : pointer for previous partition info
 * @param [in]      nVun         : virtual unit number
 *
 * @return          TRUE32
 * @return          FALSE32
 *
 * @author          SangHoon Choi
 * @version         1.0.0
 * @remark          none
 *
 * @since           since v1.0.0
 * @exception       none
 *
 */
PRIVATE FSRPartEntry*
_GetFSRPartEntry(FSRPartI    *pstPartI,
                 UINT32   nVun)
{
    UINT32         n1stVun      = 0;    /* the 1st Vun                      */
    UINT32         nLastVun     = 0;    /* the last Vun                     */
    UINT32         nNumOfPEntry = 0;    /* # of part entry                  */
    FSRPartEntry  *pstPEntry;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_BBM, (TEXT("[BBM:IN ] ++%s\r\n"),
                                     __FSR_FUNC__));

    /********************************************/
    /* Use a sequential search method           */
    /********************************************/
    /* Get pointer of PartEntry and nNumOfPartEntry */
    nNumOfPEntry     = pstPartI->nNumOfPartEntry;
    pstPEntry        = pstPartI->stPEntry + (nNumOfPEntry-1);

    while (nNumOfPEntry--)
    {
        n1stVun     = pstPEntry->n1stVun;
        nLastVun    = (n1stVun + pstPEntry->nNumOfUnits) -1;

        if ((n1stVun <= nVun) && (nLastVun >= nVun))
        {            
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));
            return pstPEntry;
        }

        pstPEntry--;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));    
    return NULL;
}
#endif /* !defined(FSR_NBL2) */
