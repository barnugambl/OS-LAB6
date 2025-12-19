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

long search_in_file(const char *filename, const unsigned char *pattern, int m) {
    FILE *f = fopen(filename, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) { fclose(f); return 0; }

    unsigned char *buf = malloc(size);
    if (!buf) { fclose(f); return -1; }

    fread(buf, 1, size, f);
    fclose(f);

    long bytes_viewed = 0;
    long found_position = -1;

    for (long i = 0; i <= size - m; i++) {
        bytes_viewed++;
        if (memcmp(buf + i, pattern, m) == 0) {
            found_position = bytes_viewed;
            break;
        }
    }

    free(buf);

    return (found_position != -1) ? found_position : size;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <directory> <pattern> <N>\n", argv[0]);
        printf("Example: %s /home/user \"hello\" 5\n", argv[0]);
        return 1;
    }
    
    char *dirpath = argv[1];
    char *text_pattern = argv[2];
    int N = atoi(argv[3]);  

    int m;
    DIR *dir;
    struct dirent *entry;
    int active = 0;
    int file_count = 0;
    int found_count = 0;

    DIR *dir = opendir(dirpath);
    if (!dir) {
        printf("Error: Directory '%s' does not exist or cannot be accessed!\n", dirpath);
        return 1;
    }
    closedir(dir);

    printf("Enter text pattern to search: ");
    fgets(text_pattern, sizeof(text_pattern), stdin);
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
            printf("Warning: System limit is %ld processes. Using %ld instead.\n", max_proc, max_proc);
            N = (int)max_proc;
        }
        break;
    }

    dir = opendir(dirpath);
    if (!dir) {
        perror("opendir");
        return 1;
    }

    unsigned char pattern[MAX_PATTERN];
    for (int i = 0; i < m; i++) {
        pattern[i] = (unsigned char)text_pattern[i];
    }

    while ((entry = readdir(dir)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;

        char full[MAX_PATH];
        int needed = snprintf(full, sizeof(full), "%s/%s", dirpath, entry->d_name);
        if (needed >= (int)sizeof(full)) {
            printf("Warning: Path too long for file: %s\n", entry->d_name);
            continue;
        }

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
            wait(NULL);
            active--;
        }

        pid_t pid = fork();
        if (pid == 0) {
            long bytes = search_in_file(full, pattern, m);
            int found = (bytes >= 0 && bytes < st.st_size);

            if (found) found_count++;

            printf("[PID %6d] File: %-30s | bytes: %8ld | %s\n",
                   getpid(),
                   entry->d_name,
                   bytes,
                   found ? "FOUND" : "not found");

            exit(found ? 1 : 0);
        }
        if (pid > 0) {
            active++;
            printf("Started process %6d for: %s (active: %d/%d)\n",
                   pid, entry->d_name, active, N);
        } else {
            perror("fork");
            printf("Error: Cannot create process for file: %s\n", entry->d_name);
        }
    }

    closedir(dir);

    while (active > 0) {
        int status;
        pid_t pid = wait(&status);
        active--;

        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) == 1) {
                found_count++;
            }
        }
    }
    return 0;
}
