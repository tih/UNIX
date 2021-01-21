#
/*
 *	Copyright 1973 Bell Telephone Laboratories Inc
 */

/*
 *   KL/DL-11 driver
 */
#include "../param.h"
#include "../conf.h"
#include "../user.h"
#include "../tty.h"
#include "../proc.h"

#ifndef NKL11
#define	NKL11	1
#define	NDL11	0
#endif

/* base address */
#define	KLADDR	0177560	/* console */
#define	KLBASE	0176500	/* kl and dl11-a */
#define	DLBASE	0175610	/* dl-e */
#define DSRDY	02
#define	RDRENB	01
#define	TBRK	01		/* send break */
#define	DSCHG	0100000
#define	CARDET	010000
#define	DIENABLE	040
#define	KLWPRI	040

struct	tty kl11[NKL11+NDL11];

struct klregs {
	int klrcsr;
	int klrbuf;
	int kltcsr;
	int kltbuf;
}

klopen(dev, flag)
{
	register char *addr;
	register struct tty *tp;
	register d;

	if((d = dev.d_minor) >= NKL11+NDL11) {
		u.u_error = ENXIO;
		return;
	}
	tp = &kl11[d];
	addr = KLADDR + 8*d;
	tp->t_dev = dev;
	if(d)
		addr =+ KLBASE-KLADDR-8;
	if(d >= NKL11)
		addr =+ DLBASE-KLBASE-8*NKL11+8;
	tp->t_addr = addr;
	if ((tp->t_state&ISOPEN) == 0) {
		tp->t_flags = XTABS|LCASE|ECHO|CRMOD;
		tp->t_erase = CERASE;
		tp->t_kill = CKILL;
		while (d >= NKL11 && (addr->klrcsr&CARDET)==0)
			{
			tp->t_state = WOPEN;
			addr->klrcsr = IENABLE|DSRDY|RDRENB|DIENABLE;
			sleep(&tp->t_canq,TTWPRI);
			}
		tp->t_state = ISOPEN|CARR_ON;
	}
	ttyopen(tp);
	addr->klrcsr =| IENABLE|DSRDY|RDRENB|DIENABLE;
	addr->kltcsr =| IENABLE;
}

klclose(dev)
{
	register struct tty *tp;
	register char *addr;

	tp = &kl11[dev.d_minor];
	ttyclose(tp);
	addr = tp->t_addr;
	addr->klrcsr = 0;
	addr->kltcsr = 0;
}

klread(dev)
{
	ttread(&kl11[dev.d_minor]);
}

klwrite(dev)
{
	ttwrite(&kl11[dev.d_minor]);
}

klxint(dev)
{
	register struct tty *tp;

	tp = &kl11[dev.d_minor];
	ttstart(tp);
	if (tp->t_outq.c_cc == 0 || tp->t_outq.c_cc == TTLOWAT)
		wakeup(&tp->t_outq);
}

klrint(dev)
{
	register int c, *addr;
	register struct tty *tp;
	int kltime();

	tp = &kl11[dev.d_minor];
	addr = tp->t_addr;
	if(dev.d_minor >= NKL11 && addr->klrcsr & DSCHG)
		{
		if(tp->t_state&ISOPEN)
			{
			signal(tp,SIGHUP);
			flushtty(tp);
			tp->t_state =& ~ CARR_ON;
			}
		else
			if(addr->klrcsr & CARDET)
				wakeup(&tp->t_canq);
			else
				{
				if((tp->t_state&TIMEOUT) == 0)
					{
					tp->t_state =| TIMEOUT;
					timeout(&kltime, tp, HZ * 10);
					}
				}
		return;
		}
	c = addr->klrbuf;
	addr->klrcsr =| RDRENB;
/*	if ((c&0177)==0)
		addr->kltbuf = c;	/* hardware botch */
	ttyinput(c, tp);
}

#ifndef IOCTL
klsgtty(dev, v)
int *v;
{
	register struct tty *tp;

	tp = &kl11[dev.d_minor];
	ttystty(tp, v);
}
#endif

#ifdef IOCTL
klioctl(dev,cmd,info,flag)
{
register struct tty *tp;
register char *addr;

tp = &kl11[dev.d_minor];
addr = tp->t_addr;
switch(cmd)
	{
case TIOCSBRK:
	addr->kltcsr =| TBRK;
	break;
case TIOCCBRK:
	addr->kltcsr =& ~TBRK;
	break;
default:
	ttyioctl(cmd,tp,info,dev);
	break;
	}
}
#endif

kltime(atp)
{
register char *addr;
register char *tp;
int wakeup();

tp = atp;
addr = tp->t_addr;
if((addr->klrcsr & CARDET) == 0)
	{
	addr->klrcsr = 0;
	tp->t_state =& ~ TIMEOUT;
	timeout(&wakeup, &tp->t_canq, HZ * 5);
	}
}
