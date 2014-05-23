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
 *  @file	fs/rfs/fcache.c
 *  @brief	FAT cache & FAT table handling functions
 *
 *
 */

#include <linux/fs.h>
#include <linux/rfs_fs.h>
#include "rfs.h"

/*
 * structure for FAT cache (INCORE)
 */
struct rfs_fcache {
	sector_t blkoff;
	unsigned int f_dirty;
	struct buffer_head *f_bh;
	struct list_head list;
};

/*
 * global variable
 */
#define FAT_CACHE_SIZE_DEF	128
#define FAT_CACHE_SIZE_MIN	16

#define FAT_CACHE_HEAD(sb)	(&(RFS_SB(sb)->fcache_lru_list))
#define FAT_CACHE_ENTRY(p)	list_entry(p, struct rfs_fcache, list)

/************************************************************************/
/* FAT table manipulations						*/
/************************************************************************/

/**
 * get fcache size from user input of mount option
 * @param sb    super block
 * @return      return fcache size (always valid)
 */
static inline unsigned int __parse_fcache_size(struct super_block *sb)
{
	char *str_fcache_size = RFS_SB(sb)->options.str_fcache;
	unsigned int fcache_size = 0;
	unsigned long opt_num = 0;

	opt_num = simple_strtoul(str_fcache_size, NULL, 0);

	if (opt_num == 0) {
		/* invalid fcache size */
		fcache_size = FAT_CACHE_SIZE_DEF;
		DPRINTK("user input %s is invalid.\n", str_fcache_size);
	} else {
		/* valid fcache size */
		unsigned int fatsize_in_bits;
		int fatsize_in_blks;

		fatsize_in_bits =
			RFS_SB(sb)->num_clusters * RFS_SB(sb)->fat_bits;
		fatsize_in_blks =
			((fatsize_in_bits >> 3) + (sb->s_blocksize - 1))
			>> sb->s_blocksize_bits;

		if (opt_num >= fatsize_in_blks) {
			/* too large fcache size */
			fcache_size = fatsize_in_blks;
			DPRINTK("user input %s is larger than FAT area"
					"(%u blks).\n",
					str_fcache_size, fatsize_in_blks);
		} else {
			/* input is smaller than FAT area */
			if (fatsize_in_blks <= FAT_CACHE_SIZE_MIN) {
				/* FAT area is too small */
				fcache_size = fatsize_in_blks;
				DPRINTK("user input %s is too small.\n",
						str_fcache_size);
			} else if (opt_num < FAT_CACHE_SIZE_MIN) {
				/* input is smaller than minimum */
				fcache_size = FAT_CACHE_SIZE_MIN;
				DPRINTK("user input %s is smaller than"
						" minimum(%u).\n",
						str_fcache_size,
						FAT_CACHE_SIZE_MIN);
			} else {
				fcache_size = opt_num;
				DPRINTK("user input is %s.\n", str_fcache_size);
			}
		}
	}

	DPRINTK("Adjusted fcache size is %u blocks.\n", fcache_size);

	return fcache_size;
}

/**
 *  initialize internal fat cache entries and add them into fat cache lru list
 * @param sb	super block
 * @return	return 0 on success, -ENOMEM on failure
 */
int rfs_fcache_init(struct super_block *sb)
{
	struct rfs_fcache *array = NULL;
	int i, len;

	/* parsing fcache size */
	if (NULL == RFS_SB(sb)->options.str_fcache) {
		/* no mount option */
		RFS_SB(sb)->fcache_size = FAT_CACHE_SIZE_DEF;
	} else {
		/* user inputs fcache size by mount option */
		RFS_SB(sb)->fcache_size = __parse_fcache_size(sb);
	}

	len = sizeof(struct rfs_fcache) * RFS_SB(sb)->fcache_size;

	array = (struct rfs_fcache *) rfs_kmalloc(len, GFP_KERNEL, NORETRY);
	if (!array) /* memory error */
	{
		DEBUG(DL0, "memory allocation was failed!");
		return -ENOMEM;
	}

	INIT_LIST_HEAD(FAT_CACHE_HEAD(sb));

	for (i = 0; i < RFS_SB(sb)->fcache_size; i++)
       	{
		array[i].blkoff = (sector_t) NOT_ASSIGNED;
		array[i].f_dirty = FALSE;
		array[i].f_bh = NULL;
		list_add_tail(&(array[i].list), FAT_CACHE_HEAD(sb));
	}

	RFS_SB(sb)->fcache_array = array;

	return 0;
}

/**
 *  destroy fat cache entries and free memory for them
 * @param sb	super block
 */
void rfs_fcache_release(struct super_block *sb)
{
	struct list_head *p;
	struct rfs_fcache *fcache_p = NULL;

	/* release buffer head */
	list_for_each(p, FAT_CACHE_HEAD(sb)) {
		fcache_p = FAT_CACHE_ENTRY(p);
		brelse(fcache_p->f_bh);
	}

	/* release fcache */
	if (RFS_SB(sb)->fcache_array) {
		kfree(RFS_SB(sb)->fcache_array);
		RFS_SB(sb)->fcache_array = NULL;
	}
}

/**
 *  sync all fat cache entries if dirty flag of them are set
 * @param sb	super block
 * @param flush whether to flush or not
 *
 *  mark dirty flag of buffer head corresponding with all fat cache entries and nullify them
 */ 
void rfs_fcache_sync(struct super_block *sb, int flush)
{
	struct rfs_fcache *fcache_p;
	struct list_head *head, *p;

	head = FAT_CACHE_HEAD(sb);

	list_for_each(p, head) 
	{
		fcache_p = FAT_CACHE_ENTRY(p);
		if (fcache_p->f_dirty) 
		{
			rfs_mark_buffer_dirty(fcache_p->f_bh, sb);
			fcache_p->f_dirty = FALSE;

			if (unlikely(flush)) 
			{
				ll_rw_block(WRITE, 1, &fcache_p->f_bh);

				/* check write I/O result */
				wait_on_buffer(fcache_p->f_bh);
				if (!buffer_uptodate(fcache_p->f_bh))
				{
					RFS_BUG_CRASH(sb, "Fail to write\n");
				}
				rfs_set_bh_bit(BH_RFS_FAT, 
						&(fcache_p->f_bh->b_state));
			}
		}
	}
}

/**
 *  change status of fat cache entry
 * @param sb		super block
 * @param blkoff	block number to modify
 *
 * if blkoff is equal to block number(blkoff) fat cache entry, set a dirty flag
 */
static void __fcache_set_modify(struct super_block *sb, sector_t blkoff)
{
	struct rfs_fcache *fcache_p = NULL;
	struct list_head *p;

	list_for_each(p, FAT_CACHE_HEAD(sb)) {
		fcache_p = FAT_CACHE_ENTRY(p);
		if (fcache_p->blkoff == blkoff) {
			fcache_p->f_dirty = TRUE;
			return;
		}
	}
}

/**
 *  append fat cache entry to fcache lru list 
 * @param sb		super block
 * @param blkoff	block number of fat cache entry
 * @return 		return buffer head corrsponding with fat cache entry on success, NULL on failure
 */
static struct buffer_head *__fcache_add_entry(struct super_block *sb, sector_t blkoff)
{
	struct rfs_fcache *fcache_p;
	struct buffer_head *bh;
	struct list_head *head, *p;

	head = FAT_CACHE_HEAD(sb);

retry:
	p = head->prev; 
	fcache_p = FAT_CACHE_ENTRY(p);
	while (fcache_p->f_bh && fcache_p->f_dirty && p != head) {
		/* We flush the dirty FAT caches only in fat_sync() */
		p = p->prev;
		fcache_p = FAT_CACHE_ENTRY(p);
	}

	if (unlikely(p == head)) {
		/* there is no clean fat cache. So, perform fat cache flush */
		rfs_fcache_sync(sb, 1);
		goto retry;
	}

	brelse(fcache_p->f_bh);

	/*
	 * initialize fat cache with some effort
	 * because possible error of following sb_bread() makes it false
	 */
	fcache_p->f_bh = NULL;
	fcache_p->blkoff = (sector_t) NOT_ASSIGNED;

	bh = rfs_bread(sb, blkoff, BH_RFS_FAT);
	if (!bh) { /* I/O error */
		DPRINTK("can't get buffer head related with fat block\n");
		return NULL;
	}

	/* fill fcache */
	fcache_p->blkoff = blkoff;
	fcache_p->f_dirty = FALSE; /* just read */
	fcache_p->f_bh = bh;

	list_move(p, head);

	return bh;
}

/**
 *  lookup fat cache entry by using blkoff
 * @param sb		super block
 * @param blkoff	block number
 * @return		buffer head of fat cache entry
 *
 * if fat cache entry doesn't exist, get free fat entry and fill it
 */
static struct buffer_head *__fcache_get_entry(struct super_block *sb, sector_t blkoff)
{
	struct rfs_fcache *fcache_p = NULL;
	struct list_head *head, *p;

	/* find fcache entry included blkoff */
	head = FAT_CACHE_HEAD(sb);
	list_for_each(p, head) 
	{
		fcache_p = FAT_CACHE_ENTRY(p);
		if (fcache_p->blkoff == blkoff) 
		{
			/* Update LRU list */
			if (p != head->next)
				list_move(p, head);
			return fcache_p->f_bh; /* found */
		}
		/* stop search fcache list */
		if (fcache_p->blkoff == NOT_ASSIGNED)
			break;
	}

	return __fcache_add_entry(sb, blkoff);
}

/**
 *  read a block that contains a fat entry
 * @param sb		super block
 * @param[in, out] index index number of fat entry to calculate block number
 * @param[out] blocknr	block number corresponding to index	
 * @return 		buffer head of fat cache entry which include a fat entry
 */
static struct buffer_head *__fat_read_block(struct super_block *sb, unsigned int *index, sector_t *blocknr)
{
	sector_t block;
	loff_t offset = (loff_t) (*index);

	if (IS_FAT16(RFS_SB(sb)))
		offset = (offset << 1) + RFS_SB(sb)->fat_start_addr;
	else if (IS_FAT32(RFS_SB(sb)))
		offset = (offset << 2) + RFS_SB(sb)->fat_start_addr;
	else
	{
		RFS_BUG_CRASH(sb, "Unknown FAT type\n");
		return NULL;
	}

	block = (sector_t) (offset >> sb->s_blocksize_bits);

	*index = offset;
	*blocknr = block;

	return __fcache_get_entry(sb, block);
}

/**
 *  read a fat entry in fat table
 * @param sb		super block
 * @param location	location to read a fat entry 
 * @param[out] content	content	cluster number which is saved in fat entry
 * @return		return 0 on sucess, EIO on failure
 */
int rfs_fat_read(struct super_block *sb, unsigned int location, unsigned int *content)
{
	struct buffer_head *bh = NULL;
	unsigned int index = location;
	sector_t dummy;

	if (IS_INVAL_CLU(RFS_SB(sb), location)) 
	{
		/* out-of-range input */
		RFS_BUG_CRASH(sb, "invalid cluster number(%u)\n", location);
		return -EINVAL;
	}

	if (!(bh = __fat_read_block(sb, &index, &dummy))) 
	{
	       	/* I/O error */
		RFS_BUG_CRASH(sb, "can't get buffer head related with fat "
				"block for idx %u\n", index);
		return -EIO; 
	}
	rfs_set_bh_bit(BH_RFS_FAT, &bh->b_state);

	index &= (sb->s_blocksize - 1);
	if (IS_FAT16(RFS_SB(sb))) 
	{
		*content = GET16(((u16 *)bh->b_data)[index >> 1]);

		if (*content >= 0xFFF8)
			*content = CLU_TAIL; /* last cluster of fat chain */

	}
       	else if (IS_FAT32(RFS_SB(sb))) 
	{
		*content = GET32(((u32 *)bh->b_data)[index >> 2]);
		*content = *content & 0x0FFFFFFF;

		if (*content >= 0xFFFFFF8) 
			*content = CLU_TAIL; /* last cluster of chain */
	}
       	else
	{
		RFS_BUG_CRASH(sb, "Unknown FAT type\n");
		return -EINVAL;
	}

	/* sanity check */
	if ((*content != CLU_FREE) && (*content != CLU_TAIL)) 
	{
		if (IS_INVAL_CLU(RFS_SB(sb), *content)) 
		{
			dump_bh(sb, bh, "at %s:%d", __func__, __LINE__);
			RFS_BUG_CRASH(sb, "invalid contents(%u, %u)\n", location,
				        *content);
			return -EIO;
		}
	}

	return 0; 
}

/**
 *  write a fat entry in fat table 1(does not write fat table 2).
 * @param sb		super block
 * @param location	location to write a fat entry 
 * @param content	next cluster number or end mark of fat chain
 * @return		return 0 on sucess, EIO or EINVAL on failure
 * @pre			content shouldn't have 0 or 1 which are reserved
 */
int rfs_fat_write(struct super_block *sb, unsigned int location, unsigned int content)
{
	struct buffer_head *bh;
	unsigned int index = location;
	sector_t block;

	if (IS_INVAL_CLU(RFS_SB(sb), location)) 
	{
		/* out-of-range input */
		RFS_BUG_CRASH(sb, "invalid cluster number(%u, %u)\n", location, 
				content);
		return -EINVAL;
	}

	/* sanity check */
	if ((content != CLU_FREE) && (content != CLU_TAIL)) 
	{
		if (IS_INVAL_CLU(RFS_SB(sb), content)) 
		{
			RFS_BUG_CRASH(sb, "invalid contents(%u, %u)\n", content, 
					content);
			return -EIO;
		}
	}

	bh = __fat_read_block(sb, &index, &block);
	if (!bh) 
	{
	       	/* I/O error */
		DPRINTK("Can't get buffer head related with fat block\n"); 
		return -EIO;  
	}

	index &= (sb->s_blocksize - 1);
	if (IS_FAT16(RFS_SB(sb))) 
	{
		content &= 0x0000FFFF;
		SET16(((u16 *)bh->b_data)[index >> 1], content);
	}
       	else if (IS_FAT32(RFS_SB(sb))) 
	{
		unsigned int value;
		
		value = GET32(((u32 *)bh->b_data)[index >> 2]);
		content = (value & 0xF0000000) | (content & 0x0FFFFFFF);
		SET32(((u32 *)bh->b_data)[index >> 2], content);
	}
       	else
	{
		RFS_BUG_CRASH(sb, "Unknown FAT type\n");
		return -EINVAL;
	}

	__fcache_set_modify(sb, block);
	return 0;
}


/************************************************************************/
/* Cluster manipulations						*/
/************************************************************************/

/**
 *  burst read blocks and insert them to fat cache
 * @param sb super block
 * @param blocknr the start block number of burst read
 * @param count the number of block to be read
 * @return return the number of blocks read on success, errno on failure
 */
static int __fcache_burst_read(struct super_block *sb, sector_t blocknr, 
		int count)
{
	struct buffer_head **bhs = NULL;
	struct list_head *head, *p;
	struct rfs_fcache *fcache_p;
	unsigned int i;

	if (!count)
		return 0;

	if (count < 0) {
		DPRINTK("Err in reading burst fat table\n");
		return -EIO;
	}

	if (count > RFS_SB(sb)->fcache_size)
		count = RFS_SB(sb)->fcache_size;


	bhs = rfs_kmalloc(sizeof(struct buffer_head *) * count, GFP_KERNEL,
				NORETRY);

	if (bhs == NULL)
		return -ENOMEM;

	/* release fat cache for memory usage */
	head = FAT_CACHE_HEAD(sb);
	for (i = 0, p = head->next; i < count; i++, p = p->next) {
		fcache_p = FAT_CACHE_ENTRY(p);
		if (fcache_p->f_bh) {
			if (fcache_p->f_dirty) {
				mark_buffer_dirty(fcache_p->f_bh);
				fcache_p->f_dirty = FALSE;
			}

			DEBUG(DL3, "relesing buffer (%lu : %p)\n",
				       (unsigned long) fcache_p->blkoff, 
				       fcache_p->f_bh);

			brelse(fcache_p->f_bh);
			fcache_p->f_bh = NULL;
			fcache_p->blkoff = NOT_ASSIGNED;
		}
	}

	for (i = 0; i < count; i++) {
		bhs[i] = sb_getblk(sb, blocknr + i);
		DEBUG(DL3, "reading buffer (%lu : %p)\n", 
				(unsigned long) (blocknr + i), bhs[i]);
		if (!bhs[i]) {
			DPRINTK("Err in getting bh\n");
			kfree(bhs);
			return -EIO;
		}
	}

	/* burst read  */
	ll_rw_block(READ, count, bhs);

	head = FAT_CACHE_HEAD(sb);
	p = head->next; 

	/* add buffer in fat cache */
	for (i = 0; i < count; i++) 
	{
		/* check I/O completion */
		wait_on_buffer(bhs[i]);
		if (!buffer_uptodate(bhs[i])) 
		{
			RFS_BUG_CRASH(sb, "Fail to read\n");
			kfree(bhs);
			return -EIO;
		}
		rfs_set_bh_bit(BH_RFS_FAT, &bhs[i]->b_state);

		fcache_p = FAT_CACHE_ENTRY(p);

		fcache_p->f_bh = bhs[i];
		fcache_p->blkoff = blocknr + i;
		fcache_p->f_dirty = FALSE;

		p = p->next;
	}

	kfree(bhs);
	return count;
}

/**
 *  count all used clusters in file system
 * @param sb super block
 * @param[out] used_clusters the number of used clusters in volume 
 * @return return 0 on success, errno on failure
 *
 * cluster 0 & 1 are reserved according to the fat spec
 */
int rfs_count_used_clusters(struct super_block *sb, unsigned int *used_clusters)
{
	sector_t start_blocknr;
	unsigned int i, clu;
	unsigned int count = 2; /* clu 0 & 1 are reserved */
	unsigned int fat_bits;
	unsigned int max_index;
	int read_cache;
	int fat_blocks;
	int err;

	/* make fat bits for multiply */
	if (IS_FAT16(RFS_SB(sb))) {
		fat_bits = 1;
	} else if (IS_FAT32(RFS_SB(sb))) {
		fat_bits = 2;
	} else {
		RFS_BUG("Unknown FAT type\n");
		return -EIO;
	}

	/* get start blocknr and size of 1st fat table */
	start_blocknr = 
		(sector_t) (RFS_SB(sb)->fat_start_addr >> sb->s_blocksize_bits);
	fat_blocks = ((RFS_SB(sb)->num_clusters << fat_bits)
			+ sb->s_blocksize - 1) >> sb->s_blocksize_bits;

	DEBUG(DL3, "start blocknr : %lu, numof fat blocks : %u\n",
			(unsigned long) start_blocknr, fat_blocks);
	fat_lock(sb);

	/* set the maximum read cluster index */
	max_index = 0;

	for (i = VALID_CLU; i < RFS_SB(sb)->num_clusters; i++) {
		if (i >= max_index) {
			/* read burst the next part of fat table */
			read_cache = __fcache_burst_read(sb, start_blocknr,
					(fat_blocks > RFS_SB(sb)->fcache_size) ?
					RFS_SB(sb)->fcache_size :
					fat_blocks);

			if (read_cache < 0) {
				DPRINTK("Err(%d) in counting free clusters\n",
						read_cache);
				fat_unlock(sb);
				return read_cache;
			}

			/* update vars for next burst read */
			fat_blocks -= read_cache;
			max_index += (sb->s_blocksize * read_cache) >> fat_bits;

			DEBUG(DL3, "read burst %uth from %lu (%u entries)\n",
					read_cache, 
					(unsigned long) start_blocknr, 
					max_index);

			start_blocknr += read_cache;
		}

		/* expect the hit on fat cache due to burst read */
		err = rfs_fat_read(sb, i, &clu);
		if (err) {
			fat_unlock(sb);
			DPRINTK("can't read a fat entry (%u)\n", i);
			return err;
		}

		if (clu)
			count++;
	}

	*used_clusters = count;

	fat_unlock(sb);

	return 0;
}
