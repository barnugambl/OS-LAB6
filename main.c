#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_PATTERN 255
#define MAX_PATH 2048
#define BUFFER_SIZE 8192

long search_in_file(const char *filename, const unsigned char *pattern, int m) {
    FILE *f = fopen(filename, "rb");
    if (!f) return -1; 

    unsigned char buffer[BUFFER_SIZE];
    long total_read = 0;
    long found_pos = -2; 
    
    while (1) {
        size_t bytes_read = fread(buffer, 1, BUFFER_SIZE, f);
        if (bytes_read == 0) break;
        
        for (size_t i = 0; i <= bytes_read - m; i++) {
            if (memcmp(buffer + i, pattern, m) == 0) {
                found_pos = total_read + i;
                break;
            }
        }
        
        if (found_pos >= 0) break; 
        
        total_read += bytes_read;
        
        if (bytes_read == BUFFER_SIZE && m > 1) {
            fseek(f, -(m - 1), SEEK_CUR);
            total_read -= (m - 1);
        }
    }
    
    fclose(f);
    return found_pos;
}

int main() {
    char dirpath[MAX_PATH];
    char text_pattern[MAX_PATTERN];
    int N;
    int m;

    printf("Enter directory path: ");
    if (fgets(dirpath, sizeof(dirpath), stdin) == NULL) {
        printf("Error reading directory path!\n");
        return 1;
    }
    dirpath[strcspn(dirpath, "\n")] = '\0';
    
    DIR *test_dir = opendir(dirpath);
    if (!test_dir) {
        printf("Error: Directory '%s' does not exist or cannot be accessed!\n", dirpath);
        return 1;
    }
    closedir(test_dir);
    
    printf("Enter text pattern to search: ");
    if (fgets(text_pattern, sizeof(text_pattern), stdin) == NULL) {
        printf("Error reading pattern!\n");
        return 1;
    }
    text_pattern[strcspn(text_pattern, "\n")] = '\0';
    
    m = strlen(text_pattern);
    if (m == 0) {
        printf("Error: Pattern cannot be empty!\n");
        return 1;
    }
    if (m >= MAX_PATTERN) {
        printf("Error: Pattern too long! Max is %d characters\n", MAX_PATTERN - 1);
        return 1;
    }
    
    while (1) {
        long max_proc = sysconf(_SC_CHILD_MAX);
        printf("Enter N (max number of processes, 1-%ld): ", max_proc);
        
        if (scanf("%d", &N) != 1) {
            printf("Error: Please enter a valid number!\n");
            while (getchar() != '\n') {}
            continue;
        }
        getchar(); 
        
        if (N <= 0) {
            printf("Error: N must be positive!\n");
            continue;
        }
        
        if (N > max_proc) {
            printf("Warning: System limit is %ld processes. Using %ld instead.\n", 
                   max_proc, max_proc);
            N = (int)max_proc;
        }
        break;
    }

    DIR *dir = opendir(dirpath);
    if (!dir) {
        printf("Error: Cannot open directory '%s'\n", dirpath);
        return 1;
    }

    unsigned char pattern[MAX_PATTERN];
    for (int i = 0; i < m; i++) {
        pattern[i] = (unsigned char)text_pattern[i];
    }

    struct dirent *entry;
    int active = 0;
    int file_count = 0;
    int found_count = 0;
    
    printf("\n=== Starting search ===\n");
    printf("Directory: %s\n", dirpath);
    printf("Pattern: '%s' (%d bytes)\n", text_pattern, m);
    printf("Max processes: %d\n\n", N);

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) 
            continue;

        char full[MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", dirpath, entry->d_name);

        struct stat st;
        if (stat(full, &st) != 0) {
            printf("Warning: Cannot access file: %s\n", entry->d_name);
            continue;
        }

        if (!S_ISREG(st.st_mode)) {
            printf("Skipping non-regular file: %s\n", entry->d_name);
            continue;
        }

        file_count++;

        while (active >= N) {
            int status;
            pid_t pid = wait(&status);
            active--;
            
            if (pid > 0 && WIFEXITED(status)) {
                if (WEXITSTATUS(status) == 1) {
                    found_count++;
                }
            }
        }

        pid_t pid = fork();
        if (pid == 0) { 
            long pos = search_in_file(full, pattern, m);
            
            if (pos == -1) {
                printf("[PID %6d] File: %-30s | ERROR opening file\n", 
                       getpid(), entry->d_name);
                exit(0);
            } else if (pos >= 0) { 
                printf("[PID %6d] File: %-30s | Found at byte: %8ld | size: %8ld | FOUND\n",
                       getpid(), entry->d_name, pos, (long)st.st_size);
                exit(1);
            } else if (pos == -2) {  
                printf("[PID %6d] File: %-30s | Not found | size: %8ld\n",
                       getpid(), entry->d_name, (long)st.st_size);
                exit(0);
            }
        }
        else if (pid > 0) { 
            active++;
            printf("Started process %6d for: %s (active: %d/%d)\n",
                   pid, entry->d_name, active, N);
        }
        else {
            printf("Error: Cannot create process for file: %s\n", entry->d_name);
            continue;
        }
    }

    closedir(dir);

    while (active > 0) {
        int status;
        pid_t pid = wait(&status);
        active--;
        
        if (pid > 0 && WIFEXITED(status)) {
            if (WEXITSTATUS(status) == 1) {
                found_count++;
            }
        }
    }

    printf("\n=== Search completed ===\n");
    printf("Total files processed: %d\n", file_count);
    printf("Files with pattern found: %d\n", found_count);
    
    return 0;
}
