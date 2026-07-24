
# define   LI0     0x00
# define   LI1     0x01
# define   LI2     0x02
# define   LI3     0x03
# define   LI4     0x04
# define   LI5     0x05
# define   LI6     0x06
# define   LI7     0x07

# define   LI8     0x08
# define   LI9     0x09
# define   LI0A    0x0A
# define   LI0B    0x0B
# define   LI0C    0x0C
# define   LI0D    0x0D
# define   LI0E    0x0E
# define   LI0F    0x0F

# define   LIB     0x10
# define   LID     0x11
# define   LIW     0x12
# define   LIN     0x13
# define   LLA     0x14
# define   LGA     0x15
# define   LSA     0x16
# define   LEA     0x17

# define   JFLC    0x18 
# define   JFL     0x19
# define   JFSC    0x1A
# define   JFS     0x1B
# define   JBLC    0x1C
# define   JBL     0x1D
# define   JBSC    0x1E
# define   JBS     0x1F

# define   LLW     0x20
# define   LGW     0x21
# define   LEW     0x22
# define   LSW     0x23
# define   LLW4    0x24
# define   LLW5    0x25
# define   LLW6    0x26
# define   LLW7    0x27

# define   LLW8    0x28
# define   LLW9    0x29
# define   LLW0A   0x2A
# define   LLW0B   0x2B
# define   LLW0C   0x2C
# define   LLW0D   0x2D
# define   LLW0E   0x2E
# define   LLW0F   0x2F

# define   SLW     0x30
# define   SGW     0x31
# define   SEW     0x32
# define   SSW     0x33
# define   SLW4    0x34
# define   SLW5    0x35
# define   SLW6    0x36
# define   SLW7    0x37

# define   SLW8    0x38
# define   SLW9    0x39
# define   SLW0A   0x3A
# define   SLW0B   0x3B
# define   SLW0C   0x3C
# define   SLW0D   0x3D
# define   SLW0E   0x3E
# define   SLW0F   0x3F

# define   LXB     0x40
# define   LXW     0x41
# define   LGW2    0x42
# define   LGW3    0x43
# define   LGW4    0x44
# define   LGW5    0x45
# define   LGW6    0x46
# define   LGW7    0x47

# define   LGW8    0x48
# define   LGW9    0x49
# define   LGW0A   0x4A
# define   LGW0B   0x4B
# define   LGW0C   0x4C
# define   LGW0D   0x4D
# define   LGW0E   0x4E
# define   LGW0F   0x4F

# define   SXB     0x50
# define   SXW     0x51
# define   SGW2    0x52
# define   SGW3    0x53
# define   SGW4    0x54
# define   SGW5    0x55
# define   SGW6    0x56
# define   SGW7    0x57

# define   SGW8    0x58
# define   SGW9    0x59
# define   SGW0A   0x5A
# define   SGW0B   0x5B
# define   SGW0C   0x5C
# define   SGW0D   0x5D
# define   SGW0E   0x5E
# define   SGW0F   0x5F

# define   LSW0    0x60
# define   LSW1    0x61
# define   LSW2    0x62
# define   LSW3    0x63
# define   LSW4    0x64
# define   LSW5    0x65
# define   LSW6    0x66
# define   LSW7    0x67

# define   LSW8    0x68
# define   LSW9    0x69
# define   LSW0A   0x6A
# define   LSW0B   0x6B
# define   LSW0C   0x6C
# define   LSW0D   0x6D
# define   LSW0E   0x6E
# define   LSW0F   0x6F

# define   SSW0    0x70
# define   SSW1    0x71
# define   SSW2    0x72
# define   SSW3    0x73
# define   SSW4    0x74
# define   SSW5    0x75
# define   SSW6    0x76
# define   SSW7    0x77

# define   SSW8    0x78
# define   SSW9    0x79
# define   SSW0A   0x7A
# define   SSW0B   0x7B
# define   SSW0C   0x7C
# define   SSW0D   0x7D
# define   SSW0E   0x7E
# define   SSW0F   0x7F

# define   RESET   0x80
# define   QUIT    0x81
# define   GETM    0x82
# define   SETM    0x83
# define   TRAP    0x84
# define   TRA     0x85
# define   TR      0x86
# define   IDLE    0x87

# define   ADD     0x88
# define   SUB     0x89
# define   MUL     0x8A
# define   DIV     0x8B
# define   SHL     0x8C
# define   SHR     0x8D
# define   ROL     0x8E
# define   ROR     0x8F

# define   INP     0x90
# define   OUT     0x91
# define   LASTR   0x92
# define   TRB     0x93

# define   FADD    0x98
# define   FSUB    0x99
# define   FMUL    0x9A
# define   FDIV    0x9B
# define   FCMP    0x9C
# define   FABS    0x9D
# define   FNEG    0x9E
# define   FFCT    0x9F

# define   LSS     0xA0
# define   LEQ     0xA1
# define   GTR     0xA2
# define   GEQ     0xA3
# define   EQU     0xA4
# define   NEQ     0xA5
# define   ABS     0xA6
# define   NEG     0xA7

# define   OR      0xA8
# define   AND     0xA9
# define   XOR     0xAA
# define   BIC     0xAB
# define   IN      0xAC
# define   BIT     0xAD
# define   NOT     0xAE
# define   MOD     0xAF

# define   DECS    0xB0
# define   DROP    0xB1
# define   LODFV   0xB2
# define   STORE   0xB3
# define   STOFV   0xB4
# define   COPT    0xB5
# define   CPCOP   0xB6
# define   PCOP    0xB7

# define   FOR1    0xB8
# define   FOR2    0xB9
# define   ENTC    0xBA
# define   XIT     0xBB
# define   ENTS    0xBC

# define   ORJP    0xBE
# define   ANDJP   0xBF

# define   MOVE    0xC0
# define   RDS     0xC1
# define   LSTA    0xC2
# define   COMP    0xC3
# define   GB      0xC4
# define   GB1     0xC5
# define   CHK     0xC6
# define   CHKZ    0xC7

# define   ALLOC   0xC8
# define   ENTR    0xC9
# define   RTN     0xCA
# define   NOP     0xCB
# define   CX      0xCC
# define   CI      0xCD
# define   CF      0xCE
# define   CL      0xCF

# define   CL0     0xD0
# define   CL1     0xD1
# define   CL2     0xD2
# define   CL3     0xD3
# define   CL4     0xD4
# define   CL5     0xD5
# define   CL6     0xD6
# define   CL7     0xD7

# define   CL8     0xD8
# define   CL9     0xD9
# define   CL0A    0xDA
# define   CL0B    0xDB
# define   CL0C    0xDC
# define   CL0D    0xDD
# define   CL0E    0xDE
# define   CL0F    0xDF

# define   INCL    0xE0
# define   EXCL    0xE1
# define   SLEQ    0xE2
# define   SGEQ    0xE3
# define   INC1    0xE4
# define   DEC1    0xE5
# define   INC     0xE6
# define   DEC     0xE7

# define   STOT    0xE8
# define   LODT    0xE9
# define   LXA     0xEA
# define   LPC     0xEB

# define   BBU 	   0xEC
# define   BBP     0xED
# define   BBLT    0xEE

# define   SWAP	   0xF0
# define   LPA     0xF1
# define   LPW     0xF2
# define   SPW     0xF3
# define   SSWU    0xF4

# define   DOT     0xFE
# define   INVLD   0xFF

