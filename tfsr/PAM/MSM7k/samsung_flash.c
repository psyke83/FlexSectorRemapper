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
#include "samsung_flash.h"
#include "FSR_SpinLock.h"

#define FS_DEVICE_OK              0
#define FS_DEVICE_FAIL            (-1)
#define FS_DEVICE_NOT_SUPPORTED   (-15)

#define FLASH_UNLOCK_OP           0x1
#define FLASH_LOCK_OP             0x2
#define FLASH_LOCKTIGHT_OP        0x3

static UINT8 aMBuf[FSR_SECTOR_SIZE * FSR_MAX_PHY_SCTS];

INT32
samsung_ftl_init(void)
{
    INT32   nRet;
    UINT32  nVol = 0;
#if defined (BUILD_JNAND)
    UINT32  nBytesReturned;
#endif
    static BOOL32 Ftl_Init_Flag = FALSE32;

    if (Ftl_Init_Flag == FALSE32)
    {
        nRet = FSR_BML_Init(FSR_BML_FLAG_NONE);
        if ((nRet != FSR_BML_SUCCESS)&&(nRet != FSR_BML_ALREADY_INITIALIZED))
        {
            return FS_DEVICE_FAIL;
        }

        nRet = FSR_BML_Open(nVol, FSR_BML_FLAG_NONE);
        if (nRet != FSR_BML_SUCCESS)
        {
            return FS_DEVICE_FAIL;
        }

#if defined (BUILD_JNAND)
        nRet = FSR_BML_IOCtl(nVol,
                             FSR_BML_IOCTL_UNLOCK_WHOLEAREA,
                             NULL,
                             0,
                             NULL,
                             0,
                             &nBytesReturned);
        if (nRet != FSR_BML_SUCCESS)
        {
            return FS_DEVICE_FAIL;
        }
#endif

        Ftl_Init_Flag = TRUE32;
    }

    return FS_DEVICE_OK;
}

INT32
samsung_bml_format(FSRPartI* pstFSRPartI)
{
#if !defined (FSR_NBL2)
     INT32 nRet;
    UINT32 nVol = 0;

    nRet = FSR_BML_Init(FSR_BML_FLAG_NONE);
    if ((nRet != FSR_BML_SUCCESS) && (nRet != FSR_BML_ALREADY_INITIALIZED))
    {
        return FS_DEVICE_FAIL;
    }

    nRet = FSR_BML_Format(nVol,
                          pstFSRPartI,
                          (FSR_BML_INIT_FORMAT | FSR_BML_AUTO_ADJUST_PARTINFO));
    if (nRet != FSR_BML_SUCCESS)
    {
        return FS_DEVICE_FAIL;
    }
#endif
    return FS_DEVICE_OK;
}

INT32
samsung_stl_init(UINT32 nPartID)
{
#if defined (BUILD_JNAND)
           INT32       nRet;
           UINT32      nVol = 0;
           FSRStlInfo  stSTLInfo;
    static BOOL32      Stl_Init_Flag = FALSE32;

    if (Stl_Init_Flag == FALSE32)
    {
        nRet = FSR_STL_Init();
        if ((nRet != FSR_STL_SUCCESS)&&(nRet != FSR_STL_ALREADY_INITIALIZED))
        {
            return FS_DEVICE_FAIL;
        }

        nRet = FSR_STL_Open(nVol,
                            nPartID,
                            &stSTLInfo,
                            FSR_STL_FLAG_DEFAULT);
        if (nRet != FSR_STL_SUCCESS)
        {
            return FS_DEVICE_FAIL;
        }

        Stl_Init_Flag = TRUE32;
    }
#endif
    return FS_DEVICE_OK;
}

INT32
samsung_stl_format(UINT32 nPartID)
{
#if defined (BUILD_JNAND)
    INT32           nRet;
    UINT32          nVol = 0;
    FSRStlFmtInfo   stStlFmt;

    nRet = FSR_STL_Init();
    if ((nRet != FSR_STL_SUCCESS) && (nRet != FSR_STL_ALREADY_INITIALIZED))
    {
        return FS_DEVICE_FAIL;
    }

    stStlFmt.nOpt = FSR_STL_FORMAT_NONE;

    nRet = FSR_STL_Format(nVol,
                          nPartID,
                          &stStlFmt);
    if ((nRet != FSR_STL_SUCCESS) && (nRet != FSR_STL_PROHIBIT_FORMAT_BY_GWL))
    {
        return FS_DEVICE_FAIL;
    }
#endif
    return FS_DEVICE_OK;
}


INT32
samsung_stl_write(UINT32 nImgSize, UINT8* pBuf, UINT32 nPartID)
{
#if defined (BUILD_JNAND)
    INT32   nRet;
    UINT32  nVol = 0;
    UINT32  nLsn = 0;
    UINT32  nNumOfScts;

    /* Calculate # of sectors to be written */
    nNumOfScts = (nImgSize % FSR_SECTOR_SIZE) ? (nImgSize / FSR_SECTOR_SIZE + 1) : (nImgSize / FSR_SECTOR_SIZE);

    nRet = FSR_STL_Write(nVol,
                         nPartID,
                         nLsn,
                         nNumOfScts,
                         pBuf,
                         FSR_STL_FLAG_DEFAULT);
    if (nRet != FSR_STL_SUCCESS)
    {
        return FS_DEVICE_FAIL;
    }
#endif
    return FS_DEVICE_OK;
}

INT32
samsung_bml_write(UINT32 nStartVpn, UINT32 nNumOfPgs, UINT8* pMBuf, FSRSpareBuf *pSBuf)
{
#if !defined (FSR_NBL2)
    INT32             nRet;
    UINT32            nVol = 0;
    FSRSpareBuf*  pstTmpSpareBuf;
    FSRSpareBuf       stSpareBuf;
    FSRSpareBufBase   stSpareBufBase;
    FSRSpareBufExt    stSpareBufExt[FSR_MAX_SPARE_BUF_EXT];

    if (pSBuf == NULL)
    {
        /* Set spare buffer */
        FSR_OAM_MEMSET(&stSpareBufBase, 0xFF, sizeof(FSRSpareBufBase));
        FSR_OAM_MEMSET(&stSpareBufExt[0], 0xFF, sizeof(FSRSpareBufExt) * FSR_MAX_SPARE_BUF_EXT);
        stSpareBuf.pstSpareBufBase  = &stSpareBufBase;
        stSpareBuf.nNumOfMetaExt    = 2;
        stSpareBuf.pstSTLMetaExt    = &stSpareBufExt[0];
        pstTmpSpareBuf = &stSpareBuf;
    }
    else
    {
        pstTmpSpareBuf = pSBuf;
    }

    nRet = FSR_BML_Write(nVol,
                         nStartVpn,
                         nNumOfPgs,
                         pMBuf,
                         pstTmpSpareBuf,
                         (FSR_BML_FLAG_ECC_ON | FSR_BML_FLAG_USE_SPAREBUF));

    if (nRet != FSR_BML_SUCCESS)
    {
        return FS_DEVICE_FAIL;
    }
#endif
    return FS_DEVICE_OK;
}

INT32
samsung_bml_erase(UINT32 nVun, UINT32 nNumOfUnits)
{
#if !defined (FSR_NBL2)
    INT32   nRet ;
    UINT32  nVol = 0;
    UINT32  nCnt = 0;

    for (nCnt = nVun; nCnt < nNumOfUnits+nVun; nCnt++)
    {
        nRet = FSR_BML_Erase(nVol, &nCnt, 1, FSR_BML_FLAG_NONE);
        if (nRet != FSR_BML_SUCCESS)
        {
            return FS_DEVICE_FAIL;
        }
    }
#endif
    return FS_DEVICE_OK;
}

INT32
samsung_is_page_erased(UINT32 nPage)
{
    INT32           nRet ;
    UINT32          nVol = 0;
    UINT32          nCnt = 0;
    FSRVolSpec      stVolSpec;

    /* Get volume information */
    nRet = FSR_BML_GetVolSpec(nVol,
                              &stVolSpec,
                              FSR_BML_FLAG_NONE);
    if (nRet != FSR_BML_SUCCESS)
    {
        return FALSE32;
    }

    nRet = samsung_bml_read(nPage * stVolSpec.nSctsPerPg,
                            stVolSpec.nSctsPerPg,
                            (UINT8*)aMBuf,
                            NULL);
    if (nRet != FS_DEVICE_OK)
    {
        return FALSE32;
    }

    for (nCnt = 0; nCnt < FSR_SECTOR_SIZE * stVolSpec.nSctsPerPg; nCnt+=sizeof(UINT32))
    {
        if (*(UINT32*)(aMBuf + nCnt) != 0xFFFFFFFF)
        {
            return FALSE32;
        }
    }

    return TRUE32;
}

INT32
samsung_bml_read(UINT32 nStartScts, UINT32 nNumOfScts, UINT8* pMBuf, FSRSpareBuf *pSBuf)
{
    INT32       nRet       = FSR_BML_SUCCESS;
    UINT32      nVol       = 0;
    UINT32      nStartVpn  = 0;
    UINT32      n1stSecOff = 0;
    FSRVolSpec  stVolSpec;

    /* Get volume information */
    nRet = FSR_BML_GetVolSpec(nVol,
                              &stVolSpec,
                              FSR_BML_FLAG_NONE);
    if (nRet != FSR_BML_SUCCESS)
    {
        return FALSE32;
    }

    /* Calculate nStartVpn, n1stSecOff */
    nStartVpn  = nStartScts / stVolSpec.nSctsPerPg;
    n1stSecOff = (nStartScts & (stVolSpec.nSctsPerPg-1));

    if (((UINT32) pMBuf == 0x0) || ((UINT32) pMBuf == 0x1000) || ((UINT32) pMBuf == 0x2000))
    {
        if (nNumOfScts > FSR_MAX_PHY_SCTS)
        {
          //the size of aMBuf is only 4096
          return FS_DEVICE_FAIL;
        }

        nRet = FSR_BML_ReadScts(nVol,
                              nStartVpn,
                              n1stSecOff,
                              nNumOfScts,
                              aMBuf,
                              pSBuf,
                              FSR_BML_FLAG_ECC_ON);

        if (nRet != FSR_BML_SUCCESS)
        {
          return FS_DEVICE_FAIL;
        }

        FSR_OAM_MEMCPY(pMBuf, aMBuf, nNumOfScts * FSR_SECTOR_SIZE);

    }
    else
    {
        nRet = FSR_BML_ReadScts(nVol,
                              nStartVpn,
                              n1stSecOff,
                              nNumOfScts,
                              pMBuf,
                              pSBuf,
                              FSR_BML_FLAG_ECC_ON);
        if (nRet != FSR_BML_SUCCESS)
        {
          return FS_DEVICE_FAIL;
        }
    }

    return FS_DEVICE_OK;
}

INT32
samsung_bml_lock_op (UINT32 nStartUnit, UINT32 nNumOfUnits, UINT32 nOpcode)
{
#if !defined (FSR_NBL2)
    INT32            nRet;
    INT32            nErr = FS_DEVICE_OK;
    UINT32           nVol = 0;
    UINT32           nBytesReturned;
    UINT32           nDevIdx = 0;
    UINT32           n1stVpn;
    UINT32           nPgsPerUnit;
    UINT32           nRsrvBlk;
    UINT32           nCnt;
    UINT32           nDieIdx;
    UINT32           nBMFIdx;
    UINT32           nSbn;
    UINT32           nPbn;
    UINT32           nLockStat;
    UINT32           nLLDCode;
    UINT32           nErrPbn;
    FSRDevSpec       stDevSpec;
    FSRBMInfo        stBMInfo;
    BmlBMF           stBmf[FSR_MAX_DIES][FSR_MAX_BAD_BLKS];
    FSRLowFuncTbl    stLFT[FSR_MAX_VOLS];
    FSRLowFuncTbl   *pstLFT[FSR_MAX_VOLS];
    LLDProtectionArg stLLDIO;

    /* check input parameter */
    if ((nOpcode != FLASH_UNLOCK_OP)    ||
        (nOpcode != FLASH_LOCK_OP)      ||
        (nOpcode != FLASH_LOCKTIGHT_OP))
    {
        return FS_DEVICE_NOT_SUPPORTED;
    }

    /* Get BMI */
    FSR_OAM_MEMSET(&stBMInfo, 0x00, sizeof(stBMInfo));
    stBMInfo.pstBMF[0] = &stBmf[0][0];
    stBMInfo.pstBMF[1] = &stBmf[1][0];

    nRet = FSR_BML_IOCtl(nVol,
                         FSR_BML_IOCTL_GET_BMI,
                         (UINT8*)&nDevIdx,
                         sizeof(nDevIdx),
                         (UINT8*)&stBMInfo,
                         sizeof(stBMInfo),
                         &nBytesReturned);
    if (nRet != FSR_BML_SUCCESS)
    {
        return FS_DEVICE_FAIL;
    }

    /* Acquire SM */
    FSR_BML_AcquireSM(nVol);

    do
    {
        /* Register LLD Function Table */
        FSR_OAM_MEMSET(&stLFT[0], 0x00, sizeof(FSRLowFuncTbl));
        pstLFT[0] = &stLFT[0];
        pstLFT[1] = &stLFT[1];

        nRet = FSR_PAM_RegLFT(pstLFT);
        if (nRet != FSR_PAM_SUCCESS)
        {
            nErr = FS_DEVICE_FAIL;
            break;
        }

        nRet = pstLFT[nVol]->LLD_GetDevSpec(nDevIdx,
                                            &stDevSpec,
                                            FSR_LLD_FLAG_NONE);
        if (nRet != FSR_LLD_SUCCESS)
        {
            nErr = FS_DEVICE_FAIL;
            break;
        }

        /* Find the SLC/MLC attributes of units
          (Let's suppose all units have same attributes) */
        nRet = FSR_BML_GetVirUnitInfo(nVol,
                                      nStartUnit,
                                      &n1stVpn,
                                      &nPgsPerUnit);
        if (nRet != FSR_BML_SUCCESS)
        {
            nErr = FS_DEVICE_FAIL;
            break;
        }

        nRsrvBlk = 0;
        if (nPgsPerUnit == (stDevSpec.nPgsPerBlkForMLC*stDevSpec.nNumOfDies))
        {
            nRsrvBlk = 30;
        }

        for (nCnt = nStartUnit; nCnt<(nStartUnit+nNumOfUnits); nCnt++)
        {
            for (nDieIdx = 0; stDevSpec.nNumOfDies; nDieIdx++)
            {
                /* Change the Vun to the Pbn */
                nSbn = nRsrvBlk + nCnt + nDieIdx*(stDevSpec.nNumOfBlks/stDevSpec.nNumOfDies);
                nPbn = nSbn;
                for (nBMFIdx = 0; nBMFIdx<stBMInfo.nNumOfBMFs[nDieIdx]; nBMFIdx++)
                {
                    if (nSbn == stBMInfo.pstBMF[nDieIdx]->nSbn)
                    {
                        nPbn = stBMInfo.pstBMF[nDieIdx]->nRbn;
                        break;
                    }
                    stBMInfo.pstBMF[nDieIdx]++;
                }

                /* check the block lock state */
                nRet = pstLFT[nVol]->LLD_IOCtl(nDevIdx,
                                               FSR_LLD_IOCTL_GET_LOCK_STAT,
                                               (UINT8*) &nPbn,
                                               sizeof(nPbn),
                                               (UINT8*) &nLockStat,
                                               sizeof(nLockStat),
                                               &nBytesReturned);
                if (nRet != FSR_LLD_SUCCESS)
                {
                    nErr = FS_DEVICE_FAIL;
                    break;
                }

                if ((nLockStat  == FSR_LLD_BLK_STAT_LOCKED_TIGHT) &&
                    (nOpcode    != FLASH_LOCKTIGHT_OP))
                {
                    nErr = FS_DEVICE_FAIL;
                    break;
                }

                if (nOpcode == FLASH_UNLOCK_OP)
                {
                    nLLDCode = FSR_LLD_IOCTL_UNLOCK_BLOCK;
                }
                else if (nOpcode == FLASH_LOCK_OP)
                {
                    nLLDCode = FSR_LLD_IOCTL_LOCK_BLOCK;
                }
                else if (nOpcode == FLASH_LOCKTIGHT_OP)
                {
                     nLLDCode = FSR_LLD_IOCTL_LOCK_TIGHT;

                    /* change the block state to lock state */
                    stLLDIO.nStartBlk = nPbn;
                    stLLDIO.nBlks     = 1;

                    nRet = pstLFT[nVol]->LLD_IOCtl(nDevIdx ,
                                                   FSR_LLD_IOCTL_LOCK_BLOCK,
                                                   (UINT8 *) &stLLDIO,
                                                   sizeof(stLLDIO),
                                                   (UINT8 *) &nErrPbn,
                                                   sizeof(nErrPbn),
                                                   &nBytesReturned);
                    if (nRet != FSR_LLD_SUCCESS)
                    {
                        nErr = FS_DEVICE_FAIL;
                        break;
                    }
                }

                /* change the block lock state according to nOpcode */
                stLLDIO.nStartBlk = nPbn;
                stLLDIO.nBlks     = 1;
                nRet = pstLFT[nVol]->LLD_IOCtl(nDevIdx,
                                               nLLDCode,
                                               (UINT8 *) &stLLDIO,
                                               sizeof(stLLDIO),
                                               (UINT8 *) &nErrPbn,
                                               sizeof(nErrPbn),
                                               &nBytesReturned);
                if (nRet != FSR_LLD_SUCCESS)
                {
                    nErr = FS_DEVICE_FAIL;
                    break;
                }

            } /* End of "for (nDieIdx = 0;...)" */

            if (nErr != FS_DEVICE_OK)
            {
                break;
            }

        } /* End of "for (nCnt = nStartUnit;...)" */

    } while(0);

    /* Release SM */
    FSR_BML_ReleaseSM(nVol);

    return nErr;
#else
    return FS_DEVICE_OK;
#endif
}

UINT32
samsung_retrieve_ID(void)
{
    INT32           nRet;
    UINT32          nVol = 0;
    UINT32          nDev = 0;
    UINT32          nDID = 0;
    FSRLowFuncTbl  *pstLFT[FSR_MAX_VOLS];
    FSRLowFuncTbl   stLFT[FSR_MAX_VOLS];
    FSRDevSpec      stDevSpec;

    nRet = samsung_ftl_init();
    if (nRet != FS_DEVICE_OK)
    {
        return FALSE32;
    }

    /* Register LLD Function Table */
    FSR_OAM_MEMSET(&stLFT[0], 0x00, sizeof(FSRLowFuncTbl));
    pstLFT[0] = &stLFT[0];
    pstLFT[1] = &stLFT[1];

    nRet = FSR_PAM_RegLFT(pstLFT);
    if (nRet != FSR_PAM_SUCCESS)
    {
        return FALSE32;
    }

    FSR_BML_AcquireSM(nVol);

    do
    {
        nRet = pstLFT[nVol]->LLD_GetDevSpec(nDev,
                                            &stDevSpec,
                                            FSR_LLD_FLAG_NONE);

        if (nRet == FSR_LLD_SUCCESS)
        {
            nDID = stDevSpec.nDID;
        }
    } while(0);

    FSR_BML_ReleaseSM(nVol);

    return nDID;
}

INT32
samsung_set_fs_rebuild(void)
{
#if !defined (FSR_NBL2)
    INT32   nRet;
    UINT32  nVol = 0;
    UINT32  nRebuilPage;
    FSRVolSpec  stVolSpec;

        /* Get volume information */
    nRet = FSR_BML_GetVolSpec(nVol,
                              &stVolSpec,
                              FSR_BML_FLAG_NONE);
    if (nRet != FSR_BML_SUCCESS)
    {
        return FS_DEVICE_FAIL;
    }

    /* The case that fs is located SLC Unit */
    nRebuilPage = (FS_REBUILD_UNIT + 1) * stVolSpec.nPgsPerSLCUnit - 1;

    nRet = samsung_bml_read(nRebuilPage * stVolSpec.nSctsPerPg,
                            stVolSpec.nSctsPerPg,
                            aMBuf,
                            NULL);

    if (nRet == FS_DEVICE_OK)
    {
        if((*(UINT32*) aMBuf     ) == FS_REBUILD_MAGIC1 &&
           (*(UINT32*)(aMBuf + 4)) == FS_REBUILD_MAGIC2)
        {
            samsung_smem_flash_spin_lock(FSR_SMEM_SPINLOCK_FS_REBUILD, FSR_SMEM_SPINLOCK_BASEADDR);

            /* set fs rebuild flag */
            *(UINT32*)FS_REBUILD_FLAG_POS += FS_REBUILD_FLAG;

            samsung_smem_flash_spin_unlock(FSR_SMEM_SPINLOCK_FS_REBUILD, FSR_SMEM_SPINLOCK_BASEADDR);
        }
        return FS_DEVICE_OK;
    }

    samsung_smem_flash_spin_lock(FSR_SMEM_SPINLOCK_FS_REBUILD, FSR_SMEM_SPINLOCK_BASEADDR);

    /* set fs rebuild flag */
    *(UINT32*)FS_REBUILD_FLAG_POS = 0x00;

    samsung_smem_flash_spin_unlock(FSR_SMEM_SPINLOCK_FS_REBUILD, FSR_SMEM_SPINLOCK_BASEADDR);

#endif
    return FS_DEVICE_OK;
}

INT32
samsung_reset_fs_rebuild(void)
{
#if !defined (FSR_NBL2)
    UINT32            nVol = 0;
    UINT32            nDevIdx = 0;
    INT32             nRet;
    UINT32            nRetByte;
    UINT32            nRebuilPage;
    UINT32            nPageOffset;
    FSRVolSpec        stVolSpec;
    FSRDevSpec        stDevSpec;
    UINT32            nErrPbn;
    FSRLowFuncTbl     stLFT[FSR_MAX_VOLS];
    FSRLowFuncTbl    *pstLFT[FSR_MAX_VOLS];
    LLDProtectionArg  stLLDIO;

    /* Get volume information */
    nRet = FSR_BML_GetVolSpec(nVol,
                              &stVolSpec,
                              FSR_BML_FLAG_NONE);
    if (nRet != FSR_BML_SUCCESS)
    {
        return FS_DEVICE_FAIL;
    }



    /* The case that fs is located SLC Unit */
    nRebuilPage = (FS_REBUILD_UNIT + 1) * stVolSpec.nPgsPerSLCUnit - 1;

    /* The case that fs is located SLC Unit */
    nRet = samsung_bml_read(nRebuilPage * stVolSpec.nSctsPerPg,
                            stVolSpec.nSctsPerPg,
                            aMBuf,
                            NULL);

    if (nRet != FS_DEVICE_OK)
    {
        /* after overprogramming the page which contains rebuild flag
           samsung_bml_read() returns read error
           so, ignore read error [namo.hwang 090202]
        */    
        return FS_DEVICE_OK;
    }



    if((*(UINT32*) aMBuf     ) != FS_REBUILD_MAGIC1 ||
       (*(UINT32*)(aMBuf + 4)) != FS_REBUILD_MAGIC2)
    {
        return FS_DEVICE_OK;
    }



    samsung_smem_flash_spin_lock(FSR_SMEM_SPINLOCK_FS_REBUILD, FSR_SMEM_SPINLOCK_BASEADDR);

#if !defined(USE_APPS_WM)
    if (*(UINT32*)FS_REBUILD_FLAG_POS != (3 * FS_REBUILD_FLAG))
    {
        *(UINT32*) FS_REBUILD_FLAG_POS += FS_REBUILD_FLAG;
        samsung_smem_flash_spin_unlock(FSR_SMEM_SPINLOCK_FS_REBUILD, FSR_SMEM_SPINLOCK_BASEADDR);
        return FS_DEVICE_OK;
    }
#endif



    do
    {
        /* Register LLD Function Table */
        FSR_OAM_MEMSET(&stLFT[0], 0x00, sizeof(FSRLowFuncTbl));
        pstLFT[0] = &stLFT[0];
        pstLFT[1] = &stLFT[1];

        nRet = FSR_PAM_RegLFT(pstLFT);
        if (nRet != FSR_PAM_SUCCESS)
        {
            nRet = FS_DEVICE_FAIL;
            break;
        }

        FSR_BML_AcquireSM(nVol);

        do
        {
            nRet = FSR_BML_FlushOp(nVol, FSR_BML_FLAG_NO_SEMAPHORE);
            if (nRet != FSR_BML_SUCCESS)
            {
                nRet = FS_DEVICE_FAIL;
                break;
            }

            /* LLD_GetDevSpec() access the device, 
               so, needs BML semaphore */
            nRet = pstLFT[nVol]->LLD_GetDevSpec(nDevIdx,
                                                &stDevSpec,
                                                FSR_LLD_FLAG_NONE);
            if (nRet != FSR_LLD_SUCCESS)
            {
                nRet = FS_DEVICE_FAIL;
                break;
            }

            /* change the block state to unlock state */
            stLLDIO.nStartBlk = (stDevSpec.nNumOfDies - 1) * stDevSpec.nNumOfBlksIn1stDie + FS_REBUILD_UNIT;
            stLLDIO.nBlks     = 1;

            nRet = pstLFT[nVol]->LLD_IOCtl(nDevIdx ,
                                           FSR_LLD_IOCTL_UNLOCK_BLOCK,
                                           (UINT8 *) &stLLDIO,
                                           sizeof(stLLDIO),
                                           (UINT8 *) &nErrPbn,
                                           sizeof(nErrPbn),
                                           &nRetByte);
            if (nRet != FSR_LLD_SUCCESS)
            {
                nRet = FS_DEVICE_FAIL;
                break;
            }
        } while(0);

        FSR_BML_ReleaseSM(nVol);

        if (nRet == FS_DEVICE_FAIL)
        {
            break;
        }




        FSR_OAM_MEMSET(aMBuf, 0x00, FSR_SECTOR_SIZE * stDevSpec.nSctsPerPG);

        nPageOffset = stDevSpec.nPgsPerBlkForSLC - 1;

        nRet = samsung_lld_write(stLLDIO.nStartBlk,
                                 nPageOffset,
                                 FSR_SECTOR_SIZE * stDevSpec.nSctsPerPG,
                                 aMBuf);
        if (nRet != FS_DEVICE_OK)
        {
            nRet = FS_DEVICE_FAIL;
            break;
        }



        FSR_BML_AcquireSM(nVol);
        do
        {
            nRet = FSR_BML_FlushOp(nVol, FSR_BML_FLAG_NO_SEMAPHORE);
            if (nRet != FSR_BML_SUCCESS)
            {
                nRet = FS_DEVICE_FAIL;
                break;
            }

            nRet = pstLFT[nVol]->LLD_IOCtl(nDevIdx ,
                                           FSR_LLD_IOCTL_LOCK_BLOCK,
                                           (UINT8 *) &stLLDIO,
                                           sizeof(stLLDIO),
                                           (UINT8 *) &nErrPbn,
                                           sizeof(nErrPbn),
                                           &nRetByte);
            if (nRet != FSR_LLD_SUCCESS)
            {
                nRet = FS_DEVICE_FAIL;
                break;
            }
        }while (0);

        FSR_BML_ReleaseSM(nVol);
    } while(0);

    samsung_smem_flash_spin_unlock(FSR_SMEM_SPINLOCK_FS_REBUILD, FSR_SMEM_SPINLOCK_BASEADDR);

#endif
    return FS_DEVICE_OK;
}

INT32
samsung_init_fs(void)
{
#if !defined (FSR_NBL2)
    UINT32         nVol = 0;
    UINT32         nDevIdx = 0;
    INT32          nRet;
    UINT32         nPbn;
    UINT32         nPageOffset;
    FSRDevSpec     stDevSpec;
    FSRLowFuncTbl  stLFT[FSR_MAX_VOLS];
    FSRLowFuncTbl *pstLFT[FSR_MAX_VOLS];

    nRet = samsung_ftl_init();
    if (nRet != FS_DEVICE_OK)
    {
       return FALSE32;
    }

    /* Register LLD Function Table */
    FSR_OAM_MEMSET(&stLFT[0], 0x00, sizeof(FSRLowFuncTbl));
    pstLFT[0] = &stLFT[0];
    pstLFT[1] = &stLFT[1];

    nRet = FSR_PAM_RegLFT(pstLFT);
    if (nRet != FSR_PAM_SUCCESS)
    {
        return  FS_DEVICE_FAIL;
    }

    nRet = pstLFT[nVol]->LLD_GetDevSpec(nDevIdx,
                                        &stDevSpec,
                                        FSR_LLD_FLAG_NONE);
    if (nRet != FSR_LLD_SUCCESS)
    {
        return  FS_DEVICE_FAIL;
    }

    nPbn = (stDevSpec.nNumOfDies - 1) * stDevSpec.nNumOfBlksIn1stDie + FS_REBUILD_UNIT;

    nPageOffset = stDevSpec.nPgsPerBlkForSLC - 1;

    FSR_OAM_MEMSET(aMBuf, 0xFF, FSR_SECTOR_SIZE * stDevSpec.nSctsPerPG);


    *(UINT32*) aMBuf      = FS_REBUILD_MAGIC1;
    *(UINT32*)(aMBuf + 4) = FS_REBUILD_MAGIC2;


    nRet = samsung_lld_write(nPbn, nPageOffset, FSR_SECTOR_SIZE * stDevSpec.nSctsPerPG, aMBuf);
    if (nRet != FS_DEVICE_OK)
    {
        return  FS_DEVICE_FAIL;
    }

    return FS_DEVICE_OK;
#else
    return FS_DEVICE_OK;
#endif
}

INT32
samsung_lld_write(UINT32 nPbn, UINT32 nPageOffset, UINT32 nSize, UINT8* pMBuf)
{
#if !defined (FSR_NBL2)
    INT32           nRet;
    INT32           nErr = FS_DEVICE_OK;
    UINT32          nVol = 0;
    UINT32          nDev = 0;
    FSRLowFuncTbl  *pstLFT[FSR_MAX_VOLS];
    FSRLowFuncTbl   stLFT[FSR_MAX_VOLS];
    FSRSpareBuf     stSpareBuf;
    FSRSpareBufBase stSpareBufBase;
    FSRSpareBufExt  stSpareBufExt[FSR_MAX_SPARE_BUF_EXT];
    FSRVolSpec      stVolSpec;
    UINT32          nSizeOfPage;
    UINT32          nNumOfPages;
    UINT32          nCnt;

    nRet = samsung_ftl_init();
    if (nRet != FS_DEVICE_OK)
    {
        return FS_DEVICE_FAIL;
    }

    /* Get volume information */
    nRet = FSR_BML_GetVolSpec(nVol, &stVolSpec, FSR_BML_FLAG_NONE);
    if (nRet != FSR_BML_SUCCESS)
    {
         return FS_DEVICE_FAIL;
    }

    nSizeOfPage = (stVolSpec.nSctsPerPg * FSR_SECTOR_SIZE);
    nNumOfPages = (nSize % nSizeOfPage) ? (nSize / nSizeOfPage + 1) : (nSize / nSizeOfPage);

    /* Acquire SM */
    FSR_BML_AcquireSM(nVol);

    do
    {
        nRet = FSR_BML_FlushOp(nVol, FSR_BML_FLAG_NO_SEMAPHORE);
        if (nRet != FSR_BML_SUCCESS)
        {
            nErr = FS_DEVICE_FAIL;
            break;
        }

        /* Register LLD Function Table */
        FSR_OAM_MEMSET(&stLFT[0], 0x00, sizeof(FSRLowFuncTbl));
        pstLFT[0] = &stLFT[0];
        pstLFT[1] = &stLFT[1];

        nRet = FSR_PAM_RegLFT(pstLFT);
        if (nRet != FSR_PAM_SUCCESS)
        {
            nErr = FS_DEVICE_FAIL;
            break;
        }

        /* Set spare buffer */
        FSR_OAM_MEMSET(&stSpareBufBase, 0xFF, sizeof(stSpareBufBase));
        FSR_OAM_MEMSET(&stSpareBufExt[0], 0xFF, sizeof(stSpareBufExt));

        stSpareBuf.pstSpareBufBase  = &stSpareBufBase;
        stSpareBuf.nNumOfMetaExt    = 2;
        stSpareBuf.pstSTLMetaExt    = &stSpareBufExt[0];

        for (nCnt = 0; nCnt < nNumOfPages; nCnt++)
        {
            /* if the pMBuf is smaller than page size */
            if ((nCnt == nNumOfPages -1) &&
                ((nSize % nSizeOfPage) != 0))
            {
                FSR_OAM_MEMSET(aMBuf, 0xFF, sizeof(aMBuf));
                FSR_OAM_MEMCPY(aMBuf, pMBuf, nSize % nSizeOfPage);
                pMBuf = aMBuf;
            }

            nRet = pstLFT[nVol]->LLD_Write(nDev,
                                           nPbn,
                                           nPageOffset + nCnt,
                                           pMBuf,
                                           &stSpareBuf,
                                           (FSR_LLD_FLAG_1X_PROGRAM | FSR_LLD_FLAG_ECC_ON | FSR_LLD_FLAG_USE_SPAREBUF));
            if (nRet != FSR_LLD_SUCCESS)
            {
                nErr = FS_DEVICE_FAIL;
                break;
            }
            nRet = pstLFT[nVol]->LLD_FlushOp(nDev,
                                             0,
                                             FSR_LLD_FLAG_NONE);
            if (nRet != FSR_LLD_SUCCESS)
            {
                nErr = FS_DEVICE_FAIL;
                break;
            }

            pMBuf += nSizeOfPage;
        }

    } while (0);

    /* Release SM */
    FSR_BML_ReleaseSM(nVol);

    return nErr;
#else
    return FS_DEVICE_OK;
#endif
}

INT32
samsung_dump_bootimg(UINT32 nID, UINT8* pBuf, UINT32* pDumpSize)
{
#if defined (BUILD_JNAND)
    INT32   nRet;
    UINT32  nVol = 0;
    UINT32  n1stVpn;
    UINT32  nPgsPerUnit;
    FSRPartEntry stPartEntry;
    UINT32  nUnitCnt;
    UINT32  nDumpSize;
    UINT8*  pTmpBuf;
    FSRVolSpec  stVolSpec;

    pTmpBuf = pBuf;
    nRet = samsung_ftl_init();
    if (nRet != FS_DEVICE_OK)
    {
        return FS_DEVICE_FAIL;
    }

    /* Get volume information */
    nRet = FSR_BML_GetVolSpec(nVol, &stVolSpec, FSR_BML_FLAG_NONE);
    if (nRet != FSR_BML_SUCCESS)
    {
         return FS_DEVICE_FAIL;
    }

    nRet = FSR_BML_LoadPIEntry(nVol, nID, &n1stVpn, &nPgsPerUnit,  &stPartEntry);
    if (nRet != FSR_BML_SUCCESS)
    {
        return FS_DEVICE_FAIL;
    }

    for (nUnitCnt = 0; nUnitCnt < stPartEntry.n1stVun + stPartEntry.nNumOfUnits; nUnitCnt++)
    {
        nRet = FSR_BML_Read(nVol, nUnitCnt * nPgsPerUnit, nPgsPerUnit, pTmpBuf, NULL, FSR_BML_FLAG_NONE);
        if (nRet != FSR_BML_SUCCESS)
        {
            return FS_DEVICE_FAIL;
        }

        pTmpBuf +=  nPgsPerUnit * stVolSpec.nSctsPerPg * FSR_SECTOR_SIZE;
    }


    nDumpSize = nUnitCnt * nPgsPerUnit * stVolSpec.nSctsPerPg * FSR_SECTOR_SIZE;
    *pDumpSize = nDumpSize;
#endif
    return FS_DEVICE_OK;
}

VOID
samsung_smem_init(void)
{
    static BOOL32 smem_Init_Flag = FALSE32;

    if(!smem_Init_Flag)
    {
        FSR_PAM_Init();
        FSR_OAM_Init();
        FSR_OAM_InitSharedMemory();
        smem_Init_Flag = TRUE32;
    }
}

INT32
samsung_block_count(void)
{
    INT32           nRet;
    FSRVolSpec  stVolSpec;
    UINT32          nVol = 0;

#if defined (BUILD_JNAND)
    FSR_PAM_InitNANDController();
#else
    nRet = samsung_ftl_init();
    if (nRet != FS_DEVICE_OK)
    {
        return FS_DEVICE_FAIL;
    }
#endif

    nRet = FSR_BML_GetVolSpec(nVol,
                              &stVolSpec,
                              FSR_BML_FLAG_NONE);
    if (nRet != FSR_BML_SUCCESS)
    {
        return FS_DEVICE_FAIL;
    }

    return stVolSpec.nNumOfUsUnits;
}

INT32
samsung_block_size(void)
{
    INT32           nRet;
    FSRVolSpec  stVolSpec;
    UINT32          nVol = 0;

#if defined (BUILD_JNAND)
    FSR_PAM_InitNANDController();
#else
    nRet = samsung_ftl_init();
    if (nRet != FS_DEVICE_OK)
    {
        return FS_DEVICE_FAIL;
    }
#endif

    nRet = FSR_BML_GetVolSpec(nVol,
                              &stVolSpec,
                              FSR_BML_FLAG_NONE);
    if (nRet != FSR_BML_SUCCESS)
    {
        return FS_DEVICE_FAIL;
    }

    return stVolSpec.nPgsPerSLCUnit;
}

INT32
samsung_page_size(void)
{
    INT32           nRet;
    FSRVolSpec  stVolSpec;
    UINT32          nVol = 0;

#if defined (BUILD_JNAND)
    FSR_PAM_InitNANDController();
#else
    nRet = samsung_ftl_init();
    if (nRet != FS_DEVICE_OK)
    {
        return FS_DEVICE_FAIL;
    }
#endif

    nRet = FSR_BML_GetVolSpec(nVol,
                              &stVolSpec,
                              FSR_BML_FLAG_NONE);
    if (nRet != FSR_BML_SUCCESS)
    {
        return FS_DEVICE_FAIL;
    }

    return stVolSpec.nSctsPerPg;
}

INT32
samsung_lld_read(UINT32 nPbn, UINT32 nPageOffset, UINT32 nSize, UINT8* pMBuf)
{
#if !defined (FSR_NBL2)
    INT32           nRet;
    INT32           nErr = FS_DEVICE_OK;
    UINT32          nVol = 0;
    UINT32          nDev = 0;
    FSRLowFuncTbl  *pstLFT[FSR_MAX_VOLS];
    FSRLowFuncTbl   stLFT[FSR_MAX_VOLS];
    FSRVolSpec      stVolSpec;
    UINT32          nSizeOfPage;

    nRet = samsung_ftl_init();
    if (nRet != FS_DEVICE_OK)
    {
        return FS_DEVICE_FAIL;
    }

    /* Get volume information */
    nRet = FSR_BML_GetVolSpec(nVol, &stVolSpec, FSR_BML_FLAG_NONE);
    if (nRet != FSR_BML_SUCCESS)
    {
         return FS_DEVICE_FAIL;
    }

    nSizeOfPage = (stVolSpec.nSctsPerPg * FSR_SECTOR_SIZE);

    /* if nSize is larger than page size, return error */
    if (nSize > nSizeOfPage)
    {
        return FS_DEVICE_FAIL;
    }

    /* Acquire SM */
    FSR_BML_AcquireSM(nVol);

    do
    {
        nRet = FSR_BML_FlushOp(nVol, FSR_BML_FLAG_NO_SEMAPHORE);
        if (nRet != FSR_BML_SUCCESS)
        {
            nErr = FS_DEVICE_FAIL;
            break;
        }

        /* Register LLD Function Table */
        FSR_OAM_MEMSET(&stLFT[0], 0x00, sizeof(FSRLowFuncTbl));
        pstLFT[0] = &stLFT[0];
        pstLFT[1] = &stLFT[1];

        nRet = FSR_PAM_RegLFT(pstLFT);
        if (nRet != FSR_PAM_SUCCESS)
        {
            nErr = FS_DEVICE_FAIL;
            break;
        }

        FSR_OAM_MEMSET(aMBuf, 0x00, sizeof(aMBuf));

        nRet = pstLFT[nVol]->LLD_Read(nDev,
                                      nPbn,
                                      nPageOffset,
                                      aMBuf,
                                      NULL,
                                      FSR_LLD_FLAG_NONE);
        if (nRet != FSR_LLD_SUCCESS)
        {
            nErr = FS_DEVICE_FAIL;
            break;
        }

        FSR_OAM_MEMCPY(pMBuf, aMBuf, nSize);
    } while (0);

    /* Release SM */
    FSR_BML_ReleaseSM(nVol);

    return nErr;
#else
    return FS_DEVICE_OK;
#endif
}

INT32
samsung_bml_get_vir_unit_info(UINT32        nVun,
                              UINT32       *pn1stVpn,
                              UINT32       *pnPgsPerUnit)
{
    UINT32          nVol = 0;
    INT32           nRet;

    nRet = FSR_BML_GetVirUnitInfo(nVol, nVun, pn1stVpn, pnPgsPerUnit);
    if (nRet == FSR_BML_SUCCESS)
    {
      return FS_DEVICE_OK;
    }
    else
    {
      return FS_DEVICE_FAIL;
    }
}

#if !defined(QCSBL) && !defined(APPSBL)
VOID
samsung_otp_read(UINT32 page, UINT8 *page_buf)
{
    FSRSpareBuf SBuf;
    FSRSpareBufExt  staSExt[FSR_MAX_SPARE_BUF_EXT];
    UINT32 nSctsPerPg;

    nSctsPerPg = samsung_page_size();

    SBuf.pstSpareBufBase = (FSRSpareBufBase *)(page_buf + nSctsPerPg * FSR_SECTOR_SIZE);
    SBuf.pstSTLMetaExt = NULL;
    SBuf.nNumOfMetaExt = 0;
    
    FSR_BML_OTPRead(0, page, 1, page_buf, &SBuf, FSR_BML_FLAG_ECC_OFF|FSR_BML_FLAG_USE_SPAREBUF);
}
#endif

VOID
samsung_smem_flash_spin_lock(UINT32 nIdx, UINT32 nBaseAddr)
{
    smem_flash_spin_lock(nIdx, nBaseAddr);
}

VOID
samsung_smem_flash_spin_unlock(UINT32 nIdx, UINT32 nBaseAddr)
{
    smem_flash_spin_unlock(nIdx, nBaseAddr);
}

