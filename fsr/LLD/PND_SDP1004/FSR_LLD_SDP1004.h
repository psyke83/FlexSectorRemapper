/**
 *   @mainpage   Flex Sector Remapper : LinuStoreIII_1.2.0_b035-FSR_1.2.1p1_b129_RTM
 *
 *   @section Intro
 *       Flash Translation Layer for Flex-OneNAND and OneNAND
 *    
 *    @section  Copyright
 *            COPYRIGHT. 2008-2009 SAMSUNG ELECTRONICS CO., LTD.               
 *                            ALL RIGHTS RESERVED                              
 *                                                                             
 *     Permission is hereby granted to licensees of Samsung Electronics        
 *     Co., Ltd. products to use or abstract this computer program for the     
 *     sole purpose of implementing a product based on Samsung                 
 *     Electronics Co., Ltd. products. No other rights to reproduce, use,      
 *     or disseminate this computer program, whether in part or in whole,      
 *     are granted.                                                            
 *                                                                             
 *     Samsung Electronics Co., Ltd. makes no representation or warranties     
 *     with respect to the performance of this computer program, and           
 *     specifically disclaims any responsibility for any damages,              
 *     special or consequential, connected with the use of this program.       
 *
 *     @section Description
 *
 */

/**
 * @file        FSR_LLD_OMAP2430.h
 * @brief       This file contain the Platform Adaptation Modules for OMAP2430
 * @author      JinHo Yi, JinHyuck Kim
 * @date        15-SEP-2009
 * @remark
 * REVISION HISTORY
 * @n   28-JAN-2008 [JinHo Yi] 	   : First writing
 * @n   15-SEP-2009 [JinHyuck Kim] : Update for FSR LLD
 *
 */

#ifndef _FSR_LLD_SDP1004_
#define _FSR_LLD_SDP1004_

#include "FSR.h"


typedef unsigned int UInt32;
typedef unsigned short UInt16;
typedef unsigned char UInt8;




#define BIT0				0x1
#define BIT1				0x2
#define BIT2				0x4
#define BIT3				0x8
#define BIT4				0x10
#define BIT5				0x20
#define BIT6				0x40
#define BIT7				0x80
#define BIT8				0x100
#define BIT9				0x200
#define BIT10				0x400
#define BIT11				0x800
#define BIT12				0x1000
#define BIT13				0x2000
#define BIT14				0x4000
#define BIT15				0x8000
#define BIT16				0x10000
#define BIT17				0x20000
#define BIT18				0x40000
#define BIT19				0x80000
#define BIT20				0x100000
#define BIT21				0x200000
#define BIT22				0x400000
#define BIT23				0x800000
#define BIT24				0x1000000
#define BIT25				0x2000000
#define BIT26				0x4000000
#define BIT27				0x8000000
#define BIT28				0x10000000
#define BIT29				0x20000000
#define BIT30				0x40000000
#define BIT31				0x80000000



#define REG_BANK_CLKGEN						(0x580)
#define REG_BANK_CHIPTOP					(0xF00)
#define REG_BANK_CHIPGPIO					(0x1580)
#define REG_BANK_FCIE0						(0x8980)
#define REG_BANK_FCIE2						(0x8A00)

#define REG_BANK_PM_GPIO					(0x780)

#define KSEG02KSEG1(addr)					((void *)((UINT32)(addr)|0x20000000))

#define NAND_MIPS_WRITE_BUFFER_PATCH		1
#define NAND_MIPS_READ_BUFFER_PATCH			1

#define NAND_DRV_LINUX				1

extern void Chip_Flush_Memory(void);
extern void Chip_Read_Memory(void);

//=====================================================
// HW parameters
//=====================================================
#define REG(Reg_Addr)						(*(volatile UINT16*)(Reg_Addr))
#define REG_OFFSET_SHIFT_BITS				2 

//----------------------------------
// should only one be selected
#define NC_SEL_FCIE3						1 

#define IF_IP_VERIFY						0 // [CAUTION]: to verify IP and HAL code, defaut 0
//----------------------------------

#define RIU_NONPM_BASE						0xBF000000
#define RIU_BASE							0xBF200000

#define MPLL_CLK_REG_BASE_ADDR				(RIU_BASE+(REG_BANK_CLKGEN<<REG_OFFSET_SHIFT_BITS))
#define CHIPTOP_REG_BASE_ADDR				(RIU_BASE+(REG_BANK_CHIPTOP<<REG_OFFSET_SHIFT_BITS))
#define FCIE_REG_BASE_ADDR					(RIU_BASE+(REG_BANK_FCIE0<<REG_OFFSET_SHIFT_BITS))
#define FCIE_NC_CIFD_BASE					(RIU_BASE+(REG_BANK_FCIE2<<REG_OFFSET_SHIFT_BITS))
#define PM_GPIO_BASE						(RIU_NONPM_BASE+(REG_BANK_PM_GPIO<<REG_OFFSET_SHIFT_BITS))
#define CHIPGIO_REG_BASE_ADDR				(RIU_BASE+(REG_BANK_CHIPGPIO<<REG_OFFSET_SHIFT_BITS))



#define REG_WRITE_UINT16(reg_addr, val)		REG(reg_addr) = val
#define REG_READ_UINT16(reg_addr, val)		val = REG(reg_addr)
#define HAL_WRITE_UINT16(reg_addr, val)		(REG(reg_addr) = val)
#define HAL_READ_UINT16(reg_addr, val)		val = REG(reg_addr)
#define REG_SET_BITS_UINT16(reg_addr, val)	REG(reg_addr) |= (val)
#define REG_CLR_BITS_UINT16(reg_addr, val)	REG(reg_addr) &= ~(val)
#define REG_W1C_BITS_UINT16(reg_addr, val)	REG_WRITE_UINT16(reg_addr, REG(reg_addr)&(val))

//=====================================================
// Nand Driver configs
//=====================================================
#define NAND_DRIVER_OS						1 // for OS / File System
#define NAND_DRIVER_OS_LINUX				(1 & NAND_DRIVER_OS)

//-------------------------------
#define NAND_ENV_FPGA						1
#define NAND_ENV_ASIC						2
#define NAND_DRIVER_ENV						NAND_ENV_ASIC /* [CAUTION] */

//=====================================================
// debug option
//=====================================================
#define NAND_TEST_IN_DESIGN					0 

#define NAND_DEBUG_MSG						0

#if defined(NAND_DEBUG_MSG) && NAND_DEBUG_MSG
#define nand_printf (x)
#define NANDMSG(x)	 (x)
#define printf FSR_OAM_DbgMsg
#else
#define nand_printf	(x) 
#define NANDMSG(x)
#define printf 
#endif


//=====================================================
// HW Timer for Delay 
//=====================================================
#define HW_TIMER_DELAY_1us					1
#define HW_TIMER_DELAY_1ms					1000
#define HW_TIMER_DELAY_1s					1000000


/*****************************************************************************/
/* NAND Controller  Masking value Definitions                                */
/*****************************************************************************/
#define GPMC_nCS0                           0
#define GPMC_nCS1                           1
#define GPMC_nCS2                           2
#define GPMC_nCS3                           3
#define GPMC_nCS4                           4
#define GPMC_nCS5                           5
#define GPMC_nCS6                           6
#define GPMC_nCS7                           7

#define WAITx_ACTIVE_HIGH                   0x1
#define NAND_FLASH_LIKE_DEVICES             0x1
#define WAIT_INPUT_PIN_IS_WAIT_(x)          (x)
#define NAND_FLASH_STATUS_BUSY_MASK_(x)     (1 << (x + 8))

/*****************************************************************************/
/* NAND Controller Register Address Definitions                              */
/*****************************************************************************/
#define SDP_NAND_BASE			0x30020000
#define SDP_NAND_BURST_BASE		0x30021000

#if 0
#define SDP_NAND_VALUE(x)			*((volatile UInt32 *) (SDP_NAND_BASE+(x)))
#define SDP_NAND_REG(x)			((volatile UInt32 *) (SDP_NAND_BASE+(x)))

#define SDP_NFCONF				SDP_NAND_VALUE(0x00)
#define SDP_NFCONT				SDP_NAND_VALUE(0x04)
#define SDP_NFCMMD				SDP_NAND_VALUE(0x08)
#define SDP_NFADDR				SDP_NAND_VALUE(0x0C)
#define SDP_NFDATA32			SDP_NAND_VALUE(0x10)
#define SDP_NFMECCD0			SDP_NAND_VALUE(0x14)
#define SDP_NFMECCD1			SDP_NAND_VALUE(0x18)
#define SDP_NFSECCD				SDP_NAND_VALUE(0x1C)
#define SDP_NFSBLK				SDP_NAND_VALUE(0x20)
#define SDP_NFEBLK				SDP_NAND_VALUE(0x24)
#define SDP_NFSTAT				SDP_NAND_VALUE(0x28)
#define SDP_NFECCERR0			SDP_NAND_VALUE(0x2C)
#define SDP_NFECCERR1			SDP_NAND_VALUE(0x30)
#define SDP_NFMECC0				SDP_NAND_VALUE(0x34)
#define SDP_NFMECC1				SDP_NAND_VALUE(0x38)
#define SDP_NFSECC				SDP_NAND_VALUE(0x3C)
#define SDP_NFMLCBITPT			SDP_NAND_VALUE(0x40)
#define SDP_NFECCCONF			SDP_NAND_VALUE(0x100)
#define SDP_NFECCCONT			SDP_NAND_VALUE(0x104)
#define SDP_NFECCSTAT			SDP_NAND_VALUE(0x108)
#define SDP_NFECCSECSTAT		SDP_NAND_VALUE(0x10C)
#define SDP_NFECCRANDSEED		SDP_NAND_VALUE(0x110)
#define SDP_NFECCCONECC0		SDP_NAND_VALUE(0x1D4)

#define SDP_NFDATA_BURST		((volatile UInt32 *) SDP_NAND_BURST_BASE)


#define SDP_SETREG(x,y,z,value)			do {sdpnand.u32value = x; sdpnand.##y.##z = value; x = sdpnand.u32Value;} while(0)
#define SDP_NFDATA8				*((volatile UInt8 *) (SDP_NAND_BASE+(0x10)))
#define SDP_NFDATA16			*((volatile UInt16 *) (SDP_NAND_BASE+(0x10)))
#define SDP_NFECCERL(x)			*((volatile UInt32 *) (SDP_NAND_BASE + 0x15C + ((x) << 2)))
#define SDP_NFECCERP(x)			*((volatile UInt32 *) (SDP_NAND_BASE + 0x1AC + ((x) << 2)))
#define SDP_NFECCPRGECC(x)		*((volatile UInt32 *) (SDP_NAND_BASE + 0x114 + ((x) << 2)))


struct NFCONF {
	UInt32 u32Reserved0                    : 1;	/* [0:0] */
	UInt32 u32Addrcycle                    : 1;	/* [1:1] */
	UInt32 u32Pagesize                     : 1;	/* [2:2] */
	UInt32 u32Mlcflash                     : 1;	/* [3:3] */
	UInt32 u32Twrph1                       : 4;	/* [7:4] */
	UInt32 u32Twrph0                       : 4;	/* [11:8] */
	UInt32 u32Tacls                        : 4;	/* [15:12] */
	UInt32 u32Reserved16                   : 7;	/* [22:16] */
	UInt32 u32Ecctype                      : 2;	/* [24:23] */
	UInt32 u32Msglength                    : 1;	/* [25:25] */
	UInt32 u32Reserved26                   : 4;	/* [29:26] */
	UInt32 u32Mlcclkctrl                   : 1;	/* [30:30] */
	UInt32 u32Cfgenbnfcon                  : 1;	/* [31:31] */
};

struct NFCONT {
	UInt32 u32Mode                         : 1;	/* [0:0] */
	UInt32 u32RegNce0                      : 1;	/* [1:1] */
	UInt32 u32RegNce1                      : 1;	/* [2:2] */
	UInt32 u32HwNce                        : 1;	/* [3:3] */
	UInt32 u32Initsecc                     : 1;	/* [4:4] */
	UInt32 u32Initmecc                     : 1;	/* [5:5] */
	UInt32 u32Secclock                     : 1;	/* [6:6] */
	UInt32 u32Mecclock                     : 1;	/* [7:7] */
	UInt32 u32RnbTransmode                 : 1;	/* [8:8] */
	UInt32 u32Enbrnbint                    : 1;	/* [9:9] */
	UInt32 u32Enbillegalaccint             : 1;	/* [10:10] */
	UInt32 u32Resetecc                     : 1;	/* [11:11] */
	UInt32 u32Enbmlcdecint                 : 1;	/* [12:12] */
	UInt32 u32Enbmlcencint                 : 1;	/* [13:13] */
	UInt32 u32Reserved14                   : 2;	/* [15:14] */
	UInt32 u32Lock                         : 1;	/* [16:16] */
	UInt32 u32LockTight                    : 1;	/* [17:17] */
	UInt32 u32MlcEccDirection              : 1;	/* [18:18] */
	UInt32 u32Reserved19                   : 3;	/* [19:21] */
	UInt32 u32RegNce2                      : 1;	/* [22:22] */
	UInt32 u32RegNce3                      : 1;	/* [23:23] */
	UInt32 u32Reserved17                   : 8;	/* [31:24] */
};

struct NFSTAT {
	UInt32 u32Busy                         : 1;	/* [0:0] */
	UInt32 u32Reserved1                    : 1;	/* [1:1] */
	UInt32 u32Reserved2                    : 2;	/* [3:2] */
	UInt32 u32RnbTransdetect               : 1;	/* [4:4] */
	UInt32 u32Illegalaccess                : 1;	/* [5:5] */
	UInt32 u32Mlcdecodedone                : 1;	/* [6:6] */
	UInt32 u32Mlcencodedone                : 1;	/* [7:7] */
	UInt32 u32FlashNce                     : 4;	/* [11:8] */
	UInt32 u32Reserved12                   : 11;	/* [22:12] */
	UInt32 u32Bootdone                     : 1;	/* [23:23] */
	UInt32 u32RnbTransdetectGrp            : 4;	/* [27:24] */
	UInt32 u32FlashRnb                     : 4;	/* [31:28] */
};

struct NFECCCONF {
	UInt32 u32MainEccType                  : 4;	/* [3:0] */
	UInt32 u32MetaEccType                  : 4;	/* [7:4] */
	UInt32 u32MetaMsgLen                   : 4;	/* [11:8] */
	UInt32 u32Reserved12                   : 4;	/* [15:12] */
	UInt32 u32MainMsgLen                   : 12;	/* [27:16] */
	UInt32 u32MetaConvSel                  : 1;	/* [28:28] */
	UInt32 u32MainConvSel                  : 1;	/* [29:29] */
	UInt32 u32Ecc8Sel                      : 1;	/* [30:30] */
	UInt32 u32Eccclk                       : 1;	/* [31:31] */
};

struct NFECCCONT {
	UInt32 u32Resetecc                     : 1;	/* [0:0] */
	UInt32 u32Reserved1                    : 1;	/* [1:1] */
	UInt32 u32Initecc                      : 1;	/* [2:2] */
	UInt32 u32Reserved3                    : 13;	/* [15:3] */
	UInt32 u32Eccdirection                 : 1;	/* [16:16] */
	UInt32 u32EccdirectionMeta             : 1;	/* [17:17] */
	UInt32 u32Reserved18                   : 2;	/* [19:18] */
	UInt32 u32ErrEstMode                   : 1;	/* [20:20] */
	UInt32 u32InvMode                      : 1;	/* [21:21] */
	UInt32 u32Enbrandomizer                : 1;	/* [22:22] */
	UInt32 u32Resetrandomizer              : 1;	/* [23:23] */
	UInt32 u32EnbDecInt                    : 1;	/* [24:24] */
	UInt32 u32EnbEncInt                    : 1;	/* [25:25] */
	UInt32 u32Reserved26                   : 6;	/* [31:26] */
};

struct NFECCSTAT {
	UInt32 u32StatError                    : 8;	/* [7:0] */
	UInt32 u32Freepagestat                 : 8;	/* [15:8] */
	UInt32 u32Reserved16                   : 8;	/* [23:16] */
	UInt32 u32Decodedone                   : 1;	/* [24:24] */
	UInt32 u32Encodedone                   : 1;	/* [25:25] */
	UInt32 u32Reserved26                   : 4;	/* [29:26] */
	UInt32 u32Eccbusy                      : 1;	/* [30:30] */
	UInt32 u32Reserved31                   : 1;	/* [31:31] */
};

struct NFECCSECSTAT {
	UInt32 u32ErrorNo                      : 8;	/* [7:0] */
	UInt32 u32ErrorLoc                     : 24;	/* [31:8] */
};

typedef union {
	UInt32 u32Value;
	struct NFCONF               Nfconf;
	struct NFCONT               Nfcont;
	struct NFSTAT               Nfstat;
	struct NFECCCONF            Nfeccconf;
	struct NFECCCONT            Nfecccont;
	struct NFECCSTAT            Nfeccstat;
	struct NFECCSECSTAT         Nfeccsecstat;
} SDP_NAND;

#else

#if defined(__KERNEL__) && !defined(__U_BOOT__)
#define NFCON_SFR_BASE			gNfconBaseAddr
#define NFCON_BURST				gNfconBurstAddr
#else
#define NFCON_SFR_BASE			0x30020000
#define NFCON_BURST				((volatile unsigned int *)0x30021000)
#endif

#define rNFCONF				(*(volatile unsigned int *)(NFCON_SFR_BASE+0x0000))
#define rNFCONT				(*(volatile unsigned int *)(NFCON_SFR_BASE+0x0004))
#define rNFCMMD				(*(volatile unsigned int *)(NFCON_SFR_BASE+0x0008))
#define rNFADDR				(*(volatile unsigned int *)(NFCON_SFR_BASE+0x000C))
//#define rNFDATA			(*(volatile unsigned int *)(NFCON_SFR_BASE+0x0010))
#define rNFMECCD0			(*(volatile unsigned int *)(NFCON_SFR_BASE+0x0014))
#define rNFMECCD1			(*(volatile unsigned int *)(NFCON_SFR_BASE+0x0018))
#define rNFSECCD			(*(volatile unsigned int *)(NFCON_SFR_BASE+0x001C))
#define rNFSBLK				(*(volatile unsigned int *)(NFCON_SFR_BASE+0x0020))
#define rNFEBLK				(*(volatile unsigned int *)(NFCON_SFR_BASE+0x0024))
#define rNFSTAT				(*(volatile unsigned int *)(NFCON_SFR_BASE+0x0028))
#define rNFECCERR0			(*(volatile unsigned int *)(NFCON_SFR_BASE+0x002C))
#define rNFECCERR1			(*(volatile unsigned int *)(NFCON_SFR_BASE+0x0030))
#define rNFMECC0			(*(volatile unsigned int *)(NFCON_SFR_BASE+0x0034))
#define rNFMECC1			(*(volatile unsigned int *)(NFCON_SFR_BASE+0x0038))
#define rNFSECC				(*(volatile unsigned int *)(NFCON_SFR_BASE+0x003C))
#define rNFMLCBITPT			(*(volatile unsigned int *)(NFCON_SFR_BASE+0x0040))
#define rNFECCCONF			(*(volatile unsigned int *)(NFCON_SFR_BASE+0x0100))
#define rNFECCCONT			(*(volatile unsigned int *)(NFCON_SFR_BASE+0x0104))
#define rNFECCSTAT			(*(volatile unsigned int *)(NFCON_SFR_BASE+0x0108))
#define rNFECCSECSTAT		(*(volatile unsigned int *)(NFCON_SFR_BASE+0x010C))
#define rNFECCRANDSEED		(*(volatile unsigned int *)(NFCON_SFR_BASE+0x0110))
#define rNFECCCONECC0		(*(volatile unsigned int *)(NFCON_SFR_BASE+0x01D4))
#define rNFECCCONECC1		(*(volatile unsigned int *)(NFCON_SFR_BASE+0x01D8))

#define	rNFDATA32			(*(volatile unsigned int *)(NFCON_SFR_BASE+0x0010))		// Data register (Word size)
#define	rNFDATA16			(*(volatile unsigned short *)(NFCON_SFR_BASE+0x0010))	// Data register (Half word size)
#define	rNFDATA08			(*(volatile unsigned char *)(NFCON_SFR_BASE+0x0010))	// Data register (Byte size)

#define	NFC_WRDATA(data)		{rNFDATA32=data;}		// Word
#define	NFC_WRDATA16(data)		{rNFDATA16=data;}	// Half word
#define	NFC_WRDATA8(data)		{rNFDATA08=data;}	// Byte

#define	NFC_RDATA()				(rNFDATA32)
#define	NFC_RDATA16()			(rNFDATA16)
#define	NFC_RDATA8()			(rNFDATA08)

#define	NFC_RAND_ENB()			{rNFECCCONT|=(1<<22);rNFECCRANDSEED=(0xa5a5a5a5);rNFECCCONT|=(1<<23);}
#define	NFC_RAND_DIS()			{rNFECCCONT&=~(1<<22);}

#define	NFC_ILL_ACC_CLR()		{rNFSTAT|=(1<<5);}
#define	NFC_CHIP_RELEASE()		{rNFCONT|=(3<<22)|(3<<1);}
#define	NFC_CHIP_ASSERT_0()		{rNFCONT&=(~(1<<1));}
#define	NFC_CHIP_ASSERT_1()		{rNFCONT&=(~(1<<2));}
#define	NFC_CHIP_ASSERT_2()		{rNFCONT&=(~(1<<22));}
#define	NFC_CHIP_ASSERT_3()		{rNFCONT&=(~(1<<23));}

#define	NFC_CHIP_DEASSE_0()		{rNFCONT|= (1<<1);}
#define	NFC_CHIP_DEASSE_1()		{rNFCONT|= (1<<2);}
#define	NFC_CHIP_DEASSE_2()		{rNFCONT|= (1<<22);}
#define	NFC_CHIP_DEASSE_3()		{rNFCONT|= (1<<23);}
#define	NFC_RNB_TRAN_CLR()		{rNFSTAT|=(1<<4);}
#define	NFC_DEC_DONE_CLR()		{rNFECCSTAT|=(1<<24);}
#define	NFC_ENC_DONE_CLR()		{rNFECCSTAT|=(1<<25);}

#define	NFC_CMD(CMD)			{rNFCMMD=CMD;}
#define	NFC_ADDR(ADDR)			{rNFADDR=ADDR;}
#define	NFC_DETECT_RNB()		{while(!(rNFSTAT&(1<<0)));}
#define NFC_DETECT_RNB_TRANS()	{while(!(rNFSTAT&(1<<4)));}
#define	NFC_O_DEC_DONE()		{while(!(rNFSTAT&=(1<<6)));}
#define	NFC_O_ENC_DONE()		{while(!(rNFSTAT|=(1<<7)));}
#define	NFC_N_DEC_DONE()		{while(!(rNFECCSTAT&=(1<<24)));}
#define	NFC_N_ENC_DONE()		{while(!(rNFECCSTAT|=(1<<25)));}

#define	NFC_O_DIR_ENC()			{rNFCONT|= (1<<18);}	// Set Direction of old ECC : program
#define	NFC_O_DIR_DEC()			{rNFCONT&=~(1<<18);}	// Set Direction of old ECC : read
#define	NFC_N_DIR_ENC()			{rNFECCCONT|= (1<<16);}	// Set Direction of new ECC : program
#define	NFC_N_DIR_DEC()			{rNFECCCONT&=~(1<<16);}	// Set Direction of new ECC : read

#define	NFC_N_ECC_RESET()		{rNFECCCONT|=(1<<0);}
#define	NFC_O_ECC_INIT()		{rNFCONT|=(3<<4);}
#define	NFC_N_ECC_INIT()		{rNFECCCONT|=(1<<2);}
#define	NFC_MECC_LOCK()			{rNFCONT|=(1<<7);}
#define	NFC_SECC_LOCK()			{rNFCONT|=(1<<6);}
#define	NFC_MECC_UNLOCK()		{rNFCONT&=~(1<<7);}
#define	NFC_SECC_UNLOCK()		{rNFCONT&=~(1<<6);}

#define	NFC_N_ECC_MAIN()		{rNFECCCONT&=~(1<<17);}
#define	NFC_N_ECC_META()		{rNFECCCONT|= (1<<17);}
#define	NFC_N_ECC_CONV()		{rNFECCCONF|= (1<<28);}

#define	NFC_RNB_INT_ENB()		{rNFCONT|= (1<<9);}
#define	NFC_RNB_INT_DIS()		{rNFCONT&=~(1<<9);}
#define	NFC_ILL_ACC_INT_ENB()	{rNFCONT|= (1<<10);}
#define	NFC_ILL_ACC_INT_DIS()	{rNFCONT|= (1<<10);}
#define	NFC_OECC_DEC_INT_ENB()	{rNFCONT|= (1<<12);}
#define	NFC_OECC_DEC_INT_DIS()	{rNFCONT&=~(1<<12);}
#define	NFC_OECC_ENC_INT_ENB()	{rNFCONT|= (1<<13);}
#define	NFC_OECC_ENC_INT_DIS()	{rNFCONT&=~(1<<13);}
#define	NFC_NECC_DEC_INT_ENB()	{rNFECCCONT|= (1<<24);}
#define	NFC_NECC_DEC_INT_DIS()	{rNFECCCONT&=~(1<<24);}
#define	NFC_NECC_ENC_INT_ENB()	{rNFECCCONT|= (1<<25);}
#define	NFC_NECC_ENC_INT_DIS()	{rNFECCCONT&=~(1<<25);}

#define NFC_NECC_ERR_CNT()		(rNFECCSECSTAT&0xFF)

#define NFC_NECC_ERR_LOC(x)		*((volatile UInt32 *) (NFCON_SFR_BASE + 0x15C + ((x) << 2)))
#define NFC_NECC_ERR_PAR(x)		*((volatile UInt32 *) (NFCON_SFR_BASE + 0x1AC + ((x) << 2)))
#define NFC_NECC_PRG(x)			*((volatile UInt32 *) (NFCON_SFR_BASE + 0x114 + ((x) << 2)))

#define NFC_TACLS(x)			do {rNFCONF&=~(0xF<<12); rNFCONF|= ((x)<<12);} while(0)
#define NFC_TWRPH0(x)			do {rNFCONF&=~(0xF<<8); rNFCONF|= ((x)<<8);} while(0)
#define NFC_TWRPH1(x)			do {rNFCONF&=~(0xF<<4); rNFCONF|= ((x)<<4);} while(0)


#endif

//------------------------------------------------------------------
#define MAIN_ECC							0
#define SPARE_ECC							1

#define ECC_READ							0
#define ECC_PROGRAM							1

#define ECC_TYPE_1BIT						0
#define ECC_TYPE_4BIT						2

#define NEW_ECC_TYPE_1BIT					2
#define NEW_ECC_TYPE_4BIT					3
#define NEW_ECC_TYPE_8BIT					5
#define NEW_ECC_TYPE_12BIT					6
#define NEW_ECC_TYPE_24BIT					0xE
#define NEW_ECC_TYPE_40BIT					0xF

#define ECC_CODE_BYTECNT_1BIT				3
#define ECC_CODE_BYTECNT_4BIT				7

#define SDP_CMD_READ						0x0
#define SDP_CMD_READ_CONFIRM				0x30
#define SDP_CMD_READ_RND					0x5
#define SDP_CMD_READ_RND_CONFIRM			0xE0
#define SDP_CMD_ERASE						0x60
#define SDP_CMD_ERASE_CONFIRM				0xD0
#define SDP_CMD_PROGRAM						0x80
#define SDP_CMD_PROGRAM_CONFIRM				0x10
#define SDP_CMD_PROGRAM_RND					0x85
#define SDP_CMD_READ_ID						0x90
#define SDP_CMD_READ_STATUS					0x70
#define SDP_CMD_RESET						0xFF

//------------------------------------------------------------------
#define IF_SPARE_AREA_DMA					0 // [CAUTION]

/*
#define NC_CIFD_ADDR(u16_pos)				GET_REG_ADDR(FCIE_NC_CIFD_BASE, u16_pos) // 256 x 16 bits
#define NC_CIFD_BYTE_CNT					0x200 // 256 x 16 bits
*/
#define NC_MAX_SECTOR_BYTE_CNT				(BIT12-1)
#define NC_MAX_SECTOR_SPARE_BYTE_CNT		(BIT8-1)
#define NC_MAX_TOTAL_SPARE_BYTE_CNT			(BIT11-1)


// OP Code: Address
#define OP_ADR_CYCLE_00						(4<<4)
#define OP_ADR_CYCLE_01						(5<<4)
#define OP_ADR_CYCLE_10						(6<<4)
#define OP_ADR_CYCLE_11						(7<<4)
#define OP_ADR_TYPE_COL						(0<<2)
#define OP_ADR_TYPE_ROW						(1<<2)
#define OP_ADR_TYPE_FUL						(2<<2)
#define OP_ADR_TYPE_ONE						(3<<2)
#define OP_ADR_SET_0						(0<<0)
#define OP_ADR_SET_1						(1<<0)
#define OP_ADR_SET_2						(2<<0)
#define OP_ADR_SET_3						(3<<0)

#define ADR_C2TRS0							(OP_ADR_CYCLE_00|OP_ADR_TYPE_ROW|OP_ADR_SET_0)
#define ADR_C2T1S0							(OP_ADR_CYCLE_00|OP_ADR_TYPE_ONE|OP_ADR_SET_0)
#define ADR_C3TRS0							(OP_ADR_CYCLE_01|OP_ADR_TYPE_ROW|OP_ADR_SET_0)
#define ADR_C3TFS0							(OP_ADR_CYCLE_00|OP_ADR_TYPE_FUL|OP_ADR_SET_0)
#define ADR_C4TFS0							(OP_ADR_CYCLE_01|OP_ADR_TYPE_FUL|OP_ADR_SET_0)
#define ADR_C4TFS1							(OP_ADR_CYCLE_01|OP_ADR_TYPE_FUL|OP_ADR_SET_1)
#define ADR_C5TFS0							(OP_ADR_CYCLE_10|OP_ADR_TYPE_FUL|OP_ADR_SET_0)
#define ADR_C6TFS0							(OP_ADR_CYCLE_11|OP_ADR_TYPE_FUL|OP_ADR_SET_0)

// OP Code: Action
#define ACT_WAITRB							0x80 	
#define ACT_CHKSTATUS						0x81    
#define ACT_WAIT_IDLE						0x82	    
#define ACT_WAIT_MMA						0x83     
#define ACT_BREAK							0x88
#define ACT_SER_DOUT						0x90 // for column addr == 0       	
#define ACT_RAN_DOUT						0x91 // for column addr != 0	
#define ACT_SER_DIN							0x98 // for column addr == 0       
#define ACT_RAN_DIN							0x99 // for column addr != 0
#define ACT_PAGECOPY_US						0xA0 	 
#define ACT_PAGECOPY_DS						0xA1 	
#define ACT_REPEAT							0xB0     

//------------------------------------------------------------------

#define NAND_MAIN_ECC_TYPE			ECC_TYPE_4BIT
#define NAND_SPARE_ECC_TYPE			ECC_TYPE_8BIT_24


#define FSR_LLD_ENABLE_CTRLLER_ECC


//===========================================================
// for LINUX functions
//===========================================================
#if defined(NAND_DRV_LINUX) && NAND_DRV_LINUX
#define  UNFD_ST_LINUX						0xC0000000
#define  UNFD_ST_ERR_NO_NFIE				(0x01 | UNFD_ST_LINUX)
#define  UNFD_ST_ERR_E_TIMEOUT				(0x07 | UNFD_ST_LINUX)
#define  UNFD_ST_ERR_W_TIMEOUT				(0x08 | UNFD_ST_LINUX)
#define  UNFD_ST_ERR_R_TIMEOUT				(0x09 | UNFD_ST_LINUX)
#define  UNFD_ST_ERR_ECC_FAIL				(0x0F | UNFD_ST_LINUX)
#define  UNFD_ST_ERR_RST_TIMEOUT			(0x12 | UNFD_ST_LINUX)
#define  UNFD_ST_ERR_MIU_RPATCH_TIMEOUT		(0x13 | UNFD_ST_LINUX)
#define  UNFD_ST_ERR_HAL_R_INVALID_PARAM	(0x14 | UNFD_ST_LINUX)
#define  UNFD_ST_ERR_HAL_W_INVALID_PARAM	(0x15 | UNFD_ST_LINUX)
#define  UNFD_ST_ERR_R_TIMEOUT_RM			(0x16 | UNFD_ST_LINUX)
#define  UNFD_ST_ERR_ECC_FAIL_RM			(0x17 | UNFD_ST_LINUX)
#define  UNFD_ST_ERR_W_TIMEOUT_RM			(0x18 | UNFD_ST_LINUX)
#endif

//===========================================================
// for HAL functions
//===========================================================
#define  UNFD_ST_RESERVED					0xE0000000
#define  UNFD_ST_ERR_E_FAIL					(0x01 | UNFD_ST_RESERVED)
#define  UNFD_ST_ERR_W_FAIL					(0x02 | UNFD_ST_RESERVED)
#define  UNFD_ST_ERR_E_BUSY					(0x03 | UNFD_ST_RESERVED)
#define  UNFD_ST_ERR_W_BUSY					(0x04 | UNFD_ST_RESERVED)
#define  UNFD_ST_ERR_E_PROTECTED			(0x05 | UNFD_ST_RESERVED)
#define  UNFD_ST_ERR_W_PROTECTED			(0x06 | UNFD_ST_RESERVED)

//===========================================================
// for IP_Verify functions
//===========================================================
#define  UNFD_ST_IP							0xF0000000
#define  UNFD_ST_ERR_UNKNOWN_ID				(0x01 | UNFD_ST_IP)
#define  UNFD_ST_ERR_DATA_CMP_FAIL			(0x02 | UNFD_ST_IP)
#define  UNFD_ST_ERR_INVALID_PARAM			(0x03 | UNFD_ST_IP)
#define  UNFD_ST_ERR_INVALID_ECC_REG51		(0x04 | UNFD_ST_IP)
#define  UNFD_ST_ERR_INVALID_ECC_REG52		(0x05 | UNFD_ST_IP)
#define  UNFD_ST_ERR_INVALID_ECC_REG53		(0x06 | UNFD_ST_IP)
#define  UNFD_ST_ERR_SIGNATURE_FAIL			(0x07 | UNFD_ST_IP)



//===========================================================
// NAND Info parameters
//===========================================================
//#define NANDINFO_TAG		'M','S','T','A','R','S','E','M','I','U','N','F','D','C','I','S'
//#define NANDINFO_TAG_LEN	16

#define VENDOR_RENESAS		0x07
#define VENDOR_ST			0x20
#define VENDOR_TOSHIBA		0x98
#define VENDOR_HYNIX		0xAD
#define VENDOR_SAMSUMG		0xEC
#define VENDOR_MICRON		0x2C

#define NAND_TYPE_MLC		BIT0 // 0: SLC, 1: MLC

//===========================================================
// driver parameters
//===========================================================
#define NAND_ID_BYTE_CNT	15



//=====================================================
// misc. do NOT edit the following content.
//=====================================================
#define NAND_DMA_RACING_PATCH		1
#define NAND_DMA_PATCH_WAIT_TIME	10000 // us -> 10ms
#define NAND_DMA_RACING_PATTERN		(((UINT32)'D'<<24)|((UINT32)'M'<<16)|((UINT32)'B'<<8)|(UINT32)'N')

//===========================================================
// Time Dalay, do NOT edit the following content, for NC_WaitComplete use.
//===========================================================
#define DELAY_100us_in_us			100
#define DELAY_500us_in_us			500
#define DELAY_1ms_in_us				1000
#define DELAY_10ms_in_us			10000
#define DELAY_100ms_in_us			100000
#define DELAY_500ms_in_us			500000
#define DELAY_1s_in_us				1000000

#define WAIT_ERASE_TIME				DELAY_1s_in_us
#define WAIT_WRITE_TIME				DELAY_1s_in_us
#define WAIT_READ_TIME				DELAY_100ms_in_us
#define WAIT_RESET_TIME				DELAY_10ms_in_us

#if defined(__KERNEL__) && !defined(__U_BOOT__)
static unsigned int gNfconBaseAddr = 0;
static unsigned int gNfconBurstAddr = 0;
#else
#define spin_lock(x)
#define spin_unlock(x)
#endif


#endif /* #define _FSR_LLD_SDP1004_ */


