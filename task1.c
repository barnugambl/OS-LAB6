#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

void print_process_info(const char* process_name) {
    struct timespec ts;
    struct tm *time_info;
    char time_buffer[50];
    
    clock_gettime(CLOCK_REALTIME, &ts);
    time_info = localtime(&ts.tv_sec);
    strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S", time_info);
    
    printf("%s: PID = %d, PPID = %d, Время = %s:%03ld\n",
           process_name, getpid(), getppid(), time_buffer, ts.tv_nsec / 1000000);
}

int main() {
    pid_t pid1, pid2;
    
    pid1 = fork();
    
    if (pid1 == -1) {
        perror("Ошибка при первом вызове fork()");
        exit(EXIT_FAILURE);
    }
    
    if (pid1 == 0) {
        print_process_info("Первый дочерний процесс");
        exit(EXIT_SUCCESS);
    }
    
    pid2 = fork();
    
    if (pid2 == -1) {
        perror("Ошибка при втором вызове fork()");
        exit(EXIT_FAILURE);
    }
    
    if (pid2 == 0) {
        print_process_info("Второй дочерний процесс");
        exit(EXIT_SUCCESS);
    }
    
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
    
    print_process_info("Родительский процесс");
    
    system("ps -x");
    
    return 0;
}
