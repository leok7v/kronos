
/*
*	SETJMP	&  LONGJMP	M.K.  15.12.86
*
*	Updated: 08.Feb.88 by 	S.Siibak
*
*  Usage :
*  	#include <setjmp.h> 
*     	...
*   	jmp_buf env;
*   	ret = setjmp (env);
*     	...
*   	longjmp (env, retval);
*/

   static int * _regs_env;
   static int   _val, _Lreg;

int
setjmp (environment) int *environment; {

asm("
	llw	5
	copt
	sw	_regs_env
	lla	0     	   ; _regs_env[0..3] = _Lreg[0..3];
	copt
	sw	_Lreg
	li	4
	move
	lw	_regs_env   ; _regs_env[4]= _Lreg
	lw	_Lreg
	ssw	4
	lw	_regs_env   ; _regs_env[5..12] = _Lreg[-8..-1]
	lsa	5
	lw	_Lreg
	li	8	    ; NB! for KRONOS P2.55 must be 16
	sub
	li	8
	move");

	return(0);   /*  normal return from setjmp   */
}

void
longjmp (environment, value) int *environment, value; {

        _regs_env = environment;
        _val = value;

asm("
	li	1     ; S = _regs_env[4]
	alloc
	lw	_regs_env
	lsw	4
	li	1
	sub
	sub
	decs
	cx	_jump");       /*  this call sets _Lreg = S !  */
}


static _jump(){

asm("
	lla	0      ; _Lreg[0..3] = _regs_env[0..3]
	lw	_regs_env
	li	4
	move
	lla	0	; _Lreg[-8..-1] = _regs_env[5..12]
	li	8
	sub
	lw	_regs_env
	lsa	5
	li	8	; NB! for KRONOS P2.55 must be 16
	move");

	if( _val == 0) _val =1;   /* check return value from longjmp */

	return(_val);
}

