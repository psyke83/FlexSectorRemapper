#ifndef _BML_DTV_H_
#define _BML_DTV_H_

#define PARTITION_ID_END			(0xFFFF)			/* nID = UINT16 */
#define DEFAULT_LOAD_ADDRESS        (0xFFFFFFFF)

#define ATTR_SLC_BASIC	FSR_BML_PI_ATTR_RW|FSR_BML_PI_ATTR_SLC
#define ATTR_SLC		FSR_BML_PI_ATTR_RW|FSR_BML_PI_ATTR_SLC|FSR_BML_PI_ATTR_PREWRITING
#define ATTR_MLC_BASIC	FSR_BML_PI_ATTR_RW|FSR_BML_PI_ATTR_MLC
#define ATTR_MLC		FSR_BML_PI_ATTR_RW|FSR_BML_PI_ATTR_MLC|FSR_BML_PI_ATTR_PREWRITING
#define ATTR_SLC_BL		ATTR_SLC|FSR_BML_PI_ATTR_BOOTLOADING
#define ATTR_SLC_STL	ATTR_SLC_BASIC|FSR_BML_PI_ATTR_STL
#define ATTR_MLC_STL	ATTR_MLC_BASIC|FSR_BML_PI_ATTR_STL
#define ENTP			FSR_BML_PI_ATTR_ENTRYPOINT

#define DTV_SUB_DEF_PARTS \
	{ FSR_PARTID_BML0,		0,	0,	"Reserved SLC",		0,		DEFAULT_LOAD_ADDRESS},	\
	{ FSR_PARTID_STL0, 		0,	0,	"mtd_xxx",		0,		DEFAULT_LOAD_ADDRESS},	\
	{ FSR_PARTID_STL1, 		0,	0,	"mtd_xxx",		0,		DEFAULT_LOAD_ADDRESS},	\
	{ FSR_PARTID_STL2, 		0,	0,	"mtd_xxx",		0,		DEFAULT_LOAD_ADDRESS},	\
	{ FSR_PARTID_STL3, 		0,	0,	"mtd_xxx",		0,		DEFAULT_LOAD_ADDRESS},	\
	{ PARTITION_ID_END, 0, 0, 0, 0, 0},	


struct fsr_parts {
	unsigned int id;
	unsigned int blocks;
	unsigned int attr;
	const char* desc;        
	unsigned int load_addr; // address to be loaded in DRAM
	unsigned int reserved; // DRAM address to be written
};

int _SetFSRPI(int nVol, FSRPartI stFSRPartI[FSR_MAX_VOLS]);

#endif
