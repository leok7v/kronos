#include <stdio.h>
#include <fcntl.h>

#define BUFSIZE 1024

main(argc, argv)
char	*argv[];
{int	opt = 0, i, j, num, fd, fd1, blksize, n, filenr;
 char	*error = "\n*** ERROR *** ", buf[BUFSIZE], 
	line[100], filename[40], ofile[40];

	if (argc == 2) {
		if (sscanf(argv[1], "%d", &opt) != 1 || !(opt == 1 || opt == 2))
			opt = 0;
		}
	if (!opt) {
		fprintf(stderr, "1 -- split file into parts\n");
		fprintf(stderr, "2 -- merge files into one\n");
		fprintf(stderr, "? ");
		gets(line);
		if (sscanf(line, "%d", &opt) != 1 || !(opt == 1 || opt == 2))
			exit(0);
		}
	if (opt == 1) {
		fprintf(stderr, "File name: ");
		gets(line);
		if (strlen(line) < 2) exit(0);
		strcpy(filename, line);
		if ((fd = open(line, O_RDONLY)) == -1) {
			perror(error);
			exit(0);
			}
		fprintf(stderr, "Block size in K ( <Ret> for 32K): ");
		gets(line);
		if (!strlen(line)) blksize = 32;
		else {
			if (sscanf(line, "%d", &num) != 1 || num < 0) {
				fprintf(stderr, error);
				fprintf(stderr, "Illegal block size\n");
				exit(0);
				}
			blksize = num;
			}
		strcat(filename, "1");
		fprintf(stderr, "%s ", filename);
		if ((fd1 = open(filename, O_CREAT|O_WRONLY, 0777)) == -1) {
			perror(error);
			exit(0);
			}
		i = 0;
		j = 1;
		n = read(fd, buf, BUFSIZE);
		while(n == BUFSIZE) {
			write(fd1, buf, BUFSIZE);
			if (++ i == blksize) {
				i = 0;
				close(fd1);
				fprintf(stderr, "\n");
				j ++;
				*(filename+strlen(filename)-1) = j+'0';
				fprintf(stderr, "%s ", filename);
				if ((fd1 = open(filename, O_CREAT|O_WRONLY,
					0777)) == -1) {
						perror(error);
						exit(0);
						}
				}
			n = read(fd, buf, BUFSIZE);
			}
		write(fd1, buf, n);
		close(fd1);
		fprintf(stderr, "\n");
		close(fd);
		exit(0);
		}
	/* p == 2 */
	fprintf(stderr, "File name cookie: ");
	gets(line);
	if (strlen(line) < 2) exit(0);
	strcpy (filename, line);
	fprintf(stderr, "Number of files: ");
	gets(line);
	if (sscanf(line, "%d", &filenr) != 1 || filenr < 1)
		exit(0);
	fprintf(stderr, "Output file name ( <Ret> for %s): ", filename);
	gets(line);
	if (!strlen(line)) strcpy(ofile, filename);
	else strcpy(ofile, line);
	if ((fd = open(ofile, O_CREAT|O_WRONLY, 0777)) == -1) {
		perror(error);
		exit(0);
		}
	for (i = 1; i <= filenr; i ++) {
		sprintf(line, "%s%d", filename, i);
		fprintf(stderr, "%s", line);
		if ((fd1 = open(line, O_RDONLY)) == -1) {
			perror(error);
			exit(0);
			}
		n = read(fd1, buf, BUFSIZE);
		while (n == BUFSIZE) {
			write(fd, buf, BUFSIZE);
			n = read(fd1, buf, BUFSIZE);
			}
		write(fd, buf, n);
		close(fd1);
		fprintf(stderr, "\n");
		}
	close(fd);
	exit(0);
}

