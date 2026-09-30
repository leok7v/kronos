%
%	SETJMP	&  LONGJMP	M.K.  15.12.86
%
%	Updated: 08.Dec.87 by S.Siibak
%
%  Usage :
%
%  #include <setjmp.h> 
%     ...
%   ret = setjmp (env);
%     ...
%   longjmp (env, retval);
%
%
	.mod	jump.c
%
%
%   int * env;
%   int   val, L;
%
	.svr	_env
	.svr	_val
	.svr	_L
%
%
%   int	  setjmp (environment)
%   int	* environment;
%
%
	.fun	setjmp
; args + 1(for LLW4, reserved for "0" when storing E-stack
	entr	2	
%
%   env = environment;
%
	llw	5
	copt
	sw	_env
%
%   save current return information (for setjmp!) in env:
%
%   env[0..3] = L[0..3];
%
	lla	0
	copt
	sw	_L
	li	4
	move
%
%   save L ( S for the program that called setjmp) in env[4] :
%
	lw	_env
	lw	_L
	ssw	4
%
%   E-stack may be saved before call to setjmp; as we don't know
%   whether it is or not, store top of S-stack (7+1) in env:
%      env[5..12] = L[-8..-1];
%
	lw	_env
	lsa	5
	lw	_L
	li	8	; NB! for KRONOS P2.5 must be 16
	sub
	li	8
	move
%
%   normal return from setjmp
%
	li	0
	rtn
	.end	_setjmp
%
%
%   VOID  longjmp (environment, value)
%   int	* environment, value;
%
%
	.vfn	longjmp
	entr	3
%
%   env = environment;
%
	llw	5
	sw	_env
%
%   val = value;
%
	llw	6
	sw	_val
%
%   !!  S = env[4]
%
	li	1
	alloc
	lw	_env
	lsw	4
	li	1
	sub
	sub
	decs
%
%   this call sets L = S !
%
	cx	_jump
%
%   NB! this return is never made
%
	rtn
%
	.end	_longjmp
%
%   JUMP
%
	.sfn	_jump
; SS. 20.Nov.87 for LLW4, related with process switching
	entr	1
%
%   move return information to L[0..3] :
%     L[0..3] = env[0..3];
%
	lla	0
	lw	_env
	li	4
	move
%
%   copy stored E-stack (or whatever there was) to top of (future) S-stack:
%     L[-8..-1] = env[5..12]
%
	lla	0
	li	8
	sub
	lw	_env
	lsa	5
	li	8	; NB! for KRONOS P2.5 must be 16
	move
%
%   check return value from longjmp :
%     if (val == 0)  val = 1;
%
	lw	_val
	not
	jcnd	M2
	li	1
	sw 	_val
M2:
%
%   load return value from longjmp
%
	lw	_val
%
%   NB! this return imitates return from setjmp !
%
	rtn
%
	.end	_jump
	.end	jump.c
