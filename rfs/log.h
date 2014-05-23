/*
 *---------------------------------------------------------------------------*
 *                                                                           *
 *          COPYRIGHT 2003-2009 SAMSUNG ELECTRONICS CO., LTD.                *
 *                          ALL RIGHTS RESERVED                              *
 *                                                                           *
 *   Permission is hereby granted to licensees of Samsung Electronics        *
 *   Co., Ltd. products to use or abstract this computer program only in     *
 *   accordance with the terms of the NAND FLASH MEMORY SOFTWARE LICENSE     *
 *   AGREEMENT for the sole purpose of implementing a product based on       *
 *   Samsung Electronics Co., Ltd. products. No other rights to reproduce,   *
 *   use, or disseminate this computer program, whether in part or in        *
 *   whole, are granted.                                                     *
 *                                                                           *
 *   Samsung Electronics Co., Ltd. makes no representation or warranties     *
 *   with respect to the performance of this computer program, and           *
 *   specifically disclaims any responsibility for any damages,              *
 *   special or consequential, connected with the use of this program.       *
 *                                                                           *
 *---------------------------------------------------------------------------*
*/
/**
 *  @version 	RFS_1.3.1_b072_RTM
 *  @file	fs/rfs/log.h
 *  @brief	header file for log
 *
 *
 */

#ifndef _LINUX_RFS_LOG_H
#define _LINUX_RFS_LOG_H

#ifdef __KERNEL__
#include <linux/sched.h>
#include <linux/rfs_fs.h>
#else
#define u8	unsigned char
#define u16	unsigned short
#define u32	unsigned int
#define u64	unsigned long long
#endif

/***************************************************************************/
/* RFS Common definitions                                                  */
/***************************************************************************/
/* max value is set with extra space */
/* #define RFS_LOG_MAX_CLUSTERS            100 */
#define RFS_LOG_MAX_CLUSTERS	115
#define RFS_LOG_MAX_CHUNKS	19	

#define MAX_REMOVE_SLOT		21 /* max extend slot + short slot */

#if defined(CONFIG_RFS_PRE_ALLOC) && (CONFIG_RFS_PRE_ALLOC < RFS_LOG_MAX_CLUSTERS)
#define RFS_LOG_PRE_ALLOC	CONFIG_RFS_PRE_ALLOC
#else
#define RFS_LOG_PRE_ALLOC	RFS_LOG_MAX_CLUSTERS
#endif

/*
 * define name of logfile
 * After ver.1.3.0, logfile is used for unlink and deallocating clusters
 */
#define RFS_LOG_FILE_NAME       "$RFS_LOG.LO$"	/* logfile name */
#define RFS_LOG_FILE_LEN	12

#define RFS_LOG_ENTRY_NAME      "$RFS_LOGLO$" /* logfile name in entry */
#define RFS_LOG_ENTRY_NAME_LEN  11

/***************************************************************************/
/* RFS dependant definitions                                               */
/***************************************************************************/
#define RFS_LOG_MAX_COUNT		256 
#define RFS_LOG_MAX_SIZE_IN_BYTE	RFS_LOG_MAX_COUNT << SECTOR_BITS 
#define RFS_LOG_MAX_MULTI_BLOCK_RECORD	((RFS_LOG_MAX_COUNT >> 1) - 1)

/* transaction type */
#define RFS_LOG_NONE			(unsigned int) 0x0000
#define RFS_LOG_CREATE			(unsigned int) 0x0001
#define RFS_LOG_RENAME                  (unsigned int) 0x0002
#define RFS_LOG_WRITE                   (unsigned int) 0x0003
#define RFS_LOG_TRUNCATE_F              (unsigned int) 0x0004
#define RFS_LOG_TRUNCATE_B              (unsigned int) 0x0005
#define RFS_LOG_UNLINK                  (unsigned int) 0x0006
#define RFS_LOG_DEL_INODE		(unsigned int) 0x0007
#define RFS_LOG_SYMLINK			(unsigned int) 0x0008
#define RFS_LOG_REPLAY			(unsigned int) 0x0010
#define RFS_LOG_XATTR			(unsigned int) 0x0011
#define RFS_LOG_INVALID			(unsigned int) 0xffff

/* sub transaction type */
#define RFS_SUBLOG_COMMIT               (unsigned int) 0x0000
#define RFS_SUBLOG_ABORT		(unsigned int) 0x0001
#define RFS_SUBLOG_START                (unsigned int) 0x0002
#define RFS_SUBLOG_ALLOC_CHAIN		(unsigned int) 0x0003
#define RFS_SUBLOG_BUILD_ENTRY          (unsigned int) 0x0005
#define RFS_SUBLOG_REMOVE_ENTRY         (unsigned int) 0x0006
//#define RFS_SUBLOG_UPDATE_ENTRY         (unsigned int) 0x0007
//#define RFS_SUBLOG_GET_CHAIN		(unsigned int) 0x0009
#define RFS_SUBLOG_MOVE_CHAIN		(unsigned int) 0x000B
//#define RFS_SUBLOG_UPDATE_POOL		(unsigned int) 0x000C
#define RFS_SUBLOG_DEALLOC		(unsigned int) 0x000D
#define RFS_SUBLOG_MRC			(unsigned int) 0x000E
#define RFS_SUBLOG_XATTR_SET		(unsigned int) 0x000F

/* bit field for log status */
#define RFS_LOG_STATUS_ENABLE		1 << 0
#define RFS_LOG_STATUS_FILE_EXIST	1 << 1
#define RFS_LOG_STATUS_RDONLY_MOUNT	1 << 2

/* logtype specific MACRO */
#define MK_LOG_TYPE(X, Y)       (((X) << 16) | (Y))
#define GET_LOG(X)              ((X) >> 16)
#define GET_SUBLOG(X)           ((X) & 0x0000ffff)

#ifdef CONFIG_GCOV_PROFILE
typedef u32	rfs_log_seq_t;
#else
typedef u64	rfs_log_seq_t;
#endif


struct clu_chunk
{
	unsigned int start;
	unsigned int count;
};

#ifdef __KERNEL__
/*
 * structure for cluster chunk list (INCORE)
 */
struct clu_chunk_list
{
	struct clu_chunk chunk;
	struct list_head list;
};

/*
 * structure of candidate segment (INCORE)
 */
struct c_segment {
	unsigned int start_cluster;
	unsigned int last_cluster;
	struct list_head list;
};
#define SEGMENT_ENTRY(p)	list_entry(p, struct c_segment, list)
#endif //(__KERNEL__)

struct rfs_log_fat
 {
	/* two fields for minimal fat entry infos */
        u32	pdir;
        u32	entry;

	/* two fields for xattr (flag, ctime, cdate) */
	u32	b_is_xattr;
	u16	xattr_ctime;
	u16	xattr_cdate;

	/* two fields for source location */
	u32	s_prev_clu;
	u32	s_next_clu;

	/* two fields for destination location */
	u32	d_prev_clu;
	u32	d_next_clu;


	/* chain information*/
	u32	numof_clus;
	u32	clus[RFS_LOG_MAX_CLUSTERS];
} __attribute__ ((packed));

struct log_FAT_info
{
	unsigned int pdir;
 	unsigned int entry;
	unsigned int s_start_clu;
	unsigned int s_last_clu;
	unsigned int s_filesize;
	unsigned int s_new_last;

	/* four fields for alloc cluster inof */
	unsigned int s_prev_clu;
	unsigned int s_next_clu;

	unsigned int d_prev_clu;
	unsigned int d_next_clu;

	unsigned int numof_clus;
	unsigned int *clus;

	/* three fields for xattr (flag, ctime, cdate) */
	unsigned int b_is_xattr;
	u16	ctime;
	u16	cdate;
};

struct rfs_log_entry {
        u32	pdir;
        u32	entry;
        u32	numof_entries;
        u8	undel_buf[MAX_REMOVE_SLOT];
};

struct log_ENTRY_info
{
	unsigned int pdir;
 	unsigned int entry;
	unsigned int numof_entries;

	unsigned char *undel_buf;
};

struct rfs_log_dealloc 
{
	/* fields for minimal fat entry infos */
        u32	pdir;
        u32	entry;
	u32	f_filesize;
	u32	f_lastcluster;

	/* tow fields for logfile location */
	u32	l_prev_clu;
	u32	l_next_clu;

	/* next!!*/
	u32	next_chunks;
	u32	record_count;

	/* cluster chunks */
	u32	nr_chunk;
	struct clu_chunk	chunks[RFS_LOG_MAX_CHUNKS];
	u32	mr_signature;
};

struct log_DEALLOC_info
{
	/* fields for minimal fat entry infos */
	int is_truncate_b;

	unsigned int pdir;
	unsigned int entry;
	unsigned int s_filesize;

	unsigned int s_new_last;
	unsigned int s_start_clu;
	unsigned int s_last_clu;

	unsigned int l_prev_clu;
	unsigned int l_next_clu;

	unsigned int next_clu;
};

struct log_XATTR_info
{
	/*
	unsigned int	xattr_start_clus;
	unsigned int	xattr_last_clus;
	unsigned int	xattr_numof_clus;

	unsigned short	xattr_valid_count;
	unsigned int	xattr_total_space;
	unsigned int	xattr_used_space;
	*/
	unsigned int	xattr_offset_new;
	unsigned int	xattr_offset_old;
};

struct rfs_log_xattr
 {
	/* two fields for minimal fat entry infos */
        u32	pdir;
        u32	entry;

	/* three fields for xattr */
	u32	xattr_start_clus;
	u32	xattr_last_clus;
	u32	xattr_numof_clus;
	u32	xattr_offset_new;
	u32	xattr_offset_old;
	u16	xattr_valid_count;
	u32	xattr_total_space;
	u32	xattr_used_space;
} __attribute__ ((packed));

struct rfs_trans_log {
	u32	type;
	union {
		rfs_log_seq_t sequence;
		u64 	dummy;	/* place holder for compatibility */
	};
	union {
		struct rfs_log_fat	log_fat;
		struct rfs_log_entry    log_entry;
		struct rfs_log_dealloc	log_dealloc;
		struct rfs_log_xattr	log_xattr;
	} __attribute__ ((packed)) ;
	u32 signature;
} __attribute__ ((packed));


#ifdef __KERNEL__
struct rfs_log_info;
struct rfs_log_operations
{
	int (*log_dealloc_chain) (struct super_block *, 
			struct log_DEALLOC_info *, unsigned int *);
	int (*log_mark_end) (struct super_block *, unsigned int);
	int (*log_read) (struct super_block *, unsigned int);
	int (*log_write) (struct super_block *sb);
	int (*log_mwrite) (struct super_block *, int , struct buffer_head **);
	int (*log_read_ahead) (struct super_block *, unsigned int, int,
			 struct buffer_head **);

	int (*log_move_chain)(struct super_block *, struct log_FAT_info *);
	int (*log_move_chain_pp)(struct super_block *sb, unsigned int, 
			unsigned int);
};

struct rfs_log_info {
	sector_t blocks[RFS_LOG_MAX_COUNT];
	unsigned int secs_per_blk;
	unsigned int secs_per_blk_bits;
	unsigned int isec;
	unsigned int type;
	rfs_log_seq_t sequence;
	int dirty;

	void *log_mutex;

	int numof_pre_alloc;
	unsigned int alloc_index;
	unsigned int pre_alloc_clus[RFS_LOG_MAX_CLUSTERS];

	struct inode *inode;		/* target file */
	struct buffer_head *bh;
	struct rfs_trans_log *log;

	unsigned int l_start_cluster;	/* for logfile itself */
	unsigned int l_last_cluster;	/* last clu # of logile itself */

	unsigned int c_start_cluster;	/* start clu # in c_segment list*/
	unsigned int c_last_cluster; 	/* last clu # in c_segment list*/
	unsigned int need_mrc;		/* whether write mrc(multi-block record commit) */

	struct list_head c_list;	/* head for candidate segment list*/
	struct rfs_log_operations *operations;

#ifdef RFS_FOR_2_4
	struct inode tr_buf_inode;	/* in order to link transaction dirty buffers */
#endif
	struct inode *symlink_inode;	/* in order to point the symlink inode */
	struct inode *self_inode;       /* inode of logfile for truncate */
};

/* get rfs log info */
static inline struct rfs_log_info *RFS_LOG_I(struct super_block *sb)
{
	return (struct rfs_log_info *)(RFS_SB(sb)->log_info);
}

/* get rfs log */
static inline struct rfs_trans_log *RFS_LOG(struct super_block *sb)
{
	return ((struct rfs_log_info *)(RFS_SB(sb)->log_info))->log;
}

static inline int is_empty_tr(struct super_block *sb)
{
	if (!(RFS_LOG_I(sb)->dirty))
		return 1;

	return 0;
}

static inline int tr_in_replay(struct super_block *sb)
{
	if (RFS_LOG_I(sb)->type == RFS_LOG_REPLAY)
		return 1;
	return 0;
}

/* called by FAT */
int rfs_log_init(struct super_block *sb);

int rfs_log_start(struct super_block *sb, unsigned int log_type, 
		struct inode *inode);
int rfs_log_end(struct super_block *sb, int result);
int rfs_log_build_entry(struct super_block *sb, struct log_ENTRY_info *lei);
int rfs_log_remove_entry(struct super_block *sb, struct log_ENTRY_info *lei);
int rfs_log_alloc_chain(struct super_block *sb, struct log_FAT_info *);

void rfs_log_release(struct super_block *sb);
int rfs_log_force_commit(struct super_block *sb, struct inode *inode);

int tr_pre_alloc(struct super_block *sb);
int tr_deferred_commit(struct super_block *sb);

/* called by cluster */
int rfs_meta_commit(struct super_block *sb);

int rfs_log_get_cluster(struct inode *inode, unsigned int *new_clu);

int rfs_log_segment_add(struct super_block *sb,unsigned int, unsigned int);
int rfs_log_update_segment(struct inode *inode);

int rfs_log_commit_mrc(struct super_block *sb, struct log_DEALLOC_info *ldi);
int rfs_log_max_chunk(struct super_block *sb);
int rfs_log_location_in_segment_list(struct super_block *sb, 
		struct log_DEALLOC_info *ldi);

/* called by logreplay */
int __log_replay(struct super_block *sb);
#ifdef CONFIG_RFS_FS_XATTR
/* called by xattr */
int rfs_log_xattr_set(struct inode *inode, struct log_XATTR_info *lxi);
#endif

#define DEBUG_print_logfile(sb)		do { } while(0)

#endif /* __KERNEL__ */

#endif /* _LINUX_RFS_FS_H */

