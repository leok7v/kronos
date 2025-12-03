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

struct i_node {
	int ref[8];  // blocks of the file data
	int mode;    // set of various modes
	int links;   // count of existing links from dirs to the file
	int eof;     // length of the file
	int cTime;   // creation time
	int wTime;   // modification time
	int pro;     // ???
	int gen;     // ???
	int rfe;     // reserved for future extensions
};
typedef struct i_node* iNode;

/*  dNode.kind bits */
#define d_del 1    // {0} - dir entry is deleted
#define d_file 2   // {1} - dir entry is a file
#define d_dir 4    // {2} - dir entry is a directory
#define d_hidden 8 // {3} - dir entry is hidden
#define d_esc 16   // {4} - dir entry is a link or what ???
#define d_sys 32   // {5} - ???
#define d_entry (d_dir + d_file + d_esc)
#define d_all (d_del + d_entry + d_hidden + d_sys)

struct d_node {
	char name[32];
	int rfe0[4];	// not used
	int inode;		// iNode number of the file
	int kind;		// set of one-bit attributes
	int rfe1[2];	// not used
};
typedef struct d_node* dNode;

void mount(char* fname);
void unmount();

struct x_file {
	iNode inode;
	int inode_no;
	int blocks_no;
	int* blocks;
	char* data;
};
typedef struct x_file* xFile;

struct x_dir {
	struct x_file file;
	int dnodes_no;
	dNode dnodes;
};
typedef struct x_dir* xDir;

void xfile_open(int ino, xFile file); // x_file struct must be allocated and controlled outside
void xfile_close(xFile file);
void xfile_create(xFile file, int eof);
char* xfile_read(xFile file); // returned buffer is allocated during the first read op and deallocated on xfile_close
void xfile_write(xFile file, char* data, int len);
void xfile_link(xDir dir, int no, char* name, int kind);
void xfile_set_time(xFile file, int created, int modified);

void xdir_open(int ino, xDir dir); // x_dir struct must be allocated and controlled outside
void xdir_create(xDir parent, char* name, xDir newdir);
void xdir_close(xDir dir);

iNode get_inode(int no);

int zero_free_blocks();

#endif