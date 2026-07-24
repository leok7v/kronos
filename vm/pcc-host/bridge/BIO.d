DEFINITION MODULE BIO; (* Leo 04-Sep-89. (c) KRONOS *)

(* Интерфейс файловой системы. *)

IMPORT SYSTEM;

TYPE FILE;
     PATHs;

VAL null: FILE;
      cd: FILE;     (* рабочая директория           *)

    here: PATHs;    (* "."   path                   *)
   empty: PATHs;    (* empty path                   *)
   cmask: BITSET;   (* creation mask                *)
   iolen: INTEGER;  (* number of bytes transmited   *)
                    (* by last read/write operation *)

    done: BOOLEAN;
   error: INTEGER;  ename: ARRAY [0..31] OF CHAR;

PROCEDURE open  (VAR file: FILE; path,mode: ARRAY OF CHAR);
PROCEDURE create(VAR file: FILE; path,mode: ARRAY OF CHAR;
                                      size: INTEGER);
PROCEDURE close (VAR file: FILE);
PROCEDURE purge (VAR file: FILE);
PROCEDURE flush (    file: FILE);
PROCEDURE link  (    file: FILE; path,mode: ARRAY OF CHAR);
PROCEDURE unlink(                path: ARRAY OF CHAR);
PROCEDURE mkdir (                path: ARRAY OF CHAR; hidden: BOOLEAN);

PROCEDURE equal(f0,f1: FILE): BOOLEAN;

PROCEDURE chmode(file: FILE; mode: ARRAY OF CHAR);
(* try change file mode *)

PROCEDURE fname(file: FILE; VAR name: ARRAY OF CHAR);

PROCEDURE mknode(path,device_name: ARRAY OF CHAR);

PROCEDURE dup(VAR dup: FILE; file: FILE);

PROCEDURE mount  (path,dev,info: ARRAY OF CHAR;
                        VAR lab: ARRAY OF CHAR; ro: BOOLEAN);
PROCEDURE unmount(path: ARRAY OF CHAR;      method: INTEGER);
(* method=0 unmount only if all ok *)
(* method=1 unmount only if all ok, otherwise all bad files purged *)
(* method=2 unconditionly unmount *)

PROCEDURE seek(file: FILE; offset,origin: INTEGER);
PROCEDURE pos (file: FILE): INTEGER;
PROCEDURE eof (file: FILE): INTEGER;

PROCEDURE cut   (file: FILE; size: INTEGER);
PROCEDURE end   (file: FILE; size: INTEGER);
PROCEDURE extend(file: FILE; size: INTEGER);

CONST
   is_file = {0};        is_dir  = {1};
   is_disk = {2};        is_tty  = {3};
   is_spec = {4};        is_sys  = {5};

PROCEDURE kind(file: FILE): BITSET;

PROCEDURE is_hidd(f: FILE): BOOLEAN;

PROCEDURE get_attr(file: FILE; no: INTEGER; VAR val: SYSTEM.WORD);
PROCEDURE set_attr(file: FILE; no: INTEGER;     val: SYSTEM.WORD);
CONST (* any attribute may be read by everyone *)
  a_links = 0;  -- read always, write for superuser only
  a_inode = 1;  -- read always, write for superuser only
  a_ctime = 2;  -- read always, write for owner     only
  a_wtime = 3;  -- read always, write for owner     only
  a_gid   = 4;  -- read only
  a_uid   = 5;  -- read only
  a_pro   = 6;  -- read only

PROCEDURE du    (cd: FILE; VAR free,used: INTEGER);
PROCEDURE fstype(f : FILE; VAR type,blocksize: INTEGER);
(* type 0: Excelsior-II, 1: Excelsior-iV *)

PROCEDURE buffers(f: FILE; no,len: INTEGER);

PROCEDURE check_io(halt_on_error: BOOLEAN);

PROCEDURE read (file: FILE; buf: SYSTEM.ADDRESS; len: INTEGER);
PROCEDURE write(file: FILE; buf: SYSTEM.ADDRESS; len: INTEGER);

PROCEDURE fread (file: FILE; buf: SYSTEM.ADDRESS; pos,len: INTEGER);
PROCEDURE fwrite(file: FILE; buf: SYSTEM.ADDRESS; pos,len: INTEGER);

PROCEDURE get(file: FILE; VAR data: ARRAY OF SYSTEM.WORD; len: INTEGER);
PROCEDURE put(file: FILE;     data: ARRAY OF SYSTEM.WORD; len: INTEGER);

------------------------ STREAMS -------------------------------

PROCEDURE getch(file: FILE; VAR ch: CHAR);
PROCEDURE putch(file: FILE;     ch: CHAR);

PROCEDURE getstr(file: FILE; VAR str: ARRAY OF CHAR; pos: INTEGER);
PROCEDURE putstr(file: FILE;     str: ARRAY OF CHAR; pos: INTEGER);

PROCEDURE print(file: FILE; fmt: ARRAY OF CHAR; SEQ args: SYSTEM.WORD);

---------------------------------------------------------------

CONST (* protection bits: *)
  own_exec   =  {0};       gro_exec   =  {4};        oth_exec   =  {8};
  own_write  =  {1};       gro_write  =  {5};        oth_write  =  {9};
  own_read   =  {2};       gro_read   =  {6};        oth_read   = {10};
  own_search = own_exec;   gro_search = gro_exec;    oth_search = oth_exec;

  link_pro  = {12};
  run_priv  = {14};   -- may be seted by superuser only
  run_uid   = {15};

PROCEDURE access(file: FILE): BITSET;
(* {own_exec..own_unlink,run_uid,run_gid} subset for current user *)

PROCEDURE owner(file: FILE; VAR owner,group: INTEGER);

PROCEDURE chcmask(mask: BITSET);

PROCEDURE chaccess(file: FILE; mask: BITSET);
PROCEDURE chowner (file: FILE; owner,group: INTEGER);

PROCEDURE fopen  (cd: FILE; VAR file: FILE; path,mode: ARRAY OF CHAR);
PROCEDURE fcreate(cd: FILE; VAR file: FILE; path,mode: ARRAY OF CHAR;
                                                       size: INTEGER);

PROCEDURE flink  (cd: FILE;     file: FILE; path,mode: ARRAY OF CHAR);
PROCEDURE funlink(cd: FILE;     path: ARRAY OF CHAR);
PROCEDURE fmkdir (cd: FILE;     path: ARRAY OF CHAR; hidden: BOOLEAN);

PROCEDURE fmknode(cd: FILE;      path,name: ARRAY OF CHAR);

PROCEDURE fmvdir (from,to: FILE; path,name: ARRAY OF CHAR; hidden: BOOLEAN);

PROCEDURE fmount  (cd: FILE; path,dev,info: ARRAY OF CHAR;
                                  VAR  lab: ARRAY OF CHAR; ro: BOOLEAN);
PROCEDURE funmount(cd: FILE; path    : ARRAY OF CHAR;  method: INTEGER);

PROCEDURE splitpathname(VAR path: ARRAY OF CHAR; VAR name: ARRAY OF CHAR);

PROCEDURE lock(milisec: INTEGER; file: FILE); (* delay *)
PROCEDURE unlock(                file: FILE);

PROCEDURE chdir (path: ARRAY OF CHAR);
PROCEDURE chroot(path: ARRAY OF CHAR);

PROCEDURE open_paths (VAR dirs: PATHs; pathnames: ARRAY OF CHAR);
PROCEDURE close_paths(VAR dirs: PATHs);
PROCEDURE get_paths  (VAR dirs: PATHs;   envname: ARRAY OF CHAR);

PROCEDURE lookup(dirs: PATHs; VAR file: FILE; name,mode: ARRAY OF CHAR);

CONST
   s_none = 00;   s_reverse = 01;
   s_name = 04;   s_dirfwd  = 02;

PROCEDURE dir_walk(cd: FILE; sort_kind: INTEGER);

PROCEDURE get_entry(cd: FILE; VAR name: ARRAY OF CHAR;
                              VAR mode: BITSET): BOOLEAN;
CONST -- file entry modes:
  e_file   = { };
  e_dir    = {0};
  e_hidden = {1};
  e_esc    = {2};
  e_sys    = {3};

PROCEDURE restart_walk(cd: FILE);

PROCEDURE end_walk (cd: FILE);

PROCEDURE mkfs  (device: FILE;
                 fstype: INTEGER;
                 block : INTEGER;
                 label : ARRAY OF CHAR;
                 bads  : ARRAY OF INTEGER);


--------------  for Xackers' applications only  -----------------
              ----------------------------------

PROCEDURE doio(f: FILE; VAR r: ARRAY OF SYSTEM.WORD (*defRequest.REQUEST*));

-------------------  П Р И М Е Ч А Н И Я  ----------------------
                   -----------------------

(***************************************************************

open & create modes:

     'r'  (read)    read  only
     'w'  (write)   write only
     'm'  (modify)  read and write
     'x'  (execute) execute or search
     'X'  (eXelent) max available modes
     'а'  (append)  seek to eof after open
     'd'  (direct write) wait until write finished
     'n'  (nodelay) read without wait
     'c'  (no casch) without system cash
     'h'  create hidden file

***************************************************************)

END BIO.
