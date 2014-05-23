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
 *  @file	fs/rfs/rfs_26.c
 *  @brief	Kernel version 2.6 specified functions (for superblock)
 *
 */

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/uio.h>
#include <linux/sched.h>
#include <linux/rfs_fs.h>
#include <linux/parser.h>
#include <linux/writeback.h>
#include <linux/statfs.h>



#include "rfs.h"
#include "log.h"


#ifdef CONFIG_GCOV_PROFILE
#undef loff_t
#endif


/*
 * handle mount option
 */

enum {
	opt_codepage, 
	opt_fcachesize,
#ifdef CONFIG_RFS_FS_XATTR
	opt_ea,
#endif
#ifdef CONFIG_RFS_VFAT
	opt_check_strict,
	opt_check_no,
#endif
	opt_err,

};

static match_table_t rfs_tokens = {
	{opt_codepage, "codepage=%s"},
	{opt_fcachesize, "fcache=%s"},
#ifdef CONFIG_RFS_FS_XATTR
	{opt_ea, "xattr"},
#endif
#ifdef CONFIG_RFS_VFAT
	{opt_check_strict, "check=strict"},
	{opt_check_no, "check=no"},
#endif
	{opt_err, NULL}
};


/**
 * parse mount options
 */
int get_parsed_option(char *options, struct rfs_mount_info *opts)
{
	substring_t args[MAX_OPT_ARGS];
	char *val;
	char *p;

	while ((p = strsep(&options, ",")) != NULL) {
		int token;
		if (!*p)
			continue;

		token = match_token(p, rfs_tokens, args);

		switch (token) {
		/* NLS codepage used in disk */
		case opt_codepage:
			val = match_strdup(&args[0]);
			if (!val)
				return -ENOENT;
			opts->codepage = val;
			break;
		case opt_fcachesize:
			val = match_strdup(&args[0]);
			if (!val)
				return -ENOENT;
			opts->str_fcache = val;
			break;
#ifdef CONFIG_RFS_FS_XATTR
		case opt_ea:
			set_opt(opts->opts, EA);
			break;
#endif
#ifdef CONFIG_RFS_VFAT
		case opt_check_no:
			set_opt(opts->opts, CHECK_NO);
		case opt_check_strict:
			set_opt(opts->opts, CHECK_STRICT);
			break;
#endif
		default:
			return -EINVAL;
		}
	}

	return 0;
}

/**
 *  Function to build super block structure
 *  @param sb		super block pointer
 *  @param data		pointer for an optional date
 *  @param silent	control flag for error message
 *  @return		zero on success, a negative error code on failure
 *
 *  Initialize the super block, system file such as logfile and recovery error by sudden power off
 */
static int rfs_fill_super(struct super_block *sb, void *data, int silent)
{
#ifndef _RFS_INTERNAL_MOUNT_OPT
	unsigned int used_clusters;
#endif
	int err = 0;

	sb = rfs_common_read_super(sb, data, silent);
	if (!sb)
		return -EINVAL;

#ifdef CONFIG_RFS_VFAT
	RFS_SB(sb)->options.isvfat = TRUE;	
#else
	if (IS_FAT32(RFS_SB(sb))) {
		DPRINTK("invalid fat type\n");
		return -EINVAL;
	}

	RFS_SB(sb)->options.isvfat = FALSE;

	sb->s_root->d_op = &rfs_dentry_operations;
#endif
	sb->s_flags |= MS_NOATIME | MS_NODIRATIME;

	RFS_SB(sb)->use_log = TRUE;

	if (rfs_log_init(sb)) {
		DPRINTK("RFS-log : Not supported[%02x:%02x]\n", 
				MAJOR(sb->s_dev), MINOR(sb->s_dev));
		return -EINVAL;
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
		return err;
	}
	RFS_SB(sb)->num_used_clusters = used_clusters;
#endif

#ifdef RFS_CLUSTER_CHANGE_NOTIFY
	/* initialize cluster_usage flag */
	set_cluster_usage_notify(sb, FALSE);
#endif
	DEBUG(DL0, "RFS successfully mounted [%02x:%02x]", MAJOR(sb->s_dev),
			MINOR(sb->s_dev));

	return err;
}

/* local variable definition */
#ifdef RFS_FOR_2_6_20
static struct kmem_cache *rfs_inode_cachep = NULL;
#else
static kmem_cache_t *rfs_inode_cachep = NULL;
#endif

/* static function definition */
/**
 * Function to initialized a newly create rfs specific inode structure
 * @param foo		memory pointer for new inode structure
 * @param cachep	a pointer for inode cache
 * @param flags		control flag
 */

#ifdef RFS_FOR_2_6_27
static void init_once(void * foo)
#elif defined (RFS_FOR_2_6_24)
static void init_once(struct kmem_cache * cachep, void * foo)
#elif defined (RFS_FOR_2_6_20)
static void init_once(void * foo, struct kmem_cache * cachep, unsigned long flags)
#else
static void init_once(void * foo, kmem_cache_t * cachep, unsigned long flags)
#endif
{
	struct rfs_inode_info *ei = (struct rfs_inode_info *) foo;

#ifndef RFS_FOR_2_6_22
	if ((flags & (SLAB_CTOR_VERIFY | SLAB_CTOR_CONSTRUCTOR)) ==
			SLAB_CTOR_CONSTRUCTOR)
#endif
		inode_init_once(&ei->vfs_inode);
}

/**
 * Function to initialize an inode cache 
 */
static int __init rfs_init_inodecache(void)
{
#ifdef RFS_FOR_2_6_24
	rfs_inode_cachep = kmem_cache_create("rfs_inode_cache",
						sizeof(struct rfs_inode_info),
						0, (SLAB_RECLAIM_ACCOUNT |
							SLAB_MEM_SPREAD),
						init_once);
#elif defined(RFS_FOR_2_6_23)
	rfs_inode_cachep = kmem_cache_create("rfs_inode_cache",
						sizeof(struct rfs_inode_info),
						0, SLAB_RECLAIM_ACCOUNT,
						init_once);
#else
	rfs_inode_cachep = kmem_cache_create("rfs_inode_cache",
						sizeof(struct rfs_inode_info),
						0, SLAB_RECLAIM_ACCOUNT,
						init_once, NULL);
#endif
	if (!rfs_inode_cachep)
		return -ENOMEM;

	return 0;
}

/**
 * Function to destroy an inode cache 
 */
static void rfs_destroy_inodecache(void)
{
	/*
	 * kmem_cache_destroy return type is changed
	 * from 'int' to 'void' after 2.6.19
	 */
	kmem_cache_destroy(rfs_inode_cachep);
}

/**
 *  Function to allocate rfs specific inode and associate it with vfs inode
 *  @param sb	super block pointer
 *  @return	a pointer of new inode on success, NULL on failure
 */
struct inode *rfs_alloc_inode(struct super_block *sb)
{
	struct rfs_inode_info *new;

	new = kmem_cache_alloc(rfs_inode_cachep, GFP_KERNEL);
	if (!new)
		return NULL;
		
	/* initialize rfs inode info, if necessary */
	new->i_state = RFS_I_ALLOC;

	return &new->vfs_inode; 
}

/**
 * Function to deallocate rfs specific inode 
 * @param inode	inode pointer
 */
void rfs_destroy_inode(struct inode *inode)
{
	if (!inode)
		DPRINTK("inode is NULL \n");

	kmem_cache_free(rfs_inode_cachep, RFS_I(inode));
}

/**
 * Interface function for super block initialization 
 * @param fs_type	filesystem type
 * @param flags		flag
 * @param dev_name	name of file system
 * @param data		private date
 * @return	a pointer of super block on success, negative error code on failure
 */
#ifdef RFS_FOR_2_6_18
static int rfs_get_sb(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data, struct vfsmount *mnt)
{
	return get_sb_bdev(fs_type, flags, dev_name, data, rfs_fill_super, mnt);
}
#else
static struct super_block *rfs_get_sb(struct file_system_type *fs_type,
				int flags, const char *dev_name, void *data)
{
	return get_sb_bdev(fs_type, flags, dev_name, data, rfs_fill_super);
}
#endif	/* RFS_FOR_2_6_18 */


/* rfs filesystem type defintion */
static struct file_system_type rfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "rfs",
	.get_sb		= rfs_get_sb,
	.kill_sb	= kill_block_super,
	.fs_flags	= FS_REQUIRES_DEV,
};

/**
 * Init function for rfs 
 */
static int __init init_rfs_fs(void)
{
	int err = 0;

	/* init inode cache */
	err = rfs_init_inodecache();	
	if (err)
		goto fail_init;

#ifdef CONFIG_PROC_FS
	err = init_rfs_proc();
	if (err)
		goto fail_after_init_inodecache;
#endif

#ifdef RFS_CLUSTER_CHANGE_NOTIFY
	/* initialize func poniter */
	gfp_cluster_usage_notify = NULL;
#endif

	err = register_filesystem(&rfs_fs_type);
	if (err)
		goto fail_after_init_proc;

	return 0;

fail_after_init_proc:
	exit_rfs_proc();
fail_after_init_inodecache:
	rfs_destroy_inodecache();
fail_init:
	return err;
}

/**
 * Exit function for rfs 
 */
static void __exit exit_rfs_fs(void)
{
#ifdef CONFIG_PROC_FS
	exit_rfs_proc();
#endif
	rfs_destroy_inodecache();
	unregister_filesystem(&rfs_fs_type);
}

module_init(init_rfs_fs);
module_exit(exit_rfs_fs);

MODULE_LICENSE("Samsung, Proprietary");
