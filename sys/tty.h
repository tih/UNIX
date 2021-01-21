/*
 * A clist structure is the head
 * of a linked list queue of characters.
 * The characters are stored in 4-word
 * blocks containing a link and 6 characters.
 * The routines getc and putc (m45.s or m40.s)
 * manipulate these structures.
 */
struct clist
{
	int	c_cc;		/* character count */
	char	*c_cf;		/* pointer to first block */
	char	*c_cl;		/* pointer to last block */
};

struct tc {
	char	t_intrc;	/* interrupt */
	char	t_quitc;	/* quit */
	char	t_startc;	/* start output */
	char	t_stopc;	/* stop output */
	char	t_eofc;		/* end-of-file */
	char	t_brkc;		/* input delimiter (like nl) */
};

/*
 * A tty structure is needed for
 * each UNIX character device that
 * is used for normal terminal IO.
 * The routines in tty.c handle the
 * common code associated with
 * these structures.
 * The definition and device dependent
 * code is in each driver. (kl.c dc.c dh.c)
 */
struct tty
{
	struct	clist t_rawq;	/* input chars right off device */
	struct	clist t_canq;	/* input chars after erase and kill */
	struct	clist t_outq;	/* output list to device */
	int	t_flags;	/* mode, settable by stty call */
	char	t_erase;	/* erase character */
	char	t_kill;		/* kill character */
	int	t_speeds;	/* output+input line speed */
	int	*t_addr;	/* device address (register or startup fcn) */
	int	t_delct;	/* number of delimiters in raw q */
	char	t_col;		/* printing column of device */
	char	t_char;		/* character temporary */
	int	t_lincnt;	/* number of chars in current input line */
	int	t_state;	/* internal state, not visible externally */
	int	t_dev;		/* device name */
	char	*t_beglin;
#ifdef LOGGING
	char	*t_logf;	/* log file pointer */
	struct tc t_tc;		/* special characters */
#endif
#ifdef SYNCECHO
	char	*t_procp;	/* task last reading tty */
	char	*t_echoq;	/* echo q pointer */
#endif
#ifdef INTIMER
	int	t_indelay;	/* allowable input delay in tics */
	int	t_intimer;	/* current input delay  in tics */
#define	SENDBRK	0100004		/* how to request a delimeter break */
#endif
};

#define	TTWPRI	5		/* wait for open */
#define	TTIPRI	10
#define	TTOPRI	20

#define	CERASE	'#'		/* default special characters */
#define	CEOT	004
#define	CKILL	'@'
#define	CQUIT	034		/* FS, cntl shift L */
#define	CINTR	0177		/* DEL */
#define	CSTARTOP	021	/* control q */
#define	CHALTOP		023	/* control s */

/* limits */
#define	TTHIWAT	100
#define	TTLOWAT	30
#define	TTYHOG	512
#define	TTYFULL	(TTYHOG*2/3)

/* modes */
#define	HUPCL	01
#define	XTABS	02
#define	LCASE	04
#define	ECHO	010
#define	CRMOD	020
#define	RAW	040
#define	ODDP	0100
#define	EVENP	0200
#define	NLDELAY	001400
#define	TBDELAY	006000
#define	CRDELAY	030000
#define	VTDELAY	040000

/* Hardware bits */
#define	DONE	0200
#define	IENABLE	0100

/* Internal state bits */
#define	TIMEOUT	01		/* Delay timeout in progress */
#define	WOPEN	02		/* Waiting for open to complete */
#define	ISOPEN	04		/* Device is open */
#define	SSTART	010		/* Has special start routine at addr */
#define	CARR_ON	020		/* Software copy of carrier-present */
#define	BUSY	040		/* Output in progress */
#define	ASLEEP	0100		/* Wakeup when output done */
#define	OPSUPRS	0200		/* control o in effect */
#define	BKSLF	0400		/* backslash processing  */
#define	HALTOP	01000		/* control s in effect */
#define	HALTSENT 02000		/* have sent CHALTOP for input full */
#define	INSLEEP	04000		/* sleeping on input */
#define	TTYILOG	010000		/* logging input lines */
#define	TTYOLOG	020000		/* logging output lines */
#define	LONGBRK	040000		/* send long break on close */
#define	SECHO 0100000		/* syncronize input/output echo */

#define	TTYLIOSHIFT	12	/* to get to TTYILOG */

int ttysopen;		/* current count of open ttys */

#define	CERASE	'#'		/* default special characters */
#define	CEOT	004
#define	CKILL	'@'
#define	CQUIT	02		/* CTL-B */
#define	CINTR	03		/* CTL-C */
#define	CSTOP	023		/* Stop output: ctl-s */
#define	CSTART	021		/* Start output: ctl-q */
#define	CBRK	0377


/*
 * tty ioctl commands
 */
#define	TIOCGETD	(('t'<<8)|0)
#define	TIOCSETD	(('t'<<8)|1)
#define	TIOCHPCL	(('t'<<8)|2)
#define	TIOCMODG	(('t'<<8)|3)
#define	TIOCMODS	(('t'<<8)|4)
#define	TIOCGETP	(('t'<<8)|8)
#define	TIOCSETP	(('t'<<8)|9)
#define	TIOCSETN	(('t'<<8)|10)
#define	TIOCEXCL	(('t'<<8)|13)
#define	TIOCNXCL	(('t'<<8)|14)
#define	TIOCFLUSH	(('t'<<8)|16)
#define	TIOCSETC	(('t'<<8)|17)
#define	TIOCGETC	(('t'<<8)|18)

#define TIOCSBRK	(('t'<<8)|123)  /* set break bit */
#define TIOCCBRK	(('t'<<8)|122)  /* clear break bit */
#define TIOCITIME	(('t'<<8)|121)  /* set input delay */

#define	TIOCLON		(('l'<<8)|0)
#define	TIOCLOFF	(('l'<<8)|1)
#define	TIOCLGETP	(('l'<<8)|2)
#define	TIOCLSETP	(('l'<<8)|3)
#define	TIOCLPUT	(('l'<<8)|4)
#define	TIOCGETS	(('l'<<8)|5)
#define	TIOCSETS	(('l'<<8)|6)

#define	DIOCLSTN	(('d'<<8)|1)
#define	DIOCNTRL	(('d'<<8)|2)
#define	DIOCMPX		(('d'<<8)|3)
#define	DIOCNMPX	(('d'<<8)|4)
#define	DIOCSCALL	(('d'<<8)|5)
#define	DIOCRCALL	(('d'<<8)|6)
#define	DIOCPGRP	(('d'<<8)|7)
#define	DIOCGETP	(('d'<<8)|8)
#define	DIOCSETP	(('d'<<8)|9)
#define	DIOCLOSE	(('d'<<8)|10)
#define	DIOCTIME	(('d'<<8)|11)
#define	DIOCRESET	(('d'<<8)|12)

#define	FIOCLEX		(('f'<<8)|1)
#define	FIONCLEX	(('f'<<8)|2)
#define	MXLSTN		(('x'<<8)|1)
#define	MXNBLK		(('x'<<8)|2)

/*
 * structure of arg for ioctl
 */
struct	ttiocb {
	char	ioc_ispeed;
	char	ioc_ospeed;
	char	ioc_erase;
	char	ioc_kill;
	int	ioc_flags;
};
#define	RARE	HUPCL		/* use HUPCL bit for now */
