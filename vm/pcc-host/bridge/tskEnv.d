DEFINITION MODULE tskEnv; (* Ned 28-Sep-89. (c) KRONOS *)

IMPORT  SYSTEM;

VAL  id: INTEGER;       -- номер задачи
   done: BOOLEAN;
  error: INTEGER;

----------------------------------------------------------------

CONST -- имена стандартных атрибутов в окружении задачи

  args    = "ARGS";       -- строка параметров задачи
  cd      = "CD";         -- имя текущей директории
  cmask   = "CMASK";      -- маска защиты создаваемых файлов
  name    = "NAME";       -- имя задачи
  etc     = "ETC";        -- путь поиска всяких файлов
  bin     = "BIN";        -- путь поиска кодофайлов
  sym     = "SYM";        -- путь поиска симфайлов
  ref     = "REF";        -- путь поиска реффайлов
  key     = "KEY";        -- имя драйвера клавиатуры
  tty     = "TTY";        -- имя драйвера терминала (вывод)
  screen  = "SCR";        -- имя драйвера экрана
  lp      = "LP";         -- имя драйвера принтера
  cpd     = "CPD";        -- имя драйвера мыши
  plot    = "PLOT";       -- имя драйвера для Plotter'а

  shell   = "SHELL";
  info    = "INFO";       -- информационная строка задачи
  msg     = "MSG";        -- сообщения об ошибках
  stk     = "STK";        -- размер стека задачи
  user    = "USER";
  echo    = "ECHO";
  insert  = "INSERT";
  prompt  = "PROMPT";     -- приглашение командной строки
  suprompt= "PROMPT1";    -- приглашение командной строки
  chain   = "CHAIN";      --
  wndman  = "WNDMAN";     -- имя Window Manager'а
  history = "HISTORY";
  alias   = "ALIAS";
  base    = "BASE";
  task    = "TASK";
  son     = "SON";
  father  = "FATHER";
  cemetry = "CEMETRY";
  home    = "HOME";
  stdin   = "STDIN";
  stdout  = "STDOUT";

PROCEDURE get_str(name: ARRAY OF CHAR; VAR s: STRING);

PROCEDURE get_env(name: ARRAY OF CHAR;
               milisec: INTEGER;
              VAR data: STRING;
              VAR priv: BOOLEAN);

PROCEDURE env_entry(entry: INTEGER;
                  milisec: INTEGER;
                 VAR name: STRING;
                 VAR data: STRING;
                 VAR priv: BOOLEAN);

PROCEDURE put_str(name: ARRAY OF CHAR;
                  data: ARRAY OF CHAR;
                  priv: BOOLEAN);
(* Только значащая часть строки (до 0c) *)

PROCEDURE put_env(name: ARRAY OF CHAR;
                  data: ARRAY OF SYSTEM.WORD;
                  priv: BOOLEAN);

----------------------------------------------------------------

PROCEDURE final(p: PROC);

PROCEDURE ipr(): BOOLEAN;

PROCEDURE become_ipr;

PROCEDURE exit(res: INTEGER);

-------------------------------------------------------------

PROCEDURE   allocate(VAR a: SYSTEM.ADDRESS;     size: INTEGER);
PROCEDURE deallocate(VAR a: SYSTEM.ADDRESS;     size: INTEGER);
PROCEDURE reallocate(VAR a: SYSTEM.ADDRESS; VAR high: INTEGER;
                                    len,el_byte_size: INTEGER);

END tskEnv.
