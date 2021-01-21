CFLAGS=-O
OBJECTS=alloc.o clock.o fio.o iget.o main.o malloc.o nami.o pipe.o prf.o rdwri.o sig.o slp.o subr.o sys1.o sys2.o sys3.o sys4.o sysent.o text.o trap.o
../lib1:: $(OBJECTS)
	ar rv $@ $?
sys1.o sys2.o sys3.o sys4.o sysent.o trap.o: ../V7.h
