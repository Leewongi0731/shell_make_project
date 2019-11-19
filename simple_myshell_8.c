#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>	/* zombie process management */
#include <signal.h>     /* signal management */
#include <fcntl.h>	/* redirection management */
#define MAX_CMD_ARG 20

const char *prompt = "myshell> ";
const char *redirection_in = "<";
const char *redirection_out = ">";
const char *pipe_in = "|";
char* cmdvector[MAX_CMD_ARG];
char  cmdline[BUFSIZ];
int background_flag = 0;
int redirection_flag = 0;
int pipe_flag = 0;

void fatal(char *str){
	perror(str);
	exit(1);
}

int makelist(char *s, const char *delimiters, char** list, int MAX_LIST){	
	int i = 0;
	int numtokens = 0;
	char *snew = NULL;
	redirection_flag = 0;
	pipe_flag = 0;

	if( (s==NULL) || (delimiters==NULL) ) return -1;

	snew = s + strspn(s, delimiters);	/* delimiters¸¦ skip */
	if( (list[numtokens]=strtok(snew, delimiters)) == NULL )
		return numtokens;

	numtokens = 1;

	while(1){
		if( (list[numtokens]=strtok(NULL, delimiters)) == NULL)
			break;
		if(numtokens == (MAX_LIST-1)) return -1;
		/* check redirection, pipe */
		if(!strcmp(list[numtokens], redirection_in) || !strcmp(list[numtokens], redirection_out))
			redirection_flag = 1;
		if(!strcmp(list[numtokens], pipe_in)) 
			pipe_flag = 1;
		numtokens++;
	}

	/* last commad '&' ->  background flag on */
	if(!strcmp(list[numtokens-1], "&")){
		background_flag = 1;
		list[numtokens-1] = '\0';
		numtokens--;
	}
	else background_flag = 0;

	return numtokens;
}

void reorderSignal(int signalNumber){	
	printf("\n");
	fputs(prompt, stdout);
	fgets(cmdline, BUFSIZ, stdin);
	return;
}

void ignoreSignal(int signalNumber){
	printf("\n");
	return;
}

void repipeCheck(char **list, int listSize){
	int in_index=-1, out_index=-1, i=0;
        int pipe_index[MAX_CMD_ARG] = {0}, pipe_num = 0;

	for(i ; i < listSize ; i++){
		if(!strcmp(list[i], redirection_in)) in_index = i;
		if(!strcmp(list[i], redirection_out)) out_index = i; 
                if(!strcmp(list[i], pipe_in)) pipe_index[pipe_num++] = i;
	}
	
	/* redirection input -> change standard I/O */
	if(in_index != -1){
		/* change standard input */
	       	inRedirection(list[in_index + 1]);
		list[in_index] = NULL;
	}
	if(out_index != -1){
		/* change standard output */
	       	outRedirection(list[out_index + 1]);
		list[out_index] = NULL;
	}
	
	if(pipe_flag == 1) pipeProcess(list, listSize, pipe_index, pipe_num);
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

/* pipe input -> pipeProcess */
void pipeProcess(char **list, int listSize, int *pipe_index, int pipe_num){
        char **command[pipe_num+1];
	int pipe_control[pipe_num][2], fork_num = 0, i = 0;
	pid_t pid;

        for(i = 0 ; i < pipe_num ; i++) if(pipe(pipe_control[i]) == -1) fatal("pipe : ");

        /* make part command list */
        for(i = 0 ; i < pipe_num ; i++) list[pipe_index[i]] = NULL;
        command[0] = list;
        for(i = 1 ; i <= pipe_num ; i++) command[i] = list + pipe_index[i-1] + 1;

        while(fork_num < pipe_num){
                pid = fork();
                if(pid <= -1) fatal("pipe : ");
                else if(pid == 0){
                        /* ouput -> pipe ouput */
                        dup2(pipe_control[fork_num][1], 1);
                        execvp(command[fork_num][0], command[fork_num]);
                        fatal(command[fork_num][0]);
                }else{
                        /* close ouput pipe (prevent deadlock) */
                        close(pipe_control[fork_num][1]);
                        /* input -> pipe input */
                        dup2(pipe_control[fork_num][0], 0);
                        wait(NULL);
                        fork_num++;
                }
        }

        execvp(command[fork_num][0], command[fork_num]);
        fatal(command[fork_num][0]);
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
	                        /* cd input -> no activate child process */
                                if(!strcmp(cmdvector[0], "cd") ) exit(0);
				/* redirection(<,>,|) input -> call repipeCheck function */
				if(redirection_flag == 1 || pipe_flag == 1) repipeCheck(cmdvector, numtokens);
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
