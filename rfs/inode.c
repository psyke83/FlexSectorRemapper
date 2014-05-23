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
 *  @file	fs/rfs/inode.c
 *  @brief	common inode operations
 *
 *
 */

#include <linux/fs.h>
#include <linux/rfs_fs.h>
#include <linux/smp_lock.h>

#include "rfs.h"
#include "log.h"
#include "xattr.h"


#ifdef RFS_FOR_2_6
#define GET_ENTRY_TIME(ep, inode)					\
do {									\
	inode->i_ctime.tv_sec = inode->i_atime.tv_sec =			\
		inode->i_mtime.tv_sec =					\
		rfs_entry_time(GET16(ep->mtime), GET16(ep->mdate));	\
	inode->i_ctime.tv_nsec = inode->i_mtime.tv_nsec =		\
		inode->i_atime.tv_nsec = 0;				\
} while(0);
#else
#define GET_ENTRY_TIME(ep, inode)					\
do {									\
	inode->i_ctime = inode->i_atime = inode->i_mtime =		\
		rfs_entry_time(GET16(ep->mtime), GET16(ep->mdate));	\
} while(0);
#endif

/*
 *  get an unique inode number
 * @param dir		parent directory
 * @param index 	dir entry's offset in parent dir's cluster(s)
 * @param[out] ino	return ino
 * @return		return 0 on success, errno on failure
 */
int rfs_iunique(struct inode *dir, unsigned int index, unsigned long *ino)
{
	struct super_block *sb = dir->i_sb;
	struct rfs_sb_info *sbi = RFS_SB(sb);

	/*
	 * NOTE: RFS uses hash value for iunique
	 * which is byte address of index >> bits of dir entry's size.
	 * 0 ~ 15th entries are reserved, because reserved area has one sector
	 * at least. Especially 1 belongs to root inode
	 */
	if ((RFS_I(dir)->start_clu != sbi->root_clu) || IS_FAT32(sbi)) {
		loff_t offset;
		unsigned int cluster_offset;
		unsigned int prev, next;
		int err;

		/* in FAT32 root dir or sub-directories */
		offset = ((loff_t) index) << DENTRY_SIZE_BITS;
		cluster_offset = (unsigned int) (offset >> sbi->cluster_bits);
		fat_lock(sb);
		err = rfs_find_cluster(sb, RFS_I(dir)->start_clu, 
				cluster_offset, &prev, &next);
		fat_unlock(sb);
		if (err)
			return err;

		offset &= sbi->cluster_size - 1;
		offset += (((loff_t) START_BLOCK(prev, sb)) << 
					sb->s_blocksize_bits);
		*ino = (unsigned long) (offset >> DENTRY_SIZE_BITS);
	} else {
		/* in root directory */
		*ino = (unsigned long) ((sbi->root_start_addr >> 
					DENTRY_SIZE_BITS) + index);
	}

	return 0;
}

/**
 *  fill up the RFS-specific inode xattr_info
 * @param inode		inode
 * @param ep		dir entry
 * @return		return 0 on success, errno on failure
 *
 * it is only invoked when inode is normal file and directory (not root dir)
 */
static int 
__fill_inode_xattr(struct inode *inode, struct rfs_dir_entry *ep)
{
	int ret = 0;
#ifdef CONFIG_RFS_FS_XATTR
	/* fill xattr's start, last */
	unsigned int xclus_start = CLU_TAIL;
	unsigned int last_clus = CLU_TAIL;
	int xattr_num_clus = 0;

	DEBUG(DL3, "ep->attr:0x%x,size:%u", ep->attr, GET32(ep->size));
	if (test_opt(inode->i_sb, EA) && ep->attr & ATTR_XATTR)
	{
		xclus_start = XATTR_START_CLUSTER(ep);

		/* assert xattr_start_clus is valid */
		RFS_BUG_ON(IS_INVAL_CLU(RFS_SB(inode->i_sb), xclus_start));

		xattr_num_clus = rfs_find_last_cluster(inode, 
				xclus_start, &last_clus, 0, 0, NULL);

	}

	/* init xattr allocation related variable */
	RFS_I(inode)->xattr_start_clus = xclus_start;
	RFS_I(inode)->xattr_last_clus = last_clus;
	RFS_I(inode)->xattr_numof_clus = xattr_num_clus;

	if (CLU_TAIL != xclus_start)
	{
		/* read xattr related etc. variable
		   total_space, used_space, valid_count,
		   uid, gid, mode,
		   ctime, cdate */
		ret = rfs_xattr_read_header_to_inode(inode, FALSE);
		if (ret)
		{
			DEBUG(DL0, "fail to rfs_xattr_read_header_to_"
					"inode(%d)", ret);
			goto out;
		}
	}
	else
	{
		/* init xattr related etc. variable */
		RFS_I(inode)->xattr_total_space = 0;
		RFS_I(inode)->xattr_used_space = 0;
		RFS_I(inode)->xattr_valid_count = 0;
		RFS_I(inode)->xattr_uid = 0;
		RFS_I(inode)->xattr_gid = 0;
		RFS_I(inode)->xattr_mode = 0;
		RFS_I(inode)->xattr_ctime = GET16(ep->ctime);
		RFS_I(inode)->xattr_cdate = GET16(ep->cdate);
	}
	init_rwsem(&(RFS_I(inode)->xattr_sem));
out:
#endif
	return ret;
}

/**
 *  compare ep->name to forbidden name(RFS_LOG_ENTRY_NAME)
 * @param ep		dir entry
 * @return		return 1 on forbidden name, 0 on normal name.
 *
 */
static inline int __is_forbidden_name(struct qstr *d_name)
{
	if (d_name->len != RFS_LOG_FILE_LEN)
		return 0;
	if (!strncmp(d_name->name, RFS_LOG_FILE_NAME, RFS_LOG_FILE_LEN))
		return 1;
	return 0;

}
/**
 *  fill up the RFS-specific inode
 * @param dir		parent dir's inode
 * @param inode		inode
 * @param ep		dir entry
 * @param dentry	dir entry index
 * @param mode		initial mode of inode
 * @return		return 0 on success, errno on failure
 *
 * it is only invoked when new inode is allocated
 */
int rfs_fill_inode(struct inode *dir, struct inode *inode, struct rfs_dir_entry *ep, unsigned int dentry, int mode, struct buffer_head *bh, struct qstr *d_name)
{
	struct super_block *sb = inode->i_sb;
	unsigned int type;
	unsigned int size;
	unsigned int start_cluster;
	unsigned int p_start_clu;

	int err = 0, link_count = 0;
	int num_clus = 0;
	
	/* fill the RFS-specific inode info */
	p_start_clu = RFS_I(dir)->start_clu;
	RFS_I(inode)->p_start_clu = p_start_clu;
	RFS_I(inode)->index = dentry;
	RFS_I(inode)->hint_last_clu = 0;
	RFS_I(inode)->hint_last_offset = 0;
	RFS_I(inode)->i_state = RFS_I_ALLOC;

	// ESR Debugging 20090711
	if (bh)
	{
		int dir_per_block = 
			sb->s_blocksize / sizeof(struct rfs_dir_entry);
		int dir_per_block_bits = ffs(dir_per_block) - 1;
		
		RFS_I(inode)->block_nr = bh->b_blocknr;
		RFS_I(inode)->offset = 
			((loff_t)bh->b_blocknr << dir_per_block_bits)
			| (ep - (struct rfs_dir_entry *)bh->b_data);
	}
	else
	{
		RFS_I(inode)->block_nr = ~0;
		RFS_I(inode)->offset = ~0;
	}

#ifdef CONFIG_RFS_FAST_SEEK
	/*
	 * Initialize fast seek info.
	 * Although following codes also exist in NEW_INODE(),
	 * lookup() only executes here
	 */ 
	RFS_I(inode)->fast_seek = NULL;
#endif
#ifdef CONFIG_RFS_FAST_LOOKUP
	/* initialize fast open info  */
	RFS_I(inode)->fast = 0;
#endif

	start_cluster = START_CLUSTER(RFS_SB(sb), ep);

	/* sanity code */
	if (start_cluster && IS_INVAL_CLU(RFS_SB(sb), start_cluster)) 
	{
		err = -EIO;
		dump_entry(sb, ep, bh, RFS_I(inode)->offset, "at %s:%d", 
				__func__, __LINE__);
		RFS_BUG_CRASH(sb, "dir entry(%u,%u) has corrupted "
			"start_clu(%u)\n", p_start_clu, dentry, start_cluster);
		goto bad_inode;
	}

	if (start_cluster) 
	{
		unsigned int last_clu;
			
		RFS_I(inode)->start_clu = start_cluster;
		num_clus = rfs_find_last_cluster(inode, start_cluster, 
				&last_clu, 0, 0, NULL);
		if (num_clus < 0) 
		{
			err = num_clus;
			DEBUG(DL0, "No last cluster (err : %d)\n", err);
			goto bad_inode;
		}
		RFS_I(inode)->last_clu = last_clu;
	} 
	else 
	{
		/* cluster was not assigned to inode */
		RFS_I(inode)->start_clu = CLU_TAIL;
		RFS_I(inode)->last_clu = CLU_TAIL;
	}

	err = __fill_inode_xattr(inode, ep);
	if (err)
	{
		DEBUG(DL0, "fail to __fill_inode_xattr(%d)", err);
		goto bad_inode;
	}

	spin_lock_init(&RFS_I(inode)->write_lock);

	type = rfs_entry_type(ep);

	/* set i_size, i_mode, i_op, i_fop, i_mapping, i_nlink */
	if (type == TYPE_DIR) 
	{
		/* directory always has zero-size by fat spec */
		inode->i_size = ((loff_t) num_clus) << RFS_SB(sb)->cluster_bits;

		inode->i_mode = S_IFDIR;
		inode->i_op = (test_opt(sb, EA))? 
			&rfs_dir_inode_operations_xattr :
			&rfs_dir_inode_operations ;
		inode->i_fop = &rfs_dir_operations;
		link_count = rfs_count_subdir(sb, RFS_I(inode)->start_clu);
		if (unlikely(link_count < 2)) 
		{
			err = link_count;
			goto bad_inode;
		} 
		else 
		{
			inode->i_nlink = (nlink_t) link_count;
		}
	} 
	else if (type == TYPE_FILE) 
	{
		int rcv_clus;

		if (IS_SYMLINK(ep))
		{
			inode->i_mode = S_IFLNK;
			inode->i_op = (test_opt(sb, EA)) ? 
				&rfs_symlink_inode_operations_xattr :
				&page_symlink_inode_operations ;
		} 
		else if (IS_SOCKET(ep))
		{
			inode->i_mode = S_IFSOCK;
			inode->i_op = (test_opt(sb, EA)) ?
				&rfs_special_inode_operations_xattr:
				&rfs_special_inode_operations;
			init_special_inode(inode, inode->i_mode, 0);
			/* CHR, BLK device file needs (dev_t) as arg */
		}
		else if (IS_FIFO(ep))
		{
			inode->i_mode = S_IFIFO;
			inode->i_op = (test_opt(sb, EA)) ?
				&rfs_special_inode_operations_xattr :
				& rfs_special_inode_operations;
			init_special_inode(inode, inode->i_mode, 0);
		}
		else 
		{
			inode->i_mode = S_IFREG;
			inode->i_op = (test_opt(sb, EA))?
					&rfs_file_inode_operations_xattr :
					&rfs_file_inode_operations;
			inode->i_fop = &rfs_file_operations;
		}
		inode->i_mapping->a_ops = &rfs_aops;
		inode->i_nlink = 1;

		size = GET32(ep->size);

		/*
		 * Prevent truncating logfile by lookup.
		 * It causes error in managing segment list.
		 * rfs_info->self_inode is valid only if RFS is on mounting.
		 */

		if (RFS_I(sb->s_root->d_inode)->start_clu == p_start_clu &&
				__is_forbidden_name(d_name))
		{
			if (RFS_SB(sb)->use_log && RFS_LOG_I(sb) &&
					RFS_LOG_I(sb)->self_inode != NULL)
			{
				/*
				 * when target is logfile and self_inode has
				 * valid inode.
				 * truncate logfile to RFS_LOG_MAX_SIZE_IN_BYTE.
				 */
			}
			else
			{
				/*
				 * when self_inode equal to NULL or RFS_LOG_I is
				 * NULL, skip truncating by lookup
				 */
				inode->i_size = 
					(loff_t) RFS_LOG_MAX_SIZE_IN_BYTE;

				inode->i_mode = 0x8000;
				inode->i_uid = 0x0;
				inode->i_gid = 0x0;

				if (size != (unsigned int) 
						RFS_LOG_MAX_SIZE_IN_BYTE)
				{
					RFS_I(inode)->i_state 
						|= RFS_I_MODIFIED;
				}

				goto cont_logfile;
			}
		}

		/* size & chain recovery */
		rcv_clus = (int) ((size + RFS_SB(sb)->cluster_size - 1)
				>> RFS_SB(sb)->cluster_bits);

		if (sb->s_flags & MS_RDONLY)
		{
			/* clear modified bit if MS_RDONLY */
			RFS_I(inode)->i_state &= ~RFS_I_MODIFIED;

			/*
			 * skip size & data recover while RO mount
			 * to prevent write operation.
			 */
			if (rcv_clus > num_clus)
			{
				inode->i_size = ((loff_t) num_clus <<
						RFS_SB(sb)->cluster_bits);
			}
			else
			{
				inode->i_size = (loff_t) size;			
			}

			goto cont_logfile;
		
		}

		/*
		 * rcv_clu is based on ep->size and
		 * num_clus is based on real cluster chain
		 */
		if (rcv_clus < num_clus) 
		{
			/* RFS-log : data replay */
			DEBUG(DL0, "data recover(\"%s\",%u,%u,%u):%d!=%d",
					ep->name,
					RFS_I(inode)->p_start_clu,
					RFS_I(inode)->index,
					RFS_I(inode)->start_clu,
					num_clus, rcv_clus);

			inode->i_size = (loff_t) size;
			RFS_I(inode)->trunc_start = 
				((unsigned long) num_clus 
				 << RFS_SB(sb)->cluster_bits);
			rfs_truncate(inode);
			if (0 == size)
			{
			/* if inode's size is '0', set entry's start_clu to '0'
			 * and RFS_I_MODIFIED bit. It will clear in rfs_lookup()
			 */
				SET_START_CLUSTER(ep, 0);
				RFS_I(inode)->i_state |= RFS_I_MODIFIED;
			}

		} 
		else if (rcv_clus > num_clus) 
		{
			/* just set inode->i_size not modify ep->size */
			DEBUG(DL0, "size recover(\"%s\",%u,%u,%u):%d!=%d\n"
					"\t %u -> %u",
					ep->name,
					RFS_I(inode)->p_start_clu,
					RFS_I(inode)->index,
					RFS_I(inode)->start_clu,
					num_clus, rcv_clus,
					size,
					(num_clus << RFS_SB(sb)->cluster_bits));

			inode->i_size = ((loff_t) num_clus <<
					RFS_SB(sb)->cluster_bits);
			SET32(ep->size, ((unsigned int) inode->i_size));
			/*
			 *  set RFS_I_MODIFIED bit in i_state
			 *  RFS_MODIFOED will clear in rfs_lookup()
			 */
			RFS_I(inode)->i_state |= RFS_I_MODIFIED;
		}
	       	else
		{
			inode->i_size = (loff_t) size;
		}
	} 
	else 
	{
		DEBUG(DL0, "dos entry type(%x) is invalid(%u, %u)\n", 
			type, RFS_I(inode)->p_start_clu, RFS_I(inode)->index);
		err = -EINVAL;
		goto bad_inode;
	}

cont_logfile:
	inode->i_version = 0;
	GET_ENTRY_TIME(ep, inode); 

#ifndef RFS_FOR_2_6_19
	/* This is the optimal IO size (for stat), not the fs block size */
	inode->i_blksize = sb->s_blocksize;
#endif

	inode->i_blocks = 
		(blkcnt_t) (((inode->i_size + RFS_SB(sb)->cluster_size - 1)
				& (~(RFS_SB(sb)->cluster_size -1))) 
				>> SECTOR_BITS);
#ifdef CONFIG_RFS_FS_XATTR
	inode->i_blocks += ((blkcnt_t) RFS_I(inode)->xattr_numof_clus 
			<< (RFS_SB(sb)->cluster_bits - SECTOR_BITS));
#endif
	set_mmu_private(inode, inode->i_size);

#ifndef CONFIG_RFS_FS_FULL_PERMISSION
	rfs_get_mode(ep, &inode->i_mode);
	rfs_get_uid(ep, &inode->i_uid);
	rfs_get_gid(ep, &inode->i_gid);
#else
	if (!test_opt(sb, EA))
	{
		rfs_get_mode(ep, &inode->i_mode);
		rfs_get_uid(ep, &inode->i_uid);
		rfs_get_gid(ep, &inode->i_gid);
	}
	else if (IS_XATTR_EXIST(RFS_I(inode)))
	{
		/* get uid, gid, mode */
		umode_t mode_tmp;

		err = rfs_xattr_get_guidmode(inode, &(inode->i_uid),
			       &(inode->i_gid), &mode_tmp);
		if (err)
		{
			DEBUG(DL0, "fail to rfs_xattr_get_guidmode(%d)", err);
			/*TODO error handling */
			goto bad_inode;
		}
		inode->i_mode |= mode_tmp; 
	}
	else
	{
		/* init permission informations(uid, gid, mode) */
		uid_t uid = rfs_current_fsuid();
		gid_t gid = rfs_current_fsgid();
		umode_t mode_tmp = (mode & (S_ISGID | S_ISVTX | S_IRWXUGO));

		if (dir->i_mode & S_ISGID)
		{
			if (type == TYPE_DIR)
			{
				mode_tmp |= S_ISGID;
			}
			gid = dir->i_gid;
		}

		/* transaction already started.
		   call directly rfs_do_xattr_set_guidmode. */
		err = rfs_do_xattr_set_guidmode(inode, &uid, &gid, &mode_tmp);
		if (err)
		{
			/*TODO error handling such as ENOSPC */
			DEBUG(DL0, "Fail to rfs_xattr_set_guidmode(%d)", err);
			goto bad_inode;
		}

		/* if rfs_do_xattr_set_guidmode() success,
		   assign uid, gid, mode in inode.*/
		inode->i_uid = uid;
		inode->i_gid = gid;
		inode->i_mode |= mode_tmp;

		DEBUG(DL3, "set guidmode(name:%s,uid:%d,gid:%d,mode:%u)", 
				ep->name, uid, gid, mode_tmp);
	}
#endif
	/*
	 * At unlink, child gets parent inode for race condition between
	 * write_inode(child) and rmdir parent
	 */
	RFS_I(inode)->parent_inode = NULL;

#ifdef RFS_FOR_2_4
	init_MUTEX(&RFS_I(inode)->data_mutex);
	init_timer(&RFS_I(inode)->timer);
	RFS_I(inode)->timer.function = rfs_data_wakeup;
	RFS_I(inode)->timer.data = (unsigned long) RFS_I(inode);
#endif

	return 0;

bad_inode:
	make_bad_inode(inode);
	return err;
}

/**
 *  drop a new inode when failed to create it
 * @param inode		inode
 */
static void __drop_new_inode(struct inode *inode)
{
	inode->i_nlink = 0;
	iput(inode);
}



/**
 *  create a new inode
 * @param dir		inode of parent directory
 * @param dentry	dentry associated with inode will be created
 * @param type		inode type
 * @return		return inode pointer on success, erro on failure
 */
struct inode *rfs_new_inode(struct inode *dir, struct dentry *dentry, unsigned int type, int mode)
{
	struct inode *inode = NULL;
	struct buffer_head *bh = NULL;
	struct rfs_dir_entry *ep = NULL;
	unsigned long new_ino;
	unsigned int index;
	int ret = 0;

	/* check valid type */
	if ((type != TYPE_DIR) && (type != TYPE_FILE) &&
			(type != TYPE_SYMLINK) && (type != TYPE_SOCKET)	
			&& (type != TYPE_FIFO))
		return ERR_PTR(-EINVAL);

	/* get a new inode */
	if (!(inode = NEW_INODE(dir->i_sb)))
		return ERR_PTR(-ENOMEM); /* memory error */

	/* initialize dir entry and extend entries */
	ret = build_entry(dir, inode, 0, type, dentry->d_name.name, mode);
	if (ret < 0)
	{
		if (-EIO == ret)
		{
			RFS_BUG_CRASH(dir->i_sb, "Err(%d) in building "
					"entry\n", ret);
		}
		goto out_free_drop;
	}
	index = (unsigned int) ret;

	/*DEBUG(DL3, "name:%s,p_start:%u,index:%d", dentry->d_name.name, 
			RFS_I(dir)->start_clu, index);*/

	ep = get_entry(dir, index, &bh);
	if (IS_ERR(ep)) 
	{
		ret = PTR_ERR(ep);
		RFS_BUG_CRASH(dir->i_sb, "Err(%d) in get_entry after building "
				"entry\n", ret);
		goto out_drop_entry;
	}

	/* fill inode info */
	ret = rfs_fill_inode(dir, inode, ep, index, mode, bh, &dentry->d_name);
#ifdef CONFIG_RFS_FS_FULL_PERMISSION
	if (ret == -ENOSPC)
	{
		/* it is possible error in rfs_fill_inode() with xattr */
		goto out_drop_entry;
	}
	else 
#endif
	if (ret)
	{
		dump_entry(dir->i_sb, ep, bh, RFS_I(inode)->offset, "at %s:%d",
			       	__func__, __LINE__);
		RFS_BUG_CRASH(dir->i_sb, "Err(%d) in fill_inode after building "
				"entry\n", ret);
		goto out_drop_entry;
	}

	/* get new inode number */
	ret = rfs_iunique(dir, index, &new_ino);
	if (ret) 
	{
		dump_entry(dir->i_sb, ep, bh, RFS_I(inode)->offset, "at %s:%d",
			       	__func__, __LINE__);
		RFS_BUG_CRASH(dir->i_sb, "Err(%d) in rfs_iunique after "
				"building entry\n", ret);
		goto out_drop_entry;
	}

	/* init security extended attribute */
	ret = rfs_init_security(inode,dir);
	if (ret)
	{
		DEBUG(DL0, "Fail to rfs_init_security(%d)", ret);
		goto out_drop_entry;
	}
		
	if (type == TYPE_DIR) {
		/* increase parent's i_nlink */
		dir->i_nlink++;
	} else if ((type == TYPE_FILE) || (type == TYPE_SYMLINK)) {
		/* initialize it when only create time */
		inode->i_mapping->nrpages = 0;
	}

	/* insert new inode to inode hash table */
	inode->i_ino = new_ino;
	insert_inode_hash(inode);

	/* 
	 * we already registerd the inode to the transaction
	 * in the above build_entry() 
	 */
	rfs_mark_inode_dirty_in_tr(inode);

	dir->i_mtime = dir->i_atime = CURRENT_TIME;
	rfs_mark_inode_dirty_in_tr(dir);

	brelse(bh);

	return inode;

out_drop_entry:
	make_bad_inode(inode);
	rfs_remove_entry(dir, inode);
	/* Dealloc chain
	 * rfs_delete_inode() does not treat bad inode (in rfs_fill_inode())
	 * So, completely deallocate the chain here. 
	 */
	if (rfs_open_unlink_chain(inode) == 0)
	{
		RFS_I(inode)->i_state = RFS_I_FREE;
#ifdef CONFIG_RFS_FS_XATTR
		DEBUG(DL0, "bad inode (pdir %d, index %d, start %d, last %d,"
			" xclu %d)", RFS_I(inode)->p_start_clu,
				RFS_I(inode)->index,
				RFS_I(inode)->start_clu,
				RFS_I(inode)->last_clu,
				RFS_I(inode)->xattr_start_clus);
#endif
		/* TODO comment */
		rfs_meta_commit(inode->i_sb);
		rfs_dealloc_clusters(inode, 0);
	}
out_free_drop:
	__drop_new_inode(inode);

	if (!IS_ERR(bh))
	{
		brelse(bh);
	}

	return ERR_PTR(ret);
}

/**
 *  delete blocks of inode and clear inode
 * @param inode	inode will be removed
 * @return	return 0 on success, errno on failure
 *
 * It will be invoked at the last iput if i_nlink is zero
 */
void rfs_delete_inode(struct inode *inode)
{
	int ret;
#ifdef RFS_FOR_2_4
	lock_kernel();
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,14)
	truncate_inode_pages(&inode->i_data, 0);
#endif

	if (!is_bad_inode(inode)) 
	{
		/* Just free in-core inode, not unlinking */
		if (RFS_I(inode)->i_state != RFS_I_FREE)
			goto out;

		// ESR Debug 20090716
		{
			if (inode->i_ino != RFS_SB(inode->i_sb)->highest_d_ino)
			{
				dump_inode(inode);
				RFS_BUG_CRASH(inode->i_sb,
						"ino(%lu) is corrupt",
						inode->i_ino);
				goto out;
			}
		}

		/* RFS-log : start of transaction */
		if (rfs_log_start(inode->i_sb, RFS_LOG_DEL_INODE, inode))
			goto out;

		/* actual de-allocation of clusters */
		/* Whatever result is, call clear inode */
		ret = rfs_dealloc_clusters(inode, 0);

		if (RFS_I(inode)->parent_inode != NULL)
		{
			iput(RFS_I(inode)->parent_inode);
			RFS_I(inode)->parent_inode = NULL;
		}

		DEBUG(DL3,"used_cluster:%u", 
				RFS_SB(inode->i_sb)->num_used_clusters);

		/* RFS-log : end of transaction */
		rfs_log_end(inode->i_sb, ret);
	}

out:
	clear_inode(inode);

#ifdef RFS_FOR_2_4
	unlock_kernel();
#endif
}

/**
 * open-unlink chain (for por, move chain to last of logfile)
 * @param inode	inode will be removed
 * @return	return 0 on success, errno on failure
 */
int rfs_open_unlink_chain(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	struct log_FAT_info lfi;
	unsigned int chain[2];
	int err, ret = 0;

	/*
	 * when unlinked empty file is first written,
	 * this function is called
	 */
	lfi.pdir = (RFS_I(inode)->i_state == RFS_I_FREE) ? CLU_TAIL :
		RFS_I(inode)->p_start_clu;

	lfi.entry = RFS_I(inode)->index;
	lfi.s_filesize = (unsigned int) inode->i_size;

	lfi.numof_clus = 2;
	lfi.clus = chain;

	fat_lock(sb);
#ifdef CONFIG_RFS_FS_XATTR
	DEBUG(DL2, "[xattr]start:%u,last:%u", RFS_I(inode)->xattr_start_clus,
		       RFS_I(inode)->xattr_last_clus); 
	if (RFS_I(inode)->i_state != RFS_I_FREE)
	{
		/*
		 * When opened unlinked empty inode gets a cluter,
		 * this function is called again. Skip this call.
		 * Make sure that xcluster of opened unlinked file is
		 * never allocated
		 */
		if (IS_XATTR_EXIST(RFS_I(inode)))
		{
			chain[0] = RFS_I(inode)->xattr_start_clus;
			chain[1] = RFS_I(inode)->xattr_last_clus;
			lfi.b_is_xattr = TRUE;
			DEBUG(DL2, "insert segment[%d,%d]", chain[0], chain[1]);
			err = RFS_LOG_I(sb)->operations->log_move_chain(sb,
				       &lfi);
			if (err)
			{
				DPRINTK("err in chain move(err:%d)", err);
				ret = err;
			}
		}
	}
#endif
	/* empty file */
	if (RFS_I(inode)->start_clu == CLU_TAIL)
	{
		DEBUG(DL2, "empty file");
		goto out;
	}

	chain[0] = RFS_I(inode)->start_clu;
	chain[1] = RFS_I(inode)->last_clu;

	lfi.b_is_xattr = FALSE;
	DEBUG(DL2, "insert segment [%d, %d]", chain[0], chain[1]);
	err = RFS_LOG_I(sb)->operations->log_move_chain(sb, &lfi);
	if (err)
	{
		DPRINTK("err in chain move");
		ret = err;
		goto out;
	}
out:
	fat_unlock(sb);
	return ret;
}

/**
 * remove entry, open-unlink chain, and change inode's state
 * @param dir	parent directory inode	
 * @param inode	inode will be removed
 * @return	return 0 on success, errno on failure
 */
int rfs_delete_entry(struct inode *dir, struct inode *inode)
{
	int ret;

#ifdef CONFIG_RFS_FAST_LOOKUP
	if ( IS_FAST_LOOKUP_INDEX(inode) ||
		(RFS_I(inode)->fast && 
		 RFS_I(dir)->start_clu != RFS_I(inode)->p_start_clu))
	{
		DPRINTK("Can not delete the file for fast seek\n");
		return -EFAULT;
	}
#endif

#ifdef CONFIG_RFS_FAST_SEEK
	/* remove fast seek info before unlink */
	rfs_free_fast_seek(RFS_I(inode));
#endif

	/* remove dos & extent entries */
	ret = rfs_remove_entry(dir, inode);
	if (ret)
	{
		DEBUG(DL0, "fail to rfs_remove_entry(%d)", ret);
		return ret;
	}

	/* until delete_inode, chain should be alive */
	ret = rfs_open_unlink_chain(inode);
	if (ret)
	{
		DPRINTK("Open unlink error (%d)\n", ret);
		return ret;
	}

	/* decrease i_nlink */
	inode->i_nlink--;

	if (S_ISDIR(inode->i_mode)) 
	{
		inode->i_nlink--;
		dir->i_nlink--;
	}

	if (unlikely(inode->i_nlink)) 
	{
		dump_inode(inode);
		RFS_BUG_CRASH(dir->i_sb, "nlink of inode(%s, %u, %u, %u) is not "
				"zero (%u) \n", S_ISDIR(inode->i_mode) ? "DIR":"FILE",
					RFS_I(inode)->p_start_clu,
			       	RFS_I(inode)->index, RFS_I(inode)->start_clu,
			       	inode->i_nlink);
		return -EIO;
	}

	inode->i_mtime = inode->i_atime = CURRENT_TIME;
	dir->i_mtime = dir->i_atime = CURRENT_TIME; 

	/* change ino to avoid duplication */
	remove_inode_hash(inode);

	/* do not change the order of statements for sync with write_inode() */
	spin_lock(&RFS_I(inode)->write_lock);
	inode->i_ino = RFS_SB(dir->i_sb)->highest_d_ino;
	RFS_I(inode)->i_state = RFS_I_FREE;
	spin_unlock(&RFS_I(inode)->write_lock);

	insert_inode_hash(inode);

	/*
	 * The parent that has opend unlinked children can be deleted.
	 * Due to conrrent unlink(open child) and * rmdir(parent)
	 * rfs_sync_inode() calls get_entry_with_cluster(opend unlink child).
	 * At that time, get_entry_with_cluster() can meets a free cluster
	 * because its parent was deleted and chain of the parent was free.
	 * To avoid deletion of parent, a child increases parent's ref in 
	 * unlink() and decrease it in delete_inode().
	 */
	/* because iget() removed in 2.6.25 kernel, ilookup() replaces iget() */
	RFS_I(inode)->parent_inode = ilookup(dir->i_sb, dir->i_ino);
	if (RFS_I(inode)->parent_inode != dir)
	{
		struct inode *p_inode = RFS_I(inode)->parent_inode;

		dump_inode(inode);

		if (p_inode)
		{
			DEBUG(DL0, "my parent(%lu): p_start# %u idx# %u start# %u offset# %lld",
					p_inode->i_ino,
					RFS_I(p_inode)->p_start_clu, RFS_I(p_inode)->index, RFS_I(p_inode)->start_clu,
					(long long) RFS_I(p_inode)->offset);
		}
		iput(RFS_I(inode)->parent_inode);

		DEBUG(DL0, "cur p.dir(%lu): p_start# %u idx# %u start# %u offset# %lld",
				dir->i_ino,
				RFS_I(dir)->p_start_clu, RFS_I(dir)->index, RFS_I(dir)->start_clu,
				(long long)RFS_I(dir)->offset);
		dump_inode(dir);

		RFS_BUG_CRASH(dir->i_sb, "Fail to get parent(ino: %lu)\n", dir->i_ino);

		return -EIO;
	}

	rfs_mark_inode_dirty_in_tr(dir);

	return 0;
}

#ifdef CONFIG_RFS_IGET4
/**
 *  fill up the in-core inode 
 *  @param inode	created new inode	
 *  @param private	private argument to fill
 *
 *  it has a same role with rfs_fill_inode
 */
void rfs_read_inode2(struct inode *inode, void *private)
{
	struct rfs_iget4_args *args;

	args = (struct rfs_iget4_args *) private;

	// not built on linux 2.6
	rfs_fill_inode(args->dir, inode, args->ep, args->index, 0, NULL, args->d_name);
}
#endif

/**
 * write a inode to dir entry
 * If data_commit is set, flush dirty data and update dir entry (size, 
 * start clu, mtime). Otherwise, update only dir entry (start clu, mtime)
 *
 * @param inode		inode
 * @param data_commit	flush dirty data and update dir entry's size
 * @param wait		flag whether inode info is flushed
 * @return		return 0 on success, errno on failure
 */
int rfs_sync_inode(struct inode *inode, int data_commit, int wait)
{
	struct super_block *sb = inode->i_sb;
	struct buffer_head *bh = NULL;
	struct rfs_dir_entry *ep = NULL;
	unsigned long backup_ino;
	int ret = 0;
	int committed = 0;

	// ESR Debugging 20090711
	int dir_per_block = sb->s_blocksize / sizeof(struct rfs_dir_entry);
	int dir_per_block_bits = ffs(dir_per_block) - 1;
	sector_t block_nr = 0;
	loff_t offset = 0;

	// unlinked inode. do not need to sync inode
	if (RFS_I(inode)->i_state == RFS_I_FREE)
		return 0;

	if (inode->i_ino == ROOT_INO)
#ifndef CONFIG_RFS_FS_XATTR
		return 0;
#else
	{
		/* write xattr_start and flag for root to boot_code in bpb */
		if (RFS_I(inode)->xattr_start_clus != CLU_TAIL &&
				RFS_I(inode)->xattr_start_clus != 0)
		{
			struct pbr *pbr_p;
			sector_t pbr_sector = 0;

			bh = rfs_bread(sb, pbr_sector, BH_RFS_MBR);

			if (!bh)
			{
				DEBUG(DL0, "fail to read reserved area");
				return 0;
			}
			pbr_p = (struct pbr *)bh->b_data;

			pbr_p->xattr_root_flag = RFS_XATTR_HEAD_SIGNATURE;
			SET32(pbr_p->xattr_start_clus, 
					RFS_I(inode)->xattr_start_clus);
			mark_buffer_dirty(bh);
			ret = rfs_sync_dirty_buffer(bh);
			if (ret)
			{
				RFS_BUG_CRASH(sb, "Fail to write\n");
			}

			brelse(bh);
		}
		return 0;
	}
#endif

	CHECK_RFS_INODE(inode, -EINVAL);

	if (IS_FAST_LOOKUP_INDEX(inode))
		return -EFAULT;

	DEBUG(DL3, "ino:%lu,i_size:%u,mode:0x%x", inode->i_ino, 
			(unsigned int)inode->i_size, inode->i_mode);

	/* write dirty data proir to inode in order to reduce locked area */
	if (!S_ISDIR(inode->i_mode) && data_commit) {
		DEBUG(DL3, "%u, %u, %u, %llu",
				RFS_I(inode)->p_start_clu,
				RFS_I(inode)->index,
				RFS_I(inode)->start_clu,
				inode->i_size);
			
		ret = rfs_sync_inode_pages(inode);
		if (!ret)
		{
			/*
			 * WARNING!!!
			 * If user data are written to device successfully,
			 * size field in directory entry can be updated.
			 * Otherwise, cluster chain are extended, but size 
			 * isn't updated.
			 * This causes inconsistency of size and cluster chain.
			 */
			committed = 1;
			DEBUG(DL3, "data_commit & committed");
		}
		else
		{
			DEBUG(DL0, "Writing data fails(%d)", ret);
		}
	}
	else
	{
		DEBUG(DL3, "metadata commit");
	}

	backup_ino = inode->i_ino;
	ep = get_entry_with_cluster(sb, RFS_I(inode)->p_start_clu, 
			RFS_I(inode)->index, &bh, inode);
	if (IS_ERR(ep)) {
		dump_inode(inode);
		RFS_BUG_CRASH(sb, "can't find entry: ino# %lu p_start# %u idx# "
				"%u start# %u last# %u offset# %lld\n",
				inode->i_ino,
				RFS_I(inode)->p_start_clu, RFS_I(inode)->index,
				RFS_I(inode)->start_clu, RFS_I(inode)->last_clu,
				(long long) RFS_I(inode)->offset);
		return -EIO;
	}

	/*
	 * synchronize the modification of directory entry with rfs_rename()
	 * : rfs_rename() changes i_ino, p_start_clu, index.
	 */
	spin_lock(&RFS_I(inode)->write_lock);

	/*
	 * confirm that inode is not changed by rename/unlink
	 * during getting entry
	 */
	if (backup_ino != inode->i_ino || RFS_I(inode)->i_state == RFS_I_FREE) 
	{
		spin_unlock(&RFS_I(inode)->write_lock);
		brelse(bh);
		return 0;
	}

	// ESR Debugging 20090711
	if (1)
	{
		block_nr = bh->b_blocknr;
		offset = ((loff_t)bh->b_blocknr << dir_per_block_bits) | 
			(ep - (struct rfs_dir_entry *)bh->b_data);

		if ((block_nr != RFS_I(inode)->block_nr) ||
				(offset != RFS_I(inode)->offset))
		{
			spin_unlock(&RFS_I(inode)->write_lock);

			dump_inode(inode);

			DEBUG(DL0, "entry: start# %u block_nr# %llu offset# "
					"%lld",
					START_CLUSTER(RFS_SB(sb), ep), 
					(unsigned long long)block_nr, 
					(long long)offset);
			dump_entry(sb, ep, bh, RFS_I(inode)->offset, "at %s:%d",
				       	__func__, __LINE__);
			RFS_BUG_CRASH(sb, "inode(ino %lu)'s position is "
					"corrupted", inode->i_ino);
			brelse(bh);

			return -EIO;
		}
	}

	/*
	 * if data are committed, update entry's size
	 * "committed" was setted after calling rfs_sync_inode_pages() 
	 */
	if (committed)
	{
		SET32(ep->size, inode->i_size);
	}
	DEBUG(DL3, "[%s]name:%s,ep->size:%u,i_size:%u, data_commit:%d, wait:%d",
			S_ISDIR(inode->i_mode)?"DIR":"FILE", ep->name, ep->size,
			(unsigned int)inode->i_size, data_commit, wait);

	/* update directory entry's start cluster number */
	if (RFS_I(inode)->start_clu != CLU_TAIL) 
	{
		/* sanity check : no case of overwriting non empty file */
		if ((START_CLUSTER(RFS_SB(sb), ep) != 0) &&
		    (START_CLUSTER(RFS_SB(sb), ep) != RFS_I(inode)->start_clu)) 
		{
			spin_unlock(&RFS_I(inode)->write_lock);
			dump_inode(inode);

			DEBUG(DL0, "entry: start# %u block_nr# %llu offset# "
					"%lld", START_CLUSTER(RFS_SB(sb), ep), 
					(unsigned long long)block_nr, 
					(long long)offset);
			dump_entry(sb, ep, bh, RFS_I(inode)->offset, "at %s:%d",
				       	__func__, __LINE__);

			RFS_BUG_CRASH(sb, "inode(%lu)'s start cluster is "
					"corrupted", inode->i_ino);

			brelse(bh);
			return -EIO;
		}
		SET_START_CLUSTER(ep, RFS_I(inode)->start_clu);
	}
	else 
	{
		SET_START_CLUSTER(ep, 0);
	}

	/* 
	 * Update directory entry's mtime and mdate into inode's mtime
	 * We mistook inode's mtime for current time at RFS-1.2.0 ver 
	 * to update directory entry's mtime and mdate
	 */
	rfs_set_entry_time(ep, inode->i_mtime);

#ifdef CONFIG_RFS_FS_XATTR
	/*
	 * set xattr exist flag (attr's 7th bit) and xattr's start cluster. 
	 */
	if (IS_XATTR_EXIST(RFS_I(inode)))
	{
		/* when inode is normal file and directory */
		DEBUG(DL1, "entry sync:name:%s,xattr_start:%u",
				ep->name, RFS_I(inode)->xattr_start_clus);
		ep->attr |= ATTR_XATTR;
		SET_XATTR_START_CLUSTER(ep, RFS_I(inode)->xattr_start_clus);
	}
#endif

	/*
	 * All changes are done. After unlock, create() after rename()
	 * can change buffer as it wants
	 */
	spin_unlock(&RFS_I(inode)->write_lock);

	/* make the buffer of directory entry dirty */
	if (wait) 
	{
		/* sync dirty buffer and confirm the result */
		int err;

		mark_buffer_dirty(bh);
		err = rfs_sync_dirty_buffer(bh);
		if (err) 
		{
			ret = (0 == ret)? err: ret;

			dump_inode(inode);
			dump_entry(sb, ep, bh, RFS_I(inode)->offset, "at %s:%d",
				       	__func__, __LINE__);
			RFS_BUG_CRASH(sb, "Fail to write(err:%d)\n", err);
		}
	}

	else
	{
		/* make buffer dirty */
		rfs_mark_buffer_dirty(bh, sb);
	}
	DEBUG(DL3, "ino:%lu, name:%s,ep->size:%u", inode->i_ino, ep->name, 
			GET32(ep->size));

	brelse(bh);
	return ret;
}

/* verify whether it is possible to write inode */
#define IS_NO_FSYNC(inode)		\
(((current->flags & PF_MEMALLOC) ||	\
  (IS_RDONLY(inode)) ||			\
  (RFS_I(inode)->i_state == RFS_I_FREE))? 1: 0)

/**
 * write inode 
 * @param inode		inode
 * @param wait		flag whether inode info is flushed
 * @return		return 0 on success, errno on failure for 2.6	
 *
 * We return 0 despite the read-only mode. see ext3_write_inode
 */
#ifdef RFS_FOR_2_6
int rfs_write_inode(struct inode *inode, int wait)
{
	if (IS_NO_FSYNC(inode))
		return 0;

	return rfs_sync_inode(inode, 1, wait);
}
#else
void rfs_write_inode(struct inode *inode, int wait)
{
	if (IS_NO_FSYNC(inode))
		return;

	lock_kernel();
	rfs_sync_inode(inode, 1, wait);
	unlock_kernel();

	return;
}
#endif


