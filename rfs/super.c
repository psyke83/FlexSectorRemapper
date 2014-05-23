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
 *  @file	fs/rfs/super.c
 *  @brief	super block and init functions
 *
 *
 */

#include <linux/fs.h>
#include <linux/bitops.h>
#include <linux/blkdev.h>
#include <linux/rfs_fs.h>
#include <linux/nls.h>
#include <linux/mount.h>
#include <linux/seq_file.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#include <linux/statfs.h>
#endif

#include "rfs.h"
#include "log.h"
#ifdef CONFIG_RFS_FS_XATTR
#include "xattr.h"
#endif


int get_parsed_option(char *options, struct rfs_mount_info *opts);

#ifdef RFS_FOR_2_6
struct inode *rfs_alloc_inode (struct super_block *);
void rfs_destroy_inode (struct inode *);
#endif /* RFS_FOR_2_6 */

#ifdef CONFIG_RFS_NLS
static const char rfs_default_codepage[] = CONFIG_RFS_DEFAULT_CODEPAGE;
#endif

/**
 *  write the super block especially commit the previous transaction and flush the fat cache 
 * @param sb	super block
 */
static void rfs_write_super(struct super_block *sb)
{
	if ((sb->s_flags & MS_RDONLY)) 
		return;

	rfs_log_force_commit(sb, NULL);

	/* sb->s_dirt was cleared in rfs_log_force_commit() */
}

/**
 *  get statistics on a file system 
 * @param sb	super block
 * @param stat	structure to fill stat info
 * @return	return 0
 */
#ifdef RFS_FOR_2_6_18
static int rfs_statfs(struct dentry *dentry, struct kstatfs *stat)
#elif RFS_FOR_2_6
static int rfs_statfs(struct super_block *sb, struct kstatfs *stat)
#else
static int rfs_statfs(struct super_block *sb, struct statfs *stat)
#endif
{
#ifdef RFS_FOR_2_6_18
	struct super_block *sb = dentry->d_sb;
#endif
	
	stat->f_type = RFS_MAGIC;
	stat->f_bsize = (long) RFS_SB(sb)->cluster_size;
	stat->f_blocks = (long) RFS_SB(sb)->num_clusters;
	stat->f_bfree =  (long) GET_FREE_CLUS(sb);
	stat->f_bavail = stat->f_bfree;
	stat->f_namelen = MAX_NAME_LENGTH;

	return 0;
}

/**
 * allow to remount to make a writable file system readonly
 * @param sb	super block
 * @param flags	to chang the mount flags
 * @param data	private data
 * @return	return 0
 */
static int rfs_remount(struct super_block *sb, int *flags, char *data)
{
	int ret = 0;

	/* rw -> ro , ro -> rw */
	if ((*flags & MS_RDONLY) != (int) (sb->s_flags & MS_RDONLY)) {
		/* mount read-write partition RDONLY */ 
		if (*flags & MS_RDONLY) {

			/* commit transaction */
			if (RFS_SB(sb)->use_log == TRUE) {
				ret = rfs_log_force_commit(sb, NULL);
				if (ret < 0) {
					DPRINTK("RFS-log : Commit "
							"fails(%d)\n", ret);

					return -EBUSY;
				}

				RFS_SB(sb)->use_log = FALSE;
			} /* end of commit */

			/* change flag */
			sb->s_flags |= MS_RDONLY;

		} else {
		/* mount RDONLY partition read-write */

			/* change flag */
			sb->s_flags &= ~MS_RDONLY;

			/* initialize logfile */
			if (RFS_SB(sb)->use_log == FALSE) {

				RFS_SB(sb)->use_log = TRUE;
				ret = rfs_log_init(sb);
				if (ret < 0) {
					RFS_SB(sb)->use_log = FALSE;
					*flags |= MS_RDONLY;

					DPRINTK("RFS-log : Not "
							"supported(%d)\n", ret);
					return -EINVAL;

				}
			} /* end of init log */
		}
	}

	/*
	 * sb->s_flags is re-set by caller (do_remount_sb).
	 * After 2.6.15,
	 * MS_NOATIME and MS_NODIRATIME of remount flag
	 * can't be reflected in new s_flags.
	 */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 15)
	*flags |= (MS_NOATIME | MS_NODIRATIME);
#else
	sb->s_flags |= (MS_NOATIME | MS_NODIRATIME);
#endif

	return 0;
}

/**
 *  release the super block
 * @param sb	super block
 * 
 * release log, internal fat cache
 */

static void rfs_put_super(struct super_block *sb)
{
#ifdef RFS_FOR_2_6
	struct rfs_sb_info *sbi = RFS_SB(sb);
#endif

	/*
	 * release loginfo
	 * It precedes rfs_fcache_release
	 * because it eventually calls rfs_fcache_sync
	 */
	rfs_log_release(sb);

	/* release fcache */
	rfs_fcache_release(sb);

	/* release fat lock */
	kfree(RFS_SB(sb)->fat_mutex);

#ifdef RFS_FOR_2_6
	if (!sbi) {
		RFS_BUG_CRASH(sb, "rfs-specific sb is corrrupted\n");
		return;
	}

	/* release filesystem specific superblock */		
	sb->s_fs_info = NULL;
	kfree(sbi);
#endif

	return;
}

/**
 * show mount options
 * @param seq	seq file pointer
 * @param vfs	vfsmount pointer 
 * @return return 0
 */
static int rfs_show_options(struct seq_file *seq, struct vfsmount *vfs)
{
	struct super_block *sb = vfs->mnt_sb;
	
	if (RFS_SB(sb)->options.codepage)
		seq_printf(seq, ",codepage=%s", RFS_SB(sb)->options.codepage);
	if (RFS_SB(sb)->options.isvfat)
		seq_puts(seq, ",vfat");

	seq_printf(seq, ",fcache(blks)=%u", RFS_SB(sb)->fcache_size);

#ifdef	CONFIG_RFS_FS_XATTR
	if (test_opt(sb, EA))
		seq_puts(seq, ",xattr");
#endif
#ifdef CONFIG_RFS_VFAT
	if (test_opt(sb, CHECK_NO))
		seq_puts(seq, ",check=no");
	else if (test_opt(sb, CHECK_STRICT))
		seq_puts(seq, ",check=strict");
#endif
	return 0;
}

static struct super_operations rfs_sops = {
#ifdef RFS_FOR_2_6
	.alloc_inode	= rfs_alloc_inode,
	.destroy_inode  = rfs_destroy_inode,
#endif
	.show_options	= rfs_show_options,
#ifdef CONFIG_RFS_IGET4
	.read_inode2	= rfs_read_inode2,
#endif
	.write_inode	= rfs_write_inode,
	.delete_inode	= rfs_delete_inode,
	.put_super	= rfs_put_super,
	.write_super	= rfs_write_super,
	.statfs		= rfs_statfs,
	.remount_fs	= rfs_remount,
};

/**
 * parse the mount options
 * @param sb		super block
 * @param options	mount options 
 * @return	zero on success
 */
static int __parse_options(struct super_block *sb, char *options)
{
	struct rfs_mount_info *opts = &(RFS_SB(sb)->options);
	int ret = 0;

	opts->codepage = NULL;
	opts->str_fcache = NULL;
 
	if (options) {
		ret = get_parsed_option(options, opts);
		if (ret)
			return ret;
	}

#ifdef CONFIG_RFS_NLS
	if (opts->codepage == NULL) {
		if (strcmp(rfs_default_codepage, "")) {
			opts->codepage = (char *) rfs_default_codepage;
			DEBUG(DL1, "Use default codepage %s\n", opts->codepage);
		} else {
			DPRINTK("If you configure the NLS, you must select codepage\n");
			return -EINVAL;
		}
	}
#endif
	return 0;
}

/**
 *  fill up the RFS-specific super block
 * @param sb			super block
 * @param old_blksize		original block size of block device
 * @param[out] new_blksize	new block size of the file system will be set 
 * @return			return 0 on success, errno on failure
 *
 * choose a minimum value between cluster size and block size of block device
 */
static int rfs_build_sb(struct super_block *sb, unsigned int old_blksize, unsigned int *new_blksize)
{
	struct buffer_head *bh;
	struct pbr *pbr_p;
	struct bpb *bpb_p;
	sector_t pbr_sector = 0;
	unsigned short sector_size, sector_bits; 
	unsigned int num_sectors, num_reserved, fat_sectors, root_sectors;
	unsigned int fat_start_sector, root_start_sector, data_start_sector;
	unsigned int num_root_entries = MAX_ROOT_DENTRY, root_clu = 0;
	unsigned int sectors_per_blk, sectors_per_blk_bits;
	unsigned int num_blks, block_size;
	loff_t device_size = sb->s_bdev->bd_inode->i_size;

	/* get PBR sector */
	bh = rfs_bread(sb, pbr_sector, BH_RFS_MBR);
	if (!bh) { /* I/O error */
		DPRINTK("unable to read the boot sector(%d) \n", 
				(unsigned int) pbr_sector);
		return -EIO;
	}

	pbr_p = (struct pbr *) bh->b_data;
	if ((u16) SIGNATURE != GET16(pbr_p->signature)) 
	{
#ifndef CONFIG_RFS_FAT_DEBUG
		DPRINTK("invalid boot sector signature (%x != %x)\n",
				SIGNATURE, GET16(pbr_p->signature));
		brelse(bh);
		return -EINVAL;
#else
		 /* check crash mark */
		if ((u16) RFS_CRASH_MARK != GET16(pbr_p->signature))
		{
			DPRINTK("invalid boot sector signature (%x != %x)\n",
					SIGNATURE, GET16(pbr_p->signature));
			brelse(bh);
			return -EINVAL;
		}
		DPRINTK("RFS-fs : volume is broken. Try to mount filesystem "
				"read-only\n");
		sb->s_flags |= MS_RDONLY;
#endif
	}

	/* fill private info of sb */
	bpb_p = (struct bpb *) pbr_p->bpb;

	/* get logical sector size */
	sector_size = GET16(bpb_p->sector_size); 
	if ((!sector_size) || ((u32) sector_size > PAGE_CACHE_SIZE)) {
		DPRINTK("invalid logical sector size : %d\n", sector_size);
		brelse(bh);
		return -EINVAL;
	}
	sector_bits = (unsigned short) ffs((int) sector_size) - 1;

	/* get reserved, fat, root sectors */
	num_reserved = GET16(bpb_p->num_reserved);
	fat_sectors = GET16(bpb_p->num_fat_sectors);
	root_sectors = (unsigned int) GET16(bpb_p->num_root_entries) 
					<< DENTRY_SIZE_BITS;
	root_sectors = ((root_sectors - 1) >> sector_bits) + 1;
	if ((!fat_sectors) && (!GET16(bpb_p->num_root_entries))) {
		/* when fat32 */
		RFS_SB(sb)->fat_bits = FAT32;
		fat_sectors = GET32(bpb_p->num_fat32_sectors);
		root_clu = GET32(bpb_p->root_cluster);
		root_sectors = 0;
		num_root_entries = 0;
	}

	/* get each area's start sector number */
	fat_start_sector = (unsigned int) (pbr_sector + num_reserved);
	root_start_sector = fat_start_sector + fat_sectors * bpb_p->num_fats; 
	data_start_sector = root_start_sector + root_sectors;

	/* get total number of sectors on volume */
	num_sectors = GET16(bpb_p->num_sectors);
	if (!num_sectors) 
		num_sectors = GET32(bpb_p->num_huge_sectors);
	/* check whether it is bigger than device size or it is not available */
	if ((!num_sectors) || (((unsigned long long) num_sectors << sector_bits)
				> (unsigned long long)device_size)) {
		DPRINTK("invalid number of sectors : %u\n", num_sectors);
		brelse(bh);
		return -EINVAL;
	}

	/* set cluster size */
	RFS_SB(sb)->cluster_size = 
		(unsigned int) bpb_p->sectors_per_clu << sector_bits;
	RFS_SB(sb)->cluster_bits = 
		(unsigned int) ffs(RFS_SB(sb)->cluster_size) - 1;

	/* get new block size */
	if (old_blksize > RFS_SB(sb)->cluster_size)
		block_size = RFS_SB(sb)->cluster_size;
	else
		block_size = old_blksize;

	/* 
	 * block size is sector size if block device's block size is not set,
	 * logical sector size is bigger than block size to set, 
	 * or start sector of data area is not aligned 
	 */
	if ((!block_size) || (sector_size > block_size) ||
			(((unsigned long long) data_start_sector << sector_bits)
			 & (block_size - 1)))
		block_size = sector_size;

	sectors_per_blk = block_size >> sector_bits;
	sectors_per_blk_bits = (unsigned int) ffs(sectors_per_blk) - 1;

	/* set number of blocks per cluster */ 
	RFS_SB(sb)->blks_per_clu = (unsigned int) bpb_p->sectors_per_clu >> 
					sectors_per_blk_bits;
	RFS_SB(sb)->blks_per_clu_bits = 
		(unsigned int) ffs(RFS_SB(sb)->blks_per_clu) - 1;

	/* set start address of fat table area */
	RFS_SB(sb)->fat_start_addr = 
		((unsigned long long) fat_start_sector) << sector_bits;

	/* set start address of root directory */
	RFS_SB(sb)->root_start_addr = 
		((unsigned long long) root_start_sector) << sector_bits;
	/* 
	 * NOTE: although total dir entries in root dir are bigger than 512 
	 * RFS only used 512 entries in root dir
	 */
	RFS_SB(sb)->root_end_addr = RFS_SB(sb)->root_start_addr + 
				(num_root_entries << DENTRY_SIZE_BITS);

	/* set start block number of data area */
	RFS_SB(sb)->data_start = data_start_sector >> sectors_per_blk_bits; 

	/* set total number of clusters */
	num_blks = (num_sectors - data_start_sector) >> sectors_per_blk_bits;
	RFS_SB(sb)->num_clusters = (num_blks >> RFS_SB(sb)->blks_per_clu_bits) 
					+ 2; /* clu 0 & clu 1 */

	/* set fat type */
	if (!RFS_SB(sb)->fat_bits) {
		if ((RFS_SB(sb)->num_clusters >= FAT12_THRESHOLD) &&
			(RFS_SB(sb)->num_clusters < FAT16_THRESHOLD))
			RFS_SB(sb)->fat_bits = FAT16;
		else {
			DPRINTK("invalid fat type\n");
			brelse(bh);
			return -EINVAL;
		}
	}

	/* set root dir's first cluster number, etc. */
	RFS_SB(sb)->root_clu = root_clu;
	RFS_SB(sb)->search_ptr = VALID_CLU; /* clu 0 & 1 are reserved */

	/* release buffer head contains pbr sector */
	brelse(bh);
		
	/* init semaphore for fat table */
	RFS_SB(sb)->fat_mutex = rfs_kmalloc(sizeof(struct rfs_semaphore), 
					GFP_KERNEL, NORETRY);  
	if (!(RFS_SB(sb)->fat_mutex)) { /* memory error */
		DEBUG(DL0, "memory allocation failed");
		brelse(bh);
		return -ENOMEM;
	}

	init_fat_lock(sb);

	/* init list for map destroy */
	INIT_LIST_HEAD(&RFS_SB(sb)->free_chunks);
	RFS_SB(sb)->nr_free_chunk = 0;

	/* new block size of block device will be set */
	*new_blksize = block_size;

	/* NLS support */
#ifdef CONFIG_RFS_NLS
	if (RFS_SB(sb)->options.codepage) {
		RFS_SB(sb)->nls_disk = load_nls(RFS_SB(sb)->options.codepage);
		if (!RFS_SB(sb)->nls_disk) {
			DPRINTK("RFS: %s not found\n", RFS_SB(sb)->options.codepage);
			return -EINVAL;
		}
	}
#endif

#ifdef RFS_FOR_2_4
	init_timer(&RFS_SB(sb)->timer);
	RFS_SB(sb)->timer.function = rfs_log_wakeup;
	RFS_SB(sb)->timer.data = (unsigned long) sb;
#endif
	RFS_SB(sb)->highest_d_ino = (num_sectors << sector_bits) >> DENTRY_SIZE_BITS;

	return 0;
}

/**
 *  fill up the root inode
 * @param inode	root inode
 * @return return 0 on success, errno on failure
 */
static int fill_root_inode(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	unsigned int last_clu;
	int err = 0, num_clusters = 0;

	inode->i_ino = ROOT_INO;
	inode->i_mode = S_IFDIR | 0777;
	inode->i_uid = 0;
	inode->i_gid = 0;

	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;

#ifndef RFS_FOR_2_6_19
	/* This is the optimal IO size (for stat), not the fs block size */
	inode->i_blksize = sb->s_blocksize;
#endif
	inode->i_version = 0;

	insert_inode_hash(inode);

	inode->i_op = (test_opt(sb, EA)) ? &rfs_dir_inode_operations_xattr :
		&rfs_dir_inode_operations ;
	inode->i_fop = &rfs_dir_operations;

	RFS_I(inode)->start_clu = RFS_SB(sb)->root_clu;
	RFS_I(inode)->p_start_clu = RFS_SB(sb)->root_clu;
	RFS_I(inode)->index = 0;

#ifdef CONFIG_RFS_FAST_SEEK
	RFS_I(inode)->fast_seek = NULL;
#endif

	if (!IS_FAT32(RFS_SB(sb))) {
		inode->i_size = RFS_SB(sb)->cluster_size;
	} else {
		num_clusters = rfs_find_last_cluster(inode, 
				RFS_I(inode)->start_clu, &last_clu, 0, 0, NULL);
		if (num_clusters <= 0) {
			err = num_clusters;
			DPRINTK("No last cluster (err : %d)\n", err);
			return -EIO;
		}
		inode->i_size = 
			((loff_t) num_clusters) << RFS_SB(sb)->cluster_bits;
		/* update last cluster */
		RFS_I(inode)->last_clu = last_clu;
	}

	inode->i_nlink = (nlink_t) rfs_count_subdir(sb, RFS_I(inode)->start_clu);
	inode->i_blocks = 
		(blkcnt_t) ((inode->i_size + SECTOR_SIZE - 1) >> SECTOR_BITS);

	set_mmu_private(inode, inode->i_size);

	/*
	 * At unlink, child gets parent inode for race condition between
	 * write_inode(child) and rmdir parent
	 */
	RFS_I(inode)->parent_inode = NULL;

	spin_lock_init(&RFS_I(inode)->write_lock);
#ifdef RFS_FOR_2_4
	init_MUTEX(&RFS_I(inode)->data_mutex);
	init_timer(&RFS_I(inode)->timer);
	RFS_I(inode)->timer.function = rfs_data_wakeup;
	RFS_I(inode)->timer.data = (unsigned long) RFS_I(inode);
#endif
#ifdef CONFIG_RFS_FS_XATTR
	/* read xattr_start for root from reserved area */
	{
		unsigned int xattr_start_clus = CLU_TAIL; 
		unsigned int last_clus = CLU_TAIL;
		unsigned int num_clus = 0;

		struct buffer_head *bh = NULL;
		struct pbr *pbr_p;
		unsigned int pbr_sector = 0;

		bh = rfs_bread(sb, pbr_sector, BH_RFS_MBR);
		if (!bh)
		{
			DEBUG(DL0, "fail to read reserved area");
			return 0;
		}

		pbr_p = (struct pbr *) bh->b_data;

		/* read xattr_start_clus only when 
		   root_flag equal to RFS_XATTR_HEAD_SIGNATURE */

		if (RFS_XATTR_HEAD_SIGNATURE == pbr_p->xattr_root_flag &&
				test_opt(sb, EA))
		{
			xattr_start_clus = 
				GET32(pbr_p->xattr_start_clus);
		
			num_clus = rfs_find_last_cluster(inode, xattr_start_clus,
					&last_clus, 0, 0, NULL);
		}

		/* init xattr allocation related variable */
		RFS_I(inode)->xattr_start_clus = xattr_start_clus;
		RFS_I(inode)->xattr_last_clus = last_clus;
		RFS_I(inode)->xattr_numof_clus = num_clus;

		if (CLU_TAIL != xattr_start_clus)
		{
			/* read xattr related etc. variable
			   total_space, used_space, valid_count,
			   uid, gid, mode,
			   ctime, cdate */

			err = rfs_xattr_read_header_to_inode(inode, FALSE);
			if (err)
			{
				DEBUG(DL0, "fail to rfs_xattr_read_header_to_"
						"inode(%d)", err);
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
			RFS_I(inode)->xattr_ctime = 0;
			RFS_I(inode)->xattr_cdate = 0;
		}

		init_rwsem(&RFS_I(inode)->xattr_sem);

		DEBUG(DL3, "[root's xattr]start:%u,last:%u,mmu_private:%u", 
					RFS_I(inode)->xattr_start_clus,
					RFS_I(inode)->xattr_last_clus,
					RFS_I(inode)->xattr_numof_clus);
out:
		brelse(bh);
		DEBUG(DL3, "blocksize:%lu", sb->s_blocksize);
	}
#endif
	return 0;
}

/**
 *  read the super block (disk such as MBR) of RFS and initialize super block (incore)
 * @param sb		super block
 * @param data		private data
 * @param silent	verbose flag
 * @return		return super block pointer on success, null on failure
 */
struct super_block *rfs_common_read_super(struct super_block *sb,
					void *data, int silent)
{
	struct inode *root_inode = NULL;
	unsigned int new_blksize, old_blksize;
	int err;

#ifdef RFS_FOR_2_6
	struct rfs_sb_info *sbi;

	sbi = rfs_kmalloc(sizeof(struct rfs_sb_info), GFP_KERNEL, NORETRY);
	if (!sbi) /* memory error */
		goto failed_mount;

	/* initialize sbi with 0x00 */
	/* log_info must be initialized with 0 */
	memset(sbi, 0x00, sizeof(struct rfs_sb_info));
	sb->s_fs_info = sbi;

	old_blksize = block_size(sb->s_bdev);
#else
	old_blksize = block_size(sb->s_dev);
#endif
	sb_min_blocksize(sb, 512);

	/* parsing mount options */
	if (__parse_options(sb, data) < 0)
		goto failed_mount;

	/* fill the RFS-specific info of sb */
	if (rfs_build_sb(sb, old_blksize, &new_blksize) < 0)
		goto failed_mount;

	/* setup the rest superblock info */
	if (!sb_set_blocksize(sb, (int) new_blksize)) {
		DPRINTK("unable to set blocksize\n");
		goto failed_mount;
	}

	sb->s_maxbytes = 0xFFFFFFFF; /* maximum file size */
	sb->s_op = &rfs_sops;
#ifdef CONFIG_RFS_FS_XATTR
	if (test_opt(sb, EA))
		sb->s_xattr = rfs_xattr_handlers;
#endif
	sb->s_magic = RFS_MAGIC;
	sb->s_dirt = 0;

	RFS_SB(sb)->fcache_array = NULL;
	if (rfs_fcache_init(sb)) { /* memory error */
		DPRINTK("unable to init fat cache\n");
		goto release_fcache;
	}

	/* allocate root inode & fill it */
	if (!(root_inode = NEW_INODE(sb)))
		goto release_fcache;

	err = fill_root_inode(root_inode); 
	if (err) {
		iput(root_inode);
		goto release_fcache;
	}

	if (!(sb->s_root = d_alloc_root(root_inode))) {
		iput(root_inode);
		goto release_fcache;
	}

	RFS_SB(sb)->root_inode = root_inode;

	return sb;

release_fcache:
	/* release fcache */
	if (RFS_SB(sb)->fcache_array)
		kfree(RFS_SB(sb)->fcache_array);
failed_mount:
	if (RFS_SB(sb)->fat_mutex)
		kfree(RFS_SB(sb)->fat_mutex);
#ifdef RFS_FOR_2_6
	if (sbi)
		kfree(sbi);
#endif	

	return NULL;
}

/**
 *  flush all dirty buffers of the file system include fat cache
 * @param sb 	super block
 * @return	return 0
 */
int rfs_sync_vol(struct super_block *sb)
{
	int err;

	/* fat cache sync */
	fat_lock(sb);
	rfs_fcache_sync(sb, 0);
	fat_unlock(sb);

	/* fat cache is dirty without waiting flush. So sync device */
#ifdef RFS_FOR_2_6
	err = sync_blockdev(sb->s_bdev);
#else	
	err = fsync_no_super(sb->s_dev);
#endif	
	return err;
}


#if defined(CONFIG_RFS_FAT_DEBUG) && defined(RFS_ERRORS_CRASH_PBR)
static void set_pbr_dead(struct super_block *sb)
{
	struct buffer_head *bh = NULL;
	struct pbr *pbr_p;
	sector_t pbr_sector = 0;
	int ret;

	sb_set_blocksize(sb, SECTOR_SIZE);
	bh = sb_bread(sb, pbr_sector);

	if (!bh)
	{
		DPRINTK("Fail to read PBR\n");
		return;
	}
	pbr_p = (struct pbr *) bh->b_data;

	DPRINTK("Break the signature of PBR\n");
	SET16(pbr_p->signature, RFS_CRASH_MARK);

	mark_buffer_dirty(bh);
	ret = rfs_sync_dirty_buffer(bh);
	if (ret)
		RFS_BUG_CRASH(sb, "Fail to write\n");

	brelse(bh);
	return;
}
#else
static void set_pbr_dead(struct super_block *sb)
{
	return;
}
#endif

/* error handler */
void rfs_handle_crash(struct super_block *sb, const char * function, int line,
		const char * fmt, ...)
{
	va_list args;
#ifdef RFS_FOR_2_4
	char message_buf[1024];

	printk(KERN_CRIT "RFS-fs error: %s[%d] ", function, line);
	va_start(args, fmt);
	vsprintf(message_buf, fmt, args);
	va_end(args);

	printk("%s\n", message_buf);
#else
	va_start(args, fmt);
	printk(KERN_CRIT "RFS-fs error: %s[%d] ", function, line);
	vprintk(fmt, args);
	printk("\n");
	va_end(args);
#endif

	printk("\thighest_d_ino: %lu\n", RFS_SB(sb)->highest_d_ino);
	if (sb->s_flags & MS_RDONLY)
		return;

#if defined(RFS_ERRORS_RDONLY)
	DPRINTK("Remounting read-only\n");
	sb->s_flags |= MS_RDONLY;
	set_pbr_dead(sb);
#endif

#if defined(RFS_ERRORS_PANIC)
	panic("RFS-fs : panic forced\n");
#else
	sb->s_flags |= MS_RDONLY;
	BUG();
#endif
	return;
}

/**
 *  wrapper function of kmalloc for retrying & tracing memory allocation
 * @param size	memory size to allocate
 * @param flags	memory allocation flag
 * @param retry	retry flag
 * @return	memory pointer on success, null pointer on failure
 */
void *rfs_kmalloc(size_t size, int flags, int retry)
{
	void *p = NULL;
	static unsigned long timeout;

	while (1) {
		p = kmalloc(size, flags);
		if (p || !retry)
			return p;

		DEBUG(DL0, "ENOMEM(%u) in 0x%p, retrying \n",
				(unsigned int) size,
				__builtin_return_address(0));

		if (time_after(jiffies, timeout + 5 * HZ)) {
			printk(KERN_NOTICE "ENOMEM in 0x%p, retrying \n",
					__builtin_return_address(0));
			timeout = jiffies;
		}

		yield();
	}
}

