#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
	int p[2];
	pipe(p);
	fprintf(1, "pipe[0]: %d, for read.\npipe[1]: %d, for write.\n");
	char ch[1];
	int n, pid;
	if ((pid = fork()) == 0){
		close(0);
		dup(p[0]);
		n = read(p[0], ch, 1);	
		if(n != 1){
			fprintf(2, "pingpong: read error\n");
			exit(1);
		}
		fprintf(1, "%d: received ping\n", getpid());
		write(p[1], " ", 1);
		close(p[0]);
		close(p[1]);
	}else{
		write(p[1], " ", 1);
		close(p[1]);
		wait(0);
		n = read(p[0], ch, 1);
		close(p[0]);
		if(n != 1){
			fprintf(2, "pingpong: parent read error\n");
			exit(1);
		}
		fprintf(1, "%d: received pong\n", getpid());
	}
	exit(0);
}
