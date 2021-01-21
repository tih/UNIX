#
/*
 *	Copyright 1973 Bell Telephone Laboratories Inc
 */

/*
 * LP-11 Line printer driver
 */

#include "../param.h"
#include "../conf.h"
#include "../user.h"

#define	LPADDR	0175614
#define	LPINT	0300	/* interrupt vector */

#define	IENABLE	0100
#define	DONE	0200

#define	LPPRI	10
#define	LPLWAT	50
#define	LPHWAT	200
#define	EJLINE	60
#define EJCOUNT	1
#define	MAXCOL	132

struct {
	int xpsr;
	int xpbuf;
};

struct  {
	int	cc;
	int	cf;
	int	cl;
	int	flag;
	int	mcc;
	int	ccc;
	int	mlc;
	int	cols;
} xp11;

#define	CAP	00
#define	EJECT	02
#define	OPEN	04
#define	TOP	010

#define	FORM	014
int xpfill 0140;		/* fill characters */

xpopen(dev, flag)
{

	if(xp11.flag & OPEN || LPADDR->xpsr < 0) {
		u.u_error = EIO;
		return;
	}
	xp11.flag =| CAP|OPEN | TOP;
	if(dev.d_minor == 0)
		 xp11.flag =| EJECT;	/* auto form feed */
	LPADDR->xpsr =| IENABLE;
	if(xp11.cols==0) xp11.cols=MAXCOL;
	xp11.mlc = xp11.mcc = 0;
	if (xp11.flag & EJECT)
		xpcanon(FORM);
	else
		xp11.flag =& ~ TOP;
}

xpclose(dev, flag)
{
	register i;
	if ((xp11.flag & EJECT)  )
		xpcanon(FORM);
	xp11.flag = 0;
}

xpwrite()
{
	register int c;

	while ((c=cpass())>=0)
		xpcanon(c);
}

xpcanon(c)
{
	register c1, c2;

	c1 = c;
#ifdef	UC
	if(xp11.flag&CAP) {
		if(c1>='a' && c1<='z')
			c1 =+ 'A'-'a'; else
		switch(c1) {

		case '{':
			c2 = '(';
			goto esc;

		case '}':
			c2 = ')';
			goto esc;

		case '`':
			c2 = '\'';
			goto esc;

		case '|':
			c2 = '!';
			goto esc;

		case '~':
			c2 = '^';

		esc:
			xpcanon(c2);
			xp11.ccc--;
			c1 = '-';
		}
	}
#endif

	switch(c1) {

	case '\t':
		xp11.ccc = (xp11.ccc+8) & ~7;
		return;

	case FORM:
	case '\n':
		/* if not at top of page */
		if( (xp11.flag & TOP) == 0) {
			xp11.mcc = 0;
			xp11.mlc++;
			if(xp11.mlc >= EJLINE && xp11.flag&EJECT)
				c1 = FORM;
			xpoutput(c1);
			for (c2 = xpfill; --c2 > 0; )
				xpoutput(0);
			if((c1 == FORM))
				{
				xp11.mlc = 0;
				xp11.flag =| TOP;
				}
		}

	case '\r':
		if(xp11.mcc > 0)
			xpoutput('\r');
		xp11.mcc = xp11.ccc = 0;
		return;

	case 010:
		if(xp11.ccc > 0)
			xp11.ccc--;
		return;

	case ' ':
		xp11.ccc++;
		return;

	default:
		if(xp11.ccc < xp11.mcc) {
			xpoutput('\r');
			xp11.mcc = 0;
		}
		if(xp11.ccc < xp11.cols) {
			while(xp11.ccc > xp11.mcc) {
				xpoutput(' ');
				xp11.mcc++;
			}
			xpoutput(c1);
			xp11.mcc++;
		}
		xp11.ccc++;
	}
}

xpstart()
{
	register int c;

	while (LPADDR->xpsr&DONE && (c = getc(&xp11)) >= 0)
		LPADDR->xpbuf = c;
}

xpint()
{
	register int c;

	xpstart();
	if (xp11.cc == LPLWAT || xp11.cc == 0)
		wakeup(&xp11);
}

xpoutput(c)
{
	xp11.flag =& ~ TOP;
	if (xp11.cc >= LPHWAT)
		sleep(&xp11, LPPRI);
	putc(c, &xp11);
	spl4();
	xpstart();
	spl0();
}
