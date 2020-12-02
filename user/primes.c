#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
is_prime(int x)
{
	int i;
	for(i = 2; i * i <= x; ++i){
		if(x % i == 0){
			return 0;
		}
	}
	return 1;
}

int
main(int argc, char *argv[])
{
	int p[2];
	pipe(p);
	int i;
	for(i = 2; i <= 35; ++i){
		if(is_prime(i) == 0){
			continue;
		}
		if(fork() == 0){
			int num, n;
			n = read(p[0], &num, 4);
			// close(p[0]);
			if(n != 4){
				fprintf(2, "primes: read error %d\n", num);
				exit(1);
			}
			fprintf(1, "prime %d\n", num);
		}else{
			int n = write(p[1], &i, 4);
			// close(p[1]);
			if(n != 4){
				fprintf(2, "primes: write error\n");
				exit(1);
			}
			wait(0);
			exit(0);
		}
	}
	exit(0);
}
