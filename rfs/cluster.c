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
 *  @file	fs/rfs/cluster.c
 *  @brief	FAT cluster handling functions
 *
 *
 */

#include <linux/fs.h>
#include <linux/rfs_fs.h>

#include "rfs.h"
#include "log.h"
#include "xattr.h"

/*
 * structure for map destroy (INCORE)
 */
struct old_map {
	unsigned int block;
	unsigned int nr_blocks;
	struct list_head list;
};

#define IS_CONSECUTION(x, y)	((x + 1) == y ? 1 : 0)

/************************************************************************/
/* Cluster manipulations						*/
/************************************************************************/
 /**
 *  find free clusters in fat table.
 * @param inode			inode
 * @param[out] free_clu		the reference of array of free clusters
 * @param[out] req_count	the request number of free clusters
 * @return			If positive, the number of found
 				free cluster. Otherwise, the errno.
  */
int rfs_find_free_clusters(struct inode *inode, unsigned int *free_clu,
		int req_count)
{
	struct rfs_sb_info *sbi = RFS_SB(inode->i_sb);
	unsigned int i;
	unsigned int value;
	int count = 0;
	int err;

	for (i = VALID_CLU; i < sbi->num_clusters; i++) 
	{ 
		/* search free cluster from hint(search_ptr) */
		if (sbi->search_ptr >= sbi->num_clusters)
			sbi->search_ptr = VALID_CLU;

		err = rfs_fat_read(inode->i_sb, sbi->search_ptr, &value);
		if (err)
			return err;

		if (value == CLU_FREE) 
		{
			free_clu[count] = sbi->search_ptr;
			count++;

			if (count == req_count) 
			{
				/* increase search_ptr for next search */
				sbi->search_ptr++;
				break;
			}
		}
		sbi->search_ptr++;
	}

	if (count == 0)
		return -ENOSPC;

	return count; 
}

/**
 *  find last cluster and return the number of clusters from specified fat chain of inode
 * @param inode		inode	
 * @param[out] last_clu	last cluster number
 * @param interval	interval to pick up fast seek points.
 *			if it is 0, do not build fast seek points
 *			its value is one of the power of 2
 * @param num_fpoints	numof fast seek points expected
 * @param[out] fpoints	the array of fast seek points
 * @return		return the number of clusters on success, errno on failure
 */
int rfs_find_last_cluster(struct inode *inode, unsigned int start_clu,
		unsigned int *last_clu, unsigned int interval, 
		unsigned int num_fpoints, unsigned int *fpoints)
{
	struct super_block *sb = inode->i_sb;
	unsigned int prev, next;
	unsigned int index_fpoints = 0;
	unsigned int interval_mask = 0;
	unsigned int max_loop = RFS_SB(sb)->num_clusters;
	unsigned int loop;
	int count = 0;
	int err = 0;

	/* set default value */
	if (last_clu)
		*last_clu = CLU_TAIL;

	/* prev = RFS_I(inode)->start_clu; */
	prev = start_clu;

	if (prev == CLU_TAIL) /* cluster dose not be allocated */
		return 0;

	if (interval)
		interval_mask = interval - 1;

	fat_lock(sb);

	for (loop = VALID_CLU; loop < max_loop; loop++) 
	{
		err = rfs_fat_read(sb, prev, &next);
		if (err) 
		{
			fat_unlock(sb);
			return -EIO;
		}

		if (next < VALID_CLU) 
		{ 
			/* out-of-range input */
			fat_unlock(sb);
			RFS_BUG_CRASH(sb, "fat entry(%u,%u) was corrupted\n",
				       prev, next);
			return -EIO;
		}

		/* get fast-seek points, while finding last cluster */
		if (interval_mask && !(count & interval_mask)) 
		{
			/* beyond array limit */
			if (index_fpoints >= num_fpoints) 
			{
				fat_unlock(sb);
				DEBUG(DL0, "prev: %u next: %u count: %d",
					prev, next, count);
				RFS_BUG_CRASH(sb, "file(%u) size(%u:%u) is "
					"larger than expected %u != %u\n",
					/* RFS_I(inode)->start_clu, */
					start_clu,
					(unsigned int)RFS_I(inode)->mmu_private,
					(unsigned int)inode->i_size,
					index_fpoints, num_fpoints);
				return -EIO;
			}
			fpoints[index_fpoints] = prev;
			index_fpoints++;
		}
		count++;

		if (next == CLU_TAIL) /* found last clu */
			break;

		prev = next;
	}

	fat_unlock(sb);

	if (loop == max_loop)
	{
		RFS_BUG_CRASH(sb, "chain is circular");
		return -EIO;
	}

	/* do meet expected count? */
	if (interval && (index_fpoints != num_fpoints)) {
		RFS_BUG_CRASH(sb, "file size is different from expectation:"
					" expected %u != %u\n",
					index_fpoints, num_fpoints);
		return -EIO;
	}

	if (last_clu) {
		*last_clu = prev;
		DEBUG(DL2, "last cluster number = %d \n", *last_clu);
	}

	return count;
}

/**
 *  append new cluster to fat chain of inode
 * @param inode		inode	
 * @param last_clu	current last cluster number of the fat chain
 * @param new_clu	new last cluster number to add
 * @return		return 0 on success, errno on failure
 */
int rfs_append_new_cluster(struct inode *inode, unsigned int last_clu, unsigned int new_clu)
{
	int err;

	err = rfs_fat_write(inode->i_sb, new_clu, CLU_TAIL);
	if (err) {
		DPRINTK("can't write a fat entry(%u)\n", new_clu);
		return err;
	}

	err = rfs_fat_write(inode->i_sb, last_clu, new_clu);
	if (err) {
		DPRINTK("can't write a fat entry(%u). "
			"cluster(%u) is lost\n", last_clu, new_clu);
		return err;
	}

	return err;
}

/**
 *  allocate a new cluster from fat table
 * @param inode		inode
 * @param start_clu	start cluster number for logging
 * @param[out] new_clu	new clsuter number to be allocated
 * @param last_clu	last clsuter number for logging
 * @return		return 0 on success, errno on failure
 */
int rfs_get_cluster(struct inode *inode, unsigned int *new_clu, 
		unsigned int last_clu, unsigned int b_is_xattr)
{
	struct super_block *sb = inode->i_sb;
	struct log_FAT_info lfi;
	unsigned int clu;
	int err;

	if (IS_FAST_LOOKUP_INDEX(inode))
		return -EFAULT;

	/* alloc-cluster from fat table */
	err = rfs_find_free_clusters(inode, &clu, 1);
	if (0 > err)
		return err;

	lfi.pdir = RFS_I(inode)->p_start_clu;
	lfi.entry = RFS_I(inode)->index;
	lfi.s_prev_clu = CLU_TAIL;
	lfi.s_next_clu = CLU_TAIL;
	lfi.d_prev_clu = last_clu;
	lfi.d_next_clu = CLU_TAIL;
	lfi.numof_clus = 1;
	lfi.clus = &clu;

	lfi.b_is_xattr = b_is_xattr;
#ifdef CONFIG_RFS_FS_XATTR
	lfi.ctime = RFS_I(inode)->xattr_ctime;
	lfi.cdate = RFS_I(inode)->xattr_cdate;
#else
	lfi.ctime = 0;
	lfi.cdate = 0;
#endif

	err = rfs_log_alloc_chain(sb, &lfi);
	if (err)
		return err;

	*new_clu = clu;
	return 0;
}

/**
 *  allocate a new cluster from fat table
 * @param inode		inode
 * @param[out] new_clu	new clsuter number to be allocated
 * @return		return 0 on success, errno on failure
 *
 * if file write or expand file(truncate), pre-allocation is available
 */ 
int rfs_alloc_cluster(struct inode *inode, unsigned int *new_clu)
{
	struct super_block *sb = inode->i_sb;
	unsigned int last_clu;
	int is_first = FALSE;
	int err;

	if (RFS_I(inode)->start_clu < VALID_CLU) { /* out-of-range input */
		DPRINTK("inode has invalid start cluster(%u)\n", 
				RFS_I(inode)->start_clu);
		return -EINVAL;
	}

	fat_lock(sb);

	if (RFS_I(inode)->start_clu != CLU_TAIL)
		last_clu = RFS_I(inode)->last_clu;
	else
		last_clu = CLU_TAIL;

	/* Phase 1 : get one free cluster in source */
	if (tr_pre_alloc(sb)) { /* pre-allocation case */
		err = rfs_log_get_cluster(inode, new_clu);	
		if (err)
			goto out;
	} else { /* normal allocation case */
		err = rfs_get_cluster(inode, new_clu, last_clu, FALSE);
		if (err)
			goto out;
	}

	/* Phase 2 : append free cluster to end of fat chain related to inode */
	if (last_clu == CLU_TAIL)
	{
		err = rfs_fat_write(sb, *new_clu, CLU_TAIL); 
		DEBUG(DL3, "eoc mark(%u)\n", *new_clu);
	}
	else
		err = rfs_append_new_cluster(inode, last_clu, *new_clu);
	if (err)
		goto out;

	/* update start & last cluster */
	if (RFS_I(inode)->start_clu == CLU_TAIL) {
		RFS_I(inode)->start_clu = *new_clu;
		is_first = TRUE;
	}
	RFS_I(inode)->last_clu = *new_clu;
	inode->i_blocks += ((unsigned long) 1 << (RFS_SB(sb)->cluster_bits - SECTOR_BITS));

	INC_USED_CLUS(sb, 1);

#ifdef RFS_CLUSTER_CHANGE_NOTIFY
	set_cluster_usage_notify(sb, TRUE);
#endif

	/* check rfs-specific inode state */
	if (unlikely(RFS_I(inode)->i_state == RFS_I_FREE)) 
	{
		/* RFS-log : have already logged things about following */
		if (is_first)
			err = rfs_open_unlink_chain(inode);
		else
			err = rfs_log_update_segment(inode);
	}

out:
	fat_unlock(sb);
	return err;
}

/**
 *  make a chunk element and add it to chunk list in sb(temp_clu_chunks).
 * @param sb		super block
 * @param start		start cluster number to free
 * @param nr_clusters	number of clusters to free	
 * @return		return 0 on success
 */

static int __add_chunk(struct super_block *sb, unsigned int start, 
		unsigned int nr_clusters)
{
	struct clu_chunk_list* p_chunk;

	if (!nr_clusters)
		return 0;

	p_chunk = (struct clu_chunk_list*)
		rfs_kmalloc(sizeof(struct clu_chunk_list), GFP_KERNEL, NORETRY);
	if (unlikely(!p_chunk)) 
	{ /* memory error */
		DEBUG(DL1, "memory allocation failed\n");
		return -ENOMEM;
	}

	p_chunk->chunk.start = start;
	p_chunk->chunk.count = nr_clusters;
	list_add_tail(&p_chunk->list, &RFS_SB(sb)->free_chunks);
	RFS_SB(sb)->nr_free_chunk++;
	DEBUG(DL2, "start:%d,count:%d,nr_free_chunk:%u", 
			start, nr_clusters, RFS_SB(sb)->nr_free_chunk);
	return 0;
}

/**
 *  remove all entry of free_chunks list in sb
 * @param sb		super block
 * @return		void
 */
static void __remove_chunk_list(struct super_block *sb)
{
	struct list_head *head = &RFS_SB(sb)->free_chunks;
	struct clu_chunk_list *chunk;

	/* make sure only single process access the list */
	while (!list_empty(head))
	{
		chunk = list_entry(head->next, struct clu_chunk_list, list);
		list_del(&chunk->list);
		kfree(chunk);
	}
}

/**
 * make a free_chunk list in sb & accumulate the count of clusters in chain
 * @param sb		super block
 * @param start_clu	start cluster number of fat chain
 * @param last_clu	last cluster number of fat chain
 * @param[out] clusters	number of clusters which may be appended or not
 * @return		return 0 on success, errno on failure
 *
 */
static int __build_chunk_list(struct super_block *sb, unsigned int *start_clu, 
		unsigned int last_clu, unsigned int *clusters)
{
	unsigned int prev = *start_clu, next;
	unsigned int total_nr_clus = 0, chunk_nr_clus = 0;
	unsigned int chunk_start = *start_clu;
	int err;

	while (1) { /* count clusters from start_clu */
		err = rfs_fat_read(sb, prev, &next);
		if (unlikely(err)) 
		{
			DPRINTK("can't read a fat entry(%u)\n", prev);
			goto del_list;
		}

		if (unlikely(next < VALID_CLU))
		{
			/* out-of-range input */
			RFS_BUG_CRASH(sb, "fat entry(%u,%u) was corrupted."
					" Expect (%u)\n", prev, next, last_clu);
			err = -EIO;
			goto del_list;
		}

		total_nr_clus++;

		/* make a chunk list (for deallocation & map destroy) */
		chunk_nr_clus++;
		if (!IS_CONSECUTION(prev, next))
		{
			err = __add_chunk(sb, chunk_start, chunk_nr_clus);
			if (unlikely(err))
			{
				DPRINTK("Err in adding chunk (%d)\n", err);
				goto del_list;
			}
			chunk_start = next;
			chunk_nr_clus = 0;
		}

		if (prev == last_clu) 
		{
			/* complete case */
			/* if not consecutive, already chunk is made */
			if (IS_CONSECUTION(prev, next))
			{
				err = __add_chunk(sb, chunk_start,
						chunk_nr_clus);
				if (unlikely(err))
				{
					DPRINTK("Err in adding chunk (%d)\n", 
							err);
					goto del_list;
				}
				chunk_nr_clus = 0;
			}
			/* finish building chunk */
			chunk_start = CLU_TAIL;
			break;
		}

		if (unlikely(next == CLU_TAIL)) 
		{
			/* out-of-range input */
			RFS_BUG_CRASH(sb, "Can't find expected last (%u)\n", 
					last_clu);
			err = -EIO;
			goto del_list;
		}
		prev = next;

		/*
		 * for efficient use of memory,
		 * build chunk list based on a capacity of logs
		 */ 
		if (rfs_log_max_chunk(sb)) {
			break;
		}
	}

	*start_clu = chunk_start;
	*clusters = (*clusters) + total_nr_clus;
	DEBUG(DL2, "total_nr_clus:%u", total_nr_clus);

	return 0;

del_list:
	/* list deletion & init outvars */
	__remove_chunk_list(sb);
	*clusters = 0;

	return err;
}

#ifdef CONFIG_RFS_MAPDESTROY
extern int (*sec_stl_delete)(dev_t dev, u32 start, u32 nums, u32 b_size);
#endif
/**
 *  deallocate clusters & call map_delete & free chunks
 * @param sb		super block
 * @param nr_chunk	chunk count to delete
 * @return		return 0 on success, errno on failure
 *
 */
static int __set_free_chunks(struct super_block *sb, unsigned int nr_chunk)
{
	struct list_head *head = &RFS_SB(sb)->free_chunks;
	struct clu_chunk_list *p_clu_chunk;
	unsigned int start, count;
	unsigned int location;
	int ret = 0;
	int idx;
	int err;


	BUG_ON(!RFS_SB(sb)->nr_free_chunk);
	BUG_ON(!nr_chunk);

	while (!list_empty(head) && nr_chunk)
	{
		p_clu_chunk = list_entry(head->next, struct clu_chunk_list, 
				list);
		start = p_clu_chunk->chunk.start;
		count = p_clu_chunk->chunk.count;
		DEBUG(DL2, "start = %d cnt = %d", start, count);

		for (idx = 0, location = start; idx < count; idx++, location++)
		{
			err = rfs_fat_write(sb, location, CLU_FREE);
			if (unlikely(err))
			{
				DPRINTK("can't write a fat entry(%u)\n", 
						location);
				if (!ret)
					ret = err;
			}

		}
#ifdef CONFIG_RFS_MAPDESTROY
		if (IS_DELETEABLE(sb->s_dev))
		{
			sec_stl_delete(sb->s_dev, START_BLOCK(start,sb),
					count << RFS_SB(sb)->blks_per_clu_bits,
					sb->s_blocksize);
		}
#endif
		/* delete chunk element from list and free them */
		list_del(&p_clu_chunk->list);
		kfree(p_clu_chunk);
		(nr_chunk) --;
		(RFS_SB(sb)->nr_free_chunk) --;
	}
	DEBUG(DL2, "nr_free_chunk%u", RFS_SB(sb)->nr_free_chunk);

	return ret;
}

/**
 * after logging, dealloc chunks 
 * @param sb		super block
 * @param ldi		log parameter (log_dealloc_info)	
 * @return		return 0 on success, errno on failure
 */
static int __dealloc_clu_chunk(struct super_block *sb, 
		struct log_DEALLOC_info *ldi)
{
	int err;
	unsigned int logged = 0; // logged cluster chunk
	struct rfs_log_info *rli = RFS_LOG_I(sb);
	
	DEBUG(DL2, "nr_free_chunk:%u", RFS_SB(sb)->nr_free_chunk);

	/* write log */
	err = rli->operations->log_dealloc_chain(sb, ldi, &logged);
	if (unlikely(err))
	{
		DPRINTK("Err in deallocating clunks (%d)\n", err);
		return err;
	}
	DEBUG(DL2, "nr_free_chunk:%u,logged:%d", 
			RFS_SB(sb)->nr_free_chunk, logged);

	/* dirty meta */
	err = __set_free_chunks(sb, logged);
	if (unlikely(err))
	{
		DPRINTK("Err in deallocating clunks (%d)\n", err);
		return err;
	}

	if (rli->need_mrc)
	{	
		DEBUG(DL2, "write mrc(%d)", logged);
		/* fat cache sync */
		/* sync meta (but, not data) */
		/* writing MRC */
		err = rfs_log_commit_mrc(sb, ldi);
		if (unlikely(err))
		{
			DPRINTK("Err in writing MRC(%d)\n", err);
			return err;
		}
	}

	if ((RFS_SB(sb)->nr_free_chunk))
	{
		DPRINTK("nr_free_chunk : %d\n", RFS_SB(sb)->nr_free_chunk);
		BUG();
	}
	RFS_BUG_ON(!list_empty(&RFS_SB(sb)->free_chunks));

	DEBUG(DL2, "f_start:%u,f_last:%u", ldi->s_start_clu, ldi->s_last_clu);
	DEBUG(DL2, "[truncate_b]f_new_last:%u", ldi->s_new_last);
	return 0;
}

/**
 *  free fat chain from start_clu to EOC
 * @param inode		inode
 * @param new_last	new last cluster number
 * @param start_clu	start cluster number to free
 * @return		return 0 on success, errno on failure
 * @pre			caller must have a mutex for fat table
 *
 *  new_last is only available if reduce a file especially truncate case
 */
static int __free_chain(struct inode *inode, unsigned int new_last, 
		unsigned int start_clu)
{
	struct super_block *sb = inode->i_sb;
	struct log_DEALLOC_info ldi;
	unsigned int free_count = 0;
	int ret = 0;
	int err = 0;

#ifdef CONFIG_RFS_FAST_LOOKUP
	if (RFS_I(inode)->index == RFS_FAST_LOOKUP_INDEX)
		return -EFAULT;
#endif

	BUG_ON(RFS_SB(sb)->nr_free_chunk);

	ldi.pdir = RFS_I(inode)->p_start_clu;
	ldi.entry = RFS_I(inode)->index;
	ldi.s_filesize = inode->i_size;
	ldi.s_new_last = new_last;
	ldi.s_start_clu = start_clu;
	ldi.s_last_clu = RFS_I(inode)->last_clu;
	ldi.is_truncate_b = (RFS_I(inode)->i_state != RFS_I_FREE);

	DEBUG(DL2, "f_start:%u,f_last:%u,is_truncate:%d", ldi.s_start_clu, 
			ldi.s_last_clu, ldi.is_truncate_b);

	/*
	 * for code sharing with delete_inode,
	 * make segment for truncated chain
	 */
	if (ldi.is_truncate_b)
	{
		err = rfs_log_segment_add(sb, ldi.s_start_clu, ldi.s_last_clu);
		if (err)
		{ /* Only ENOMEM, just return */
			DPRINTK("can't add segment!!");
			return err;
		}
	}

	/* logging segment location for replay */
	err = rfs_log_location_in_segment_list(sb, &ldi);
	if (err)
	{
		DPRINTK("can't find segment!!");
		return err;
	}
		
	/* each loop sets free clusters within a capacity of log */
	do 
	{
		/* build chunk list for logging & stl delete */
		err = __build_chunk_list(sb, &start_clu, RFS_I(inode)->last_clu,
				&free_count);
		if (unlikely(err))
		{ /* break and keep going while remains may be lost */
			DPRINTK("Err(%d) in building chunk list\n", err);
			ret = err;
			break;
		}

		/* logging next to delete */
		ldi.next_clu = start_clu;

		err = __dealloc_clu_chunk(sb, &ldi);
		if (unlikely(err)) 
		{ /* break and keep going while remains may be lost */
			DPRINTK("can't free clusters (%d)\n", err);
			ret = err;
			break;
		}
	} while (start_clu != CLU_TAIL);

	/* after dealloc chain update rfs_log_info */
	err = RFS_LOG_I(sb)->operations->log_move_chain_pp(sb, ldi.s_start_clu,
			ldi.s_last_clu);
	if (err)
	{
		DPRINTK("Err(%d) after dealloc chain\n", err);
		if (!ret)
			ret = err;
	}

	if (ldi.is_truncate_b)
	{/* set new last cluster */
		if (ldi.s_new_last != CLU_TAIL)
		{
			err = rfs_fat_write(sb, ldi.s_new_last, CLU_TAIL);
			if (unlikely(err))
			{
				DPRINTK("Err(%d) in write tail to truncated file\n", err);
				if (!ret)
					ret = err;
			}

			RFS_I(inode)->last_clu = new_last;
		}
		else
		{
			RFS_I(inode)->start_clu = CLU_TAIL;
			RFS_I(inode)->last_clu = CLU_TAIL;
		}
	}

	/* update used clusters */
	DEC_USED_CLUS(sb, free_count);

#ifdef RFS_CLUSTER_CHANGE_NOTIFY
	set_cluster_usage_notify(sb, TRUE);
#endif

	return ret;
}

#ifdef CONFIG_RFS_FS_XATTR
/**
 *  free fat chain of xattr from xattr_start_clus to xattr_last_clus
 * @param inode		inode
 * @return		return 0 on success, errno on failure
 * @pre			caller must have a mutex for fat table
 *
 *  new_last is only available if reduce a file especially truncate case
 */

static int __free_xattr_chain(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	struct log_DEALLOC_info ldi;
	unsigned int free_count = 0;
	int ret = 0;
	int err = 0;

	unsigned int start_clu = RFS_I(inode)->xattr_start_clus;

	BUG_ON(RFS_SB(sb)->nr_free_chunk);

	ldi.pdir = RFS_I(inode)->p_start_clu;
	ldi.entry = RFS_I(inode)->index;
	ldi.s_filesize = inode->i_size;
	ldi.s_new_last = NOT_ASSIGNED;
	ldi.s_start_clu = start_clu;
	ldi.s_last_clu = RFS_I(inode)->xattr_last_clus;
	ldi.is_truncate_b = FALSE;

	DEBUG(DL3, "f_start:%u,f_last:%u,is_truncate:%d", ldi.s_start_clu, 
			ldi.s_last_clu, ldi.is_truncate_b);

	/* logging segment location for replay */
	err = rfs_log_location_in_segment_list(sb, &ldi);
	if (err)
	{
		DPRINTK("can't find segment!!");
		return err;
	}
		
	/* each loop sets free clusters within a capacity of log */
	do 
	{
		/* build chunk list for logging & stl delete */
		err = __build_chunk_list(sb, &start_clu, 
				RFS_I(inode)->xattr_last_clus, &free_count);
		if (unlikely(err))
		{ /* break and keep going while remains may be lost */
			DPRINTK("Err(%d) in building chunk list\n", err);
			ret = err;
			break;
		}

		/* logging next to delete */
		ldi.next_clu = start_clu;

		err = __dealloc_clu_chunk(sb, &ldi);
		if (unlikely(err)) 
		{ /* break and keep going while remains may be lost */
			DPRINTK("can't free clusters (%d)\n", err);
			ret = err;
			break;
		}
	} while (start_clu != CLU_TAIL);

	/* after dealloc chain update rfs_log_info */
	err = RFS_LOG_I(sb)->operations->log_move_chain_pp(sb, ldi.s_start_clu,
			ldi.s_last_clu);
	if (err)
	{
		DPRINTK("Err(%d) after dealloc chain\n", err);
		if (!ret)
			ret = err;
	}

	/* update used clusters */
	DEC_USED_CLUS(sb, free_count);

#ifdef RFS_CLUSTER_CHANGE_NOTIFY
	set_cluster_usage_notify(sb, TRUE);
#endif

	return ret;
}
#endif

/**
 *  deallocate clusters after skipping some clusters
 * @param inode	inode
 * @param skip	the number of clusters to skip
 * @return	return 0 on success, errno on failure
 */
int rfs_dealloc_clusters(struct inode *inode, unsigned int skip)
{
	struct super_block *sb = inode->i_sb;
	unsigned int new_last = NOT_ASSIGNED;
	unsigned int prev, next;
	unsigned int off = skip - 1;
	int err = 0;

	fat_lock(sb);

 #ifdef CONFIG_RFS_FAST_SEEK
 	/* fast seek infos are no more valid */
 	rfs_free_fast_seek(RFS_I(inode));
 #endif

	if (!skip) { /* free all clusters */
		next = RFS_I(inode)->start_clu;
		goto free;
	}

	err = rfs_find_cluster(sb, RFS_I(inode)->start_clu, off, &prev, &next);
	if (err)
		goto out;

	/* change last cluster number */
	new_last = prev;

free:	
	if (next == CLU_TAIL) /* do not need free chain */
	{
		DEBUG(DL2, "empty file");
		/*goto xattr;*/ /* skip __free_chain for empty file */
		/*goto out;*/
	}
	else /*if (next != CLU_TAIL)*/
	{
		err = __free_chain(inode, new_last, next);
		if (err)
		{
			DEBUG(DL0, "fail to free chain");
			goto out;
		}
#ifdef CONFIG_RFS_FS_XATTR
		/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
		/* commit previous sub-log(dealloc) */
		err = rfs_meta_commit(sb);
		if (err)
		{
			DEBUG(DL0, "fail to rfs_meta_commit(%d)", err);
			goto out;	
		}
#endif
	}
#ifdef CONFIG_RFS_FS_XATTR
	/*if oeration is not truncate_b and inode has xattr cluster*/
	if ( (RFS_I(inode)->i_state == RFS_I_FREE) && 
			IS_XATTR_EXIST(RFS_I(inode)))
	{
		DEBUG(DL3, "free xattr clu:%u", RFS_I(inode)->xattr_start_clus);
		err = __free_xattr_chain(inode);
		if (err)
		{
			DEBUG(DL0, "fail to xattr free chain");
			goto out;
		}
	}
#endif

out:
	fat_unlock(sb);

	return err;
}

/**
 *  find a cluster which has an offset into fat chain
 * @param sb 		super block
 * @param start_clu	start cluster number of fat chain
 * @param off		offset within fat chain
 * @param[out] clu	cluster number found 
 * @param[out] value	fat entry value of cluster
 * @return return 0 on success, errno on failure
 * @pre			caller should get fat lock
 */ 
int rfs_find_cluster(struct super_block *sb, unsigned int start_clu, unsigned int off, unsigned int *clu, unsigned int *value)
{
	unsigned int prev, next;
	unsigned int i;
	int err;

	prev = start_clu;
	for (i = 0; i <= off; i++) {
		err = rfs_fat_read(sb, prev, &next);
		if (err) {
			DPRINTK("can't read a fat entry (%u)\n", prev);
			return err;
		}

		if (next < VALID_CLU) { /* out-of-range input */
			/*
			 * If reset happened during appending a cluster,
			 * the appending cluster had a free status (0).
			 * EFAULT notifies it to replay method.
			 */
			if (tr_in_replay(sb))
				return -EFAULT;
			else 
			{
				DPRINTK("invalid value (%u:%u)\n", prev, next);
 				return -EIO;
			}
		}

		if (i == off) /* do not need to change prev to next */
			break;

		if (next == CLU_TAIL)
		{
			/* It is normal in rfs_count_subdir() */
 			return -EFAULT; /* over request */
		}

		prev = next;
	}

	*clu = prev;
	*value = next;
	return 0;
}

