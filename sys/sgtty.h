
struct tc {
	char	t_intrc;	/* interrupt */
	char	t_quitc;	/* quit */
	char	t_startc;	/* start output */
	char	t_stopc;	/* stop output */
	char	t_eofc;		/* end-of-file */
	char	t_brkc;		/* input delimiter (like nl) */
};


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


#define	CERASE	'#'		/* default special characters */
#define	CEOT	004
#define	CKILL	'@'
#define	CQUIT	034		/* FS, cntl shift L */
#define	CINTR	0177		/* DEL */
#define	CSTOP	023		/* Stop output: ctl-s */
#define	CSTART	021		/* Start output: ctl-q */
#define	CBRK	0377


/* modes */
#define	TANDEM	01
#define	CBREAK	02
#define	LCASE	04
#define	ECHO	010
#define	CRMOD	020
#define	RAW	040
#define	ODDP	0100
#define	EVENP	0200
#define	NLDELAY	001400
#define	TBDELAY	006000
#define	XTABS	006000
#define	CRDELAY	030000
#define	VTDELAY	040000



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

#define	TIOCLON		(('l'<<8)|0)
#define	TIOCLOFF	(('l'<<8)|1)
#define	TIOCLGETP	(('l'<<8)|2)
#define	TIOCLSETP	(('l'<<8)|3)
#define	TIOCLPUT	(('l'<<8)|4)

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
