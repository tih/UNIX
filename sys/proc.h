/*
 * One structure allocated per active
 * process. It contains all data needed
 * about the process while the
 * process may be swapped out.
 * Other per process data (user.h)
 * is swapped with the process.
 */
struct	proc
{
	char	p_stat;		/* 0 process status */
	char	p_flag;		/* 1 status flags */
	char	p_pri;		/* 2 priority, negative is high */
	char	p_sig;		/* 3 signal number sent to this process */
	char	p_uid;		/* 4 user id, used to direct tty signals */
	char	p_time;		/* 5 resident time for scheduling */
	int	p_ttyp;		/* 6 controlling tty */
	int	p_pid;		/* 8 unique process id */
	int	p_ppid;		/* 10 process id of parent */
	int	p_addr;		/* 12 address of swappable image */
	int	p_size;		/* 14 size of swappable image (*64 bytes) */
	int	p_wchan;	/* 16 event proecss is awaiting */
	int	*p_textp;	/* 18 pointer to text structure */
	int	p_clktim;	/* 20 time to alarm clock signal */
} proc[NPROC];

struct	
{
	char	p_stat;
	char	p_flag;
	char	p_pri;		/* priority, negative is high */
	char	p_sig;		/* signal number sent to this process */
	char	p_uid;		/* user id, used to direct tty signals */
	char	p_time;		/* resident time for scheduling */
	int	p_ttyp;		/* controlling tty */
	int	p_pid;		/* unique process id */
	int	p_ppid;		/* process id of parent */
	int 	p_cutime[2];	/* saved user time */
	int 	p_cstime[2];	/* saved system time */
	int	p_clktim;	/* time to alarm clock signal */
};

#define	p_r1	p_ttyp
/* stat codes */
#define	SSLEEP	1		/* sleeping on high priority */
#define	SWAIT	2		/* sleeping on low priority */
#define	SRUN	3		/* running */
#define	SIDL	4		/* intermediate state in process creation */
#define	SZOMB	5		/* intermediate state in process termination */
#define	SSTOP	6		/* process being traced */

/* flag codes */
#define	SLOAD	01		/* in core */
#define	SSYS	02		/* scheduling process */
#define	SLOCK	04		/* process cannot be swapped */
#define	SSWAP	010		/* process is being swapped out */
#define	STRC	020		/* process is being traced */
#define	SWTED	040		/* another tracing flag */
