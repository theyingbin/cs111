#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>
#include <mcrypt.h>
#include <fcntl.h>

static int ENCRYPT;
MCRYPT td;

struct thread_args{
  int shell_pid;
  int shell_input_fd;
};

void* forward_to_shell(void* arg);
void handler(int signum);
void exit_function();

int main(int argc, char *argv[])
{
  int sockfd, newsockfd, portno, clilen;

  // Get the --port argument
  static struct option longOptions[] = {
    {"port", required_argument, NULL, 'p'},
    {"encrypt", no_argument, &ENCRYPT, 1}
  };
  while(1){
    int index = 0;
    int c = getopt_long(argc, argv, "", longOptions, &index);
    if(c == -1)
      break;
    switch(c){
    case 'p':
      portno = atoi(optarg);
      break;
    case 0:
    default:
      break;
    }
  }

  if(ENCRYPT){
    atexit(exit_function);
    int keysize=16; /* 128 bits */
    char key[16];
    int keyFd;
    if( (keyFd = open("my.key", O_RDONLY)) < 0){
      fprintf(stderr, "error reading from my.key");
      exit(1);
    }
    if(read(keyFd, key, keysize) < keysize){
      fprintf(stderr, "my.key does not contain 128 bits(16 bytes)");
      exit(1);
    }
    close(keyFd);
    td = mcrypt_module_open("twofish", NULL, "cfb", NULL);
    if (td==MCRYPT_FAILED) {
      return 1;
    }
    char* IV = (char*) malloc(mcrypt_enc_get_iv_size(td));
    int i;
    for (i=0; i< mcrypt_enc_get_iv_size( td); i++) {
      IV[i]=rand();
    }
    i=mcrypt_generic_init( td, key, keysize, IV);
    if (i<0) {
      mcrypt_perror(i);
      exit(1);
    }
  }
  
  struct sockaddr_in serv_addr, cli_addr;
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0){
    fprintf(stderr, "ERROR opening socket\n");
    exit(1);
  }
  memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
    fprintf(stderr, "ERROR on binding\n");
    exit(1);
  }
  listen(sockfd,5);
  clilen = sizeof(cli_addr);
  newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
  close(sockfd);
  if (newsockfd < 0){
    fprintf(stderr, "ERROR on accept\n");
    exit(1);
  }

  // Set up pipes
  int pipe_from_shell[2];
  int pipe_to_shell[2];
  if(pipe(pipe_from_shell) == -1){
    fprintf(stderr, "pipe error\n");
    exit(1);
  }
  if(pipe(pipe_to_shell) == -1){
    fprintf(stderr, "pipe error\n");
    exit(1);
  }

  // Create a shell to execute the commands receieved from the client
  pid_t pid = fork();
  if(pid == 0){
    close(pipe_from_shell[0]);    // This closes the read end of pipe_from_shell
    dup2(pipe_from_shell[1], 1);  // This redirects stdout to the write end of pipe_from_shell
    close(pipe_to_shell[1]);      // This closes the write end of pipe_to_shell
    dup2(pipe_to_shell[0], 0);    // This redirects stdin to the read end of pipe_to_shell
    execl("/bin/bash", NULL);
  }
  else{
    close(pipe_to_shell[0]);
    close(pipe_from_shell[1]);
    
    dup2(newsockfd, 0);
    dup2(newsockfd, 1);
    dup2(newsockfd, 2);

    struct thread_args arg;
    arg.shell_pid = pid;
    arg.shell_input_fd = pipe_to_shell[1];
    pthread_t tid;
    pthread_create(&tid, NULL, &forward_to_shell, &arg);

    char buf[1];
    int size = 0;
    while( (size = read(pipe_from_shell[0], buf, 1)) > 0){
      if(ENCRYPT){
	mcrypt_generic (td, buf, 1);
      }
      write(1 ,buf, size);
    }
    close(0);
    kill(pid, SIGINT);
    exit(2);
  }
}

void* forward_to_shell(void* arg){
  int shell_fd = ((struct thread_args*) arg)->shell_input_fd;
  int pid = ((struct thread_args*) arg)->shell_pid;
  char buf[1];
  int size = 0;
  while( (size = read(0, buf, 1)) > 0){
    if(ENCRYPT){
      mdecrypt_generic (td, buf, 1);
    }
    write(shell_fd, buf, size);
  }
  close(0);
  kill(pid, SIGINT);
  exit(1);
}

void handler(int signum){
  close(0);
  exit(2);
}

void exit_function(){
  mcrypt_generic_end(td);
}
