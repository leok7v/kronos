#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>

/* COMP  -  program to compare two arbitrary files   MK  30.Mar.87 */

main(argc, argv)
int	argc;
char	* argv[];
{
int	fd1, fd2, i, j, n1, n2, err = 0;
union {unsigned int word; char a[4];} w1, w2;
char	* p1, * p2;

	if (argc < 3) {
		fprintf(stderr, "Usage: comp file1 file2\n");
		exit (0);
		}
	i = 1;
	if ((fd1 = open (argv[i], O_RDONLY)) == -1 ||
	    (fd2 = open (argv[++i], O_RDONLY)) == -1) {
		fprintf(stderr, "No file: %s\n", argv[i]);
		exit (0);
		}
	j = 0;
	p1 = (char *) & w1;
	p2 = (char *) & w2;
	n1 = read (fd1, p1, 4);
	n2 = read (fd2, p2, 4);
	while (n1 == 4 && n2 == 4) {
		j ++;
		if (w1.word != w2.word) {
			if (!err) {
				err = 1;
				printf("\n\t%s\t\t\t\t%s\n\n", 
							argv[1], argv[2]);
				}
			printf(" %3d:", j);
			hex (w1.a);
			ascii (w1.a);
			hex (w2.a);
			ascii (w2.a);
			printf("\n");
			}
		w1.word = w2.word = 0;
		n1 = read (fd1, p1, 4);
		n2 = read (fd2, p2, 4);
		}
	if (!j) {
		printf("File too small: %s\n", n1 < 4 ? argv[1] : argv[2]);
		exit (0);
		}
	if (n1 != n2) {
		printf("End of file %s. Last words:\n", n1 < n2 ?
			argv[1] : argv[2]);

			hex (w1.a);
			ascii (w1.a);
			hex (w2.a);
			ascii (w2.a);
			printf("\n");
		exit (0);
		}
	if (err) printf("End of both files\n");
	else printf("Files are identical\n");
}

hex (word)
char	* word;
{
int	i;

	printf("\t");
	for (i = 0; i <= 3; i ++)
		printf("%2x ", word[i]&0377);
}

ascii (word)
char	* word;
{
int	i;

	printf("\t");
	for (i = 0; i <= 3; i ++)
		printf(isprint(word[i]&0377) ? "%c " : ". ", word[i]);
}

