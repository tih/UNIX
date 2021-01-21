#
#include "../param.h"

#define	PIR	0177772

struct pir
{
int (*p_rtn) ();		/* routine to call on interrupt */
int p_data;		/* data to pass to it */
} pirdata[8];

pirq(prio,rtn,data) int (*rtn)();
{
/*
 * invoked with a priority level and the address to be passed
 * to wakeup.
 */
register struct pir *p;			/* pointer to this one's data */

prio =& 07;			/* insure its in range */
p = &pirdata[prio];
p->p_rtn = rtn;
p->p_data = data;
PIR->integ =| 1<<(prio+8);	/* lobyteg the request */
}

pirint()
{
/* 
 * got the pir interrupt, so do the wakeup that was delayed
 */
register int prio;
register struct pir *p;			/* pointer to this one's data */

prio = (PIR->lobyte >> 1) & 07;
p = &pirdata[prio];
PIR->integ =& ~(1<<(prio+8));	/* clear the request */
PS->lobyte = PIR->lobyte;		/* go to the lobytewer priority level */
(*p->p_rtn)(p->p_data);
}
