#
/*
 *	Copyright 1981, Bill Webb (UBC)
 */

/*
 * ioctl interface for V7 emulation
 */
#include "../param.h"
#include "../inode.h"
#include "../user.h"
#include "../tty.h"
#include "../reg.h"
#include "../file.h"
#include "../conf.h"
#include "../V7.h"


#ifdef IOCTL

/*
 * The routine implementing the gtty system call.
 * fake it as a ioctl call
 */
gtty()
{
	u.u_arg[2] = u.u_arg[0];	/* array address */
	u.u_arg[0] = u.u_ar0[R0];	/* file descriptor */
	u.u_arg[1] = TIOCGETP;		/* stty fn */
	ioctl();
}

/*
 * The routine implementing the stty system call.
 * fake it as a ioctl call
 */
stty()
{
	u.u_arg[2] = u.u_arg[0];	/* array address */
	u.u_arg[0] = u.u_ar0[R0];	/* file descriptor */
	u.u_arg[1] = TIOCSETP;		/* stty fn */
	ioctl();
}

/*
 * ioctl interface.
 * arg[0] is file descriptor
 * arg[1] is cmd
 * arg[2] is addr
 */
ioctl()
{
	register struct file *fp;
	register struct inode *ip;

	if ((fp = getf(u.u_arg[0])) == NULL)
		return;
	ip = fp->f_inode;
	if ((ip->i_mode&IFMT) != IFCHR) {
		u.u_error = ENOTTY;
		return;
	}
	(*cdevsw[ip->i_addr[0].d_major].d_sgtty)(ip->i_addr[0], u.u_arg[1],
	u.u_arg[2], fp->f_flag);	/* invoke device driver */
}

iostty(dev,cmd,addr,rtn)
int (*rtn)();
int *addr;
/*
 * convert ioctl call to old stty/gtty driver call.
 * and then call it.
 */
{
int v[3];
register int *vp;

switch(cmd)
	{
case TIOCSETP:
	u.u_arg[0] = fuword(addr);
	u.u_arg[1] = fuword(++addr);
	u.u_arg[2] = fuword(++addr);
	(*rtn) (dev, 0);		/* store information */
	break;
case TIOCGETP:
	(*rtn) (dev, vp = v);		/* get information */
	suword(addr, *vp++);
	suword(++addr, *vp++);
	suword(++addr, *vp++);
	break;
default:
	u.u_error = ENOTTY;
	break;
	}
}

ttyioctl(cmd,atp,addr,dev)
struct tty *atp;
int *addr;
/*
 * the real ioctl routine for tty devices
 * for gtty/stty use the old routine, otherwise handle it here.
 */
{
register struct tty *tp;
register struct file *fp;
register int *vp;
int v[3];

tp = atp;
switch(cmd)
	{
case TIOCSETP:
	u.u_arg[0] = fuword(addr);
	u.u_arg[1] = fuword(++addr);
	u.u_arg[2] = fuword(++addr);
	ttystty(tp,0);
	break;
case TIOCGETP:
	ttystty(tp,vp = v);
	suword(addr, *vp++);
	suword(++addr, *vp++);
	suword(++addr, *vp++);
	break;
case TIOCFLUSH:
	flushtty(tp);
	break;
#ifdef LOGGING
case TIOCLON:
	if (tp->t_logf != NULL)
		u.u_error = EBUSY;
	else
		if ((fp = getf(fuword(addr))) != NULL)
			{
			if ((fp->f_flag&FWRITE) == 0)
				u.u_error = EBADF;
			else
				{
				tp->t_logf = fp;
				fp->f_count++;
				tp->t_state =| TTYILOG;
				}
			}
	break;
case TIOCLOFF:
	if ((fp = tp->t_logf) != NULL)
		{
		tp->t_logf = NULL;
		closef(fp);
		}
	tp->t_state =& ~(TTYILOG | TTYOLOG);
	break;
case TIOCLGETP:
	if (tp->t_logf == NULL)
		u.u_error = EINVAL;
	else
		suword(addr,(tp->t_state&(TTYILOG|TTYOLOG)) >> TTYLIOSHIFT);
	break;
case TIOCLSETP:
	if (tp->t_logf == NULL)
		u.u_error = EINVAL;
	else
		{
		tp->t_state =& ~(TTYILOG | TTYOLOG);
		tp->t_state =| (TTYILOG|TTYOLOG) & (fuword(addr) << TTYLIOSHIFT);
		}
	break;
case TIOCLPUT:
		ttyinput(fuword(addr)&0177,tp);
	break;
#endif
#ifdef SYNCECHO
case TIOCSETS:
	tp->t_state =& ~SECHO;
	tp->t_state =| (fuword(addr) << 15) & SECHO;
	break;
case TIOCGETS:
	suword(addr, (tp->t_state & SECHO) != 0);
	break;
#endif
#ifdef INTIMER
case TIOCITIME:
	tp->t_indelay = fuword(addr);
	tp->t_intimer = 0;
	break;
#endif
default:
	u.u_error = ENOTTY;
	break;
	}
return(u.u_error == 0);
}

#endif

#ifndef IOCTL
ioctl()
{
u.u_ar0[R0] = u.u_arg[0];	/* copy fildes */
u.u_arg[0] = u.u_arg[2];	/* copy the argument pointer */
switch(u.u_arg[1])
	{
case TIOCSETP:
	stty();
	break;
case TIOCGETP:
	gtty();
	break;
default:
	u.u_error = ENOTTY;
	break;
	}
}


/*
 * The routine implementing the gtty system call.
 * Just call lower level routine and pass back values.
 */
gtty()
{
	int v[3];
	register *up, *vp;

	vp = v;
	sgtty(vp);
	if (u.u_error)
		return;
	up = u.u_arg[0];
	suword(up, *vp++);
	suword(++up, *vp++);
	suword(++up, *vp++);
}

/*
 * The routine implementing the stty system call.
 * Read in values and call lower level.
 */
stty()
{
	register int *up;

	up = u.u_arg[0];
	u.u_arg[0] = fuword(up);
	u.u_arg[1] = fuword(++up);
	u.u_arg[2] = fuword(++up);
	sgtty(0);
}

/*
 * Stuff common to stty and gtty.
 * Check legality and switch out to individual
 * device routine.
 * v  is 0 for stty; the parameters are taken from u.u_arg[].
 * c  is non-zero for gtty and is the place in which the device
 * routines place their information.
 */
sgtty(v)
int *v;
{
	register struct file *fp;
	register struct inode *ip;

	if ((fp = getf(u.u_ar0[R0])) == NULL)
		return;
	ip = fp->f_inode;
	if ((ip->i_mode&IFMT) != IFCHR) {
		u.u_error = ENOTTY;
		return;
	}
	(*cdevsw[ip->i_addr[0].d_major].d_sgtty)(ip->i_addr[0], v);
}
#endif

/*
 * Common code for gtty and stty functions on typewriters.
 * If v is non-zero then gtty is being done and information is
 * passed back therein;
 * if it is zero stty is being done and the input information is in the
 * u_arg array.
 */
ttystty(atp, av)
int *atp, *av;
{
	register  *tp, *v;

	tp = atp;
	if(v = av) {
		*v++ = tp->t_speeds;
		v->lobyte = tp->t_erase;
		v->hibyte = tp->t_kill;
		++v;
		*v = tp->t_flags;
		u.u_ar0[R0] = tp->t_delct;	/* return # of delimiters */
		return(1);
	}
	wflushtty(tp);
	v = u.u_arg;
	tp->t_speeds = *v++;
	tp->t_erase = v->lobyte;
	tp->t_kill = v->hibyte;
	tp->t_flags = v[1];
	return(0);
}

#ifdef LOGGING
int logdbg;
logtty(atp,base,count) struct tty *atp;
{
/*
 * routine to write output to tty log file.
 * don't log output if logging is off, if in RAW or NOECHO
 * modes.
 * convert a count=0 to "^D\n" so that ^D's will work.
 */
register struct tty *tp;
register struct file *fp;
int ocount;
char *offset[2];
char *obase;
int segflg;
int error;

/*
if (logdbg)
	printf("state=%o flags=%o logf=%o base=%o count=%d\n",tp->t_state,tp->t_flags,tp->t_logf,base,count); /* debug */
tp = atp;
if ((fp = tp->t_logf) == NULL || (tp->t_flags&RAW) || (tp->t_flags&ECHO) == 0)
	return(1);		/* don't log it */

/* save the system variables used for I/O */
obase = base;
segflg = u.u_segflg;
offset[0] = u.u_offset[0];
offset[1] = u.u_offset[1];
ocount = u.u_count;
if (error = u.u_error)
	return(1);		/* something wrong - don't log it */

/* set up our own I/O */
u.u_base = base;
u.u_offset[0] = fp->f_offset[0];
u.u_offset[1] = fp->f_offset[1];
if ((u.u_count = count) == 0)
	{
	u.u_segflg = 1;		/* use kernal string */
	u.u_base = "^D\n";	/* simulated ^D */
	count = u.u_count = 3;	/* length of above string */
	}
writei(fp->f_inode);
if (u.u_error)
	tp->t_state =& ~(TTYILOG|TTYOLOG);	/* turn off logging if error */
dpadd(fp->f_offset,count-u.u_count);
/*
if (logdbg)
	printf("writei done\n");	/* debug */
/* restore previous I/O parameters */
u.u_segflg = segflg;
u.u_base = obase;
u.u_count = ocount;
u.u_offset[0] = offset[0];
u.u_offset[1] = offset[1];
u.u_error = error;
return(0);			/* logging done */
}
#endif	/* LOGGING */
