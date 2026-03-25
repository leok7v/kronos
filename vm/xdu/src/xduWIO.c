/*
* XDU -- Windows files IO
*/

#include "xduWIO.h"
#include "xduTime.h"
#include <windows.h>
#include <winbase.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
  
extern void fatal(char* fmt, ...);  // defined in xduDisk.c

static void set_time_attrs(HANDLE file, int created, int modified, char* fname)
{
	ULARGE_INTEGER wtime;
	FILETIME ct, wt;
	wtime.QuadPart = time_kronos_to_windows(created);
	ct.dwHighDateTime = wtime.HighPart;
	ct.dwLowDateTime = wtime.LowPart;
	wtime.QuadPart = time_kronos_to_windows(modified);
	wt.dwHighDateTime = wtime.HighPart;
	wt.dwLowDateTime = wtime.LowPart;
	if (!SetFileTime(file, &ct, NULL, &wt)) {
		fprintf(stderr, "\nWARNING: can't set time to \"%s\": %s\n", fname, strerror(errno));
	}
}

static int fileTimeToKronos(FILETIME* ft)
{
	SYSTEMTIME wt;
	if (!FileTimeToSystemTime(ft, &wt)) {
		return -1;
	}
	return pack_kronos_time(wt.wYear, wt.wMonth, wt.wDay, wt.wHour, wt.wMinute, wt.wSecond);
}

static bool get_time_attrs(HANDLE file, int* t_created, int* t_modified)
{
	FILETIME ct, mt;
	if (!GetFileTime(file, &ct, NULL, &mt)) {
		return false;
	}
	*t_created = fileTimeToKronos(&ct);
	*t_modified = fileTimeToKronos(&mt);
	return true;
}

static bool isTextExt(char* fname)
{
	int len = strlen(fname) - 1;
	while (len >= 0 && fname[len] != '.') len--;
	if (len >= 0) {
		char* ext = fname + len;
		return 
			(strcmp(ext, ".m")   == 0) || 
			(strcmp(ext, ".d")   == 0) ||
			(strcmp(ext, ".@")   == 0) ||
			(strcmp(ext, ".sh")  == 0) ||
			(strcmp(ext, ".txt") == 0) ||
			(strcmp(ext, ".doc") == 0);
	}
	return false;
}

#define RS (0x1e)
#define CR (0x0d)
#define LF (0x0a)

static char* toUTF8(char *src, int *src_len)
{
	int len = *src_len, lines = 0, i = 0;
	while (i < len) {
		if (src[i++] == RS) lines++;
	}
	
	char* str = malloc(len + lines);
	assert(str != NULL);

	int o = 0;
	i = 0;
	while (i < len) {
		if (src[i] == RS) {
			str[o++] = CR;
			str[o++] = LF;
		}
		else {
			str[o++] = src[i];
		}
		i++;
	}

	int wchar_len = MultiByteToWideChar(20866, 0, str, len + lines, NULL, 0);
	wchar_t* wideString = malloc(sizeof(wchar_t) * wchar_len);
	assert(wideString != NULL);
	MultiByteToWideChar(20866, 0, str, len + lines, wideString, wchar_len);

	int utf_len = WideCharToMultiByte(CP_UTF8, 0, wideString, wchar_len, NULL, 0, NULL, NULL);
	char* utf = malloc(utf_len + 1);
	assert(utf != NULL);
	WideCharToMultiByte(CP_UTF8, 0, wideString, wchar_len, utf, utf_len, NULL, NULL);
	utf[utf_len] = 0;

	free(wideString);
	free(str);
	*src_len = utf_len;
	return utf;
}

static char* fromUTF8(const char* src, int* src_len)
{
	int wchar_len = MultiByteToWideChar(CP_UTF8, 0, src, (*src_len), NULL, 0);
	wchar_t* wideString = malloc(sizeof(wchar_t) * wchar_len);
	assert(wideString != NULL);
	MultiByteToWideChar(CP_UTF8, 0, src, (*src_len), wideString, wchar_len);

	int dkoi_len = WideCharToMultiByte(20866, 0, wideString, wchar_len, NULL, 0, NULL, NULL);
	char* dkoi = malloc(dkoi_len);
	WideCharToMultiByte(20866, 0, wideString, wchar_len, dkoi, dkoi_len, NULL, NULL);
	free(wideString);

	int i = 0, o = 0, lines = 0; /* input, output positions and lines counter */
	while (i < dkoi_len) {
		if (dkoi[i] == CR) {
			if ((i+1) < dkoi_len && dkoi[i+1] == LF) {
				dkoi[o++] = RS;
				i++; lines++;
			} else {
				dkoi[o++] = CR;
			}
		}
		else {
			dkoi[o++] = dkoi[i];
		}
		i++;

	}

	*src_len = dkoi_len - lines;
	return dkoi;
}

void w_copy_file(char* path, char* content, int eof, int ctime, int wtime)
{
	char* converted = NULL;

	HANDLE file = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		fatal("ERROR creating file %s: %d", path, strerror(errno));
	}
	char* src = content;
	int src_len = eof;
	if (isTextExt(path)) { 
		converted = toUTF8(content, &src_len); 
		src = converted;
	}

	int written = 0;
	if (!WriteFile(file, src, src_len, &written, NULL)) {
		fatal("ERROR writing to file %s: %s\n", path, strerror(errno));
	}
	if (converted) { free(converted); converted = NULL; }
	set_time_attrs(file, ctime, wtime, path);
	if (!CloseHandle(file)) {
		fatal("ERROR closing %s: %s\n", path, strerror(errno));
		exit(1);
	}
}

void w_create_dir(char* path, int ctime, int wtime)
{
	if (!CreateDirectory(path, NULL)) {
		int error = GetLastError();
		if (error != ERROR_ALREADY_EXISTS) {
			fatal("Can not create directory \"%s\": %s\n", path, strerror(error));
		}
	}
}

void w_read_file(char* path, char** data, int* len, int* t_created, int* t_modified)
// returned in "data" buffer is malloc'ed, don't forget to free it after use
{
	HANDLE file = CreateFile(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		fatal("ERROR opening file %s: %s", path, strerror(errno));
	}

	unsigned long src_len = GetFileSize(file, NULL);
	char* src = malloc(src_len);
	if (src == NULL) {
		fatal("NO MEMORY to read file %s\n", path);
	}

	if (!ReadFile(file, src, src_len, NULL, NULL)) {
		fatal("ERROR reafing file %s: %s", path, strerror(errno));
	}

	*data = src;
	if (isTextExt(path)) {
		char *converted = fromUTF8(src, &src_len);
		free(src);
		*data = converted;
	}
	*len = src_len;
	if (!get_time_attrs(file, t_created, t_modified)) {
		fatal("Error getting timestamps of \"%s\"\n", path);
	}
	if (!CloseHandle(file)) {
		fatal("Error closing \"%s\"\n", path, strerror(errno));
	}
}
