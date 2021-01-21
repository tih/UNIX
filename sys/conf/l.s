
/ Copyright 1975 Bell Telephone Laboratories Inc
/ low core

br4 = 200
br5 = 240
br6 = 300
br7 = 340

. = 0^.
	br	1f
	br7+10.		/random trap

/ trap vectors
	trap; br7+0.		/ bus error
	trap; br7+1.		/ illegal instruction
	trap; br7+2.		/ bpt-trace trap
	trap; br7+3.		/ iot trap
	trap; br7+4.		/ power fail
	trap; br7+5.		/ emulator trap
	trap; br7+6.		/ system entry

. = 40^.
.globl	start, dump
1:
	jmp	start
	jmp	dump


. = 60^.
	klin; br4
	klou; br4

.if pc
. = 70^.
	pcin; br4
	pcou; br4
.endif

. = 100^.
	kwlp; br6
	kwlp; br6

. = 114^.
	trap; br7+7.		/ 11/70 parity

.if xy
. = 120^.
	xyou; br5		/ xy plotter (120)
.endif

.if rl
. = 160^.
	rlint; br5
.endif

.if ic
. = 170^.
	icio; br4		/ pdp8 interface (170)
.endif

.if ir
. = 174^.
	irin; br4		/ digitorg interface
.endif

.if lp
. = 200^.
	lpou; br4

.endif

.if tc
. = 214^.
	tcio; br6
.endif

.if rk
. = 220^.
	rkio; br5
.endif

.if tm
. = 224^.
	tmio; br5
.endif

.if cr
. = 230^.
	crin; br7
.endif

.if pir
. = 240^.
	pirin; br7+7.		/ programmed interrupt
.endif
. = 244^.
	trap; br7+8.		/ floating point
. = 250^.
	trap; br7+9.		/ segmentation violation
.if hm
. = 254^.
	hmin; br5		/hm interrupt 
.endif

.if rp
. = 254^.
	rpin; br5		/rp interrupt 
.endif

.if rx2
. = 264^.
	rx2int; br5		/floppy disk interrupt
.endif

. = 300^.
.if dl
	klin; br4+1
	klou; br4+1
.endif

.if dl2
	klin; br4+2
	klou; br4+2
.endif

.if dl3
	klin; br4+3
	klou; br4+3
.endif

.if dl4
	klin; br4+4
	klou; br4+4
.endif

.if dl5
	klin; br4+5
	klou; br4+5
.endif

.if dup
. = 320^.
	dupin; br7
	dupout;br7
.endif

.if dz
. = 330^.
	dzin; br5
	dzou; br5
.endif

.if dz2
. = 340^.
	dzin; br5+1
	dzou; br5+1
.endif

.if dz3
. = 350^.
	dzin; br5+2
	dzou; br5+2
.endif

.globl CSW
.if 1-ad
.if 1-csw
.	=	400-2^.
CSW:	.+2
	173030
.endif
.endif

.if ad
.=400^.
	adin; br5+0
	aderr; br5+0
.if 1-csw
CSW:	.+2
	173030
.endif
.endif


.if csw
CSW:	177570
.endif

.globl _lks
.if ltc
/ simulated location for 11/23 line freq clock
_lks:	.+2
. = .+2
.endif

.if 1-ltc
_lks: 0
.endif
//////////////////////////////////////////////////////
/		interface code to C
//////////////////////////////////////////////////////

.globl	call, trap

.globl	_klrint
klin:	jsr	r0,call; _klrint
.globl	_klxint
klou:	jsr	r0,call; _klxint

.if dup
.globl _dupxintr
.globl _duprintr
dupin:	jsr	r0,call; _duprintr
dupout:	jsr	r0,call; _dupxintr
.endif

.if pc
.globl	_pcrint
pcin:	jsr	r0,call; _pcrint
.globl	_pcpint
pcou:	jsr	r0,call; _pcpint
.endif

.globl	_clock
kwlp:	jsr	r0,call; _clock


.if lp
.globl	_lpint
lpou:	jsr	r0,call; _lpint
.endif

.if tc
.globl	_tcintr
tcio:	jsr	r0,call; _tcintr
.endif

.if rk
.globl	_rkintr
rkio:	jsr	r0,call; _rkintr
.endif

.if cr
.globl	_crint
crin:	jsr	r0,call; _crint
.endif

.if xy
.globl	_xyint
xyou:	jsr	r0,call; _xyint
.endif

.if ic
.globl	_icint
icio:	jsr	r0,call; _icint
.endif

.if ir
.globl	_irint
irin:	jsr	r0,call; _irint
.endif

.if dz
.globl _dzrint,_dzxint
dzin:	jsr	r0,call; _dzrint
dzou:	jsr	r0,call; _dzxint
.globl _dzaddr
_dzaddr:
	160100	/first dz address
.endif

.if tm
.globl _tmintr
tmio:	jsr	r0,call; _tmintr
.endif

.if rx2
.globl _rx2intr
rx2int:	jsr	r0,call; _rx2intr
.endif

.if hm
.globl _hmintr
hmin:	jsr	r0,call; _hmintr
.endif

.if rp
.globl _rpintr
rpin:	jsr	r0,call; _rpintr
.endif

.if pir
.globl _pirint
pirin:	jsr	r0,call; _pirint
.endif

.if rl
.globl _rlintr
rlint:	jsr	r0,call; _rlintr
.endif

.if ad
.globl _adrint, _aderr
adin:	jsr	r0,call; _adrint
aderr:	jsr	r0,call; _aderr
.endif
