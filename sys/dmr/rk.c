#
/*
 *	Copyright 1973 Bell Telephone Laboratories Inc
 */

/*
 * RK disk driver
 */

#include "../param.h"
#include "../buf.h"
#include "../conf.h"
#include "../user.h"

#define	RKADDR	0177400
#define	NRK	4
#define	NRKBLK	4872

#define	RESET	0
#define	GO	01
#define	DRESET	014
#define	IENABLE	0100
#define	DRY	0200
#define	ARDY	0100
#define	WLO	020000
#define	CTLRDY	0200

#define	LATE	01000		/* bus didn't get there in time */
struct {
	int rkds;
	int rker;
	int rkcs;
	int rkwc;
	int rkba;
	int rkda;
};

struct	devtab	rktab;
struct	buf	rrkbuf;

#ifdef HMRK
struct	devtab	hmtab;		/* if both in use */
#endif

rkstrategy(abp)
struct buf *abp;
{
	register struct buf *bp;
	register char *p1, *p2;
	int d;

	bp = abp;
	d = bp->b_dev.d_minor-7;
	if(d <= 0)
		d = 1;
	if (bp->b_blkno >= NRKBLK*d) {
		bp->b_flags =| B_ERROR;
		iodone(bp);
		return;
	}
	bp->av_forw = 0;
	spl5();
	if ((p1 = rktab.d_actf)==0)
		rktab.d_actf = bp;
	else {
		for (; p2 = p1->av_forw; p1 = p2) {
			if (p1->b_blkno <= bp->b_blkno
			 && bp->b_blkno <  p2->b_blkno
			 || p1->b_blkno >= bp->b_blkno
			 && bp->b_blkno >  p2->b_blkno) 
				break;
		}
		bp->av_forw = p2;
		p1->av_forw = bp;
	}
	if (rktab.d_active==0)
		rkstart();
	spl0();
}

rkaddr(bp)
struct buf *bp;
{
	register struct buf *p;
	register int b;
	int d, m;

	p = bp;
	b = p->b_blkno;
	m = p->b_dev.d_minor - 7;
	if(m <= 0)
		d = p->b_dev.d_minor;
	else {
		d = lrem(b, m);
		b = ldiv(b, m);
	}
	return(d<<13 | (b/12)<<4 | b%12);
}

rkstart()
{
	register struct buf *bp;

#ifdef HMRK
	if (hmtab.d_active)
		return;		/* if hm active then quit */
#endif
	if ((bp = rktab.d_actf) == 0)
		{
#ifdef HMRK
		if (hmtab.d_actf)
			hmstart();	/* no rk io ... start hm */
#endif
		return;
		}
	rktab.d_active++;
	devstart(bp, &RKADDR->rkda, rkaddr(bp), 0);
}

rkintr()
{
	register struct buf *bp;

	if (rktab.d_active == 0)
		return;
	bp = rktab.d_actf;
	rktab.d_active = 0;
	if (RKADDR->rkcs < 0) {		/* error bit */
		if ((RKADDR->rker&LATE) == 0)
			deverror(bp, RKADDR->rker);
		RKADDR->rkcs = RESET|GO;
		while((RKADDR->rkcs&CTLRDY) == 0) ;
		if (++rktab.d_errcnt <= 10) {
			rkstart();
			return;
		}
		bp->b_flags =| B_ERROR;
	}
	rktab.d_errcnt = 0;
	rktab.d_actf = bp->av_forw;
	iodone(bp);
	rkstart();
}

rkread(dev)
{

	physio(rkstrategy, &rrkbuf, dev, B_READ);
}

rkwrite(dev)
{

	physio(rkstrategy, &rrkbuf, dev, B_WRITE);
}
