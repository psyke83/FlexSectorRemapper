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
 *  @file       FSR_STL_Types.h
 *  @brief      Definition for ATL data structures.
 *  @author     Wonmoon Cheon
 *  @date       02-MAR-2007
 *  @remark
 *  REVISION HISTORY
 *  @n  02-MAR-2007 [Wonmoon Cheon] : first writing
 *  @n  22-JUN-2007 [Wonmoon Cheon] : re-design for v1.2.0 
 *
 */

#ifndef __FSR_STL_TYPES_H__
#define __FSR_STL_TYPES_H__

/*****************************************************************************/
/* Header file inclusions                                                    */
/*****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus*/

/*****************************************************************************/
/* Macro                                                                     */
/*****************************************************************************/
/* STL meta format version*/
#define STL_META_VER_2K         0x12000200           /**< v1.2.0 2K format  */
#define STL_META_VER_4K         0x12000400           /**< v1.2.0 4K format  */
#define STL_META_VER_8K         0x12000800           /**< v1.2.0 8K format  */
#define STL_META_VER_16K        0x12001000           /**< v1.2.0 16K format */

/* Log block state (Log.nState)*/
#define LS_FREE                 0x00    /**< free state                     */
#define LS_ALLOC                0x01    /**< allocated state                */
#define LS_SEQ                  0x02    /**< sequential state               */
#define LS_RANDOM               0x03    /**< random state                   */
#define LS_GARBAGE              0x04    /**< garbage log state              */

#define LS_STATES_BIT_MASK      0x0F    /**< lower 4bits : state            */
#define LS_ACTIVE_SHIFT         0x04    /**< active state (bit shift)       */
#define LS_MLC_FAST_MODE_SHIFT  0x05    /**< MLC fast mode flag (bit shift) */

/* Page Type Field */
#define TF_LOG                  0x00    /**< log or data page               */
#define TF_BU                   0x01    /**< buffer page                    */
#define TF_META                 0x02    /**< meta page                      */
#define TF_NULL                 0x00    /**< NULL value                     */

/* Meta Type Field */
#define MT_ROOT                 0x01    /**< Root meta page                 */
#define MT_HEADER               0x02    /**< Directory header meta page     */
#define MT_BMT                  0x04    /**< BMT meta page                  */
#define MT_PMT                  0x08    /**< PMT meta page                  */
#define MT_TU                   0x10    /**< Temporal meta page             */

/* LSBMSB & MIDEND Field */
#define MIDLSB                  0x01    /**< LSB page & MID                 */
#define ENDLSB                  0x02    /**< LSB page & END                 */
#define MIDMSB                  0x04    /**< MSB page & MID                 */
#define ENDMSB                  0x08    /**< MSB page & END                 */

/* NULL values */
#define NULL_DGN                    (0xFFFF)
#define NULL_VBN                    (0xFFFF)
#define NULL_POFFSET                (0xFFFF)
#define NULL_VPN                    (0xFFFFFFFF)
#define NULL_LOGIDX                 (0xFF)

/* Buffer state (BU) */
#define STL_BUFSTATE_CLEAN      0
#define STL_BUFSTATE_DIRTY      1

#define STL_BUFFER_CTX_AFTER_INVALIDATE(ctx) do { \
        (ctx)->nBufState = STL_BUFSTATE_CLEAN;  \
        (ctx)->nValidSBitmap = 0;               \
        (ctx)->nBufferedDgn = NULL_DGN;         \
        (ctx)->nBufferedVpn = NULL_VPN;         \
        (ctx)->nBufferedLpn = NULL_VPN;         \
    } while(0);

#define STL_BUFFER_CTX_AFTER_FLUSH(ctx) do { \
       (ctx)->nBufState = STL_BUFSTATE_CLEAN; \
    } while(0);


/*****************************************************************************/
/* Type defines                                                              */
/*****************************************************************************/
typedef enum
{
    RBW_DEVTYPE_SLC                 = 1,
    RBW_DEVTYPE_MLC                 = 2,
    RBW_DEVTYPE_ONENAND             = 4,
    RBW_DEVTYPE_HYBRID_ONENAND      = 8,
    RBW_DEVTYPE_NUL                 = 0x7FFFFFFF
} RBWDevType;


/****************************************************************************/
/* global data structure                                                    */
/****************************************************************************/
/**
 * @brief     NAND device attribute structure
 */
typedef struct 
{
    RBWDevType    nDeviceType;                  ///< the type of device 
    UINT32        nLSBModeBranchPgOffset;       ///< LSB only mode recognition page offset 
    UINT32        nNumWays;                     ///< number of ways 

    UINT32        nPagesPerSBlk;                ///< the count of pages per super block 
    UINT32        nSecPerSBlk;                  ///< number of sectors per super block 
    UINT32        nSecPerVPg;                   ///< the number of sectors per virtual page 
    UINT32        nBytesPerVPg;                 ///< bytes per virtual page (main) 
    UINT32        nFullSBitmapPerVPg;           ///< full sector bitmap per virtual page 

    UINT32        nNumWaysShift;
    UINT32        nSecPerVPgShift;
    UINT32        nPagesPerSbShift;
} RBWDevInfo;


/**
 *  @brief  Zone information data structure
 */
typedef struct
{
    BADDR           nStartVbn;              /**< Start VBN of zone                          */
    UINT16          nNumTotalBlks;          /**< number total blocks in one zone            */
    UINT16          nNumMetaBlks;           /**< number of meta blocks                      */
    UINT16          nNumDirHdrPgs;          /**< number of directory header pages           */
    UINT16          nMaxPMTDirEntry;        /**< max number of entries in PMT dir           */
    UINT16          nNumLogGrpPerPMT;       /**< number of log groups per PMT               */

    UINT16          nNumFBlks;              /**< total number of free blocks                */
    UINT16          nNumUserBlks;           /**< total number of user blocks                */

    UINT32          nNumUserScts;           /**< number of total user sectors               */
    UINT32          nNumUserPgs;            /**< number of total user pages                 */

    UINT16          nNumLA;                 /**< total number of LA                         */
    UINT16          nDBlksPerLA;            /**< number of managed data blocks per one LA   */
    UINT32          nDBlksPerLAShift;       /**< data blocks per LA (bit shift)             */

    BADDR           aMetaVbnList[MAX_META_BLKS];
                                            /**< meta block VBN list                        */

} STLZoneInfo;


/**
 *  @brief  Root information data structure
 */
typedef struct
{
    UINT8           aSignature[8];          /**< STL signature                              */
    UINT32          nVersion;               /**< STL version                                */

    UINT32          nAge_High;              /**< header age (high 32bits)                   */
    UINT32          nAge_Low;               /**< header age (low  32bits)                   */

    UINT32          nMetaVersion;           /**< meta format version                        */

    BADDR           nRootStartVbn;          /**< root block start VBN                       */
    UINT16          nRootCPOffs;            /**< current root page offset                   */

    UINT16          nNumZone;               /**< number of zone                             */
    UINT16          nNumRootBlks;           /**< number of root blocks                      */

    UINT16          nN;                     /**< mapping constant 'N'                       */
    UINT16          nK;                     /**< mapping constant 'k'                       */

    UINT16          nNShift;                /**< 'N' bit shift                              */
    UINT16          nKShift;                /**< 'k' bit shift                              */

    UINT32          aRootBlkEC[MAX_ROOT_BLKS];
                                            /**< root block erase count info                */
    STLZoneInfo     aZone[MAX_ZONE];        /**< STLZoneInfo array of STL                   */

} STLRootInfo;


/**
 *  @brief  Meta block location information for Root block
 */
typedef struct
{
    STLRootInfo     stRootInfo;
    UINT8           aPadding[ROOT_INFO_BUF_SIZE - sizeof(STLRootInfo)];

} STLRootInfoBuf;


/**
 *  @brief  Meta format layout information
 */
typedef struct
{
    /* buffer size configuration */
    UINT32          nDirHdrBufSize;         /**< directory header buffer size               */
    UINT32          nBMTBufSize;            /**< BMT buffer size                            */
    UINT32          nCtxBufSize;            /**< context info buffer size                   */
    UINT32          nPMTBufSize;            /**< PMT buffer size                            */

    /* sector bitmap configuration */
    UINT32          nDirHdrSBM;             /**< directory header sector bitmap             */
    UINT32          nBMTCtxSBM;             /**< BMT+CTX sector bitmap                      */
    UINT32          nCtxPMTSBM;             /**< CTX+PMT sector bitmap                      */
    UINT32          nBMTSBM;                /**< BMT sector bitmap                          */
    UINT32          nPMTSBM;                /**< PMT sector bitmap                          */

    /* meta data size */
    UINT32          nMaxInaLogGrps;         /**< max. inactive log groups per zone          */
    UINT32          nMaxBlksPerZone;        /**< max. number of blocks per zone             */
    UINT32          nPagesPerLGMT;          /**< number of pages per log group PMT          */
    UINT32          nBytesPerLogGrp;        /**< log group size                             */
    UINT32          nMaxFreeSlots;          /**< maximum number of slots in free list       */

} STLMetaLayout;


/**
 *  @brief  Zero bit count confirmation structure
 */
typedef struct 
{
    UINT32          nZBC;                   /**< zero bit count                             */
    UINT32          nInvZBC;                /**< inverted ZBC                               */

} STLZbcCfm;


/**
 *  @brief  Directory header structure (fixed size members)
 */
typedef struct
{
    /* Header age information */
    UINT32          nAge_High;              /**< header age (high 32bits)                   */
    UINT32          nAge_Low;               /**< header age (low 32bits)                    */

    /* Meta block compaction victim information */
    UINT32          nVictimIdx;             /**< compaction victim block index              */

    /* Current working meta block information */
    UINT16          nHeaderIdx;             /**< current latest map header index            */
    POFFSET         nCPOffs;                /**< current clean page offset (RAM)            */

    /* Header index & context page index for meta compaction */
    UINT16          nLatestBlkIdx;          /**< block index of the latest CTX page         */
    UINT16          nLatestPOffs;           /**< page offset of the latest CTX page         */

    /* Idle meta block list */
    UINT16          nIdleBoListHead;        /**< head index of idle meta block list         */
    UINT16          nNumOfIdleBlk;          /**< number of idle meta blocks                 */

    /* PMT directory management information */
    UINT16          nMinECPMTIdx;           /**< index which has minimum erase count        */
    UINT16          nPMTEntryCnt;           /**< log group entry count in astPMTDir[]       */

} STLDirHeaderFm;


/**
 *  @brief  PMT directory entry
 */
typedef struct
{
    /* nDgn : PMT ratio < 100%, nCost : PMT ratio = 100% */
    UINT16          nCost;                  /**< compaction cost                            */
    POFFSET         nPOffs;                 /**< meta page offset                           */

} STLPMTDirEntry;


/**
 *  @brief  PMT wear-leveling group
 */
typedef struct
{
    BADDR           nDgn;                   /**< data group number                          */
    BADDR           nMinVbn;                /**< minimum EC blocks' VBN                     */
    UINT32          nMinEC;                 /**< minimum EC in this group                   */

} STLPMTWLGrp;


/**
 *  @brief  Directory header structure  (RAM manipulation)
 */
typedef struct
{
    /* variable size members */
    UINT16          *pIdleBoList;           /**< idle block offset list of meta blocks      */
    UINT16          *pValidPgCnt;           /**< valid page count of each meta block        */
    UINT32          *pMetaBlksEC;           /**< erase count of meta blocks                 */
    POFFSET         *pBMTDir;               /**< BMT location directory                     */
    UINT8           *pPMTMergeFlags;        /**< merge status flag of a log group           */
                                            /* 0 bit : merge -> free block generation (X)   */
                                            /* 1 bit : merge -> free block generation (O)   */
    STLPMTDirEntry  *pstPMTDir;             /**< PMT location directory                     */
    STLPMTWLGrp     *pstPMTWLGrp;           /**< PMT wear-leveling group information        */

    STLDirHeaderFm  *pstFm;                 /**< pointer to fixed size members              */

    UINT8           *pBuf;                  /**< directory header buffer pointer            */
    UINT32          nBufSize;               /**< buffer size                                */

} STLDirHdrHdl;


/**
 *  @brief  Context information structure (fixed size members)
 */
typedef struct
{
    /* age information */
    UINT32          nAge_High;              /**< context age (high 32bits)                  */
    UINT32          nAge_Low;               /**< context age (low 32bits)                   */

    /* POR Info : Merged log block information for PMTDir update */
    BADDR           nMergedDgn;             /**< inactive log group during merge/compaction */
    BADDR           nMergedVbn;             /**< inactive log during merge/compaction       */
    UINT16          nMetaReclaimFlag;       /**< meta reclaim flag : TRUE(1)/FALSE(0)       */
    BADDR           nMergeVictimDgn;        /**< merge victim group number during GC/RGC    */
    UINT16          nMergeVictimFlag;       /**< merge victim group flag : TRUE(1)/FALSE(0) */

    /* POR Info : Modified cost for PMTDir update */
    BADDR           nModifiedDgn;           /**< The DGN of which cost is modified          */
    UINT16          nModifiedCost;          /**< The modified cost                          */

    /* active log information */
    UINT16          nNumLBlks;              /**< current number of active log blocks        */

    /* free block list information */
    UINT16          nNumFBlks;              /**< current number of free blocks              */
    UINT16          nFreeListHead;          /**< free block list head position              */

    /* data wear-leveling information */
    UINT16          nMinECLan;              /**< LAN of minimum EC data block               */

    /* buffer block information (mis-aligned write request) */
    BADDR           nBBlkVbn;               /**< buffer block VBN                           */

    /* buffer, temporary block erase count */
    UINT32          nBBlkEC;                /**< buffer block erase count                   */
    UINT32          nTBlkEC;                /**< temporary block erase count                */

    /* idle data block information */
    UINT32          nNumIBlks;              /**< number of idle data block                  */
    
    /* temporary block information (MLC LSB page recovery) */
    BADDR           nTBlkVbn;               /**< temporary block VBN                        */

    /* zone to zone wear-leveling information */
    UINT16          nZoneWLMark;            /**< zone wear-leveling start/end mark          */
    UINT16          nZone1Idx;              /**< zone target1 index                         */
    UINT16          nZone2Idx;              /**< zone target2 index                         */
    BADDR           nZone1Vbn;              /**< zone target1 VBN                           */
    BADDR           nZone2Vbn;              /**< zone target2 VBN                           */
    UINT32          nZone1EC;               /**< zone target1 erase count                   */
    UINT32          nZone2EC;               /**< zone target2 erase count                   */

    /* PMT wear-leveling information (POR) */
    UINT16          nMinECPMTIdx;           /**< mimimum EC PMT WL group index              */
    UINT16          nUpdatedPMTWLGrpIdx;    /**< updated PMT WL group index                 */
    STLPMTWLGrp     stUpdatedPMTWLGrp;      /**< updated PMT WL group                       */

} STLCtxInfoFm;


/**
 *  @brief  Active Log Entry structure
 */
typedef struct
{
    BADDR           nDgn;                   /**< data block group number                    */
    BADDR           nVbn;                   /**< virtual block number                       */

} STLActLogEntry;


/**
 *  @brief  Context Information structure  (RAM manipulation)
 */
typedef struct
{
    /* fixed size members */
    STLCtxInfoFm    *pstFm;                 /**< pointer to fixed size members              */

    /* variable size members */
    STLActLogEntry  *pstActLogList;         /**< active log list                            */
    BADDR           *pFreeList;             /**< free blocks list                           */
    UINT32          *pFBlksEC;              /**< erase count list of free blocks            */
    UINT32          *pMinEC;                /**< minimum erase count of each LA             */
    UINT16          *pMinECIdx;             /**< minimum EC block index of each LA          */

    /* zero bit count confirmation */
    STLZbcCfm       *pstCfm;                /**< ZBC confirmation                           */
    UINT32          nCfmBufSize;            /**< target buffer size in ZBC calculation      */

    /* I/O buffer memory */
    UINT8           *pBuf;                  /**< context info buffer pointer                */
    UINT32          nBufSize;               /**< buffer size                                */

} STLCtxInfoHdl;

/**
 *  @brief  BMT(Block Mapping Table) entry structure
 */
typedef struct
{
    BADDR           nVbn;                   /**< virtual block number                       */

#if defined(__GNUC__)
} __attribute ((packed)) STLBMTEntry;
#else
} STLBMTEntry;
#endif


/**
 *  @brief  BMT data structure (fixed size members)
 */
typedef struct
{
    UINT16          nLan;                   /**< LAN (Local Area Number)                    */
    UINT16          nInvLan;                /**< inverted LAN                               */

} STLBMTFm;


/**
 *  @brief  BMT data structure (RAM manipulation)
 */
typedef struct
{
    /* fixed size members */
    STLBMTFm        *pstFm;                 /**< pointer to fixed size members              */

    /* variable size memebers */
    STLBMTEntry     *pMapTbl;               /**< data block mapping unit                    */
    UINT32          *pDBlksEC;              /**< data block erase count table               */
    UINT8           *pGBlkFlags;            /**< garbage flag bitstream                     */

    /* zero bit count confirmation */
    STLZbcCfm       *pstCfm;                /**< ZBC confirmation                           */
    UINT32          nCfmBufSize;            /**< target buffer size in ZBC calculation      */

    /* I/O buffer memory */
    UINT8           *pBuf;                  /**< BMT buffer pointer                         */
    UINT32          nBufSize;               /**< buffer size                                */

} STLBMTHdl;


/**
 *  @brief  Log block structure (fixed size members).
 */
typedef struct
{
    UINT8           nState;                 /**< flag, log block state                      */
    
    UINT8           nSelfIdx;               /**< self index in its log group                */
    UINT8           nPrevIdx;               /**< previous log index (doubly linked list)    */
    UINT8           nNextIdx;               /**< next log index (doubly linked list)        */
    
    BADDR           nVbn;                   /**< log virtual block number                   */
    POFFSET         nLastLpo;               /**< lastly programmed LPN Offset               */

    POFFSET         nCPOffs;                /**< clean page offset                          */
    UINT16          nLSBOffs;               /**< LSB page offset in MLC fast mode           */

    UINT16          nNOP;                   /**< NOP of current programming page            */
    UINT16          nCSOffs;                /**< clean sector offset                        */
                                   
    UINT32          nEC;                    /**< erase count                                */

} STLLog;


/**
 *  @brief  Log group structure (fixed size members)
 */
typedef struct
{
    BADDR           nDgn;                   /**< data group number                          */
    UINT16          nMinVPgLogIdx;          /**< index of log which has minimum valid pages */

    UINT8           nState;                 /**< log group state                            */
    UINT8           nNumLogs;               /**< number of logs in this log group           */
    UINT8           nHeadIdx;               /**< Log list head index                        */
    UINT8           nTailIdx;               /**< Log list tail index                        */

    UINT16          nMinVbn;                /**< VBN of log which has minimum erase count   */
    UINT16          nMinIdx;                /**< index of log which has minimum erase count */
    UINT32          nMinEC;                 /**< minimum EC                                 */

} STLLogGrpFm;


/**
 *  @brief  Log group structure (RAM manipulation)
 */
typedef struct _LGHdl
{
    /* fixed size members */
    STLLogGrpFm     *pstFm;                 /**< log group fixed size members               */

    /* variable size members */
    STLLog          *pstLogList;            /**< log list                                   */
    BADDR           *pLbnList;              /**< LBN list in each log block                 */
    UINT16          *pNumDBlks;             /**< number of mapped data blocks in each log   */
    UINT16          *pLogVPgCnt;            /**< number of valid pages in each log          */
    POFFSET         *pMapTbl;               /**< log group page mapping table               */

    /* zero bit count confirmation */
    STLZbcCfm       *pstCfm;                /**< ZBC confirmation                           */
    UINT32          nCfmBufSize;            /**< target buffer size in ZBC calculation      */

    /* I/O buffer memory */
    UINT8           *pBuf;                  /**< log group buffer pointer                   */
    UINT32          nBufSize;               /**< buffer size                                */

    struct _LGHdl   *pPrev;                 /**< previous link                              */
    struct _LGHdl   *pNext;                 /**< next link                                  */

} STLLogGrpHdl;


/**
 *  @brief  PMT structure to load/store in meta page
 */
typedef struct
{
    UINT32          nNumLogGrps;            /**< number of log groups                       */

    STLLogGrpHdl    astLogGrps[MAX_LOGGRP_PER_PMT];
                                            /**< pointer array of log group                 */

    /* I/O buffer memory */
    UINT8           *pBuf;                  /**< PMT buffer pointer                         */
    UINT32          nBufSize;               /**< buffer size                                */

} STLPMTHdl;


/**
 *  @brief  Log group list structure
 */
typedef struct
{
    UINT32          nNumLogGrps;            /**< current number of log groups in use        */
    STLLogGrpHdl    *pstHead;               /**< log group head pointer                     */
    STLLogGrpHdl    *pstTail;               /**< log group tail pointer                     */

} STLLogGrpList;

#if (OP_SUPPORT_STATISTICS_INFO == 1)
/** 
 *  @brief  Statistics information
 */
typedef struct
{
    UINT32          nSimpleMergeCnt;        /**< simple merge count                         */
    UINT32          nCopyMergeCnt;          /**< copy merge count                           */
    UINT32          nCompactionCnt;         /**< compaction count                           */
    UINT32          nActCompactCnt;         /**< active compaction count                    */
    UINT32          nTotalLogBlkCnt;        /**< total allocated log block count            */
    UINT32          nTotalLogPgmCnt;        /**< total log block program count              */
    UINT32          nCtxPgmCnt;             /**< total meta page program count              */
#if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    UINT32          nBufferWriteCnt;
    UINT32          nBufferMissCnt;
    UINT32          nBufferHitCnt;
#endif

} STLStats;
#endif  /* (OP_SUPPORT_STATISTICS_INFO == 1) */

/**
 *  @brief  Deleted context structure
 */
typedef struct
{
    BADDR           nDelPrevDgn;            /**< latest deleted DGN                         */
    BADDR           nDelPrevLan;            /**< latest deleted LAN                         */
    STLLogGrpHdl    *pstDelLogGrpHdl;       /**< latest deleted log group pointer           */
    PADDR           nDelLpn;                /**< latest deleted LPN                         */
    UINT32          nDelSBitmap;            /**< latest deleted pages' sector bitmap        */

} STLDelCtxObj;

/**
 * @brief   Buffer unit context structure
 */
typedef struct
{
    UINT16          nBufState;              /* represent the state of the buffered page (in memory) 
                                             * Specifically, this represents whether the buffer
                                             * requires to be flushed or not                */
    UINT16          nPadding;
    SBITMAP         nValidSBitmap;          /**< Bitmap representing valid scts  
                                             (also written in spare area in flash)          */
    UINT16          nBBlkCPOffset;          /**< buffer block clean page offset             */
    BADDR           nBufferedDgn;
    PADDR           nBufferedLpn;           /**< current buffered LPN                       */
    PADDR           nBufferedVpn;           /**< LPN -> VPN mapping information             */

} STLBUCtxObj;


/**
*  @brief  External environment variables
*/
typedef struct
{
    UINT32          nECDiffThreshold;       /**< wear-leveling EC diff threshold            */
    UINT32          nStartVpn;              /**< cluster start VPN                          */
    UINT32          nStartVbn;              /**< cluster start VBN                          */

    RBWDevInfo      stDevInfo;              /**< cluster device info                        */
} STLClstEnvVar;


/**
 *  @brief  Zone global object type
 */
typedef struct
{
    BOOL32          bOpened;                /**< open or not                                */

    UINT32          nVolID;                 /**< Volume ID                                  */
    UINT32          nPart;                  /**< Partition Index, not exact ID              */

    UINT32          nClstID;                /**< cluster id                                 */
    UINT32          nZoneID;                /**< zone id                                    */

    RBWDevInfo      *pstDevInfo;            /**< device Info                                */

    STLMetaLayout   *pstML;                 /**< meta layout format                         */

    STLRootInfo     *pstRI;                 /**< root info                                  */
    STLZoneInfo     *pstZI;                 /**< zone info                                  */

    STLDirHdrHdl    *pstDirHdrHdl;          /**< directory header                           */
    UINT8           *pDirHdrBuf;            /**< directory header buffer                    */

    STLCtxInfoHdl   *pstCtxHdl;             /**< context info                               */
    STLBMTHdl       *pstBMTHdl;             /**< BMT                                        */
    STLPMTHdl       *pstPMTHdl;             /**< PMT                                        */

    UINT8           *pMetaPgBuf;            /**< meta page buffer                           */
    UINT8           *pTempMetaPgBuf;        /**< temporary meta page buffer                 */

    STLLogGrpList   *pstActLogGrpList;      /**< active log group list                      */
    STLLogGrpHdl    *pstActLogGrpPool;      /**< active log group handle pool               */
    UINT8           *pActLogGrpPoolBuf;     /**< active log group memory pool               */
    STLLogGrpList   *pstInaLogGrpCache;     /**< inactive log group cache                   */
    STLLogGrpHdl    *pstInaLogGrpPool;      /**< inactive log group handle pool             */
    UINT8           *pInaLogGrpPoolBuf;     /**< inactive log group memory pool             */

    UINT8           *pGCScanBitmap;         /**< LA updated flag bitmap for GC              */

    #if (OP_SUPPORT_PAGE_DELETE == 1)
    STLDelCtxObj    *pstDelCtxObj;          /**< deleted info                               */
    #endif

    #if (OP_SUPPORT_PAGE_MISALIGNED_WRITE == 1)
    STLBUCtxObj     *pstBUCtxObj;           /**< BU info                                    */
    #endif

    UINT8           *pFullBMTBuf;           /**< full BMT buffer                            */

    #if (OP_SUPPORT_STATISTICS_INFO == 1)
    STLStats        *pstStats;              /**< statistical information                    */
    #endif

    #if (OP_SUPPORT_RUNTIME_PMT_BUILDING == 1)
    BADDR           abInitActLogDgn[MAX_ACTIVE_LBLKS];
                                            /**< Active Log's Data Group Number             */
    BOOL32          abActLogScanCompleted;  /**< active log scanning is completed or not    */
    #endif

} STLZoneObj;


/**
 *  @brief  Top level cluster global object type
 */
typedef struct
{
    UINT32          nVolID;                 /**< volume id                                  */

    STLRootInfoBuf  stRootInfoBuf;          /**< Root information                           */

    STLMetaLayout   stML;                   /**< meta page layout info                      */

    STLZoneObj      stZoneObj[MAX_ZONE];    /**< zone objects array                         */

    RBWDevInfo      *pstDevInfo;            /**< device Info                                */

    UINT32          nVFLParamCnt;           /**< VFL parameter in use count                 */
    VFLParam        astParamPool[MAX_VFL_PARAMS];
                                            /**< VFL parameter pool                         */

    UINT32          nVFLExtCnt;             /**< extended parameter busy count              */
    VFLExtParam     astExtParamPool[MAX_VFLEXT_PARAMS];         
                                            /**< extended VFL parameter pool                */

    UINT8           *pTempPgBuf;            /**< temporary page buffer                      */

    UINT32          nMaxWriteReq;           /** < The maximum BML_Write request             */

    STLClstEnvVar   *pstEnvVar;             /**< external environment variables             */

    FSRSpareBuf     staSBuf[FSR_MAX_WAYS];  /**< FSR spare data pointers                    */
    FSRSpareBufBase staSBase[FSR_MAX_WAYS]; /**< FSR base spare data                        */
    FSRSpareBufExt  staSExt[FSR_MAX_SPARE_BUF_EXT];
                                            /**< FSR extended spare data                    */

    BMLCpBkArg      *pstBMLCpBk[FSR_MAX_WAYS];
                                            /**< FSR BML copyback pointer                   */
    BMLCpBkArg      staBMLCpBk[FSR_MAX_WAYS];
                                            /**< FSR BML copyback arguments                 */
    BMLRndInArg     staBMLRndIn[3];         /**< FSR BML Random-in arguments                */

    BOOL32          bTransBegin;            /**< Does transaction begin?                    */

#if (OP_SUPPORT_MSB_PAGE_WAIT == 1)
    BOOL32          baMSBProg[FSR_MAX_WAYS];/**< Is previous program MSB?                   */
#endif

} STLClstObj;


#if (OP_SUPPORT_WRITE_BUFFER == 1)
/**
 * @brief  Write buffer object type
 */
typedef struct
{
    /* WB device spec */
    UINT32          nNumWays;
    UINT32          nPgsPerBlk;
    UINT32          nPgsPerBlkSft;
    UINT32          nSctsPerPg;
    UINT32          nSctsPerPgSft;

    /* WB partition information */
    BADDR           nStartVbn;              /**< Start VBN of WB                            */
    UINT16          nNumTotalBlks;          /**< # of total blocks in WB                    */
    PADDR           nStartVpn;              /**< Start VPN of WB                            */
    PADDR           nNumTotalPgs;           /**< # of total pages in WB                     */

    /* WB mother partition information */
    UINT32          nNumTotalBlksM;         /**< # of total blocks in mother                */
    UINT32          nNumTotalPgsM;          /**< # of total pages in mother                 */

    /* WB offset pointers */
    BADDR           nHeadBlkOff;
    BADDR           nTailBlkOff;
    POFFSET         nHeadPgOff;

    /* Header age */
    UINT32          nHeaderAge;

    /* Flag for CloseWB */
    BOOL32          bDirty;

    /* Mapping information */
    POFFSET         *pIndexT;
    POFFSET         *pCollisionT;
    PADDR           *pPgMapT;
    UINT8           *pPgBitmap;

    /* Deleted page list */
    UINT32          nDeletedListOff;
    PADDR          *pDeletedList;

    /* Temporary buffer */
    UINT8           *pMainBuf;
    FSRSpareBuf     *pstSpareBuf;
    FSRSpareBufBase *pstSpareBufBase;
    FSRSpareBufExt  *pstSpareBufExt;

} STLWBObj;
#endif  /* (OP_SUPPORT_WRITE_BUFFER == 1) */

/**
 *  @brief  User level partition information object type
 */
typedef struct _STLPart
{
    UINT32          nVolID;                 /**< Volume ID                                  */
    UINT32          nPart;                  /**< Partition Index, not exact ID              */

    BOOL32          nOpenCnt;               /**< The count of open                          */
    UINT32          nOpenFlag;              /*<< Is the read only partition ?               */
    BOOL32          nInitOpenFlag;          /**< The initial flag                           */

    UINT32          nClstID;                /*<< The cluster ID of the partition            */
    UINT32          nZoneID;                /*<< The Zone ID of the partition               */

    UINT32          nNumZone;               /*<< The # of zones of the partition            */
    UINT32          nNumPart;               /*<< The # of partitions of the cluster         */

    UINT32          nMaxUnit;               /*<< The # of unit of the partition             */

    struct _STLPart *pst1stPart;            /*<< The part ID of the 1st zone                */

    SM32            nSM;                    /*<< handle ID for semaphore                    */

    UINT32          nBlkSft;                /*<< The shift bit for LBN                      */
    UINT32          nBlkMsk;                /*<< The mask bit for LBN                       */
    UINT32          nZoneSft;               /*<< The shift bit for zone                     */
    UINT32          nZoneMsk;               /*<< The mask bit for zone                      */

    STLClstEnvVar   stClstEV;               /*<< environment variables                      */

#if (OP_SUPPORT_WRITE_BUFFER == 1)
    STLWBObj       *pstWBObj;               /*<< Write buffer                               */
#endif

#if (OP_SUPPORT_HOT_DATA_DETECTION == 1)
    UINT32         *pLRUT;                  /*<< Two level LRU table                        */
    UINT32          nLRUTSize;              /*<< Size of LRU table                          */
    UINT32          nDecayCnt;              /*<< Write count for decaying                   */
    UINT32          nWriteCnt;              /*<< Write count                                */
    UINT32          nHitCnt;                /*<< Write count to be hit                      */
    UINT32          nLSBPos;                /*<< LSB bits position in LRU table             */
#endif

#if (OP_SUPPORT_STATISTICS_INFO == 1)
    UINT32          nSTLLastECnt;
    UINT32          nSTLRdScts;
    UINT32          nSTLWrScts;
    UINT32          nSTLDelScts;
#endif

} STLPartObj;


#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* __FSR_STL_TYPES_H__*/
