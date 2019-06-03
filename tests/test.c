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
#define TESTMOD_IOCLASTSTRDROP    _IO(TESTMOD_IOC_MAGIC, 2)

#define TESTMOD_IOC_MAXNR 3


#define BUFFER_SIZE 5
#define ENDING_CONDITION 1
char *randomString(size_t);
char *wbuff;
char rbuff[100];

int test1(int fd);
int test2(int fd);
int test3(int fd);
int test4(int fd);

int main(int argc, char** argv) {

  int res = 0;
  int choice;
  srand(time(NULL));
  
  
  
  int fd = open(argv[1], O_RDWR);
   printf("Which test would you like to run? \n");
   scanf(" %i", &choice);
   printf("Running test %i.\n", choice);

   switch(choice)
   {
	   case 1:
               test1(fd);
	       break;
	   case 2:
               test2(fd);
	       break;
	   case 3:
               test3(fd);
	       break;
	   case 4:
               test4(fd);
	       break;
	   case 5:
               test5(fd);
	       break;
	    default:
	        printf("Wrong selection!\n");

   }
}

int test1(int fd){
  printf("Running ioctl test\n");
  printf("\n");
  printf("Removing last entry\n", ioctl(fd,TESTMOD_IOCLASTSTRDROP));
  
  return 0;

  
}

int test2(int fd){
  printf("Running ioctl test\n");
  printf("Using ioctl to reset a buffer/show buffer size\n");
  ioctl(fd,TESTMOD_IOCBUFFERRESET);
  return 0;

  
}

int test3(int fd){
  printf("Using ioctl to get a buffersize. \nCurrent buffersize is: %d.\n", ioctl(fd,TESTMOD_IOCBUFFERGET));
  printf("Check dmesg for more info \n");
  
  return 0;

  
}

int test4(int fd){
  printf("This test writes random string of size %d to device\n", BUFFER_SIZE);
  wbuff = randomString(BUFFER_SIZE);
  printf("\nGenerated string: %s\n", wbuff);
  write(fd, wbuff, BUFFER_SIZE);
  return 0;
  
}

int test5(int fd){
  printf("This test reads %d bytes drom the device\n", BUFFER_SIZE);
  read(fd, rbuff, sizeof(rbuff));
  printf("Data: %s\n", rbuff);
  return 0;
}

char *randomString(size_t length) {

    static char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,.-#'?!";
    char *rndstr = NULL;

    if (length) {
        rndstr = malloc(sizeof(char) * (length +1));

        if (rndstr) {
            for (int n = 0;n < length;n++) {
                int key = rand() % (int)(sizeof(charset) -1);
                rndstr[n] = charset[key];
            }

            rndstr[length] = '\0';
        }
    }

    return rndstr;
}
