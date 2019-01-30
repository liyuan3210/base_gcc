#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include "memdev.h"  /* ��������� */

/* ��������MEMDEV_IOCSETDATA */
static inline void setpos(int fd, int pos) {
	printf("ioctl: Call MEMDEV_IOCSETDATA to set position\n");
	ioctl(fd, MEMDEV_IOCSETDATA, &pos);
}

/* ��������MEMDEV_IOCGETDATA */
static inline int getpos(int fd) {
    int pos;
	printf("ioctl: Call MEMDEV_IOCGETDATA to get position\n");
	ioctl(fd, MEMDEV_IOCGETDATA, &pos);
    return pos;
}

int main(void)
{
	int fd = 0;
	int arg = 0;
	char buf[256];
	
	/*���豸�ļ�*/
	if (-1==(fd=open("/dev/memdev0",O_RDWR))) {
		printf("Open Dev Mem0 Error!\n");
		_exit(EXIT_FAILURE);
	}
	
	/* ��������MEMDEV_IOCPRINT */
	printf("ioctl: Call MEMDEV_IOCPRINT to printk in driver\n");
    ioctl(fd, MEMDEV_IOCPRINT, &arg);
    printf("\n");
    strcpy(buf, "This is a test for ioctl");
    printf("write %d bytes in new open file\n", strlen(buf));
    write(fd, buf, strlen(buf)); 
    printf("new pos is %d\n", getpos(fd));
    printf("\n");
    setpos(fd, 0);
    printf("set pos = 0\n");
    printf("after setpos(fd, 0), new pos is %d\n", getpos(fd));
     
	close(fd);
	return 0;	
}
