#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <error.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define BUFFSIZE 4096

int main(){
	ssize_t br;
	int fd;
	char buf[BUFFSIZE];

	strcpy(buf, "<pwrite test>\n<pwrite test>\n<pwrite test>\n<pwrite test>\n<pwrite test>\n<pwrite test>\n");


	fd = open("/home/seonjoo/Documents/hybrid/schedule.log", O_RDWR | O_CREAT, 0777);

	if( fd == -1 ){
		perror("open");
	}

	br = pwrite(fd, buf, strlen(buf), (off_t) 200);
	if( br == -1 ){
		perror("pwrite");
	}


	return 0;
}