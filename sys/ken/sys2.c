#
/*
 *	Copyright 1973 Bell Telephone Laboratories Inc
 */

#include "../param.h"
#include "../systm.h"
#include "../user.h"
#include "../reg.h"
#include "../file.h"
#include "../inode.h"
#include "../V7.h"

/*
 * read system call
 */
read()
{
	rdwr(FREAD);
}

/*
 * write system call
 */
write()
{
	rdwr(FWRITE);
}

/*
 * common code for read and write calls:
 * check permissions, set base, count, and offset,
 * and switch out to readi, writei, or pipe code.
 */
rdwr(mode)
{
	register *fp, m;

	m = mode;
	fp = getf(u.u_ar0[R0]);
	if(fp == NULL)
		return;
	if((fp->f_flag&m) == 0) {
		u.u_error = EBADF;
		return;
	}
	u.u_base = u.u_arg[0];
	u.u_count = u.u_arg[1];
	u.u_segflg = 0;
	if(fp->f_flag&FPIPE) {
		if(m==FREAD)
			readp(fp); else
			writep(fp);
	} else {
		u.u_offset[1] = fp->f_offset[1];
		u.u_offset[0] = fp->f_offset[0];
		if(m==FREAD)
			readi(fp->f_inode); else
			writei(fp->f_inode);
		dpadd(fp->f_offset, u.u_arg[1]-u.u_count);
	}
	u.u_ar0[R0] = u.u_arg[1]-u.u_count;
}

/*
 * open system call
 */
open()
{
	register *ip;
	register int mode;
	extern uchar;

	ip = namei(&uchar, 0);
	if(ip == NULL)
		return;
	mode = u.u_arg[1];
	if (++mode == 04)
		mode = 07;	/* 0 ==>FREAD 1==>FWRITE 2==>FREAD+FWRITE 3 ==> 7 */
	open1(ip, mode, 0);
}

/*
 * creat system call
 */
creat()
{
	register *ip;
	extern uchar;

	ip = namei(&uchar, 1);
	if(ip == NULL) {
		if(u.u_error)
			return;
		ip = maknode(u.u_arg[1]&07777&(~ISVTX)&~u.u_mask);
		open1(ip, FWRITE, 2);
	} else
		open1(ip, FWRITE, 1);
}

/*
 * common code for open and creat.
 * Check permissions, allocate an open file structure,
 * and call the device open routine if any.
 * modified June 15/81 by W. Webb to allow super-user open of
 * a directory (mode=03) for read/write.
 */
open1(ip, mode, trf)
int *ip;
{
	register struct file *fp;
	register *rip, m;
	int i;

	rip = ip;
	m = mode;
	if(trf != 2) {
		if(m&FREAD)
			access(rip, IREAD);
		if(m&FWRITE) {
			access(rip, IWRITE);
			if((rip->i_mode&IFMT) == IFDIR && !((m&04) && suser()))
				u.u_error = EISDIR;
		}
	}
	if(u.u_error)
		goto out;
	if(trf)
		itrunc(rip);
	prele(rip);
	if ((fp = falloc()) == NULL)
		goto out;
	fp->f_flag = m&(FREAD|FWRITE);
	fp->f_inode = rip;
	i = u.u_ar0[R0];
	openi(rip, m&FWRITE);
	if(u.u_error == 0)
		return;
	u.u_ofile[i] = NULL;
	fp->f_count--;

out:
	iput(rip);
}

/*
 * close system call
 */
close()
{
	register *fp;

	fp = getf(u.u_ar0[R0]);
	if(fp == NULL)
		return;
	u.u_ofile[u.u_ar0[R0]] = NULL;
	closef(fp);
}

/*
 * seek system call
 */
seek()
{
	int n[2];
	register *fp, t;

	fp = getf(u.u_ar0[R0]);
	if(fp == NULL)
		return;
	if(fp->f_flag&FPIPE) {
		u.u_error = ESPIPE;
		return;
	}
	t = u.u_arg[1];
	if(t > 2) {
		n[1] = u.u_arg[0]<<9;
		n[0] = u.u_arg[0]>>7;
		if(t == 3)
			n[0] =& 0777;
	} else {
		n[1] = u.u_arg[0];
		n[0] = 0;
		if(t!=0 && n[1]<0)
			n[0] = -1;
	}
	switch(t) {

	case 1:
	case 4:
		n[0] =+ fp->f_offset[0];
		dpadd(n, fp->f_offset[1]);
		break;

	default:
		n[0] =+ fp->f_inode->i_size0&0377;
		dpadd(n, fp->f_inode->i_size1);

	case 0:
	case 3:
		;
	}
	fp->f_offset[1] = n[1];
	fp->f_offset[0] = n[0];
}
/*
 * lseek system call
 */
lseek()
{
	int n[2];
	register *fp, t;

	fp = getf(u.u_ar0[R0]);
	if(fp == NULL)
		return;
	if(fp->f_flag&FPIPE) {
		u.u_error = ESPIPE;
		return;
	}
	t = u.u_arg[2];
	n[0] = u.u_arg[0];
	n[1] = u.u_arg[1];
	switch(t) {

	case 0:		/* absolute */
		break;
	case 1:		/* relative */
		n[0] =+ fp->f_offset[0];
		dpadd(n, fp->f_offset[1]);
		break;

	case 2:		/* to end of the file */
		n[0] =+ fp->f_inode->i_size0&0377;
		dpadd(n, fp->f_inode->i_size1);
		break;
	default:
		u.u_error = EFAULT;

	}
	u.u_ar0[R0] = fp->f_offset[0];	/* returned results in r0, r1 */
	u.u_ar0[R1] = fp->f_offset[1];
	fp->f_offset[1] = n[1];
	fp->f_offset[0] = n[0];
}

/*
 * link system call
 */
link()
{
	register *ip, *xp;
	extern uchar;

	ip = namei(&uchar, 0);
	if(ip == NULL)
		return;
	if((ip->i_nlink & BYTEMASK) >= MAXLINK) {
		u.u_error = EMLINK;
		goto out;
	}
	if((ip->i_mode&IFMT)==IFDIR && !suser())
		goto out;
	/*
	 * unlock to avoid possibly hanging the namei
	 */
	ip->i_flag =& ~ILOCK;
	u.u_dirp = u.u_arg[1];
	xp = namei(&uchar, 1);
	if(xp != NULL) {
		u.u_error = EEXIST;
		iput(xp);
	}
	if(u.u_error)
		goto out;
	if(u.u_pdir->i_dev != ip->i_dev) {
		iput(u.u_pdir);
		u.u_error = EXDEV;
		goto out;
	}
	wdir(ip);
	ip->i_nlink++;
	ip->i_flag =| IUPD;

out:
	iput(ip);
}

/*
 * mknod system call
 */
mknod()
{
	register *ip;
	extern uchar;

	if(suser()) {
		ip = namei(&uchar, 1);
		if(ip != NULL) {
			u.u_error = EEXIST;
			goto out;
		}
	}
	if(u.u_error)
		return;
	ip = maknode(u.u_arg[1] & ~u.u_mask);
	ip->i_addr[0] = u.u_arg[2];

out:
	iput(ip);
}

/*
 * sleep system call
 * not to be confused with the sleep internal routine.
 */
sslep()
{
	char *d[2];

	spl7();
	d[0] = time[0];
	d[1] = time[1];
	dpadd(d, u.u_ar0[R0]);

	while(dpcmp(d[0], d[1], time[0], time[1]) > 0) {
		if(dpcmp(tout[0], tout[1], time[0], time[1]) <= 0 ||
		   dpcmp(tout[0], tout[1], d[0], d[1]) > 0) {
			tout[0] = d[0];
			tout[1] = d[1];
		}
		sleep(tout, PSLEP);
	}
	spl0();
}

#ifdef V7CODE
/*
 * faccess test access to a file
 * trick is to set uid, gid to ruid, rgid so that the internal
 * access routine can be used.
 */
faccess()
{
char uid, gid;
register char *ip;
register int mode;
extern uchar;

if ((ip = namei(&uchar,0)) == NULL)
	return;
uid = u.u_uid; gid = u.u_gid;
u.u_uid = u.u_ruid; u.u_gid = u.u_rgid;
mode = u.u_arg[1];
if (mode & (IEXEC>>6))
	access(ip,IEXEC);
if (mode & (IWRITE>>6))
	access(ip,IWRITE);
if (mode & (IREAD>>6))
	access(ip,IREAD);
u.u_uid = uid; u.u_gid = gid;
iput(ip);
}
#endif

