/*
 * statistics collection header.
 */
#define	ST_INDIR	020	/* indirect thru ptr */
#define	ST_ID		040	/* pass on ID information */
#define	ST_GID		0100	/* return group ID */
#define	ST_DEVMASK	017	/* mask for minor device */
#define	ST_IDMASK	07400	/* mask for ID code */
#define	ST_TIME		0100000	/* provide actual time */
#define	ST_ELAPSED	0040000	/* provide elapsed since last call */
#define	ST_UID		0020000	/* provide UID */
#define	ST_PID		0010000	/* provide PID */
#define	ST_CPU		0400	/* CPU ID code */
#define	ST_LP		01000	/* LP ID code */
#define	ST_ACC		ST_ELAPSED+ST_UID+ST_PID+ST_ID
