#
/*	#define	MX	1	/* if mini-unix system version */

#include "../param.h"
#include "../buf.h"
#include "../user.h"
#include "../proc.h"
#include "../conf.h"

#ifndef MX
#define	CTRL	1		/* define for control functions (formatting) */
#endif

#define	tracef	if (rx2dbg) printf	/* if tracing desired */
int rx2dbg 0;
int rx2cgp 0;

/*
 * RX02 floppy disk device driver
 *
 * Bill Shannon   CWRU   05/29/79
 *
 *
 *	Layout of logical devices:
 *
 *	name	min dev		unit	density		mapped?
 *	----	-------		----	-------		-------
 *	rx0	   0		  0	single		yes
 *	rx1	   1		  1	single		yes
 *	rx2	   2		  0	double		yes
 *	rx3	   3		  1	double		yes
 *	rx4	   4		  0	single		no
 *	rx5	   5		  1	single		no
 *	rx6	   6		  0	double		no
 *	rx7	   7		  1	double		no
 *
 *
 *	Stty function call may be used to format a disk.
 *	To enable this feature, define CTRL in this module
 *	and add #define B_CTRL 020000 to ../buf.h
 */

/* The following structure is used to access av_back as an integer */

struct {
	int	dummy0;
	struct	buf *dummy1;
	struct	buf *dummy2;
	struct	buf *dummy3;
	int	seccnt;
};

struct {
	int	rx2cs;
	int	rx2db;
};

#define	RETRY	8		/* # of times to re-try operation */

/*
 *	the following defines use some fundamental
 *	constants of the RX02.
 */
#define	RXADDR	0177170
#define	NSPB	((bp->b_dev.d_minor&2) ? 2 : 4)	/* Number of floppy sectors per unix block */
#define	NBPS	((bp->b_dev.d_minor&2) ? 256 : 128)	/* Number of bytes per floppy sector	*/
#define	NWPS	((bp->b_dev.d_minor&2) ? 128 : 64)	/* Number of words per floppy sector	*/
#define	NRXBLKS	((bp->b_dev.d_minor&2) ? 1001 : 500)	/* Number of unix blocks on a floppy */
#define	DENSITY	(bp->b_dev.d_minor&2)	/* Density: 0 = single, 2 = double */
#define	UNIT	(bp->b_dev.d_minor&1)	/* Unit Number: 0 = left, 1 = right */
#define	MAPPED	((bp->b_dev.d_minor&4) == 0)	/* not mapped to RT-11 format */

#define	B_CTRL	020000

#define	TRWAIT		while (ptcs->lobyte >= 0)

struct	devtab	rx2tab;

struct	buf	rrx2buf;
int rx2buf[128];	/* transfer buffer */

#ifdef CTRL
struct	buf	crx2buf;	/* buffer header for control functions */
#endif

#define	GO	0000001	/* execute command function	*/
#define	UNIT1	0000020	/* unit select (drive 0=0, 1=1)	*/
#define	RXDONE	0000040	/* function complete		*/
#define	INTENB	0000100	/* interrupt enable		*/
#define	TRANREQ	0000200	/* transfer request (data only)	*/
#define	RXINIT	0040000	/* rx211 initialize		*/
#define	RXERROR	0100000	/* general error bit		*/

/*
 *	rx211 control function bits 1-3 of rx2cs
 */
#define	FILL	0000000	/* fill buffer			*/
#define	EMPTY	0000002	/* empty buffer			*/
#define	WRITE	0000004	/* write buffer to disk		*/
#define	READ	0000006	/* read disk sector to buffer	*/
#define	FORMAT	0000010	/* set media density (format)	*/
#define	RSTAT	0000012	/* read disk status		*/
#define	WSDD	0000014	/* write sector deleted data	*/
#define	RDERR	0000016	/* read error register function	*/

/*
 * Drive status returned in data buffer at end of function.
 */
#define	CRCERR		000001
#define	PARERR		000002
#define	INITDONE	000004
#define	WRITPROT	000010
#define	DENERR		000020
#define	DRVDEN		000040
#define	DELDATA		000100
#define	DRVREADY	000200
#define	UNITSEL		000400
#define	HEADSEL		001000
#define	WCOVFL		002000

/*
 *	states of driver, kept in d_active
 */
#define	SREAD	1	/* read started  */
#define	SEMPTY	2	/* empty started */
#define	SFILL	3	/* fill started  */
#define	SWRITE	4	/* write started */
#define	SINIT	5	/* init started  */
#define	SFORMAT	6	/* format started */


rx2open(dev, flag)
{
	tracef("rx2open %d\n",dev);	/* debug */
	if(dev.d_minor >= 8)
		u.u_error = ENXIO;
}

rx2strategy(bp)
 struct buf *bp;
{

	tracef("rx2strategy %o\n",bp);	/* debug */
	tracef("blk %d flags 0%o wc -%d addr 0%o xm 0%o\n",bp->b_blkno,bp->b_flags,-bp->b_wcount,bp->b_addr,bp->b_xmem); /* debug */
	if(bp->b_blkno >= NRXBLKS) {
		if(bp == &rrx2buf && bp->b_flags&B_READ)
			bp->b_resid = bp->b_wcount;
		else {
			bp->b_flags =| B_ERROR;
			bp->b_error = ENXIO;
		}
		iodone(bp);
		return;
	}
	bp->av_forw = 0;
	/*
	 * seccnt is actually the number of floppy sectors transferred,
	 * incremented by one after each successful transfer of a sector.
	 */
	bp->seccnt = 0;
	/*
	 * We'll modify b_resid as each piece of the transfer
	 * successfully completes.  It will also tell us when
	 * the transfer is complete.
	 */
	bp->b_resid = bp->b_wcount;
	spl5();
	if(rx2tab.d_actf == 0)
		rx2tab.d_actf = bp;
	else
		rx2tab.d_actl->av_forw = bp;
	rx2tab.d_actl = bp;
	if(rx2tab.d_active == 0)
		rx2start();
	spl0();
}

rx2start()
{
	register int *ptcs, *ptdb;
	register struct buf *bp;
	int sector, track;
	char *addr, *xmem;
	int nw;

	if((bp = rx2tab.d_actf) == 0) {
		rx2tab.d_active = 0;
		return;
	}

	tracef("rx2start %o\n",bp);	/* debug */
	ptcs = &RXADDR->rx2cs;
	ptdb = &RXADDR->rx2db;


#ifdef CTRL
	if (bp->b_flags&B_CTRL) {	/* is it a control request ? */
		if (rx2tab.d_errcnt == 0)
			rx2tab.d_errcnt = RETRY-1;	/* try it twice only */
		rx2tab.d_active = SFORMAT;
		*ptcs = FORMAT | GO | INTENB | (UNIT << 4) | (DENSITY << 7);
		TRWAIT;
		*ptdb = 'I';
	} else
#endif
	if(bp->b_flags&B_READ) {
		rx2tab.d_active = SREAD;
		rx2factr(bp,bp->b_blkno * NSPB + bp->seccnt, &sector, &track);
		*ptcs = READ | GO | INTENB | (UNIT << 4) | (DENSITY << 7);
		TRWAIT;
		*ptdb = sector;
		TRWAIT;
		*ptdb = track;
	} else {
		rx2tab.d_active = SFILL;
		rx2addr(bp, &addr, &xmem);
		nw = (-bp->b_resid) >= NWPS ? NWPS : -bp->b_resid;
                if( rx2wrap(addr,nw))  { /* cpg1 use rx2buf */
                   if(rx2cgp) printf("rx2buf used\n");

		   copymem(nw, xmem, addr, 0, rx2buf); /* into my buffer */
		   *ptcs = FILL | GO | INTENB  | (DENSITY << 7);
		   TRWAIT;
		   *ptdb = nw;
	   	   TRWAIT;
		   *ptdb = rx2buf;
                }
                else  {  /* start dma transfer from addr */
                   if(rx2cgp) printf("copy from addr\n");
                   *ptcs = FILL | GO | INTENB | ( xmem << 12) | (DENSITY <<7);
                   TRWAIT;
                   *ptdb=nw;
                   TRWAIT;
                   *ptdb=addr;
                }

	}
}

rx2intr() {
	register int *ptcs, *ptdb;
	register struct buf *bp;
	int i;				/* for type conversion */
	int sector, track;
	char *addr, *xmem;
	int nw;

	tracef("rx2intr %o\n",rx2tab.d_actf);	/* debug */
	if((bp = rx2tab.d_actf) == 0)
		return;

	ptcs = &RXADDR->rx2cs;
	ptdb = &RXADDR->rx2db;

	if(*ptcs < 0) {
		tracef("rx2error %o\n",*ptdb);	/* debug */
		if(++rx2tab.d_errcnt > RETRY ||
				(*ptdb & (WRITPROT | DENERR))) {
			bp->b_flags =| B_ERROR;
			deverror(bp, *ptdb, *ptcs);
			rx2tab.d_errcnt = 0;
			rx2tab.d_actf = bp->av_forw;
			iodone(bp);
			rx2start();
			return;
		}			/* try it again */
		*ptcs = RXINIT;
		*ptcs = INTENB;
		rx2tab.d_active = SINIT;
		tracef("rx2 INIT\n");	/* debug */
		return;
	}
	switch (rx2tab.d_active) {

	case SREAD:			/* read done, start empty */
		tracef("rx2 EMPTY\n");	/* debug */
		rx2tab.d_active = SEMPTY;
                rx2addr(bp,&addr,&xmem);
		nw = (-bp->b_resid) >= NWPS ? NWPS : -bp->b_resid;
                if(!rx2wrap(addr,nw))  { /* cpg1 dma transfer can be done to addr */
                  if(rx2cgp) printf("dma to addr\n");
                  *ptcs = EMPTY | GO  | INTENB | (DENSITY << 7) | (xmem << 12);
                  TRWAIT;
                  *ptdb=nw;
                  TRWAIT;
                  *ptdb=addr;
                }
                else  {
                  if(rx2cgp) printf(" dma to rx2buf\n");
		  *ptcs = EMPTY | GO | INTENB | (DENSITY << 7);
		  TRWAIT;
		  *ptdb = nw;
		  TRWAIT;
		  *ptdb = rx2buf;
                }

		return;

	case SFILL:			/* fill done, start write */
/*		tracef("rx2 FILL\n");	/* debug */
		rx2tab.d_active = SWRITE;
		rx2factr(bp,bp->b_blkno * NSPB + bp->seccnt, &sector, &track);
		*ptcs = WRITE | GO | INTENB | (UNIT << 4) | (DENSITY << 7);
		TRWAIT;
		*ptdb = sector;
		TRWAIT;
		*ptdb = track;
		return;

	case SEMPTY:			/* empty done, start next read */
		rx2addr(bp, &addr, &xmem);
		nw = (-bp->b_resid) >= NWPS ? NWPS : -bp->b_resid;
                if( rx2wrap(addr,nw)) {
                    /*  cpg1 test if addr wraps around  */
                    if(rx2cgp) printf("copymem on read\n");
		    copymem(nw, 0, rx2buf, xmem, addr);	/* copy to addr */
                 }
	case SWRITE:			/* write done, start next fill */
		/*
		 * increment amount remaining to be transferred.
		 * if it becomes positive, last transfer was a
		 * partial sector and we're done, so set remaining
		 * to zero.
		 */
		if ((i = (bp->b_resid =+ NWPS)) >= 0) {
done:
			bp->b_resid = 0;
			rx2tab.d_errcnt = 0;
			rx2tab.d_actf = bp->av_forw;
			iodone(bp);
			break;
		}

		bp->seccnt++;
		break;

#ifdef CTRL
	case SFORMAT:			/* format done (whew!!!) */
		goto done;		/* driver's getting too big... */
#endif
	}
	/* end up here from states SWRITE, SEMPTY, and SINIT */

	rx2start();
}

/*
 *	rx2factr -- calculates the physical sector and physical
 *	track on the disk for a given logical sector.
 *	call:
 *		rx2factr(logical_sector,&p_sector,&p_track);
 *	the logical sector number (0 - 2001) is converted
 *	to a physical sector number (1 - 26) and a physical
 *	track number (0 - 76).
 *	if using a mapped device (0-3 minor code) then
 *	the logical sectors specify physical sectors that
 *	are interleaved with a factor of 2. thus the sectors
 *	are read in the following order for increasing
 *	logical sector numbers (1,3, ... 23,25,2,4, ... 24,26)
 *	There is also a 6 sector slew between tracks.
 *	Logical sectors start at track 1, sector 1; go to
 *	track 76 and then to track 0.  Thus, for example, unix block number
 *	498 starts at track 0, sector 21 and runs thru track 0, sector 2.
 *	The unmapped devices (4-7) map directly into the floppy sector
 *	and track. e.g. block 0 starts at track 0 sector 1.
 */
rx2factr(bp, sectr, psectr, ptrck)
struct buf *bp;
 int sectr;
int *psectr, *ptrck;
{
	register int p1, p2;

	tracef("rx2factr %d bp 0%o\n",sectr,bp);	/* debug */
	p1 = sectr/26;
	p2 = sectr%26;
	if (MAPPED)		/* map into DEC's RT-11 sectors */
		{
		/* 2 to 1 interleave */
		p2 = (2*p2 + (p2 >= 13 ? 1 : 0)) % 26;
		/* 6 sector per track slew */
		p2 = (p2 + 6*p1) % 26;
		if (++p1 >= 77)
			p1 = 0;
		}
	*psectr = 1 + p2;
	*ptrck = p1;
}


/*
 *	rx2addr -- compute core address where next sector
 *	goes to, based on bp->b_addr, bp->b_xmem,
 *	and bp->seccnt.
 */
rx2addr(bp, addr, xmem)
 struct buf *bp;
 char **addr, **xmem;
{
	*addr = bp->b_addr + bp->seccnt * NBPS;
#ifndef	MX
	*xmem = bp->b_xmem;
	if (*addr < bp->b_addr)			/* overflow, bump xmem */
		(*xmem)++;
#endif
tracef("rx2addr 0%o xm 0%o seccnt %d\n",*addr,*xmem,bp->seccnt); /* debug */
}


#ifndef MX
rx2read(dev)
{
	physio(rx2strategy, &rrx2buf, dev, B_READ, dev.d_minor&2 ? 1001 : 500);
}


rx2write(dev)
{
	physio(rx2strategy, &rrx2buf, dev, B_WRITE, dev.d_minor&2 ? 1001 : 500);
}
#endif


#ifdef CTRL
/*
 *	rx2sgtty -- format RX02 disk, single or double density.
 *	stty with word 0 == 010 does format.  density determined
 *	by device opened.
 */
rx2sgtty(dev, v)
 int *v;
{
	register struct buf *bp;

	if (v) {	/* gtty */
err:
		u.u_error = ENXIO;
		return;
	}
	v = u.u_arg;
	if (*v != 010) goto err;
	bp = &crx2buf;
	spl6();
	while (bp->b_flags & B_BUSY) {
		bp->b_flags =| B_WANTED;
		sleep(bp, PRIBIO);
	}
	bp->b_flags = B_BUSY | B_CTRL;
	bp->b_dev = dev;
	bp->b_error = 0;
	rx2strategy(bp);
	iowait(bp);
	bp->b_flags = 0;
}
#endif
rx2wrap(addr,nw)
 {  /* cpg1 check if the dma will wrap around */
  int a[2];

  a[0]=0;
  a[1]=addr;
  dpadd(a,nw<<1);
  if(rx2cgp)  { printf("rx2wrap a[0]=%d \n",a[0]);
                printf("addr=%d nw*2=%d\n",addr, nw<<1);}
  return(a[0]);
 }
