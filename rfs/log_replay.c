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
 *  @file	fs/rfs/log_replay.c
 *  @brief	functions for replaying log
 *
 *
 */

#include <linux/rfs_fs.h>
#include "rfs.h"
#include "log.h"

#ifdef CONFIG_RFS_FS_XATTR
#include "xattr.h"
#endif


/**
 * undo alloc chain from fat table
 * @param sb super block
 * @return 0 on success, errno on failure
 *
 * source : fat table, destination : file
 */
static int __log_undo_alloc_chain(struct super_block *sb)
{
	struct rfs_log_fat *log_fat;
	unsigned int numof_clus;
	unsigned int pdir;
	unsigned int i;
	int ret = 0;

	log_fat = &(RFS_LOG(sb)->log_fat);

	/* assert */
	RFS_BUG_ON((GET32(log_fat->pdir) != CLU_TAIL) &&
		   (GET32(log_fat->pdir) != ROOT_CLU) &&
			(IS_INVAL_CLU(RFS_SB(sb), GET32(log_fat->pdir))));
	RFS_BUG_ON(GET32(log_fat->s_prev_clu) != CLU_TAIL);
	RFS_BUG_ON(GET32(log_fat->s_next_clu) != CLU_TAIL);
	RFS_BUG_ON(GET32(log_fat->d_prev_clu != CLU_TAIL) &&
			IS_INVAL_CLU(RFS_SB(sb), GET32(log_fat->d_prev_clu)));
	RFS_BUG_ON(GET32(log_fat->d_next_clu != CLU_TAIL) &&
			IS_INVAL_CLU(RFS_SB(sb), GET32(log_fat->d_next_clu)));

	pdir = GET32(log_fat->pdir);

	/* source file was not empty */
	if (GET32(log_fat->d_prev_clu) != CLU_TAIL) 
	{
		/* undo destination file */
		if (rfs_fat_write(sb, GET32(log_fat->d_prev_clu),
					GET32(log_fat->d_next_clu)))
			return -EIO;
		DEBUG(DL0, "undo source %u -> %u", GET32(log_fat->d_prev_clu),
				GET32(log_fat->d_next_clu));
	} 
	else if (pdir != CLU_TAIL) /* and (d_prev_clu == CLU_TAIL) */
	{
		/*
		 * cases of (pdir == CLU_TAIL)
		 * 1. mkdir : before building entry, system off
		 * 2. write to unlinked file
		 */
		struct buffer_head *bh = NULL;
		struct rfs_dir_entry *ep;

		ep = get_entry_with_cluster(sb, pdir,
				GET32(log_fat->entry), &bh, NULL);
		if (IS_ERR(ep)) 
		{
			/*
			 * EFAULT means extension of parent is not flushed 
			 * and there is no dir entry for new file.
			 */
			if (PTR_ERR(ep) != -EFAULT) 
			{
				brelse(bh);
				return -EIO;
			}
		}
		else
		{
			/* case for data cluster allocation */
			if (FALSE == GET32(log_fat->b_is_xattr))
			{
				DEBUG(DL1, "ep's start_clu [%u -> 0]",
						START_CLUSTER(RFS_SB(sb), ep));
				SET_START_CLUSTER(ep, 0);
			}
			else /* case for data xattr cluster allocation */
			{
				/* revert xattr flag & xattr_start */
				ep->attr &= ~ATTR_XATTR;
				ep->ctime = GET16(log_fat->xattr_ctime);
				ep->cdate = GET16(log_fat->xattr_cdate);	
			}
			mark_buffer_dirty(bh);
			brelse(bh);
		}
	}

	numof_clus = GET32(log_fat->numof_clus);

	/* set free allocated chain */
	for (i = 0; i < numof_clus; i++) {
		if (rfs_fat_write(sb, GET32(log_fat->clus[i]), CLU_FREE))
			return -EIO;
		DEBUG(DL0, "free %u", GET32(log_fat->clus[i]));
	}

	return ret;
}

/**
 * undo built entries
 * @param sb super block
 * @return 0 on success, errno on failure
 *
 *	for each entry in built entries {
 *		if (entry is built)
 *			entry->name[0] = DELETION_MARK;
 *	}
 */
static int __log_undo_built_entry(struct super_block *sb)
{
	struct rfs_log_entry *log_entry;
	struct buffer_head *bh = NULL;
	struct rfs_dir_entry *ep;
	unsigned int numof_entries;
	unsigned int clu, entry;
	unsigned int i;
	int ret = 0;

	log_entry = &(RFS_LOG(sb)->log_entry);
	numof_entries = GET32(log_entry->numof_entries);
	clu = GET32(log_entry->pdir);
	entry = GET32(log_entry->entry);

	for (i = 0; i < numof_entries; i++) {
		ep = get_entry_with_cluster(sb, clu, entry - i, &bh, NULL);
		if (IS_ERR(ep)) {
			ret = PTR_ERR(ep);
			if (ret == -EFAULT) {
				/* beyond file limit */
				/* the extended cluster was
				   not flushed, so do nothing
				   for build-entry on it */
				ret = 0;
			}
			goto rel_bh;
		}

		if (!IS_FREE(ep->name))
			ep->name[0] = (unsigned char) DELETE_MARK;

		mark_buffer_dirty(bh);
	}

rel_bh:
	brelse(bh);

	return ret;
}

/**
 * undo removed entries
 * @param sb super block
 * @return 0 on success, errno on failure
 * @pre the way to delete entries is marking deletion at ep->name[0] 
 *
 *	for each entry in built entries {
 *		entry->name[0] = log_entry->undel_buf[index];
 *	}
 */
static int __log_undo_removed_entry(struct super_block *sb)
{
	struct rfs_log_entry *log_entry;
	struct buffer_head *bh = NULL;
	struct rfs_dir_entry *ep;
	unsigned int numof_entries;
	unsigned int clu, entry;
	unsigned int i;
	unsigned int start_clu = CLU_TAIL;
	int ret = 0;

	log_entry = &(RFS_LOG(sb)->log_entry);
	numof_entries = GET32(log_entry->numof_entries);
	clu = GET32(log_entry->pdir);
	entry = GET32(log_entry->entry);

	for (i = 0; i < numof_entries; i++) {
		ep = get_entry_with_cluster(sb, clu, entry - i, &bh, NULL);
		if (IS_ERR(ep)) {
			/*
			 * In rfs_symlink(), removing entry could happen
			 * for recovery of ENOSPC. In this case,
			 * entries can not be found, so returns EFAULT.
			 */
			if ((ret = PTR_ERR(ep)) == -EFAULT)
				ret = 0;

			goto rel_bh;
		}

		ep->name[0] = log_entry->undel_buf[i];

		mark_buffer_dirty(bh);

		/* cache useful information */
		if ((i == 0) && (rfs_entry_type(ep) == TYPE_DIR))
			start_clu = START_CLUSTER(RFS_SB(sb), ep);
	}

	/*
	 * if this entry is a moved directory, it is possible that
	 * the start cluster of '..' references other parent
	 */
	if (start_clu != CLU_TAIL) {
		unsigned int p_start_clu;

		/* read dotdot */
		ep = get_entry_with_cluster(sb, start_clu, 1, &bh, NULL);
		if (IS_ERR(ep)) {
			ret = PTR_ERR(ep);
			goto rel_bh;
		}

		/* sanity check */
		if (strncmp(ep->name, DOTDOT, DOS_NAME_LENGTH)) {
			dump_entry(sb, ep, bh, -1, "at %s:%d", 
					__func__, __LINE__    );
			RFS_BUG_CRASH(sb, "[%d, %d] entry is corrupted\n",
				       	clu, entry);
			ret = -EIO;
			goto rel_bh;
		}
		
		if (clu == RFS_SB(sb)->root_clu)
			p_start_clu = ROOT_CLU;
		else
			p_start_clu = clu;

		if (START_CLUSTER(RFS_SB(sb), ep) != p_start_clu) {
			/* an already moved directory. change the parent */
			DEBUG(DL2, "directory is already moved");

			SET_START_CLUSTER(ep, p_start_clu);

			mark_buffer_dirty(bh);
		}
	}

rel_bh:
	brelse(bh);

	return ret;
}

/**
 * undo xattr set.
 * @param sb super block
 * @return 0 on success, errno on failure
 * @pre
 *
 */
static int __log_undo_xattr_set(struct super_block *sb)
{
	int ret = 0;
#ifdef CONFIG_RFS_FS_XATTR
	struct rfs_log_xattr *log_xattr;
	struct inode *i_dummy;
	unsigned int xattr_start;
	unsigned int xattr_last;
	unsigned int xattr_num_clus;
	unsigned int offset_new;
	unsigned int offset_old;
	unsigned int tr_type;
	char type;
	int size;

	log_xattr = &(RFS_LOG(sb)->log_xattr);

	tr_type = GET32(RFS_LOG(sb)->type) >> 16;

	RFS_BUG_ON((tr_type != RFS_LOG_CREATE) && (tr_type != RFS_LOG_SYMLINK) 
		&& (tr_type != RFS_LOG_XATTR));

	/* skip replay XATTR_SET in CREATE(dir, file, sym) */
	if (RFS_LOG_XATTR != tr_type)
	{
		return 0;
	}

	xattr_start = GET32(log_xattr->xattr_start_clus);
	xattr_last = GET32(log_xattr->xattr_last_clus);
	xattr_num_clus = GET32(log_xattr->xattr_numof_clus);
	
	offset_new = GET32(log_xattr->xattr_offset_new);
	offset_old = GET32(log_xattr->xattr_offset_old);

	/* assert */
	RFS_BUG_ON(IS_INVAL_CLU(RFS_SB(sb), xattr_start));
	RFS_BUG_ON(IS_INVAL_CLU(RFS_SB(sb), xattr_last));
	RFS_BUG_ON((offset_new < sizeof(struct rfs_xattr_header))
			&& (offset_new != 0));
	RFS_BUG_ON((offset_old < sizeof(struct rfs_xattr_header))
			&& (offset_old != 0));

	DEBUG(DL0, "xstart:%u,xlast:%u,xnumofclus:%u,off_new:%u,off_old:%u",
			xattr_start, xattr_last, xattr_num_clus, 
			offset_new, offset_old);

	if (!(i_dummy = NEW_INODE(sb)))
	{
		/* memory error */
		DEBUG(DL0, "Can not alloc inode for logfile");
		ret = -ENOMEM;
		goto out;
	}

	/* minimum inode setting for xattr_bmap() */
	RFS_I(i_dummy)->xattr_start_clus = xattr_start;
	RFS_I(i_dummy)->xattr_last_clus = xattr_last;
	RFS_I(i_dummy)->xattr_numof_clus = xattr_num_clus;

	/* replay step 1 : recover xattr's header */
	ret = rfs_xattr_read_header_to_inode(i_dummy, FALSE);
	if (ret)
	{
		DEBUG(DL0, "fail to rfs_xattr_read_header_to_inode(%d)", ret);
		goto err_iput;
	}

	/* other variable doesnot concern in log_replay() */
	RFS_I(i_dummy)->xattr_valid_count = GET16(log_xattr->xattr_valid_count);
	RFS_I(i_dummy)->xattr_total_space = GET32(log_xattr->xattr_total_space);
	RFS_I(i_dummy)->xattr_used_space = GET32(log_xattr->xattr_used_space);

	ret = rfs_xattr_write_header(i_dummy);
	if (ret)
	{
		DEBUG(DL0, "fail to rfs_xattr_read_header(%d)", ret);
		goto err_iput;
	}

	/* relpaly step 2 : delete new entry (mark type as delete) */
	size = sizeof(char);
	type = XATTR_ENTRY_DELETE;

	/* offset_new == 0 mean, no need to delete(mark delete) */
	if (0 < offset_new) 
	{
		unsigned int clus_new; // the cluster contain offset_new

		clus_new = (offset_new + (RFS_SB(sb)->cluster_size - 1)) >>
			RFS_SB(sb)->cluster_bits;

		if (clus_new < RFS_I(i_dummy)->xattr_numof_clus)
		{
			ret = __xattr_io(WRITE, i_dummy, offset_new, &type, 
					size);
			if (ret)
			{
				DEBUG(DL0, "fail to __xattr_io(%d)", ret);
				goto err_iput;
			}
		}
		else
		{
			/* fix for 22064 defect refer the CQ */
			/* this case means offset_new requires new cluster,
			 * but log_replay() perform undo alloc sub-log.
			 * therefore there is nothing to do for offset_new.
			 */
			; /* do nothing */
		}
	}

	/* replay step 3 : recover old entry (mark type as used) */
	type = XATTR_ENTRY_USED;

	/* offset_old == 0 mean, no need to recover(mark used) */
	if (0 < offset_old)
	{
		ret = __xattr_io(WRITE, i_dummy, offset_old, &type, size);
		if (ret)
		{
			DEBUG(DL0, "fail to __xattr_io(%d)", ret);
			goto err_iput;
		}
	}

err_iput:
	iput(i_dummy);
out:
#endif
	return ret;
}

/**
 * undo xattr_unlink which moves all chain to last of logfile's pool
 * @param sb super block
 * @return 0 on success, errno on failure
 * @pre logging only head and tail for moved chain
 *
 * source : file, destination : logfile
 */
static int __log_undo_move_chain(struct super_block *sb)
{
	struct rfs_log_fat *log_fat;
	int ret = 0;

	log_fat = &(RFS_LOG(sb)->log_fat);

	/* assert */
	RFS_BUG_ON((GET32(log_fat->pdir) != CLU_TAIL) &&
		   (GET32(log_fat->pdir) != ROOT_CLU) &&
			(IS_INVAL_CLU(RFS_SB(sb), GET32(log_fat->pdir))));
	RFS_BUG_ON(GET32(log_fat->s_prev_clu) != CLU_TAIL);
	RFS_BUG_ON(GET32(log_fat->s_next_clu) != CLU_TAIL);
	RFS_BUG_ON(IS_INVAL_CLU(RFS_SB(sb), GET32(log_fat->d_prev_clu)));
	RFS_BUG_ON(GET32(log_fat->d_next_clu) != CLU_TAIL);
	RFS_BUG_ON(GET32(log_fat->numof_clus) != 2);
	RFS_BUG_ON(IS_INVAL_CLU(RFS_SB(sb), GET32(log_fat->clus[0])));
	RFS_BUG_ON(IS_INVAL_CLU(RFS_SB(sb), GET32(log_fat->clus[1])));
	
	if (GET32(log_fat->pdir) != CLU_TAIL && 
			FALSE == GET32(log_fat->b_is_xattr))
	{
		/*
		 * In case of run-time rollback of mkdir by -ENOSPC,
		 * because dir entry have not been built yet,
		 * log_fat->pdir has CLU_TAIL.
		 */
		struct buffer_head *bh = NULL;
		struct rfs_dir_entry *ep;

		ep = get_entry_with_cluster(sb, GET32(log_fat->pdir),
				GET32(log_fat->entry), &bh, NULL);
		if (IS_ERR(ep)) {
			brelse(bh);
			return -EIO;
		}
		DEBUG(DL1, "ep's start_clu [%u -> %u]",
				START_CLUSTER(RFS_SB(sb), ep), GET32(log_fat->clus[0]));

		SET_START_CLUSTER(ep, GET32(log_fat->clus[0]));

		mark_buffer_dirty(bh);

		brelse(bh);
	}

	/* link tail of chain with tail of chain */
	if (rfs_fat_write(sb, GET32(log_fat->clus[1]), CLU_TAIL))
		return -EIO;

	DEBUG(DL1, "make target  %u ... %u -> CLU_TAIL",
			GET32(log_fat->clus[0]), GET32(log_fat->clus[1]));
	
	/* make pool chain complete */
	if (rfs_fat_write(sb, GET32(log_fat->d_prev_clu), CLU_TAIL))
		return -EIO;

	DEBUG(DL1, "make logfile %u -> CLU_TAIL", GET32(log_fat->d_prev_clu));

	return ret;
}


/**
 * calculate checksume and compare it with signatures of dealloc records
 * @param sb		super block
 * @param[IN,OUT] cur_isec	index of first record
 * @param nr_logs	the number of records
 * @param bhs		the buffers containing records
 * @return 0 on success, errno on failure
 */
static int __sanity_check_dealloc_tr_bhs(struct super_block *sb,
		unsigned int *cur_isec, unsigned int nr_logs,
		struct buffer_head **bhs)
{
	struct rfs_trans_log *rtl;
	struct rfs_log_dealloc *rld;
	struct clu_chunk *chunks;
	unsigned int signature = 0;
	unsigned int secs_per_blk;
	unsigned int isec = 0;
	unsigned int sec_off;
	unsigned int type;
	int ilog;
	int ret = 0;

	secs_per_blk = RFS_LOG_I(sb)->secs_per_blk;

	/* get type of last log which must has dealloc subtype */
	ilog = nr_logs - 1;
	isec = ((*cur_isec) + ilog) & (RFS_LOG_MAX_COUNT - 1);
	sec_off = isec & (secs_per_blk - 1);
	rtl = (struct rfs_trans_log *)
		((bhs[ilog]->b_data) + (sec_off << SECTOR_BITS));

	type = GET32(rtl->type);

	/*
	 * all of types of multiple records must be same.
	 * calculate signature.
	 */
	for (; ilog >= 0; ilog--)
	{
		/* extract log and chunk array from buffer */
		isec = ((*cur_isec) + ilog) & (RFS_LOG_MAX_COUNT - 1);
		sec_off = isec & (secs_per_blk - 1);
		rtl = (struct rfs_trans_log *)
			((bhs[ilog]->b_data) + (sec_off << SECTOR_BITS));
		/*
		 * sequence is valid but type is invalid.
		 * multiple records are partially written.
		 * find first of deallocs for replaying prev transaction.
		 */
		if (GET32(rtl->type) != type)
		{
			DEBUG(DL0, "different types (%x != %x)",
					GET32(rtl->type), type);
			/* point to first dealloc log */
			isec = (isec + 1) & (RFS_LOG_MAX_COUNT - 1);
			ret = -EINVAL;
			goto out;
		}

		rld = &(rtl->log_dealloc);

		chunks = rld->chunks;

		signature += GET32(chunks[0].start);
		type = GET32(rtl->type);
	}

	/* check signature */
	for (ilog = nr_logs - 1; ilog >= 0; ilog--)
	{
		/* extract log and chunk array from buffer */
		isec = ((*cur_isec) + ilog) & (RFS_LOG_MAX_COUNT - 1);
		sec_off = isec & (secs_per_blk - 1);
		rtl = (struct rfs_trans_log *)
			((bhs[ilog]->b_data) + (sec_off << SECTOR_BITS));
		rld = &(rtl->log_dealloc);

		/*
		 * sequence and type are valid but signature is invalid.
		 * one log record is corrupted
		 */
		if (GET32(rld->mr_signature) != signature)
		{
			DPRINTK("Broken signature (%u != %u)\n",
					GET32(rld->mr_signature), signature);
			/* point to first dealloc log */
			isec = (isec + 1) & (RFS_LOG_MAX_COUNT - 1);
			ret = -EIO;
			break;
		}
	}

out:
	*cur_isec = isec;
	return ret;
}

/**
 * redo source and logfile for dealloc
 * @param sb	super block
 * @param rtl	log record about dealloc
 * @return 0 on success, errno on failure
 */
static int __log_redo_dealloc_source_and_log(struct super_block *sb,
		struct rfs_trans_log *rtl)
{
	struct rfs_log_dealloc *rld;
	struct rfs_dir_entry *ep = NULL;
	unsigned int l_prev, l_next;
	int err = 0;

	/*
	 * NOTE. don't change the order of statements for replay for
	 * truncating logfile during mount time
	 */
	rld = &(rtl->log_dealloc);

	/* redo source file for truncate */
	if (GET_LOG(GET32(rtl->type)) == RFS_LOG_TRUNCATE_B)
	{
		struct buffer_head *bh = NULL;
		unsigned int size;

		DEBUG(DL0, "Redo truncated file");
		ep = get_entry_with_cluster(sb, GET32(rld->pdir),
				GET32(rld->entry), &bh, NULL);
		if (unlikely(IS_ERR(ep)))
		{
			DPRINTK("Can't get entry\n");
			brelse(bh);
			return -EIO;
		}

		/* redo size */
		size = GET32(rld->f_filesize);
		SET32(ep->size, size);

		DEBUG(DL0, "Redo size (%u)", size);

		/* redo file chain */
		if (GET32(rld->f_lastcluster) == CLU_TAIL)
		{ /* truncate file as zero size */
			SET16(ep->start_clu_lo, 0);
			SET16(ep->start_clu_hi, 0);
		}
		else
		{ /* mark end of file */
			err = rfs_fat_write(sb, GET32(rld->f_lastcluster), CLU_TAIL);
			if (unlikely(err))
			{
				DPRINTK("Can't write fat\n");
				brelse(bh);
				return err;
			}
		}
		DEBUG(DL0, "Redo ep's start (%u)", GET32(rld->f_lastcluster));

		mark_buffer_dirty(bh);
		brelse(bh);
	}

	l_prev = GET32(rld->l_prev_clu);

	/*
	 * If there is remaining chain yet to be deallocated,
	 * connect this. Otherwise, connect next segment.
	 * After replay, all segments connected to logfile are deallocated
	 * during mount time
	 */
	if (GET32(rld->next_chunks) != CLU_TAIL)
		l_next = GET32(rld->next_chunks);
	else
		l_next = GET32(rld->l_next_clu);

	err = rfs_fat_write(sb, l_prev, l_next);

	DEBUG(DL0, "connect logfile [%d, %d]", l_prev, l_next);

	if (unlikely(err))
	{
		DPRINTK("Can't write fat\n");
		return err;
	}

	return 0;
}

#ifdef CONFIG_RFS_MAPDESTROY
extern int (*sec_stl_delete)(dev_t dev, u32 start, u32 nums, u32 b_size);
#endif
/**
 * redo dealloc chunks and make logfile and source complete
 * @param sb		super block
 * @param cur_isec	index of first record
 * @param nr_logs	the number of records
 * @param bhs		the buffers containing records
 * @return 0 on success, errno on failure
 *
 * source : file, destination : logfile
 *
 * This function is called when last (multi) records ars
 * uncommitted dealloc chain by truncate or delete inode
 */
static int __redo_dealloc_chain(struct super_block *sb, unsigned int cur_isec,
		unsigned int nr_logs, struct buffer_head **bhs)
{
	struct rfs_trans_log *rtl;
	struct rfs_log_dealloc *rld;
	struct clu_chunk *chunks;
	unsigned int start;
	unsigned int nr_chunk, nr_clus;
	unsigned int secs_per_blk;
	unsigned int ilog, iclu, isec, ichunk;
	unsigned int sec_off;
	int err = 0;

	secs_per_blk = RFS_LOG_I(sb)->secs_per_blk;

	/* take first log record */
	isec = cur_isec & (RFS_LOG_MAX_COUNT - 1);
	sec_off = isec & (secs_per_blk - 1);
	rtl = (struct rfs_trans_log *) ((bhs[0]->b_data) +
		 (sec_off << SECTOR_BITS));
	rld = &(rtl->log_dealloc);

	err = __log_redo_dealloc_source_and_log(sb, rtl);

	if (unlikely(err))
	{
		DPRINTK("Can't replay dealloc source & logfile\n");
		return -EIO;
	}

	/* release chunks */
	for (ilog = 0; ilog < nr_logs; ilog++)
	{
		/* extract log and chunk array from buffer */
		isec = (cur_isec + ilog) & (RFS_LOG_MAX_COUNT - 1);
		sec_off = isec & (secs_per_blk - 1);
		rtl = (struct rfs_trans_log *) ((bhs[ilog]->b_data) +
			 (sec_off << SECTOR_BITS));
		rld = &(rtl->log_dealloc);

		chunks = rld->chunks;
		nr_chunk = GET32(rld->nr_chunk);

		/* release chunks in a log */
		for (ichunk = 0; ichunk < nr_chunk; ichunk++)
		{
			nr_clus = GET32(chunks[ichunk].count);
			start = GET32(chunks[ichunk].start);
			DEBUG(DL0, "This chunk [%u, %u]", start, nr_clus);
			/* release clus in a chunk */
			for (iclu = 0; iclu < nr_clus; iclu++)
			{
				err = rfs_fat_write(sb, start + iclu, CLU_FREE);
				if (unlikely(err))
				{
					DPRINTK("Can't write fat\n");
					return err;
				}
			}
#ifdef CONFIG_RFS_MAPDESTROY
			if (IS_DELETEABLE(sb->s_dev))
			{
				sec_stl_delete(sb->s_dev, START_BLOCK(start, sb),
					nr_clus <<RFS_SB(sb)->blks_per_clu_bits,
					sb->s_blocksize);
			}
#endif
		}
	}

	/*
	 * following actions will be taken by rfs_log_replay()
	 * sync meta;
	 * mark commit;
	 */
	return 0;
}

/**
 * redo dealloc chain from the last of multiple records
 * @param sb	super block
 * @param index the index of last one of multiple records
 * @return 0 on success, errno on failure
 *
 * source : file, destination : logfile
 */
static int __log_redo_dealloc(struct super_block *sb, unsigned int *index)
{
	struct rfs_log_dealloc *log_dealloc;
	struct buffer_head **bhs;
	struct rfs_log_info *rli;
	unsigned int nr_logs;
	unsigned int i_log;
	unsigned int ifirst;
	unsigned int type;
	int ret = 0;

	rli = RFS_LOG_I(sb);
	log_dealloc = &(RFS_LOG(sb)->log_dealloc);
	nr_logs = GET32(log_dealloc->record_count);

	/* find first of multiple records */
	/* (count - 1) results in offset */
	if (*index < nr_logs - 1)
		/* fix for 22037 defect refer the CQ */
		ifirst = (RFS_LOG_MAX_COUNT) - ((nr_logs - 1) - (*index));
	else
		ifirst = (*index) - (nr_logs - 1);

	type = GET_LOG(GET32(RFS_LOG(sb)->type));
	if (RFS_LOG_DEL_INODE != type && RFS_LOG_TRUNCATE_B != type)
	{
		DEBUG(DL0, "skip dealloc replay");
		ret = -EAGAIN;
		goto skip_redo;
	}

	bhs = kmalloc(sizeof(struct buffer_head *) * nr_logs, GFP_KERNEL);
	if (unlikely(!bhs))
		return -ENOMEM;

	for (i_log = 0; i_log < nr_logs; i_log++)
		bhs[i_log] = NULL;

	DEBUG(DL0, "dealloc records [%u, %u]", ifirst, nr_logs);

	/* get bhs containing dealloc tr */
	ret = rli->operations->log_read_ahead(sb, ifirst, nr_logs, bhs);
	if (unlikely(ret)) {
		DPRINTK("Err(%d) in read-ahead\n", ret);
		ret = -EIO;
		goto free;
	}

	/* sanity check */
	ret = __sanity_check_dealloc_tr_bhs(sb, &ifirst, nr_logs, bhs);
	if (unlikely(ret)) {
		DEBUG(DL0, "sanity check fails (%d)", ret);
		goto free;
	}

	/* redo dealloc transaction */
	ret = __redo_dealloc_chain(sb, ifirst, nr_logs, bhs);
	if (unlikely(ret)) {
		DPRINTK("Err(%d) redo dealloc chain\n", ret);
		ret = -EIO;
	}

free:
	for (i_log = 0; i_log < nr_logs; i_log++)
		brelse(bhs[i_log]);
	kfree(bhs);

skip_redo:
	*index = ifirst;

	return ret;
}

/**
 * redo source and logfile for dealloc
 * @param sb	super block
 * @return 0 on success, errno on failure
 * source : file
 */
static int __log_redo_mrc(struct super_block *sb)
{
	struct rfs_trans_log *rtl = RFS_LOG(sb);
	int err;

	err = __log_redo_dealloc_source_and_log(sb, rtl);
	if (unlikely(err))
	{
		DPRINTK("Can't replay dealloc source & logfile\n");
		return -EIO;
	}

	return err;

}

/**
 * check the sanity of remaining log records
 * @param sb super block
 * @param from next to latest record
 * @param to the last index of records
 * @param start_seq the sequence of from
 * @return 0 on success, EIO on failure.
 */
static int __sanity_check_sequence(struct super_block *sb, unsigned int from,
		unsigned int to, rfs_log_seq_t start_seq)
{
	struct rfs_log_info* rli = RFS_LOG_I(sb);
	unsigned int i;
	rfs_log_seq_t prev = start_seq;
	rfs_log_seq_t seq;

	DEBUG(DL3, "from : %d, to : %d, seq : %d\n", from, to, (int) seq);
	for (i = from + 1; i < to; i++) {
		if (rli->operations->log_read(sb, i))
		{
			DPRINTK("log_read fail");
			return -EIO;
		}

		seq = GET64(RFS_LOG(sb)->sequence);

		if (start_seq)
		{
			if (prev + 1 != seq) 
			{
				DPRINTK("sequence broken (%u != %u)\n"
					"from:%u,to:%u,curr:%d(%d)", 
					(int) prev, (int) seq, from, to, 
					i, (int)RFS_LOG_I(sb)->blocks[i]);
				return -EIO;
			}
			prev = seq;
		}
		else
		{
			if (seq) {
				DPRINTK("sequence broken (%u is not zero)\n",
					(int) seq);
				return -EIO;
			}
		}
	}

	return 0;
}

/**
 * sequential search for the index of a log with max sequence
 * @param sb super block
 * @param from start index to search
 * @param to end index to search
 * @param[out] last_index the index of log with max sequence
 * @return 0 on success, error no on failure.
 */
static int __sequential_search_max(struct super_block *sb, unsigned int from,
		unsigned int to, unsigned int *last_index)
{
	struct rfs_log_info* rli = RFS_LOG_I(sb);
	rfs_log_seq_t max_seq, seq, prev_seq;
	unsigned int i;

	*last_index = from;

	if (rli->operations->log_read(sb, from))
		return -EIO;

	max_seq = GET64(RFS_LOG(sb)->sequence);
	prev_seq = max_seq;

	/* empty log file */
	if (!max_seq && GET32(RFS_LOG(sb)->type) == RFS_LOG_NONE)
		return -ENOENT;

	DEBUG(DL3, "from : %d, to : %d\n", from, to);
	for (i = from + 1; i < to; i++) {
		if (rli->operations->log_read(sb, i))
			return -EIO;

		seq = GET64(RFS_LOG(sb)->sequence);

		/* the unwritten or oldest log record */
		if (!seq || (prev_seq == seq + RFS_LOG_MAX_COUNT - 1)) {
			DEBUG(DL3, "find the latest log record"
					" (cur : %u, prev : %u)",
					(unsigned int) seq, (unsigned int) prev_seq);
			/* found max seq, and check remaining records */
			if (__sanity_check_sequence(sb, i, to, seq))
			{
				return -EIO;
			}

			break;
		}

		/* not sequential!! log file must be corrupted */
		if (prev_seq != seq - 1) {
			DPRINTK("Log file must be corrupted (prev_seq : %u,"
				" seq : %u)\n", (unsigned int) prev_seq,
				(unsigned int) seq);
			return -EIO;
		}

		DEBUG(DL3, "%d, max : 0x%0x, cur : 0x%0x\n",
				i, (unsigned int) max_seq, (unsigned int) seq);
		if (max_seq < seq) {
			max_seq = seq;
			*last_index = i;
		}

		prev_seq = seq;
	}

	DEBUG(DL2, "last tr is : %d, max_seq : 0x%0x\n",
			*last_index, (unsigned int) max_seq);

	/* notice that it is possible to overflow sequence number */
	if (max_seq == (rfs_log_seq_t) (~0))
		return -EILSEQ;

	return 0;
}

/**
 * get the index of a log with max sequence in log file
 * @param sb super block
 * @param[out] last_index the index of log with max sequence
 * @return 0 or -ENOENT on success, other errnos on failure
 */
static inline int __log_get_trans(struct super_block *sb,
		unsigned int *last_index)
{
	return  __sequential_search_max(sb, 0, RFS_LOG_MAX_COUNT, last_index);
}

/**
 * replay logs from latest uncommited log
 * @param sb rfs private super block
 * @return 0 on success, errno on failure
 * @pre super block should be initialized
 */
int __log_replay(struct super_block *sb)
{
	/* sub_type should be used as 'int' type */
	struct rfs_log_info *rli = RFS_LOG_I(sb);
	unsigned int sub_type = RFS_LOG_NONE;
	unsigned int last_index = 0, index;
	int ret = 0;

	if ((ret = __log_get_trans(sb, &last_index))) {
		if (ret == -ENOENT) {
			/* empty logfile. point first entry */
			RFS_LOG_I(sb)->isec = 0;
			ret = 0;
		}
		goto out;
	}

	ret = rli->operations->log_read(sb, last_index);
	if (ret)
		goto out;

	index = last_index;
	RFS_LOG_I(sb)->sequence = GET64(RFS_LOG(sb)->sequence) + 1;
	RFS_LOG_I(sb)->type = RFS_LOG_REPLAY;
	sub_type = GET_SUBLOG(GET32(RFS_LOG(sb)->type));

	while (1) {
		/*
		 * Note: It's important use 'int' type in swtich condition.
		 *	'unsigned int' condition can't handle it correctly
		 *	 in gcov mode
		 */

		/*
		 * Check signature before replaying the log record.
		 * For backward compatibility, no replay makes no
		 * checking signature.
		 */
		if (sub_type != RFS_SUBLOG_COMMIT &&
				sub_type != RFS_SUBLOG_MRC &&
				sub_type != RFS_SUBLOG_ABORT) {
			if (GET32(RFS_LOG(sb)->signature) != RFS_MAGIC) 
			{
				DPRINTK("signature(0x%x) is not 0x%x\n",
						GET32(RFS_LOG(sb)->signature),
						(unsigned int) RFS_MAGIC);
				ret = -EIO;
				goto out;
			}
		}

		switch (sub_type) {
		case RFS_SUBLOG_ALLOC_CHAIN:
			DEBUG(DL0, "RFS_SUBLOG_ALLOC_CHAIN");
			ret = __log_undo_alloc_chain(sb);
			break;
		case RFS_SUBLOG_MOVE_CHAIN:
			DEBUG(DL0, "RFS_SUBLOG_MOVE_CHAIN");
			ret = __log_undo_move_chain(sb);
			break;
		case RFS_SUBLOG_BUILD_ENTRY:
			DEBUG(DL0, "RFS_SUBLOG_BUILD_ENTRY");
			ret = __log_undo_built_entry(sb);
			break;
		case RFS_SUBLOG_REMOVE_ENTRY:
			DEBUG(DL0, "RFS_SUBLOG_REMOVE_ENTRY");
			ret = __log_undo_removed_entry(sb);
			break;
		case RFS_SUBLOG_XATTR_SET:
			DEBUG(DL0, "RFS_SUBLOG_XATTR_SET");
			ret = __log_undo_xattr_set(sb);
			break;

		case RFS_SUBLOG_DEALLOC:
			DEBUG(DL0, "RFS_SUBLOG_DEALLOC");
			ret = __log_redo_dealloc(sb, &index);
			/* After redo undo can not be replayed */
			if (ret)
			{
				if (ret == -EAGAIN)
				{
					/* skips replay */
					ret = 0;
					break;
				}
				if (ret == -EINVAL)
				{/*
				  * multiple logs could be written wholly
				  * index must point first of dealloc records
				  */
					DEBUG(DL0, "partially written");
					ret = 0;
					break;
				}
				goto out;
			}
			else
				goto commit;
		case RFS_SUBLOG_MRC: /* one part of dealloc completes */
			DEBUG(DL0, "RFS_SUBLOG_MRC");
			ret = __log_redo_mrc(sb);
			/* After redo undo can not be replayed */
			if (ret)
				goto out;
			else
				goto commit;
			/*
			 * Soon after replay, other parts of dealloc
			 * will complete in mount time
			 */
		case RFS_SUBLOG_ABORT: /* transaction is aborted */
			DEBUG(DL0, "Latest transaction is aborted."
				" Strongly recommend to check filesystem\n");
		case RFS_SUBLOG_COMMIT: /* transaction completes */
		case RFS_SUBLOG_START: /* transaction does nothing */
			/* do nothing. do not need to commit */
			DEBUG(DL0, "RFS_SUBLOG_COMMIT (%x,%02x:%02x)",
				       sub_type, MAJOR(sb->s_dev),
				       MINOR(sb->s_dev));
			if (last_index == index) {
				brelse(RFS_LOG_I(sb)->bh);
				RFS_LOG_I(sb)->bh = NULL;
				RFS_LOG_I(sb)->log = NULL;
				goto out;
			}

			goto commit;
		default:
			RFS_BUG_CRASH(sb,
				"RFS-log : Unsupporting log record\n");
			ret = -EIO;
			break;
		}

		if (ret)
			goto out;

		/* Check wrap-around of index */
		if (index == 0)
			index = RFS_LOG_MAX_COUNT;

		/* get next log */
		ret = rli->operations->log_read(sb, --index);
		if (ret)
			goto out;

		sub_type = GET_SUBLOG(GET32(RFS_LOG(sb)->type));
	}
commit:
	ret = rfs_sync_vol(sb);
	if (ret)
	{
		DEBUG(DL0, "Err(%d) in rfs_sync_vol\n", ret);
		return ret;
	}

	/* commit mark */
	if (last_index == RFS_LOG_MAX_COUNT - 1)
		RFS_LOG_I(sb)->isec = 0;
	else
		RFS_LOG_I(sb)->isec = last_index + 1;

	ret = rli->operations->log_mark_end(sb, RFS_SUBLOG_COMMIT);

	DEBUG(DL0, "end mark commit");
	return ret;
out:
	RFS_LOG_I(sb)->type = RFS_LOG_NONE;
	return ret;
}


