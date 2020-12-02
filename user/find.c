#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"


int
judge(char *path, char *pattern)
{
	int i, j;
	if(strlen(path) != strlen(pattern)){
		return 0;
	}
	i = 0, j = 0;
	while(i < strlen(path) && path[i] == pattern[j]){
		++i, ++j;
	}
	if(i == strlen(path)){
		return 1;
	}else{
		return 0;
	}
}
void
find(char *path, char *pattern)
{
	int fd;
	struct dirent de;
	struct stat st;
	if((fd = open(path, 0)) < 0){
		fprintf(2, "find: cannot open %s\n", path);
		return;
	}

	if(fstat(fd, &st) < 0){
		fprintf(2, "find: cannot stat %s\n", path);
		close(fd);
		return;
	}
	
	char buf[512], *p;
	p = buf;
	memmove(p, path, strlen(path));
	p[strlen(path)] = 0;
	p += strlen(path);
	switch(st.type){
	case T_FILE:
		if(judge(path, pattern) == 1){
			fprintf(1, "%s\n", path);
		}
		break;
	case T_DIR:
		while(read(fd, &de, sizeof(de)) == sizeof(de)){
			if(de.inum == 0){
				continue;
			}
			if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0){
				continue;
			}
			p[0] = '/';
			memmove(p + 1, de.name, DIRSIZ);
			p[DIRSIZ + 1] = 0;
			if(stat(buf, &st) < 0){
				fprintf(2, "ls: cannot stat %s\n", buf);
				continue;
			}
			if(st.type == T_FILE){
				if(judge(de.name, pattern) == 1){
					fprintf(1, "%s\n", buf);
				}
			}else if(st.type == T_DIR){
				find(buf, pattern);
			}
		}
		break;
	}
	close(fd);
}
int
main(int argc, char *argv[])
{
	if(argc < 3){
		fprintf(2, "find: parameter error.\n");
		exit(1);
	}

	find(argv[1], argv[2]);
	exit(0);
}
