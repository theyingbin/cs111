#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <fcntl.h>
#include <mcrypt.h>

static int ENCRYPT;
static int logFlag = 0;
int logFd;
MCRYPT td;

struct termios attr_saved;

void restore_mode();
void restore_mode_encryption();
void* read_from_server(void* arg);

int main(int argc, char* argv[]){
  int portno;
  static struct option longOptions[] = {
    {"port", required_argument, NULL, 'p'},
    {"log", required_argument, NULL, 'l'},
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
    case 'l':
      logFlag = 1;
      logFd = creat(optarg, S_IWUSR | S_IRUSR);
      break;
    case 0:
    default:
      break;
    }
  }

  if(ENCRYPT){
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
  
  tcgetattr(0, &attr_saved);
  struct termios tattr;
  tcgetattr(0, &tattr);
  tattr.c_lflag &= ~(ICANON|ECHO); // This changes the local mode to non-canonical input mode with no echo
  tattr.c_cc[VTIME] = 0;
  tattr.c_cc[VMIN] = 1;
  tcsetattr(0, TCSAFLUSH, &tattr);

  if(ENCRYPT){
    atexit(restore_mode_encryption);
  }
  else{
    atexit(restore_mode);
  }
  int sockfd;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("ERROR opening socket");
    exit(1);
  }

  server = gethostbyname("localhost");

  if (server == NULL) {
    fprintf(stderr,"ERROR, no such host\n");
    exit(0);
  }

  memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  memcpy((char *)&serv_addr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
  serv_addr.sin_port = htons(portno);

  /* Now connect to the server */
  if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("ERROR connecting");
    exit(1);
  }

  pthread_t tid;
  pthread_create(&tid, NULL, &read_from_server, &sockfd);
  /* Now ask for a message from the user, this message
   * will be read by server
   */

  char buffer[1];
  int size = 0;
  
  while((size=read(0, buffer, 1)) > 0){
    if(buffer[0] == '\004'){
      close(sockfd);
      exit(0);
    }
    write(1, buffer, 1);
    if(ENCRYPT){
      mcrypt_generic (td, buffer, 1);
    }
    write(sockfd, buffer, 1);
    if(logFlag){
      write(logFd, "SENT 1 bytes: ", 14);
      write(logFd, buffer, size);
      write(logFd, "\n", 1);
    }
  }
}

void restore_mode(void){
  tcsetattr(0, TCSANOW, &attr_saved);
}

void restore_mode_encryption(void){
  mcrypt_generic_end(td);
  tcsetattr(0, TCSANOW, &attr_saved);
}

void* read_from_server(void* arg){
  int sockfd = *((int*)arg);
  int buf[1];
  int size = 0;
  while( (size = read(sockfd, buf, 1)) > 0){
    if(logFlag){
      write(logFd, "RECEIVED 1 bytes: ", 18);
      write(logFd, buf, size);
      write(logFd, "\n", 1);
    }

    if(ENCRYPT){
      mdecrypt_generic (td, buf, 1);
    }
    
    if(buf[0] == '\004'){
      close(sockfd);
      exit(1);
    }
    write(1, buf, 1); 
  }
  close(sockfd);
  exit(1);
}
