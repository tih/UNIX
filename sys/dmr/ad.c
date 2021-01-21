#
/*
 *	Copyright 1973 Bell Telephone Laboratories Inc
 */

/*
 * ADV-11 analog to digit driver
 */

#include "../param.h"
#include "../conf.h"
#include "../user.h"

#define	ADADDR	0170400

#define	CLOSED	0
#define	OPEN	1
#define	READING	2
#define	ERROR	3

#define	IENABLE	0100
#define	DONE	0200
#define	ERROR	0100000
#define	GO	01

#define	ADIPRI	30

struct {
	int adcsr;
	int addbr;
};

struct clist {
	int	cc;
	int	cf;
	int	cl;
};

struct ad11 {
	int	debug;
	int	ad_state;
	struct	clist adin;
	int	ad_csr;
	int	ad_dbr;
        int     ad_port;
} ad11;

#define	tracef	if (ad11.debug) printf

adopen(dev, flag)
{

	ad11.debug = 0;
	tracef("adopen");
	if (ad11.ad_state!=CLOSED) {
		u.u_error = ENXIO;
		return;
	}
	ad11.ad_state = OPEN;
        ad11.ad_port = dev.d_minor * 256;
        tracef("  Port Address %o \n",ad11.ad_port);
}

adclose(dev, flag)
{
	tracef("adclose\n");
	spl5();
	while (getc(&ad11.adin) >= 0);
	ADADDR->adcsr = 0;
	ad11.ad_state = CLOSED;
	spl0();
}

adread()
{
	register int c;

	tracef("adread\n");
	spl5();
	ad11.ad_state = READING;
        ADADDR->adcsr = ad11.ad_port | IENABLE | GO;
	while (ad11.ad_state == READING)
			sleep(&ad11.adin, ADIPRI);
	c = ad11.ad_dbr & 07777;
	tracef("adread ==> %o %o\n",ad11.ad_csr,ad11.ad_dbr);
	passc(c & 0377);
	passc(c >> 8);
	spl0();
}



adrint()
{
	tracef("adrint state=%d\n",ad11.ad_state);
	if (ad11.ad_state==READING) {
		ad11.ad_csr = ADADDR->adcsr;
		ad11.ad_dbr = ADADDR->addbr;
		ADADDR->adcsr = 0;
		wakeup(&ad11.adin);
		ad11.ad_state = OPEN;
	}
}


adsgtty()
{
tracef("adsgtty\n");
}

aderr()
{
tracef("aderr\n");
ad11.ad_state = ERROR;
}
