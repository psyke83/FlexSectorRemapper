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
 *  @file       FSR_STL_Common.h
 *  @brief      This header file defines common functions in STL.
 *  @author     Wonmoon Cheon
 *  @date       02-MAR-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  02-MAR-2007 [Wonmoon Cheon] : first writing
 *  @n  26-JUN-2007 [Wonmoon Cheon] : re-design for v1.2.0
 *  @n  29-JAN-2008 [MinYoung Kim] : dead code elimination
 *
 */

#ifndef __FSR_STL_COMMON_H__
#define __FSR_STL_COMMON_H__

/*****************************************************************************/
/* Header file inclusions                                                    */
/*****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus*/

/*****************************************************************************/
/* Macro                                                                     */
/*****************************************************************************/
#define GET_META_PAGE_SIZE(_METAVER)        (((_METAVER) >> 8) & 0x000000FF)

#define GET_REMAINDER(_DIV, _BITSHIFT)      ((_DIV) & ((1 << (_BITSHIFT)) - 1))

/* 
* Get OneNand Spare Index to support 42nm.
* 2KB : nIdx(0,2)      ==> nSpareIdx(0,1)
* 4KB : nIdx(0,2,4,6)  ==> nSpareIdx(0,1,4,5)
* ex) 2 ==> ((0>>2) * 4) + ((1% 4) >> 1) ==> 1
* ex) 6 ==> ((6>>2) * 4) + ((6% 4)  >> 1) ==> 5
*/
#define GET_ONENAND_SPARE_IDX(nIdx)         ((nIdx >> 2) * 4) + ((nIdx % 4) >> 1)

/* check if the CRC in a log page is valid or not */
#define IS_LOG_CRC_VALID(ptf)   ( \
                ((ptf) & 0xf0) == 0x10 && \
                ( (((ptf) & 0xf00) >> 8) == ENDLSB || \
                (((ptf) & 0xf00) >> 8) == ENDMSB) )

/* check if it is buffer block page or not */
#define IS_BU_PAGE(zone, ptf)     ( \
                ((ptf) & 0xf0) == 0x20 && \
                ((ptf) & 0xf) == (zone)->nZoneID )


/* all 0xFF page's CRC value */
#define STL_DELETED_SCT_CRC_VALUE           0xbd7bc39f

/*****************************************************************************/
/* Type defines                                                              */
/*****************************************************************************/

/*****************************************************************************/
/* Constant definitions                                                      */
/*****************************************************************************/

/*****************************************************************************/
/* Exported variable declarations                                            */
/*****************************************************************************/

/*****************************************************************************/
/* Global variable definition                                                */
/*****************************************************************************/
extern PUBLIC STLPartObj      gstSTLPartObj[FSR_MAX_VOLS][FSR_MAX_STL_PARTITIONS];
extern PUBLIC STLClstObj     *gpstSTLClstObj[MAX_NUM_CLUSTERS];

extern PUBLIC const UINT32 	  CRC32_Table[4][256];

/*****************************************************************************/
/* Exported function prototype                                               */
/*****************************************************************************/

/*---------------------------------------------------------------------------*/
/* FSR_STL_Init.c                                                            */

PUBLIC VOID     FSR_STL_InitVFLParamPool   (STLClstObj     *pstClst);

PUBLIC VOID     FSR_STL_SetDirHdrHdl       (STLZoneObj     *pstZone,
                                            STLDirHdrHdl   *pstDH,
                                            UINT8          *pBuf,
                                            UINT32          nBufSize);

PUBLIC VOID     FSR_STL_SetCtxInfoHdl      (STLZoneObj     *pstZone,
                                            STLCtxInfoHdl  *pstCtx,
                                            UINT8          *pBuf,
                                            UINT32          nBufSize);

PUBLIC VOID     FSR_STL_SetBMTHdl          (STLZoneObj     *pstZone,
                                            STLBMTHdl      *pstBMT,
                                            UINT8          *pBuf,
                                            UINT32          nBufSize);

PUBLIC VOID     FSR_STL_SetLogGrpHdl       (STLZoneObj     *pstZone,
                                            STLLogGrpHdl   *pstLogGrp,
                                            UINT8          *pBuf,
                                            UINT32          nBufSize);

PUBLIC VOID     FSR_STL_SetPMTHdl          (STLZoneObj     *pstZone,
                                            STLPMTHdl      *pstPMT,
                                            UINT8          *pBuf,
                                            UINT32          nBufSize);

PUBLIC VOID     FSR_STL_InitDirHdr         (STLZoneObj     *pstZone,
                                            STLDirHdrHdl   *pstDH);

PUBLIC VOID     FSR_STL_InitCtx            (STLZoneObj     *pstZone,
                                            STLCtxInfoHdl  *pstCtx);

PUBLIC VOID     FSR_STL_InitBMT            (STLZoneObj     *pstZone,
                                            STLBMTHdl      *pstBMT,
                                            BADDR           nLan);

PUBLIC VOID     FSR_STL_InitBUCtx          (STLBUCtxObj    *pstBUCtx);

PUBLIC VOID     FSR_STL_InitDelCtx         (STLDelCtxObj   *pstDelCtx);

PUBLIC VOID     FSR_STL_InitLog            (STLZoneObj     *pstZone,
                                            STLLogGrpHdl   *pstLogGrp,
                                            STLLog         *pstLog);

PUBLIC VOID     FSR_STL_InitLogGrp         (STLZoneObj     *pstZone, 
                                            STLLogGrpHdl   *pstLogGrp);

PUBLIC VOID     FSR_STL_InitLogGrpList     (STLLogGrpList  *pstLogGrpList);

PUBLIC INT32    FSR_STL_InitRootInfo       (RBWDevInfo     *pstDev,
                                            STLRootInfo    *pstRootInfo);

PUBLIC INT32    FSR_STL_InitMetaLayout     (RBWDevInfo     *pstDev,
                                            STLRootInfo    *pstRI,
                                            STLMetaLayout  *pstML);

PUBLIC STLClstObj*  FSR_STL_GetClstObj     (UINT32          nClstID);

/*---------------------------------------------------------------------------*/
/* FSR_STL_ZoneMgr.c                                                         */

PUBLIC VOID     FSR_STL_ResetZoneObj       (UINT32          nVolID,
                                            RBWDevInfo     *pstDevInfo,
                                            STLRootInfo    *pstRI,
                                            STLMetaLayout  *pstML,
                                            STLZoneObj     *pstZone);

PUBLIC INT32    FSR_STL_AllocZoneMem       (STLZoneObj     *pstZone);

PUBLIC VOID     FSR_STL_FreeZoneMem        (STLZoneObj     *pstZone);

PUBLIC VOID     FSR_STL_InitZoneInfo       (STLZoneInfo    *pstZI);

PUBLIC INT32    FSR_STL_SetupZoneInfo      (STLClstObj     *pstClst,
                                            STLZoneObj     *pstZone,
                                            UINT16          nStartVbn,
                                            UINT16          nNumBlks);

PUBLIC VOID     FSR_STL_InitZone           (STLZoneObj     *pstZone);


/*---------------------------------------------------------------------------*/
/* FSR_STL_Format.c                                                          */

/*---------------------------------------------------------------------------*/
/* FSR_STL_Open.c                                                            */

/*---------------------------------------------------------------------------*/
/* FSR_STL_Read.c                                                            */

/*---------------------------------------------------------------------------*/
/* FSR_STL_Write.c                                                           */

PUBLIC INT32    FSR_STL_MakeRoomForActLogGrpList(STLZoneObj    *pstZone,
                                                BADDR           nSelfDgn,
                                                BOOL32          bEntireLogs);

PUBLIC VOID     FSR_STL_InsertIntoInaLogGrpCache(STLZoneObj     *pstZone,
                                                STLLogGrpHdl    *pstLogGrp);

PUBLIC STLLogGrpHdl*   FSR_STL_NewActLogGrp    (STLZoneObj     *pstZone,
                                                BADDR           nDgn);

PUBLIC STLLogGrpHdl*  FSR_STL_PrepareActLGrp   (STLZoneObj     *pstZone, 
                                                PADDR           nLpn);

PUBLIC INT32    FSR_STL_Convert_ExtParam       (STLZoneObj     *pstZone,
                                                VFLParam       *pstParam);

PUBLIC VOID     FSR_STL_InitCRCs               (STLZoneObj     *pstZone,
                                                VFLParam       *pstParam);

PUBLIC VOID     FSR_STL_ComputeCRCs            (STLZoneObj     *pstZone,
                                                VFLParam       *pstSrcParam,
                                                VFLParam       *pstDstParam,
                                                BOOL32          bCRCValid);

PUBLIC VOID     FSR_STL_SetLogSData123         (STLZoneObj     *pstZone,
                                                PADDR           nLpn,
                                                PADDR           nDstVpn,
                                                VFLParam       *pstParam,
                                                BOOL32          bConfirmPage);

PUBLIC INT32    FSR_STL_FlashModiCopybackCRC   (STLZoneObj     *pstZone,
                                                PADDR           nSrcVpn,
                                                PADDR           nDstVpn,
                                                VFLParam       *pstParam);

PUBLIC INT32    FSR_STL_FlashCopybackCRC       (STLZoneObj     *pstZone,
                                                PADDR           nLpn,
                                                PADDR           nSrcVpn,
                                                PADDR           nDstVpn,
                                                SBITMAP         nBitmap);

PUBLIC BOOL32   FSR_STL_CheckFullInaLogs       (STLLogGrpHdl   *pstLogGrp);

PUBLIC INT32    FSR_STL_WriteSinglePage        (STLZoneObj     *pstZone,
                                                STLLogGrpHdl   *pstLogGrp,
                                                STLLog         *pstLog,
                                                VFLParam       *pstParam, 
                                                UINT32          nTrTotalSize,
                                                BOOL32          bConfirmPage);

PUBLIC INT32    FSR_STL_GetLogToWrite          (STLZoneObj     *pstZone,
                                                PADDR           nLpn,
                                                STLLogGrpHdl   *pstTargetGrp,
                                                STLLog        **pstTargetLog,
                                                UINT32         *pnNumAvailPgs);

/*---------------------------------------------------------------------------*/
/* FSR_STL_Delete.c                                                          */

PUBLIC INT32    FSR_STL_StoreDeletedInfo       (STLZoneObj     *pstZone);

/*---------------------------------------------------------------------------*/
/* FSR_STL_MetaMgr.c                                                         */

PUBLIC INT32    FSR_STL_StoreRootInfo          (STLClstObj     *pstClst);

PUBLIC INT32    FSR_STL_StoreDirHeader         (STLZoneObj     *pstZone,
                                                BOOL32          bEnableMetaWL, 
                                                BOOL32         *pbWearLevel);

PUBLIC INT32    FSR_STL_StoreBMTCtx            (STLZoneObj     *pstZone,
                                                BOOL32          bEnableMetaWL);

PUBLIC INT32    FSR_STL_LoadBMT                (STLZoneObj     *pstZone,
                                                BADDR           nLan,
                                                BOOL32          bOpenFlag);

PUBLIC INT32    FSR_STL_StorePMTCtx            (STLZoneObj     *pstZone,
                                                STLLogGrpHdl   *pstLogGrp,
                                                BOOL32          bEnableMetaWL);

PUBLIC INT32    FSR_STL_LoadPMT                (STLZoneObj     *pstZone,
                                                BADDR           nDgn,
                                                POFFSET         nMetaPOffs,
                                                STLLogGrpHdl  **pstLogGrp,
                                                BOOL32          bOpenFlag);

 PUBLIC INT32   FSR_STL_ReclaimMeta            (STLZoneObj     *pstZone,
                                                BADDR           nVbn,
                                                UINT16          nDstBlkOffset,
                                                UINT32          nVictimIdx,
                                                UINT16          nMinPgCnt,
                                                BOOL32          bReserveMeta);

PUBLIC INT32    FSR_STL_ProgramHeader          (STLZoneObj     *pstZone,
                                                BADDR           nHeaderVbn);

PUBLIC INT32    FSR_STL_ReadHeader             (STLZoneObj     *pstZone,
                                                BADDR           nHeaderVbn);

PUBLIC INT32    FSR_STL_ReserveMetaPgs         (STLZoneObj     *pstZone, 
                                                UINT16          nNCleanPgs,
                                                BOOL32          bEnableMetaWL);


/*---------------------------------------------------------------------------*/
/* FSR_STL_MappingMgr.c                                                      */

PUBLIC POFFSET  FSR_STL_GetWayCPOffset         (RBWDevInfo     *pstDev,
                                                POFFSET         nLogCPOffset,
                                                PADDR           nLpn);

PUBLIC INT32    FSR_STL_SearchPMTDir           (STLZoneObj     *pstZone,
                                                BADDR           nDgn,
                                                POFFSET        *pnMetaPOffs);

PUBLIC INT32    FSR_STL_SearchMetaPg           (STLZoneObj     *pstZone,
                                                BADDR           nDgn,
                                                POFFSET        *pnMetaPOffs);

PUBLIC VOID     FSR_STL_UpdatePMTDir           (STLZoneObj     *pstZone,
                                                BADDR           nDgn,
                                                POFFSET         nMetaPOffs,
                                                UINT32          nMinEC,
                                                BADDR           nMinVbn,
                                                BOOL32          bOpenFlag);

PUBLIC VOID     FSR_STL_UpdatePMTDirMinECGrp   (STLZoneObj     *pstZone,
                                                STLDirHdrHdl   *pstDH);

PUBLIC INT32    FSR_STL_CheckMergeVictimGrp    (STLZoneObj     *pstZone,
                                                STLLogGrpHdl   *pstLogGrp,
                                                BOOL32          bOpenFlag,
                                                BOOL32         *pbVictim);

PUBLIC BOOL32   FSR_STL_IsMergeVictimGrp       (STLZoneObj     *pstZone,
                                                STLLogGrpHdl   *pstLogGrp);

PUBLIC VOID     FSR_STL_SetMergeVictimGrp      (STLZoneObj     *pstZone,
                                                BADDR           nDgn,
                                                BOOL32          bSet);

PUBLIC BOOL32   FSR_STL_SearchMergeVictimGrp   (STLZoneObj     *pstZone,
                                                BADDR          *pnDgn,
                                                UINT32         *pnDirIdx);

PUBLIC STLLogGrpHdl   *FSR_STL_SearchLogGrp    (STLLogGrpList  *pstLogGrpList,
                                                BADDR           nDgn);

PUBLIC STLLogGrpHdl   *FSR_STL_AllocNewLogGrp  (STLZoneObj     *pstZone,
                                                STLLogGrpList  *pstLogGrpList);

PUBLIC VOID     FSR_STL_AddLogGrp              (STLLogGrpList  *pstLogGrpList,
                                                STLLogGrpHdl   *pstLogGrp);

PUBLIC VOID     FSR_STL_MoveLogGrp2Head        (STLLogGrpList  *pstLogGrpList,
                                                STLLogGrpHdl   *pstLogGrp);

PUBLIC VOID     FSR_STL_RemoveLogGrp           (STLZoneObj     *pstZone,
                                                STLLogGrpList  *pstLogGrpList,
                                                STLLogGrpHdl   *pstLogGrp);

#if (OP_SUPPORT_CLOSE_TIME_UPDATE_FOR_FASTER_OPEN == 1)
PUBLIC STLLogGrpHdl   *FSR_STL_SelectVictimLogGrp (STLLogGrpList *pstLogGrpList,
                                                BADDR           nSelfDgn);

PUBLIC VOID     FSR_STL_FlushAllMetaInfo       (STLZoneObj     *pstZone);

#endif /*OP_SUPPORT_CLOSE_TIME_UPDATE_FOR_FASTER_OPEN*/

PUBLIC STLLog   *FSR_STL_AddNewLog             (STLZoneObj     *pstZone,
                                                STLLogGrpHdl   *pstLogGrp,
                                                BADDR           nLbn,
                                                BADDR           nVbn,
                                                UINT32          nEC);

PUBLIC BOOL32   FSR_STL_SearchLog              (STLLogGrpHdl   *pstLogGrp,
                                                BADDR           nVbn,
                                                STLLog        **pstLog);

PUBLIC VOID     FSR_STL_RemoveLog              (STLZoneObj     *pstZone,
                                                STLLogGrpHdl   *pstLogGrp,
                                                STLLog         *pstLog);

PUBLIC STLLog   *FSR_STL_SelectVictimLog       (STLLogGrpHdl   *pstLogGrp,
                                                BOOL32          bActiveLog);

PUBLIC VOID     FSR_STL_ChangeLogState         (STLZoneObj     *pstZone,
                                                STLLogGrpHdl   *pstLogGrp,
                                                STLLog         *pstLog,
                                                PADDR           nLpn);

PUBLIC STLLog   *FSR_STL_FindAvailLog          (STLZoneObj     *pstZone,
                                                STLLogGrpHdl   *pstLogGrp,
                                                PADDR           nLpn,
                                                UINT32         *pnNumAvailPgs);

PUBLIC VOID     FSR_STL_AddActLogList          (STLZoneObj     *pstZone,
                                                BADDR           nDgn,
                                                STLLog         *pstLog);

PUBLIC VOID     FSR_STL_RemoveActLogList       (STLZoneObj     *pstZone,
                                                BADDR           nDgn, 
                                                STLLog         *pstLog);

PUBLIC INT32    FSR_STL_GetLpn2Vpn             (STLZoneObj     *pstZone,
                                                PADDR           nLpn,
                                                PADDR          *pnVpn,
                                                BOOL32         *pbDeleted);

PUBLIC PADDR    FSR_STL_GetLogCPN              (STLZoneObj     *pstZone,
                                                STLLog         *pstLog,
                                                PADDR           nLpn);

PUBLIC VOID     FSR_STL_UpdatePMT              (STLZoneObj     *pstZone,
                                                STLLogGrpHdl   *pstLogGrp,
                                                STLLog         *pstLog,
                                                PADDR           nLpn,
                                                PADDR           nVpn);

PUBLIC VOID     FSR_STL_UpdatePMTCost          (STLZoneObj     *pstZone,
                                                STLLogGrpHdl   *pstLogGrp);

#if (OP_SUPPORT_MLC_LSB_ONLY == 1)
PUBLIC VOID     FSR_STL_CheckMLCFastMode       (STLZoneObj     *pstZone,
                                                STLLog         *pstLog);
#endif  /* (OP_SUPPORT_MLC_LSB_ONLY == 1) */

/*---------------------------------------------------------------------------*/
/* FSR_STL_FreeBlkMgr.c                                                      */

PUBLIC BOOL32   FSR_STL_CheckFullFreeList      (STLZoneObj     *pstZone);

PUBLIC VOID     FSR_STL_AddFreeList            (STLZoneObj     *pstZone,
                                                BADDR           nVbn,
                                                UINT32          nEC);

PUBLIC INT32    FSR_STL_ReserveFreeBlks        (STLZoneObj     *pstZone,
                                                BADDR           nDgn,
                                                UINT32          nNumRsvdFBlks,
                                                UINT32         *pnNumRsvd);

PUBLIC INT32    FSR_STL_ReserveYFreeBlks       (STLZoneObj     *pstZone,
                                                BADDR           nDgn,
                                                UINT32          nNumRsvdFBlks,
                                                UINT32         *pnNumRsvd);

PUBLIC INT32    FSR_STL_GetFreeBlk             (STLZoneObj     *pstZone,
                                                BADDR          *pnVbn,
                                                UINT32         *pnEC);

/*---------------------------------------------------------------------------*/
/* FSR_STL_MergeMgr.c                                                        */

PUBLIC INT32    FSR_STL_StoreMapInfo           (STLZoneObj     *pstZone,
                                                STLLogGrpHdl   *pstLogGrp,
                                                STLLog         *pstLog,
                                                BOOL32          bMergeFlag);

PUBLIC INT32    FSR_STL_CompactLog             (STLZoneObj     *pstZone,
                                                STLLogGrpHdl   *pstLogGrp,
                                                STLLog        **pstLog);

PUBLIC BOOL32   FSR_STL_CheckCopyCompaction    (STLZoneObj     *pstZone,
                                                STLLogGrpHdl   *pstLogGrp,
                                                UINT8          *pnSrcLogIdx,
                                                UINT8          *pnDstLogIdx,
                                                BOOL32          bActiveDstOnly);

PUBLIC INT32    FSR_STL_CheckInaLogGrpCompaction(STLZoneObj     *pstZone,
                                                STLDirHdrHdl   *pstDirHdr,
                                                UINT32          nDirIdx,
                                                BOOL32         *pbCompaction,
                                                UINT8          *nSrcLogIdx,
                                                UINT8          *nDstLogIdx);

PUBLIC INT32    FSR_STL_CompactActLogGrp       (STLZoneObj     *pstZone,
                                                STLLogGrpHdl   *pstLogGrp,
                                                UINT8           nSrcLogIdx,
                                                UINT8           nDstLogIdx);

/*---------------------------------------------------------------------------*/
/* FSR_STL_WearLevelMgr.c                                                    */

PUBLIC VOID     FSR_STL_UpdateBMTMinEC         (STLZoneObj     *pstZone);

PUBLIC INT32    FSR_STL_UpdatePMTMinEC         (STLZoneObj     *pstZone,
                                                STLLogGrpHdl   *pstLogGrp,
                                                UINT32          nRemovalVbn);

PUBLIC INT32    FSR_STL_WearLevelFreeBlk       (STLZoneObj     *pstZone,
                                                UINT32          nNumBlks,
                                                BADDR           nRcvDgn);

PUBLIC INT32    FSR_STL_RecoverGWLInfo         (STLZoneObj     *pstZone);

PUBLIC INT32    FSR_STL_GetAllBlksEC           (STLZoneObj     *pstZone,
                                                UINT32         *paBlkEC,
                                                UINT32         *pRootBlksECNT,
                                                UINT32         *pZoneBlksECNT,
                                                UINT32          nNumBlks);

PUBLIC UINT32   FSR_STL_FindMinEC              (STLZoneObj     *pstZone);

#if (OP_SUPPORT_STATISTICS_INFO == 1)
PUBLIC INT32    FSR_STL_GetTotalECinCluster    (STLPartObj     *pstSTLPartObj,
                                                UINT32         *nTotalECnt,
                                                UINT32          nNumBlks);
#endif

#if (0)
PUBLIC INT32    FSR_STL_ForceMetaWearLevel     (STLZoneObj     *pstZone);
#endif


/*-------------------------------------------------------------------------------*/
/* FSR_STL_GCMgr.c                                                               */

PUBLIC INT32    FSR_STL_GC                     (STLZoneObj     *pstZone);

PUBLIC INT32    FSR_STL_ReverseGC              (STLZoneObj     *pstZone,
                                                UINT32          nFreeSlots);

PUBLIC INT32    FSR_STL_UpdateGCScanBitmap     (STLZoneObj     *pstZone,
                                                BADDR           nLan,
                                                BOOL32          bSet);


/*---------------------------------------------------------------------------*/
/* FSR_STL_FlashIO.c                                                         */

PUBLIC INT32    FSR_STL_FlashRead              (STLZoneObj     *pstZone,
                                                UINT32          nVpn,
                                                VFLParam       *pstParam,
                                                BOOL32          bCleanCheck);

PUBLIC INT32    FSR_STL_FlashReliableRead      (STLZoneObj     *pstZone,
                                                UINT32          nVpn,
                                                VFLParam       *pstParam);

PUBLIC INT32    FSR_STL_FlashCheckRead         (STLZoneObj     *pstZone,
                                                const UINT32    nVpn,
                                                VFLParam       *pstParam,
                                                const UINT32    nRetryCnt,
                                                const BOOL32    bReadRefresh);

PUBLIC INT32    FSR_STL_FlashProgram           (STLZoneObj     *pstZone,
                                                UINT32          nVpn,
                                                VFLParam       *pstParam);

PUBLIC INT32    FSR_STL_FlashErase             (STLZoneObj     *pstZone,
                                                UINT32          nVbn);

PUBLIC INT32    FSR_STL_FlashCopyback          (STLZoneObj     *pstZone,
                                                UINT32          nSrcVpn,
                                                UINT32          nDstVpn,
                                                UINT32          nSBitmap);

PUBLIC INT32    FSR_STL_FlashModiCopyback      (STLZoneObj     *pstZone,
                                                UINT32          nSrcVpn,
                                                UINT32          nDstVpn,
                                                VFLParam       *pstParam);

PUBLIC INT32    FSR_STL_FlashFlush             (STLZoneObj     *pstZone);

PUBLIC INT32    FSR_STL_FlashRun               (STLZoneObj     *pstZone);

PUBLIC BOOL32   FSR_STL_FlashIsLSBPage         (STLZoneObj     *pstZone,
                                                UINT32          nVpn);

PUBLIC BOOL32   FSR_STL_FlashIsCleanPage       (STLZoneObj     *pstZone,
                                                VFLParam       *pstParam);


/*---------------------------------------------------------------------------*/
/* FSR_STL_Common.c                                                          */

PUBLIC VFLParam*    FSR_STL_AllocVFLParam      (STLZoneObj     *pstZone);

PUBLIC VOID     FSR_STL_FreeVFLParam           (STLZoneObj     *pstZone,
                                                VFLParam       *pstVFLParam);

PUBLIC VFLExtParam* FSR_STL_AllocVFLExtParam   (STLZoneObj     *pstZone);

PUBLIC VOID     FSR_STL_FreeVFLExtParam        (STLZoneObj     *pstZone,
                                                VFLExtParam    *pstVFLExtParam);

PUBLIC SBITMAP  FSR_STL_MarkSBitmap            (SBITMAP         nSrcBitmap);

PUBLIC SBITMAP  FSR_STL_SetSBitmap             (UINT32          nLsn,
                                                UINT32          nSectors,
                                                UINT32          nSecPerVPg);

PUBLIC INT32    FSR_STL_CheckArguments         (UINT32          nClstID,
                                                UINT32          nZoneID,
                                                UINT32          nLsn,
                                                UINT32          nNumOfScts,
                                                UINT8          *pBuf);

PUBLIC UINT32   FSR_STL_GetShiftBit            (UINT32          nVal);

PUBLIC UINT32   FSR_STL_GetZBC                 (UINT8          *pBuf,
                                                UINT32          nBufSize);

PUBLIC UINT32   FSR_STL_CalcCRC32              (const UINT8    *pBuf,
                                                UINT32          nSize);

PUBLIC UINT16   FSR_STL_SetPTF                 (STLZoneObj     *pstZone,
                                                UINT8           nTypeField, 
                                                UINT8           nMetaType,
                                                UINT8           nLSBOrMSB,
                                                UINT8           nSctCnt);

PUBLIC VOID     FSR_STL_SetRndInArg            (VFLExtParam    *pExtParam,
                                                UINT16          nOffset,
                                                UINT16          nNumOfBytes,
                                                VOID           *pBuf,
                                                UINT16          nIdx);

#if (OP_STL_DEBUG_CODE == 1)
PUBLIC VOID     FSR_STL_DBG_CheckValidity      (STLZoneObj     *pstZone);
PUBLIC VOID     FSR_STL_DBG_CheckValidPgCnt    (STLZoneObj     *pstZone);
PUBLIC VOID     FSR_STL_DBG_CheckMetaInfo      (STLZoneObj     *pstZone);
PUBLIC VOID     FSR_STL_DBG_CheckMergeVictim   (STLZoneObj     *pstZone);
PUBLIC VOID     FSR_STL_DBG_CheckFreeList      (STLZoneObj     *pstZone);
PUBLIC VOID     FSR_STL_DBG_PMTHistoryLog      (STLZoneObj     *pstZone,
                                                BADDR           nDgn);
PUBLIC VOID     FSR_STL_GetPTF                 (STLZoneObj     *pstZone,
                                                UINT32          nVbn);
PUBLIC BOOL32   FSR_STL_GetVbn                 (STLZoneObj     *pstZone,
                                                UINT32          nVbn,
                                                UINT32         *pnBlkCnt);
PUBLIC VOID     FSR_STL_DBG_CheckWLGrp         (STLZoneObj     *pstZone);
#endif


/*---------------------------------------------------------------------------*/
/* FSR_STL_BufferBlkMgr.c                                                    */

PUBLIC BOOL32   FSR_STL_DetermineBufferWrite   (STLZoneObj     *pstZone,
                                                STLLog         *pstLog,
                                                VFLParam       *pstParam,
                                                UINT32          nTrTotalSize);

PUBLIC INT32    FSR_STL_FlushBufferPage        (STLZoneObj     *pstZone);

PUBLIC INT32    FSR_STL_WriteSinglePage        (STLZoneObj     *pstZone,
                                                STLLogGrpHdl   *pstLogGrp,
                                                STLLog         *pstLog,
                                                VFLParam       *pstParam,
                                                UINT32          nTrTotalSize,
                                                BOOL32          bConfirmPage);


/*---------------------------------------------------------------------------*/
/* FSR_STL_Open.c                                                            */
#if (OP_SUPPORT_RUNTIME_PMT_BUILDING == 1)
PUBLIC INT32    FSR_STL_ScanActLogBlock        (STLZoneObj     *pstZone,
                                                BADDR nDgn);
#endif  /*OP_SUPPORT_RUNTIME_PMT_BUILDING*/


/*---------------------------------------------------------------------------*/
/* FSR_STL_WriteBufferMgr.c                                                  */

#if (OP_SUPPORT_WRITE_BUFFER == 1)
PUBLIC INT32    FSR_STL_OpenWB                  (UINT32         nVol,
                                                 UINT32         nPart,
                                                 UINT32         nFlag);

PUBLIC INT32    FSR_STL_CloseWB                 (UINT32         nVol,
                                                 UINT32         nPart);

PUBLIC INT32    FSR_STL_FormatWB                (UINT32         nVol,
                                                 UINT32         nPart);

PUBLIC INT32    FSR_STL_ReadWB                  (UINT32         nVol,
                                                 UINT32         nPart,
                                                 UINT32         nLsn,
                                                 UINT32         nNumOfScts,
                                                 UINT8         *pBuf,
                                                 UINT32         nFlag);

PUBLIC INT32    FSR_STL_WriteWB                 (UINT32         nVol,
                                                 UINT32         nPart,
                                                 UINT32         nLsn,
                                                 UINT32         nNumOfScts,
                                                 UINT8         *pBuf,
                                                 UINT32         nFlag);

PUBLIC INT32    FSR_STL_DeleteWB                (UINT32         nVol,
                                                 UINT32         nPart,
                                                 UINT32         nLsn,
                                                 UINT32         nNumOfScts,
                                                 BOOL32         bSyncMode,
                                                 UINT32         nFlag);

PUBLIC INT32    FSR_STL_IsInWB                  (UINT32         nVol,
                                                 UINT32         nPart,
                                                 UINT32         nLsn,
                                                 UINT32         nNumOfScts,
                                                 BOOL32        *pnNumOfSctsInWB);

PUBLIC INT32    FSR_STL_FlushWB                 (UINT32         nVol,
                                                 UINT32         nPart,
                                                 UINT32         nFreeRatio,
                                                 UINT32         nFlag);


/*---------------------------------------------------------------------------*/
/* FSR_STL_Interface.c                                                       */

PUBLIC UINT32   FSR_STL_GetZoneLsn              (STLPartObj     *pstSTLPart,
                                                 const UINT32    nLsn,
                                                       UINT32   *nZoneIdx);
PUBLIC  INT32   FSR_STL_ChangePartAttr          (UINT32          nVol,
                                                 UINT32          nPartID,
                                                 UINT32          nAttr);

#endif  /* #if (OP_SUPPORT_WRITE_BUFFER == 1) */

#ifdef __cplusplus
}
#endif /* __cplusplus*/

#endif /* __FSR_STL_COMMON_H__*/
