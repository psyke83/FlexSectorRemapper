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
 *  @file       fs/rfs/misc.c
 *  @brief      misc. such as debugging code
 */

#include <linux/fs.h>
#include <linux/rfs_fs.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <asm/uaccess.h>

#include "rfs.h"

#ifdef CONFIG_PROC_FS
#define RFS_PROC_DIR	"rfs_fs"

/* strings for rfs_state_bits in linux/rfs_fs.h */
char * rfs_state_string[] =
{
	"BH_RFS_PRIVATE_START",
	"BH_RFS_LOG_START",
	"BH_RFS_LOG_COMMIT",
	"BH_RFS_LOG",
	"BH_RFS_FAT",
	"BH_RFS_ROOT",
	"BH_RFS_DIR",
	"BH_RFS_MBR",
	"BH_RFS_XATTR",
	"BH_RFS_DATA",
	"BH_RFS_HOT",
	"BH_RFS_PRIVATE_END"
};

/* proc dir entry instance */
struct proc_dir_entry *rfs_proc_dir = NULL;

/* variable for hot and cold boundary */
int g_bh_hot_boundary = BH_RFS_XATTR;

/**
 *  returns rfs_state_string for their state
 * @param	state	state
 * @param[out]	out	str ptr
 * @return	return 0 on success, error on failure	
 */
int do_get_state_string(int state, char **out)
{
	int idx;

	if (out == NULL)
		return -EINVAL;

	if ((BH_RFS_PRIVATE_START >= state) || (BH_RFS_PRIVATE_END <= state))
		return -ERANGE;

	idx = state - BH_RFS_PRIVATE_START;
	*out = rfs_state_string[idx];

	return 0;
}

int (*rfs_get_state_string)(int state, char **out) = NULL;
EXPORT_SYMBOL(rfs_get_state_string); /* export for store debug routine */

#ifdef RFS_INTERNAL_PROC_HOT_BOUNDARY 
/**
 *  read function for "hot_boundary" proc entry 
 * @return	return length of output string on success, error on failure	
 */
static int read_hot_boundary(char *page, char **start, off_t off,
		                int count, int *eof, void *data_unused)
{
	char * state_string;
	int len = 0, i;

	*eof = 0;
	len += snprintf(page + len, PAGE_SIZE - len, 
		"RFS has private bh state as below :\n");

	/* iterate from i == BH_RFS_LOG_START to i == BH_RFS_DATA */
	for (i = BH_RFS_PRIVATE_START + 1; i < BH_RFS_HOT; i++)
	{
		if (rfs_get_state_string != NULL)
		{
			if (rfs_get_state_string(i, &state_string))
			{
				continue;
			}
			len += snprintf(page + len, PAGE_SIZE - len, "\t%s:%d ",
				       state_string, i);

			if (g_bh_hot_boundary == i)
			{
				len += snprintf(page + len, PAGE_SIZE - len,
						"\t<-- HOT data to here \n");
			}
			else if (g_bh_hot_boundary + 1== i)
			{
				len += snprintf(page + len, PAGE_SIZE - len,
						"\t<-- COLD data from here \n");
			}
			else
			{
				len += snprintf(page + len, PAGE_SIZE - len,
						"\n");		
			}
		}
	}

	if (!rfs_get_state_string(g_bh_hot_boundary, &state_string))
	{
		len += snprintf(page + len, PAGE_SIZE - len, 
				"current boundary of hot & cold data :%s\n", 
				state_string);
	}

	return len;
}

/**
 *  write function for "hot_boundary" proc entry 
 * @return	return length of input string on success, error on failure	
 */
static int write_hot_boundary(struct file *file, const char *user_buffer,
		                unsigned long count, void *data)
{
	char *state_string;
	char *str;
	int tmp, ret, i;

	str = kmalloc(count + 1, GFP_KERNEL);
	if (NULL == str)
	{
		DPRINTK("memory allocation error");
		return -ENOMEM;
	}
	str[count] = '\0';

	ret = copy_from_user(str, user_buffer, count);
	/* memory error */
	if (ret)
	{
		DPRINTK("copy_from_user error !!!\n");
		ret = -EFAULT;
		goto out;
	}

	tmp = simple_strtoul(str, NULL, 0);
	DEBUG(DL3, "tmp value:%d\n",tmp);

	/* check return value of simple_strtoul() */
	if ((BH_RFS_PRIVATE_START >= tmp) || (BH_RFS_HOT <= tmp))
	{
		DPRINTK("%d is invalid boundary of hot & cold data!!!\n", tmp);
		DPRINTK("range BH_RFS_LOG_START(%d) ~ BH_RFS_DATA(%d)\n",
				BH_RFS_LOG_START, BH_RFS_DATA);
		ret = -ERANGE;
		goto out;
	}

	g_bh_hot_boundary = tmp;

	printk("RFS has private bh state as below :\n");
	/* iterate from i == BH_RFS_LOG_START to i == BH_RFS_DATA */
	for (i = BH_RFS_PRIVATE_START + 1; i < BH_RFS_HOT; i++)
	{
		if (rfs_get_state_string != NULL)
		{
			if (rfs_get_state_string(i, &state_string))
			{
				continue;
			}			
			printk("\t%s:%d ", state_string, i);

			if (g_bh_hot_boundary == i)
			{
				printk("\t<-- HOT data to here \n");
			}
			else if (g_bh_hot_boundary + 1== i)
			{
				printk("\t<-- COLD data from here \n");
			}
			else
			{
				printk("\n");
			}
		}
	}

	if (!rfs_get_state_string(g_bh_hot_boundary, &state_string))
	{
		printk("current boundary of hot & cold data :%s\n", 
				state_string);
	}

	ret = count;
out:
	kfree(str);
	return ret;
}
#endif /* (RFS_INTERNAL_PROC_HOT_BOUNDARY) */

static struct proc_dir_entry *add_proc_entry(struct proc_dir_entry *p_dir,
		const char *name,
		read_proc_t *read_proc,
		write_proc_t *write_proc)
{
	struct proc_dir_entry *new_entry = NULL;

	new_entry = create_proc_entry(name, 0, p_dir);
	if (new_entry)
	{
		new_entry->read_proc = read_proc;
		new_entry->write_proc = write_proc;
	}

	return new_entry;
}

static int read_version(char *page, char **start, off_t off,
		                int count, int *eof, void *data_unused)
{
	int len = 0;
	
	*eof = 0;

	len += snprintf(page + len, PAGE_SIZE - len, "%s\n", RFS_VERSION_STRING);

	return len;
}

static int write_version(struct file *file, const char *user_buffer,
		                unsigned long count, void *data)
{
	return count;
}

int init_rfs_proc(void)
{
	rfs_proc_dir = proc_mkdir(RFS_PROC_DIR, NULL);
	if (!rfs_proc_dir)
		return -EINVAL;

	add_proc_entry(rfs_proc_dir, "version", read_version, write_version);
#ifdef RFS_INTERNAL_PROC_HOT_BOUNDARY
	add_proc_entry(rfs_proc_dir, "hot_boundary", read_hot_boundary,
		       write_hot_boundary);
#endif /* (RFS_INTERNAL_PROC_HOT_BOUNDARY) */
	rfs_get_state_string = do_get_state_string;

	return 0;
}

void exit_rfs_proc(void)
{
	if (rfs_proc_dir)
	{
		remove_proc_entry("version", rfs_proc_dir);
#ifdef RFS_INTERNAL_PROC_HOT_BOUNDARY
		remove_proc_entry("hot_boundary", rfs_proc_dir);
#endif /* (RFS_INTERNAL_PROC_HOT_BOUNDARY) */
		remove_proc_entry(RFS_PROC_DIR, NULL);
	}
	rfs_get_state_string = NULL;
}

#ifdef RFS_CLUSTER_CHANGE_NOTIFY
/* cluster usage chage notify callback function pointer */
FP_CLUSTER_USAGE_NOTIFY *gfp_cluster_usage_notify = NULL;

/**
 *  register callback function for cluster usage notify
 * @param pf	function ptr
 */
void register_cluster_usage_notify(FP_CLUSTER_USAGE_NOTIFY *p_func)
{
	if (p_func)
	{
		DEBUG(DL0, "volume usage notify callback function registering "
				"from(%p) to(%p)", 
				gfp_cluster_usage_notify, p_func);
		gfp_cluster_usage_notify = p_func;
	}

	return;
}
EXPORT_SYMBOL(register_cluster_usage_notify);

void call_cluster_usage_notify(struct super_block *sb)
{
	if (gfp_cluster_usage_notify && RFS_SB(sb)->cluster_usage_changed)
	{
		/* call notify function*/
		gfp_cluster_usage_notify(sb);

		/* reset changed flag */
		RFS_SB(sb)->cluster_usage_changed = FALSE;
	}

	return;
}

void set_cluster_usage_notify(struct super_block *sb, int changed)
{
	if (changed != 0)
		RFS_SB(sb)->cluster_usage_changed = TRUE;
	else
		RFS_SB(sb)->cluster_usage_changed = FALSE;
}

#endif // RFS_CLUSTER_CHANGE_NOTIFY
#endif //#ifdef CONFIG_PROC_FS

void printout_memory(char *data, int size);
void dump_bh(struct super_block *sb, struct buffer_head *bh, const char *fmt, ...);
void dump_inode(struct inode *inode);
void dump_entry(struct super_block *sb, struct rfs_dir_entry *ep, struct buffer_head *bh, loff_t offset, const char *fmt, ...);

void
dump_entry(struct super_block *sb, struct rfs_dir_entry *ep, struct buffer_head *bh, loff_t offset, const char *fmt, ...)
{
	va_list args;
	int i;

#ifdef RFS_FOR_2_4
	char message_buf[1024];

	printk(KERN_CRIT "[RFS-fs] Dump directory entry: ");
	va_start(args, fmt);
	vsprintf(message_buf, fmt, args);
	va_end(args);

	printk("%s\n", message_buf);
#else
	va_start(args, fmt);
	printk(KERN_CRIT "[RFS-fs][%02x:%02x] Dump directory entry: ", MAJOR(sb->s_dev), MINOR(sb->s_dev));
	vprintk(fmt, args);
	printk("\n");
	va_end(args);
#endif

	printk("\tname:\t");
	for(i = 0; i < 11; i++)
		printk("%c ", ep->name[i]);
	printk("\n");

	printk("\tattr:\t0x%x\n"
			"\tsysid:\t0x%02X\n", ep->attr, ep->sysid);
	printk("\tcmsec: ctime: cdate = \t%d \t0x%x \t0x%x\n", \
			ep->cmsec, GET16(ep->ctime), GET16(ep->cdate));
	printk("\tadate: mtime: mdate = \t0x%x \t0x%x \t0x%x\n", \
			GET16(ep->adate), GET16(ep->mtime), GET16(ep->mdate));
	printk("\tstart_clu:\t0x%04x %04x (%u)\n",
			GET16(ep->start_clu_hi), GET16(ep->start_clu_lo),
			(unsigned int) ((unsigned long)GET16(ep->start_clu_lo) |
				GET16(ep->start_clu_hi) << 16));
	printk("\tsize:\t%d\n", GET32(ep->size));

	dump_bh(sb, bh, "");

	return;
}

void dump_bh(struct super_block *sb, struct buffer_head *bh, const char *fmt, ...)
{
	va_list args;

#ifdef RFS_FOR_2_4
	char message_buf[1024];

	printk(KERN_CRIT "[RFS-fs] Dump buffer head: ");
	va_start(args, fmt);
	vsprintf(message_buf, fmt, args);
	va_end(args);

	printk("%s\n", message_buf);
#else
	va_start(args, fmt);
	printk(KERN_CRIT "[RFS-fs][%02x:%02x] Dump buffer head: ", MAJOR(sb->s_dev), MINOR(sb->s_dev));
	vprintk(fmt, args);
	printk("\n");
	va_end(args);
#endif

	printk("\tb_blocknr=%llu b_state=0x%08lx b_size=%zu\n",
			(unsigned long long) bh->b_blocknr,
			bh->b_state, bh->b_size);
	if (bh->b_page)
		printk("\tb_page.index=%lu b_data=%p\n",
				bh->b_page->index, bh->b_data);

	if (bh->b_data)
	{
		printout_memory(bh->b_data, bh->b_size);
	}

	return;
}

void dump_fat(struct super_block *sb, unsigned int clus_idx)
{
	return;
}

void printout_memory(char *data, int size)
{
	int i = 0;

	for (; i < size; i++)
	{
		if (0 == (i % 32))
		{
			if (i == 0)
				printk("%08x: ", i);
			else
				printk("\n%08x: ", i);
		}
		else if (0 == (i % 16))
			printk("\n        : ");

		printk("%02x ", data[i] & 0x0FF);
	}
	printk("\n");

	return;
}

void
dump_inode(struct inode *inode)
{
	struct rfs_inode_info *rfsi = NULL;

	if (!inode)
	{
		printk("inode is NULL\n");
		return;
	}

	printk(KERN_CRIT "[RFS-fs][%02x:%02x] Dump inode: \n", MAJOR(inode->i_sb->s_dev), MINOR(inode->i_sb->s_dev));

	printk("\tinode=%p\n", inode);
	printk("\ti_ino=%lu i_count=%u i_nlink=%u i_size=%llu\n",
			inode->i_ino, atomic_read(&inode->i_count), inode->i_nlink,
			(unsigned long long) inode->i_size);
	printk("\ti_blkbits=%u i_blocks=%lu i_mode=0%o i_flags=%x\n",
			inode->i_blkbits, inode->i_blocks, inode->i_mode,
			inode->i_flags);

	rfsi = RFS_I(inode);
	printk("RFS_I:\tstart_clu=%u p_start_clu=%u index=%u last_clu=%u\n",
			rfsi->start_clu, rfsi->p_start_clu,
			rfsi->index, rfsi->last_clu);
	printk("\tmmu_private=%llu", (unsigned long long) rfsi->mmu_private);
	printk("\tblock_nr=%llu offset=%llu\n",
			(unsigned long long) rfsi->block_nr,
			(unsigned long long) rfsi->offset);

	if (rfsi->parent_inode)
	{
		struct inode *pinode = rfsi->parent_inode;
		struct rfs_inode_info *prfsi = RFS_I(pinode);

		printk("parent_inode=%p\n", rfsi->parent_inode);
		printk("\ti_ino=%lu i_count=%u i_nlink=%u i_size=%llu\n",
				pinode->i_ino, atomic_read(&pinode->i_count), pinode->i_nlink,
				(unsigned long long) pinode->i_size);
		printk("\ti_blkbits=%u i_blocks=%lu i_mode=0%o i_flags=%x\n",
				pinode->i_blkbits, pinode->i_blocks, pinode->i_mode,
				pinode->i_flags);
		printk("P.RFS_I:\tstart_clu=%u p_start_clu=%u index=%u last_clu=%u\n",
				prfsi->start_clu, prfsi->p_start_clu,
				prfsi->index, prfsi->last_clu);
		printk("\tblock_nr=%llu offset=%llu\n",
				(unsigned long long) prfsi->block_nr,
				(unsigned long long) prfsi->offset);
	}

	return;
}

// end of file

