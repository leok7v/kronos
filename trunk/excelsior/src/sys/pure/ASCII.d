DEFINITION MODULE ASCII; (* Leo & Ned 14-Apr-87. (c) KRONOS *)

CONST
(***************************************************************
**  Мнемоника      Семантика       Контрольный Дополнительная **
** ASCII-7.68                         символ     мнемоника    **
**                                  CNTRL+char                **
***************************************************************)

  NUL =  0c; (* NUL NUL -- line break     @  *)
  SOH =  1c; (* Начало заголовка          A  *)
  STX =  2c; (* Начало текста             B  *)
  ETX =  3c; (* Конец текста              C  *)  BREAK = ETX;
  EOT =  4c; (* Конец передачи            D  *)
  ENQ =  5c; (* Запрос                    E  *)
  ACK =  6c; (* Подтверждение             F  *)
  BEL =  7c; (* Звонок                    G  *)
   BS = 10c; (* Шаг назад                 H  *)
   HT = 11c; (* Горизонтальная табуляция  I  *)
   LF = 12c; (* Перевод строки            J  *)
   VT = 13c; (* Вертикальная табуляция    K  *)
   FF = 14c; (* Перевод формата           L  *)
   CR = 15c; (* Возврат каретки           M  *)
   SO = 16c; (* Национальный регистр      N  *)
   SI = 17c; (* Латинский регистр         O  *)

  DLE = 20c; (* Авторегистр 1             P  *)
  DC1 = 21c; (* Управление устройством 1  Q  *)  XON  = DC1;
  DC2 = 22c; (* ---------------------- 2  R  *)
  DC3 = 23c; (* ---------------------- 3  S  *)  XOFF = DC3;
  DC4 = 24c; (* ---------------------- 4  T  *)
  NAK = 25c; (* Отрицание                 U  *)
  SYN = 26c; (* Синхронизация             V  *)
  ETB = 27c; (* Конец блока               W  *)
  CAN = 30c; (* Аннулирование             X  *)
   EM = 31c; (* Конец носителя            Y  *)
  SUB = 32c; (* Замена                    Z  *)
  ESC = 33c; (* Авторегистр 2             [  *)
   FS = 34c; (* Разделитель файлов        \  *)
   GS = 35c; (* Разделитель групп         ]  *)
   RS = 36c; (* Разделитель записей       ^  *)
   US = 37c; (* Разделитель элементов     _  *)

SPACE = 40c; (* Пробел                       *)
  DEL =177c; (* "Забой"                      *)  RUBBOUT = DEL;
----------------------------------------------------------------
CONST  (* Соглашения ОС  Excelsior о семантике нек. символов: *)

    NL = RS;    (* New Line. Отрабатывается  как  CR  LF  для
                   телевизоров  и для других последовательных
                   устройств.  Разделитель  строк в текстовых
                   файлах.
                *)
    EOF = FS;   (* End Of File. Конец файла. *)

(*  US  в пропорциональных шрифтах означает пробел в 1ну точку *)

----------------------------------------------------------------

CONST (* типы символов *)
  control = 00; (* контрольный 0c..37c, 200c..237c       *)
  special = 01; (* специальный, т.е.  ?,/+!"#$%&  и т.д. *)
    digit = 02; (* 0123456789                            *)
    dig16 = 03; (* 0123456789ABCDEF                      *)
    cyril = 04; (* буква кириллицы                       *)
    latin = 05; (* буква латинского алфавита             *)
    small = 06; (* строчная буква                        *)
  capital = 07; (* прописная буква                       *)

PROCEDURE  KIND(сh:  CHAR):  BITSET;
(* Возвращает набор признаков символа *)

PROCEDURE   SMALL(ch: CHAR): CHAR;
(* превращает букву в строчную, не изменяет остальных символов *)

PROCEDURE CAPITAL(ch: CHAR): CHAR;
(* превращает букву в прописную, не изменяет остальных символов *)

(***************************************************************

-------------------------  ПРИМЕЧАНИЯ  -------------------------
                         --------------

   OS  Excelsior  II  поддерживает работу с набором символов
   КОИ-8.   При   этом   символы   с   кодировками  0c..177c
   соответствуют    стандарту    ASCII-7.68,   в   остальных
   кодировках  располагаются  символы  кириллицы и некоторые
   служебные.

   Все символы делятся на следующие группы:
      - буквы (латиницы или кириллицы),
      - цифры;
      - специальные символы (?/+;!"#$%&'()=-_*:>.|\<,@01...)
      - контрольные (управляющие) символы

***************************************************************)

END ASCII.
