CC=occ
AS=-as
unix-mlab: l-mlab.o c-mlab.o f23.o ../lib1 ../lib2
	ld -x -v0 l-mlab.o f23.o c-mlab.o ../lib1 ../lib2
	size; mv a.out unix-mlab
l-mlab.o: l-mlab.s l.s
	-as -o l-mlab.o l-mlab.s l.s
