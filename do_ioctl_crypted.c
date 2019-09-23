#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#define SAMPLE_IOC_MAGIC 'k'
#define SAMPLE_IOCCRYPTED _IO(SAMPLE_IOC_MAGIC, 0)
#define MDP 10

int main(int argc, char **argv){
  int file;

  if(argc == 2){
    printf("Doing ioctl crypted on %s\n", argv[1]);
    file = open(argv[1], O_RDWR);
    if(file < 0){
      perror("open");
      printf("Error opening file %s!\n", argv[1]);
      return(errno);
    }
    ioctl(file,SAMPLE_IOCCRYPTED, MDP);
    close(file);
  }
  else
    printf("Usage: ./do_ioctl_crypted <filename>\n");
}
