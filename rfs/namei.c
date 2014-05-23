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
 *  @file	fs/rfs/namei.c
 *  @brief	Here is adaptation layer between the VFS and RFS filesystem for inode ops
 *
 *
 */

#include <linux/fs.h>
#include <linux/rfs_fs.h>
#include <linux/sched.h>
#include <linux/ctype.h>
#include <linux/nls.h>
#include <linux/namei.h>

#ifdef CONFIG_RFS_FS_XATTR
#include <linux/xattr.h>
#include "xattr.h"
#endif

#include "rfs.h"
#include "log.h"

#define SLASH	'/'

#define ROOT_MTIME(sb)	((sb)->s_root->d_inode->i_mtime)

/* structure for rename */
struct rename_info {
	struct inode *old_dir;
	struct inode *old_inode;
	struct inode *new_dir;
	unsigned int *new_index;
	const unsigned char *new_name;
};

/**
 *  Function extending a directory on request of saving larger entries that left entries.
 *  @param dir		inode corresponding to extending directory
 *  @param start_clu	return value to contain a start cluster value when this function is used to create new directory
 *  @return	zero on success, if not, negative error code.
 *
 * __extend_dir invokes alloc_cluster in it, and assumes that alloc_cluster
 * takes care of not only allocating new cluster but also updating FAT table.
 * In addition, this function is used to create new directory.
 */
static struct buffer_head *__extend_dir(struct inode * dir, unsigned int *start_clu)
{
	struct super_block *sb = dir->i_sb;
	struct buffer_head *bh, *ret = NULL;
	unsigned int start_block, last_block, cur_block;
	unsigned int new_clu;
	int err;

	if ((RFS_I(dir)->start_clu == RFS_SB(sb)->root_clu) 
			&& (!IS_FAT32(RFS_SB(sb)))) 
		return ERR_PTR(-ENOSPC);

	if ((dir->i_size + RFS_SB(sb)->cluster_size) > 
			(MAX_DIR_DENTRY << DENTRY_SIZE_BITS))
		return ERR_PTR(-ENOSPC);

  	/* alloc new cluster  */	
	err = rfs_alloc_cluster(dir, &new_clu);
	if (err)
		return ERR_PTR(err);

	if (start_clu)
		*start_clu = new_clu;

	/* reset new cluster with zero to initalize directory entry */
	start_block = START_BLOCK(new_clu, sb);
	last_block = start_block + RFS_SB(sb)->blks_per_clu;
	cur_block = start_block;
	do {
		if ((bh = sb_getblk(sb, (int) cur_block))) {
			memset(bh->b_data, 0x00 , sb->s_blocksize);
#ifdef RFS_FOR_2_6
			set_buffer_uptodate(bh);
#else
			mark_buffer_uptodate(bh, 1);
#endif			
			rfs_mark_buffer_dirty(bh, sb);
			if (!ret)
				ret = bh;
			else
				brelse(bh);
		}
	} while (++cur_block < last_block);

	if (!ret)	/* I/O error */
		return ERR_PTR(-EIO);

	/* new inode is also initialized by 0 */
	dir->i_size += RFS_SB(sb)->cluster_size;
	RFS_I(dir)->mmu_private += RFS_SB(sb)->cluster_size;

	return ret;
}

/**
 *  initialize the dot(.) and dotdot(..) dir entries for new directory
 * @param inode		inode for itself
 * @return		return 0 on success, EIO on failure
 *
 * inode must have start cluster of itself and start cluster of parent dir
 */
static int __set_new_dir(struct inode *inode)
{
	struct buffer_head *bh = NULL;
	struct rfs_dir_entry *dot_ep = NULL;
	struct rfs_dir_entry *dotdot_ep = NULL;
	unsigned char dummy = 0;
	unsigned int p_start_clu;

	/* initialize .(itself) and ..(parent) */
	dot_ep = get_entry(inode, 0, &bh);
	if (IS_ERR(dot_ep)) {
		brelse(bh);
		return -EIO;
	}

	dotdot_ep = (struct rfs_dir_entry *) (bh->b_data + DENTRY_SIZE);

	/* If parent is root dir, '..' should have 0 as its start cluster */
	if (RFS_I(inode)->p_start_clu == RFS_SB(inode->i_sb)->root_clu)
	{
		p_start_clu = ROOT_CLU;
	}
	else
	{
		p_start_clu = RFS_I(inode)->p_start_clu;
	}

	rfs_init_dir_entry(inode, dot_ep, TYPE_DIR, 
			RFS_I(inode)->start_clu, DOT, &dummy, S_IRWXUGO);
	rfs_init_dir_entry(inode, dotdot_ep, TYPE_DIR, 
			p_start_clu, DOTDOT, &dummy, S_IRWXUGO);

	rfs_mark_buffer_dirty(bh, inode->i_sb);
	brelse(bh);

	return 0;
}


/** 
 * allocate a cluster for new directory 
 * @param inode		new inode
 * @param start_clu	allocated cluster to inode
 * @return		zero on success, or errno
 */
static inline int __init_new_dir(struct inode *dir, struct inode *inode, unsigned int *start_clu)
{
	struct buffer_head *bh = NULL;
	int ret;

	bh = __extend_dir(inode, start_clu);
	if (IS_ERR(bh))
		return PTR_ERR(bh);

	RFS_I(inode)->p_start_clu = RFS_I(dir)->start_clu;

	ret = __set_new_dir(inode);

	brelse(bh);
	return ret;
}

/**
 *  check whether directory is emtpy
 * @param dir	inode corresponding to the directory
 * @return	return 0 on success, errno on failure
 *
 * __is_dir_empty is usually invoked before removing or renaming directry.
 */
static int __is_dir_empty(struct inode *dir)
{
	struct buffer_head *bh = NULL;
	struct rfs_dir_entry *ep;
	unsigned int cpos = 0;
	int err = 0, count = 0;
	unsigned int type;

	if (dir->i_ino == ROOT_INO)
		return -ENOTEMPTY;

	while (1) {
		ep = get_entry(dir, cpos++, &bh);
		if (IS_ERR(ep)) {
			err = PTR_ERR(ep);
			if (err == -EFAULT)
				err = 0;
			goto out;
		}

		type = rfs_entry_type(ep);
		if ((type == TYPE_FILE) || (type == TYPE_DIR)) {
			/* check entry index bigger than 
			   entry index of parent directory (..) */
			if (++count > 2) {
				err = -ENOTEMPTY;
				goto out;
			}
		} else if (type == TYPE_UNUSED) {
			/* do not need checking anymore */
			goto out;
		}
	}

out:
	brelse(bh);
	return err; 
}



/**
 * Function to find proper postion where new entry is stored 
 * @param dir	inode relating to parent directory
 * @param slots	the number of entries to be saved
 * @param bh	buffer head containing directory entry
 * @return	a offset of empty slot of a current directory on sucess, negative value on falure.
 *
 * This function is invoked by upper functions, upper functions are different
 * according the FAT type of current system.
 */
static int __find_empty_entry(struct inode *dir, unsigned int slots, struct buffer_head **bh) {
	struct super_block *sb = dir->i_sb;
	struct buffer_head *new_bh;
	struct rfs_dir_entry *ep;
	unsigned int cpos = 0;
	unsigned int nr_clus = 0;
	int free = 0;
	
	while (1) {
		ep = get_entry(dir, cpos, bh);
		if (IS_ERR(ep)) {
			if (PTR_ERR(ep) == -EFAULT)
				break;
			else
				return PTR_ERR(ep);
		}

		if (IS_FREE(ep->name)) {
			if (++free == slots)
				return (int) cpos;
		} else
			free = 0;

		cpos++;
	}	

	/* If fail to find requested slots, decide whether to extend directory */
	if ((RFS_I(dir)->start_clu == RFS_SB(sb)->root_clu) &&
		(!IS_FAT32(RFS_SB(sb))))
	{
		return -ENOSPC;
	}

	/* calculate the number of cluster */
	nr_clus = ((slots - free + 
			(RFS_SB(sb)->cluster_size >> DENTRY_SIZE_BITS) - 1) /
		 (RFS_SB(sb)->cluster_size >> DENTRY_SIZE_BITS));

	if (nr_clus > GET_FREE_CLUS(sb))
		return -ENOSPC;

	/* extend entry */
	while (nr_clus-- > 0) {
		new_bh = __extend_dir(dir, NULL); 
		if (IS_ERR(new_bh))
			return PTR_ERR(new_bh);
	}

	while (1) {
		ep = get_entry(dir, cpos, &new_bh);
		if (IS_ERR(ep)) {
			if (PTR_ERR(ep) == -EFAULT)
				return -ENOSPC;
			else
				return PTR_ERR(ep);
		}
		if (++free == slots)
			break;
		cpos++;
	} 

	*bh = new_bh;
	/* return accumulated offset regardless of cluster */
	return (int) cpos;
}

#ifndef CONFIG_RFS_VFAT
/**
 *  compare two dos names 
 * @param dentry	dentry to be computed
 * @param a		file name cached in dentry cache
 * @param b		file name to be compared for corresponding dentry
 * @return 		return 0 on success, errno on failure
 *
 * If either of the names are invalid, do the standard name comparison
 */

static int rfs_dentry_cmp(struct dentry *dentry, struct qstr *a, struct qstr *b)
{
	u8 a_dosname[DOS_NAME_LENGTH], b_dosname[DOS_NAME_LENGTH];
	unsigned char valid_name[NAME_MAX + 1];
	int err = 0;

	if ((a->len > NAME_MAX) || (b->len > NAME_MAX))	/* out-of-range input */
		return -EINVAL;

	memcpy(valid_name, a->name, a->len);
	valid_name[a->len] = '\0';
	err = rfs_convert_cstring_to_dosname(a_dosname, valid_name, NULL, FALSE,
			test_opt(dentry->d_sb, CHECK_NO));
	if (err < 0)
		goto out;

	memcpy(valid_name, b->name, b->len);
	valid_name[b->len] = '\0';
	err = rfs_convert_cstring_to_dosname(b_dosname, valid_name, NULL, FALSE,
			test_opt(dentry->d_sb, CHECK_NO));
	if (err < 0)
		goto out;
	err = memcmp(a_dosname, b_dosname, DOS_NAME_LENGTH);

out:
	return err;
}

/**
 *  compute the hash for the dos name corresponding to the dentry
 * @param dentry	dentry to be computed
 * @param qstr		parameter to contain resulting hash value
 * @return		return 0
 */
static int rfs_dentry_hash(struct dentry *dentry, struct qstr *qstr)
{
	unsigned char hash_name[DOS_NAME_LENGTH];
	unsigned char valid_name[NAME_MAX + 1];
	int err;

	if (qstr->len > NAME_MAX)	/* out-of-range input */
		return 0;

	memcpy(valid_name, qstr->name, qstr->len);
	valid_name[qstr->len] = '\0';
	err = rfs_convert_cstring_to_dosname(hash_name, valid_name, NULL, FALSE,
			test_opt(dentry->d_sb, CHECK_NO));
	if (!err)
		qstr->hash = full_name_hash(hash_name, DOS_NAME_LENGTH);

	return 0;
}

struct dentry_operations rfs_dentry_operations = {
	.d_hash		= rfs_dentry_hash,
	.d_compare	= rfs_dentry_cmp,
};

/**
 *  create a dir entry and fill it
 * @param dir		parent dir inode
 * @param inode		inode of new dir entry
 * @param start_clu	start cluster number for itself
 * @param type		entry type
 * @param name		object name 
 * @return 		return index(pointer to save dir entry index) on success, errno on failure
 */
int rfs_build_entry_short(struct inode *dir, struct inode *inode, unsigned int start_clu, unsigned int type, const unsigned char *name, int mode)
{
	struct buffer_head *bh = NULL;
	struct rfs_dir_entry *ep = NULL;
	struct log_ENTRY_info lei;
	unsigned char dosname[DOS_NAME_LENGTH];
	unsigned char is_mixed = 0;
	unsigned int index;
	int ret = 0;

	/* convert a cstring into dosname */
	ret = rfs_make_dosname(dir, name, dosname, &is_mixed, NULL);
	if (ret < 0) 
		goto out;

	/* find empty dir entry */
	ret = __find_empty_entry(dir, 1, &bh);
	if (ret < 0)
		goto out;

	index = (unsigned int) ret;

	/* allocate a new cluster for directory */
	if ((type == TYPE_DIR) && inode) {
		if ((ret = __init_new_dir(dir, inode, &start_clu)) < 0) 
			goto out;
	}

	/* get dir entry pointer */
	ep = get_entry(dir, index, &bh); 
	if (IS_ERR(ep)) {
		ret = PTR_ERR(ep);
		goto out;
	}
	
	lei.pdir = RFS_I(dir)->start_clu;
	lei.entry = index;
	lei.numof_entries = 1;
	/* RFS-log sub trans */
	if (rfs_log_build_entry(dir->i_sb, &lei)) {
		ret = -EIO;
		goto out;
	}

	/* init dir entry */
	rfs_init_dir_entry(dir, ep, type, start_clu, dosname, &is_mixed, mode);

	rfs_mark_buffer_dirty(bh, dir->i_sb);

	ret = (int) index;
out:
	brelse(bh);
	return ret;
}

#else	/* CONFIG_RFS_VFAT */

/*
 * revalidate dentry when lookup do not indend to create
 * @param dentry	dentry to be computed
 * @param nd		nameidata
 * @return		return 0
 */
static int rfs_ci_revalidate(struct dentry *dentry, struct nameidata *nd)
{
	/*
	 * This may be nfsd (or something), anyway, we can't see the
	 * intent of this. So, since this can be for creation, drop it.
	 */
	if (!nd)
		return 0;

	/*
	 * Drop the negative dentry, in order to make sure to use the
	 * case sensitive name which is specified by user if this is
	 * for creation.
	 */
	if (!(nd->flags & (LOOKUP_CONTINUE | LOOKUP_PARENT))) {
		if (nd->flags & LOOKUP_CREATE)
			return 0;
	}

	return 1;
}

/*
 * compute the hash for the case insensitive vfat name corresponding
 * to the dentry
 * @param dentry	dentry to be computed
 * @param qstr		parameter to contain resulting hash value
 * @return		return 0
 */
static int rfs_ci_hash(struct dentry *dentry, struct qstr *qstr)
{
	struct nls_table *t = RFS_SB(dentry->d_inode->i_sb)->nls_disk;
	const unsigned char *name;
	unsigned int len;
	unsigned long hash;

	name = qstr->name;
	len = qstr->len;

	hash = init_name_hash();
	while (len--)
		hash = partial_name_hash(nls_tolower(t, *name++), hash);
	qstr->hash = end_name_hash(hash);

	return 0;
}

/**
 * case insensitive compare of two vfat names
 * @param dentry	dentry to be computed
 * @param a		file name cached in dentry cache
 * @param b		file name to be compared for corresponding dentry
 * @return 		return 0 on success, errno on failure
 *
 */
static int rfs_ci_cmp(struct dentry *dentry, struct qstr *a, struct qstr *b)
{
	struct nls_table *t = RFS_SB(dentry->d_inode->i_sb)->nls_disk;

	/* A filename cannot end in '.' or we treat it like it has none */
	if (a->len == b->len) {
		if (nls_strnicmp(t, a->name, b->name, a->len) == 0)
			return 0;
	}
	return 1;
}

struct dentry_operations rfs_ci_dentry_ops = {
	.d_revalidate	= rfs_ci_revalidate,
	.d_hash		= rfs_ci_hash,
	.d_compare	= rfs_ci_cmp,
};

/**
 *  create a dir entry and extend entries 
 * @param dir		parent dir inode
 * @param inode		inode of new dir entry
 * @param start_clu	start cluster number for itself
 * @param type		entry type
 * @param name		object name
 * @return 		return index(pointer to save dir entry index) on success, errno on failure
 */
int rfs_build_entry_long(struct inode *dir, struct inode *inode, unsigned int start_clu, unsigned int type, const unsigned char *name, int mode)
{
	struct buffer_head *bh = NULL;
	struct rfs_dir_entry *ep = NULL;
	struct rfs_ext_entry *extp = NULL;
	struct log_ENTRY_info lei;
	unsigned char checksum;
	unsigned short uname[UNICODE_NAME_LENGTH];
	unsigned char dosname[DOS_NAME_LENGTH + 1]; /* +1 is margin for '\0' */
	unsigned int num_entries, i;
	unsigned char is_mixed = 0;
	unsigned int index;
	int ret = 0;

	/* 
	 * convert a cstring into dosname & 
	 * return the count of needed extend slots 
	 */
	if ((ret = rfs_make_dosname(dir, name, dosname, &is_mixed, uname)) < 0)
		return ret;

	/* ret has the number of the extend slot */
	num_entries = ret + 1;

	/* find empty dir entry */
	ret = __find_empty_entry(dir, num_entries, &bh);
	if (ret < 0)
		goto out;

	index = (unsigned int) ret;

	if (test_opt(dir->i_sb, CHECK_NO)) {
		/* make dummy SFN name when CHECK_NO option is enabled. */
		/* dumm SFN Name as below
		 * "+++NNNNN.+++" (NNNNN is SFN entry offset in dir).
		 */
		/* index is must less than 65536 */
		snprintf(dosname, DOS_NAME_LENGTH + 1, "+++%05u+++", index);
		DEBUG(DL3, "dosname=[%s]", dosname);
	}

	/* allocate a new cluster for directory */
	if (type == TYPE_DIR && inode) {
		if ((ret = __init_new_dir(dir, inode, &start_clu)) < 0) 
			goto out;
	}

	/* get dir entry pointer */
	ep = get_entry(dir, index, &bh);
	if (IS_ERR(ep)) {
		ret = PTR_ERR(ep);
		goto out;
	}

	lei.pdir = RFS_I(dir)->start_clu;
	lei.entry = index;
	lei.numof_entries = num_entries;
	/* RFS-log sub trans */
	if (rfs_log_build_entry(dir->i_sb, &lei)) 
	{
		ret = -EIO;
		goto out;
	}

	/* init dir entry */
	rfs_init_dir_entry(dir, ep, type, start_clu, dosname, &is_mixed, mode);
	
	rfs_mark_buffer_dirty(bh, dir->i_sb);

	/* only have dos entry */
	if (num_entries == 1) {
		ret = (int) index;
		goto out;
	}

	checksum = rfs_calc_checksum(dosname);

	/* init extend entries */
	for (i = 1; i < num_entries; i++) {
		ep = get_entry(dir, index - i, &bh);
		if (IS_ERR(ep)) {
			ret = PTR_ERR(ep);
			goto out;
		}
		extp = (struct rfs_ext_entry *) ep;
		if (rfs_init_ext_entry(extp, TYPE_EXTEND, 
				((i == (num_entries - 1))? i + EXT_END_MARK: i),
				&(uname[EXT_UNAME_LENGTH * (i - 1)]), 
				checksum) < 0) { /* out-of-range input */
			ret = -EIO;
			goto out;
		}

		rfs_mark_buffer_dirty(bh, dir->i_sb);
	}

	ret = (int) index;
out:
	brelse(bh);
	return ret;	
}

#endif	/* !CONFIG_RFS_VFAT */

#ifdef CONFIG_RFS_FAST_LOOKUP
static void __fast_lookup_init_inode(struct inode *inode,
		unsigned int p_start_clu, unsigned int index);
#endif

/**
 * find inode for dir entry and return
 * @param sb		super block
 * @param inode		parent's inode
 * @param ino		inode number of dir entry
 * @param index		dir entry's offset in parent dir
 * @param ep		dir entry
 * @return		inode of dir entry on success or errno on failure
 */
struct inode *rfs_iget(struct super_block *sb, struct inode *dir, unsigned long ino, unsigned int index, struct rfs_dir_entry *ep, struct buffer_head *bh, struct qstr * d_name)
{
#ifndef CONFIG_RFS_IGET4
	struct inode *inode = iget_locked(sb, ino);

	if (inode && (inode->i_state & I_NEW)) {
		rfs_fill_inode(dir, inode, ep, index, 0, bh, d_name);
		unlock_new_inode(inode);
	}
#else
	struct inode *inode;
	struct rfs_iget4_args args;

	args.dir = dir;
	args.ep = ep;
	args.index = index;
	args.d_name = d_name;

	/* 
	 * Some kernel version(under Linux kernel version 2.4.25) does not 
	 * support iget_locked.
	 */
 	inode = iget4(sb, ino, NULL, (void *) (&args));
#endif

#ifdef CONFIG_RFS_FAST_LOOKUP
	if (inode)
		__fast_lookup_init_inode(inode, p_start_clu, index);
#endif

	return inode;
}

/**
 *  find dir entry for inode in parent dir and return a dir entry & entry index
 * @param dir		parent dir inode
 * @param inode		inode for itself
 * @param bh		buffer head included dir entry for inode
 * @return 		return dir entry on success, errno on failure
 */
static int __search_entry(struct inode *dir, struct inode *inode)
{
	struct buffer_head *bh = NULL;
	struct rfs_dir_entry *ep = NULL;
	int ret = 0;

	if (!inode)	/* out-of-range input */
		return (-EINVAL);

#ifdef CONFIG_RFS_FAST_LOOKUP
	if ( IS_FAST_LOOKUP_INDEX(inode) ||
		(RFS_I(inode)->fast && 
		 (RFS_I(dir)->start_clu != RFS_I(inode)->p_start_clu)) )
		return (-EFAULT);
#endif

	CHECK_RFS_INODE(inode, (-EINVAL));

	ep = get_entry(dir, RFS_I(inode)->index, &bh);

	if (IS_ERR(ep))
		ret = PTR_ERR(ep);
	else
		ret = 0;

	brelse(bh);
	return ret;
}

#ifdef CONFIG_RFS_FAST_LOOKUP
/**
 * create a checksum value from file size, hi cluster number and low cluster number.
 * @param data		the data to be summed
 * @param len		legnth of the data
 * @return		the checksum vale
 */
static __u32 __check_sum(struct rfs_dir_entry *ep)
{
	__u32 check;

	check = ((ep->start_clu_lo << 16) & 0xffff0000) | (ep->start_clu_hi & 0x0000ffff);
	check = check ^ ep->size;

	return check;
}

/**
 * decode string to interger
 * @param name		encoded string of num
 * @return		decoded interger value
 *
 * decode 8 bytes string to 4 bytes integer
 */
static inline unsigned long __decode_s2i(__u8 const *name)
{
	int i;
	unsigned long num = 0;

	for (i = 0; i < 8; i++)
	{
		num <<= 4;
		num |= *name++ & 0x0f;
	}

	return num;

}

#define IS_FAST_LOOKUP(dentry)					\
	(((dentry->d_name.len == RFS_FAST_NAME_LENGTH) &&	\
	 !strncmp(dentry->d_name.name, RFS_FAST_SYMBOL, 	\
		 RFS_FAST_SYMBOL_LENGTH))? 1: 0)


static struct rfs_dir_entry *__fast_lookup_get_entry(
		struct super_block *sb, struct dentry *dentry,
		struct buffer_head **res_bh, unsigned long *ino)
{
	struct rfs_dir_entry *ep = NULL;
	unsigned long check = 0;
	sector_t block = 0;
	unsigned long off = 0;
	int ret = 0;

	/* decode name and get inode number and check sum value */
	*ino = __decode_s2i(dentry->d_name.name + RFS_FAST_SYMBOL_LENGTH);
	check = __decode_s2i(dentry->d_name.name + RFS_FAST_SYMBOL_NEXT);

	/* get the block number of directory entry 
	   from the inode number */
	off = *ino << DENTRY_SIZE_BITS;
	block = (sector_t) (off >> sb->s_blocksize_bits);

	/* read block */
	*res_bh = rfs_bread(sb, block, BH_RFS_DIR);
	if (*res_bh == NULL) {
		DPRINTK("fast open lookup error: "
				"directory entry is out of range.\n");
		return ERR_PTR(-EIO);
	}

	/* read directory entry */
	off &= (sb->s_blocksize - 1);
	ep = (struct rfs_dir_entry *) ((*res_bh)->b_data + off);

	/* check empty file */
	if (ep->size == 0) {
		DPRINTK("fast open lookup error: "
				"can not open empty file.\n");
		ret = -ENOENT;
		goto out;
	}

	/* check file name */
	if (__check_sum(ep) != check || (ep->attr & ATTR_SUBDIR)) {
		DPRINTK("fast open lookup error: "
				"wrong check sum value.\n");
		ret = -ENOENT;
		goto out;
	}

	return ep;

out:
	brelse(*res_bh);
	*res_bh = NULL;
	return ERR_PTR(ret);
}

static void __fast_lookup_init_inode(struct inode *inode,
		unsigned int p_start_clu, unsigned int index)
{
	if (index == RFS_FAST_LOOKUP_INDEX)
	{
		/* file is opened with fast lookup option */
		RFS_I(inode)->fast = 1;
	}
	else if (IS_FAST_LOOKUP_INDEX(inode))
	{
		/* file is opened with normal lookup operation
		   for the first time after fast lookup */
		RFS_I(inode)->p_start_clu = p_start_clu;
		RFS_I(inode)->index = index;
	}

}
#endif	/* CONFIG_RFS_FAST_LOOKUP */

/**
 *  lookup inode associated with dentry
 * @param dir		inode of parent directory
 * @param dentry	dentry for itself
 * @param nd		namei data structure
 * @return		return dentry object on success, errno on failure
 *
 * if inode doesn't exist, allocate new inode, fill it, and associated with dentry
 */
#ifdef RFS_FOR_2_6
static struct dentry *rfs_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
#else
static struct dentry *rfs_lookup(struct inode *dir, struct dentry *dentry)
#endif	
{
	struct inode *inode = NULL;
	struct buffer_head *bh = NULL;
	struct rfs_dir_entry *ep = NULL;
	unsigned int index = 0;
	unsigned long ino = 0;
	int ret = 0;

	/* check the name length */
	if (dentry->d_name.len > NAME_MAX)
		return ERR_PTR(-ENAMETOOLONG);

#ifdef CONFIG_RFS_FAST_LOOKUP
	/* check if the file is opened with a fast lookup option "<?>" */
	if (IS_FAST_LOOKUP(dentry))
	{
		ep = __fast_lookup_get_entry(dir->i_sb, dentry, &bh, &ino);
		if (IS_ERR(ep)) 
		{
			ret = PTR_ERR(ep);
			goto out;
		}

		index = RFS_FAST_LOOKUP_INDEX;
	}
	else
#endif
	{
		/* find dir entry */
		ret = find_entry(dir, dentry->d_name.name, &bh, TYPE_ALL);
		if (ret < 0) {
			if (ret == -ENOENT)
				goto add;
			if ((ret != -EINVAL) && (ret != -ENAMETOOLONG))
				ret = -EIO;
			goto out;
		}
		index = (unsigned int) ret;

		ep = get_entry(dir, index, &bh);
		if (IS_ERR(ep)) {
			ret = -EIO;
			goto out;
		}

		/* get unique inode number */
		ret = rfs_iunique(dir, index, &ino);
		if (ret != 0)
			goto out;
	}

	inode = rfs_iget(dir->i_sb, dir, ino, index, ep, bh, &dentry->d_name);
	if (!inode) {
		ret = -EACCES;
		goto out;
	}

	// 20090716
	if (RFS_I(dir)->start_clu != RFS_I(inode)->p_start_clu)
	{
		RFS_BUG_CRASH(dir->i_sb, "inode(%lu) is corrupted: guessing "
				"p_start_clu=%u inode's p_start_clu=%u",
				ino, RFS_I(dir)->start_clu, 
				RFS_I(inode)->p_start_clu);
	}

	if (RFS_I(inode)->i_state & RFS_I_MODIFIED)
	{
		RFS_I(inode)->i_state &= ~RFS_I_MODIFIED;
		mark_buffer_dirty(bh);
	}

#ifdef CONFIG_RFS_VFAT 
	if (!test_opt(dir->i_sb, CHECK_STRICT)) {
		dentry->d_op = &rfs_ci_dentry_ops;
	} 

	do {
		struct dentry *alias = NULL;

		alias = d_find_alias(inode);
		if (alias) {
			if (d_invalidate(alias) == 0) {
				dput(alias);
			} else {
				iput(inode);
				brelse(bh);
				return alias;
			}
		}
	} while (0);
#else
	dentry->d_op = &rfs_dentry_operations;
#endif

add:
	brelse(bh);
#ifdef RFS_FOR_2_6
	return d_splice_alias(inode, dentry);
#else	
	d_add(dentry, inode);
	return NULL;
#endif
out:
	brelse(bh);
	return ERR_PTR(ret);
}

/**
 *  create a new file
 * @param dir		inode of parent directory
 * @param dentry	dentry corresponding with a file will be created
 * @param mode		mode
 * @param nd		namei data structure
 * @return		return 0 on success, errno on failure	 
 */
#ifdef RFS_FOR_2_6
static int rfs_create(struct inode *dir, struct dentry *dentry, int mode, struct nameidata *nd)
#else
static int rfs_create(struct inode *dir, struct dentry *dentry, int mode)
#endif
{
	struct inode *inode = NULL;
	int ret;

	/* check the validity */
	if (IS_RDONLY(dir))
		return -EROFS;

	if (rfs_log_start(dir->i_sb, RFS_LOG_CREATE, dir))
		return -EIO;

	/* create a new inode */
	inode = rfs_new_inode(dir, dentry, TYPE_FILE, mode);
	if (IS_ERR(inode))
		ret = PTR_ERR(inode);
	else
		ret = 0;

	/* If failed, RFS-log can't rollback due to media error */
	if (rfs_log_end(dir->i_sb, ret))
		return -EIO; 

	if (!ret) /* attach inode to dentry */
		d_instantiate(dentry, inode); 

	return ret;
}

/**
 *  check whether a file of inode has start cluster number of RFS reserved files
 * @param inode	inode of checked file
 * @param name 	name of checked file
 * @return	return 0 if inode has different start cluster number, else return errno
 *
 * special files for fast unlink & logging are reserved files
 */
int rfs_check_reserved_files(struct inode *inode, const struct qstr *str)
{
	if (inode) 
	{
		struct super_block *sb = inode->i_sb;
		unsigned int cluster = RFS_I(inode)->start_clu;

		if (RFS_SB(sb)->use_log && 
				(cluster == RFS_LOG_I(sb)->l_start_cluster))
		{
			return -EPERM;
		}
	} 
	else if (str) 
	{
		if (str->len != RFS_LOG_FILE_LEN)
			return 0;

		if (!strncmp(str->name, RFS_LOG_FILE_NAME, RFS_LOG_FILE_LEN))
			return -EPERM;
	}
	
	return 0;
}

/**
 *  remove a file
 * @param dir		parent directory inode
 * @param dentry	dentry corresponding with a file will be removed
 * @return		return 0 on success, errno on failure	 
 */
static int rfs_unlink(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	int ret;

	/* check the validity */
	if (IS_RDONLY(dir))
		return -EROFS;

	/* check the name length */
	if (dentry->d_name.len > NAME_MAX)
		return -ENAMETOOLONG;

	/* check the system files */
	if (rfs_check_reserved_files(inode, NULL))
		return -EPERM;

	/* find dir entry */
	ret = __search_entry(dir, inode);
	if (ret)
		return ret;

	/*DEBUG(DL0, "name:%s", dentry->d_name.name);*/

	/* RFS-log : start unlink */
	if (rfs_log_start(dir->i_sb, RFS_LOG_UNLINK, dir))
		return -EIO;

	/* remove dir entry and dealloc all clusters for inode allocated */
	ret = rfs_delete_entry(dir, inode);

	/* If failed, RFS-log can't rollback due to media error */
	if (rfs_log_end(dir->i_sb, ret))
		return -EIO;

	return ret;
}

/**
 *  create a new directory
 * @param dir		parent directory inode
 * @param dentry	dentry corresponding with a directory will be created
 * @param mode		mode
 * @return		return 0 on success, errno on failure	 
 */
static int rfs_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
	struct inode *inode = NULL;
	int ret;

	/* check the validity */
	if (IS_RDONLY(dir))
		return -EROFS;

	/* RFS-log : start mkdir */
	if (rfs_log_start(dir->i_sb, RFS_LOG_CREATE, dir))
		return -EIO;

	/* create a new inode */
	inode = rfs_new_inode(dir, dentry, TYPE_DIR, mode);
	if (IS_ERR(inode))
		ret = PTR_ERR(inode);
	else
		ret = 0;

	/* If failed, RFS-log can't rollback due to media error */
	if (rfs_log_end(dir->i_sb, ret))
		return -EIO; 

	if (!ret) /* attach inode to dentry */
		d_instantiate(dentry, inode); 

	return ret;
}

/**
 *  remove a directory
 * @param dir		parent directory inode
 * @param dentry	dentry corresponding with a directory will be removed 
 * @return		return 0 on success, errno on failure	 
 */
static int rfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	int ret;

	if (IS_FAST_LOOKUP_INDEX(inode))
		return -EFAULT;

	/* check the validity */
	if (IS_RDONLY(dir))
		return -EROFS;

	/* check the name length */
	if (dentry->d_name.len > NAME_MAX)
		return -ENAMETOOLONG;

	/* find dir entry */
	ret = __search_entry(dir, inode);
	if (ret)
		return ret;

	if (((dir->i_ino == ROOT_INO) && ((int) RFS_I(inode)->index < 0)) || 
			((dir->i_ino != ROOT_INO) && (RFS_I(inode)->index < 2)))
		return -ENOENT;

	/* check whether directory is empty */
	ret = __is_dir_empty(inode);
	if (ret)
		return ret;

	/* RFS-log : start rmdir */
	if (rfs_log_start(dir->i_sb, RFS_LOG_UNLINK, dir))
		return -EIO;

	/* remove directory */
	ret = rfs_delete_entry(dir, inode);

	DEBUG(DL2, "nlink %d, count %d\n", (int) (inode->i_nlink), 
			(int) atomic_read(&inode->i_count));
	DEBUG(DL2, "dentry %d, parent %d\n", 
			(int) atomic_read(&dentry->d_count), 
			(int) atomic_read(&(dentry->d_parent)->d_count));

	/* If failed, RFS-log can't rollback due to media error */
	if (rfs_log_end(dir->i_sb, ret))
		return -EIO;

	return ret;
}

/**
 *  change a directory
 * @param r_info	argument to move
 * @return		return 0 on success, errno on failure	 
 */
static int __move_to_dir(struct rename_info *r_info, sector_t *block_nr, loff_t *offset)
{
	struct super_block *sb = r_info->old_dir->i_sb;
	unsigned int old_index = RFS_I(r_info->old_inode)->index;
	unsigned int *new_index = r_info->new_index;
	struct buffer_head *old_bh = NULL;
	struct rfs_dir_entry *old_ep = NULL;
	unsigned int old_type;
	int ret = 0;
	int mode = 0;
	struct buffer_head *new_bh = NULL;
	struct rfs_dir_entry *new_ep = NULL;

	old_ep = get_entry(r_info->old_dir, old_index, &old_bh);
	if (IS_ERR(old_ep)) {
		ret = PTR_ERR(old_ep);
		goto out;
	}

	rfs_get_mode(old_ep, (umode_t *)&mode);

	old_type = rfs_entry_type(old_ep);
	if ((old_type == TYPE_DIR) || (old_type == TYPE_FILE))
	{
		if ((old_type == TYPE_FILE) && IS_SYMLINK(old_ep))
			old_type = TYPE_SYMLINK;
		else if ((old_type == TYPE_FILE) && IS_SOCKET(old_ep))
			old_type = TYPE_SOCKET;
		else if ((old_type == TYPE_FILE) && IS_FIFO(old_ep))
			old_type = TYPE_FIFO;

		ret = build_entry(r_info->new_dir, NULL,
				(unsigned int) START_CLUSTER(RFS_SB(sb), old_ep), 
				old_type, r_info->new_name, mode);
	}
	else
	{
		ret = -EINVAL;	/* out-of-range input */
	}
	if (ret < 0) 
		goto out;

	*new_index = (unsigned int) ret;

	/* update mtime of new entry */
	new_ep = get_entry(r_info->new_dir, *new_index, &new_bh);
	if (IS_ERR(new_ep)) {
		ret = PTR_ERR(new_ep);
		goto out;
	}

	SET16(new_ep->mtime, GET16(old_ep->mtime));
	SET16(new_ep->mdate, GET16(old_ep->mdate));
	SET32(new_ep->size, GET32(old_ep->size));

#ifdef CONFIG_RFS_FS_XATTR
	/*
	 * set xattr exist flag (attr's 7th bit) and xattr's start cluster
	 * to new_ep. 
	 */
	if (RFS_I(r_info->old_inode)->xattr_start_clus != CLU_TAIL)
	{
		/* when inode is normal file and directory */
		DEBUG(DL2, "move xattr start to new entry:name:%s,"
				"xattr_start:%u", new_ep->name, 
				RFS_I(r_info->old_inode)->xattr_start_clus);
		new_ep->attr |= ATTR_XATTR;
		SET16(new_ep->cdate, (__u16)
			(RFS_I(r_info->old_inode)->xattr_start_clus >> 16));
		SET16(new_ep->ctime, (__u16)
			(RFS_I(r_info->old_inode)->xattr_start_clus & 0x0FFFF));
	}
#endif
	rfs_mark_buffer_dirty(new_bh, sb);

	// ESR Debugging 20090711
	if (new_bh)
	{
		int dir_per_block = 
			sb->s_blocksize / sizeof(struct rfs_dir_entry);
		int dir_per_block_bits = ffs(dir_per_block) - 1;

		if (block_nr)
			*block_nr = new_bh->b_blocknr;
		if (offset)
		{
			*offset = ((loff_t) new_bh->b_blocknr << dir_per_block_bits) | 
				(new_ep - (struct rfs_dir_entry *) new_bh->b_data);
		}
	}


	if ((old_type == TYPE_DIR) && (r_info->old_dir != r_info->new_dir)) {
		/* change pointer of parent dir */
		old_ep = get_entry(r_info->old_inode, 1, &old_bh);
		if (IS_ERR(old_ep)) {
			ret = PTR_ERR(old_ep);
			goto out;
		}

		if (ROOT_INO != r_info->new_dir->i_ino) {
			new_ep = get_entry(r_info->new_dir, 0, &new_bh);
			if (IS_ERR(new_ep)) {
				ret = PTR_ERR(new_ep);
				goto out;
			}
			memcpy(old_ep, new_ep, sizeof(struct rfs_dir_entry));
			memcpy(old_ep->name, DOTDOT, DOS_NAME_LENGTH);
		} else {
			/* '..' should have 0 as its start cluster */
			SET_START_CLUSTER(old_ep, ROOT_CLU);
			rfs_set_entry_time(old_ep, ROOT_MTIME(sb));
		}
		rfs_mark_buffer_dirty(old_bh, sb);
	}

	/* delete old entry */
	ret = rfs_remove_entry(r_info->old_dir, r_info->old_inode);
out:
	brelse(new_bh);
	brelse(old_bh);
	return ret;
}

/**
 *  change name or location of a file or directory
 * @param old_dir	old parent directory
 * @param old_dentry	old dentry
 * @param new_dir	new parent directory
 * @param new_dentry	new dentry
 * @return		return 0 on success, errno on failure	 
 */
static int rfs_rename(struct inode *old_dir, struct dentry *old_dentry, struct inode *new_dir, struct dentry *new_dentry)
{
	struct inode *old_inode = old_dentry->d_inode;
	struct inode *new_inode = new_dentry->d_inode;
	struct qstr *old_qname = &old_dentry->d_name;
	struct qstr *new_qname = &new_dentry->d_name;
	struct rename_info r_info;
	unsigned long new_ino;
	unsigned int new_index = 0;
	int is_exist = FALSE;
	int ret;

	// ESR Debugging 20090711
	sector_t new_block_nr = 0;
	loff_t new_offset = 0;

	if (IS_FAST_LOOKUP_INDEX(old_inode))
		return -EFAULT;

	/* check the validity */
	if (IS_RDONLY(old_dir))
		return -EROFS;

	/* check the name length */
	if ((old_qname->len > NAME_MAX) ||
			(new_qname->len > NAME_MAX))
		return -ENAMETOOLONG;

	/* check permission */
	if (rfs_check_reserved_files(old_inode, NULL))
		return -EPERM;

	/* if new_inode is NULL, compare name */
	if (rfs_check_reserved_files(new_inode, new_qname))
		return -EPERM;
	
	if ((old_dir->i_ino != ROOT_INO) && (RFS_I(old_inode)->index < 2)) 
		return -EINVAL;

	/* find new dir entry if exists, remove it */
	ret = __search_entry(new_dir, new_inode);
	if (0 == ret) {
		if (S_ISDIR(new_inode->i_mode)) {
			ret = __is_dir_empty(new_inode);
			if (ret) {
				return ret;
			}
		}
		/* mark is_exist for later deletion */
		is_exist = TRUE;
	}

	/* fill rename info */
	r_info.old_dir = old_dir;
	r_info.old_inode = old_inode;
	r_info.new_dir = new_dir;
	r_info.new_index = &new_index;
	r_info.new_name = new_qname->name;

	/* RFS-log : start rename */
	if (rfs_log_start(old_dir->i_sb, RFS_LOG_RENAME, old_inode))
		return -EIO;

	ret = __move_to_dir(&r_info, &new_block_nr, &new_offset);
	if (ret) {
		DEBUG(DL0, "__move_to_dir fails(%d): old (dir=%lu, inode=%lu), "
				"new (dir=%lu)", ret, old_dir->i_ino, 
				old_inode->i_ino, new_dir->i_ino);
		goto end_log;
	}

	/* delete destination, if exist */
	if (is_exist == TRUE) {
		ret = rfs_delete_entry(new_dir, new_inode);
		if (ret) {
			DEBUG(DL0, "rfs_delete_entry fails(%d): new_dir.i_ino"
					"=%lu new_inode.i_ino=%lu", 
					ret, new_dir->i_ino, new_inode->i_ino);
			goto end_log;
		}
	}

	/* update inode fields */
	ret = rfs_iunique(new_dir, new_index, &new_ino);
	if (ret != 0) {
		DEBUG(DL0, "rfs_iunique fails(%d): new_dir.i_ino=%lu new_index"
				"=%u", ret, new_dir->i_ino, new_index);
		goto end_log;
	}

	remove_inode_hash(old_inode);

	/*
	 * don't change the order of statements for
	 * synchronization with rfs_sync_inode() in locked area
	 */
	spin_lock(&RFS_I(old_inode)->write_lock);
	old_inode->i_ino = new_ino;
	RFS_I(old_inode)->p_start_clu = RFS_I(new_dir)->start_clu;
	RFS_I(old_inode)->index = new_index;
	RFS_I(old_inode)->block_nr = new_block_nr;
	RFS_I(old_inode)->offset = new_offset;
	spin_unlock(&RFS_I(old_inode)->write_lock);

	insert_inode_hash(old_inode);
	/* 
	 * we already reigsterd the inode to the transaction
	 * in the above move_to_dir() 
	 */
	rfs_mark_inode_dirty(old_inode);

	if (old_dir != new_dir) {
		if (S_ISDIR(old_inode->i_mode)) { 
			old_dir->i_nlink--;
			new_dir->i_nlink++;
		}
		new_dir->i_mtime = CURRENT_TIME;
		rfs_mark_inode_dirty_in_tr(new_dir);
	}

	old_dir->i_mtime = CURRENT_TIME;
	rfs_mark_inode_dirty_in_tr(old_dir);

end_log:
	/* If failed, RFS-log can't rollback due to media error */
	if (rfs_log_end(old_dir->i_sb, ret))
	{
		DEBUG(DL0, "rfs_log_end fails");
		return -EIO;
	}
	if (ret != 0)
		DEBUG(DL0, "rfs_rename fails(%d): old dir(%lu) %s -> new dir"
			"(%lu) %s", ret, old_dir->i_ino, old_qname->name, 
			new_dir->i_ino, new_qname->name);

	return ret;
}

/**
 *  generate a symlink file
 * @param dir		parent directory inode
 * @param dentry	dentry corresponding with new symlink file
 * @param symname	full link of target
 * @return		return 0 on success, errno on failure	 
 */
static int rfs_symlink(struct inode *dir, struct dentry *dentry, const char *symname)
{
	struct inode *inode = NULL;
	struct qstr target;
	int ret;
	int b_ENOSPC = FALSE;

	/* check the validity */
	if (IS_RDONLY(dir))
		return -EROFS;

	/* check the system files */
	target.name = (unsigned char *) strrchr(symname, SLASH);
	if (target.name == NULL)
		target.name = (unsigned char *) symname;
	else
		target.name++;

	target.len = strlen(symname) + 1;

	if (rfs_check_reserved_files(NULL, &target))
		return -EPERM;

	/* RFS-log : start create-symlink */
	if (rfs_log_start(dir->i_sb, RFS_LOG_SYMLINK, dir))
		return -EIO;


	/* create a symlink file */
	inode = rfs_new_inode(dir, dentry, TYPE_SYMLINK, S_IRWXUGO);
	if (IS_ERR(inode)) {
		ret = PTR_ERR(inode);
		goto end_log;
	} 
		
#ifdef RFS_FOR_2_6
	ret = page_symlink(inode, symname, target.len);
#else	
	ret = block_symlink(inode, symname, target.len);
#endif
	if (ret) {
		DEBUG(DL0, "fail to page_symlink(%d), target.leg:%d", ret,
				target.len);
		if (ret == -ENOSPC) {
			rfs_delete_entry(dir, inode);
			b_ENOSPC = TRUE;
		}
	}

end_log:
	/* for transaction sync, we remember the inode of symbolic link. */
	RFS_LOG_I(dir->i_sb)->symlink_inode = (ret) ? NULL : inode;

	/* If failed, RFS-log can't rollback due to media error */
	if (rfs_log_end(dir->i_sb, ret))
		return -EIO;

	if (TRUE == b_ENOSPC)
		iput(inode);

	if (!ret) /* attach inode to dentry */
		d_instantiate(dentry, inode); 

	return ret;
}

#include <linux/kdev_t.h>
static int rfs_mknod (struct inode *dir, struct dentry *dentry, int mode, dev_t rdev)
{
	struct inode *inode = NULL;
	unsigned int type = 0;
	int ret;
	
	/* not support except socket file or named FIFO */
	if (S_ISSOCK(mode))
		type = TYPE_SOCKET;
	else if (S_ISFIFO(mode))
		type = TYPE_FIFO;
	else 
		return -EINVAL;

	if (!new_valid_dev(rdev))
		return -EINVAL;

	/* RFS-log: start mknod */
	/* FIXME: Is new transaction type needed? */
	if (rfs_log_start(dir->i_sb, RFS_LOG_CREATE, dir))
		return -EIO;

	/* create a new inode */
	inode = rfs_new_inode(dir, dentry, type, mode);
	if (IS_ERR(inode))
		ret = PTR_ERR(inode);
	else
		ret = 0;

	if (rfs_log_end(dir->i_sb, ret))
		return -EIO;

	if (0 == ret) {
		/* attach inode to dentry */
		d_instantiate(dentry, inode);
	}

	return ret;
}

struct inode_operations rfs_dir_inode_operations = {
	.create		= rfs_create,
	.lookup		= rfs_lookup,
	.unlink		= rfs_unlink,
	.symlink	= rfs_symlink,
	.mkdir		= rfs_mkdir,
	.rmdir		= rfs_rmdir,
	.rename		= rfs_rename,
	.permission	= rfs_permission,
	.setattr	= rfs_setattr,
	.mknod		= rfs_mknod,
};

struct inode_operations rfs_dir_inode_operations_xattr = {
	.create		= rfs_create,
	.lookup		= rfs_lookup,
	.unlink		= rfs_unlink,
	.symlink	= rfs_symlink,
	.mkdir		= rfs_mkdir,
	.rmdir		= rfs_rmdir,
	.rename		= rfs_rename,
	.permission	= rfs_permission,
	.setattr	= rfs_setattr,
	.mknod		= rfs_mknod,
#ifdef CONFIG_RFS_FS_XATTR
	.setxattr	= generic_setxattr,
	.getxattr	= generic_getxattr,
	.listxattr	= rfs_xattr_list,
	.removexattr	= generic_removexattr,
#endif
};
