#

#include "../param.h"
#include "../user.h"

#define	ienable	0100
#define	xyprio 030
#define	xyhwat 200
#define	xylwat 50
#define	xyaddr 0172554
#define	closed	0
#define	idle	1
#define	busy	2
#define	full	3

/* xy11 plotter driver. currently just sends pen moves */
struct {
	int cc;
	int cf;
	int cl;
	char state;
	} xy11;
struct {
	int xycsr;
	int xydbr;
	};

xyopen(dev,flag)
{
	if(xy11.state!=closed)
		{	/* already open */ 
		u.u_error = ENODEV;
		return;
		}
	xyaddr->xycsr = ienable;
	xy11.state=idle;
}

xywrite()
{
register c;
extern lbolt;
	while ((c=cpass())>=0)
		{
		spl5();
		if(xy11.state==idle)
			{
			xy11.state=busy;
			xyaddr->xydbr = c;
			}
		else
			{
			while (xy11.cc > xyhwat)
				{
				xy11.state=full;
				sleep(&xy11,xyprio);
				}
			while (putc(c,&xy11) < 0)
				sleep(&lbolt,xyprio);
			}
		spl0();
		}
}

xyint()
{

register c;

	if ((c=getc(&xy11))>=0)
		{	/* got another one to output */
		xyaddr->xydbr = c;
		if(xy11.state==full && xy11.cc< xylwat)
			{
			xy11.state=busy;
			wakeup(&xy11);
			}
		}
	else	/* no more output for now */ 
		{
		xy11.state = idle;
		wakeup(&xy11);
		}
}

xyclose(dev,flag)
{
	spl5();
	while (xy11.state == busy)
		sleep(&xy11,xyprio);
	xy11.state=closed;
	while ((getc(&xy11))>=0) ; 
	xyaddr->xycsr = 0;
	spl0();
}
