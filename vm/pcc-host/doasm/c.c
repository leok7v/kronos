/* Copyright (c) 1988  KDG
 * 
 * Kronos  C  Compiler
 *
 * c  [options] file [file]...
 *
 *  File must have one of three suffixes:  .c, .i, .a. The suffix
 *  determines which passes will be run on a particular file.
 *  The output of ASM is a singular object file:
 * CPP file.c      -> file.i;  ...
 * PCC file.i      -> file.a;  ...
 * ASM file.a, ... -> file.o
 *  Normally, all temporary files created by c are deleted, source and
 *  output files retained.
 *  Options :
 * -i call CPP only, retain .i files
 * -a call CPP and/or PCC, stop at .a files
 * -o file call CPP/PCC if necessary, force ASM to make object file
 *  named <file>.o (otherwise the object file will be named
 *  after first source file listed in command line)
 *  -e echo consecutive steps on screen
 * -v  pass verbose flag to CPP, PCC, ASM
 * -d debug: display all steps, do not execute
 *  -k produce code for KRONOS P2
 * -D..  #define (for CPP)
 * -U..  #undefine (for CPP)
 * -I..  #include (for CPP)
 * -P profiling
 *
 *  Written by MK. 21.Aug.86
 *  Updated by MK. 02.Apr.87 : names, suffixes etc.; KRONOS version
 *          SS. 15.Apr.87 meaning of options changed 
 *        SS. 08.Jun.87 new options: -s(interlisting),-u(optimizations)
 *        MK. 17.Feb.88 multiple files, no linking
 *        MK. 18.Feb.88 new order: CPP & PCC together for one file
 *        MK. 13.Apr.88 -o -v check (SS..)
 *        SS. 18.Apr.88 -k flag added always if compiled with -DKRONOS2
 */


#include <stdio.h>
#include <ctype.h>
#include <string.h>


static int keep_i,  /* -i: call only CPP, retain .i files */
  keep_a,  /* -a: stop at .a files, don't assemble */
  verbose, /* -v: verbose mode     */
  echo,   /* -e: echo all steps    */
  quiet,  /* -q: send the output to "/dev/null"  */
  debug,  /* -d: don't execute    */
  kronos2; /* -k: gen.code for Kronos P2   */

static char cpp[128] = 
#if  unix
    "mycpp ",
#else
    "cpp ",
#endif
  pcc[80] = 
#if unix
    "mypcc ",
#else
    "pcc ",
#endif
  as[80]  =
#if  unix
     "Asm ";
#else
     "asm ";
#endif

static char errexec[] = "Exec failed";

#define MAXFILE   20
#define  FILE_C   0
#define FILE_I   1
#define FILE_A   2

static char *files [MAXFILE]; /* file names  */
static int flags  [MAXFILE]; /* file type flags  */
static int nfiles;   /* number of files  */
static char ofname [80];  /* ASM output file name */

static char *release = "KRONOS [P2.5] C v1.0  /14-Apr-88/  (c) 1988 KDG";

#define  MAXHELP 21

static char *help[MAXHELP] = {

"  Usage :     c  [options]  file  [file]...\n",
"  Data flow :     CPP file.c      -> file.i; ...",
"      PCC file.i      -> file.a; ...",
"      ASM file.a, ... -> file.o",
"Options :",
" -i      call CPP only",
" -a      call CPP & PCC, don't assemble",
" -o file   leave ASM output in file.o (default: first file listed)",
" -d  echo all steps, don't execute",
" -e  echo all steps before executing",
" -v  pass verbose flag to CPP, PCC, ASM",
"CPP: -D..   #define    -U..  #undefine",
" -I..    #include   -R   allow recursive macros",
" -C      retain comments   -P   don't insert line numbers",
"PCC:    -s      gen. C and assembler interlisting ( -> -a)",
"   -k     gen. code for Kronos P2",
" -u      don't perform stack optimizations",
"   -z     stricter stack overflow check",
" -q      don't gen. code, perform only syntax check",
"   -w     shut up warnings",
" -p      don't optimize parameter passing" };

main  (argc, argv)
int argc;
char * argv[];
{
int i;
char * cp;

    if (argc < 2) {
 fprintf(stderr, "%s\n\n", release);
 for (i = 0; i < MAXHELP; i ++)
  fprintf(stderr, "%s\n", help[i]);
  exit (1);
  }
 while (*++argv) {
     if (argv[0][0] == '-') {
  switch (argv[0][1]) {
   case 'i': keep_i = 1; 
      strcat (cpp, "-C "); 
      break;
   case 's': strcat(pcc, "-i "); /* NB! fall through ! */
   case 'a': keep_a = 1; 
      strcat (pcc, "-c ");
      break; 
   case 'o': if (* ofname)
     error ("  Duplicate -o", "");
      if (!(*++argv))
     error ("  No output file name", "");
      if (argv[0][0] == '-')
     error ("  Illegal file name: ",argv[0]);
      if (cp = strrchr (argv[0], '.')) 
     if (strcmp(cp, ".o"))
        error ("  Illegal suffix: ",
         argv[0]);
      strcpy (ofname, argv[0]);
      break;
   case 'u': strcat(pcc, "-O "); break;
   case 'w': strcat(pcc, "-W ");   break;
   case 'z': strcat(pcc, "-S ");   break;
   case 'p': strcat(pcc, "-p ");   break;
   case 'v': verbose ++;   break;
   case 'd': debug ++;    break;
   case 'e': echo ++;    break;
   case 'q': quiet ++;    break;
   case 'k': kronos2 ++;   break;
   case 'I':
   case 'U':
   case 'D': strcat (cpp, argv[0]);
      if (!argv[0][2]) {
     if (!(*++argv))
         error("  -IUD syntax error", "");
     strcat (cpp, argv[0]);
     }
      strcat (cpp, " "); 
      break;
   case 'R':
   case 'P':
   case 'C': strcat (cpp, argv[0]);
      strcat (cpp, " "); 
      break;
   default: 
      error ("  Unknown flag :",argv[0]);
   }
     continue;
  }
     if (nfiles + 1 == MAXFILE) 
  error ("  Too many files:", argv[0]);
     if ((cp = strrchr (argv[0], '.')) == (char *) 0)
   error ("  Illegal file name :", argv[0]);
     if (!strcmp (cp, ".c")) 
   flags [nfiles] = FILE_C;
     else if (!strcmp (cp, ".i")) 
   flags [nfiles] = FILE_I;
     else if (!strcmp (cp, ".a")) 
   flags [nfiles] = FILE_A;
     else error ("  Illegal file name :",argv[0]);
 /* NB !!   USE argv[] char space */
     * cp = '\0';
     for (i = 0; i < nfiles; i ++)
  if (!strcmp(files[i], argv[0]))
    error ("  Duplicate file :", argv[0]);
     files [nfiles ++] = argv[0];
     }

 if (debug || verbose) echo ++;
 if (echo) fprintf(stderr, "%s\n\n", release);
 if (nfiles == 0) 
  error("  No source file", "");
 doit ();
}


doit ()

{int i;
 char line [400];

/* CPP, PCC */

    if (verbose) {
 strcat(cpp, "-v "); 
 strcat(pcc, "-v ");
 }
    for (i = 0; i < nfiles; i ++)
        if (flags [i] <= FILE_I) {
     if (!echo && nfiles > 1) 
  fprintf(stderr, "%s.%s:\n", files[i],
     flags[i] == FILE_C ? "c" : "i");
     if (flags[i] == FILE_C) {
  sprintf(line, "%s %s.c > %s.i", cpp, files[i], files[i]);
  if (echo) 
   fprintf(stderr, "\t%s\n", line);
  if (!debug)
   if (system (line))
    error (errexec, "");
  }

     if (keep_i && !(keep_a || *ofname))
  continue;

     if (quiet)
         sprintf(line, "%s %s.i > /dev/null", pcc, files[i]);
     else
  sprintf(line, "%s %s.i > %s.a", pcc, files[i], files[i]);
     if (echo) 
  fprintf(stderr, "\t%s\n", line);
     if (!debug) {
  if (system (line))
   error (errexec, "");
  if (!keep_i && flags [i] != FILE_I) {
   sprintf (line, "%s.i", files[i]);
   unlink(line); /* remove .i file */
   }
  }
     }

 if (quiet)
  exit (0);
 if (!(*ofname))
  if (keep_a || keep_i)
   exit (0);

/* ASM */

 if (verbose) 
  strcat(as, "-v ");
#ifndef KRONOS2
 if( kronos2 ) 
#endif
  strcat(as, "-k ");
 strcpy (line, as);
 if (* ofname) {
  strcat (line, "-o ");
  strcat (line, ofname);
  }
 for (i = 0; i < nfiles; i ++) {
  strcat (line, " ");
  strcat (line, files[i]);
  }
 if (echo) 
  fprintf(stderr, "\t%s\n", line);
 if (!debug) {
  if (system (line))
   error (errexec, "");
  if (!keep_a) {
   for (i = 0; i < nfiles; i ++) 
        if (flags[i] != FILE_A) {
     sprintf (line, "%s.a", files[i]);
     unlink (line); /* remove .a file */
     }
   }
  }
 exit (0);
}

error (s, t)
char * s, *t;
{
 fprintf (stderr, "%s %s\n\n", s, t);
 exit (1);
}

