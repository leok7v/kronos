/* SS. 19.Oct.87 module representing excII environment list, used by C-RTS
   functions: getenv(), putenv(). This is the only module which belongs to
   recompiling, when moving C programming system to the new KRONOS.  */

/* NB! this file must be renamed to "env.c" before recompiling */
/* NB! special version of env.c for using with "BKRON" in UNIX environment */

char *environ[] = {
 "TMPDIR=/we/C/tmp/",     /* place for temporary files, used by asm */
 "CDIR=/we/C/bin/",       /* place for files "pccerr.msg","clib.lib" */
 "INCDIR=/we/C/include",  /* standard place for header files */
 "TERM=labtam",           /* not used yet */
 "TZ=EST-10",             /* not used yet */
 0 };

