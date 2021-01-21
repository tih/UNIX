#
/*
 *	Copyright 1973 Bell Telephone Laboratories Inc
 */

/*
 * TJU16 tape driver
 */

#include "../param.h"
#include "../buf.h"
#include "../conf.h"
#include "../user.h"

struct {
	int	htcs1;
	int	htwc;
	int	htba;
	int	htfc;
	int	htcs2;
	int	htds;
	int	hter;
	int	htas;
	int	htck;
	int	htdb;
	int	htmr;
	int	htdt;
	int	htsn;
	int	httc;
};

struct	devtab	httab;
struct	buf	rhtbuf;

char	h_openf[8];
char	*h_blkno[8];
char	*h_nxrec[8];

#define	HTADDR	0172440

#define	GO	01
#define	NOP	0
#define	WEOF	026
#define	SFORW	030
#define	SREV	032
#define	ERASE	024
#define	REW	06
#define	CLR	010
#define	P1600	02300		/* 1600 + pdp11 mode */
#define	IENABLE	0100
#define	CRDY	0200
#define	EOF	04
#define	DRY	0200
#define	MOL	010000
#define	PIP	020000
#define	ERR	040000

#define	SSEEK	1
#define	SIO	2

htopen(dev, flag)
{
	register unit;

	unit = dev.d_minor;
	if (h_openf[unit])
		u.u_error = ENXIO;
	else {
		h_openf[unit]++;
		h_blkno[unit] = 0;
		h_nxrec[unit] = 65535;
		hcommand(unit, NOP);
	}
}

htclose(dev, flag)
{
	register int unit;

	unit = dev.d_minor;
	h_openf[unit] = 0;
	if (flag) {
		hcommand(unit, WEOF);
		hcommand(unit, WEOF);
	}
	hcommand(unit, REW);
}

hcommand(unit, com)
{
	extern lbolt;

	while (httab.d_active || (HTADDR->htcs1 & CRDY)==0)
		sleep(&lbolt, 1);
	HTADDR->htcs2 = unit>>3;
	while((HTADDR->htds&DRY) == 0)
		sleep(&lbolt, 1);
	HTADDR->httc = P1600 | (unit&07);
	while((HTADDR->htds&(MOL|PIP)) != MOL)
		sleep(&lbolt, 1);
	HTADDR->htcs1 = com | GO;
}

htstrategy(abp)
struct buf *abp;
{
	register struct buf *bp;
	register char **p;

	bp = abp;
	p = &h_nxrec[bp->b_dev.d_minor];
	if (*p <= bp->b_blkno) {
		if (*p < bp->b_blkno) {
			bp->b_flags =| B_ERROR;
			iodone(bp);
			return;
		}
		if (bp->b_flags&B_READ) {
			clrbuf(bp);
			iodone(bp);
			return;
		}
	}
	if ((bp->b_flags&B_READ)==0)
		*p = bp->b_blkno + 1;
	bp->av_forw = 0;
	spl5();
	if (httab.d_actf==0)
		httab.d_actf = bp;
	else
		httab.d_actl->av_forw = bp;
	httab.d_actl = bp;
	if (httab.d_active==0)
		htstart();
	spl0();
}

htstart()
{
	register struct buf *bp;
	register int unit;
	register char *blkno;

    loop:
	if ((bp = httab.d_actf) == 0)
		return;
	unit = bp->b_dev.d_minor;
	HTADDR->htcs2 = unit>>3;
	HTADDR->httc = P1600 | (unit&07);
	blkno = h_blkno[unit];
	if (h_openf[unit] < 0 || (HTADDR->htcs1 & CRDY)==0) {
		bp->b_flags =| B_ERROR;
		httab.d_actf = bp->av_forw;
		iodone(bp);
		goto loop;
	}
	if (blkno != bp->b_blkno) {
		httab.d_active = SSEEK;
		if (blkno < bp->b_blkno) {
			HTADDR->htfc = blkno - bp->b_blkno;
			HTADDR->htcs1 = SFORW|IENABLE|GO;
		} else {
			if (bp->b_blkno == 0)
				HTADDR->htcs1 = REW|IENABLE|GO;
			else {
				HTADDR->htfc = bp->b_blkno - blkno;
				HTADDR->htcs1 = SREV|IENABLE|GO;
			}
		}
		return;
	}
	httab.d_active = SIO;
	rhstart(bp, &HTADDR->htfc, bp->b_wcount<<1);
}

htintr()
{
	register struct buf *bp;
	register int unit;

	if ((bp = httab.d_actf)==0)
		return;
	unit = bp->b_dev.d_minor;
	if (HTADDR->htcs1 & ERR) {
/*
		deverror(bp, HTADDR->hter);
 */
		if(HTADDR->htds&EOF) {
			if(bp != &rhtbuf && h_openf[unit])
				h_openf[unit] = -1;
		}
		HTADDR->htcs1 = ERR|CLR|GO;
		if ((HTADDR->htds&DRY)!=0 && httab.d_active==SIO) {
			if (++httab.d_errcnt < 10) {
				h_blkno[unit]++;
				httab.d_active = 0;
				htstart();
				return;
			}
		}
		bp->b_flags =| B_ERROR;
		httab.d_active = SIO;
	}
	if (httab.d_active == SIO) {
		httab.d_errcnt = 0;
		h_blkno[unit]++;
		httab.d_actf = bp->av_forw;
		httab.d_active = 0;
		iodone(bp);
		bp->b_resid = HTADDR->htfc;
	} else
		h_blkno[unit] = bp->b_blkno;
	htstart();
}

htread(dev)
{
	htphys(dev);
	physio(htstrategy, &rhtbuf, dev, B_READ);
	u.u_count = -rhtbuf.b_resid;
}

htwrite(dev)
{
	htphys(dev);
	physio(htstrategy, &rhtbuf, dev, B_WRITE);
	u.u_count = 0;
}

htphys(dev)
{
	register unit, a;

	unit = dev.d_minor;
	a = lshift(u.u_offset, -9);
	h_blkno[unit] = a;
	h_nxrec[unit] = ++a;
}
