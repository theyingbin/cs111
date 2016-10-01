#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <signal.h>

static int segfault = 0;
static int catch = 0;
void handler(int signum);
int main(int argc, char* argv[]){
  static struct option longOptions[] = {
    {"input", required_argument, NULL, 'i'},
    {"output", required_argument, NULL, 'o'},
    {"segfault", no_argument, &segfault, 1},
    {"catch", no_argument, &catch, 1}
  };
  // Get options
  while(1){
    int index = 0;
    int c = getopt_long(argc, argv, "", longOptions, &index);
    if(c == -1)
      break;
    int fd;
    switch(c){
    case 'i':
      // Redirection of input
      fd = open(optarg, O_RDONLY);
      if(fd == -1){
	fprintf(stderr, "Cannot open %s\n", optarg);
	exit(1);
      }
      close(0);
      dup(fd);
      close(fd);
      break;
    case 'o':
      // Redirection of output
      fd = creat(optarg, S_IWUSR | S_IRUSR);
      if(fd == -1){
	fprintf(stderr, "Cannot create or write to %s\n", optarg);
	exit(2);
      }
      close(1);
      dup(fd);
      break;
    case 0:
    default:
      break;
    }
  }
  if(catch){
    signal(SIGSEGV, handler);
  }
  if(segfault){
    char* ptr = NULL;
    char err = *ptr;
  }
  int ch[1];
  while(1){
    int readStatus = read(0, &ch, 1);
    if(readStatus < 0){
      fprintf(stderr, "Read failed with error %d\n", readStatus);
      exit(-1);
    }
    if(readStatus == 0)
      break;
    write(1, &ch, 1);
  }
  exit(0);
}
void handler(int signum){
  fprintf(stderr, "Segfault caught. Exiting with error code 3");
  exit(3);
}
