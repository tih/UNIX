#
/*
 * TM tape driver
 */

#include "../param.h"
#include "../buf.h"
#include "../conf.h"
#include "../user.h"

#define	LOOPCOUNT	1000		/* time limit for gap shutdown */

struct {
	int tmer;
	int tmcs;
	int tmbc;
	int tmba;
	int tmdb;
	int tmrd;
};

struct	devtab	tmtab;
struct	buf	rtmbuf,posbuf;

char	t_flag[8];
char	t_raw[8];
char	t_openf[8];
char	*t_blkno[8];
char	*t_nxrec[8];
int	t_error[8];
int	t_errcnt[8];	/* count of number of errors */
int	t_resid[8];

#define	TMADDR	0172520

#define	GO	01
#define	RCOM	02
#define	WCOM	04
#define	WEOF	06
#define	SFORW	010
#define	SREV	012
#define	WIRG	014
#define	REW	016
#define	DENS	060000		/* 9-channel */
#define	IENABLE	0100
#define	CRDY	0200
#define GAPSD	010000
#define	TUR	1
#define RWS	2
#define WRING	4
#define SELR 	0100
#define ERRBITS	0177600
#define	HARD	0102600	/* ILC, BTE, EOT, NXM */
#define	EOF	0040000
#define BOT	040
#define EOT	02000
#define	OFFL	0

#define RAW	1
#define STTY	4
#define DREW	0200	/* disable rewinds sign bit of t_raw */

#define	SSEEK	1
#define	SIO	2
#define SEOT	3
#define SBEOT	4
#define SPOS	5
#define SBCEOT	6

#define EOTMARK 2	/* # of EOF's for EOT mark */
tmopen(dev, flag)
{
	register dminor;

	dminor = dev.d_minor & 07;
	if (t_openf[dminor])
		u.u_error = ENXIO;	/* already open */
	else {
		t_raw[dminor] = 0;
		t_flag[dminor] = 0;
		t_openf[dminor]++;
		t_blkno[dminor] = 0;
		t_nxrec[dminor] = 65535;
		t_error[dminor] = 0;
		t_errcnt[dminor] = 0;
		t_resid[dminor] = 0;
	}
	if (dev.d_minor & ~07) t_raw[dminor] =| DREW;
}

tmclose(dev, flag)
{
	register int dminor;
	int cnt,pos,save;

	dminor = dev.d_minor & 07;
	save = t_error[dminor];
	cnt = 0;
	if(t_openf[dminor]) {
		pos = t_flag[dminor];
		for (cnt = 0;cnt < pos;++cnt)
			tcontrol(dev,0,1);	/* write final EOF's */
		if(!t_raw[dminor]) {
			if(save & EOF) ++cnt;
			tcontrol(dev,2,++cnt);	/* if cooked, back to BOF */
			if(!(t_error[dminor]&BOT))
				tcontrol(dev,3,1);  /* and forward into file*/
		} else if(t_raw[dminor] > 0)
			tcontrol(dev,5,1);	/* if raw, rewind unless disabled */
	}
	t_openf[dminor] = 0;
}

tmstrategy(abp)
struct buf *abp;
{
	register struct buf *bp;
	register char **p;

	bp = abp;
	spl5();
	p = &t_nxrec[bp->b_dev.d_minor & 07];
	if (*p <= bp->b_blkno) {
		if (*p < bp->b_blkno) {
			bp->b_flags =| B_ERROR;
			iodone(bp);
			spl0();
			return;
		}
		if (bp->b_flags&B_READ) {
			clrbuf(bp);
			iodone(bp);
			spl0();
			return;
		}
	}
	if ((bp->b_flags&B_READ)==0)
		*p = bp->b_blkno + 1;
	tmsetpt(bp);
	spl0();
}

tmsetpt(bp)   struct buf *bp;
{
	bp->av_forw = 0;
	if (tmtab.d_actf==0)
		tmtab.d_actf = bp;
	else
		tmtab.d_actl->av_forw = bp;
	tmtab.d_actl = bp;
	if (tmtab.d_active==0)
		tmstart();
}

tmstart()
{
/*
 * start tape operation.
 * entered from interrupt service and from the top level
 * routines.
 */
	register struct buf *bp;
	register int com;
	int unit;
	register char *blkno;

    loop:
	if ((bp = tmtab.d_actf) == 0)
		return;
	unit = bp->b_dev.d_minor & 07;
	blkno = t_blkno[unit];
	TMADDR->tmcs = (unit<<8);
	if (	(t_error[unit]&EOF && t_raw[unit] &&  !t_raw[unit]&STTY)
	 || 	((TMADDR->tmcs & CRDY)==0 || (TMADDR->tmer&SELR)==0)
	 ||	(!(bp->b_flags&B_READ) && (TMADDR->tmer&WRING)))
	{
		bp->b_flags =| B_ERROR;
		tmtab.d_actf = bp->av_forw;
		tmtab.d_active = 0;
		iodone(bp);
		goto loop;
	}
	com =(unit<<8) | ((bp->b_xmem & 03) << 4) |IENABLE|DENS;
	if((t_error[unit] & EOF) && bp != &rtmbuf && bp != &posbuf)
	    if (tmtab.d_active != SBCEOT) {
		tmtab.d_active = SBCEOT;	/* back up cooked tape into file */
		com =| SREV|GO;
		TMADDR->tmbc = -1;
		TMADDR->tmcs = com;
		return;
	}
	if (t_flag[unit] &&( blkno != bp->b_blkno || bp->b_flags&B_READ) )
	    if (tmtab.d_active != SBEOT ) {
		tmtab.d_active = SEOT;
		com =| WEOF|GO;
		TMADDR->tmcs = com;
		return;
	}
	if (tmtab.d_active == SEOT) t_flag[unit] = EOTMARK;
	if(t_flag[unit] && (tmtab.d_active==SEOT || tmtab.d_active==SBEOT)) {
		tmtab.d_active = SBEOT;
		TMADDR->tmbc = -100;
		com =| SREV|GO;
		TMADDR->tmcs = com;
		return;
	}
	if(bp == &posbuf) {
		tmtab.d_active = SPOS;
		TMADDR->tmbc = bp->b_wcount;
		com =| bp->b_addr|GO;
		TMADDR->tmcs = com;
		return;
	}
	if (blkno != bp->b_blkno) {
		tmtab.d_active = SSEEK;
		if (blkno < bp->b_blkno) {
			com =| SFORW|GO;
			TMADDR->tmbc = blkno - bp->b_blkno;
		} else {
			com =| SREV|GO;
			TMADDR->tmbc = bp->b_blkno - blkno;
		}
		TMADDR->tmcs = com;
		return;
	}
	tmtab.d_active = SIO;
	TMADDR->tmbc = bp->b_wcount << 1;
	TMADDR->tmba = bp->b_addr;
	TMADDR->tmcs = com | ((bp->b_flags&B_READ)?	RCOM|GO:
			     ((tmtab.d_errcnt)?		WIRG|GO:
							WCOM|GO));
	t_flag[unit] = (bp->b_flags & B_READ ? 0: EOTMARK);
}

tmintr()
{
	register struct buf *bp;
	register int unit;
	register int n;

	if ((bp = tmtab.d_actf)==0)
		return;
	n = LOOPCOUNT;
	unit = bp->b_dev.d_minor & 07;
	t_error[unit] = TMADDR->tmer;
	t_resid[unit] = TMADDR->tmbc;
	if (TMADDR->tmcs < 0 && (TMADDR->tmer&ERRBITS) != EOF) {
		while(--n && TMADDR->tmrd & GAPSD) 
			; /* wait a while for gap shutdown */
		if ((TMADDR->tmer&(HARD|EOF))==0 && tmtab.d_active==SIO) {
			++t_errcnt[unit];	/* count errors */
			if (++tmtab.d_errcnt < 10) {
				t_blkno[unit]++;
				tmtab.d_active = 0;
				tmstart();
				return;
			}
		}
		bp->b_flags =| B_ERROR;
		goto tmnext;
	}

tmok:
	if (tmtab.d_active == SIO ) {
		t_blkno[unit]++;
		goto tmnext;
	} else if (tmtab.d_active == SSEEK) {
		t_blkno[unit] = bp->b_blkno + TMADDR->tmbc;
	} else if (tmtab.d_active == SEOT || tmtab.d_active == SBEOT) {
		t_flag[unit]--;
	} else if(tmtab.d_active == SPOS) {
tmnext:
		tmtab.d_errcnt = 0;
		tmtab.d_actf = bp->av_forw;
		tmtab.d_active = 0;
		bp->b_resid = ((TMADDR->tmer&EOF)?bp->b_wcount<<1:TMADDR->tmbc);
		iodone(bp);
	}
	tmstart();
}

tmread(dev)
{
	tmphys(dev);
	physio(tmstrategy, &rtmbuf, dev, B_READ);
	u.u_count = -rtmbuf.b_resid;
	setpri();		/* back to user priority level */
}

tmwrite(dev)
{
	tmphys(dev);
	physio(tmstrategy, &rtmbuf, dev, B_WRITE);
	u.u_count = 0;
}

tmphys(dev)
{
	register unit, a;

	unit = dev.d_minor & 07;
	t_raw[unit] =| RAW;		/* remember raw unit */
	a = lshift(u.u_offset, -9);
	t_blkno[unit] = a;
	t_nxrec[unit] = ++a;
}

tmstty(dev,arg) int *arg;
	{
	register int dminor;
	dminor = dev.d_minor & 07;
	if(arg != 0) {
		arg[0] = t_error[dminor];
		arg[1] = t_resid[dminor];
		arg[2] = t_errcnt[dminor];
		return;
	}
	arg = u.u_arg;
	if(arg[0] == -1) {
		t_raw[dminor] =| DREW; /* set sign bit to disable rew */
		return;
	}
	tcontrol(dev,arg[0],arg[1]);
}

#ifdef IOCTL
tmioctl(dev,cmd,addr,flag)
{
iostty(dev,cmd,addr,&tmstty);
}
#endif

/* tape control routine */
tcontrol(dev,arg0,arg1) int arg0,arg1;
{
/*
 * do a tape control function.
 * dev is the device code
 * arg0 is the function code
 * arg1 is the optional count
 */
	int dminor;
	register struct buf *bp;
	int comcnt,poscom,poscnt;

	bp = &posbuf;
	dminor = dev.d_minor & 07;
	comcnt = 1;
	poscnt = -1;
	switch( arg0 ) {
	case 0:  /*  write EOF */
		comcnt = arg1;
		poscom = WEOF;
		t_flag[dminor] = 0;
		break;
	case 1:  /* file space forward */
		comcnt = arg1;
		poscom = SFORW;
		poscnt = -65535;
		break;
	case 2: /* file space back */
		comcnt = arg1;
		poscom = SREV;
		poscnt = -65535;
		break;
	case 3: /* record space forward */
		poscom = SFORW;
		poscnt = -arg1;
		break;
	case 4:   /* block space back */
		poscom = SREV;
		poscnt = -arg1;
		break;
	case 5:  /* rewind the whole tape */
		poscom = REW;
		break;
	case 6:  /* off line the unit */
		poscom = OFFL;
		break;
	default: return;
	}
	while(--comcnt >= 0) {
		spl5();
		while (bp->b_flags & B_BUSY) {
			bp->b_flags =| B_WANTED;
			sleep(bp, -1);
		}
		bp->b_flags = B_BUSY | B_READ;
		bp->b_dev = dev;
		bp->b_addr = poscom;
		bp->b_wcount = poscnt;
		bp->b_error = 0;
		tmsetpt(bp);
		while ((bp->b_flags & B_DONE) == 0)
			sleep(bp, -1);
		bp->b_flags =& ~(B_BUSY | B_WANTED);
		wakeup(bp);
		if (bp->b_flags & B_ERROR) break;
		spl0();
	}
	spl0();
	geterror(bp);
	t_raw[dminor] =& ~STTY;
}
