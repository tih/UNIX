#
#define	pcsr	0172134
/*
 *	Copyright 1975 Bell Telephone Laboratories Inc
 */

#include "../param.h"
#include "../user.h"
#include "../systm.h"
#include "../proc.h"
#include "../text.h"
#include "../inode.h"
#include "../seg.h"

#define	CLOCK1	0177546
#define	CLOCK2	0172540
#define KL_CLOCK	0176526	/* DLV11-J #2 Xmit data register */
/*
 * Icode is the octal bootstrap
 * program executed in user mode
 * to bring up the system.
 */
int	icode[]
{
	0104413,	/* sys exec; init; initp */
	0000014,
	0000010,
	0000777,	/* br . */
	0000014,	/* initp: init; 0 */
	0000000,
	0062457,	/* init: </etc/init\0> */
	0061564,
	0064457,
	0064556,
	0000164,
};

/*
 * Initialization code.
 * Called from m40.s or m45.s as
 * soon as a stack and segmentation
 * have been established.
 * Functions:
 *	clear and free user core
 *	find which clock is configured
 *	hand craft 0th process
 *	call all initialization routines
 *	fork - process 0 to schedule
 *	     - process 1 execute bootstrap
 *
 * panic: no clock -- neither clock responds
 * loop at loc 6 in user mode -- /etc/init
 *	cannot be executed.
 */
main()
{
	extern schar;
	register int l, m;

	/*
	 * zero and free all of core
	 */

	updlock = 0;
	printf("unix v6 11/%d\n",cputype);

	UISA->r[7] = ka6[1]; /* io segment */
	UISD->r[7] = 077406;

	m = 0;
	l = 0;
	UISA->r[0] = *ka6 + USIZE;
	UISD->r[0] = 077406;
	for(; fuibyte(0) >= 0; UISA->r[0]++) {
		if(UISA->r[0] == 07600)
			break;
		clearseg(UISA->r[0]);
		if(testmem(UISA->r[0]))
			{
			++l;
			++m;
			if(l > maxmem) 
				maxmem = l;
			mfree(coremap, 1, UISA->r[0]);
			}
		else		/* memory is bad or not to be used */
			{
			l = 0;
			if (sws()&01)	/* stop on bad memory if sws odd */
				break;
			}
	}
	printf("mem = %l KW", m*5/160);
	maxmem = min(maxmem, MAXMEM);
	printf(" max = %l\n", (maxmem-USIZE)*5/160);
	mfree(swapmap, nswap, swplo);

	startclk();

	/*
	 * set up system process
	 */

	proc[0].p_addr = *ka6;
	proc[0].p_size = USIZE;
	proc[0].p_stat = SRUN;
	proc[0].p_flag =| SLOAD|SSYS;
	u.u_procp = &proc[0];

	/*
	 * set up 'known' i-nodes
	 */

	*lks = 0115;
	cinit();
	binit();
	iinit();
	rootdir = iget(rootdev, ROOTINO);
	rootdir->i_flag =& ~ILOCK;
	u.u_cdir = iget(rootdev, ROOTINO);
	u.u_cdir->i_flag =& ~ILOCK;

	/*
	 * make init process
	 * enter scheduling loop
	 * with system process
	 */

	if(newproc(0)) {
		expand(USIZE+1);
		estabur(0, 1, 0, 0);
		copyout(icode, 0, sizeof icode);
		/*
		 * Return goes to loc. 0 of user init
		 * code just copied out.
		 */
		return;
	}
	sched();
}

/*
 * determine clock
 * use:
 * (1) line frequency clock
 * (2) programmable clock
 * (3) LTC (if available)
 * (4) DLV11J port # 3 (at 600 baud)
 */
startclk()
{

	int clk;


	clk = lks;
	lks = CLOCK1;
	if(fuiword(lks) == -1) {
		if (fuword(lks = CLOCK2) == -1)
			{
			if (clk > 0 && clk < 01000)
				{
				lks = clk;
				printf("start LTC\n");
				}
			else
				{
				if (fuword(lks = KL_CLOCK) == -1)
					panic("no clock");
				/* load interrupt vector 320 from 100 */
				0324->integ = 0100->integ;
				0326->integ = 0300;	/* prio = 6 */
				lks[-1] = 0100;		/* alllow interrupts */
				printf("DLV11J#2 clock\n");
				}
			}
	}
}

/*
 * Load the user hardware segmentation
 * registers from the software prototype.
 * The software registers must have
 * been setup prior by estabur.
 */
sureg()
{
	register *up, *rp, a;

	a = u.u_procp->p_addr;
	up = &u.u_uisa[16];
	rp = &UISA->r[16];
	if(cputype <= 40) {
		up =- 8;
		rp =- 8;
	}
	while(rp > &UISA->r[0])
		*--rp = *--up + a;
	if((up=u.u_procp->p_textp) != NULL)
		a =- up->x_caddr;
	up = &u.u_uisd[16];
	rp = &UISD->r[16];
	if(cputype <= 40) {
		up =- 8;
		rp =- 8;
	}
	while(rp > &UISD->r[0]) {
		*--rp = *--up;
		if((*rp & WO) == 0)
			rp[(UISA-UISD)/2] =- a;
	}
}

/*
 * Set up software prototype segmentation
 * registers to implement the 3 pseudo
 * text,data,stack segment sizes passed
 * as arguments.
 * The argument sep specifies if the
 * text and data+stack segments are to
 * be separated.
 */
estabur(nt, nd, ns, sep)
{
	register a, *ap, *dp;

	if(sep) {
		if(cputype <= 40)
			goto err;
		if(nseg(nt) > 8 || nseg(nd)+nseg(ns) > 8)
			goto err;
	} else
		if(nseg(nt)+nseg(nd)+nseg(ns) > 8)
			goto err;
	if(nt+nd+ns+USIZE > maxmem)
		goto err;
	a = 0;
	ap = &u.u_uisa[0];
	dp = &u.u_uisd[0];
	while(nt >= 128) {
		*dp++ = (127<<8) | RO;
		*ap++ = a;
		a =+ 128;
		nt =- 128;
	}
	if(nt) {
		*dp++ = ((nt-1)<<8) | RO;
		*ap++ = a;
	}
	if(sep)
	while(ap < &u.u_uisa[8]) {
		*ap++ = 0;
		*dp++ = 0;
	}
	a = USIZE;
	while(nd >= 128) {
		*dp++ = (127<<8) | RW;
		*ap++ = a;
		a =+ 128;
		nd =- 128;
	}
	if(nd) {
		*dp++ = ((nd-1)<<8) | RW;
		*ap++ = a;
		a =+ nd;
	}
	while(ap < &u.u_uisa[8]) {
		*dp++ = 0;
		*ap++ = 0;
	}
	if(sep)
	while(ap < &u.u_uisa[16]) {
		*dp++ = 0;
		*ap++ = 0;
	}
	a =+ ns;
	while(ns >= 128) {
		a =- 128;
		ns =- 128;
		*--dp = (127<<8) | RW;
		*--ap = a;
	}
	if(ns) {
		*--dp = ((128-ns)<<8) | RW | ED;
		*--ap = a-128;
	}
	if(!sep) {
		ap = &u.u_uisa[0];
		dp = &u.u_uisa[8];
		while(ap < &u.u_uisa[8])
			*dp++ = *ap++;
		ap = &u.u_uisd[0];
		dp = &u.u_uisd[8];
		while(ap < &u.u_uisd[8])
			*dp++ = *ap++;
	}
	sureg();
	return(0);

err:
	u.u_error = ENOMEM;
	return(-1);
}

/*
 * Return the arg/128 rounded up.
 */
nseg(n)
{

	return((n+127)>>7);
}

testmem(addr)
{

register int i;

i = sws();
if((fuiword(pcsr)+1) & ~ 3)
	{
	printf("bad mem %o00\n",addr);
	pcsr->integ = 0;
	return(0);
	}

if(addr >= i && addr <= (i + 07))
	{
	printf("%o00...%o77 not used\n",addr,addr);
	return(0);
	}
return(1);
}
