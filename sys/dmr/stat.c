#

#include "../param.h"
#include	"../user.h"
#include	"../conf.h"
#include	"../systm.h"
#include	"../proc.h"
#include	"../stat.h"
#include	"../tty.h"

#define	STATPRIO	0	/* rather hi priority */
#define	SMAXDEV	5		/* maximum # of stat devices */
#define	STATLEN	128

#define	CLOSED	0
#define	OPEN	1

struct { char lo, hi ;};
struct stat 
{
char s_state;			/* open, closed etc */
int s_time[3];			/* last time something happened */
struct clist s_q;		/* data q */
} stats[SMAXDEV];

stopen(dev,flag)
{
register int i;
register char *s;

i = dev.d_minor;
if (i < 0 || i > SMAXDEV)
	{
	u.u_error = ENODEV;
	return;
	}
s = &stats[i];
s->s_state = OPEN;
s->s_time[0]=time[0]; s->s_time[1]=time[1]; s->s_time[2]=lbolt;
}

stclose(dev)
{
register char *s;

s = &stats[dev.d_minor];
s->s_state = CLOSED;
while (getc(&s->s_q) >= 0)
	;
}

stread(dev)
{
register char *s;
register int i, c;

s = &stats[dev.d_minor];
while (s->s_q.c_cc <= 0)
	sleep(s,STATPRIO);
while ((c = getc(&s->s_q)) >= 0 && passc(c) >= 0)
	;
}

putstat(id,len,info) char *info;
{
register int i;
register char *s;
int sps;
register char *p;
int elt;

if ((i = (id&ST_DEVMASK)) > SMAXDEV || (s= &stats[i])->s_state != OPEN)
	return(0);

if (id & ST_INDIR)
	p = info;
else
	p = &info;

sps = PS->integ;
spl6();
if (id&ST_ID)
	statput(s,id.hi);
if (id&ST_ELAPSED)
	{
	elt = (time[1]-s->s_time[1])*HZ+(lbolt-s->s_time[2]);
	s->s_time[0]=time[0]; s->s_time[1]=time[1]; s->s_time[2]=lbolt;
	statput(s,elt.lo);
	statput(s,elt.hi);
	}
if (id&ST_UID)
	statput(s,u.u_ruid);
if (id&ST_GID)
	statput(s,u.u_rgid);
if (id&ST_PID)
	{
	statput(s,u.u_procp->p_pid.lo);
	statput(s,u.u_procp->p_pid.hi);
	}
for (i=0; i<len; ++i)
	statput(s,*p++);
wakeup(s);			/* wakeup after each stat call */
PS->integ=sps;
return(1);
}

statput(sp,info) char *sp, info;
{
register char *s;

s = sp;
if (putc(info,&s->s_q) < 0 || (s->s_q.c_cc % STATLEN) == 0)
	wakeup(s);
}
