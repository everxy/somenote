#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>

pthread_mutex_t mutex;

int main(int argc, char **argv) {

  if (argc != 2)
    return 0;
  int n = atoi(argv[1]);
  int i = 0;
  int status = 0;

  pthread_mutex_init(&mutex, NULL);

  pid_t pid = 1;
  static int *x;
  x = mmap(NULL, sizeof *x, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  *x = 0;

  printf("Creating %d children\n", n);

  for(i = 0; i < n; i++) {
    if (pid != 0)
      pid = fork();
  }

  if (pid == 0) {
    pthread_mutex_lock(&mutex);
    *x = *x + 1;
    printf("[CHLD] PID: %d PPID: %d X: %d\n", getpid(), getppid(), *x);
    pthread_mutex_unlock(&mutex);
    exit(0);
  }

  wait(&status);

  printf("[PRNT] PID: %d X: %d\n", getpid(), *x);
  munmap(x, sizeof *x);

  return 0;
}
