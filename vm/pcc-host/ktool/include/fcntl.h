/*    @(#)fcntl.h	1.3    */
/* Flag values accessible to open(2) and fcntl(2) */
/*  (The first three can only be set by open) */
#define	O_RDONLY 0
#define	O_WRONLY 1
#define	O_RDWR	 2
#define	O_NDELAY 04	/* Non-blocking I/O */
#define	O_APPEND 010	/* append (writes guaranteed at the end) */

/* Flag values accessible only to open(2) */
#define	O_CREAT	00400	/* open with file create (uses third open arg)*/
#define	O_TRUNC	01000	/* open with truncation */
#define	O_EXCL	02000	/* exclusive open */
