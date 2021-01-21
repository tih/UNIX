#
#define	STAT	0
/*
 *	Copyright 1975 Bell Telephone Laboratories Inc
 */

int	(*bdevsw[])()
{
	&nulldev,	&nulldev,	&rkstrategy, 	&rktab,	/* rk */
	&nodev,		&nodev,		&nodev,		0,	/* rp */
	&nodev,		&nodev,		&nodev,		0,	/* rf */
	&tmopen,	&tmclose,	&tmstrategy,	&tmtab,	/* tm */
	&nulldev,	&tcclose,	&tcstrategy, 	&tctab,	/* tc */
	&nodev,		&nodev,		&nodev,		0,	/* hs */
	&hmopen,	&nulldev,	&hmstrategy,	&hmtab,	/* hm */
	&rx2open,	&nulldev,	&rx2strategy,	&rx2tab,/* rx */
	0
};

int	(*cdevsw[])()
{
	&klopen,   &klclose,  &klread,   &klwrite,  &klsgtty,	/* console */
	&pcopen,   &pcclose,  &pcread,   &nodev,    &nodev,	/* pc */
	&lpopen,   &lpclose,  &nodev,    &lpwrite,  &nodev,     /* lp */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/* dc */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/* dh */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/* dp */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/* dj */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/* dn */
	&nulldev,  &nulldev,  &mmread,   &mmwrite,  &nodev,	/* mem */
	&nulldev,  &nulldev,  &rkread,   &rkwrite,  &nodev,	/* rk */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/* rf */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/* rp */
	&tmopen,   &tmclose,  &tmread,   &tmwrite,  &tmstty,    /* tm */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/* hs */
	&hmopen,   &nodev,    &hmread,   &hmwrite,  &nodev,	/* hm */
	&cropen,   &crclose,  &crread,   &nodev,    &nodev,	/*  cr */
	&xyopen,   &xyclose,  &nodev,    &xywrite,  &nodev,	/*  xy */
	&icopen,   &icclose,  &icread,   &icwrite,  &icsgtty,	/*  ic */
	&syopen,   &nulldev,  &syread,   &sywrite,   &sysgtty,  /* sys */
        &iropen,   &irclose,  &irread,   &nodev,     &nodev,    /* ir */
	&dzopen,  &dzclose,   &dzread,   &dzwrite,   &dzsgtty,	/*  dz  */
#ifdef	STAT
	&stopen,  &stclose,   &stread,   &nodev,     &nodev,	/*  st */
#endif
#ifndef	STAT
	&nodev,    &nodev,  &nodev,  &nodev,  	&nodev,		/* st */
#endif
	&dupopen, &dupclose,  &dupread,  &dupwrite,   &nodev,	/* dup */
	&fakeopen, &fakeclose, &fakeread, &fakewrite, &fakegstty, /* fake */
	&rx2open,  &nulldev,	&rx2read, &rx2write, &rx2sgtty,	/* rx */
	0
};

int	rootdev	{(0<<8)|0};
int	swapdev	{(0<<8)|0};
int	swplo	3600;	/* cannot be zero */
int	nswap	1272;
