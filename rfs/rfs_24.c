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
 *  @file	fs/rfs/rfs_24.c
 *  @brief	Kernel version 2.4 specified functions for superblock
 *
 *
 */

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/rfs_fs.h>

#include "rfs.h"
#include "log.h"

/**
 * parse mount options
 */
int get_parsed_option(char *options, struct rfs_mount_info *opts)
{
	char *val;
	char *p;

	p = strtok(options, ",");
	if (p == NULL)
		return 0;

	do {
		if (!strncmp(p, "codepage=", 9)) 
		{
			val = strchr(p, '=');
			if (!val)
				return -ENOENT;
			opts->codepage = val + 1;
		}
		else if (!strncmp(p, "fcache=", 7))
		{
			val = strchr(p, '=');
			if (!val)
				return -ENOENT;
			opts->str_fcache = val + 1;
		}
	       	else 
		{
			return -EINVAL;
		}
	} while((p = strtok(NULL, ",")) != NULL);

	return 0;
}


/**
 * wakeup function of logging process
 * @param __data	: super block
 */
void rfs_log_wakeup(unsigned long __data)
{
	struct super_block *sb = (struct super_block *) __data;
	struct task_struct *p = RFS_SB(sb)->sleep_proc;

	mod_timer(&RFS_SB(sb)->timer, DATA_EXPIRES(jiffies));
	if (p->state == TASK_UNINTERRUPTIBLE)
		wake_up_process(p);
}

/**
 * wakeup function for a process committing data
 * @param __data	: private inode
 */
void rfs_data_wakeup(unsigned long __data)
{
	struct rfs_inode_info *rfsi = (struct rfs_inode_info *) __data;
	struct task_struct *p = rfsi->sleep_proc;

	mod_timer(&rfsi->timer, DATA_EXPIRES(jiffies));
	if (p->state == TASK_UNINTERRUPTIBLE)
		wake_up_process(p);
}

/**
 *  build the super block
 * @param sb		super block
 * @param data		private data 
 * @param silent	verbose flag
 * @return		return super block pointer on success, null on failure
 *
 * initialize the super block, system file such as logfile and recovery error by sudden power off
 */
static struct super_block *rfs_read_super(struct super_block *sb, void *data, int silent)
{
	struct super_block *res = NULL;
	unsigned int used_clusters;
	int err;

	res = rfs_common_read_super(sb, data, silent);
	if (!res) 
		return NULL;
#ifdef CONFIG_RFS_VFAT
	RFS_SB(sb)->options.isvfat = TRUE;	
#else
	if (IS_FAT32(RFS_SB(res))) {
		DPRINTK("invalid fat type\n");
		return NULL;
	}

	sb->s_root->d_op = &rfs_dentry_operations;
	RFS_SB(sb)->options.isvfat = FALSE;
#endif
	sb->s_flags |= MS_NOATIME | MS_NODIRATIME;

	RFS_SB(sb)->use_log = TRUE;

	if (rfs_log_init(sb)) 
	{
		DPRINTK("RFS-log : Not supported\n");
		return NULL;
	}

#if defined(_RFS_INTERNAL_SANITY_CHECK_MOUNT_LOG) || defined(_RFS_INTERNAL_SANITY_CHECK_LOG)
	err = sanity_check_log(sb);
	if (err && !(sb->s_flags & MS_RDONLY))
	{
		DPRINTK("RFS-log : corrupted\n");
		return -EINVAL;
	}
#endif
#ifdef _RFS_INTERNAL_MOUNT_OPT
	/* set invalid value */
	RFS_SB(sb)->num_used_clusters = 0xFFFFFFFF;
#else
	/* update total number of used clusters */
	err = rfs_count_used_clusters(sb, &used_clusters);
	if (err) { /* I/O error */
		DPRINTK("FAT has something wrong\n");
		return NULL;
	}
	RFS_SB(sb)->num_used_clusters = used_clusters;
#endif

#ifdef RFS_CLUSTER_CHANGE_NOTIFY
	/* initialize cluster_usage flag */
	set_cluster_usage_notify(sb, FALSE);
#endif
	DEBUG(DL0, "RFS successfully mounted [%02x:%02x]", MAJOR(sb->s_dev),
			MINOR(sb->s_dev));

	return res;
}

static DECLARE_FSTYPE_DEV(rfs_fs_type, "rfs", rfs_read_super);

/**
 *  register RFS file system
 */
static int __init init_rfs_fs(void)
{
#ifdef RFS_CLUSTER_CHANGE_NOTIFY
	/* initialize func poniter */
	gfp_cluster_usage_notify = NULL;
#endif

	return register_filesystem(&rfs_fs_type);
}

/**
 *  unregister RFS file system
 */
static void __exit exit_rfs_fs(void)
{
	unregister_filesystem(&rfs_fs_type);
}

module_init(init_rfs_fs);
module_exit(exit_rfs_fs);

MODULE_LICENSE("Samsung, Proprietary");
