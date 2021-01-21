BEGIN { nbdev = 0.; ncdev = 0. ; nadev = 0. ; sys="def"; CPU=23; FPU="m"}
$1 == "#" { next }	# ignore comments 
FILENAME == "Adevs" {
	dev = $1
	adevs[dev] = $0
	aname[nadev] = dev
	nadev++;
	next
	}
FILENAME == "Bdevs" {
	dev = $(NF-1)
	bdevs[dev] = $0
	bname[nbdev] = dev
	bmajor[dev] = nbdev
	if (devnames[nbdev] == "")
		devnames[nbdev] = $(NF-2)
	nbdev++;
	next
	}
FILENAME == "Cdevs" {
	dev = $(NF-1)
	cdevs[dev] = $0
	cname[ncdev] = dev
	cmajor[dev] = ncdev
	if (devnames[ncdev] == "")
		devnames[ncdev] = $(NF-2)
	ncdev++;
	next
	}

$1 == "root"	{
	if (NF != 3.)
		print "ERROR: ",FILENAME,NR,"bad swap sys",$0
	dev = $2
	n = bmajor[dev];
	if (n == "")
		print "ERROR: ",FILENAME,NR,"No such block device",$0
	wanted[dev] = "yes"
	rootdev = sprintf("int rootdev device(%d,%d);",n,$3) 
	next
	}

$1 == "swap"	{
	if (NF != 5)
		print "ERROR: ",FILENAME,NR,"bad swap sys",$0
	dev = $2
	n = bmajor[dev];
	if (n == "")
		print "ERROR: ",FILENAME,NR,"No such block device",$0
	wanted[dev] = "yes"
	swapdev = sprintf("int swapdev device(%d,%d);",n,$3) 
	nswap = sprintf("int nswap %d;\nint swplo %d;\n",$5,$4)
	next
	}

$1 == "cpu" { CPU = $2; next }
$1 == "fpu" { FPU = "f"; next }
NF == 1. || NF == 2.	{
	dev = $1
	if (cdevs[dev] == "" && bdevs[dev] == "" && adevs[dev] == "")
		print "ERROR: ",FILENAME,NR,"No such device",$0
	wanted[dev] = "yes"
	for (i=2.; i<=$2; ++i)
		{
		x = sprintf("%s%d",dev,i)
#		print "define",x
		wanted[x] = "yes"
		}
	next
	}
	{
	print "ERROR: ",FILENAME,NR,"Bad format",$0
	}
END	{
	c = sprintf("c-%s.c",sys);
	print "#" > c
	print "/* Copyright Bell Labs and UBC 1982 */" > c
	print "/* Configuration for",sys," */" >c
	print " int	(*bdevsw[])()\n {" > c
	for (i=0.; i<nbdev; ++i)
		{
		dev = bname[i];
		if (wanted[dev] != "")
			print bdevs[dev]  > c
		else
			printf("	&nodev,		&nodev,		&nodev,		0,	/* %2d    %s */\n", i, dev) > c
		}
	print "	0\n};" > c

	print " int	(*cdevsw[])()\n {" > c
	for (i=0.; i<ncdev; ++i)
		{
		dev = cname[i];
		if (wanted[dev] != "")
			print cdevs[dev]  > c
		else
			printf("	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/* %2d     %s */\n", i, dev) > c
		}
	print "	0\n};" > c
	print "\nchar *devnames[] {" >c
	for (i=0.; i<ncdev; ++i)
		printf(" \"%s\",",devnames[i]) >c
	print "\n 0 };\n" >c
	print "#define device(x,y) { (x<<8)+y }" > c
	print rootdev > c
	print swapdev > c
	print nswap > c
	c = sprintf("l-%s.s",sys);
	for (i=0.; i<nadev; ++i)
		{
		dev = aname[i];
		if (wanted[dev] != "")
			print adevs[dev]  > c
		else
			printf("%s	=	0\n",dev)  > c
		}
	c = sprintf("make.%s",sys);
	mch=sprintf("%s%d.o",FPU,CPU);
	printf("AS=-as\nunix.%s: l-%s.o c-%s.o %s ../lib1 ../lib2\n",sys,sys,sys,mch) >c
	printf("	ld -x -v0 l-%s.o %s c-%s.o ../lib1 ../lib2\n",sys,mch,sys) > c
	printf("	size; mv a.out unix.%s\n",sys) > c
	printf("l-%s.o: l-%s.s l.s\n	-as -o l-%s.o l-%s.s l.s\n",sys,sys,sys,sys) >c
	}
