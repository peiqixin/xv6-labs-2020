#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
	int i;
	if(argc <= 1 || argc > 2){
		fprintf(2, "sleep: command error\n");
		exit(1);
	}
	for(i = 0; i < strlen(argv[1]); i++){
		if (argv[1][i] < '0' || argv[1][i] > '9'){
			fprintf(2, "sleep: parameter must be a number\n");
			exit(1);
		}
	}
	int ticks = atoi(argv[1]);
	sleep(ticks);
	exit(0);
}

