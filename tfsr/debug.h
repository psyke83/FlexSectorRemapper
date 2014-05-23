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
 * @file	drivers/tfsr/debug.h
 * @brief	Debug macro, time measure macro
 *
 */

#ifndef _FSR_DEBUG_H_
#define _FSR_DEBUG_H_

#define MAX_NUMBER_SECTORS	16
#define IO_DIRECTION		2
#define STL_IOSTAT_PROC_NAME	"stl-iostat"
#define BML_IOSTAT_PROC_NAME	"bml-iostat"

#ifdef FSR_TIMER
#define DECLARE_TIMER	struct timeval start, stop
#define START_TIMER()	do { do_gettimeofday(&start); } while (0)
#define STOP_TIMER(name)					\
do {								\
	do_gettimeofday(&stop);					\
	if (stop.tv_usec < start.tv_usec) {			\
		stop.tv_sec -= (start.tv_sec + 1);		\
		stop.tv_usec += (1000000U - start.tv_usec);	\
	} else {						\
		stop.tv_sec -= start.tv_sec;			\
		stop.tv_usec -= start.tv_usec;			\
	}							\
	printk("STOP: %s: %d.%06d",				\
	 name, (int) (stop.tv_sec), (int) (stop.tv_usec));	\
} while (0)

#else
#define DECLARE_TIMER		do { } while (0)
#define START_TIMER()		do { } while (0)
#define STOP_TIMER(name)	do { } while (0)
#endif

/* These symbols will be use the user-level*/
/*
 * for debugging
 */
#define DL0     (0)     /* Quiet   */
#define DL1     (1)     /* Audible */
#define DL2     (2)     /* Loud    */
#define DL3     (3)     /* Noisy   */

/* These symbols will be use the user-level*/
#define _LINUSTOREIII_TINY_DEBUG_
#ifdef _LINUSTOREIII_TINY_DEBUG_
#define DEBUG(n, format, args...)					\
do {									\
        if (n <= CONFIG_LINUSTOREIII_TINY_DEBUG_VERBOSE) {		\
                printk("%s[%d]: " format "\n",				\
                         __func__, __LINE__, ##args);			\
        }								\
} while(0)
#else
#define DPRINTK(...)		do { } while (0)
#endif	/* _LINUSTOREIII_TINY_DEBUG_ */

#define ERRPRINTK(format, args...)					\
do {									\
	printk("LinuStoreIII_TERR: %s[%d] " format "\n", __func__, __LINE__, ##args);	\
} while (0)

/* These macros will be use for iostat */
#ifdef CONFIG_RFS_FSR_DEBUG
void stl_count_iostat(int num_sectors, int rw);
void bml_count_iostat(int num_sectors, int rw);
#else
#define stl_count_iostat(...)		do { } while (0)
#define bml_count_iostat(...)		do { } while (0)
#endif /* CONFIG_RFS_FSR_DEBUG */

#define FSR_DOWN(x)			down(x)
#define FSR_UP(x)			up(x)

typedef void (*fsr_reset_t)(void);

struct fsr_platform {
	fsr_reset_t	reset;
};

#ifdef CONFIG_PROC_FS
extern struct proc_dir_entry *fsr_proc_dir;
#endif

#endif	/* _FSR_DEBUG_H_ */
