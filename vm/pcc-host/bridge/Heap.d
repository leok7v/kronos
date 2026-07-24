DEFINITION MODULE Heap; (* Andy & Leo 20-Dec-89. (c) KRONOS *)

IMPORT  SYSTEM;

VAL done: BOOLEAN;
   error: INTEGER;
   limit: INTEGER;
  credit: INTEGER;

(* Процедуры, возбуждающие TRAP 4Eh при нехватке памяти: *)

PROCEDURE   ALLOCATE(VAR a: SYSTEM.ADDRESS;     size: INTEGER);
PROCEDURE DEALLOCATE(VAR a: SYSTEM.ADDRESS;     size: INTEGER);
PROCEDURE REALLOCATE(VAR a: SYSTEM.ADDRESS; VAR high: INTEGER;
                                            len,el_byte_size: INTEGER);

(* Процедуры, при нехватке памяти возвращающие NIL: *)

PROCEDURE   allocate(VAR a: SYSTEM.ADDRESS;     size: INTEGER);
PROCEDURE deallocate(VAR a: SYSTEM.ADDRESS;     size: INTEGER);
PROCEDURE reallocate(VAR a: SYSTEM.ADDRESS; VAR high: INTEGER;
                                            len,el_byte_size: INTEGER);

PROCEDURE set_credit(size: INTEGER); (* words *)

PROCEDURE set_limit(total: INTEGER); (* words *)

TYPE
  GARBAGE_COLLECTOR = PROCEDURE (): BOOLEAN;
  (* TRUE, если удалось что-то вернуть, иначе FALSE *)

PROCEDURE install_gc(gc: GARBAGE_COLLECTOR);

PROCEDURE  remove_gc(gc: GARBAGE_COLLECTOR);

PROCEDURE statistics(VAR  from_os: INTEGER;
                     VAR     free: INTEGER;
                     VAR user_sum: INTEGER);

(*************************************************************

   Длина запрашиваемого и возвращаемого куска всегда
   неотрицательна.

   Запрос на 0 слов памяти считается корректным
   - возвращается NIL. (В том числе в процедурах
   возбуждающих прерывания по нехватке памяти.)
   Можно вернуть куче кусок памяти с адресом
   начала NIL и произвольной длиной.

PROCEDURE reallocate(VAR a: SYSTEM.ADDRESS; VAR high: INTEGER;
                                            len,el_byte_size: INTEGER);
      Пытается переразместить массив начинающийся по
   адресу "a", под "len" элементов длиной "el_byte_size" БАЙТОВ каждый.
      В случае успеха копирует содержимое массива
   и перевычисляет "a" и "high=len-1". В случае
   расширения массива значения новых элементов не определены.
      В случае неудачи "a" и "high" не изменяются.

PROCEDURE statistics(VAR  from_os: INTEGER;
                     VAR     free: INTEGER;
                     VAR user_sum: INTEGER);
   Возвращается общий размер памяти, взятой у OS (from_os),
   общий размер памяти в списке свободных сегментов (free),
   и сумма запросов пользователя на память (user_sum).
   (from_os >= free + user_sum)

PROCEDURE set_limit(total: INTEGER);
   Установление верхнего предела на общий размер памяти,
   который может быть запрошен у OS.
   При total<0 верхний предел отменяется.

PROCEDURE set_credit(size: INTEGER);
   Установление размера запроса памяти у OS.
   Начальное значение отлично от 0 и устанавливается при
   инициализации. Библиотеки использующие set_credit(0)
   в своей инициализации ОБЯЗАНЫ вернуть значение
   credit в начальное.

PROCEDURE install_gc(gc: GARBAGE_COLLECTOR);
   Процедура "gc" будет вызвана при нехватке памяти
   и если она вернет TRUE попытка запросить память
   будет повторена. Если после вторичной попытки
   памяти все-таки не оказалось "gc" повторно
   НЕ вызывается.

PROCEDURE  remove_gc(gc: GARBAGE_COLLECTOR);
   Удаляет "gc" из множества зарегистрированных
   "мусорщиков".

*************************************************************

   Heap 1.1 (Замечания по реализации)
   Планировщик не оптимизирует работу с маленькими кусками
   памяти.
   Минимальный полезный размер сегмента памяти - 2 слова.
   Накладные расходы на каждый сегмент  памяти - 2 слова.
   Работа ведется на области памяти (osMemory.AREA) задачи.

*************************************************************)
END Heap.
