struct {
	int d_inum;
	char d_name[16];	/* need trailing zero */
} dbuf;

struct {
	int s_dev;
	int s_inum;
	int fill[16];
} sbufp, sbufc, sbufr;

int fd;
int fdout 1;
int cc;			/* component counter */

main()
{

	stat("", &sbufc);
	prnam();
	if (fdout == 2)
		write(fdout, "/.\n", 3);
	else
		write(fdout, "\n", 1);
	exit(--fdout);
}

prnam()
{
	char name[16];
	register char *np, *dp;

	if (chdir("..") != 0) {
		fdout = 2;
		write(fdout, "cannot chdir to parent of ?", 27);
		return;
	}
	fd = open("", 0);
	if (fd < 0) {
		fdout = 2;
		write(fdout, "cannot read parent of ?", 23);
		return;
	}
	fstat(fd, &sbufp);
	if (sbufc.s_inum == sbufp.s_inum && sbufc.s_dev == sbufp.s_dev) {
		if (cc == 0)
			write(fdout, "/", 1);
		return;
	}

	seek(fd, 32, 0);
	if (sbufc.s_dev == sbufp.s_dev) {
		do
			read(fd, &dbuf, 16);
		while(dbuf.d_inum != sbufc.s_inum);
	} else
		for (;;) {
			read(fd, &dbuf, 16);
			if (dbuf.d_inum) {
				stat(dbuf.d_name, &sbufr);
				if (sbufr.s_inum == sbufc.s_inum &&
				    sbufr.s_dev == sbufc.s_dev)
					break;
			}
		}
	dp = dbuf.d_name;
	np = name;
	while (*np++ = *dp++)
		;

	close(fd);
	cc++;
	sbufc.s_inum = sbufp.s_inum;
	sbufc.s_dev = sbufp.s_dev;
	prnam();
	write(fdout, "/", 1);
	for (np = name; *np; np++)
		write(fdout, np, 1);
}
