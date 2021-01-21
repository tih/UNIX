#
#include	"../param.h"
#include	"../conf.h"
#include	"../user.h"
#include	"../tty.h"
#include	"../proc.h"

int icstart();
int icflag;		/* ic.io flag word */

/* PDP8 driver */

/*
 * a	PDP8 has interrupted 11
 * b	PDP8 iterface ready 
 */
#define	ICPRIO	20
#define	PDP8	0164000
#define	cleara	0100000
#define	enableb	040	/* in status register */
#define	enablea	0100	/* in status register */
#define	ready	0100000	/* in status register */
#define	readya	0200	/* in status register */
#define	readyb	0100000	/* same as ready */
#define	fn_addr	07601
#define	rc_addr	07604
#define	wdata	040000
#define	waddr	060000
#define	raddr	020000
#define	int8	0140000

#define	idle	0
#define	busy	1

#define	read	0
#define	write	1
#define	maxaddr	017777
#define	ic_mem 0	/* minor device for memory */

#define	MAXWAIT	10	/* maximum number of loops to wait for ready */
#define	rc_input	1
#define	rc_output	2

struct
	{
	char lo,hi;
	};
struct
	{
	int	p_csr;
	int	p_cmd;
	int	p_data;
	};

#define	FN_OPEN	1	/* open for input */
#define	FN_OUT	2	/* output character */
#define	FN_CLOSE	3	/* close off input */

#define	nic11	2	/* number if ttys present */
struct tty ic11[nic11];

icopen(dev,flag)
{
register struct tty *tp;
if(dev.d_minor & 07)
	{
	if (dev.d_minor > nic11)
		{
		err:
		u.u_error = ENXIO;
		return;
		}
	tp = &ic11[dev.d_minor-1];
	tp->t_addr = icstart;
	if (u.u_procp->p_ttyp == 0)
		{
		u.u_procp->p_ttyp = tp;
		tp->t_dev = dev;
		}
	if ((tp->t_state & ISOPEN) == 0)
		tp->t_flags = XTABS | LCASE | ECHO | CRMOD;
	tp->t_state = ISOPEN | SSTART | CARR_ON;
	PDP8->p_csr =| enablea;	/* let 8 interrupt */
	icsend(dev,FN_OPEN,0);
	}
else
	{
	if (dev.d_minor)
		{
		if (icflag)
			goto err;
		else
			++icflag;
		}
	}
}

icclose(dev)
{
register struct tty *tp;

if (dev.d_minor & 07)
	{
	tp = &ic11[dev.d_minor-1];
	wflushtty(tp);		/* finish off output */
	tp->t_state = 0;
	icsend(dev,FN_CLOSE,0);
	}
else
	if (dev.d_minor)
		icflag = 0;
}

icwrite(dev)
{
if ((dev.d_minor & 07) == 0)
	icio(dev,write);
else
	ttwrite(&ic11[dev.d_minor-1]);
}

icread(dev)
{
if ((dev.d_minor & 07) == 0)
	icio(dev,read);
else
	ttread(&ic11[dev.d_minor-1]);
}

icio(dev,fn)
{
register int cmd;
register int off;
int w;

if(dev.d_minor!=ic_mem)
	{
	iciowt(fn);
	return;
	}
off = u.u_offset[1] & (maxaddr<<1);
if(u.u_offset[0] || off!=u.u_offset[1] || u.u_count&1)
	{
	u.u_error=ENXIO;
	return;
	}

off =>> 1;
switch(fn)
{
case	read:
cmd = (off) | raddr;
do
	{
	spl4(); /* lock out interrupts */
	PDP8->p_cmd = cmd ++;
	if(! (PDP8->p_csr&ready))
		icwait();
	w = PDP8->p_data;
	spl0();
	passc(w.lo);
	passc(w.hi);
	}
while ((u.u_count>0) && (cmd & maxaddr));
break;

case	write:
	cmd = (off) | waddr;
	do
		{
		w.lo=cpass();
		w.hi=cpass() & 017;
		spl4();
		PDP8->p_cmd = cmd++;
		if (!(PDP8->p_csr&ready))
			icwait();
		PDP8->p_cmd=w | wdata;
		spl0();
		/*
		if(PDP8->p_data!=w)
			prdev("write error",dev);
		*/
		}
	while ((u.u_count>0) && (cmd & maxaddr));
	break;
	}
}

icwait()
{
	while ((PDP8->p_csr&ready) == 0)
		{
		PDP8->p_csr =| enableb;
		sleep(&ic11,ICPRIO);
		}
	PDP8->p_csr=& ~enableb;
}

icint()
{
register int rc, dev;
register struct tty *tp;
int data;
if (PDP8->p_csr & enableb)
	wakeup(&ic11);
else
	{
	PDP8->p_cmd = cleara;		/* clear interrupt source */
	PDP8->p_cmd = rc_addr | raddr;
	if ((rc = icget()) < 0 || (dev = icget()) < 0)
		return;		/* 8 not up */
	if (dev == 0)
		{
		wakeup(&icflag);
		goto done;
		}
	data = icget();
/* 	printf("rc=%o dev=%o data=%o\n",rc,dev,data); */
	tp = &ic11[dev-1];
	switch(rc)
		{
	case rc_input:		/* input done */
		ttyinput(data,tp);
		break;
	case rc_output:
		tp->t_state =& ~ BUSY;
		icstart(tp);
		if (tp->t_outq.c_cc == 0 || tp->t_outq.c_cc == TTLOWAT)
			wakeup(&tp->t_outq);
		}
	done:
	icput(rc_addr,0);	/* let 8 know we got it */
	}
}



icstart(atp) struct tty *atp;
{
register struct tty *tp;
register sps;
register int c;
extern ttrstrt;

sps = PS->integ;
spl4();
tp = atp;

	if ((tp->t_state & (TIMEOUT | HALTOP | BUSY)) )
		goto out;
	if ((PDP8->p_csr & ready) == 0)
		{
		timeout(&icstart, tp, 10);
		goto out;
		}
	if ((c=getc(&tp->t_outq)) >= 0) {
		if (c<=0177)
			{
			icsend(tp->t_dev,FN_OUT,c);
			tp->t_state =| BUSY;
			}
		else {
			timeout(&ttrstrt, tp, c&0177);
			tp->t_state =| TIMEOUT;
		}
	}
out:
PS->integ = sps;
}

icsend(dev,cmd,data)
{
register int sps;
register int n;
extern int lbolt;
if ((PDP8->p_csr & ready) == 0)
	icwait();
sps = PS->integ;
spl4();
retry:
n = 1000;
do
	{
	PDP8->p_cmd = fn_addr + raddr;
	if (--n == 0)
		{
		prdev("pdp8 hung",dev);
		sleep(&lbolt,ICPRIO);
		goto retry;
		}
	}
while (icget());
icput(fn_addr,cmd);
icput(fn_addr+1,dev.d_minor);
icput(fn_addr+2,data);
PDP8->p_cmd = int8;		/* interrupt the 8 */
PS->integ = sps;
}

icget()
{
register int n;
n = 0;
while ((PDP8->p_csr & ready) == 0)
	if (++n > MAXWAIT)
		return(-1);		/* 8 not ready */
return(PDP8->p_data);
}

icput(addr,data)
{
register int n;

/* printf("put %o ==> %o\n",data,addr); */
PDP8->p_cmd = waddr | addr;
n = 0;
while ((PDP8->p_csr & ready) == 0)
	if (++n > MAXWAIT)
		return(-1);
PDP8->p_cmd = wdata | data;
}

icsgtty(dev,v) int *v;
{
if (dev.d_minor & 07)
	ttystty(&ic11[dev.d_minor-1],v);
}

iciowt(fn)
/* if fn==read then wait for the PDP8 to complete function 
 *
 *      ==write then just clear any pending interrupt
 */
{

if (fn==read)
	{
	icsend(0,FN_OUT,0);
	sleep(&icflag,ICPRIO);
	}
}
