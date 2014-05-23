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
 *  @file	fs/rfs/rfs.h
 *  @brief	local header file for rfs
 *
 *
 */

#ifndef _RFS_H
#define _RFS_H

#include <linux/sched.h>
#include <linux/version.h>

#define RFS_NAME_STRING		"RFS"
#define RFS_VERSION_STRING	CONFIG_RFS_VERSION
#define	RFS_VERSION_MAJOR	1
#define	RFS_VERSION_MIDDLE	3
#define	RFS_VERSION_MINOR	1
#define RFS_VERSION_PATCH	0
#define RFS_VERSION_BUILD	72

/*
 *  kernel version macro
 */
#undef RFS_FOR_2_4
#undef RFS_FOR_2_6
#undef RFS_FOR_2_6_10
#undef RFS_FOR_2_6_16
#undef RFS_FOR_2_6_17
#undef RFS_FOR_2_6_18
#undef RFS_FOR_2_6_19
#undef RFS_FOR_2_6_20
#undef RFS_FOR_2_6_22
#undef RFS_FOR_2_6_23
#undef RFS_FOR_2_6_24
#undef RFS_FOR_2_6_27
#undef RFS_FOR_2_6_29

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#define RFS_FOR_2_6		1
#else
#define RFS_FOR_2_4		1
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30)
#define RFS_FOR_2_6_30		1
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
#define RFS_FOR_2_6_29		1
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
#define RFS_FOR_2_6_27		1
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
#define RFS_FOR_2_6_24		1
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23)
#define RFS_FOR_2_6_23		1
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
#define RFS_FOR_2_6_22		1
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
#define RFS_FOR_2_6_20		1
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
#define RFS_FOR_2_6_19		1
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
#define RFS_FOR_2_6_18		1
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 17)
#define RFS_FOR_2_6_17		1
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 16)
#define RFS_FOR_2_6_16          1
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 10)
#define RFS_FOR_2_6_10		1
#endif

#ifdef CONFIG_RFS_FS_XATTR
#define CONFIG_RFS_FS_FULL_PERMISSION
#endif

typedef enum rfs_lock_type {
	RFS_FAT_LOCK,
	RFS_LOG_LOCK,
} rfs_lock_type_t;

/*
 * for locking
 */
#define fat_lock(sb)		rfs_down(RFS_SB(sb)->fat_mutex, RFS_FAT_LOCK)
#define fat_unlock(sb)		rfs_up(RFS_SB(sb)->fat_mutex, RFS_FAT_LOCK)
#define init_fat_lock(sb)	rfs_init_mutex(RFS_SB(sb)->fat_mutex, sb)

#define lock_log(sb)		rfs_down(RFS_LOG_I(sb)->log_mutex, RFS_LOG_LOCK)
#define unlock_log(sb)		rfs_up(RFS_LOG_I(sb)->log_mutex, RFS_LOG_LOCK)
#define init_log_lock(sb)	rfs_init_mutex(RFS_LOG_I(sb)->log_mutex, sb)
#define get_log_lock_depth(sb)	rfs_get_lock_depth(RFS_LOG_I(sb)->log_mutex)
#define get_log_lock_owner(sb)	rfs_get_lock_owner(RFS_LOG_I(sb)->log_mutex)

#define IS_LOG_LOCK(type)	((type) == RFS_LOG_LOCK)

/* 
 * for debugging
 */
#define DL0        (0)     /* Quiet   */
#define DL1        (1)     /* Audible */
#define DL2        (2)     /* Loud    */
#define DL3        (3)     /* Noisy   */

#define DPRINTK(format, args...)					\
do {									\
	printk(KERN_ERR "%s[%d]: " format, __func__ , __LINE__, ##args);\
} while (0)

#undef DEBUG

/* handle error */
#define RFS_CRASH_MARK          0xDEAD
void rfs_handle_crash(struct super_block *sb, const char * function, int line,
		const char * fmt, ...);

#ifdef CONFIG_RFS_FAT_DEBUG

#define DEBUG(n, format, arg...) 					\
do {									\
       	if (n <= CONFIG_RFS_FAT_DEBUG_VERBOSE) {       		 	\
		printk("%s[%d]: " format "\n",				\
				 __func__, __LINE__, ##arg);		\
       	}								\
} while(0)

#define RFS_BUG(format, args...)					\
do {									\
	DPRINTK(format, ##args);					\
	BUG();								\
} while (0)

#define RFS_BUG_ON(condition)						\
do {									\
	BUG_ON(condition);						\
} while (0)

#else

#define DEBUG(n, arg...)	do { } while (0)
#define RFS_BUG(format, args...)	DPRINTK(format, ##args)
#define RFS_BUG_ON(condition)		do { } while (0)	

#endif /* CONFIG_RFS_FAT_DEBUG */

/* always make crash */
#define RFS_BUG_CRASH(sb, fmt, args...)					\
do {									\
	rfs_handle_crash(sb, __func__, __LINE__, fmt, ##args);		\
} while (0)

/* dump function for debugging */
void dump_bh(struct super_block *sb, struct buffer_head *bh, const char *fmt, ...);
void dump_inode(struct inode *inode);
void dump_entry(struct super_block *sb, struct rfs_dir_entry *ep, struct buffer_head *bh, loff_t offset, const char *fmt, ...);

#define DATA_EXPIRES(j)		((j) + CONFIG_RFS_LOG_WAKEUP_DELAY)

#define IS_INVAL_CLU(sbi, clu)						\
	((clu < VALID_CLU || clu >= (sbi)->num_clusters) ? 1 : 0)

/*
 * File types (special file)
 */
#define IS_SYMLINK(ep)		((ep->cmsec & SPECIAL_MASK) == SYMLINK_MARK)
#define IS_SOCKET(ep)		((ep->cmsec & SPECIAL_MASK) == SOCKET_MARK)
#define IS_FIFO(ep)		((ep->cmsec & SPECIAL_MASK) == FIFO_MARK)

#define CHECK_RFS_INODE(inode, errno)					\
do {									\
	if (inode->i_ino != ROOT_INO) {					\
		if ((RFS_I(inode)->p_start_clu >= 			\
			RFS_SB(inode->i_sb)->num_clusters) ||		\
		    (RFS_I(inode)->start_clu < VALID_CLU)) {		\
			RFS_BUG("out of range (%u, %u, %u)",		\
				RFS_I(inode)->index,			\
				RFS_I(inode)->p_start_clu,		\
				RFS_I(inode)->start_clu);		\
			return errno;					\
		}							\
	}								\
} while (0)

#ifdef RFS_FOR_2_6_27
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif

struct rfs_semaphore {
	struct semaphore mutex;
	pid_t owner;
	int depth;
	struct super_block *sb;
};

/*
 * Function prototypes
 */
#ifdef CONFIG_PROC_FS
int init_rfs_proc(void);
void exit_rfs_proc(void);
#endif

#ifdef CONFIG_RFS_FAST_SEEK
void rfs_free_fast_seek(struct rfs_inode_info *);
#else
#define rfs_free_fast_seek(rii)	do { } while(0)
#endif

/* check logfile's corruption */
int sanity_check_log(struct super_block *);

/**
 * down the mutex
 * @param lock    a specific lock structure
 * @param type    lock type
 * @return        return the modified lock depth
*/
static inline int rfs_down(struct rfs_semaphore *lock, rfs_lock_type_t type)
{
	if ((lock->owner == current->pid) && (lock->depth >= 1)) {
		/* recursive lock */
		lock->depth++;
		DEBUG(DL3, "owner = %d depth = %d", lock->owner, lock->depth);
		return lock->depth;
	}

	/* first acquire */
	down(&lock->mutex);

#ifdef RFS_FOR_2_4
	if (IS_LOG_LOCK(type)) {
		/* register timer to avoid indefinite hang in wait_on_buffer */
		RFS_SB(lock->sb)->sleep_proc = current;
		RFS_SB(lock->sb)->timer.expires = DATA_EXPIRES(jiffies);
		add_timer(&RFS_SB(lock->sb)->timer);
	}
#endif

	lock->owner = current->pid;
	lock->depth++;

	DEBUG(DL3, "owner = %d depth = %d", lock->owner, lock->depth);

	return lock->depth;
}

/** 
 * up the mutex  
 * @param lock     a specific lock structure 
 * @param type     lock type
 * @return         return the modified lock depth
 */
static inline int rfs_up(struct rfs_semaphore *lock, rfs_lock_type_t type)
{
	if (lock->depth > 1) {
		lock->depth--;
	} else {
		DEBUG(DL3, "owner = %d depth = %d", lock->owner, lock->depth);

		lock->owner = -1;
		lock->depth--;

#ifdef RFS_FOR_2_4
		if (IS_LOG_LOCK(type))
			del_timer_sync(&RFS_SB(lock->sb)->timer);
#endif

		up(&lock->mutex);
	}
	return lock->depth;
}

#ifdef RFS_FOR_2_4
/** 
 * down data mutex of ordered transaction for kernel2.4
 * @param rfsi     a native inode info
 */
static inline void rfs_data_down(struct rfs_inode_info *rfsi)
{
	down(&rfsi->data_mutex);

	/* register timer to avoid indefinite hang in wait_on_buffer */
	rfsi->sleep_proc = current;
	rfsi->timer.expires = DATA_EXPIRES(jiffies);
	add_timer(&rfsi->timer);
}

/** 
 * up data mutex of ordered transaction for kernel2.4
 * @param rfsi     a private inode
 */
static inline void rfs_data_up(struct rfs_inode_info *rfsi)
{
	del_timer_sync(&rfsi->timer);
	up(&rfsi->data_mutex);
}
#endif

/**
 * init the mutex
 * @param lock	a specific lock structure
 * @param sb	super block
*/
static inline void rfs_init_mutex(struct rfs_semaphore *lock,
				  struct super_block *sb)
{
	init_MUTEX(&lock->mutex);
	lock->owner = -1;
	lock->depth = 0;
	lock->sb = sb;
}

/** 
 * get the current depth  
 * @param lock     a specific lock structure 
 * @return         return the current lock depth
 */
static inline int rfs_get_lock_depth(struct rfs_semaphore *lock)
{
	return lock->depth;
}

/**
 * get the current lock owner
 * @param lock     a specific lock structure
 * @return         return the current lock owner
 */
static inline int rfs_get_lock_owner(struct rfs_semaphore *lock)
{
	return lock->owner;
}

#define rfs_set_bh_bit(state, bh_state) do { }while (0)

extern int g_bh_hot_boundary;

inline static struct buffer_head * rfs_bread(struct super_block *sb,
		sector_t block, int rfs_state)
{
	struct buffer_head *bh;
	bh = sb_bread(sb, block);
	if (bh)
	{
		rfs_set_bh_bit(rfs_state, &bh->b_state);
	}

	return bh;
}

inline static struct inode *NEW_INODE(struct super_block *sb)
{
	struct inode *inode;
	struct rfs_inode_info *rii;

	inode = new_inode(sb);
	if (!inode)
		return NULL;

	rii = RFS_I(inode);

#ifdef CONFIG_RFS_FAST_SEEK
	/* initialize fast seek info */
	rii->fast_seek = NULL;
#endif
#ifdef CONFIG_RFS_FAST_LOOKUP
	rii->fast = 0;
#endif
	rii->start_clu = CLU_TAIL;
	rii->p_start_clu = CLU_TAIL;
	rii->mmu_private = 0;

	return inode;
}

#ifdef RFS_CLUSTER_CHANGE_NOTIFY
/* cluster usage chage notify callback function pointer */
extern FP_CLUSTER_USAGE_NOTIFY *gfp_cluster_usage_notify;
#endif

/*
 * MACRO for kernel specific functions
 */

/* MACROs for current information fsgid, fsuid, umask */
#ifdef RFS_FOR_2_6_29
#define rfs_current_fsgid()	(current_fsgid())
#define rfs_current_fsuid()	(current_fsuid())
#else   /* RFS_FOR_2_6_29 */
#define rfs_current_fsgid()	(current->fsgid)
#define rfs_current_fsuid()	(current->fsuid)
#endif  /* RFS_FOR_2_6_29 */

#ifdef RFS_FOR_2_6_30
#define rfs_current_umask()	(current_umask())
#else
#define rfs_current_umask()	(current->fs->umask)
#endif  /* RFS_FOR_2_6_30 */

/* sync single buffer */
#ifdef RFS_FOR_2_6
# define rfs_sync_dirty_buffer(bh)      sync_dirty_buffer(bh)

#else   /* RFS_FOR_2_4 */
inline static int __rfs_sync_dirty_buffer(struct buffer_head *bh)
{
	ll_rw_block(WRITE, 1, &(bh));
	wait_on_buffer(bh);
	return (!buffer_uptodate(bh))? -EIO: 0;
}
# define rfs_sync_dirty_buffer(bh)      __rfs_sync_dirty_buffer(bh)
#endif

/* sync pages in inode */
#ifdef RFS_FOR_2_6_16
# define rfs_sync_inode_pages(inode)    filemap_write_and_wait(inode->i_mapping)

#elif defined(RFS_FOR_2_6)
inline static int __rfs_sync_inode_pages(struct inode *inode)
{
	int ret = 0;

	if (inode->i_mapping->nrpages) {
		ret = filemap_fdatawrite(inode->i_mapping);
		if (ret == 0)
			ret = filemap_fdatawait(inode->i_mapping);

		if (ret)
			DEBUG(DL0, "filemap_fdatawrite/fdatawait fails(%d)", ret);
	}

	return ret;
}
# define rfs_sync_inode_pages(inode)    __rfs_sync_inode_pages(inode)

#else /* RFS_FOR_2_4 */
inline static int __rfs_sync_inode_pages(struct inode *inode)
{
	int ret = 0;

	rfs_data_down(RFS_I(inode));
	ret = fsync_inode_data_buffers(inode);
	rfs_data_up(RFS_I(inode));
	if (ret)
		DEBUG(DL0, "fsync_inode_data_buffers fails(%d)", ret);

	return ret;
}
# define rfs_sync_inode_pages(inode)    __rfs_sync_inode_pages(inode)
#endif

/**
 * for mark_buffer_dirty 
 */
#define rfs_mark_buffer_dirty(x, sb)	mark_buffer_tr_dirty(x, sb)

/**
 * for mark inode dirty
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
#define rfs_mark_inode_dirty(x)		mark_inode_dirty(x)
#else
#define rfs_mark_inode_dirty(inode)					\
do {									\
	if ((inode->i_state & (I_LOCK | I_NEW)) == (I_LOCK | I_NEW)) {	\
		DEBUG(DL0, "Don't make inode dirty when I_LOCK | I_NEW "\
				"bit is set.(under 2.6.24)");		\
	} else {							\
		mark_inode_dirty(inode);				\
	}								\
} while(0)
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24) */

#endif /* _RFS_H */
