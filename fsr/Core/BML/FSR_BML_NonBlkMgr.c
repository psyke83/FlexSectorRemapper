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
 *  @file       FSR_BML_NonBlkMgr.c
 *  @brief      This file consists of functions for non-blocking operation.
 *  @author     SuRuyn Lee
 *  @date       26-JUL-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  26-JUL-2007 [SuRyun Lee] : first writing
 *
 */

/**
 *  @brief  Header file inclusions
 */

#define  FSR_NO_INCLUDE_STL_HEADER
#include "FSR.h"

#include "FSR_BML_Config.h"
#include "FSR_BML_Types.h"
#include "FSR_BML_NonBlkMgr.h"
#include "FSR_BML_BIFCommon.h"
#include "FSR_BML_BBMCommon.h"
#include "FSR_BML_BadBlkMgr.h"

/**
 *  @brief  Code Implementation
 */
#if !defined(FSR_NBL2)
/**
 *  @brief      This function implements non-blocking read operation.
 *
 *  @param [in]   nDev         : Physical Device Number (0 ~ 3)
 *  @param [in]   nPbn         : Physical Block  Number
 *  @param [in]   nPgOffset    : Page Offset within a block
 *  @param [in]  *pMBuf        : Memory buffer for main array of NAND flash
 *  @param [in]  *pSBuf        : Memory buffer for spare array of NAND flash
 *  @param [in]   nFlag        : Operation options such as ECC_ON, OFF
 *
 *  @return     FSR_BML_INVALID_PARAM
 *  @return     FSR_BML_VOLUME_NOT_OPENED
 *  @return     Some LLD returns
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
FSR_NBM_Read(UINT32         nDev,
             UINT32         nPbn,
             UINT32         nPgOffset,
             UINT8         *pMBuf,
             FSRSpareBuf   *pSBuf,
             UINT32         nFlag)
{
    UINT32          nVol        = 0;                /* Volume number                */
    BOOL32          bRet        = TRUE32;           /* Temporary return value       */
    INT32           nRe         = FSR_LLD_SUCCESS;  /* Return value of LLD_Read     */
    BmlVolCxt      *pstVol;
    FSRLowFuncTbl  *pstLFT;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_LOG, (TEXT("[NBM:IN ] ++%s(nDev:%d, nPbn:%d, nPgOffset:%d, nFlag:0x%x)\r\n"),
                                      __FSR_FUNC__, nDev, nPbn, nPgOffset, nFlag));

    /* Get volume number (Only support 2 way/vol.)*/
    nVol = nDev >> DEVS_PER_VOL_SHIFT;

    /* checking volume range */
    CHK_VOL_RANGE(nVol);

    /* Get pointer to volume context*/
    pstVol = _GetVolCxt(nVol);

    /* Check whether pstVol is valid or not */
    CHK_VOL_POINTER(pstVol);

    /* Initialize interrupt bit */
    pstVol->pstNBCxt->nIntBit   = BML_CLEAR_INTERRUPT_BIT;

    /* Set interrupt bit except transfer only */
    if ((nFlag & FSR_LLD_FLAG_CMDIDX_MASK) != 0)
    {
        pstVol->pstNBCxt->nIntBit = BML_SET_INTERRUPT_BIT;
    }

    /* Get pointer to FSRLowFuncTbl */
    pstLFT = _GetLFT(nVol);

    /* Call LLD_Read */
    nRe = pstLFT->LLD_ReadOptimal(nDev,
                                  nPbn,
                                  nPgOffset,
                                  pMBuf,
                                  pSBuf,
                                  nFlag | pstVol->pstNBCxt->nIntBit);

    /* If the operation time is more than FSR_BML_KERNEL_LOCKUP_TIME,
     * Call FSR_OAM_ReceiveEvent() to sleep the current thread */
    if (pstVol->pstNBCxt->nIntBit == BML_SET_INTERRUPT_BIT)
    {
        bRet = FSR_OAM_ReceiveEvent(pstVol->nNBEvent);
        if (bRet != TRUE32)
        {
            nRe = FSR_OAM_CRITICAL_ERROR;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[NBM:ERR]   %s(nDev:%d, nPbn:%d, nPgOffset:%d, nFlag:0x%x)\r\n"),
                                            __FSR_FUNC__, nDev, nPbn, nPgOffset, nFlag));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[NBM:ERR]   FSR_OAM_ReceiveEvent(Event:0x%x, nRe:FSR_OAM_CRITICAL_ERROR)\r\n"), pstVol->nNBEvent));
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_LOG, (TEXT("[NBM:OUT] --%s(nRe:0x%x)\r\n"), __FSR_FUNC__, nRe));

    return nRe;
}
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function implements non-blocking write operation.
 *
 *  @param [in]   nDev         : Physical Device Number (0 ~ 3)
 *  @param [in]   nPbn         : Physical Block  Number
 *  @param [in]   nPgOffset    : Page Offset within a block
 *  @param [in]  *pMBuf        : Memory buffer for main array of NAND flash
 *  @param [in]  *pSBuf        : Memory buffer for spare array of NAND flash
 *  @param [in]   nFlag        : Operation options such as ECC_ON, OFF
 *
 *  @return     FSR_BML_INVALID_PARAM
 *  @return     FSR_BML_VOLUME_NOT_OPENED
 *  @return     Some LLD returns
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
FSR_NBM_Write(UINT32         nDev,
              UINT32         nPbn,
              UINT32         nPgOffset,
              UINT8         *pMBuf,
              FSRSpareBuf   *pSBuf,
              UINT32         nFlag)
{
    UINT32          nVol        = 0;            /* Volume number                */
    UINT32          nDevIdx     = 0;            /* Device Index in Volume       */
    UINT32          nDieIdx     = 0;            /* Die Index in Device          */
    UINT32          nWayIdx     = 0;            /* Way Index                    */
    UINT32          nIntWay     = 0;            /* Way index for interrupt      */
    UINT32          nOpOrder    = 0;            /* Operation order              */
    BOOL32          bRet    = TRUE32;           /* Temporary return value       */
    INT32           nRe     = FSR_LLD_SUCCESS;  /* Return value of LLD_Write    */
    BmlVolCxt      *pstVol;
    FSRLowFuncTbl  *pstLFT;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_LOG, (TEXT("[NBM:IN ] ++%s(nDev:%d, nPbn:%d, nPgOffset:%d, nFlag:0x%x)\r\n"),
                                      __FSR_FUNC__, nDev, nPbn, nPgOffset, nFlag));

    /* Get volume number (Only support 2 way/vol.)*/
    nVol = nDev >> DEVS_PER_VOL_SHIFT;

    /* checking volume range */
    CHK_VOL_RANGE(nVol);

    /* Get pointer to volume context*/
    pstVol = _GetVolCxt(nVol);

    /* Check whether pstVol is valid or not */
    CHK_VOL_POINTER(pstVol);

    /* Get device index by nDev */
    nDevIdx     = nDev >> DEVS_PER_VOL_SHIFT;

    /* Get die Index by Pbn*/
    if (nPbn < pstVol->nNumOfBlksInDie)
    {
        nDieIdx = 0;
    }
    else
    {
        nDieIdx = 1;
    }

    /* Get way index by nDevIdx, nDieIdx */
    nWayIdx     = (nDevIdx << pstVol->nSftDDP) + nDieIdx;

    /* Initialize interrupt bit */
    pstVol->pstNBCxt->nIntBit   = BML_CLEAR_INTERRUPT_BIT;

    /*Set interrupt bit by # of ways */
    if (pstVol->nNumOfWays == 1)
    {
        pstVol->pstNBCxt->nIntBit = BML_SET_INTERRUPT_BIT;
    }
    else
    {
        /*Set interrupt bit by operation order */
        nOpOrder    = nFlag & BML_NBM_MASK_SAME_OPTYPE;
        nIntWay     = ((pstVol->pstNBCxt->nStartWay + (pstVol->nNumOfWays - 1)) &
                       (pstVol->nNumOfWays - 1));
        switch (nOpOrder)
        {
        case BML_NBM_FLAG_START_SAME_OPTYPE:
            pstVol->pstNBCxt->nStartWay = nWayIdx;
            break;

        case BML_NBM_FLAG_CONTINUE_SAME_OPTYPE:
            if (nWayIdx == nIntWay)
            {
                pstVol->pstNBCxt->nIntBit = BML_SET_INTERRUPT_BIT;
            }
            break;

        case BML_NBM_FLAG_END_OF_SAME_OPTYPE:
            pstVol->pstNBCxt->nIntBit = BML_SET_INTERRUPT_BIT;
            break;
        }
    }

    /* Get pointer to FSRLowFuncTbl */
    pstLFT = _GetLFT(nVol);

    /* Call LLD_Read */
    nRe = pstLFT->LLD_Write(nDev,
                            nPbn,
                            nPgOffset,
                            pMBuf,
                            pSBuf,
                            nFlag | pstVol->pstNBCxt->nIntBit);

    /* If the operation time is more than FSR_BML_KERNEL_LOCKUP_TIME,
     * Call FSR_OAM_ReceiveEvent() to sleep the current thread */
    if (pstVol->pstNBCxt->nIntBit == BML_SET_INTERRUPT_BIT)
    {
        bRet = FSR_OAM_ReceiveEvent(pstVol->nNBEvent);
        if (bRet != TRUE32)
        {
            nRe = FSR_OAM_CRITICAL_ERROR;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[NBM:ERR]   %s(nDev:%d, nPbn:%d, nPgOffset:%d, nFlag:0x%x)\r\n"),
                                            __FSR_FUNC__, nDev, nPbn, nPgOffset, nFlag));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[NBM:ERR]   FSR_OAM_ReceiveEvent(Event:0x%x, nRe:FSR_OAM_CRITICAL_ERROR)\r\n"), pstVol->nNBEvent));
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_LOG, (TEXT("[NBM:OUT] --%s(nRe:0x%x)\r\n"), __FSR_FUNC__, nRe));

    return nRe;
}
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function implements non-blocking copyback operation.
 *
 *  @param [in]   nDev         : Physical Device Number (0 ~ 3)
 *  @param [in]  *pstCpArg     : pointer to the structure LLDCpBkArg
 *  @param [in]   nFlag        : FSR_LLD_FLAG_1X_CPBK_LOAD
 *  @n                           FSR_LLD_FLAG_1X_CPBK_PROGRAM
 *
 *  @return     FSR_BML_INVALID_PARAM
 *  @return     FSR_BML_VOLUME_NOT_OPENED
 *  @return     Some LLD returns
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
FSR_NBM_CopyBack(UINT32         nDev,
                 LLDCpBkArg    *pstCpArg,
                 UINT32         nFlag)
{
    UINT32          nVol        = 0;            /* Volume number                    */
    UINT32          nPbn        = 0;            /* Temporary physical block num.    */
    UINT32          nDevIdx     = 0;            /* Device Index in Volume           */
    UINT32          nDieIdx     = 0;            /* Die Index in Device              */
    UINT32          nWayIdx     = 0;            /* Way Index                        */
    UINT32          nIntWay     = 0;            /* Way index for interrupt          */
    UINT32          nOpOrder    = 0;            /* Operation order                  */
    BOOL32          bRet    = TRUE32;           /* Temporary return value           */
    INT32           nRe     = FSR_LLD_SUCCESS;  /* Return value of LLD_CopyBack     */
    BmlVolCxt      *pstVol;
    FSRLowFuncTbl  *pstLFT;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_LOG, (TEXT("[NBM:IN ] ++%s(nDev:%d, pstCpArg:%d, nFlag:0x%x)\r\n"),
                                      __FSR_FUNC__, nDev, pstCpArg, nFlag));

    /* Get volume number (Only support 2 way/vol.)*/
    nVol = nDev >> DEVS_PER_VOL_SHIFT;

    /* checking volume range */
    CHK_VOL_RANGE(nVol);

    /* Get pointer to volume context*/
    pstVol = _GetVolCxt(nVol);

    /* Check whether pstVol is valid or not */
    CHK_VOL_POINTER(pstVol);

    /* Initialize interrupt bit */
    pstVol->pstNBCxt->nIntBit = BML_CLEAR_INTERRUPT_BIT;

    /*
     * <Set interrupt bit by cmd type>
     * 1. Copyback_Load: Set interrupt bit by each LLD_Copyback
     * 2. Copyback_Program: Set interrupt bit last way
     */
    if (nFlag & BML_FLAG_CPBK_LOAD)
    {
        pstVol->pstNBCxt->nIntBit = BML_SET_INTERRUPT_BIT;
    }
    else
    {
        /* Get device index by nDev */
        nDevIdx     = nDev >> DEVS_PER_VOL_SHIFT;

        nPbn        = pstCpArg->nDstPbn;

        /* Get Die Index by Pbn*/
        if (nPbn < pstVol->nNumOfBlksInDie)
        {
            nDieIdx = 0;
        }
        else
        {
            nDieIdx = 1;
        }

        /* Get way index by nDieIdx, nDevIdx */
        nWayIdx     = (nDevIdx << pstVol->nSftDDP) + nDieIdx;

        /*Set interrupt bit by # of ways */
        if (pstVol->nNumOfWays == 1)
        {
            pstVol->pstNBCxt->nIntBit = BML_SET_INTERRUPT_BIT;
        }
        else
        {
            /* Get operation order by nFlag */
            nOpOrder    = nFlag & BML_NBM_MASK_SAME_OPTYPE;
            nIntWay     = ((pstVol->pstNBCxt->nStartWay + (pstVol->nNumOfWays - 1)) &
                           (pstVol->nNumOfWays - 1));
            switch (nOpOrder)
            {
            case BML_NBM_FLAG_START_SAME_OPTYPE:
                pstVol->pstNBCxt->nStartWay = nWayIdx;
                break;

            case BML_NBM_FLAG_CONTINUE_SAME_OPTYPE:
                if (nWayIdx == nIntWay)
                {
                    pstVol->pstNBCxt->nIntBit = BML_SET_INTERRUPT_BIT;
                }
                break;

            case BML_NBM_FLAG_END_OF_SAME_OPTYPE:
                pstVol->pstNBCxt->nIntBit = BML_SET_INTERRUPT_BIT;
                break;
            }
        }
    }

    /* Get pointer to FSRLowFuncTbl */
    pstLFT = _GetLFT(nVol);

    /* Call LLD_Read */
    nRe = pstLFT->LLD_CopyBack(nDev,
                               pstCpArg,
                               nFlag | pstVol->pstNBCxt->nIntBit);

    /* If the operation time is more than FSR_BML_KERNEL_LOCKUP_TIME,
     * Call FSR_OAM_ReceiveEvent() to sleep the current thread */
    if (pstVol->pstNBCxt->nIntBit == BML_SET_INTERRUPT_BIT)
    {
        bRet = FSR_OAM_ReceiveEvent(pstVol->nNBEvent);
        if (bRet != TRUE32)
        {
            nRe = FSR_OAM_CRITICAL_ERROR;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[NBM:ERR]   %s(nDev:%d, pstCpArg:%d, nFlag:0x%x)\r\n"),
                                            __FSR_FUNC__, nDev, pstCpArg, nFlag));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[NBM:ERR]   FSR_OAM_ReceiveEvent(Event:0x%x, nRe:FSR_OAM_CRITICAL_ERROR)\r\n"), pstVol->nNBEvent));
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_LOG, (TEXT("[NBM:OUT] --%s(nRe:0x%x)\r\n"), __FSR_FUNC__, nRe));

    return nRe;
}
#endif /* FSR_NBL2 */

#if !defined(FSR_NBL2)
/**
 *  @brief      This function implements non-blocking erase operation.
 *
 * @param [in]    nDev         : Physical Device Number (0 ~ 3)
 * @param [in]   *pnPbn        : array of blocks, not necessarilly consecutive
 * @n                            which will be supported in the future
 * @param [in]    nNumOfBlks   : The number of blocks to erase
 * @param [in]    nFlag        : FSR_LLD_FLAG_1X_ERASE
 * @n                            FSR_LLD_FLAG_2X_ERASE
 *
 *  @return     FSR_BML_INVALID_PARAM
 *  @return     FSR_BML_VOLUME_NOT_OPENED
 *  @return     Some LLD returns
 *
 *  @author     SuRyun Lee
 *  @version    1.0.0
 *
 */
PUBLIC INT32
FSR_NBM_Erase(UINT32         nDev,
              UINT32        *pnPbn,
              UINT32         nNumOfBlks,
              UINT32         nFlag)
{
    UINT32          nVol        = 0;            /* Volume number                */
    UINT32          nPbn        = 0;            /* Temporary physical block num.*/
    UINT32          nDevIdx     = 0;            /* Device Index in Volume       */
    UINT32          nDieIdx     = 0;            /* Die Index in Device          */
    UINT32          nWayIdx     = 0;            /* Way Index                    */
    UINT32          nIntWay     = 0;            /* Way index for interrupt      */
    UINT32          nOpOrder    = 0;            /* Operation order              */
    BOOL32          bRet    = TRUE32;           /* Temporary return value       */
    INT32           nRe     = FSR_LLD_SUCCESS;  /* Return value of LLD_Erase    */
    BmlVolCxt      *pstVol;
    FSRLowFuncTbl  *pstLFT;

    FSR_STACK_VAR;

    FSR_STACK_END;

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_LOG, (TEXT("[NBM:IN ] ++%s(nDev:%d, *pnPbn:%d, nNumOfBlks:%d, nFlag:0x%x)\r\n"),
                                      __FSR_FUNC__, nDev, *pnPbn, nNumOfBlks, nFlag));

    /* Get volume number (Only support 2 way/vol.)*/
    nVol = nDev >> DEVS_PER_VOL_SHIFT;

    /* checking volume range */
    CHK_VOL_RANGE(nVol);

    /* Get pointer to volume context*/
    pstVol = _GetVolCxt(nVol);

    /* Check whether pstVol is valid or not */
    CHK_VOL_POINTER(pstVol);

    nPbn = *pnPbn;

    /* Get device index by nDev */
    nDevIdx     = nDev >> DEVS_PER_VOL_SHIFT;

    /* Get Die Index by Pbn*/
    if (nPbn < pstVol->nNumOfBlksInDie)
    {
        nDieIdx = 0;
    }
    else
    {
        nDieIdx = 1;
    }

    /* Get way index by nDevIdx, nDieIdx */
    nWayIdx     = (nDevIdx << pstVol->nSftDDP) + nDieIdx;

    /* Initialize interrupt bit */
    pstVol->pstNBCxt->nIntBit   = BML_CLEAR_INTERRUPT_BIT;

    /*Set interrupt bit by # of ways */
    if (pstVol->nNumOfWays == 1)
    {
        pstVol->pstNBCxt->nIntBit = BML_SET_INTERRUPT_BIT;
    }
    else
    {
        /* Set interrupt bit by operation order */
        nOpOrder    = nFlag & BML_NBM_MASK_SAME_OPTYPE;
        nIntWay     = ((pstVol->pstNBCxt->nStartWay + (pstVol->nNumOfWays - 1)) &
                       (pstVol->nNumOfWays - 1));
        switch (nOpOrder)
        {
        case BML_NBM_FLAG_START_SAME_OPTYPE:
            pstVol->pstNBCxt->nStartWay = nWayIdx;
            break;

        case BML_NBM_FLAG_CONTINUE_SAME_OPTYPE:
            if (nWayIdx == nIntWay)
            {
                pstVol->pstNBCxt->nIntBit = BML_SET_INTERRUPT_BIT;
            }
            break;

        case BML_NBM_FLAG_END_OF_SAME_OPTYPE:
            pstVol->pstNBCxt->nIntBit = BML_SET_INTERRUPT_BIT;
            break;
        }
    }

    /* Get pointer to FSRLowFuncTbl */
    pstLFT = _GetLFT(nVol);

    /* Call LLD_Read */
    nRe = pstLFT->LLD_Erase(nDev,
                            pnPbn,
                            nNumOfBlks,
                            nFlag | pstVol->pstNBCxt->nIntBit);

    /* If the operation time is more than FSR_BML_KERNEL_LOCKUP_TIME,
     * Call FSR_OAM_ReceiveEvent() to sleep the current thread */
    if (pstVol->pstNBCxt->nIntBit == BML_SET_INTERRUPT_BIT)
    {
        bRet    = FSR_OAM_ReceiveEvent(pstVol->nNBEvent);
        if (bRet != TRUE32)
        {
            nRe = FSR_OAM_CRITICAL_ERROR;
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[NBM:ERR]   %s(nDev:%d, *pnPbn:%d, nNumOfBlks:%d, nFlag:0x%x)\r\n"),
                                            __FSR_FUNC__, nDev, *pnPbn, nNumOfBlks, nFlag));
            FSR_DBZ_RTLMOUT(FSR_DBZ_ERROR,  (TEXT("[NBM:ERR]   FSR_OAM_ReceiveEvent(Event:0x%x, nRe:FSR_OAM_CRITICAL_ERROR)\r\n"), pstVol->nNBEvent));
        }
    }

    FSR_DBZ_DBGMOUT(FSR_DBZ_BML_LOG, (TEXT("[NBM:OUT] --%s(nRe:0x%x)\r\n"), __FSR_FUNC__, nRe));

    return nRe;
}
#endif /* FSR_NBL2 */
