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
 *  @file	fs/rfs/inode_26.c
 *  @brief	address_space_operations for Linux2.6
 *
 *
 */


#include <linux/fs.h>
#include <linux/rfs_fs.h>
#include <linux/smp_lock.h>

#include <linux/mpage.h>
#include <linux/backing-dev.h>
#include <linux/writeback.h>
#include <linux/uio.h>

#include "rfs.h"
#include "log.h"

#ifdef RFS_FOR_2_6

/**
 *  read a specified page
 * @param file		file to read
 * @param page		page to read
 * @return		return 0 on success
 */
static int rfs_readpage(struct file *file, struct page *page)
{
	return mpage_readpage(page, rfs_get_block);
}

/**
 *  write a specified page
 * @param page	to write page
 * @param wbc	writeback control	
 * @return	return 0 on success, errno on failure
 */
static int rfs_writepage(struct page *page, struct writeback_control *wbc)
{
	return block_write_full_page(page, rfs_get_block, wbc);
}

/**
 *  read multiple pages
 * @param file		file to read
 * @param mapping	address space to read
 * @param pages		page list to read	
 * @param nr_pages	number of pages 
 * @return		return 0 on success, errno on failure
 */
static int rfs_readpages(struct file *file, struct address_space *mapping,
		struct list_head *pages, unsigned nr_pages)
{
	return mpage_readpages(mapping, pages, nr_pages, rfs_get_block);
}

/**
 *  write multiple pages
 * @param mapping	address space to write
 * @param wbc		writeback_control	
 * @return		return 0 on success, errno on failure
 */
static int rfs_writepages(struct address_space *mapping, struct writeback_control *wbc)
{
	return mpage_writepages(mapping, wbc, rfs_get_block);
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
#ifdef RFS_FOR_2_6_24
static int rfs_write_begin(struct file *file, struct address_space *mapping,
		loff_t pos, unsigned len, unsigned flags,
		struct page **pagep, void **fsdata)
{
	struct inode *inode = mapping->host;
	int ret = 0;

	if (rfs_log_start(inode->i_sb, RFS_LOG_WRITE, inode))
		return -EIO;

	RFS_I(inode)->trunc_start = RFS_I(inode)->mmu_private;

	*pagep = NULL;
	ret = cont_write_begin(file, mapping, pos, len, flags, pagep, fsdata,
			rfs_get_block,
			&(RFS_I(inode)->mmu_private));

	if (ret)
		rfs_log_end(inode->i_sb, ret);

	return ret;
}
#else
static int rfs_prepare_write(struct file *file, struct page *page, unsigned from, unsigned to)
{
	struct inode *inode = page->mapping->host;
	int ret = 0;
		
	if (rfs_log_start(inode->i_sb, RFS_LOG_WRITE, inode))
		return -EIO;

	ret = cont_prepare_write(page, from, to, rfs_get_block,
			&(RFS_I(inode)->mmu_private));

	if (ret) {
		RFS_I(inode)->trunc_start = RFS_I(inode)->mmu_private;
		rfs_log_end(inode->i_sb, ret);
	}

	return ret;
}
#endif	/* RFS_FOR_2_6_24 */


/**
 *  write a specified page
 * @param file		to write file
 * @param page		page descriptor
 * @param from		start position in page		
 * @param to		end position in page	
 * @return		return 0 on success, errno on failure
 */
#ifdef RFS_FOR_2_6_24
static int rfs_write_end(struct file *file, struct address_space *mapping,
		loff_t pos, unsigned len, unsigned copied,
		struct page *pagep, void *fsdata)
{
	struct inode *inode = mapping->host;
	int ret;

	ret = generic_write_end(file, mapping, pos, len, copied, pagep, fsdata);

	if (rfs_log_end(inode->i_sb, ((ret < 0)? -1: 0)))
		return -EIO;

	return ret;
}
#else
static int rfs_commit_write(struct file *file, struct page *page, unsigned from, unsigned to)
{
	struct inode *inode = page->mapping->host;
	int ret;

	ret = generic_commit_write(file, page, from, to);
	if (rfs_log_end(inode->i_sb, ret))
		return -EIO;

	return ret;
}
#endif /* RFS_FOR_2_6_24 */

#ifdef RFS_FOR_2_6_17
/*
 * In linux 2.6.17 or more, the callback function in direct io is changed.
 * Now, it is used to single get block instead of multiple get blocks.
 */
#define rfs_get_blocks		rfs_get_block

#else	/* !RFS_FOR_2_6_17 */
/**
 *  Function to translate a logical block into physical block
 *  @param inode	inode
 *  @param iblock	logical block number
 *  @param max_blocks	dummy variable new
 *  @param bh_result	buffer head pointer
 *  @param create	control flag
 *  @return		zero on success, negative value on failure
 *
 *  This function is only invoked by direct IO
 */
static int rfs_get_blocks(struct inode *inode, sector_t iblock, unsigned long max_blocks, struct buffer_head *bh_result, int create)
{
	int ret;

	ret = rfs_get_block(inode, iblock, bh_result, create);
	if (!ret)
		bh_result->b_size = (1 << inode->i_blkbits);
	return ret;
}
#endif	/* RFS_FOR_2_6_17 */

#ifndef RFS_FOR_2_6_18
/*
 * In linux 2.6.18 or more, struct writeback_control file is changed
 * from start, end to range_start, range_end
 */
#define range_start		start
#define range_end		end
#endif	/* !RFS_FOR_2_6_18 */

/**
 *  Write and wait upon the last page for inode
 * @param inode	inode pointer
 * @return	zero on success, negative value on failure
 *
 * This is a data integrity operation for a combination of 
 * zerofill and direct IO write
 */
static int __sync_last_page(struct inode *inode)
{
	loff_t lstart = (i_size_read(inode) - 1) & PAGE_CACHE_MASK;
	struct address_space *mapping = inode->i_mapping;
	struct writeback_control wbc = {
		.sync_mode	= WB_SYNC_ALL,
		.range_start	= lstart,
		.range_end	= lstart + PAGE_CACHE_SIZE - 1,
	};

	/*
	 * Note: There's race condition. We don't use page cache operation 
	 * directly.
	 */
	return mapping->a_ops->writepages(mapping, &wbc);
}

#define rfs_flush_cache(iov, nr_segs)	do { } while (0)

/**
 *  RFS function excuting direct I/O operation
 *  @param rw	I/O command 
 *  @param iocb	VFS kiocb pointer 
 *  @param iov	VFS iovc pointer
 *  @param offset	I/O offset
 *  @param nr_segs	the number segments
 *  @return 	written or read date size on sucess, negative value on failure
 */
static ssize_t rfs_direct_IO(int rw, struct kiocb * iocb,
		const struct iovec *iov, loff_t offset,
		unsigned long nr_segs)
{
	struct inode *inode = iocb->ki_filp->f_mapping->host;
	struct super_block *sb = inode->i_sb;
	int ret = 0;

#ifdef CONFIG_GCOV_PROFILE
/*
 * Note: We *MUST* use the correct API in direct IO.
 *	 It is correct place when use gcov
 */
#define	loff_t			off_t
#endif
	
	if (rw == WRITE) {
		unsigned int clu_size, clu_bits;
		unsigned int alloc_clus, req_clus, free_clus;
		size_t write_len = iov_length(iov, nr_segs);
		loff_t i_size = i_size_read(inode);

		clu_size = RFS_SB(sb)->cluster_size;
		clu_bits = RFS_SB(sb)->cluster_bits;

		/* compare the number of required clusters with free clusters */
		alloc_clus = (i_size + clu_size - 1) >> clu_bits;
		req_clus = (offset + write_len + clu_size - 1) >> clu_bits;
		if (req_clus > alloc_clus)
			req_clus -= alloc_clus;
		else
			req_clus = 0;

		/*
		 * bugfix from RFS_1.2.3p2 branch
		 * check whether offset is aligned to blocksize
		 */
		if (offset & (inode->i_sb->s_blocksize - 1))
			return -EINVAL;

		if (rfs_log_start(inode->i_sb, RFS_LOG_WRITE, inode))
			return -EIO;

		free_clus = GET_FREE_CLUS(sb);
		if (req_clus > free_clus) {
			DEBUG(DL1, "req_clus = %d free_clus = %d \n",
					req_clus, free_clus);
			ret = -ENOSPC;
			goto end_log;
		}

		/*
		 * lseek case in direct IO
		 * Note: We have to cast 'offset' as loff_t
		 *	 to correct operation in kernel gcov
		 *	 the loff_t means off_t in gcov mode
		 */
		if ((loff_t) offset > i_size) {
			/*
			 * NOTE: In spite of direc IO,
			 * we use page cache for rfs_extend_with_zerofill
			 */
			ret = rfs_extend_with_zerofill(inode, 
					(u32) i_size,
					(u32) offset);
			if (ret)
			{
				DEBUG(DL0, "fail to extend zerofill (%u to %u)",
						(u32) i_size, (u32) offset);
				goto end_log;
			}

			i_size_write(inode, offset);
			set_mmu_private(inode, offset);

			ret = __sync_last_page(inode);
			if (ret)
			{
				/*
				 * it is possible that real allocated clusters
				 * and size in dir entry can be different
				 */
				DEBUG(DL0, "fail to sync last page(%d)", ret);
				goto end_log;
			}
		}
	}

	/* flush cache */
	rfs_flush_cache(iov, nr_segs);

	ret = blockdev_direct_IO(rw, iocb, inode, inode->i_sb->s_bdev, iov,
			offset, nr_segs, rfs_get_blocks, NULL);

	if (rw == WRITE) {
end_log:
		if (rfs_log_end(inode->i_sb, (ret >= 0) ? 0 : -EIO))
			return -EIO;
	}

	return ret;
}

struct address_space_operations rfs_aops = {
	.readpage	= rfs_readpage,
	.writepage	= rfs_writepage,
	.readpages	= rfs_readpages,
	.writepages	= rfs_writepages,
	.sync_page	= block_sync_page,
#ifdef RFS_FOR_2_6_24
	.write_begin	= rfs_write_begin,
	.write_end	= rfs_write_end,
#else
	.prepare_write	= rfs_prepare_write,
	.commit_write	= rfs_commit_write,
#endif
	.direct_IO	= rfs_direct_IO,
};
#endif	/* RFS_FOR_2_6 */
