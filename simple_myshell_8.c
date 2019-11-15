#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>	/* zombie process management */
#include <signal.h>     /* signal management */
#include <fcntl.h>	/* redirection management */
#define MAX_CMD_ARG 10

const char *prompt = "myshell> ";
const char *redirection_in = "<";
const char *redirection_out = ">";
const char *pipe_in = "|";
char* cmdvector[MAX_CMD_ARG];
char  cmdline[BUFSIZ];
int background_flag = 0;
int redirection_flag = 0;

void fatal(char *str){
	perror(str);
	exit(1);
}

int makelist(char *s, const char *delimiters, char** list, int MAX_LIST){	
	int i = 0;
	int numtokens = 0;
	char *snew = NULL;
	redirection_flag = 0;

	if( (s==NULL) || (delimiters==NULL) ) return -1;

	snew = s + strspn(s, delimiters);	/* delimiters¸¦ skip */
	if( (list[numtokens]=strtok(snew, delimiters)) == NULL )
		return numtokens;

	numtokens = 1;

	while(1){
		if( (list[numtokens]=strtok(NULL, delimiters)) == NULL)
			break;
		if(numtokens == (MAX_LIST-1)) return -1;
		/* check redirection */
		if(!strcmp(list[numtokens], redirection_in) ||
		   !strcmp(list[numtokens], redirection_out) ||
		   !strcmp(list[numtokens], pipe_in))
			redirection_flag = 1;
		numtokens++;
	}

	/* last commad '&' ->  background flag on */
	if(!strcmp(list[numtokens-1], "&")){
		background_flag = 1;
		list[numtokens-1] = '\0';
	}
	else background_flag = 0;

	return numtokens;
}

void reorderSignal(int unused){	
	printf("\n");
	fputs(prompt, stdout);
	fgets(cmdline, BUFSIZ, stdin);
	return;
}

void ignoreSignal(int signalNumber){
	printf("\n");
	return;
}

int redirection(char **list, int listSize){
	int in_index=-1, out_index=-1, i=0;
	int pipe_index[MAX_CMD_ARG] = {0}, pipe_num = 0;

	for(i ; i < listSize ; i++){
		if(!strcmp(list[i], redirection_in)) in_index = i;
		if(!strcmp(list[i], redirection_out)) out_index = i; 
                if(!strcmp(list[i], pipe_in)) pipe_index[pipe_num++] = i;
	}
	
	if(in_index != -1 && out_index != -1){
                /* inRedirection & outRedirection */
                inRedirection(list[in_index + 1]);
                outRedirection(list[out_index + 1]);
                list[in_index] = NULL;
                execvp(list[0], list);
                fatal("redirection : ");
	}
	else if(in_index != -1){
                /* only inRedirection */
                inRedirection(list[in_index + 1]);
                list[in_index] = NULL;
                execvp(list[0], list);
                fatal("inRedirection : ");
	}
	else if(out_index != -1){
                /* only outRedirection */
                outRedirection(list[out_index + 1]);
                list[out_index] = NULL;
                execvp(list[0], list);
                fatal("outRedirection : ");
	}

	exit(0);
}


/* standard input -> file */
void inRedirection(char *filename){
	int fs = open(filename, O_RDONLY);
	if(fs == -1) fatal(filename);
	dup2(fs, 0);
	close(fs);
}

/* standard output -> file */
void outRedirection(char *filename){
	int fs = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if(fs == -1) fatal(filename);
	dup2(fs, 1);
	close(fs);
}

int main(int argc, char**argv){
	int i=0, numtokens;
	pid_t pid;
	
	/* signal management */
	struct sigaction reorder, ignore;
	sigemptyset(&reorder.sa_mask); reorder.sa_handler = reorderSignal; reorder.sa_flags = SA_NOMASK;	
	sigemptyset(&ignore.sa_mask); ignore.sa_handler = ignoreSignal; ignore.sa_flags = SA_NOMASK;
	
	/* zombie process management */
	struct sigaction zombie_manager;
	sigemptyset(&zombie_manager.sa_mask);
	zombie_manager.sa_handler = SIG_IGN;
	zombie_manager.sa_flags = SA_NOCLDSTOP;
	sigaction(SIGCHLD, &zombie_manager , NULL);

	while (1) {
		/* shell process signal -> retry input command */
		sigaction(SIGINT, &reorder, NULL); sigaction(SIGQUIT, &reorder, NULL); sigaction(SIGTSTP, &reorder, NULL);
		
		fputs(prompt, stdout);
		fgets(cmdline, BUFSIZ, stdin);
		cmdline[ strlen(cmdline) -1] ='\0';

		numtokens = makelist(cmdline, " \t", cmdvector, MAX_CMD_ARG);
		/* exit input -> shell process exit. */
		if(!strcmp(cmdvector[0], "exit")) exit(0);

		switch(pid=fork()){
			case 0:
				/* child process signal -> collect work */
				signal(SIGINT, SIG_DFL); signal(SIGQUIT, SIG_DFL); signal(SIGTSTP, SIG_DFL);
				/* redirection(<,>,|) input -> call redirection function */
				if(redirection_flag == 1) redirection(cmdvector, numtokens);
				/* cd input -> no activate child process */
				if(!strcmp(cmdvector[0], "cd") ) exit(0);
				execvp(cmdvector[0], cmdvector);
				fatal("main()");
			case -1:
				fatal("main()");
			default:
				/* shell process signal -> ignore */
				sigaction(SIGINT, &ignore, NULL); sigaction(SIGQUIT, &ignore, NULL); sigaction(SIGTSTP, &ignore, NULL);
				if(background_flag == 0) wait(NULL);
				/* cd input -> parent process chage directory. */
				if(!strcmp(cmdvector[0], "cd") && chdir(cmdvector[1]) == -1 ) perror("main()");
		}
	}
	return 0;
}
