#
/*
 *	Copyright 1973 Bell Telephone Laboratories Inc
 */

/*
 * Everything in this file is a routine implementing a system call.
 */

#include "../param.h"
#include "../user.h"
#include "../reg.h"
#include "../inode.h"
#include "../systm.h"
#include "../proc.h"
#include "../V7.h"

getswit()
{

	u.u_ar0[R0] = sws();
}

gtime()
{

	u.u_ar0[R0] = time[0];
	u.u_ar0[R1] = time[1];
}

stime()
{

	if(suser()) {
		time[0] = u.u_ar0[R0];
		time[1] = u.u_ar0[R1];
		wakeup(tout);
	}
}

setuid()
{
	register uid;

	uid = u.u_ar0[R0].lobyte;
	if(u.u_ruid == uid.lobyte || suser()) {
		u.u_uid = uid;
		u.u_procp->p_uid = uid;
		u.u_ruid = uid;
	}
}

getuid()
{
#ifdef V7CODE
	if (V7)
		{
		u.u_ar0[R0] = u.u_ruid &0377;
		u.u_ar0[R1] = u.u_uid &0377;
		}
	else
#endif
	{
	u.u_ar0[R0].lobyte = u.u_ruid;
	u.u_ar0[R0].hibyte = u.u_uid;
	}
}

setgid()
{
	register gid;

	gid = u.u_ar0[R0].lobyte;
	if(u.u_rgid == gid.lobyte || suser()) {
		u.u_gid = gid;
		u.u_rgid = gid;
	}
}

getgid()
{
#ifdef V7CODE
	if (V7)
		{
		u.u_ar0[R0] = u.u_rgid &0377;
		u.u_ar0[R1] = u.u_gid &0377;
		}
	else
#endif
	{
	u.u_ar0[R0].lobyte = u.u_rgid;
	u.u_ar0[R0].hibyte = u.u_gid;
	}
}

getpid()
{
	u.u_ar0[R0] = u.u_procp->p_pid;
}

sync()
{

	update();
}

nice()
{
	register n, i;

	i = u.u_nice;			/* previous priority */
	n = u.u_ar0[R0];
	if(n < 0 && !suser())
		n = 0;
	n =+ u.u_nice;		/* increment priority */
	if(n > 20)
		n = 20;
	u.u_nice = n;
	u.u_ar0[R0] = i;
}

/*
 * Unlink system call.
 * panic: unlink -- "cannot happen"
 */
unlink()
{
	register *ip, *pp;
	extern uchar;

	pp = namei(&uchar, 2);
	if(pp == NULL)
		return;
	prele(pp);
	ip = iget(pp->i_dev, u.u_dent.u_ino);
	if(ip == NULL)
		panic("unlink -- iget");
	if((ip->i_mode&IFMT)==IFDIR && !suser())
		goto out;
	u.u_offset[1] =- DIRSIZ+2;
	u.u_base = &u.u_dent;
	u.u_count = DIRSIZ+2;
	u.u_dent.u_ino = 0;
	writei(pp);
	if (ip->i_nlink)
		ip->i_nlink--;
	ip->i_flag =| IUPD;

out:
	iput(pp);
	iput(ip);
}

chdir()
{
	register *ip;
	extern uchar;

	ip = namei(&uchar, 0);
	if(ip == NULL)
		return;
	if((ip->i_mode&IFMT) != IFDIR) {
		u.u_error = ENOTDIR;
	bad:
		iput(ip);
		return;
	}
	if(access(ip, IEXEC))
		goto bad;
	iput(u.u_cdir);
	u.u_cdir = ip;
	prele(ip);
}

chmod()
{
	register *ip;

	if ((ip = owner()) == NULL)
		return;
	ip->i_mode =& ~07777;
	if (u.u_uid)
		u.u_arg[1] =& ~ISVTX;
	ip->i_mode =| u.u_arg[1]&07777;
	ip->i_flag =| IUPD;
	iput(ip);
}

chown()
{
	register *ip;

	if (!suser() || (ip = owner()) == NULL)
		return;
	ip->i_uid = u.u_arg[1].lobyte;
#ifdef V7CODE
	if (V7)
		ip->i_gid = u.u_arg[2].lobyte;
	else
#endif
		ip->i_gid = u.u_arg[1].hibyte;
	ip->i_flag =| IUPD;
	iput(ip);
}

/*
 * Change modified date of file:
 * time to r0-r1; sys smdate; file
 * sys utime; file; datep
 * This call has been withdrawn because it messes up
 * incremental dumps (pseudo-old files aren't dumped).
 * It works though and you can uncomment it if you like.
*/

smdate()
{
	register struct inode *ip;
	register int *tp;
	int tbuf[4];	/* access time, mod time */

	if ((ip = owner()) == NULL)
		return;
	tp = tbuf;
#ifdef V7CODE
	if (V7)
		{
		ip->i_flag =| IUPD | IACC;
		*tp++ = fuaword();
		*tp++ = fuaword();
		*tp++ = fuaword();
		*tp++ = fuaword();
		}
	else
#endif
		{
		ip->i_flag =| IUPD;
		*tp++ = time[0];
		*tp++ = time[1];
		*tp++ = u.u_ar0[R0];
		*tp++ = u.u_ar0[R1];
		}
	iupdat(ip, tbuf, tbuf+2);
	ip->i_flag =& ~(IUPD|IACC);
	iput(ip);
}

ssig()
{
	register a;

	a = u.u_arg[0];
	if(a<=0 || a>=NSIG) {
		u.u_error = EINVAL;
		return;
	}
	u.u_ar0[R0] = u.u_signal[a];
	u.u_signal[a] = u.u_arg[1];
	u.u_signal[9] = 0;		/* kill not allowed */
	if(u.u_procp->p_sig == a && u.u_ar0[R0] != SIG_HOLD)
		u.u_procp->p_sig = 0;
}

kill()
{
	register struct proc *p, *q;
	register a;
	int f;

	f = 0;
	a = u.u_ar0[R0];
	q = u.u_procp;
	for(p = &proc[0]; p < &proc[NPROC]; p++) {
		if(p == q)
			continue;
		if(a > 0 && p->p_pid != a)
			continue;	/* not specified process */
		if(a == 0 && (p->p_ttyp != q->p_ttyp || p <= &proc[1]))
			continue;
		if(a == -1 && (u.u_uid != 0 || p <= &proc[1]))
			continue;
		if(u.u_uid != 0 && u.u_uid != p->p_uid)
			continue;
		f++;
		psignal(p, u.u_arg[0]);
	}
	if(f == 0)
		u.u_error = ESRCH;
}

times()
{
	register int i;
	register int n;

#ifdef V7CODE
	if (V7)
		{
		suaword(u.u_utime[0]);
		suaword(u.u_utime[1]);
		suaword(0);
		suaword(u.u_stime);
		suaword(u.u_cutime[0]);
		suaword(u.u_cutime[1]);
		suaword(u.u_cstime[0]);
		suaword(u.u_cstime[1]);
		}
	else
#endif
		{
		for (i=0; i<6; ++i)
			{
			switch(i)
				{
			case 0:
				n = u.u_utime[1];
				break;
			case 1:
				n = u.u_stime;
				break;
			default:
				n = u.u_utime[i];
				break;
				}
			suword(u.u_arg[0], n);
			u.u_arg[0] =+ 2;
			}
		}
}

suaword(n)
{
/*
 * store "n" into user area thru arg[0].
 */
suword(u.u_arg[0],n);
u.u_arg[0] =+ 2;
}

fuaword()
{
/*
 * fetch the next word pointed to by arg1 and then update arg1.
 */
register int i;

i = fuword(u.u_arg[1]);
u.u_arg[1] =+ 2;
}


profil()
{

	u.u_prof[0] = u.u_arg[0] & ~1;	/* base of sample buf */
	u.u_prof[1] = u.u_arg[1];	/* size of same */
	u.u_prof[2] = u.u_arg[2];	/* pc offset */
	u.u_prof[3] = (u.u_arg[3]>>1) & 077777; /* pc scale */
}


/*
 * allow for user program to use trap instruction by setting
 * a flag (u.u_trap) to non-zero and that word in user space
 * to non-zero. a sigsys will result when the user program 
 * issues a trap instruction. 
 */

systrap()
{

	u.u_trap = u.u_arg[0];

}

/*
 * indefinite wait.
 * no-one should wakeup(&u)
 */
pause()
{
	for (;;)
		sleep(&u,PSLEP);
}

/*
 * alarm clock signal.
 */
alarm()
{
	register c, *p;

	p = u.u_procp;
	c = p->p_clktim;
	p->p_clktim = u.u_ar0[R0];
	u.u_ar0[R0] = c;
}

/*
 * umask() set file access mask
 */
umask()
{
	register int i;

	i = u.u_mask;
	u.u_mask = u.u_arg[0] & 0777;
	u.u_ar0[R0] = i;
}

#ifdef V7CODE

struct timeb
{
int time[2];
int millitm;
int timezone;
int dstflag;
} timeb {
0,0,
0,
TIMEZONE*60,
DST };

ftime()
{
	register int *p, *q, i;

	p = u.u_arg[0];
	timeb.time[0] = time[0];
	timeb.time[1] = time[1];
	timeb.millitm = lbolt * 100 / (HZ/10);
	for (q = &timeb, i=0; i < (sizeof timeb)/2; ++i)
		suword(p++,*q++);
	
}
#endif
