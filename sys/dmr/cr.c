#
/* cr11 card reader driver - source obtained from harvard */
/*
 * modified Apr 6/81 to use internal buffer to reduce getc/putc
 * overhead ... still have TIMERR's when system busy.
 * modified May 29/81 to use hardware priority 7 to reduce
 * lost characters due to interrupt latency.
 */

#include "../param.h"
#include "../conf.h"
#include "../user.h"


#define	craddr	0177160

/* card reader states */
#define	CLOSED	0		/* not open */
#define	WAITING	1		/* for card reader to be ready */
#define	READING	2		/* while reading the card */
#define	EOF	3		/* when last card read */
#define	GOTCARD	4		/* when we have a card in buffer */
#define	ERROR	5		/* after timing error */
#define	COPY	6		/* when we have data for user */

#define	RDRENB		01
#define	EJECT		02
#define	IENABLE		0100
#define	COL_DONE	0200
#define	READY		0400
#define	BUSY		01000
#define	ONLINE		02000
#define	TIMERR		04000
#define	RDCHECK		010000
#define	PICKCHECK	020000
#define	CARD_DONE	040000
#define	ERR		0100000

#define	NLCHAR	0140
#define	BLANK	0
#define	NL	'\n'
#define	EOFCHAR	07417

#define	CRPRI	10
#define	CRLOWAT	50
#define	CRHIWAT	80

struct  {
	int crcsr;
	int crbuf1;
	int crbuf2;
	};

#define	MAXCOL	80		/* not counting the NL at the end */
struct cr11
	{
	int state;			/* state of the driver */
	int buffer[MAXCOL+1+1];		/* card image + NL + NULL buffer */
	int *rptr;			/* read pointer */
	int *wptr;			/* write pointer */
	int csr;			/* copy of csr */
	} cr11;

char asctab[]
{
	' ', '1', '2', '3', '4', '5', '6', '7',
	'8', ' ', ':', '#', '@', '\'', '=', '"',
	'9', '0', '/', 's', 't', 'u', 'v', 'w',
	'x', 'y', ' ', '\\', ',', '%', '_', '>',
	'?', 'z', '-', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', ' ', '!', '$', '*', ')',
	';', '~', 'r', '&', 'a', 'b', 'c', 'd',
	'e', 'f', 'g', 'h', ' ', '[', '.', '<',
	'(', '+', '|', 'i', ' ', ' ', ' ', ' ' 
};

cropen(dev,flag)
{
if (flag!=0 || cr11.state!=CLOSED)
	{
	u.u_error=ENXIO;
	return;
	}
cr11.state = WAITING;
craddr->crcsr = 0;
spl7();
while (craddr->crcsr & READY)
	{
	craddr->crcsr = IENABLE;		/* allow for online interrupt */
	sleep(&cr11.state,CRPRI);
	craddr->crcsr = 0;
	}
crstart();
spl0();
}

crclose(dev,flag)
{
spl7();
craddr->crcsr = 0;
cr11.state = CLOSED;
cr11.rptr = cr11.wptr = cr11.buffer;
spl0();
}

crread()
{
register int c;
register int *p;

spl7();
while (cr11.state == READING)
	sleep(&cr11.state,CRPRI);		/* wait til card read */
spl0();
switch(cr11.state)		/* determine what to do next */
	{
case GOTCARD:				/* got card, trim blanks etc. */
	if (cr11.wptr <= cr11.buffer)
		break;			/* we don't have a card */
	for (p = cr11.wptr; (*--p == BLANK); )
		;			/* note, state will stop it */
	cr11.wptr = ++p; 	/* point past end of buffer again */
	for (p=cr11.buffer; p < cr11.wptr; )
		*p++ = asciicon(*p);
	*p++ = NL;
	*p = 0;
	cr11.state = COPY;
	cr11.rptr = cr11.buffer;
/* fall thru to the copy case */
case COPY:			/* have card, will copy to user buffer */
	for (p = cr11.rptr; (c = *p++) && passc(c) >= 0 ; )
		;
	cr11.rptr = p;
	if (c == 0 || *p == 0)		/* need new card */
		{
		spl7();
		crstart();
		spl0();
		}
	break;
case ERROR:
	u.u_error = EIO;
case EOF:
	break;
	}
}

crstart()
{

cr11.state = READING;
craddr->crcsr =| IENABLE | RDRENB;
cr11.rptr = cr11.wptr = cr11.buffer;
}

crint()
{
register int card;
extern int wakeup();
register int wakeflag;

wakeflag = 0;
cr11.csr = craddr->crcsr;
if(cr11.state==READING)
	{	/* have a card being read */
	if(craddr->crcsr&COL_DONE)
		{
		card = craddr->crbuf2;
		*cr11.wptr++ = card;
		}
	if (craddr->crcsr&CARD_DONE || cr11.wptr >= cr11.buffer+MAXCOL)	/* complete card ? */
		{
		cr11.state = GOTCARD;
		wakeflag++;
		}
	if(craddr->crcsr & ERR)
		{
		if (craddr->crcsr & TIMERR)
			cr11.state = ERROR;
		else
			if (cr11.state != GOTCARD)
				cr11.state = EOF;
		wakeflag++;		/* wake up top level */
		craddr->crcsr = 0;	/* clear ERR */
		}
	}
else	/* not READING  */
	{
	wakeflag++;
	craddr->crcsr = 0;
	}
if (wakeflag)
	pirq(1,&wakeup,&cr11.state);	/* request a priority 1 interrupt */
}

asciicon(c)
{

register c1,c2;

c1 = c & 0377;
if (c1 == BLANK)
	return(' ');
if(c1>=0200)
	c1 =- 040;
c2 = c1;
while ((c2 =- 040) >= 0)
	c1 =- 017;
if(c1>0107)
	c1 = ' ';
else
	c1 = asctab[c1];
return(c1);
}
