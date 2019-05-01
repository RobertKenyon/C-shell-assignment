// ROBERT KENYON 
// 260561896

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>

int getcmd(char *promp, char *args[], int *background, char *bg_cmds[], int bg_cmd_ctr)
{
	int length, i=0;
	char *token, *loc;
	char *line = NULL;
	size_t linecap = 0;

	printf("%s", promp);
	length = getline(&line, &linecap, stdin);

	if (length <=0)
	{
		exit(-1);
	}

	// check if background is specified..
	if ((loc = index(line, '&')) != NULL)
	{
		*background = 1;
		*loc = ' ';
		//next entry of bg_cmds gets filled with line
		bg_cmds[bg_cmd_ctr]=strdup(line);
	}else
		*background = 0;

	while((token = strsep(&line, " \t\n")) !=NULL)
	{
		for(int j=0; j<strlen(token); j++)
		{
			if (token[j] <=32)
			{
				token[j] = '\0';
			}
		}
		if (strlen(token) >0)
			args[i++]=token;
	}
	return i;
}

static void sigHandler(int sig) //only called in ctrl+c
{
	printf("\nCaught signal %d\n", sig);
	exit(1);
}

int redirect(char* args[], int index)
{
	args[index]=NULL;
	int status = 0;
	int store_1 = dup(1);
	close(1);
	int tempfile = open(args[index+1],O_RDWR | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);

	pid_t x = fork();
	if(x==0) //child only
	{
		if(execvp(args[0],args) <0)
			printf("unexecutable.");
		return -1; //unneeded, if it ever reaches this point execvp error'd
	}

	waitpid(x,&status,0); //wait for child to finish, then restore and close

	close(tempfile);
	dup2(store_1,1);
	close(store_1);

	return 1;
}

int piping(char *args[], int index)
{
	args[index]=NULL;
	int status = 0;
	int pipe_array[2];
	pid_t x;

	int store0 = dup(0); //stdout
	int store1 = dup(1); //stdin

	if(pipe(pipe_array)==0)
	{
		x = fork();
		if(x==0) //child
		{
			close(0); //close stdin, replace with pipe input
			dup(pipe_array[0]);
			execvp(args[0],args); //execute and terminate child
		}
		close(1);
		dup(pipe_array[1]);
		
		waitpid(x,&status,0);

		//now restore everything
		close(0);
		close(1);
		dup2(store0,0);
		dup2(store1,1);
		close(pipe_array[0]);
		close(pipe_array[1]);

		return 1;
	}
}

int main(void)
{
	signal(SIGINT, sigHandler); //make ctrl+c call function sigHandler
	signal(SIGTSTP, SIG_IGN); //ignore ctrl+z

	char *args[20];
	int bg;
	pid_t child_pid;
	pid_t parent_pid = getpid();
	int status = 0;

	char *bg_cmds[100]; //for jobs BIC, store bg cmds
	int bg_cmd_ctr = 0;
	pid_t pid_cmds[100]; //for fg BIC, store child pids
	for(int i=0;i<100;i++)
	{
		bg_cmds[i]=NULL;
	}

	int x = 0;

	while(1)
	{
		bg =0;
		int cnt = getcmd("\n>> ", args, &bg, bg_cmds, bg_cmd_ctr); //args holds the command
		if(cnt<=0)
			continue;
		args[cnt]=NULL;

		if (bg==1)
			bg_cmd_ctr++;

		//redirect + piping
		for(int k=0;k<cnt;k++)
		{
			if(strcmp(args[k],">")==0)
			{
				redirect(args,k);
				continue;
			}
			if(strcmp(args[k],"|")==0)
			{
				piping(args,k);
				continue;
			}
		}

		//built in commands
		if(strcmp(args[0],"cd") ==0)
		{
                	chdir(args[1]);
			continue;
		}
		if(strcmp(args[0],"pwd")==0)
                {
                	char *temp = (char *)malloc(100*sizeof(char));
                        getcwd(temp,100); //puts cwd into temp
                        printf("%s\n",temp);
			free (temp);
			continue;
		}
                if(strcmp(args[0],"exit") ==0)
                {
                	exit(1);
                }
                if(strcmp(args[0],"fg") ==0)
                {
                	x = atoi(args[1]);
			waitpid(pid_cmds[x],&status,0);
			bg_cmds[x]=NULL; //not needed normally, except jobs isn't implemented well
			continue;
                }
		if(strcmp(args[0],"jobs") ==0)
		{
			for(int i=0;i<100;i++)
			{
				if(bg_cmds[i]!=NULL)
				{
					printf("%d %s\n",i,bg_cmds[i]);
				}
			}
			continue;
		}

		//other commands, fork and use execvp()
		child_pid = fork();
		if(child_pid==0) //child only
		{
			if (bg==1)
			{
				pid_cmds[bg_cmd_ctr]=getpid();
			}
			if(execvp(args[0],args) <0) //still need to set bg_cmd[] back to NULL
			{
				printf("unexecutable\n");
			}
		}
		if (bg==0) //front = wait for child to finish
			waitpid(child_pid,&status,0);
	}
}
