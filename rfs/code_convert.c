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
 *  @file	fs/rfs/code_convert.c
 *  @brief	Dos name and Unicode name handling operations  
 *
 *
 */

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/rfs_fs.h>
#include <linux/ctype.h>
#include <linux/nls.h>

#include "rfs.h"

/**
 * check the invalid character in the short entry
 * this character will be changed to underscore when configured RFS_VFAT
 * otherwise, making a entry with this character fails
 *
 * @param c	dosname character to check
 * @return	zero on valid character
 */
static inline int check_replace_short(u8 c)
{
	/* replace rule is not changed even though "check=no" mount option is asked.
	 * Because this make a decision to build long entry or not. */
	if ((c == '+') || (c == ',') || (c == ';') || 
			(c == '=') || (c == '[') || (c == ']'))
		return 1;
	
	return 0;
}

/**
 * check the invalid character for FAT file name
 * @param c		unicode character to check 
 * @param no_check	permit ilegal characters
 * @return		zero on valid character
 */
static inline int check_invalid(u16 c, unsigned int no_check)
{
	/* no_check is set if "check=no" mount option is asked */
	if (!no_check)
	{
		/* invalid character in Short/Long File Name */
		if ((c < 0x20)
			|| (c == '\\') || (c == '/') || (c == ':') || (c == '*')
			|| (c == '?') || (c == '\"') || (c == '<') || (c == '>')
			|| (c == '|'))
			return 1;

#ifdef CONFIG_RFS_VFAT
#else
		/* invalid character in only Short File Name */
		if ((c == ' ') || (c == '+') || (c == ',') || (c == ';') 
				|| (c == '=')  || (c == '[') || (c == ']'))
			return 1;
#endif
	}
	else
	{
		/* asset VFAT option */
		/* if require no_check option, VFAT should enabled. */
		/* RFS_BUG_ON(RFS_SB(sb)->options.isvfat != TRUE); */

		/* invalid characters in short/long file name */
		if ((c < 0x20) || (c == '\\') || (c == '/'))
			return 1;
#ifdef CONFIG_RFS_VFAT
#else
		/*
		 * When user confingure without CONFIG_RFS_VFAT, RFS does
		 * not provide "check=no" mount option.
		 * Thus there are no need to check invalid char on here.
		 */
#endif
	}
	
	return 0;
}

/**
 * Function converting cstring to dosname
 * @param dosname	resulting dosname
 * @param cstring	cstring to be converted
 * @param status	flag indicating the capital combination and whether resulting dosname fits the 8.3 dosname or not
 * @param check		flag indicating whether check invalid characters or not
 * @return		zero on sucess, negative error code on failure. 
 */
int rfs_convert_cstring_to_dosname(u8 *dosname, const unsigned char *cstring,
		unsigned int *status, unsigned int check, unsigned int no_check)
{
	const unsigned char *end_of_name = NULL;
	const unsigned char *last_period = NULL;
	unsigned int len = 0;
	unsigned int lossy = FALSE;
	int i = 0;
	unsigned char mixed = 0;

	/* strip all leading spaces and periods */
	while ((*cstring == SPACE) || (*cstring == PERIOD))
	{
		lossy = TRUE;
		cstring++;
	}
	
	len = strlen(cstring);
	if (!len)
		return -EINVAL;

	end_of_name = cstring + len - 1;

	/* check the trailing period & space */
	if ((!check) && ((*end_of_name == PERIOD) || (*end_of_name == SPACE)))
		return -EINVAL;

	/* search for the last embedded period */
	last_period = strrchr(cstring, PERIOD);
	if (last_period == NULL)
		last_period = end_of_name + 1;

	memset(dosname, SPACE, DOS_NAME_LENGTH);

	for (i = 0; (i < DOS_NAME_LENGTH) && (cstring <= end_of_name);
			cstring++)
	{
		/* check invalid character */
		if (check && check_invalid((u16)(*cstring & 0x00ff), no_check))
		{
			DEBUG(DL3, "Invalid character");
			return -EINVAL;
		}

		if (*cstring == SPACE)
		{
			/* not legal in short name */
			lossy = TRUE;
			continue;
		} 

		if (*cstring == PERIOD)
		{
			if (cstring < last_period)
			{
				/* current period is not last one */
				lossy = TRUE;
			}
			else
			{
				/* current period is the last one */
				i = SHORT_NAME_LENGTH;
			}
			continue;
        	} 

#if !defined(CONFIG_RFS_NLS) && defined(CONFIG_RFS_VFAT)
		/* not support non-ASCII */
		if (check && (*cstring & 0x80))
		{
			DEBUG(DL3, "NLS not support");
			return -EINVAL;
		}
#endif

		/* fill dosname */
		if (check_replace_short(*cstring))
		{	/* not legal in short name */
			dosname[i++] = UNDERSCORE;
			lossy = TRUE;
		}
		else if (!isascii(*cstring))
		{	/* not ascii character ; make long name */
			mixed |= (PRIMARY_UPPER | PRIMARY_LOWER);
			dosname[i++] = *cstring;
		}
		else if (isascii(*cstring) && islower(*cstring))
		{	/* lower character */
			if (i < SHORT_NAME_LENGTH) 
				mixed |= PRIMARY_LOWER;
			else 
				mixed |= EXTENSION_LOWER;
			dosname[i++] = toupper(*cstring);
		}
		else if (isascii(*cstring) && isupper(*cstring))
		{	/* upper character */
			if (i < SHORT_NAME_LENGTH) 
				mixed |= PRIMARY_UPPER;
			else
				mixed |= EXTENSION_UPPER;
			dosname[i++] = *cstring;
		}
		else
		{
			dosname[i++] = *cstring;
		}

		if ((i == SHORT_NAME_LENGTH) && ((cstring + 1) < last_period))
		{
			lossy = TRUE;
#ifdef CONFIG_RFS_VFAT
			if (check)
			{
				while (++cstring < last_period)
				{
					if (check_invalid((u16)(*cstring & 0x0ff), no_check))
						return -EINVAL;
				}
			}
#endif
			cstring = last_period;
		}
	} /* end of loop */

	if (cstring <= end_of_name)
	{
		lossy = TRUE;
#ifdef CONFIG_RFS_VFAT
		if (check) {
			while (cstring <= end_of_name)
			{
				if (check_invalid((u16)(*cstring++ & 0x0ff), no_check))
					return -EINVAL;
			}
		}
#endif
	}

	/* post check */
	if (dosname[0] == KANJI_LEAD) 
		dosname[0] = REPLACE_KANJI;

	if (status != NULL)
	{
		*status = 0;

		if ((primary_masked(mixed) == (PRIMARY_UPPER | PRIMARY_LOWER)) 
			|| (extension_masked(mixed) == 
				(EXTENSION_UPPER | EXTENSION_LOWER)))
		{
			put_mix(*status, UPPER_N_LOWER);
		}
		else
		{
			if (primary_masked(mixed) == PRIMARY_LOWER)
				put_mix(*status, PRIMARY_LOWER);
			if (extension_masked(mixed) == EXTENSION_LOWER) 
				put_mix(*status, EXTENSION_LOWER);
		}
		/* always make LFN, if no_check option is enabled */
		if (no_check)
		{
			lossy = TRUE;
		}

		put_lossy(*status, lossy);
	}

	return 0;
}

/**
 * Function converting dos name to cstring
 * @param cstring	dosname to be converted
 * @param dosname	resulting cstring
 * @param sysid		flag indicating the capital combination
 * @return		length of cstring on success
 */
int rfs_convert_dosname_to_cstring(unsigned char *cstring, const u8 *dosname,
		unsigned char sysid)
{
	int i = 0;
	int name_len = 0;

	/* check KANJI */
	if (dosname[0] == REPLACE_KANJI)
	{
		*cstring++ = (s8) KANJI_LEAD;
		i = 1;
	}

	/* convert primary name */
	for (; (i < SHORT_NAME_LENGTH) && (dosname[i] != SPACE); i++)
	{
		if ((sysid & PRIMARY_LOWER) && isascii(dosname[i]))
			*cstring++ = (s8) tolower(dosname[i]);
		else
			*cstring++ = (s8) dosname[i];
	}

	name_len = i;

	/* no extension */
	if (dosname[SHORT_NAME_LENGTH] == SPACE)
	{
		/* Finish conversion */
		goto out;
	}

	*cstring++ = PERIOD;

	/* convert extension name */
	for (i = SHORT_NAME_LENGTH;
			(i < DOS_NAME_LENGTH) && (dosname[i] != SPACE); i++)
	{
		if ((sysid & EXTENSION_LOWER) && isascii(dosname[i]))
			*cstring++ = (s8) tolower(dosname[i]);
		else
			*cstring++ = (s8) dosname[i];
	}

	name_len += ((i - SHORT_NAME_LENGTH) + 1);

out:
	*cstring = '\0';
	return name_len;
}

#ifdef CONFIG_RFS_VFAT

#ifdef CONFIG_RFS_NLS
/**
 * Function to convert encoded character to unicode by NLS table
 * @param nls		NLS table
 * @param chars		encoded characters
 * @param chars_len	the length of character buffer
 * @param uni		the unicode character converted
 * @param lower		flag indicating the case type of unicode character
 * @return		the length of character converted
 */
static int __char2uni(struct nls_table *nls, const unsigned char *chars, int chars_len, wchar_t *uni)
{
	int clen = 0;
	wchar_t uni_tmp;

	clen = nls->char2uni(chars, chars_len, &uni_tmp);
	if (clen > 0)
		*uni = uni_tmp;
	else
		clen = -EINVAL;

	return clen;
}
#endif 	/* CONFIG_RFS_NLS */

/**
 * Function converting cstring to unicode name
 * @param uname		unicode name to be converted
 * @param cstring	resulting cstring
 * @param nls		Linux NLS object pointer for future enchancement
 * @param check		flag indicating wether check invalid characters or not
 * @return		the length of uname on success, negative error code on failure
 *
 * (This Function supposes that all characters are valid for spec.)
 */
#ifdef CONFIG_RFS_NLS
static int __convert_cstring_to_uname(u16 *uname, const unsigned char *cstring,
		struct nls_table *nls) 
{
	/* support nls codepage to convert character */
	int clen = 0;
	int uni_len = 0;
	int cstring_len = 0;
	int i = 0;

	cstring_len = strlen(cstring);

	for (; (cstring[i] != '\0') && (uni_len < MAX_NAME_LENGTH); uni_len++)
	{
		clen = __char2uni(nls, &cstring[i], (cstring_len - i),
				(wchar_t *) &uname[uni_len]);
		if (clen < 0)
		{
			DEBUG(DL3, "Invalid character");
			return -EINVAL;
		}

		i += clen;

		if (uname[uni_len] == 0x00)
			break;

 		/* It supposes that all characters are valid.
		   So, check is always FALSE
		if (check && check_invalid(uname[uni_len])) {
			DEBUG(DL3, "Invalid character");
			return -EINVAL;
		}
		*/
	}

	/* the length of unicode name is never over the limitation */
	uname[uni_len] = 0x0000;

	/* return the length of name */
	return uni_len;
}
#else	/* !CONFIG_RFS_NLS */ 	/* if not support nls */
static int __convert_cstring_to_uname(u16 *uname, const unsigned char *cstring)
{

	int i = 0;

	while (cstring[i] != 0x00)
	{
		if (cstring[i] & 0x80)
		{
			/* cannot convert to unicode without codepage */
			DEBUG(DL3, "NLS not support");
			return -EINVAL;
		}

		uname[i] = (u16) (cstring[i] & 0x00ff);
		i++;
	}

	/* add the null */
	uname[i] = 0x0000;
	return i;
}
#endif	/* CONFIG_RFS_NLS */

/**
 * Function converting unicode name to cstring
 * @param cstring	resulting converted
 * @param uname		unicode name to be converted
 * @param nls		Linux NLS object pointer for future use
 * @return		the length of cstring on success, negative error code on failure
 */
#ifdef CONFIG_RFS_NLS
int rfs_convert_uname_to_cstring(unsigned char *cstring, const u16 *uname, struct nls_table *nls)
{
	unsigned char buf[NLS_MAX_CHARSET_SIZE];
	const u16 *uname_ptr = uname;
	int i = 0;
	int stridx = 0;

	/* need nls codepage to convert character */
	for (;(i < MAX_NAME_LENGTH) && (stridx <= NAME_MAX); i++, uname_ptr++)
	{
		int clen = 0;

		/* end of uname is NULL */
		if (*uname_ptr == 0x0000)
			break;

		clen = nls->uni2char(*uname_ptr, buf, sizeof(buf));
		if (clen <= 0)
		{
			cstring[stridx] = UNDERSCORE;
			clen = 1;
		}
		else if (clen == 1)
		{
			cstring[stridx] = buf[0];
		}
		else
		{
			if (stridx + clen > NAME_MAX)
				break;
			memcpy(&cstring[stridx], buf, clen);
		}

		stridx += clen;
	}

	cstring[stridx] = '\0';
	return stridx;
}

#else	/* !CONFIG_RFS_NLS */ 	/* if not support nls */
int rfs_convert_uname_to_cstring(unsigned char *cstring, const u16 *uname, struct nls_table *nls)
{
	int i = 0;

	for (; i < MAX_NAME_LENGTH; i++)
	{
		/* cannot convert */
		if (uname[i] & 0xff80)
			cstring[i] = UNDERSCORE;
		else 
			cstring[i] = (s8) uname[i];

		if (cstring[i] == '\0')
			break;
	}

	return i;

}
#endif	/* CONFIG_RFS_NLS */
#endif	/* CONFIG_RFS_VFAT */

/**
 * translate encoded string from user to 8.3 dosname and unicode
 *
 * @param cstring	encoded string from user IO
 * @param dosname	resulting dosname of encoded string
 * @param unicode	resulting unicode name
 * @param status	flag indicating the capital combination and whether resulting dosname fits the 8.3 dosname or not
 * @param nls		NLS codepage table
 * @param check		flag whether check invalid characters or not
 * @return		zero or length of uname on success, or errno on failure
 */
int rfs_convert_dosname(const unsigned char *cstring, u8 *dosname, u16 *unicode, unsigned int *status, struct super_block *sb, unsigned int check)
{
	/*
	 * support NLS or not
	 */
	int ret = 0;

#ifdef CONFIG_RFS_NLS
	struct nls_table *nls = RFS_SB(sb)->nls_disk;

	if (unlikely(!nls))	/* out-of-range input */
		return -EINVAL;
#endif	/* CONFIG_RFS_NLS */

	if (dosname)
	{
		/* check trailing space, period */
		ret = rfs_convert_cstring_to_dosname(dosname, cstring, status, check, test_opt(sb, CHECK_NO));
		if (ret < 0)
			return ret;
	}

#ifdef CONFIG_RFS_VFAT
	if (unicode && status)
	{
		/* 
		 * don't check the length of unicode name
		 * because it's checked at rfs_lookup
		 */

		/* make unicode only if condition is satisfied */
		if (get_lossy(*status) || (get_mix(*status) == UPPER_N_LOWER))
		{
			/* don't check the length of unicode */
#ifdef CONFIG_RFS_NLS
			ret = __convert_cstring_to_uname(unicode, cstring, nls);
#else
			ret = __convert_cstring_to_uname(unicode, cstring);
#endif
			if (ret < 0)
				return ret;
		}
		else
		{
			unicode[0] = 0x00;
			return 0;
		}
	}
#endif	/* CONFIG_RFS_VFAT */

	return ret;
}

/**
 * compare string translate encoded string from user to 8.3 dosname and unicode
 *
 * @param sb		super_block
 * @param ext_uname	unicode name (from ext entries)
 * @param cstring	encoded string (from user IO)
 * @param len		length to compare
 * @return		zero when identical else 1 or errno on failure
 */

int rfs_strnicmp(struct super_block *sb, const u16 *uname_1, 
		const unsigned char *cstring_2)
{
	struct nls_table *nls = RFS_SB(sb)->nls_disk;

	unsigned char cstring_1[NAME_MAX + 1];
	int len_1, len_2;

	len_1 = rfs_convert_uname_to_cstring(cstring_1, uname_1, nls);
	len_2 = strlen(cstring_2);

	if (len_1 != len_2)
		return 1;
#ifdef CONFIG_RFS_NLS
	DEBUG(DL3, "[ext:%s,name:%s],len:%d", cstring_1, cstring_2, len_1);
	return  nls_strnicmp(nls, cstring_1, cstring_2, len_1);
#else
	/* this is dead code. 
	 * rfs invoks rfs_strnicmp() when RFS_VFAT enabled. 
	 * CONFIG_RFS_NLS come together RFS_VFAT. 
	 */
{
	int i = 0;
	for (; i < len_1; i++)
	{
		if (uname_1[i] & 0xff80)
		{
			continue;
		}
		if (__tolower((s8) uname_1[i]) != __tolower(cstring_2[i]))
		{
			DEBUG(DL3, "s1:%c != s2:%c", 
					__tolower((s8) uname_1[i]), 
					__tolower(cstring_2[i]));
			return 1;
		}
	}
}
	return 0;
#endif
}

