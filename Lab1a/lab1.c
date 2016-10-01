#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/types.h>

struct termios attr_saved;

static int shell = 0;
int shell_eof = 0;

void restore_mode(void);
void restore_mode_shell(void);
void sigpipe_handler(int signum);
void* thread_func(void* arg);

struct threadArg{
  int shell_fd;
  int shell_pid;
};

int main(int argc, char* argv[]){
  static struct option longOptions[] = {
    {"shell", no_argument, &shell, 1}
  };
  while(1){
    int index = 0;
    int c = getopt_long(argc, argv, "", longOptions, &index);
    if(c == -1)
      break;
  }

  tcgetattr(0, &attr_saved);    // saves the current attributes to restore later
  struct termios tattr;
  tcgetattr(0, &tattr);
  tattr.c_lflag &= ~(ICANON|ECHO); // This changes the local mode to non-canonical input mode with no echo
  tattr.c_cc[VTIME] = 0;
  tattr.c_cc[VMIN] = 1;
  tcsetattr(0, TCSAFLUSH, &tattr);
  if(!shell){                   // --shell option used
    char buf[128];
    int size = 0;
    while((size=read(0, buf, 128)) > 0){
      if(size == 0)
	break;
      else if(buf[0] == '\r' || buf[0] == '\n'){
	write(1, "\r\n", 2);
      }
      else if(buf[0] == '\004'){
	atexit(restore_mode);
	exit(0);
      }
      else{
	write(1, buf, size);
      }
    }
    return 0;
  }
  else{                         // --shell option used
    tattr.c_lflag &= ~(ISIG); // If we unset the ISIG bit, we can capture ^C as '\003'
    tcsetattr(0, TCSAFLUSH, &tattr);

    atexit(restore_mode_shell);

    int pipe_from_shell[2];
    int pipe_to_shell[2];
    if(pipe(pipe_from_shell) == -1){
      fprintf(stderr, "pipe error");
      exit(1);
    }
    if(pipe(pipe_to_shell) == -1){
      fprintf(stderr, "pipe error");
      exit(1);
    }
    pid_t pid = fork();
    if(pid == 0){               // Child process, this is the shell in our case
      // We must do redirections to get the shell to read/write to pipes
      close(pipe_from_shell[0]);    // This closes the read end of pipe_from_shell
      dup2(pipe_from_shell[1], 1);  // This redirects stdout to the write end of pipe_from_shell
      close(pipe_to_shell[1]);      // This closes the write end of pipe_to_shell
      dup2(pipe_to_shell[0], 0);    // This redirects stdin to the read end of pipe_to_shell
      execl("/bin/bash", NULL);
    }
    else{                       // Parent process, this is the terminal process
      signal(SIGPIPE, sigpipe_handler);// setup SIGPIPE handler using signal

      close(pipe_to_shell[0]);         // terminal doesn't use the read end of pipe_to_shell
      close(pipe_from_shell[1]);       // terminal doesn't use the write end of pipe_from_shell

      struct threadArg thread_args;
      thread_args.shell_fd = pipe_from_shell[0];
      thread_args.shell_pid = pid;
      pthread_t tid;
      pthread_create(&tid, NULL, &thread_func, (void*)(&thread_args));

      char buf[128];
      int size = 0;
      while((size = read(0, buf, 128)) > 0){
	if(shell_eof){
	  fprintf(stderr, "Received EOF from shell\n");
	  atexit(restore_mode_shell);
	  kill(pid, SIGINT);
	  exit(1);
	}
	if(buf[0] == '\r' || buf[0] == '\n'){
	  write(1, "\r\n", 2);
	  write(pipe_to_shell[1], "\n" ,1);
	}
	else if(buf[0] == '\004'){
	  close(pipe_to_shell[1]);
	  close(pipe_from_shell[0]);
	  kill(pid, SIGHUP);
	  exit(0);
	}
	else if(buf[0] == '\003'){
	  kill(pid, SIGINT);
	}
	else{
	  write(1, buf, size);
	  write(pipe_to_shell[1], buf, size);
	}
      }
    }
  }
}

void sigpipe_handler(int signum){
  fprintf(stderr, "SIGPIPE error\n");
  exit(1);
}

/*
 * This function reads from the output from the shell and outputs it to stdout
 * It takes as input the file descriptor of the shell as type void* since its a thread function
 */
void* thread_func(void* arg){
  struct threadArg* args = (struct threadArg*) arg;
  int pipe_fd = args->shell_fd;
  char buf[128];
  int size = 0;
  while((size=read(pipe_fd, buf, 128)) > 0){
    if(buf[0] == '\004'){
      shell_eof = 1;
      exit(1);
    }
    else{
      write(1, buf, size);
    }
  }
}

void restore_mode(void){
  tcsetattr(0, TCSANOW, &attr_saved);
}

void restore_mode_shell(void){
  tcsetattr(0, TCSANOW, &attr_saved);
  int status;
  waitpid(-1, &status, 0);
  if(WIFEXITED(status)){
    fprintf(stderr, "thread process exited with error code %d\n", WEXITSTATUS(status));
  }
  else{
    fprintf(stderr, "child process exited abnormally\n");
  }
}
