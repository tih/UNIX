/*
 * RL-11/RL01/02 disk driver
 *	changes b_addr, b_xmem, and b_blkno for raw IO.
 *	This is not the best way to do it.
 */

#include "../param.h"
#include "../buf.h"
#include "../conf.h"
#include "../user.h"

#define	RLADDR	0174400
#define	NRL	4
#define	NRLBLK	20480	/* RL02 readable space, includes bad sector table */
#define	NRLWBLK	(NRLBLK - NBPT)	/* writable space */

#define NBPT	20	/* blocks (2 sectors) per track */
#define NODRIVE (-1)

/* rlcs */
#define DE 040000
#define NXM 020000
#define DLT 010000
#define CRC 04000
#define OPI 02000
#define	CTLRDY	0200
#define	IENABLE	0100
#define NOP	0	/* no-op | reset | controller-test */
#define GETSTAT	04
#define SEEK	06
#define READHD	010
#define WCOM 012
#define RCOM 014
#define	DRVRDY	01	/* unused */

/* rlda: get/reset status */
#define	RESET	010
#define STS	02
#define MARK	01	/* also during seek */

/* rlda: IO */
#define CYL 0177600
#define SURF 0100
#define SECTOR 077

/* rlda: seek */
#define MVCENTER 04

/* rlmp: get status */
#define VC 01000
#define DT 0200	/* drive is an RL02 */

#define SSEEK	1
#define SIO	2

#define ioct(bp) ((unsigned) (bp)->av_back)	/* bytes in current IO */
#define bcount(wc) (-(wc) << 1)	/* in v6, b_wcount is negative */

#define	BSIZE	512
#define	BSHIFT	9
#define	BMASK	(BSIZE-1)

struct {
	int rlcs;
	int rlba;
	int rlda;
	int rlmp;
};

struct	devtab	rltab;
struct	buf	rrlbuf;
int rldrive = NODRIVE;
struct rl {
	unsigned rl_curr;	/* current cylinder */
	unsigned rl_known : 1;	/* known position */
} rl[NRL];

rlstrategy(bp)
register struct buf *bp;
{
	register int d;

#ifdef B_PHYS
	if(bp->b_flags&B_PHYS)
		mapalloc(bp);
#endif
	d = bp->b_dev.d_minor - 7;
	if(d <= 0)
		d = 1;
	if (bp->b_blkno >= NRLBLK*d ||
	    (bp->b_flags&B_READ) == 0 && bp->b_blkno >= NRLWBLK*d) {
		bp->b_flags =| B_ERROR;
		iodone(bp);
		return;
	}
	bp->av_forw = NULL;
	spl5();
	if (rltab.d_actf==NULL)
		rltab.d_actf = bp;
	else
		rltab.d_actl->av_forw = bp;
	rltab.d_actl = bp;
	bp->b_resid = 0;
	if (rltab.d_active==NULL)
		rlstart();
	spl0();
}

rlstart()
{
	register struct buf *bp;
	register unsigned * rlcp;
	register unsigned newcyl, new, diff, bc, b;
	register int com;
	int d, m, sector;
	unsigned nb;

	if ((bp = rltab.d_actf) == NULL) {
		rldrive = NODRIVE;
		return;
	}
	rltab.d_active = SSEEK;
	b = bp->b_blkno;
	m = bp->b_dev.d_minor - 7;
	if(m <= 0)
		d = bp->b_dev.d_minor;
	else {
		d = b % m;
		b /= m;
	}
	sector = b % NBPT;
	new = (b/NBPT)<<6 | sector<<1;
	while ((RLADDR->rlcs & CTLRDY) == 0) ;
	if (!rl[d].rl_known)
		rlreset(d);
	rlcp = &rl[d].rl_curr;
	newcyl = new & CYL;
	if (newcyl > *rlcp)
		diff = (newcyl - *rlcp) | MVCENTER;
	else
		diff = *rlcp - newcyl;
	*rlcp = newcyl;
	RLADDR->rlda = (diff + MARK) | (new & SURF)>>2;
	RLADDR->rlcs = SEEK | d << 8;
	rltab.d_active = SIO;
	if ((bc = bcount(bp->b_wcount) - bp->b_resid) > BSIZE) {
		nb = bc >> BSHIFT;
		if (bc & BMASK)
			nb++;
		if (sector + nb > NBPT)
			bc = (NBPT - sector) << BSHIFT;
	}
	ioct(bp) = bc;
	com = (bp->b_xmem&03)<<4 | IENABLE | (d&03)<<8;
	if (bp->b_flags & B_READ)
		com =| RCOM;
	else
		com =| WCOM;
	while ((RLADDR->rlcs & CTLRDY) == 0)
		;
	RLADDR->rlda = new;
	RLADDR->rlba = bp->b_addr;
	RLADDR->rlmp = -(bc>>1);
	RLADDR->rlcs = com;
	rldrive = d;
}

rlintr()
{
	register struct buf *bp;

	if (rltab.d_active == NULL || rldrive == NODRIVE)
		return;
	bp = rltab.d_actf;
	rltab.d_active = NULL;
	if (RLADDR->rlcs < 0) {		/* error bit */
		if (RLADDR->rlcs&DE) {
			register int status;

			RLADDR->rlda = STS|MARK;
			rlexec(GETSTAT, rldrive);
			status = RLADDR->rlmp;
			if (status&VC)
				--rltab.d_errcnt;
			else {
				printf("rlmp; ");
				deverror(bp, status, RLADDR->rlda);
			}
		}
		else if (RLADDR->rlcs&(NXM|DLT|CRC|OPI)) {
			printf("rlcs; ");
			deverror(bp, RLADDR->rlcs, RLADDR->rlda);
		}
		rlreset(rldrive);
		if (++rltab.d_errcnt <= 10) {
			rlstart();
			return;
		}
		else
			bp->b_flags =| B_ERROR;
	}
	else {
		bp->b_resid =+ ioct(bp);
		if (bcount(bp->b_wcount) != bp->b_resid) {
			register unsigned a;

			bp->b_blkno =+ ioct(bp) >> BSHIFT;
			a = bp->b_addr;
			if ((bp->b_addr =+ ioct(bp)) < a)	/* overflow */
				bp->b_xmem++;
			rlstart();
			return;
		}
	}
	rltab.d_errcnt = 0;
	rltab.d_actf = bp->av_forw;
	bp->b_resid = 0;
	iodone(bp);
	rlstart();
}

rlreset(drive)
register int drive;
{
	RLADDR->rlda = RESET|STS|MARK;
	rlexec(GETSTAT, drive);
	rlexec(READHD, drive);
	rl[drive].rl_curr = RLADDR->rlmp & CYL;
	rl[drive].rl_known = 1;
}

rlexec(c, d)
{
	RLADDR->rlcs = c | (d&03)<<8;
	while((RLADDR->rlcs & CTLRDY) == 0)
		;
}

rlread(dev)
{
	physio(rlstrategy, &rrlbuf, dev, B_READ);
}

rlwrite(dev)
{
	physio(rlstrategy, &rrlbuf, dev, B_WRITE);
}
