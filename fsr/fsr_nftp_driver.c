/*****************************************************************************/
/*  @ project : NFTP (NAND FLASH TEST PROJECT)                               */
/*  @ version NFTP 1.0Ver. (FSR)                                             */
/*  @ file  nftp_fsr_driver.c                                                */
/*  @ brief This file is NFTP linux driver part.                             */
/*      NFTP provide stable NAND Flash Device to doing several test method.  */
/*                                                                           */
/*---------------------------------------------------------------------------*/
/* Update History                                                            */
/*                                                                           */
/*  01-SEP-2008 [NamJae Jeon] : Implement and First Writing.                 */
/*  08-SEP-2008 [YongSik Kim] : Implement FSR driver.                        */
/*  30-JUNE-2009 [NamJae Jeon] : add Read Disturb Testcase, ECC testcase,    */
/*	 Wearleveling testcase                                                   */
/*  22-SEP-2010 [Sangyeon Park] : 2.6.4030 porting                           */
/*****************************************************************************/
#define __KERNEL_SYSCALLS__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/ioctl.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>

#include "fsr_base.h"
#include "crc32.c"
#include "FSR.h"
#include "Core/STL/FSR_STL_Config.h"
#include "Core/STL/FSR_STL_CommonType.h"
#include "Core/STL/FSR_STL_Types.h"
#include "Core/STL/FSR_STL_Common.h"

//#define NFTP_DEBUG 0
extern struct semaphore fsr_mutex;

//#define FSR_TEST_PAGE_SIZE 8192
#define FSR_TEST_PAGE_SIZE 2048
#define FSR_TEST_SECTOR_SIZE 2*FSR_SECTOR_SIZE
#define FSR_BML 137
#define FSR_STL 138
#define CRC_CHECK_ON 1
#define CRC_CHECK_OFF 0 
#define MAX_COUNT 3000
#define TRUE 1
#define FALSE 0
#define k_time 1000
#define ECCbit 5
#define page_per_block_ond 64
#define page_per_block_mlc 128
#define ER_ON 1
#define ER_OFF 0

struct _nftp_report
{
	u8 bml_read;
	u8 bml_read_write;
	u8 stl_read;
	u8 stl_read_write;
	u8 bml_overwrite;
	u8 stl_overwrite;
	u8 bml_kthread;
	u8 stl_kthread;
	u8 ecc;
};

struct fsr_test_info
{
	u32 volume;
	u32 minor;
	u32 part_type;
	u32 firstvbn;
	u32 endvbn;
	u32 part_first_page;
	u32 part_end_page;
	u32 page_per_unit;
	/* Variables for STL test */
	u32 part_id;
	u32 part_first_sector;
	u32 part_end_sector;
	u32 nScts;
	/* CRC variables */
	u32 nftp_write_index;
	u8 crc_check;
	u32 flash_w_crc;
	u32 flash_r_crc;	
	u32 thread_count;	
	void (*nftp_loop_task)(struct fsr_test_info *part_info);	
};

struct _nftp_report *nftp_report;
u8 nftp_bml_pattern[2][FSR_TEST_PAGE_SIZE];
u8 nftp_stl_pattern[2][FSR_TEST_SECTOR_SIZE];
int nftp_flag;

STLClstObj     *gpstSTLClstObj[MAX_NUM_CLUSTERS] = {NULL,};


void set_write_pattern(u32 part_type)
{
	if(part_type == FSR_BML)
	{
		memset(nftp_bml_pattern[0], 0xAA , FSR_TEST_PAGE_SIZE);
		memset(nftp_bml_pattern[1], 0x55 , FSR_TEST_PAGE_SIZE);
	}
	else if(part_type == FSR_STL)
	{
		memset(nftp_stl_pattern[0], 0xAA , FSR_TEST_SECTOR_SIZE);
		memset(nftp_stl_pattern[1], 0x55 , FSR_TEST_SECTOR_SIZE);	
	}
}		

/**
 * spin wheel dumping image
 */
inline void spin_wheel (void)
{
	static int p = 0;
	static char w[] = "\\/-";
	
	printk (KERN_ALERT "\r%c", w[p]);
	(++p == 3) ? (p = 0) : 0;
}

/**
 *  Show percent
 * @param total total value 
 * @param cur current value 
 */
inline void percent (int total, int cur)
{
	int val;

	if (cur == 0)
	{
		printk(KERN_ALERT "\r %4d%c", cur, '%');
	}
	else 
	{
		val = (cur * 100)/total;
		printk(KERN_ALERT "\r %4d%c", val, '%');
	}
}

/*                                                                           
* Name : crc_check  
* Description :                                                                            
*																			 
*/   
int crc_check(struct fsr_test_info *part_info, u32 pos, u8 *buf)
{
	if(part_info->flash_r_crc != part_info->flash_w_crc)
	{
		int j;
//#ifdef NFTP_DEBUG
		printk(KERN_ALERT "CRC error!! Write crc : %d, Read crc : %d\n", part_info->flash_w_crc , part_info->flash_r_crc);
		printk(KERN_ALERT "=== Check the flash memory ===\n");
		printk(KERN_ALERT "first page : %d, end page %d, current page %d\n", part_info->part_first_page, part_info->part_end_page, pos);
		printk(KERN_ALERT "Write pattern value : %x\n", nftp_bml_pattern[part_info->nftp_write_index][0]);
		printk(KERN_ALERT "Read pattern value dump\n");
		
		if(part_info->part_type == FSR_BML)
		{
			for(j = 0; j < FSR_TEST_PAGE_SIZE; j++)
			{
				printk(" %02x", buf[j]);
				//if((j!=0) && ((j%8) == 0))
				if( ((j+1)%32) == 0)
					printk("\n");
			}
		}
		else if(part_info->part_type == FSR_STL)
		{
			for(j = 0; j < FSR_TEST_SECTOR_SIZE; j++)
			{
				printk(" %02x", buf[j]);
				if( ((j+1)%32) == 0)
					printk("\n");
			}
		}		
//#endif
		kfree(buf);
		return 1;
	}	
	
//	printk(KERN_ALERT "CRC CHECK : OK!!\n");
	return 0;
}

/*                                                                           
* Name : nftp_bml_overwrite_test  
* Description :                                                                            
*																			 
*/   
u32	nftp_bml_overwrite_test(struct fsr_test_info *part_info)
{
	u32 current_unit, current_page= part_info->part_first_page;
	u32 end_page;
	u32 ret;

	printk(KERN_ALERT "Start: Partition[%d] Write 0x%x pattern to flash\n", part_info->minor, nftp_bml_pattern[part_info->nftp_write_index][0]);	

	ret = FSR_BML_Open(part_info->volume, FSR_BML_FLAG_NONE);
	if (ret != FSR_BML_SUCCESS) 
	{
           	printk(KERN_ERR "BML: open error = %d\n", ret);
		return 1;			
	}		

	for (current_unit = part_info->firstvbn; current_unit <= part_info->endvbn; current_unit++) 
	{
		ret = FSR_BML_Erase(part_info->volume, &current_unit, 1, FSR_BML_FLAG_NONE);
#ifdef NFTP_DEBUG
		printk(KERN_ALERT "erase block %d\n", current_unit - part_info->firstvbn);
#endif
		if (ret != FSR_BML_SUCCESS) 
		{
			printk(KERN_ALERT "thread partition number : %d | erase error = %x\n",part_info->minor, ret);
			return 1;			
		}

		end_page = current_page + part_info->page_per_unit;

		printk("current : %d end : %d\n", current_page, end_page);
		for (; current_page < end_page; current_page++) 
		{
			ret = FSR_BML_Write(part_info->volume, current_page, 
					1, nftp_bml_pattern[part_info->nftp_write_index], NULL, FSR_BML_FLAG_ECC_ON);
			if (ret != FSR_BML_SUCCESS) 
			{
				printk(KERN_ALERT "FSR BML: %s transfer error = %x\n",  "WRITE" , ret);
				return 1;			
			}
			else
			{
#ifdef NFTP_DEBUG					
				printk(KERN_ALERT "bml: %s transfer page %d\n", "WRITE" , current_page);
#endif
			}
		}	
#ifdef CONFIG_PREEMPT_NONE		
		yield();
#endif				
	}

	printk(KERN_ALERT "BML Overwrite End : OK!!\n");	

	FSR_BML_Close(part_info->volume, FSR_BML_FLAG_NONE);		
#ifdef CONFIG_PREEMPT_NONE		
	yield();
#endif				

	return 0;
}

/*                                                                           
* Name : nftp_stl_overwrite_test  
* Description :                                                                            
*																			 
*/   
u32	nftp_stl_overwrite_test(struct fsr_test_info *part_info)
{
	u32 current_sector;
	u32 ret;
	FSRStlInfo info;
	
	printk(KERN_ALERT "Start: Write 0x%x pattern to flash\n", nftp_stl_pattern[part_info->nftp_write_index][0]);		

	FSR_DOWN(&fsr_mutex);
	ret = FSR_STL_Open(part_info->volume, part_info->part_id, &info, FSR_STL_FLAG_DEFAULT);
	FSR_UP(&fsr_mutex);

	if (ret == FSR_STL_PARTITION_ALREADY_OPENED) 
	{
		printk(KERN_ALERT "FSR STL: Device is busy\n");
		printk(KERN_ALERT "FSR STL: If you had execute only write benchmark,"
				" you have to execute read benchmark.\n");
		return 1;		
	} 
	else if (ret == FSR_STL_UNFORMATTED) 
	{
		printk(KERN_ALERT "FSR STL: Device is unformatted\n");
		return 1;		
	} 
	else if (ret != FSR_STL_SUCCESS) 
	{
		printk(KERN_ALERT "FSR STL: Out of device\n");
		return 1;		
	}

	FSR_DOWN(&fsr_mutex);		
	for (current_sector = part_info->part_first_sector; 
			current_sector <= part_info->part_end_sector;
			current_sector += 2) 
	{
		ret = FSR_STL_Delete(part_info->volume, part_info->part_id, part_info->part_first_sector, 1, FSR_STL_FLAG_USE_SM);	
		if (ret != FSR_STL_SUCCESS) 
		{
			printk(KERN_ALERT "thread partition number : %d | erase error = %x\n",part_info->minor, ret);
			return 1;			
		}
		
		ret = FSR_STL_Write(part_info->volume, part_info->part_id, 
				current_sector, 2, nftp_stl_pattern[part_info->nftp_write_index], FSR_STL_FLAG_USE_SM);
		if (ret != FSR_STL_SUCCESS) 
		{
			printk(KERN_ALERT "stl: %s transfer error = %x\n",  "WRITE", ret);
			printk(KERN_ALERT "stl: partition id = %d, current sector = %d\n", 
				part_info->part_id, current_sector);
			return 1;
		}
		else
		{
#ifdef NFTP_DEBUG					
			printk(KERN_ALERT "stl: %s transfer sector %d\n", "WRITE" , current_sector);
#endif
		}
#ifdef CONFIG_PREEMPT_NONE		
		yield();
#endif				
	}
	FSR_UP(&fsr_mutex);						

	printk(KERN_ALERT "STL Overwrite End : OK!!\n");	

	FSR_DOWN(&fsr_mutex);
	FSR_STL_Close(part_info->volume, part_info->part_id);			
	FSR_UP(&fsr_mutex);	

#ifdef CONFIG_PREEMPT_NONE		
	yield();
#endif				

	return 0;	
}

/*                                                                           
* Name : nftp_bml_read_test  
* Description :                                                                            
*																			 
*/   
u32	nftp_bml_read_test(struct fsr_test_info *part_info)
{
	u32 current_page;
	u32 ret = 0;
	u8 *buf = (u8 *)kmalloc(sizeof(u8)*FSR_TEST_PAGE_SIZE, GFP_KERNEL);

	printk(KERN_ALERT "Start: Partition[%d] Read 0x%x pattern from flash\n", 
					part_info->minor, nftp_bml_pattern[part_info->nftp_write_index][0]);
	part_info->flash_w_crc = 0;

	if(part_info->crc_check == CRC_CHECK_ON)
		part_info->flash_w_crc = crc32(part_info->flash_w_crc, nftp_bml_pattern[part_info->nftp_write_index], FSR_TEST_PAGE_SIZE-1);

	ret = FSR_BML_Open(part_info->volume, FSR_BML_FLAG_NONE);
	if (ret != FSR_BML_SUCCESS) 
	{
		printk(KERN_ERR "BML: open error = %d\n", ret);
		kfree(buf);			
		return 1;
	}	

	for (current_page = part_info->part_first_page;
			current_page <= part_info->part_end_page;
			current_page++ ) 
	{
		part_info->flash_r_crc = 0;
		ret = FSR_BML_Read(part_info->volume, current_page, 1, buf , NULL, FSR_BML_FLAG_ECC_ON);
		if (ret != FSR_BML_SUCCESS) {
			printk(KERN_ALERT "thread partition number : %d | read error = %x\n",part_info->minor, ret);
			kfree(buf);
			return 1;
		}
		
		if(part_info->crc_check == CRC_CHECK_ON)				
			part_info->flash_r_crc = crc32 (part_info->flash_r_crc, buf, FSR_TEST_PAGE_SIZE -1);

		if(part_info->crc_check == CRC_CHECK_ON)						
			crc_check(part_info, current_page, buf);		
#ifdef CONFIG_PREEMPT_NONE		
		yield();
#endif		
	}

	printk(KERN_ALERT "Partition : %d Read End : OK!!\n", part_info->minor);

	FSR_BML_Close(part_info->volume, FSR_BML_FLAG_NONE);			

#ifdef CONFIG_PREEMPT_NONE		
	yield();
#endif		
	
	kfree(buf);
	return 0;
}

/*                                                                           
* Name : nftp_stl_read_test  
* Description :                                                                            
*																			 
*/   
u32	nftp_stl_read_test(struct fsr_test_info *part_info)
{
	u32 current_sector;
	u32 ret = 0;
	u8 *buf = (u8 *)kmalloc(sizeof(u8)*FSR_TEST_SECTOR_SIZE, GFP_KERNEL);
	FSRStlInfo info;

	printk(KERN_ALERT "Start: Read 0x%x pattern from flash\n", nftp_stl_pattern[part_info->nftp_write_index][0]);
	part_info->flash_w_crc = 0;

	if(part_info->crc_check == CRC_CHECK_ON)
		part_info->flash_w_crc = crc32(part_info->flash_w_crc, nftp_stl_pattern[part_info->nftp_write_index], FSR_TEST_SECTOR_SIZE-1);		

	FSR_DOWN(&fsr_mutex);

	ret = FSR_STL_Open(part_info->volume, part_info->part_id, &info, FSR_STL_FLAG_DEFAULT);

	FSR_UP(&fsr_mutex);
	if (ret == FSR_STL_PARTITION_ALREADY_OPENED) 
	{
		printk(KERN_ALERT "STL: Device is busy\n");
		kfree(buf);			
		return 1;		
	} 
	else if (ret == FSR_STL_UNFORMATTED) 
	{
		printk(KERN_ALERT "STL: Device is unformatted\n");
		kfree(buf);					
		return 1;		
	} 
	else if (ret != FSR_STL_SUCCESS) 
	{
		printk(KERN_ALERT "STL: Out of device\n");
		kfree(buf);					
		return 1;
	}

	FSR_DOWN(&fsr_mutex);
	for (current_sector = part_info->part_first_sector; 
			current_sector <= part_info->part_end_sector;
			current_sector += 2) 
	{
		part_info->flash_r_crc = 0;
				
		ret = FSR_STL_Read(part_info->volume, part_info->part_id, 
				current_sector, 2, buf, FSR_STL_FLAG_USE_SM);

		if (ret != FSR_STL_SUCCESS) 
		{
			printk(KERN_ALERT "stl: %s transfer error = %x\n", "READ", ret);
			printk(KERN_ALERT "stl: partition id = %d, current sector = %d\n", 
				part_info->part_id, current_sector);
			kfree(buf);						
			return 1;			
		}

		if(part_info->crc_check == CRC_CHECK_ON)						
			part_info->flash_r_crc = crc32 (part_info->flash_r_crc, buf, FSR_TEST_SECTOR_SIZE -1);		

		if(part_info->crc_check == CRC_CHECK_ON)						
			crc_check(part_info, current_sector, buf);
#ifdef CONFIG_PREEMPT_NONE		
		yield();
#endif				
	}
	FSR_UP(&fsr_mutex);

	printk(KERN_ALERT "Partition : %d Read End : OK!!\n", part_info->minor);

	FSR_DOWN(&fsr_mutex);
	FSR_STL_Close(part_info->volume, part_info->part_id);			
	FSR_UP(&fsr_mutex);

#ifdef CONFIG_PREEMPT_NONE		
	yield();
#endif			

	kfree(buf);			
	return 0;
}

/*                                                                           
* Name : nftp_bml_write_test  
* Description :                                                                            
*																			 
*/   
u32	nftp_bml_write_test(struct fsr_test_info *part_info)
{
	u32 current_unit, current_page;
	u32 ret;

	printk(KERN_ALERT "Start: Partition[%d] Write 0x%x pattern to flash\n", 
			part_info->minor, nftp_bml_pattern[part_info->nftp_write_index][0]);	

	ret = FSR_BML_Open(part_info->volume, FSR_BML_FLAG_NONE);
	if (ret != FSR_BML_SUCCESS) 
	{
           	printk(KERN_ERR "BML: open error = %d\n", ret);
		return 1;			
	}		
	
	for (current_unit = part_info->firstvbn; current_unit <= part_info->endvbn; current_unit++) 
	{
		ret = FSR_BML_Erase(part_info->volume, &current_unit, 1, FSR_BML_FLAG_NONE);
#ifdef NFTP_DEBUG
		printk(KERN_ALERT "erase block %d\n", current_unit - part_info->firstvbn);
#endif
		if (ret != FSR_BML_SUCCESS) 
		{
			printk(KERN_ALERT "thread partition number : %d | erase error = %x\n",part_info->minor, ret);
			return 1;			
		}
	}

#ifdef CONFIG_PREEMPT_NONE		
	yield();
#endif				

	for (current_page = part_info->part_first_page; 
			current_page <= part_info->part_end_page;
			current_page += 1) 
	{
		ret = FSR_BML_Write(part_info->volume, current_page, 
				1, nftp_bml_pattern[part_info->nftp_write_index], NULL, FSR_BML_FLAG_ECC_ON);
		if (ret != FSR_BML_SUCCESS) 
		{
			printk(KERN_ALERT "FSR BML: %s transfer error = %x\n",  "WRITE" , ret);
			return 1;			
		}
		else
		{
#ifdef NFTP_DEBUG					
			printk(KERN_ALERT "bml: %s transfer page %d\n", "WRITE" , current_page);
#endif
		}
//		percent(part_info->part_end_page - part_info->part_first_page, current_page - part_info->part_first_page);
//		spin_wheel();		
#ifdef CONFIG_PREEMPT_NONE		
		yield();
#endif				

	}
	printk(KERN_ALERT "Write End : OK!!\n");	
	FSR_BML_Close(part_info->volume, FSR_BML_FLAG_NONE);			

#ifdef CONFIG_PREEMPT_NONE		
	yield();
#endif				

	return 0;
}

/*                                                                           
* Name : nftp_stl_write_test  
* Description :                                                                            
*																			 
*/   
u32	nftp_stl_write_test(struct fsr_test_info *part_info)
{
	u32 current_sector;
	u32 ret;
	FSRStlInfo info;
	
	printk(KERN_ALERT "Start: Write 0x%x pattern to flash\n", nftp_stl_pattern[part_info->nftp_write_index][0]);		

	FSR_DOWN(&fsr_mutex);
	ret = FSR_STL_Open(part_info->volume, part_info->part_id, &info, FSR_STL_FLAG_DEFAULT);
	FSR_UP(&fsr_mutex);

	if (ret == FSR_STL_PARTITION_ALREADY_OPENED) 
	{
		printk(KERN_ALERT "FSR STL: Device is busy\n");
		printk(KERN_ALERT "FSR STL: If you had execute only write benchmark,"
				" you have to execute read benchmark.\n");
		return 1;		
	} 
	else if (ret == FSR_STL_UNFORMATTED) 
	{
		printk(KERN_ALERT "FSR STL: Device is unformatted\n");
		return 1;		
	} 
	else if (ret != FSR_STL_SUCCESS) 
	{
		printk(KERN_ALERT "FSR STL: Out of device\n");
		return 1;		
	}

	FSR_DOWN(&fsr_mutex);		
	ret = FSR_STL_Delete(part_info->volume, part_info->part_id, part_info->part_first_sector, part_info->nScts, FSR_STL_FLAG_USE_SM);
#ifdef CONFIG_PREEMPT_NONE		
	yield();
#endif				
	
	for (current_sector = part_info->part_first_sector; 
			current_sector <= part_info->part_end_sector;
			current_sector += 2) 
	{
		ret = FSR_STL_Write(part_info->volume, part_info->part_id, 
				current_sector, 2, nftp_stl_pattern[part_info->nftp_write_index], FSR_STL_FLAG_USE_SM);
		if (ret != FSR_STL_SUCCESS) 
		{
			printk(KERN_ALERT "stl: %s transfer error = %x\n",  "WRITE", ret);
			printk(KERN_ALERT "stl: partition id = %d, current sector = %d\n", 
				part_info->part_id, current_sector);
		}
		else
		{
#ifdef NFTP_DEBUG					
			printk(KERN_ALERT "stl: %s transfer sector %d\n", "WRITE" , current_sector);
#endif
		}
//		percent(part_info->part_end_sector- part_info->part_first_sector, current_sector - part_info->part_first_sector);
//		spin_wheel();			
#ifdef CONFIG_PREEMPT_NONE		
		yield();
#endif				
	}
	FSR_UP(&fsr_mutex);						

	printk(KERN_ALERT "Write End : OK!!\n");	

	FSR_DOWN(&fsr_mutex);
	FSR_STL_Close(part_info->volume, part_info->part_id);			
	FSR_UP(&fsr_mutex);	

#ifdef CONFIG_PREEMPT_NONE		
	yield();
#endif				

	return 0;	
}


/*                                                                           
* Name : nftp_ecc_write_test  
* Description :                                                                            
*																			 
*/   
u32	nftp_block_write_test(struct fsr_test_info *part_info, u32 ecc, u32 block)
{
	u32 current_unit, first_page, current_page;
	u32 ret;
	
#ifdef NFTP_DEBUG
	printk(KERN_ALERT "Start: Partition[%d] Write 0x%x pattern to flash\n", part_info->minor, nftp_bml_pattern[part_info->nftp_write_index][0]);	
#endif

	ret = FSR_BML_Open(part_info->volume, FSR_BML_FLAG_NONE);
	if (ret != FSR_BML_SUCCESS) 
	{
           	printk(KERN_ERR "BML: open error = %d\n", ret);
		return 1;			
	}		

	current_unit = part_info->firstvbn + block - 1; 
	ret = FSR_BML_Erase(part_info->volume, &current_unit, 1, FSR_BML_FLAG_NONE);
#ifdef NFTP_DEBUG
	printk(KERN_ALERT "erase block %d\n", current_unit - part_info->firstvbn);
#endif
	if (ret != FSR_BML_SUCCESS) 
	{
		printk(KERN_ALERT "thread partition number : %d | erase error = %x\n",part_info->minor, ret);
		return 1;			
	}

	first_page = part_info->part_first_page + (block - 1)*256; 
	
	for(current_page = first_page; current_page < first_page + 256; current_page++)
	ret = FSR_BML_Write(part_info->volume, current_page, 
			1, nftp_bml_pattern[part_info->nftp_write_index], NULL, ecc);
	if (ret != FSR_BML_SUCCESS) 
	{
		printk(KERN_ALERT "FSR BML: %s transfer error = %x\n",  "WRITE" , ret);
		return 1;			
	}
	else
	{
#ifdef NFTP_DEBUG					
		printk(KERN_ALERT "bml: %s transfer page %d\n", "WRITE" , current_page);
#endif
	}

#ifdef NFTP_DEBUG					
	printk(KERN_ALERT "Write End : OK!!\n");	
#endif

	FSR_BML_Close(part_info->volume, FSR_BML_FLAG_NONE);			

	return 0;
}

/*                                                                           
* Name : nftp_ecc_read_test  
* Description :                                                                            
*																			 
*/   
u32	nftp_page_read_test(struct fsr_test_info *part_info, u32 ecc, u32 page)
{
	u32 ret = 0;
	u8 *buf = (u8 *)kmalloc(sizeof(u8)*FSR_TEST_PAGE_SIZE, GFP_KERNEL);
#ifdef NTFS_DEBUG
	printk(KERN_ALERT "Start: Partition[%d] Read 0x%x pattern from flash\n", part_info->minor, nftp_bml_pattern[part_info->nftp_write_index][0]);
#endif
	part_info->flash_w_crc = 0;

	if(part_info->crc_check == CRC_CHECK_ON)
		part_info->flash_w_crc = crc32 (part_info->flash_w_crc, nftp_bml_pattern[part_info->nftp_write_index], FSR_TEST_PAGE_SIZE-1);		
#if 0
	ret = FSR_BML_Open(part_info->volume, FSR_BML_FLAG_NONE);
	if (ret != FSR_BML_SUCCESS) 
	{
           	printk(KERN_ERR "BML: open error = %d\n", ret);
		return 1;
	}
#endif

//	current_page = part_info->part_first_page +  ((part_info->part_end_page - part_info->part_first_page)/2);

	part_info->flash_r_crc = 0;

	memset(buf, 0x00, 4096);
	if(ecc == FSR_BML_FLAG_ECC_ON)
		ret = FSR_BML_Read(part_info->volume, page, 
				1, buf , NULL, FSR_BML_FLAG_ECC_ON);
	else ret = FSR_BML_Read(part_info->volume, page, 
				1, buf , NULL, FSR_BML_FLAG_ECC_OFF);

	if (ret != FSR_BML_SUCCESS) {
		printk("first page = %d, current page = %d\n", part_info->part_first_page, page);
		printk("thread partition number : %d | read error = %x\n",part_info->minor, ret);
		return 1;
	}
	
	if(part_info->crc_check == CRC_CHECK_ON)				
		part_info->flash_r_crc = crc32 (part_info->flash_r_crc, buf, FSR_TEST_PAGE_SIZE -1);

	if(part_info->crc_check == CRC_CHECK_ON)
	{
		ret = crc_check(part_info, page, buf);	
		if(ret)
			return 1;
	}

#ifdef NTFS_DEBUG
	printk(KERN_ALERT "Partition : %d Read End : OK!!\n", part_info->minor);
#endif
#if 0
	FSR_BML_Close(part_info->volume, FSR_BML_FLAG_NONE);			
#endif
	kfree(buf);
	
	return 0;
}

int check_sync_on_close(void)
{
#ifdef CONFIG_RFS_SYNC_ON_CLOSE 
	if(CONFIG_RFS_SYNC_ON_CLOSE == 1)
	{
		printk("****************************************************\n");
		printk("you must turn off robust FS option : 'SYNC ON CLOSE'\n");
		printk("****************************************************\n");
		return -1;
	}
#endif
	return 0;
}

/*                                                                           
* Name : nftp_overwrite_loop_test                                                
* Description :                                                                            
*																			 
*/   
void nftp_overwrite_loop_test(struct fsr_test_info *part_info)
{
	u32 ret, i=0;
	
	part_info->nftp_write_index = 0;

	while(i++ < MAX_COUNT)
	{	
		if(part_info->part_type == FSR_BML)
		{	
			ret = nftp_bml_overwrite_test(part_info);
			if (ret) break;
			
			ret = nftp_bml_read_test(part_info);
			if (ret) break;
		}
		else if(part_info->part_type == FSR_STL)
		{
			ret = nftp_stl_overwrite_test(part_info);
			if (ret) break;
			
			ret = nftp_stl_read_test(part_info);
			if (ret) break;
		}	

		part_info->nftp_write_index += 1;
		if(part_info->nftp_write_index == 2)
		{
			part_info->nftp_write_index = 0;
		}		
	}
	printk("Overwrite Thread End !! \n");	
	if(part_info->thread_count == 0)
	{
		if(part_info->part_type == FSR_BML)
			nftp_report->bml_overwrite = 1;
		else if(part_info->part_type == FSR_STL)
			nftp_report->stl_overwrite = 1;
	}	
	kfree(part_info);
}

/*                                                                           
* Name : nftp_read_loop_test                                          
* Description :                                                                            
*																			 
*/    
void	nftp_read_loop_test(struct fsr_test_info * part_info)
{
	u32 ret, i=0;
	
	part_info->nftp_write_index = 0;
	if(part_info->part_type == FSR_BML)
	{
		ret = nftp_bml_write_test(part_info);
		while(i++ < MAX_COUNT)
		{
			ret = nftp_bml_read_test(part_info);
			if(ret) break;
		}
	}
	else if(part_info->part_type == FSR_STL)
	{
		ret = nftp_stl_write_test(part_info);
		while(i++ < MAX_COUNT)
		{
			ret = nftp_stl_read_test(part_info);
			if(ret) break;			
		}
	}
	printk("Read Thread End !! \n");	

	if(part_info->thread_count == 0)
	{
		if(part_info->part_type == FSR_BML)
			nftp_report->bml_read = 1;
		if(part_info->part_type == FSR_STL)
			nftp_report->stl_read = 1;
	}		
	kfree(part_info);
}

/*                                                                           
* Name : nftp_write_loop_test                                          
* Description :                                                                            
*																			 
*/   
void	nftp_write_loop_test(struct fsr_test_info * part_info)
{
	u32 ret, i=0;
	
	part_info->nftp_write_index = 0;

	while(i++ < MAX_COUNT)
	{
		if(part_info->part_type == FSR_BML)
		{
			ret = nftp_bml_write_test(part_info);
			if(ret) break;					
		}
		else if(part_info->part_type == FSR_STL)
		{
			ret = nftp_stl_write_test(part_info);
			if(ret) break;					
		}
	}
	printk("Write Thread End !! \n");	
	kfree(part_info);	
}

/*                                                                           
* Name : nftp_read_write_loop_test                                                          
* Description :                                                                            
*																			 
*/   
void nftp_read_write_loop_test(struct fsr_test_info *part_info)
{
	u32 ret, i = 0;

	part_info->nftp_write_index= 0;

	while(i++ < MAX_COUNT)
	{
		if(part_info->part_type == FSR_BML)
		{	
			ret = nftp_bml_write_test(part_info);
			if (ret) break;
			
			ret = nftp_bml_read_test(part_info);
			if (ret) break;
		}
		else if(part_info->part_type == FSR_STL)
		{
			ret = nftp_stl_write_test(part_info);
			if (ret) break;
			
			ret = nftp_stl_read_test(part_info);
			if (ret) break;
		}
		
		part_info->nftp_write_index += 1;
		if(part_info->nftp_write_index == 2)
		{
			part_info->nftp_write_index = 0;
		}
	}
	printk(KERN_ALERT "Read/Write Thread End !! \n");	
	if(part_info->thread_count == 0)
	{
		if(part_info->part_type == FSR_BML)
			nftp_report->bml_read_write = 1;
		if(part_info->part_type == FSR_STL)
			nftp_report->stl_read_write = 1;
	}	
	kfree(part_info);	
}

/*                                                                           
* Name : nftp_wl_test  
* Description : wearleveling testcase                                                                           
*																			 
*/   

u32	nftp_wl_test(struct fsr_test_info *part_info)
{
	INT32 nErr;
	UINT32 nVol = 0;
	UINT32 nPartID = part_info->part_id;
	UINT32 BytesReturned;
	UINT32 nNumOfECNT;
	UINT32 *pnaECnt;
#if 0
	UINT32 nCode;
	UINT8 *pBufIn;
	UINT32 nLenIn;
	UINT8 *pBufOut;
	UINT32 nLenOut;
	UINT32 nECntThresHold;
	UINT32 nMaxWriteReq;
	UINT32 nLogSects;
	UINT32 nPageSize;
	u32 count = 0;
	u32 current_sector=0;
#endif
	u32 i, j, k, m = 0;
	FSRStlInfo info;
	u32 ret;
	u8 *buf = (u8 *)kmalloc(sizeof(u8)*FSR_SECTOR_SIZE, GFP_KERNEL);
	
	memset(buf,0xAA,sizeof(u8)*FSR_SECTOR_SIZE);
	
	FSR_DOWN(&fsr_mutex);
	ret = FSR_STL_Open(part_info->volume, part_info->part_id, &info, FSR_STL_FLAG_DEFAULT);
	FSR_UP(&fsr_mutex);
	
#if 0	
	nErr = FSR_STL_IOCtl(nVol, nPartID, FSR_STL_IOCTL_GET_WL_THRESHOLD ,NULL, sizeof(u32) , &nECntThresHold,sizeof(u32),&BytesReturned);

	if (nErr != FSR_STL_SUCCESS)
	{
		return FALSE32;
	}

	printk("nECntThresHold %u\n", nECntThresHold);
#endif
	FSR_DOWN(&fsr_mutex);
	nErr = FSR_STL_IOCtl(part_info->volume,part_info->part_id,FSR_STL_IOCTL_GET_ECNT_BUFFER_SIZE,NULL,sizeof(u32),&nNumOfECNT, sizeof(u32),&BytesReturned);
	FSR_UP(&fsr_mutex);

	if (nErr != FSR_STL_SUCCESS)
	{
		return 1;
	}
#ifdef NFTP_DEBUG                   
	printk("nNumOfEcnt %u\n", nNumOfECNT);
#endif 
	pnaECnt = kmalloc(nNumOfECNT * sizeof(UINT32), GFP_KERNEL);

	if (pnaECnt == NULL)
	{
		return 1;
	}

	printk("Wearlevel Test case Start\n");
	
	while (m++ < 10) {
		printk("try to write one sector during 128 time.\n");
		printk("we can see erase count of blocks dispersed by wearleveling.\n");
		for (j = 0; j < 128; j++)
		{
			ret = FSR_STL_Write(part_info->volume, part_info->part_id,
					part_info->part_first_sector ,1, buf, FSR_STL_FLAG_USE_SM);
			if (ret != FSR_STL_SUCCESS) 
			{
				printk(KERN_ALERT "stl: %s transfer error = %x\n",  "WRITE", ret);
				printk(KERN_ALERT "stl: partition id = %d, current sector = %d\n",
						part_info->part_id, 0);
				return 1;
			}
			else 
			{
#ifdef NFTP_DEBUG                   
				printk(KERN_ALERT "stl: %s transfer sector %d\n", "WRITE" , current_sector);
#endif
			}

			memset(pnaECnt, 0, nNumOfECNT * sizeof(u32));

			FSR_DOWN(&fsr_mutex);
			nErr = FSR_STL_IOCtl(nVol, nPartID, FSR_STL_IOCTL_READ_ECNT,NULL, sizeof(u32), pnaECnt, nNumOfECNT * sizeof(UINT32),&BytesReturned);
			FSR_UP(&fsr_mutex);

			if (nErr != FSR_STL_SUCCESS)
			{
				return FALSE32;
			}

			if(!(j % 128)) {
				for(i =22,k = 1; i < nNumOfECNT; i++,k++) {
					if(pnaECnt[i] != 0)
						printk("Erase count of %dth block : %u\n", k ,pnaECnt[i]);
				}
				printk("\n");
				printk("----------------------------------\n");
				printk("\n");
			}

		}
	}

	kfree(pnaECnt);
	kfree(buf);
	FSR_DOWN(&fsr_mutex);
	FSR_STL_Close(part_info->volume, part_info->part_id);
	FSR_UP(&fsr_mutex);
	
	printk("Wearlevel Test case end\n");
	return 0;
}

int ntfp_check_ecc_bit(struct fsr_test_info *part_info, u32 current_page)
{
	u32 ret = 0,i,j, ecc_bit =0;
	u8 *abuf = (u8 *)kmalloc(sizeof(u8)*FSR_TEST_PAGE_SIZE, GFP_KERNEL);
	u8 *bbuf = (u8 *)kmalloc(sizeof(u8)*FSR_TEST_PAGE_SIZE, GFP_KERNEL);

	
	memset (abuf, 0, FSR_TEST_PAGE_SIZE);
	memset (bbuf, 0, FSR_TEST_PAGE_SIZE);
	
	ret = FSR_BML_Read(part_info->volume, current_page,1, abuf , NULL, FSR_BML_FLAG_ECC_OFF);
	if (ret != FSR_BML_SUCCESS) {
		printk("Ecc OFF , first page = %d, current page = %d\n", part_info->part_first_page, current_page);
		printk("thread partition number : %d | read error = %x\n",part_info->minor, ret);
		return -1;
	}

	ret = FSR_BML_Read(part_info->volume, current_page,1, bbuf , NULL, FSR_BML_FLAG_ECC_ON);
	if (ret != FSR_BML_SUCCESS) {
		printk("Ecc On ,first page = %d, current page = %d\n", part_info->part_first_page, current_page);
		printk("thread partition number : %d | read error = %x\n",part_info->minor, ret);
		return -1;
	}
	
	for(i = 0; i < FSR_TEST_PAGE_SIZE; i++)
	{
		for(j = 1; j <= 128; j *= 2)
			if ((abuf[i] ^ bbuf[i]) & (128/j))
			{
				ecc_bit++;
			}
	}
	
	kfree(abuf);	
	kfree(bbuf);	
	return ecc_bit;
}



/*                                                                           
* Name : nftp_ecc_test  
* Description :                                                                            
*																			 
*/   

u32	nftp_ecc_test(struct fsr_test_info *part_info)
{
	u32 ret, i = 0, ecc_bit;
	u32 current_page;
	u8 *buf = (u8 *)kmalloc(sizeof(u8)*FSR_TEST_PAGE_SIZE, GFP_KERNEL);
	u8 ecc_test_patten[5] = {0xBF,0xBE,0xBC,0xB8,0xB0};
	
	part_info->nftp_write_index = 0;

	memset(nftp_bml_pattern[part_info->nftp_write_index], 0xBF , FSR_TEST_PAGE_SIZE);
	
	ret = FSR_BML_Open(part_info->volume, FSR_BML_FLAG_NONE);
	if (ret != FSR_BML_SUCCESS) 
	{
           	printk(KERN_ERR "BML: open error = %d\n", ret);
		return 1;			
	}
	ret = FSR_BML_Erase(part_info->volume, &part_info->firstvbn, 1, FSR_BML_FLAG_NONE);

	ret = FSR_BML_Write(part_info->volume, part_info->part_first_page, 1,
			nftp_bml_pattern[part_info->nftp_write_index], NULL, FSR_BML_FLAG_ECC_OFF);
	
	while (i++ < ECCbit) //until ecc 5bit
	{
		nftp_bml_pattern[part_info->nftp_write_index][0] = ecc_test_patten[i];
		current_page = part_info->part_first_page;
		ret = FSR_BML_Write(part_info->volume, current_page, 
				1, nftp_bml_pattern[part_info->nftp_write_index], NULL, FSR_BML_FLAG_ECC_OFF);
		if (ret != FSR_BML_SUCCESS) 
		{
			printk(KERN_ALERT "FSR BML: %s transfer error = %x\n",  "WRITE" , ret);
			return 1;			
		}
		else
		{
#if 1//def NFTP_DEBUG					
			printk(KERN_ALERT "bml: %s transfer page %d\n", "WRITE" , current_page);
#endif
		}
		
		{ // first, we can see to be fused wrong data for error bit correction.
			ecc_bit = ntfp_check_ecc_bit(part_info, current_page);
			if (ret == -1) {
				printk("first page = %d, current page = %d\n", part_info->part_first_page, current_page);
				printk("thread partition number : %d | read error = %x\n",part_info->minor, ret);
				return 1;
			}

			printk("ecc error bit : %u\n", ecc_bit);
		}
		FSR_BML_Close(part_info->volume, FSR_BML_FLAG_NONE);		

		{ // After turn on ECC, check whether operate bit correction or not.
			ret = nftp_page_read_test(part_info,FSR_BML_FLAG_ECC_ON, current_page);
			if(!ret)
				printk("ECC correction OK!!!\n");
			else 
				printk("ECC correction Fail!!\n");
		}
	}
	memset(nftp_bml_pattern[part_info->nftp_write_index], 0xAA , FSR_TEST_PAGE_SIZE);	
	kfree(part_info);	
	kfree(buf);	
	return 0;
}

u32	nftp_er_chk(struct fsr_test_info *part_info)
{
	u32 current_page, ecc_bit = 0;
	
	printk("Erase Refresh Check Start!!\n");

	part_info->nftp_write_index= 0;
	
	for (current_page = part_info->part_first_page; 
			current_page <= part_info->part_first_page + page_per_block_ond*2;
			current_page += 1) 
	{
		ecc_bit = ntfp_check_ecc_bit(part_info, current_page);
		if (ecc_bit > 1) 
		{
			printk("remain Read Disturbance problem yet. current page : %u\n",current_page);
			printk("** ecc error bit : %d\n", ecc_bit);
			kfree(part_info);	
			return 1;
		}
		else if(ecc_bit  == -1)
		{
			printk("read error !!!!\n");
			return 1;
		}
	}

	printk("RD is disappeared by erase refresh.\n");
	printk("Erase Refresh Check End!!\n");
	return 0;
}
	
/*                                                                           
* Name : nftp_rd_test  
* Description :                                                                            
*																			 
*/   
u32	nftp_rd_test(struct fsr_test_info *part_info, u32 er)
{
	u32 ret, i = 0, j = 0, k =0, l = 0, m = 0;
	u32 current_page;
	u32 read_count = k_time*100 , total_count = 0;
	//int fd = 0;
	//u8 *filename = "/mtd_rwarea/er_test";
	u32 ecc_bit = 0;
	printk("Read Disturb Test Start!! %d\n", er);
	
	part_info->nftp_write_index= 0;
	
	ret = nftp_bml_write_test(part_info);
	if (ret)
	{
		printk("error ..\n");
		return 0;
	}
#if 1
		printk("p/e cycles start\n");
		while(k++ < k_time * 2) 	{
			ret = nftp_block_write_test(part_info,FSR_BML_FLAG_ECC_ON, 2 );
			if (ret) break;
		}
		printk("end p/e cycles : %u\n", k-1);
#endif

	while (1)
	{
		l++;
		while (m++ < 100)
		{
			//printk(" %d : try to read only one page for 1k cycles\n", l);
			printk(" %d : try to read only one page for 100k cycles\n", l);
			for(i = 0; i < read_count; i ++) 
				for(j = 0; j < 128; j++)
				{
			//		current_page = part_info->part_first_page + page_per_block_mlc +  page_per_block_mlc/2 ;
			//		ret = nftp_page_read_test(part_info,FSR_BML_FLAG_ECC_ON, current_page);
					current_page = part_info->part_first_page + page_per_block_mlc;// +  page_per_block_mlc/2 ;
					ret = nftp_page_read_test(part_info,FSR_BML_FLAG_ECC_ON, current_page + j );
					if (ret) {
						printk("read page crc error!! count : %u\n", total_count);
					}
				}
			total_count += read_count;

			printk(" %d : try to check all block to know whether RD happened or not\n", l);
			for (current_page = part_info->part_first_page; 
					current_page <= part_info->part_first_page+ page_per_block_mlc*2;
					current_page += 1) {
				ecc_bit = ntfp_check_ecc_bit(part_info, current_page);				
				if (ecc_bit == -1) {
					while(1){
						printk("Device Fail !!!!!!!!!!!!!!!!\n");
					}
					return 1;
				}

				if (ecc_bit) {
					printk("Read Disturbance problem happened!!!!. current page : %u, total count %u\n",current_page, total_count);
					printk("** ecc error bit : %d\n", ecc_bit);
					if (er == ER_ON)
					{
						if(ecc_bit < 2)
						{
							ret = nftp_page_read_test(part_info,FSR_BML_FLAG_ECC_ON, current_page);
							if(!ret)
								printk("ECC correction OK!!!\n");
							else printk("ECC correction Fail!!\n");
						}
						else {	
							kfree(part_info);
							/*
							mm_segment_t old_fs = get_fs ();
							set_fs (KERNEL_DS);
							fd = sys_open (filename, O_RDWR | O_CREAT, 0644);
							if (fd >= 0)
								printk("file creation success!!!!\n");
							else 
								printk("file creation fail!!!!\n");
							sys_close(fd);
							set_fs (old_fs);
							*/
							return 1;
						}
					}
					else {
						ret = nftp_page_read_test(part_info,FSR_BML_FLAG_ECC_ON, current_page);
						if(!ret)
							printk("ECC correction OK!!!\n");
						else printk("ECC correction Fail!!\n");
					}
				}
			}
		}
		m = 0;
	}
	
	printk(KERN_ALERT "RD Test End !! \n");	
	kfree(part_info);	
	return 0;
}



/*                                                                           
* Name : bml_get_partition_info                                                          
* Description :                                                                            
*																			 
*/ 
u32 bml_get_partition_info(struct fsr_test_info *part_info)
{
	u32 ret;
	u32 part_no;
	FSRVolSpec *vs;
	FSRPartI *ps;
	u32 ppu, first_page;

	set_write_pattern(part_info->part_type);

	part_info->volume = fsr_vol(part_info->minor);
	part_no = fsr_part(part_info->minor);

	vs = fsr_get_vol_spec(part_info->volume);
	ps = fsr_get_part_spec(part_info->volume);

	ret = FSR_BML_Open(part_info->volume, FSR_BML_FLAG_NONE);
	if (ret != FSR_BML_SUCCESS) 
	{
           	printk(KERN_ERR "BML: open error = %d\n", ret);
		return 1;			
	}
	
	ret = FSR_BML_GetVirUnitInfo(part_info->volume, fsr_part_start(ps, part_no), &first_page, &ppu);
	if (ret != FSR_BML_SUCCESS)
	{
		printk(KERN_ALERT "FSR_BML_GetVirUnitInfo FAIL : 0x%08X\r\n", ret);
		return 1;			
	}
	
	part_info->firstvbn = fsr_part_start(ps,part_no);
	part_info->endvbn = part_info->firstvbn + fsr_part_units_nr(ps, part_no) - 1;

	part_info->part_first_page = first_page;
	part_info->part_end_page = first_page + fsr_part_units_nr(ps, part_no) * ppu - 1;

	part_info->page_per_unit = ppu;

	FSR_BML_Close(part_info->volume, FSR_BML_FLAG_NONE);		

	return 0;
}	

/*                                                                           
* Name : stl_get_partition_info
* Description :                                                                            
*																			 
*/ 
u32 stl_get_partition_info(struct fsr_test_info *part_info)
{
	u32 ret;
	u32 part_no;
	FSRPartI *ps;
	u32 stl_len;
	FSRStlInfo info;	

	set_write_pattern(part_info->part_type);

	part_info->volume = fsr_vol(part_info->minor);
	part_no = fsr_part(part_info->minor);

	ps = fsr_get_part_spec(part_info->volume);

	part_info->part_id = fsr_part_id(ps, part_no);

	FSR_DOWN(&fsr_mutex);
	ret = FSR_STL_Open(part_info->volume, part_info->part_id, &info, FSR_STL_FLAG_DEFAULT);
	FSR_UP(&fsr_mutex);
	
	if (ret == FSR_STL_UNFORMATTED) 
	{
/*		printk(KERN_ALERT "FSR STL: Device is unformatted\n");
		ret=stl_do_ioctl(part_info->volume, part_no, STL_FORMAT, (u32)NULL);
		if(ret == FSR_STL_SUCCESS)
		{
			FSR_DOWN(&fsr_mutex);
			ret = FSR_STL_Open(part_info->volume, part_info->part_id, &info, FSR_STL_FLAG_DEFAULT);
			FSR_UP(&fsr_mutex);
		}
		else
		{*/
			printk(KERN_ALERT "FSR STL: Format Error\n");
			return 1;			
//		}
	} 
	
	if (ret == FSR_STL_PARTITION_ALREADY_OPENED) 
	{
		printk(KERN_ALERT "FSR STL: Device is busy\n");
		printk(KERN_ALERT "FSR STL: If you had execute only write benchmark,"
				" you have to execute read benchmark.\n");
		return 1;			
	} 

	else if (ret != FSR_STL_SUCCESS) 
	{
		printk(KERN_ALERT "FSR STL: Out of device\n");
		return 1;			
	}

	FSR_DOWN(&fsr_mutex);
	ret = FSR_STL_IOCtl(part_info->volume, part_info->part_id, FSR_STL_IOCTL_LOG_SECTS, NULL, 
				sizeof(u32), &part_info->nScts, sizeof(u32), &stl_len);
	FSR_UP(&fsr_mutex);

	if (ret != FSR_STL_SUCCESS) 
	{
		printk(KERN_ALERT "FSR STL: Ioctl error\n");
		printk(KERN_ALERT "FSR STL: If you want to execute only read benchmark,"
				" you have to execute write benchmark before that.\n");
		return 1;			
	}
	
	/* first sector of stl partition is always zero */
	part_info->part_first_sector = 0; 
	part_info->part_end_sector = part_info->nScts- 1;		

	FSR_DOWN(&fsr_mutex);
	FSR_STL_Close(part_info->volume, part_info->part_id);		
	FSR_UP(&fsr_mutex);	

	return 0;
}	

/*                                                                           
* Name : my_atoi                                                          
* Description :                                                                            
*																			 
*/ 
static int my_atoi(const char *name)
{
	int val = 0;

	for (;; name++) {
		switch (*name) {
			case '0'...'9':
				val = 10*val+(*name-'0');
				break;
			default:
				return val;
		}
	}
}

int nftp_kernel_thread(struct fsr_test_info *part_info)
{
	int ret = 0;
	part_info->nftp_loop_task(part_info);
	
	if(part_info->part_type == FSR_BML)
		nftp_report->bml_kthread++;
	else if(part_info->part_type == FSR_STL)
		nftp_report->stl_kthread++;
		
	if(part_info->thread_count == nftp_report->bml_kthread)
		nftp_flag = 1;

	return ret;
}


/*                                                                           
* Name : nftp_write_proc                                                          
* Description :                                                                            
*																			 
*/     
#define LIST 10
#define STRING 50
static char save_buf[LIST][STRING];
static int usage[LIST];
struct fsr_test_info part_info[LIST];

static void show_list(void)
{
	int i;

	printk(KERN_ALERT "\n============================================================\n");
	printk(KERN_ALERT "saved command list\n");
	for(i=0; i<LIST; i++)
		if( usage[i] )
			printk( KERN_ALERT "[%d] %s\n", i, save_buf[i]);
	printk(KERN_ALERT "============================================================\n");
}

int getlen(const char *s1, const char *s2)
{
    size_t l1, l2, org;

    l2 = strlen(s2);
    if (!l2)
        return 0;

    org = l1 = strlen(s1);
    while (l1 >= l2) {
        l1--;
        if (!memcmp(s1, s2, l2))
            return org - l1 - 1;
        s1++;
    }
    return 0;
}

void show_error(void)
{
	printk( KERN_ALERT "invalid input format...\n");
	printk("usage : bml/stl partition_number thrd read/write/over/all end\n");
	printk("if you want to do overwrite test, insert the following command to /proc/nftp\n");
	printk("ex) stl 8 over\n");
}

static void start_test(void);

static int nftp_write_proc(struct file *file, const char __user *buffer, unsigned long count, void *data)
{

	char *strstr_buff;
	u32 thread_count;
	int i,j,ret = 0;
	int len;

    /*********************************************************************************/
	// 1.1 start saved command list
    /*********************************************************************************/
	if(!strncmp(buffer,"start",5))
	{
		printk( KERN_ALERT "START NFTP SCENARIO!!!!~~\n");
		start_test();
		return -1;
	}
    /*********************************************************************************/
	// 1.2 show current saved command list
    /*********************************************************************************/
	else if(!strncmp(buffer,"show",4))
	{
		show_list();
		return -1;
	}
    /*********************************************************************************/

    /*********************************************************************************/
	// 2. find available command buffer & copy it
    /*********************************************************************************/
	for(i=0; i<LIST; i++)
		if( usage[i] == 0 )
			break;

	if( i == LIST )
	{
		printk( KERN_ALERT "there is no available command list");
		show_list();
		return -1;
	}

	if( strstr(buffer, "end") == NULL)
	{
		show_error();
		return -1;
	}

	len = getlen(buffer, "end");
	if( len <= 0)
	{
		show_error();
		return -1;
	}

	if( __copy_from_user(save_buf[i], buffer, len))
	{
		printk( KERN_ALERT "__copy_from_user() fail (%s)\n", buffer);
		return -1;
	}
    /*********************************************************************************/

    /*********************************************************************************/
	// 3. command parsing & set test environment
    /*********************************************************************************/
	printk( KERN_ALERT "\n\"%s\" is insered\n", save_buf[i]);
	//memset(&part_info[i],0,sizeof(struct fsr_test_info ));			

	strstr_buff = strstr(buffer," ");
	thread_count = my_atoi(&strstr_buff[1]);
	part_info[i].minor = thread_count;

	if(!strncmp(buffer,"bml",3)){
		part_info[i].part_type = FSR_BML;
		ret = bml_get_partition_info(&part_info[i]);				
	}
	else if(!strncmp(buffer,"stl",3)){
		part_info[i].part_type = FSR_STL;
		ret = stl_get_partition_info(&part_info[i]);	
	}
	else
	{
		show_error();
		return -1;
	}

	if(ret)
	{
		printk(KERN_ALERT "Test Error : Getting Partition Information Error. Check the target Partition is alright.\n");
		return -1;
	}	

	printk( KERN_ALERT "\n======================================================\n");
	printk( KERN_ALERT "volume : %d\n", part_info[i].volume);
	printk( KERN_ALERT "minor : %d\n", part_info[i].minor);
	if(part_info[i].part_type == FSR_BML)
		printk( KERN_ALERT "part_type : BML\n");
	else
		printk( KERN_ALERT "part_type : STL\n");
	printk( KERN_ALERT "firstvbn : %d\n", part_info[i].firstvbn);
	printk( KERN_ALERT "endvbn : %d\n", part_info[i].endvbn);
	printk( KERN_ALERT "part_first_page : %d\n", part_info[i].part_first_page);
	printk( KERN_ALERT "part_end_page : %d\n", part_info[i].part_end_page);
	printk( KERN_ALERT "page_per_unit : %d\n", part_info[i].page_per_unit);
	printk( KERN_ALERT "part_id : %d\n", part_info[i].part_id);
	printk( KERN_ALERT "part_first_sector : %d\n", part_info[i].part_first_sector);
	printk( KERN_ALERT "part_end_sector : %d\n", part_info[i].part_end_sector);
	printk( KERN_ALERT "nScts : %d\n", part_info[i].nScts);
	printk( KERN_ALERT "======================================================\n");

	usage[i] = 1;
    /*********************************************************************************/

	return -1;
}

int start_nftp_thread( struct fsr_test_info *part_info, char *buf)
{
    int ret = 0;
	struct task_struct *task;

	task = kthread_create((void*)nftp_kernel_thread, part_info, buf);
    if (IS_ERR(task)) {
        ret = PTR_ERR(task);
        task = NULL;
        return ret;
    }

    task->flags |= PF_NOFREEZE;
    wake_up_process(task);

	return 0;
}

void start_test(void)
{
	int i;
	char *buff_command;
	char buf[20];
	u32 thread_count;

	thread_count = 1;
	for(i=0; i<LIST; i++)
	{
		if ( usage[i] == 0 )
			continue;

		printk( KERN_ALERT "==============================================================\n");
		printk( KERN_ALERT "[NFTP] \"%s\" execution!!!! \n", save_buf[i]); 
		printk( KERN_ALERT "==============================================================\n");
		if ((buff_command = strstr(save_buf[i],"thrd")) != NULL)
		{
			part_info[i].thread_count = thread_count;
			thread_count++;

			memset(buf, 0x0, 20);
			if(part_info[i].part_type == FSR_BML)
				sprintf(buf, "NFTP_bml_%d", part_info[i].minor);
			else
				sprintf(buf, "NFTP_stl_%d", part_info[i].minor);

			if ((buff_command = strstr(save_buf[i],"all")) != NULL)
			{
				part_info[i].crc_check = CRC_CHECK_ON;
				part_info[i].nftp_loop_task = nftp_read_write_loop_test;					
				//kernel_thread((void *)nftp_kernel_thread, &part_info[i], 0);
				start_nftp_thread(&part_info[i], buf);
			}
			else if ((buff_command = strstr(save_buf[i],"read")) != NULL)
			{ 
				part_info[i].crc_check = CRC_CHECK_ON;
				part_info[i].nftp_loop_task = nftp_read_loop_test;
				//kernel_thread((void *)nftp_kernel_thread, &part_info[i], 0);
				start_nftp_thread(&part_info[i], buf);
			}
			else if ((buff_command = strstr(save_buf[i],"write")) != NULL)
			{
				part_info[i].crc_check = CRC_CHECK_OFF;
				part_info[i].nftp_loop_task = nftp_write_loop_test;
				//kernel_thread((void *)nftp_kernel_thread, &part_info[i], 0);
				start_nftp_thread(&part_info[i], buf);
			}
			else if ((buff_command = strstr(save_buf[i],"over")) != NULL)
			{
				part_info[i].crc_check = CRC_CHECK_OFF;
				part_info[i].nftp_loop_task = nftp_overwrite_loop_test;
				//kernel_thread((void *)nftp_kernel_thread, &part_info[i], 0);
				start_nftp_thread(&part_info[i], buf);
			}
		}
		else
		{

			if ((buff_command = strstr(save_buf[i],"all")) != NULL)
			{
				part_info[i].crc_check = CRC_CHECK_ON;
				nftp_read_write_loop_test(&part_info[i]);
			}
			else if ((buff_command = strstr(save_buf[i],"read")) != NULL)
			{ 
				part_info[i].crc_check = CRC_CHECK_ON;
				nftp_read_loop_test( &part_info[i]);
			}
			else if ((buff_command = strstr(save_buf[i],"write")) != NULL)
			{
				part_info[i].crc_check = CRC_CHECK_OFF;
				nftp_write_loop_test(&part_info[i]);
			}
			else if ((buff_command = strstr(save_buf[i],"over")) != NULL)
			{
				part_info[i].crc_check = CRC_CHECK_OFF;
				nftp_overwrite_loop_test(&part_info[i]);
			}
			else if ((buff_command = strstr(save_buf[i],"ecc")) != NULL)
			{
				part_info[i].crc_check = CRC_CHECK_ON;
				nftp_ecc_test(&part_info[i]);
			}
			else if ((buff_command = strstr(save_buf[i],"rd")) != NULL)
			{
				part_info[i].crc_check = CRC_CHECK_ON;
				nftp_rd_test(&part_info[i], ER_OFF);
			}
			else if ((buff_command = strstr(save_buf[i],"er")) != NULL)
			{
				part_info[i].crc_check = CRC_CHECK_ON;
				nftp_rd_test(&part_info[i], ER_ON);
			}
			else if ((buff_command = strstr(save_buf[i],"eck")) != NULL)
			{
				part_info[i].crc_check = CRC_CHECK_ON;
				nftp_er_chk(&part_info[i]);
			}
			else if ((buff_command = strstr(save_buf[i],"wl")) != NULL)
			{
				part_info[i].crc_check = CRC_CHECK_ON;
				nftp_wl_test(&part_info[i]);
			}
		}
	}
}

static int nftp_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len;

	if(nftp_flag == FALSE)
		sprintf(page,"%s\n","Still Testing...");
	else if(nftp_flag == TRUE)
		len = sprintf(page,
				"******* NFTP Test Complete *******\n"
				"    BML Read Test:			   <%s>\n"
				"    BML Read/Write Test:	   <%s>\n"
				"    STL Read Test:			   <%s>\n"
				"    STL Read/Write Test:      <%s>\n"
				"    BML OverWrite Test:       <%s>\n"
				"    STL OverWrite Test:       <%s>\n"
				"    BML Kernel Thread Test:   <%s>\n"
				"    STL Kernel Thread Test:   <%s>\n"
				"    ECC Test:				   <%s>\n",
				nftp_report->bml_read ? "PASS" : "FAIL",
				nftp_report->bml_read_write ? "PASS" : "FAIL",
				nftp_report->stl_read ? "PASS" : "FAIL",
				nftp_report->stl_read_write ? "PASS" : "FAIL",
				nftp_report->bml_overwrite ? "PASS" : "FAIL",
				nftp_report->stl_overwrite ? "PASS" : "FAIL",
				nftp_report->bml_kthread ? "PASS" : "FAIL",
				nftp_report->stl_kthread ? "PASS" : "FAIL",
				nftp_report->ecc ? "PASS" : "FAIL"
				);
	printk("%s\n",page);
	return 0;

}

static int __init nftp_driver_init(void)
{
	struct proc_dir_entry *nftp_proc, *nftp_result_proc;
	nftp_report = (struct _nftp_report *)kmalloc(sizeof(struct _nftp_report),GFP_KERNEL);
//	console_verbose();

	memset(nftp_report,0,sizeof(struct _nftp_report));

	nftp_proc = create_proc_entry("nftp" ,0777, NULL);
	nftp_result_proc = create_proc_entry("nftp_test_report", 0777, NULL);

	if(nftp_proc)
	{
		nftp_proc->write_proc = nftp_write_proc;
		//nftp_proc->owner = THIS_MODULE;
	}
	if(nftp_result_proc)
	{
		nftp_result_proc->read_proc = nftp_read_proc;
		//nftp_result_proc->owner = THIS_MODULE;
	}
	
	printk(KERN_ALERT "SAMSUNG NFTP Driver\n");

	return 0;
}


static void __exit nftp_driver_cleanup(void)
{
	remove_proc_entry("nftp", NULL);
	remove_proc_entry("nftp_test_report", NULL);	
	kfree(nftp_report);		
}

module_init(nftp_driver_init);
module_exit(nftp_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("NFTP(NAND Flash Test Project)");
