#
/*
 *	Copyright 1973 Bell Telephone Laboratories Inc
 */

/*
 * general TTY subroutines
 */
#include "../param.h"
#include "../user.h"
#include "../tty.h"
#include "../proc.h"
#include "../inode.h"
#include "../file.h"
#include "../reg.h"
#include "../conf.h"

#define FF 014
struct { int integ; };

/*
 * Input mapping table-- if an entry is non-zero, when the
 * corresponding character is typed preceded by "\" the escape
 * sequence is replaced by the table value.  Mostly used for
 * upper-case only terminals.
 */
char	maptab[]
{
	000,000,000,000,004,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,'|',000,000,000,000,000,'`',
	'{','}',000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,'~',000,
	000,'A','B','C','D','E','F','G',
	'H','I','J','K','L','M','N','O',
	'P','Q','R','S','T','U','V','W',
	'X','Y','Z',000,000,000,000,000,
};

/*
 * The actual structure of a clist block manipulated by
 * getc and putc (mch.s)
 */
struct cblock {
	struct cblock *c_next;
	char info[6];
};

/* The character lists-- space for 6*NCLIST characters */
struct cblock cfree[NCLIST];
/* List head for unused character blocks. */
struct cblock *cfreelist;

/*
 * structure of device registers for KL, DL, and DC
 * interfaces-- more particularly, those for which the
 * SSTART bit is off and can be treated by general routines
 * (that is, not DH).
 */
struct {
	int ttrcsr;
	int ttrbuf;
	int tttcsr;
	int tttbuf;
};

/*
 * Wait for output to drain, then flush input waiting.
 */
wflushtty(atp)
struct tty *atp;
{
	register struct tty *tp;

	tp = atp;
	spl5();
	while (tp->t_outq.c_cc) {
		tp->t_state =| ASLEEP;
		sleep(&tp->t_outq, TTOPRI);
	}
	flushtty(tp);
	spl0();
}

/*
 * Initialize clist by freeing all character blocks, then count
 * number of character devices. (Once-only routine)
 */
cinit()
{
	register int ccp;
	register struct cblock *cp;
	register struct cdevsw *cdp;

	ccp = cfree;
	for (cp=(ccp+07)&~07; cp <= &cfree[NCLIST-1]; cp++) {
		cp->c_next = cfreelist;
		cfreelist = cp;
	}
	ccp = 0;
	for(cdp = cdevsw; cdp->d_open; cdp++)
		ccp++;
	nchrdev = ccp;
}

/*
 * put character on TTY output queue, adding delays,
 * expanding tabs, and handling the CR/NL bit.
 * It is called both from the top half for output, and from
 * interrupt level for echoing.
 * The arguments are the character and the tty structure.
 */
ttyoutput(ac, tp)
struct tty *tp;
{
	extern char partab[];	/* ASCII table: parity, character class */
	register int c;
	register struct tty *rtp;
	register char *colp;
	int ctype;

	rtp = tp;
	if (rtp->t_flags == RAW)	/* just in raw mode */
		{
		putc(ac,&rtp->t_outq);
		return;
		}
	c = ac&0177;
	/*
	 * Ignore EOT in normal mode to avoid hanging up
	 * certain terminals.
	 */
	if (c==004 && (rtp->t_flags&RAW)==0)
		return;
	/*
	 * Turn tabs to spaces as required
	 */
	if (ac=='\t' && rtp->t_flags&XTABS) {
		do
			ttyoutput(' ', rtp);
		while (rtp->t_col&07);
		return;
	}
	/*
	 * generate a series of line feeds for a 
	 * form feed.
	 */
	if(ac == FF && (rtp->t_flags&(RAW|VTDELAY)) == 0) {
		c = 3;
		do
			ttyoutput('\n',rtp);
		while (--c);
		return;
	}
	/*
	 * for upper-case-only terminals,
	 * generate escapes.
	 */
	if (rtp->t_flags&LCASE) {
		colp = "({)}!|^~'`";
		while(*colp++)
			if(c == *colp++) {
				ttyoutput('\\', rtp);
				c = colp[-2];
				break;
			}
		if ('a'<=c && c<='z')
			c =+ 'A' - 'a';
	}
	/*
	 * turn <nl> to <cr><lf> if desired.
	 */
	if (c=='\n' && rtp->t_flags&CRMOD)
		ttyoutput('\r', rtp);
	if (putc(c, &rtp->t_outq))
		return;
	/*
	 * Calculate delays.
	 * The numbers here represent clock ticks
	 * and are not necessarily optimal for all terminals.
	 * The delays are indicated by characters above 0200,
	 * thus (unfortunately) restricting the transmission
	 * path to 7 bits.
	 */
	colp = &rtp->t_col;
	ctype = partab[c];
	c = 0;
	switch (ctype&077) {

	/* ordinary */
	case 0:
		(*colp)++;

	/* non-printing */
	case 1:
		break;

	/* backspace */
	case 2:
		if (*colp)
			(*colp)--;
		break;

	/* newline */
	case 3:
		ctype = (rtp->t_flags >> 8) & 03;
		if(ctype == 1) { /* tty 37 */
			if (*colp)
				c = max((*colp>>4) + 3, 6);
		} else if(ctype == 2)  /* vt05 */
			c = 6;
		else if(ctype == 3) /* Ann Arbor stupid terminal */
			c = 1;
		*colp = 0;
		break;

	/* tab */
	case 4:
		ctype = (rtp->t_flags >> 10) & 03;
		if(ctype == 1) { /* tty 37 */
			c = 1 - (*colp | ~07);
			if(c < 5)
				c = 0;
		}
		*colp =| 07;
		(*colp)++;
		break;

	/* vertical motion */
	case 5:
		if(rtp->t_flags & VTDELAY) /* tty 37 */
			c = 0177;
		break;

	/* carriage return */
	case 6:
		ctype = (rtp->t_flags >> 12) & 03;
		if(ctype == 1) { /* tn 300 */
			c = 5;
		} else
		if(ctype == 2) { /* ti 700 */
			c = 10;
		}
		*colp = 0;
		break;
	/* sync == delay output for .1 second. */
	case 7:
		c = HZ/10;
		break;
	}
	if(c)
		putc(c|0200, &rtp->t_outq);
}

/*
 * Restart typewriter output following a delay
 * timeout.
 * The name of the routine is passed to the timeout
 * subroutine and it is called during a clock interrupt.
 */
ttrstrt(atp)
{
	register struct tty *tp;

	tp = atp;
	tp->t_state =& ~TIMEOUT;
	ttstart(tp);
}

/*
 * Start output on the typewriter. It is used from the top half
 * after some characters have been put on the output queue,
 * from the interrupt routine to transmit the next
 * character, and after a timeout has finished.
 * If the SSTART bit is off for the tty the work is done here,
 * using the protocol of the single-line interfaces (KL, DL, DC);
 * otherwise the address word of the tty structure is
 * taken to be the name of the device-dependent startup routine.
 */
ttstart(atp)
struct tty *atp;
{
	register int *addr, c;
	register struct tty *tp;
	struct { int (*func)(); };
	extern char partab[];	/* ASCII table: parity, character class */

	tp = atp;
	addr = tp->t_addr;
	if (tp->t_state&SSTART) {
		(*addr.func)(tp);
		return;
	}
	if ((addr->tttcsr&DONE)==0 || tp->t_state&(TIMEOUT|HALTOP))
		return;
	/*
	 * if RAW and no parity bits set, then use all 8 bits.
	 * if not RAW, or if parity bits set, then check for timeout
	 * and generate appropriate parity bits.
	 */
	if ((c=getc(&tp->t_outq))>=0)
		{
		if (tp->t_flags != RAW)
			{
			if (c>=0200)
				{
				tp->t_state=|TIMEOUT;
				timeout(ttrstrt,tp,(c&0177)+6);
				}
			else
				{
				switch (tp->t_flags&(EVENP|ODDP))
					{
				case EVENP:
					c =| (partab[c]&0200);
					break;
				case ODDP:
					c =| (partab[c]&0200) ^ 0200;
					}
				addr->tttbuf = c;
				}
			}
		else
			addr->tttbuf = c;	 /* use all 8 bits */
		}
}

/*
 * Called from device's read routine after it has
 * calculated the tty-structure given as argument.
 * The pc is backed up for the duration of this call.
 * In case of a caught interrupt, an RTI will re-execute.
 */
ttread(atp)
struct tty *atp;
{
	register struct tty *tp;
	register char *base;
	tp = atp;
	base = u.u_base;
	if ((tp->t_state&CARR_ON)==0)
		return;
	tp->t_state =& ~ OPSUPRS;
	if (tp->t_canq.c_cc || canon(tp))
		while (tp->t_canq.c_cc && passc(getc(&tp->t_canq))>=0)
			;
#ifdef LOGGING
	if ((tp->t_state & TTYILOG) && (tp->t_flags&RAW) == 0 && (tp->t_flags&ECHO) )
		logtty(tp, base, u.u_base-base);	/* log what's read */
#endif
}

/*
 * Called from the device's write routine after it has
 * calculated the tty-structure given as argument.
 */
ttwrite(atp)
struct tty *atp;
{
	register struct tty *tp;
	register int c;

	tp = atp;
	if ((tp->t_state&CARR_ON)==0)
		return;
#ifdef LOGGING
	if ((tp->t_state & TTYOLOG) && (tp->t_flags&RAW) == 0 && (tp->t_flags&ECHO) )
		logtty(tp, u.u_base, u.u_count);
#endif
	while ((c=cpass())>=0) {
		if((tp->t_state&OPSUPRS)==0) {
			spl5();
			while (tp->t_outq.c_cc > TTHIWAT) {
				ttstart(tp);
				tp->t_state =| ASLEEP;
				sleep(&tp->t_outq, TTOPRI);
			}
			spl0();
			ttyoutput(c, tp);
		}
	}
	spl5();
	ttstart(tp);
	spl0();
}

/*
 * flush all TTY queues
 */
flushtty(atp)
struct tty *atp;
{
	register struct tty *tp;
	register int sps;

	tp = atp;
	while (getc(&tp->t_canq) >= 0);
	while (getc(&tp->t_outq) >= 0);
	wakeup(&tp->t_canq);
	wakeup(&tp->t_rawq);
	wakeup(&tp->t_outq);
	sps = PS->integ;
	spl5();
	ttstop(tp);
	while (getc(&tp->t_rawq) >= 0);
	tp->t_beglin = 0;
	tp->t_lincnt = 0;
	tp->t_delct = 0;
#ifdef SYNCECHO
	tp->t_echoq = 0;
#endif
	PS->integ = sps;
}

/*
 * transfer raw list to canonical list,
 * doing code conversion.
 * such as \ processing, EOT recognition
 * the characters are taken of rawq and put on canq.
 * input sleep is done until the rawq has a delimiter.
 * if there is insuffient input on the rawq and we turned
 * off input via ^S then send a ^Q to get more input.
 */
canon(atp)
struct tty *atp;
{
	register struct tty *tp;
	register int c, slf;

	tp = atp;
	spl5();
#ifdef SYNCECHO
	tp->t_procp = u.u_procp;
	if (tp->t_state&SECHO)
		ttecho(tp);
#endif
	while (tp->t_delct==0) {	/* if no delimiters on rawq */
		if ((tp->t_state&CARR_ON)==0)
			{ spl0(); return(0); }
		if (tp->t_state & HALTSENT)
			{
			ttyoutput(CSTARTOP,tp);
			ttstart(tp);		/* insure its going out */
			tp->t_state =& ~HALTSENT;
			}
		tp->t_state =| INSLEEP;		/* we are now sleeping */
		sleep(&tp->t_rawq, TTIPRI);	/* sleep til delcnt != 0 */
	}
	spl0();
	slf = 0;
	while ((c=getc(&tp->t_rawq)) >= 0) {
/*
 * treat 0377 as delimiter,
 * except if in 8 bit mode and if first of a pair
 * of delimiters.
 */
		if (c==0377 && (tp->t_flags != RAW || (tp->t_rawq.c_cc&1) == 0)) {
			tp->t_delct--;
			break;
		}
		if ((tp->t_flags&RAW)==0 && (tp->t_flags&LCASE)) {
			if(c=='\\'){
				if(slf){
					slf = 0;
					putc(c, &tp->t_canq);
				} else
					slf++;
				continue;
			} else {
				if(slf){
					if (maptab[c])
						c = maptab[c];
					else
						putc('\\', &tp->t_canq);
					slf = 0;
				} else
					if(c==CEOT)
						continue;
			}
		}
		putc(c, &tp->t_canq);
	}
#ifdef SYNCECHO
	if (tp->t_rawq.c_cc == 0)
		tp->t_echoq = 0;	/* nothing on q */
#endif
	return(1);
}

reducq(qp)
/*
 * remove a character from the raw input q.
 * free the clist block if required to.
 */
struct clist *qp;
{
	extern cblkuse;
	register struct clist *rqp;
	register char *rcb, *rcb1;
	int sps;

	if ((rqp = qp) == 0)
		return(-1);
	if(rqp->c_cc==0)
		return(-1);
	sps = PS->integ;
	spl6();
	if(--rqp->c_cc==0){
		rcb = rqp->c_cf&~07;
		rcb->c_next = cfreelist;
		cfreelist = rcb;
		cblkuse--;
		rqp->c_cf = 0;
		rqp->c_cl = 0;
	} else {
		if(((rcb1 = --rqp->c_cl)&07)==2){
			rcb1 =& ~07;
			for(rcb = rqp->c_cf& ~07; rcb->c_next!=rcb1; rcb = rcb->c_next);
			rcb1->c_next = cfreelist;
			cfreelist = rcb1;
			cblkuse--;
			rqp->c_cl = rcb+010;
			rcb->c_next = 0;
		}
	}
	PS->integ = sps;
	return((rcb = rqp->c_cl) ? (*(rcb-1))&0377 : -1);
}


ttstop()
{
}

ttyopen(atp) struct tty *atp;
{
/*
 * common code for opening a TTY device 
 */
register struct tty *tp;

	tp = atp;
	if (u.u_procp->p_ttyp == 0)
		u.u_procp->p_ttyp = tp;
	tp->t_state =& ~WOPEN;
	tp->t_state =| ISOPEN;
	++ttysopen;			/* count number opened */
}

ttyclose(atp) struct tty *atp;
{
/*
 * common code for closing a TTY device 
 */
register struct tty *tp;

	tp = atp;
	wflushtty(tp);
#ifdef LOGGING
	if (tp->t_logf != NULL)
		{
		closef(tp->t_logf);
		tp->t_logf = NULL;
		}
#endif
	if (tp->t_state&ISOPEN)
		--ttysopen;		/* no longer open */
	tp->t_state = 0;
#ifdef INTIMER
	tp->t_intimer = tp->t_indelay = 0;	/* reset input timer */
#endif
}

#ifdef SYNCECHO
ttecho(atp) struct tty *atp;
{
/*
 * routine to echo those characters on the input (RAW) Q 
 * that are to be read on the current read and that have
 * not been echoed yet.
 * scan along raw q with tp->t_echoq pointer echoing characters
 * til we hit a delimiter.
 */
register struct tty *tp;
register char *q;
int startf;
int c;

startf = 0;
tp = atp;
if (tp->t_rawq.c_cc == 0)
	{			/* nothing to echo */
	tp->t_echoq = NULL;
	return;
	}
if (tp->t_echoq == NULL)
	tp->t_echoq = tp->t_rawq.c_cf;	/* point to first one */
while ((q = tp->t_echoq) && q != tp->t_rawq.c_cl)
	{
	if ((q & 07) == 0)
		q = (q-010)->integ + 2;	 /* point to next entry */
	c = *q++ & 0377;
	tp->t_echoq = q;

	if (c == 0377)
		break;
	if (tp->t_flags&ECHO)
		{
		ttyecho(c,tp);		/* actually echo it */
		++startf;
		while (tp->t_outq.c_cc > TTHIWAT)
			{
			ttstart(tp);
			startf = 0;
			tp->t_state =| ASLEEP;
			sleep(&tp->t_outq, TTOPRI);
			}
		}
	}
if (startf)
	ttstart(tp);		/* start echo if needed */
}
#endif
