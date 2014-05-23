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
 *  @version    RFS_1.3.1_b072_RTM
 *  @file       fs/rfs/xattr.c
 *  @brief      extended attributes
 */

#include <linux/buffer_head.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/mbcache.h>
#include <linux/quotaops.h>
#include <linux/rwsem.h>

#include <linux/rfs_fs.h>
#include <linux/limits.h>
#include <linux/xattr.h>
#ifdef CONFIG_RFS_FS_POSIX_ACL
#include <linux/posix_acl_xattr.h>
#endif
#include "rfs.h"
#include "log.h"
#include "xattr.h"

/* handlers for xattr */
struct xattr_handler *rfs_xattr_handlers[] = 
{
	&rfs_xattr_user_handler,
	&rfs_xattr_trusted_handler,
#ifdef CONFIG_RFS_FS_POSIX_ACL
	&rfs_xattr_acl_access_handler,
	&rfs_xattr_acl_default_handler,
#endif
#ifdef CONFIG_RFS_FS_SECURITY
	&rfs_xattr_security_handler,
#endif
	NULL
};

struct XATTR_NAME_SPACE
{
	XATTR_NS	nsid;
	unsigned char	length;
	char		*prefix;
};

#ifdef CONFIG_RFS_FS_POSIX_ACL
#define POSIX_ACL_ACCESS_LEN	(sizeof(POSIX_ACL_XATTR_ACCESS) - 1)
#define POSIX_ACL_DEFAULT_LEN	(sizeof(POSIX_ACL_XATTR_DEFAULT) - 1)
#endif

/* table for name_spcae */
static const struct XATTR_NAME_SPACE _ns_table[] =
{
	{ RFS_XATTR_NS_USER, 
	  XATTR_USER_PREFIX_LEN, 
	  XATTR_USER_PREFIX		},
#ifdef CONFIG_RFS_FS_POSIX_ACL
	{ RFS_XATTR_NS_POSIX_ACL_ACCESS, 
	  POSIX_ACL_ACCESS_LEN, 
	  POSIX_ACL_XATTR_ACCESS	},
	{ RFS_XATTR_NS_POSIX_ACL_DEFAULT, 
	  POSIX_ACL_DEFAULT_LEN, 
	  POSIX_ACL_XATTR_DEFAULT	},
#else
	{RFS_XATTR_NS_POSIX_ACL_ACCESS, 0, 0},
	{RFS_XATTR_NS_POSIX_ACL_DEFAULT, 0, 0},
#endif
	{ RFS_XATTR_NS_TRUSTED,
	  XATTR_TRUSTED_PREFIX_LEN, 
	  XATTR_TRUSTED_PREFIX		},
	{ RFS_XATTR_NS_SECURITY, 
	  XATTR_SECURITY_PREFIX_LEN, 
	  XATTR_SECURITY_PREFIX		}
};

/**
 *  get extended attribute name size
 * @param inode				inode
 * @param[in] param->name		name string
 * @param[out] param->name_length	name length
 * @return      returns 0 on success, errno on failure
 */
static int
__xattr_get_name_length(struct rfs_xattr_param *param)
{
	unsigned int length;

	/* strnlen? */
	length = strnlen(param->name, RFS_XATTR_NAME_LENGTH_MAX);

	if (RFS_XATTR_NAME_LENGTH_MAX < length)
	{
		DEBUG(DL0, "attibute name is too long");
		return -EINVAL;
	}

	/* 
	 *  No need to check  name is null. already checked
	 *  in handler like 'rfs_xattr_user_set()'.
	 */

	param->name_length  = length;
	return 0;
}

/**
 *  translation index into logical block number
 * @param inode         inode
 * @param index         blcok index number
 * @param[out] phys     logical block number
 * @return      returns 0 on success, errno on failure
 * @pre         FAT16 root directory's inode does not invoke this function
 */
static int
__xattr_bmap(struct inode *inode, long index, sector_t *phys)
{
	struct super_block *sb = inode->i_sb;
	struct rfs_sb_info *sbi = RFS_SB(sb);
	unsigned int cluster, offset;
	unsigned int start, prev, next;
	int err = 0;

	/* check xattr_start */
	if ( ! IS_XATTR_EXIST(RFS_I(inode)))
	{
		DEBUG(DL0, "inode does not have xattr cluster");
		return -EFAULT;
	}

	start = RFS_I(inode)->xattr_start_clus;
	cluster = (unsigned int) (index >> sbi->blks_per_clu_bits);
	offset = (unsigned int) (index & (sbi->blks_per_clu - 1));

	DEBUG(DL3, "clu:%u,num_clusters:%u,offset:%u", start, cluster, offset);

	if (RFS_I(inode)->xattr_numof_clus <= cluster)
	{
		DEBUG(DL0, "index exceeds allocated cluster");
		return -EFAULT;
	}

	fat_lock(sb);

	err = rfs_find_cluster(sb, start, cluster, &prev, &next);
	if (err)
	{
		DEBUG(DL0, "can't find cluster %d (from %u, hop %u)", err,
				start, cluster);
		goto out; /* need to fat_unlock() */
	}
	*phys = START_BLOCK(prev, sb) + offset;
out:
	fat_unlock(sb);
	 return err;
}

/**
 *  allocate a new cluster from fat table to xattr
 * @param inode			inode
 * @param [out] new_clus	new clsuter number to be allocated
 * @return	return 0 on success, errno on failure
 */
static int 
__xattr_alloc_cluster(struct inode *inode, unsigned int *new_clus)
{
	struct super_block *sb = inode->i_sb;
	unsigned int last_clus;
	int ret;

	if (RFS_I(inode)->xattr_start_clus < VALID_CLU) /*TODO is valid macro*/
	{
		RFS_BUG("inode has invalid xattr start cluster(%u)",
				RFS_I(inode)->xattr_start_clus);
		return -EINVAL;
	}

	fat_lock(sb);

	last_clus = RFS_I(inode)->xattr_last_clus;
	DEBUG(DL3 ,"start:%u,last:%u", RFS_I(inode)->xattr_start_clus, 
			RFS_I(inode)->xattr_last_clus);

	/* Phase 1 : get one free cluster */
	RFS_BUG_ON(RFS_LOG_I(sb)->numof_pre_alloc > 0);

#ifndef _RFS_INTERNAL_LOG_DEBUG
	ret = rfs_get_cluster(inode, new_clus, last_clus, TRUE);
#else
	ret = rfs_get_cluster(inode, RFS_I(inode)->xattr_start_clus, new_clus, 
			last_clus, TRUE);
#endif
	if (ret)
	{
		DEBUG(DL0, "cannot get cluster(%d)", ret);
		goto out;
	}
	DEBUG(DL2, "get a new cluster(%u,0x%x)", *new_clus, *new_clus);

	/* Phase 2 : append free cluster to end of fat chain */
	ret = rfs_fat_write(sb, *new_clus, CLU_TAIL);
	if (ret)
	{
		DEBUG(DL0, "fail to write fat(%d)", ret);
		goto out;
	}
	if (last_clus == CLU_TAIL)
	{
		RFS_I(inode)->xattr_start_clus = *new_clus;
		DEBUG(DL3, "eoc mark(%u)\n", *new_clus);
	}
	else
	{
		ret = rfs_fat_write(sb, last_clus, *new_clus);
		if (ret)
		{
			DEBUG(DL0, "fail to write fat(%d)", ret);
			goto out;
		}
	}

	INC_USED_CLUS(sb, 1);

#ifdef RFS_CLUSTER_CHANGE_NOTIFY
	set_cluster_usage_notify(sb, TRUE);
#endif

	RFS_I(inode)->xattr_last_clus = *new_clus;
	DEBUG(DL3 ,"start:%u,last:%u", RFS_I(inode)->xattr_start_clus, 
			RFS_I(inode)->xattr_last_clus);

	/*TODO : consider opened unlink situation*/
out:
	fat_unlock(sb);
	return ret;
}

/**
 *  perform one block I/O opertion.
 * @param rw			mode 'READ' or 'WRITE'
 * @param inode			inode
 * @param block_i		block index
 * @param[in/out] data		source or destination buffer ptr
 * @param start_offset		start offset in a block
 * @param size			size in a block
 * @param cursor		cursor of data buffer	
 * @return	return 0 on success, errno on failure
 */
static int
__xattr_io_one_block(int rw, struct inode *inode, long block_i, char *data, 
	unsigned long start_offset, unsigned long size, unsigned int cursor)
{
	struct super_block *sb = inode->i_sb;
	struct buffer_head *bh = NULL;
	int ret;
	sector_t phys = 0;

	/* get logical block number */
	ret = __xattr_bmap(inode, block_i, &phys);
	if (ret)
	{
		RFS_BUG_CRASH(sb, "block(%ld) does not have a physical mapping",
			       	block_i);
		goto out;
	}

	/* read before READ/WRITE */
	bh = rfs_bread(sb, phys, BH_RFS_XATTR);
	if (bh == NULL)
	{
		DEBUG(DL0, "cannot get buffer head");
		return -EIO;
	}

	/* assert size range */
	RFS_BUG_ON((start_offset + size) > 
			(unsigned long) (1 << inode->i_blkbits));
	
	if (READ == rw)
	{
		memcpy((data + cursor), (bh->b_data + start_offset), size);
	}
	else /* if (WRITE == rw) */
	{
		memcpy((bh->b_data + start_offset), (data + cursor), size);
		rfs_mark_buffer_dirty(bh, sb);
	}
	brelse(bh);
out:
	return ret;
}
/**
 *  expand cluster if require
 * @param inode		inode
 * @param offset	offset to io (byte)
 * @param size		size to I/O
 * @return		return 0 on success, errno on failure
 * @pre			call only when rw=WRITE
 */
static int
__xattr_expand_xattr_clusters(struct inode *inode, unsigned long offset, 
		unsigned int size)
{
	struct super_block *sb = inode->i_sb;
	int ret = 0;
	unsigned int last_clus, required_clus, new_clus;

	last_clus = (offset + size + (RFS_SB(sb)->cluster_size - 1)) >>
		RFS_SB(sb)->cluster_bits;

	required_clus = (last_clus <= RFS_I(inode)->xattr_numof_clus)? 0 :
		last_clus - RFS_I(inode)->xattr_numof_clus;

	DEBUG(DL2, "xattr cluster will allocate(required_clus:%u)", 
			required_clus);

	if ((required_clus > 0) && 
			(required_clus > GET_FREE_CLUS(inode->i_sb)))
	{
		DEBUG(DL2, "No space for xclusters\n");
		ret = -ENOSPC;
		goto out;
	}

	/* iterate expand cluster one by one. it is unlikely to happen 
	 * for the clusters to be allocate at the same time. it is not 
	 * performance bottle neck.
	 */
	while (0 < required_clus)
	{
		/* allocate one cluster */
		ret = __xattr_alloc_cluster(inode, &new_clus);
		if (ret)
		{
			DEBUG(DL0, "cannot allocate cluster");
			goto out;
		}

		RFS_I(inode)->xattr_numof_clus++;
		inode->i_blocks += ((blkcnt_t) 
				1 << (RFS_SB(sb)->cluster_bits - SECTOR_BITS));
		required_clus--;
		DEBUG(DL3, "xattr_numof_clus:%u", 
				RFS_I(inode)->xattr_numof_clus);
	}
out:
	return ret;
}

/**
 *  perform read/write opertion in xattr clusters.
 * @param rw			mode 'READ' or 'WRITE'
 * @param inode			inode
 * @param offset		offset to io (byte)
 * @param[in/out] data		source or destination buffer ptr
 * @param size			size to I/O
 * @return	return 0 on success, errno on failure
 */
int
__xattr_io(int rw, struct inode *inode, unsigned long offset, char *data, unsigned int size)
{
	int ret = 0;
	unsigned long first_blk_index; /*index of first block will I/O */
	unsigned long last_blk_index; /*index of last block will I/O */
	unsigned long block_i; /* block index for loop */

	unsigned int offset_curr;
	unsigned long offset_fr_in_blk; /* offset in a block (offset) */
	unsigned long blk_end_offset; /* start offset of block_end (offset) */
	unsigned long offset_to_in_blk; /* end offset in a block (offset) */
	unsigned long size_in_blk; /* size in a block (size) */

	unsigned long data_written;
	unsigned blocksize, bbits;

	bbits = inode->i_blkbits;
	blocksize = (unsigned) (1 << bbits);

	RFS_BUG_ON(size <= 0);

	if (WRITE == rw)
	{
		/* if need more clusters, allocate clusters */
		ret = __xattr_expand_xattr_clusters(inode, offset, size);
		if (ret)
		{
			DEBUG(DL0, "fail to __xattr_expand_clusters(%d)", ret);
			goto out;
		}
	}

	/* perform block I/O */
	offset_curr = offset;
	first_blk_index = offset >> bbits;
	/* TODO comment (size - 1) : block_last mean block's offset  */
	last_blk_index = (offset + (size - 1)) >> bbits;

	data_written = 0;
	
	DEBUG(DL2, "[%s]offset:%lu,size:%u,blk_first:%lu,blk_last:%lu", 
			(rw==READ) ? "READ":"WRITE", offset, size, first_blk_index, 
			last_blk_index);

	for (block_i = first_blk_index; block_i <= last_blk_index; block_i++)
	{
		blk_end_offset = (block_i << bbits) + (blocksize - 1);
		offset_fr_in_blk = offset_curr & (blocksize - 1);
		offset_to_in_blk = ((offset + (size - 1)) > blk_end_offset) ?
			(blocksize - 1) /* whole block */ :
			(offset + (size - 1)) & (blocksize - 1)/*patial block*/;

		size_in_blk = offset_to_in_blk - offset_fr_in_blk + 1;

		DEBUG(DL2, "[i:%lu]fr:%lu,to:%lu,size:%lu,blk_end:%lu,"
				"offset_curr:%u", block_i, offset_fr_in_blk, 
				offset_to_in_blk, size_in_blk, blk_end_offset, 
				offset_curr);

		ret = __xattr_io_one_block(rw, inode, (long) block_i, data, 
				offset_fr_in_blk, size_in_blk, data_written);
		if (ret)
		{
			DEBUG(DL0, "fail to __xattr_io_one_block(%d)", ret);
			goto out;
		}
		
		data_written += size_in_blk;
		offset_curr = blk_end_offset + 1;
		DEBUG(DL3, "data_written:%lu,offset_curr:%u", data_written, 
				offset_curr);
	}
out:
	return ret;
}

/**
 *  alculate crc32
 * @param param
 * @param entry
 */
static __u16
__calc_crc16(struct rfs_xattr_param *param, struct rfs_xattr_entry *entry)
{
	/*TODO : need implementation*/
	/*TODO calc crc here */
	return 0xFFFE;
}

/**
 *  read entry
 * @param inode		inode
 * @param offset_curr	offset to read (byte)
 * @param entry		entry struct ptr to read
 * @return		return 0 on success, errno on failure
 *  endian should consider in this function.
 */
static int
__xattr_read_entry_head(struct inode *inode, unsigned int offset_curr, 
		struct rfs_xattr_entry *entry)
{
	int ret = 0;
	struct rfs_xattr_entry entry_tmp;

	/* read entry */
	ret = __xattr_io(READ, inode, offset_curr, (char*)&entry_tmp, 
				sizeof(struct rfs_xattr_entry));
	if (ret)
	{
		DEBUG(DL0, "fail to __xattr_io(%d)", ret);
		return ret;
	}

	/* check entry's signature */
	if (RFS_XATTR_ENTRY_SIGNATURE != GET16(entry_tmp.signature))
	{
		DEBUG(DL0, "abnormal entry signature(%u!=%u),offset:%u",
				RFS_XATTR_ENTRY_SIGNATURE, 
				GET16(entry_tmp.signature), 
				offset_curr);
		return -EIO;
	}

	/*TODO crc check */

	/* assign entry_tmp with endian considering */
	entry->type_flag 	= entry_tmp.type_flag;
	entry->ns_id		= entry_tmp.ns_id;
	entry->name_length	= GET16(entry_tmp.name_length);
	entry->value_length	= GET32(entry_tmp.value_length);
	entry->entry_length	= GET32(entry_tmp.entry_length);
	entry->crc16		= GET16(entry_tmp.crc16); 
	entry->signature	= GET16(entry_tmp.signature);

	return ret;
}

/**
 *  write entry
 * @param inode	inode
 * @param entry	entry struct ptr to write
 * @return	return 0 on success, errno on failure
 *  endian should consider in this function.
 */
static int
__xattr_write_entry(struct inode *inode, unsigned int offset_cur, 
		struct rfs_xattr_param *param, struct rfs_xattr_entry *entry)
{
	int ret = 0;
	unsigned int name_length = entry->name_length;
	struct rfs_xattr_entry entry_tmp;

	/* assert entry's signature */
	RFS_BUG_ON(entry->signature != RFS_XATTR_ENTRY_SIGNATURE);

	/* assign entry_tmp with endian considering */
	memset(&entry_tmp, 0x77, sizeof(struct rfs_xattr_entry));
	entry_tmp.type_flag = entry->type_flag;
	entry_tmp.ns_id = entry->ns_id;
	SET16(entry_tmp.name_length,	entry->name_length);
	SET32(entry_tmp.value_length,	entry->value_length);
	SET32(entry_tmp.entry_length,	entry->entry_length);
	SET16(entry_tmp.crc16,		__calc_crc16(param, entry)); 
	SET16(entry_tmp.signature,	entry->signature);

	/* write entry */
	DEBUG(DL1, "write ENTRY-header(offset:%u)", offset_cur);
	ret = __xattr_io(WRITE, inode, offset_cur, (char*)&entry_tmp, 
			sizeof(struct rfs_xattr_entry));
	if (ret)
	{
		DEBUG(DL0, "fail to write xattr entry (offset:%u)", offset_cur);
		return ret; /* direct return on IO fail */
	}
	offset_cur += sizeof(struct rfs_xattr_entry);

	/* write entry's name */
	DEBUG(DL2, "write ENTRY-name(offset:%u)", offset_cur);
	ret = __xattr_io(WRITE, inode, offset_cur, (char*)param->name, 
			name_length);
	if (ret)
	{
		DEBUG(DL0, "fail to write xattr entry's name (offset:%u)", 
				offset_cur);
		return ret; /* direct return on IO fail */
	}
	offset_cur += name_length;

	DEBUG(DL3, "[WRITE]offset:%u,value:0x%p,len:%u", 
			offset_cur, param->value, param->value_length);

	/* write entry's value */
	DEBUG(DL2, "write ENTRY-value(offset:%u)", offset_cur);
	ret = __xattr_io(WRITE, inode, offset_cur, (char*)param->value, 
			param->value_length);
	if (ret)
	{
		DEBUG(DL0, "fail to write xattr entry's value (offset:%u)",
				offset_cur);
		return ret; /* direct return on IO fail */
	}
	DEBUG(DL1, "write ENTRY-end");
	offset_cur += param->value_length;

	return ret;
}

/**
 *  write header
 * @param inode	inode
 * @return	return 0 on success, errno on failure
 *  endian should consider in this function.
 */
int
rfs_xattr_write_header(struct inode *inode)
{
	int ret = 0;
	struct rfs_xattr_header head_tmp;
	struct rfs_inode_info *rii = RFS_I(inode);

	/* assert header's signature */
	/*RFS_BUG_ON(head->signature != RFS_XATTR_HEAD_SIGNATURE);*/

	/* assign head_tmp with endian considering */
	memset(&head_tmp, 0x33, sizeof(struct rfs_xattr_header));
	SET16(head_tmp.signature,	RFS_XATTR_HEAD_SIGNATURE);
	SET16(head_tmp.ctime,		rii->xattr_ctime);
	SET16(head_tmp.cdate,		rii->xattr_cdate);
	SET16(head_tmp.mode,		rii->xattr_mode);
	SET32(head_tmp.uid,		rii->xattr_uid);
	SET32(head_tmp.gid,		rii->xattr_gid);
	SET16(head_tmp.valid_count,	rii->xattr_valid_count);
	SET32(head_tmp.total_space,	rii->xattr_total_space);
	SET32(head_tmp.used_space,	rii->xattr_used_space);
	SET32(head_tmp.numof_clus,	rii->xattr_numof_clus);
	SET32(head_tmp.pdir,		rii->p_start_clu);
	SET32(head_tmp.entry,		rii->index);

	/* write head */
	DEBUG(DL1, "write HEAD-start");
	ret = __xattr_io(WRITE, inode, 0, (char*)&head_tmp, 
			sizeof(struct rfs_xattr_header));
	if (ret)
	{
		DEBUG(DL0, "fail to write xattr head(%d)", ret);
	}
	DEBUG(DL1, "write HEAD-end");
	return ret;
}

/**
 *  read header to inode
 * @param inode		inode
 * @param[out] head	head struct ptr for output
 * @return	return 0 on success, errno on failure
 *  endian should consider in this function.
 */
int 
rfs_xattr_read_header_to_inode(struct inode *inode, int b_need_lock)
{
	int ret = 0;
	struct rfs_xattr_header head_tmp;
	struct rfs_inode_info *rii = RFS_I(inode);

	if (TRUE == b_need_lock)
	{
		/* aquire xattr_sem(read) */
		down_read(&RFS_I(inode)->xattr_sem);
	}

	/* read head */
	ret = __xattr_io(READ, inode, 0, (char*)&head_tmp, 
			sizeof(struct rfs_xattr_header));
	if (ret)
	{
		DEBUG(DL0, "fail to read xattr head(%d)", ret); 
		goto out;
	}

	/* check head's signature */
	if (unlikely(RFS_XATTR_HEAD_SIGNATURE != GET16(head_tmp.signature)))
	{
		DEBUG(DL0, "abnormal head  signature(%u!=%u)",
				RFS_XATTR_HEAD_SIGNATURE, 
				GET16(head_tmp.signature));
		ret = -EIO;
		goto out;
	}

	/* assign head from head_tmp with endian considering */
	rii->xattr_ctime	= GET16(head_tmp.ctime);
	rii->xattr_cdate	= GET16(head_tmp.cdate);
	rii->xattr_mode		= GET16(head_tmp.mode);
	rii->xattr_uid		= GET32(head_tmp.uid);
	rii->xattr_gid		= GET32(head_tmp.gid);
	rii->xattr_valid_count	= GET16(head_tmp.valid_count);
	rii->xattr_total_space	= GET32(head_tmp.total_space);
	rii->xattr_used_space	= GET32(head_tmp.used_space);

	/* TODO ~ */
	/*rii->xattr_numof_clus	= GET32(head_tmp.numof_clus);*/
out:
	if (TRUE == b_need_lock)
	{
		/* release xattr_sem(read) */
		up_read(&RFS_I(inode)->xattr_sem);
	}

	return ret;
}

/**
 *  compare extended attribute
 * @param inode		inode
 * @param param		parameter struct contain extended attribute info
 * @param entry		entry to compare
 * @param offset	entry's offset
 * @param name_buff	name buffer (caller should provide buff)
 * @return		return TRUE on entry match, FALSE on mismatch,
 *			errno on failure
 */
static int
__xattr_compare_entry(struct inode *inode, struct rfs_xattr_param *param,
		struct rfs_xattr_entry *entry, unsigned int offset, 
		char* name_buff)
{
	int ret = FALSE;

	/* compare namespace_id */
	if (entry->ns_id != param->id)
		return FALSE;

	/* compare name_length */
	if (entry->name_length != param->name_length)
		return FALSE;


	/* compare name */
	if (0 == param->name_length)
	{
		/* POSIX_ACL_ACCESS or POSIX_ACL_DEFAULT */
		return TRUE;
	}
	else
	{
		/* USER or TRUSTED or SECURITY */
		ret = __xattr_io(READ, inode, 
				offset + sizeof(struct rfs_xattr_entry), 
				name_buff, param->name_length);
		if (ret)
		{
			DEBUG(DL0, "fail to read name (offset:%u,size:%u)", 
					offset, param->name_length);
			return ret;
		}
		/* compare name string */
		if (!strncmp(param->name, name_buff, param->name_length))
		{
			return TRUE;
		}
		else
		{
			DEBUG(DL3, "name is diff");
			return FALSE;
		}
	}
	return ret;
}

/**
 *  lookup extened attribute 
 * @param inode			inode
 * @param param			parameters struct contain extened attribute info
 * @param head			head
 * @param[out] entry_old	founded entry's ptr
 * @param[out] offset_curr	offset of founded entry
 * @param[out] offset_free	offset next free
 * @return			return -ENOEXIST on entry found,
 *					-ENOENTRY on entry not found,
 *					errno on failure.
 */
static int 
__xattr_lookup_entry(struct inode *inode, struct rfs_xattr_param *param,
	struct rfs_xattr_entry *entry_old, unsigned int *offset_curr,
	unsigned int *offset_free, unsigned int entry_length)
{
	char *name_buff = NULL;

	int ret = 0;
	int left_count = RFS_I(inode)->xattr_valid_count;
	int b_entry_found = FALSE;
	int b_victim_found = FALSE;

	unsigned int offset_cursor = sizeof(struct rfs_xattr_header);

	struct rfs_xattr_entry entry_tmp;

	/* TODO TODO if left_count equal to zero, no need to lookup */
	if (left_count == 0)
	{
		if (offset_free)
			*offset_free = offset_cursor;
		return -ENOENT;	
	}

	/* allocate name_buff */
	if (0 < param->name_length)
	{
		name_buff =  (char *) rfs_kmalloc(param->name_length, 
				GFP_KERNEL, NORETRY);
		if (NULL == name_buff)
		{
			DEBUG(DL0, "memory allocation failed\n");
			return -ENOMEM;
		}
	}

	while (left_count)
	{
		ret = __xattr_read_entry_head(inode, offset_cursor, entry_old);
		if (ret)
		{
			DEBUG(DL0, "fail to read xattr entry(offset:%u)", 
					offset_cursor);
			goto out;
		}

		DEBUG(DL3,"leftcnt:%d", left_count);

		if (IS_USED_XATTR_ENTRY(entry_old->type_flag))
		{
			/* assert entry is valid(check entry_used bit) */
			RFS_BUG_ON((entry_old->type_flag & XATTR_ENTRY_USED) 
					== 0);

			left_count--;
			if (b_entry_found == TRUE)
			{
				/* entry already found, now finding free space 
				 * offset, keeping going. */
				RFS_BUG_ON(b_victim_found == TRUE);
				goto next;
			}

			ret = __xattr_compare_entry(inode, param, entry_old, 
					offset_cursor, name_buff);
			if (TRUE == ret)
			{
				/* entry match */
				b_entry_found = TRUE;
				*offset_curr =  offset_cursor;
				if ((NULL == offset_free) || 
						(TRUE == b_victim_found))
				{
					/* no need to lookup for free */
					ret = -EEXIST;
					goto out;
				}
			}
			else if (FALSE == ret)
			{
				/* entry mis-match */
				goto next;
			}
			else /* error case */
			{
				RFS_BUG("fail to compare entry(%d)", ret);
				goto out;
			}
		}
		/* check deleted entry size; 
		 * whether possible to write over or not.
		 */
		else if (NULL != offset_free && FALSE == b_victim_found) 
		{
			/* entry_length mean length 
			   (entry_header + name + value + padding) */
			if (entry_old->entry_length == entry_length)
			{
				*offset_free = offset_cursor;
				b_victim_found = TRUE;
				if (TRUE == b_entry_found)
				{
					ret = -EEXIST;
					goto out;
				}

			}
		}
		DEBUG(DL3,"leftcnt:%d", left_count);

next:
		offset_cursor += entry_old->entry_length;
		DEBUG(DL3,"leftcnt:%d", left_count);
		if (TRUE == b_entry_found)
		{
			/* release caller's entry and point local entry
			 * for further _xattr_read_entry()
			 */
			entry_old = &entry_tmp;
		}
	}

	if ((NULL != offset_free) && (FALSE == b_victim_found))
	{
		*offset_free = offset_cursor;
	}

	ret = (b_entry_found)? -EEXIST: -ENOENT;
out:
	if (NULL != name_buff)
	{
		kfree(name_buff);
	}

	return ret;
}

/**
 *  perform entry compaction. (remove deleted attribute)
 * @param inode	inode
 * @return	return 0 on success, errno on failure
 */
#if 0
static int
__xattr_compaction(struct inode *inode)
{
	/*TODO : need implementation*/
	int ret = 0;
	DEBUG(DL0, "compaction was called");
	return ret;
}
#endif

/**
 *  initial creation of extended attribute. (header and 1st entry)
 * @param inode	inode
 * @param param	parameters struct contain extened attribute info
 * @param entry 1st entry value
 * @return	return 0 on success, errno on failure
 */
static int
__xattr_create_with_entry(struct inode *inode, struct rfs_xattr_param *param, 
		struct rfs_xattr_entry *entry)
{
	int ret = 0;
	unsigned int xattr_size_in_byte;

	xattr_size_in_byte = sizeof(struct rfs_xattr_header) 
		+ entry->entry_length;
	


	/* write xattr entry */
	ret = __xattr_write_entry(inode, sizeof(struct rfs_xattr_header), 
			param, entry);
	if (ret)
	{
		DEBUG(DL0, "fail to write xattr entry");
		goto out;
	}

	/* assign xattr's head info in rfs_inode_info */
	/* endian will consider in read/write head */

	/* [ctime, cdate], [uid, gid, mode] already initialize with '0'
	   in rfs_fill_inode() or fill_root_inode() */

	RFS_I(inode)->xattr_valid_count = 1;
	RFS_I(inode)->xattr_total_space = xattr_size_in_byte;
	RFS_I(inode)->xattr_used_space = xattr_size_in_byte;

	/* write xattr head */
	ret = rfs_xattr_write_header(inode);
	if (ret)
	{
		DEBUG(DL0, "fail to write xattr head");
		goto out;
	}

	/* dirty inode, xattr's start & exist flag bit will be write to storage
	 * in inode sync */
	if (inode->i_state & I_LOCK || inode->i_state & I_NEW)
		DEBUG(DL2, "\n\n\n!!inode->i_state has %s && %s!!\n\n\n",
				(inode->i_state & I_LOCK)?"I_LOCK":"",
				(inode->i_state & I_NEW)?"I_NEW":"");

	/* flush xattr_start_clus to entry in rfs_sync_inode(). */
	rfs_sync_inode(inode, 0, 0);
out:
	return ret;
}

/**
 *  initialize xattr_entry value 
 * @param entry entry ptr value to initialize
 */
#define _ENTRY_PAD_MASK		0x1F /* 31 in decimal */
#define _PADDED_ENTRY_SIZE(x) \
	(((x) + (_ENTRY_PAD_MASK)) & (~_ENTRY_PAD_MASK))

static inline void 
__xattr_init_entry(struct rfs_xattr_param *param, struct rfs_xattr_entry *entry)
{
	/* endian will consider in read/write entry */
	entry->type_flag = (__u8) XATTR_ENTRY_USED;
	entry->ns_id = (__u8) param->id;
	entry->name_length = param->name_length;
	entry->value_length = param->value_length;
	entry->entry_length = sizeof(struct rfs_xattr_entry) + 
		_PADDED_ENTRY_SIZE(param->name_length + param->value_length);
	entry->signature = RFS_XATTR_ENTRY_SIGNATURE;
}

/**
 *  set an extended attribute
 * @param inode		inode
 * @param param		parameters struct contain extened attribute info
 * @return	return 0 on success, errno on failure
 */
int
rfs_do_xattr_set(struct inode *inode, struct rfs_xattr_param *param)
{
	int ret = 0;
	int b_xhead_dirty = FALSE;
	int lookup_result;
	struct rfs_inode_info * rii = RFS_I(inode);

	unsigned int entry_offset_curr = 0;
	unsigned int entry_offset_free = 0;

	struct log_XATTR_info lxi;
	struct rfs_xattr_entry entry_old;
	struct rfs_xattr_entry entry_new;

	/* assert transaction started */
	RFS_BUG_ON(get_log_lock_depth(inode->i_sb) < 1);

	/* parameter check */
	/* TODO */
	if (unlikely(rii->i_state == RFS_I_FREE))
	{
		/*TODO consider cluster expanding when opended unlink */
		ret = -EINVAL;
		goto out;
	}

	ret = __xattr_get_name_length(param);
	if (ret)
	{
		DEBUG(DL0, "invalid name size");
		goto out;
	}

	/* set a new entry struct */
	__xattr_init_entry(param, &entry_new);

	/* when xattr does not exist */
	if ( ! IS_XATTR_EXIST(rii))
	{
		DEBUG(DL1, "xattr will create(it does not exist)");
		if (param->set_flag & XATTR_SET_FLAG_REPLACE)
		{
			DEBUG(DL0, "there is no extended attribute to replace");
			ret = -ENOATTR;
			goto out;
		}

		/* create new xattr */
		ret = __xattr_create_with_entry(inode, param, &entry_new);
		if (ret)
		{
			DEBUG(DL0, "fail to create enxtended attribute(%d)",
				       	ret);
			goto out; /* return fail */
		}
		/* __xattr_createwith_entry() treat all, just return success */
		goto out;
	}

	/* check whether need xattr compaction or not */
	if ((rii->xattr_used_space + entry_new.entry_length)
			> XATTR_COMPACTION_THRESHOLD)
	{
		ret = -ENOSPC;
		goto out;

		/*TODO implement compaction */
#if 0
		unsigned int margin_space 
			= rii.total_space - rii.used_space;

		if (margin_space < entry_new.entry_length)
		{
			/*marginal space should greater than entry_size*/
			DEBUG(DL0, "insufficent space to store extended "
					"attribute(%u<%u)", margin_space, 
					entry_new.entry_length);
			ret = -ENOSPC;
			goto out;
		}
		/* perform compaction */
		ret = __xattr_compaction(inode);
		if (ret)
		{
			DEBUG(DL0, "fail to xattr compaction");
			goto out;
		}
#endif
	}

	/* find entry with given name */
	lookup_result  = __xattr_lookup_entry(inode, param, &entry_old, 
			&entry_offset_curr, &entry_offset_free, 
			entry_new.entry_length);

	if (-EEXIST == lookup_result)
	{
		/* entry found */
		if (XATTR_SET_FLAG_CREATE == param->set_flag)
		{
			DEBUG(DL0, "already exist extended attribute(name:%s)",
				       param->name);
			ret = -EEXIST;
			goto out;
		}
	}
	else if (-ENOENT == lookup_result)
	{
		/* entry is not found */
		if (XATTR_SET_FLAG_REPLACE == param->set_flag)
		{
			DEBUG(DL0, "there is no extended attribute(name:%s)",
					param->name);
			ret = -ENOATTR;
			goto out;
		}

		/* if number of xattr is at maximum, return error */
		if (RFS_XATTR_ENTRY_NUMBER_MAX <= rii->xattr_valid_count)
		{
			DEBUG(DL0, "too many extended attribute");
			ret = -ENOSPC;
			goto out;
		}
	}
	else if (lookup_result)
	{
		/* error case */
		ret = lookup_result;
		DEBUG(DL0, "fail to lookup extended attribute(%d)", ret);
		goto out;
	}

	/* sub-log rfs_xattr_set here */
	lxi.xattr_offset_new = entry_offset_free; //entry offset will write.
	lxi.xattr_offset_old = entry_offset_curr; //entry offset will delete.

	ret = rfs_log_xattr_set(inode, &lxi);
	if (ret)
	{
		DEBUG(DL0, "fail to rfs_log_xattr_set(%d)", ret);
		goto out;
	}

	/* if entry is found, delete it */
	if (-EEXIST == lookup_result)
	{
		// assert(entry_old.type_flag & XATTR_ENTRY_USED
		entry_old.type_flag = XATTR_ENTRY_DELETE;

		/* write only entry type_flag (1byte) */
		ret = __xattr_io(WRITE, inode, entry_offset_curr, 
				(char*)(&entry_old.type_flag), 
				sizeof(entry_old.type_flag));
		if (ret)
		{
			DEBUG(DL0, "fail to delete old extended entry(%d)", 
					ret);
			goto out;
		}

		/* update head info in rfs_inode_info */
		rii->xattr_used_space -= entry_old.entry_length;
		rii->xattr_valid_count--;
		b_xhead_dirty = TRUE; /* require header_write() */
	}

	/* write new entry */
	ret = __xattr_write_entry(inode, entry_offset_free, param, &entry_new);
	if (ret)
	{
		DEBUG(DL0, "fail to write xattr entry(%d)", ret);
		goto out;
	}
	/* update head info in rfs_inode_info */
	rii->xattr_total_space += entry_new.entry_length;
	rii->xattr_used_space += entry_new.entry_length;
	rii->xattr_valid_count++;
	b_xhead_dirty = TRUE; /* require header_write() */

out:
	/* write head */
	if (TRUE == b_xhead_dirty)
	{
		b_xhead_dirty = FALSE;
		ret = rfs_xattr_write_header(inode);
		if (ret)
		{
			DEBUG(DL0, "fail to write xattr head(%d)", ret);
			goto out;
		}
	}

	return ret;
}

/**
 *  wrapper function for setxattr(). it treats transaction start/end.
 * @param inode		inode
 * @param param		parameters struct contain extened attribute info
 * @return	return 0 on success, errno on failure
 */
int
rfs_xattr_set(struct inode *inode, struct rfs_xattr_param *param)
{
	int ret;

	/* aquire xattr_sem(write) */
	down_write(&RFS_I(inode)->xattr_sem);

	/* transaction start */
	if (rfs_log_start(inode->i_sb, RFS_LOG_XATTR, inode))
	{
		DEBUG(DL0, "failed to rfs_log_start()");
		ret = -EIO;
		goto out;
	}

	ret = rfs_do_xattr_set(inode, param);
	if (ret)
	{
		if (ret != -ENOSPC)
			DEBUG(DL0, "fail to __do_xattr_set(%d)", ret);
		else
			DEBUG(DL2, "fail to __do_xattr_set(%d)", ret);
	}

#ifdef RFS_INTERNAL_R2_DEBUG_XATTR_SET
	printk("manual R2 Test!!!\n");
	printk("power off!!!\n");
	printk("manual R2 Test!!!\n");

	while(1);
#endif
	if (rfs_log_end(inode->i_sb, ret))
	{
		DEBUG(DL0, "fail to rfs_log_end");
		ret = -EIO;
		goto out;
	}
out:
	/* release xattr_sem(write) */
	up_write(&RFS_I(inode)->xattr_sem);
	return ret;
}

/**
 *  get an extended attribute
 * @param inode			inode
 * @param param			parameters struct contain extened attribute info
 * @param[out] value_size	value size as result
 * @return	return 0 on success, errno on failure
 */
int
rfs_xattr_get(struct inode *inode, struct rfs_xattr_param *param, 
		unsigned int *value_size)
{
	int ret = 0;

	unsigned int entry_offset_curr = 0;
	struct rfs_xattr_entry entry_get;

	/* parameter check */
	ret = __xattr_get_name_length(param);
	if (ret)
	{
		DEBUG(DL0, "invalid name size");
		return ret;
	}

	/* when xattr does not exist */
	if ( ! IS_XATTR_EXIST(RFS_I(inode)))
	{
		return -ENOATTR;
	}	

	/* aquire xattr_sem(read) */
	down_read(&RFS_I(inode)->xattr_sem);

	DEBUG(DL3, "xattr_start:%u,CLUTAIL:%u", RFS_I(inode)->xattr_start_clus,
		       	CLU_TAIL);

	/* find entry with given name */
	ret = __xattr_lookup_entry(inode, param, &entry_get, 
			&entry_offset_curr, NULL, 0);

	/* if entry was not found, return error */
	if (ret == -ENOENT)
	{
		ret = -ENOATTR;
		DEBUG(DL2, "fail to search entry(%d)", ret);
		goto out;
	}
	else if (ret != -EEXIST)
	{ 
		/* unexpected error */
		DEBUG(DL0, "unexpected err(%d) in looking up entry\n", ret);
		goto out;
	}

	/* if value_length is zero, return value length */
	if (0 == param->value_length)
	{
		*value_size = entry_get.value_length;
		ret = 0;
		goto out;
	}

	/* check size buffer for value */
	if (param->value_length < entry_get.value_length)
	{
		ret = -ERANGE;
		DEBUG(DL0, "too small buffer for extended attribute value");
		goto out;
	}

	entry_offset_curr += 
		(sizeof(struct rfs_xattr_entry) + entry_get.name_length);

	/* read value */
	ret = __xattr_io(READ, inode, entry_offset_curr, (char*)param->value, 
			entry_get.value_length);
	if (ret)
	{
		DEBUG(DL0, "fail to read extended attribute value(%d)", ret);
		goto out;
	}
	
	DEBUG(DL3, "name_len:%u,value_len:%u,entry_len:%u,value:%s", 
			entry_get.name_length, 	entry_get.value_length, 
			entry_get.entry_length, (char*)param->value);
	*value_size = entry_get.value_length;
out:
	/* release xattr_sem(read) */
	up_read(&RFS_I(inode)->xattr_sem);

	return ret;	
}

/**
 *  get "name list" and it's size
 * @param inode			inode
 * @param valid_count		count of valid entries
 * @param[in/out] list		buffer to store name of attribute
 * @param size			size of list
 * @param[out] list_size	list size as result
 * @return	return 0 on success, errno on failure
 */
static int
__get_xattr_list(struct inode *inode, unsigned int valid_count, char * list,
		unsigned int size, unsigned int *list_size)
{
	int ret = 0;

	struct rfs_xattr_entry entry;
	unsigned int offset_curr;
	int prefix_size;
	unsigned int total_size;
	unsigned int entry_size = sizeof(struct rfs_xattr_entry);

	*list_size = 0;
	total_size = 0;

	if (valid_count == 0)
	{
		return 0;
	}

	offset_curr = sizeof(struct rfs_xattr_header);

	while (valid_count)
	{
		/* read a entry */
		ret = __xattr_read_entry_head(inode, offset_curr, &entry);
		if (ret)
		{
			DEBUG(DL0, "fail to read xattr entry(offset:%u)", 
					offset_curr);
			goto out;
		}

		/* if entry is valid (not deleted) */
		if ((entry.type_flag & XATTR_ENTRY_DELETE) == 0)
		{
			prefix_size = _ns_table[entry.ns_id - 1].length;

			if (NULL != list)
			{
				/* check buffer size */
				if (size < (total_size + 
					prefix_size + entry.name_length + 1))
				{
					DEBUG(DL0, "the buffer size of list is"
							" too small");
					return -ERANGE;
				}
				/* copy prefix name */
				memcpy(list, _ns_table[entry.ns_id - 1].prefix,
					       	prefix_size);
				list += prefix_size;

				if (0 != entry.name_length)
				{
					/* read attribute's name */
					ret = __xattr_io(READ, inode, 
						offset_curr + entry_size, 
						list, entry.name_length);
					if (ret)
					{
						DEBUG(DL0, 
							"fail to read extended"
							" attribute entry(%d)",
							ret);
						goto out;
					}
					list += entry.name_length;
				}
				/*add null-terminator on name end */
				*(list++) = '\0';
			}
			total_size += prefix_size + entry.name_length + 1;
			valid_count--;
		}
		offset_curr += entry.entry_length;
	}
	*list_size = total_size;

out:
	return ret;
}

/**
 *  list extened attribute names
 * @param dentry	dentry
 * @param list		buffer for names
 * @param list_size	size of name list
 * @return	return 0 on success, errno on failure
 */
ssize_t
rfs_xattr_list(struct dentry *dentry, char *list, size_t list_size)
{
	int ret = 0;
	struct inode *inode = dentry->d_inode;
	ssize_t result_list_size;

	/* aquire xattr_sem(read) */
	down_read(&RFS_I(inode)->xattr_sem);

	/* when value_length is zero, 
	 * return required size for whole list of name 
	 */
	if (0 == list_size)
	{
		ret = __get_xattr_list(inode, RFS_I(inode)->xattr_valid_count, 
				NULL, 0, &result_list_size);
	}
	/* else, return whole list of name */
	else
	{
		ret = __get_xattr_list(inode, RFS_I(inode)->xattr_valid_count, 
				list, list_size, &result_list_size);
	}

	if (ret)
	{
		DEBUG(DL0, "fail to list extended attribute(%d)", ret);
		goto out;
	}
out:
	/* release xattr_sem(read) */
	up_read(&RFS_I(inode)->xattr_sem);

	return result_list_size;

}

/**
 *  delete an extened attribute 
 * @param inode	inode
 * @param param	parameters struct contain extened attribute info
 * @return	return 0 on success, errno on failure
 */
int
rfs_do_xattr_delete(struct inode *inode, struct rfs_xattr_param *param)
{
	int ret = 0;
	int lookup_result;

	unsigned int entry_offset_curr = 0;

	struct log_XATTR_info lxi;
	struct rfs_xattr_entry entry_del;

	/* assert transaction started */
	RFS_BUG_ON(get_log_lock_depth(inode->i_sb) < 1);

	/* parameter check */
	/* TODO */
	if (unlikely(RFS_I(inode)->i_state == RFS_I_FREE))
	{
		/* really? */
		DEBUG(DL0, "rfs does not suuport xattr on opened unlink file");
		ret = -EINVAL;
		goto out;
	}

	if ( ! IS_XATTR_EXIST(RFS_I(inode)))
	{
		ret = -ENOATTR;
		goto out;
	}

	ret = __xattr_get_name_length(param);
	if (ret)
	{
		DEBUG(DL0, "invalid name size");
		goto out;
	}

	/* find entry with given name */
	lookup_result  = __xattr_lookup_entry(inode, param, &entry_del, 
			&entry_offset_curr, NULL, 0);

	/* if entry is found, delete it */
	if (-EEXIST == lookup_result)
	{

		/* sub-log rfs_xattr_set here */
		/* assign invalid value to offset_new.*/
		lxi.xattr_offset_new = 0; 
		lxi.xattr_offset_old = entry_offset_curr; 

		ret = rfs_log_xattr_set(inode, &lxi);
		if (ret)
		{
			DEBUG(DL0, "fail to rfs_log_xattr_set(%d)", ret);
			goto out;
		}

		// assert(entry_old.type_flag & XATTR_ENTRY_USED)
		entry_del.type_flag = XATTR_ENTRY_DELETE;

		DEBUG(DL3, "offset_curr:%u", entry_offset_curr);

		/* write only entry type_flag (1byte) */
		ret = __xattr_io(WRITE, inode, entry_offset_curr, 
				(&entry_del.type_flag), 
				sizeof(entry_del.type_flag));
		if (ret)
		{
			DEBUG(DL0, "fail to delete old extended entry(%d)", 
					ret);
			goto out;
		}
		RFS_I(inode)->xattr_used_space -= entry_del.entry_length;
		RFS_I(inode)->xattr_valid_count--;

		/* write head */
		ret = rfs_xattr_write_header(inode);
		if (ret)
		{
			DEBUG(DL0, "fail to write xattr head(%d)", ret);
			goto out;
		}
	}
	/* entry not founded */
	else if (-ENOENT == lookup_result)
	{
		DEBUG(DL0, "fail to find extended attribute(name:%s)",
					param->name);
		ret = -ENOATTR;
		goto out;
	}
	/* other error case */
	else if (lookup_result)
	{
		ret = lookup_result;
		goto out;
	}
out:
	return ret;
}
/**
 *  wrapper function for setxattr(delete). it treats transaction start/end.
 * @param inode	inode
 * @param param	parameters struct contain extened attribute info
 * @return	return 0 on success, errno on failure
 */
int
rfs_xattr_delete(struct inode *inode, struct rfs_xattr_param *param)
{
	int ret = 0;

	/* aquire xattr_sem(write) */
	down_write(&RFS_I(inode)->xattr_sem);

	/* transaction start */
	if (rfs_log_start(inode->i_sb, RFS_LOG_XATTR, inode))
	{
		DEBUG(DL0, "fail to rfs_log_start");
		goto out;
	}

	ret = rfs_do_xattr_delete(inode, param);
	if (ret)
	{
		DEBUG(DL0, "fail to rfs_do_xattr_delete(%d)", ret);
	}

	if (rfs_log_end(inode->i_sb, ret))
	{
		DEBUG(DL0, "fail to rfs_log_end");
		ret = -EIO;
		goto out;
	}
out:
	/* release xattr_sem(write) */
	up_write(&RFS_I(inode)->xattr_sem);

	return ret;
}

#ifdef CONFIG_RFS_FS_FULL_PERMISSION
/**
 *  initial creation of extended attribute. (header only)
 * @param inode	inode
 * @param param	parameters struct contain extened attribute info
 * @return	return 0 on success, errno on failure
 */
static int
__xattr_create_head(struct inode *inode, uid_t *uid, gid_t *gid, umode_t *mode)
{
	int ret = 0;
	/* struct super_block *sb = inode->i_sb;*/
	unsigned int xattr_size_in_byte;

	xattr_size_in_byte = sizeof(struct rfs_xattr_header);
	
	/* assign xattr's head */
	/* endian will consider in read/write head */

	/* [ctime, cdate], [uid, gid, mode] already initialize with '0'
	   in rfs_fill_inode() or fill_root_inode() */
	RFS_I(inode)->xattr_valid_count = 0;
	RFS_I(inode)->xattr_total_space = xattr_size_in_byte;
	RFS_I(inode)->xattr_used_space = xattr_size_in_byte;
	if (uid)
		RFS_I(inode)->xattr_uid = *uid; 
	if (gid)
		RFS_I(inode)->xattr_gid = *gid;
	if (mode)
		RFS_I(inode)->xattr_mode = *mode;

	/* write xattr head */
	ret = rfs_xattr_write_header(inode);
	if (ret)
	{
		DEBUG(DL0, "fail to write xattr head(%d)", ret);
		goto out;
	}
	
	/* dirty inode, xattr's start & exist flag bit will be write to storage
	 * in inode sync */
	if (inode->i_state & I_LOCK || inode->i_state & I_NEW)
		DEBUG(DL2, "!!inode->i_state has %s && %s!!",
				(inode->i_state & I_LOCK)?"I_LOCK":"",
				(inode->i_state & I_NEW)?"I_NEW":"");

	rfs_sync_inode(inode, 0, 0);

out:
	return ret;
}

/**
 *  set uid, gid, mode to xattr header.(if xattr doesnoet exist, create it)
 * @param inode	inode
 * @param uid	prt variable of uid, if 
 * @param gid
 * @param mode
 * @return	return 0 on success, errno on failure
 */
/*TODO parameter refactoring ? */
int 
rfs_do_xattr_set_guidmode(struct inode* inode, uid_t *uid, gid_t *gid, 
		umode_t *mode)
{
	int ret = 0;
	int b_interal_log_lock = FALSE;

	/* parameter check */
	if (unlikely(RFS_I(inode)->i_state == RFS_I_FREE))
	{
		return -EINVAL;
	}

	/* start - temporary code for invokes without log-lock */
	if (get_log_lock_depth(inode->i_sb) < 1)
	{
		b_interal_log_lock = TRUE;
		/* transaction start */
		if (rfs_log_start(inode->i_sb, RFS_LOG_XATTR, inode))
		{
			DEBUG(DL0, "failed to rfs_log_start");
			return -EIO;
		}
	}
	/* end - temporary code for invokes without log-lock */

	/* assert transaction started */
	RFS_BUG_ON(get_log_lock_depth(inode->i_sb) < 1);


	/* when xattr does not exist */
	if ( ! IS_XATTR_EXIST(RFS_I(inode)))
	{
		DEBUG(DL2, "xattr will create(inode has no xattr)");
		/* create new xattr */
		ret = __xattr_create_head(inode, uid, gid, mode);
		if (ret)
		{
			DEBUG(DL0, "fail to create enxtended attribute(%d)",
				       	ret);
			goto out; /* return fail */
		}
		goto out; /* __xattr_create treat all, just return success */
	}

	/* assign new uid, gid and mode; if ptr is NULL skip assign */
	if (NULL != uid)
		RFS_I(inode)->xattr_uid = *uid;
	if (NULL != gid)
		RFS_I(inode)->xattr_gid = *gid;
	if (NULL != mode)
		RFS_I(inode)->xattr_mode = (*mode & 
				(S_ISGID | S_ISVTX | S_IRWXUGO));

	/* write head */
	ret = rfs_xattr_write_header(inode);
	if (ret)
	{
		DEBUG(DL0, "fail to write xattr head");
		goto out;
	}

out:
	/* start - temporary code for invokes without log-lock */
	if (TRUE == b_interal_log_lock)
	{
		/* transaction end */
		if (rfs_log_end(inode->i_sb, ret))
		{
			DEBUG(DL0, "fail to rfs_log_end");
			return -EIO;
		}
	}
	/* end - temporary code for invokes without log-lock */
	return ret;
}

/**
 *  wrapper function for set_guidmode. it treats transaction start/end.
 * @param inode	inode
 * @param uid	prt variable of uid, it will ignore when value is NULL.
 * @param gid	prt variable of gid, it will ignore when value is NULL.
 * @param mode	prt variable of mode, it will ignore when value is NULL.
 * @return	return 0 on success, errno on failure
 */
/*TODO parameter refactoring ? */
int 
rfs_xattr_set_guidmode(struct inode* inode, uid_t *uid, gid_t *gid, 
		umode_t *mode)
{
	int ret = 0;

	/* aquire xattr_sem(write) */ 
	/* caution : it can cause of dead-lock with xattr_sem and 
	 * transaction-lock, calling this function when transaction 
	 * already start. */
	down_write(&RFS_I(inode)->xattr_sem);

	/* transaction start */
	if (rfs_log_start(inode->i_sb, RFS_LOG_XATTR, inode))
	{
		DEBUG(DL0, "failed to rfs_log_start");
		ret = -EIO;
		goto out;
	}

	ret = rfs_do_xattr_set_guidmode(inode, uid, gid, mode);
	if (ret)
	{
		DEBUG(DL0, "fail to rfs_do_xattr_set_guidmode(%d)", ret);
	}
#ifdef RFS_INTERNAL_R2_DEBUG_XATTR_ALLOC
	printk("manual R2 Test alloc!!!\n");
	printk("power off!!!\n");
	printk("manual R2 Test!!!\n");

	while(1) ;
#endif
	/* transaction end */
	if (rfs_log_end(inode->i_sb, ret))
	{
		DEBUG(DL0, "fail to rfs_log_end");
		ret = -EIO;
		goto out;
	}
out:
	/* release xattr_sem(write) */
	up_write(&RFS_I(inode)->xattr_sem);

	return ret;
}

/**
 *  get uid, gid, mode from xattr header.
 * @param inode	inode
 * @param uid	prt variable of uid, if 
 * @param gid
 * @param mode
 * @return	return 0 on success, errno on failure
 */
/*TODO parameter refactoring ?*/
int 
rfs_xattr_get_guidmode(struct inode* inode, uid_t *uid, gid_t *gid, 
		umode_t *mode)
{
	int ret = 0;

	/* when xattr does not exist */
	if ( ! IS_XATTR_EXIST(RFS_I(inode)))
	{
		return -ENOATTR;
	}

	/* get permission value*/
	if (uid)
		*uid = RFS_I(inode)->xattr_uid;
	if (gid)
		*gid = RFS_I(inode)->xattr_gid;
	if (mode)
		*mode = RFS_I(inode)->xattr_mode;

	return ret;
}
#endif

