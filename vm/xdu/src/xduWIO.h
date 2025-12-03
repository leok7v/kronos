#ifndef XDUWIO_INCLUDED
#define XDUWIO_INCLUDED

void w_copy_file(char* path, char* content, int eof, int ctime, int wtime);
void w_create_dir(char* path, int ctime, int wtime);
void w_read_file(char* path, char** data, int* len, int* t_created, int* t_modified); 
// returned via "char* data" buffer is malloc'ed, don't forget to free it after use

#endif