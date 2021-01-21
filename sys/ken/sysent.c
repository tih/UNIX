#include "../param.h"
#include "../V7.h"

#ifdef V7CODE
#define	exece	exec
#endif

#ifndef V7CODE
#define	ftime	nosys
#define	exece	nosys
#define	ioctl	nosys
#define	faccess	nosys
#endif

/*
 *	Copyright 1973 Bell Telephone Laboratories Inc
 */

/*
 * This table is the switch used to transfer
 * to the appropriate routine for processing a system call.
 * Each row contains the number of arguments expected
 * and a pointer to the routine.
 */
int	sysent[]
{
	0, &nullsys,			/*  0 = indir */
	0, &rexit,			/*  1 = exit */
	0, &fork,			/*  2 = fork */
	2, &read,			/*  3 = read */
	2, &write,			/*  4 = write */
	2, &open,			/*  5 = open */
	0, &close,			/*  6 = close */
	0, &wait,			/*  7 = wait */
	2, &creat,			/*  8 = creat */
	2, &link,			/*  9 = link */
	1, &unlink,			/* 10 = unlink */
	2, &exec,			/* 11 = exec */
	1, &chdir,			/* 12 = chdir */
	0, &gtime,			/* 13 = time */
	3, &mknod,			/* 14 = mknod */
	2, &chmod,			/* 15 = chmod */
	2, &chown,			/* 16 = chown */
	1, &sbreak,			/* 17 = break */
	2, &stat,			/* 18 = stat */
	2, &seek,			/* 19 = seek */
	0, &getpid,			/* 20 = getpid */
	3, &smount,			/* 21 = mount */
	1, &sumount,			/* 22 = umount */
	0, &setuid,			/* 23 = setuid */
	0, &getuid,			/* 24 = getuid */
	0, &stime,			/* 25 = stime */
	3, &ptrace,			/* 26 = ptrace */
	0, &alarm,			/* 27 = alarm */
	1, &fstat,			/* 28 = fstat */
	0, &pause,			/* 29 = pause */
	1, &smdate,			/* 30 = smdate; inoperative */
	1, &stty,			/* 31 = stty */
	1, &gtty,			/* 32 = gtty */
	2, &faccess,			/* 33 = access */
	0, &nice,			/* 34 = nice */
	0, &sslep,			/* 35 = sleep */
	0, &sync,			/* 36 = sync */
	1, &kill,			/* 37 = kill */
	0, &getswit,			/* 38 = switch */
	0, &nosys,			/* 39 = x */
	0, &nosys,			/* 40 = x */
	0, &dup,			/* 41 = dup */
	0, &pipe,			/* 42 = pipe */
	1, &times,			/* 43 = times */
	4, &profil,			/* 44 = prof */
	1, &ftime,			/* 45 = tiu */
	0, &setgid,			/* 46 = setgid */
	0, &getgid,			/* 47 = getgid */
	2, &ssig,			/* 48 = sig */
	0, &nosys,			/* 49 = x */
	0, &nosys,			/* 50 = x */
	0, &nosys,			/* 51 = x */
	0, &nosys,			/* 52 = x */
	0, &nosys,			/* 53 = x */
	3, &ioctl,			/* 54 = ioctl */
	0, &nosys,			/* 55 = x */
	0, &nosys,			/* 56 = x */
	0, &nosys,			/* 57 = x */
	0, &nosys,			/* 58 = x */
	3, &exece,			/* 59 = exece */
	1, &umask,			/* 60 = umask */
	2, &smdate,			/* 61 = utime */
	3, &lseek,			/* 62 = lseek */
	1, &systrap,			/* 63 = systrap */
	0, &nosys			/* 64 = bad system call */
};

#ifdef V7CODE

char v7map[64]
{
 0, /*  0 = indir */
 1, /*  1 = exit */
 2, /*  2 = fork */
 3, /*  3 = read */
 4, /*  4 = write */
 5, /*  5 = open */
 6, /*  6 = close */
 7, /*  7 = wait */
 8, /*  8 = creat */
 9, /*  9 = link */
10, /* 10 = unlink */
11, /* 11 = exec */
12, /* 12 = chdir */
13, /* 13 = time */
14, /* 14 = mknod */
15, /* 15 = chmod */
16, /* 16 = chown */
17, /* 17 = break */
18, /* 18 = stat */
62, /* 19 = seek ==> lseek */
20, /* 20 = getpid */
21, /* 21 = mount */
22, /* 22 = umount */
23, /* 23 = setuid */
24, /* 24 = getuid */
25, /* 25 = stime */
26, /* 26 = ptrace */
27, /* 27 = alarm */
28, /* 28 = fstat */
29, /* 29 = pause */
61, /* 30 = utime */
31, /* 31 = stty */
32, /* 32 = gtty */
33, /* 33 = access */
34, /* 34 = nice */
45, /* 35 = ftime */
36, /* 36 = sync */
37, /* 37 = kill */
38, /* 38 = switch */
39, /* 39 = x */
40, /* 40 = x */
41, /* 41 = dup */
42, /* 42 = pipe */
43, /* 43 = times */
44, /* 44 = prof */
45, /* 45 = tiu */
46, /* 46 = setgid */
47, /* 47 = getgid */
48, /* 48 = sig */
49, /* 49 = x */
50, /* 50 = x */
64, /* 51 = acct */
64, /* 52 = phys */
53, /* 53 = lock */
54, /* 54 = ioctl */
55, /* 55 = x */
56, /* 56 = x */
57, /* 57 = x */
58, /* 58 = x */
59, /* 59 = exece */
60, /* 60 = umask */
64, /* 61 = chroot */
62, /* 62 = lseek */
63, /* 63 = systrap */
};

#endif /* V7CODE */
