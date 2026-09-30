#include <stdio.h>

main(argc, argv)
int	argc;
char	*argv[];
{

	char buf1[200], buf2[200], line[20], *lp, *b1, *b2, c;
	int i=0, diff=0, skip, skip1;
	FILE * fopen(), *fp1, *fp2;

	if (argc != 3) {
		fprintf(stderr, "Usage: mkcmp file1 file2\n");
		exit (0);
		}
	if ((fp1 = fopen(*++argv, "r")) == NULL ||
	    (fp2 = fopen(*++argv, "r")) == NULL) {
		fprintf(stderr, "Can't open %s\n", *argv);
		exit (0);
		}
	fgets(buf1, 200, fp1);
	fgets(buf2, 200, fp2);
	for (;;i++) {
		b1 = buf1;
		while (*b1 == ' ' || *b1 == '\t') b1++;
		b2 = buf2;
		while (*b2 == ' ' || *b2 == '\t') b2++;
		if (strcmp(b1, b2)) {
			diff = 1;
			printf("**Line %d :\n", i);
			printf("A : %s", buf1);
			printf("B : %s", buf2);
		    sw: printf("?");
			skip = 0;
			lp = line;
			while ((c = getchar()) != '\n')
				*lp++ = c;
			*lp = '\0';
			c = *line;
			lp = line + 1;
			switch (c) {
		case 'A':
			if (sscanf(lp, "%d", &skip) != 1)
				goto sw;
			skip1 = skip;
			while (skip-- > 0) {
				i++;
				fgets(buf1, 200, fp1);
				b1 = buf1;
				while (*b1 == ' ' || *b1 == '\t') b1++;
				if (!strcmp(b1, b2)) {
			printf("%d lines skipped in A\n", skip1 - skip);
					break;
					}
				}
			break;
		case 'B':
			if (sscanf(lp, "%d", &skip) != 1)
				goto sw;
			skip1 = skip;
			while (skip-- > 0) {
				fgets(buf2, 200, fp2);
				b2 = buf2;
				while (*b2 == ' ' || *b2 == '\t') b2++;
				if (!strcmp(b1, b2)) {
			printf("%d lines skipped in B\n", skip1 - skip);
					break;
					}
				}
			break;
		case '\0': break;
		default: goto sw;
		}
		}
		fgets(buf1, 200, fp1);
		fgets(buf2, 200, fp2);
		if (feof(fp1)) {
			if (!feof(fp2))
				printf("End of file A\n");
		else printf(diff? "End of both files\n":
				  "Files are identical\n");
			exit (0);
			}
		if (feof(fp2)) {
			printf("End of file B\n");
			exit (0);
			}
		}
	}

