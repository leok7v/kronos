#ifndef XDU_DISK_INCLUDED
#define XDU_DISK_INCLUDED

#ifndef NULL
#define NULL 0
#endif
#define null NULL

/* iNode.mode bits */
#define i_dir   2  // {1} - file is directory
#define i_long  4  // {2}
#define i_esc   8  // {3} - ???
#define i_sysf 16  // {4} 
#define i_all (i_dir + i_long + i_esc + i_sysf) // 2+4+8+16

typedef struct {
	int ref[8];  // blocks of the file data
	int mode;    // set of various modes
	int links;   // count of existing links from dirs to the file
	int eof;     // length of the file
	int cTime;   // creation time
	int wTime;   // modification time
	int pro;     // ???
	int gen;     // ???
	int rfe;     // reserved for future extensions
} iNodeRec, * iNode;

/*  dNode.kind bits */
#define d_del 1    // {0} - dir entry is deleted
#define d_file 2   // {1} - dir entry is a file
#define d_dir 4    // {2} - dir entry is a directory
#define d_hidden 8 // {3} - dir entry is hidden
#define d_esc 16   // {4} - dir entry is a link or what ???
#define d_sys 32   // {5} - ???
#define d_entry (d_dir + d_file + d_esc)
#define d_all (d_del + d_entry + d_hidden + d_sys)

typedef struct {
	char name[32];
	int rfe0[4];	// not used
	int inode;		// iNode number for dir data
	int kind;		// set of one-bit attributes
	int rfe1[2];	// not used
} dNodeRec, *dNode;

void mount(char* fname);
void unmount();

typedef struct {
	iNode inode;
	int blocks_no;
	int* blocks;
	char* data;
} xFile;

void xfile_open(int ino, xFile *file);
void xfile_close(xFile* file);
char* xfile_read(xFile* file); // allocate buffer, reads eof bytes and returns pointer to the buf

typedef struct {
	xFile file;
	int dnodes_no;
	dNode dnodes;
} xDir;

void xdir_open(int ino, xDir* dir);
void xdir_close(xDir* dir);

iNode get_inode(int no);

#endif