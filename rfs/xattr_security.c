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
 *  @file       fs/rfs/xattr_security.c
 *  @brief      handler for storing security labels as extended attributes.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/rfs_fs.h>
#include <linux/xattr.h>
#include <linux/security.h>
#include "rfs.h"
#include "xattr.h"

static int
rfs_xattr_security_get(struct inode *inode, const char *name, void *buffer, size_t size)
{
	struct rfs_xattr_param param;
	unsigned int    size_read = 0;
	int ret;

	if (!test_opt(inode->i_sb, EA))
	{
		return -EOPNOTSUPP;
	}

	if (strcmp(name, "") == 0)
	{
		DEBUG(DL0, "xattr's name is NULL string");
		return -EINVAL;
	}

	param.id = RFS_XATTR_NS_SECURITY;
	param.name = (char *)name;
	param.value = (void *)buffer;
	param.value_length = size;
	param.set_flag = XATTR_SET_FLAG_DUMMY;

	ret = rfs_xattr_get(inode, &param, &size_read);
	if (ret)
	{
		DEBUG(DL1, "fail to rfs_xattr_get(%d)", ret);
		return ret;
	}
	
	return size_read;
}

static int
rfs_xattr_security_set(struct inode *inode, const char *name, const void *value, size_t size, int flags)
{
	struct rfs_xattr_param param;

	if (!test_opt(inode->i_sb, EA))
	{
		return -EOPNOTSUPP;
	}

	if (strcmp(name, "") == 0)
	{
		DEBUG(DL0, "xattr's name is NULL string");
		return -EINVAL;
	}

	param.id = RFS_XATTR_NS_SECURITY;
	param.name = (char *)name;
	param.value = (void *)value;
	param.value_length = size;
	param.set_flag = flags;

	DEBUG(DL3, "id:%d,name:%s,value:%p[%s],value_length:%d,flag:%d",
			param.id, param.name, param.value, 
			(param.value == NULL) ? "NULL" : (char *)param.value,
			param.value_length, param.set_flag);


	if (NULL != value && 0 != size)
	{
		DEBUG(DL2, "##id:SECURITY,name:%s,value:%s", name, (char*)value);
		return rfs_xattr_set(inode, &param);
	}
	else
	{
		/* remove entry if value is NULL or value_legth is Zero */
		 return rfs_xattr_delete(inode, &param);
	}
}

int
rfs_init_security(struct inode *inode, struct inode *dir)
{
	int err;
	struct rfs_xattr_param param;

	if (!test_opt(inode->i_sb, EA))
	{
		return 0;
	}

	err = security_inode_init_security(inode, dir, &param.name, 
			&param.value, &param.value_length);
	if (err) 
	{
		if (err == -EOPNOTSUPP)
			return 0;
		return err;
	}

	param.id = RFS_XATTR_NS_SECURITY;
	param.set_flag = XATTR_SET_FLAG_CREATE;

	/* transaction already start. call directly rfs_do_xattr_set().*/
	err = rfs_do_xattr_set(inode, &param);
	kfree(param.name);
	kfree(param.value);

	return err;
}

struct xattr_handler rfs_xattr_security_handler = 
{
	.prefix = XATTR_SECURITY_PREFIX,
	.get    = rfs_xattr_security_get,
	.set    = rfs_xattr_security_set,
	.list   = NULL,
};

