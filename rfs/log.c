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
 *  @file	fs/rfs/log.c
 *  @brief	functions for logging
 *
 *
 */

#include <linux/bitops.h>
#include <linux/rfs_fs.h>
#include "rfs.h"
#include "log.h"

/*
 * Define Internal Macros
 */
#define RFS_LOG_MODE_ENABLE    0       /* TODO: description */

#define GET_CLUSTER_FROM_BLOCK(sb, x)					\
((((x) - RFS_SB(sb)->data_start) >> RFS_SB(sb)->blks_per_clu_bits) +	\
	VALID_CLU)

#ifdef CONFIG_RFS_FAT_DEBUG
#define CHECK_MUTEX(X, Y)						\
do {									\
	if ((X) != (Y)) {						\
		RFS_BUG("RFS-log : lock is broken %d "			\
			"(should be %d)\n",				\
			(int)(X), (int)(Y));				\
	}								\
} while (0)
#else
#define CHECK_MUTEX(...)	do { } while (0)
#endif

/*
 * Define Internal Functions
 */
static int __log_open(struct super_block *sb);

static int __make_logfile(struct super_block *sb);
static void __log_start_nolock(struct super_block *sb, unsigned int type,
		struct inode *inode);
static int __pre_alloc_clusters(struct inode *inode);
static int __commit_deferred_tr(struct super_block *sb, unsigned long ino);

static int __register_log_operation(struct super_block *sb, int mode);

/*
 * Operation sets for log enabled mode
 */

/* log enabled mode operations*/
static int __log_mark_end(struct super_block *sb, unsigned int sub_type);
static int __log_read(struct super_block *sb, unsigned int isec);
static int __log_write(struct super_block *sb);
static int __log_mwrite(struct super_block *sb, int count, struct buffer_head **bhs);
static int __log_read_ahead(struct super_block *sb, unsigned int isec, 
		int count, struct buffer_head **bhs);
static int __log_dealloc_chain(struct super_block *sb, struct log_DEALLOC_info *,
		unsigned int *);
static int __log_move_chain(struct super_block *sb, struct log_FAT_info *);
static int __log_move_chain_pp(struct super_block *sb, unsigned int, unsigned int);

/**
 * mark buffer dirty & register buffer to the transaction if we are inside transaction
 * @param bh	buffer 
 * @param sb	super block 
 */
void mark_buffer_tr_dirty(struct buffer_head *bh, struct super_block *sb)
{
	/*
	 * In kernel 2.6,
	 * mark_buffer_dirty() set the dirty bit against the buffer and
	 * set its backing page dirty.
	 * Then tag the page as dirty in its address_space's radix tree
	 * and then attach the address_space's inode to its superblock's
	 * dirty inode list.
	 * Inodes in dirty list will be written to the device when sync volume.
	 *
	 * In kernel 2.4, mark_buffer_dirty() just set the buffer dirty.
	 * Therefore, RFS attaches the dirty buffer to the tr_inode
	 * to sync dirty buffers that any inode doesn't include.
	 */

#ifdef RFS_FOR_2_4
	struct inode *tr_inode = &(RFS_LOG_I(sb)->tr_buf_inode);

	mark_buffer_dirty(bh);

	if (RFS_LOG_I(sb) && (get_log_lock_owner(sb) == current->pid)) {
		/* inside transaction */
		buffer_insert_inode_queue(bh, tr_inode);
	}
#endif
#ifdef RFS_FOR_2_6
	mark_buffer_dirty(bh);
#endif
}

/**
 * mark inode dirty & write inode if we are inside transaction
 * @param inode	inode 
 */
void rfs_mark_inode_dirty_in_tr(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;

	rfs_mark_inode_dirty(inode);

	if (RFS_LOG_I(sb) && (get_log_lock_owner(sb) == current->pid)) {
		/* inside transaction */
		rfs_sync_inode(inode, 0, 0);
	}
}

/**
 * commit rfs meta-data
 * @param sb super block
 */
int rfs_meta_commit(struct super_block *sb)
{
#ifdef RFS_FOR_2_4
	struct inode *tr_inode = &(RFS_LOG_I(sb)->tr_buf_inode);
#endif
	struct inode *inode = RFS_LOG_I(sb)->inode;
	int err = 0, ret = 0;
	int data_commit = 0;

	/*
	 * sync fcache
	 * (just make buffers dirty in fcache)
	 */
	fat_lock(sb);
	rfs_fcache_sync(sb, 0);
	fat_unlock(sb);

	/* write & truncate should explicitly write FAT entry */
	if (tr_pre_alloc(sb) && inode)
	{
		data_commit = 0;

#ifdef CONFIG_RFS_SYNC_ON_CLOSE
		/*
		 * If SYNC_ON_CLOSE is configured,
		 * RFS should guarantee user data on close.
		 * 1) RFS sync NULL data created by truncate_f here,
		 * because truncate(filepath) do not open/close its file.
		 * 2) dirty data by write will be written on close.
		 */
		if (RFS_LOG_I(sb)->type == RFS_LOG_TRUNCATE_F)
			data_commit = 1;
#endif
	}
	else if ((RFS_LOG_I(sb)->type == RFS_LOG_TRUNCATE_B) && inode)
	{
		/* write dirty data to commit entry's size */
		data_commit = 1;
	}
	else if ((RFS_LOG_I(sb)->type == RFS_LOG_SYMLINK) &&
			(RFS_LOG_I(sb)->symlink_inode))
	{
		/* link path is also meta-data in RFS */
		data_commit = 1;
		inode = RFS_LOG_I(sb)->symlink_inode;
	}
	else
	{
		/* do not sync inode */
		data_commit = -1;
	}

	/*
	 * If data_commit is set,
	 * rfs_sync_inode writes dirty data and updates size
	 */
	if (data_commit >= 0) {
		ret = rfs_sync_inode(inode, data_commit, 0);
		if (ret) {
			DEBUG(DL0, "rfs_sync_inode%s fails(%d)",
					(data_commit)? "(data_commit)":"", ret);
		}
	}

	/*
	 * sync dirty buffers in volume (metadata including fcache)
	 */
#ifdef RFS_FOR_2_6
	err = sync_blockdev(sb->s_bdev);
#else
	err = fsync_inode_buffers(tr_inode);
#endif
	if (err) {
		DEBUG(DL0, "sync volume fails(%d)", err);
		ret = (!ret)? err: ret;
	}

	return ret;
}

/**
 * Does transaction deferredly commit?
 * @param sb super block
 * @return if transaction is feasible for deferred commit, then return TRUE,
 *	otherwise return FALSE.
 */
int tr_deferred_commit(struct super_block *sb)
{
	if (RFS_LOG_I(sb)->type == RFS_LOG_WRITE)
		return TRUE;

	return FALSE;
}

/**
 * Does transaction need pre allocation?
 * @param sb super block
 * @return if transaction is feasible for pre-allocation, then return TRUE,
 *	otherwise return FALSE.
 */
int tr_pre_alloc(struct super_block *sb)
{
	/* must include all tr_deferred_commit() */
	if (RFS_LOG_I(sb)->type == RFS_LOG_WRITE ||
			RFS_LOG_I(sb)->type == RFS_LOG_TRUNCATE_F)
		return TRUE;

	return FALSE;
}

/*****************************************************************************/
/* log init/exit functions						     */
/*****************************************************************************/

struct rfs_log_operations rfs_log_op_enabled =
{
	.log_dealloc_chain	= __log_dealloc_chain,
	.log_mark_end		= __log_mark_end,
	.log_read		= __log_read,
	.log_write		= __log_write,
	.log_read_ahead		= __log_read_ahead,
	.log_mwrite		= __log_mwrite,
	.log_move_chain		= __log_move_chain,
	.log_move_chain_pp	= __log_move_chain_pp
};

/*
 * register operation set
 */
static int __register_log_operation(struct super_block *sb, int mode)
{
	struct rfs_log_info *rli = RFS_LOG_I(sb);
	int ret = 0;

	if (unlikely(NULL == rli))
	{
		ret = -EIO;
		goto out;
	}

	switch(mode)
	{
	case RFS_LOG_MODE_ENABLE:
		rli->operations = &rfs_log_op_enabled;
		break;
	default:
		rli->operations = NULL;
		ret = -EIO;
		break;
	}
out:
	return ret;

}
/**
 * init logfile
 * @param sb super block
 * @return 0 on success, errno on failure
 */
static int init_logfile(struct super_block *sb)
{
	unsigned int i;
	int err;

	for (i = 0; i < RFS_LOG_MAX_COUNT; i++) 
	{
		if (RFS_LOG_I(sb)->operations->log_read(sb, i))
		{
			DPRINTK("log_read fail");
			return -EIO;
		}
		memset(RFS_LOG(sb), (unsigned char) RFS_LOG_NONE, SECTOR_SIZE);
		mark_buffer_dirty(RFS_LOG_I(sb)->bh);
	}

	brelse(RFS_LOG_I(sb)->bh);
	RFS_LOG_I(sb)->bh = NULL;
	RFS_LOG_I(sb)->log = NULL;

	err = rfs_sync_vol(sb);
	if (err)
	{
		DEBUG(DL0, "Err(%d) in rfs_sync_vol\n", err);
	}
	return err;
}

/**
 * initialize log
 * @param sb super block
 * @return 0 on success, errno on failure
 */

int rfs_log_init(struct super_block *sb)
{
	int ret;

	/* check log record size */
	if (sizeof(struct rfs_trans_log) > SECTOR_SIZE) 
	{
		DPRINTK("RFS-log : Current log record is %u bytes."
			"Log record must be 512 bytes at most\n",
			sizeof(struct rfs_trans_log));
		return -EINVAL;
	}

	if ((ret = __log_open(sb)) == -ENOENT) 
	{ /* there is no logfile */
		if (sb->s_flags & MS_RDONLY)
		{
#ifdef RFS_RDONLY_MOUNT
			RFS_SB(sb)->log_info = NULL;
			RFS_SB(sb)->use_log = FALSE;
			return 0;
#else
			DPRINTK("RFS-log(%d) : Couldn't make log file."
				" RFS cannot mount without logfile\n", ret);
			return -EROFS;
#endif

		}


		if ((ret = __make_logfile(sb))) 
		{
			DPRINTK("RFS-log(%d) : Couldn't make log file.\n", ret);
#if defined(RFS_NOLOG_MOUNT)
#elif defined(RFS_RDONLY_MOUNT)
			/* when rfs can't make logfile, mount as read-only */
			if (ret == -ENOSPC) 
			{
				DPRINTK("There is no space to create logfile."
					" RFS mount as read-only.\n");
				sb->s_flags |= MS_RDONLY;
				RFS_SB(sb)->log_info = NULL;
				RFS_SB(sb)->use_log = FALSE;
				ret = 0;
			}
#else
			if (ret == -ENOSPC) 
			{
				DPRINTK(" RFS cannot mount without logfile");
				DPRINTK("Please check there is"
					" enough space to create logfile"
					" (MIN : %u bytes)\n",
					RFS_LOG_MAX_COUNT * SECTOR_SIZE);
			}
			else
			{
				DPRINTK("RFS-log(%d):can't make log file.\n",
						ret);
				return ret;
			}
#endif
			return ret;
		}

		/* open again */
		if ((ret = __log_open(sb))) 
		{
			/* I/O error */
			DPRINTK("RFS-log(%d): Although it was made"
				" can't open log file[%02x:%02x]\n", ret, 
				MAJOR(sb->s_dev), MINOR(sb->s_dev));
			return -EIO;
		}

		/* initialize log file to prevent confusing garbage value */
		if ((ret = init_logfile(sb))) 
		{
			/* I/O error */
			DPRINTK("RFS-log(%d) : Cannot init logfile\n", ret);
			goto out;
		}
	} 
	else if (ret) 
	{ /* I/O error */
		DPRINTK("RFS-log(%d) : Can't open log file\n", ret);
		return -EIO;
	} 
	else 
	{ /* success in openning log */
		struct inode *inode = RFS_LOG_I(sb)->self_inode;
		struct buffer_head *bh = NULL;
		struct rfs_dir_entry *ep = NULL;
		struct qstr d_name;

		if (sb->s_flags & MS_RDONLY)
		{
			iput(RFS_LOG_I(sb)->self_inode);
			RFS_LOG_I(sb)->self_inode = NULL;
			RFS_SB(sb)->use_log = FALSE;
			return 0;
		}

		ret = __log_replay(sb);
		if (ret) 
		{
			/* I/O error */
			DPRINTK("RFS-log(%d) : Fail to replay log\n", ret);
			goto out;
		}

		/* remaining segment deletion */
		ep = get_entry_with_cluster(sb, RFS_I(inode)->p_start_clu,
				RFS_I(inode)->index, &bh, inode);
		if (IS_ERR(ep)) {
			ret = PTR_ERR(ep);
			brelse(bh);
			goto out;
		}

		SET32(ep->size, RFS_LOG_MAX_SIZE_IN_BYTE);

		/*
		 * only if logfile is greater than predefined size,
		 * trucate extra chain with transaction
		 */
		d_name.name = (const unsigned char*)RFS_LOG_FILE_NAME;
		d_name.len = RFS_LOG_FILE_LEN;

		ret = rfs_fill_inode(RFS_SB(sb)->root_inode, inode, ep, 
				RFS_I(inode)->index, 0, bh, &d_name);
		brelse(bh);
		if (ret)
		{
			DPRINTK("Err(%d) in truncating logfile\n", ret);
		}

	}

out:
	/* release useless inode of logfile */
	iput(RFS_LOG_I(sb)->self_inode);
	RFS_LOG_I(sb)->self_inode = NULL;

	return ret;
}

/**
 * destroy loginfo and release memory
 *
 * @param sb super block
 */
void rfs_log_release(struct super_block *sb)
{
	if (unlikely(!RFS_LOG_I(sb)))
		return;

	/*
	 * commit deferred transaction for pre-allocation
	 * and write commit mark
	 */
	__commit_deferred_tr(sb, 0);
	/* release buffer head for current log block */
	brelse(RFS_LOG_I(sb)->bh);

	/* free memory for loginfo */
	kfree(RFS_LOG_I(sb)->log_mutex);
	kfree(RFS_LOG_I(sb));

	/* clear superblock's field */
	RFS_SB(sb)->log_info = NULL;
}

/*****************************************************************************/
/* log open function							     */
/*****************************************************************************/

/**
 * open logfile
 * @param sb super block
 * @return 0 on success, errno on failure
 */
static int __log_open(struct super_block *sb)
{
	struct inode *root_dir;
	struct inode *inode;
	struct dentry *root_dentry;
	struct rfs_dir_entry *ep;
	struct buffer_head *bh = NULL;
	struct rfs_log_info *rli;
	unsigned int i;
	unsigned int iblock;
	int index;
	int ret = 0;

	root_dentry = sb->s_root;
	root_dir = root_dentry->d_inode;

	index = find_entry(root_dir, RFS_LOG_FILE_NAME, &bh, TYPE_ALL);
	if (index < 0) 
	{
		DEBUG(DL0, "can't find log (%d) [%02x:%02x]", index, 
				MAJOR(sb->s_dev), MINOR(sb->s_dev));
		ret = -ENOENT;
		goto rel_bh;
	}

	ep = get_entry(root_dir, (unsigned int) index, &bh);
	if (IS_ERR(ep)) 
	{
		/* I/O error */
		DEBUG(DL0, "can't get entry in root_dir");
		ret = PTR_ERR(ep);
		goto rel_bh;
	}

	/* basic check of logfile
	 * check logfiles's tyep, start_clu, size 
	 */
	if ((rfs_entry_type(ep) != TYPE_FILE) ||
			(START_CLUSTER(RFS_SB(sb), ep) == 0) ||
			(GET32(ep->size) == 0))
	{
		DEBUG(DL0, "logfile is not proper");
		ret = -EFAULT;
		goto rel_bh;	
	}

	/* get a new inode */
	if (!(inode = NEW_INODE(root_dir->i_sb))) 
	{
		/* memory error */
		DEBUG(DL0, "Can not alloc inode for logfile");
		ret = -ENOMEM;
		goto rel_bh;
	}

	/* Note. minimum setting for rfs_bmap() */
	RFS_I(inode)->start_clu = START_CLUSTER(RFS_SB(sb), ep);
	RFS_I(inode)->mmu_private = RFS_LOG_MAX_SIZE_IN_BYTE;
	RFS_I(inode)->p_start_clu = RFS_I(root_dir)->start_clu;
	RFS_I(inode)->index = index;
	rfs_invalidate_hint(inode);

	rli = rfs_kmalloc(sizeof(struct rfs_log_info), GFP_KERNEL, NORETRY);
	if (!rli) 
	{
		/* memory error */
		DEBUG(DL0, "memory allocation error");
		ret = -ENOMEM;
		goto err_iput;
	}

	/* log size is 9th power of 2 (512B) */
	rli->secs_per_blk = sb->s_blocksize >> 9;
	rli->secs_per_blk_bits =
		(unsigned int) ffs(rli->secs_per_blk) - 1;

	DEBUG(DL3, "sectors per block : %u its bit : %u\n",
			rli->secs_per_blk, rli->secs_per_blk_bits);

	for (i = 0; i < RFS_LOG_MAX_COUNT; i++) 
	{
		iblock = i >> rli->secs_per_blk_bits;
		ret = rfs_bmap(inode, (long) iblock, &(rli->blocks[i]));
		if (ret) 
		{
			kfree(rli);
			goto err_iput;
		}
		DEBUG(DL2, "%uth : %llu", i, (unsigned long long) rli->blocks[i]);
	}
	rli->type = RFS_LOG_NONE;
	rli->sequence = 1;
	rli->bh = NULL;
	rli->inode = NULL;
	rli->isec = 0;

	/* init fields for deferred commit */
	rli->alloc_index = 0;
	rli->numof_pre_alloc = 0;

	rli->need_mrc = FALSE;

	/*
	 * start cluster can pick up from entry, but last cluster
	 * is calculated from last block number.
	 */
	rli->l_start_cluster = RFS_I(inode)->start_clu;
	rli->l_last_cluster = GET_CLUSTER_FROM_BLOCK(sb,
			rli->blocks[RFS_LOG_MAX_COUNT - 1]);

	DEBUG(DL1, "logfile range [%d:%d], last block %u",
			rli->l_start_cluster,
			rli->l_last_cluster,
			(int) rli->blocks[RFS_LOG_MAX_COUNT - 1]);

	/* segment (previously called as candidate) is removed during mount */
	rli->c_start_cluster = CLU_TAIL;
	rli->c_last_cluster = CLU_TAIL;

	rli->log_mutex = 
		rfs_kmalloc(sizeof(struct rfs_semaphore), GFP_KERNEL, NORETRY);

	if (!(rli->log_mutex))
	{
		/* memory error */
		DEBUG(DL0, "memory allocation failed");
		kfree(rli);
		ret = -ENOMEM;
		goto err_iput;
	}
	INIT_LIST_HEAD(&(rli->c_list));

	rli->self_inode = inode;
	rli->symlink_inode = NULL;

#ifdef RFS_FOR_2_4
	INIT_LIST_HEAD(&(rli->tr_buf_inode.i_dirty_buffers));
#endif
	RFS_SB(sb)->log_info = (void *) rli;

	init_log_lock(sb);

	ret = __register_log_operation(sb, RFS_LOG_MODE_ENABLE);
	if (ret)
	{
		RFS_BUG("cannot regsiter log operation");
	}

rel_bh:
	brelse(bh);
	return ret;

err_iput:
	iput(inode);
	brelse(bh);
	return ret;
}

/*****************************************************************************/
/* logging functions							     */
/*****************************************************************************/

/**
 * force to commit write transaction
 * @param sb	super block
 * @param inode	inode for target file or NULL for volume sync
 * @return 0 on success, errno on failure
 * @pre It can be called in middle of transaction, so 
 *	it should get a lock for committing transaction
 *
 * Never sleep holding super lock. Log lock can protects RFS key data
 */
int rfs_log_force_commit(struct super_block *sb, struct inode *inode)
{
	int ret;

	if (inode && (inode != RFS_LOG_I(sb)->inode))
		return 0;

	lock_log(sb);

	/* volume sync */
	if (!inode) {
		unlock_super(sb);
		ret = __commit_deferred_tr(sb, 0);

		/* sb->s_dirt is must be synchronous with write transaction */
		sb->s_dirt = 0;

		/* WARNING!!
		 * do not change the lock order to avoid deadlock
		 */
		unlock_log(sb);
		lock_super(sb);
	} else {
		/* inode sync */
		ret = __commit_deferred_tr(sb, inode->i_ino);
		unlock_log(sb);
	}

	return ret;
}

/**
 * release remaining pre-allocations
 * @param sb	super block
 * @return 0 on success, errno on failure
 */
static int release_pre_alloc(struct super_block *sb)
{
	struct rfs_log_info *rli = RFS_LOG_I(sb);
	int ret = 0;

	if (rli->alloc_index >= rli->numof_pre_alloc)
		goto out;

	fat_lock(sb);


	/* clusters still remained free as allocated from fat table */
	RFS_SB(sb)->search_ptr = rli->pre_alloc_clus[rli->alloc_index];
	fat_unlock(sb);
out:
	rli->alloc_index = rli->numof_pre_alloc = 0;
	return ret;
}

/**
 * commit deferred transaction for pre-allocation
 * @param sb	super block
 * @param ino	inode number (If zero, volume sync)
 * @return 0 on success, errno on failure
 */
static int __commit_deferred_tr(struct super_block *sb, unsigned long ino)
{
	int ret = 0;

	/*
	 * commit deffered transaction
	 * if volume sync or loginfo's inode and requested inode are the same.
	 * : 1. release pre-allocated clusters
	 *   2. commit metadata (fcache and superblock's dirty buffers, sometimes inode)
	 *   3. write commit mark to logfile
	 *   4. clear dirty flag of superblock
	 */

	if (tr_deferred_commit(sb) && 
		((!ino) || (RFS_LOG_I(sb)->inode && 
			(RFS_LOG_I(sb)->inode->i_ino == ino)))) 
	{
		ret = release_pre_alloc(sb);
		if (ret) 
		{	
			/* I/O error */
			DEBUG(DL0, "release_pre_alloc fails(%d)", ret);
			goto out;
		}

		ret = rfs_meta_commit(sb);
		if (ret) 
		{
			DEBUG(DL0, "rfs_meta_commit fails(%d)", ret);
			goto out;
		}

		if (RFS_LOG_I(sb)->operations->
				log_mark_end(sb, RFS_SUBLOG_COMMIT)) 
		{
			/* I/O error */
			DPRINTK("RFS-log : Couldn't commit write"
				" transaction\n");
			ret = -EIO;
			goto out;
		}
		sb->s_dirt = 0;
	}

out:
	return ret;
}

/**
 * sync metadata & write MRC(Multi-block Record Commit).
 *
 * @param sb	super block
 * @param ldi	log dealloc info written with mrc
 * @return 0 on success, errno on failure
 */
int rfs_log_commit_mrc(struct super_block *sb, struct log_DEALLOC_info *ldi)
{
#ifdef RFS_FOR_2_4
	struct inode *tr_inode = &(RFS_LOG_I(sb)->tr_buf_inode);
#endif
	struct rfs_log_info *rli = RFS_LOG_I(sb);
	int ret = 0;

	/*
	 * sync fcache
	 * (just make buffers dirty in fcache)
	 */
	fat_lock(sb);
	rfs_fcache_sync(sb, 0);
	fat_unlock(sb);

	/*
	 * sync dirty buffers in volume (metadata including fcache)
	 */
#ifdef RFS_FOR_2_6
	ret = sync_blockdev(sb->s_bdev);
#else
	ret = fsync_inode_buffers(tr_inode);
#endif
	if (ret) 
	{
		DEBUG(DL0, "sync volume fails(%d)", ret);
		goto out;
	}

	if (rli->operations->log_read(sb, RFS_LOG_I(sb)->isec))
	{
		DEBUG(DL0, "log_read(%u) fails", RFS_LOG_I(sb)->isec);
		ret = -EIO;
		goto out;
	}

	/* write MRC commit log */
	SET32(RFS_LOG(sb)->type, 
			MK_LOG_TYPE(RFS_LOG_I(sb)->type, RFS_SUBLOG_MRC));

	SET32(rli->log->log_dealloc.pdir, ldi->pdir);
	SET32(rli->log->log_dealloc.entry, ldi->entry);
	SET32(rli->log->log_dealloc.f_filesize, ldi->s_filesize);
	SET32(rli->log->log_dealloc.f_lastcluster, ldi->s_new_last);

	SET32(rli->log->log_dealloc.l_prev_clu, ldi->l_prev_clu);
	SET32(rli->log->log_dealloc.l_next_clu, ldi->l_next_clu);

	SET32(rli->log->log_dealloc.nr_chunk, 0);
	SET32(rli->log->log_dealloc.next_chunks, ldi->next_clu);

	if (rli->operations->log_write(sb))
	{
		DEBUG(DL0, "log_write fails");
		ret = -EIO;
		goto out;
	}

	sb->s_dirt = 0;
	rli->need_mrc = FALSE;

out:
	return ret;
}

/**
 * get a lock and mark start of transaction
 * @param sb		super block
 * @param log_type	log type
 * @param inode		relative inode
 * @return 0 on success, errno on failure
 * @post locking log has been remained until trasaction ends
 *
 * pre-allocation is commited if there is no further write to the same file
 *	as that of previous transaction
 */
int rfs_log_start(struct super_block *sb, unsigned int log_type,
		struct inode *inode)
{
	int ret;

	if (!RFS_LOG_I(sb)) 
	{
		RFS_BUG_CRASH(sb, "RFS-log : System crashed\n");
		return -EIO;
	}

	/* check nested transaction */
	/* Any process cannot set owner to own pid without a mutex */
	if (lock_log(sb) >= 2) 
	{
		/* recursive acquisition */
		return 0;
	}

	if (tr_deferred_commit(sb)) 
	{
		if ((log_type == RFS_LOG_I(sb)->type) &&
				(RFS_LOG_I(sb)->inode == inode)) 
		{
			/* write data in the same file */
			return 0;
		}

		DEBUG(DL2, "prev : write, cur : %d, ino : %lu", 
				log_type, inode->i_ino);

		/* different transaction; flush prev write-transaction */
		ret = __commit_deferred_tr(sb, RFS_LOG_I(sb)->inode->i_ino);
		if (ret) 
		{
			/* I/O error */
			DEBUG(DL0, "__commit_deffered_tr fails(%d)", ret);
			goto err;
		}
	}

	/*
	 * starting new transaction
	 */
	__log_start_nolock(sb, log_type, inode);

	return 0;

err:
	unlock_log(sb);

	return ret;
}

/**
 * closing current transaction
 * : write commit mark (end of transaction) and release log lock
 *
 * @param sb		super block
 * @param result	result of transaction (0 : success, others : failure)
 * @return 0 on success, errno on failure
 * @pre trasaction must have started
 *
 * mark EOT and flush it unless transaction is for pre-allocation.
 */
int rfs_log_end(struct super_block *sb, int result)
{
	unsigned int sub_type = RFS_SUBLOG_COMMIT;
	int ret = 0;

	if (!RFS_LOG_I(sb)) 
	{
		RFS_BUG("RFS-log : System crashed\n");
		return -EIO;
	}

	/* recursive lock */
	/* Any process cannot increase ref_count without a mutex */
	if (get_log_lock_depth(sb) > 1) 
	{
		unlock_log(sb);
		DEBUG(DL2, "release self recursive lock (ref: %d)",
			get_log_lock_depth(sb));
		return 0;
	}

	/*
	 * 'empty_tr' means that transaction did not change any metadata
	 */
	if (is_empty_tr(sb)) 
	{
		DEBUG(DL3, "empty transaction");

		brelse(RFS_LOG_I(sb)->bh);
		RFS_LOG_I(sb)->bh = NULL;
		RFS_LOG_I(sb)->log = NULL;
		RFS_LOG_I(sb)->type = RFS_LOG_NONE;

		goto rel_lock;
	}

	DEBUG(DL2, "result : %d, type : %d", result, RFS_LOG_I(sb)->type);

	/* mark failure of current transaction */
	if (result)
		sub_type = RFS_SUBLOG_ABORT;

	/*
	 * When closing deferred transaction, just go out.
	 * The deferred tr(write) will be handled at the next transaction's start
	 */
	if (tr_deferred_commit(sb) && (sub_type == RFS_SUBLOG_COMMIT)) 
	{
		DEBUG(DL2, "deferred commit");
		ret = 0;
		sb->s_dirt = 1;
		goto rel_lock;
	}

	/* when current transaction is write or truncate_f */
	if (tr_pre_alloc(sb)) 
	{
		ret = release_pre_alloc(sb);
		if (ret) /* I/O error */
		{
			DEBUG(DL0, "release_per_alloc fails(%d)", ret);
			goto rel_lock;
		}
	}

	/* write metadata */
	ret = rfs_meta_commit(sb);
	if (ret)
	{
		DEBUG(DL0, "rfs_meta_commit fails(%d)", ret);
		goto rel_lock;
	}

	ret = RFS_LOG_I(sb)->operations->log_mark_end(sb, sub_type);
	if (ret < 0)
		DEBUG(DL0, "log_mark_end fails(%d)", ret);

rel_lock:
	CHECK_MUTEX(get_log_lock_depth(sb), 1);

	unlock_log(sb);

#ifdef RFS_CLUSTER_CHANGE_NOTIFY
	call_cluster_usage_notify(sb);
#endif

	return ret;
}

/**
 * logging for entry to be built
 * @param sb	super block
 * @param lei	loginfo for entry (pdir, entry idx, numof_entry)
 * @return 0 on success, errno on failure
 * @pre trasaction must have been started
 */
int rfs_log_build_entry(struct super_block *sb, struct log_ENTRY_info *lei)
{
	struct rfs_log_info *rli = RFS_LOG_I(sb);

	if (!RFS_LOG_I(sb))
	{ 
		/* special case : log file is created right after format */
		return 0;
	}

	if (rli->operations->log_read(sb, RFS_LOG_I(sb)->isec))
	{
		DEBUG(DL0, "log_read(%u) fails", RFS_LOG_I(sb)->isec);
		return -EIO;
	}

	/*
	 * Logging for new entry
	 * - pdir: parent directory's start clu
	 * - entry: entry index of new SFN
	 * - numof_entries: the number of entries including SFN and LFN
	 */
	SET32(RFS_LOG(sb)->type, MK_LOG_TYPE(RFS_LOG_I(sb)->type,
				RFS_SUBLOG_BUILD_ENTRY)); 
	SET32(RFS_LOG(sb)->log_entry.pdir, lei->pdir);
	SET32(RFS_LOG(sb)->log_entry.entry, lei->entry);
	SET32(RFS_LOG(sb)->log_entry.numof_entries, lei->numof_entries);

	DEBUG(DL2, "pdir : %u, entry : %u, numof_entries : %u",
			lei->pdir, lei->entry, lei->numof_entries);

	if (rli->operations->log_write(sb))
	{
		DEBUG(DL0, "log_write fails");
		return -EIO;
	}

	return 0;
}

/**
 * logging for entry to be removed
 * @param sb	super block
 * @param lei	log info of entry (pdir, entry idx, numof_entry, first char)
 * @return 0 on success, errno on failure
 * @pre trasaction must have been started
 */
int rfs_log_remove_entry(struct super_block *sb, struct log_ENTRY_info *lei)
{
	struct rfs_log_info *rli = RFS_LOG_I(sb);
	unsigned int i;

	if (rli->operations->log_read(sb, RFS_LOG_I(sb)->isec))
	{
		DEBUG(DL0, "log_read(%u) fails", RFS_LOG_I(sb)->isec);
		return -EIO;
	}

	/*
	 * Logging for removed entries
	 * - pdir: start cluster of parent dir
	 * - entry: entry index of SFN
	 * - numof_entries: the number of entries including SFN and LFN
	 * - undel_buf: the original characters which are replaced with Delete Mark
	 */
	SET32(RFS_LOG(sb)->type, MK_LOG_TYPE(RFS_LOG_I(sb)->type,
				RFS_SUBLOG_REMOVE_ENTRY)); 
	SET32(RFS_LOG(sb)->log_entry.pdir, lei->pdir);
	SET32(RFS_LOG(sb)->log_entry.entry, lei->entry);
	SET32(RFS_LOG(sb)->log_entry.numof_entries, lei->numof_entries);
	for (i = 0; i < lei->numof_entries; i++)
		RFS_LOG(sb)->log_entry.undel_buf[i] = lei->undel_buf[i];

	DEBUG(DL2, "pdir : %u, entry : %u, numof_entries : %u",
			lei->pdir, lei->entry, lei->numof_entries);


	if (rli->operations->log_write(sb))
		return -EIO;

	return 0;
}

/**
 * logging alloc chain from fat table
 * @param sb	super block
 * @param lfi	log fat info
 * @return 0 on success, errno on failure
 */
int rfs_log_alloc_chain(struct super_block *sb, struct log_FAT_info *lfi)
{
	int ret, i;
	struct rfs_log_info *rli = RFS_SB(sb)->log_info;

	/* commit previous transaction */
	if ((!is_empty_tr(sb)) && tr_pre_alloc(sb)) 
	{
		struct inode *inode = RFS_LOG_I(sb)->inode;
		unsigned int type = RFS_LOG_I(sb)->type;

		/* commit pre-allocation windows */
		ret = rfs_meta_commit(sb);
		if (ret)
		{
			DEBUG(DL0, "rfs_meta_commit fails(%d)", ret);
			return ret;
		}

		if (rli->operations->log_mark_end(sb, RFS_SUBLOG_COMMIT))
		{
			DEBUG(DL0, "log_mark_end fails");
			return -EIO;
		}

		/*
		 * starting new transaction
		 */
		__log_start_nolock(sb, type, inode);
	}

	/*
	 * Thus RFS I/O unit is block, you should read log before writing,
	 * If doesn't, it possible to bring data loss of same block log record. 
	 */
	if (rli->operations->log_read(sb, RFS_LOG_I(sb)->isec))
	{
		DEBUG(DL0, "log_read(%u) fails", RFS_LOG_I(sb)->isec);
		return -EIO;
	}

	/* modify log infomation for RFS_LOG_ALLOC_CHAIN */
	SET32(rli->log->type, MK_LOG_TYPE(RFS_LOG_I(sb)->type, 
				RFS_SUBLOG_ALLOC_CHAIN));

	/*
	 * Logging for alloc chain
	 * - pdir: parent directory's start clu
	 * - entry: entry index of new SFN
	 * - s_prev_clu, s_next_clu: source prev/next infomation
	 * - d_prev_clu, d_next_clu: destination prev/next infomation
	 * - numof_clus: the number of cluster will be allocate
	 * - clus[RFS_LOG_MAX_CLUSTES]: array of cluster will be allocate
	 * - b_is_xattr: whether allocation for xattr or not
	 * - xattr_ctime: ctime for xattr
	 * - xattr_cdate: cdate for xattr
	 */

	SET32(rli->log->log_fat.pdir, lfi->pdir);
	SET32(rli->log->log_fat.entry, lfi->entry);

	SET32(rli->log->log_fat.s_prev_clu, lfi->s_prev_clu);
	SET32(rli->log->log_fat.s_next_clu, lfi->s_next_clu);

	SET32(rli->log->log_fat.d_prev_clu, lfi->d_prev_clu);
	SET32(rli->log->log_fat.d_next_clu, lfi->d_next_clu);

	SET32(rli->log->log_fat.numof_clus, lfi->numof_clus);
	for (i = 0; i < lfi->numof_clus; i++) 
	{
		DEBUG(DL3, "%u, ", lfi->clus[i]);
		SET32(RFS_LOG(sb)->log_fat.clus[i], lfi->clus[i]);
	}

	SET32(rli->log->log_fat.b_is_xattr, lfi->b_is_xattr);
	SET16(rli->log->log_fat.xattr_ctime, lfi->ctime);
	SET16(rli->log->log_fat.xattr_cdate, lfi->cdate);

	if (rli->operations->log_write(sb))
	{
		DEBUG(DL0, "log_write fails");
		return -EIO;
	}

	return 0;
}
#ifdef CONFIG_RFS_FS_XATTR
/**
 * logging set xattr
 * @param sb super block
 * @param lxi log xattr info
 * @return 0 on success, errno on failure
 */
int rfs_log_xattr_set(struct inode *inode, struct log_XATTR_info *lxi)
{
	int err = 0;
	struct super_block *sb = inode->i_sb;
	struct rfs_inode_info * rii = RFS_I(inode);
	struct rfs_log_info *rli = RFS_SB(sb)->log_info;
	struct rfs_log_xattr *log_xattr;

	err = rli->operations->log_read(sb, rli->isec);
	if (err)
	{
		RFS_BUG_CRASH(sb, "Err(%d) in reading log\n", err);
		return err;
	}

	SET32(RFS_LOG(sb)->type, MK_LOG_TYPE(RFS_LOG_I(sb)->type,
				RFS_SUBLOG_XATTR_SET)); 

	/* ASSERT - check transaction type */
 	RFS_BUG_ON((RFS_LOG_I(sb)->type != RFS_LOG_CREATE) &&
			(RFS_LOG_I(sb)->type != RFS_LOG_SYMLINK) &&
			(RFS_LOG_I(sb)->type != RFS_LOG_XATTR));

	/* set log_xattr in RFS_LOG */
	log_xattr = &(RFS_LOG(sb)->log_xattr);

	SET32(log_xattr->pdir,			rii->p_start_clu);
	SET32(log_xattr->entry,			rii->index);
	SET32(log_xattr->xattr_start_clus,	rii->xattr_start_clus);
	SET32(log_xattr->xattr_last_clus,	rii->xattr_last_clus);
	SET32(log_xattr->xattr_numof_clus,	rii->xattr_numof_clus);
	SET16(log_xattr->xattr_valid_count,	rii->xattr_valid_count);
	SET32(log_xattr->xattr_total_space,	rii->xattr_total_space);
	SET32(log_xattr->xattr_used_space,	rii->xattr_used_space);

	SET32(log_xattr->xattr_offset_new,	lxi->xattr_offset_new);
	SET32(log_xattr->xattr_offset_old,	lxi->xattr_offset_old);

	DEBUG(DL2, "xstart:%u, xlast:%u, xnum:%u, valid:%u, total_spc:%u, "
			"used_spc:%u, new:%u, old:%u", 
			rii->xattr_start_clus, rii->xattr_last_clus, 
			rii->xattr_numof_clus, rii->xattr_valid_count,
			rii->xattr_total_space, rii->xattr_used_space,
			lxi->xattr_offset_new, lxi->xattr_offset_old);

	/* write log */
	if (rli->operations->log_write(sb))
		return -EIO;

	return 0;
}
#endif
/**
 * generate a signature value for multi-block record
 * @param sb super_block
 * @param record_count 
 * @return 0 on success, errno on failure
 */
static int __generate_signature(struct super_block *sb, int nr_record,
		unsigned int *signature)
{
	int i_rec, i_chunk = 0;
	int ret = 0;
	unsigned int _signature = 0;

	struct clu_chunk_list *p_chunk;
	struct list_head *head = &RFS_SB(sb)->free_chunks;
	struct list_head *p = head->next;

	if (list_empty(head))
	{
		DPRINTK("Error free_chunks list is empty\n");
		ret = -EIO;
		goto out;
	}
	for (i_rec = 0; i_rec < nr_record; i_rec++)
	{
		for (i_chunk = 0; i_chunk < RFS_LOG_MAX_CHUNKS; i_chunk++)
		{
			DEBUG(DL3, "nr_record:%d,nr_chunk:%d", 
					nr_record, i_chunk);

			if (0 == i_chunk)
			{
				p_chunk = list_entry(p, struct clu_chunk_list,
						list);
				_signature += p_chunk->chunk.start;
			}

			/* if (list_is_last(p, head)) */
			if (p->next == head) 
			{
				DEBUG(DL3,"list is last");
				goto out;
			}
			p = p->next;
		}
	}
out:
	DEBUG(DL2, "nr_record:%d,nr_chunk:%d,signature:%llu", 
			nr_record, i_chunk, (unsigned long long)_signature);

	*signature = _signature;
	return ret;
}

/**
 *  lookup a candidate segment corresponding with start cluster of inode in list
 * @param sb		superblock
 * @param i_start	start cluster of inode
 * @return		return segment on success, NULL on failure
 */ 
static struct c_segment *__lookup_segment(struct super_block *sb, 
		unsigned int i_start)
{
	struct rfs_log_info *rli = RFS_SB(sb)->log_info;
	struct c_segment *segment;
	struct list_head *p;

	list_for_each(p, &rli->c_list)
	{
		segment = SEGMENT_ENTRY(p);
		if (segment->start_cluster == i_start)
			return segment; /* found */
	}

	return NULL;
}

/**
 * update last cluster of the segment,
 * (when appending new cluster for writing data to unlinked file)
 *
 * @param inode inode
 * @return 0 on success, errno on failure
 */
int rfs_log_update_segment(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	struct rfs_log_info *rli = RFS_LOG_I(sb);
	struct c_segment *segment, *next_seg;
	int err;

	/* lookup candidate segment in list */
	segment = __lookup_segment(sb, RFS_I(inode)->start_clu);
	if (!segment) {
		/* NOT found */
		RFS_BUG_CRASH(sb, "segment does not exist for %d\n",
				RFS_I(inode)->start_clu);
		return -EIO;
	}

	/* insert a new cluster between seg and next seg */
	if (segment->last_cluster != rli->c_last_cluster)
	{
		next_seg = SEGMENT_ENTRY(segment->list.next);
		err = rfs_fat_write(sb, RFS_I(inode)->last_clu,
				next_seg->start_cluster);
		if (err) {
			DPRINTK("can't write a fat entry (%u)\n",
					RFS_I(inode)->last_clu);
			return err;
		}
	}
	else
	{ /* segment->last_cluster == rli->c_last_cluster */
		rli->c_last_cluster = RFS_I(inode)->last_clu;
	}

	segment->last_cluster = RFS_I(inode)->last_clu;

	return 0;
}

/**
 * find the prev and next of segement to be deallocated
 * @param sb	super block
 * @param ldi	log parameter for dealloc
 * @return 0 on success, errno on failure
 */
int rfs_log_location_in_segment_list(struct super_block *sb, 
		struct log_DEALLOC_info *ldi)
{
	struct rfs_log_info *rli = RFS_SB(sb)->log_info;
	struct c_segment *segment = NULL, *prev_seg, *next_seg;
	int ret = 0;

	ldi->l_prev_clu = rli->l_last_cluster;
	ldi->l_next_clu = CLU_TAIL;

	/* lookup candidate segment in list */
	segment = __lookup_segment(sb, ldi->s_start_clu);
	if (!segment)
	{ /* NOT found */
		RFS_BUG_CRASH(sb, "segment(%p) does not exist for %d\n",
				segment, ldi->s_start_clu);
		ret = -EIO;
		goto out;
	}

	if (segment->last_cluster != ldi->s_last_clu)
	{ /* NOT found */
		RFS_BUG_CRASH(sb, "segment is corrupted "
				"(%d, %d != %d, %d, %p)\n", ldi->s_start_clu, 
				ldi->s_last_clu, segment->start_cluster, 
				segment->last_cluster, segment);
		ret = -EIO;
		goto out;
	}

	if (segment->list.prev != &rli->c_list) 
	{
		prev_seg = SEGMENT_ENTRY(segment->list.prev);
		ldi->l_prev_clu = prev_seg->last_cluster;
	}
		
	if (segment->list.next != &rli->c_list) 
	{
		next_seg = SEGMENT_ENTRY(segment->list.next);
		ldi->l_next_clu = next_seg->start_cluster;
	}
out:
	return ret;
}

/**
 * leave record(s) for chunk list to delete
 * @param sb		super block
 * @param ldi		log parameter for dealloc
 * @param nr_record	the number of records for current chunks
 * @param bhs		the buffer heads containing log records
 * @param cur_isec	the start index to write the record(s)
 * @param[out] p_seq		the sequence number
 * @param[out] pnr_chunk	the number of chunks
 * @return 0 on success, errno on failure
 */
static int __log_update_dealloc_chain(struct super_block *sb, 
		struct log_DEALLOC_info *ldi, unsigned int nr_record, 
		struct buffer_head **bhs, unsigned int cur_isec, 
		rfs_log_seq_t *p_seq, unsigned int *pnr_chunk)
{
	struct rfs_log_info *rli = RFS_SB(sb)->log_info;
	struct list_head *head, *p;
	struct clu_chunk_list *p_chunk;

	unsigned int signature;
	unsigned int i_sec;
	unsigned int nr_logged_chunk = 0;
	int i_rec, i_chunk, ret = 0;

	/* generate multi-block record's signature */
	ret = __generate_signature(sb, nr_record, &signature);
	if (ret)
	{
		DPRINTK("err in multi-block record signature\n");
		goto out;
	}

	/* modify & dirty records's data */
	head = &RFS_SB(sb)->free_chunks;
	p = head->next;

	p_chunk = list_entry(head->next, struct clu_chunk_list, list);
	for (i_rec = 0; i_rec < nr_record; i_rec++)
	{
		BUG_ON(p == head);
		i_sec = (cur_isec + i_rec) & (RFS_LOG_MAX_COUNT - 1);
		rli->log = (struct rfs_trans_log *) ((bhs[i_rec]->b_data) +
			 ((i_sec & (rli->secs_per_blk - 1)) << SECTOR_BITS));

		SET32(rli->log->type, MK_LOG_TYPE(RFS_LOG_I(sb)->type,
				RFS_SUBLOG_DEALLOC));

		SET64(rli->log->sequence, (*p_seq));

		SET32(rli->log->log_dealloc.pdir, ldi->pdir);
		SET32(rli->log->log_dealloc.entry, ldi->entry);
		SET32(rli->log->log_dealloc.f_filesize, ldi->s_filesize);
		SET32(rli->log->log_dealloc.f_lastcluster, ldi->s_new_last);

		SET32(rli->log->log_dealloc.l_prev_clu, ldi->l_prev_clu);
		SET32(rli->log->log_dealloc.l_next_clu, ldi->l_next_clu);

		for(i_chunk = 0; (i_chunk < RFS_LOG_MAX_CHUNKS) && (p != head);
				i_chunk++, p = p->next)
		{
			p_chunk = list_entry(p, struct clu_chunk_list, list);
			SET32(rli->log->log_dealloc.chunks[i_chunk].start, 
					p_chunk->chunk.start);
			SET32(rli->log->log_dealloc.chunks[i_chunk].count, 
					p_chunk->chunk.count);
			nr_logged_chunk++;
		}
		SET32(rli->log->log_dealloc.nr_chunk, i_chunk);

		SET32(rli->log->log_dealloc.next_chunks, ldi->next_clu);
		SET32(rli->log->log_dealloc.record_count, nr_record);

		SET32(rli->log->log_dealloc.mr_signature, signature);
		SET32(rli->log->signature, RFS_MAGIC);

		//set dirty
		mark_buffer_dirty(bhs[i_rec]);
		*p_seq = (*p_seq) + 1;
	}

	DEBUG(DL2,"sequence:%lld,nr_logged_chunk:%u", 
			*p_seq, nr_logged_chunk);
	*pnr_chunk = nr_logged_chunk;
out:
	return ret;
}

/**
 * check the count of free chunks is equal to the maximum count of log
 * @param sb		super block
 * @return 0 when the count is less than it, 1 when equal to
 */
int rfs_log_max_chunk(struct super_block *sb)
{
	static const unsigned int max_chunks = RFS_LOG_MAX_MULTI_BLOCK_RECORD *
			RFS_LOG_MAX_CHUNKS;

	if (RFS_SB(sb)->nr_free_chunk > max_chunks)
	{
		RFS_BUG_CRASH(sb, "corrupted chunk count (%u)\n",
				RFS_SB(sb)->nr_free_chunk);
	}
		
	if (RFS_SB(sb)->nr_free_chunk == max_chunks)
	{
		DEBUG(DL0, "reach max chunks\n");
		RFS_LOG_I(sb)->need_mrc = TRUE;
		return 1;
	}

	RFS_LOG_I(sb)->need_mrc = FALSE;
	return 0;
}

/**
 * logging dealloc
 * @param inode inode
 * @param count output variable for logged chunk count
 * @return 0 on success, errno on failure
 */
static int __log_dealloc_chain(struct super_block *sb, 
		struct log_DEALLOC_info *ldi, unsigned int *pnr_chunk)
{
	struct rfs_log_info *rli = RFS_SB(sb)->log_info;
	struct buffer_head **bhs = NULL;

	rfs_log_seq_t _sequence;
	unsigned int cur_isec = rli->isec;
	unsigned int nr_logged_chunk = 0;
	unsigned int nr_record;
	int ret = 0;

	nr_record = ((RFS_SB(sb)->nr_free_chunk + (RFS_LOG_MAX_CHUNKS - 1))
			/ RFS_LOG_MAX_CHUNKS);
	nr_record = min_t(unsigned int, 
			nr_record, RFS_LOG_MAX_MULTI_BLOCK_RECORD);

	BUG_ON(!nr_record);
	DEBUG(DL2, "nr_record:%u,nr_free_chunk:%u", 
			nr_record, RFS_SB(sb)->nr_free_chunk);

	/* memory allocation for buffhead array */
	bhs = rfs_kmalloc(sizeof(struct buffer_head*) * nr_record, GFP_KERNEL,
			NORETRY);
	if (NULL == bhs)
	{
		DPRINTK("RFS-log : err in memory allocation\n");
		ret = -ENOMEM;
		goto out;
	}

	/* log read-ahead for record count */
	_sequence = rli->sequence;
	DEBUG(DL2,"sequence:%lld", _sequence);

	ret = rli->operations->log_read_ahead(sb, cur_isec, nr_record, bhs);
	if (ret)
	{
		DPRINTK("log_read_ahead fail(%d)\n", ret);
		goto out;
	}
	
	/* log record update */
	ret = __log_update_dealloc_chain(sb, ldi, nr_record, bhs, cur_isec,
			&_sequence, &nr_logged_chunk);
	if (ret)
	{
		DPRINTK("__log_update_dealloc_chain fail(%d)\n", ret);
		goto out;
	}

	/* flush records */
	ret = rli->operations->log_mwrite(sb, nr_record, bhs);
	if (ret)
	{
		DPRINTK("log_mwrite fail(%d)\n", ret);
		goto out;
	}

	rli->sequence = _sequence;
	rli->isec = (cur_isec + nr_record) & (RFS_LOG_MAX_COUNT - 1);
	*pnr_chunk = nr_logged_chunk;
out:
	if (bhs)
		kfree(bhs);
	return ret;
}

/**
 * post processing for rfs_log_move_chain
 * @param inode inode
 * @return 0 on success, errno on failure
 */
static int __log_move_chain_pp(struct super_block *sb, unsigned int i_start, 
		unsigned int i_last)
{
	struct c_segment *segment = NULL;
	struct c_segment *prev_seg, *next_seg;
	struct rfs_log_info *rli = RFS_SB(sb)->log_info;

	unsigned int c_start = rli->c_start_cluster;
	unsigned int c_last = rli->c_last_cluster;
	unsigned int target_clus, value;

	int ret = 0;
	DEBUG(DL2, "c_start:%u,c_last:%u, i_starti:%u,i_last:%u", c_start,
			c_last, i_start, i_last);

	if (IS_INVAL_CLU(RFS_SB(sb), i_start)) 
	{
		/* out-of-range input */
		//RFS_BUG("invalid cluster number(%u)\n", i_start);
		return -EINVAL;
	}

	if (IS_INVAL_CLU(RFS_SB(sb), i_last)) 
	{
		/* out-of-range input */
		//RFS_BUG("invalid cluster number(%u)\n", i_last);
		return -EINVAL;
	}

	
	/* lookup candidate segment in list */
	segment = __lookup_segment(sb, i_start);
	if (!segment || segment->last_cluster != i_last) 
	{ /* NOT found */
		RFS_BUG_CRASH(sb, "segment does not exist for %d\n", i_start);
		return -EIO;
	}

	prev_seg = SEGMENT_ENTRY(segment->list.prev);
	next_seg = SEGMENT_ENTRY(segment->list.next);
	
	DEBUG(DL2, "c_start:%u,c_last:%u, seg_start:%u,seg_last:%u", c_start,
			c_last, segment->start_cluster, segment->last_cluster);

	if (c_start == segment->start_cluster && 
			c_last == segment->last_cluster) 
	{ /* only one */
		rli->c_start_cluster = CLU_TAIL;
		rli->c_last_cluster = CLU_TAIL;
		target_clus = rli->l_last_cluster;
		value = CLU_TAIL;
		DEBUG(DL2, "[A]target:%u,value:%u", target_clus, value);
	} 
	else if (c_last == segment->last_cluster) 
	{ /* last seg */
		rli->c_last_cluster = prev_seg->last_cluster;
		target_clus = rli->c_last_cluster;
		value = CLU_TAIL;
		DEBUG(DL2, "[B]target:%u,value:%u", target_clus, value);
	} 
	else if (c_start == segment->start_cluster) 
	{ /* first seg */
		rli->c_start_cluster = next_seg->start_cluster;
		target_clus = rli->l_last_cluster;
		value = rli->c_start_cluster;
		DEBUG(DL2, "[C]target:%u,value:%u", target_clus, value);
	} 
	else 
	{ /* middle seg */
		target_clus = prev_seg->last_cluster;
		value = next_seg->start_cluster;
		DEBUG(DL2, "[D]target:%u,value:%u", target_clus, value);
	}

	ret = rfs_fat_write(sb, target_clus, value);
	if (unlikely(ret))
	{
		RFS_BUG_CRASH(sb, "can't write a fat entry (%u:%u)\n",
				target_clus, value);
	}

	list_del(&segment->list);
	DEBUG(DL2, "target:%u,value:%u", target_clus, value);

	if (segment)
		kfree(segment);

	return ret;
}

/**
 * logging and moving unlinked chain to the last of logfile
 * @param sb super block
 * @param lfi log param for fat
 * @return 0 on success, errno on failure
 */
static int __log_move_chain(struct super_block *sb, struct log_FAT_info *lfi)
{
	struct c_segment *segment = NULL;
	struct rfs_log_info * rli = RFS_LOG_I(sb);
	int err;

	/* sanity check */
	BUG_ON(IS_INVAL_CLU(RFS_SB(sb), lfi->clus[0])); //start cluster
	BUG_ON(IS_INVAL_CLU(RFS_SB(sb), lfi->clus[1])); //last cluster

	/* Thus RFS I/O unit is block, you should read log before writing,
	   if doesn't, It possible bring data loss of same block log record. */
	err = rli->operations->log_read(sb, RFS_LOG_I(sb)->isec);
	if (err)
	{
		RFS_BUG_CRASH(sb, "Err(%d) in reading log(%u)\n", 
				err, RFS_LOG_I(sb)->isec);
		return err;
	}
	/* modify log infomation for RFS_LOG_MOVE_CHAIN */
	SET32(rli->log->type, MK_LOG_TYPE(RFS_LOG_I(sb)->type, 
				RFS_SUBLOG_MOVE_CHAIN));

	SET32(rli->log->log_fat.pdir, lfi->pdir);
	SET32(rli->log->log_fat.entry, lfi->entry);

	// thus, RFS_1.3.0 does not move partial chain
	// fill source chain's prev/next info with CLU_TAIL
	SET32(rli->log->log_fat.s_prev_clu, CLU_TAIL);
	SET32(rli->log->log_fat.s_next_clu, CLU_TAIL);

	SET32(rli->log->log_fat.d_prev_clu, (rli->c_start_cluster == CLU_TAIL) 
			? rli->l_last_cluster //when, there is no segment
			: rli->c_last_cluster);

	SET32(rli->log->log_fat.d_next_clu, CLU_TAIL);

	SET32(rli->log->log_fat.numof_clus, 2);
	SET32(rli->log->log_fat.clus[0], lfi->clus[0]);
	SET32(rli->log->log_fat.clus[1], lfi->clus[1]);

	SET32(rli->log->log_fat.b_is_xattr, lfi->b_is_xattr);

	DEBUG(DL2, "start_clu:%u,last_clu:%u,c_start:%u", lfi->clus[0],
			lfi->clus[1], rli->c_start_cluster);

	//Write RFS_SUB_LOG_MOVE
	err = rli->operations->log_write(sb);
	if (err)
	{
		RFS_BUG_CRASH(sb, "Err(%d) in writing log\n", err);
		return err;
	}

	/* make a segment */
	segment = (struct c_segment *)
		rfs_kmalloc(sizeof(struct c_segment), GFP_KERNEL, RETRY);
	if (!segment)
	{
		RFS_BUG_CRASH(sb, "memory allocation failed\n");
		return -ENOMEM;
	}

	segment->start_cluster = lfi->clus[0];
	segment->last_cluster = lfi->clus[1];

	DEBUG(DL2, "[add] %p :start_clu:%u,last_clu:%u,c_start:%u", segment,
		segment->start_cluster, segment->last_cluster, 
		rli->c_start_cluster);

	fat_lock(sb); 

	//attach file to end of logfile
	if (rli->c_start_cluster == CLU_TAIL)
	{	
		err = rfs_fat_write(sb, rli->l_last_cluster, segment->start_cluster);
		if (err)
		{
			RFS_BUG_CRASH(sb, "can't write a fat entry (%u)\n", 
					rli->l_last_cluster);
			kfree(segment);
			goto out;
		}
		rli->c_start_cluster = segment->start_cluster;
		rli->c_last_cluster = segment->last_cluster;
	}
	else
	{	
		err = rfs_fat_write(sb, rli->c_last_cluster, segment->start_cluster);
		if (err)
		{
			RFS_BUG_CRASH(sb, "can't write a fat entry (%u)\n", 
					rli->c_last_cluster);
			kfree(segment);
			goto out;
		}
		rli->c_last_cluster = segment->last_cluster;
	}

#ifdef CONFIG_RFS_FAT_DEBUG
	{
		struct c_segment *tmp;
		tmp = __lookup_segment(sb, segment->start_cluster);
		if (tmp != NULL)
		{
			DEBUG(DL0, "segment(%p) found (%d, %d)", tmp, tmp->start_cluster, tmp->last_cluster);
			RFS_BUG();
		}
	}
#endif

	// add segment to list in log_info
	list_add_tail(&segment->list, &rli->c_list);

out:
	fat_unlock(sb);

	return err;
}

/**
 * add segment for truncated chain for recovery
 * @param sb super block
 * @param start_clu start cluster of the truncated chain
 * @param last_clu last cluster of the truncated chain
 * @return 0 on success, ENOMEM
 */
int rfs_log_segment_add(struct super_block *sb, unsigned int start_clu, 
		unsigned int last_clu)
{
	struct c_segment *segment = NULL;
	struct rfs_log_info * rli = RFS_LOG_I(sb);

	/* sanity check */
	BUG_ON(IS_INVAL_CLU(RFS_SB(sb), start_clu));
	BUG_ON(IS_INVAL_CLU(RFS_SB(sb), last_clu));

	/* make a segment */
	segment = (struct c_segment *)
		rfs_kmalloc(sizeof(struct c_segment), GFP_KERNEL, RETRY);
	if (!segment)
	{
		RFS_BUG_CRASH(sb, "memory allocation failed\n");
		return -ENOMEM;
	}

	segment->start_cluster = start_clu;
	segment->last_cluster = last_clu;

	/* add segment to list in log_info */
	list_add_tail(&segment->list, &RFS_LOG_I(sb)->c_list);
	DEBUG(DL2, "[add] %p :start_clu:%u,last_clu:%u", segment,
			start_clu, last_clu);

	/* attach file to end of logfile */
	if (CLU_TAIL == rli->c_start_cluster)
	{	
		rli->c_start_cluster = start_clu;
	}
	rli->c_last_cluster = last_clu;
	
	return 0;
}

/**
 * logging start of transaction without a lock
 * @param sb	super block
 * @param type	log type
 * @param inode	modified inode
 * @return 0 on success, errno on failure
 */
static void __log_start_nolock(struct super_block *sb, unsigned int type,
		struct inode *inode)
{
	RFS_LOG_I(sb)->type = type;
	RFS_LOG_I(sb)->inode = inode;
	RFS_LOG_I(sb)->dirty = FALSE;

	return;
}

/**
 * mark an end of transaction
 * @param sb super block
 * @param sub_type commit or abort	
 * @return 0 on success, errno on failure
 */
static int __log_mark_end(struct super_block *sb, unsigned int sub_type)
{
	struct rfs_log_info *rli = RFS_LOG_I(sb);
	if (rli->operations->log_read(sb, RFS_LOG_I(sb)->isec))
	{
		DEBUG(DL0, "log_read(%u) fails", RFS_LOG_I(sb)->isec);
		return -EIO;
	}

	rfs_set_bh_bit(BH_RFS_LOG_COMMIT, &(RFS_LOG_I(sb)->bh->b_state));

	SET32(RFS_LOG(sb)->type, MK_LOG_TYPE(RFS_LOG_I(sb)->type, sub_type));
	if (rli->operations->log_write(sb))
	{
		DEBUG(DL0, "log_write fail");
		return -EIO;
	}
		
	RFS_LOG_I(sb)->inode = NULL;
	RFS_LOG_I(sb)->type = RFS_LOG_NONE;

	return 0;
}

/*****************************************************************************/
/* log read / write functions						     */
/*****************************************************************************/
/**
 * read log
 * @param sb super block
 * @param isec logical sector to read
 * @return 0 on success, errno on failure
 *
 * read log block indicating by isec, then set the isec as (isec + 1)
 */
static int __log_read(struct super_block *sb, unsigned int isec)
{
	struct buffer_head *bh;
	int state = BH_RFS_LOG;
	unsigned int sec_off;

	if (RFS_LOG_I(sb)->bh)
		brelse(RFS_LOG_I(sb)->bh);

	/* rotational logfile */
	if (isec >= RFS_LOG_MAX_COUNT) 
	{
		RFS_BUG_CRASH(sb, "RFS-log : can't read log record\n"
			"logical sector(%d) is out of range", isec);
		return -EIO;
	}

	DEBUG(DL3, "read log [%dth : 0x%x]\n", isec, 
			(unsigned int) RFS_LOG_I(sb)->sequence);

	if (is_empty_tr(sb))
		state = BH_RFS_LOG_START;

	bh = rfs_bread(sb, RFS_LOG_I(sb)->blocks[isec], state); 
	if (!bh) 
	{
		RFS_BUG_CRASH(sb, "RFS-log : Couldn't read log\n"
			"logical sector(%u), physical sector(%llu)", 
			isec, (unsigned long long)RFS_LOG_I(sb)->blocks[isec]);
		return -EIO;
	}

	RFS_LOG_I(sb)->bh = bh;

	/* point next log record */
	if (isec == RFS_LOG_MAX_COUNT - 1)
		RFS_LOG_I(sb)->isec = 0;
	else
		RFS_LOG_I(sb)->isec = isec + 1;

	sec_off = isec & (RFS_LOG_I(sb)->secs_per_blk - 1);
	RFS_LOG_I(sb)->log = (struct rfs_trans_log *) ((bh->b_data) +
			(sec_off << SECTOR_BITS));

	return 0;
}

/**
 * read-ahead logs for multiple-block log record
 * @param sb super block
 * @param isec logical start sector to read
 * @param count count of record-block [block unit]
 * @param bhs pointer of buffer_head array 
 * @return 0 on success, errno on failure
 */
static int __log_read_ahead(struct super_block *sb, unsigned int start_sec, 
		int nr_record, struct buffer_head **bhs)
{
	int i_rec, ret = 0;
	unsigned int i_sec = start_sec;
	struct rfs_log_info *rli = RFS_SB(sb)->log_info;

	/* rotational logfile */
	if (start_sec >= RFS_LOG_MAX_COUNT)
	{
		RFS_BUG_CRASH(sb, "RFS-log:can't read log record"
				"(sector:%u)\n", start_sec);
		ret = -EIO;
		goto out;
	}

	if (rli->bh) {
		brelse(rli->bh);
		rli->bh = NULL;
		rli->log = NULL;
	}

	for (i_rec = 0; i_rec < nr_record; i_rec++)
	{
		i_sec = (start_sec + i_rec) & (RFS_LOG_MAX_COUNT - 1);
		bhs[i_rec] = sb_getblk(sb, rli->blocks[i_sec]);
		DEBUG(DL3, "(%lu:%p)", (unsigned long) rli->blocks[i_sec], 
				bhs[i_rec]);
		if (!bhs[i_rec])
		{
			RFS_BUG_CRASH(sb, "Err in getting bh\n"
				"logical sector:%u,physical sector:%llu\n",
				i_sec, (unsigned long long) rli->blocks[i_sec]);
			ret = -EIO;
			goto out;
		}
	}

	/* bust read */
	ll_rw_block(READ, nr_record, bhs);

	for (i_rec = 0; i_rec < nr_record; i_rec++)
	{
		/* check I/O completion */
		wait_on_buffer(bhs[i_rec]);
		if (!buffer_uptodate(bhs[i_rec]))
		{
			RFS_BUG_CRASH(sb, "RFS-log : cann't read log "
					"record:%u\n", i_rec);
			ret = -EIO;
			goto out;
		}
		rfs_set_bh_bit(BH_RFS_LOG, &bhs[i_rec]->b_state);
	}
out:
	return ret;
}
	
/**
 * write log to logfile and release it
 * @param log_info rfs's log structure	
 * @return 0 on success, errno on failure
 */
static int __log_write(struct super_block *sb)
{
	int ret = 0;
	struct rfs_log_info *rli = RFS_LOG_I(sb);

	if (!rli->bh) 
	{
		RFS_BUG_CRASH(sb, "RFS-log : No buffer head for log\n");
		return -EIO;
	}

	SET64(rli->log->sequence, rli->sequence);
	SET32(rli->log->signature, RFS_MAGIC);
	mark_buffer_dirty(rli->bh);
	ret = rfs_sync_dirty_buffer(rli->bh);
	if (ret) 
	{
		RFS_BUG_CRASH(sb, "RFS-log : Fail to write a log record\n");
	}

	brelse(rli->bh);
	rli->bh = NULL;
	rli->log = NULL;
	rli->dirty = TRUE;
	rli->sequence = rli->sequence + 1;

	return ret;
}

/**
 * write multi-block log to logfile and release it
 * @param log_info rfs's log structure	
 * @param count count of bhs array [record unit]
 * @param bhs pointer of buffer_head array
 * @return 0 on success, errno on failure
 */
static int __log_mwrite(struct super_block *sb, int nr_record, 
		struct buffer_head **bhs)
{
	int i_rec;

	DEBUG(DL3, "cnt:%d\n", nr_record);
	/* bust write */
	ll_rw_block(WRITE, nr_record, bhs);
	for (i_rec = 0; i_rec < nr_record; i_rec++)
	{
		/* check I/O completion */
		wait_on_buffer(bhs[i_rec]);
		if (!buffer_uptodate(bhs[i_rec]))
		{
			RFS_BUG_CRASH(sb, "RFS-log: cann't write log "
					"record(%u)\n", i_rec);
			return -EIO;
		}
		/* reduce ref count */
		brelse(bhs[i_rec]);
	}
	RFS_LOG_I(sb)->dirty = TRUE;

	return 0;
}

/*****************************************************************************/
/* misc.								     */
/*****************************************************************************/
/**
 * make logfile
 * @param sb		super block
 * @return 0 on success, errno on failure
 */
static int __make_logfile(struct super_block *sb)
{
	struct inode *inode;
	struct inode *root_dir;
	struct dentry *root_dentry;
	struct rfs_dir_entry *ep;
	struct buffer_head *bh = NULL;
	unsigned int file_size;
	unsigned int *clus;
	unsigned int index;
	int numof_clus;
	int i;
	int ret;

	root_dentry = sb->s_root;
	root_dir = root_dentry->d_inode;

	/* scan enough space to create log file */
	file_size = RFS_LOG_MAX_COUNT * SECTOR_SIZE;
	numof_clus = file_size >> RFS_SB(sb)->cluster_bits;
	if (file_size & (RFS_SB(sb)->cluster_size - 1))
		numof_clus++;

	DEBUG(DL3, "file_size : %u, numof_clus : %u\n", file_size, numof_clus);
	DEBUG(DL3, "sector_size : %lu, cluster_size : %u", sb->s_blocksize,
			RFS_SB(sb)->cluster_size);

	if (!(inode = NEW_INODE(sb)))
		return -ENOMEM;

	clus = rfs_kmalloc(numof_clus * sizeof(unsigned int), 
			GFP_KERNEL, NORETRY);
	if (!clus) 
	{
		iput(inode);
		return -ENOMEM;
	}

	ret = rfs_find_free_clusters(root_dir, clus, numof_clus);
	if (ret < 0) 
	{
		if (-ENOSPC != ret)
			ret = -EACCES;
		goto free;
	}

	if (ret < numof_clus) 
	{ /* insufficient space */
		ret = -ENOSPC;
		goto free;
	}

	ret = rfs_fat_write(sb, clus[0], CLU_TAIL);
	if (ret) 
		goto free;

	INC_USED_CLUS(sb, 1);

	index = build_entry(root_dir, NULL, clus[0], TYPE_FILE,	
			RFS_LOG_FILE_NAME, 0);
	if ((int) index < 0) 
	{
		ret = (int) index;
		goto free;
	}

	ep = get_entry(root_dir, index, &bh);
	if (IS_ERR(ep)) 
	{
		ret = PTR_ERR(ep);
		goto out;
	}

	/* set inode for log file */
	RFS_I(inode)->start_clu = clus[0];

	/* allocate enough clusters */
	for (i = 1; i < numof_clus; i++) 
	{
		ret = rfs_append_new_cluster(inode, clus[i - 1], clus[i]);
		if (ret) 
			goto out;
		INC_USED_CLUS(sb, 1);
	}

	ep->attr |= (ATTR_READONLY | ATTR_SYSTEM | ATTR_HIDDEN);
	SET32(ep->size, file_size);
	mark_buffer_dirty(bh);
out:
	brelse(bh);
free:
	iput(inode);
	kfree(clus);
	return ret;
}

/**
 * return a pre-allocated cluster
 * @param inode	inode of file to be extended
 * @param new_clu out-var to save free cluster number
 * @return 0 on success, errno on failure
 */
int rfs_log_get_cluster(struct inode *inode, unsigned int *new_clu)
{
	struct super_block *sb = inode->i_sb;
	int ret;

	/* set default invalid value */
	*new_clu = CLU_TAIL;

	if (RFS_LOG_I(sb)->alloc_index >= RFS_LOG_I(sb)->numof_pre_alloc) 
	{
		ret = __pre_alloc_clusters(inode);
		if (ret)
			return ret;
	}

	if (likely(RFS_LOG_I(sb)->alloc_index <
				RFS_LOG_I(sb)->numof_pre_alloc)) 
	{
		*new_clu = RFS_LOG_I(sb)->pre_alloc_clus[
			(RFS_LOG_I(sb)->alloc_index)++];
	} 
	else 
	{
		RFS_BUG_CRASH(sb, "RFS-log : pre-allocation corruption\n");
		return -EIO;
	}

	DEBUG(DL3, "alloc_cluster : %u", *new_clu);
	return 0;
}

/**
 * do pre-allocation
 * @param inode inode of file to be written
 * @return 0 on success, errno on failure
 *
 * pre-allocate clusters and save them in log buffer
 */
static int __pre_alloc_clusters(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	struct log_FAT_info lfi;
	int count = 0;
	int err;

	if (IS_FAST_LOOKUP_INDEX(inode))
		return -EFAULT;

	count = rfs_find_free_clusters(inode, RFS_LOG_I(sb)->pre_alloc_clus,
				RFS_LOG_PRE_ALLOC);
	if (count < 0)
		return count;
	RFS_LOG_I(sb)->alloc_index = 0;
	RFS_LOG_I(sb)->numof_pre_alloc = count;

	lfi.s_prev_clu = CLU_TAIL;
	lfi.s_next_clu = CLU_TAIL;
	/* in case of unlinked inode, t_next_clu is the head of other chain */
	if (unlikely(RFS_I(inode)->i_state == RFS_I_FREE)) 
	{
		if (RFS_I(inode)->last_clu != CLU_TAIL) 
		{
			/*
			 * read the start cluster of possible
			 * other unlinked open file
			 */
			err = rfs_fat_read(sb, RFS_I(inode)->last_clu,
					&lfi.d_next_clu);
			if (err)
				return err;
		}
	}
	else
	{
		lfi.d_next_clu = CLU_TAIL;
	}

	/* unlinked inode does not have dir entry */
	lfi.pdir = (RFS_I(inode)->i_state == RFS_I_FREE) ?
		CLU_TAIL : RFS_I(inode)->p_start_clu;
	lfi.entry = RFS_I(inode)->index;
	lfi.d_prev_clu = RFS_I(inode)->last_clu;
	/* lfi.t_next_clu is already assigned */
	lfi.numof_clus = RFS_LOG_I(sb)->numof_pre_alloc;
	lfi.clus = RFS_LOG_I(sb)->pre_alloc_clus;

	lfi.b_is_xattr = 0;
	lfi.ctime = 0;
	lfi.cdate = 0;

	err = rfs_log_alloc_chain(sb, &lfi);
	if (err)
		return err;

	return 0; 
}

/*
 * check validity of log file (_RFS_INTERNAL_SANITY_CHECK_LOG)
 */
int sanity_check_log(struct super_block *sb)
{
	unsigned int prev, next;
	unsigned int chain_cnt = 0;
	unsigned int file_size; /*filesize of logfile in byte*/
	unsigned int numof_clus; /* cluster count of logfile */
	int err;

	if (IS_INVAL_CLU(RFS_SB(sb), RFS_LOG_I(sb)->l_start_cluster))
	{
		/* wrong start cluster out-of-range */
		RFS_BUG_CRASH(sb, "log has invalid start cluster number(%u)\n",
				RFS_LOG_I(sb)->l_start_cluster);
		return -1;
	}
	else
	{
		prev = RFS_LOG_I(sb)->l_start_cluster;
		next = prev;
		chain_cnt ++;
	}

	/* calculate cluster count of logfile */
	file_size = RFS_LOG_MAX_COUNT * SECTOR_SIZE ;
	numof_clus = file_size >> RFS_SB(sb)->cluster_bits;
	if (file_size & (RFS_SB(sb)->cluster_size - 1))
		numof_clus++;

	/* scan log file chain until expected count */
	fat_lock(sb);

	do
	{
		err = rfs_fat_read(sb, prev, &next);
		if (err)
		{
			fat_unlock(sb);
			RFS_BUG("fail to read FAT:%d", err);
			return -1;
		}

		if (next < VALID_CLU)
		{ /* out-of-range input */
			fat_unlock(sb);
			goto corrupted;
		}

		if (next == CLU_TAIL)
			break;
		chain_cnt ++ ;
		prev = next;

		if (chain_cnt == numof_clus)
			break;

	} while (chain_cnt < numof_clus);

	/* next should be end of cluster */
	err = rfs_fat_read(sb, prev, &next);
	if (err)
	{
		fat_unlock(sb);
		RFS_BUG("fail to read FAT:%d", err);
		return -1;
	}

	fat_unlock(sb);

	if (CLU_TAIL != next)
		goto corrupted;

	/* It means log file corrupted that next not equal CLU_TAIL */
	if (chain_cnt != numof_clus)
		goto corrupted;

	return 0;

corrupted:
	RFS_BUG_CRASH(sb, "log file chain corrupted (%d!=%d)\n",
			numof_clus, chain_cnt);
	return -1;
}

