/*
 * tunable variables
 */

#define	NBUF	14		/* size of buffer cache */
#define	NINODE	50		/* number of in core inodes */
#define	NFILE	50		/* number of in core file structures */
#define	NMOUNT	5		/* number of mountable file systems */
#define	NEXEC	3		/* number of simultaneous exec's */
#define	MAXMEM	(64*32)		/* max core per process - first # is Kw */
#define	SSIZE	20		/* initial stack size (*64 bytes) */
#define	SINCR	20		/* increment of stack (*64 bytes) */
#define	NOFILE	15		/* max open files per process */
#define	CANBSIZ	256		/* max size of typewriter line */
#define	CMAPSIZ	100		/* size of core allocation area */
#define	SMAPSIZ	100		/* size of swap allocation area */
#define	NCALL	20		/* max simultaneous time callouts */
#define	NPROC	25		/* max number of processes */
#define	NTEXT	15		/* max number of pure texts */
#define	NCLIST	200		/* max total clist size */
#define	HZ	60		/* Ticks/second of the clock */
#define	MAXUPRC	25		/* max number of procs per user */

/*
 * priorities
 * probably should not be
 * altered too much
 */

#define	PSWP	-100
#define	PINOD	-90
#define	PRIBIO	-50
#define	PPIPE	1
#define	PWAIT	40
#define	PSLEP	90
#define	PUSER	100

/*
 * signals
 * dont change
 */

#define	NSIG	20
#define		SIGHUP	1	/* hangup */
#define		SIGINT	2	/* interrupt (rubout) */
#define		SIGQIT	3	/* quit (FS) */
#define		SIGINS	4	/* illegal instruction */
#define		SIGTRC	5	/* trace or breakpoint */
#define		SIGIOT	6	/* iot */
#define		SIGEMT	7	/* emt */
#define		SIGFPT	8	/* floating exception */
#define		SIGKIL	9	/* kill */
#define		SIGBUS	10	/* bus error */
#define		SIGSEG	11	/* segmentation violation */
#define		SIGSYS	12	/* sys */
#define		SIGPIPE	13	/* end of pipe */
#define		SIGCLK	14	/* alarm clock */
#define		SIGTERM	15	/* software termination */

#define	SIG_IGN	1
#define	SIG_HOLD 1
/*
 * fundamental constants
 * cannot be changed
 */

#define	USIZE	16		/* size of user block (*64) */
#define	NULL	0
#define	NODEV	(-1)
#define	ROOTINO	1		/* i number of all roots */
#define	DIRSIZ	14		/* max characters per directory */

/*
 * structure to access an
 * integer in bytes
 */
struct
{
	char	lobyte;
	char	hibyte;
};

/*
 * structure to access an integer
 */
struct
{
	int	integ;
};

/*
 * Certain processor registers
 */
#define PS	0177776
#define KL	0177560
#define SW	0177570

/*
 * number of tty's of various types.
 */
#define	NKL11	6
#define	NDL11	0
#define	NDZ11	8
#define	SWAPDELAY	01
#define	BYTEMASK	0377
#define	MAXLINK	127		/* max number of links to a file */
#define	MEM_DELAY	5	/* must be out of memory MEM_DELAY seconds */
#define	SWAP_DELAY	3	/* must be out SWAP_DELAY secs. before swap out */
#define	SLOWSWAP	1	/* if swap device is slow */

#define	TIMEZONE	8
#define	DST	1

/* #define	ERRLOG	1	/* log errors into memory buffer */
#define	IOCTL	1	/* real ioctl code generated */
#define	LOGGING	1	/* code to log tty I/O */
/* #define	SYNCECHO	1	/* sync echo with read */
/* #define	INTIMER	1	/* input delay on CBREAK mode */
