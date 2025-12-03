/*
 *   Purpose: I/O operation on XD virturl volume
 */

/*
* disk layout:
* block 0 -- cold booter
* block 1 -- superblock (label, blocks and inodes free/busy maps)
* blocks 2..(inodes_no + 63) DIV 64 + 2 - inodes table
* . . . 
* . . .
* . . .
*/

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include "xduDisk.h"
#include "xduTime.h"

typedef int  WBLOCK[1024];
typedef char CBLOCK[4096];
typedef struct i_node IBLOCK[4096 / 64];

struct x_disk {
	FILE* file;
	union {
		char* data;
		WBLOCK* iblocks; 
		CBLOCK* cblocks; 
	};
	iNode inodes;
	char label[12];
	int i_no;
	int b_no;
	int c_time;
	int* bset;
	int* iset;
};

/* mounted XD volume */
static struct x_disk* disk = NULL;

void fatal(char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr,fmt, args);
	va_end(args);
	exit(1);
}

#define ISSET(set,bit) (((set[bit/32] >> (bit % 32)) & 1))
#define ISBUSY(set,bit) (!ISSET(set,bit))
#define INCL(set,bit) set[bit/32] |= (1 << (bit % 32))
#define EXCL(set,bit) set[bit/32] &= ~(1 << (bit % 32))

static void flushBlock(int no)
{
	assert(disk != null && no >= 0 && no < disk->b_no);

	if (fseek(disk->file, no * 4096, SEEK_SET) != 0) {
		fatal("Error writing block %d\n", no);
	}
	if (fwrite(disk->data + 4096 * no, 1, 4096, disk->file) != 4096) {
		fatal("Error writing block %d\n", no);
	}
}

static void flushInode(int no)
{
	assert(disk != null && no >= 0 && no < disk->i_no);
	
	if (fseek(disk->file, 8192 + no * 64, SEEK_SET) != 0) {
		fatal("Error writing iNode %d\n", no);
	}
	if (fwrite(disk->inodes + no, 1, 64, disk->file) != 64) {
		fatal("Error writing iNode %d\n", no);
	}
}

static void dnode2cash(char* to, char* from)
{
	memcpy(to, from, 64);
}

static void flushDnode(xDir dir, int index)
// copies dNode to a global cash and then flushes to disk
{
	assert(index >= 0 && index < dir->dnodes_no);

	int block_no = index / 64;
	char* block = (char *)(disk->cblocks + dir->file.blocks[block_no]);
	memcpy(block + 64 * (index % 64), dir->dnodes + index, 64);
//	dnode2cash(block + 64 * (index % 64), dir->dnodes + index);
	flushBlock(dir->file.blocks[block_no]);
}

static int calcBusy(int* set, int sz)
{
	int total = 0;
	for (int i = 0; i < sz; i++) {
		if (ISBUSY(set, i)) total++;
	}
	return total;
}

static void unpackSuper(struct x_disk* disk) {
	char* super = disk->data + 4096;
	strncpy(disk->label, super, 8);
	int *label = (int*)super;
	disk->i_no = label[4];
	disk->b_no = label[5];
	disk->c_time = label[7];
	disk->inodes = (iNode)&disk->data[4096 + 4096];
	disk->bset = (int*)(super + 64);
	disk->iset = disk->bset + ((disk->b_no + 31) / 32);
}

void mount(char* fname) {
	assert(disk == null);

	static struct x_disk _disk = {0};
	
	FILE *file = fopen(fname, "r+b");
	if (!file) {
		fatal("ERROR openinig file %s: %s\n", fname, strerror(errno));
	}
	if (fseek(file, 0, SEEK_END) != 0) {
		fatal("ERROR repositioning file %s: %s\n", fname, strerror(errno));
	}
	long fsize = ftell(file);
	if (fsize < 0) {
		fatal("ERROR getting length of file %s: %s\n", fname, strerror(errno));
	}
	if (fsize % 4096 != 0) fatal("%s: invalid file size %d", fname, fsize);
	
	_disk.data = (char *)malloc(fsize);
	if (_disk.data == NULL) 
		fatal("ERROR: not enough memory for %d bytes\n", fsize);

	rewind(file);
	size_t read = fread(_disk.data, 1, fsize, file);
	if (read != fsize) {
		fatal("ERROR reading file %s: %s\n", fname, strerror(errno)); 
	}

	unpackSuper(&_disk);

	_disk.file = file;
	disk = &_disk;
	int year, month, day, hour, minute, second;
	unpack_kronos_time(disk->c_time, &year, &month, &day, &hour, &minute, &second);
	printf("XD volume \"%s\":\n", fname);
	printf("  label  : \"%s\"\n", disk->label);
	printf("  blocks : %d / %d\n", disk->b_no, calcBusy(disk->bset, disk->b_no));
	printf("  inodes : %d / %d\n", disk->i_no, calcBusy(disk->iset, disk->i_no));
	printf("  created : %02d.%02d.%d %02d:%02d:%02d", day, month, year, hour, minute, second);
	printf("\n");
}

void unmount()
{
	if (disk) {
		free(disk->data);
		fclose(disk->file);
		disk->file = null;
		disk = null;
	}
}

static int allocBlocks(int no, int* buf)
// returns true if not enough free space
{
	int b = 0, high = (disk->b_no + 31) / 32;

	while (b < high && disk->bset[b] == 0) b++; // skip 32-bit chunks of busy blocks
	b = b * 32;
	while (no && b < disk->b_no) {
		if (ISSET(disk->bset, b)) {
			EXCL(disk->bset, b);
			*(buf++) = b;
			no--;
			// TODO: zero the block
		}
		b++;
	}
	return no != 0;
}

static int allocInode()
// returns allocated inode index or -1
{
	int no = 0, high = (disk->i_no + 31) / 32;
	while (no < high && disk->iset[no] == 0) no++; // skip empty words
	no = no * 32;
	while (no < disk->i_no && ISSET(disk->iset, no) == 0) no++;
	if (no < disk->i_no) {
		EXCL(disk->iset, no);
		memset(&(disk->inodes[no]), 0, 64);
		return no;
	}
	return -1;
}

void xfile_extend(xFile file, int neweof)
{
	assert((file != null && file->inode != null));
	assert(neweof > file->inode->eof);

	int allocated = file->blocks_no * 4096;
	if (neweof < allocated) {
		file->inode->eof = neweof;
		flushInode(file->inode_no);
		return;
	}

    // allocate blocks
	int required = (neweof + 4095) / 4096;
	if (required > 8) {
		if ((file->inode->mode & i_long) == 0) { // convert to long
			if (required > 4096) {
				fatal("files larger 4Mb are not supported yet, sorry\n");
			}
			int xb = 0;
			if (allocBlocks(1, &xb)) {
				fatal("not enough free space on XD volume\n");
			}
			for (int i = 0; i < file->blocks_no; i++) {
				disk->iblocks[xb][i] = file->inode->ref[i];
			}
			file->inode->ref[0] = xb;
			file->inode->mode |= i_long;
			file->blocks = (int *)(disk->iblocks + xb);
		}
		if (allocBlocks(required - file->blocks_no, (int *)&(disk->iblocks[file->inode->ref[0]]) + file->blocks_no)) {
			fatal("not enough free space on XD volume\n");
		}
		flushBlock(file->inode->ref[0]);
	} else {
		if (allocBlocks(required - file->blocks_no, file->inode->ref + file->blocks_no)) {
			fatal("not enough free space on XD volume\n");
		}
	}
	file->blocks_no = required;
	char* newdata = malloc(file->blocks_no * 4096);
	if (file->data != null) {
		if (newdata != null) {
			memcpy(newdata, file->data, file->inode->eof);
		} else {
			fatal("no memory\n");
		}			
		free(file->data);
	}
	file->data = newdata;
	file->inode->eof = neweof;
	flushInode(file->inode_no);
	flushBlock(1); // superblock, including free blocks map
}

void xfile_open(int ino, xFile file)
{
	assert((file != null));
	assert((disk != null));
	assert((ino >= 0 && ino < disk->i_no));

	file->inode = &disk->inodes[ino];
	file->inode_no = ino;
	file->blocks_no = (file->inode->eof + 4095) / 4096;
	if (file->inode->mode & i_long) {
		file->blocks = (int*)(disk->iblocks + file->inode->ref[0]);
		if (file->blocks_no > 1024)
			fatal("files over 4Mb are not supported yet, sorry\n");
	}
	else {
		file->blocks = file->inode->ref;
		if (file->blocks_no > 8)
			fatal("invalid file descriptor %d\n", ino);
	}
	file->data = null;
}

void xfile_create(xFile file, int eof)
{
	assert(disk != null && file != null);

	file->inode_no = allocInode();
	if (file->inode_no < 0) {
		fatal("Too many files on XD volume\n");
	}
	flushBlock(1); // superblock

	file->inode = get_inode(file->inode_no);
	iNode i = file->inode;
	i->eof = 0;
	i->links = 0;
	i->mode = 0; 
	i->cTime = 0; // TODO: set current time
	i->wTime = 0; // TODO: set current time
	file->blocks_no = 0; 
	file->blocks = i->ref;
	file->data = null;
	if (eof > 0) xfile_extend(file, eof);
}

void xfile_close(xFile file)
{
	assert((file != null && file->inode != null));

	if (file->data != null) free(file->data);
	file->data = null;
	file->blocks = null;
	file->blocks_no = 0;
	file->inode = null;
}

char* xfile_read(xFile file) // allocate cash buffer, reads eof bytes into cash and returns pointer to the cash buffer
{
	assert((file != null && file->inode != null));

	if (file->data == null) {
		file->data = malloc(file->blocks_no * 4096);
		if (file->data == null) {
			fatal("not enough memory\n");
		}
		CBLOCK* buf = (CBLOCK*)file->data;
		int i = 0;
		while (i < file->blocks_no) {
			memcpy(buf++, disk->cblocks + file->blocks[i++], sizeof(CBLOCK));
		}
	}
	return file->data;
}

void xfile_write(xFile file, char* data, int len)
{
	assert(file != null);
	assert(file->inode->eof == len); // file must be properly created before
	assert(data != null);

	int w = 0;
	while (w < len) {
		int bx = w / 4096;
		int wl = ((len - w) >= 4096) ? 4096 : (len - w);
		assert(bx < file->blocks_no);
		char* block = (char *)(disk->cblocks + file->blocks[bx]);
		memcpy(block, data + w, wl);
		flushBlock(file->blocks[bx]);
		w += wl;
	}
}

void xfile_set_time(xFile file, int created, int modified)
{
	file->inode->cTime = created;
	file->inode->wTime = modified;
	flushInode(file->inode_no);
}

void xdir_open(int ino, xDir dir)
{
	assert((dir != null));
	assert((disk != null));
	assert((ino >= 0 && ino < disk->i_no));

	iNode inode = &disk->inodes[ino];
	if ((inode->mode & i_dir) == 0) {
		fatal("file %d is not a directory", ino);
	}
	if (inode->eof % sizeof(dNode) != 0) {
		fprintf(stderr, "WARNING: directory %d has invalid file size %d\n", ino, inode->eof);
	}
	xfile_open(ino, &dir->file);
	dir->dnodes_no = inode->eof / sizeof(struct d_node);
	dir->dnodes = (dNode)xfile_read(&dir->file);
}

int xdir_find(xDir dir, char* name)
// returns dNode index or -1
{
	int x = 0, nodes = dir->dnodes_no;
	while (nodes--) {
		dNode dnode = &(dir->dnodes[x]);
		if ((dnode->kind & d_del) == 0 && strncmp(name, dnode->name, 32) == 0) {
			return x;
		}
		x++;
	}
	return -1;
}

static int allocDnode(xDir dir)
{
	int x = 0;
	while (x < dir->dnodes_no) {
		dNode dnode = &(dir->dnodes[x]);
		if ((dnode->kind & d_del) != 0 || (dnode->kind & d_entry) == 0 || dnode->name[0] == 0) {
			memset(&(dir->dnodes[x]), 0, 64);
			return x;
		}
		// TODO: free dnodes seems to be not marked by d_del
		x++;
	}
	xfile_extend(&dir->file, dir->file.inode->eof + 64);
	memset(&(dir->dnodes[dir->dnodes_no]), 0, 64);
	return dir->dnodes_no++;
	
}

void xfile_link(xDir dir, int no, char* name, int kind)
{
	assert(disk != null);
	assert(dir != null && name != null);

	int entry = xdir_find(dir, name);
	int fileToDelete = -1;
	if (entry >= 0) {
		fileToDelete = dir->dnodes[entry].inode;
	} else {
		entry = allocDnode(dir);
	}

	dNode dnode = dir->dnodes + entry;
	strncpy(dnode->name, name, 32);
	dnode->inode = no;
	dnode->kind = kind;
	flushDnode(dir, entry);
	disk->inodes[no].links++;
	flushInode(no);
	if (fileToDelete >= 0) {
		iNode inode = &(disk->inodes[fileToDelete]);
		if (inode->links > 1) {
			inode->links--;
			flushInode(fileToDelete);
		} else {
			int blocks_no = (inode->eof + 4095) / 4096;
			if (blocks_no >= 1024) { 			
				fatal("Existing file %s is too long to be deleted. Consider removing by OS Excelsior.\n", name);
			}
			int index_block = -1;
			int* blocks = inode->ref;
			if ((inode->mode & i_long) != 0) {
				index_block = inode->ref[0];
				blocks = disk->iblocks[index_block];
			}
			for (int i = 0; i < blocks_no; i++) {
				INCL(disk->bset, blocks[i]);
			}
			if (index_block >= 0) {
				INCL(disk->bset, index_block);
			}

			INCL(disk->iset, fileToDelete);
			flushBlock(1); // super block
		}
	}
}

void xdir_create(xDir parent, char* name, xDir newdir)
{
	assert(disk != null);
	assert(parent != null);
	assert(newdir != null);
	assert((parent->file.inode->mode & i_dir) != 0);

	int x = xdir_find(parent, name);
	if (x >= 0) { 
		// name exists, try open as a dir
		xdir_open(parent->dnodes[x].inode, newdir);
		return;
	}
	xFile file = &newdir->file;
	xfile_create(file, 64);  
	assert(file->inode->eof == 64 && file->blocks_no == 1 && file->data != null);

	file->inode->mode = i_dir;
	file->data = malloc(4096);
	newdir->dnodes = (dNode)file->data;
	newdir->dnodes_no = 1;

	dNode dnode = newdir->dnodes;
	strncpy(dnode->name, "..", 3);
	dnode->inode = parent->file.inode_no;
	dnode->kind = d_dir + d_hidden;
	flushDnode(newdir, 0);
	xfile_link(parent, newdir->file.inode_no, name, d_dir);
}

void xdir_close(xDir dir)
{
	if (dir == null || dir->file.data == null) return;
	xfile_close(&dir->file);
}

iNode get_inode(int no)
{
	assert(disk != null && no >= 0 && no < disk->i_no);
	return disk->inodes + no;
}


int isEmpty(WBLOCK blk)
{
	int i = 1024;
	while (i--)
		if (*(blk++)) 
			return 0;
	return 1;
}

int zero_free_blocks()
{
	assert(disk != null);
	int i = disk->b_no, total = 0;
	while (i--) {
		if (ISSET(disk->bset,i) && !isEmpty(disk->iblocks[i])) {
			total++;
			memset(disk->cblocks[i], 0, 4096);
			flushBlock(i);
		}
	}
	printf("%d\n", total);
	return total;
}
