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
 *  @file       FSR_BML_RWInterface.c
 *  @brief      This file consists of FSR_BML functions except FSR_BML_ROInterface.c
 *  @author     SuRuyn Lee
 *  @date       15-JAN-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  15-JAN-2007 [SuRyun Lee] : first writing
 *  @n  30-MAY-2007 [SuRyun Lee] : seperate orignal FSR_BML_Interface file
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

#if !defined(FSR_NBL2)
PRIVATE BOOL32  gbAdjustPartInfo[FSR_MAX_VOLS] = {FALSE32, FALSE32};
#endif
/*  Static function prototypes */
#if !defined(FSR_NBL2)
PRIVATE BOOL32  _ChkWLAttr          (FSRPartI      *pstPartI,
                                     UINT32         nPartIdx,
                                     UINT16         nGRPIdx);
PRIVATE INT32  _ChkPIValidity       (UINT32         nVol,
                                     FSRPartI      *pstPartI);
PRIVATE BOOL32 _IsROPartition       (UINT32         nVun,
                                     BmlVolCxt     *pstVol);

/* This internal functions only use for FSR_BML_IOCtl()*/
PRIVATE INT32  _LockTightData       (UINT32         nVol,
                                     UINT32         n1stVun,
                                     UINT32         nNumOfUnits);
PRIVATE INT32  _LockTightRsv        (UINT32         nVol);
PRIVATE INT32  _ChangePA            (UINT32         nVol,
                                     FSRChangePA   *pstPA);
PRIVATE INT32  _LockOTPBlk          (UINT32         nVol,
                                     UINT32         nOTPFlag);
PRIVATE INT32  _AdjustPartInfo      (UINT32         nVol,
                                     FSRPartI      *pstPartIIn,
                                     FSRPartI      *pstPartIOut);
PRIVATE VOID   _AllotRanInData      (BmlVolCxt     *pstVol,
                                     UINT32         nPlnIdx,
                                     UINT32         nRndInCnt,
                                     BMLRndInArg   *pstRndInArgIn,
                                     LLDRndInArg   *pstRndInArgOut);
PRIVATE INT32   _HandleCacheErr     (UINT32         nVol,
                                     UINT32         nPDev,
                                     UINT32         nErrDie);
PRIVATE INT32   _FakeLLDWrite       (UINT32         nDev,
                                     UINT32         nPbn,
                                     UINT32         nPgOffset,
                                     UINT8         *pMBuf,
                                     FSRSpareBuf   *pSBuf,
                                     UINT32         nFlag);
PRIVATE INT32   _UnlockUserBlk      (UINT32         nVol,
                                     BmlVolCxt     *pstVol);

#if defined(BML_CHK_FREE_PAGE)
PRIVATE INT32   _ChkFreePg          (UINT32         nVol,
                                     UINT32         nVpn,
                                     UINT32         nNumOfPgs);
#endif /* BML_CHK_FREE_PAGE */

#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
PRIVATE INT32   _HandleErrForDualCore (UINT32     nVol);
#endif /* FSR_NBL2*/

/**
 *  @brief  Code Implementation
 */
#if !defined(FSR_NBL2)
/**
 *  @brief      This function handles the all errors
 *  @n          for special case in dual core system.
 *
 *  @param [in]  nVol    : Volume number
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     some PAM errors
 *  @return     some BML errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
_HandleErrForDualCore(UINT32   nVol)
{
    INT32           nPAMRe = FSR_PAM_SUCCESS;
    INT32           nBMLRe = FSR_BML_SUCCESS;

    BmlVolCxt      *pstVol;
    FsrVolParm      stPAM[FSR_MAX_VOLS];

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d)\r\n"),
                                    __FSR_FUNC__, nVol));

    /* Get the pointer to volume context */
    pstVol = _GetVolCxt(nVol);

    do
    {
        nPAMRe = FSR_PAM_GetPAParm(&stPAM[0]);
        if (nPAMRe != FSR_PAM_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:   ]   %s(nVol: %d, nRe:0x%x)/ %d line\r\n"),
                                            __FSR_FUNC__, nVol, nPAMRe,  __LINE__));
            nBMLRe = nPAMRe;
            break;
        }

        if ((stPAM[0].bProcessorSynchronization == TRUE32) &&
            ((*pstVol->pnSharedOpenCnt) == 1))
        {
            nBMLRe = FSR_BML_FlushOp(nVol, FSR_BML_FLAG_NO_SEMAPHORE);
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:   ]   %s(nVol: %d, nRe:0x%x)/ %d line\r\n"),
                                                __FSR_FUNC__, nVol, nBMLRe,  __LINE__));
            }
        }
    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));

    return nBMLRe;
}
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function handles the previous write/erase error.
 *
 *  @param [in]  nVol    : Volume number
 *  @param [in]  nPDev   : physical device number
 *  @param [in]  nDieIdx : Die Index 
 *  @param [in]  nLLDRe  : error value of LLD
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_CRITICAL_ERROR
 *  @return     FSR_BML_WR_PROTECT_ERROR
 *  @return     some BML errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
_HandlePrevError(UINT32   nVol,
                 UINT32   nPDev,
                 UINT32   nDieIdx,
                 INT32    nLLDRe)
{
    INT32 nMajorErr;                    /* Major Error              */
    INT32 nMinorErr;                    /* Minor Error              */
    INT32 nBMLRe    = FSR_BML_SUCCESS;  /* BML Return value         */

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, nPDev:%d, nDieIdx:%d, nLLDRe:0x%x)\r\n"),
                                    __FSR_FUNC__, nVol, nPDev, nDieIdx, nLLDRe));

    nMajorErr = FSR_RETURN_MAJOR(nLLDRe);
    nMinorErr = FSR_RETURN_MINOR(nLLDRe);

    switch (nMajorErr)
    {
    case (FSR_LLD_PREV_WRITE_ERROR):
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:   ]   %s(nVol: %d, nPDev:%d, nDieIdx:%d, nLLDRe:FSR_LLD_PREV_WRITE_ERROR)\r\n"),
                                        __FSR_FUNC__, nVol, nPDev, nDieIdx));
        /* Handle the previous write error */
        nBMLRe = _HandlePrevWrErr(nVol,
                                  nPDev,
                                  nDieIdx,
                                  nMinorErr,
                                  FALSE32);
        break;

    case (FSR_LLD_PREV_ERASE_ERROR):
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:   ]   %s(nVol: %d, nPDev:%d, nDieIdx:%d, nLLDRe:FSR_LLD_PREV_ERASE_ERROR)\r\n"),
                                        __FSR_FUNC__, nVol, nPDev, nDieIdx));

        FSR_BBM_WaitUntilPowerDn();

        /* Handle the previous erase error */
        nBMLRe = _HandlePrevErErr(nVol,
                                  nPDev,
                                  nDieIdx,
                                  nMinorErr);
        break;

    case (FSR_LLD_WR_PROTECT_ERROR):
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:   ]   %s(nVol: %d, nPDev:%d, nDieIdx:%d, nLLDRe:FSR_LLD_WR_PROTECT_ERROR)\r\n"),
                                        __FSR_FUNC__, nVol, nPDev, nDieIdx));
        /* Return the current write protect error */
        nBMLRe = FSR_BML_WR_PROTECT_ERROR;
        break;

    case (FSR_LLD_PREV_READ_DISTURBANCE):
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:   ]   %s(nVol: %d, nPDev:%d, nDieIdx:%d, nLLDRe:FSR_LLD_PREV_READ_DISTURBANCE)\r\n"),
                                        __FSR_FUNC__, nVol, nPDev, nDieIdx));
        break;

    case (FSR_LLD_PREV_READ_ERROR):
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:   ]   %s(nVol: %d, nPDev:%d, nDieIdx:%d, nLLDRe:FSR_LLD_PREV_READ_ERROR)\r\n"),
                                        __FSR_FUNC__, nVol, nPDev, nDieIdx));
        nBMLRe = FSR_BML_READ_ERROR;
        break;

    default:
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:   ]   %s(nVol: %d, nPDev:%d, nDieIdx:%d, nLLDRe:0x%x)\r\n"),
                                        __FSR_FUNC__, nVol, nPDev, nDieIdx, nLLDRe));
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Unexpected result. \r\n")));
        nBMLRe = FSR_BML_CRITICAL_ERROR;
        break;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));

    return nBMLRe;
}
#endif

#if !defined(FSR_NBL2)
/**
 *  @brief      This function is a fake LLD_Write function for write error handling of DDP
 *
 *  @param [in]   nDev         : Physical Device Number (0 ~ 3)
 *  @param [in]   nPbn         : Physical Block  Number
 *  @param [in]   nPgOffset    : Page Offset within a block
 *  @param [in]  *pMBuf        : Memory buffer for main array of NAND flash
 *  @param [in]  *pSBuf        : Memory buffer for spare array of NAND flash
 *  @param [in]   nFlag        : Operation options such as ECC_ON, OFF
 *
 *  @return     FSR_LLD_SUCCESS
 *  @return     FSR_LLD_PREV_WRITE_ERROR
 *  @return     some LLD errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PRIVATE INT32
_FakeLLDWrite(UINT32         nDev,
              UINT32         nPbn,
              UINT32         nPgOffset,
              UINT8         *pMBuf,
              FSRSpareBuf   *pSBuf,
              UINT32         nFlag)
{
    UINT32          nVol    = 0;    /* Volume number (0 or 1)   */
    UINT32          nDieIdx = 0;    /* Die index                */
    INT32           nLLDRe  = FSR_LLD_SUCCESS;
    BmlVolCxt      *pstVol;
    BmlDevCxt      *pstDev;
    BmlDieCxt      *pstDie;
    FSRLowFuncTbl  *pstLFT;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nDev: %d, nPbn: %d, nPgOffset: %d)\r\n"),
                                    __FSR_FUNC__, nDev, nPbn, nPgOffset));

    /* Get volume number (Only support 2 way/vol.)*/
    nVol = nDev >> DEVS_PER_VOL_SHIFT;

    /* Get pointer to BmlVolCxt structure */
    pstVol = _GetVolCxt(nVol);

    /* Get pointer to FSRLowFuncTbl structure */
    pstLFT = _GetLFT(nVol);

    /* Get Die Index by Pbn*/
    if (nPbn < pstVol->nNumOfBlksInDie)
    {
        nDieIdx = 0;
    }
    else
    {
        nDieIdx = 1;
    }

    /* Get pointer to BmlDevCxt & BmlDieCxt structure */
    pstDev = _GetDevCxt(nDev);
    pstDie = pstDev->pstDie[nDieIdx];

    /* Get LLD return value */
    if (pstDie->bPrevErr == TRUE32)
    {
        nLLDRe = pstDie->nPrevErrRet;

        /* Initialize bPrevErr, nPrevErr*/
        pstDie->bPrevErr    = FALSE32;
        pstDie->nPrevErrRet = FSR_LLD_SUCCESS;
    }
    else
    {
        nLLDRe = pstLFT->LLD_Write(nDev,
                                   nPbn,
                                   nPgOffset,
                                   pMBuf,
                                   pSBuf,
                                   nFlag);
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));

    return nLLDRe;
}
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function handles the write-error that is occured from previous
 *  @n          and next previous operation of LLD.
 *
 *  @param [in]  nVol       : Volume number
 *  @param [in]  nPDev      : physical device number
 *  @param [in]  nErrDie    : Die Index in error
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_WRITE_ERROR
 *  @return     FSR_BML_VOLUME_NOT_OPENED
 *  @return     some BBM errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PRIVATE INT32
_HandleCacheErr(UINT32     nVol,
                UINT32     nPDev,
                UINT32     nErrDie)
{
    UINT32          nDieIdx     = 0;                        /* Die index                */
    UINT32          nPbn        = 0;                        /* Physical block number    */
    UINT32          nPgOffset   = 0;                        /* Page offset              */
    UINT32          nPgmFlag    = FSR_LLD_FLAG_1X_PROGRAM;  /* Flag for program cmd     */
    UINT32          nIdx        = 0;                        /* Temporary index          */
    BOOL32          bWrErr      = FALSE32;
    BOOL32          bDummyPgm   = FALSE32;
    INT32           nLLDRe      = FSR_LLD_SUCCESS;
    INT32           nRet        = FSR_LLD_SUCCESS;
    INT32           nBMLRe      = FSR_BML_SUCCESS;
    INT32           nMajorErr;
    INT32           nMinorErr;
    BmlVolCxt      *pstVol;
    BmlDevCxt      *pstDev;
    BmlDieCxt      *pstDie;
    FSRLowFuncTbl  *pstLFT;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, nPDev: %d, nErrDie: %d)\r\n"),
                                    __FSR_FUNC__, nVol, nPDev, nErrDie));

    /* Get pointer to BmlVolCxt structure */
    pstVol  = _GetVolCxt(nVol);

    /* Get pointer to BmlDevCxt structure */
    pstDev  = _GetDevCxt(nPDev);

    /* Get pointer to FSRLowFuncTbl */
    pstLFT = _GetLFT(nVol);

    /* Change function pointer */
    pstVol->LLD_Write         = pstLFT->LLD_Write;

    /*
     * << Procedure of handling cache error for DDP>>
     * STEP1. Call LLD_FlushOp to get previous return value.
     * STEP2. Set nPbn, nPgOffset, nPgmFlag by NAND type and # of plane.
     *        if a special condition is accepted, a dummy program is enable.
     * STEP3. Call LLD_Write and LLD_FlushOp, if a dummy program is enable.
     * STEP4. Save or return the write error by die index.
     */

    /* Initialize nDieIdx */
    nDieIdx = 0;
    nIdx    = 0;
    do
    {
        /* Get pointer to BmlDieCxt structure */
        pstDie      = pstDev->pstDie[nDieIdx];

        /********************************************************/
        /* STEP1. Call LLD_FlushOp to get previous return value */
        /********************************************************/
        nLLDRe = pstVol->LLD_FlushOp(nPDev,
                                     nDieIdx,
                                     FSR_LLD_FLAG_NONE);
        nMajorErr = FSR_RETURN_MAJOR(nLLDRe);
        nMinorErr = FSR_RETURN_MINOR(nLLDRe);
        if (nMajorErr == FSR_LLD_PREV_WRITE_ERROR)
        {
            bWrErr   = TRUE32;
            nRet    |= nMinorErr;
        }
        else if (nMajorErr == FSR_LLD_INVALID_PARAM)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nPDev:%d, nErrDie:%d, nRe:FSR_LLD_INVALID_PARAM) / %d line\r\n"),
                                            __FSR_FUNC__, nVol, nPDev, nErrDie, __LINE__));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));
            return FSR_LLD_INVALID_PARAM;
        }

        /********************************************************************/
        /* STEP2. Set nPbn, nPgOffset, nPgmFlag by NAND type and # of plane */
        /********************************************************************/
        /* Set nPbn & nPgOffset by NAND type */
        if ((pstVol->nNANDType      == FSR_LLD_FLEX_ONENAND) &&
            (pstVol->nNumOfPlane    != FSR_MAX_PLANES))
        {
            bDummyPgm = TRUE32;

            nPbn        = (UINT32)pstDie->pstRsvSh->nREFSbn;

            /* hybrid reservoir or MLC reservoir */
            if ((pstDie->pstRsv->nRsvrType == BML_HYBRID_RESERVOIR) ||
                (pstDie->pstRsv->nRsvrType == BML_MLC_RESERVOIR))
            {
                /* MLC REF block, program dummy data to last LSB page */
                nPgOffset = pstVol->pLSBPgMap[pstVol->nNumOfPgsInSLCBlk - 1];
            }
            else /* BML_SLC_RESERVOIR */
            {
                /* SLC REF block, program dummy data to last page */
                nPgOffset = pstVol->nNumOfPgsInSLCBlk - 1;
            }

            nPgmFlag    = FSR_LLD_FLAG_1X_PROGRAM;
        }

        if ((pstVol->nNANDType      == FSR_LLD_SLC_ONENAND) &&
            (pstVol->nNumOfPlane    == FSR_MAX_PLANES)      &&
            (pstDie->nNumOfLLDOp    == 0))
        {
            bDummyPgm = TRUE32;

            pstDie->nCurSbn[0] = pstDie->pstPreOp->nSbn;
            pstDie->nCurSbn[1] = pstDie->pstPreOp->nSbn + 1;

            pstDie->nNumOfLLDOp = 0;
            _GetPBN(pstDie->nCurSbn[0], pstVol, pstDie);

            nPbn        = pstDie->nCurPbn[0];
            nPgOffset   = pstDie->pstPreOp->nPgOffset;
            nPgmFlag    = FSR_LLD_FLAG_2X_PROGRAM;
        }

        /***********************************************************************/
        /* STEP3. Call LLD_Write and LLD_FlushOp, if a dummy program is enable.*/
        /***********************************************************************/
        if (bDummyPgm == TRUE32)
        {
            nLLDRe = pstVol->LLD_Write(nPDev,
                                       nPbn,
                                       nPgOffset,
                                       NULL,
                                       NULL,
                                       nPgmFlag | FSR_LLD_FLAG_BACKUP_DATA);
            nMajorErr = FSR_RETURN_MAJOR(nLLDRe);
            nMinorErr = FSR_RETURN_MINOR(nLLDRe);
            if (nMajorErr == FSR_LLD_PREV_WRITE_ERROR)
            {
                bWrErr   = TRUE32;
                nRet    |= nMinorErr;
            }
            else if (nMajorErr == FSR_LLD_INVALID_PARAM)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nPDev:%d, nErrDie:%d, nRe:FSR_LLD_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, nPDev, nErrDie, __LINE__));
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));
                return FSR_LLD_INVALID_PARAM;
            }

            nLLDRe = pstVol->LLD_FlushOp(nPDev,
                                         nDieIdx,
                                         FSR_LLD_FLAG_NONE);
            nMajorErr = FSR_RETURN_MAJOR(nLLDRe);
            nMinorErr = FSR_RETURN_MINOR(nLLDRe);
            if (nMajorErr == FSR_LLD_PREV_WRITE_ERROR)
            {
                bWrErr   = TRUE32;

                /* Shift nMinorErr bit */
                nMinorErr= nMinorErr << BML_SHIFT_BIT_FOR_DUMMY_PGM;
                nRet    |= nMinorErr;
            }
            else if (nMajorErr == FSR_LLD_INVALID_PARAM)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nPDev:%d, nErrDie:%d, nRe:FSR_LLD_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, nPDev, nErrDie, __LINE__));
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));
                return FSR_LLD_INVALID_PARAM;
            }
        } /* End of "if (bDummyPgm == TRUE32)" */

        /********************************************************/
        /* STEP4. Save or return the write error by die index.  */
        /********************************************************/
        if (bWrErr == TRUE32)
        {
            nRet    |= FSR_LLD_PREV_WRITE_ERROR;

            if (nDieIdx == nErrDie)
            {
                /* Return error */
                nBMLRe = nRet;
            }
            else /* nDieIdx != nErrDie */
            {
                /* Save error to BmlDieCxt structure */
                pstDie->bPrevErr    = TRUE32;
                pstDie->nPrevErrRet = nRet;

                /* Change function pointer */
                pstVol->LLD_Write    = _FakeLLDWrite;
            }
        }

        /* Change die index to opposite die */
        nDieIdx = (nDieIdx + 1) & 0x01;

        /* Initialize bWrErr */
        bWrErr      = FALSE32;
        nRet        = FSR_LLD_SUCCESS;
        bDummyPgm   = FALSE32;

    } while(++nIdx < pstVol->nNumOfDieInDev);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));

    return nBMLRe;
}
#endif

#if !defined(FSR_NBL2)
/**
 *  @brief      This function handles the write-error that is occured from previous
 *  @n          and next previous operation of LLD.
 *
 *  @param [in]  nVol       : Volume number
 *  @param [in]  nPDev      : physical device number
 *  @param [in]  nDieIdx    : Die Index 
 *  @param [in]  nMinorErr  : error value of LLD
 *  @param [in]  bOn        : flag to inform the current state
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_WRITE_ERROR
 *  @return     FSR_BML_VOLUME_NOT_OPENED
 *  @return     some BBM errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
_HandlePrevWrErr(UINT32   nVol,
                 UINT32   nPDev,
                 UINT32   nDieIdx,
                 INT32    nMinorErr,
                 BOOL32   bOn)
{
    UINT32          nPgOffset   = 0;        /* physical page offset                     */
    UINT32          nPlnIdx     = 0;        /* plane index                              */
    UINT32          nTmpPln     = 0;        /* Temporary plane index                    */
    UINT32          nSbn        = 0;        /* Semi-physical block number               */
    UINT32          nTmpSbn     = 0;        /* Temporary semi-physical block number     */
    UINT32          nErrCnt     = 0;        /* # of blocks in error                     */
    UINT32          nDataType   = BML_FLAG_PREV_DATA; /* Flag for FSR_BBM_RestorePrevData
                                            (BML_FLAG_PREV_DATA/BML_FLAG_NEXT_PREV_DATA)*/
    UINT32          nNumOfRstr  = 0;        /* calling # of FSR_BBM_RestorePrevData     */
    UINT32          nRstrIdx    = 0;        /* Index for FSR_BBM_RestorePrevData        */
    UINT32          nReplace    = 0;        /* # of bad block replacement               */
    UINT32          nBBMFlag    = 0;        /* flag for recovered LSB page              */
    UINT32          nIdx        = 0;        /* Temporary index                          */
    BOOL32          bExistErr   = FALSE32;  /* flag to inform the next previous error   */
    INT32           nBMLRe      = FSR_BML_SUCCESS;
    INT32           nLLDRe      = FSR_LLD_SUCCESS;
    INT32           nRet        = FSR_BML_SUCCESS;
    UINT8          *pMBuf;
    FSRSpareBuf    *pSBuf;
    FSRSpareBuf     stSBuf;
    BmlVolCxt      *pstVol;
    BmlDevCxt      *pstDev;
    BmlDieCxt      *pstDie;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, nPDev: %d, nDieIdx: %d, nMinorErr: 0x%x)\r\n"),
                                    __FSR_FUNC__, nVol, nPDev, nDieIdx, nMinorErr));

    /* checking volume range */
    CHK_VOL_RANGE(nVol);

    /* Get pointer to volume context */
    pstVol = _GetVolCxt(nVol);

    /* checking volume open */
    CHK_VOL_OPEN(pstVol->bVolOpen);

    FSR_ASSERT(nVol < FSR_MAX_VOLS);

    /* Get pointer to device context */ 
    pstDev = _GetDevCxt(nPDev);

    /* Find pointer to get die context */
    pstDie =pstDev->pstDie[nDieIdx];

    /* Handle error of cache program for all Die */
    if (bOn == TRUE32)
    {
        nRet = _HandleCacheErr(nVol,
                               nPDev,
                               nDieIdx);
        if (nRet == FSR_LLD_INVALID_PARAM)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   _HandleCacheErr(nVol:%d, nPDev:%d, nDieIdx:%d nRe:FSR_LLD_INVALID_PARAM)\r\n"),
                                            nVol, nPDev, nDieIdx));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));
            return nRet;
        }
        else
        {
            nMinorErr |= FSR_RETURN_MINOR(nRet);
        }
    }

    /************************************************/
    /* <Procedure for backup of the previous data   */
    /* STEP1. Backup of the previous data           */
    /* STEP2. Backup of the next previous data      */
    /************************************************/
    /* STEP1. Backup the previous data */
    /* if this device is Flex-ONENAND or SLC OneNAND, DataRAM has the buffer data of previous log.
     * if not, this data has already been in the buffer
     */
    if ((pstVol->nNANDType == FSR_LLD_FLEX_ONENAND) ||
        (pstVol->nNANDType == FSR_LLD_SLC_ONENAND))
    {
        pMBuf   =   NULL;
        pSBuf   =   NULL;

        nBMLRe = FSR_BBM_BackupPrevData(pstVol,
                                        pstDev,
                                        nDieIdx,
                                        pMBuf,
                                        pSBuf,
                                        BML_FLAG_PREV_DATA);
        if (nBMLRe != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   FSR_BBM_BackupPrevData(nDieIdx:%d, nFlag:BML_FLAG_PREV_DATA, nRe:0x%x) / %d line\r\n"),
                                            nDieIdx, nBMLRe, __LINE__));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));
            return nBMLRe;
        }
    }

    /* STEP2. Backup the next previous data*/
    if (bOn == TRUE32) 
    {
        pMBuf   =   pstDie->pNextPreOpMBuf;
        pSBuf   =   pstDie->pNextPreOpSBuf;
    }
    else
    {
        pMBuf   =   pstDie->pMBuf;
        pSBuf   =   pstDie->pSBuf;
    }

    nBMLRe = FSR_BBM_BackupPrevData(pstVol,
                                    pstDev,
                                    nDieIdx,
                                    pMBuf,
                                    pSBuf,
                                    BML_FLAG_NEXT_PREV_DATA);
    if (nBMLRe != FSR_BML_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   FSR_BBM_BackupPrevData(nDieIdx:%d, nFlag:BML_FLAG_NEXT_PREV_DATA, nRe:0x%x) / %d line\r\n"),
                                        nDieIdx, nBMLRe, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));
        return nBMLRe;
    }

    /* <<Error Case>>
     * --------------------+-----------+-----------+
     *                     | 1st Plane | 2nd Plane |
     * --------------------+-----------+-----------+
     * Next previous error |     A     |     B     |
     * Previous error      |     C     |     D     |
     *---------------------+-----------+-----------+
     *
     * <Parameters of each case>
     * 1. 1x operation of 1x-device, 2x operation of 2x device
     * ------------+---------+---------+----------+------------+----------+
     *  Error Case | nErrCnt | nPlnIdx | nReplace | nNumOfRstr | nSbnType |
     * ------------+---------+---------+----------+------------+----------+
     *      A      |    1    |    0    |     1    |     2      | NEXTPREV |
     *      B      |    1    |    1    |     1    |     2      | NEXTPREV |
     *      C      |    1    |    0    |     1    |  1 (PREV)  |   PREV   |
     *      D      |    1    |    1    |     1    |  1 (PREV)  |   PREV   |
     *    A & C    |    1    |    0    |     1    |     2      | NEXTPREV |
     *    B & D    |    1    |    1    |     1    |     2      | NEXTPREV |
     *    A & B    |    2    |    0    |     1    |     2      | NEXTPREV |
     *    A & D    |    2    |    0    |     1    |     2      | NEXTPREV |
     *    B & C    |    2    |    0    |     1    |     2      | NEXTPREV |
     *    C & D    |    2    |    0    |     1    |     2      | NEXTPREV |
     * ------------+---------+---------+----------+------------+----------+
     * Notes) The others are included in above cases.
     *        (ex. A & B & C, A & B & C & D, ......)
     *
     * 2. 1x operation of 2x device
     * ------------+---------+---------+----------+------------+----------+
     *  Error Case | nErrCnt | nPlnIdx | nReplace | nNumOfRstr | nSbnType |
     * ------------+---------+---------+----------+------------+----------+
     *      A      |    1    |    0    |    1     |  1 (PREV)  | NEXTPREV |
     *      C      |    1    |    0    |    1     |  1 (PREV)  |   PREV   |
     *    A & C    |    1    |    0    |    2     |  1 (PREV)  | NEXTPREV |
     * ------------+---------+---------+----------+------------+----------+
     */

    /********************************************/
    /* STEP1. Find Sbn & nNumOfErr by nMinorErr */
    /********************************************/
    /* Error Case: (1x program of 2x-device) && (next previous nSbn != previous nSbn) */
    if ((pstDie->pstNextPreOp->nSbn != pstDie->pstPreOp->nSbn) && (nMinorErr & BML_NEXTPREV_ERROR))
    {
        nPlnIdx     = 0;        /* This case always returns 1st plane minor error   */
        nErrCnt     = 1;        /* In this case, nErrCnt always is 1                */

        if (nMinorErr & BML_NEXTPREV_ERROR)
        {
            nReplace        ++;                                 /* Increase calling # of FSR_BBM_HandleBadBlk   */
            nSbn            = pstDie->pstNextPreOp->nSbn;       /* Set nSbn to next previous one                */
            nPgOffset       = pstDie->pstNextPreOp->nPgOffset;  /* Set nPgOffset to next previous one           */
            bExistErr       = TRUE32;                           /* Notify the next previous error               */
            nNumOfRstr      = 2;                                /* calling # of FSR_BBM_RestorePrevData         */
            nDataType       = BML_FLAG_NEXT_PREV_DATA;
        }

        if (nMinorErr & BML_PREV_ERROR)
        {
            nReplace        ++;                                 /* Increase calling # of FSR_BBM_HandleBadBlk   */
            if (bExistErr == FALSE32)
            {
                nSbn        = pstDie->pstPreOp->nSbn;       /* Set nSbn to previous one     */
                nPgOffset   = pstDie->pstPreOp->nPgOffset;  /* Set nPgOffset to previous one*/
                nNumOfRstr  ++;        /* FSR_BBM_RestorePrevData should be called one time*/
                nDataType   = BML_FLAG_PREV_DATA;
            }
        }
    }
    else /* Error Case: 1x program of 1x-device / 2x program of 2x-device */
    {
        /* Set calling # of FSR_BBM_HandleBadBlk to 1 */
        nReplace      = 1;

        if (nMinorErr & BML_PREV_ERROR)
        {
            nSbn            = pstDie->pstPreOp->nSbn;
            nPgOffset       = pstDie->pstPreOp->nPgOffset;
            nDataType       = BML_FLAG_PREV_DATA;
            nNumOfRstr      = 1;
        }

        if (nMinorErr & BML_NEXTPREV_ERROR)
        {
            nSbn            = pstDie->pstNextPreOp->nSbn;
            nPgOffset       = pstDie->pstNextPreOp->nPgOffset;
            nDataType       = BML_FLAG_NEXT_PREV_DATA;
            nBBMFlag        = BML_RECOVER_PREV_ERROR;
            nNumOfRstr      = 2;
        }

        if (nMinorErr & BML_2NDPLN_ERROR)
        {
            nErrCnt     ++;
            nPlnIdx     = 1;
        }

        if (nMinorErr & BML_1STPLN_ERROR)
        {
            nErrCnt     ++;
            nPlnIdx     = 0;
        }

    } /* End of "if (pstDie->pstNextPreOp->nSbn !=...)" */

    do
    {
        /********************************************************/
        /* STEP2. Replace the bad block to the reserved block   */
        /********************************************************/
        nBMLRe = FSR_BBM_HandleBadBlk(pstVol,
                                      pstDev,
                                      nSbn + nPlnIdx,
                                      nPgOffset,
                                      nErrCnt,
                                      BML_HANDLE_WRITE_ERROR | nBBMFlag);
        if (nBMLRe != FSR_BML_SUCCESS)
        {
            /* Inform the error of FSR_BBM_HandleBadBlk*/
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   FSR_BBM_HandleBadBlk(nSbn:%d, nPgOffset:%d, nErrCnt:%d, nFlag:BML_HANDLE_WRITE_ERROR, nRe:0x%x) / %d line\r\n"),
                                            nSbn + nPlnIdx, nPgOffset, nErrCnt, nBMLRe, __LINE__));
            break;
        }

        /* Second Translation: translate Pbn[] */
        pstDie->nNumOfLLDOp = 0;
        nTmpSbn             = nSbn;
        if ((pstVol->nNumOfPlane == FSR_MAX_PLANES) && (nSbn & 0x01))
        {
            nTmpSbn --;
        }
        _GetPBN(nTmpSbn, pstVol, pstDie);

        /* <Paired bad block replacement>
         * If BBM replaces a bad block by paired ones,
         * the block has no error should be rewritten.
         */
        if (pstDie->nNumOfLLDOp == 0)
        {
            nPlnIdx = 0;
            nErrCnt = pstVol->nNumOfPlane;
        }

        /********************************************************/
        /* STEP3. Rewrite the next previous or previous page    */
        /********************************************************/
        nRstrIdx = 0;
        do
        {
            /* Restore data */
            pMBuf       =   pstDie->pMBuf;

            pSBuf       =   pstDie->pSBuf;
            FSR_OAM_MEMCPY(&stSBuf, pSBuf, sizeof(FSRSpareBuf));
            stSBuf.nNumOfMetaExt = pstVol->nSizeOfPage / FSR_PAGE_SIZE_PER_SPARE_BUF_EXT;

            nBMLRe = FSR_BBM_RestorePrevData(pstVol,
                                             pstDev,
                                             nDieIdx,
                                             pMBuf,
                                             &stSBuf,
                                             nDataType);
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                /* Inform the error of FSR_BBM_RestorePrevData*/
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   FSR_BBM_RestorePrevData(nDieIdx:%d, nDataType:0x%x, nRe:0x%x) / %d line\r\n"),
                                                nDieIdx, nDataType, nBMLRe, __LINE__));
                break;
            }

            nTmpPln = nPlnIdx;
            do
            {
                /* Set pMBuf and pSBuf */
                pMBuf      =   pstDie->pMBuf + nTmpPln * pstVol->nSizeOfPage;

                /* Change the pointer for STL meta extension data */
                stSBuf.pstSTLMetaExt   += nTmpPln;

                /* 
                 * <<Unpaired bad block replacement>>
                 * This code use when the previous operation is 1x operation in 2x-device.
                 * In this condition, all returned error is one of plane 0.
                 */
                if ((pstDie->nNumOfLLDOp != 0) && (nSbn & 0x01))
                {
                    nTmpPln = (nTmpPln + 0x01) & 0x01;
                }

                /* STEP2. Call LLD_Write for next previous data */
                nLLDRe    =  pstVol->LLD_Write(nPDev,
                                               pstDie->nCurPbn[nTmpPln],
                                               nPgOffset,
                                               pMBuf,
                                               &stSBuf,
                                               FSR_LLD_FLAG_1X_PROGRAM | FSR_LLD_FLAG_USE_SPAREBUF);

                /* Message out */
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:   ]   Re-write(nPDev:%d, nPbn:%d, nPgOffset:%d)\r\n"),
                                                nPDev, pstDie->nCurPbn[nTmpPln], nPgOffset));

                /* Handle a write error using LLD_FlushOp */
                nLLDRe = pstVol->LLD_FlushOp(nPDev,
                                             nDieIdx,
                                             FSR_LLD_FLAG_NONE);
                if (nLLDRe != FSR_LLD_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_Write(nPDev:%d, nPbn:%d, nPgOffset:%d, nRe:0x%x)\r\n"),
                                                    nPDev, pstDie->nCurPbn[nTmpPln], nPgOffset, nLLDRe));
                    nBMLRe = FSR_BML_WRITE_ERROR;
                    break;
                }

            } while (++nTmpPln < nErrCnt);

            if (nBMLRe != FSR_BML_SUCCESS)
            {
                break;
            }

            /* Set nPgOffset, nDataType for rewriting previous data */
            nPgOffset   = pstDie->pstPreOp->nPgOffset;
            nDataType   = BML_FLAG_PREV_DATA;

        } while(++nRstrIdx <nNumOfRstr);

        if (nBMLRe != FSR_BML_SUCCESS)
        {
            /* break this while loop in error case about ESR_BBM_RestorePrevData() */
            if (nBMLRe == FSR_BML_CRITICAL_ERROR)
            {
                break;
            }
        }
        else
        {
            /* Set nSbn, nPgOffset for next loop */
            nSbn        = pstDie->pstPreOp->nSbn;
            nReplace--;
        }

    } while (nReplace > 0);

    /*
     * Backup program to REF block is used to set the returns of previous operation.
     * This operation should be carried out for all die in device.
     * So, the below function shuld be called
     * to prevent from a NOP violation and sequential program violation of REF block.
     */
     if ((nBMLRe == FSR_BML_SUCCESS)            &&
         (bOn == TRUE32)                        &&
         (pstVol->nNANDType == FSR_LLD_FLEX_ONENAND))
     {
         for (nIdx = 0; nIdx < pstVol->nNumOfDieInDev; nIdx++)
         {
             nBMLRe = FSR_BBM_EraseREFBlk(pstVol,
                                          pstDev,
                                          nIdx);
             if (nBMLRe != FSR_BML_SUCCESS)
             {
                 FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   FSR_BBM_EraseREFBlk(nDieIdx:%d, nRe:0x%x) / %d line\r\n"),
                                                nIdx, nBMLRe, __LINE__));
                 break;
             }
         }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));

    return nBMLRe;
}
#endif

#if !defined(FSR_NBL2)
/**
 *  @brief      This function handles the erase-error that is occured from previous
 *  @n          operation of LLD.
 *
 *  @param [in]  nVol        : Volume number
 *  @param [in]  nPDev       : physical device number
 *  @param [in]  nDieIdx     : Die Index
 *  @param [in]  nMinorErr   : error value of LLD
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_ERASE_ERROR
 *  @return     FSR_BML_VOLUME_NOT_OPENED
 *  @return     some BBM errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
_HandlePrevErErr(UINT32     nVol,
                 UINT32     nPDev,
                 UINT32     nDieIdx,
                 INT32      nMinorErr)
{
    UINT32          nPlnIdx = 0;                /* plane index                          */
    UINT32          nSbn    = 0;                /* Semi-physical block number           */
    UINT32          nTmpSbn = 0;                /* Temporary Sbn for _GetPBN()          */
    UINT32          nPbn    = 0;                /* array of physical blocks             */
    UINT32          nErrCnt = 1;                /* # of blocks in error                 */
    INT32           nBMLRe  = FSR_BML_SUCCESS;  /* BBM Return value                     */
    INT32           nLLDRe  = FSR_LLD_SUCCESS;  /* LLD Return value                     */
    BmlVolCxt      *pstVol;
    BmlDevCxt      *pstDev;
    BmlDieCxt      *pstDie;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, nPDev: %d, nDieIdx: %d, nMinorErr: 0x%x)\r\n"),
                                    __FSR_FUNC__, nVol, nPDev, nDieIdx, nMinorErr));

    /* check volume range */
    CHK_VOL_RANGE(nVol);

    /* Get pointer to Volume context */
    pstVol = _GetVolCxt(nVol);

    /* checking volume open */
    CHK_VOL_OPEN(pstVol->bVolOpen);

    FSR_ASSERT(nVol < FSR_MAX_VOLS);

    /* Get pointer to device context */
    pstDev = _GetDevCxt(nPDev);

    /* Find pointer to get die context */ 
    pstDie = pstDev->pstDie[nDieIdx];

    /* Call LLD_FlushOp to remove previous error */
    nLLDRe = pstVol->LLD_FlushOp(nPDev,
                                 nDieIdx,
                                 FSR_LLD_FLAG_NONE);

    /* Set nSbn by plane index */
    nSbn    = pstDie->pstPreOp->nSbn;

    /********************************************************/
    /* STEP2. Handle the bad blk using FSR_BBM_HandleBadBlk */
    /********************************************************/
    nBMLRe = FSR_BBM_HandleBadBlk(pstVol,
                                  pstDev,
                                  nSbn,
                                  0,
                                  1,
                                  BML_HANDLE_ERASE_ERROR);
    /* Inform the error of FSR_BBM_HandleBadBlk*/
    if (nBMLRe != FSR_BML_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   FSR_BBM_HandleBadBlk(nSbn:%d, nErrCnt:%d, nFlag:BML_HANDLE_ERASE_ERROR, nRe:0x%x) / %d line\r\n"),
                                            nSbn + nPlnIdx, nErrCnt, nBMLRe, __LINE__));
    }
    else /* if (nBMLRe == FSR_BML_SUCCESS) */
    {
        /* Second Translation: translate Pbn[] */
        pstDie->nNumOfLLDOp = 0;
        nTmpSbn             = nSbn;
        /* Fine the first Sbn in 2 plane device */
        if ((pstVol->nNumOfPlane == FSR_MAX_PLANES) && (nTmpSbn & 0x01))
        {
            nTmpSbn --;
        }
        _GetPBN(nTmpSbn, pstVol, pstDie);

        /* <Paired bad block replacement>
         * If BBM replaces a bad block by paired ones,
         * the block has no error should be re-erased.
         */
        if (pstDie->nNumOfLLDOp == 0)
        {
            nPlnIdx = 0;
            nErrCnt = pstVol->nNumOfPlane;
        }

        /* STEP2-1. Handle the bad blk using FSR_BBM_HandleBadBlk */
        do
        {
            /* <<Unpaired bad block replacement>>
             * This code use when the previous operation is 1x operation in 2x-device.
             * In this condition, all returned error is one of plane 0.
             */
            if ((pstDie->nNumOfLLDOp != 0) && (nSbn & 0x01))
            {
                nPlnIdx = (nPlnIdx + 0x01) & 0x01;
            }

            /* Get nPbn*/
            nPbn = pstDie->nCurPbn[nPlnIdx];

            /* Call LLD_Erase */
            nLLDRe = pstVol->LLD_Erase(nPDev,
                                       &nPbn,
                                       1,
                                       FSR_LLD_FLAG_1X_ERASE);

            /* Message out */
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:   ]   Re-erase(nPDev:%d, nPbn:%d)\r\n"),
                                            nPDev, nPbn));

            /* Call LLD_FlushOp to remove a write error */
            nLLDRe = pstVol->LLD_FlushOp(nPDev,
                                         nDieIdx,
                                         FSR_LLD_FLAG_NONE);
            if (nLLDRe != FSR_LLD_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nPDev: %d, nDieIdx: %d, nMinorErr: 0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nPDev, nDieIdx, nMinorErr));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_Erase(nPDev:%d, nPbn:%d, nRe:0x%x) / %d line \r\n"),
                                                nPDev, nPbn, nLLDRe, __LINE__));
                nBMLRe = FSR_BML_ERASE_ERROR;
            }

            /* Increase nPlnIdx */
            nPlnIdx++;

        } while (--nErrCnt);

    } /* End of "if (nBMLRe == FSR_BML_SUCCESS)" */

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));

    return nBMLRe;
}
#endif

/**
 *  @brief      This function process erase refresh.and loads ERL and processes it.
 *
 *  @param [in]  nVol    : volume number
 *  @param [in]  nFlag   : FSR_BML_FLAG_REFRESH_ALL
 *  @n                     FSR_BML_FLAG_REFRESH_PARTIAL
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     some BBM errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
_ProcessEraseRefresh(UINT32    nVol,
                     UINT32    nFlag)
{
    UINT32      nPDev   = 0;    /* Physical device number   */
    UINT32      nDevIdx = 0;    /* Device index             */
    UINT32      nDieIdx = 0;    /* Die index                */
    INT32       nBMLRe  = FSR_BML_SUCCESS;
    BmlVolCxt  *pstVol;
    BmlDevCxt  *pstDev;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol:%d, nFlag:0x%x)\r\n"),
                                    __FSR_FUNC__, nVol, nFlag));

    /* Get pointer to volume context */
    pstVol = _GetVolCxt(nVol);

    for (nDevIdx = 0; nDevIdx < pstVol->nNumOfDev; nDevIdx++)
    {
        /* Set nPDev */
        nPDev = nVol * DEVS_PER_VOL + nDevIdx;

        /* Get pointer to device context */
        pstDev  = _GetDevCxt(nPDev);

        for (nDieIdx = 0; nDieIdx < pstVol->nNumOfDieInDev; nDieIdx++)
        {
            /* Step 1. Checks Refresh block */
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]  Step 1. Checks Refresh block\r\n")));

            nBMLRe = FSR_BBM_ChkRefreshBlk(pstVol,
                                           pstDev,
                                           nDieIdx);
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nFlag:0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nFlag));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   FSR_BBM_ChkRefreshBlk(nPDev:%d, nDieIdx:%d, nRe:0x%x) / %d line \r\n"),
                                                nPDev, nDieIdx, nBMLRe, __LINE__));
                break;
            }

            /* Step 2. Process ERL */
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]  Step 2. Processes ERL\r\n")));

            nBMLRe = FSR_BBM_RefreshByErase(pstVol,
                                            pstDev,
                                            nDieIdx,
                                            nFlag);
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nFlag:0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nFlag));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   FSR_BBM_RefreshByErase(nPDev:%d, nDieIdx:%d, nRe:0x%x) / %d line \r\n"),
                                                nPDev, nDieIdx, nBMLRe, __LINE__));
                break;
            }
        }

        if (nBMLRe != FSR_BML_SUCCESS)
        {
            break;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));

    return nBMLRe;
}

#if !defined(FSR_NBL2)
/**
 *  @brief      This function checks whether the given nVsn is included in RO
 *  @n          Partition or not.
 *
 *  @param [in]  nVun    : virtual unit number
 *  @param [in] *pstVol  : pointer to VolCxt structure
 *
 *  @return     TRUE32
 *  @return     FALSE32
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PRIVATE BOOL32
_IsROPartition(UINT32       nVun,
               BmlVolCxt   *pstVol)
{
    UINT32         n1stVun          = 0;    /* the 1st Vun          */
    UINT32         nLastVun         = 0;    /* the last Vun         */
    UINT32         nNumOfPartEntry  = 0;    /* # of part entry      */
    FSRPartEntry  *pstPEntry;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVun:%d)\r\n"), __FSR_FUNC__, nVun));

    /* Get pointer of PartEntry and nNumOfPartEntry */
    pstPEntry       = pstVol->pstPartI->stPEntry;
    nNumOfPartEntry = pstVol->pstPartI->nNumOfPartEntry;

    /* 
     * If pstVol->bPreProgram is TRUE32,
     * attribute of RO Partition has Read/Write in flasher mode.
     */
    if (pstVol->bPreProgram == TRUE32)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));
        return FALSE32;
    }
    
    while (nNumOfPartEntry--)
    {
        /* if the attribute of the partition entry is ReadOnly, compare 
           the given nVsn with the 1stVsn and lastVsn of the partition area */
        if ((pstPEntry->nAttr & FSR_BML_PI_ATTR_RO) != 0)
        {
            n1stVun     = pstPEntry->n1stVun;
            nLastVun    = (n1stVun + pstPEntry->nNumOfUnits) -1;

            if ((n1stVun <= nVun) && (nLastVun >= nVun))
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVun:%d)\r\n"), __FSR_FUNC__, nVun));
                return FSR_OAM_GetROLockFlag();
            }
        }

        pstPEntry++;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));

    return FALSE32;
}
#endif

#if !defined(FSR_NBL2)
/**
 *  @brief      This function checks global wareleveling attributes.
 *
 *  @param [in] *pstPartI: FSRPartI structure pointer
 *  @param [in]  nPartIdx: Partition Index of the 1st global wereleveling attr.
 *  @param [in]  nGRPIdx : STL Group ID attribute for Global wareleveling.
 *
 *  @return     TRUE32
 *  @return     FALSE32
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PRIVATE BOOL32
_ChkWLAttr(FSRPartI *pstPartI,
           UINT32    nPartIdx,
           UINT16    nGRPIdx)
{
    UINT32  nIdx            = 0;            /* Temporary Index                  */
    UINT32  nlinearIdx      = 0;            /* Temporary Index 2                */
    UINT32  nNumOfPartEntry = 0;            /* # of partEntries                 */
    UINT32  nBaseAttr       = 0x00000000;   /* The base attribute of nPartIdx   */
    UINT32  nAttr           = 0x00000000;   /* Partition attribute              */

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nPartIdx:%d, nGRPIdx: 0x%x)\r\n"),
                                    __FSR_FUNC__, nPartIdx, nGRPIdx));

    /* Set nNumOfPartEntry */
    nNumOfPartEntry = pstPartI->nNumOfPartEntry;
    nBaseAttr       = (UINT32) pstPartI->stPEntry[nPartIdx].nAttr;
    nlinearIdx         = nPartIdx;

    /* Check whther nBaseAttr is a STL partition or not */
    if ((nBaseAttr & FSR_BML_PI_ATTR_STL) != FSR_BML_PI_ATTR_STL)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nPartIdx:%d, nGRPIdx: 0x%x, Non-STL Patition) / %d line\r\n"),
                                        __FSR_FUNC__, nPartIdx, nGRPIdx, __LINE__));
        return FALSE32;
    }

    for (nIdx = nPartIdx; nIdx < nNumOfPartEntry; nIdx++)
    {
        nAttr       = pstPartI->stPEntry[nIdx].nAttr;

        /* Check whether Group Partition ID is same or not */
        if (nAttr & nGRPIdx)
        {
            /* Check the partition ID whether it is linear or not */
            if (nIdx != nlinearIdx)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nPartIdx:%d, nGRPIdx: 0x%x, Non-Linear Patition) / %d line\r\n"),
                                                __FSR_FUNC__, nPartIdx, nGRPIdx, __LINE__));
                return FALSE32;
            }

            /* Set nOldIdx to nIdx to check a linear character */
            nlinearIdx++;

            /* Check whether new partition attribute and a base partition
             * is same or not.*/
            if (nAttr != nBaseAttr)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nPartIdx:%d, nGRPIdx: 0x%x, No same Patition) / %d line\r\n"),
                                                __FSR_FUNC__, nPartIdx, nGRPIdx, __LINE__));
                return FALSE32;
            }
        }

    } /* End of "for (nIdx = nPartIdx;...)" */

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));

    return TRUE32;
}

#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function checks whether the given pstPartI is valid or not.
 *
 *  @param [in]  nVol    : volume number
 *  @param [in] *pstPartI: pointer to FSRPartI structure pointer
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_INVALID_PARTITION
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PRIVATE INT32
_ChkPIValidity(UINT32    nVol,
               FSRPartI *pstPartI)
{
    UINT32        nIdx      = 0;            /* Temporary index                          */
    UINT32        nIdx2     = 0;            /* Temporary index 2                        */
    UINT32        nNumOfPartEntry   = 0;    /* # of partEntries                         */
    UINT32        nLastVun  = 0;            /* Temporary variable for storing lastVbn   */
    UINT32        nPartAttr = 0x00000000;   /* Partition attributes                     */
    UINT16        nPartID   = 0x0000;       /* Partition ID                             */
    BOOL32        bOnGRP0   = FALSE32;      /* Flag to check GRP0 Attributes            */
    BOOL32        bOnGRP1   = FALSE32;      /* Flag to check GRP1 Attributes            */
    BOOL32        bRet      = TRUE32;       /* Temporary return value                   */
    BmlVolCxt    *pstVol;
    FSRPartEntry *pstPEntry;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));

    /* Get a pointer for device context */
    pstVol = _GetVolCxt(nVol);

    /* if pstPartI is NULL, use the loaded pstVol->stPartI */
    if (pstPartI == NULL)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, pstPartI:0x%x, nRe:FSR_BML_INVALID_PARTITION | FSR_BML_NO_PARTI) / %d line\r\n"),
                                         __FSR_FUNC__, nVol, pstPartI, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
        return FSR_BML_INVALID_PARTITION | FSR_BML_NO_PARTI;
    }

    /* Get nNumOfPartEntry, pstPEntry */
    nNumOfPartEntry = pstPartI->nNumOfPartEntry;
    pstPEntry       = &(pstPartI->stPEntry[0]);

    /* check whether Partition Information is vaild or not */
    if (nNumOfPartEntry == 0 || nNumOfPartEntry > FSR_BML_MAX_PARTENTRY)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:FSR_BML_INVALID_PARTITION | FSR_BML_UNSUPPORTED_PARTENTRY) / %d line\r\n"),
                                        __FSR_FUNC__, nVol, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
        return FSR_BML_INVALID_PARTITION | FSR_BML_UNSUPPORTED_PARTENTRY;
    }

    /* check whether ID is duplicated */
    for (nIdx = 0; nIdx < nNumOfPartEntry; nIdx++)
    {
        for (nIdx2 = nIdx + 1; nIdx2 < nNumOfPartEntry; nIdx2++)
        {
            if (pstPEntry[nIdx].nID == pstPEntry[nIdx2].nID)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:FSR_BML_INVALID_PARTITION | FSR_BML_DUPLICATE_PARTID) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, __LINE__));
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
                return FSR_BML_INVALID_PARTITION | FSR_BML_DUPLICATE_PARTID;
            }
        }
    }

    for (nIdx = 0; nIdx < nNumOfPartEntry; nIdx++)
    {
        /* Get nPartAttr */
        nPartAttr   = pstPEntry[nIdx].nAttr;
        nPartID     = pstPEntry[nIdx].nID;

        /* Support the STL global wareleveling */
        if ((nPartAttr & FSR_BML_PI_ATTR_STL_GLOBAL_WL_GRP0) &&
            (nPartAttr & FSR_BML_PI_ATTR_STL_GLOBAL_WL_GRP1))
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:FSR_BML_INVALID_PARTITION | FSR_BML_INVALID_GLOBAL_WL) / %d line\r\n"),
                                            __FSR_FUNC__, nVol, __LINE__));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
            return FSR_BML_INVALID_PARTITION | FSR_BML_INVALID_GLOBAL_WL;
        }

        if (nPartAttr & FSR_BML_PI_ATTR_STL_GLOBAL_WL_GRP0)
        {
            if (bOnGRP0 == FALSE32)
            {
                bOnGRP0 = TRUE32;
                bRet    = _ChkWLAttr(pstPartI,
                                     nIdx,
                                     FSR_BML_PI_ATTR_STL_GLOBAL_WL_GRP0);
                if (bRet == FALSE32)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:FSR_BML_INVALID_PARTITION | FSR_BML_INVALID_GLOBAL_WL)\r\n"),
                                                    __FSR_FUNC__, nVol));
                    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
                    return FSR_BML_INVALID_PARTITION | FSR_BML_INVALID_GLOBAL_WL;
                }
            }
            /* Remove the attributes for global wereleveling GRP0 */
            nPartAttr &= ~FSR_BML_PI_ATTR_STL_GLOBAL_WL_GRP0;
        }

        if (nPartAttr & FSR_BML_PI_ATTR_STL_GLOBAL_WL_GRP1)
        {
            if (bOnGRP1 == FALSE32)
            {
                bOnGRP1 = TRUE32;
                bRet    = _ChkWLAttr(pstPartI,
                                     nIdx,
                                     FSR_BML_PI_ATTR_STL_GLOBAL_WL_GRP1);
                if (bRet == FALSE32)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:FSR_BML_INVALID_PARTITION | FSR_BML_INVALID_GLOBAL_WL)\r\n"),
                                                    __FSR_FUNC__, nVol));
                    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
                    return FSR_BML_INVALID_PARTITION | FSR_BML_INVALID_GLOBAL_WL;
                }
            }
            /* Remove the attributes for global wereleveling GRP1 */
            nPartAttr &= ~FSR_BML_PI_ATTR_STL_GLOBAL_WL_GRP1;
        }

        /* Check the attributes by the NAND type*/
        /* SLC OneNAND or SLC NAND*/
        if (((pstVol->nNANDType & FSR_LLD_SLC_NAND)     == FSR_LLD_SLC_NAND) ||
            ((pstVol->nNANDType & FSR_LLD_SLC_ONENAND)  == FSR_LLD_SLC_ONENAND))
        {
            if ((nPartAttr & FSR_BML_PI_ATTR_SLC) != FSR_BML_PI_ATTR_SLC)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:FSR_BML_INVALID_PARTITION | FSR_BML_INVALID_NAND_TYPE) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, __LINE__));
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
                return FSR_BML_INVALID_PARTITION | FSR_BML_INVALID_NAND_TYPE;
            }
        }
        /* MLC NAND */
        else if ((pstVol->nNANDType & FSR_LLD_MLC_NAND)  == FSR_LLD_MLC_NAND)
        {
            if ((nPartAttr & FSR_BML_PI_ATTR_MLC) != FSR_BML_PI_ATTR_MLC)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:FSR_BML_INVALID_PARTITION | FSR_BML_INVALID_NAND_TYPE) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, __LINE__));
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
                return FSR_BML_INVALID_PARTITION | FSR_BML_INVALID_NAND_TYPE;
            }
        }
        /*FLEX-ONENAND */
        else 
        {
            if ((nIdx == 0) && ((nPartAttr & FSR_BML_PI_ATTR_SLC) != FSR_BML_PI_ATTR_SLC))
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:FSR_BML_INVALID_PARTITION | FSR_BML_INVALID_NAND_TYPE) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, __LINE__));
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
                return FSR_BML_INVALID_PARTITION | FSR_BML_INVALID_NAND_TYPE;
            }

            if (((nPartAttr & (FSR_BML_PI_ATTR_SLC | FSR_BML_PI_ATTR_MLC)) == (FSR_BML_PI_ATTR_SLC | FSR_BML_PI_ATTR_MLC)) ||
                ((nPartAttr & (FSR_BML_PI_ATTR_SLC | FSR_BML_PI_ATTR_MLC)) == 0))
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:FSR_BML_INVALID_PARTITION | FSR_BML_INVALID_NAND_TYPE) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, __LINE__));
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
                return FSR_BML_INVALID_PARTITION | FSR_BML_INVALID_NAND_TYPE;
            }
        }

        /* Remove the attributes for SLC and MCL */
        if (nPartAttr & FSR_BML_PI_ATTR_SLC)
        {
           nPartAttr &= ~FSR_BML_PI_ATTR_SLC;
        }

        if (nPartAttr & FSR_BML_PI_ATTR_MLC)
        {
            nPartAttr &= ~FSR_BML_PI_ATTR_MLC;
        }

        if ((nPartAttr & (FSR_BML_PI_ATTR_STL | FSR_BML_PI_ATTR_DPW)) == 
            (FSR_BML_PI_ATTR_STL | FSR_BML_PI_ATTR_DPW))
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:FSR_BML_INVALID_PARTITION | FSR_BML_COEXIST_STL_DPW_ATTR) / %d line\r\n"),
                                            __FSR_FUNC__, nVol, __LINE__));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
            return FSR_BML_INVALID_PARTITION | FSR_BML_COEXIST_STL_DPW_ATTR;
        }

        if (nPartAttr  & FSR_BML_PI_ATTR_STL)
        {
            if (((nPartID & FSR_PARTID_MASK) != FSR_PARTID_STL0) &&
                ((nPartID & FSR_PARTID_MASK) != FSR_PARTID_STL1) &&
                ((nPartID & FSR_PARTID_MASK) != FSR_PARTID_STL2) &&
                ((nPartID & FSR_PARTID_MASK) != FSR_PARTID_STL3) &&
                ((nPartID & FSR_PARTID_MASK) != FSR_PARTID_STL4) &&
                ((nPartID & FSR_PARTID_MASK) != FSR_PARTID_STL5) &&
                ((nPartID & FSR_PARTID_MASK) != FSR_PARTID_STL6) &&
                ((nPartID & FSR_PARTID_MASK) != FSR_PARTID_STL7))
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:FSR_BML_INVALID_PARTITION | FSR_BML_NO_STL_PARTID) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, __LINE__));
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
                return FSR_BML_INVALID_PARTITION | FSR_BML_NO_STL_PARTID;
            }

            if (nPartAttr & FSR_BML_PI_ATTR_PROCESSOR_ID0)
            {
                nPartAttr &= ~FSR_BML_PI_ATTR_PROCESSOR_ID0;
            }

            if (nPartAttr & FSR_BML_PI_ATTR_PROCESSOR_ID1)
            {
                nPartAttr &= ~FSR_BML_PI_ATTR_PROCESSOR_ID1;
            }
        }

        /* Remove the attributes for STL, DPW, BOORLOAD, SECURE */
        if (nPartAttr & FSR_BML_PI_ATTR_STL)
        {
            nPartAttr &= ~FSR_BML_PI_ATTR_STL;
        }

        if (nPartAttr & FSR_BML_PI_ATTR_DPW)
        {
            nPartAttr &= ~FSR_BML_PI_ATTR_DPW;
        }

        if (nPartAttr & FSR_BML_PI_ATTR_ENTRYPOINT)
        {
            nPartAttr &= ~FSR_BML_PI_ATTR_ENTRYPOINT;
        }

        if (nPartAttr & FSR_BML_PI_ATTR_BOOTLOADING)
        {
            nPartAttr &= ~FSR_BML_PI_ATTR_BOOTLOADING;
        }

        if (nPartAttr & FSR_BML_PI_ATTR_PREWRITING)
        {
            nPartAttr &= ~FSR_BML_PI_ATTR_PREWRITING;
        }

        /* check whether nAttr is valid or not */
        if (!((nPartAttr & (FSR_BML_PI_ATTR_LOCKTIGHTEN | FSR_BML_PI_ATTR_RO)) == nPartAttr) &&
            !((nPartAttr & (FSR_BML_PI_ATTR_LOCK | FSR_BML_PI_ATTR_RO)) == nPartAttr) &&
            !((nPartAttr & FSR_BML_PI_ATTR_RO) == nPartAttr) &&
            !((nPartAttr & FSR_BML_PI_ATTR_RW) == nPartAttr))
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:FSR_BML_INVALID_PARTITION | FSR_BML_INVALID_LOCK_ATTR) / %d line\r\n"),
                                            __FSR_FUNC__, nVol, __LINE__));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
            return FSR_BML_INVALID_PARTITION | FSR_BML_INVALID_LOCK_ATTR;
        }

        /* Remove the attributes for lock or unlock */
        nPartAttr &= ~(FSR_BML_PI_ATTR_LOCKTIGHTEN | FSR_BML_PI_ATTR_LOCK | FSR_BML_PI_ATTR_RO | FSR_BML_PI_ATTR_RW);

        if (nPartAttr != 0)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:FSR_BML_INVALID_PARTITION | FSR_BML_UNSUPPORTED_ATTR) / %d line\r\n"),
                                            __FSR_FUNC__, nVol, __LINE__));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
            return FSR_BML_INVALID_PARTITION | FSR_BML_UNSUPPORTED_ATTR;
        }

        /* check whether nNumOfUnits of each entry is 0 or not.
           if nNumOfUnits of each entry is 0, it is invalid entry */
        if (pstPEntry[nIdx].nNumOfUnits == 0)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:FSR_BML_INVALID_PARTITION | FSR_BML_NO_UNITS) / %d line\r\n"),
                                            __FSR_FUNC__, nVol, __LINE__));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
            return FSR_BML_INVALID_PARTITION | FSR_BML_NO_UNITS;
        }

        /* Calculate nLastVun of this partition */
        nLastVun = pstPEntry[nIdx].n1stVun + pstPEntry[nIdx].nNumOfUnits - 1;

        /* check whether last Vun of each entry is greater than 
           the last unit number in volume or not.
           if the last vun is greater than it, it is invalid entry */
        if (nLastVun > pstVol->nLastUnit)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:FSR_BML_INVALID_PARTITION | FSR_BML_UNSUPPORTED_UNITS) / %d line\r\n"),
                                            __FSR_FUNC__, nVol, __LINE__));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
            return FSR_BML_INVALID_PARTITION | FSR_BML_UNSUPPORTED_UNITS;
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));

    return FSR_BML_SUCCESS;
}
#endif  /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function changes the state of data blocks
 *  @n          having lock-tight attribution to lock-tight.
 *
 *  @param [in]  nVol        : Volume number
 *  @param [in]  n1stVun     : The 1st Vun of current partition
 *  @param [in]  nNumOfUnits : # of units in current partition
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_INVALID_PARAM
 *  @return     Some LLD errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PRIVATE INT32
_LockTightData(UINT32       nVol,
               UINT32       n1stVun,
               UINT32       nNumOfUnits)
{
    UINT32      nDieIdx = 0;        /* Die Index                            */
    UINT32      n1stSbn = 0;        /* The 1st semi-physical block number   */
    UINT32      nPDev   = 0;        /* Physical device number               */
    UINT32      nPbn    = 0;        /* Physical block number                */
    UINT32      nByteRet= 0;        /* # of bytes returned                  */
    UINT32      nLockStat   = FSR_LLD_BLK_STAT_LOCKED;/* Lock state of block*/
    INT32       nBMLRe  = FSR_BML_SUCCESS;
    INT32       nLLDRe  = FSR_LLD_SUCCESS;
    BmlVolCxt  *pstVol;
    BmlDevCxt  *pstDev;
    BmlDieCxt  *pstDie;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, n1stVun: %d, nNumOfUnits: %d)\r\n"),
                                    __FSR_FUNC__, nVol, n1stVun, nNumOfUnits));

    /* Get pointer to BmlVolCxt structure */
    pstVol  = _GetVolCxt(nVol);

    /* Get the 1st Sbn of current partition */
    n1stSbn = n1stVun * pstVol->nNumOfWays;

    /* Get pointer to the 1st device context in a volume */
    nPDev   = nVol << DEVS_PER_VOL_SHIFT;
    pstDev  = _GetDevCxt(nPDev);

    /* Get pointer to the 1st die context in a device */
    pstDie = pstDev->pstDie[0];

    /* Set Sbn and Pbn */
    for (nDieIdx = 0; nDieIdx < pstVol->nNumOfDieInDev; nDieIdx++)
    {
        pstDie->nCurSbn[nDieIdx] = (UINT16)(n1stSbn + nDieIdx);
        pstDie->nCurPbn[nDieIdx] = (UINT16)(n1stSbn + nDieIdx);
    }

    /* Get PBN of the current partition */
    pstDie->nNumOfLLDOp = 0;
    _GetPBN(pstDie->nCurSbn[0], pstVol, pstDie);

    /* Set Pbn */
    nPbn = (UINT32) pstDie->nCurPbn[0];

    do
    {
        /* Check the lock state of the current partition */
        nLLDRe = pstVol->LLD_IOCtl(nPDev,
                                   FSR_LLD_IOCTL_GET_LOCK_STAT,
                                   (UINT8 *) &nPbn,
                                   sizeof(nPbn),
                                   (UINT8 *) &nLockStat,
                                   sizeof(nLockStat),
                                   &nByteRet);
        if (nLLDRe != FSR_LLD_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, n1stVun: %d, nNumOfUnits: %d)\r\n"),
                                            __FSR_FUNC__, n1stVun, nNumOfUnits));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_IOCtl(nPDev:%D, nCode: FSR_LLD_IOCTL_GET_LOCK_STAT, nPbn:%d, nRe:0x%x) / %d line\r\n"),
                                            nPDev, nPbn, nLLDRe, __LINE__));
            nBMLRe = nLLDRe;
            break;
        }

        /* Unlocked block can not be changed to locked-tight directly
         * So, this block should be set to locked block.
         */
        if (nLockStat == FSR_LLD_BLK_STAT_UNLOCKED)
        {
            nBMLRe = _SetBlkState(nVol,
                                  n1stVun,
                                  nNumOfUnits,
                                  FSR_LLD_IOCTL_LOCK_BLOCK);
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, n1stVun: %d, nNumOfUnits: %d, nRe: 0x%x)\r\n"),
                                                __FSR_FUNC__, n1stVun, nNumOfUnits, nBMLRe));
                break;
            }
        }

        nBMLRe = _SetBlkState(nVol,
                              n1stVun,
                              nNumOfUnits,
                              FSR_LLD_IOCTL_LOCK_TIGHT);
        if (nBMLRe != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, n1stVun: %d, nNumOfUnits: %d, nRe: 0x%x)\r\n"),
                                            __FSR_FUNC__, n1stVun, nNumOfUnits, nBMLRe));
            break;
        }
    } while(0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));

    return nBMLRe;
}
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function changes the state of replaced blocks
 *  @n          having lock-tight attribution to lock-tight.
 *
 *  @param [in]  nVol        : Volume number
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     Some FSR_BBM errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PRIVATE INT32
_LockTightRsv(UINT32       nVol)
{
    UINT32      nDevIdx = 0;    /* Device index             */
    UINT32      nPDev   = 0;    /* Physical device number   */
    UINT32      nDieIdx = 0;    /* Die index                */
    INT32       nBMLRe  = FSR_BML_SUCCESS;
    BmlVolCxt  *pstVol;
    BmlDevCxt  *pstDev;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));

    /* Get pointer to BmlVolCxt structure */
    pstVol = _GetVolCxt(nVol);

    for (nDevIdx = 0; nDevIdx < pstVol->nNumOfDev; nDevIdx++)
    {
        /* Get pointer to volume context */
        nPDev = (nVol << DEVS_PER_VOL_SHIFT) + nDevIdx;
        pstDev = _GetDevCxt(nPDev);

        for (nDieIdx = 0; nDieIdx < pstVol->nNumOfDieInDev; nDieIdx++)
        {
            /* Lock-tight reservoir by die */
            nBMLRe = FSR_BBM_LockTight(pstVol,
                                       pstDev,
                                       nDieIdx);
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   FSR_BBM_LockTight(nPDev:%d, nDieIdx:%d, nRe:0x%x) / %d line\r\n"),
                                                nPDev, nDieIdx, nBMLRe, __LINE__));
                break;
            }
        } /* end of for (nDie = 0;...) */

        /* error case */
        if (nBMLRe != FSR_BML_SUCCESS)
        {
            break;
        }

    } /* end of "for (nPDev = 0; ...)" */

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));

    return nBMLRe;
}
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function changes the partition attribution by new partition attribution.
 *
 *  @param [in]  nVol       : Volume number
 *  @param [in] *pstPA      : Pointer to FSRChangePA data structure
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     Some FSR_BBM errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PRIVATE INT32
_ChangePA(UINT32       nVol,
          FSRChangePA *pstPA)
{
    UINT32          nCnt        = 0;    /* Temporary # of partition entry   */
    UINT32          n1stSbn     = 0;    /* The 1st Sbn in the partition     */
    UINT32          nPDev       = 0;    /* Physical Device number           */
    UINT32          nByteRet    = 0;    /* # of bytes of return             */
    UINT32          nPbn        = 0;    /* Physical Block Number            */
    UINT32          nIdx        = 0;    /* Temporary index                  */
    UINT32          nPEIdx      = 0;    /* Temporary partition entry index  */
    UINT32          nPEntryIdx  = 0;    /* Partition entry index            */
    UINT32          nNumOfPEntry= 0;    /* # of partitions to change partition attribute    */
    UINT32          nPartAttr   = 0x00000000;               /* Partition attributes         */
    UINT32          nLockStat   = FSR_LLD_BLK_STAT_LOCKED;  /* State of the current block   */
    UINT32          nCode       = FSR_LLD_IOCTL_LOCK_BLOCK; /* New patition attribution     */
    INT32           nLLDRe      = FSR_LLD_SUCCESS;
    INT32           nBMLRe      = FSR_BML_SUCCESS;
    BOOL32          bFound      = FALSE32;
    BOOL32          bFirstPE    = TRUE32;
    BOOL32          bEnPartAttrChg = FALSE32;
    BmlVolCxt      *pstVol;
    BmlDevCxt      *pstDev;
    BmlDieCxt      *pstDie;
    FSRPartI       *pstPI;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, pstPA: 0x%x)\r\n"), __FSR_FUNC__, nVol, pstPA));

    /* Get pointer to VolCxt data structure */
    pstVol = _GetVolCxt(nVol);

    /* check whether new attribute is valid or not */
    nPartAttr = pstPA->nNewAttr;
    if (!((nPartAttr & (FSR_BML_PI_ATTR_LOCKTIGHTEN | FSR_BML_PI_ATTR_RO)) == nPartAttr)&&
        !((nPartAttr & (FSR_BML_PI_ATTR_LOCK | FSR_BML_PI_ATTR_RO)) == nPartAttr)       &&
        !((nPartAttr & FSR_BML_PI_ATTR_RO) == nPartAttr)                                &&
        !((nPartAttr & FSR_BML_PI_ATTR_RW) == nPartAttr))
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] New attribute is not valid \r\n")));    
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nRe:FSR_BML_INVALID_PARAM)\r\n"),
                                        __FSR_FUNC__, nVol));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
        return FSR_BML_INVALID_PARAM;
    }

    if((nPartAttr & FSR_BML_PI_ATTR_STL) == FSR_BML_PI_ATTR_STL)
    {
        /* Check to enable of Part Attr changing */
        bEnPartAttrChg = FSR_BML_GetPartAttrChg (nVol, pstPA->nPartID, FSR_BML_FLAG_NONE);
        if (bEnPartAttrChg)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   This partition is locked by STL \r\n")));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nRe:FSR_BML_INVALID_PARAM)\r\n"),
                                            __FSR_FUNC__, nVol));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
            return FSR_BML_INVALID_PARAM;
        }
    }

    /* Get pointer to Partition Information */
    pstPI  = pstVol->pstPartI;

    /************************************************************************************/
    /* <Procedure of "FSR_BML_IOCTL_CHANGE_PART_ATTR">                                  */
    /* STEP1. Check original attributes of nPartID                                      */
    /* STEP2. Check the original partition attribute                                    */
    /*        whether it has STL global wareleveling attribute or not.                  */
    /* STPE3. Check lock state of the current partition                                 */
    /* STEP4. Change the current partition attribute by new attribute.                  */
    /************************************************************************************/

    for (nCnt = 0; nCnt < pstPI->nNumOfPartEntry; nCnt++)
    {
        /****************************************************/
        /* STEP1. Check original attributes of nPartID      */
        /****************************************************/
        if (pstPI->stPEntry[nCnt].nID == pstPA->nPartID)
        {
            bFound = TRUE32;

            /********************************************************************/
            /* STEP2. Check the original partition attribute                    */
            /*        whether it has STL global wareleveling attribute or not   */
            /********************************************************************/
            nPartAttr = pstPI->stPEntry[nCnt].nAttr;
            if (nPartAttr & FSR_BML_PI_ATTR_STL_GLOBAL_WL_GRP0)
            {
                /* Find another partition that had a STL global wareleveing attribute */
                for (nIdx = 0; nIdx < pstPI->nNumOfPartEntry; nIdx++)
                {
                    if (pstPI->stPEntry[nIdx].nAttr & FSR_BML_PI_ATTR_STL_GLOBAL_WL_GRP0)
                    {
                        if (bFirstPE == TRUE32)
                        {
                            nPEntryIdx  = nIdx;
                            bFirstPE    = FALSE32;
                        }
                        nNumOfPEntry++;
                    }
                }
            }
            else if (nPartAttr & FSR_BML_PI_ATTR_STL_GLOBAL_WL_GRP1)
            {
                /* Find another partition that had a STL global wareleveing attribute */
                for (nIdx = 0; nIdx < pstPI->nNumOfPartEntry; nIdx++)
                {
                    if (pstPI->stPEntry[nIdx].nAttr & FSR_BML_PI_ATTR_STL_GLOBAL_WL_GRP1)
                    {
                        if (bFirstPE == TRUE32)
                        {
                            nPEntryIdx  = nIdx;
                            bFirstPE    = FALSE32;
                        }
                        nNumOfPEntry++;
                    }
                }
            }
            else /* Case: No STL global wareleveling attribute */
            {
                nPEntryIdx      = nCnt;
                nNumOfPEntry    = 1;
            }
        }
    }

    if (bFound == TRUE32)
    {
        for (nCnt = 0; nCnt < nNumOfPEntry; nCnt++)
        {
            nPEIdx = nPEntryIdx + nCnt;

            /********************************************************************/
            /* STEP3. Check lock state of the current partition                 */
            /* STEP3-1. Read lock state of the 1st block in current partition.  */
            /* STEP3-2. If this lock state is lock-tighen, stop this routine.   */
            /********************************************************************/

            /********************************************************************/
            /* STEP3-1. Read lock state of the 1st block in current partition   */
            /********************************************************************/
            /* STEP3-1-1. Get the 1st Sbn of current partition */
            n1stSbn = pstPI->stPEntry[nPEIdx].n1stVun * pstVol->nNumOfWays;

            /* STEP3-1-2. Get pointer to the 1st device context in a volume */
            nPDev   = nVol << DEVS_PER_VOL_SHIFT;
            pstDev  = _GetDevCxt(nPDev);

            /* STEP3-1-3. Get pointer to the 1st die context in a device */
            pstDie = pstDev->pstDie[0];

            /* STEP3-1-4. Set Sbn and Pbn */
            pstDie->nCurSbn[0] = (UINT16)n1stSbn;
            pstDie->nCurPbn[0] = (UINT16)n1stSbn;

            /* STEP3-1-5. Get PBN of the current partition */
            pstDie->nNumOfLLDOp = 0;
            _GetPBN(pstDie->nCurSbn[0], pstVol, pstDie);

            nPbn = (UINT32) pstDie->nCurPbn[0];

            /* STEP3-1-5. Check the lock state of the current partition */
            nLLDRe = pstVol->LLD_IOCtl(nPDev,
                                       FSR_LLD_IOCTL_GET_LOCK_STAT,
                                       (UINT8 *) &nPbn,
                                       sizeof(nPbn),
                                       (UINT8 *) &nLockStat,
                                       sizeof(nLockStat),
                                       &nByteRet);
            if (nLLDRe != FSR_LLD_SUCCESS)
            {
                nBMLRe = FSR_BML_CANT_CHANGE_PART_ATTR;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nRe:FSR_BML_CANT_CHANGE_PART_ATTR)\r\n"),
                                                __FSR_FUNC__, nVol));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_IOCtl(nPDev:%d, nCode: FSR_LLD_IOCTL_GET_LOCK_STAT, nPbn:%d, nRe:0x%x) / %d line\r\n"),
                                                nPDev, nPbn, nLLDRe, __LINE__));
                break;
            }

            /********************************************************************/
            /* STEP3-2. If this lock state is lock-tighen, stop this routine.   */
            /********************************************************************/
            if ((nLockStat == FSR_LLD_BLK_STAT_LOCKED_TIGHT) &&
                ((pstPA->nNewAttr & FSR_BML_PI_ATTR_LOCKTIGHTEN) != FSR_BML_PI_ATTR_LOCKTIGHTEN))
            {
                nBMLRe = FSR_BML_CANT_CHANGE_PART_ATTR;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nRe:FSR_BML_CANT_CHANGE_PART_ATTR, LockTight Block)\r\n"),
                                                __FSR_FUNC__, nVol));
                break;
            }

            /************************************************************************/
            /* STEP4. Change the current partition attribute by new attribute       */
            /* STEP4-1. Change the state of blocks in the current partition         */
            /* STEP4-2. Update the attributes of this partition to new attributes   */
            /************************************************************************/

            /************************************************************************/
            /* STEP4-1. Change the state of blocks in the current partition         */
            /************************************************************************/
            if (pstPA->nNewAttr & FSR_BML_PI_ATTR_LOCKTIGHTEN)
            {
                nCode = FSR_LLD_IOCTL_LOCK_TIGHT;
                if (nLockStat == FSR_LLD_BLK_STAT_UNLOCKED)
                {
                    /* Unlocked block can not be changed to locked-tight directly
                     * So, this block should be set to locked block.
                     */
                    nBMLRe = _SetBlkState(nVol,
                                          pstPI->stPEntry[nPEIdx].n1stVun,
                                          pstPI->stPEntry[nPEIdx].nNumOfUnits,
                                          FSR_LLD_IOCTL_LOCK_BLOCK);
                    if (nBMLRe != FSR_BML_SUCCESS)
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nRe:0x%x)\r\n"),
                                                        __FSR_FUNC__, nVol, nBMLRe));
                        break;
                    }
                } /* End of "if (nLockStat == FSR_LLD_BLK_STAT_UNLOCKED)" */
            }
            else if (pstPA->nNewAttr & FSR_BML_PI_ATTR_LOCK)
            {
                nCode = FSR_LLD_IOCTL_LOCK_BLOCK;
            }
            else /* FSR_BML_PI_ATTR_RW or FSR_BML_PI_ATTR_RO*/
            {
                nCode = FSR_LLD_IOCTL_UNLOCK_BLOCK;
            }

            nBMLRe = _SetBlkState(nVol,
                                  pstPI->stPEntry[nPEIdx].n1stVun,
                                  pstPI->stPEntry[nPEIdx].nNumOfUnits,
                                  nCode);
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nRe:0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nBMLRe));
                break;
            }

            /************************************************************************/
            /* STEP4-2. Update the attributes of this partition                     */
            /************************************************************************/
            /* STEP4-2-1. Remove RO/RW/LOCK/LOCKTIGHTEN from original attribute */
            pstPI->stPEntry[nPEIdx].nAttr &= ~(FSR_BML_PI_ATTR_RO         |
                                               FSR_BML_PI_ATTR_RW         |
                                               FSR_BML_PI_ATTR_LOCK       |
                                               FSR_BML_PI_ATTR_LOCKTIGHTEN);

            /* STEP4-2-2. Update attributes using nNewAttr*/
            if ((pstPA->nNewAttr & FSR_BML_PI_ATTR_RW) != 0)
            {
                pstPI->stPEntry[nPEIdx].nAttr |=  FSR_BML_PI_ATTR_RW;
            }
            else /* Default: FSR_BML_PI_ATTR_RO */
            {
                pstPI->stPEntry[nPEIdx].nAttr |=  FSR_BML_PI_ATTR_RO;

                if ((pstPA->nNewAttr & FSR_BML_PI_ATTR_LOCK) != 0)
                {
                    pstPI->stPEntry[nPEIdx].nAttr |=  FSR_BML_PI_ATTR_LOCK;
                }
                else if ((pstPA->nNewAttr & FSR_BML_PI_ATTR_LOCKTIGHTEN) != 0)
                {
                    pstPI->stPEntry[nPEIdx].nAttr |=  FSR_BML_PI_ATTR_LOCKTIGHTEN;
                }
            } /* End of "if ((pstPA->nNewAttr...)" */
        } /* End of "for (nCnt = 0; nCnt < nNumOfPEntry; nCnt++)" */
    }
    else /* if (bFound != TRUE32) */
    {
        nBMLRe = FSR_BML_CANT_FIND_PART_ID;
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nRe:0x%x)\r\n"),
                                        __FSR_FUNC__, nVol, nBMLRe));
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));

    return nBMLRe;
}
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function locks 1st block OTP or OTP block.
 *
 *  @param [in]  nVol        : Volume number
 *  @param [in]  nOTPFlag    : The type of OTP block
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     Some FSR_LLD errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PRIVATE INT32
_LockOTPBlk(UINT32  nVol,
            UINT32  nOTPFlag)
{
    UINT32      nPDev       = 0;                        /* Physical device number                   */
    UINT32      nOTPBlk     = 0x00000000;               /* The type of OTP block in LLD             */
    UINT32      nMinor      = 0x00000000;               /* Return value for the type of OTP block   */
    UINT32      nLockstat   = FSR_LLD_BLK_STAT_LOCKED;  /* The lock state of OTP block              */
    UINT32      nByteRet    = 0;                        /* # of bytes returned                      */
    INT32       nBMLRe = FSR_BML_SUCCESS;
    INT32       nLLDRe = FSR_LLD_SUCCESS;
    BmlVolCxt  *pstVol;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, nOTPFlag: 0x%x)\r\n"), __FSR_FUNC__, nVol, nOTPFlag));

    /* Get pointer to VolCxt data structure */
    pstVol = _GetVolCxt(nVol);

    do
    {
        /* Get the state of OTP Block */
        nBMLRe = _GetOTPInfo(nVol,
                             &nLockstat);
        if (nBMLRe != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nOTPFlag: 0x%x, nRe:0x%x) / %d line\r\n"),
                                            __FSR_FUNC__, nVol, nOTPFlag, nBMLRe, __LINE__));
            break;
        }

        /* Set nOTPBlk by nOTPFlag */
        {
            nOTPBlk = 0x00000000;
            nMinor  = 0x00000000;
            if (nOTPFlag & FSR_BML_OTP_LOCK_1ST_BLK)
            {
                if (nLockstat & FSR_LLD_OTP_1ST_BLK_LOCKED)
                {
                    nBMLRe  =   FSR_BML_ALREADY_OTP_LOCKED;
                    nMinor  |=  FSR_BML_OTP_1STBLK;
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nOTPFlag: 0x%x, nRe:0x%x) / %d line\r\n"),
                                                    __FSR_FUNC__, nVol, nOTPFlag, nBMLRe, __LINE__));
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   1st block OTP is already locked.\r\n")));
                }
                nOTPBlk |= FSR_LLD_OTP_LOCK_1ST_BLK;
            }

            if (nOTPFlag & FSR_BML_OTP_LOCK_OTP_BLK)
            {
                if (nLockstat & FSR_LLD_OTP_OTP_BLK_LOCKED)
                {
                    nBMLRe = FSR_BML_ALREADY_OTP_LOCKED;
                    nMinor  |=  FSR_BML_OTP_BLK;
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nOTPFlag: 0x%x, nRe:0x%x) / %d line\r\n"),
                                                    __FSR_FUNC__, nVol, nOTPFlag, nBMLRe, __LINE__));
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   OTP block is already locked.\r\n")));
                }
                nOTPBlk |= FSR_LLD_OTP_LOCK_OTP_BLK;
            }

            /* Set nBMLRe by nMinor */
            nBMLRe |= nMinor;

            if (nBMLRe != FSR_BML_SUCCESS)
            {
                break;
            }
        }

        /* Set physical device number */
        nPDev = nVol << DEVS_PER_VOL_SHIFT;

        nLLDRe = pstVol->LLD_IOCtl(nPDev,
                                   FSR_LLD_IOCTL_OTP_LOCK,
                                   (UINT8 *) &nOTPBlk,
                                   sizeof(nOTPBlk),
                                   NULL,
                                   0,
                                   &nByteRet);
        if (nLLDRe != FSR_LLD_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nOTPFlag: 0x%x)\r\n"),
                                            __FSR_FUNC__, nVol, nOTPFlag));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_IOCtl(nPDev:%d, nCode: FSR_LLD_IOCTL_OTP_LOCK, nRe:0x%x) / %d line\r\n"),
                                            nPDev, nLLDRe, __LINE__));
            /* Return nLLDRe directly */
            nBMLRe = nLLDRe;
            break;
        }
    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));

    return nBMLRe;
}
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function fixs the number of SLC blocks and MLC blocks 
 *
 *  @param [in] nVol    : Volume number
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     Some FSR_LLD errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PRIVATE INT32
_FixSLCBoundary(UINT32  nVol)
{
    UINT32      nPDev   = 0;                /* Physical Device number   */
    UINT32      nDevIdx = 0;                /* Device Index             */
    UINT32      nDieIdx = 0;                /* Die Index                */
    BOOL32      bRet    = TRUE32;           /* Temporary return value   */
    INT32       nRe     = FSR_LLD_SUCCESS;  /* Temporary return value   */
    INT32       nBMLRe  = FSR_BML_SUCCESS;
    BmlVolCxt  *pstVol;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));

    /* Get pointer to VolCxt data structure */
    pstVol = _GetVolCxt(nVol);

    do
    {
        /* Check NAND type */
        if (pstVol->nNANDType != FSR_LLD_FLEX_ONENAND)
        {
            nBMLRe = FSR_BML_UNSUPPORTED_FUNCTION;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:FSR_BML_UNSUPPORTED_FUNCTION) / %d line\r\n"),
                                            __FSR_FUNC__, nVol, __LINE__));
            break;
        }

        for (nDevIdx = 0; nDevIdx < DEVS_PER_VOL; nDevIdx++)
        {
            /* Get physical device number */
            nPDev   =  nVol * DEVS_PER_VOL + nDevIdx;

            bRet    = _IsOpenedDev(nPDev);
            if (bRet == FALSE32)
            {
                continue;
            }

            for (nDieIdx = 0; nDieIdx < pstVol->nNumOfDieInDev; nDieIdx++)
            {
                /* Call FSR_BBM_FixSLCBoundary() by a die */
                nRe = FSR_BBM_FixSLCBoundary(pstVol,
                                             nPDev,
                                             nDieIdx);
                if (nRe != FSR_LLD_SUCCESS)
                {
                    nBMLRe = nRe;
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d)\r\n"), __FSR_FUNC__, nVol));
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   FSR_BBM_FixSLCBoundary(nPDev:%d, nDieIdx:%d, nRe:0x%x) / %d line\r\n"),
                                                    nPDev, nDieIdx, nBMLRe, __LINE__));
                    break;
                }
            }

            /* Error case: FSR_BBM_FixSLCBoundary() */
            if (nRe != FSR_LLD_SUCCESS)
            {
                break;
            }
        }
    } while(0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));

    return nBMLRe;
}
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function allots random-in data for 2x-device performing 1x opration.
 *  @n          Designed for use in FSR_BML_CopyBack
 *
 *  @param [in]  pstVol         : pointer to BmlVolCxt structure
 *  @param [in]  nPlnIdx        : Plane index
 *  @param [in]  nRndInCnt      : Random-in count for copyback
 *  @param [in]  pstRndInArgIn  : pointer to BMLRndInArg structure
 *  @param [out] pstRndInArgOut : pointer to LLDRndInArg structure
 *
 *  @return     None
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PRIVATE VOID
_AllotRanInData(BmlVolCxt       *pstVol,
                UINT32           nPlnIdx,
                UINT32           nRndInCnt,
                BMLRndInArg     *pstRndInArgIn,
                LLDRndInArg     *pstRndInArgOut)
{
    UINT32      nCntIdx     = 0;    /* Index for random-in count        */
    UINT32      nOffset     = 0;    /* Start offset of random-in data   */
    UINT32      nNumOfBytes = 0;    /* # of bytes for random-in data    */
    UINT8      *pBuf;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] --%s(nPlnIdx: %d, nRndInCnt: %d)\r\n"),
                                    __FSR_FUNC__, nPlnIdx, nRndInCnt));

    nCntIdx = 0;
    do
    {
        /* Get nOffset, nNumOfBytes and pBuf */
        nOffset     = pstRndInArgIn->nOffset;
        nNumOfBytes = pstRndInArgIn->nNumOfBytes;
        pBuf        = (UINT8*) pstRndInArgIn->pBuf;

        if (nOffset >= FSR_BML_CPBK_SPARE_START_OFFSET) /* case) random-in data for spare page */
        {
            nOffset -= FSR_BML_CPBK_SPARE_START_OFFSET;
            /* Base area Only */
            if ((nOffset + nNumOfBytes) <= FSR_SPARE_BUF_BASE_SIZE)
            {
                pstRndInArgOut->nOffset     = (UINT16)(nOffset + FSR_BML_CPBK_SPARE_START_OFFSET);
                pstRndInArgOut->nNumOfBytes = (UINT16)nNumOfBytes;
                pstRndInArgOut->pBuf        = pBuf;
            }
            else /* STLMetaExtension Only */
            {
                if (nOffset <= FSR_SPARE_BUF_SIZE_2KB_PAGE) /* Start offset: 1st plane*/
                {
                    if (nPlnIdx == 0)
                    {
                        pstRndInArgOut->nOffset     = (UINT16)(nOffset + FSR_BML_CPBK_SPARE_START_OFFSET);
                        pstRndInArgOut->nNumOfBytes = (UINT16)(FSR_SPARE_BUF_SIZE_2KB_PAGE - nOffset);
                        pstRndInArgOut->pBuf        = pBuf;
                    }
                    else /* if (nPlnIdx == 1)*/
                    {
                        pstRndInArgOut->nOffset     = (UINT16)(FSR_SPARE_BUF_BASE_SIZE + FSR_BML_CPBK_SPARE_START_OFFSET);
                        pstRndInArgOut->nNumOfBytes = (UINT16)(nOffset + nNumOfBytes - FSR_SPARE_BUF_SIZE_2KB_PAGE);
                        pstRndInArgOut->pBuf        = pBuf + (FSR_SPARE_BUF_SIZE_2KB_PAGE - nOffset);
                    }
                }
                else /* Start offset: 2nd plane */
                {
                    if (nPlnIdx == 0)
                    {
                        pstRndInArgOut->nOffset     = 0;
                        pstRndInArgOut->nNumOfBytes = 0;
                        pstRndInArgOut->pBuf        = pBuf;
                    }
                    else /* if (nPlnIdx == 1)*/
                    {
                        pstRndInArgOut->nOffset     = (UINT16)(nOffset + FSR_BML_CPBK_SPARE_START_OFFSET);
                        pstRndInArgOut->nNumOfBytes = (UINT16)nNumOfBytes;
                        pstRndInArgOut->pBuf        = pBuf;
                    }
                }
            }
        }
        /* case) random-in data for main page */
        else if (nOffset < pstVol->nSizeOfPage) /* Start offset: 1st plane */
        {
            if ((nNumOfBytes + nOffset) < pstVol->nSizeOfPage) /* Last offset: 1st plane */
            {
                if (nPlnIdx == 0)
                {
                    pstRndInArgOut->nOffset     = (UINT16) nOffset;
                    pstRndInArgOut->nNumOfBytes = (UINT16) nNumOfBytes;
                    pstRndInArgOut->pBuf        = pBuf;
                }
                else /* if (nPlnIdx == 1) */
                {
                    pstRndInArgOut->nOffset     = 0;
                    pstRndInArgOut->nNumOfBytes = 0;
                    pstRndInArgOut->pBuf        = pBuf;
                }
            }
            else /* Last offset: 2nd plane */
            {
                if (nPlnIdx == 0)
                {
                    pstRndInArgOut->nOffset     = (UINT16) nOffset;
                    pstRndInArgOut->nNumOfBytes = (UINT16) (pstVol->nSizeOfPage - nOffset);
                    pstRndInArgOut->pBuf        = pBuf;
                }
                else /* if (nPlnIdx == 1) */
                {
                    pstRndInArgOut->nOffset     = (UINT16) (nOffset - pstVol->nSizeOfPage);
                    pstRndInArgOut->nNumOfBytes = (UINT16) ((nOffset + nNumOfBytes) - pstVol->nSizeOfPage);
                    pstRndInArgOut->pBuf        = pBuf + (pstVol->nSizeOfPage - nOffset);
                }
            }
        }
        else /* Start offset: 2nd plane */
        {
            if (nPlnIdx == 0)
            {
                pstRndInArgOut->nOffset     = 0;
                pstRndInArgOut->nNumOfBytes = 0;
                pstRndInArgOut->pBuf        = pBuf;
            }
            else /* if (nPlnIdx == 1) */
            {
                pstRndInArgOut->nOffset     = (UINT16) (nOffset - pstVol->nSizeOfPage);
                pstRndInArgOut->nNumOfBytes = (UINT16) nNumOfBytes;
                pstRndInArgOut->pBuf        = pBuf;
            }
        }

        /* Increase the pointer by one */
        pstRndInArgIn   ++;
        pstRndInArgOut  ++;
    } while (++nCntIdx < nRndInCnt);

    /* Decrease the pointer by random-in count */
    pstRndInArgIn   -= nRndInCnt;
    pstRndInArgOut  -= nRndInCnt;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"), __FSR_FUNC__));
}
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function adjusts partition configuration.
 *
 *  @param [in]   nVol        : Volume number
 *  @param [in]  *pstPartIIn  : Pointer to FSRPartI data structure (user input) 
 *  @param [out] *pstPartIOut : Pointer to FSRPartI data structure (adjusted by BML)
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_INVALID_PARAM
 *  @return     FSR_BML_PAM_ACCESS_ERROR
 *  @return     FSR_BML_DEVICE_ACCESS_ERROR
 *  @return     Some LLD errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PRIVATE INT32
_AdjustPartInfo(UINT32        nVol,
                FSRPartI     *pstPartIIn,
                FSRPartI     *pstPartIOut)
{

    UINT32          nLastUnit       = 0;
    UINT32          nCnt            = 0;
    UINT32          nPEIdx          = 0;
    UINT32          nAddedPartCnt   = 0;
    UINT32          nNew1stVun      = 0;
    INT32           nBMLRe          = FSR_BML_SUCCESS;
    INT32           nPESortIdx      = 0;
    BOOL32          bAdjusted = FALSE32;
    UINT16          nLargestID;
    UINT16          nDummyID;
    FSRPartEntry   *pstPEntryIn;
    FSRPartEntry   *pstPEntryOut;
    FSRPartEntry    stTmpPEntry;
    BmlVolCxt      *pstVol;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));

    /* Check the volume range */
    CHK_VOL_RANGE(nVol);

    /* Get the pointer of volume context */
    pstVol = _GetVolCxt(nVol);

    /* Check the pointer to volume context */
    CHK_VOL_POINTER(pstVol);
    
    /* if pointer of Inout partition or output partition is null */
    if ((pstPartIIn == NULL) || (pstPartIOut == NULL))
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Pointer of input partiotn or output partition is NULL \r\n")));
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, pstPartIIn:0x%x, pstPartIOut:0x%x, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                        __FSR_FUNC__, nVol, pstPartIIn, pstPartIOut, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, FSR_BML_INVALID_PARAM));
        return FSR_BML_INVALID_PARAM;
    }

    /* if there is no partition */
    if (pstPartIIn->nNumOfPartEntry < 1)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] There is no partition \r\n")));
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nNumOfPartEntry:%d, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                        __FSR_FUNC__, nVol, pstPartIIn->nNumOfPartEntry, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, FSR_BML_INVALID_PARAM));
        return FSR_BML_INVALID_PARAM;
    }
    /* sort partition info */

    pstPEntryIn = &(pstPartIIn->stPEntry[0]);

    nDummyID   = FSR_PARTID_DUMMY0;
    nLargestID = FSR_PARTID_DUMMY0;

    if (pstPartIIn->nNumOfPartEntry > 1)
    {
        /* sort partition info by ascending order of n1stVun */
        for (nCnt = 0; nCnt < pstPartIIn->nNumOfPartEntry; nCnt++)
        {
            for (nPESortIdx = pstPartIIn->nNumOfPartEntry - 2; nPESortIdx > (INT32)nCnt - 1; --nPESortIdx)
            {
                if (pstPEntryIn[nPESortIdx + 1].n1stVun < pstPEntryIn[nPESortIdx].n1stVun)
                {
                    /* swap partition entry */
                    FSR_OAM_MEMCPY(&stTmpPEntry, &pstPEntryIn[nPESortIdx], sizeof(FSRPartEntry));
                    FSR_OAM_MEMCPY(&pstPEntryIn[nPESortIdx], &pstPEntryIn[nPESortIdx + 1], sizeof(FSRPartEntry));
                    FSR_OAM_MEMCPY(&pstPEntryIn[nPESortIdx + 1], &stTmpPEntry, sizeof(FSRPartEntry));
                }
            }
        }

        /* Search the largest dummy partition ID if partition info has already dummy partition */
        for (nCnt = 0; nCnt < pstPartIIn->nNumOfPartEntry; nCnt++)
        {
            if ((pstPartIIn->stPEntry[nCnt].nID >= FSR_PARTID_DUMMY0) &&
                (pstPartIIn->stPEntry[nCnt].nID >= nLargestID))
            {
                /* update the largest dummy partition ID */
                nLargestID = pstPartIIn->stPEntry[nCnt].nID;
                nDummyID   = nLargestID + 1;
            }
        }
    }

    /* copy input partition to output partition */
    FSR_OAM_MEMCPY(pstPartIOut, pstPartIIn, sizeof(FSRPartI));

    nAddedPartCnt = 0;

    /* if the front area is not allocated */
    if (pstPEntryIn[0].n1stVun > 0)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ] Partition should be adjusted(current first unit: %d)\r\n"),
                                         pstPEntryIn[0].n1stVun));

        nNew1stVun = 0;

        /* if first partition is MLC partition */
        if (pstPEntryIn[0].nAttr & FSR_BML_PI_ATTR_MLC)
        {
             /* if there is enough free partition entry */
            if (pstPartIOut->nNumOfPartEntry < FSR_BML_MAX_PARTENTRY)
            {
                /* allocated partition entry for new partition */
                pstPEntryOut = &(pstPartIOut->stPEntry[pstPartIOut->nNumOfPartEntry]);

                /* set value of newly added partition (dummy partition) */
                pstPEntryOut->n1stVun     = (UINT16) 0;
                pstPEntryOut->nNumOfUnits = (UINT16) 1;
                pstPEntryOut->nID         = (UINT16) (nDummyID + nAddedPartCnt);

                /* first unit should be SLC (OneNAND or Flex-OneNAND) */
                pstPEntryOut->nAttr       = FSR_BML_PI_ATTR_RW | FSR_BML_PI_ATTR_SLC;

                pstPartIOut->nNumOfPartEntry++;
                nAddedPartCnt++;

                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ] Dummy partition(ID: %d) is added(Unit: %d - %d)\r\n"),
                                pstPEntryOut->nID, 
                                pstPEntryOut->n1stVun, 
                                pstPEntryOut->n1stVun + pstPEntryOut->nNumOfUnits - 1));

                nNew1stVun = 1;

                bAdjusted = TRUE32;
            }
        }

        /* if there is enough free partition entry */
        if ((pstPartIOut->nNumOfPartEntry < FSR_BML_MAX_PARTENTRY) &&
            ((pstPEntryIn[0].n1stVun - nNew1stVun) > 0))
        {
            /* allocated partition entry for new partition */
            pstPEntryOut = &(pstPartIOut->stPEntry[pstPartIOut->nNumOfPartEntry]);

            /* set value of newly added partition (dummy partition) */
            pstPEntryOut->n1stVun     = (UINT16) nNew1stVun;
            pstPEntryOut->nNumOfUnits = (UINT16) (pstPEntryIn[0].n1stVun - nNew1stVun);
            pstPEntryOut->nID         = (UINT16) (nDummyID + nAddedPartCnt);

            pstPEntryOut->nAttr       = FSR_BML_PI_ATTR_RW;

            /* inherit partition attribute from the next partition (SLC or MLC) */
            pstPEntryOut->nAttr      |= (pstPEntryIn[0].nAttr & (FSR_BML_PI_ATTR_SLC | FSR_BML_PI_ATTR_MLC));
            
            pstPartIOut->nNumOfPartEntry++;
            nAddedPartCnt++;

            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ] Dummy partition(ID: %d) is added(Unit: %d - %d)\r\n"),
                                            pstPEntryOut->nID, 
                                            pstPEntryOut->n1stVun, 
                                            pstPEntryOut->n1stVun + pstPEntryOut->nNumOfUnits - 1));

            bAdjusted = TRUE32;
        }

    }

    if (pstPartIIn->nNumOfPartEntry > 1)
    {
        /* check unallocated area to spend all usable units */
        for (nPEIdx = 0; nPEIdx < pstPartIIn->nNumOfPartEntry - 1; nPEIdx++)
        {
            /* If free space exists between current partition and next partition */
            if ((pstPEntryIn[nPEIdx].n1stVun + pstPEntryIn[nPEIdx].nNumOfUnits) != pstPEntryIn[nPEIdx + 1].n1stVun)
            {
                /* add dummy partition to free space */

                /* if there is enough free partition entry */
                if (pstPartIOut->nNumOfPartEntry < FSR_BML_MAX_PARTENTRY)
                {
                    /* allocated partition entry for new partition */
                    pstPEntryOut = &(pstPartIOut->stPEntry[pstPartIOut->nNumOfPartEntry]);

                    /* set value of newly added partition (dummy partition) */
                    pstPEntryOut->n1stVun     = (UINT16) (pstPEntryIn[nPEIdx].n1stVun + pstPEntryIn[nPEIdx].nNumOfUnits);
                    pstPEntryOut->nNumOfUnits = (UINT16) pstPEntryIn[nPEIdx + 1].n1stVun - pstPEntryOut->n1stVun;
                    pstPEntryOut->nID         = (UINT16) (nDummyID + nAddedPartCnt);

                    pstPEntryOut->nAttr       = FSR_BML_PI_ATTR_RW;

                    /* inherit partition attribute from the next partition (SLC or MLC) */
                    pstPEntryOut->nAttr      |= (pstPEntryIn[nPEIdx + 1].nAttr & (FSR_BML_PI_ATTR_SLC | FSR_BML_PI_ATTR_MLC));
                    
                    pstPartIOut->nNumOfPartEntry++;
                    nAddedPartCnt++;

                    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ] Dummy partition(ID: %d) is added(Unit: %d - %d)\r\n"),
                                                    pstPEntryOut->nID, 
                                                    pstPEntryOut->n1stVun, 
                                                    pstPEntryOut->n1stVun + pstPEntryOut->nNumOfUnits - 1));

                    bAdjusted = TRUE32;
                }
            }
        }
    }

    /* last unit number in the last partition */
    nLastUnit = pstPEntryIn[pstPartIIn->nNumOfPartEntry - 1].n1stVun + 
                pstPEntryIn[pstPartIIn->nNumOfPartEntry - 1].nNumOfUnits - 1;

    /* Last usable unit (except reservoir area) */
    pstVol->nLastUnit = (pstVol->nNumOfUsBlks >> (pstVol->nSftNumOfPln+ pstVol->nSftNumOfWays)) - 1;

    /* if last unit of input partition info is smaller than available last unit */
    if (nLastUnit < pstVol->nLastUnit)
    {
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ] Partition should be adjusted(current last unit: %d, max last unit: %d)\r\n"),
                                         nLastUnit, pstVol->nLastUnit));

        /* if there is enough free partition entry */
        if (pstPartIOut->nNumOfPartEntry < FSR_BML_MAX_PARTENTRY)
        {
            /* allocated partition entry for new partition */
            pstPEntryOut = &(pstPartIOut->stPEntry[pstPartIOut->nNumOfPartEntry]);
            
            /* set value of newly added partition (dummy partition) */
            pstPEntryOut->n1stVun     = (UINT16)nLastUnit + 1;
            pstPEntryOut->nNumOfUnits = (UINT16)pstVol->nLastUnit - (UINT16)nLastUnit;
            pstPEntryOut->nID         = (UINT16) (nDummyID + nAddedPartCnt);
            pstPEntryOut->nAttr       = FSR_BML_PI_ATTR_RW;
            pstPEntryOut->nAttr      |= (pstPartIIn->stPEntry[pstPartIIn->nNumOfPartEntry - 1].nAttr &
                                     (FSR_BML_PI_ATTR_SLC | FSR_BML_PI_ATTR_MLC));

            pstPartIOut->nNumOfPartEntry++;
            nAddedPartCnt++;

            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ] Dummy partition(ID: %d) is added(Unit: %d - %d)\r\n"),
                                            pstPEntryOut->nID, 
                                            pstPEntryOut->n1stVun, 
                                            pstPEntryOut->n1stVun + pstPEntryOut->nNumOfUnits - 1));
            bAdjusted = TRUE32;
        }
    }

    if (bAdjusted == TRUE32)
    {
        /* print input partition info */
        FSR_DBZ_RTLMOUT(FSR_DBZ_BML_INF, (TEXT("[BIF:   ]   Adjusted partition info [nVol = %d]\r\n"),
                                         nVol));
        FSR_DBZ_RTLMOUT(FSR_DBZ_BML_INF, (TEXT("[BIF:   ]   - Number of partition: %d\r\n"),
                                         pstPartIOut->nNumOfPartEntry));
        FSR_DBZ_RTLMOUT(FSR_DBZ_BML_INF, (TEXT("[BIF:   ]   - Newly added partitions: %d\r\n"),
                                         nAddedPartCnt));


        /* print adjusted partition info */
        for (nPEIdx = 0; nPEIdx < pstPartIOut->nNumOfPartEntry; nPEIdx++)
        {
            pstPEntryOut = &(pstPartIOut->stPEntry[nPEIdx]);

            FSR_DBZ_RTLMOUT(FSR_DBZ_BML_INF, (TEXT("[BIF:   ]   + Partition ID    : %d\r\n"),
                                             pstPEntryOut->nID));
            FSR_DBZ_RTLMOUT(FSR_DBZ_BML_INF, (TEXT("[BIF:   ]     Partition Attr  : 0x%x\r\n"),
                                             pstPEntryOut->nAttr));
            FSR_DBZ_RTLMOUT(FSR_DBZ_BML_INF, (TEXT("[BIF:   ]     First vun       : %d\r\n"),
                                             pstPEntryOut->n1stVun));
            FSR_DBZ_RTLMOUT(FSR_DBZ_BML_INF, (TEXT("[BIF:   ]     Number of units : %d\r\n"),
                                             pstPEntryOut->nNumOfUnits));
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, nBMLRe));

    return nBMLRe;
}
#endif

#if !defined(FSR_NBL2)
/**
 *  @brief      This function unlocks user blocks in the volume.
 *
 *  @param [in]  nVol            : Volume number
 *  @param [in] *pstVol  : Pointer to VolCxt structure
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_CANT_UNLOCK_WHOLEAREA
 *  @return     FSR_BML_VOLUME_ALREADY_LOCKTIGHT
 *
 *  @author     SangHoon Choi
 *  @version    1.0.1
 *
 */
PRIVATE INT32
_UnlockUserBlk(UINT32        nVol,
               BmlVolCxt   *pstVol)
{
    UINT32              nPEntryIdx  = 0;    /* PartEntry Index                      */
    UINT32              nStartPEntry= 0;    /* start Num of PartEntry               */
    UINT32              nNumOfPEntry= 0;    /* # of PartEntries                     */
    INT32               nBMLRe      = FSR_BML_SUCCESS;
    FSRPartI           *pstPartI;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ]  ++%s\r\n"),
                                    __FSR_FUNC__));

    do
    {
        pstPartI = pstVol->pstPartI;

        /* Set # of part entries */
        nNumOfPEntry = pstPartI->nNumOfPartEntry;    

        /* UnLock all the user blocks */
        for (nPEntryIdx = nStartPEntry; nPEntryIdx < nNumOfPEntry ; nPEntryIdx++)
        {            
            nBMLRe = _SetBlkState(nVol,
                                  pstPartI->stPEntry[nPEntryIdx].n1stVun,
                                  pstPartI->stPEntry[nPEntryIdx].nNumOfUnits,
                                  FSR_LLD_IOCTL_UNLOCK_BLOCK);
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   Unlock operation is failed\r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d) / %d line\r\n"), 
                                                __FSR_FUNC__, nVol, __LINE__));
                break;
            }
        } /* End of "for (nPEntryIdx = 0 ;...)" */
    } while (0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));
    return nBMLRe;
}
#endif /* FSR_NBL2 */

#if defined(BML_CHK_FREE_PAGE) /* ADD 2007/10/29 */
/**
 *  @brief      This function checks whether given vpn is free page or not.
 *
 *  @param [in]  nVol        : Volume number
 *  @param [in]  nVpn        : First virtual page number for writing
 *  @param [in]  nNumOfPgs   : The number of pages to write
 *  @param [in]  nFlag       : FSR_BML_FLAG_ECC_ON
 *  @n                         FSR_BML_FLAG_ECC_OFF
 *  @n                         FSR_LLD_FLAG_USE_SPAREBUF
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_READ_ERROR
 *  @return     FSR_BML_CRITICAL_ERROR
 *  @return     FSR_BML_INVALID_PARAM
 *  @return     FSR_BML_ACQUIRE_SM_ERROR
 *  @return     FSR_BML_RELEASE_SM_ERROR
 *  @return     FSR_BML_VOLUME_NOT_OPENED
 *  @return     Some LLD errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PRIVATE INT32
_ChkFreePg(UINT32   nVol,
           UINT32   nVpn,
           UINT32   nNumOfPgs)
{
    UINT32          nIdx    = 0;
    UINT32          nPgIdx  = 0;
    UINT32          nPDev   = 0;
    UINT32          nDieIdx = 0;
    INT32           nBMLRe  = FSR_BML_SUCCESS;
    BmlVolCxt      *pstVol;
    BmlDevCxt      *pstDev;
    BmlDieCxt      *pstDie;
    UINT32         *pBuf32;
    UINT8          *pMBuf;
static UINT8        nSBuf[FSR_MAX_VIR_SCTS * 16];

    /* Get pointer to volume context */
    pstVol = _GetVolCxt(nVol);

    /************************************************************/
    /* check this vpn is free page or not                       */
    /************************************************************/
    for (nPgIdx = 0; nPgIdx < nNumOfPgs; nPgIdx++)
    {
        /* Get pointer to device context */
        nPDev    = (nVol << DEVS_PER_VOL_SHIFT) + ((nVpn >> pstVol->nDDPMask) & pstVol->nMaskPDev);
        pstDev   = _GetDevCxt(nPDev);

        /* Get pointer to die context */
        nDieIdx  = nVpn & pstVol->nDDPMask;
        pstDie   = pstDev->pstDie[nDieIdx];

        /* Set pMBuf, pSBuf */
        pMBuf = pstDie->pMBuf;

        /* Call FSR_BML_Read */
        nBMLRe = FSR_BML_ReadExt(nVol,
                                 nVpn+nPgIdx,
                                 1,
                                 pMBuf,
                                 &nSBuf[0],
                                 FSR_BML_FLAG_NONE);
        if (nBMLRe != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs:%d, nRe:0x%x) / %d line\r\n"),
                                            __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nBMLRe, __LINE__));
            break;
        }

        /* Check free page for main area */
        pBuf32    = (UINT32 *) pMBuf;
        for (nIdx = 0; nIdx < (pstVol->nSizeOfVPage/4); nIdx++)
        {
            if (pBuf32[nIdx] != 0xffffffff)
            {
                nBMLRe = FSR_BML_NO_FREE_PAGE;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs:%d, nRe:FSR_BML_NO_FREE_PAGE) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, nVpn, nNumOfPgs, __LINE__));
                return nBMLRe;
            }
        }

        /* Check free page for spare area */
        for (nIdx = 0; nIdx < (pstVol->nSparePerSct * pstVol->nSctsPerPg); nIdx++)
        {
            if (nSBuf[nIdx] != 0xff)
            {
                nBMLRe = FSR_BML_NO_FREE_PAGE;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs:%d, nRe:FSR_BML_NO_FREE_PAGE) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, nVpn, nNumOfPgs, __LINE__));
                return nBMLRe;
            }
        }

    } /* End of "for (nPgIdx = 0;...)" */

    return nBMLRe;
}
#endif /* BML_CHK_FREE_PAGE */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function writes virtual pages.
 *
 *  @param [in]  nVol        : Volume number
 *  @param [in]  nVpn        : First virtual page number for writing
 *  @param [in]  nNumOfPgs   : The number of pages to write
 *  @param [in] *pMBuf       : Main page buffer for writing
 *  @param [in] *pSBuf       : Spare page buffer for writing
 *  @param [in]  nFlag       : FSR_BML_FLAG_ECC_ON
 *  @n                         FSR_BML_FLAG_ECC_OFF
 *  @n                         FSR_LLD_FLAG_USE_SPAREBUF
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_READ_ERROR
 *  @return     FSR_BML_CRITICAL_ERROR
 *  @return     FSR_BML_INVALID_PARAM
 *  @return     FSR_BML_ACQUIRE_SM_ERROR
 *  @return     FSR_BML_RELEASE_SM_ERROR
 *  @return     FSR_BML_VOLUME_NOT_OPENED
 *  @return     Some LLD errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
_BML_Write(UINT32        nVol,
           UINT32        nVpn,
           UINT32        nNumOfPgs,
           UINT8        *pMBuf,
           FSRSpareBuf  *pSBuf,
           UINT32        nFlag)
{
    UINT32       nVun       = 0;            /* Virtual Unit number                      */
    UINT32       nBaseSbn   = 0;            /* The base Sbn                             */
    UINT32       nSbn       = 0;            /* Semi-physical block number               */
    UINT32       nPDev      = 0;            /* Physical Device Number                   */
    UINT32       nDevIdx    = 0;            /* Device index in a volume: 0 ~ 3          */
    UINT32       nPgOffset  = 0;            /* Physical Page Offset                     */
    UINT32       nNumOfLoop = 0;            /* The number of LLD Operation              */
    UINT32       nWrFlag    = 0x00000000;   /* The LLD command Flag to write            */
    UINT32       nDieIdx    = 0;            /* Die index   : 0(chip 0) or 1(chip 1)     */
    UINT32       nPlnIdx    = 0;            /* Plane index : 0~(FSR_MAX_PLANES-1)       */
    UINT32       nIdx       = 0;            /* Temporary die index                      */
    UINT32       nLLDFlag   = 0x00000000;   /* LLD Flag                                 */
    UINT32       nCacheFlag = FSR_LLD_FLAG_1X_CACHEPGM; /* The flag for cache program   */
    UINT32       nPgmFlag   = FSR_LLD_FLAG_1X_PROGRAM;  /* The flag for nomal program   */
    UINT32       nFirstAccess       = 0;    /* The number of LLDOp for FlushOp          */
    UINT32       nNumOfBlksInRsvr   = 0;    /* # of reserved blks for Flex-OneNAND      */
    UINT32       nLockStat      = 0x00000000;   /* Locked state of block                */
    UINT32       nOrderFlag     = BML_NBM_FLAG_START_SAME_OPTYPE;   /* LLD flag 
                                                            for non-blocking operation  */
    BOOL32       bWrErr         = FALSE32;  /* flag for rewiting a page returned error  */
    BOOL32       bLastPgInBlk   = FALSE32;  /* Flag to inform the last page in block    */
    BOOL32       bRet           = TRUE32;   /* Temporary return value                   */
    INT32        nMajorErr;                 /* Major Return error                       */
    INT32        nMinorErr;                 /* Minor Return error                       */
    INT32        nBMLRe = FSR_BML_SUCCESS;
    INT32        nLLDRe = FSR_LLD_SUCCESS;
    BmlVolCxt   *pstVol;
    BmlDevCxt   *pstDev;
    BmlDieCxt   *pstDie;
    UINT8       *pExtraMBuf;
    FSRSpareBuf  stOrgSBuf;
    FSRSpareBuf *pExtraSBuf;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, nVpn:%d, nNumOfPgs:%d, nFlag:0x%x)\r\n")
                                    , __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag)); 
    /* check volume range */
    CHK_VOL_RANGE(nVol);

    /* Get pointer of volume context */
    pstVol = _GetVolCxt(nVol);

    /* Check the pointer to volume context */
    CHK_VOL_POINTER(pstVol);

    /* checking volume open */
    CHK_VOL_OPEN(pstVol->bVolOpen);

    FSR_ASSERT(nVol < FSR_MAX_VOLS);

#if defined(BML_CHK_PARAMETER_VALIDATION)
    /* check the boundaries of input parameter*/
    if ((nVpn + nNumOfPgs - 1) > pstVol->nLastVpn)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Number of input page is bigger than Last Vpn \r\n")));
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs:%d, nFlag:0x%x, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                        __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"),__FSR_FUNC__));
        return FSR_BML_INVALID_PARAM;
    }

    /* check the boundary of input parameter */
    if (nNumOfPgs == 0)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Number of page is NULL \r\n")));
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs:%d, nFlag:0x%x, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                        __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"),__FSR_FUNC__));
        return FSR_BML_INVALID_PARAM;
    }

#endif /* BML_CHK_PARAMETER_VALIDATION */

#if defined(BML_CHK_FREE_PAGE) /* ADD 2007/10/29 */
    nBMLRe = _ChkFreePg(nVol,
                        nVpn,
                        nNumOfPgs);
    if (nBMLRe != FSR_BML_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs:%d, nRe:0x%x) / %d line\r\n"),
                                        __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nBMLRe, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"),__FSR_FUNC__));
        return nBMLRe;
    }
#endif

    /************************************************/
    /* Handle the 1st block OTP                     */
    /************************************************/
    /* Get nVun from nVpn */
    nVun = nVpn >> (pstVol->nSftSLC + pstVol->nSftNumOfWays);
    /* Check whether the 1st block is OTP or not */
    if ((nVun == 0) && (pstVol->b1stBlkOTP))
    {
        /* Get the state of OTP Block */
        nBMLRe = _GetOTPInfo(nVol,
                             &nLockStat);
        if (nBMLRe != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs:%d, nFlag:0x%x, nRe:0x%x)\r\n")
                                            , __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag, nBMLRe));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"),__FSR_FUNC__));
            return nBMLRe;
        }

        /* If OTP block is locked, it cannot be programed */
        if (nLockStat & FSR_LLD_OTP_1ST_BLK_LOCKED)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs:%d, nFlag:0x%x, nRe:FSR_BML_WR_PROTECT_ERROR) / %d line\r\n")
                                            , __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag, __LINE__));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"),__FSR_FUNC__));
            return FSR_BML_WR_PROTECT_ERROR;
        }
    } /* End of "if((nVun == 0) && ...)*/

    /* Initialize nLLDFlag */
    nLLDFlag = 0x00000000;

    /* Set ECC ON/OFF flag */
    nLLDFlag = nFlag & FSR_BML_FLAG_ECC_MASK;

    if ((nFlag & FSR_BML_FLAG_USE_SPAREBUF) == FSR_BML_FLAG_USE_SPAREBUF)
    {
        nLLDFlag |= FSR_LLD_FLAG_USE_SPAREBUF;
    }

    /****************************************************/
    /* At the first access, LLD_FlushOp should be called*/
    /* for removing errors of current die               */
    /****************************************************/
    /* Set # of FlushOp operation */
    if (nNumOfPgs < pstVol->nNumOfWays)
    {
        nFirstAccess = nNumOfPgs;
    }
    else
    {
        nFirstAccess = pstVol->nNumOfWays;
    }

    /* Initialize # of loops */
    nNumOfLoop = 0;
    while (nNumOfPgs > 0)
    {
        /************************************************************/
        /*Address Translation                                       */
        /************************************************************/
        /* STEP1: translate PgOffset */
        /* page offset for SLC area */
        nPgOffset = (nVpn >> (pstVol->nSftNumOfWays)) & pstVol->nMaskSLCPgs;
        /* ADD 2007/07/09 */
        bLastPgInBlk = FALSE32;
        if (nPgOffset == pstVol->nMaskSLCPgs)
        {
            bLastPgInBlk = TRUE32;
        }

        if (nVpn >= pstVol->n1stVpnOfMLC)
        {
            /* page offset for MLC area */
            nPgOffset = ((nVpn - pstVol->n1stVpnOfMLC) >> (pstVol->nSftNumOfWays)) & pstVol->nMaskMLCPgs;

            /* ADD 2007/07/09 */
            bLastPgInBlk = FALSE32;
            if (nPgOffset == pstVol->nMaskMLCPgs)
            {
                bLastPgInBlk = TRUE32;
            }
        }

        /* STEP2: translate PDev */
        nDevIdx  = (nVpn >> pstVol->nDDPMask) & pstVol->nMaskPDev;
        nPDev    = (nVol << DEVS_PER_VOL_SHIFT) + nDevIdx;

        /* Find pointer to get device context */
        pstDev   = _GetDevCxt(nPDev);
        
        /* STEP4: translate nDieIdx */
        nDieIdx  = nVpn & pstVol->nDDPMask;

        /* Find pointer to get die context */
        pstDie   = pstDev->pstDie[nDieIdx];

        FSR_ASSERT((nDieIdx == 0) || (nDieIdx == 1));

        /* STEP3: translate Vun */
        if ((nPgOffset == 0) || (nNumOfLoop == 0))
        {
            /* Vun for SLC area */
            nVun                = nVpn >> (pstVol->nSftSLC + pstVol->nSftNumOfWays);
            nNumOfBlksInRsvr    = 0;
            if (nVpn >= pstVol->n1stVpnOfMLC)
            {
                /* Vun for MLC area */
                nVun        = (pstVol->nNumOfSLCUnit) +
                              ((nVpn - (pstVol->nNumOfSLCUnit << (pstVol->nSftSLC + pstVol->nSftNumOfWays))) >>
                              (pstVol->nSftMLC + pstVol->nSftNumOfWays));
                
                if (pstDie->pstRsv->nRsvrType == BML_HYBRID_RESERVOIR)
                {
                    nNumOfBlksInRsvr = pstDie->pstRsv->nLastSbnOfRsvr - pstDie->pstRsv->n1stSbnOfRsvr + 1;
                }
            }

            /* Check the RO partition           */
            bRet = _IsROPartition(nVun, pstVol);
            if (bRet == TRUE32)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs:%d, nFlag:0x%x, nRe:FSR_BML_WR_PROTECT_ERROR) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag, __LINE__));
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"),__FSR_FUNC__));
                nBMLRe = FSR_BML_WR_PROTECT_ERROR;
                break;
            }
        }

        /* Check and handle the previous operation error */
        if (nNumOfLoop < nFirstAccess)
        {
            /* 
             * S/W work-around : Flex-OneNAND 8Gb DDP
             * This code is for 8G DDP Flex-OneNAND.
             * In DDP, it waits until the non-program operation of opposite die of finished.
             */
            if (pstVol->nNumOfDieInDev == 2)
            {
                nIdx = (nDieIdx + 1) & 1;
                pstDie = pstDev->pstDie[nIdx];
                if (pstDie->pstPreOp->nOpType != BML_PRELOG_WRITE)
                {
                    nLLDRe = pstVol->LLD_FlushOp(nPDev,
                                                 nIdx,
                                                 FSR_LLD_FLAG_NONE);
                    if (nLLDRe != FSR_LLD_SUCCESS)
                    {
                        nBMLRe = _HandlePrevError(nVol,
                                                  nPDev,
                                                  nIdx,
                                                  nLLDRe);
                        if (nBMLRe != FSR_BML_SUCCESS)
                        {
                            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs:%d, nFlag:0x%x, nRe:0x%x)\r\n"),
                                                            __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag, nBMLRe));
                            break;
                        }
                    }
                } /* End of "if (pstVol->nNumOfWays == 2)" */

                /* Recover pointer to BmlDieCxt structure */
                pstDie = pstDev->pstDie[nDieIdx];
            }

            nLLDRe = pstVol->LLD_FlushOp(nPDev,
                                         nDieIdx,
                                         FSR_LLD_FLAG_NONE);
            if (nLLDRe != FSR_LLD_SUCCESS)
            {
                nBMLRe = _HandlePrevError(nVol,
                                          nPDev,
                                          nDieIdx,
                                          nLLDRe);
                if (nBMLRe != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs:%d, nFlag:0x%x, nRe:0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag, nBMLRe));
                    break;
                }
            }

            /* Get the return value by partitionfor current partition */
            nBMLRe = _GetPIRet(pstVol,
                               pstDie,
                               nVun,
                               nBMLRe);
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                /* message out */
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs:%d, nFlag:0x%x, nRe:0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag, nBMLRe));
                break;
            }

        } /* End of "if (nNumOfLoop < nFirstAccess)" */
        else if (nPgOffset == 0)
        {
            nLLDRe = pstVol->LLD_FlushOp(nPDev,
                                         nDieIdx,
                                         FSR_LLD_FLAG_NONE);
            if (nLLDRe != FSR_LLD_SUCCESS)
            {
                nBMLRe = _HandlePrevError(nVol,
                                          nPDev,
                                          nDieIdx,
                                          nLLDRe);
                if (nBMLRe != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs:%d, nFlag:0x%x, nRe:0x%x)\r\n"),
                                                    __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag, nBMLRe));
                    break;
                }
            }
        } /* End of "if (nPgOffset == 0)" */

        /* STEP5: translate Sbn[] to Pbn[] */
        if ((nPgOffset == 0) || (nNumOfLoop < nFirstAccess))
        {
            nBaseSbn = ((nDieIdx) << pstVol->nSftNumOfBlksInDie) +
                       (nVun << pstVol->nSftNumOfPln)
                       + nNumOfBlksInRsvr;

            nPlnIdx  = pstVol->nNumOfPlane - 1;
            do
            {
                pstDie->nCurSbn[nPlnIdx] = (UINT16)nBaseSbn + (UINT16)nPlnIdx;
                pstDie->nCurPbn[nPlnIdx] = (UINT16)nBaseSbn + (UINT16)nPlnIdx;
            } while (nPlnIdx-- > 0);

            /* Second Translation: translate Pbn[]
             * If the pstDev->nCurSbn[] should be replaced by a reserved block, 
             * pstDev->nNumOfLLDOp is equal to pstVol->nNumOfPlane-1.
             * If not, pstDev->nNumOfLLDOp is equal to 0.
             */
            pstDie->nNumOfLLDOp = 0;
            _GetPBN(pstDie->nCurSbn[0], pstVol, pstDie);
        }

        nPlnIdx = 0;
        do
        {
            /* Initialize bExit to handle a write error */
            bWrErr = FALSE32;
            do
            {
                /* Set nCacheFlag, nPgmFlag by # of palnes and a replaced block */
                if (pstDie->nNumOfLLDOp == 0)
                {
                    if (pstVol->nNumOfPlane == 1)
                    {
                        nCacheFlag = FSR_LLD_FLAG_1X_CACHEPGM;
                        nPgmFlag   = FSR_LLD_FLAG_1X_PROGRAM;
                    }
                    else if (pstVol->nNumOfPlane == 2)
                    {
                        nCacheFlag = FSR_LLD_FLAG_2X_CACHEPGM;
                        nPgmFlag   = FSR_LLD_FLAG_2X_PROGRAM;
                    }

                    /* ADD to support non-blocking operation (2007/08/22) */
                    if (nNumOfPgs == 1)
                    {
                        nOrderFlag = BML_NBM_FLAG_END_OF_SAME_OPTYPE;
                    }
                }
                /* In case that Sbn is replaced as reserved block in reservoir */
                else
                {
                    nCacheFlag = FSR_LLD_FLAG_1X_CACHEPGM;
                    nPgmFlag   = FSR_LLD_FLAG_1X_PROGRAM;

                    /* ADD to support non-blocking operation (2007/08/22) */
                    if ((nNumOfPgs == 1)    &&
                        (nPlnIdx == (pstVol->nNumOfPlane -1 )))
                    {
                        nOrderFlag = BML_NBM_FLAG_END_OF_SAME_OPTYPE;
                    }
                }

                /* Set the Flags for a cache program   (MODIFY 2007/07/09)*/
                if ((nNumOfPgs <= pstVol->nNumOfWays) || (bLastPgInBlk == TRUE32))
                {
                    nWrFlag = nPgmFlag;
                }
                else
                {
                    nWrFlag = nCacheFlag;
                }

                if (pSBuf == NULL)
                {
                    pExtraSBuf = pSBuf;
                }
                else
                {
                    FSR_OAM_MEMCPY(&stOrgSBuf, pSBuf, sizeof(FSRSpareBuf));
                    if (stOrgSBuf.nNumOfMetaExt != 0)
                    {
                        stOrgSBuf.nNumOfMetaExt = pstVol->nSizeOfPage / FSR_PAGE_SIZE_PER_SPARE_BUF_EXT;
                        stOrgSBuf.pstSTLMetaExt += nPlnIdx;
                    }
                    pExtraSBuf  = &stOrgSBuf;
                }

                nLLDRe= pstVol->LLD_Write(nPDev,
                                          pstDie->nCurPbn[nPlnIdx],
                                          nPgOffset,
                                          pMBuf,
                                          pExtraSBuf,
                                          nLLDFlag | nWrFlag | nOrderFlag);

                /* Handle the write error */
                nMajorErr = FSR_RETURN_MAJOR(nLLDRe);
                nMinorErr = FSR_RETURN_MINOR(nLLDRe);

                switch(nMajorErr)
                {
                case FSR_LLD_SUCCESS:

                    /* Store the pre Op buffers to the next pre Op for cache program */
                    FSR_OAM_MEMCPY(pstDie->pstNextPreOp, pstDie->pstPreOp, sizeof(BmlPreOpLog));
                    pstDie->pNextPreOpMBuf        =  pstDie->pPreOpMBuf;
                    pstDie->pNextPreOpSBuf        =  pstDie->pPreOpSBuf;
                    
                    /* Store the Pre operation Log */
                    pstDie->pstPreOp->nOpType     =  BML_PRELOG_WRITE;
                    pstDie->pstPreOp->nSbn        =  pstDie->nCurSbn[nPlnIdx];
                    pstDie->pstPreOp->nPgOffset   =  nPgOffset;
                    pstDie->pPreOpMBuf            =  pMBuf;
                    pstDie->pPreOpSBuf            =  pSBuf;
                    pstDie->pstPreOp->nFlag       =  nLLDFlag | nWrFlag;

                    /* Initialize bWrErr */
                    bWrErr = FALSE32;
                    break;

                case FSR_LLD_PREV_WRITE_ERROR:

                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs:%d, nFlag:0x%x)\r\n"),
                                                    __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag));
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_Write(nPDev:%d, nPbn:%d, nPgOffset:%d, nRe:FSR_LLD_PREV_WRITE_ERROR)\r\n"),
                                                    nPDev, pstDie->nCurPbn[nPlnIdx], nPgOffset));
                    nSbn = pstDie->nCurSbn[0];

                    nBMLRe = _HandlePrevWrErr(nVol,
                                              nPDev,
                                              nDieIdx,
                                              nMinorErr,
                                              TRUE32);

                    if (nBMLRe == FSR_BML_SUCCESS)
                    {
                        /* translate Pbn[]*/
                        pstDie->nNumOfLLDOp = 0;
                        _GetPBN(nSbn, pstVol, pstDie);

                        /* Set bWrErr to write a current page */
                        bWrErr  = TRUE32;
                    }

                    break;

                case FSR_LLD_WR_PROTECT_ERROR:

                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs:%d, nFlag:0x%x)\r\n"),
                                                    __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag));
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_Write(nPDev:%d, nPbn:%d, nPgOffset:%d, nRe:FSR_LLD_WR_PROTECT_ERROR)\r\n"),
                                                    nPDev, pstDie->nCurPbn[nPlnIdx], nPgOffset));
                    nBMLRe = FSR_BML_WR_PROTECT_ERROR;
                    break;

                default: /* handle other errors */
                    nBMLRe = nLLDRe;
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_Write(nPDev:%d, nPbn:%d, nPgOffset:%d, nRe:0x%x)\r\n"),
                                                    nPDev, pstDie->nCurPbn[nPlnIdx], nPgOffset, nBMLRe));
                    break;
                }

                /* ADD to support non-blocking operation (2007/08/22) */
                nOrderFlag = BML_NBM_FLAG_CONTINUE_SAME_OPTYPE;

                /* Occur error of _HandlePrevWrErr and FSR_LLD_PREV_WR_PROTECT_ERROR*/
                if (nBMLRe != FSR_BML_SUCCESS)
                {
                    break;
                }

            } while (bWrErr == TRUE32);
            
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                break;
            }

            /* Increase the pMBuf pointer when pMBuf is not NULL */
            if (pMBuf != NULL)
            {
                pMBuf += (pstVol->nSizeOfPage * (pstVol->nNumOfPlane - pstDie->nNumOfLLDOp));
            }

        } while(nPlnIdx++ < pstDie->nNumOfLLDOp);

        /* Occur error of _HandlePrevWrErr and FSR_LLD_PREV_WR_PROTECT_ERROR*/
        if (nBMLRe != FSR_BML_SUCCESS)
        {
            break;
        }

        /* MEMCOPY to store last page */
        if (nNumOfPgs <= (pstVol->nNumOfWays << 1)) 
        {
            pExtraMBuf = NULL;
            if (pMBuf != NULL)
            {
                pExtraMBuf = pMBuf - pstVol->nSizeOfVPage;
            }

            /* Memcopy for cache program */
            if ((nNumOfPgs > pstVol->nNumOfWays) && (pstVol->bCachedProgram == TRUE32))
            {
                pstDie->pNextPreOpMBuf    = pstDie->pMBuf;
                pstDie->pNextPreOpSBuf    = pstDie->pSBuf;

                /* Memcopy for main and spare data of pstNextPreOp */
                FSR_OAM_MEMCPY(pstDie->pNextPreOpMBuf, pExtraMBuf, pstVol->nSizeOfVPage);
                if (pSBuf != NULL)
                {
                    FSR_OAM_MEMCPY(pstDie->pNextPreOpSBuf->pstSpareBufBase, pSBuf->pstSpareBufBase, FSR_SPARE_BUF_BASE_SIZE);
                    pstDie->pNextPreOpSBuf->nNumOfMetaExt      = pSBuf->nNumOfMetaExt;
                    FSR_OAM_MEMCPY(pstDie->pNextPreOpSBuf->pstSTLMetaExt,
                                   pSBuf->pstSTLMetaExt,
                                   FSR_SPARE_BUF_EXT_SIZE * pSBuf->nNumOfMetaExt);
                }
            }

            /* Memcopy for program */
            if (nNumOfPgs <= pstVol->nNumOfWays)
            {
                if ((pstVol->nNANDType != FSR_LLD_FLEX_ONENAND) && (pstVol->nNANDType != FSR_LLD_SLC_ONENAND))
                {
                    pstDie->pPreOpMBuf = pstDie->pMBuf;
                    pstDie->pPreOpSBuf = pstDie->pSBuf;

                    /* Memcopy for main and spare data of pstPreOp*/
                    FSR_OAM_MEMCPY(pstDie->pPreOpMBuf, pExtraMBuf, pstVol->nSizeOfVPage);
                    if (pSBuf != NULL)
                    {
                        FSR_OAM_MEMCPY(pstDie->pPreOpSBuf->pstSpareBufBase, pSBuf->pstSpareBufBase, FSR_SPARE_BUF_BASE_SIZE);
                        pstDie->pPreOpSBuf->nNumOfMetaExt      = pSBuf->nNumOfMetaExt;
                        FSR_OAM_MEMCPY(pstDie->pPreOpSBuf->pstSTLMetaExt,
                                       pSBuf->pstSTLMetaExt,
                                       FSR_SPARE_BUF_EXT_SIZE * pSBuf->nNumOfMetaExt);
                    }
                }
            }
        }   /* End of if (nNumOfPgs <= (pstVol->nNumOfWays << 1)) */

        /* Increase the spare buffer pointer */
        if (pSBuf != NULL)
        {
            pSBuf   ++;
        }

        /* Change the nNumOfPgs, nNumOfLoop, nVpn*/
        nNumOfPgs --;
        nNumOfLoop++;
        nVpn      ++;
    }


    /* 
     * When only BB has opened (AP has not yet opend) in the dual core system,
     * all errors should be handled.
     * Otherwise, these errors can return to AP that is in open sequence.
     */
    if (nBMLRe == FSR_BML_SUCCESS)
    {
        nBMLRe = _HandleErrForDualCore(nVol);
        if (nBMLRe != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs:%d, nFlag:0x%x, nRe: 0x%x)\r\n"),
                                            __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag, nBMLRe));
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, nBMLRe));

    return nBMLRe;
}
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function copy source virtual page to target virtual page
 *  @n          using random input method.
 *
 *  @param [in]  nVol                        : Volume number
 *  @param [in] *pstBMLCpArg[FSR_MAX_WAYS]   : Pointer to BMLCpBkArg structure
 *  @param [in]  nFlag                       : FSR_BML_FLAG_NONE
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_WR_PROTECT_ERROR
 *  @return     FSR_BML_READ_ERROR | nBMLReMinor
 *  @return     FSR_BML_CRITICAL_ERROR
 *  @return     FSR_BML_INVALID_PARAM
 *  @return     FSR_BML_VOLUME_NOT_OPENED
 *  @return     FSR_BML_ACQUIRE_SM_ERROR
 *  @return     FSR_BML_RELEASE_SM_ERROR
 *  @return     FSR_BML_VOLUME_NOT_OPENED
 *  @return     Some LLD errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
FSR_BML_CopyBack(UINT32        nVol,
                 BMLCpBkArg   *pstBMLCpArg[FSR_MAX_WAYS],
                 UINT32        nFlag)
{
    UINT32       nVun               = 0;    /* Virtual Unit Number                          */
    UINT32       nSbn               = 0;    /* Semi-physical block number                   */
    UINT32       nBaseSbn           = 0;    /* Base Sbn                                     */
    UINT32       nNumOfBlksInRsvr   = 0;    /* # of reserved blks for Flex-OneNAND          */
    UINT32       nPDev              = 0;    /* Physical Device Number                       */
    UINT32       nPgOffset          = 0;    /* Physical Page Offset                         */
    UINT32       nLLDOpStep         = 0;    /* The step of LLD Operation(load, program)     */
    UINT32       nRndInCnt          = 0;    /* # of random-in data                          */
    UINT32       nWayIdx            = 0;    /* Way Index                                    */
    UINT32       nDieIdx            = 0;    /* Die index   : 0(chip 0) or 1(chip 1)         */
    UINT32       nPlnIdx            = 0;    /* Plane index : 0~(FSR_MAX_PLANES-1)           */
    UINT32       nLdFlag    = FSR_LLD_FLAG_1X_CPBK_LOAD;    /* LLD flag for loading data    */
    UINT32       nPgmFlag   = FSR_LLD_FLAG_1X_CPBK_PROGRAM; /* LLD flag for programming data*/
    UINT32       nCpFlag    = FSR_LLD_FLAG_1X_CPBK_LOAD;/* Flag for CopyBack (Load & program)*/
    UINT32       nOrderFlag = BML_NBM_FLAG_START_SAME_OPTYPE;   /* LLD flag for 
                                                                    non-blocking operation  */
    UINT32       nIdx       = 0;            /* Temporary index                              */
    UINT32       nChkWayIdx = 0;            /* Temporary way err index                      */
    BOOL32       bWrErr     = FALSE32;      /* flag for rewiting a page returned error      */
    BOOL32       bRet       = TRUE32;       /* Temporary return value                       */
    INT32        nMinorRe   = 0;            /* Minor error for Read error                   */
    INT32        nMajorErr;                 /* Major Return error                           */
    INT32        nMinorErr;                 /* Minor Return error                           */
    INT32        nBMLRe = FSR_BML_SUCCESS;
    INT32        nLLDRe = FSR_LLD_SUCCESS;
    LLDRndInArg  stLLDRndInArg[FSR_BML_CPBK_MAX_RNDINARG];
    LLDCpBkArg   stLLDCpArg;
    BMLRndInArg *pstRndInArg;
    BmlVolCxt   *pstVol;
    BmlDevCxt   *pstDev;
    BmlDieCxt   *pstDie;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, nFlag:0x%x)\r\n"), __FSR_FUNC__, nVol, nFlag));

    /* check the volume range */
    CHK_VOL_RANGE(nVol);

    /* Get the pointer of volume context */
    pstVol = _GetVolCxt(nVol);

    /* Check the pointer to volume context */
    CHK_VOL_POINTER(pstVol);

    /* check volume open */
    CHK_VOL_OPEN(pstVol->bVolOpen);

    FSR_ASSERT(nVol < FSR_MAX_VOLS);

#if defined(BML_CHK_PARAMETER_VALIDATION)
    for (nLLDOpStep = 0; nLLDOpStep < 2; nLLDOpStep++)
    {
        nWayIdx = 0;
        do
        {
            if (pstBMLCpArg[nWayIdx] == NULL)
            {
                continue;
            }

            /* check the boundaries of random-in data */
            pstRndInArg = pstBMLCpArg[nWayIdx]->pstRndInArg;
            for (nRndInCnt = 0 ; nRndInCnt < pstBMLCpArg[nWayIdx]->nRndInCnt; nRndInCnt++)
            {
                if ((pstRndInArg->nOffset & 0x01) || (pstRndInArg->nNumOfBytes & 0x01))
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]  Offset of RndInArg or number of bytes is not even number\r\n")));
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nOffset:%d, nNumOfBytes:%d, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                    __FSR_FUNC__, nVol, pstRndInArg->nOffset, pstRndInArg->nNumOfBytes, __LINE__));
                    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"),__FSR_FUNC__));
                    return FSR_BML_INVALID_PARAM;
                }

                pstRndInArg++;
            }

            if (nLLDOpStep == 0)
            {
                nVun      = pstBMLCpArg[nWayIdx]->nSrcVun;
                nPgOffset = pstBMLCpArg[nWayIdx]->nSrcPgOffset >> pstVol->nSftNumOfWays;
            }
            else
            {
                nVun      = pstBMLCpArg[nWayIdx]->nDstVun;
                nPgOffset = pstBMLCpArg[nWayIdx]->nDstPgOffset >> pstVol->nSftNumOfWays;
            }

            /* check the boundaries of nVun*/
            if (nVun > pstVol->nLastUnit)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]  Vun is bigger than last unit number \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVun:%d, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, nVun, __LINE__));
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"),__FSR_FUNC__));
                return FSR_BML_INVALID_PARAM;
            }

            /* check the page offset boundaries*/
            if ((nVun < pstVol->nNumOfSLCUnit) && (nPgOffset >= pstVol->nNumOfPgsInSLCBlk))
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]  Page offset boundary is greater than number of page in SLC block \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nBlkType: SLC, nPgOffset:%d, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, nPgOffset, __LINE__));
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"),__FSR_FUNC__));
                return FSR_BML_INVALID_PARAM;
            }
            else if ((nVun >= pstVol->nNumOfSLCUnit) && (nPgOffset >= pstVol->nNumOfPgsInMLCBlk))
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]  Page offset boundary is greater than number of page in MLC block \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nBlkType: MLC, nPgOffset:%d, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, nPgOffset, __LINE__));
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"),__FSR_FUNC__));
                return FSR_BML_INVALID_PARAM;
            }
        } while (++nWayIdx < pstVol->nNumOfWays);
    }
#endif /* BML_CHK_PARAMETER_VALIDATION */
 
    /* Get semaphore */
    bRet = FSR_OAM_AcquireSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
    if (bRet == FALSE32)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nRe:FSR_BML_ACQUIRE_SM_ERROR) / %d line\r\n"),
                                        __FSR_FUNC__, nVol, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"),__FSR_FUNC__));
        return FSR_BML_ACQUIRE_SM_ERROR;
    }

    /*Copyback operates a load operation.
     * if nLLDOpStep = 1, it operates a program operation with random-in
     */
    nLLDOpStep    = 0;
    nChkWayIdx = 0;
    do
    {
        /* loop for ways*/
        nWayIdx = nChkWayIdx;
        do
        {
            /* Use only the valid data arary */
            if (pstBMLCpArg[nWayIdx] == NULL)
            {
                nWayIdx++;
                continue;
            }

            /* Get nVun and nPgOffset for source and destination  */
            if (nLLDOpStep == 0)
            {
                /* unaligned page offset */
                FSR_ASSERT((pstBMLCpArg[nWayIdx]->nSrcPgOffset & pstVol->nMaskWays) == nWayIdx);

                nVun      = pstBMLCpArg[nWayIdx]->nSrcVun;
                nPgOffset = pstBMLCpArg[nWayIdx]->nSrcPgOffset;
            }
            else
            {
                /* unaligned page offset */
                FSR_ASSERT((pstBMLCpArg[nWayIdx]->nDstPgOffset & pstVol->nMaskWays) == nWayIdx)

                nVun      = pstBMLCpArg[nWayIdx]->nDstVun;
                nPgOffset = pstBMLCpArg[nWayIdx]->nDstPgOffset;

                /* Check the RO partition            */
                bRet = _IsROPartition(nVun, pstVol);
                if (bRet == TRUE32)
                {
                    nBMLRe = FSR_BML_WR_PROTECT_ERROR;
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nVun:%d, nRe:FSR_BML_WR_PROTECT_ERROR)\r\n"),
                                                    __FSR_FUNC__, nVol, nVun));
                    break;
                }
            }

            /* Relation among nPDev, nDieIdx and nWayIdx
             *  +------------+-----------------+-----------------+
             *  |            |      MDP        |      DDP        |
             *  |            +-------+---------+-------+---------+
             *  |            | nPDev | nDieIdx | nPDev | nDieIdx |
             *  +------------+-------+---------+-------+---------+
             *  | nWayIdx=00 |  00   |   00    |  00   |    00   |
             *  | nWayIdx=01 |  01   |   00    |  00   |    01   |
             *  | nWayIdx=10 |  10   |   00    |  01   |    00   |
             *  | nWayIdx=11 |  11   |   00    |  01   |    01   |
             *  +------------+-------+---------+-------+---------+
             */

            /* Get nPDev, nDieIdx */
            nPDev     = (nWayIdx >> pstVol->nSftDDP) + nVol * DEVS_PER_VOL;
            nDieIdx   = pstVol->nDDPMask & nWayIdx;

            /* Get pointer to device and die */
            pstDev = _GetDevCxt(nPDev);
            pstDie = pstDev->pstDie[nDieIdx];

            /* Set the nRsvrBlks according to nVun*/
            nNumOfBlksInRsvr = 0; 
            if (nVun > (pstVol->nNumOfSLCUnit -1))
            {
                if (pstDie->pstRsv->nRsvrType == BML_HYBRID_RESERVOIR)
                {
                    nNumOfBlksInRsvr = pstDie->pstRsv->nLastSbnOfRsvr - pstDie->pstRsv->n1stSbnOfRsvr + 1;
                }
            }

            /* Handle previous error before LLD Load operates*/
            if (nLLDOpStep == 0)
            {
                nIdx = pstVol->nNumOfDieInDev - 1;
                do
                {
                    nLLDRe = pstVol->LLD_FlushOp(nPDev,
                                                 nIdx,  /* nDieIdx */
                                                 FSR_LLD_FLAG_NONE);
                    if (nLLDRe != FSR_LLD_SUCCESS)
                    {
                        nSbn = pstDev->pstDie[nIdx]->nCurSbn[0];
                        nBMLRe = _HandlePrevError(nVol,
                                                  nPDev,
                                                  nIdx, /* nDieIdx */
                                                  nLLDRe);
                        if (nBMLRe != FSR_BML_SUCCESS)
                        {
                            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:0x%x)\r\n"),
                                                            __FSR_FUNC__, nVol, nBMLRe));

                            if (nBMLRe == FSR_BML_READ_ERROR)
                            {
                                /* Set nBMLReMinor to inform the error way
                                 * This minor error can indicate up to 2 way */
                                nMinorRe    = FSR_RETURN_VALUE(0, 01, 0x0000, nPDev + nIdx);

                                /* Return read error considerring the multi-way */
                                nBMLRe      |= nMinorRe;
                            }
                            break;
                        }

                        /* Translate Pbn[] */
                        pstDev->pstDie[nIdx]->nNumOfLLDOp = 0;
                        _GetPBN(nSbn, pstVol, pstDev->pstDie[nIdx]);
                    }
                } while (nIdx-- > 0);

                /* Error case */
                if (nBMLRe != FSR_BML_SUCCESS)
                {
                    break;
                }
            }
            else
            {
                /* S/W work-around : Flex-OneNAND 8Gb DDP */
                if (nWayIdx == 0)
                {
                    if (pstVol->nNumOfDieInDev == FSR_MAX_DIES)
                    {
                        nIdx = (nDieIdx + 1) & 1;
                    }
                    else
                    {
                        nIdx = nDieIdx;
                    }

                    nLLDRe = pstVol->LLD_FlushOp(nPDev,
                                                 nIdx,  /* nDieIdx */
                                                 FSR_LLD_FLAG_NONE);
                    if (nLLDRe != FSR_LLD_SUCCESS)
                    {
                        nSbn = pstDev->pstDie[nIdx]->nCurSbn[0];
                        nBMLRe = _HandlePrevError(nVol,
                                                  nPDev,
                                                  nIdx, /* nDieIdx */
                                                  nLLDRe);
                        if (nBMLRe != FSR_BML_SUCCESS)
                        {
                            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:0x%x)\r\n"),
                                                            __FSR_FUNC__, nVol, nBMLRe));

                            if (nBMLRe == FSR_BML_READ_ERROR)
                            {
                                /* Set nBMLReMinor to inform the error way
                                 * This minor error can indicate up to 2 way */
                                nMinorRe    = FSR_RETURN_VALUE(0, 01, 0x0000, nPDev + nIdx);

                                /* Return read error considerring the multi-way */
                                nBMLRe      |= nMinorRe;
                                break;
                            }
                        }

                        /* Translate Pbn[] */
                        pstDev->pstDie[nIdx]->nNumOfLLDOp = 0;
                        _GetPBN(nSbn, pstVol, pstDev->pstDie[nIdx]);
                    } /* End of "if (nLLDRe != FSR_LLD_SUCCESS)" */
                } /* End of "if (nWayIdx == 0)" */
            } /* End of "if (nLLDOpStep == 0)" */

            /* Get the return value by partition for current partition */
            nBMLRe = _GetPIRet(pstVol,
                               pstDie,
                               nVun,
                               nBMLRe);
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                /* message out */
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nBMLRe));
                break;
            }

            /* Get nPgOffset, nBaseSbn*/
            nPgOffset = nPgOffset >> pstVol->nSftNumOfWays;
            nBaseSbn  = (nDieIdx << pstVol->nSftNumOfBlksInDie) +
                        (nVun << pstVol->nSftNumOfPln) +
                        nNumOfBlksInRsvr;

            /* Translate Sbn[] to Pbn[] */
            nPlnIdx  = pstVol->nNumOfPlane-1;
            do
            {
                pstDie->nCurSbn[nPlnIdx] = (UINT16)nBaseSbn + (UINT16)nPlnIdx;
                pstDie->nCurPbn[nPlnIdx] = (UINT16)nBaseSbn + (UINT16)nPlnIdx;
            } while (nPlnIdx-- > 0);

            /* Second Translation: translate Pbn[]
             * If the pstDev->nCurSbn[] should be replaced by a reserved block,
             * pstDev->nNumOfLLDOp is equal to pstVol->nNumOfPlane-1.
             * If not, pstDev->nNumOfLLDOp is equal to 0.
             */
            pstDie->nNumOfLLDOp = 0;
            _GetPBN(pstDie->nCurSbn[0], pstVol, pstDie);

            /* 
             * << Set the flag for LLD_CopyBack >>
             * --------------------------------------------
             * FSR_LLD_FLAG_NO_CPBK_CMD        (0x00000000)
             * FSR_LLD_FLAG_1X_CPBK_LOAD       (0x00000001)
             * FSR_LLD_FLAG_1X_CPBK_PROGRAM    (0x00000002)
             * FSR_LLD_FLAG_2X_CPBK_LOAD       (0x00000003)
             * FSR_LLD_FLAG_2X_CPBK_PROGRAM    (0x00000004)
             * --------------------------------------------
             */

            /* this expression only use for 1x- and 2x- operation*/
            if (pstDie->nNumOfLLDOp == 0)
            {
                if (pstVol->nNumOfPlane == 1)
                {
                    nLdFlag = FSR_LLD_FLAG_1X_CPBK_LOAD;
                    nPgmFlag = FSR_LLD_FLAG_1X_CPBK_PROGRAM;
                }
                else if (pstVol->nNumOfPlane == 2)
                {
                    nLdFlag = FSR_LLD_FLAG_2X_CPBK_LOAD;
                    nPgmFlag = FSR_LLD_FLAG_2X_CPBK_PROGRAM;
                }
            }
            else
            {
                nLdFlag = FSR_LLD_FLAG_1X_CPBK_LOAD;
                nPgmFlag   = FSR_LLD_FLAG_1X_CPBK_PROGRAM;
            }

            if (nLLDOpStep == 0) /* Load operation */
            {
                nCpFlag = nLdFlag;
            }
            else /* Program operation */
            {
                nCpFlag = nPgmFlag;
            }

            /* Call LLD_CopyBack according to # of plane */
            nPlnIdx = 0;
            do
            {
                /* Set the data structure of LLDCpBkArg */
                if (nLLDOpStep == 0)
                {
                    stLLDCpArg.nSrcPbn      =  pstDie->nCurPbn[nPlnIdx];
                    stLLDCpArg.nSrcPgOffset =  (UINT16)nPgOffset;
                }
                else
                {
                    stLLDCpArg.nDstPbn      =  pstDie->nCurPbn[nPlnIdx];
                    stLLDCpArg.nDstPgOffset =  (UINT16)nPgOffset;
                    stLLDCpArg.nRndInCnt    =  pstBMLCpArg[nWayIdx]->nRndInCnt;

                    if (pstDie->nNumOfLLDOp == 0)
                    {
                        stLLDCpArg.pstRndInArg  =  (LLDRndInArg *) pstBMLCpArg[nWayIdx]->pstRndInArg;

                        /* ADD to support non-blocking operation (2007/08/22) */
                        if (nWayIdx == (pstVol->nNumOfWays - 1))
                        {
                            nOrderFlag = BML_NBM_FLAG_END_OF_SAME_OPTYPE;
                        }
                    }
                    else /* In case of unpaired replacement */
                    {
                        FSR_OAM_MEMSET(&stLLDRndInArg, 0x00, sizeof(LLDRndInArg) * FSR_BML_CPBK_MAX_RNDINARG);
                        stLLDCpArg.pstRndInArg = &stLLDRndInArg[0];

		        if ((stLLDCpArg.nRndInCnt != 0) && (pstBMLCpArg[nWayIdx]->pstRndInArg != NULL))  /* hotfix_091103 */
        		{
                        	/* Allot random-in data for 2x-device performing 1x opration */
                        	_AllotRanInData(pstVol,
                                	        nPlnIdx,
	                                        stLLDCpArg.nRndInCnt,
        	                                pstBMLCpArg[nWayIdx]->pstRndInArg,
                	                        stLLDCpArg.pstRndInArg);
        		}
        		
                        /* ADD to support non-blocking operation (2007/08/22) */
                        if ((nWayIdx == (pstVol->nNumOfWays - 1)) &&
                            (nPlnIdx == (pstVol->nNumOfPlane- 1)))
                        {
                            nOrderFlag = BML_NBM_FLAG_END_OF_SAME_OPTYPE;
                        }
                    }
                }

                do
                {
                    nLLDRe = pstVol->LLD_CopyBack(nPDev,
                                                  &stLLDCpArg,
                                                  nCpFlag | nOrderFlag);

                    nMajorErr = FSR_RETURN_MAJOR(nLLDRe);
                    nMinorErr = FSR_RETURN_MINOR(nLLDRe);

                    switch (nMajorErr)
                    {
                    case FSR_LLD_SUCCESS:
                        bWrErr = FALSE32;
                        break;

                    case FSR_LLD_PREV_READ_ERROR:
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d)\r\n"), __FSR_FUNC__, nVol));
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_CopyBack(nPDev:%d, nRe:FSR_LLD_PREV_READ_ERROR) / %d line\r\n"),
                                                        nPDev, __LINE__));
                        nBMLRe = FSR_BML_READ_ERROR;

                        /* Set nBMLReMinor to inform the error way
                         * This minor error can indicate up to 2 way */
                        nMinorRe    = FSR_RETURN_VALUE(0, 01, 0x0000, nPDev + nIdx);

                        /* Return read error considerring the multi-way */
                        nBMLRe      |= nMinorRe;
                        break;

                    case FSR_LLD_PREV_READ_DISTURBANCE:
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:INF]   %s(nVol:%d)\r\n"), __FSR_FUNC__, nVol));
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:INF]   LLD_CopyBack(nPDev:%d, nRe:FSR_LLD_PREV_READ_DISTURBANCE) / %d line\r\n"),
                                                        nPDev, __LINE__));
                        break;

                    case FSR_LLD_PREV_WRITE_ERROR:
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d)\r\n"), __FSR_FUNC__, nVol));
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_CopyBack(nPDev:%d, nRe:FSR_LLD_PREV_WRITE_ERROR) / %d line\r\n"),
                                                        nPDev, __LINE__));

                        /* In program case of copyback*/
                        if (nCpFlag == nPgmFlag)
                        {
                            nMinorErr |= BML_CPYBACK_1X_ERROR;
                            nChkWayIdx = nWayIdx;

                            nSbn = pstDev->pstDie[nIdx]->nCurSbn[0];
                        } 

                        nBMLRe = _HandlePrevWrErr(nVol,
                                                  nPDev,
                                                  nDieIdx,
                                                  nMinorErr,
                                                  FALSE32);
                        if (nBMLRe == FSR_BML_SUCCESS)
                        {
                            if (nCpFlag == nPgmFlag)
                            {
                                /* translate Pbn[]*/
                                pstDev->pstDie[nIdx]->nNumOfLLDOp = 0;
                                _GetPBN(nSbn, pstVol, pstDev->pstDie[nIdx]);
                            }
                            else
                            {
                                /* Set bWrErr to write a current page */
                                bWrErr  = TRUE32;
                            }
                        }
                        break;

                    default: /* for example: FSR_LLD_INVALID_PARAM */

                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                            (TEXT("[BIF:ERR]   %s(nVol:%d) / %d line\r\n"), 
                            __FSR_FUNC__, nVol, __LINE__));
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                            (TEXT("[BIF:ERR]   LLD_CopyBack(nPDev:%d,nSrcPbn:%d,nSrcPgOff:%d,nDstPbn:%d,nDstPgOff:%d,nFlag:0x%x) / nRe:0x%x)\r\n"),
                            nPDev, nLLDRe,
                            stLLDCpArg.nSrcPbn,
                            stLLDCpArg.nSrcPgOffset,
                            stLLDCpArg.nDstPbn,
                            stLLDCpArg.nDstPgOffset,
                            nCpFlag | nOrderFlag));

                        nBMLRe = nLLDRe;
                        break;
                    }

                    /* The error case */
                    if (nBMLRe != FSR_BML_SUCCESS)
                    {
                        break;
                    }

                    if (nMinorErr & BML_CPYBACK_1X_ERROR)
                    {
                        break;
                    }

                } while (bWrErr == TRUE32);

                /* ADD to support non-blocking operation (2007/08/22) */
                nOrderFlag = BML_NBM_FLAG_CONTINUE_SAME_OPTYPE;

                /* The error case */
                if (nBMLRe != FSR_BML_SUCCESS)
                {
                    break;
                }

                if (nMinorErr & BML_CPYBACK_1X_ERROR)
                {
                    break;
                }

                /* Store the Previous operation Log after LLD_CopyBack(program) */
                if (nLLDOpStep == 1)
                {
                    pstDie->pstPreOp->nOpType           =  BML_PRELOG_WRITE;
                    pstDie->pstPreOp->nSbn              =  pstDie->nCurSbn[nPlnIdx];
                    pstDie->pstPreOp->nPgOffset         =  nPgOffset;
                    pstDie->pPreOpSBuf                  =  pstDie->pSBuf; 
                    pstDie->pPreOpSBuf->nNumOfMetaExt   =  pstVol->nSizeOfPage / FSR_PAGE_SIZE_PER_SPARE_BUF_EXT;
                }

            } while (nPlnIdx++ < pstDie->nNumOfLLDOp);

            /* The error case */
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                break;
            }

            if (nMinorErr & BML_CPYBACK_1X_ERROR)
            {
                /* Clear minor err massage */
                nMinorErr  = 0;

                /* Reset LLD Op step */
                nLLDOpStep = 0;

                continue;
            }

            ++nWayIdx;

        } while (nWayIdx < pstVol->nNumOfWays);

        /* the error of FSR_OQM_Put and _HandlePrevError for NonBlocking mode */
        if (nBMLRe != FSR_BML_SUCCESS)
        {
            break;
        }

    } while (++nLLDOpStep < BML_NUM_OF_LLD_CPBK_PER_PAGE);

    /* 
     * When only BB has opened (AP has not yet opend) in the dual core system,
     * all errors should be handled.
     * Otherwise, these errors can return to AP that is in open sequence.
     */
    if (nBMLRe == FSR_BML_SUCCESS)
    {
        nBMLRe = _HandleErrForDualCore(nVol);
        if (nBMLRe != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:0x%x) / %d line\r\n"),
                                        __FSR_FUNC__, nVol, nBMLRe, __LINE__));
        }
    }

    /* Release a semaphore */
    bRet = FSR_OAM_ReleaseSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
    if (bRet == FALSE32)
    {
        nBMLRe = FSR_BML_RELEASE_SM_ERROR;
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:FSR_BML_RELEASE_SM_ERROR) / %d line\r\n"),
                                        __FSR_FUNC__, nVol, __LINE__));
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nVol: %d, nRe: 0x%x)\r\n"), __FSR_FUNC__, nVol, nBMLRe));

    return nBMLRe;
}
#endif  /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief     This function erases virtual units.
 *
 *  @param [in]  nVol        : Volume number
 *  @param [in] *pVun        : Pointer to virtual unit number
 *  @param [in]  nNumOfUnits : The number of units to erase
 *  @param [in]  nFlag       : FSR_BML_FLAG_NONE
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_WR_PROTECT_ERROR
 *  @return     FSR_BML_WRITE_ERROR
 *  @return     FSR_BML_ERASE_ERROR
 *  @return     FSR_BML_INVALID_PARAM
 *  @return     FSR_BML_VOLUME_NOT_OPENED
 *  @return     FSR_BML_ACQUIRE_SM_ERROR
 *  @return     FSR_BML_RELEASE_SM_ERROR
 *  @return     Some LLD errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
FSR_BML_Erase(UINT32     nVol,
              UINT32    *pVun,
              UINT32     nNumOfUnits,
              UINT32     nFlag)
{
    UINT32       nVun       = 0;    /* Virtual Unit Number                      */
    UINT32       nBaseSbn   = 0;    /* Base Sbn                                 */
    UINT32       nPDev      = 0;    /* Physical Device Number                   */
    UINT32       nDevIdx    = 0;    /* Device index in a volume: 0 ~ 3          */
    UINT32       nTmpPln    = 0;    /* Temporary plane index                    */
    UINT32       nDieIdx    = 0;    /* Die index   : 0(chip 0) or 1(chip 1)     */
    UINT32       nPlnIdx    = 0;    /* Plane index : 0~(FSR_MAX_PLANES-1)       */
    UINT32       nPbn       = 0;    /* Pointer to physical block array          */
    UINT32       nNumOfBlks = 1;    /* The number of blocks to erase            */
    UINT32       nNumOfBlksInRsvr   = 0;/* # of reserved blks for Flex-OneNAND  */
    UINT32       nLockStat  = 0x00000000;   /* Lock state of OTP block          */
    UINT32       nSbn       = 0;        /* Semi-physical block number           */
    UINT32       nOrderFlag = BML_NBM_FLAG_START_SAME_OPTYPE;   /* LLD flag 
                                                    for non-blocking operation  */
    BOOL32       bExit      = TRUE32;   /* flag for re-erasing a block in error */
    BOOL32       bRet       = TRUE32;   /* Temporary return value               */
    INT32        nMajorErr;             /* Major Return error                   */
    INT32        nMinorErr;             /* Minor Return error                   */
    INT32        nBMLRe = FSR_BML_SUCCESS;
    INT32        nLLDRe = FSR_LLD_SUCCESS;
    INT32        nRet   = FSR_BML_SUCCESS;
    BmlVolCxt   *pstVol;
    BmlDevCxt   *pstDev;
    BmlDieCxt   *pstDie;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, *pVun: %d, nNumOfUnits: %d)\r\n"),
                                    __FSR_FUNC__, nVol, *pVun, nNumOfUnits));

    /* checking volume range */
    CHK_VOL_RANGE(nVol);

    /* Get the pointer to volume context */
    pstVol = _GetVolCxt(nVol);

    /* Check the pointer to volume context */
    CHK_VOL_POINTER(pstVol);

    /* check volume open */
    CHK_VOL_OPEN(pstVol->bVolOpen);

    FSR_ASSERT(nVol < FSR_MAX_VOLS);

    do
    {
        /* Get nVun */
        nVun = *pVun;

#if defined(BML_CHK_PARAMETER_VALIDATION)
        /* check the boundaries of input parameter*/
        if (nVun > pstVol->nLastUnit)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]  Vun is bigger than last unit \r\n")));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, *pVun: %d, nNumOfUnits: %d, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                            __FSR_FUNC__, nVol, *pVun, nNumOfUnits, __LINE__));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"),__FSR_FUNC__));
            return FSR_BML_INVALID_PARAM;
        }
#endif /* BML_CHK_PARAMETER_VALIDATION */

        /************************************************/
        /* Handle the 1st block OTP                     */
        /************************************************/
        /* Check whether the 1st block is OTP or not */
        if ((nVun == 0) && (pstVol->b1stBlkOTP))
        {
            /* Get the state of OTP Block */
            nBMLRe = _GetOTPInfo(nVol,
                                 &nLockStat);
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, *pVun: %d, nNumOfUnits: %d, nRe:0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, *pVun, nNumOfUnits, nBMLRe));
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"),__FSR_FUNC__));
                return nBMLRe;
            }

            /* If OTP block is locked, it cannot be programed */
            if (nLockStat & FSR_LLD_OTP_1ST_BLK_LOCKED)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, *pVun: %d, nNumOfUnits: %d, nRe:FSR_BML_WR_PROTECT_ERROR) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, *pVun, nNumOfUnits, __LINE__));
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"),__FSR_FUNC__));
                return FSR_BML_WR_PROTECT_ERROR;
            }
        } /* End of "if((nVun == 0) && ...)*/

        /* Check the RO partition */
        bRet = _IsROPartition(nVun, pstVol);
        if (bRet == TRUE32)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, *pVun: %d, nNumOfUnits: %d, nRe:FSR_BML_WR_PROTECT_ERROR)\r\n"),
                                            __FSR_FUNC__, nVol, *pVun, nNumOfUnits));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"),__FSR_FUNC__));
            return FSR_BML_WR_PROTECT_ERROR;
        }

        /*
         *----------------------------------------------
         * FSR_BML_Erase is called by a virtual unit.
         * So, it should call LLD_FlushOp and LLD_Erase 
         * for all the devices and dies.
         *----------------------------------------------
         */

        nPlnIdx = 0;
        
        /* Get semaphore */
        bRet = FSR_OAM_AcquireSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
        if (bRet == FALSE32)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, *pVun: %d, nNumOfUnits: %d, nRe:FSR_BML_ACQUIRE_SM_ERROR) / %d line\r\n"),
                                            __FSR_FUNC__, nVol, *pVun, nNumOfUnits, __LINE__));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"),__FSR_FUNC__));
            return FSR_BML_ACQUIRE_SM_ERROR;
        }
        
        do
        {
            /* The loop for device */
            nDevIdx = 0;
            do
            {
                /* Translate the nPDev */
                nPDev     = (nVol << DEVS_PER_VOL_SHIFT) + nDevIdx;

                /* Get the pointer to a device context */
                pstDev    = _GetDevCxt(nPDev);

                nDieIdx = 0;
                do
                {
                    /* Get the pointer to die context */
                    pstDie = pstDev->pstDie[nDieIdx];

                    if (nPlnIdx == 0)
                    {
                        /* Set the nNumOfBlksInRsvr according to nVun */
                        nNumOfBlksInRsvr = 0;
                        if (nVun > (pstVol->nNumOfSLCUnit - 1))
                        {
                            if (pstDie->pstRsv->nRsvrType == BML_HYBRID_RESERVOIR)
                            {
                                nNumOfBlksInRsvr = pstDie->pstRsv->nLastSbnOfRsvr - pstDie->pstRsv->n1stSbnOfRsvr + 1;
                            }
                        }
#if defined(FSR_BML_WAIT_OPPOSITE_DIE)
                        if (pstVol->nNumOfDieInDev == FSR_MAX_DIES)
                        {
                            /* Handle the previous error using LLD_FlushOp and _HandlePrevError */
                            nLLDRe = pstVol->LLD_FlushOp(nPDev,
                                                         (nDieIdx + 1) & 1,
                                                         FSR_LLD_FLAG_NONE);
                            if (nLLDRe != FSR_LLD_SUCCESS)
                            {
                                nBMLRe = _HandlePrevError(nVol,
                                                          nPDev,
                                                          (nDieIdx + 1) & 1,
                                                          nLLDRe);
                                if (nBMLRe != FSR_BML_SUCCESS)
                                {
                                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, *pVun: %d, nNumOfUnits: %d, nRe:0x%x)\r\n"),
                                                                    __FSR_FUNC__, nVol, *pVun, nNumOfUnits, nBMLRe));
                                    break;
                                }
                            } /* End of "if (nLLDRe != FSR_LLD_SUCCESS)" */
                        }
#endif

                        /* Handle the previous error using LLD_FlushOp and _HandlePrevError */
                        nLLDRe = pstVol->LLD_FlushOp(nPDev,
                                                     nDieIdx,
                                                     FSR_LLD_FLAG_NONE);
                        if (nLLDRe != FSR_LLD_SUCCESS)
                        {
                            nBMLRe = _HandlePrevError(nVol,
                                                      nPDev,
                                                      nDieIdx,
                                                      nLLDRe);
                            if (nBMLRe != FSR_BML_SUCCESS)
                            {
                                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, *pVun: %d, nNumOfUnits: %d, nRe:0x%x)\r\n"),
                                                                __FSR_FUNC__, nVol, *pVun, nNumOfUnits, nBMLRe));
                                break;
                            }
                        } /* End of "if (nLLDRe != FSR_LLD_SUCCESS)" */

                        /* Get the return value by partitionfor current partition */
                        nBMLRe = _GetPIRet(pstVol,
                                           pstDie,
                                           nVun,
                                           nBMLRe);
                        if (nBMLRe != FSR_BML_SUCCESS)
                        {
                            /* message out */
                            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, *pVun: %d, nNumOfUnits: %d, nRe:0x%x)\r\n"),
                                                            __FSR_FUNC__, nVol, *pVun, nNumOfUnits, nBMLRe));
                            break;
                        }

                        /* Translate Sbn[]*/
                        nTmpPln  = pstVol->nNumOfPlane - 1;
                        nBaseSbn = ((nDieIdx) << pstVol->nSftNumOfBlksInDie) +
                                    (nVun << pstVol->nSftNumOfPln) + nNumOfBlksInRsvr;

                        do
                        {
                            pstDie->nCurSbn[nTmpPln] = (UINT16)nBaseSbn + (UINT16)nTmpPln;
                            pstDie->nCurPbn[nTmpPln] = (UINT16)nBaseSbn + (UINT16)nTmpPln;
                        } while (nTmpPln-- > 0);

                        /* Second Translation: translate Pbn[]
                         * If the pstDev->nCurSbn[] should be replaced by a reserved block,
                         * pstDev->nNumOfLLDOp is equal to pstVol->nNumOfPlane-1.
                         * If not, pstDev->nNumOfLLDOp is equal to 0.
                         */
                        pstDie->nNumOfLLDOp = 0;
                        _GetPBN(pstDie->nCurSbn[0], pstVol, pstDie);
                    } /* End of "if (nPlnIdx == 0)*/

                    /* Call FSR_BBM_UpdateERL() to remove Sbn from ERL List*/
                    nRet = FSR_BBM_UpdateERL(pstVol,
                                             pstDev,
                                             nDieIdx,
                                             pstDie->nCurSbn[nPlnIdx],
                                             BML_FLAG_ERL_DELETE);
                    if (nRet != FSR_BML_SUCCESS)
                    {
                        /*
                         * Skip BBM Error.
                         * FSR_BBM_UpdateERL() has no influence with erase operation
                         */
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, *pVun: %d, nNumOfUnits: %d)\r\n"),
                                                        __FSR_FUNC__, nVol, *pVun, nNumOfUnits));
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   FSR_BBM_UpdateERL(nPDev: %d, nDieIdx: %d, nSbn: %d, nFlag: BBM_FLAG_ERL_DELETE, nRe:0x%x)\r\n"),
                                                        nPDev, nDieIdx, pstDie->nCurSbn[nPlnIdx], nRet));
                    }

                    /* ADD to support non-blocking operation (2007/08/22) */
                    if ((nDevIdx == (pstVol->nNumOfDev -     1))    &&
                        (nDieIdx == (pstVol->nNumOfDieInDev -1))    &&
                        (nPlnIdx == (pstVol->nNumOfPlane    -1)))
                    {
                        nOrderFlag = BML_NBM_FLAG_END_OF_SAME_OPTYPE;
                    }

                    /* Initialize bExit to handle a erase error */
                    bExit = TRUE32;
                    do
                    {
                        nPbn = (UINT32) pstDie->nCurPbn[nPlnIdx];

                        nLLDRe = pstVol->LLD_Erase(nPDev,
                                                   &nPbn,
                                                   nNumOfBlks,
                                                   FSR_LLD_FLAG_1X_ERASE | nOrderFlag);
                        /* Handle the erase error */
                        nMajorErr = FSR_RETURN_MAJOR(nLLDRe);
                        nMinorErr = FSR_RETURN_MINOR(nLLDRe);

                        if (nMajorErr == FSR_LLD_SUCCESS)
                        {
                            /* Initialize bExit */
                            bExit       = TRUE32;

                            /* Store the Pre operation Log */
                            pstDie->pstPreOp->nOpType      =  BML_PRELOG_ERASE;
                            pstDie->pstPreOp->nSbn         =  pstDie->nCurSbn[nPlnIdx];
                            pstDie->pstPreOp->nPgOffset    =  0;
                            pstDie->pPreOpMBuf             =  pstDie->pMBuf;
                            pstDie->pPreOpSBuf             =  pstDie->pSBuf;
                            pstDie->pstPreOp->nFlag        =  FSR_LLD_FLAG_1X_ERASE;
                        }
                        else if (nMajorErr == FSR_LLD_PREV_ERASE_ERROR)
                        {
                            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, *pVun: %d, nNumOfUnits: %d)\r\n"),
                                                            __FSR_FUNC__, nVol, *pVun, nNumOfUnits));
                            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_Erase(nPDev: %d, nPbn: %d, nErFlag:0x%x, nRe:FSR_LLD_PREV_ERASE_ERROR)\r\n"),
                                                            nPDev, nPbn, FSR_LLD_FLAG_1X_ERASE));

                            nSbn = pstDie->nCurSbn[0];

                            nBMLRe = _HandlePrevErErr(nVol,
                                                      nPDev,
                                                      nDieIdx,
                                                      nMinorErr);
                            if (nBMLRe != FSR_BML_SUCCESS)
                            {
                                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, *pVun: %d, nNumOfUnits: %d, nRe:0x%x)\r\n"),
                                                                __FSR_FUNC__, nVol, *pVun, nNumOfUnits, nBMLRe));
                                break;
                            }

                            /* translate Pbn[]*/
                            pstDie->nNumOfLLDOp = 0;
                            _GetPBN(nSbn, pstVol, pstDie);

                            /* Set bExit to erase a current block */
                            bExit = FALSE32;
                        }
                        else /* for example: FSR_LLD_INVALID_PARAM */
                        {
                            nBMLRe = nLLDRe;
                            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, *pVun: %d, nNumOfUnits: %d)\r\n"),
                                                            __FSR_FUNC__, nVol, *pVun, nNumOfUnits));
                            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_Erase(nPDev: %d, nPbn: %d, nErFlag:0x%x, nRe:0x%x) / %d line\r\n"),
                                                            nPDev, nPbn, FSR_LLD_FLAG_1X_ERASE, nBMLRe, __LINE__));
                            break;
                        }
                    } while (bExit == FALSE32);

                    /* ADD to support non-blocking operation (2007/08/22) */
                    nOrderFlag = BML_NBM_FLAG_CONTINUE_SAME_OPTYPE;

                    if (nBMLRe != FSR_BML_SUCCESS)
                    {
                        break;
                    }

                }while (++nDieIdx < pstVol->nNumOfDieInDev);

                /* 
                 * When only BB has opened (AP has not yet opend) in the dual core system,
                 * all errors should be handled.
                 * Otherwise, these errors can return to AP that is in open sequence.
                 */
                if (nBMLRe == FSR_BML_SUCCESS)
                {
                    nBMLRe = _HandleErrForDualCore(nVol);
                    if (nBMLRe != FSR_BML_SUCCESS)
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, *pVun: %d, nNumOfUnits: %d, nRe: 0x%x) / %d line\r\n"),
                                                    __FSR_FUNC__, nVol, *pVun, nNumOfUnits, nBMLRe, __LINE__));
                    }
                }

                if (nBMLRe != FSR_BML_SUCCESS)
                {
                    break;
                }

            } while(++nDevIdx < pstVol->nNumOfDev);

            if (nBMLRe != FSR_BML_SUCCESS)
            {
                break;
            }

        } while(++nPlnIdx < pstVol->nNumOfPlane);

        /* Release semaphore */
        bRet = FSR_OAM_ReleaseSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
        if (bRet == FALSE32)
        {
            nBMLRe = FSR_BML_RELEASE_SM_ERROR;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, *pVun: %d, nNumOfUnits: %d, nRe:FSR_BML_RELEASE_SM_ERROR) / %d line\r\n"),
                                            __FSR_FUNC__, nVol, *pVun, nNumOfUnits, __LINE__));
        }
        
        if (nBMLRe != FSR_BML_SUCCESS)
        {   
            break;
        }

        /* Increase nVun */
        pVun++;

        if (nBMLRe != FSR_BML_SUCCESS)
        {
            break;
        }

    } while (--nNumOfUnits);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, nBMLRe));

    return nBMLRe;
}
#endif  /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function formats the volume.
 *
 *  @param [in]  nVol        : Volume number
 *  @param [in] *pstPartI    : Pointer to FSRPartI structure
 *  @param [in]  nFlag       : FSR_BML_INIT_FORMAT
                              FSR_BML_REPARTITION
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_ALREADY_OPENED
 *  @return     FSR_BML_INVALID_PARAM
 *  @return     FSR_BML_UNLOCK_ERROR
 *  @return     FSR_BML_CRITICAL_ERROR
 *  @return     Some LLD errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
FSR_BML_Format(UINT32    nVol,
               FSRPartI *pstPartI,
               UINT32    nFlag)
{
    UINT32       nDevIdx    = 0;            /* Device Index                     */
    UINT32       nPDev      = 0;            /* Physical Device number           */
#if !defined(FSR_OAM_RTLMSG_DISABLE)
    UINT32       nVersion;                  /* Version name                     */
#endif
    BOOL32       bRet   = TRUE32;           /* Temporary return value           */
    INT32        nBMLRe = FSR_BML_SUCCESS;  /* BML Return value                 */
    INT32        nPAMRe = FSR_PAM_SUCCESS;  /* PAM Return value                 */
    INT32        nRe    = FSR_BML_SUCCESS;  /* Temporary return value for _Close*/
    FsrVolParm   stPAM[FSR_MAX_VOLS];
    BmlVolCxt   *pstVol;
    BmlDevCxt   *pstDev;
    FSRPartI     stPartIOut;
    UINT32      *pnGlockHdl;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   FSR VERSION: %s. \r\n"), FSR_Version(&nVersion)));

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, nFlag: 0x%x)\r\n"), __FSR_FUNC__, nVol, nFlag));

    FSR_ASSERT(((nFlag & BML_FORMAT_FLAG_MASK) == FSR_BML_INIT_FORMAT) || 
                ((nFlag & BML_FORMAT_FLAG_MASK) == FSR_BML_REPARTITION));

    /* Message to inform about nFlag*/
    if ((nFlag & BML_FORMAT_FLAG_MASK) == FSR_BML_INIT_FORMAT)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   Init formating\r\n")));
    }
    else /* if (nFlag == FSR_BML_REPARTITION) */
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   Repartitioning\r\n")));
    }

    /* Check the volume range */
    CHK_VOL_RANGE(nVol);

    /* Get pointer to volume context */
    pstVol = _GetVolCxt(nVol);

    /* Check the pointer to volume context */
    CHK_VOL_POINTER(pstVol);

    pnGlockHdl = _GetGLockHdl(nVol);

    /* Acquire semaphore */
    bRet = FSR_PAM_AcquireSL(*pnGlockHdl, FSR_OAM_SM_TYPE_BML);
    if (bRet == FALSE32)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nRe: FSR_BML_ACQUIRE_SM_ERROR) / %d line\r\n"),
                                        __FSR_FUNC__, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, FSR_BML_ACQUIRE_SM_ERROR));
        return FSR_BML_ACQUIRE_SM_ERROR;
    }

    do
    {
        /* PAM_GetPAParm */
        nPAMRe = FSR_PAM_GetPAParm(&stPAM[0]);
        if (nPAMRe != FSR_PAM_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:   ]   %s(nVol: %d, nFlag: 0x%x, nRe:0x%x)\r\n"),
                                            __FSR_FUNC__, nVol, nFlag, nPAMRe));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"),__FSR_FUNC__));
            nBMLRe = nPAMRe;
            break;
        }

        if (stPAM[nVol].bProcessorSynchronization == TRUE32)
        {
            /* Check the shared open count is more than one or not */
            if (*(pstVol->pnSharedOpenCnt) > 0)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:   ]   %s(nVol: %d, nFlag: 0x%x, nRe:FSR_BML_ALREADY_OPENED)\r\n"),
                                                __FSR_FUNC__, nVol, nFlag));
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"),__FSR_FUNC__));
                nBMLRe = FSR_BML_ALREADY_OPENED | FSR_BML_USE_MULTI_CORE;
                break;
            }
        }

        /* If FSR_BML_Open is already called, FSR_BML_Format returns BML_ALREADY_OPENED. */
        if (pstVol->bVolOpen == TRUE32)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:   ]   %s(nVol: %d, nFlag: 0x%x, nRe:FSR_BML_ALREADY_OPENED)\r\n"),
                                            __FSR_FUNC__, nVol, nFlag));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"),__FSR_FUNC__));
            nBMLRe = FSR_BML_ALREADY_OPENED;
            break;
        }

        /* check whether FSR_BML_AdjustPartInfo is called or not */
        if ((gbAdjustPartInfo[nVol] == TRUE32) ||
            (((nFlag & FSR_BML_AUTO_ADJUST_PARTINFO) == FSR_BML_AUTO_ADJUST_PARTINFO) && 
              (gbAdjustPartInfo[nVol] == FALSE32)))
        {
            gbAdjustPartInfo[nVol] = FALSE32;
        }
        else
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nFlag: 0x%x, nRe:FSR_BML_CANT_ADJUST_PARTINFO)\r\n"),
                                            __FSR_FUNC__, nVol, nFlag));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"),__FSR_FUNC__));
            nBMLRe = FSR_BML_CANT_ADJUST_PARTINFO;
            break;
        }

        do
        {
            nBMLRe = _Open(nVol);
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nFlag: 0x%x, nRe:0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nFlag, nBMLRe));
                break;
            }

            /* check PI validity */
            nBMLRe = _ChkPIValidity(nVol, pstPartI);
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nFlag: 0x%x, nRe:0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nFlag, nBMLRe));
                break;
            }

            /* If user wants to adjust partitition info automatically */
            if ((nFlag & FSR_BML_AUTO_ADJUST_PARTINFO) == FSR_BML_AUTO_ADJUST_PARTINFO)
            {
                /* adjust partition info */
                nBMLRe = _AdjustPartInfo(nVol, pstPartI, &stPartIOut);
                if (nBMLRe != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nFlag: 0x%x, nRe:0x%x)\r\n"),
                                                    __FSR_FUNC__, nVol, nFlag, nBMLRe));
                    break;
                }
                else
                {
                    /* use newly adjusted partition info instead of partition info given by user */
                    pstPartI = &stPartIOut;
                }
            }

            /* In order to repartition, Reservoir should be mounted */
            if ((nFlag & BML_FORMAT_FLAG_MASK)== FSR_BML_REPARTITION)
            {
                /* 
                 * mount reservoir and load previous partition info
                 * (Actually, load BMI and PI) 
                 */
                nBMLRe = _MountRsvr(nVol,
                                    pstVol->pstPartI,
                                    NULL);
                if (nBMLRe != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nFlag: 0x%x, nRe:0x%x)\r\n"),
                                                    __FSR_FUNC__, nVol, nFlag, nBMLRe));
                    break;
                }
            }

            /* print partition information */
            _PrintPartI(nVol, pstPartI);

            for (nDevIdx = 0; nDevIdx < DEVS_PER_VOL; nDevIdx++)
            {
                nPDev = nVol * DEVS_PER_VOL + nDevIdx;

                bRet = _IsOpenedDev(nPDev);
                if (bRet == FALSE32)
                {
                    continue;
                }

                /* Get pointer to device context */
                pstDev = _GetDevCxt(nPDev);

                if ((nFlag & BML_FORMAT_FLAG_MASK) == FSR_BML_INIT_FORMAT)
                {
                    nBMLRe = FSR_BBM_Format(pstVol,
                                            pstDev,
                                            pstPartI);
                    if (nBMLRe != FSR_BML_SUCCESS)
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nFlag: 0x%x)\r\n"),
                                                        __FSR_FUNC__, nVol, nFlag));
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   FSR_BBM_Format(nPDev:%d, nRe:0x%x) / %d line\r\n"),
                                                        nPDev, nBMLRe, __LINE__));
                        /* Return FSR_BBM_ERROR */
                        break;
                    }
                }
                /* repartition case */
                else
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   After BML format with repartition flag,\r\n")));
                    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   STL format should be executed for each partition\r\n")));
                    FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   And repartition may damage wear-level of the device\r\n")));

                    nBMLRe = FSR_BBM_Repartition(pstVol,
                                                 pstDev,
                                                 pstPartI);
                    if (nBMLRe != FSR_BML_SUCCESS)
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nFlag: 0x%x)\r\n"),
                                                        __FSR_FUNC__, nVol, nFlag));
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   FSR_BBM_Repartition(nPDev:%d, nRe:0x%x) / %d line\r\n"),
                                                        nPDev, nBMLRe, __LINE__));
                        /* Return FSR_BBM_ERROR */
                        break;
                    }
                }
            }

        } while (0); /* End of " _Open()"*/

        nRe = _Close(nVol);
        if (nRe != FSR_BML_SUCCESS)
        {
            nBMLRe = FSR_BML_CRITICAL_ERROR;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nFlag: 0x%x, nRe:FSR_BML_CRITICAL_ERROR)\r\n"),
                                            __FSR_FUNC__, nVol, nFlag));
        }

    } while (0); /* End of "FSR_OAM_AcquireSM()" */

    bRet = FSR_PAM_ReleaseSL(*pnGlockHdl, FSR_OAM_SM_TYPE_BML);
    if (bRet == FALSE32)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nRe: FSR_BML_RELEASE_SM_ERROR) / %d line\r\n"),
                                        __FSR_FUNC__, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, FSR_BML_RELEASE_SM_ERROR));
        nBMLRe = FSR_BML_RELEASE_SM_ERROR;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, nBMLRe));

    return nBMLRe;
}
#endif /* FSR_NBL2*/

#if !defined(FSR_NBL2)
/**
 *  @brief      This function get the paired page mapping information for MLC NAND.
 *
 *  @param [in]  nVol        : Volume number
 *  @param [in]  nVPgOff     : Virtual page offset in Vun
 *
 *  @return     The paired virtual page offset in Vun
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
FSR_BML_GetPairedVPgOff(UINT32        nVol,
                        UINT32        nVPgOff)
{
    UINT32       nPairedPgOff   = 0;  /* Paired page offet    */
    BmlVolCxt   *pstVol;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, nVPgOff: %d)\r\n"), __FSR_FUNC__, nVol, nVPgOff));

    /* Get pointer to volume context */
    pstVol = _GetVolCxt(nVol);

    /* Check the pointer to volume context */
    CHK_VOL_POINTER(pstVol);

    /* ======================================================================
     * Information of paired page for MLCNAND
     * -------------+-------------+-
     *   LSB Page   |   MSB Page  |     The paired page scheme is kept in the same physical block.
     * -------------+-------------+-      
     *    0x00    <--->   0x04    |     If the multi-plane scheme is supported,
     *    0x01    <--->   0x05    |     the page offset is same as each other.
     *    0x02    <--->   0x08    |
     *    0x03    <--->   0x09    |
     *             ...            |
     * ---------------------------+-
     * 
     * ======================================================================
     *        Dev0            Dev1
     *    +-----------+    +-----------+     PPgOff 
     *    |nVPgOff = 0|    |nVPgOff = 1| => 0x00 <=+
     *    |nVPgOff = 2|    |nVPgOff = 3| => 0x01 <-|--+
     *    |nVPgOff = 4|    |nVPgOff = 5| => 0x02   |  |
     *    |nVPgOff = 6|    |nVPgOff = 7| => 0x03   |  | <=== page pair
     *    |nVPgOff = 8|    |nVPgOff = 9| => 0x04 <=+  |
     *    |nVPgOff =10|    |nVPgOff =11| => 0x05 <----+
     *    |nVPgOff =12|    |nVPgOff =13| => 0x06
     *    |nVPgOff =14|    |nVPgOff =15| => 0x07
     *    |    ...    |    |    ...    |     ...
     *    +-----------+    +-----------+
     * =====================================================================
     * STEP1: Find the PPgOff( = the Physical PgOff in block)
     *
     * PPgOff = pstVol->pPairedPgMap[nVPgOff >> pstVol->nSftNumOfWays]
     * =====================================================================
     * STEP2: Calculate the nBaseVPgOff( = the nVPgOff of 1st way)
     * 
     * nBaseVPgOff = PPgOff << pstVol->nSftNumOfWays
     * =====================================================================
     * STEP3: Calculate the nVPgOff
     * 
     * nPairedPgOff = nBaseVPgOff + (nVPgOff & pstVol->nMaskWays)
     * =====================================================================
     */
    if ((pstVol->nNANDType == FSR_LLD_FLEX_ONENAND) ||
        (pstVol->nNANDType == FSR_LLD_MLC_NAND))
    {
        nPairedPgOff = (pstVol->pPairedPgMap[nVPgOff >> pstVol->nSftNumOfWays] << pstVol->nSftNumOfWays) +
                       (nVPgOff & pstVol->nMaskWays);
    }
    else
    {
        nPairedPgOff = nVPgOff;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"),__FSR_FUNC__));

    return nPairedPgOff;
}
#endif

#if !defined(FSR_NBL2)
/**
 *  @brief      This function get the real page offset of LSBPgOff for MLC NAND.
 *
 *  @param [in]  nVol        : Volume number
 *  @param [in]  nLSBPgOff   : Virtual LSB page offset in Vun
 *
 *  @return     The real virtual page offset in Vun
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
FSR_BML_GetVPgOffOfLSBPg(UINT32        nVol,
                         UINT32        nLSBPgOff)
{
    UINT32       nVPgOff    = 0;    /* Virtual page offset */
    BmlVolCxt   *pstVol;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, nLSBPgOff: %d)\r\n"),
                                    __FSR_FUNC__, nVol, nLSBPgOff));

    /* Get pointer to volume context */
    pstVol = _GetVolCxt(nVol);

    /* Check the pointer to volume context */
    CHK_VOL_POINTER(pstVol);

    /* =======================================================================================
     * Information of Page offset for LSB page (MLC NAND)
     * +---------------------+----------------------+
     * |   LSB Page offset   |   Real Page offset   |
     * +---------------------+----------------------+
     * |        0x00       <--->       0x00         |
     * |        0x01       <--->       0x01         |
     * |        0x02       <--->       0x02         |
     * |        0x03       <--->       0x03         |
     * |        0x04       <--->       0x06         |
     * |        0x05       <--->       0x07         |
     * |        0x06       <--->       0x0A         |
     * |        0x07       <--->       0x0B         |
     * +--------------------------------------------+ 
     * =======================================================================================
     *          Dev0                Dev1                           Dev0              Dev1
     *    +--------------+    +--------------+     PgOff      +------------+    +------------+
     *    |nVLSBPgOff = 0|    |nVLSBPgOff = 1|  =>  0x00  =>  |nVPgOff = 0 |    |nVPgOff = 1 |
     *    |nVLSBPgOff = 2|    |nVLSBPgOff = 3|  =>  0x01  =>  |nVPgOff = 2 |    |nVPgOff = 1 |
     *    |nVLSBPgOff = 4|    |nVLSBPgOff = 5|  =>  0x02  =>  |nVPgOff = 4 |    |nVPgOff = 5 |
     *    |nVLSBPgOff = 6|    |nVLSBPgOff = 7|  =>  0x03  =>  |nVPgOff = 6 |    |nVPgOff = 7 |
     *    |nVLSBPgOff = 8|    |nVLSBPgOff = 9|  =>  0x06  =>  |nVPgOff = 12|    |nVPgOff = 13|
     *    |nVLSBPgOff =10|    |nVLSBPgOff =11|  =>  0x07  =>  |nVPgOff = 14|    |nVPgOff = 15|
     *    |nVLSBPgOff =12|    |nVLSBPgOff =13|  =>  0x0A  =>  |nVPgOff = 20|    |nVPgOff = 21|
     *    |nVLSBPgOff =14|    |nVLSBPgOff =15|  =>  0x0B  =>  |nVPgOff = 22|    |nVPgOff = 23|
     *    |     ...      |    |     ...      |     ....   =>  |     ...    |    |     ...    |
     *    +--------------+    +--------------+                +------------+    +------------+
     * =======================================================================================
     * STEP1: Find the PgOff( = the Physical LSB PgOff in block)
     *
     * PgOff = pstVol->pnLSBPgs[nLSBPgOff >> pstVol->nSftNumOfWays]
     * =======================================================================================
     * STEP2: Calculate the nBaseVPgOff( = the nVPgOff of 1st way)
     * 
     * nBaseVPgOff = PgOff << pstVol->nSftNumOfWays
     * =======================================================================================
     * STEP3: Calculate the nVPgOff
     * 
     * nVPgOff = nBaseVPgOff + (nLSBPgOff & pstVol->nMaskWays)
     * =======================================================================================
     */
    if ((pstVol->nNANDType == FSR_LLD_FLEX_ONENAND) ||
        (pstVol->nNANDType == FSR_LLD_MLC_NAND))
    {
        nVPgOff = (pstVol->pLSBPgMap[nLSBPgOff >> pstVol->nSftNumOfWays] << pstVol->nSftNumOfWays) +
                  (nLSBPgOff & pstVol->nMaskWays);
    }
    else
    {
        nVPgOff = nLSBPgOff;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"),__FSR_FUNC__));

    return nVPgOff;
}
#endif

/**
 *  @brief      This function acquires the semaphore for the given volume.
 *
 *  @param [in]  nVol    : Volume number
 *
 *  @return     none
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC VOID
FSR_BML_AcquireSM(UINT32    nVol)
{
    BOOL32       bRet   = TRUE32;
    BmlVolCxt   *pstVol;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));

    /* Get pointer to volume context */
    pstVol  = _GetVolCxt(nVol);

    bRet = FSR_OAM_AcquireSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
    if (bRet == FALSE32)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, Can't acquire SM) / %d line\r\n"),
                                        __FSR_FUNC__, nVol, __LINE__));
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"),__FSR_FUNC__));
}

/**
 *  @brief      This function releases the semaphore for the given volume.
 *
 *  @param [in]  nVol    : Volume number
 *
 *  @return     none
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC VOID
FSR_BML_ReleaseSM(UINT32    nVol)
{
    BOOL32       bRet   = TRUE32;
    BmlVolCxt   *pstVol;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));

    /* Get pointer to volume context */
    pstVol  = _GetVolCxt(nVol);

    bRet = FSR_OAM_ReleaseSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
    if (bRet == FALSE32)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, Can't release SM) / %d line\r\n"),
                                        __FSR_FUNC__, nVol, __LINE__));
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s\r\n"),__FSR_FUNC__));
}

#if !defined(FSR_NBL2)
/**
 *  @brief      This function stores Partition Information Extension into PIA.
 *
 *  @param [in]  nVol    : Volume number
 *  @param [in] *pExtInfo: Pointer to FSRPIExt structure
 *  @param [in]  nFlag   : FSR_BML_FLAG_NONE

 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_INVALID_PARAM
 *  @return     FSR_BML_ACQUIRE_SM_ERROR
 *  @return     FSR_BML_RELEASE_SM_ERROR
 *  @return     FSR_BML_VOLUME_NOT_OPENED
 *  @return     Some BBM errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
FSR_BML_StorePIExt(UINT32      nVol,
                   FSRPIExt   *pPIExt,
                   UINT32      nFlag)
{
    BOOL32       bRet   = TRUE32;
    INT32        nBMLRe = FSR_BML_SUCCESS;
    BmlVolCxt   *pstVol;
    BmlDevCxt   *pstDev;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d,nID: 0x%x, nFlag: 0x%x)\r\n"),
                                    __FSR_FUNC__, nVol, pPIExt->nID, nFlag));

    /* Check a volume range */
    CHK_VOL_RANGE(nVol);

    /* Get the pointer of volume context */
    pstVol = _GetVolCxt(nVol);

    /* Check the pointer to volume context */
    CHK_VOL_POINTER(pstVol);

    /* Check whether this volume is opened */
    CHK_VOL_OPEN(pstVol->bVolOpen);

    do
    {
        if (pPIExt == NULL)
        {
            nBMLRe = FSR_BML_INVALID_PARAM;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]  Pointer of PIExt is NULL \r\n")));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, pPIExt:0x%x, nFlag:0x%x, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                            __FSR_FUNC__, nVol, pPIExt, nFlag, __LINE__));
            break;
        }

        /* Just store PIExt to the 1st device in volume */
        pstDev = _GetDevCxt(nVol << DEVS_PER_VOL_SHIFT);

        /* Acquire semaphore */
        bRet = FSR_OAM_AcquireSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
        if (bRet == FALSE32)
        {
            nBMLRe = FSR_BML_ACQUIRE_SM_ERROR;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d,nID: 0x%x, nFlag: 0x%x, nRe:FSR_BML_ACQUIRE_SM_ERROR)\r\n"),
                                            __FSR_FUNC__, nVol, pPIExt->nID, nFlag));
            break;
        }

        nBMLRe = FSR_BBM_UpdatePIExt(pstVol,
                                     pstDev,
                                     pPIExt);
        if (nBMLRe != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nID: 0x%x, nFlag: 0x%x)\r\n"),
                                            __FSR_FUNC__, nVol, pPIExt->nID, nFlag));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]  FSR_BBM_UpdatePIExt(nPDev:%d, nRe:0x%x) / %d line\r\n"),
                                            nVol << DEVS_PER_VOL_SHIFT, nBMLRe));
        }
        else
        {
            if (pPIExt->nSizeOfData > FSR_BML_MAX_PIEXT_DATA)
            {
                pPIExt->nSizeOfData = FSR_BML_MAX_PIEXT_DATA;
            }

            /* Memcpy pData*/
            FSR_OAM_MEMCPY(pstVol->pPIExt, pPIExt, sizeof(FSRPIExt));
        }

        /* Release semaphore */
        bRet = FSR_OAM_ReleaseSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
        if (bRet == FALSE32)
        {
            nBMLRe = FSR_BML_RELEASE_SM_ERROR;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d,nID: 0x%x, nFlag: 0x%x, nRe:FSR_BML_RELEASE_SM_ERROR)\r\n"),
                                            __FSR_FUNC__, nVol, pPIExt->nID, nFlag));
            break;
        }

    } while(0);

   FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, nBMLRe));

    return nBMLRe;
}
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function loads Partition Information Extension from PIA.
 *
 *  @param [in]  nVol     : Volume number
 *  @param [in]   nID     : ID of Partition Information Extension Entry
 *  @param [out] *pExtInfo: pointer to FSRPIExt structure
 *  @param [in]  nFlag    : FSR_BML_FLAG_NONE

 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_INVALID_PARAM
 *  @return     FSR_BML_NO_PIENTRY
 *  @return     FSR_BML_VOLUME_NOT_OPENED
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
FSR_BML_LoadPIExt(UINT32        nVol,
                  FSRPIExt     *pPIExt,
                  UINT32        nFlag)
{
    INT32        nBMLRe = FSR_BML_SUCCESS;
    BmlVolCxt   *pstVol;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, nFlag: 0x%x)\r\n"),
                                    __FSR_FUNC__, nVol, nFlag));

    /* Check the volume range */
    CHK_VOL_RANGE(nVol);

    /* Get pointer of volume context */
    pstVol = _GetVolCxt(nVol);

    /* Check the pointer to volume context */
    CHK_VOL_POINTER(pstVol);

    /* Check whether this volume is opened */
    CHK_VOL_OPEN(pstVol->bVolOpen);

    do
    {
        if (pPIExt == NULL)
        {
            nBMLRe = FSR_BML_INVALID_PARAM;

            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]  Pointer of PIExt is NULL \r\n")));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,
                (TEXT("[BIF:ERR]   %s(nVol: %d, pPIExt:0x%x, nFlag: 0x%x, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                __FSR_FUNC__, nVol, pPIExt, nFlag, __LINE__));
            break;
        }

        FSR_OAM_MEMCPY(pPIExt, pstVol->pPIExt, sizeof(FSRPIExt));

    } while(0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, nBMLRe));

    return nBMLRe;
}
#endif

#if !defined(FSR_NBL2)
/**
 *  @brief      This function reads some pages of OTP block.
 *
 *  @param [in]  nVol        : Volume number
 *  @param [in]  nVpn        : First virtual page number for reading
                              (nVpn range = # of pages of SLC block)
 *  @param [in]  nNumOfPgs   : The number of pages to read
 *  @param [in] *pMBuf       : Main page buffer for reading
 *  @param [in] *pSBuf       : Spare page buffer for reading
 *  @param [in]  nFlag       : FSR_BML_FLAG_ECC_ON
                               FSR_BML_FLAG_ECC_OFF
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_READ_ERROR
 *  @return     FSR_BML_CRITICAL_ERROR
 *  @return     FSR_BML_INVALID_PARAM
 *  @return     FSR_BML_ACQUIRE_SM_ERROR
 *  @return     FSR_BML_RELEASE_SM_ERROR
 *  @return     FSR_BML_VOLUME_NOT_OPENED
 *  @return     FSR_BML_UNSUPPORTED_FUNCTION
 *  @return     Some LLD errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
FSR_BML_OTPRead(UINT32        nVol,
                UINT32        nVpn,
                UINT32        nNumOfPgs,
                UINT8        *pMBuf,
                FSRSpareBuf  *pSBuf,
                UINT32        nFlag)
{
    UINT32          nPDev       = 0;        /* Physical Device Number           */
    UINT32          nPgOffset   = 0;        /* Physical Page Offset in block    */
    UINT32          nByteRet    = 0;        /* # of bytes returned              */
    UINT32          nPbn        = 0;        /* Physical Block Number            */
    UINT32          nDieIdx     = 0;        /* Die Index (always 0)             */
    UINT32          nLLDFlag    = 0x00000000;   /* LLD Flag for ECC ON/OFF      */
    BOOL32          bRet        = TRUE32;   /* Temporary return value           */
    INT32           nMajorErr;              /* Major Return Error               */
    INT32           nLLDRe  = FSR_LLD_SUCCESS;
    INT32           nBMLRe  = FSR_BML_SUCCESS;
    BmlVolCxt      *pstVol;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, nVpn: %d, nNumOfPgs: %d, nFlag: 0x%x)\r\n"),
                                    __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag));

    /* check volume range */
    CHK_VOL_RANGE(nVol);

    /* Get pointer to Volume context */
    pstVol = _GetVolCxt(nVol);

    /* Check the pointer to volume context */
    CHK_VOL_POINTER(pstVol);

    /* check volume open */
    CHK_VOL_OPEN(pstVol->bVolOpen);

    FSR_ASSERT(nVol < FSR_MAX_VOLS);

    /* Check whether the current mode is OTP or not */
    if (pstVol->bOTPEnable == FALSE32)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn: %d, nNumOfPgs: %d, nFlag: 0x%x, nRe:FSR_BML_UNSUPPORTED_FUNCTION) / %d line\r\n"),
                                        __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FSR_BML_UNSUPPORTED_FUNCTION));
        return FSR_BML_UNSUPPORTED_FUNCTION;
    }

    /* Set ECC ON/OFF flag */
    nLLDFlag = nFlag & FSR_BML_FLAG_ECC_MASK;

    if ((nFlag & FSR_BML_FLAG_USE_SPAREBUF) == FSR_BML_FLAG_USE_SPAREBUF)
    {
        nLLDFlag |= FSR_LLD_FLAG_USE_SPAREBUF;
    }

    /************************************************/
    /* Address translation                          */
    /************************************************/
    /* STEP1: translate PgOffset for SLC area */
    nPgOffset = nVpn & pstVol->nMaskSLCPgs;     /* nVpn range = 0~63(# of pages in SLC block) */

    if (nFlag & FSR_BML_FLAG_DUMP)
    {
        nPDev       = (nVpn >> FSR_BML_DUMP_OTP_PDEV_SHIFTBIT) & FSR_BML_DUMP_OTP_MASK;
        nDieIdx     = (nVpn >> FSR_BML_DUMP_OTP_DIE_SHIFTBIT) & FSR_BML_DUMP_OTP_MASK;
        nLLDFlag   |= FSR_LLD_FLAG_DUMP_ON;
    }
    else
    {
        /* STEP2: translate PDev */
        nPDev    = nVol << DEVS_PER_VOL_SHIFT;
    }

#if defined(BML_CHK_PARAMETER_VALIDATION)
    /* check the boundary of input parameter */
    if (pstVol->nNumOfPgsInSLCBlk < nPgOffset + nNumOfPgs)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]  Page offset value is bigger than number of page in SLC block \r\n")));
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn: %d, nNumOfPgs: %d, nFlag: 0x%x, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                        __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, FSR_BML_INVALID_PARAM));
        return FSR_BML_INVALID_PARAM;
    }

    /* check the validity of input parameter */
    if (nNumOfPgs == 0)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Number of page is NULL \r\n")));
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn: %d, nNumOfPgs: %d, nFlag: 0x%x, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                        __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, FSR_BML_INVALID_PARAM));
        return FSR_BML_INVALID_PARAM;
    }

    /* check the main and spare buffer pointer */
    if ((pMBuf == NULL) && (pSBuf == NULL))
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Main and Spare buffer point is NULL \r\n")));
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] ++%s(nVol: %d, nVpn: %d, pMBuf:0x%x, pSBuf:0x%x, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                        __FSR_FUNC__, nVol, nVpn, pMBuf, pSBuf, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, FSR_BML_INVALID_PARAM));
        return FSR_BML_INVALID_PARAM;
    }
#endif /* BML_CHK_PARAMETER_VALIDATION */

    /* Acquire semaphore */
    bRet = FSR_OAM_AcquireSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
    if (bRet == FALSE32)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn: %d, nNumOfPgs: %d, nFlag: 0x%x, nRe:FSR_BML_ACQUIRE_SM_ERROR) / %d line\r\n"),
                                        __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, FSR_BML_ACQUIRE_SM_ERROR));
        return FSR_BML_ACQUIRE_SM_ERROR;
    }

    /************************************************************/
    /* <Procedure of FSR_BML_OTPRead()                          */
    /* STEP1. Check and Handle previous error                   */
    /* STEP2. Set OTP Mode                                      */
    /* STEP3. Read data form OTP block                          */
    /* STEP4. Set NAND Core reset mode                          */
    /************************************************************/
    do
    {
        /************************************************/
        /* STEP1. Check and Handle previous error       */
        /************************************************/

        /* Handle previous error in opposite die*/
        if (pstVol->nNumOfDieInDev == FSR_MAX_DIES)
        {
            /* Handle the previous error using LLD_FlushOp and _HandlePrevError */
            nLLDRe = pstVol->LLD_FlushOp(nPDev,
                                         (nDieIdx + 1) & 1,
                                         FSR_LLD_FLAG_NONE);
            if (nLLDRe != FSR_LLD_SUCCESS)
            {
                nBMLRe = _HandlePrevError(nVol,
                                          nPDev,
                                          (nDieIdx + 1) & 1,
                                          nLLDRe);
                if (nBMLRe != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn: %d, nNumOfPgs: %d, nFlag: 0x%x, nRe:0x%x)\r\n"),
                                                     __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag, nBMLRe));
                    break;
                }
            } /* End of "if (nLLDRe != FSR_LLD_SUCCESS)" */
        }

        nLLDRe = pstVol->LLD_FlushOp(nPDev,
                                     nDieIdx,
                                     FSR_LLD_FLAG_NONE);
        if (nLLDRe != FSR_LLD_SUCCESS)
        {
            nBMLRe = _HandlePrevError(nVol,
                                      nPDev,
                                      nDieIdx,
                                      nLLDRe);
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn: %d, nNumOfPgs: %d, nFlag: 0x%x, nRe:0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag, nBMLRe));
                break;
            }
        }

        /************************************************/
        /* STEP2. Set OTP Mode by FSR_LLD_IOCTL()       */
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
            /* Return nLLDRe directly */
            nBMLRe = nLLDRe;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn: %d, nNumOfPgs: %d, nFlag: 0x%x)\r\n"),
                                            __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_IOCtl(nPDev:%d, nCode: FSR_LLD_IOCTL_OTP_ACCESS, nDieIdx:%d, nRe:0x%x) / %d line\r\n"),
                                            nPDev, nDieIdx, nBMLRe, __LINE__));
            break;
        }

        /************************************************/
        /* STEP3. Read data form OTP block              */
        /************************************************/
        /* STEP3-1. Get real OTP block # */
        nPbn     = 0;

        nNumOfPgs += nPgOffset;
        /* STEP3-2. Read data by LLD_Read() */
        for (;nPgOffset < nNumOfPgs ; nPgOffset++)
        {
            nLLDRe = pstVol->LLD_Read(nPDev,
                                      nPbn,
                                      nPgOffset,
                                      pMBuf,
                                      pSBuf,
                                      FSR_LLD_FLAG_1X_LOAD | nLLDFlag);

            nLLDRe = pstVol->LLD_Read(nPDev,
                                      nPbn,
                                      nPgOffset,
                                      pMBuf,
                                      pSBuf,
                                      FSR_LLD_FLAG_TRANSFER | nLLDFlag);

            /* Handle read error */
            nMajorErr = FSR_RETURN_MAJOR(nLLDRe);

            switch(nMajorErr)
            {
            case FSR_LLD_SUCCESS:
                break;

            case FSR_LLD_PREV_READ_DISTURBANCE:
                break;

            case FSR_LLD_PREV_READ_ERROR:
                nBMLRe = nLLDRe;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn: %d, nNumOfPgs: %d, nFlag: 0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_Read(nPDev: %d, nPbn: %d, nPgOffset: %d, nRe:FSR_LLD_PREV_READ_ERROR) / %d line\r\n"), 
                                                __FSR_FUNC__, nPDev, nPbn, nPgOffset, __LINE__));
                break;

            case FSR_LLD_INVALID_PARAM:
                nBMLRe = nLLDRe;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn: %d, nNumOfPgs: %d, nFlag: 0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_Read(nPDev: %d, nPbn: %d, nPgOffset: %d, nRe:FSR_LLD_INVALID_PARAM) / %d line\r\n"), 
                                                __FSR_FUNC__, nPDev, nPbn, nPgOffset, __LINE__));
                break;

            default:
                /* If code reaches this line, abnormal case.. */
                nBMLRe = nLLDRe;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn: %d, nNumOfPgs: %d, nFlag: 0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_Read(nPDev: %d, nPbn: %d, nPgOffset: %d, nRe:0x%x) / %d line\r\n"), 
                                                __FSR_FUNC__, nPDev, nPbn, nPgOffset, nBMLRe, __LINE__));
                break;

            } /* End of "switch(nMajorErr)" */

            /* error case */
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                break;
            }

            /* Increase the pMBuf pointer when pMBuf is not NULL */
            if (pMBuf != NULL)
            {
                pMBuf += pstVol->nSizeOfPage;
            }

            /* Increase the spare buffer pointer when pSBuf is not NULL */
            if (pSBuf != NULL)
            {
                pSBuf ++;
            }

        }/* End of "for (;nPgOffset <...)" */

        /************************************************/
        /* STEP4. Set NAND Core reset mode              */
        /************************************************/
        nLLDRe = pstVol->LLD_IOCtl(nPDev,
                                   FSR_LLD_IOCTL_HOT_RESET,
                                   (UINT8 *) &nDieIdx,
                                   sizeof(nDieIdx),
                                   NULL,
                                   0,
                                   &nByteRet);
        if (nLLDRe != FSR_LLD_SUCCESS)
        {
            /* Return nLLDRe directly */
            nBMLRe = nLLDRe;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn: %d, nNumOfPgs: %d, nFlag: 0x%x)\r\n"),
                                            __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_IOCtl(nPDev: %d, nCode: FSR_LLD_IOCTL_HOT_RESET, nDieIdx:%d, nRe:0x%x) / %d line\r\n"), 
                                            __FSR_FUNC__, nPDev, nDieIdx, nBMLRe, __LINE__));
            break;
        }

    } while(0);

    /* Release semaphore */
    bRet = FSR_OAM_ReleaseSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
    if (bRet == FALSE32)
    {
        nBMLRe = FSR_BML_RELEASE_SM_ERROR;
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn: %d, nNumOfPgs: %d, nFlag: 0x%x, nRe:FSR_BML_RELEASE_SM_ERROR)\r\n"),
                                        __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag));
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, nBMLRe));

    return nBMLRe;
}
#endif

#if !defined(FSR_NBL2)
/**
 *  @brief      This function writes some pages of OTP block.
 *
 *  @param [in]  nVol        : Volume number
 *  @param [in]  nVpn        : First virtual page number for writing
                              (nVpn range = # of pages of SLC block)
 *  @param [in]  nNumOfPgs   : The number of pages to write
 *  @param [in] *pMBuf       : Main page buffer for writing
 *  @param [in] *pSBuf       : Spare page buffer for writing
 *  @param [in]  nFlag       : FSR_BML_FLAG_ECC_ON
 *  @n                         FSR_BML_FLAG_ECC_OFF
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_CRITICAL_ERROR
 *  @return     FSR_BML_INVALID_PARAM
 *  @return     FSR_BML_ACQUIRE_SM_ERROR
 *  @return     FSR_BML_RELEASE_SM_ERROR
 *  @return     FSR_BML_VOLUME_NOT_OPENED
 *  @return     FSR_BML_UNSUPPORTED_FUNCTION
 *  @return     FSR_BML_WR_PROTECT_ERROR
 *  @return     Some LLD errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
FSR_BML_OTPWrite(UINT32        nVol,
                 UINT32        nVpn,
                 UINT32        nNumOfPgs,
                 UINT8        *pMBuf,
                 FSRSpareBuf  *pSBuf,
                 UINT32        nFlag)
{
    UINT32          nPDev       = 0;            /* Physical Device Number           */
    UINT32          nPgOffset   = 0;            /* Physical Page Offset in block    */
    UINT32          nByteRet    = 0;            /* # of bytes returned              */
    UINT32          nPbn        = 0;            /* Physical Block Number            */
    UINT32          nDieIdx     = 0;            /* Die Index (always 0)             */
    UINT32          nLockStat   = 0x00000000;   /* Locked state of block            */
    UINT32          nLLDFlag    = 0x00000000;   /* LLD Flag for ECC ON/OFF          */
    BOOL32          bRet        = TRUE32;       /* Temporary return value           */
    INT32           nLLDRe  = FSR_LLD_SUCCESS;
    INT32           nBMLRe  = FSR_BML_SUCCESS;
    BmlVolCxt      *pstVol;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, nVpn: %d, nNumOfPgs: %d, nFlag: 0x%x)\r\n"),
                                    __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag));

    /* check volume range */
    CHK_VOL_RANGE(nVol);

    /* Get pointer to Volume context */
    pstVol = _GetVolCxt(nVol);

    /* Check the pointer to volume context */
    CHK_VOL_POINTER(pstVol);

    /* check volume open */
    CHK_VOL_OPEN(pstVol->bVolOpen);

    FSR_ASSERT(nVol < FSR_MAX_VOLS);

    /* Check whether this volume supports OTP mode or not */
    if (pstVol->bOTPEnable == FALSE32)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn: %d, nNumOfPgs: %d, nFlag: 0x%x, nRe:FSR_BML_UNSUPPORTED_FUNCTION) / %d line\r\n"),
                                        __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FSR_BML_UNSUPPORTED_FUNCTION));
        return FSR_BML_UNSUPPORTED_FUNCTION;
    }

    /* Set ECC ON/OFF flag */
    nLLDFlag = nFlag & FSR_BML_FLAG_ECC_MASK;

    if ((nFlag & FSR_BML_FLAG_USE_SPAREBUF) == FSR_BML_FLAG_USE_SPAREBUF)
    {
        nLLDFlag |= FSR_LLD_FLAG_USE_SPAREBUF;
    }

    /************************************************/
    /* Address translation                          */
    /************************************************/
    /* STEP1: translate PgOffset for SLC area */
    nPgOffset = nVpn & pstVol->nMaskSLCPgs;

    /* STEP2: translate PDev */
    nPDev    = nVol << DEVS_PER_VOL_SHIFT;

#if defined(BML_CHK_PARAMETER_VALIDATION)
    /* check the boundary of input parameter */
    if (pstVol->nNumOfPgsInSLCBlk < nPgOffset + nNumOfPgs)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]  Page offset value is bigger than number of page in SLC block \r\n")));
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn: %d, nNumOfPgs: %d, nFlag: 0x%x, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                        __FSR_FUNC__, nVol, nPgOffset, nNumOfPgs, nFlag, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FSR_BML_INVALID_PARAM));
        return FSR_BML_INVALID_PARAM;
    }

    /* check the validity of input parameter */
    if (nNumOfPgs == 0)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]  Number of page is NULL \r\n")));
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs: %d, nFlag: 0x%x, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                        __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FSR_BML_INVALID_PARAM));
        return FSR_BML_INVALID_PARAM;
    }

    /* check the main and spare buffer pointer */
    if ((pMBuf == NULL) && (pSBuf == NULL))
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Main and Spare buffer point is NULL \r\n")));
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, pMBuf:%d, pSBuf: %d, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                        __FSR_FUNC__, nVol, pMBuf, pSBuf, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FSR_BML_INVALID_PARAM));
        return FSR_BML_INVALID_PARAM;
    }
#endif /* BML_CHK_PARAMETER_VALIDATION */

    /* Acquire semaphore */
    bRet = FSR_OAM_AcquireSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
    if (bRet == FALSE32)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs: %d, nRe:FSR_BML_ACQUIRE_SM_ERROR) / %d line\r\n"),
                                        __FSR_FUNC__, nVol, nVpn, nNumOfPgs, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FSR_BML_ACQUIRE_SM_ERROR));
        return FSR_BML_ACQUIRE_SM_ERROR;
    }

    /************************************************************/
    /* <Procedure of FSR_BML_OTPRead()                          */
    /* STEP1. Check and Handle previous error                   */
    /* STEP2. Set OTP Mode                                      */
    /* STEP3. Check a state of OTP Block                        */
    /* STEP4. Write data to OTP block                           */
    /* STEP5. Set NAND Core reset mode                          */
    /************************************************************/
    do
    {
        /************************************************/
        /* STEP1. Check and Handle previous error       */
        /************************************************/

        /* Handle previous error in opposite die*/
        if (pstVol->nNumOfDieInDev == FSR_MAX_DIES)
        {
            /* Handle the previous error using LLD_FlushOp and _HandlePrevError */
            nLLDRe = pstVol->LLD_FlushOp(nPDev,
                                         (nDieIdx + 1) & 1,
                                         FSR_LLD_FLAG_NONE);
            if (nLLDRe != FSR_LLD_SUCCESS)
            {
                nBMLRe = _HandlePrevError(nVol,
                                          nPDev,
                                          (nDieIdx + 1) & 1,
                                          nLLDRe);
                if (nBMLRe != FSR_BML_SUCCESS)
                {
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs: %d, nRe:0x%x)\r\n"),
                                                     __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nBMLRe));
                    break;
                }
            } /* End of "if (nLLDRe != FSR_LLD_SUCCESS)" */
        }

        nLLDRe = pstVol->LLD_FlushOp(nPDev,
                                     nDieIdx,
                                     FSR_LLD_FLAG_NONE);
        if (nLLDRe != FSR_LLD_SUCCESS)
        {
            nBMLRe = _HandlePrevError(nVol,
                                      nPDev,
                                      nDieIdx,
                                      nLLDRe);
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs: %d, nRe:0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nBMLRe));
                break;
            }
        }

        /************************************************/
        /* STEP2. Set OTP Mode by FSR_LLD_IOCTL()       */
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
            /* Return nLLDRe directly */
            nBMLRe = nLLDRe;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs: %d)\r\n"),
                                            __FSR_FUNC__, nVol, nVpn, nNumOfPgs));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_IOCtl(nPDev:%d, nCode: FSR_LLD_IOCTL_OTP_ACCESS, nDieIdx:%d, nRe:0x%x) / %d line\r\n"),
                                            nPDev, nDieIdx, nBMLRe, __LINE__));
            break;
        }

        /************************************************/
        /* STEP3. Check a state of OTP Block            */
        /************************************************/
        /* Get the state of OTP Block */
        nBMLRe = _GetOTPInfo(nVol,
                            &nLockStat);
        if (nBMLRe != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs: %d, nRe:0x%x)\r\n"),
                                            __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nBMLRe));
            break;
        }

        /* If OTP block is locked, it cannot be programed */
        if (nLockStat & FSR_LLD_OTP_OTP_BLK_LOCKED)
        {
            nBMLRe = FSR_BML_WR_PROTECT_ERROR;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nRe:FSR_BML_WR_PROTECT_ERROR, OTP Block is locked)\r\n"),
                                            __FSR_FUNC__, nVol, __LINE__));
            break;
        }

        /************************************************/
        /* STEP4. Write data to OTP block               */
        /************************************************/
        /* STEP4-1. Get real OTP block # */
        nPbn     = 0;

        nNumOfPgs += nPgOffset;
        /* STEP4-2. Write data to OTP block */
        for (;nPgOffset < nNumOfPgs ; nPgOffset++)
        {
            nLLDRe = pstVol->LLD_Write(nPDev,
                                       nPbn,
                                       nPgOffset,
                                       pMBuf,
                                       pSBuf,
                                       FSR_LLD_FLAG_1X_PROGRAM | nLLDFlag);

            /* Only use OTP block in 1st die in device */
            nLLDRe = pstVol->LLD_FlushOp(nPDev,
                                         0,
                                         FSR_LLD_FLAG_NONE);
            if (nLLDRe != FSR_LLD_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs: %d)\r\n"),
                                                __FSR_FUNC__, nVol, nVpn, nNumOfPgs));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_Write(nPDev:%d, nPbn:%d, nPgOffset:%d, nRe: 0x%x) / %d line\r\n"),
                                                nPDev, nPbn, nPgOffset, nLLDRe, __LINE__));
                /* Return nLLDRe directly */
                nBMLRe = nLLDRe;
                break;
            }

            /* Increase the pMBuf pointer when pMBuf is not NULL */
            if (pMBuf != NULL)
            {
                pMBuf += pstVol->nSizeOfPage;
            }

            /* Increase the spare buffer pointer when pSBuf is not NULL */
            if (pSBuf != NULL)
            {
                pSBuf ++;
            }

        }/* End of "for (;nPgOffset <...)" */

        /************************************************/
        /* STEP5. Set NAND Core reset mode              */
        /************************************************/
        nLLDRe = pstVol->LLD_IOCtl(nPDev,
                                   FSR_LLD_IOCTL_HOT_RESET,
                                   (UINT8 *) &nDieIdx,
                                   sizeof(nDieIdx),
                                   NULL,
                                   0,
                                  &nByteRet);
        if (nLLDRe != FSR_LLD_SUCCESS)
        {
            /* Return nLLDRe directly */
            nBMLRe = nLLDRe;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs: %d)\r\n"),
                                            __FSR_FUNC__, nVol, nVpn, nNumOfPgs));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_IOCtl(nPDev:%d, nCode:FSR_LLD_IOCTL_HOT_RESET, nDieIdx:%d, nRe:0x%x) / %d line\r\n"),
                                            nPDev, nDieIdx, nLLDRe, __LINE__));
            break;
        }

    } while(0);

    /* Release semaphore */
    bRet = FSR_OAM_ReleaseSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
    if (bRet == FALSE32)
    {
        nBMLRe = FSR_BML_RELEASE_SM_ERROR;
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs: %d, nRe:FSR_BML_RELEASE_SM_ERROR) / %d line\r\n"),
                                        __FSR_FUNC__, nVol, nVpn, nNumOfPgs, __LINE__));
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nBMLRe));

    return nBMLRe;
}
#endif

#if !defined(FSR_NBL2)

/**
 *  @brief      This function adjusts partition configuration.
 *
 *  @param [in]   nVol        : Volume number
 *  @param [in]  *pstPartIIn  : Pointer to FSRPartI data structure (user input) 
 *  @param [out] *pstPartIOut : Pointer to FSRPartI data structure (adjusted by BML)
 *  @param [out] *pstFlashInfo: Pointer to FSRFlashInfo data structure
 *  @param [in]   nFlag       : FSR_BML_FLAG_NONE
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_INVALID_PARAM
 *  @return     FSR_BML_PAM_ACCESS_ERROR
 *  @return     FSR_BML_DEVICE_ACCESS_ERROR
 *  @return     Some LLD errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
FSR_BML_AdjustPartInfo(UINT32        nVol,
                       FSRPartI     *pstPartIIn,
                       FSRPartI     *pstPartIOut,
                       FSRFlashInfo *pstFlashInfo,
                       UINT32        nFlag)
{
    INT32           nBMLRe = FSR_BML_SUCCESS;  /* BML Return value          */
    INT32           nRe    = FSR_BML_SUCCESS;  /* BML Return value          */
    BOOL32          bRe    = TRUE32;
    BmlVolCxt      *pstVol;
    UINT32         *pnGlockHdl;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, nFlag: 0x%x)\r\n"), __FSR_FUNC__, nVol, nFlag));

    /* Check the volume range */
    CHK_VOL_RANGE(nVol);

    /* Get the pointer of volume context */
    pstVol = _GetVolCxt(nVol);

    /* Check the pointer to volume context */
    CHK_VOL_POINTER(pstVol);

    pnGlockHdl = _GetGLockHdl(nVol);

    /* Acquire semaphore */
    bRe = FSR_PAM_AcquireSL(*pnGlockHdl, FSR_OAM_SM_TYPE_BML);
    if (bRe == FALSE32)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nRe: FSR_BML_ACQUIRE_SM_ERROR) / %d line\r\n"),
                                        __FSR_FUNC__, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, FSR_BML_ACQUIRE_SM_ERROR));
        return FSR_BML_ACQUIRE_SM_ERROR;
    }

    do
    {
        /* In case that FSR_BML_Open is not called yet */
        if (pstVol->bVolOpen != TRUE32)
        {
            /* Open a volume */
            nBMLRe = _Open(nVol);
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:ERR]   %s(nVol: %d, nFlag: 0x%x, nRe:0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nFlag, nBMLRe));
                break;
            }
        }

        /* Check PI validity */
        nBMLRe = _ChkPIValidity(nVol, pstPartIIn);
        if (nBMLRe != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nFlag: 0x%x, nRe:0x%x)\r\n"),
                                            __FSR_FUNC__, nVol, nFlag, nBMLRe));
            break;
        }

        /* Adjust Partition Information */
        nBMLRe = _AdjustPartInfo(nVol, pstPartIIn, pstPartIOut);
        if (nBMLRe != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:ERR]   %s(nVol: %d, nFlag: 0x%x, nRe:0x%x)\r\n"),
                                            __FSR_FUNC__, nVol, nFlag, nBMLRe));
            break;
        }

        /* After this function is called, FSR_BML_Format call is available. */
        gbAdjustPartInfo[nVol] = TRUE32;

    } while (0);

    if (pstVol->bVolOpen != TRUE32)
    {
        /* Close a volume */
        nRe = _Close(nVol);
        if (nRe != FSR_BML_SUCCESS)
        {
            nBMLRe = nRe;
            FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:ERR]   %s(nVol: %d, nFlag: 0x%x, nRe:0x%x)\r\n"),
                                            __FSR_FUNC__, nVol, nFlag, nBMLRe));
        }
    }

    bRe = FSR_PAM_ReleaseSL(*pnGlockHdl, FSR_OAM_SM_TYPE_BML);
    if (bRe == FALSE32)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nRe: FSR_BML_RELEASE_SM_ERROR) / %d line\r\n"),
                                        __FSR_FUNC__, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, FSR_BML_RELEASE_SM_ERROR));
        nBMLRe = FSR_BML_RELEASE_SM_ERROR;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, nBMLRe));

    return nBMLRe;
}
#endif

#if !defined(FSR_NBL2)
/**
 *  @brief      This function flushes the current operation in the given volume.
 *
 *  @param [in]  nVol        : Volume number
 *  @param [in]  nFlag       : FSR_BML_FLAG_NORMAL_MODE
 *  @n                         FSR_BML_FLAG_EMERGENCY_MODE
 *  @n                         FSR_BML_FLAG_NO_SEMAPHORE
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_CRITICAL_ERROR
 *  @return     FSR_BML_INVALID_PARAM
 *  @return     FSR_BML_ACQUIRE_SM_ERROR
 *  @return     FSR_BML_RELEASE_SM_ERROR
 *  @return     FSR_BML_VOLUME_NOT_OPENED
 *  @return     Some LLD errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
FSR_BML_FlushOp(UINT32    nVol,
                UINT32    nFlag)
{
    UINT32      nPDev   = 0;                /* Physical device number   */
    UINT32      nDevIdx = 0;                /* Device Index             */
    UINT32      nDieIdx = 0;                /* Die Index                */
    BOOL32      bRet    = TRUE32;           /* Temporary return value   */
    INT32       nLLDRe  = FSR_LLD_SUCCESS;  /* LLD Return value         */
    INT32       nBMLRe  = FSR_BML_SUCCESS;  /* BML Return value         */
    BmlVolCxt  *pstVol;
    BmlDevCxt  *pstDev;
    BmlDieCxt  *pstDie;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d,nFlag: 0x%x)\r\n"), __FSR_FUNC__, nVol, nFlag));

    /* check the volume range */
    CHK_VOL_RANGE(nVol);

    /* Get the pointer of volume context */
    pstVol = _GetVolCxt(nVol);

    /* Check the pointer to volume context */
    CHK_VOL_POINTER(pstVol);

    /* Check whether this volume is opened */
    CHK_VOL_OPEN(pstVol->bVolOpen);

    /* 
     * FSR_BML_FlushOp can be used in BML_Write, BML_Erase and BML_CopyBack.
     * In this case, FSR_BML_FlushOp should not release or acquire the semaphore
     * because other BML APIs use the semaphore.
     */
    if ((nFlag & FSR_BML_FLAG_NO_SEMAPHORE) != FSR_BML_FLAG_NO_SEMAPHORE)
    {
        /* Acquire semaphore */
        bRet = FSR_OAM_AcquireSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
        if (bRet == FALSE32)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:FSR_BML_ACQUIRE_SM_ERROR) / %d line\r\n"),
                                            __FSR_FUNC__, nVol, __LINE__));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, FSR_BML_ACQUIRE_SM_ERROR));
            return FSR_BML_ACQUIRE_SM_ERROR;
        }
    }

    for (nDevIdx = 0; nDevIdx < DEVS_PER_VOL; nDevIdx++)
    {
        nPDev = nVol * DEVS_PER_VOL + nDevIdx;

        bRet = _IsOpenedDev(nPDev);
        if (bRet == FALSE32)
        {
            continue;
        }

        /* Get the pointer of device context */
        pstDev = _GetDevCxt(nPDev);

        for (nDieIdx = 0; nDieIdx <pstVol->nNumOfDieInDev; nDieIdx++)
        {
            nLLDRe = pstVol->LLD_FlushOp(nPDev,
                                         nDieIdx,
                                         FSR_LLD_FLAG_NONE);
            if ((nFlag & FSR_BML_FLAG_NORMAL_MODE) == FSR_BML_FLAG_NORMAL_MODE)
            {
                if (nLLDRe != FSR_LLD_SUCCESS)
                {
                    nBMLRe = _HandlePrevError(nVol,
                                              nPDev,
                                              nDieIdx,
                                              nLLDRe);
                    if (nBMLRe!= FSR_BML_SUCCESS)
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:0x%x)\r\n"),
                                                        __FSR_FUNC__, nVol, nBMLRe));
                        break;
                    }
                }
                pstDie = pstDev->pstDie[nDieIdx];

                pstDie->pstPreOp->nOpType = BML_PRELOG_NOP;
            } /* End of "if ((nFlag &...)"*/
        } /* End of "for (nDieIdx = 0;...)" */

        /* handle an error of _HandlePrevError for FSR_BML_FLAG_NORMAL_MODE */
        if (nBMLRe != FSR_BML_SUCCESS)
        {
            break;
        }
    }/* End of "for (nDevIdx = 0;...)" */

    /* 
     * FSR_BML_FlushOp can be used in BML_Write, BML_Erase and BML_CopyBack.
     * In this case, FSR_BML_FlushOp should not release or acquire the semaphore
     * because other BML APIs use the semaphore.
     */
    if ((nFlag & FSR_BML_FLAG_NO_SEMAPHORE) != FSR_BML_FLAG_NO_SEMAPHORE)
    {
        /* Release semaphore */
        bRet = FSR_OAM_ReleaseSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
        if (bRet == FALSE32)
        {
            nBMLRe = FSR_BML_RELEASE_SM_ERROR;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nRe:FSR_BML_RELEASE_SM_ERROR) / %d line\r\n"),
                                            __FSR_FUNC__, nVol, __LINE__));
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, nBMLRe));

    return nBMLRe;
}
#endif /* FSR_NBL2*/

#if !defined(FSR_NBL2)
/**
 *  @brief      This function resumes the LLD operation
 *
 *  @param [in]  nVol            : Volume number
 *
 *  @return     FSR_BML_SUCCESS
 *
 *  @author     SongHo Yoon
 *  @version    1.0.0
 *
 *  @remark     this function is used to support non-blocking I/O mode
 *              Non-blocking Manager calls FSR_LLD_XXX and FSR_OAM_ReceiveEvent() and 
 *              the current task sleeps Int pin of OneNAND makes system interrupts and 
 *              then, the ISR calls FSR_BML_ResumeOp. FSR_BML_ResumeOp() calls 
 *              FSR_OAM_SendEvent() to resume the suspened BML operation.
 *
 */
PUBLIC INT32
FSR_BML_ResumeOp(UINT32    nVol)
{
    INT32           nBMLRe = FSR_BML_SUCCESS;
    BmlVolCxt      *pstVol;

    /* Get the pointer of volume context */
    pstVol = _GetVolCxt(nVol);

    FSR_OAM_SendEvent(pstVol->nNBEvent);

    return nBMLRe;
}
#endif /* FSR_NBL2*/

#if !defined(FSR_NBL2)
#if defined(FSR_BML_RWEXT)
/**
 *  @brief      This function reads virtual pages.
 *
 *  @param [in]  nVol        : Volume number
 *  @param [in]  nVpn        : First virtual page number for reading
 *  @param [in]  nNumOfPgs   : The number of pages to read
 *  @param [in] *pMBuf       : Main page buffer for reading
 *  @param [in] *pSBuf       : Spare page buffer for reading
 *  @param [in]  nFlag       : None
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_READ_ERROR
 *  @return     FSR_BML_CRITICAL_ERROR
 *  @return     FSR_BML_INVALID_PARAM
 *  @return     FSR_BML_ACQUIRE_SM_ERROR
 *  @return     FSR_BML_RELEASE_SM_ERROR
 *  @return     FSR_BML_VOLUME_NOT_OPENED
 *  @return     Some LLD errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
FSR_BML_ReadExt(UINT32        nVol,
                UINT32        nVpn,
                UINT32        nNumOfPgs,
                UINT8        *pMBuf,
                UINT8        *pSBuf,
                UINT32        nFlag)
{
    UINT32        nVun              = 0;    /* Virtual Unit number                                  */
    UINT32        nBaseSbn          = 0;    /* Base Sbn                                             */
    UINT32        nPDev             = 0;    /* Physical Device Number                               */
    UINT32        nDevIdx           = 0;    /* Device Index in volume                               */
    UINT32        nPgOffset         = 0;    /* Physical Page Offset (SLC Blk:0~63, MLC Blk: 0~127)  */
    UINT32        nPlnIdx           = 0;    /* Plane Index                                          */
    UINT32        nDieIdx           = 0;    /* Die Index                                            */
    UINT32        nNumOfLoop        = 0;    /* # of times in proportion to vpn                      */
    UINT32        nNumOfBlksInRsvr  = 0;    /* # of reservec blks for Flex-OneNAND                  */
    UINT32        nIdx              = 0;    /* Temporary index                                      */
    UINT32        nFirstAccess      = 0;    /* The number of LLDOp for FlushOp                      */
    BOOL32        bRe               = TRUE32;   /* Temporary return value                           */
    BOOL32        bReadErr          = FALSE32;  /* Flag for indicating uncorrectable read error     */
    INT32         nMajorErr;                /* Major Return Error                                   */
    INT32         nBMLRe = FSR_BML_SUCCESS; /* BML Return value                                     */
    INT32         nLLDRe = FSR_LLD_SUCCESS; /* LLD Return value                                     */
    BmlVolCxt    *pstVol;
    BmlDevCxt    *pstDev;
    BmlDieCxt    *pstDie;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, nVpn: %d, nNumOfPgs: %d, nFlag: 0x%x)\r\n")
                                    , __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag));

    /* check volume range */
    CHK_VOL_RANGE(nVol);

    /* Get pointer to Volume context */
    pstVol = _GetVolCxt(nVol);

    /* Check the pointer to volume context */
    CHK_VOL_POINTER(pstVol);

    /* check volume open */
    CHK_VOL_OPEN(pstVol->bVolOpen);

    FSR_ASSERT(nVol < FSR_MAX_VOLS);

#if defined(BML_CHK_PARAMETER_VALIDATION)
    /* check the boundaries of input parameter*/
    if ((nVpn + nNumOfPgs - 1) > pstVol->nLastVpn)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Number of page from Vpn is bigger than last Vpn \r\n")));
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVpn:%d, nNumOfPgs:%d, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                        __FSR_FUNC__, nVpn, nNumOfPgs, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, FSR_BML_INVALID_PARAM));
        return FSR_BML_INVALID_PARAM;
    }

    /* check the boundary of input parameter */
    if (nNumOfPgs == 0)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Number of page is NULL \r\n")));
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nNumOfPgs:%d, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                        __FSR_FUNC__, nNumOfPgs, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, FSR_BML_INVALID_PARAM));
        return FSR_BML_INVALID_PARAM;
    }

    /* check the main and spare buffer pointer */
    if ((pMBuf == NULL) || (pSBuf == NULL))
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Main and Spare buffer point is NULL \r\n")));
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(pMBuf:0x%x, pSBuf:0x%x, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                        __FSR_FUNC__, pMBuf, pSBuf, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, FSR_BML_INVALID_PARAM));
        return FSR_BML_INVALID_PARAM;
    }
#endif /* BML_CHK_PARAMETER_VALIDATION */

    /* Acquire semaphore */
    bRe = FSR_OAM_AcquireSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
    if (bRe == FALSE32)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nRe: FSR_BML_ACQUIRE_SM_ERROR) / %d line\r\n"),
                                        __FSR_FUNC__, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, FSR_BML_ACQUIRE_SM_ERROR));
        return FSR_BML_ACQUIRE_SM_ERROR;
    }

    /****************************************************/
    /* At the first access, LLD_FlushOp should be called*/
    /* for removing errors of current die               */
    /****************************************************/
    /* Set # of FlushOp operation */
    if (nNumOfPgs < pstVol->nNumOfWays)
    {
        nFirstAccess = nNumOfPgs;
    }
    else
    {
        nFirstAccess = pstVol->nNumOfWays;
    }

    /* Initialize # of loops */
    nNumOfLoop = 0;
    while (nNumOfPgs > 0)
    {
        /********************************************************/
        /* Address translation                                  */
        /********************************************************/
        /* STEP1: translate PgOffset */
        /* page offset for SLC area  */
        nPgOffset = (nVpn >> (pstVol->nSftNumOfWays)) & pstVol->nMaskSLCPgs;

        if (nVpn >= pstVol->n1stVpnOfMLC)
        {
            /* page offset for MLC area */
            nPgOffset = ((nVpn - pstVol->n1stVpnOfMLC) >> (pstVol->nSftNumOfWays)) & pstVol->nMaskMLCPgs;
        }

        /* STEP2: translate PDev */
        nDevIdx  = (nVpn >> pstVol->nDDPMask) & pstVol->nMaskPDev;
        nPDev    = (nVol << DEVS_PER_VOL_SHIFT) + nDevIdx;
        pstDev   = _GetDevCxt(nPDev);

        /* STEP3: translate nDieIdx */
        nDieIdx  = nVpn & pstVol->nDDPMask;
        pstDie   = pstDev->pstDie[nDieIdx];

        /* STEP4: translate Vun */
        if ((nPgOffset == 0) || (nNumOfLoop == 0))
        {
            /* Vun for SLC area */
            nVun                = nVpn >> (pstVol->nSftSLC + pstVol->nSftNumOfWays);
            nNumOfBlksInRsvr    = 0;
            if (nVpn >= pstVol->n1stVpnOfMLC)
            {
                /* Vun for MLC area */
                nVun        = (pstVol->nNumOfSLCUnit) +
                              ((nVpn - (pstVol->nNumOfSLCUnit << (pstVol->nSftSLC + pstVol->nSftNumOfWays))) >>
                              (pstVol->nSftMLC + pstVol->nSftNumOfWays));
                
                if (pstDie->pstRsv->nRsvrType == BML_HYBRID_RESERVOIR)
                {
                    nNumOfBlksInRsvr = pstDie->pstRsv->nLastSbnOfRsvr - pstDie->pstRsv->n1stSbnOfRsvr + 1;
                }
            }
        }

        /* Check and handle the previous operation error */
        if (nNumOfLoop < nFirstAccess)
        {
            nIdx = pstVol->nNumOfDieInDev - 1;
            do
            {
                nLLDRe = pstVol->LLD_FlushOp(nPDev,
                                             nIdx,  /* nDieIdx */
                                             FSR_LLD_FLAG_NONE);
                if (nLLDRe != FSR_LLD_SUCCESS)
                {
                    nBMLRe = _HandlePrevError(nVol,
                                              nPDev,
                                              nIdx, /* nDieIdx */
                                              nLLDRe);
                    if (nBMLRe != FSR_BML_SUCCESS)
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn: %d, nNumOfPgs: %d, nFlag: 0x%x, nRe: 0x%x)\r\n"), 
                                                        __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag, nBMLRe));
                        break;
                    }
                }
            } while (nIdx-- > 0);

            /* Get the return value by partitionfor current partition */
            nBMLRe = _GetPIRet(pstVol,
                               pstDie,
                               nVun,
                               nBMLRe);
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                /* message out */
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs:%d, nFlag:0x%x, nRe:0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag, nBMLRe));
                break;
            }

        } /* End of "if (nNumOfLoop < nFirstAccess)" */

        /* STEP5: translate Sbn[] to Pbn[] */
        if ((nPgOffset == 0) || (nNumOfLoop < nFirstAccess))
        {
            nPlnIdx  = pstVol->nNumOfPlane - 1;
            nBaseSbn = ((nDieIdx) << pstVol->nSftNumOfBlksInDie)+
                        (nVun << pstVol->nSftNumOfPln)          +
                        nNumOfBlksInRsvr;
            do
            {
                pstDie->nCurSbn[nPlnIdx] = (UINT16)nBaseSbn + (UINT16)nPlnIdx;
                pstDie->nCurPbn[nPlnIdx] = (UINT16)nBaseSbn + (UINT16)nPlnIdx;
            } while (nPlnIdx-- > 0);

            /* Second Translation: translate Pbn[] */
            pstDie->nNumOfLLDOp = 0;
            _GetPBN(pstDie->nCurSbn[0], pstVol, pstDie);
        }

        nPlnIdx = 0;
        do
        {
            /* Call LLD_Read() for "load" operation */
            nLLDRe = pstVol->LLD_Read(nPDev,
                                      pstDie->nCurPbn[nPlnIdx],
                                      nPgOffset,
                                      pMBuf,
                                      (FSRSpareBuf *) pSBuf,
                                      FSR_LLD_FLAG_DUMP_ON      |
                                      FSR_LLD_FLAG_USE_SPAREBUF |
                                      FSR_LLD_FLAG_ECC_ON       |
                                      FSR_LLD_FLAG_1X_LOAD);

            /* Call LLD_Read() for "transfer" operation */
            nLLDRe = pstVol->LLD_Read(nPDev,
                                      pstDie->nCurPbn[nPlnIdx],
                                      nPgOffset,
                                      pMBuf,
                                      (FSRSpareBuf *) pSBuf,
                                      FSR_LLD_FLAG_DUMP_ON      |
                                      FSR_LLD_FLAG_USE_SPAREBUF |
                                      FSR_LLD_FLAG_ECC_ON       |
                                      FSR_LLD_FLAG_TRANSFER);

            /* Handle the write error */
            nMajorErr = FSR_RETURN_MAJOR(nLLDRe);

            switch (nMajorErr)
            {
            case FSR_LLD_SUCCESS:
            case FSR_LLD_PREV_READ_DISTURBANCE:
                break;

            case FSR_LLD_PREV_READ_ERROR:
                bReadErr = TRUE32;
                /* message out */
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_Read(nPDev: %d, nPbn: %d, nPgOffset: %d, nRe:FSR_LLD_PREV_READ_ERROR), / %d line\r\n"), 
                                                nPDev, pstDie->nCurPbn[nPlnIdx], nPgOffset, __LINE__));
                break;

            case FSR_LLD_INVALID_PARAM:     /* This code return LLD error to a upper layer*/
                nBMLRe = nLLDRe;
                /* message out */
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_Read(nPDev: %d, nPbn: %d, nPgOffset: %d, nRe: FSR_LLD_INVALID_PARAM), / %d line\r\n"),
                                                nPDev, pstDie->nCurPbn[nPlnIdx], nPgOffset, __LINE__));
                break;

            default:
                /* if code reaches this line, abnormal case.. */
                nBMLRe = nLLDRe;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_Read(nPDev: %d, nPbn: %d, nPgOffset: %d, nRe: 0x%x), / %d line\r\n"), 
                                                nPDev, pstDie->nCurPbn[nPlnIdx], nPgOffset, nBMLRe, __LINE__));
                break;
            } /* End of switch(nMajorErr)*/

            /* case) FSR_LLD_INVALID_PARAM or abnornal case */
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                break;
            }

            /* Increase pMBuf, pSBuf */
            pMBuf += pstVol->nSizeOfPage;
            pSBuf += pstVol->nSparePerSct * pstVol->nSctsPerPg;

        } while(++nPlnIdx < pstVol->nNumOfPlane);

        /* case) FSR_LLD_INVALID_PARAM or abnornal case */
        if (nBMLRe != FSR_BML_SUCCESS)
        {
            break;
        }

        /* Change the nNumOfPgs, nNumOfLoop, nVpn*/
        nNumOfPgs --;
        nNumOfLoop++;
        nVpn      ++;

    } /* End of "while (nNumOfPgs > 0)" */

    /* if the uncorrectable read error exists,
     * FSR_BML_READ_ERROR should be returned.
     */
    if (bReadErr == TRUE32)
    {
        nBMLRe = FSR_BML_READ_ERROR;
    }

    /* Release semaphore */
    bRe = FSR_OAM_ReleaseSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
    if (bRe == FALSE32)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nRe: FSR_BML_RELEASE_SM_ERROR) / %d line\r\n"),
                                        __FSR_FUNC__, __LINE__));
        nBMLRe = FSR_BML_RELEASE_SM_ERROR;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, nBMLRe));

    return nBMLRe;
}

/**
 *  @brief      This function writes virtual pages.
 *
 *  @param [in]  nVol        : Volume number
 *  @param [in]  nVpn        : First virtual page number for reading
 *  @param [in]  nNumOfPgs   : The number of pages to write
 *  @param [in] *pMBuf       : Main page buffer for writing
 *  @param [in] *pSBuf       : Spare page buffer for writing
 *  @param [in]  nFlag       : None
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_CRITICAL_ERROR
 *  @return     FSR_BML_INVALID_PARAM
 *  @return     FSR_BML_ACQUIRE_SM_ERROR
 *  @return     FSR_BML_RELEASE_SM_ERROR
 *  @return     FSR_BML_VOLUME_NOT_OPENED
 *  @return     Some LLD errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
FSR_BML_WriteExt(UINT32        nVol,
                 UINT32        nVpn,
                 UINT32        nNumOfPgs,
                 UINT8        *pMBuf,
                 UINT8        *pSBuf,
                 UINT32        nFlag)
{
    UINT32        nVun              = 0;    /* Virtual Unit number                                  */
    UINT32        nBaseSbn          = 0;    /* Base Sbn                                             */
    UINT32        nPDev             = 0;    /* Physical Device Number                               */
    UINT32        nDevIdx           = 0;    /* Device Index in volume                               */
    UINT32        nPgOffset         = 0;    /* Physical Page Offset (SLC Blk:0~63, MLC Blk: 0~127)  */
    UINT32        nPlnIdx           = 0;    /* Plane Index                                          */
    UINT32        nDieIdx           = 0;    /* Die Index                                            */
    UINT32        nNumOfLoop        = 0;    /* # of times in proportion to vpn                      */
    UINT32        nNumOfBlksInRsvr  = 0;    /* # of reservec blks for Flex-OneNAND                  */
    UINT32        nIdx              = 0;    /* Temporary index                                      */
    UINT32        nFirstAccess      = 0;    /* The number of LLDOp for FlushOp                      */
    BOOL32        bRe               = TRUE32;   /* Temporary return value                           */
    INT32         nMajorErr;                /* Major Return Error                                   */
    INT32         nBMLRe = FSR_BML_SUCCESS; /* BML Return value                                     */
    INT32         nLLDRe = FSR_LLD_SUCCESS; /* LLD Return value                                     */
    BmlVolCxt    *pstVol;
    BmlDevCxt    *pstDev;
    BmlDieCxt    *pstDie;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, nVpn: %d, nNumOfPgs: %d, nFlag: 0x%x)\r\n")
                                    , __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag));

    /* check volume range */
    CHK_VOL_RANGE(nVol);

    /* Get pointer to Volume context */
    pstVol = _GetVolCxt(nVol);

    /* Check the pointer to volume context */
    CHK_VOL_POINTER(pstVol);

    /* check volume open */
    CHK_VOL_OPEN(pstVol->bVolOpen);

    FSR_ASSERT(nVol < FSR_MAX_VOLS);

#if defined(BML_CHK_PARAMETER_VALIDATION)
    /* check the boundaries of input parameter*/
    if ((nVpn + nNumOfPgs - 1) > pstVol->nLastVpn)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Number of page from Vpn is bigger than last Vpn \r\n")));
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVpn:%d, nNumOfPgs:%d, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                        __FSR_FUNC__, nVpn, nNumOfPgs, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, FSR_BML_INVALID_PARAM));
        return FSR_BML_INVALID_PARAM;
    }

    /* check the boundary of input parameter */
    if (nNumOfPgs == 0)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Number of page is NULL \r\n")));
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nNumOfPgs:%d, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                        __FSR_FUNC__, nNumOfPgs, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, FSR_BML_INVALID_PARAM));
        return FSR_BML_INVALID_PARAM;
    }

    /* check the main and spare buffer pointer */
    if ((pMBuf == NULL) || (pSBuf == NULL))
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Main and Spare buffer point is NULL \r\n")));
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(pMBuf:0x%x, pSBuf:0x%x, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                        __FSR_FUNC__, pMBuf, pSBuf, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, FSR_BML_INVALID_PARAM));
        return FSR_BML_INVALID_PARAM;
    }
#endif /* BML_CHK_PARAMETER_VALIDATION */

    /* Acquire semaphore */
    bRe = FSR_OAM_AcquireSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
    if (bRe == FALSE32)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nRe: FSR_BML_ACQUIRE_SM_ERROR) / %d line\r\n"),
                                        __FSR_FUNC__, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, FSR_BML_ACQUIRE_SM_ERROR));
        return FSR_BML_ACQUIRE_SM_ERROR;
    }

    /****************************************************/
    /* At the first access, LLD_FlushOp should be called*/
    /* for removing errors of current die               */
    /****************************************************/
    /* Set # of FlushOp operation */
    if (nNumOfPgs < pstVol->nNumOfWays)
    {
        nFirstAccess = nNumOfPgs;
    }
    else
    {
        nFirstAccess = pstVol->nNumOfWays;
    }

    /* Initialize # of loops */
    nNumOfLoop = 0;
    while (nNumOfPgs > 0)
    {
        /********************************************************/
        /* Address translation                                  */
        /********************************************************/
        /* STEP1: translate PgOffset */
        /* page offset for SLC area  */
        nPgOffset = (nVpn >> (pstVol->nSftNumOfWays)) & pstVol->nMaskSLCPgs;

        if (nVpn >= pstVol->n1stVpnOfMLC)
        {
            /* page offset for MLC area */
            nPgOffset = ((nVpn - pstVol->n1stVpnOfMLC) >> (pstVol->nSftNumOfWays)) & pstVol->nMaskMLCPgs;
        }

        /* STEP2: translate PDev */
        nDevIdx  = (nVpn >> pstVol->nDDPMask) & pstVol->nMaskPDev;
        nPDev    = (nVol << DEVS_PER_VOL_SHIFT) + nDevIdx;
        pstDev   = _GetDevCxt(nPDev);

        /* STEP3: translate nDieIdx */
        nDieIdx  = nVpn & pstVol->nDDPMask;
        pstDie   = pstDev->pstDie[nDieIdx];

        /* STEP4: translate Vun */
        if ((nPgOffset == 0) || (nNumOfLoop == 0))
        {
            /* Vun for SLC area */
            nVun                = nVpn >> (pstVol->nSftSLC + pstVol->nSftNumOfWays);
            nNumOfBlksInRsvr    = 0;
            if (nVpn >= pstVol->n1stVpnOfMLC)
            {
                /* Vun for MLC area */
                nVun        = (pstVol->nNumOfSLCUnit) +
                              ((nVpn - (pstVol->nNumOfSLCUnit << (pstVol->nSftSLC + pstVol->nSftNumOfWays))) >>
                              (pstVol->nSftMLC + pstVol->nSftNumOfWays));
                
                if (pstDie->pstRsv->nRsvrType == BML_HYBRID_RESERVOIR)
                {
                    nNumOfBlksInRsvr = pstDie->pstRsv->nLastSbnOfRsvr - pstDie->pstRsv->n1stSbnOfRsvr + 1;
                }
            }
        }

        /* Check and handle the previous operation error */
        if (nNumOfLoop < nFirstAccess)
        {
            nIdx = pstVol->nNumOfDieInDev - 1;
            do
            {
                nLLDRe = pstVol->LLD_FlushOp(nPDev,
                                             nIdx,  /* nDieIdx */
                                             FSR_LLD_FLAG_NONE);
                if (nLLDRe != FSR_LLD_SUCCESS)
                {
                    nBMLRe = _HandlePrevError(nVol,
                                              nPDev,
                                              nIdx, /* nDieIdx */
                                              nLLDRe);
                    if (nBMLRe != FSR_BML_SUCCESS)
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn: %d, nNumOfPgs: %d, nFlag: 0x%x, nRe: 0x%x)\r\n"), 
                                                        __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag, nBMLRe));
                        break;
                    }
                }
            } while (nIdx-- > 0);

            /* Get the return value by partitionfor current partition */
            nBMLRe = _GetPIRet(pstVol,
                               pstDie,
                               nVun,
                               nBMLRe);
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                /* message out */
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nVpn:%d, nNumOfPgs:%d, nFlag:0x%x, nRe:0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag, nBMLRe));
                break;
            }

        } /* End of "if (nNumOfLoop < nFirstAccess)" */

        /* STEP5: translate Sbn[] to Pbn[] */
        if ((nPgOffset == 0) || (nNumOfLoop < nFirstAccess))
        {
            nPlnIdx  = pstVol->nNumOfPlane - 1;
            nBaseSbn = ((nDieIdx) << pstVol->nSftNumOfBlksInDie)+
                        (nVun << pstVol->nSftNumOfPln)          +
                        nNumOfBlksInRsvr;
            do
            {
                pstDie->nCurSbn[nPlnIdx] = (UINT16)nBaseSbn + (UINT16)nPlnIdx;
                pstDie->nCurPbn[nPlnIdx] = (UINT16)nBaseSbn + (UINT16)nPlnIdx;
            } while (nPlnIdx-- > 0);

            /* Second Translation: translate Pbn[] */
            pstDie->nNumOfLLDOp = 0;
            _GetPBN(pstDie->nCurSbn[0], pstVol, pstDie);
        }

        nPlnIdx = 0;
        do
        {
            /* Call LLD_Write() for "1x-program" operation */
            nLLDRe = pstVol->LLD_Write(nPDev,
                                       pstDie->nCurPbn[nPlnIdx],
                                       nPgOffset,
                                       pMBuf,
                                       (FSRSpareBuf *) pSBuf,
                                       FSR_LLD_FLAG_DUMP_ON      |
                                       FSR_LLD_FLAG_USE_SPAREBUF |
                                       FSR_LLD_FLAG_ECC_ON       |
                                       FSR_LLD_FLAG_1X_PROGRAM);

            /* Call LLD_FlushOp() to get the return value */
            nLLDRe = pstVol->LLD_FlushOp(nPDev,
                                         nDieIdx,
                                         FSR_LLD_FLAG_NONE);

            /* Handle the write error */
            nMajorErr = FSR_RETURN_MAJOR(nLLDRe);

            switch (nMajorErr)
            {
            case FSR_LLD_SUCCESS:
                break;

            case FSR_LLD_PREV_WRITE_ERROR:
                nBMLRe = nLLDRe;
                /* message out */
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_Write(nPDev: %d, nPbn: %d, nPgOffset: %d, nRe:FSR_LLD_PREV_WRITE_ERROR), / %d line\r\n"), 
                                                nPDev, pstDie->nCurPbn[nPlnIdx], nPgOffset, __LINE__));
                break;

            case FSR_LLD_WR_PROTECT_ERROR:
                nBMLRe = nLLDRe;
                /* message out */
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_Write(nPDev: %d, nPbn: %d, nPgOffset: %d, nRe:FSR_LLD_WR_PROTECT_ERROR), / %d line\r\n"), 
                                                nPDev, pstDie->nCurPbn[nPlnIdx], nPgOffset, __LINE__));
                break;

            case FSR_LLD_INVALID_PARAM:     /* This code return LLD error to a upper layer*/
                nBMLRe = nLLDRe;
                /* message out */
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_Write(nPDev: %d, nPbn: %d, nPgOffset: %d, nRe: FSR_LLD_INVALID_PARAM), / %d line\r\n"),
                                                nPDev, pstDie->nCurPbn[nPlnIdx], nPgOffset, __LINE__));
                break;

            default:
                /* if code reaches this line, abnormal case.. */
                nBMLRe = nLLDRe;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   LLD_Write(nPDev: %d, nPbn: %d, nPgOffset: %d, nRe: 0x%x), / %d line\r\n"), 
                                                nPDev, pstDie->nCurPbn[nPlnIdx], nPgOffset, nBMLRe, __LINE__));
                break;
            } /* End of switch(nMajorErr)*/

            /* case) FSR_LLD_INVALID_PARAM or abnornal case */
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                break;
            }

            /* Increase pMBuf, pSBuf */
            pMBuf += pstVol->nSizeOfPage;
            pSBuf += pstVol->nSparePerSct * pstVol->nSctsPerPg;

        } while(++nPlnIdx < pstVol->nNumOfPlane);

        /* case) FSR_LLD_INVALID_PARAM or abnornal case */
        if (nBMLRe != FSR_BML_SUCCESS)
        {
            break;
        }

        /* Change the nNumOfPgs, nNumOfLoop, nVpn*/
        nNumOfPgs --;
        nNumOfLoop++;
        nVpn      ++;

    } /* End of "while (nNumOfPgs > 0)" */

    /* Release semaphore */
    bRe = FSR_OAM_ReleaseSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
    if (bRe == FALSE32)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nRe: FSR_BML_RELEASE_SM_ERROR) / %d line\r\n"),
                                        __FSR_FUNC__, __LINE__));
        nBMLRe = FSR_BML_RELEASE_SM_ERROR;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, nBMLRe));

    return nBMLRe;
}
#endif /* #if defined(FSR_BML_RWEXT) */
#endif /* #if !defined(FSR_NBL2) */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function provides a generic I/O control code (IOCTL) for getting
 *  @n          or storing information.
 *
 *  @param [in]  nVol            : Volume number
 *  @param [in]  nCode           : IO control code, which can be pre-defined
 *  @param [in] *pBufIn          : Pointer to the input buffer that can contain additional information
 *  @n                             associated with the nCode parameter
 *  @param [in]  nLenIn          : Size in bytes of pBufIn
 *  @param [out] *pBufOut        : Pointer to the output buffer supplied by the caller
 *  @param [in]  nLenOut         : Specifies the maximum number of bytes that can be returned in lpOutBuf.
 *  @param [out] *pBytesReturned : Number of bytes returned in lpOutBuf
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_INVALID_PARAM
 *  @return     FSR_BML_ACQUIRE_SM_ERROR
 *  @return     FSR_BML_RELEASE_SM_ERROR
 *  @return     FSR_BML_VOLUME_NOT_OPENED
 *  @return     Some LLD errors
 *  @return     Some BML errors
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
FSR_BML_IOCtl(UINT32        nVol,
              UINT32        nCode,
              UINT8        *pBufIn,
              UINT32        nLenIn,
              UINT8        *pBufOut,
              UINT32        nLenOut,
              UINT32       *pBytesReturned)
{
    UINT32          nIdx    = 0;    /* Temporary index  */
    INT32           nBMLRe  = FSR_BML_SUCCESS;
    INT32           nOAMRe  = FSR_OAM_SUCCESS;
    INT32           nPAMRe  = FSR_PAM_SUCCESS;
    BOOL32          bRet    = TRUE32;
    BmlVolCxt      *pstVol;
    FSRLowFuncTbl  *pstLFT;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, nCode: 0x%x)\r\n"), __FSR_FUNC__, nVol, nCode));

    /* Check the volume range */
    CHK_VOL_RANGE(nVol);

    /* Get the pointer of volume context */
    pstVol = _GetVolCxt(nVol);

    /* Check the pointer to volume context */
    CHK_VOL_POINTER(pstVol);

    /* Check whether this volume is opened */
    CHK_VOL_OPEN(pstVol->bVolOpen);

    /* Acquire semaphore */
    bRet = FSR_OAM_AcquireSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
    if (bRet == FALSE32)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: 0x%x, nRe:FSR_BML_ACQUIRE_SM_ERROR) / %d line\r\n"),
                                        __FSR_FUNC__, nVol, nCode, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, FSR_BML_ACQUIRE_SM_ERROR));
        return FSR_BML_ACQUIRE_SM_ERROR;
    }

    /* Flush Op */
    nBMLRe = FSR_BML_FlushOp(nVol, FSR_BML_FLAG_NO_SEMAPHORE);
    if (nBMLRe != FSR_BML_SUCCESS)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:   ]   %s(nVol: %d, nRe:0x%x)/ %d line\r\n"),
                                        __FSR_FUNC__, nVol, nBMLRe,  __LINE__));
        return nBMLRe;
    }

    switch(nCode)
    {
        case FSR_BML_IOCTL_LOCK_FOREVER:
        {
            UINT32  nPartIdx        = 0;            /* Partition index                  */
            UINT32  nNumOfPartEntry = 0;            /* # of partition entries           */
            UINT32  nAttr           = 0x00000000;   /* Attributes of partition          */
            UINT32  n1stVun         = 0;            /* Virtual unit number              */
            UINT32  nNumOfUnits     = 0;            /* # of units                       */
            BOOL32  bLockTight      = FALSE32;      /* flag to inform that
                                                       LLD func.called for lock-tight   */

            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]  nCode = FSR_BML_IOCTL_LOCK_FOREVER\r\n")));

            /* Get # of partition entries */
            nNumOfPartEntry = pstVol->pstPartI->nNumOfPartEntry;

            for (nPartIdx = 0; nPartIdx < nNumOfPartEntry; nPartIdx++)
            {
                nAttr = pstVol->pstPartI->stPEntry[nPartIdx].nAttr;
                if ((nAttr & FSR_BML_PI_ATTR_LOCKTIGHTEN) == FSR_BML_PI_ATTR_LOCKTIGHTEN)
                {
                    /* Set bLockTight */
                    bLockTight = TRUE32;

                    /* Set nVun and nNumOfUnits by partition entry */
                    n1stVun             = pstVol->pstPartI->stPEntry[nPartIdx].n1stVun;
                    nNumOfUnits         = pstVol->pstPartI->stPEntry[nPartIdx].nNumOfUnits;

                    nBMLRe = _LockTightData(nVol,
                                            n1stVun,
                                            nNumOfUnits);
                    /* error case */
                    if (nBMLRe != FSR_BML_SUCCESS)
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode:FSR_BML_IOCTL_LOCK_FOREVER, nRe:0x%x)\r\n"),
                                                        __FSR_FUNC__, nVol, nBMLRe));
                        break;
                    }
                }   /* End of "if ((nAttr & FSR_BML_PI_ATTR_LOCKTIGHTEN)==...) */
            }   /* End of "for (nPartIdx = 0;...)"*/

            if ((bLockTight == TRUE32) && (nBMLRe == FSR_BML_SUCCESS))
            {
                nBMLRe = _LockTightRsv(nVol);
            } /* End of "if (bLockTight == TRUE32)"*/

            /* Set pBytesReturned */
            if (pBytesReturned != NULL)
            {
                *pBytesReturned = 0;
            }
            
        }
        break;

    case FSR_BML_IOCTL_UNLOCK_WHOLEAREA:
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]  nCode = FSR_BML_IOCTL_UNLOCK_WHOLEAREA\r\n")));

                    /* Call _UnlockAllBlk to unlock all block in a die */
            nBMLRe = _UnlockUserBlk(nVol, pstVol);
                    if (nBMLRe != FSR_BML_SUCCESS)
                    {
                        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_UNLOCK_WHOLEAREA, nRe:0x%x)\r\n"),
                                                        __FSR_FUNC__, nVol, nBMLRe));
                        break;
                    }

            /* Message out */
            if (nBMLRe == FSR_BML_SUCCESS)
            {
                pstVol->bPreProgram = TRUE32;   /* Set PreProgram for */
                FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]  Unlocking whole area is completed.\r\n")));
            }
            else
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]  Unlocking whole area is failed.\r\n")));
            }

            /* Set pBytesReturned */
            if (pBytesReturned != NULL)
            {
                *pBytesReturned = 0;
            }
        }
        break;

    case FSR_BML_IOCTL_CHANGE_PART_ATTR:
        {
            FSRChangePA    *pstPA;

            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]  nCode = FSR_BML_IOCTL_CHANGE_PART_ATTR\r\n")));

            /* Check whether pBufIn is null or not */
            if (pBufIn == NULL)
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Pointer of BufIn is NULL \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_CHANGE_PART_ATTR, pBufIn:0x%x, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, pBufIn, __LINE__));
                break;
            }

            /* Check the length of pBufIn */
            if (nLenIn != sizeof(FSRChangePA))
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Length of BufIn is not valid \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_CHANGE_PART_ATTR, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, __LINE__));
                break;
            }

            /* Set pstPA using pBufIn */
            pstPA = (FSRChangePA *) pBufIn;

            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]  nSpecID   : 0x%x\r\n"), pstPA->nPartID));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]  nSpecAttr : %x\r\n"), pstPA->nNewAttr));

            nBMLRe = _ChangePA(nVol,
                               pstPA);
            if ((nBMLRe != FSR_BML_SUCCESS) && (nBMLRe != FSR_BML_CANT_FIND_PART_ID))
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_CHANGE_PART_ATTR, nRe:0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nBMLRe));
                break;
            }

            /* Set pBytesReturned */
            if (pBytesReturned != NULL)
            {
                *pBytesReturned = 0;
            }
        }
        break;

    case FSR_BML_IOCTL_GET_BMI:
        {
            UINT32          nDevIdx     = 0;    /* Device Index                 */
            UINT32          nDieIdx     = 0;    /* Die Index                    */
            FSRBMInfo      *pstBMInfo;
            BmlDevCxt      *pstDev;
            BmlReservoir   *pstRsv;
            BmlReservoirSh *pstRsvSh;

            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]  nCode = FSR_BML_IOCTL_GET_BMI\r\n")));

            /****************************************/
            /* Check input and output parameters    */
            /****************************************/
            if ((pBufIn == NULL) || (pBufOut == NULL))
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Pointer of BufIn or BufOut is NULL \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_GET_BMI, pBufIn:0x%x, pBufOut:0x%x, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, pBufIn, pBufOut, __LINE__));
                break;
            }

            if (nLenIn != sizeof(UINT32))
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Length of BufIn is not valid \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_GET_BMI, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, __LINE__));
                break;
            }

            if (nLenOut != sizeof(FSRBMInfo))
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Length of BufOut is not valid \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_GET_BMI, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, __LINE__));
                break;
            }

            /* Get nDevIdx by memcpy */
            FSR_OAM_MEMCPY(&nDevIdx, pBufIn, sizeof(UINT32));

            /* check the boundary of device by DEVS_PER_VOL */
            if (nDevIdx >= DEVS_PER_VOL)
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Device index is greater than device per volume \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_GET_BMI, nDevIdx:%d ,nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, nDevIdx, __LINE__));
                break;
            }

            /* check the boundary of device by pstVol->nNumOfDev */
            if (nDevIdx >= pstVol->nNumOfDev)
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Device index is greater than number of device \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_GET_BMI, nDevIdx:%d ,nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, nDevIdx, __LINE__));
                break;
            }

            /* Get pointer to device context */
            pstDev = _GetDevCxt(nVol * DEVS_PER_VOL + nDevIdx);

            /* Get pointer to pBufOut */
            pstBMInfo   = (FSRBMInfo *) pBufOut;

            /* Set # of dies in a device */
            pstBMInfo->nNumOfDies = pstVol->nNumOfDieInDev;

            for (nDieIdx = 0; nDieIdx < pstVol->nNumOfDieInDev; nDieIdx++)
            {
                /* Get BMF, # of BMFs from FSR_BBM_GetBMI() */
                FSR_BBM_GetBMI(pstVol,
                               pstDev,
                               nDieIdx,
                               pstBMInfo->pstBMF[nDieIdx],
                               &pstBMInfo->nNumOfBMFs[nDieIdx]);

                /* Get pointer to pstRsv BmlReservoir structure */
                pstRsv      = pstDev->pstDie[nDieIdx]->pstRsv;
                pstRsvSh    = pstDev->pstDie[nDieIdx]->pstRsvSh;

                /* Set stRsvInfo of FSRRsvrInfo*/
                pstBMInfo->stRsvInfo[nDieIdx].nUPCB         = pstRsvSh->nUPCBSbn;
                pstBMInfo->stRsvInfo[nDieIdx].nLPCB         = pstRsvSh->nLPCBSbn;
                pstBMInfo->stRsvInfo[nDieIdx].nTPCB         = pstRsvSh->nTPCBSbn;
                pstBMInfo->stRsvInfo[nDieIdx].nREF          = pstRsvSh->nREFSbn;
                pstBMInfo->stRsvInfo[nDieIdx].n1stBlkOfRsv  = (UINT16)pstRsv->n1stSbnOfRsvr;
                pstBMInfo->stRsvInfo[nDieIdx].nLastBlkOfRsv = (UINT16)pstRsv->nLastSbnOfRsvr;
            }

            /* Set pBytesReturned */
            if (pBytesReturned != NULL)
            {
                *pBytesReturned = sizeof(FSRBMInfo);
            }
        }
        break;

    case FSR_BML_IOCTL_SET_WTIME4ERR:
         {
            UINT32  nNMSec  = 0;

            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]  nCode = FSR_BML_IOCTL_SET_WTIME4ERR\r\n")));

            /* Check pBufIn whether it is null or not*/
            if (pBufIn == NULL)
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Pointer of BufIn is NULL \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_SET_WTIME4ERR, pBufIn:0x%x ,nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, pBufIn, __LINE__));
                break;
            }

            /* Check nLenIn*/
            if (nLenIn != sizeof(UINT32))
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Length of BufIn is not valid \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_SET_WTIME4ERR, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, __LINE__));
                break;
            }

            /* Set nNMSec using pBufIn */
            FSR_OAM_MEMCPY(&nNMSec, pBufIn, sizeof(UINT32));

            /* Set waiting time using BBM func. */
            FSR_BBM_SetWaitTimeForErError(nNMSec);

            /* Set pBytesReturned */
            if (pBytesReturned != NULL)
            {
                *pBytesReturned = 0;
            }
        }
        break;

    case FSR_BML_IOCTL_GET_OTP_INFO:
        {
            UINT32      nLockstat   = 0x00000000;  /* The lock state of OTP block  */

            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]  nCode = FSR_BML_IOCTL_GET_OTP_INFO\r\n")));

            /* Check whether this volume supports OTP mode or not */
            if (pstVol->bOTPEnable == FALSE32)
            {
                nBMLRe = FSR_BML_UNSUPPORTED_FUNCTION;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_GET_OTP_INFO, nRe:FSR_BML_UNSUPPORTED_FUNCTION) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, __LINE__));
                break;
            }

            /* Check pBufOut */
            if (pBufOut == NULL)
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Pointer of BufOut is NULL \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_GET_OTP_INFO, pBufOut:0x%x, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, pBufOut, __LINE__));
                break;
            }

            /* Check size of pBufOut */
            if (nLenOut != sizeof(UINT32))
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Length of BufOut is not valid \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_GET_OTP_INFO, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, __LINE__));
                break;
            }

            /* Get the state of OTP Block */
            nBMLRe = _GetOTPInfo(nVol,
                                 &nLockstat);
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_GET_OTP_INFO, nRe:0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nBMLRe));
                break;
            }

            /* Set pBufOut using nOPMode */
            FSR_OAM_MEMCPY(pBufOut, &nLockstat, sizeof(UINT32));

            /* Set pBytesReturned */
            if (pBytesReturned != NULL)
            {
                *pBytesReturned = sizeof(UINT32);
            }

        }
        break;

    case FSR_BML_IOCTL_OTP_LOCK:
        {
            UINT32      nOTPFlag    = 0x00000000;   /* The type of OTP block in BML             */

            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]  nCode = FSR_BML_IOCTL_OTP_LOCK\r\n")));

            /* Check whether this volume supports OTP mode or not */
            if (pstVol->bOTPEnable == FALSE32)
            {
                nBMLRe = FSR_BML_UNSUPPORTED_FUNCTION;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_OTP_LOCK, nRe:FSR_BML_UNSUPPORTED_FUNCTION) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, __LINE__));
                break;
            }

            /* Check pBufIn */
            if (pBufIn == NULL)
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Pointer of BufIn is NULL \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_OTP_LOCK, pBufIn:0x%x, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, pBufIn, __LINE__));
                break;
            }

            /* Check size of pBufIn */
            if (nLenIn != sizeof(UINT32))
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Length of BufIn is not valid \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_OTP_LOCK, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, __LINE__));
                break;
            }

            /* Set nOTPFlag from pBufIn */
            nOTPFlag     = *(UINT32 *)pBufIn;

            FSR_ASSERT(nOTPFlag & (FSR_BML_OTP_LOCK_1ST_BLK | FSR_BML_OTP_LOCK_OTP_BLK));

            nBMLRe = _LockOTPBlk(nVol,
                                 nOTPFlag);
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_OTP_LOCK, nRe:0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nBMLRe));
                break;
            }

            /* Set pBytesReturned */
            if (pBytesReturned != NULL)
            {
                *pBytesReturned = 0;
            }

        }
        break;

    case FSR_BML_IOCTL_GET_SLC_BOUNDARY:    /* By a device unit */
        {
            UINT32           nPDev      = 0;    /* Physical Device number       */
            UINT32           nDieIdx    = 0;    /* Die Index                    */
            UINT32           nByteRet   = 0;    /* # of returned bytes          */
            INT32            nLLDRe     = FSR_LLD_SUCCESS;
            LLDPIArg         stLLDPIArg;
            FSRSLCBoundary  *pstSLCBoundary;

            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]  nCode = FSR_BML_IOCTL_GET_SLC_BOUNDARY\r\n")));

            /* Check NAND type */
            if(pstVol->nNANDType != FSR_LLD_FLEX_ONENAND)
            {
                nBMLRe = FSR_BML_UNSUPPORTED_FUNCTION;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_GET_SLC_BOUNDARY, nRe:FSR_BML_UNSUPPORTED_FUNCTION) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, __LINE__));
                break;
            }

            /* Check pBufIn */
            if (pBufIn == NULL)
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Pointer of BufIn is NULL \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_GET_SLC_BOUNDARY, pBufIn:0x%x, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, pBufIn, __LINE__));
                break;
            }

            /* Check size of pBufIn */
            if (nLenIn != sizeof(UINT32))
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Length of BufIn is not valid \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_GET_SLC_BOUNDARY, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, __LINE__));
                break;
            }

            /* Check pBufOut */
            if (pBufOut == NULL)
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Pointer of BufOut is NULL \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_GET_SLC_BOUNDARY, pBufOut:0x%x, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, pBufOut, __LINE__));
                break;
            }

            /* Check size of pBufOut */
            if (nLenOut != sizeof(FSRSLCBoundary))
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Length of BufOut is not valid \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_GET_SLC_BOUNDARY, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, __LINE__));
                break;
            }

            /* Get nPDev from pBufIn */
            nPDev    = *(UINT32 *)pBufIn;

            /* Check the range of nPDev */
            if (nPDev >= (pstVol->nNumOfDev + (nVol << DEVS_PER_VOL_SHIFT)))
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] nPDev is greater than device boundary \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_GET_SLC_BOUNDARY, nPDev:%d, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, nPDev, __LINE__));
                break;
            }

            /* Set pstSLCBoundary by pBufOut */
            pstSLCBoundary = (FSRSLCBoundary *) pBufOut;

            /* Set nNumOfDies */
            pstSLCBoundary->nNumOfDies = pstVol->nNumOfDieInDev;

            for (nDieIdx = 0; nDieIdx < pstVol->nNumOfDieInDev; nDieIdx++)
            {
                /* Initilaize stLLDPIArg */
                FSR_OAM_MEMSET(&stLLDPIArg, 0x00, sizeof(LLDPIArg));

                /* get # of SLC Blocks in a device using LLD_IOCtl*/
                nLLDRe = pstVol->LLD_IOCtl(nPDev,
                                           FSR_LLD_IOCTL_PI_READ,
                                           (UINT8 *)&nDieIdx,
                                           sizeof(nDieIdx),
                                           (UINT8 *)&(stLLDPIArg),
                                           sizeof(stLLDPIArg),
                                           &nByteRet);
                if (nLLDRe != FSR_LLD_SUCCESS)
                {
                    nBMLRe = nLLDRe;
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] LLD_IOCtl(nPDev:%d, nCode:FSR_LLD_IOCTL_PI_READ, nDieIdx:%d)\r\n"), 
                                                    nPDev, nDieIdx));
                    FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_GET_SLC_BOUNDARY, nPDev:%d, nRe:0x%x)/ %d line\r\n"),
                                                    __FSR_FUNC__, nVol, nPDev, nBMLRe, __LINE__));
                   break;
                }

                /* Get nLastSLCBlkNum*/
                pstSLCBoundary->nLastSLCBlkNum[nDieIdx] = (UINT32)stLLDPIArg.nEndOfSLC;
                pstSLCBoundary->nPILockValue[nDieIdx]   = stLLDPIArg.nPILockValue;
            }

            /* Set pBytesReturned */
            if (pBytesReturned != NULL)
            {
                *pBytesReturned = sizeof(FSRSLCBoundary);
            }
        }
        break;

        case FSR_BML_IOCTL_FIX_SLC_BOUNDARY: /* By a volume unit */
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]  nCode = FSR_BML_IOCTL_FIX_SLC_BOUNDARY\r\n")));

            nBMLRe = _FixSLCBoundary(nVol);
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_FIX_SLC_BOUNDARY, nRe:0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nBMLRe));
            }

            /* Set pBytesReturned */
            if (pBytesReturned != NULL)
            {
                *pBytesReturned = 0;
            }
        }
        break;

    case FSR_BML_IOCTL_SET_NONBLOCKING:
        {
            UINT32      nFuncType;

            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]  nCode = FSR_BML_IOCTL_SET_NONBLOCKING\r\n")));

            /* Check the current operation mode */
            if (pstVol->bNonBlkMode == FALSE32)
            {
                /* Change the operation mode*/
                pstVol->bNonBlkMode = TRUE32;
            }
            else
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:   ]   %s(nVol: %d, nCode: FSR_BML_IOCTL_SET_NONBLOCKING)\r\n"), __FSR_FUNC__, nVol));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:   ]   The current operation mode is a non-blocking mode.")));
                break;
            }

            /* Check pBufIn */
            if (pBufIn == NULL)
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Pointer of BufIn is NULL \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_GET_SLC_BOUNDARY, pBufIn:0x%x, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, pBufIn, __LINE__));
                break;
            }

            /* Check size of pBufIn */
            if (nLenIn != sizeof(UINT32))
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Length of BufIn is not valid \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_GET_SLC_BOUNDARY, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, __LINE__));
                break;
            }

            /* Set nFuncType by pBufIn */
            nFuncType = *pBufIn;

            /* Check whether nFuncType is vaild or not */
            if ((nFuncType != FSR_BML_SET_NONBLOCKING_ALL)      &&
                (nFuncType != FSR_BML_SET_NONBLOCKING_READ_EXCEPT))
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Length of BufIn is not valid \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_GET_SLC_BOUNDARY, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, __LINE__));
                break;
            }

            /*
             * <Map the LLD function pointer to NonBlk function pointer>
             * if nFuncType is FSR_BML_SET_NONBLOCKING_READ_EXCEPT,
             * FSR_BML_IOCtl does not change the LLD function pointer for read operation.
             */
            if (nFuncType == FSR_BML_SET_NONBLOCKING_ALL)
            {
                pstVol->LLD_Read          = FSR_NBM_Read;
            }
            pstVol->LLD_Write         = FSR_NBM_Write;
            pstVol->LLD_CopyBack      = FSR_NBM_CopyBack;
            pstVol->LLD_Erase         = FSR_NBM_Erase;

            for (nIdx = FSR_INT_ID_NAND_0; nIdx <= FSR_INT_ID_NAND_7; nIdx++)
            {
                nPAMRe = FSR_PAM_InitInt(nIdx);
                if (nPAMRe != FSR_PAM_SUCCESS)
                {
                    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, nPAMRe));
                    return FSR_BML_PAM_ACCESS_ERROR;
                }

                nOAMRe = FSR_OAM_InitInt(nIdx);
                if (nOAMRe != FSR_OAM_SUCCESS)
                {
                    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, nOAMRe));
                    return FSR_BML_OAM_ACCESS_ERROR;
                }

                nOAMRe = FSR_OAM_ClrNDisableInt(nIdx);
                if (nOAMRe != FSR_OAM_SUCCESS)
                {
                    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, nOAMRe));
                    return FSR_BML_OAM_ACCESS_ERROR;
                }
            }

            /* Create event */
            bRet = FSR_OAM_CreateEvent(&(pstVol->nNBEvent));
            if (bRet != TRUE32)
            {
                nBMLRe = FSR_BML_OAM_ACCESS_ERROR;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:   ]   %s(nVol: %d, nCode: FSR_BML_IOCTL_SET_NONBLOCKING, nRe:FSR_BML_OAM_ACCESS_ERROR) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, __LINE__));
            }

            /* Set pBytesReturned */
            if (pBytesReturned != NULL)
            {
                *pBytesReturned = 0;
            }
        }
        break;

    case FSR_BML_IOCTL_SET_BLOCKING:
        {
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]  nCode = FSR_BML_IOCTL_SET_BLOCKING\r\n")));

            /* Check the current operation mode */
            if (pstVol->bNonBlkMode == TRUE32)
            {
                /* Change the operation mode*/
                pstVol->bNonBlkMode = FALSE32;
            }
            else
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:   ]   %s(nVol: %d, nCode: FSR_BML_IOCTL_SET_BLOCKING)\r\n"), __FSR_FUNC__, nVol));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:   ]   The current operation mode is a blocking mode.")));
                break;
            }

            /* Get pointer to FSRLowFuncTbl */
            pstLFT = _GetLFT(nVol);

            /* Map the LLD function pointer to original LLD function pointer */
            pstVol->LLD_Read          = pstLFT->LLD_ReadOptimal;
            pstVol->LLD_Write         = pstLFT->LLD_Write;
            pstVol->LLD_CopyBack      = pstLFT->LLD_CopyBack;
            pstVol->LLD_Erase         = pstLFT->LLD_Erase;

            /* Delete event */
            bRet = FSR_OAM_DeleteEvent(pstVol->nNBEvent);
            if (bRet != TRUE32)
            {
                nBMLRe = FSR_BML_OAM_ACCESS_ERROR;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:   ]   %s(nVol: %d, nCode: FSR_BML_IOCTL_SET_BLOCKING, nRe:FSR_BML_OAM_ACCESS_ERROR) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, __LINE__));
            }

            for (nIdx = FSR_INT_ID_NAND_0; nIdx <= FSR_INT_ID_NAND_7; nIdx++)
            {
                nOAMRe = FSR_OAM_ClrNDisableInt(nIdx);
                if (nOAMRe != FSR_OAM_SUCCESS)
                {
                    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, nOAMRe));
                    return FSR_BML_OAM_ACCESS_ERROR;
                }

                nOAMRe = FSR_OAM_DeinitInt(nIdx);
                if (nOAMRe != FSR_OAM_SUCCESS)
                {
                    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, nOAMRe));
                    return FSR_BML_OAM_ACCESS_ERROR;
                }

                nPAMRe = FSR_PAM_DeinitInt(nIdx);
                if (nPAMRe != FSR_PAM_SUCCESS)
                {
                    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, nPAMRe));
                    return FSR_BML_PAM_ACCESS_ERROR;
                }
            }

            /* Set pBytesReturned */
            if (pBytesReturned != NULL)
            {
                *pBytesReturned = 0;
            }
        }
        break;

    case FSR_BML_IOCTL_GET_OPMODE_STAT:
        {
            UINT32  nOPMode= FSR_BML_BLOCKING_MODE;/* Operation mode
                                                (FSR_BML_NONBLOCKING_MODE or FSR_BML_BLOCKING_MODE) */

            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]  nCode = FSR_BML_IOCTL_GET_OPMODE_STAT\r\n")));

            /* Check pBufOut */
            if (pBufOut == NULL)
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Pointer of BufOut is NULL \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, pBufOut:0x%x, nCode: FSR_BML_IOCTL_GET_OPMODE_STAT, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, pBufOut, __LINE__));
                break;
            }

            /* Check size of pBufOut */
            if (nLenOut != sizeof(UINT32))
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Length of BufOut is not valid \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_GET_OPMODE_STAT, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, __LINE__));
                break;
            }

            /* Set nOPMode */
            if (pstVol->bNonBlkMode == TRUE32)
            {
                nOPMode = FSR_BML_NONBLOCKING_MODE;
            }
            else /* In case of Blocking mode */
            {
                nOPMode = FSR_BML_BLOCKING_MODE;
            }

            /* Set pBufOut using nOPMode */
            FSR_OAM_MEMCPY(pBufOut, &nOPMode, sizeof(UINT32));

            /* Set pBytesReturned */
            if (pBytesReturned != NULL)
            {
                *pBytesReturned = sizeof(UINT32);
            }

        }
        break;

    case FSR_BML_IOCTL_GET_NUMOFERB:
        {
            UINT32        nDevIdx;
            UINT32        nDieIdx;
            UINT32        nPDev;
            UINT32        nTCnt;
            BmlDevCxt    *pstDev;
            BmlReservoir *pstRsv;
            BmlERL       *pstERL;

            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]  nCode = FSR_BML_IOCTL_GET_NUMOFERB\r\n")));
        
            /* Check output parameters    */
            if (pBufOut == NULL)
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Pointer of BufOut is NULL \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_GET_BMI, pBufIn:0x%x, pBufOut:0x%x, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, pBufIn, pBufOut, __LINE__));
                break;
            }

            /* Check size of pBufOut */
            if (nLenOut != sizeof(UINT32))
            {
                nBMLRe = FSR_BML_INVALID_PARAM;
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR] Length of BufOut is not valid \r\n")));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: FSR_BML_IOCTL_GET_OPMODE_STAT, nRe:FSR_BML_INVALID_PARAM) / %d line\r\n"),
                                                __FSR_FUNC__, nVol, __LINE__));
                break;
            }
            
            nTCnt = 0;

            for (nDevIdx = 0; nDevIdx < pstVol->nNumOfDev; nDevIdx++)
            {
                /* Set nPDev */
                nPDev      = nVol * DEVS_PER_VOL + nDevIdx;

                /* Get pointer to device context */
                pstDev     = _GetDevCxt(nPDev);

                for (nDieIdx = 0; nDieIdx < pstVol->nNumOfDieInDev; nDieIdx++)
                {
                    /* Get ERL */
                    pstRsv     = pstDev->pstDie[nDieIdx]->pstRsv;
                    pstERL     = pstRsv->pstERL;
                    nTCnt     += pstERL->nCnt;
                }
            }

            FSR_OAM_MEMCPY(pBufOut, &nTCnt, sizeof(UINT32));

            /* Set pBytesReturned */
            if (pBytesReturned != NULL)
            {
                *pBytesReturned = 0;
            }
        }
        break;

    default:
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]   Unsupported IOCtl\r\n")));
        nBMLRe = FSR_BML_UNSUPPORTED_IOCTL;

        /* Set pBytesReturned */
        if (pBytesReturned != NULL)
        {
            *pBytesReturned = 0;
        }
        break;

    }

    /* Release semaphore */
    bRet = FSR_OAM_ReleaseSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
    if (bRet == FALSE32)
    {
        nBMLRe = FSR_BML_RELEASE_SM_ERROR;
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol: %d, nCode: 0x%x, nRe:FSR_BML_RELEASE_SM_ERROR) / %d line\r\n"),
                                        __FSR_FUNC__, nVol, nCode, __LINE__));
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"), __FSR_FUNC__, nBMLRe));

    return nBMLRe;
}
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
#if !defined(TINY_FSR)
/**
 *  @brief      This function perform run time erase refresh by user.
 *
 *  @param [in]  nVol        : Volume number
 *  @param [in]  nNumOfunit  : The number of units to be erase refresh
 *  @param [in]  nFlag       : FSR_BML_FLAG_NONE
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     Some BML errors
 *
 *  @author     Donghoon Ham
 *  @version    1.1.0
 *
 */

PUBLIC INT32
FSR_BML_EraseRefresh(UINT32        nVol,
                     UINT32        nNumOfUnit,
                     UINT32        nFlag)
{
    UINT32      nPDev        = 0;    /* Physical device number   */
    UINT32      nDevIdx      = 0;    /* Device index             */
    UINT32      nDieIdx      = 0;    /* Die index                */
    UINT32      nRefreshFlag;
    UINT32      nERCnt       = 0;
    UINT32      nMaxERCnt    = 0;
    UINT32      nLargeDieIdx = 0;
    UINT32      nDieERLCnt[FSR_MAX_DIES];
    INT32       nBMLRe       = FSR_BML_SUCCESS;
    BOOL32      bRe          = FALSE32;

    BmlVolCxt  *pstVol;
    BmlDevCxt  *pstDev;
    BmlReservoir *pstRsv;
    BmlERL     *pstERL;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol:%d, nFlag:0x%x)\r\n"),
                                    __FSR_FUNC__, nVol, nFlag));

    /* check volume range */
    CHK_VOL_RANGE(nVol);

    /* Get pointer to volume context */
    pstVol = _GetVolCxt(nVol);

    /* Check the pointer to volume context */
    CHK_VOL_POINTER(pstVol);

    /* Check whether this volume is opened */
    CHK_VOL_OPEN(pstVol->bVolOpen);

    /* Check nFlag */
    if (nFlag != FSR_BML_FLAG_NONE)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:ERR] ++%s(nVol:%d, nFlag:0x%x)\r\n"),
                                        __FSR_FUNC__, nVol, nFlag));
        return FSR_BML_INVALID_PARAM;
    }

    if (nNumOfUnit == 0)
    {
        /* set default count of erase refresh */
        FSR_DBZ_RTLMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:INF]   nNumOfUnit is 0)\r\n")));
        return FSR_BML_SUCCESS;
    }
    else
    {
        /* set the user's count of retase regresh */
        nERCnt = nNumOfUnit;
    }

    /* Acquire semaphore */
    bRe = FSR_OAM_AcquireSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
    if (bRe == FALSE32)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nRe: FSR_BML_ACQUIRE_SM_ERROR) / %d line\r\n"),
                                        __FSR_FUNC__, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, FSR_BML_ACQUIRE_SM_ERROR));
        return FSR_BML_ACQUIRE_SM_ERROR;
    }

    do
    {
        /* Execute erase block refresh as nNumOfUnit */
        for (nDevIdx = 0; nDevIdx < pstVol->nNumOfDev; nDevIdx++)
        {
            /* Set nPDev */
            nPDev      = nVol * DEVS_PER_VOL + nDevIdx;

            /* Get pointer to device context */
            pstDev     = _GetDevCxt(nPDev);
            nLargeDieIdx  = 0;

            /* find die to have bigger ERCnt */
            for (nDieIdx = 0; nDieIdx < pstVol->nNumOfDieInDev; nDieIdx++)
            {
                /* Get ERL */
                pstRsv     = pstDev->pstDie[nDieIdx]->pstRsv;
                pstERL     = pstRsv->pstERL;

                /* Store ERCnt of die */
                nDieERLCnt[nDieIdx] = pstERL->nCnt;

                nLargeDieIdx = (nMaxERCnt > nDieERLCnt[nDieIdx]) ? nLargeDieIdx : nDieIdx;

                if (nLargeDieIdx == nDieIdx)
                {
                    nMaxERCnt = nDieERLCnt[nDieIdx];
                }
            }

            /* Set the number of erase refresh count to */
            nRefreshFlag = BML_FLAG_REFRESH_USER | (1 << 16);

            /* Process ERL */
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:   ]  Step 2. Processes ERL\r\n")));

            nBMLRe = FSR_BBM_RefreshByErase(pstVol,
                                            pstDev,
                                            nLargeDieIdx,
                                            nRefreshFlag | BML_FLAG_NOTICE_READ_ERROR);
            if (nBMLRe != FSR_BML_SUCCESS)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nFlag:0x%x)\r\n"),
                                                __FSR_FUNC__, nVol, nFlag));
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   FSR_BBM_RefreshByErase(nPDev:%d, nDieIdx:%d, nRe:0x%x) / %d line \r\n"),
                                                nPDev, nDieIdx, nBMLRe, __LINE__));
                break;
            }
            if (--nERCnt == 0)
            {
                break;
            }
        }
        if (nBMLRe != FSR_BML_SUCCESS)
        {
            break;
        }
    } while (nERCnt != 0);

    /* Release semaphore */
    bRe = FSR_OAM_ReleaseSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
    if (bRe == FALSE32)
    {
        FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nRe: FSR_BML_RELEASE_SM_ERROR) / %d line\r\n"),
                                        __FSR_FUNC__, __LINE__));
        FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, FSR_BML_RELEASE_SM_ERROR));
        nBMLRe = FSR_BML_RELEASE_SM_ERROR;
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nVol: %d)\r\n"), __FSR_FUNC__, nVol));

    return nBMLRe;

}

/**
 *  @brief      This function writes virtual pages.
 *
 *  @param [in]  nVol        : Volume number
 *  @param [in]  nVpn        : First virtual page number for writing
 *  @param [in]  nNumOfPgs   : The number of pages to write
 *  @param [in] *pMBuf       : Main page buffer for writing
 *  @param [in] *pSBuf       : Spare page buffer for writing
 *  @param [in]  nFlag       : FSR_BML_FLAG_ECC_ON
 *  @n                         FSR_BML_FLAG_ECC_OFF
 *  @n                         FSR_LLD_FLAG_USE_SPAREBUF
 *
 *  @return     FSR_BML_SUCCESS
 *  @return     FSR_BML_READ_ERROR
 *  @return     FSR_BML_CRITICAL_ERROR
 *  @return     FSR_BML_INVALID_PARAM
 *  @return     FSR_BML_ACQUIRE_SM_ERROR
 *  @return     FSR_BML_RELEASE_SM_ERROR
 *  @return     FSR_BML_VOLUME_NOT_OPENED
 *  @return     Some LLD errors
 *
 *  @author     DongHoon Ham
 *  @version    1.1.0
 *
 */
PUBLIC INT32
FSR_BML_Write(UINT32        nVol,
              UINT32        nVpn,
              UINT32        nNumOfPgs,
              UINT8        *pMBuf,
              FSRSpareBuf  *pSBuf,
              UINT32        nFlag)
{
    UINT32        nSplitVpn;
    UINT32        nNumOfSplitPgs;
    UINT32        nRemainPgs;
    UINT8        *pSplitMBuf;
    FSRSpareBuf  *pSplitSBuf;
    BmlVolCxt    *pstVol;
    INT32         nBMLRe = FSR_BML_SUCCESS;
    BOOL32        bRe;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:IN ] ++%s(nVol: %d, nVpn: %d, nNumOfPgs: %d, nFlag: 0x%x)\r\n")
                                    , __FSR_FUNC__, nVol, nVpn, nNumOfPgs, nFlag));

    /* check volume range */
    CHK_VOL_RANGE(nVol);

    /* Get pointer to Volume context */
    pstVol = _GetVolCxt(nVol);

    /* Check the pointer to volume context */
    CHK_VOL_POINTER(pstVol);

    /* check volume open */
    CHK_VOL_OPEN(pstVol->bVolOpen);

    FSR_ASSERT(nVol < FSR_MAX_VOLS);

    nRemainPgs = nNumOfPgs;
    nSplitVpn  = nVpn;
    pSplitMBuf = pMBuf;
    pSplitSBuf = pSBuf;

    do
    {
        if (nRemainPgs > FSR_BML_PGS_PER_SM_CYCLE)
        {
            nNumOfSplitPgs = FSR_BML_PGS_PER_SM_CYCLE;
        }
        else
        {
            nNumOfSplitPgs = nRemainPgs;
        }

        /* Acquire semaphore */
        bRe = FSR_OAM_AcquireSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
        if (bRe == FALSE32)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nRe: FSR_BML_ACQUIRE_SM_ERROR) / %d line\r\n"),
                                            __FSR_FUNC__, __LINE__));
            FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, FSR_BML_ACQUIRE_SM_ERROR));
            nBMLRe = FSR_BML_ACQUIRE_SM_ERROR;
            break;
        }

        nBMLRe = _BML_Write(nVol,
                           nSplitVpn,
                           nNumOfSplitPgs,
                           pSplitMBuf,
                           pSplitSBuf,
                           nFlag);
        if (nBMLRe != FSR_BML_SUCCESS)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nVol:%d, nSplitVpn:%d, nNumOfSplitPgs:%d, nRe:0x%x)\r\n"),
                                            __FSR_FUNC__, nVol, nSplitVpn, nNumOfSplitPgs, nBMLRe));
            bRe = FSR_OAM_ReleaseSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
            if (bRe == FALSE32)
            {
                FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nRe: FSR_BML_RELEASE_SM_ERROR) / %d line\r\n"),
                                                __FSR_FUNC__, __LINE__));
            }
            break;
        }

        /* Release semaphore */
        bRe = FSR_OAM_ReleaseSM(pstVol->nSM, FSR_OAM_SM_TYPE_BML);
        if (bRe == FALSE32)
        {
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[BIF:ERR]   %s(nRe: FSR_BML_RELEASE_SM_ERROR) / %d line\r\n"),
                                            __FSR_FUNC__, __LINE__));
            nBMLRe = FSR_BML_RELEASE_SM_ERROR;
            break;
        }

        nSplitVpn  += nNumOfSplitPgs;

        if (pSplitMBuf != NULL)  /* If main buffer is NULL, buffer should be not moved. */
        {
            pSplitMBuf += (pstVol->nSizeOfVPage * nNumOfSplitPgs);
        }
        
        if (pSplitSBuf != NULL)
        {     
            pSplitSBuf += nNumOfSplitPgs;
        }
        nRemainPgs -= nNumOfSplitPgs;

    } while (nRemainPgs > 0);

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_IF, (TEXT("[BIF:OUT] --%s(nRe: 0x%x)\r\n"),__FSR_FUNC__, nBMLRe));

    return nBMLRe;
}
#endif /* TINY_FSR */
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function get status of partiion attribute changing
 *
 *  @param [in]  nVol        : Volume number
 *  @param [in]  nPartID     : Partition ID
 *  @param [in]  nFlag       : FSR_BML_FLAG_NONE
 *
 *  @return     PartAttrChgHdl
 *
 *  @author     Donghoon Ham
 *  @version    1.1.0
 *
 */
BOOL32
FSR_BML_GetPartAttrChg (UINT32        nVol,
                        UINT32        nPartID,
                        UINT32        nFlag)
{
    return *_GetPartAttrChgHdl(nVol,nPartID);
}
/**
 *  @brief      This function enable partition attribute changing.
 *
 *  @param [in]  nVol        : Volume number
 *  @param [in]  nPartID     : Partition ID
 *  @param [in]  bSet        : Set PartAttrChg
 *  @param [in]  nFlag       : FSR_BML_FLAG_NONE
 *
 *  @return     None
 *
 *  @author     Donghoon Ham
 *  @version    1.1.0
 *
 */
VOID
FSR_BML_SetPartAttrChg(UINT32        nVol,
                       UINT32        nPartID,
                       BOOL32        bSet,
                       UINT32        nFlag)
{
    BOOL32     *pnPartAttrChg;
    pnPartAttrChg = _GetPartAttrChgHdl(nVol,nPartID);
    *pnPartAttrChg = bSet;
}
#endif /* FSR_NBL2 */
