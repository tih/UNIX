#
/*	new tty input routine
 *
 *		march 75		bsb
 */

#include "../tty.h"
#include "../param.h"
#define	PS	0177776
#define	ttmode	0	/* only 1 mode possible */
#define	ALTESC	0
#define	RARE	HUPCL		/* use HUPCL bit for now */

struct { int integ; };

struct cblock {
	struct cblock *c_next;
	char info[6];
};

struct cblock *cfreelist;
int	cblkuse, maxcbu;

/* modes for all the known chars */

#define	NORM_MD	0	/* normal everyday char */
#define	UCAS_MD	1	/* upper case normal char */
#define	CTRL_MD	2	/* non - break ctrl char */
#define	RUB_MD	3	/* rubout or ^a */
#define	ERAS_MD	4	/* ^u */
#define	RTYP_MD	5	/* ^r */
#define	INT_MD	6	/* ^c */
#define	QUIT_MD	7	/* ^b */
#define	NULL_MD	8	/* <break> */
#define	CR_MD	9	/* <cr> */
#define	LF_MD	10	/* <lf> */
#define	EOT_MD	11	/* ^d */
#define	BELL_MD	12	/* ^g */
#define	SINK_MD	13	/* ^o */
#define	ESC_MD	14	/* <esc> */
#define	FF_MD	15	/* <ff> ^l */
#define	UPRO_MD	16	/* ^ */
#define	ALT_MD	17	/* <alt> 0175 */
#define	BS_MD	18	/* <bs> 010 */

/**/

char	chrare[] {	/* all set to CTRL_MD except \r and \n. */

CTRL_MD,	CTRL_MD,	CTRL_MD,	CTRL_MD,
CTRL_MD,	CTRL_MD,	CTRL_MD,	CTRL_MD,
CTRL_MD, 	CTRL_MD,	LF_MD,  	CTRL_MD,
CTRL_MD,  	CR_MD,  	CTRL_MD,	SINK_MD,
CTRL_MD,	CTRL_MD,	CTRL_MD,	CTRL_MD,
CTRL_MD,	CTRL_MD,	CTRL_MD,	CTRL_MD,
CTRL_MD,	CTRL_MD,	CTRL_MD,	CTRL_MD,
CTRL_MD,	CTRL_MD,	CTRL_MD,	CTRL_MD };

char	chtype[] {

NULL_MD,	RUB_MD ,	QUIT_MD,	INT_MD,
EOT_MD, 	CTRL_MD,	CTRL_MD,	BELL_MD,
BS_MD,	 	NORM_MD,	LF_MD,  	CTRL_MD,
FF_MD,  	CR_MD,  	CTRL_MD,	SINK_MD,
CTRL_MD,	0,		RTYP_MD,	0,
CTRL_MD,	ERAS_MD,	CTRL_MD,	CTRL_MD,
CTRL_MD,	CTRL_MD,	CTRL_MD,	ESC_MD,
CTRL_MD,	CTRL_MD,	CTRL_MD,	CTRL_MD,
NORM_MD,	NORM_MD,	NORM_MD,	NORM_MD,
NORM_MD,	NORM_MD,	NORM_MD,	NORM_MD,
NORM_MD,	NORM_MD,	NORM_MD,	NORM_MD,
NORM_MD,	NORM_MD,	NORM_MD,	NORM_MD,
NORM_MD,	NORM_MD,	NORM_MD,	NORM_MD,
NORM_MD,	NORM_MD,	NORM_MD,	NORM_MD,
NORM_MD,	NORM_MD,	NORM_MD,	NORM_MD,
NORM_MD,	NORM_MD,	NORM_MD,	NORM_MD,
NORM_MD,	UCAS_MD,	UCAS_MD,	UCAS_MD,
UCAS_MD,	UCAS_MD,	UCAS_MD,	UCAS_MD,
UCAS_MD,	UCAS_MD,	UCAS_MD,	UCAS_MD,
UCAS_MD,	UCAS_MD,	UCAS_MD,	UCAS_MD,
UCAS_MD,	UCAS_MD,	UCAS_MD,	UCAS_MD,
UCAS_MD,	UCAS_MD,	UCAS_MD,	UCAS_MD,
UCAS_MD,	UCAS_MD,	UCAS_MD,	NORM_MD,
NORM_MD,	NORM_MD,	UPRO_MD,	NORM_MD,
NORM_MD,	NORM_MD,	NORM_MD,	NORM_MD,
NORM_MD,	NORM_MD,	NORM_MD,	NORM_MD,
NORM_MD,	NORM_MD,	NORM_MD,	NORM_MD,
NORM_MD,	NORM_MD,	NORM_MD,	NORM_MD,
NORM_MD,	NORM_MD,	NORM_MD,	NORM_MD,
NORM_MD,	NORM_MD,	NORM_MD,	NORM_MD,
NORM_MD,	NORM_MD,	NORM_MD,	NORM_MD,
NORM_MD,	ALT_MD, 	NORM_MD,	RUB_MD

	};
/**/

/* and now for the functional codes which drive this
 * convoluted touring machine
 */

#define	DONE_FN	0	/* chain end */
#define	ECHO_FN	1	/* echo char as seen */
#define	STOR_FN	2	/* store char in input buffer */
#define	BRK_FN	3	/* store brk in input buffer (0377) */
#define	CTRL_FN	4	/* echo <^> <char + '@'> */
#define	CTRS_FN	5	/* echo <^> <char + '@'> <nl> */
#define	RUB_FN	6	/* rubout or ^a */
#define	ERAS_FN	7	/* ^u */
#define	SINK_FN	8	/* ^o */
#define	RTYP_FN	9	/* ^r */
#define	INT_FN	10	/* ^c */
#define	QUIT_FN	11	/* ^b */
#define	NULL_FN	12	/* break */
#define	FF_FN	13	/* <ff> */
#define	CR_FN	14	/* <cr> */
#define	SECH_FN	15	/* echo nxt chain entry */
#define	BELL_FN	16	/* <bell> ^g */
#define	ESC_FN	17	/* <esc> */
#define	UCAS_FN	18	/* upper case conversion to lower maybe */
#define	ALT_FN	19	/* <alt> 0175 */
#define	BKSL_FN	20	/* initial \ checker */
#define	BKSC_FN	21	/* clear backslash */
#define	UPRO_FN	22	/* ^ */
#define	BS_FN	23	/* backspace */
#define	FULL_FN	24	/* 8 bit code echo */

/**/

/* chain names are "cn" followed by the mode to which they
 *  apply. extras for various functions (based on differences
 *  due to tty mode) are named mode followed by pp, dd, te for
 *  the applicable mode.
 */

char	cnnorm[] { BKSL_FN, ECHO_FN, STOR_FN, DONE_FN };
char	cnucas[] { BKSL_FN, UCAS_FN, ECHO_FN, STOR_FN, DONE_FN };
char	cnctrl[] { BKSL_FN, CTRL_FN, STOR_FN, DONE_FN };
char	cnrub[]  { RUB_FN };
char	cneras[] { BKSC_FN, CTRS_FN, ERAS_FN };
char	cnrtyp[] { BKSC_FN, CTRS_FN, RTYP_FN };
char	cnint[]  { BKSC_FN, INT_FN, CTRS_FN, DONE_FN };
char	cnquit[] { BKSC_FN, QUIT_FN, CTRS_FN, DONE_FN };
char	cnnull[] { NULL_FN };
char	cncr[]   { CR_FN };
char	cnlf[]   { BKSL_FN, ECHO_FN, STOR_FN, BRK_FN };
char	cneot[]  { BKSL_FN, BRK_FN };
char	cnbell[] { BKSL_FN, BELL_FN };
char	cnsink[] { CTRL_FN, SINK_FN };
char	cnesc[]  { BKSL_FN, ESC_FN };
char	cnff[]   { BKSL_FN, FF_FN };
char	cnupro[] { UPRO_FN };
char	cnalt[]  { BKSL_FN, ALT_FN };
char	cnbs[]	{ BS_FN, ECHO_FN, SECH_FN, ' ', ECHO_FN, ECHO_FN, SECH_FN, ' ', ECHO_FN};

/* use these chains when an esc (033) is seen */

char	escno[]  { SECH_FN, '$', STOR_FN, DONE_FN };
char	escpp[]  { SECH_FN, '$', STOR_FN, BRK_FN };
#define	escte	escpp
char	escdd[]  { STOR_FN, BRK_FN };

/* use these chains when an up arrow (^) is seen */

#define	uprono	cnnorm
#define	upropp	cnnorm
#define	uprote	cnlf
#define	uprodd	cnlf

/* use these chains when a bell <bell> (^g) is seen */

char	bellno[] { CTRL_FN, ECHO_FN, STOR_FN, DONE_FN };
char	bellpp[] { CTRL_FN, ECHO_FN, STOR_FN, BRK_FN };
#define	bellte	bellpp
#define	belldd	bellpp

/* now for special chains for whatever needs... */

char	rawchain[] { ECHO_FN, STOR_FN, BRK_FN };
char	rarechain[] { FULL_FN, STOR_FN, DONE_FN };
char	sinkex[] { CTRL_FN, DONE_FN };
/**/

/* here is the main dispatch table for chains based
 *  upon mode. second level dispatch is performed by the
 *  appropriate functions where necessary.
 */

int	nochain[] {

	cnnorm,
	cnucas,
	cnctrl,
	cnrub,
	cneras,
	cnrtyp,
	cnint,
	cnquit,
	cnnull,
	cncr,
	cnlf,
	cneot,
	cnbell,
	cnsink,
	cnesc,
	cnff,
	cnupro,
	cnalt,
	cnbs
};

int	escchain[] {

	escno,
	escpp,
	escte,
	escdd
};

int	uprochain[] {

	uprono,
	upropp,
	uprote,
	uprodd
};

int	bellchain[] {

	bellno,
	bellpp,
	bellte,
	belldd
};
/**/

#define	NEXT	goto swt

/* and finally to the actual job of walking through
 * the maze. this is the tty input function driven by the
 * above touring machine functions.
 */

ttyinput(ac, tp)
struct tty *tp;
{
	register char *md;
	register int c, t_flags;
	int ch;
	int startf;

	startf = 0;
	t_flags = tp->t_flags;
	c = ac;
	c =& 0177;
	if (c == CHALTOP)
		{
		if (tp->t_state&HALTOP)
			return;
		tp->t_state =| HALTOP;
			ttstop(tp);
			return;
		}
	if (c == CSTARTOP)
		{
		if ((tp->t_state&HALTOP) == 0)
			return;
		tp->t_state =& ~HALTOP;
		goto donestart;
		}
	if(t_flags&RAW) {
		if ((t_flags & (EVENP|ODDP)) == 0)
			c = ac;			/* use all 8 bits */
		tp->t_state =& ~(BKSLF);
		md = rawchain;
		if(t_flags&LCASE && (c =& 0177) >= 'A' && c <= 'Z')
			c =+ 'a' - 'A';
		NEXT;
	}
	if ((t_flags&RARE))
		{
		if ((ac&0200) && (t_flags & (EVENP|ODDP)) == 0)
			{
			if (ac != 0377)
				c = ac;		/* use all 8 bits */
			md = rarechain;
			}
		else if (c < sizeof chrare)
			md = nochain[chrare[c]];
		else
			md = nochain[chtype[c]];
		}
	else
		md = nochain[chtype[c]];

swt:	switch(*md++) {

	case DONE_FN:
done:			if(startf)
donestart:			ttstart(tp);
			return;

	case ECHO_FN:	if(t_flags&ECHO) {
				ttyoutput(c, tp);
				startf++;
			}
			NEXT;

	case STOR_FN:	if(tp->t_rawq.c_cc > TTYHOG) {
				if(t_flags&ECHO) ttyoutput(07, tp);	/* bell */
				startf++;
				NEXT;
			}
			if (tp->t_rawq.c_cc == TTYFULL && (t_flags&ECHO) == 0 &&
					(tp->t_state&HALTSENT) == 0)
				{
				tp->t_state =| HALTSENT;
				ttyoutput(CHALTOP,tp);		/* send halt */
				++startf;
				}
			if(putc(c, &tp->t_rawq) == 0) {
				if(tp->t_beglin == 0)
					tp->t_beglin = tp->t_rawq.c_cl-1;
				tp->t_lincnt++;
			}
			NEXT;

	case BRK_FN:	md = tp;
			if(putc(0377, &md->t_rawq) == 0) {
				md->t_delct++;
				md->t_lincnt = 0;
				md->t_beglin = 0;
				if (tp->t_state & INSLEEP)
					{
					wakeup(&md->t_rawq);
					tp->t_state =& ~INSLEEP;
					}
			} else {
				--md->t_lincnt;
				if((c = reducq(&md->t_rawq)) < 0 || c == 0377)
					md->t_beglin = 0;
				return;
			}
			goto donestart;

	case FULL_FN: if(t_flags&ECHO) {
				ttyoutput('\\', tp);
				ttyoutput(((c >> 6) &07) + '0', tp);
				ttyoutput(((c >> 3) &07) + '0', tp);
				ttyoutput((c&07) + '0',tp);
				startf++;
			}
			NEXT;

	case CTRL_FN:	if(t_flags&ECHO) {
				ttyoutput('^', tp);
				ttyoutput(c + '@', tp);
				startf++;
			}
			NEXT;

	case CTRS_FN:	if(t_flags&ECHO) {
				ttyoutput('^', tp);
				ttyoutput(c + '@', tp);
				if((t_flags&CRMOD) == 0)
					ttyoutput('\r', tp);
				ttyoutput('\n', tp);
				startf++;
			}
			NEXT;

	case RUB_FN:	if((tp->t_state&BKSLF) == 0) {
				if(t_flags&ECHO) {
					ttyoutput('\\', tp);
					startf++;
				}
				tp->t_state =| BKSLF;
			}
			if(tp->t_beglin == 0) {
				goto done;
			} else {
				if(t_flags&ECHO) ttyoutput(*(tp->t_rawq.c_cl-1), tp);
				reducq(&tp->t_rawq);
				if(--tp->t_lincnt == 0)
					tp->t_beglin = 0;
				goto donestart;
			}

	case BS_FN:
			if(tp->t_beglin == 0)
				return;
			if(tp->t_state & BKSLF) {
				md = cnrub;
				NEXT;
			}
			ch = *(tp->t_rawq.c_cl-1);
			reducq(&tp->t_rawq);
			if (chtype[ch] != CTRL_MD)
				md =+ 4;		/* ignore unless ^ */
			if(--tp->t_lincnt == 0)
				tp->t_beglin = 0;
			NEXT;

	case ERAS_FN:
			md = tp;
			if(c = md->t_beglin) {
				md->t_beglin = 0;
				md->t_rawq.c_cc =- md->t_lincnt;
				md->t_lincnt = 0;
				t_flags = (c-1) & ~07;
				if (md->t_rawq.c_cf == c) {
					md->t_rawq.c_cf = 0;
					md->t_rawq.c_cl = 0;
					md = t_flags;
				} else {
					md->t_rawq.c_cl = c;
					md = t_flags->c_next;
				}
				if (md) {
					c = PS->integ;
					spl6();
					do {
						t_flags = md->c_next;
						md->c_next = cfreelist;
						cfreelist = md;
						cblkuse--;
					} while(md = t_flags);
					PS->integ = c;
				}
			}
			goto done;

	case SINK_FN:	md = tp;
			if(md->t_state =^ OPSUPRS) {
				ttstop(md);
/*				md->t_nrem = 0;		*/
				while(getc(&md->t_outq) >= 0);
			}
			md = sinkex;
			NEXT;

	case RTYP_FN:
			md = tp;
			if(t_flags = md->t_beglin) {
				c = md->t_lincnt;
				md->t_rawq.c_cl = t_flags;
				for(;;) {
					/* echo even if echo-flag is off */
					ttyoutput(*(md->t_rawq.c_cl++), md);
					if(--c == 0) break;
					if((md->t_rawq.c_cl&07) == 0)
		md->t_rawq.c_cl = ((md->t_rawq.c_cl-010)->integ)+2;
				}
			}
			goto donestart;

	case NULL_FN:	
	case INT_FN:	flushtty(tp);
			signal(tp, SIGINT);
			NEXT;

	case QUIT_FN:	flushtty(tp);
			signal(tp, SIGQIT);
			NEXT;


	case FF_FN:	md = cnnorm;
			NEXT;

	case CR_FN:
			if(t_flags&CRMOD) {
				c = '\n';
				md = cnlf;
			} else
				md = cnnorm;
			NEXT;

	case SECH_FN:	if(t_flags&ECHO) ttyoutput(*md, tp);
			md++;
			startf++;
			NEXT;

	case BELL_FN:	md = bellchain[ttmode];
			NEXT;

	case ESC_FN:
escfn:			md = escchain[ttmode];
			NEXT;

	case UCAS_FN:	if(t_flags&LCASE) c =+ 'a' - 'A';
			NEXT;

	case ALT_FN:	if(t_flags&ALTESC) {
				c = 033;
				goto escfn;
			}
			md = cnnorm;
			NEXT;

	case BKSL_FN:	if(tp->t_state&BKSLF) {
				if(t_flags&ECHO) {
					ttyoutput('\\', tp);
					startf++;
				}
				tp->t_state =^ BKSLF;
			}
			NEXT;

	case BKSC_FN:	tp->t_state =& ~BKSLF;
			NEXT;

	case UPRO_FN:	md = uprochain[ttmode];
			NEXT;

	}
}
