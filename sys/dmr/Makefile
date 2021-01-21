OBJECTS= ad.o bio.o cat.o cr.o dc.o dh.o dhdm.o dhfdm.o dn.o dp.o dup.o dz.o fake.o hm.o hp.o hs.o ht.o ic.o ioctl.o ir.o kl.o lp.o mem.o partab.o pc.o pir.o rf.o rk.o rl.o rp.o rx2.o stat.o sys.o tc.o tm.o tty.o ttyi.o vs.o vt.o xp.o xy.o
CFLAGS=-O
../lib2:: $(OBJECTS)
	ar rv $@ $?
ioctl.o: ../V7.h

rl.o: rl.c
	ncc -O -c rl.c
dz.o fake.o ioctl.o kl.o sys.o tty.o ttyi.o: ../tty.h
