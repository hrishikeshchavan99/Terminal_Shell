#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <malloc.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

int setflag[3];
int nextpos[3], pipecount, pipeindex, pos_pipe[64], minuspara, bgindex = 1, zflag;

struct bgprocess{
	int pids;
	int pstatus;
	char processname[32];
}bgp[32];

//to handle ctrl-c
void siginthandler(){
	signal(SIGINT, siginthandler);
}
//to handle ctrl-z
void sigtstphandler(){
	zflag = 1;
	signal(SIGTSTP, sigtstphandler);
	printf("\n[%d] + stopped", bgindex);
}
//to handle ctrl-z if it is pressed before taking input
void sigtstp2handler(){
	signal(SIGTSTP, sigtstp2handler);
}
//removes pipe and divide cmds into 2 parts.
int DivideIfPipe(char **cmds, char **cmds2, int pipepos){
	int i, j, k;	
	for (k = 0; k < 64; k++)
		cmds2[k] = NULL;
	i = 0;
	k = 0;				
	k = pipepos + 1;
		
	while (cmds[k] != NULL){
		cmds2[i] = cmds[k];
		cmds[k] = NULL;
		i++;
		k++;
	}
	
	cmds[pipepos] = NULL;
	cmds2[i] = NULL;

	minuspara = minuspara + pipepos + 1;
	return i;
}
// calls itself recursively if multiple pipes present. Before calling itself, input string will be divided into 2 parts.
int HandlePipe(char **cmds, char **cmds2){
	int pfd[2], pid1, pid2, i, k, pipepos;
	char **temp;
	pipecount--;
	
	temp = (char **)malloc(32 * sizeof(char *));
	for (i = 0; i < 32; i++){
		temp[i] = (char *)malloc(32 * sizeof(char));
	}

	pipe(pfd);
	pid1 = fork(); 
	if (pid1 == 0) {
		close(1); 
	       	dup(pfd[1]); 
	     	close(pfd[0]);   						
		execvp(cmds[0], cmds);
	}
	else {
		pid2 = fork();  
		if (pid2 == 0) { 
	        	close(0); 
	        	dup(pfd[0]); 
	        	close(pfd[1]);
	        		//if multiple pipes present
        			if (pipecount != 0){
					pipepos = pos_pipe[pipeindex++];
				        DivideIfPipe(cmds2, temp, pipepos - minuspara);
					HandlePipe(cmds2, temp);
				}        
        		execvp(cmds2[0], cmds2);	        	
	        } 
	        else { 
	        	close(pfd[0]);
			close(pfd[1]);
	        	wait(0); 
			wait(0); 	
		}
	}
	exit(0);
	return 1;

}
//checks if input string contains <, > or |.
int checkfortoken(char *token, int i){
	//if < or > present, saves file position also
	if (strcmp(token, "<") == 0){
		setflag[0] = 1;
		nextpos[0] = i + 1;	
	}
	if ((strcmp(token, ">")) == 0){
		setflag[1] = 1;
		nextpos[1] = i + 1;
	}
	
//	if pipe found, saves pipe positions.
	if (!strcmp(token, "|")){
		setflag[2] = 1;
		pos_pipe[pipecount] = i;
		pipecount++;
	}
}
//tokenizes input string.
int gettokens(char *cmd, char **cmds){
	int i = 0, j = 0;
	char *token = NULL;
	for (j = 0; j < 64; j++)
		cmds[j] = NULL;
	token = strtok(cmd, " ");
	
	while (token != NULL){
		cmds[i] = token;
		checkfortoken(token, i);
		i++;
		token = strtok(NULL, " ");
	}
	cmds[i] = NULL;
	return i;
}
int main(int argc, char *argv[]) {
	int pid, i = 0, j, k, fd1, fd2, filepos, pipepos, pfd[2], pid1, pid2, status;
	char **cmds, cmd[128], c, path[128], **cmds2;
	
	cmds = (char **)malloc(64 * sizeof(char *));
	cmds2 = (char **)malloc(64 * sizeof(char *));

	for (i = 0; i < 64; i++){
		cmds[i] = (char *)malloc(64 * sizeof(char));
		cmds2[i] = (char *)malloc(64 * sizeof(char));
	} 
 
	system("clear");
	while (1) {
		i = 0;
		for (k = 0; k < 3; k++)
			setflag[k] = 0;
		for (k = 0; k < 2; k++)
			nextpos[k] = 0;
		pipecount = 0;
		pipeindex = 0;
		minuspara = 0;
		zflag = 0;
		
		printf(ANSI_COLOR_BLUE "MyPrompt>" ANSI_COLOR_RESET); 

		printf(ANSI_COLOR_GREEN "%s" ANSI_COLOR_RESET, getcwd(path, 100));
		printf("$ ");

		signal(SIGINT, siginthandler);
		signal(SIGTSTP, sigtstp2handler);

		while (( c = getchar()) != '\n')
			cmd[i++] = c;
		cmd[i] = '\0';
		if (i == 0)
			continue;
		if (strcmp(cmd, "exit") == 0)
			break;
		
		signal(SIGINT, siginthandler);
		signal(SIGTSTP, sigtstphandler);

		j = gettokens(cmd, cmds);
		
		//prints all current running processes
		if (strcmp(cmds[0], "jobs") == 0){
			k = bgindex - 1;
			
			while (bgp[k].pstatus == 1)
				printf("%s\n", bgp[k--].processname);
			
		}
		else if (strcmp(cmds[0], "fg") == 0){
			if (bgp[bgindex - 1].pstatus == 1) {
				printf("%s\n", bgp[--bgindex].processname);
				kill(bgp[bgindex].pids, SIGCONT);
				waitpid(bgp[bgindex].pids, &status, WUNTRACED);
			}
		}
		else if (strcmp(cmds[0], "bg") == 0){
			if (bgp[bgindex - 1].pstatus == 1) {
				printf("%s\n", bgp[--bgindex].processname);
				kill(bgp[bgindex].pids, SIGCONT);
			}
		}
		else if (strcmp(cmds[0], "cd") == 0){
			chdir(cmds[1]);
		}
		else {
			pid = fork(); 
			
			if (pid == 0){
				signal(SIGINT, siginthandler);
				signal(SIGTSTP, sigtstphandler);

				if (setflag[2] == 1){
					pipepos = pos_pipe[pipeindex++];
					i = DivideIfPipe(cmds, cmds2, pipepos);
					j = pipepos;				
				}
				//to handle input redirection.
				if (setflag[0] == 1){
					close(0);
					filepos = nextpos[0];
					//if pipe present and filename is present after pipe, adjust filepos as index of file has been changed
					if (filepos >= j){
						filepos = filepos - j - 1;
						fd1 = open(cmds2[filepos], O_RDONLY);		
						
						if ( fd1 == -1) {
							perror("Open Failed:\n");
							return errno;
						}
						k = filepos - 1;
						while (cmds2[k + 2] != NULL){
							strcpy(cmds2[k], cmds2[k + 2]);
							k++;
						}
						cmds2[k] = NULL;
						cmds2[k + 1] = NULL;
						i -= 2;
					}
					//else take prevoiusly calculated file position.
					else {
						fd1 = open(cmds[filepos], O_RDONLY);
						
						if ( fd1 == -1) {
							perror("Open Failed:\n");
							return errno;
						}
						k = filepos - 1;
						while (cmds[k + 2] != NULL){
							strcpy(cmds[k], cmds[k + 2]);
							k++;
						}
						cmds[k] = NULL;
						cmds[k + 1] = NULL;
						j -= 2;
						nextpos[1] = nextpos[1] - 2;
					}
							
				}
				//to handle output redirection
				if (setflag[1] == 1){
					close(1);
					filepos = nextpos[1];
					//if pipe present and filepos is after pipe, adjust file position.						
					if (filepos >= j){
						filepos = filepos - j - 1;
						fd2 = open(cmds2[filepos], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR|S_IWUSR);
											
						if ( fd2 == -1) {
							perror("Open Failed: \n");
							return errno;
						}
						k = filepos - 1;
						while (cmds2[k + 2] != NULL){
							strcpy(cmds2[k], cmds2[k + 2]);
							k++;
						}
						cmds2[k] = NULL;
						cmds2[k + 1] = NULL;
						i -= 2;
					}
					//else take prevoiusly calculated file position.
					else {
						fd2 = open(cmds[filepos], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR|S_IWUSR);
					
						if ( fd2 == -1) {
							perror("Open Failed: \n");
							return errno;
						}
						k = filepos - 1;
						while (cmds[k + 2] != NULL){
							strcpy(cmds[k], cmds[k + 2]);
							k++;
						}
						cmds[k] = NULL;
						cmds[k + 1] = NULL;
					
						j -= 2;
					}
				}
				//if pipe present
				if (setflag[2] == 1){
					HandlePipe(cmds, cmds2);
					exit(0);
				}
				else
					execvp(cmds[0], cmds);
			}	
			else {	
				waitpid(pid, &status, WUNTRACED);
				//if ctrl-z is pressed
				if (zflag == 1){
					bgp[bgindex].pids = pid;
					bgp[bgindex].pstatus = 1;
					strcpy(bgp[bgindex].processname, cmd);
					printf("\t\t\t%s\n", bgp[bgindex].processname);
					bgindex++;
				}

			}
		}
	}
	return 0;
}
