#
/*
 *	Copyright 1973 Bell Telephone Laboratories Inc
 */

#include "../param.h"
#include "../systm.h"
#include "../user.h"
#include "../proc.h"
#include "../buf.h"
#include "../reg.h"
#include "../inode.h"
#include "../stat.h"
#include "../V7.h"

/*
 * exec system call.
 * Because of the fact that an I/O buffer is used
 * to store the caller's arguments during exec,
 * and more buffers are needed to read in the text file,
 * deadly embraces waiting for free buffers are possible.
 * Therefore the number of processes simultaneously
 * running in exec has to be limited to NEXEC.
 */
#define EXPRI	-1

exec()
{
	int ap, na, nc, *bp;
	int ts, ds, sep;
	register c, *ip;
	register char *cp;
	extern uchar;
	int hdr[8];		/* the program header */

	/*
	 * pick up file names
	 * and check various modes
	 * for execute permission
	 */

	ip = namei(&uchar, 0);
	if(ip == NULL)
		return;
	while(execnt >= NEXEC)
		sleep(&execnt, EXPRI);
	execnt++;
	bp = getblk(NODEV);
	if(access(ip, IEXEC) || (ip->i_mode&IFMT)!=0)
		goto bad;

	/*
	 * pack up arguments into
	 * allocated disk buffer
	 */

	cp = bp->b_addr;
	na = 0;
	nc = 0;
	while(ap = fuword(u.u_arg[1])) {
		na++;
		if(ap == -1)
			goto bad;
		u.u_arg[1] =+ 2;
		for(;;) {
			c = fubyte(ap++);
			if(c == -1)
				goto bad;
			*cp++ = c;
			nc++;
			if(nc > 510) {
				u.u_error = E2BIG;
				goto bad;
			}
			if(c == 0)
				break;
		}
	}
	if((nc&1) != 0) {
		*cp++ = 0;
		nc++;
	}

	/*
	 * read in first 8 bytes
	 * of file for segment
	 * sizes:
	 * w0 = 407/410/411 (410 implies RO text) (411 implies sep ID)
	 * w1 = text size
	 * w2 = data size
	 * w3 = bss size
	 */

	u.u_base = &hdr[0];
	u.u_count = 16;
	u.u_offset[1] = 0;
	u.u_offset[0] = 0;
	u.u_segflg = 1;
	readi(ip);
	u.u_segflg = 0;
	if(u.u_error)
		goto bad;
	for (c=0; c<4; ++c)
		u.u_arg[c] = hdr[c];	/* copy into u.u_arg ... we have to */
	sep = 0;
	if(u.u_arg[0] == 0407) {
		u.u_arg[2] =+ u.u_arg[1];
		u.u_arg[1] = 0;
	} else
	if(u.u_arg[0] == 0411)
		sep++; else
	if(u.u_arg[0] != 0410) {
		u.u_error = ENOEXEC;
		goto bad;
	}
	if(u.u_arg[1]!=0 && (ip->i_flag&ITEXT)==0 && ip->i_count!=1) {
		u.u_error = ETXTBSY;
		goto bad;
	}

	/*
	 * find text and data sizes
	 * try them out for possible
	 * exceed of max sizes
	 */

	ts = ((u.u_arg[1]+63)>>6) & 01777;
	ds = ((u.u_arg[2]+u.u_arg[3]+63)>>6) & 01777;
	if(estabur(ts, ds, SSIZE, sep))
		goto bad;

	/*
	 * allocate and clear core
	 * at this point, committed
	 * to the new image
	 */

	u.u_prof[3] = 0;
	xfree();
	expand(USIZE);
	xalloc(ip);
	c = USIZE+ds+SSIZE;
	expand(c);
	while(--c >= USIZE)
		clearseg(u.u_procp->p_addr+c);

	/*
	 * read in data segment
	 */

	estabur(0, ds, 0, 0);
	u.u_base = 0;
	u.u_offset[1] = 020+u.u_arg[1];
	u.u_count = u.u_arg[2];
	readi(ip);

	/*
	 * initialize stack segment
	 */

	u.u_tsize = ts;
	u.u_dsize = ds;
	u.u_ssize = SSIZE;
	u.u_sep = sep;
	estabur(u.u_tsize, u.u_dsize, u.u_ssize, u.u_sep);
	/*
	 * copy parameter list
	 */
	cp = bp->b_addr;
	ap = -nc - na*2 - 4;
#ifdef V7CODE
	u.u_system = hdr[7] >> 8;	/* set version number */
	if (V7)
		ap =- 2;	/* leave room for extra zero flag */
#endif
	u.u_ar0[R6] = ap;
	suword(ap, na);
	c = -nc;
	while(na--) {
		suword(ap=+2, c);
		do
			subyte(c++, *cp);
		while(*cp++);
	}
#ifdef V7CODE
	if (V7) {
		suword(ap+2, 0);		/* mark end of parameter list */
		suword(ap+4, 0);		/* mark end of parameter list */
	}
	else
#endif
		suword(ap+2, -1);		/* mark end of parameter list */

	/*
	 * set SUID/SGID protections, if no tracing
	 */

	if ((u.u_procp->p_flag&STRC)==0) {
		if(ip->i_mode&ISUID)
			if(u.u_uid != 0) {
				u.u_uid = ip->i_uid;
				u.u_procp->p_uid = ip->i_uid;
			}
		if(ip->i_mode&ISGID)
			u.u_gid = ip->i_gid;
	}

	/*
	 * clear sigs, regs and return
	 */

	u.u_trap = 0;		/* enable system calls */
	c = ip;
	for(ip = &u.u_signal[0]; ip < &u.u_signal[NSIG]; ip++)
		if((*ip & 1) == 0)
			*ip = 0;
	for(cp = &regloc[0]; cp < &regloc[6];)
		u.u_ar0[*cp++] = 0;
	u.u_ar0[R7] = 0;
	for(ip = &u.u_fsav[0]; ip < &u.u_fsav[25];)
		*ip++ = 0;
	ip = c;

bad:
	iput(ip);
	brelse(bp);
	if(execnt >= NEXEC)
		wakeup(&execnt);
	execnt--;
}

/*
 * exit system call:
 * pass back caller's r0
 */
rexit()
{

	u.u_arg[0] = u.u_ar0[R0] << 8;
	exit();
}

/*
 * Release resources.
 * Save u. area for parent to look at.
 * Enter zombie state.
 * Wake up parent and init processes,
 * and dispose of children.
 */
exit()
{
	register int *q, a;
	register struct proc *p;

	p = u.u_procp;
	p->p_flag =& ~STRC;
	p->p_clktim = 0;
	for(q = &u.u_signal[0]; q < &u.u_signal[NSIG];)
		*q++ = 1;
	for(q = &u.u_ofile[0]; q < &u.u_ofile[NOFILE]; q++)
		if(a = *q) {
			*q = NULL;
			closef(a);
		}
	putstat(ST_ACC+ST_CPU,6,u.u_stime,u.u_utime[1],q->p_size);
#ifdef ACC_LP
	if (u.u_lplines || u.u_lppages || u.u_lpplot)
		putstat(ST_ACC+ST_LP,6,u.u_lppages,u.u_lplines,u.u_lpplot);
#endif
	iput(u.u_cdir);
	xfree();
	q = u.u_procp;
	mfree(coremap, q->p_size, q->p_addr);
	q->p_stat = SZOMB;
	dpadd(u.u_cstime,u.u_stime);
	dpadd2(u.u_cutime,u.u_utime);
	q->p_cstime[0] = u.u_cstime[0];
	q->p_cstime[1] = u.u_cstime[1];
	q->p_cutime[0] = u.u_cutime[0];
	q->p_cutime[1] = u.u_cutime[1];
	q->p_r1 = u.u_arg[0];

loop:
	for(p = &proc[0]; p < &proc[NPROC]; p++)
	if(q->p_ppid == p->p_pid) {
			wakeup(&proc[1]);	/* wake init */
		wakeup(p);
		for(p = &proc[0]; p < &proc[NPROC]; p++)
		if(q->p_pid == p->p_ppid) {
			p->p_ppid  = 1;
			if (p->p_stat == SSTOP)
				setrun(p);
		}
		swtch();
		/* no return */
	}
	q->p_ppid = 1;
	goto loop;
}

/*
 * Wait system call.
 * Search for a terminated (zombie) child,
 * finally lay it to rest, and collect its status.
 * Look also for stopped (traced) children,
 * and pass back status from them.
 */
wait()
{
	register f, *bp;
	register struct proc *p;

	f = 0;

loop:
	for(p = &proc[0]; p < &proc[NPROC]; p++)
	if(p->p_ppid == u.u_procp->p_pid) {
		f++;
		if(p->p_stat == SZOMB) {
			u.u_ar0[R0] = p->p_pid;
			u.u_ar0[R1] = p->p_r1;
			dpadd2(u.u_cstime,p->p_cstime);
			dpadd2(u.u_cutime,p->p_cutime);
			p->p_stat = NULL;
			p->p_pid = 0;
			p->p_ppid = 0;
			p->p_sig = 0;
			p->p_ttyp = 0;
			p->p_flag = 0;
			return;
		}
		if(p->p_stat == SSTOP) {
			if((p->p_flag&SWTED) == 0) {
				p->p_flag =| SWTED;
				u.u_ar0[R0] = p->p_pid;
				u.u_ar0[R1] = (p->p_sig<<8) | 0177;
				return;
			}
			p->p_flag =& ~(STRC|SWTED);
			setrun(p);
		}
	}
	if(f) {
		sleep(u.u_procp, PWAIT);
		goto loop;
	}
	u.u_error = ECHILD;
}

/*
 * fork system call.
 */
fork()
{
	register struct proc *p1, *p2;
	register int a;

	a=0; p2 = NULL;
	for(p1 = &proc[0]; p1 < &proc[NPROC]; p1++)
		{
		if(p1->p_stat == NULL)
			{
			if (p2 == NULL)
				p2 = p1;
			}
		else if (p1->p_uid == u.u_uid &&
				p1->p_ttyp == u.u_procp->p_ttyp)
			++a;
		}
	if (p2 == NULL || (a > MAXUPRC && u.u_uid != 0))
		{
		u.u_error = EAGAIN;
		goto out;
		}

	p1 = u.u_procp;
	if(newproc(0)) {
		u.u_ar0[R0] = p1->p_pid;
		u.u_cstime[0] = 0;
		u.u_cstime[1] = 0;
		u.u_stime = 0;
		u.u_cutime[0] = 0;
		u.u_cutime[1] = 0;
		u.u_utime[0] = 0;
		u.u_utime[1] = 0;
#ifdef ACC_LP
		u.u_lppages = 0;
		u.u_lplines = 0;
		u.u_lpplot = 0;
#endif	/* ACC_LP */
		return;
	}
	u.u_ar0[R0] = p2->p_pid;

out:
	u.u_ar0[R7] =+ 2;
}

/*
 * break system call.
 *  -- bad planning: "break" is a dirty word in C.
 */
sbreak()
{
	register a, n, d;
	int i;

	/*
	 * set n to new data size
	 * set d to new-old
	 * set n to new total size
	 */

	n = (((u.u_arg[0]+63)>>6) & 01777);
	if(!u.u_sep)
		n =- nseg(u.u_tsize) * 128;
	if(n < 0)
		n = 0;
	d = n - u.u_dsize;
	n =+ USIZE+u.u_ssize;
	if(estabur(u.u_tsize, u.u_dsize+d, u.u_ssize, u.u_sep))
		return;
	u.u_dsize =+ d;
	if(d > 0)
		goto bigger;
	a = u.u_procp->p_addr + n - u.u_ssize;
	i = n;
	n = u.u_ssize;
	while(n--) {
		copyseg(a-d, a);
		a++;
	}
	expand(i);
	return;

bigger:
	expand(n);
	a = u.u_procp->p_addr + n;
	n = u.u_ssize;
	while(n--) {
		a--;
		copyseg(a-d, a);
	}
	while(d--)
		clearseg(--a);
}
