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
 *  @file	fs/rfs/dir.c
 *  @brief	directory handling functions 
 *
 *
 */

#include <linux/fs.h>
#include <linux/rfs_fs.h>

#include "rfs.h"

#ifdef CONFIG_GCOV_PROFILE
#define	loff_t		off_t
#endif

/* internal data structure for readdir */
struct rfs_dir_info {
	unsigned int type;
	unsigned long ino;
	unsigned char name[NAME_MAX + 1];
	int len;
};

/**
 *  counts the number of sub-directories in specified directory 
 * @param sb	super block
 * @param clu	start cluster number of specified directory to count
 * @return	return the number of sub-directories on sucess, errno on failure
 */
int rfs_count_subdir(struct super_block *sb, unsigned int clu)
{
	struct buffer_head *bh = NULL;
	struct rfs_dir_entry *ep = NULL;
	unsigned int cpos = 0;
	unsigned int type;
	int count = 0, err = 0;

	while (1) { 
		ep = get_entry_with_cluster(sb, clu, cpos++, &bh, NULL);
		if (IS_ERR(ep)) {
			err = PTR_ERR(ep);
			if (err == -EFAULT) /* end of cluster */
				break;
			brelse(bh);
			return err;
		}

		/* check type of dir entry */
		type = rfs_entry_type(ep);
		if (type == TYPE_DIR)
			count++;
		else if (type == TYPE_UNUSED)
			break;
	}

	brelse(bh);
	return count;
}

/**
 *  read dir entry in specified directory
 * @param inode		specified directory inode
 * @param bh		buffer head to read dir entries
 * @param ppos		entry position to read
 * @param[out] dir_info	to save dir entry info
 * @return		return 0 on success, errno on failure
 */
static int __internal_readdir(struct inode *inode, struct buffer_head **bh, loff_t *ppos, struct rfs_dir_info *dir_info)
{
#ifdef CONFIG_RFS_VFAT
	unsigned short uname[UNICODE_NAME_LENGTH];
#endif
	struct rfs_dir_entry *ep = NULL;
	loff_t index = *ppos;
	unsigned long ino;
	unsigned int type;
	int len;
	int ret = 0;

	while (1) {
		ep = get_entry(inode, (u32) index, bh);
		if (IS_ERR(ep)) 
			return PTR_ERR(ep);

		index++;

		type = rfs_entry_type(ep);

		/* for special file */
		if ((type == TYPE_FILE) && IS_SYMLINK(ep))
			type = TYPE_SYMLINK;
		else if ((type == TYPE_FILE) && IS_SOCKET(ep))
			type = TYPE_SOCKET;
		else if ((type == TYPE_FILE) && IS_FIFO(ep))
			type = TYPE_FIFO;


		dir_info->type = type;

		if (type == TYPE_UNUSED) 
			return -INTERNAL_EOF; /* not error case */

		if ((type == TYPE_DELETED) || (type == TYPE_EXTEND) || 
				(type == TYPE_VOLUME))
			continue;

#ifdef CONFIG_RFS_VFAT
		uname[0] = 0x0;
		rfs_get_uname_from_entry(inode, index - 1, uname);
		if (uname[0] != 0x0 && IS_VFAT(RFS_SB(inode->i_sb))) 
			len = rfs_convert_uname_to_cstring(dir_info->name, uname, RFS_SB(inode->i_sb)->nls_disk);
		else
#endif
			len = rfs_convert_dosname_to_cstring(dir_info->name, ep->name, ep->sysid);	

		ret = rfs_iunique(inode, (unsigned int) (index - 1), &ino);
		if (ret)
			return ret;
		dir_info->ino = ino;
		dir_info->len = len;

		*ppos = index;
		break;
	}

	return 0;
}

/**
 *  read all dir entries in specified directory
 * @param filp		file pointer of specified directory to read	
 * @param dirent	buffer pointer
 * @param filldir	function pointer which fills dir info
 * @return		return 0 on success, errno on failure
 */
static int rfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct dentry *dentry = filp->f_dentry;
	struct inode *inode = dentry->d_inode;
	struct buffer_head *bh = NULL;
	struct rfs_dir_info dir_info;
	unsigned int type;
	loff_t pos;
	int ret;

	CHECK_RFS_INODE(inode, -ENOENT);

	while (1) {
		pos = filp->f_pos;
	
		ret = __internal_readdir(inode, &bh, &filp->f_pos, &dir_info);
		if (ret < 0) 
			break;

		if (dir_info.type == TYPE_DIR)
			type = DT_DIR;
		else if (dir_info.type == TYPE_SYMLINK)
			type = DT_LNK;
		else if (dir_info.type == TYPE_SOCKET)
			type = DT_SOCK;
		else if (dir_info.type == TYPE_FIFO)
			type = DT_FIFO;
		else
			type = DT_REG;

		ret = filldir(dirent, dir_info.name, dir_info.len, 
				pos, dir_info.ino, type);	
		if (ret < 0) {
			filp->f_pos = pos; /* rollback */
			break;
		}
	}
	
	brelse(bh);
	return 0;
}

/**
 *  dummy function for .fsync vector of rfs_dir_operations
 * @param file		file pointer
 * @param dentry	dentry pointer
 * @param datasync	flag
 * @return return 0 on success.
 *
 * Thus RFS no need of sync directory, Older version of RFS does not connect 
 * ".fsync". in this case VFS returns "-EINVAL".
 * However this can be a cause of problem in some application like 
 * "market download" of android.
 * To avoid this RFS connect dummy function to ".fsync" vector in directory
 * inode operation.
 *
 */
static int rfs_dir_fsync(struct file * file, struct dentry *dentry, int datasync)
{
	/* do nothing */
	return 0;
}

struct file_operations rfs_dir_operations = {
	.read		= generic_read_dir,
	.readdir	= rfs_readdir,
	.fsync		= rfs_dir_fsync,
};
