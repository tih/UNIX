#
/* Copyright Bell Labs and UBC 1983 */
/* Configuration for mlab  */
 int	(*bdevsw[])()
 {
	&nodev,		&nodev,		&nodev,		0,	/*  0    rk */
	&nodev,		&nodev,		&nodev,		0,	/*  1    rp */
	&nulldev,	&nulldev,	&rlstrategy,	&rltab, /*  2 rl rl */
	&nodev,		&nodev,		&nodev,		0,	/*  3    tm */
	&nodev,		&nodev,		&nodev,		0,	/*  4    tc */
	&rx2open,	&nulldev,	&rx2strategy,	&rx2tab,/*  5 rx rx2 */
	&nodev,		&nodev,		&nodev,		0,	/*  6    hm */
	0
};
 int	(*cdevsw[])()
 {
	&klopen,   &klclose,  &klread,   &klwrite,  &klioctl,	/*  0 tty console */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/*  1     pc */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/*  2     lp */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/*  3     dc */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/*  4     dh */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/*  5     dp */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/*  6     dj */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/*  7     dn */
	&nulldev,  &nulldev,  &mmread,   &mmwrite,  &nodev,	/*  8 mem mem */
	&syopen,   &nulldev,  &syread,   &sywrite,  &sysgtty,   /*  9 tty sys */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/* 10     rk */
	&rx2open,  &nulldev,  &rx2read,  &rx2write, &rx2sgtty,	/* 11 rrx rx2 */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/* 12     rp */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/* 13     tm */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/* 14     ir */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/* 15     hm */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/* 16     cr */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/* 17     xy */
	&nulldev,  &nulldev,  &rlread,   &rlwrite,  &nodev,	/* 18 rrl rl */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/* 19     dup */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/* 20     dz */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/* 21     stat */
	&nodev,    &nodev,    &nodev,    &nodev,    &nodev,	/* 22     fake */
        &adopen,   &adclose,  &adread,   &nodev,    &adsgtty,   /* 23     ad */
	0
};

char *devnames[] {
 "rk", "rp", "rl", "mt", "tap", "rx", "hm", "dn", "mem", "tty", "rrk", "rrx", "rrp", "rmt", "ir", "rhm", "cr", "xy", "rrl", "dup", "tty", "stat", "tty", "ad",
 0 };

#define device(x,y) { (x<<8)+y }
int rootdev device(2,0);
int swapdev device(2,0);
int nswap 1480;
int swplo 19000;

