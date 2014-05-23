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
 *  @file	fs/rfs/file.c
 *  @brief	file and file inode functions
 *
 *
 */

#include <linux/spinlock.h>
#include <linux/smp_lock.h>
#include <linux/fs.h>
#include <linux/rfs_fs.h>
#include <asm/uaccess.h>
#ifdef CONFIG_RFS_FS_XATTR
#include <linux/xattr.h>
#include "xattr.h"
#endif

#include "rfs.h"
#include "log.h"

#define RFS_FAST_SEEK			0x6701
#define RFS_FSEEK_DEFAULT_INTERVAL	4

#ifdef CONFIG_GCOV_PROFILE
#define	loff_t		off_t
#endif


/**
 *  check the hint info of inode whether it is available 
 * @param inode		inode
 * @param num_clusters	number of clusters to skip
 * @param start_clu	start cluster number to read after skipping hint
 * @return		return 0 on success, EINVAL on failure
 *
 *  If hint info has availability, rest cluster chain can be read after skipping specified clusters
 */
static int __rfs_lookup_hint(struct inode *inode, unsigned int *num_clusters, unsigned int *start_clu)
{
	struct rfs_inode_info *rii = RFS_I(inode);
	unsigned int clus = *num_clusters;

	if (rii->start_clu == CLU_TAIL)
		return -EFAULT;

	/* use hint */
	if ((clus > 0) && (rii->hint_last_offset > 0)
			&& (rii->hint_last_clu >= VALID_CLU)
			&& (clus >= rii->hint_last_offset)) {
		*start_clu = rii->hint_last_clu;
		clus -= rii->hint_last_offset;
	} else {
		*start_clu = rii->start_clu;
	}

#ifdef CONFIG_RFS_FAST_SEEK
	{
		struct fast_seek_info *fast_seek = rii->fast_seek;

		/* are fast seek infos more effective than hint? */
		if (fast_seek && (clus > fast_seek->interval)) {
			unsigned int index;

			index = (*num_clusters) >> fast_seek->interval_bits;
			if (index < fast_seek->num_fpoints) {
				clus = (*num_clusters) & fast_seek->interval_mask;
				*start_clu = fast_seek->fpoints[index];
			}
			/* "else" means file was extended after building fast seek */
		}
	}
#endif

	*num_clusters = clus;

	return 0;
}

/**
 *  invalidate the hint info of inode
 * @param inode	inode
 */ 
void inline rfs_invalidate_hint(struct inode *inode)
{
	RFS_I(inode)->hint_last_clu = 0;
	RFS_I(inode)->hint_last_offset = 0;
}

/**
 *  update the hint info of inode 
 * @param inode		inode
 * @param cluster	previous last cluster number accessed
 * @param offset	cluster offset into a fat chain
 */
static inline void
__rfs_update_hint(struct inode *inode, 
		unsigned int cluster, unsigned int offset)
{
	if ((cluster < VALID_CLU) || (!offset)) 
	{
		rfs_invalidate_hint(inode);
	} 
	else 
	{
		RFS_I(inode)->hint_last_clu = cluster;
		RFS_I(inode)->hint_last_offset = offset;
	}
}

/**
 *  fill the page with zero
 * @param inode	inode 	 
 * @param page	page pointer
 * @param zerofrom	start offset within page 	
 * @param zeroto 	last offset within page	 
 * @param get_block 	get_block funciton	 
 * @return	return 0 on success, errno on failure
 *
 * This function is invoked by rfs_extend_with_zerofill
 */
static int 
__rfs_page_zerofill(struct inode *inode, struct page *page,
		unsigned zerofrom, unsigned zeroto, get_block_t *get_block) 
{
	struct buffer_head *bh, *head;
	unsigned long block;
	unsigned block_start, block_end, blocksize, bbits, blk_aligned_zeroto; 
	int err = 0, partial = 0;
	char *kaddr;

	bbits = inode->i_blkbits;
	blocksize = (unsigned) (1 << bbits);

#ifdef RFS_FOR_2_6
	if (!page_has_buffers(page))
		create_empty_buffers(page, blocksize, 0);
	head = page_buffers(page);
#else
	if (!page->buffers) 
		create_empty_buffers(page, inode->i_dev, blocksize);
	head = page->buffers;
#endif

	/* start block # */
	block = page->index << (PAGE_CACHE_SHIFT - bbits);

	/*
	 * In the first phase,
	 *  we allocate buffers and map them to fill zero
	 */
	for (bh = head, block_start = 0; (bh != head) || (!block_start);
		block++, 
		block_start = block_end + 1,
		bh = bh->b_this_page)
	{
		if (!bh) { 		/* I/O error */
			err = -EIO;
			RFS_BUG_CRASH(inode->i_sb, "can't get buffer head\n");
			goto out;
		}
		block_end = block_start + blocksize - 1;
		if (block_end < zerofrom) 
			continue;
		else if (block_start > zeroto) 
			break;
		clear_bit(BH_New, &bh->b_state);

		/* map new buffer head */	
#ifdef RFS_FOR_2_6
 		err = get_block(inode, (sector_t) block, bh, 1);
#else
 		err = get_block(inode, (long) block, bh, 1);
#endif
 		if (err) {
 			DEBUG(DL1, "no block\n");	
 			goto out;
 		}

		if ((!buffer_uptodate(bh)) && (block_start < zerofrom)) {
			ll_rw_block(READ, 1, &bh);
			wait_on_buffer(bh);
			if (!buffer_uptodate(bh)) 
			{
				err = -EIO;
				RFS_BUG_CRASH(inode->i_sb, 
						"Fail to read\n");
				goto out;
			}
		}
	}

	/* 
	 * In the second phase, we memset the page with zero,
	 * in the block aligned manner.
	 * If memset is not block-aligned, hole may return garbage data.
	 */
	blk_aligned_zeroto = zeroto | (blocksize - 1);
	kaddr = kmap_atomic(page, KM_USER0);
	memset(kaddr + zerofrom, 0, blk_aligned_zeroto - zerofrom + 1);

	/*
	 * In the third phase, 
	 * we make the buffers uptodate and dirty
	 */
	for (bh = head, block_start = 0; (bh != head) || (!block_start);
		block_start = block_end + 1,
		bh = bh->b_this_page)
	{
		block_end = block_start + blocksize - 1;
		if (block_end < zerofrom) {
			/* block exists in the front of zerofrom. */
			if (!buffer_uptodate(bh))
				partial = 1;
			continue;
		} else if (block_start > zeroto) {  
			/* block exists in the back of zeroto. */
			partial = 1;
			break;
		}

#ifdef RFS_FOR_2_6
		set_buffer_uptodate(bh);
		mark_buffer_dirty(bh);
#else
		mark_buffer_uptodate(bh, 1);
		mark_buffer_dirty(bh);
		down(&RFS_I(inode)->data_mutex);
		buffer_insert_inode_data_queue(bh, inode);
		up(&RFS_I(inode)->data_mutex);
#endif
	}

	/* if all buffers of a page were filled zero */ 
	if (!partial)
		SetPageUptodate(page);

out:
	flush_dcache_page(page);	
	kunmap_atomic(kaddr, KM_USER0);
	return err;	
}

/**
 *  extend a file with zero-fill
 * @param inode	inode
 * @param origin_size	file size before extend	
 * @param new_size 	file size to extend	 
 * @return	return 0 on success, errno on failure
 *
 * rfs doesn't allow holes. 
 */
int rfs_extend_with_zerofill(struct inode *inode,
		unsigned int origin_size, unsigned int new_size) 
{
	struct address_space *mapping = inode->i_mapping;
	struct super_block *sb = inode->i_sb;
	struct page *page = NULL;
	unsigned long index, final_index;
	unsigned long next_page_start, offset, next_offset;
	unsigned int origin_clusters, new_clusters, clusters_to_extend;
	unsigned zerofrom, zeroto;  /* offset within page */
	int err = 0;
	
	/* compare the number of required clusters with that of free clusters */
	origin_clusters = (origin_size + RFS_SB(sb)->cluster_size - 1)
				>> RFS_SB(sb)->cluster_bits;
	new_clusters = (new_size + RFS_SB(sb)->cluster_size - 1)
				>> RFS_SB(sb)->cluster_bits;
	clusters_to_extend = new_clusters - origin_clusters;

	if (clusters_to_extend && 
			(clusters_to_extend > GET_FREE_CLUS(sb))) {
		DEBUG(DL2, "No space \n");
		return -ENOSPC;
	}	

	offset = origin_size;
	final_index = (new_size - 1) >> PAGE_CACHE_SHIFT; /* newsize isn't 0 */
	while ((index = (offset >> PAGE_CACHE_SHIFT)) <= final_index) {

		/* get cache page */
		page = grab_cache_page(mapping, index);
		if (!page) { /* memory error */
			DEBUG(DL0, "out of memory !!");
			return -ENOMEM;
		}

		/* calculate zerofrom and zeroto */
		next_page_start = (index + 1) << PAGE_CACHE_SHIFT;
		next_offset = (new_size > next_page_start) ? 
			next_page_start : new_size;

		zerofrom = offset & (PAGE_CACHE_SIZE - 1);
		zeroto = (next_offset - 1) & (PAGE_CACHE_SIZE - 1);

		err = __rfs_page_zerofill(inode, page, zerofrom, zeroto, rfs_get_block);
		if (err) {
			if (unlikely(err == -ENOSPC)) {
				DEBUG(DL0, "The # of the real free clusters is"
						" different from super block.");
				err = -EIO;
			}
			ClearPageUptodate(page);
			unlock_page(page);
			page_cache_release(page);	
			DEBUG(DL1, "zero fill failed (err : %d)\n", err);
			goto out;
		}

		offset = next_page_start;
		unlock_page(page);
		page_cache_release(page);
	}

out:
	return err;
}

/**
 *  truncate a file to a specified size
 * @param inode	inode
 *
 * support to reduce or enlarge a file 
 */
void rfs_truncate(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	unsigned int num_clusters = 0;
	unsigned long origin_size, origin_mmu_private;
	int is_back = 0;
	int err;

	origin_size = RFS_I(inode)->trunc_start;
	origin_mmu_private = RFS_I(inode)->mmu_private; 

	/* check the validity */
	if (IS_RDONLY(inode))
		goto rollback_size; 

	if (!(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode) || 
				S_ISLNK(inode->i_mode)))
		goto rollback_size; 

	if (IS_IMMUTABLE(inode) || IS_APPEND(inode))
		goto rollback_size;

	/* RFS-log : start truncate */
	if ((loff_t) origin_size == (loff_t) inode->i_size) {
		/* at 2.4 kernel */
		/* truncate forward but already zero filled, so do nothing */
		return;
	} else if ((loff_t) origin_size < (loff_t) inode->i_size) {
		/* if caller is inode_setattr, tr can be nested */
		err = rfs_log_start(sb, RFS_LOG_TRUNCATE_F, inode);
		is_back = 0;
	} else {
		err = rfs_log_start(sb, RFS_LOG_TRUNCATE_B, inode);
		is_back = 1;
	}
	if (err)
		goto rollback_size;

	/* transactin starts from here */
	if (RFS_I(inode)->i_state == RFS_I_FREE)
		/* RFS do not perform truncate of unlinked file */
		goto end_log;

	/*
	 * reduce a file
	 */
	if (is_back) {
		block_truncate_page(inode->i_mapping,
				inode->i_size, rfs_get_block);
		num_clusters = (unsigned int) 
				((inode->i_size + RFS_SB(sb)->cluster_size - 1) 
				 >> RFS_SB(sb)->cluster_bits);

		err = rfs_dealloc_clusters(inode, num_clusters);
		if (err) {
			/* 
			 * media failure 
			 * Even though this failure may result in serious error,
			 * rfs_truncate can propagate it to the upper layer.
			 * This will make it difficult to debug the error 
			 * caused by media failure.
			 * Should we revive the dealloced clusters?
			 * Or, should we truncate the file anyway?
			 *
			 * We can mark the inode with media fail.
			 */
			goto invalidate_hint;
		}
		set_mmu_private(inode, inode->i_size);
	}
	/*
	 * extend a file
	 */
	else if (origin_mmu_private < (unsigned long) (inode->i_size))
	{
		/* extending file with zero */
		err = rfs_extend_with_zerofill(inode,
				(u32) origin_mmu_private, (u32) inode->i_size);
		if (err == -ENOSPC) {
			DEBUG(DL2, "failed to enlarge a file");
			goto end_log;
		} else if (err) {
			DEBUG(DL2, "failed to enlarge a file");
			truncate_inode_pages(inode->i_mapping, origin_size);
			/*
			 * media failure
			 * can not recover in run-time empirically
			 */
			RFS_BUG_CRASH(sb, "failed to reduce a file\n");

			goto invalidate_hint;
		}
	}
	else
	{
		/* truncate forward but already zero filled, so do nothing */
	}

	/* invalidate hint info */
	rfs_invalidate_hint(inode);

	inode->i_blocks = (blkcnt_t) 
			(((inode->i_size + RFS_SB(sb)->cluster_size - 1)
			  	& (~(RFS_SB(sb)->cluster_size -1)))
			 	>> SECTOR_BITS);
	inode->i_mtime = inode->i_atime = CURRENT_TIME;

	rfs_mark_inode_dirty_in_tr(inode);

	/* RFS-log : end truncate */
	if (rfs_log_end(sb, 0)) 
	{
		/* should we mark the file with media failure */
		;
	}
	return;

	/*
	 * handle error
	 */

invalidate_hint:
	rfs_invalidate_hint(inode);
	RFS_I(inode)->mmu_private = origin_mmu_private;
	inode->i_size = (loff_t) origin_size;
	rfs_mark_inode_dirty_in_tr(inode);
	rfs_log_end(sb, 1);
	return;

end_log:
	inode->i_size = (loff_t) origin_size;
	rfs_mark_inode_dirty_in_tr(inode);
	rfs_log_end(sb, 1);
	return;

rollback_size:
	inode->i_size = (loff_t) origin_size;
	rfs_mark_inode_dirty(inode);
	return;
}

#ifdef CONFIG_RFS_POSIX_ATTR
void rfs_get_uid(struct rfs_dir_entry *ep, uid_t *uid)
{
	*uid = ep->ctime & UID_MASK;
}

void rfs_get_gid(struct rfs_dir_entry *ep, gid_t *gid)
{
	*gid = (ep->ctime >> UID_BITS) & GID_MASK;
}

void rfs_get_mode(struct rfs_dir_entry *ep, umode_t *mode)
{
	umode_t perm = S_IRWXUGO;
	unsigned short grp_perm;

	/* If cmsec has real value for create time */
	if ((ep->cmsec & SPECIAL_MARK) != SPECIAL_MARK)
	{
		*mode |= perm;

		/* handle read-only case */
		if (ep->attr & ATTR_READONLY)
			*mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
		return;
	}

	/* user permission */
	if (ep->attr & ATTR_HIDDEN)
		perm &= ~S_IRUSR;
	if (ep->attr & ATTR_READONLY)
		perm &= ~S_IWUSR;
	if (ep->attr & ATTR_ARCHIVE)
		perm &= ~S_IXUSR;

	/* group&other permission */
	grp_perm = (ep->cmsec >> GID_PERM_BITS) & GID_PERM_RWX;
	if (grp_perm & 0x1)
		perm &= ~(S_IXGRP | S_IXOTH);
	if (grp_perm & 0x2)
		perm &= ~(S_IWGRP | S_IWOTH);
	if (grp_perm & 0x4)
		perm &= ~(S_IRGRP | S_IROTH);

	*mode |= perm;
}

void rfs_get_mask(struct rfs_dir_entry *ep, umode_t *mode)
{
	if (!IS_SYMLINK(ep))
		*mode &= ~(rfs_current_umask());
}

void rfs_set_uid(struct rfs_dir_entry *ep, uid_t uid)
{
	unsigned short ctime;

	ctime = (GET16(ep->ctime) & ~UID_MASK) | (uid & UID_MASK);
	SET16(ep->ctime, ctime);
}

void rfs_set_gid(struct rfs_dir_entry *ep, gid_t gid)
{
	unsigned short ctime;

	ctime = (GET16(ep->ctime) & ~(GID_MASK << UID_BITS)) | 
		((gid & GID_MASK) << UID_BITS);
	SET16(ep->ctime, ctime);
}

#define rfs_attr_perm(attr, mode, type) ((mode)? (attr & ~type): (attr | type))

void rfs_set_mode(struct rfs_dir_entry *ep, umode_t mode)
{
	if (ep->attr & ATTR_SYSTEM)
		return;		/* -EPERM */

	/* set owner permission */
	ep->attr = rfs_attr_perm(ep->attr, mode & S_IRUSR, ATTR_HIDDEN);
	ep->attr = rfs_attr_perm(ep->attr, mode & S_IWUSR, ATTR_READONLY);
	ep->attr = rfs_attr_perm(ep->attr, mode & S_IXUSR, ATTR_ARCHIVE);

	/* set group permission */
	ep->cmsec |= (GID_PERM_RWX << GID_PERM_BITS);
	ep->cmsec &= ~(((mode & S_IRWXG) >> 3) << GID_PERM_BITS);

	/* mark that cmsec field don't have normal ctime */
	if (!IS_SYMLINK(ep) && !IS_SOCKET(ep) && !IS_FIFO(ep))
		ep->cmsec = ((ep->cmsec & ~SPECIAL_MASK) | SPECIAL_MARK);
}
#else	/* ifdef CONFIG_RFS_POSIX_ATTR */
void rfs_get_uid(struct rfs_dir_entry *ep, uid_t *uid)
{
	*uid = 0;
}

void rfs_get_gid(struct rfs_dir_entry *ep, gid_t *gid)
{
	*gid = 0;
}

void rfs_get_mode(struct rfs_dir_entry *ep, umode_t *mode)
{
	*mode |= S_IRWXUGO;
	if (ep->attr & ATTR_READONLY)   /* handle read-only case */
		*mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
}

void rfs_set_uid(struct rfs_dir_entry *ep, uid_t uid)
{
	/* nothing */
	return;
}

void rfs_set_gid(struct rfs_dir_entry *ep, gid_t gid)
{
	/* nothing */
	return;
}

void rfs_set_mode(struct rfs_dir_entry *ep, umode_t mode)
{
	if (mode & (S_IWUSR | S_IWGRP | S_IWOTH))
		ep->attr &= ~ATTR_READONLY;
	else
		ep->attr |= ATTR_READONLY;
}

void rfs_get_mask(struct rfs_dir_entry *ep, umode_t *mode)
{
	return;
}
#endif

static int
__set_guidmode(struct inode * inode, struct iattr *attr)
{
	int err = 0;
	const unsigned int ia_valid = attr->ia_valid;

	struct buffer_head *bh = NULL;
	struct rfs_dir_entry *ep = NULL;
	struct rfs_inode_info *rii = RFS_I(inode);

#ifdef CONFIG_RFS_FS_FULL_PERMISSION
	if (!test_opt(inode->i_sb, EA))
#endif
	{
		ep = get_entry_with_cluster(inode->i_sb, rii->p_start_clu, 
				rii->index, &bh, inode);
		if (IS_ERR(ep)) {
			if (PTR_ERR(ep) == -EFAULT)
				err = -ENOENT;
			else
				err = PTR_ERR(ep);
			return err;
		}

		/* change uid & gid */
		if ((ia_valid & ATTR_UID && attr->ia_uid != inode->i_uid) ||
			(ia_valid & ATTR_GID && attr->ia_gid != inode->i_gid)) {
			if (attr->ia_valid & ATTR_UID)	/* change uid */
				rfs_set_uid(ep, attr->ia_uid);
			if (attr->ia_valid & ATTR_GID)	/* change gid */
				rfs_set_gid(ep, attr->ia_gid);
		}
		/* change permission */
		if (ia_valid & ATTR_MODE)
			rfs_set_mode(ep, attr->ia_mode);
		
		mark_buffer_dirty(bh);

		brelse(bh);
	}
#ifdef CONFIG_RFS_FS_FULL_PERMISSION
	/*TODO handling ATTR_FORCE */
	else if ((ia_valid & ATTR_UID && attr->ia_uid != inode->i_uid) ||
		(ia_valid & ATTR_GID && attr->ia_gid != inode->i_gid) ||
		(ia_valid & ATTR_MODE) )
	{
		umode_t *p_mode = NULL;
		uid_t *p_uid = NULL;
		gid_t *p_gid = NULL;
		umode_t mode_tmp;

	      	/* assign uid */
		if (attr->ia_valid & ATTR_UID)
			p_uid = & attr->ia_uid;
		/* assign gid */
		if (attr->ia_valid & ATTR_GID)
		       	p_gid = & attr->ia_gid;
		/* assign mode */
		if ((attr->ia_valid & ATTR_MODE))
		{
			mode_tmp = (attr->ia_mode & 
					(S_ISGID | S_ISVTX | S_IRWXUGO));
			p_mode = & mode_tmp;
		}

		/* set uid, gid, mode to xattr head */
		err = rfs_xattr_set_guidmode(inode, p_uid, p_gid, p_mode);
		if (err)
		{
			DEBUG(DL0, "fail to rfs_xattr_set_guidmode(err:%d)", 
					err);
			return err;
		}
	}
#endif
	return err;
}
/**
 *  change an attribute of inode
 * @param dentry	dentry
 * @param attr		new attribute to set
 * @return		return 0 on success, errno on failure
 * 
 * it is only used for chmod, especially when read only mode be changed
 */
int rfs_setattr(struct dentry *dentry, struct iattr *attr)
{
	struct inode *inode = dentry->d_inode;
	struct super_block *sb = inode->i_sb;
	
	int err = 0;
	int tr_start = 0;
	const unsigned int ia_valid = attr->ia_valid;

	CHECK_RFS_INODE(inode, -EINVAL);

	if (IS_FAST_LOOKUP_INDEX(inode))
		return -EFAULT;

	if (rfs_check_reserved_files(inode, NULL))
		return -EPERM;

	if (ia_valid & ATTR_SIZE) 
	{
		if ((loff_t) inode->i_size < (loff_t) attr->ia_size) 
		{
			/* bugfix from RFS_1.2.3p2_b017 :
			 * RFS can return error from rfs_truncate to VFS
			 * by vmtruncate. Because we have to check free
			 * clusters and expand file here.
			 */
			if (rfs_log_start(sb, RFS_LOG_TRUNCATE_F, inode))
			{
				return -EIO;
			}

			tr_start = 1; /* transaction starts */

			err = rfs_extend_with_zerofill(inode,
					(u32) RFS_I(inode)->mmu_private,
					(u32) attr->ia_size);
			if (err) 
			{
				rfs_log_end(sb, err);
				return err;
			}

			/* update zerofilled inode's size */
			inode->i_size = attr->ia_size;
			inode->i_blocks = (blkcnt_t)
				(((inode->i_size + RFS_SB(sb)->cluster_size - 1)
				& (~(RFS_SB(sb)->cluster_size -1)))
				>> SECTOR_BITS);		
			attr->ia_valid |= ATTR_MTIME;
			attr->ia_valid &= ~ATTR_SIZE;
		}

		/* keep current inode size for truncate operation */
		RFS_I(inode)->trunc_start = (unsigned long) (inode->i_size);
	}
	
	err = inode_change_ok(inode, attr);
	if (err)
	{
		DEBUG(DL0, "fail to inode_change_ok():%d", err);
		goto out_in_tr;
	}

	/* set gid, uid, mode */
	err = __set_guidmode(inode, attr);
	if (err)
	{
		DEBUG(DL0, "fail to __set_guidmode(%d)", err);
		goto out_in_tr;
	}

	err = inode_setattr(inode, attr);

out_in_tr:

	if (tr_start)
		rfs_log_end(sb, err);

	return err;
}

/**
 *  check a permission of inode
 * @param inode		inode
 * @param mode		mode
 * @param nd		nameidata
 * @return		return 0 on success, EPERM on failure
 *
 * System file (logfile) can not be accessed 
 */
#if defined(RFS_FOR_2_6) && !defined(RFS_FOR_2_6_27)
int rfs_permission(struct inode *inode, int mode, struct nameidata *nd)
#else
int rfs_permission(struct inode *inode, int mode)
#endif
{
	int error = 0;

	if (mode & (MAY_WRITE | MAY_READ))
	{
		error = rfs_check_reserved_files(inode, NULL);
		if (error)
			return error;
	}

#ifdef CONFIG_RFS_FAST_LOOKUP
	if (RFS_I(inode)->fast && (mode & (MAY_WRITE | MAY_EXEC)))
		return -EPERM;
#endif


#ifdef CONFIG_RFS_POSIX_ATTR
# ifdef RFS_FOR_2_6_10
	return generic_permission(inode, mode & ~MAY_APPEND, NULL);
# else
	return vfs_permission(inode, mode);
# endif
#else
	return 0;
#endif
}

#ifdef CONFIG_GCOV_PROFILE
#undef loff_t
#endif

/**
 *  write up to count bytes to file at speicified position 
 * @param file		file
 * @param buf		buffer pointer
 * @param count		number of bytes to write
 * @param ppos		offset in file
 * @return 		return write bytes on success, errno on failure
 * 
 * use pre-allocation for reducing working logs
 */
static ssize_t rfs_file_write(struct file *file, const char *buf,
		size_t count, loff_t *ppos)
{
	struct inode *inode;
	ssize_t ret;
	int err;

#ifdef RFS_FOR_2_6_19
	ret = do_sync_write(file, buf, count, ppos);
#else
	ret = generic_file_write(file, buf, count, ppos);
#endif
	if (ret <= 0)
		return ret;

	inode = file->f_dentry->d_inode->i_mapping->host;
	if ((file->f_flags & O_SYNC) || IS_SYNC(inode)) {
		err = rfs_log_force_commit(inode->i_sb, inode);
		if (err)
			return -EIO;
	}

	return ret;
}

/**
 *  flush all dirty buffers of inode include data and meta data
 * @param file		file pointer
 * @param dentry	dentry pointer
 * @param datasync	flag
 * @return return 0 on success, EPERM on failure
 */
static int rfs_file_fsync(struct file * file, struct dentry *dentry, int datasync)
{
	struct inode *inode = dentry->d_inode;
	struct super_block *sb = inode->i_sb;
	int ret = 0, err = 0; 
	
	/* data commit & entty update */
#ifdef RFS_FOR_2_6
	ret = rfs_write_inode(inode, 1);
#else
	rfs_write_inode(inode, 1);
#endif

	/* meta-commit deferred tr */
	if (tr_deferred_commit(sb) && RFS_LOG_I(sb)->inode &&
			(RFS_LOG_I(sb)->inode == inode)) {
		err = rfs_log_force_commit(inode->i_sb, inode); 
		if (err && (!ret))
			ret = err;
	}
	
	return ret;
}

#ifdef CONFIG_RFS_SYNC_ON_CLOSE
/**
 *  flush modified data of file object on .flush
 * @file       file object to flush
 *
 * It is only called when sync_on_close option is enable.
 */
static int rfs_file_flush(struct file * file, fl_owner_t id)
{
	int ret = 0;
	struct inode * inode = file->f_dentry->d_inode;

	ret = rfs_sync_inode_pages(inode);
	if (ret)
	{
		DEBUG(DL0, "rfs_sync_inode_pages fails (%d)", ret);
		return ret;
	}

	return ret;
}
#else
#define rfs_file_flush		NULL
#endif

#if defined(CONFIG_RFS_SYNC_ON_CLOSE) || defined(CONFIG_RFS_FAST_SEEK)
/**
 *  flush modified data of file object
 * @inode	inode of file object to flush
 * @file	file object to flush
 *
 * It is only called when all files are closed, that is inode is released
 */
static int rfs_file_release(struct inode * inode, struct file * file)
{

	rfs_free_fast_seek(RFS_I(inode));

#ifndef CONFIG_RFS_SYNC_ON_CLOSE
	return 0;
#endif

	return rfs_file_fsync(file, file->f_dentry, 0);
}
#else

#define rfs_file_release	NULL
#endif

#ifdef CONFIG_RFS_FAST_SEEK
/**
 *  free fast-seek infos
 * @param rii		private inode
 */
void rfs_free_fast_seek(struct rfs_inode_info *rii)
{
	/* free fast seek info */
	if (rii->fast_seek) {
		DEBUG(DL2, "free %p fast seek\n", rii->fast_seek->fpoints);
		kfree(rii->fast_seek->fpoints);
		kfree(rii->fast_seek);
		rii->fast_seek = NULL;
	}
}
#else

#define rfs_free_fast_seek(rii)	do { } while(0)

#endif

#ifdef CONFIG_RFS_FAST_SEEK
/**
 *  build fast-seek infos
 * @inode	inode to need fast-seek
 * @interval	the user-defined interval
 */
static int rfs_build_fseek(struct inode *inode, unsigned int interval)
{
	struct super_block *sb = inode->i_sb;
	struct rfs_inode_info *rii = RFS_I(inode);
	unsigned int num_clus;
	unsigned int num_fpoints;
	unsigned int interval_bits;
	unsigned int *fpoints;
	int ret;
	
	/*
	 * To prevent race condition with other system calls which
	 * modify file size, obtain log lock
	 */
	lock_log(sb);

	/* don't build fast seek for an unlinked open file */
	if (RFS_I(inode)->i_state == RFS_I_FREE) {
		unlock_log(sb);
		return 0;
	}

	/* don't build fast seek for an empty file */
	if (!(rii->mmu_private)) {
		unlock_log(sb);
		return 0;
	}

	/* How many clusters does a file have? */
	num_clus = (unsigned int) ((rii->mmu_private +
				RFS_SB(sb)->cluster_size - 1) >>
			(RFS_SB(sb)->cluster_bits));

	/* trim interval as the power of 2 */
	interval_bits = (unsigned int) ffs(interval) - 1;
	interval = (1 << interval_bits);

	while (1)
	{ /* keep try to alloc memory */
		if (interval >= num_clus) {
			/* enough small to seek fast */
			unlock_log(sb);
			return 0;
		}

		/* How many fast seek infos will a file need? */
		num_fpoints = (unsigned int) ((num_clus + interval - 1) >>
				interval_bits);

		fpoints = rfs_kmalloc(sizeof(unsigned int) * num_fpoints,
				GFP_KERNEL, NORETRY);
		if (fpoints)
			break;
		
		/* short memory. increase interval */
		interval = interval << 1;	
		interval_bits++;
	}

	/* get fast fpoints while scaning a whole chain */
	ret = rfs_find_last_cluster(inode, inode->start_clu, NULL, interval, 
			num_fpoints, fpoints);

	unlock_log(sb);

	if (ret < 0)
	{
		DPRINTK("can not build fast seek infos (%d)\n", ret);
		kfree(fpoints);
		return ret;
	}

	/* free existing fast seek array */
	if (rii->fast_seek)
	{
		kfree(rii->fast_seek->fpoints);
	}
	else
	{
		rii->fast_seek = (struct fast_seek_info *)
			rfs_kmalloc(sizeof(struct fast_seek_info), GFP_KERNEL,
					NORETRY);
		if (!rii->fast_seek)
		{
			DPRINTK("No memory for fast seek\n");
			kfree(fpoints);
			return -ENOMEM;
		}
	}

	rii->fast_seek->interval = interval;
	rii->fast_seek->interval_bits = interval_bits;
	rii->fast_seek->interval_mask = interval - 1;
	rii->fast_seek->fpoints = fpoints;
	rii->fast_seek->num_fpoints = num_fpoints;

	DEBUG(DL2, "allocate %u:%p fast seek\n", num_fpoints, fpoints);
	return 0;
}

static int rfs_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
		unsigned long arg)
{
	unsigned int interval = 0;
	int ret = 0;

	switch (cmd) {
	case RFS_FAST_SEEK:
		DEBUG(DL2, "build fseek");
		if ((ret = get_user(interval, (unsigned int *)arg)) != 0) {
			DPRINTK("Error(%d) in getting value from user\n", ret);
			break;
		}

		if (interval < 2)
		{
			DEBUG(DL1, "Interval is less than 2. Change it to default %u",
					RFS_FSEEK_DEFAULT_INTERVAL);
			interval = RFS_FSEEK_DEFAULT_INTERVAL;
		}

		ret = rfs_build_fseek(inode, interval);
		break;
	default:
		DEBUG(DL0, "unsupported cmd : %u", cmd);
		break;
	}

	return ret;
}
#else 	/* CONFIG_RFS_FAST_SEEK */

#define rfs_ioctl		NULL
#endif

#ifdef CONFIG_RFS_FAST_LOOKUP
/**
 * check flags of a file
 * @inode	inode of file object to open
 * @file	file object to open
 *
 * If a file is open with the fast open option, the flag must be O_RDONLY.
 */
static int rfs_file_open(struct inode * inode, struct file * filp)
{
	if (RFS_I(inode)->fast && ((filp->f_flags & O_ACCMODE) != O_RDONLY))
		return -EPERM;
	return 0;
}
#else	/* CONFIG_RFS_FAST_LOOKUP */

#define rfs_file_open		NULL
#endif

struct file_operations rfs_file_operations = {
#ifdef RFS_FOR_2_6_19
	.read		= do_sync_read,
	.write		= rfs_file_write,
	.aio_read	= generic_file_aio_read,
	.aio_write	= generic_file_aio_write,
#else
	.read		= generic_file_read,
	.write		= rfs_file_write,
#endif
	.mmap		= generic_file_mmap,
	.fsync		= rfs_file_fsync,
#ifdef RFS_FOR_2_6_18
	.splice_read	= generic_file_splice_read,
	.splice_write	= generic_file_splice_write,
#endif
	.release	= rfs_file_release,
	.ioctl		= rfs_ioctl,
	.open		= rfs_file_open,
	.flush		= rfs_file_flush,

#ifdef RFS_FOR_2_6_23
	.splice_read	= generic_file_splice_read,
#elif RFS_FOR_2_6
	.sendfile	= generic_file_sendfile,
#endif
};

struct inode_operations rfs_file_inode_operations = {
	.truncate	= rfs_truncate,
	.permission	= rfs_permission,
	.setattr	= rfs_setattr,
};

struct inode_operations rfs_file_inode_operations_xattr = {
	.truncate	= rfs_truncate,
	.permission	= rfs_permission,
	.setattr	= rfs_setattr,
#ifdef CONFIG_RFS_FS_XATTR
	.setxattr	= generic_setxattr,
	.getxattr	= generic_getxattr,
	.listxattr	= rfs_xattr_list,
	.removexattr	= generic_removexattr,
#endif
};

struct inode_operations rfs_symlink_inode_operations_xattr = {
	.readlink	= generic_readlink,
	.follow_link	= page_follow_link_light,
	.put_link	= page_put_link,
#ifdef CONFIG_RFS_FS_XATTR
	.setxattr       = generic_setxattr,
	.getxattr       = generic_getxattr,
	.listxattr      = rfs_xattr_list,
	.removexattr    = generic_removexattr,
#endif
};

/* for special files; socket file */
struct inode_operations rfs_special_inode_operations = {
	.setattr	= rfs_setattr,
	.permission	= rfs_permission,
};

struct inode_operations rfs_special_inode_operations_xattr = {
	.setattr	= rfs_setattr,
	.permission	= rfs_permission,
#ifdef CONFIG_RFS_FS_XATTR
	.setxattr       = generic_setxattr,
	.getxattr       = generic_getxattr,
	.listxattr      = rfs_xattr_list,
	.removexattr    = generic_removexattr,
#endif
};

/**
 *  translate index into a logical block
 * @param inode		inode
 * @param iblock	index
 * @param bh_result	buffer head pointer
 * @param create	flag whether new block will be allocated
 * @return		returns 0 on success, errno on failure 
 *
 * if there aren't logical block, allocate new cluster and map it
 */
#ifdef RFS_FOR_2_6
int rfs_get_block(struct inode *inode, sector_t iblock, struct buffer_head *bh_result, int create)
#else
int rfs_get_block(struct inode *inode, long iblock, struct buffer_head *bh_result, int create)
#endif
{
	sector_t phys = 0;
	struct super_block *sb = inode->i_sb;
	unsigned int new_clu;
	int ret = 0;

#ifdef RFS_FOR_2_4
	lock_kernel();
#endif

	ret = rfs_bmap(inode, (long) iblock, &phys);
	if (!ret) 
	{
#ifdef RFS_FOR_2_6
		map_bh(bh_result, sb, phys);
#else		
		bh_result->b_dev = inode->i_dev;
		bh_result->b_blocknr = phys;
		bh_result->b_state |= (1UL << BH_Mapped);
#endif
		goto out;
	}

	ret = -EIO;
	if (!create)
		goto out;
#if 1
	/*
	 * bugfix from RFS_1.2.3p2 branch
	 * bugfix of aligned mmu_private:
	 * if the mmu_private is not aligned to blocksize,
	 * the number of valid mapped block should be calculated
	 * with aligned mmu_private.
	 */
	if (RFS_I(inode)->mmu_private & (sb->s_blocksize - 1))
	{
		unsigned long long aligned_mmu_private;
		aligned_mmu_private = (unsigned long long) 
			(RFS_I(inode)->mmu_private | (sb->s_blocksize -1));
		aligned_mmu_private++;

		if ((long) iblock != 
			(long) (aligned_mmu_private >> sb->s_blocksize_bits))
		{
			DEBUG(DL0, "[%02x:%02x] can not get abnormal block (%ld"
				", %llu->%llu)\n\t: inode(%lu): p_start# %u idx"
				"# %u start# %u block_nr$ %llu offset# %lld\n",
				MAJOR(sb->s_dev), MINOR(sb->s_dev),
				(long) iblock, aligned_mmu_private,
				(aligned_mmu_private >> sb->s_blocksize_bits),
				inode->i_ino, RFS_I(inode)->p_start_clu, 
				RFS_I(inode)->index, RFS_I(inode)->start_clu,
				(unsigned long long) RFS_I(inode)->block_nr,
				(unsigned long long) RFS_I(inode)->offset);
			goto out;
		}

	} else
#endif
	if ((long) iblock != (long) 
			(RFS_I(inode)->mmu_private >> sb->s_blocksize_bits))
	{
		DEBUG(DL0, "[%02x:%02x] can not get abnormal block (%ld, %llu->"
			"%llu)\n\t: inode(%lu): p_start# %u idx# %u start# %u"
			" block_nr$ %llu offset# %lld\n",
			MAJOR(sb->s_dev), MINOR(sb->s_dev),
			(long) iblock, 
			(unsigned long long) RFS_I(inode)->mmu_private,
			(unsigned long long) (RFS_I(inode)->mmu_private >> 
				sb->s_blocksize_bits),
			inode->i_ino,
			RFS_I(inode)->p_start_clu, RFS_I(inode)->index,
			RFS_I(inode)->start_clu, 
			(unsigned long long) RFS_I(inode)->block_nr,
			(unsigned long long) RFS_I(inode)->offset);
		goto out;
	}

	if (!(iblock & (RFS_SB(sb)->blks_per_clu - 1))) 
	{
		ret = rfs_alloc_cluster(inode, &new_clu);
		if (ret)
			goto out;
	}

	RFS_I(inode)->mmu_private += sb->s_blocksize;
#ifdef RFS_FOR_2_6_24
	RFS_I(inode)->trunc_start = RFS_I(inode)->mmu_private;
#endif
	ret = rfs_bmap(inode, iblock, &phys);
	if (ret) {
		RFS_I(inode)->mmu_private -= sb->s_blocksize;
		DPRINTK("inode(%lu): p_start# %u idx# %u start# %u block_nr$ "
			"%llu offset# %lld\n",
			inode->i_ino, RFS_I(inode)->p_start_clu, 
			RFS_I(inode)->index, RFS_I(inode)->start_clu,
			(unsigned long long) RFS_I(inode)->block_nr,
			(unsigned long long) RFS_I(inode)->offset);
		dump_inode(inode);
		RFS_BUG_CRASH(sb, "iblock(%ld) doesn't have a physical mapping",
				iblock);
		goto out;
	}

#ifdef RFS_FOR_2_6
	set_buffer_new(bh_result);
	map_bh(bh_result, sb, phys);
#else		
	bh_result->b_dev = inode->i_dev;
	bh_result->b_blocknr = phys;
	bh_result->b_state |= (1UL << BH_Mapped);
	bh_result->b_state |= (1UL << BH_New);
#endif

out:
#ifdef RFS_FOR_2_4
	unlock_kernel();
#endif
	return ret;
}

/**
 *  translation index into logical block number
 * @param inode		inode	
 * @param index		index number	
 * @param[out] phys	logical block number
 * @return	returns 0 on success, errno on failure	
 * @pre		FAT16 root directory's inode does not invoke this function	
 */
int rfs_bmap(struct inode *inode, long index, sector_t *phys)
{
	struct super_block *sb = inode->i_sb;
	struct rfs_sb_info *sbi = RFS_SB(sb);
	unsigned int cluster, offset, num_clusters;
	blkcnt_t last_block;
	unsigned int clu, prev, next; 
	int err = 0;

	fat_lock(sb);

	cluster = (unsigned int) (index >> sbi->blks_per_clu_bits);
	offset = (unsigned int) (index & (sbi->blks_per_clu - 1));

	/* check hint info */
	num_clusters = cluster;
	err = __rfs_lookup_hint(inode, &num_clusters, &clu);
	if (err)
		goto out;

	last_block = (blkcnt_t) ((RFS_I(inode)->mmu_private +
				(sb->s_blocksize - 1)) >> sb->s_blocksize_bits);
	if ((blkcnt_t) index >= last_block) {
		err = -EFAULT;
		goto out;
	}

	err = rfs_find_cluster(sb, clu, num_clusters, &prev, &next);
	if (err)
	{
		DPRINTK("[%02x:%02x] can't find cluster %d (from %u, hop %u) "
				"of inode (%lu)\n",
				MAJOR(sb->s_dev), MINOR(sb->s_dev), 
				err, clu, num_clusters, inode->i_ino);
		dump_inode(inode);	
		goto out;
	}

	/* update hint info */
	__rfs_update_hint(inode, prev, cluster);

	*phys = START_BLOCK(prev, sb) + offset;
out:
	fat_unlock(sb);

	return err;
}
