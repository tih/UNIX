#
/*
 * mem driver:
 * modified '76 by bill webb
 * 1) give error indication upon read/write error 
 * 2) support /dev/imem ... kernal i space.
 * modified May/79
 *	use iomove for better efficiency for kmem.
 */

/*
 *	Memory special file
 *	minor device 0 is physical memory
 *	minor device 1 is kernel memory
 *	minor device 2 is EOF/RATHOLE
 *	minor device 3 is kernel i space memory
 */

#include "../param.h"
#include "../user.h"
#include "../conf.h"
#include "../seg.h"
#include "../buf.h"

#define	KISA	0172340

mmread(dev)
{
	register c, bn, on;
	struct buf bf;
	char *p;
	int a, d;

	switch(dev.d_minor)
		{
	case 2:		/* eof rathole */
		return;
	case 1:		/* kmem */
		bf.b_addr = u.u_offset[1];
		iomove(&bf,0,u.u_count,B_READ);
		return;

	case 3:		/* imem */
		p = KISA;
		break;
		}
	do {
		bn = lshift(u.u_offset, -6);
		on = u.u_offset[1] & 077;
		a = UISA->r[0];
		d = UISD->r[0];
		spl7();
		UISA->r[0] = bn;
		UISD->r[0] = 077406;
		if(dev.d_minor )
			UISA->r[0] = p->r[(bn>>7)&07] + (bn & 0177);
		c = fuibyte(on);
		if(c<0)
			u.u_error = ENXIO;
		UISA->r[0] = a;
		UISD->r[0] = d;
		spl0();
	} while(u.u_error==0 && passc(c)>=0);
}

mmwrite(dev)
{
	register c, bn, on;
	int a, d;
	char *p;

	switch(dev.d_minor)
		{
	case 2:		/* eof rathole */
		c = u.u_count;
		u.u_count = 0;
		u.u_base =+ c;
		dpadd(u.u_offset, c);
		return;
	case 1:		/* kmem */
		p = ka6 - 6;
		break;

	case 3:		/* imem */
		p = KISA;
		break;
		}
	for(;;) {
		bn = lshift(u.u_offset, -6);
		on = u.u_offset[1] & 077;
		if ((c=cpass())<0 || u.u_error!=0)
			break;
		a = UISA->r[0];
		d = UISD->r[0];
		spl7();
		UISA->r[0] = bn;
		UISD->r[0] = 077406;
		if(dev.d_minor == 1)
			UISA->r[0] = p->r[(bn>>7)&07] + (bn & 0177);
		if(suibyte(on, c)<0)
			u.u_error = ENXIO;
		UISA->r[0] = a;
		UISD->r[0] = d;
		spl0();
	}
}
