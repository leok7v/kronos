#include "xduWIO.h"
#include <windows.h>
#include <winbase.h>
#include "stdio.h"
#include "xduTime.h"
#include <assert.h>
  
extern void fatal(char* fmt, ...);

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

static int get_time_attrs(HANDLE file, int* t_created, int* t_modified)
{
	FILETIME ct, mt;
	if (!GetFileTime(file, &ct, NULL, &mt)) {
		return 1;
	}
	*t_created = windows_time_to_kronos(&ct);
	*t_modified = windows_time_to_kronos(&mt);
	return 0;
}

int isText(char* fname)
{
	int len = strlen(fname);
	char* c = fname + len;
	do { len--; c--; } while (len && *c != '.');
	return ((len != 0) && ((strcmp(c, ".m") == 0) || (strcmp(c, ".d") == 0)));
}

#define RS (0x1e)
#define CR (0x0d)
#define LF (0x0a)

char* toUTF8(char *src, int *src_len)
{
	int i = *src_len;
	int lines = 0;
	char* c = src;
	while (i) {
		if (*(c++) == RS) lines++;
		i--;
	}
	char* str = malloc(*src_len + lines);
	assert(str != NULL);
	c = src;
	char* d = str;
	i = *src_len;
	while (i) {
		if (*c == RS) { *(d++) = CR; *(d++) = LF; }
		else *(d++) = *c;
		c++; i--;
	}

	int wchar_len = MultiByteToWideChar(20866, 0, str, (*src_len) + lines, NULL, 0);
	wchar_t* wideString = malloc(sizeof(wchar_t) * wchar_len);
	MultiByteToWideChar(20866, 0, str, (*src_len) + lines, wideString, wchar_len);

	int utf_len = WideCharToMultiByte(CP_UTF8, 0, wideString, wchar_len, NULL, 0, NULL, NULL);
	char* utf = malloc(utf_len + 1);
	WideCharToMultiByte(CP_UTF8, 0, wideString, wchar_len, utf, utf_len, NULL, NULL);
	utf[utf_len] = 0;

	free(wideString);
	free(str);
	*src_len = utf_len;
	return utf;
}

char* fromUTF8(const char* src, int* src_len)
{
	int wchar_len = MultiByteToWideChar(CP_UTF8, 0, src, (*src_len), NULL, 0);
	wchar_t* wideString = malloc(sizeof(wchar_t) * wchar_len);
	assert(wideString != NULL);
	MultiByteToWideChar(CP_UTF8, 0, src, (*src_len), wideString, wchar_len);

	int dkoi_len = WideCharToMultiByte(20866, 0, wideString, wchar_len, NULL, 0, NULL, NULL);
	char* dkoi = malloc(dkoi_len);
	WideCharToMultiByte(20866, 0, wideString, wchar_len, dkoi, dkoi_len, NULL, NULL);
	free(wideString);

	char *d = dkoi, *s = dkoi;
	int i = dkoi_len, lines = 0;
	while (i) {
		char c = *s++;
		if (c == CR) {
			if ((i > 1) && (*s == LF)) {
				*(d++) = RS; lines++;
				i--; s++; 
			} else {
				*d++ = CR;
			}
		} else {
			*d++ = c;
		}
		i--;
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
	if (isText(path)) { 
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
/*  Commented out because did not work for directories and also is not much necessary for directories
	HANDLE file = CreateFile(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL );
	if (file == INVALID_HANDLE_VALUE) {
		printf("\nERROR opening directory %s for setting creation time\n", path);
		return;
	}
	set_time_attrs(file, ctime, wtime);
	if (!CloseHandle(file)) {
		printf("ERROR closing % s: % d\n", path, GetLastError());
		exit(1);
	}
*/
}

void w_read_file(char* path, char** data, int* len, int* t_created, int* t_modified)
// returned  in "data" buffer is malloc'ed, don't forget to free it after use
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
	if (isText(path)) {
		char *converted = fromUTF8(src, &src_len);
		free(src);
		*data = converted;
	}
	*len = src_len;
	if (get_time_attrs(file, t_created, t_modified)) {
		fatal("Error getting timestamps of \"%s\"\n", path);
	}
	if (!CloseHandle(file)) {
		fatal("Error closing \"%s\"\n", path, strerror(errno));
	}
}
