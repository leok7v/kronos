#include <stdio.h>
#include <fcntl.h>
#if unix
#define	FD "/dev/dsk/fd13"
#endif
#if excII
#define	FD "/dev/fe1"
#endif

#define SECTORSZ 512
#define BLOCKS 1280

main()
{
	int	i,j, fd, *k;
	char	*p, buf[SECTORSZ];

	if ( ( fd = open(FD,O_WRONLY )) == -1 )
	{
		perror("error during the floppy disk open\n");
		exit(1);
	}
	for(j=0;j<BLOCKS-1;j++)
	{
		for(i=0,p=buf;i<SECTORSZ;i++,p++)
			*p=(char)(j);
		memcpy(buf,"Sector no.  ",12);
		k = (int *)&buf[12];
		*k = j;
		if(write(fd,buf,SECTORSZ) != SECTORSZ)
		{
			perror("error during write\n");
			exit(1);
		}
	}
}
