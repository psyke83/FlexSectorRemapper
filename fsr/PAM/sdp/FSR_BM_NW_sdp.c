/*****************************************************************************/
/* Header file declararation                                                 */
/*****************************************************************************/
#if defined(SUB_FLASH_AUTO_FORMAT)
#include    "FSR.h"
#include    "FSR_PartitionID.h"
#include    "FSR_LLD_OneNAND.h"
#include    "dtv_bml.h"

#define MAX_PART_COUNT	20
/**************************************************
 * DTV ONENAND default partition definition
 **************************************************/
static struct fsr_parts sub_def_parts[] = {
	DTV_SUB_DEF_PARTS
};

int _SetFSRPI(int nVol, FSRPartI    stFSRPartI[FSR_MAX_VOLS])
{
	int i = 0;	
	unsigned int vbn = 0;
	struct fsr_parts* part = sub_def_parts;
	
	if (nVol == 0)
		return 0;
	
	stFSRPartI[nVol].nNumOfPartEntry = 0;
	do {
		stFSRPartI[nVol].stPEntry[i].nID = part->id;
		stFSRPartI[nVol].stPEntry[i].nAttr = part->attr;
		stFSRPartI[nVol].stPEntry[i].n1stVun = vbn;
		stFSRPartI[nVol].stPEntry[i].nNumOfUnits = part->blocks;
		stFSRPartI[nVol].stPEntry[i].nLoadAddr = part->load_addr;
		stFSRPartI[nVol].stPEntry[i].nReserved = part->reserved;		
		vbn += part->blocks;
		i++;
		part++;
	} while (part->id != PARTITION_ID_END && i <MAX_PART_COUNT);
	stFSRPartI[nVol].nNumOfPartEntry = i;
	return i;
}

PUBLIC BOOL32
FSR_BM_SetFSRPI(UINT32      nVol,
                FSRVolSpec *pstVolSpec,
                FSRPartI    stFSRPartI[FSR_MAX_VOLS])
{
    //UINT32  nUsableUnits;
    UINT32  nNextId;    

    FSR_OAM_MEMCPY(stFSRPartI[nVol].aSig, "FSRPART", sizeof("FSRPART"));
    stFSRPartI[nVol].nVer                        = 0x00010000;

    switch (nVol)
    {
    /* set partition information for FSR volume 0 */
    case 0:
	 	nNextId = _SetFSRPI(nVol, stFSRPartI);
        break;

    case 1:
        nNextId = _SetFSRPI(nVol, stFSRPartI);
        break;

    default:
        return -1;
    }

    return 1;
}
#endif

