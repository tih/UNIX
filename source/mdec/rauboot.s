/ RA bootstrap
/
/ This is pieced together from the 2.11BSD MSCP bootstrap, and the file
/ system bootstrap code in 6th Edition.  It loads "/unix" unconditionally,
/ as there is not enough space in 512 bytes to do both interaction and the	
/ MSCP bootstrap operations.

/ MSCP protocol constants:

MSCPSIZE =	64.	/ One MSCP command packet is 64 bytes long (need 2)

RASEMAP	=	100000	/ RA controller owner semaphore

RAERR =		100000	/ error bit 
RASTEP1 =	04000	/ step1 has started
RAGO =		01	/ start operation, after init
RASTCON	= 	4	/ Setup controller info 
RAONLIN	=	11	/ Put unit on line
RAREAD =	41	/ Read command code
RAWRITE =	42	/ Write command code
RAEND =		200	/ End command code

RACMDI =	4.	/ Command Interrupt
RARSPI =	6.	/ Response Interrupt
RARING =	8.	/ Ring base
RARSPL =	8.	/ Response Command low
RARSPH = 	10.	/ Response Command high
RACMDL =	12.	/ Command to controller low
RACMDH =	14.	/ Command to controller high
RARSPS =	16.	/ Response packet length (location)
RARSPREF =	20.	/ Response reference number
RACMDS =	80.	/ Command packet length (location)
RACMDREF =	84.	/ Command reference number
RAUNIT = 	88.	/ Command packet unit 
RAOPCODE =	92.	/ Command opcode offset
RABYTECT =	96.	/ Transfer byte count
RABUFL =	100.	/ Buffer location (16 bit addressing only)
RABUFH = 	102.	/ Buffer location high 6 bits
RALBNL =	112.	/ Logical block number low
RALBNH = 	114.	/ Logical block number high

/ constants:

CLSIZE	= 1.		/ physical disk blocks per logical block
CLSHFT	= 0.		/ shift to multiply by CLSIZE
BSIZE	= 512.*CLSIZE	/ logical block size
INOSIZ	= 32.		/ size of inode in bytes
INADDRS	= 8.		/ number of block pointers in inode
ADDROFF	= 8.		/ offset of first address in inode
LARGE	= 10000		/ flag set in first word of inode if large
INOPB	= 16.		/ inodes per logical block = BSIZE / INOSIZ
INOFF	= 31.		/ inode offset = (INOPB * (SUPERB+1)) - 1
PBSHFT	= -4.		/ shift to divide by inodes per block
ROOTINO	= 1.		/ the inode number of the root directory
DIRSIZ	= 16.		/ the size of a directory entry
NAMOFF	= 2.		/ the start of a file name in an entry
NAMLEN	= 14.		/ the length of a file name

/ this bootstrap moves itself to the end of core:

ENDCORE	= 160000	/ end of core, mem. management off

.. = ENDCORE-512.	/ where we will move ourselves to

/ establish sp, copy program up to end of core.

	0240			/ These two lines must be present or DEC
	br	start		/ boot ROMs will refuse to run boot block!
start:
	mov	r0,unit		/ Save unit number passed by ROMs
	mov	r1,raip		/ save csr passed by ROMs
	mov	$..,sp
	mov	sp,r1
	clr	r0		/ we're initially loaded at 0
1:
	mov	(r0)+,(r1)+	/ relocate ourself
	cmp	r1,$end
	blo	1b
	jmp	*$2f		/ jump to the relocated copy

/ on error, restart from here
	
restart:
	clr	r0
	
/ clear core to make things clean
	
2:
	clr	(r0)+
	cmp	r0,sp
	blo	2b

/ initialize MSCP controller

	mov	$RASTEP1,r0
	mov	raip,r1
	clr	(r1)+			/ go through controller init seq.
	mov	$icons,r2
1:
	bit	r0,(r1)
	beq	1b
	mov	(r2)+,(r1)
	asl	r0
	bpl	1b
	mov	$ra+RARSPREF,*$ra+RARSPL / set controller characteristics
	mov	$ra+RACMDREF,*$ra+RACMDL
	mov	$RASTCON,r0
	jsr	pc,racmd
	mov	unit,*$ra+RAUNIT	/ bring boot unit online
	mov	$RAONLIN,r0
	jsr	pc,racmd

/ set up to boot from the name in bootnm, and get the root inode

	mov	$bootnm, r1		/ constant boot name
	mov	$ROOTINO,r0		/ root inode number
	jsr	pc,iget			/ fetch it
	clr	r2			/ we know offset is 0 for root

/ now read through the root directory, looking for the name

again:
	jsr	pc,readdir
	beq	restart			/ error - restart
	mov	r0,r3
	add	$NAMOFF,r3		/ r3 = name in directory
	mov	r1,r5			/ r5 = wanted name
	mov	$NAMLEN,r4		/ r4 = max name length
9:
	cmpb	(r3)+,(r5)+		/ compare characters
	bne	again			/ no match - go read next entry
	tst	(r5)			/ check if both ended here
	beq	9f			/ yes - we have a match
	sob	r4,9b			/ loop until max length
9:	
	mov	(r0),r0			/ r0 = inode number of boot
	jsr	pc,iget			/ fetch boot's inode
	br	loadfile		/ go read it

/ get the inode specified in r0

iget:
	add	$INOFF,r0		/ inode number if counting from start of disk
	mov	r0,r5			/ save in r5
	ash	$PBSHFT,r0		/ block number of inode block in r0
	bic	$!7777,r0		/ limit to maximum 4095
	mov	r0,dno			/ block number now stored in dno
	clr	r0
	jsr	pc,rblk			/ go read it
	bic	$!17,r5			/ inode number modulo 15 in r5 - $!($INOPB-1)
	mov	$INOSIZ,r0
	mul	r0,r5			/ offset in block of this inode
	add	$buf,r5			/ offset in buf (where it is now stored)
	mov	$inod,r4		/ address of inode storage area
1:
	movb	(r5)+,(r4)+		/ copy inode to $inod
	sob	r0,1b			/ r0 holds inode size from a few lines up
	rts	pc

/ read a directory entry

readdir:
	bit	$BSIZE-1,r2		/ check for offset
	bne	1f			/ branch if there is
	jsr	pc,rmblk		/ read mapped block (bno) into buf
		br err			/ end of file branch
	clr	r2			/ start at beginning of buf
1:
	mov	$buf,r0			/ address of buffer in r0
	add	r2,r0			/ add current offset to point at entry
	add	$DIRSIZ,r2		/ bump r2 up to point to next entry
	tst	(r0)			/ inode == 0?
	beq	readdir			/ yes - go look at next
	rts	pc			/ return with r0 pointing to inode
err:
	clr	r0			/ return with NULL pointer
	rts	pc

/ read file into core until a mapping error (no disk address)
	
loadfile:
	clr	bno			/ start at block 0 of inode in 'inod'
	clr	r1
1:
	jsr	pc,rmblk		/ read one block
		br 1f			/ on error, assume done
	mov	$buf,r2			/ get buffer address
2:
	mov	(r2)+,(r1)+		/ copy buffer to right place in memory
	cmp	r2,$buf+BSIZE
	blo	2b
	br	1b
	
/ relocate core around assembler header
	
1:
	clr	r0
	cmp	(r0),$407		/ check for 0407 header
	bne	2f			/ skip if not present
1:
	mov	20(r0),(r0)+		/ move everything down to overwrite header
	cmp	r0,sp
	blo	1b

/ enter program and restart if it returns

2:
	jsr	pc,*$0
	jmp	restart

/ routine to read in block number specified by bno after applying file system
/ mapping algorithm in inode. bno is incremented, success return is a skip,
/ error (eof) is direct return.
	
rmblk:
	add	$2,(sp)			/ assume success
	mov	bno,r0			/ get next block number to read
	inc	bno			/ bump for next round
	bit	$LARGE,mode		/ are we using indirect blocks?
	bne	1f			/ go to large algoritm if so
	asl	r0			/ convert byte offset to word
	mov	addr(r0),r0		/ get the actual block number
	bne	rblka			/ if non-zero, go read it, else fail
2:
	sub	$2,(sp)			/ register failure
	rts	pc			/ failure return
	
/ large algorithm - inode has indirect instead of direct blocks

1:
	clr	-(sp)			/ zero next word below stack
	movb	r0,(sp)			/ stash the block counter modulo 256
	clrb	r0			/ calculate which indirect pointer
	swab	r0			/   within the inode we are at
	asl	r0			/ convert byte offset to word
	mov	addr(r0),r0		/ get the actual indirect block number
	beq	2b			/ if zero, go do a failure return
	jsr	pc,rblka		/ fetch the indirect block
	mov	(sp)+,r0		/ get the (mod 256) block counter back
	asl	r0			/ convert byte offset to word
	mov	buf(r0),r0		/ get the actual direct block number
	beq	2b			/ if zero, go do a failure return
rblka:
	mov	r0,dno			/ put physical block number in dno
	br	rblk			/ go to rblk, which will read and return

/ RA MSCP read block routine.  This is very primitive, so don't expect
/ too much from it.  Note that MSCP requires large data communications
/ space at end of ADDROFF for command area.
/ N.B.  This MUST preceed racmd - a "jsr/rts" sequence is saved by
/	falling thru!
/
/	dno	->	block # to load
/	buf	->	address of buffer to put block into
/	BSIZE	->	size of block to read
/ 
/ Tim Tucker, Gould Electronics, August 23rd 1985

rblk:
	mov	dno,r0
	ash	$CLSHFT,r0
	mov	r0,*$ra+RALBNL		/ Put in disk block number
	mov	$BSIZE,*$ra+RABYTECT	/ Put in bytes to transfer
	mov	$buf,*$ra+RABUFL	/ Put in disk buffer location
	mov	$RAREAD,r0

/ perform MSCP command -> response poll version

racmd:
	movb	r0,*$ra+RAOPCODE	/ fill in command type
	mov	$MSCPSIZE,*$ra+RARSPS	/ give controller struct sizes
	mov	$MSCPSIZE,*$ra+RACMDS
	mov	$RASEMAP,*$ra+RARSPH	/ set mscp semaphores
	mov	$RASEMAP,*$ra+RACMDH
	mov	*raip,r0		/ tap controllers shoulder
	mov	$ra+RACMDH,r0
1:
	tst	(r0)
	bmi	1b			/ Wait till command read
	mov	$ra+RARSPH,r0
2:
	tst	(r0)
	bmi	2b			/ Wait till response written
	mov	$ra+RACMDI,r0
	clr	(r0)+			/ Tell controller we saw it, ok.
	clr	(r0)			/ Tell controller we go it
	rts	pc

icons:	RAERR + 033
	ra+RARING
	0
	RAGO

bootnm:	<unix\0\0>
unit: 0			/ unit number from ROMs
raip: 0			/ csr address from ROMs
end:

inod = ..-512.-BSIZE	/ room for inod, buf, stack
mode = inod
addr = inod+ADDROFF	/ first address in inod
buf = inod+INOSIZ
bno = buf+BSIZE
dno = bno+2
ra = dno + 2		/ ra mscp communications area (BIG!)
