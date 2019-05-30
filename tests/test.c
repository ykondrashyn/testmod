#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#define TESTMOD_IOC_MAGIC  82

#define TESTMOD_IOCBUFFERRESET    _IO(TESTMOD_IOC_MAGIC, 0)
#define TESTMOD_IOCBUFFERGET    _IO(TESTMOD_IOC_MAGIC, 1)

#define TESTMOD_IOC_MAXNR 2


#define BUFFER_SIZE 5
#define ENDING_CONDITION 1
static void *randomString(char*, int);

int test1(int fd);

int main(int argc, char** argv) {

  int res = 0;
  int choice;
  
  
  
  int fa = open("/dev/testmod0", O_RDWR);
  int fb = open("/dev/testmod1", O_RDWR);
  int fc = open("/dev/testmod2", O_RDWR);

   test1(fa);
   test1(fb);
   test1(fc);


}

int test1(int fd){

  printf("Running ioctl tests\n");
  printf("\n");
  printf("Using ioctl to get a buffersize. Current buffersize is: %d.\n", ioctl(fd,TESTMOD_IOCBUFFERGET));
  printf("Using ioctl to reset a buffer \n");
  ioctl(fd,TESTMOD_IOCBUFFERRESET);
  printf("Using ioctl to get a buffersize. Current buffersize is: %d.\n", ioctl(fd,TESTMOD_IOCBUFFERGET));
  printf("Check dmesg for more info \n");
  
  return 0;

  
}
