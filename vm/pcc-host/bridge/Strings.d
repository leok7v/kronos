DEFINITION MODULE Strings; (* Ned 20-Jun-89. (c) KRONOS *)

IMPORT  SYSTEM;

PROCEDURE len (str: ARRAY OF CHAR): INTEGER;

PROCEDURE app (VAR dst: ARRAY OF CHAR; str: ARRAY OF CHAR);

PROCEDURE copy(VAR dst: ARRAY OF CHAR; str: ARRAY OF CHAR);


PROCEDURE print (VAR str: ARRAY OF CHAR;
                     fmt: ARRAY OF CHAR;
                 SEQ arg: SYSTEM.WORD);

PROCEDURE append(VAR str: ARRAY OF CHAR;
                     fmt: ARRAY OF CHAR;
                 SEQ arg: SYSTEM.WORD);

PROCEDURE image (VAR str: ARRAY OF CHAR;
                 VAR pos: INTEGER;
                     fmt: ARRAY OF CHAR;
                 SEQ arg: SYSTEM.WORD);


PROCEDURE delete (VAR str: ARRAY OF CHAR; pos,len: INTEGER);
PROCEDURE insert (VAR str: ARRAY OF CHAR; pos,len: INTEGER);

PROCEDURE sub_str(VAR dst: ARRAY OF CHAR;
                      str: ARRAY OF CHAR; pos,len: INTEGER);

PROCEDURE sub_arr(VAR dst: ARRAY OF CHAR;
                      str: ARRAY OF CHAR; pos,len: INTEGER);

PROCEDURE replace(VAR dst: ARRAY OF CHAR;
                      str: ARRAY OF CHAR; pos,len: INTEGER);

PROCEDURE skip  (str: ARRAY OF CHAR; VAR pos: INTEGER; ch: CHAR);
PROCEDURE search(str: ARRAY OF CHAR; VAR pos: INTEGER; ch: CHAR);

PROCEDURE scan(str : ARRAY OF CHAR;
           VAR pos : INTEGER;
               patt: ARRAY OF CHAR;
           VAR done: BOOLEAN);

PROCEDURE iscan(VAR num : SYSTEM.WORD;
                    str : ARRAY OF CHAR;
                VAR pos : INTEGER;
                VAR done: BOOLEAN);

PROCEDURE rscan(VAR real: REAL;
                    str : ARRAY OF CHAR;
                VAR pos : INTEGER;
                VAR done: BOOLEAN);

PROCEDURE move(VAR to: ARRAY OF CHAR; t_ofs: INTEGER;
                 from: ARRAY OF CHAR; f_ofs: INTEGER; len: INTEGER);

(***************************************************************

     Строка  -  массив  литер  любой длины. Все процедуры (кроме
sub_arr)  работает  с  частью строки от начала строки до символа
"конец  строки"  (000c)  или  до  конца массива (HIGH) в котором
хранится   строка.   Во   всех  процедурах  возбуждается  ошибка
"неверный аргумент", если позиция или длина <0.

   ВНИМАНИЕ:

     Все   процедуры   "добавляющие"   в  строку  игнорируют  не
влезающие  символы. И гарантируют наличия символа "конец строки"
(000с)   в   строке-результате,   если   массив  для  сохранения
результата имеет не нулевой размер (HIGH>=0).

                        ОБЫЧНЫЕ ОПЕРАЦИИ

PROCEDURE len
-------------
     Подсчитывает длину строки до 0с или HIGH.

PROCEDURE app
-------------
     Дописывает  строку  str  в строку dst с позиции len(dst)
до 0с или HIGH(dst) или HIGH(str) (что встретится раньше).

PROCEDURE copy
--------------
     Копирует  строку  str  в строку dst до 0с или HIGH(dst) или
HIGH(str) (что встретится раньше).


                    ФОРМАТНЫЙ ВЫВОД В СТРОКИ

PROCEDURE print
---------------
     Формирует строку, определяемую парой (format,args).

PROCEDURE append
----------------
     Добавляет строку, определяемую парой (format,args) к строке
str, начиная с позиций в которой стоит символ 0c.

PROCEDURE image
---------------
     Добавляет строку, определяемую парой (format,args) к строке
str, начиная с позиций pos. После вызова pos указывает на символ
0c, или за конец (HIGH) строки.

                      РАБОТА С ПОДСТРОКАМИ

PROCEDURE delete
----------------
     Удаляет часть строки начиная с pos длиной len

PROCEDURE insert
----------------
     Вставляет в строку len пробелов начиная с позиции pos

PROCEDURE sub_str
-----------------

     Копирует в dst подстроку строки str начиная с pos длиной не
больше   чем   len.  Если  pos  меньше  чем  длинна  строки  str
подсчитаная функцией len(str), результатом будет пустая строка.

PROCEDURE sub_arr
-----------------

     Аналогична  sub_str но не пользуется функцией len(str), что
позволяет "добывать" из строки "str" произвольный под-массивы.

PROCEDURE replace
-----------------

     Заменяет  в dst символы начиная с позиции pos на символы из
строки   str  (начиная  с  начала).  Замена  прекращается,  если
кончилась  строка str или строка dst или в строке str встретился
символ 0с. НЕ завершает замененные символы символом 0c.

PROCEDURE skip
--------------

     Пропускает  символы  равные  ch,  начиная  с  позиции  pos,
увеличивая    pos.    Завершается    если    кончилась    строка
(pos>HIGH(str)) или текущий символ не равен ch (str[pos]#ch).

PROCEDURE search
----------------

     Поиск символа ch, начиная с позиции pos, Завершается если
кончилась  строка  (pos>HIGH(str)) или текущий символ равен ch
(str[pos]=ch).

PROCEDURE scan(
---------------
               str : ARRAY OF CHAR;
           VAR pos : INTEGER;
               patt: ARRAY OF CHAR;
           VAR done: BOOLEAN);

     Пропускает  символы в строке, начиная с позиции pos, пока
они   совпадают   с  образцом  patt.  Если  образец  совпал  с
подстрокой, то done=TRUE.

PROCEDURE iscan(
----------------
                VAR num : SYSTEM.WORD;
                    str : ARRAY OF CHAR;
                VAR pos : INTEGER;
                VAR done: BOOLEAN);

     Считывает  из  строки  str  число, начиная с позиции pos.
Пропускает пробелы. После вызова:

        done  -- = TRUE, если удалось считать число;
        pos   -- индекс следующего за числом символа;
        num   -- число, если done.

     Число  может быть представлено в любом виде, допустимом в
Модуле-2:

        123456789
       -1
        0ABCDEFh
        177b
       -177b
        377c

PROCEDURE rscan(
----------------
                VAR real: REAL;
                    str : ARRAY OF CHAR;
                VAR pos : INTEGER;
                VAR done: BOOLEAN);

     Аналогично iscan, только считывает вещественное число.

***************************************************************)

END Strings.
