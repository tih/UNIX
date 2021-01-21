.globl _kdword
_kdword:
	mov	2(sp),r1		/the address
	jsr	pc,1f
	rts	pc
1:	mov	PS,-(sp)
	spl	HIGH
	mov	nofault,-(sp)
	mov	$err,nofault
	mov	(r1),r0
	mov	(sp)+,nofault
	mov	(sp)+,PS
	rts	pc
err:
	mov	(sp)+,nofault
	mov	(sp)+,PS
	tst	(sp)+
	mov	$-1,r0
	rts	pc

