#
#include "../conf.h"
#include "../param.h"
#include "../proc.h"
#include "../tty.h"
#include "../user.h"

#ifndef NDZ11
#define	NDZ11	16	/* number of lines */
#endif
#define	DZDELAY	HZ/4	/* 4 times a second */
 
/*
 * DZ11 driver.
 * originally written by:	sundaram ramakesavan queen's university
 * modified by:			bill webb, university of b. c.
 *				604-228-6527
 * note that since this version was typed in from a listing of the
 * queen's version all of the original comments have been deleted.
 * only those comments relevent to an experienced unix programmer 
 * have been added.
 * other differences from queen's version:
 * 1	modem control lines are supported
 *	modem bits are checked once a second.
 * 2	parity is set up properly with both even and odd are specified.
 */

char dzmap1[] 		/* map unix (dh) speeds into dz speeds */
{ 0, 0, 1, 2, 3, 4, 4, 5, 6, 7, 8, 10, 12, 14, 15, 13 };

			/* bits in dzlpr */
#define	BITS5	000
#define	BITS6	010
#define	BITS7	020
#define	BITS8	030
#define	PARITY	0100
#define	ODDPAR	0200
#define	RCVRON	010000
#define	STOP2	040

			/* bits in rbuf */
#define	OVERRUN	040000
#define	FRERROR	020000
#define	PERROR	010000
#define	RCVRENA	0100
#define	SCANON	040
#define	SILOON	010000
#define	TRANSON	040000
 
#define	CTLR_MASK	07
#define	UNIT_MASK	07

#define NDZCTLRS	((NDZ11+7)/8)	/* number of controllers */

char *dzaddr[NDZCTLRS];
/* { 0160040, 0160050, 0160060, 0160070 };		/* controller base registers */
 
struct dzregs1 {
int dzcsr;
int dzlpr;
char dztcr;
char dzdtr;
char dztbuf;
char dzbrk;
};
 
struct dzregs2{
int aux1;
int dzrbuf;
int aux2;
char dzring;
char dzcar;
};
 
struct tty dz11[NDZ11];
char dzoldcar[NDZCTLRS];			/* car image */
char dzorcar[NDZCTLRS];			/* or'ed car image */
char dzbrk[NDZCTLRS];			/* or'ed brk image */
char dzflag;				/* if giving timeouts */
int dzmaxctlr -1;			/* max controller in use */
 

dzopen(dev,flag)
int dev, flag;
{
register struct tty *tp;
register char *rp;
register int ctlr;
int j;
extern dzstart();
 
if (dev.d_minor >= NDZ11)
	{
badopen:
	u.u_error = ENXIO;
	return; 
	}
tp = &dz11[dev.d_minor];
rp = dzaddr[(ctlr = dev.d_minor>>3)];	/* proper registers */
if (kdword(rp) == -1)
	goto badopen;			/* oops, no such dz */
if (ctlr > dzmaxctlr)
	dzmaxctlr = ctlr;			/* only look at ones in use */
tp->t_dev=dev;
tp->t_state =| SSTART;
tp->t_addr=dzstart;
if (dzflag == 0)
	{
	++dzflag;
	dztime();
	}
if ((tp->t_state&ISOPEN) == 0)
	{
	tp->t_erase=CERASE;
	tp->t_kill=CKILL;
	dzparam(tp,1);
	tp->t_state =| WOPEN;
	spl5();
	j = 1<<(tp->t_dev.d_minor&UNIT_MASK);
/* 	rp->dzbrk = dzbrk[tp->t_dev.d_minor>>3] =& ~ctlr;   /* turn off brk */
	if ((dzorcar[ctlr]&j) || (rp->dzcar&j))
		tp->t_state =| CARR_ON;
	while ((tp->t_state&CARR_ON) == 0)
		sleep(&tp->t_canq,TTWPRI);	/* sleep for open */
	spl0();
	}
 
ttyopen(tp);
}
 
dzclose(dev)
{
register struct tty *tp;
register char *rp;
 
rp = dzaddr[dev.d_minor>>3];	/* proper registers */
tp = &dz11[dev.d_minor];
ttyclose(tp);
rp->dzdtr =& ~(1<<(tp->t_dev.d_minor&07));	/* turn off dtr */
/* rp->dzbrk = dzbrk[tp->t_dev.d_minor>>3] =| (1<<(tp->t_dev.d_minor&07));	/* turn on brk */
}
 
dzread(dev)
int dev;
{
ttread(&dz11[dev.d_minor]);
}
 
dzwrite(dev)
{
ttwrite(&dz11[dev.d_minor]);
}
 
dzrint(dev)
{
register struct tty *tp;
register int c;
register char *rp;
int ctlr;
 
ctlr = dev&CTLR_MASK;
rp = dzaddr[ctlr];	/* proper registers */
if ((c = rp->dzcar|dzorcar[ctlr]) != dzoldcar[ctlr])
	dzint(ctlr,c);
while ((c = rp->dzrbuf) < 0)
	{
	tp = &dz11[((c>>8)&07)+(ctlr<<3)];
	if ((tp->t_state&ISOPEN)==0 || (c&PERROR))
		continue;
	if(c&FRERROR)
		c = 0;
	if (c&OVERRUN)
		prdev("dz OVR\n",tp->t_dev);
	c =& 0377;
	ttyinput(c,tp);
	}
}
 
dzxint(dev)
{
register struct tty *tp;
register unit;
register char *rp;
int ctlr;

ctlr = dev&CTLR_MASK;
rp = dzaddr[ctlr];	/* proper registers */
unit = (((rp->dzcsr)>>8)&07);
tp = &dz11[unit+(ctlr<<3)];
if (tp->t_state&BUSY)
	{
	rp->dztbuf=tp->t_char.lobyte;
	tp->t_state=& ~BUSY;
	return;
	}
 
rp->dztcr=& ~(1<<unit);
dzstart(tp);
}
 
dzparam(atp,csr)
struct tty *atp;
{
register struct tty *tp;
register int lpr;
register char *rp;
int unit;
 
tp = atp;
rp = dzaddr[tp->t_dev.d_minor>>3];
unit = tp->t_dev.d_minor&UNIT_MASK;
spl5();
if (csr == 1)
	{
	rp->dzcsr=| (SCANON|RCVRENA|TRANSON);
	rp->dzdtr =| (1<<unit);	/* turn on dtr */
	}
lpr=unit;
lpr =| dzmap1[tp->t_speeds.lobyte&017]<<8;
if (tp->t_speeds.lobyte == 3)		/* 110 */
	lpr =| STOP2;
/*
 * if both or no parity set to no parity.
 * if even set to even. if odd set to odd.
 */
switch((tp->t_flags) & (EVENP|ODDP))
	{
case ODDP:
	lpr =| BITS7 | PARITY | ODDPAR;
	break;
case EVENP:
	lpr =| BITS7| PARITY;
	break;
default:
	lpr =| BITS8;
	}
lpr =| RCVRON;
rp->dzlpr = lpr;
ext: spl0();
}

dzstart(atp)
struct tty *atp;
{
register struct tty *tp;
register c;
int sps;
register char *rp;
extern ttrstrt();
 
tp = atp;
rp = dzaddr[tp->t_dev.d_minor>>3];

sps = PS->integ;
spl5();
if(tp->t_state&(TIMEOUT|BUSY|HALTOP))
	goto out;
if(tp->t_outq.c_cc<=TTLOWAT && tp->t_state&ASLEEP)
	{
	tp->t_state =& ~ASLEEP;
	wakeup(&tp->t_outq);
	}
if ((c=getc(&tp->t_outq))>=0)
	{
	if (tp->t_flags != RAW)
		{
		if (c>=0200)
			{
			tp->t_state=|TIMEOUT;
			timeout(ttrstrt,tp,(c&0177)+6);
			goto out;
			}
		}
	tp->t_char = c;
	tp->t_state=| BUSY;
	rp->dztcr =| (1<<(tp->t_dev.d_minor&07));
	}
out: PS->integ = sps;
}
 
#ifndef IOCTL
dzsgtty(dev,av)
int dev; int *av;
{
register struct tty *tp;

tp = &dz11[dev.d_minor];
if (ttystty(tp,av) == 0)
	dzparam(tp,0);
}
#endif

#ifdef IOCTL
dzioctl(dev,cmd,addr,flag)
{
register struct tty *tp;
register char *rp;
register unit;

tp = &dz11[dev.d_minor];
rp = dzaddr[tp->t_dev.d_minor>>3];
unit = tp->t_dev.d_minor&UNIT_MASK;

switch(cmd)
	{
case TIOCSBRK:
	rp->dzbrk = dzbrk[tp->t_dev.d_minor>>3] =| (1<<unit);		/* turn on brk */
	break;
case TIOCCBRK:
	rp->dzbrk = dzbrk[tp->t_dev.d_minor>>3] =& ~(1<<unit);	/* turn off brk */
	break;
default:
	if (ttyioctl(cmd,tp,addr,dev))
		if (cmd == TIOCSETP)
			dzparam(tp,0);
	break;
	}
}
#endif

dzint(ctlr,c) char c;
{
/*
 * simulated change of carrier state interrupt.
 * controller "ctlr" changed to "c"
 * scan thru the lines to find out which one it was
 * then if OFF->ON line set CARR_ON
 * otherwise clear CARR_ON and send hangup signal.
 */
register struct tty *tp;
register int k;

k = 1;
for (tp = &dz11[ctlr<<3]; tp < &dz11[NDZ11]; ++tp)
	{
	if (k&0400)
		break;
	if (k & (dzoldcar[ctlr]^c))		/* line change */
		{
		wakeup(&tp->t_canq);		/* wake top level */
		if (k&c)		/* now on */
			tp->t_state =| CARR_ON;
		else
			{
			tp->t_state =& ~CARR_ON;
			signal(tp,SIGHUP);
			flushtty(tp);
			}
		}
	k =<< 1;
	}
dzoldcar[ctlr] = c;		/* remember it */
}

dztime()
{
/*
 * DZ11 timer interrupt routine.
 * every HZ tics (currently 1 second's worth) check to see if there
 * were any changes in the modem CAR bits. If there were call
 * dzint (modem bits change interrupt) to handle it.
 *
 * also scan thru the tty's and check for a timer delay being on.
 * this delay is set every time a character is received, and reset
 * to zero on a delimeter.
 * it is decremented by DZDELAY every DZDELAY tics, and when it goes to
 * <= zero a delimeter is put on the input Q if the terminal is in 
 * RARE mode.
 */
register int c;
register char *rp;
register int i;

for (i=0; i <= dzmaxctlr; ++i)
	{
	rp = dzaddr[i];
	if ((c = rp->dzcar|dzorcar[i]) != dzoldcar[i])
		dzint(i,c);
	}
#ifdef INTIMER
for (i=0; i<NDZ11; ++i)
	{
	rp = &dz11[i];
	if (rp != NULL && rp->t_intimer && (rp->t_intimer =- DZDELAY) <= 0)
		{
		rp->t_intimer = 0;
		if ((rp->t_state&ISOPEN) && (rp->t_flags&RARE))
			ttyinput(SENDBRK,rp);
		}
	}
#endif
timeout(&dztime,0,DZDELAY);
}
