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
 *  @file	fs/rfs/inode_24.c
 *  @brief	common inode operations
 *
 *
 */


#include <linux/fs.h>
#include <linux/rfs_fs.h>
#include <linux/smp_lock.h>
#include <linux/iobuf.h>

#include "rfs.h"
#include "log.h"

/**
 *  read a specified page
 * @param file		file to read
 * @param page		page to read
 * @return		return 0 on success
 */
static int rfs_readpage(struct file *file, struct page *page)
{
	return block_read_full_page(page, rfs_get_block);
}

/**
 *  write a specified page
 * @param page	to write page
 * @param wbc	writeback control	
 * @return	return 0 on success, errno on failure
 */
static int rfs_writepage(struct page *page)
{
	struct inode *inode = page->mapping->host;
	int ret;

	down(&RFS_I(inode)->data_mutex);
	ret = block_write_full_page(page, rfs_get_block);
	up(&RFS_I(inode)->data_mutex);

	return ret;
}


/**
 *  prepare the blocks and map them 
 * @param inode	inode 	 
 * @param page	page pointer
 * @param from	start offset within page 	
 * @param to 	last offset within page	 
 * @param get_block 	get_block funciton	 
 * @return	return 0 on success, errno on failure
 */
static int __rfs_block_prepare_write(struct inode * inode, struct page * page, unsigned from, unsigned to, get_block_t *get_block) 
{
	struct buffer_head *bh, *head;
	unsigned long block;
	unsigned block_start, block_end, blocksize, bbits; 
	int err = 0;
	char *kaddr = kmap(page);

	bbits = inode->i_blkbits;
	blocksize = (unsigned) 1 << bbits;

	if (!page->buffers) 
		create_empty_buffers(page, inode->i_dev, blocksize);
	head = page->buffers;

	block = page->index << (PAGE_CACHE_SHIFT - bbits); /* start block # */

	/* we allocate buffers and map them */
	for (bh = head, block_start = 0; (bh != head) || (!block_start);
		block++,
		block_start = block_end + 1,
		bh = bh->b_this_page) {

		if (!bh) {
			err = -EIO;
			RFS_BUG_CRASH(inode->i_sb, "can't get buffer head\n");
			goto out;
		}

		block_end = block_start + blocksize - 1;
		if (block_end < from) {
			continue;
		} else if (block_start > to) { 
			break;
		}
		clear_bit(BH_New, &bh->b_state);

		/* map new buffer if necessary*/	
		if ((!buffer_mapped(bh)) || 
			((unsigned long) inode->i_size <= 
				(block<<(inode->i_sb->s_blocksize_bits)))) {
 			err = get_block(inode, (long) block, bh, 1);
 			if (err) {
 				DEBUG(DL1, "no block\n");	
 				goto out;
 			}
			if (buffer_new(bh) && (block_end > to)) {
				memset(kaddr+to+1, 0, block_end-to);
				continue;
			}
		}			
		if (!buffer_uptodate(bh) && 
			((block_start < from) || (block_end > to))) 
		{
			ll_rw_block(READ, 1, &bh);
			wait_on_buffer(bh);
			if (!buffer_uptodate(bh)) 
			{
				err = -EIO;
				RFS_BUG_CRASH(inode->i_sb, "Fail to Read");
				goto out;
			}
		}
	}
out:
	flush_dcache_page(page);	
	kunmap_atomic(kaddr, KM_USER0);
	return err;	
}

/**
 *  block commit write 
 * @param inode	inode 	 
 * @param page	page pointer
 * @param from	start offset within page 	
 * @param to 	last offset within page	 
 * @return	return 0 on success, errno on failure
 */
static int __rfs_block_commit_write(struct inode *inode, struct page *page,
		unsigned from, unsigned to)
{
	unsigned block_start, block_end;
	unsigned blocksize;
	struct buffer_head *bh, *head;

	blocksize = (unsigned) 1 << inode->i_blkbits;

	for(bh = head = page->buffers, block_start = 0;
		(bh != head) || (!block_start);
		block_start=block_end + 1, bh = bh->b_this_page) 
	{

		block_end = block_start + blocksize - 1;
		if (block_end < from)
			continue;
		else if (block_start > to) 
			break;
		else 
		{
			set_bit(BH_Uptodate, &bh->b_state);
			__mark_buffer_dirty(bh);
			down(&RFS_I(inode)->data_mutex);
			buffer_insert_inode_data_queue(bh, inode);
			up(&RFS_I(inode)->data_mutex);
		}
	}

	return 0;
}

/**
 *  read some partial page to write rest page
 * @param file		to read file
 * @param page		specified page to read
 * @param from		start position in page
 * @param to		bytes counts to prepare in page 
 * @return		return 0 on success, errno on failure
 *
 * This function requires addtional code saving inode->i_size because there is
 * case when inode->i_size is chagned after cont_prepare_write.  
 */
static int rfs_prepare_write(struct file *file, struct page *page, unsigned from, unsigned to)
{
	struct inode *inode = page->mapping->host;
	unsigned page_start_offset = page->index << PAGE_CACHE_SHIFT;
	unsigned mmu_private = RFS_I(inode)->mmu_private;
	unsigned mmu_private_in_page = mmu_private & (PAGE_CACHE_SIZE - 1);
	unsigned newfrom;
	char *kaddr;
	int ret = 0;
		
	if (rfs_log_start(inode->i_sb, RFS_LOG_WRITE, inode))
		return -EIO;

	if ((page_start_offset + from) > mmu_private) {
		if (page_start_offset > mmu_private)
			newfrom = 0;
		else
			newfrom = mmu_private_in_page;
	} else 
			newfrom = from;

	if (page_start_offset > mmu_private) {
		/* zerofill the previous hole pages */
		ret = rfs_extend_with_zerofill(inode, mmu_private,
				page_start_offset);
		if (ret)
			goto out;
	}

	ret = __rfs_block_prepare_write(inode, page, newfrom,
			(to - 1), rfs_get_block);
	if (ret)
		goto out;

	if (from > newfrom) {
		/* memset & commit the previous hole in page */
		kaddr = page_address(page);
		memset(kaddr+newfrom, 0, from-newfrom);
		flush_dcache_page(page);
		__rfs_block_commit_write(inode, page, newfrom, from);
		kunmap(page);
	} 

	return ret;

out:
	RFS_I(inode)->trunc_start = RFS_I(inode)->mmu_private;
	rfs_log_end(inode->i_sb, ret);

	return ret;
}

/**
 *  write a specified page
 * @param file		to write file
 * @param page		page descriptor
 * @param from		start position in page		
 * @param to		end position in page	
 * @return		return 0 on success, errno on failure
 */
static int rfs_commit_write(struct file *file, struct page *page, unsigned from, unsigned to)
{
	struct inode *inode = page->mapping->host;
	int ret;

	down(&RFS_I(inode)->data_mutex);
	ret = generic_commit_write(file, page, from, to);
	up(&RFS_I(inode)->data_mutex);

	if (rfs_log_end(inode->i_sb, ret))
		return -EIO;

	return ret;
}

/**
 *  rfs_direct_IO - directly read/write from/to user space buffers without cache
 * @param rw		read/write type
 * @param inode		inode
 * @param iobuf		iobuf
 * @param blocknr	number of blocks
 * @param blocksize	block size
 * @return		write/read size on success, errno on failure
 */
static int rfs_direct_IO(int rw, struct inode *inode, struct kiobuf *iobuf, unsigned long blocknr, int blocksize)
{
	struct super_block *sb = inode->i_sb;
	loff_t offset, old_size = inode->i_size;
	unsigned int alloc_clus = 0;
	int zerofilled = FALSE, ret;

	offset = blocknr << sb->s_blocksize_bits;

	if (rw == WRITE) {
		unsigned int clu_size, clu_bits;
		unsigned int req_clus, free_clus;

		clu_size = RFS_SB(sb)->cluster_size;
		clu_bits = RFS_SB(sb)->cluster_bits;

		/* compare the number of required clusters with free clusters */
		alloc_clus = (unsigned int) (inode->i_size + clu_size - 1) 
				>> clu_bits;
		req_clus = (unsigned int) 
			(offset + iobuf->length + clu_size - 1) >> clu_bits;
		if (req_clus > alloc_clus)
			req_clus -= alloc_clus; /* required clusters */
		else 
			req_clus = 0;

		if (rfs_log_start(inode->i_sb, RFS_LOG_WRITE, inode))
			return -EIO;

		free_clus = GET_FREE_CLUS(sb);
		if (req_clus > free_clus) {
			DEBUG(DL1, "req_clus = %d free_clus = %d \n", 
					req_clus, free_clus);
			ret = -ENOSPC;
			goto end_log;
		}

		/* lseek case in direct IO */
		if (offset > old_size) {
			/* 
			 * NOTE: In spite of direc IO, 
			 * we use page cache for extend_with_zeorfill 
			 */
			ret = rfs_extend_with_zerofill(inode, 
					(u32) old_size, (u32) offset);
			if (ret)
				goto end_log;

			inode->i_size = offset;
			set_mmu_private(inode, offset);
			zerofilled = TRUE;
		}
	}

	ret = generic_direct_IO(rw, inode, iobuf, 
			blocknr, blocksize, rfs_get_block);

	if (rw == WRITE) {
		if (ret == -EINVAL) { 
			int err;

			/* 
			 * free some clusters if only unaligned offset & length 
			 * of iobuf exists which were allocated at direct IO op
			 */
			err = rfs_dealloc_clusters(inode, alloc_clus);
			if (!err) {
				inode->i_size = old_size;
				set_mmu_private(inode, old_size);
			}

			/* invalidate hint info */
			rfs_invalidate_hint(inode);
		} 

end_log:
		if (rfs_log_end(inode->i_sb, (ret >= 0) ? 0 : -EIO))
			return -EIO;
		
		if ((!ret) && zerofilled)
			ret = fsync_inode_data_buffers(inode);
	}

	return ret;
}

struct address_space_operations rfs_aops = {
	.readpage	= rfs_readpage,
	.writepage	= rfs_writepage,
	.sync_page	= block_sync_page,
	.prepare_write	= rfs_prepare_write,
	.commit_write	= rfs_commit_write,
	.direct_IO	= rfs_direct_IO,
};

