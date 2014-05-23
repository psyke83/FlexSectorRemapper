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
 *  @file       fs/rfs/xattr_trusted.c
 *  @brief      handler for trusted extended attributes.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/rfs_fs.h>
#include <linux/xattr.h>
#include "rfs.h"
#include "xattr.h"

static int
rfs_xattr_trusted_get(struct inode *inode, const char *name, void *buffer, size_t size)
{
	struct rfs_xattr_param param;
	unsigned int size_read;

	if (strcmp(name, "") == 0)
	{
		DEBUG(DL0, "xattr's name is NULL string");
		return -EINVAL;
	}
	param.id = RFS_XATTR_NS_TRUSTED;
	param.name = (char *)name;
	param.value = (void *)buffer;
	param.value_length = size;
	param.set_flag = XATTR_SET_FLAG_DUMMY;

	return rfs_xattr_get(inode, &param, &size_read);
}

static int
rfs_xattr_trusted_set(struct inode *inode, const char *name, const void *value, size_t size, int flags)
{
	struct rfs_xattr_param param;

	if (strcmp(name, "") == 0)
	{
		DEBUG(DL0, "xattr's name is NULL string");
		return -EINVAL;
	}

	param.id = RFS_XATTR_NS_TRUSTED;
	param.name = (char *)name;
	param.value = (void *)value;
	param.value_length = size;
	param.set_flag = flags;

	if (strcmp(value, "") == 0)
	{
		/* remove entry if pValue is NULL */
		 return rfs_xattr_delete(inode, &param);
	}
	else
	{
		DEBUG(DL2, "##id:TRUSTED,name:%s,value:%s", name, (char*)value);
		return rfs_xattr_set(inode, &param);
	}
}

struct xattr_handler rfs_xattr_trusted_handler = 
{
	.prefix = XATTR_TRUSTED_PREFIX,
	.get    = rfs_xattr_trusted_get,
	.set    = rfs_xattr_trusted_set,
	.list   = NULL,
};
