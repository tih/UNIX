#
/*
 * MSCP disk driver for 6th Edition UNIX.
 *
 * This is based on one by Rick Macklem for 2.9BSD on the DEC Pro 350.
 *
 * Restrictions:
 *   Only one controller supported for now.
 */
#include "../param.h"
#include "../systm.h"
#include "../buf.h"
#include "../conf.h"
#include "../user.h"

/* Setting these to 2 is adequate for the RQDX1 but should be
 * increased to 4 or 5 for the UDA50. This is because the credit limit
 * is 4 for the RQDX1 and about 22 for the current UDA50 rev.  Done
 * this way to force 2^N counts.  Testing with an RQDX3 revealed that
 * it had a credit limit of 12.  Note: changing the number of packets
 * is only for performance enhancement, and grows the size of the
 * kernel.  I'm leaving the constants at 3 for the 6th edition
 * version, which is OK for my 11/23 with RQDX3, but will go back down
 * to 2 if I decide I need to free up the space.
 */
#define	NRSPL2	3		/* log2 number of response packets */
#define	NCMDL2	3		/* log2 number of command packets */
#define	NRSP	(1<<NRSPL2)
#define	NCMD	(1<<NCMDL2)

/* This is the old standard setup for MicroPDP-11 with RQDXn */
/* The RX50 is 800 blocks, RD51: 21600, RD52: 60480, RD53: 138672 */

struct	rasizes	{
	long nblocks;
	long blkoff;
} ra_sizes[8] = {	/* Setup for RX50, RD51, RD52 and RD53 disks */
	9700,	0,	/* A=blk 0 thru 9699 (root for 52 & 53) */
	17300,	9700,	/* B=blk 9700 thru 26999 (52 & 53 only) */
	3100,	27000,	/* C=blk 27000 thru 30099 (swap 52 & 53) */
	-1,	30100,	/* D=blk 30100 thru end (52 & 53 only) */
	7460,	0,	/* E=blk 0 thru 7459 (root 51) */
	2240,	7460,	/* F=blk 7460 thru 9699 (swap 51) */
	-1,	9700,	/* G=blk 9700 thru end (51, 52 & 53) */
	-1,	0,	/* H=blk 0 thru end (51, 52, 53 & RX50) */
};

#include "ra.h"

/* For now, limit to one MSCP controller, four disks */
#define NRA 4
struct  radevice *RAADDR = 0172150;

struct ra_softc {
	int	sc_state;	/* state of controller */
	int	sc_ivec;	/* interrupt vector address */
	int	sc_credits;	/* transfer credits */
	int	sc_lastcmd;	/* pointer into command ring */
	int	sc_lastrsp;	/* pointer into response ring */
	int	sc_onlin;	/* flags for drives online */
} ra_softc;

/*
 * Controller states
 */
#define	S_IDLE	0		/* hasn't been initialized */
#define	S_STEP1	1		/* doing step 1 init */
#define	S_STEP2	2		/* doing step 2 init */
#define	S_STEP3	3		/* doing step 3 init */
#define	S_SCHAR	4		/* doing "set controller characteristics" */
#define	S_RUN	5		/* running */

struct ra {
	struct raca	ra_ca;		/* communications area */
	struct ms	ra_rsp[NRSP];	/* response packets */
	struct ms	ra_cmd[NCMD];	/* command packets */
} ra;

int	raerr = 0;			/* causes hex dump of packets */

long	radsize[NRA];			/* disk size, from ONLINE end packet */
struct	ms *ragetcp();

struct	buf rrabuf;
struct	devtab ratab;
struct	buf rautab[NRA];
struct	buf rawtab;			/* I/O wait queue, per controller */

#define dkblock(bp)     ((bp)->b_blkno)
#define dkunit(bp)      ((bp)->b_dev.d_minor >> 3)
#define dkpart(bp)      ((bp)->b_dev.d_minor & 7)

#define bcount(wc) (-(wc) << 1) /* in v6, b_wcount is negative */

/* This is needed because the kernel lacks support for the long operations, */
/* and the compiler used lacks support for the maneuvers needed in a macro. */
/* Could probably be fixed by expanding the assembler support in e.g. m23.s */

SWAPW(lp)
long *lp;
{
	unsigned i;
	union {
		unsigned u[2];
		long l;
	} un;
	un.l = *lp;
	i = un.u[0];
	un.u[0] = un.u[1];
	un.u[1] = i;
	*lp = un.l;
}

int wakeup();

/*
 * Open an RA.  Initialize the device and
 * set the unit online.
 */
raopen(dev)
	int dev;
{
	register int unit;
	register struct ra_softc *sc;

	unit = dev.d_minor >> 3;
	if (unit >= NRA || RAADDR == NULL) {
		u.u_error = ENXIO;
		return;
	}
	sc = &ra_softc;
	spl5();
	/* If the controller is not running, start it */
	if (sc->sc_state != S_RUN) {
		if (sc->sc_state == S_IDLE)
			rainit();
		sleep(sc, PSWP+1);
		if (sc->sc_state != S_RUN) {
			u.u_error = EIO;
			goto out;
		}
	}
	/* If the unit is not ONLINE, do it */
	if ((sc->sc_onlin & (1<<unit)) == 0) {
		int i;
		struct devtab *dp;
		struct ms *mp;
		dp = &rautab[unit];
		if ((sc->sc_credits < 2) || ((mp = ragetcp()) == NULL)) {
			u.u_error = EIO;
			goto out;
		}
		sc->sc_credits--;
		mp->ms_opcode = M_OP_ONLIN;
		mp->ms_unit = unit;
		dp->d_active = 2;
		ratab.d_active++;
		mp->ms_dscptr->high =| RA_OWN|RA_INT;
		i = RAADDR->raip;
		sleep(sc, PSWP+1);
		if ((sc->sc_onlin & (1<<unit)) == 0||sc->sc_state != S_RUN) {
			u.u_error = EIO;
			goto out;
		}
	}
out:
	spl0();
	return;
}

/*
 * Initialize an RA.  Set up UB mapping registers,
 * initialize data structures, and start hardware
 * initialization sequence.
 */
rainit()
{
	register struct ra_softc *sc;
	register struct ra *rap;

	sc = &ra_softc;
	sc->sc_ivec = RA_IVEC;
	ratab.d_active++;
	rap = &ra;

	/*
	 * Start the hardware initialization sequence.
	 */
	RAADDR->raip = 0;		/* start initialization */
	while ((RAADDR->rasa & RA_STEP1) == 0)
		;
	RAADDR->rasa = RA_ERR|(NCMDL2<<11)|(NRSPL2<<8)|RA_IE|(sc->sc_ivec/4);
	/*
	 * Initialization continues in interrupt routine.
	 */
	sc->sc_state = S_STEP1;
	sc->sc_credits = 0;
}

rastrategy(bp)
	register struct buf *bp;
{
	register struct devtab *dp;
	register int unit;
	int part = dkpart(bp);
	long sz, maxsz;

	sz = (bcount(bp->b_wcount)+511) >> 9;
	unit = dkunit(bp);
	if (unit >= NRA)
		goto bad;
	if ((maxsz = ra_sizes[part].nblocks) < 0)
		maxsz = radsize[unit] - ra_sizes[part].blkoff;
	if (dkblock(bp) < 0 || dkblock(bp)+sz > maxsz)
		goto bad;
	dp = &rautab[unit];
	spl5();
	/*
	 * Link the buffer onto the drive queue
	 */
	if (dp->d_actf == 0)
		dp->d_actf = bp;
	else
		dp->d_actl->av_forw = bp;
	dp->d_actl = bp;
	bp->av_forw = 0;
	/*
	 * Link the drive onto the controller queue
	 */
	if (dp->d_active == 0) {
		dp->b_forw = NULL;
		if (ratab.d_actf == NULL)
			ratab.d_actf = dp;
		else
			ratab.d_actl->b_forw = dp;
		ratab.d_actl = dp;
		dp->d_active = 1;
	}
	if (ratab.d_active == 0) {
		rastart();
	}
	spl0();
	return;

bad:
	bp->b_error = EINVAL;
	bp->b_flags =| B_ERROR;
	iodone(bp);
	return;
}

rastart()
{
	register struct buf *bp;
	register struct devtab *dp;
	register struct ms *mp;
	register struct ra_softc *sc;
	int i;

	sc = &ra_softc;

loop:
	if ((dp = ratab.d_actf) == NULL) {
		ratab.d_active = 0;
		return (0);
	}
	if ((bp = dp->d_actf) == NULL) {
		/*
		 * No more requests for this drive, remove
		 * from controller queue and look at next drive.
		 * We know we're at the head of the controller queue.
		 */
		dp->d_active = 0;
		ratab.d_actf = dp->b_forw;
		goto loop;
	}
	ratab.d_active++;
	if ((RAADDR->rasa&RA_ERR) || sc->sc_state != S_RUN) {
		deverror(bp, RAADDR);
		printf("rasa %o stat %d\n", RAADDR->rasa, sc->sc_state);
		rainit();
		/* SHOULD REQUEUE OUTSTANDING REQUESTS */
		return (0);
	}
	/*
	 * If no credits, can't issue any commands
	 * until some outstanding commands complete.
	 */
	if (sc->sc_credits < 2)
		return (0);
	if ((mp = ragetcp()) == NULL)
		return (0);
	sc->sc_credits--;	/* committed to issuing a command */
	/* If drive not ONLINE, issue an online com. */
	if ((sc->sc_onlin & (1<<dkunit(bp))) == 0) {
		mp->ms_opcode = M_OP_ONLIN;
		mp->ms_unit = dkunit(bp);
		dp->d_active = 2;
		ratab.d_actf = dp->b_forw;	/* remove from controller q */
		mp->ms_dscptr->high =| RA_OWN|RA_INT;
		i = RAADDR->raip;
		goto loop;
	}
#ifdef B_PHYS
	if(bp->b_flags&B_PHYS)
		mapalloc(bp);
#endif
	mp->ms_cmdref.low = bp;	/* pointer to get back */
	mp->ms_opcode = bp->b_flags&B_READ ? M_OP_READ : M_OP_WRITE;
	mp->ms_unit = dkunit(bp);
	mp->ms_lbn = dkblock(bp)+ra_sizes[bp->b_dev.d_minor&7].blkoff;
	SWAPW(&mp->ms_lbn);
	mp->ms_bytecnt = bcount(bp->b_wcount);
	SWAPW(&mp->ms_bytecnt);
	mp->ms_buffer.high = bp->b_xmem;
	mp->ms_buffer.low = bp->b_addr;
	mp->ms_dscptr->high =| RA_OWN|RA_INT;
	i = RAADDR->raip;		/* initiate polling */
	/*
	 * Move drive to the end of the controller queue
	 */
	if (dp->b_forw != NULL) {
		ratab.d_actf = dp->b_forw;
		ratab.d_actl->b_forw = dp;
		ratab.d_actl = dp;
		dp->b_forw = NULL;
	}
	/*
	 * Move buffer to I/O wait queue
	 */
	dp->d_actf = bp->av_forw;
	dp = &rawtab;
	bp->av_forw = dp;
	bp->av_back = dp->av_back;
	dp->av_back->av_forw = bp;
	dp->b_back = bp;
	goto loop;
}

/*
 * RA interrupt routine.
 */
raintr()
{
	struct buf *bp;
	register int i;
	register struct ra_softc *sc = &ra_softc;
	register struct ra *rap = &ra;
	struct ms *mp;

	switch (sc->sc_state) {
	case S_IDLE:
		printf("ra: randm intr\n");
		return;

	case S_STEP1:
#define	STEP1MASK	0174377
#define	STEP1GOOD	(RA_STEP2|RA_IE|(NCMDL2<<3)|NRSPL2)
		if ((RAADDR->rasa&STEP1MASK) != STEP1GOOD) {
			sc->sc_state = S_IDLE;
			wakeup(sc);
			return;
		}
		RAADDR->rasa = &ra.ra_ca.ca_rspdsc[0];
		sc->sc_state = S_STEP2;
		return;

	case S_STEP2:
#define	STEP2MASK	0174377
#define	STEP2GOOD	(RA_STEP3|RA_IE|(sc->sc_ivec/4))
		if ((RAADDR->rasa&STEP2MASK) != STEP2GOOD) {
			sc->sc_state = S_IDLE;
			wakeup(sc);
			return;
		}
		RAADDR->rasa = 0;
		sc->sc_state = S_STEP3;
		return;

	case S_STEP3:
#define	STEP3MASK	0174000
#define	STEP3GOOD	RA_STEP4
		if ((RAADDR->rasa&STEP3MASK) != STEP3GOOD) {
			sc->sc_state = S_IDLE;
			wakeup(sc);
			return;
		}
		RAADDR->rasa = RA_GO;
		sc->sc_state = S_SCHAR;

		/*
		 * Initialize the data structures.
		 */
		for (i = 0; i < NRSP; i++) {
			rap->ra_ca.ca_rspdsc[i].low = &rap->ra_rsp[i].ms_cmdref;
			rap->ra_ca.ca_rspdsc[i].high = RA_OWN|RA_INT;
			rap->ra_rsp[i].ms_dscptr = &rap->ra_ca.ca_rspdsc[i];
			rap->ra_rsp[i].ms_header.ra_msglen = sizeof(*mp);
		}
		for (i = 0; i < NCMD; i++) {
			rap->ra_ca.ca_cmddsc[i].low = &rap->ra_cmd[i].ms_cmdref;
			rap->ra_ca.ca_cmddsc[i].high = RA_INT;
			rap->ra_cmd[i].ms_dscptr = &rap->ra_ca.ca_cmddsc[i];
			rap->ra_cmd[i].ms_header.ra_msglen = sizeof(*mp);
		}
		bp = &rawtab;
		bp->av_forw = bp->av_back = bp;
		sc->sc_lastcmd = 0;
		sc->sc_lastrsp = 0;
		if ((mp = ragetcp()) == NULL) {
			sc->sc_state = S_IDLE;
			wakeup(sc);
			return;
		}
		mp->ms_opcode = M_OP_STCON;
		mp->ms_cntflgs = M_CF_ATTN|M_CF_MISC|M_CF_THIS;
		mp->ms_dscptr->high =| RA_OWN|RA_INT;
		i = RAADDR->raip;	/* initiate polling */
		return;

	case S_SCHAR:
	case S_RUN:
		break;

	default:
		printf("ra: intr in stat %d\n",
			sc->sc_state);
		return;
	}

	if (RAADDR->rasa&RA_ERR) {
		printf("ra: fatal err (%o)\n", RAADDR->rasa);
		RAADDR->raip = 0;
		wakeup(sc);
	}

	/*
	 * Check for response ring transition.
	 */
	if (rap->ra_ca.ca_rspint) {
		rap->ra_ca.ca_rspint = 0;
		for (i = sc->sc_lastrsp;; i++) {
			i =% NRSP;
			if (rap->ra_ca.ca_rspdsc[i].high&RA_OWN)
				break;
			rarsp(rap, sc, i);
			rap->ra_ca.ca_rspdsc[i].high =| RA_OWN;
		}
		sc->sc_lastrsp = i;
	}

	/*
	 * Check for command ring transition.
	 */
	if (rap->ra_ca.ca_cmdint) {
		rap->ra_ca.ca_cmdint = 0;
	}
	rastart();
}

/*
 * Process a response packet
 */
rarsp(ra, sc, i)
	register struct ra *ra;
	register struct ra_softc *sc;
	int i;
{
	register struct ms *mp;
	struct buf *bp;
	struct devtab *dp;
	int st;

	mp = &ra->ra_rsp[i];
	mp->ms_header.ra_msglen = sizeof(*mp);
	sc->sc_credits =+ mp->ms_header.ra_credits & 0xf;
	if ((mp->ms_header.ra_credits & 0xf0) > 0x10)
		return;
	/*
	 * If it's an error log message (datagram),
	 * pass it on for more extensive processing.
	 */
	if ((mp->ms_header.ra_credits & 0xf0) == 0x10) {
		raerror(mp);
		return;
	}
	if (mp->ms_unit >= NRA)
		return;
	st = mp->ms_status&M_ST_MASK;
	switch ((mp->ms_opcode)&0377) {
	case M_OP_STCON|M_OP_END:
		if (st == M_ST_SUCC)
			sc->sc_state = S_RUN;
		else
			sc->sc_state = S_IDLE;
		ratab.d_active = 0;
		wakeup(sc);
		break;

	case M_OP_ONLIN|M_OP_END:
		/*
		 * Link the drive onto the controller queue
		 */
		dp = &rautab[mp->ms_unit];
		dp->b_forw = NULL;
		if (ratab.d_actf == NULL)
			ratab.d_actf = dp;
		else
			ratab.d_actl->b_forw = dp;
		ratab.d_actl = dp;
		if (st == M_ST_SUCC) {
			sc->sc_onlin =| (1<<mp->ms_unit);
			radsize[mp->ms_unit] = ((mp->ms_unt2)<<16)|((mp->ms_unt1)&0xffff);
		} else {
			deverror(dp->d_actf, RAADDR);
			printf("OFFLINE\n");
			while (bp = dp->d_actf) {
				dp->d_actf = bp->av_forw;
				bp->b_flags =| B_ERROR;
				iodone(bp);
			}
		}
		dp->d_active = 1;
		wakeup(sc);
		break;

	case M_OP_AVATN:
		sc->sc_onlin =& ~(1<<mp->ms_unit);
		break;

	case M_OP_READ|M_OP_END:
	case M_OP_WRITE|M_OP_END:
		bp = mp->ms_cmdref.low;
		/*
		 * Unlink buffer from I/O wait queue.
		 */
		bp->av_back->av_forw = bp->av_forw;
		bp->av_forw->av_back = bp->av_back;
		dp = &rautab[mp->ms_unit];
		if (st == M_ST_OFFLN || st == M_ST_AVLBL) {
			sc->sc_onlin =& ~(1<<mp->ms_unit);
			/*
			 * Link the buffer onto the front of the drive queue
			 */
			if ((bp->av_forw = dp->d_actf) == 0)
				dp->d_actl = bp;
			dp->d_actf = bp;
			/*
			 * Link the drive onto the controller queue
			 */
			if (dp->d_active == 0) {
				dp->b_forw = NULL;
				if (ratab.d_actf == NULL)
					ratab.d_actf = dp;
				else
					ratab.d_actl->b_forw = dp;
				ratab.d_actl = dp;
				dp->d_active = 1;
			}
			return;
		}
		if (st != M_ST_SUCC) {
			deverror(bp, RAADDR);
			bp->b_flags =| B_ERROR;
		}
		{
			long t = mp->ms_bytecnt;
			SWAPW(&t);
			bp->b_resid = bcount(bp->b_wcount) - t;
		}
		iodone(bp);
		break;

	case M_OP_GTUNT|M_OP_END:
		break;

	default:
		printf("ra: unknown pckt\n");
	}
}


/*
 * Process an error log message
 *
 * For now, just log the error on the console.
 * Only minimal decoding is done, only "useful"
 * information is printed.  Eventually should
 * send message to an error logger.
 */
raerror(mp)
	register struct ml *mp;
{
	printf("ra%d: %s err, ", mp->ml_unit,
		mp->ml_flags&(M_LF_SUCC|M_LF_CONT) ? "soft" : "hard");
	switch (mp->ml_format&0377) {
	case M_FM_CNTERR:
		printf("cntrl err ev 0%o\n", mp->ml_event);
		break;

	case M_FM_BUSADDR:
		printf("mem err ev 0%o, addr 0%O\n",
			mp->ml_event, mp->ml_busaddr);
		break;

	case M_FM_DISKTRN:
		printf("dsk xfer err unit %d, grp 0x%x, hdr 0x%X\n",
			mp->ml_unit, mp->ml_group, mp->ml_hdr);
		break;

	case M_FM_SDI:
		printf("SDI err, unit %d, ev 0%o, hdr 0x%X\n",
			mp->ml_unit, mp->ml_event, mp->ml_hdr);
		break;

	case M_FM_SMLDSK:
		printf("sml dsk err unit %d, ev 0%o, cyl %d\n",
			mp->ml_unit, mp->ml_event, mp->ml_sdecyl);
		break;

	default:
		printf("unknown err, unit %d, fmt 0%o, ev 0%o\n",
			mp->ml_unit, mp->ml_format, mp->ml_event);
	}

	if (raerr) {
		register int *p = mp;
		register int i;

		for (i = 0; i < mp->ml_header.ra_msglen; i =+ sizeof(*p))
			printf("%x ", *p++);
		printf("\n");
	}
}

/*
 * Find an unused command packet
 */
struct ms *
ragetcp()
{
	register struct ms *mp;
	register struct raca *cp;
	register struct ra_softc *sc;
	register int i;

	cp = &ra.ra_ca;
	sc = &ra_softc;
	i = sc->sc_lastcmd;
	if ((cp->ca_cmddsc[i].high & (RA_OWN|RA_INT)) == RA_INT) {
		cp->ca_cmddsc[i].high =& ~RA_INT;
		mp = &ra.ra_cmd[i];
		mp->ms_unit = mp->ms_modifier = 0;
		mp->ms_opcode = mp->ms_flags = 0;
		mp->ms_buffer.high = mp->ms_buffer.low = 0;
		mp->ms_bytecnt = 0;
		mp->ms_errlgfl = mp->ms_copyspd = 0;
		sc->sc_lastcmd = (i + 1) % NCMD;
		return(mp);
	}
	return(NULL);
}

raread(dev)
	unsigned dev;
{
	physio(rastrategy, &rrabuf, dev, B_READ);
}

rawrite(dev)
	unsigned dev;
{
	physio(rastrategy, &rrabuf, dev, B_WRITE);
}
