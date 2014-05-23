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
 * @version	RFS_1.3.1_b046-LinuStoreIII_1.1.0_b016-FSR_1.1.1_b109_Houdini
 * @file        drivers/tfsr/tfsr_base.c
 * @brief       This file is a basement for FSR adoption. It povides
 *              partition management, contexts management
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <FSR.h>
#include <FSR_OAM.h>
#include "tfsr_base.h"
#include <FSR_LLD_FlexOND.h>
#include <FSR_LLD_OneNAND.h>

/* This is the contexts to keep the specification of volume */
static FSRVolSpec      vol_spec[FSR_MAX_VOLUMES];
static FSRPartI        part_spec[FSR_MAX_VOLUMES];
static stl_info_t stl_info_list[FSR_MAX_VOLUMES * MAX_FLASH_PARTITIONS];

int (*sec_stl_delete)(dev_t dev, u32 start, u32 nums, u32 b_size) = NULL;
EXPORT_SYMBOL(sec_stl_delete);

/**
 * fsr_get_vol_spec - get a volume instance
 * @param volume        : a volume number
 */
FSRVolSpec *fsr_get_vol_spec(u32 volume)
{
	return &vol_spec[volume];
}

/**
 * fsr_get_part_spec - get a partition instance
 * @param volume        : a volume number
 */

FSRPartI *fsr_get_part_spec(u32 volume)
{
	return &part_spec[volume];
}

/**
 * fsr_get_vol_spec - get a STL info instance
 * @param volume        : a volume number
 & @param partno        : a partition number
 */
stl_info_t *fsr_get_stl_info(u32 volume, u32 partno)
{
	return (stl_info_list + (volume * partno) + partno);
}

/**
 * fsr_update_vol_spec - update volume & partition instance from the device
 * @param volume        : a volume number
 */
int fsr_update_vol_spec(u32 volume)
{
	int error;
	FSRVolSpec *vs;
	FSRPartI *pi;
	
	DEBUG(DL3,"TINY[I]: volume(%d)\n", volume);

	vs = fsr_get_vol_spec(volume);
	pi = fsr_get_part_spec(volume);
	
	memset(vs, 0x00, sizeof(FSRVolSpec));
	memset(pi, 0x00, sizeof(FSRPartI));
	
	error = FSR_BML_GetVolSpec(volume, vs, FSR_BML_FLAG_NONE);
	/* I/O error */
	if (error != FSR_BML_SUCCESS)
	{
		ERRPRINTK("FSR_BML_GetVolSpec func fail\n");
		return -1;
	}
	error = FSR_BML_GetFullPartI(volume, pi);
	/* I/O error */
	if (error != FSR_BML_SUCCESS)
	{
		ERRPRINTK("FSR_BML_GetFullPartI func fail\n");
		return -1;
	}
	
	DEBUG(DL3,"TINY[O]: volume(%d)\n", volume);

	return 0;
}

/**
 * fsr_read_partitions - read partition table into the device
 * @param volume        : a volume number
 * @param parttab       : buffer to store the partition table
 */
int fsr_read_partitions(u32 volume, BML_PARTTAB_T *parttab)
{
	int error;
	u32 partno;
	FSRPartI *pi;
	
	DEBUG(DL3,"TINY[I]: volume(%d)\n", volume);

	pi = fsr_get_part_spec(volume);
	
	/*could't find vaild part table*/
	if (!pi->nNumOfPartEntry) 
	{
		error = fsr_update_vol_spec(volume);
		/* I/O error */
		if (error) /*never action, because of low-formatting*/
		{
			DEBUG(DL1,"error(%x)", error);
		}
	}
	
	DEBUG(DL1,"pi->nNumOfPartEntry: %d", pi->nNumOfPartEntry);
	parttab->num_parts =  (int) pi->nNumOfPartEntry;
	for (partno = 0; partno < pi->nNumOfPartEntry; partno++) 
	{
		parttab->part_size[partno]      = (int) pi->stPEntry[partno].nNumOfUnits;
		parttab->part_id[partno]        = (int) pi->stPEntry[partno].nID;
		parttab->part_attr[partno]      = (int) pi->stPEntry[partno].nAttr;
	}
	
	DEBUG(DL3,"TINY[I]: volume(%d)\n", volume);

	return 0;
}

/**
 * fsr_init - [Init] initalize the fsr
 */
static int __init fsr_init(void)
{
	int error;
	
	DECLARE_TIMER;
	/*initialize global array*/
	START_TIMER();

	DEBUG(DL3,"TINY[I]\n");

#if defined(FSR_OAM_ALL_DBGMSG)
	FSR_DBG_SetDbgZoneMask(FSR_DBZ_ALL_ENABLE);
#endif
	
	FSR_OAM_MEMSET(vol_spec, 0x00,	sizeof(FSRVolSpec) * FSR_MAX_VOLUMES);
	FSR_OAM_MEMSET(part_spec, 0x00, sizeof(FSRPartI) * FSR_MAX_VOLUMES);
	
	error = FSR_BML_Init(FSR_BML_FLAG_NONE);
	STOP_TIMER("BML_Init");
	
	if (error != FSR_BML_SUCCESS && 
		error != FSR_BML_ALREADY_INITIALIZED) 
	{
		ERRPRINTK("Tiny BML_Init: error (%x)\n", error);
		ERRPRINTK("Check the PAM module\n");
		return -ENXIO;
	}

	/* call init bml block device */
	if(bml_block_init())
	{
		ERRPRINTK("Tiny BML_Block init: error (%x)\n", error);
		return -ENXIO;
	}

	DEBUG(DL3,"TINY[O]\n");

	return 0;
}

/**
 * fsr_exit - exit all 
 */
static void __exit fsr_exit(void)
{
	bml_block_exit();
}

module_init(fsr_init);
module_exit(fsr_exit);

