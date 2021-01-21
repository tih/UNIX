#
/*
 *	Copyright 1973 Bell Telephone Laboratories Inc
 */

#include "../param.h"
#include "../user.h"
#include "../buf.h"
#include "../conf.h"
/*
/*
 * release the buffer, cancel any pending i/o.
 */
bdrel(bp)
struct buf *bp;
{
	register struct buf *rbp;

	rbp = bp;
	if (rbp->b_flags&B_DELWRI)
	{
	if (rbp->b_flags&B_WANTED)
		wakeup(rbp);
	if (bfreelist.b_flags&B_WANTED) {
		bfreelist.b_flags =& ~B_WANTED;
		wakeup(&bfreelist);
	}
	rbp->b_flags =& ~ (B_DELWRI | B_BUSY | B_ASYNC | B_WANTED);
	}

	else
		brelse(rbp);
}
