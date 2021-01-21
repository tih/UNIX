#
/*
 *	Copyright 1973 Bell Telephone Laboratories Inc
 */

#include "../param.h"
#include "../systm.h"
#include "../reg.h"
#include "../buf.h"
#include "../filsys.h"
#include "../user.h"
#include "../inode.h"
#include "../file.h"
#include "../conf.h"
#include "../V7.h"

/*
 * the fstat system call.
 */
fstat()
{
	register *fp;

	fp = getf(u.u_ar0[R0]);
	if(fp == NULL)
		return;
	stat1(fp->f_inode, u.u_arg[0]);
}

/*
 * the stat system call.
 */
stat()
{
	register ip;
	extern uchar;

	ip = namei(&uchar, 0);
	if(ip == NULL)
		return;
	stat1(ip, u.u_arg[1]);
	iput(ip);
}

/*
 * The basic routine for fstat and stat:
 * get the inode and pass appropriate parts back.
 */
stat1(ip, ub)
int *ip;
{
	register i, *bp, *cp;
	struct { char *charp; };

	iupdat(ip, time, time);
	bp = bread(ip->i_dev, ldiv(ip->i_number+31, 16));
	cp = bp->b_addr + 32*lrem(ip->i_number+31, 16) + 24;
	ip = &(ip->i_dev);
#ifdef V7CODE
	if (V7) {
		suword(ub, *ip++); ub =+ 2; /* dev, ino, flags */
		suword(ub, *ip++); ub =+ 2; /* dev, ino, flags */
		suword(ub, *ip++ & ~0110000); ub =+ 2; /* dev, ino, flags */
		for(i=0; i<3; i++) {
			suword(ub, *ip.charp++ &0377); ub =+ 2; /* nlink uid gid */
		}
		suword(ub, cp[-8]); ub =+ 2;	/* rdev */
		suword(ub, *ip.charp++ &0377); ub =+ 2;	/* size0 */
		suword(ub, *ip++); ub =+ 2;	/* size 1*/
		for(i=0; i<6; i++) {
			if (i == 4)
				cp =- 2;
			suword(ub, *cp++);	/* copy disk part inode */
			ub =+ 2;
		}
	}
else
#endif
		{
		for(i=0; i<14; i++) {
			suword(ub, *ip++);	/* copy in-core inode */
			ub =+ 2;
		}
		for(i=0; i<4; i++) {
			suword(ub, *cp++);	/* copy disk part inode */
			ub =+ 2;
		}
	}
	brelse(bp);
}


/*
 * the dup system call.
 */
dup()
{
	register i, m, *fp;

	m = u.u_ar0[R0] & 077;
	fp = getf(m);
	if(fp == NULL)
		return;
	if ((u.u_ar0[R0]&0100) == 0) {
		if ((i = ufalloc()) < 0)
			return;
	} else {
		i = u.u_ar0[R1];
		if (i<0 || i>=NOFILE) {
			u.u_error = EBADF;
			return;
		}
		u.u_ar0[R0] = i;
	}
	if (i!=m) { /* close if not the same as original */
		if (u.u_ofile[i]!=NULL)
			closef(u.u_ofile[i]);
		u.u_ofile[i] = fp;
		fp->f_count++;
	}
}

/*
 * the mount system call.
 */
smount()
{
	int d;
	register *ip;
	register struct mount *mp, *smp;
	extern uchar;

	d = getmdev();
	if(u.u_error)
		return;
	u.u_dirp = u.u_arg[1];
	ip = namei(&uchar, 0);
	if(ip == NULL)
		return;
	if(ip->i_count!=1 || (ip->i_mode&(IFBLK&IFCHR))!=0)
		goto out;
	smp = NULL;
	for(mp = &mount[0]; mp < &mount[NMOUNT]; mp++) {
		if(mp->m_bufp != NULL) {
			if(d == mp->m_dev)
				goto out;
		} else
		if(smp == NULL)
			smp = mp;
	}
	if(smp == NULL)
		goto out;
	(*bdevsw[d.d_major].d_open)(d, !u.u_arg[2]);
	if(u.u_error)
		goto out;
	mp = bread(d, 1);
	if(u.u_error) {
		brelse(mp);
		goto out1;
	}
	smp->m_inodp = ip;
	smp->m_dev = d;
	smp->m_bufp = getblk(NODEV);
	bcopy(mp->b_addr, smp->m_bufp->b_addr, 256);
	smp = smp->m_bufp->b_addr;
	smp->s_ilock = 0;
	smp->s_flock = 0;
	smp->s_ronly = u.u_arg[2] & 1;
	brelse(mp);
	ip->i_flag =| IMOUNT;
	prele(ip);
	return;

out:
	u.u_error = EBUSY;
out1:
	iput(ip);
}

/*
 * the umount system call.
 */
sumount()
{
	int d;
	register struct inode *ip;
	register struct mount *mp;

	update();
	d = getmdev();
	if(u.u_error)
		return;
	for(mp = &mount[0]; mp < &mount[NMOUNT]; mp++)
		if(mp->m_bufp!=NULL && d==mp->m_dev)
			goto found;
	u.u_error = EINVAL;
	return;

found:
	for(ip = &inode[0]; ip < &inode[NINODE]; ip++)
		if(ip->i_number!=0 && d==ip->i_dev) {
			u.u_error = EBUSY;
			return;
		}
	(*bdevsw[d.d_major].d_close)(d, 0);
	ip = mp->m_inodp;
	ip->i_flag =& ~IMOUNT;
	iput(ip);
	ip = mp->m_bufp;
	mp->m_bufp = NULL;
	brelse(ip);
}

/*
 * Common code for mount and umount.
 * Check that the user's argument is a reasonable
 * thing on which to mount, and return the device number if so.
 */
getmdev()
{
	register d, *ip;
	extern uchar;

	ip = namei(&uchar, 0);
	if(ip == NULL)
		return;
	if((ip->i_mode&IFMT) != IFBLK)
		u.u_error = ENOTBLK;
	d = ip->i_addr[0];
	if(ip->i_addr[0].d_major >= nblkdev)
		u.u_error = ENXIO;
	iput(ip);
	return(d);
}
