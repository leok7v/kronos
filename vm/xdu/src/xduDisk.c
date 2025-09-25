/*
* disk layout:
* block 0 -- cold booter
* block 1 -- superblock (label, inodes busy map)
* blocks 2..X - inodes table
* . . . 
* . . .
* . . .
*/

#include "stdio.h"
#include "xduDisk.h"
#include "errno.h"
#include "stdlib.h"
#include "string.h"
#include "assert.h"
#include "xduTime.h"

typedef int  WBLOCK[1024];
typedef char CBLOCK[4096];
typedef iNodeRec IBLOCK[4096 / 64];

typedef struct {
	char *data;
	WBLOCK* iblocks;
	CBLOCK* cblocks;
	iNodeRec* inodes;

	char label[8];
	int i_no;
	int b_no;
	int c_time;
} xDiskRec;

static xDiskRec *disk = NULL;
static char iobuf[8192];

void unpackSuper(xDiskRec* disk) {
	char* super = disk->data + 4096;
	strncpy(disk->label, super, 7);
	int *label = (int*)super;
	disk->i_no = label[4];
	disk->b_no = label[5];
	disk->c_time = label[7];
	disk->inodes = (iNode)&disk->data[4096 + 4096];
	disk->iblocks = (WBLOCK*)disk->data;
	disk->cblocks = (CBLOCK*)disk->data;
}

void mount(char* fname) {
	static xDiskRec _disk;
	if (disk != null) {
		printf("disk \"%s\" already mounted\n", disk->label);
		exit(1);
	}
	FILE *file = fopen(fname, "rb");
	if (!file) {
		perror("file open");
		exit(1);
	}
	if (fseek(file, 0, SEEK_END) != 0) {
		perror("SEEK ERROR");
		exit(1);
	}
	long fsize = ftell(file);
	if (fsize < 0) {
		perror("FTELL ERROR");
		exit(1);
	}
	if (fsize % 4096 != 0) {
		printf("%s: invalid file size %d", fname, fsize);
		exit(1);
	}
	_disk.data = (char *)malloc(fsize);
	if (_disk.data == NULL) {
		printf("not enough memory for %d bytes\n", fsize);
		exit(1);
	}
	rewind(file);
	size_t read = fread(_disk.data, 1, fsize, file);
	if (read != fsize) {
		perror("FILE READ ERROR");
		exit(1);
	}

	unpackSuper(&_disk);
	disk = &_disk;
	printf("XD volume \"%s\":  label \"%s\" blocks %d created: ", fname, disk->label, disk->b_no);
	pKronosTime(disk->c_time);
	printf("\n");
}

void unmount()
{
	if (disk) {
		free(disk->data);
		disk = null;
	}
}

void xfile_open(int ino, xFile *file)
{
	assert((file != null));
	assert((disk != null));
	assert((ino >= 0 && ino < disk->i_no));

	file->inode = &disk->inodes[ino];
	file->blocks_no = (file->inode->eof + 4095) / 4096;
	if (file->inode->mode & i_long) {
		file->blocks = (int *) (disk->iblocks + file->inode->ref[0]);
		if (file->blocks_no > 1024) {
			printf("files over 4Mb are not supported yet, sorry\n");
			exit(1);
		}
	}
	else {
		file->blocks = file->inode->ref;
		if (file->blocks_no > 8) {
			printf("invalid file descriptor %d\n",ino);
			exit(1);
		}
	}
	file->data = null;
}

void xfile_close(xFile* file)
{
	assert((file != null && file->inode != null));

	if (file->data != null) free(file->data);
	file->data = null;
	file->blocks = null;
	file->blocks_no = 0;
	file->inode = null;
}

char* xfile_read(xFile* file) // allocate buffer, reads eof bytes and returns pointer to the buf
{
	assert((file != null && file->inode != null));

	if (file->data == null) {
		file->data = malloc(file->blocks_no * 4096);
		if (file->data == null) {
			printf("not enough memory\n");
			exit(1);
		}
		int i = 0;
		CBLOCK* buf = (CBLOCK*)file->data;
		while (i < file->blocks_no) {
			//memcpy(buf, &disk->cblocks[file->blocks[i]], sizeof(CBLOCK));
			memcpy(buf, disk->cblocks + file->blocks[i], sizeof(CBLOCK));
			buf++;
			i++;
		}
	}
	return file->data;
}

void xdir_open(int ino, xDir* dir)
{
	assert((dir != null));
	assert((disk != null));
	assert((ino >= 0 && ino < disk->i_no));

	iNode inode = &disk->inodes[ino];
	if ((inode->mode & i_dir) == 0) {
		printf("file %d is not directory", ino);
		exit(1);
	}
	if (inode->eof % sizeof(dNode) != 0) {
		printf("WARNING: directory %d has invalid file size %d", ino, inode->eof);
	}
	xfile_open(ino, &dir->file);
	dir->dnodes_no = inode->eof / sizeof(dNodeRec);
	dir->dnodes = (dNode)xfile_read(&dir->file);
}

void xdir_close(xDir* dir)
{
	if (dir == null || dir->file.data == null) return;
	xfile_close(&dir->file);
}

iNode get_inode(int no)
{
	assert(disk != null && no >= 0 && no < disk->i_no);
	return disk->inodes + no;
}