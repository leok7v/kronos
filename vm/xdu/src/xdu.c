// xdu.cpp : Defines the entry point for the application.
//

#include "stdio.h"
#include "xduDisk.h"
#include "xduWIO.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "xduTime.h"

void pindent(int level)
{
	while (level-- > 0) printf("  ");
}

void pfilename(char* fn, int dir)
{
	char fname[40];
	strncpy(fname, fn, 33);
	if (dir) strcat(fname, "/");
	printf("%-16s", fname);
}

void copy_file(int ino, char* fname, char* path)
{
	xFile file;
	xfile_open(ino, &file);
	assert((file.inode->mode & i_dir) == 0);

	char fullname[248];
	strncpy(fullname, path,248);
	if (strlen(fullname) + strlen(fname) >= 248) {
		printf("ERROR: too long filename \"%s/%s\"\n", path, fname);
		exit(1);
	}
	strcat(fullname, "/");
	strcat(fullname, fname);
	printf("%s\r", fullname);
	char* content = xfile_read(&file);
	w_copy_file(fullname, content, file.inode->eof, file.inode->cTime, file.inode->wTime);
	xfile_close(&file);
	printf("%s -- DONE\n", fullname);
}

void copy_dir(int ino, char* fname, char* path)
{
	xDir dir;
	xdir_open(ino, &dir);
	assert(dir.file.inode->mode & i_dir);

	char fullname[248];
	strncpy(fullname, path, 248);
	if (strlen(fullname) + strlen(fname) >= 248) {
		printf("ERROR: too long filename \"%s/%s\"\n", path, fname);
		exit(1);
	}
	if (fullname[0] != 0) {
		strcat(fullname, "/");
	}
	strcat(fullname, fname);

	w_create_dir(fullname, dir.file.inode->cTime, dir.file.inode->wTime);

	printf("DIR %s\n", fullname);
	dNode dnode = dir.dnodes;
	int i = dir.dnodes_no;
	while (i--) {
		if ((dnode->kind & d_del) == 0) {
			char fname[40];
			strncpy(fname, dnode->name, 33);
			if ((dnode->kind & d_dir) && (strcmp(fname,"..") != 0)) {
				copy_dir(dnode->inode, fname, fullname);
			}
			else if (dnode->kind & d_file) {
				copy_file(dnode->inode, fname, fullname);
			}
		}
		dnode++;
	}
	xdir_close(&dir);
	printf("DIR %s -- DONE\n", fullname);
}

void listDir(int ino, int level)
{
	xDir dir;
	xdir_open(ino, &dir);
	/* print subdirectories */
	int i = dir.dnodes_no;
	dNode dnode = dir.dnodes;
	while (i--) {
		if ((dnode->kind & d_del) == 0) {
			if (dnode->kind & d_dir) {
				iNode inode = get_inode(dnode->inode);
				pindent(level);
				pfilename(dnode->name,1);
				printf(" CR:");
				pKronosTime(inode->cTime);
				printf("  MD:");
				pKronosTime(inode->wTime);
				printf("\n");
			}
		}
		dnode++;
	}
	/* print files */
	i = dir.dnodes_no;
	dnode = dir.dnodes;
	while (i--) {
		if ((dnode->kind & d_del) == 0) {
			if (dnode->kind & d_file) {
				iNode inode = get_inode(dnode->inode);
				pindent(level);
				pfilename(dnode->name,0);
				printf(" %7d B  ", inode->eof);
				printf("CR:");
				pKronosTime(inode->cTime);
				printf("  MD:");
				pKronosTime(inode->wTime);
				printf("\n");
			}
		}
		dnode++;
	}
	/* iterate subdirectories */
	i = dir.dnodes_no;
	dnode = dir.dnodes;
	while (i--) {
		if ((dnode->kind & d_del) == 0) {
			if ((dnode->kind & d_dir) && (strncmp("..", dnode->name, 2) != 0)) {
				pindent(level);
				pfilename(dnode->name,1);
				printf("\n");
				listDir(dnode->inode, level + 1);
			}
		}
		dnode++;
	}
}

void list()
{
	printf("/\n");
	listDir(0, 1);
}

void copy()
{
	copy_dir(0, "TMP", "");
}

void help()
{
	printf("xdu -- XD virtual vopume utility (c) 2025 Kronos\n");
	printf("usage:\n");
	printf("  xdu XDFile\n");
	printf("     Prnts XDfile volume file tree, like \"ls //*\"\n");
	printf("  xdu XDFile get\n");
	printf("    Copy all file tree to ./TMP/ directory\n\n");
}

int main(int argc, char** argv)
{
	if (argc < 2) {
		help(); return 0;
	}
	init_console();
	mount(argv[1]);
	if (argc > 2) {
		if (strcmp(argv[2], "get") == 0) 
			copy();
		else {
			help(); exit(0);
		}
	} else list();
	return 0;
}
