#
#define	TRACE	0
#define	debug	dup11.dbg
#define	DELAY	5		/* wait 5 seconds before re-transmit */
#define	NODELAY	-1		/* when not expecting re-transmit */

#include "../param.h"
#include "../conf.h"
#include "../user.h"
#include "../buf.h"

#define	trace	if (duptrf) duptrace 	/* conditional trace */
#define	DUPPRIO		3
#define	DUPXPRIO	-2
#define	DUPQPRIO	-1
/*
 * following are in rxcsr.
 */
#define	DSCHG	0100000
#define	RING	040000
#define	CTS	020000
#define	CARRIER	010000
#define	RCVACT	04000
#define	SCRCVD	02000
#define	DSR	01000
#define	STRSYNC	0400
#define	RCVDONE	0200
#define	RXITEN	0100
#define	DSITEN	040
#define	RCVEN	020
#define	SCTRDT	010
#define	RTS	04
#define	DTR	02
#define	DSCHNG	01
/*
 * following are in rxbuf.
 */
#define	RCVERR	0100000
#define	OVRRUN	040000
#define	CRCERR	010000
#define	RABORT	02000
#define	REOM	01000
#define	RSOM	0400
/*
 * following are in parcsr.
 */
#define	DECMODE	0100000
#define	SECADDR	010000
#define	CRCINH	01000
/*
 * following are in txcsr.
 */
#define	TXLATE	0100000
#define	MTXOUT	040000
#define	MTSCLK	020000
#define	MTSELB	010000
#define	MTSELA	04000
#define	MINPUT	02000
#define	TXACT	01000
#define	RESET	0400
#define	TXDONE	0200
#define	TXITEN	0100
#define	SEND	020
#define	HALF	010
/*
 * following are in txbuf.
 */
#define	RCRCIN	040000
#define	TCRCTIN	010000
#define	MTTIME	04000
#define	ABORT	02000
#define	TEOM	01000
#define	TSOM	0400

struct { char lo, hi; };
struct dup
{
int rxcsr;		/* status */
int rxbuf;		/* data etc */
#define parcsr rxbuf
int txcsr;		/* xmit status */
int txbuf;	/* transmit data */
};

#define	DUP	0160250		/* device register */

#define	IGNORE	0
#define	BUSY	1
#define	READY	2
#define	IDLE	3
#define	ERROR	4

#define	CLOSED	0
#define	OPEN	1
struct dup11
{
char dbg;		/* debug flag */
char maint;		/* maint bit setting */
char reset;		/* 1 if reset to be given if xlate */
char state;		/* driver status */
char ostate;		/* output state */
char addr;		/* current line address */
char siline;		/* input state */
char opcnt;		/* number of times opened */
char late;		/* # of late transmissions */
char abort;		/* aborted messages recv'd */
char ovrrun;		/* receiver overrun */
int xintr;		/* # of transmitter interrupts */
int ignore;		/* # of messages ignored */
int oddcnt;		/* # of messages with odd count */
char *tp;		/* pointer to current line */
char *optr;		/* output pointer */
int olength;		/* output length */
int ocnt;		/* output count */
#define	MAXBUFF	512
char *obuff;		/* output buf pointer */
char timeflg;		/* zero if no timer going */
int xtimer;		/* timer for lost xmit interrupts */
} dup11;

int duptimer();		/* the timeout routine */
#define	MAXLINE	8	/* number of logical lines */
struct line
{
char istate;		/* if open etc. */
char isbuff;		/* previous input state */
char inerr;		/* # of input errors */
int inign;		/* ignored lines */
char *ibuff;		/* input buf pointer */
int ilength;		/* input length */
char *iptr;		/* input pointer */
int rcnt;		/* # of messages read */
int wcnt;		/* # of messages written */
int olen;		/* last output length */
char timer;		/* # of seconds allowed on read before returning */
char timecnt;		/* # of times we timed out */
} duplines[MAXLINE];

int duptrf 1;		/* 1 if tracing is on; 0 otherwise */
int wakeup();

dupopen(dev,flag)
{
register char *tp;

/*	if (debug) printf("dupopen\n"); */
wakeup(&dup11.ostate);			/* just in case we are hung */
if (dev.d_minor >= MAXLINE || (tp = &duplines[dev.d_minor])->istate != CLOSED)
	{
	u.u_error = ENXIO;
	return;
	}
if (dup11.state == CLOSED)
	{
	dup11.state = OPEN;
	dup11.obuff = getblk(NODEV);
	dup11.ostate = IDLE;
	if (dup11.timeflg == 0)
		{
		++dup11.timeflg;
		duptimer();
		}
	spl7();
	dupreset();		/* reset the device */
	spl0();
	}
++dup11.opcnt;			/* # of times major device opened */
tp->ibuff = getblk(NODEV);
dup11.siline = IDLE;
tp->istate = OPEN;
tp->isbuff = IDLE;
}

dupwrite(dev)
{
register char *p;
register int c;
register char *tp;

tp = &duplines[dev.d_minor];
/*	if (debug) printf("dupwrite\n"); */
spl6();
while (dup11.ostate != IDLE)
	sleep(&dup11.ostate,DUPQPRIO);
dup11.ostate = BUSY;
spl0();
if (u.u_count > MAXBUFF)
	{
	u.u_error = ENXIO;
	return;
	}
tp->olen = dup11.olength = u.u_count;
if (u.u_count&1)
	{
	++dup11.oddcnt;
	++u.u_count;
	}
iomove(dup11.obuff,0,u.u_count,B_WRITE);
trace('s',dup11.olength,dup11.obuff->b_addr);
spl7();
do
	{
	if (dup11.ostate == ERROR)
		{
		while (DUP->rxcsr & RCVACT)
			sleep(&dup11.ostate,DUPPRIO);
		dupreset();			/* reset to get xmit going */
		}
	dupxstart();
	while (dup11.ostate == BUSY)
		sleep(&dup11.ostate,DUPXPRIO);
	}
while (dup11.ostate == ERROR);
spl0();
++tp->wcnt;
}

dupxintr()
{
register int c;
register int wakeflag;

wakeflag = 0;
++dup11.xintr;			/* count # of interrupts */
if (DUP->txcsr & TXLATE)
	{
	++dup11.late;		/* count # of lates */
	DUP->txcsr = DUP->txbuf = 0;			/* clear xmit'er */
	if (dup11.reset)
		dupxstart();		/* restart without reset */
	else
		{
		dup11.ostate = ERROR;
		++wakeflag;
		}
	}
else if (DUP->txbuf & TSOM)
	{
	dup11.optr = dup11.obuff->b_addr;
	dup11.ocnt = dup11.olength;
	goto output;
	}
else if (DUP->txbuf & TEOM)
	{
	DUP->txbuf = 0;
	DUP->txcsr =& ~SEND;
	dup11.ostate = IDLE;
	dup11.xtimer = 0;
	++wakeflag;
	}
else if ((DUP->txcsr & TXACT) == 0)
	;
else if (dup11.ocnt > 0)
	{
output:
	c = 0;
	c =| *dup11.optr++;
	DUP->txbuf = c;
	dup11.ocnt--;
	}
else
	{
	DUP->txbuf = TEOM;
	}
if (wakeflag)
	pirq(2,&wakeup,&dup11.ostate);
}

dupread(dev)
{
register char *tp;
register char *p;

tp = &duplines[dev.d_minor];
/*	if (debug) printf("dupread\n"); */
spl6();
if (tp->olen > 2)
	tp->timer = DELAY;
else
	tp->timer = NODELAY;
while (tp->isbuff != READY)
	{
	if (tp->timer == 0)
		{ spl0(); tp->olen = 0; ++tp->timecnt; return; }
	sleep(&tp->istate,DUPPRIO);
	}
spl0();
tp->ilength =- 2;		/* remove crc */
trace('r',tp->ilength, tp->ibuff->b_addr);
if (tp->ilength > u.u_count)
	{
	u.u_error = ENXIO;
	return;
	}
iomove(tp->ibuff,0,tp->ilength,B_READ);
tp->isbuff = IDLE;
tp->rcnt++;
tp->olen = 0;		/* reset upon successful read */
}

duprintr()
{
register int c;
register char *tp;

tp = dup11.tp;
c = DUP->rxbuf;
if (c & RSOM)
	{
/*
 * got first character of the message (address)
 * if we device is open, and address is in range, and that line
 * doesn't already have a message start filling up that buffer.
 */
	c = DUP->rxbuf&0377;		/* get line address */
	dup11.addr = c;
	tp = 0;
	if (c > MAXLINE*2 || (tp = &duplines[c>>1])->istate != OPEN ||
			tp->isbuff == READY)
		{
		if (tp)
			++tp->inign;
		else
			++dup11.ignore;
		dup11.siline = IGNORE;
		}
	else
		{
		dup11.tp = tp;
		tp->isbuff = dup11.siline = BUSY;
		tp->iptr = tp->ibuff->b_addr;
		tp->ilength = 0;
		goto input;
		}
	}
else if (c & REOM)
	{
/*
 * end of message. if we were actively receiving a message
 * then isbuff==BUSY and we have a message.
 * otherwise start reading again.
 */
	DUP->rxcsr =& ~RCVEN;
	DUP->rxcsr =| RCVEN;
	if (c & (RCVERR | RABORT | OVRRUN | CRCERR))
		++tp->inerr;
	else if (tp->isbuff == BUSY)
		{
		tp->isbuff = READY;
		tp->timer = NODELAY;		/* don't wake up */
		pirq(3,&wakeup,&tp->istate);
		}
	dup11.siline = IDLE;	/* look for next message */
	if (dup11.ostate == ERROR)
		pirq(2,&wakeup,&dup11.ostate);
	}
else if (c & (RABORT | RCVERR | OVRRUN))
	{
	dup11.siline = IGNORE;
	if (tp->isbuff == BUSY)
		tp->isbuff = IDLE;
	if (c&RABORT)
		++dup11.abort;		/* count # of aborted messages */
	if (c&OVRRUN)
		++dup11.ovrrun;
	}
else	/* normal character */
	{
	c = DUP->rxbuf.lo;
	input:
	switch(dup11.siline)
		{
	case BUSY:
		if (tp->ilength >= MAXBUFF)
			dup11.siline = IGNORE;
		else
			{
			*tp->iptr++ = c;
			++tp->ilength;
			}
		break;
		}
	}
}


dupxstart()
{
DUP->txbuf =| TSOM;
DUP->txcsr =| SEND;
dup11.ostate = BUSY;
dup11.xtimer = DELAY;		/* just in case of lost interrupt */
}

dupclose(dev)
{
/*
 * close given line (dev.d_minor)
 * after last line closed free the output buffer.
 */
register char *tp;

tp = &duplines[dev.d_minor];
/*	if (debug) printf("dupclose\n"); */
tp->istate = CLOSED;
tp->isbuff = IDLE;
brelse(tp->ibuff);
tp->ibuff = 0;
tp->olen = 0;
if (--dup11.opcnt == 0)
	{
	dup11.siline = CLOSED;
	brelse(dup11.obuff);
	dup11.obuff = 0;
	dup11.ostate = CLOSED;
	dup11.state = CLOSED;
	dup11.xtimer = 0;
	DUP->txcsr = RESET;	/* clear the device */
	}
}

dupreset()
{
DUP->txcsr = RESET;	/* clear the device */
DUP->txcsr = (dup11.maint<<11) | TXITEN;
DUP->rxcsr = RXITEN | RCVEN | RTS | DTR;
}

duptimer()
{
/*
 * routine entered at 1 second intervals to check for messages that
 * have not been responded to.
 * wake up the read routine when the timer goes to zero.
 */
register struct line *tp;

for (tp = duplines; tp < duplines+MAXLINE; ++tp)
	{
	if (tp->timer > 0 && --tp->timer == 0)
		wakeup(&tp->istate);
	}
if (dup11.xtimer && --dup11.xtimer == 0)
	{
	printf("DUP11 lost interrupt\n");
	dupxstart();		/* restart transmission */
	}
if (dup11.state == OPEN)
	timeout(&duptimer,0,HZ);
else
	dup11.timeflg = 0;
}

#ifdef	TRACE
#define	TRCMAX	1024
char trcbuff[TRCMAX];
char *trcptr &trcbuff;
duptrace(code,len,buff)
{
register char *p;
struct { int *INT; };
register int *b;
extern int time[2];
extern int lbolt;


b = buff;
p = trcptr;
*p++ = code;
*p++ = len;
*p.INT++ = time[1];
*p.INT++ = *b++;
*p.INT++ = *b++;
if (p >= trcbuff+TRCMAX)
	p = trcbuff;
trcptr = p;
*--p = lbolt;
}
#endif

#ifndef	TRACE
duptrace()
{ }
#endif
