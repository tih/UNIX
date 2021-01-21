#
#include "../param.h"	/* for possible STAT definition */
/*
 * c23.c	configuration file for Computer Science's 11/23.
 *
 *	Copyright 1975 Bell Telephone Laboratories Inc
 */

int	(*bdevsw[])()
{
	&rx2open,	&nulldev,	&rx2strategy,	&rx2tab,/* rx */
	&nulldev,	&nulldev,	&rkstrategy, 	&rktab,	/* rk */
	0
};

int	(*cdevsw[])()
{
	&klopen,   &klclose,  &klread,   &klwrite,  &klsgtty,	/* console */
	&nulldev,  &nulldev,  &mmread,   &mmwrite,  &nodev,	/* mem */
	&nulldev,  &nulldev,  &rkread,   &rkwrite,  &nodev,	/* rk */
	&syopen,   &nulldev,    &syread,   &sywrite,   &sysgtty,  /* sys */
	&rx2open,  &nulldev,	&rx2read, &rx2write, &rx2sgtty,	/* rx */
#ifdef	STAT
	&stopen,  &stclose,   &stread,   &nodev,     &nodev,	/*  st */
#endif
#ifndef	STAT
	&nodev,    &nodev,  &nodev,  &nodev,  	&nodev,		/* st */
#endif
	0
};

int	rootdev	{(0<<8)|2};	/* /dev/rx2 */
int	swapdev	{(0<<8)|2};	/* /dev/rx2 */
/*
 * first level boot strap in block 0,
 * file system is in blocks 1...699
 * swap area in blocks 700 to 998
 * file system bootstrap in blocks 999 and 1000.
 */
int	swplo	700;	/* cannot be zero */
int	nswap	299;
