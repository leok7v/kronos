#include <stdio.h>

long	lseek();

static int	msgfd,
		nmessages;

typedef struct {
	int 	m_nr, m_len;
	int	m_offset;
	} MSG;

static	MSG	*msg;

static	int	nofile;


msginit(fname) char *fname; {
	if ((msgfd = open(fname, 0)) == -1){
		nofile = 1;
		return(1);	/* SS. 16.Oct.87 to note unsuccessful open */
		}
	else 
		nofile = 0;
	read (msgfd, (char *)&nmessages, sizeof(int));
	if ((msg = (MSG *) malloc(nmessages*sizeof(MSG))) == (MSG *) 0) {
		nofile = 1;
		return(0);
		}
	read (msgfd, (char *)msg, nmessages*sizeof(MSG));
	return(0);
}


msgterm(){

	if (!nofile){
		close (msgfd);
		free ((char *)msg);
	}
}


char *
msgget (code, message) int code; char *message; {
	register int	beg, end, num, temp;

	if (nofile) {
		sprintf(message, "# %d", code);
		return (message);
		}
	beg = 0;
	end = nmessages - 1;
	for (;;) {
		num = (int) ((beg+end)/2);
		if (!(temp = code - msg[num].m_nr))
			break;
		if (temp < 0)
			end = num - 1;
		else
			beg = num + 1;
		if (beg > end) {
			sprintf(message, "# %d", code);
			return (message);
			}
		}
	if (lseek(msgfd, msg[num].m_offset, 0) == -1) {
		sprintf(message, "Error reading message # %d", code);
		return (message);
		}
	read (msgfd, message, msg[num].m_len);
	return (message);
}


