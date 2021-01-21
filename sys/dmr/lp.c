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
#include "../proc.h"

#define	LPADDR	0177514

#define	IENABLE	0100
#define	DONE	0200
#define	ERROR	0100000

#define	LPPRI	10
#define	LPLWAT	50
#define	LPHWAT	200
#define	EJLINE	60
#define EJCOUNT	1
#define	MAXCOL	132

int lplines EJLINE;	/* number of lines on the page */

struct {
	int lpsr;
	int lpbuf;
};

struct  {
	int	cc;	/* for clist */
	int	cf;	/* for clist */
	int	cl;	/* for clist */
	int	flag;
	int	mcc;
	int	ccc;
	int	mlc;
	int	cols;
	int	plot;
	char	lastcnt;	/* number of times to output at lowest level */
	char	lastc;		/* last character output at lowest level*/
	char	c;		/* top level last character */
	char	outc;		/* lpoutput last character */
	char	cnt;		/* number of times in lpoutput() */
	char	dev;		/* minor device number */
	int	pid;		/* process ID that has it open */
	int	errcnt;		/* count errors */
	int	timer;		/* non-zero if timeout in progress */
} lp11;

#define	AUTOEJECT	01
#define	LFIGNORE	02

#define	CAP	00
#define	EJECT	02
#define	OPEN	04
#define	TOP	010

#define	FORM	014
#define	VTAB	013

lpopen(dev, flag)
{

	if(lp11.flag & OPEN || LPADDR->lpsr < 0) {
		u.u_error = EIO;
		return;
	}
	lp11.flag =| CAP|OPEN | TOP;
	lp11.plot = 0;
	lp11.cnt = 0; lp11.lastcnt = 0;
	if ((dev.d_minor&AUTOEJECT) == 0)
		 lp11.flag =| EJECT;	/* auto form feed */
	LPADDR->lpsr =| IENABLE;
	if(lp11.cols==0) lp11.cols=MAXCOL;
	lp11.mlc = lp11.mcc = 0;
	if (lp11.flag & EJECT)
		lpcanon(FORM);
	else
		lp11.flag =& ~ TOP;
	lp11.dev = dev.d_minor;		/* remember minor device */
	lp11.pid = u.u_procp->p_pid;	/* remember process id */
}

lpclose(dev, flag)
{
	register i;
	if ((lp11.flag & EJECT)  )
		lpcanon(FORM);
	lp11.flag = 0;
}

lpwrite()
{
	register int c;
	register char n;

	while ((c=cpass())>=0)
		{
		if (c&0200)
			{
			if (lp11.plot)
				{
				n = c;
				while (++n <= 0)
					lpcanon(lp11.c);
				}
			else
				u.u_error = ENXIO;	/* complain */
			}
		else
			lpcanon(c);
		}
}

lpcanon(c)
{
	register c1, c2;

	lp11.c = c1 = c;
#ifdef	UC
	if(lp11.flag&CAP) {
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
			lpcanon(c2);
			lp11.ccc--;
			c1 = '-';
		}
	}
#endif

	switch(c1) {

	case '\t':
		lp11.ccc = (lp11.ccc+8) & ~7;
		return;

	case VTAB:
		if (lp11.mlc > lplines/2)
			c1 = FORM;		/* use FF instead */
		--lp11.mlc;			/* fudge the line count */
	case FORM:
	case '\n':
		/* if not at top of page or if raw */
		if( (lp11.flag & TOP) == 0 || (lp11.dev&LFIGNORE)) {
			lp11.mcc = 0;
			if (lp11.plot == 0)
				{
				lp11.mlc++;
#ifdef ACC_LP
				++u.u_lplines;		/* count lines */
				}
			else
				{
				++u.u_lpplot;		/* count plot lines */
#endif
				}
			if(lp11.mlc >= lplines && lp11.flag&EJECT)
				c1 = FORM;
			lpoutput(c1);
			if((c1 == FORM))
				{
#ifdef ACC_LP
				++u.u_lppages;
#endif
				lp11.mlc = 0;
				lp11.flag =| TOP;
				}
		}

	case '\r':
		lp11.plot = 0;
		if(lp11.mcc > 0)
			{
			lpoutput('\r');
#ifdef ACC_LP
			++u.u_lplines;
#endif
			}
		lp11.mcc = lp11.ccc = 0;
		return;

	case 016:
		lpoutput(010);		/* double size mode */
		return;

	case 010:		/* back space */
		if(lp11.ccc > 0)
			lp11.ccc--;
		return;

	case ' ':
		lp11.ccc++;
		return;

	case 05:	/* printronix plot mode */
		++lp11.plot;
		lpoutput(c1);
		return;
	default:
		if(lp11.ccc < lp11.mcc) {
			lpoutput('\r');
			lp11.mcc = 0;
		}
		if(lp11.ccc < lp11.cols) {
			while(lp11.ccc > lp11.mcc) {
				lpoutput(' ');
				lp11.mcc++;
			}
			lpoutput(c1);
			lp11.mcc++;
		}
		lp11.ccc++;
	}
}

lpstart()
{
	register int c;

	while (LPADDR->lpsr&DONE)
		{
		if (lp11.lastcnt)
			{
			++lp11.lastcnt;
			LPADDR->lpbuf = lp11.lastc;
			}
		else if ((c = getc(&lp11)) >= 0)
			{
			if (c&0200)
				{
				lp11.lastcnt = c+1;
				c = lp11.lastc;
				}
			LPADDR->lpbuf = c;
			lp11.lastc = c;
			}
		else
			break;
		}
}

lptimer(dev) char *dev;	/* handy mode */
{
/*
 * entered 1 second after printer error.
 * if ERROR is on, or DONE is off, then
 * queue up another timer call.
 */
lp11.timer--;
LPADDR->lpsr = IENABLE;
LPADDR->lpbuf = 0;		/* try again */
if (LPADDR->lpsr&ERROR || (LPADDR->lpsr&DONE) == 0)
	{
	++lp11.timer;
	timeout(&lptimer,&lp11,HZ);
	}
}

lpint(dev) char *dev;	/* handy mode */
{
	register int c;

	if (LPADDR->lpsr&ERROR)
		{
		if (lp11.timer <= 0)
			{		/* queue up another try in 1 second */
			LPADDR->lpsr = IENABLE;		/* clear error */
			++lp11.errcnt;
			++lp11.timer;
			timeout(&lptimer,&lp11,HZ);
			}
		}
	else
		lpstart();
	if (lp11.cc == LPLWAT || lp11.cc == 0)
		wakeup(&lp11);
}

lpoutput(c) char c;
{
/*
 * if this character is the same as the last character then just
 * decrement the count (lp11.cnt).
 * otherwise output the count (if any) and then the current character.
 */
	if (c == lp11.outc)
		{
		if (--lp11.cnt != -127)
			return;
		}
	lp11.outc = c;
	lp11.flag =& ~ TOP;
	if (lp11.cc >= LPHWAT)
		sleep(&lp11, LPPRI);
	if (lp11.cnt)
		{
		putc(lp11.cnt,&lp11);
		lp11.cnt = 0;
		}
	putc(c, &lp11);
	spl4();
	lpstart();
	spl0();
}
