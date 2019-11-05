#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>	/* zombie process management */
#define MAX_CMD_ARG 10

const char *prompt = "myshell> ";
char* cmdvector[MAX_CMD_ARG];
char  cmdline[BUFSIZ];
int background_flag = 0;

void fatal(char *str){
	perror(str);
	exit(1);
}

int makelist(char *s, const char *delimiters, char** list, int MAX_LIST){	
	int i = 0;
	int numtokens = 0;
	char *snew = NULL;

	if( (s==NULL) || (delimiters==NULL) ) return -1;

	snew = s + strspn(s, delimiters);	/* delimiters¸¦ skip */
	if( (list[numtokens]=strtok(snew, delimiters)) == NULL )
		return numtokens;

	numtokens = 1;

	while(1){
		if( (list[numtokens]=strtok(NULL, delimiters)) == NULL)
			break;
		if(numtokens == (MAX_LIST-1)) return -1;
		numtokens++;
	}

	/* last commad '&' ->  background flag on */
	if(!strcmp(cmdvector[numtokens-1], "&")){
		background_flag = 1;
		cmdvector[numtokens-1] = '\0';
	}
	else background_flag = 0;

	return numtokens;
}

int main(int argc, char**argv){
	int i=0;
	pid_t pid;
	while (1) {
		fputs(prompt, stdout);
		fgets(cmdline, BUFSIZ, stdin);
		cmdline[ strlen(cmdline) -1] ='\0';

		/* zombie process management */
		waitpid(-1, NULL, WNOHANG);

		makelist(cmdline, " \t", cmdvector, MAX_CMD_ARG);
		/* exit input -> shell process exit. */
		if(!strcmp(cmdvector[0], "exit")) exit(0);

		switch(pid=fork()){
			case 0:
				/* cd input -> no activate child process */
				if(!strcmp(cmdvector[0], "cd") ) exit(0);
				execvp(cmdvector[0], cmdvector);
				fatal("main()");
			case -1:
				fatal("main()");
			default:
				if(background_flag == 0) wait(NULL);
				/* cd input -> parent process chage directory. */
				if(!strcmp(cmdvector[0], "cd") && chdir(cmdvector[1]) == -1 ) perror("main()");
		}
	}
	return 0;
}
