
/*
 *  K R O N O S   C  Librarian 04.Apr.87
 *
 * 20.Oct.87 help; Kronos 2 object files also
 * 16.Nov.87 Kronos time
 * 24.Feb.88 version for new object file format
 */

#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include "asm.h"
#include "mkinstr.h"

#define  MAXFILE 20

#define  F_OBJ 010
#define  F_LIB 020


/*  table of files */

struct {
 char name [MAXIDENT + 1];
 int flag;
 }
  files [MAXFILE];

int filenr;

typedef  struct  {
 char name [MAXIDENT + 1];
 int creat_time;
 int varno;
 int funno;
 int offset;
 }
  FILEREC;

struct {
 int version;
 int creat_time;
 int filenum;
 FILEREC files[MAXFILE];
 }
  header;

typedef  struct {
 char name [MAXIDENT + 1];
 int num;
 }
  EXPTSYM;

#define MAXSYM 2000

EXPTSYM  symtab [MAXSYM];
int symbols;

int debug, verbose, nerrors, fsize;

char libname [80] = "", tempname[80];

void error(), doit(), getsymbols();
char * malloc(), * calloc(), * strrchr();

long mytime;
extern long time ();

/*
 * ----- M A I N  -----
 */

main (argc, argv)
char * argv [];
{
int i;
char * cp, * strchr();

 fprintf (stderr, "Kronos C Librarian [2.5] /24-Feb-88/\n");
 
 if (argc < 2) {

printf("\nUsage : libr file.o [file.o].. -o file\n\n");
printf("-o <file> name of library file (.lib may be omitted)\n");
printf("  NB! if the output file exists, LIBR will fail\n");
printf("-v  verbose\n\n");
printf("Limits:\n");
printf("\tnumber of object files: %d\n", MAXFILE);
printf("\tsymbol table size: %d\n", MAXSYM);
printf("\nThese limits are set according to standard library requirements.\n");
printf("NB! All files must reside in the current directory.\n");
printf("LIBR is actually a development tool for Kronos C implementors.\n");
exit(1);
  }

 /* set time */
 mytime = time ((long *)0);
#if unix
 mytime -= MAGICTIME;
#endif

 for (i = 1; i < argc; i ++) {
  if (argv[i][0] == '-') {
      switch (argv[i][1]) {
   case 'd':
#ifdef DEBUG
    debug ++;
#else
    fprintf(stderr, "No debugging\n");
#endif
    break;
   case 'o':
    if (++i == argc)
     error("\nSyntax error: -o\n");
    strcpy (libname, argv[i]);
    break;
   case 'v':
    verbose ++;
    break;
   default:
    error ("\nUnknown flag : %s\n", argv[i]);
   }
      continue;
      }
  addfile (argv[i]);
  }
 if (filenr == 0)
  error ("\nNo files given\n");
 if (libname[0] == '\0')
  error ("\nNo output file name\n");

#ifdef DEBUG
 if (debug)
  for (i = 0; i < filenr; i ++)
   printf("%d - %s flag=%d\n", i,
    files[i].name, files[i].flag);
#endif
  
 if (cp = strchr(libname, '.')) {
  if (strcmp(cp, SUF_LIB))
   error ("\nIllegal library file name : %s\n", libname);
  }
 else strcat (libname, SUF_LIB);

 getsymbols ();

 if (nerrors) exit (1);

 doit();

 exit (0);
}


void error (s, t)  /* fatal error: can't continue */  /* host-port: was static, conflicts with K&R fwd decl */
char * s, * t;
{
 fprintf (stderr, s, t);
 exit (1);
}


addfile (s) /* add a file to files' table */
char * s;
{
char * cp, *strchr();
int i;

 if (cp = strchr(s, '.')) {
  if (strcmp(cp, SUF_OBJ))
   error ("Illegal file name : %s\n", s);
  * cp = '\0';
  }
 else error ("Illegal file name: %s\n", s);

 /* check if file listed already */
 for (i = 0; i < filenr; i ++)
  if (!strcmp(s, files[i].name))
   error ("File occurs twice: %s\n", s);

 if (++ filenr == MAXFILE)
  error ("Too many files\n");
 strncpy(files[filenr - 1].name, s, MAXIDENT + 1);
 files[filenr - 1].flag = F_OBJ;
}


void addsymbol (sym)
EXPTSYM  * sym;
{
EXPTSYM  * lastsym, * i;
int c;

 c = sym->name[0];
 lastsym = & symtab [symbols];
 for (i = & symtab [0]; i < lastsym; i ++) 
  if (c == *(i->name) && !strcmp(sym->name, i->name)) {
   fprintf(stderr, "Multiple definition: %s\n", i->name);
   nerrors ++;
   }
 if (++symbols == MAXSYM)
  error ("Symbol table overflow\n");
 strcpy (i->name, sym->name);
 i->num = sym->num;
}


void getsymbols () /* read export symbols of all files */
{
int i, j, fd, varno, funno, otime, poolsize;
EXPTSYM  sym;int oheader [HDSIZE];
char * s = (char *) & sym;

    fsize = filenr * sizeof(FILEREC) + 12;

    for (i = 0; i < filenr; i ++) {

 if (files[i].flag == F_OBJ) { /*  OBJECT FILE */

 /* open file */
  strcpy (tempname, files[i].name);
  strcat (tempname, SUF_OBJ);
  if ((fd = open (tempname, O_RDONLY)) == -1)
   error ("No file: %s\n", tempname);
 /* read header & check version */
  read (fd, (char *) & oheader[0], 4 * HDSIZE);
  if (oheader[0] != OBJ2_VERSION && oheader[0] != OBJ25_VERSION)
   error ("Not an object file: %s\n", tempname);
  if (oheader [11] != 0)
   error ("File %s contains _main()\n", tempname);
 /* get necessary info, position at start of export symbols */
  otime = oheader [1];
  varno = oheader [2];
  funno = oheader [3];
  poolsize = oheader [6];
  lseek (fd, (long)(poolsize * 4), 1);
 /* fill header */
  strcpy (header.files[i].name, files[i].name);
  header.files[i].creat_time = otime;
  header.files[i].offset = fsize;
  header.files[i].varno = varno;
  header.files[i].funno = funno;
  fsize += ((varno + funno) * sizeof (EXPTSYM));
 /* read & insert export variables */
  for (j = 0; j < varno; j ++) {
   read (fd, s, sizeof (EXPTSYM));
   addsymbol (& sym);
   }
 /* read & insert export functions */
  for (j = 0; j < funno; j ++) {
   read (fd, s, sizeof (EXPTSYM));
   addsymbol (& sym);
   }
 /* close file */
  close (fd);
  }
 }
}

void doit ()  /* form library file */
{
int hdsize, fdlib;

   /* create output (library) file */
 if ((fdlib = open(libname, O_CREAT|O_EXCL|O_WRONLY, 0777)) == -1)
  error ("\nCan't create library file\n");

 fprintf(stderr, "Creating %s ...\n", libname);

   /* fill & write out header */
 hdsize = filenr * sizeof(FILEREC) + 12;
 header.version = LIB_VERSION;
 header.creat_time = mytime;
 header.filenum = filenr;
 write (fdlib, (char *)  & header, (unsigned) hdsize);

   /* write export info */
 write (fdlib, (char *) & symtab [0], symbols * sizeof (EXPTSYM));
   /* close output file */
 close (fdlib);
 fprintf(stderr, "\n");
 if (verbose)
  fprintf(stderr, "File size %d bytes\n", fsize);
}

