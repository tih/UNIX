#
/* driver for digitorg ir
*/
#include "../param.h"
#include "../user.h"
#define	irhiwat	512
#define dummy 0
#define iraddr 0164010
#define iripri 30
#define	error	-1
#define sleeping 2
#define busy 1
#define	closed	0
struct{
	char irset;
	char irclear;
	int irrbuf;
};
struct{
	int cc;
	int cf;
	int cl;
}irlist;
int irstate;

iropen(dev,flag)
{
	register int dump;

	if(irstate != closed)
		{
		u.u_error = EIO;
		return;
		}
	dump = iraddr->irrbuf;
	iraddr->irclear = dummy;
	iraddr->irset = dummy;
	irstate = busy;
}
irclose(dev,flag)
{
	iraddr->irclear = dummy;
	while(getc(&irlist) >= 0);
	irstate = closed;
}
irread( )
{
	register int c;
	spl4();
	do {
		while(  (c = getc(&irlist)) < 0){
		if(irstate == error)
			{
			u.u_error = ENXIO;
			spl0();
			return;
			}
		irstate = sleeping;
		sleep(&irlist,iripri);
		}
	}while(passc(c) >= 0);
spl0();
}

irint(dev)
{
	register int i;
	register char *irtimes;
	extern lbolt, time;

	if (irlist.cc > irhiwat)
		{
	err:
		iraddr->irclear = 0;
		prdev("ir q full",dev);
		irstate = error;
		wakeup(&irlist);
		}
	putc(lbolt,&irlist);
	putc(iraddr->irrbuf,&irlist);
	irtimes = &time;
	for(i = 0;i < 4;i++)
		if(putc(*irtimes++,&irlist) < 0) goto err;
	if(irstate == sleeping)
		{irstate = busy;wakeup(&irlist);}
}
