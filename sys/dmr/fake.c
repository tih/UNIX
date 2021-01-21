#
#include "../param.h"
#include "../conf.h" 
#include "../user.h"
#include "../proc.h"
#include "../buf.h"
#include "../tty.h"

/*
 * fake tty interface.
 * on one side it looks just like a normal tty.
 * on the other side is a program that gets control when the tty side
 * is used.
 */
#define	TIMELIMIT	15	/* send idle's every 15 seconds */
#define	GO	-1
#define	MAXLEN	240		/* maximum data length */
#define	FLAG	0100		/* program side flag */
#define	CMDPRIO		30
#define	RESPPRIO	-31
#define	WAITPRIO	32
#define	SYNCPRIO	-29
#define	OPENED	1
#define	CLOSED	0

#define	DONE	1	/* done the function */
#define	DATA	2	/* here is the data for a read */
#define	ERROR	3	/* error, code follows */
#define	KILL	4	/* send signal to tasks */

#define	OPEN	1	/* open the device */
#define	CLOSE	2	/* close it */
#define	READ	3	/* read from it */
#define	WRITE	4	/* write to it */
#define	STTY	6	/* stty */
#define	GTTY	5	/* gtty */
#define	IDLE	7	/* idle ... allows test for attention */
#define	MAXLINES	4	/* # of lines on device */

struct { char lo, hi ;};

struct fake
{
struct tty f_tty;
#define	f_timeout t_char	/* timeout counter */
char f_state;			/* state of prog side */
char f_sync;			/* basic sync variable */
struct clist f_cmd;		/* cmd clist */
struct clist f_resp;		/* responce clist */
} faketty[MAXLINES];
int faketr;
int faketm;
int faketimer();

fakeopen(dev,flag)
{
register int n;
register char *tp;

n = dev.d_minor&0277;
if (n >= MAXLINES)
	{
	u.u_error = ENXIO;	/* no such device */
	return;
	}
tp = &faketty[n];
if (faketm == 0)
	{
	++faketm;
	timeout(&faketimer,0,HZ);
	}
if (dev.d_minor&FLAG)
	{
	if (tp->f_state != CLOSED)
		u.u_error = ENXIO;
	fakemtq(tp);
	tp->f_state = OPENED;
	wakeup(&tp->f_state);
	return;
	}
else
	{
	if (u.u_procp->p_ttyp == 0)
		u.u_procp->p_ttyp = tp;
	tp->t_dev = dev;
	while (tp->f_state == CLOSED)
		sleep(&tp->f_state,WAITPRIO);
	fakesync(OPEN,tp);
	switch(fakerecv(tp))
		{
	case ERROR:
		u.u_error = fakerecv(tp);
	case DONE:
		;
		}
	fakesync(GO,tp);
	}
if (faketr) printf("fakeopen %d return\n",dev.d_minor);
}

fakeclose(dev)
{
register char *tp;

tp = &faketty[dev.d_minor&077];
if (dev.d_minor&FLAG)
	{
	tp->f_state = CLOSED;
	signal(tp,1);
	fakemtq(tp);
	wakeup(&tp->f_resp);		/* wake anyone waiting */
	wakeup(&tp->f_sync);		/* wake anyone waiting */
	tp->f_sync = 0;
	}
else
	{
	fakesync(CLOSE,tp);
	switch(fakerecv(tp))
		{
	case ERROR:
		u.u_error = fakerecv(tp);
		break;
		}
	fakesync(GO,tp);
	}
}

fakewrite(dev)
{
register char *tp;
register int c;
int len;

if (faketr) printf("fakewrite %o\n",dev.d_minor);
tp = &faketty[dev.d_minor&077];
if (tp->f_state == CLOSED)
	return;
tp->f_timeout = 0;		/* kill the timer */
if (dev.d_minor&FLAG)
	{
	c = cpass();
	switch(c)
		{
	case KILL:
		signal(tp,cpass());
		return;
	case IDLE:
		return;
		}
	do
		{
		putc(c,&tp->f_resp);
		}
	while ((c = cpass()) >= 0);
	wakeup(&tp->f_resp);
	}
else
	{
	while (u.u_count > 0)
		{
		c = cpass();
		putc(c,&tp->t_outq);
		if (c == '\n' || tp->t_outq.c_cc >= MAXLEN)
			{
			fakeflush(tp);		/* send data to user */
			if (issig())
				psig();		/* allow signals */
			}
		}
	}
}

fakeread(dev)
{
int n;
register char *tp;
register int c;

if (faketr) printf("fakeread %o\n",dev.d_minor);
tp = &faketty[dev.d_minor&077];
if (tp->f_state == CLOSED)
	return;
tp->f_timeout = 0;		/* kill timer */
if (dev.d_minor&FLAG)
	{
	while ((c = getc(&tp->f_cmd)) < 0)
		{
		sleep(&tp->f_cmd,CMDPRIO);
		if (tp->f_timeout >= TIMELIMIT)
			{
			passc(IDLE);
			return;
			}
		}
	do
		passc(c);
	while ((c = getc(&tp->f_cmd)) >= 0);
	}
else
	{
	if (n = tp->t_rawq.c_cc)
		{
		n = min(n,u.u_count);
		while (--n >= 0)
			passc(getc(&tp->t_rawq));
		return;
		}
	fakeflush(tp);
	fakesync(READ,tp);
	fakesend(u.u_count.lo,tp);
	fakesend(u.u_count.hi,tp);
	switch(fakerecv(tp))
		{
	case DATA:
		n.lo = fakerecv(tp);
		n.hi = fakerecv(tp);
		while (--n >= 0)
			{
			c = fakerecv(tp)&0177;
			if (u.u_count > 0)
				passc(c);
			else
				putc(c,&tp->t_rawq);
			}
		break;
	case ERROR:
		u.u_error = fakerecv(tp);
		break;
		}
	fakesync(GO,tp);
	}
}

fakerecv(tp) char *tp;
{
/*
 * receive a byte from the program task.
 * if nothing on the q then sleep til there is.
 */
register int c;

wakeup(&tp->f_cmd);		/* indicate we are ready for response */
while ((c = getc(&tp->f_resp)) < 0)
	{
	if (tp->f_state == CLOSED)
		return(ERROR);
	sleep(&tp->f_resp,RESPPRIO);
	}
return(c);
}

fakesend(c,tp) char *tp; char c;
{
/*
 * send a byte to the program task.
 */
putc(c,&tp->f_cmd);
}

fakegstty(dev,av) int *av;
{
/*
 * v == 0 for stty (use u.arg) 
 * v != 0 for gtty 
 */
register char *tp;
register int i;
register char *v;

tp = &faketty[dev.d_minor&077];
if (faketr) printf("fakegstty\n");
if (v = av)
	{
	fakesync(GTTY,tp);
	switch(fakerecv(tp))
		{
	case ERROR:
		u.u_error = fakerecv(tp);
		break;
	case DONE:
		for (i=0; i<6; ++i)
			*v++ = fakerecv(tp);
		break;
		}
	fakesync(GO,tp);
	}
else
	{
	v = u.u_arg;
	fakesync(STTY,tp);
	for (i=0; i<6; ++i)
		fakesend(*v++,tp);
	switch(fakerecv(tp))
		{
	case ERROR:
		u.u_error = fakerecv(tp);
		}
	fakesync(GO,tp);
	}
}

#ifdef IOCTL
fakeioctl(dev,cmd,addr,flag)
{
iostty(dev,cmd,addr,&fakegstty);
}
#endif

fakesync(cmd,tp) char *tp;
{
/*
 * basic sync routine:
 * cmd==GO then let next one through.
 *    !=   then wait until f_sync is zero.
 */
if (cmd == GO)
	{
	tp->f_sync = 0;
	wakeup(&tp->f_sync);
	return;
	}
while (tp->f_sync)
	{
	if (tp->f_state == CLOSED)
		return;
	sleep(&tp->f_sync,SYNCPRIO);
	}
++tp->f_sync;
fakesend(cmd,tp);
}

fakeflush(tpp) char *tpp;
{
register char *tp;
register int c;
int len;

tp = tpp;
if ((len = tp->t_outq.c_cc) == 0)
	return;
fakesync(WRITE,tp);
fakesend(len.lo,tp);
fakesend(len.hi,tp);
while (--len >= 0 && (c = getc(&tp->t_outq)) >= 0)
	fakesend(c,tp);
if (fakerecv(tp) != DONE)
	u.u_error = fakerecv(tp);
fakesync(GO,tp);
}

fakemtq(tp) char *tp;
{
while (getc(&tp->f_cmd) >= 0)
	;
while (getc(&tp->f_resp) >= 0)
	;
while (getc(&tp->t_outq) >= 0)
	;
while (getc(&tp->t_rawq) >= 0)
	;
}

faketimer()
{
/*
 * for each open tty that is idle (sync==0), increment the count of
 * the number of seconds since it was used. if its > TIMELIMIT then
 * wake up the fake read routine to send an IDLE to test for an
 * attention interrupt.
 */
register struct tty *tp;

for (tp = &faketty[0]; tp < &faketty[MAXLINES]; ++tp)
	{
	if (tp->f_state == OPENED && tp->f_sync == 0 &&
			++tp->f_timeout == TIMELIMIT)
		wakeup(&tp->f_cmd);
	}
timeout(&faketimer,0,HZ);
}
