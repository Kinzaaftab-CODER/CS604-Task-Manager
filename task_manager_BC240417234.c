
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

struct TaskResult {
    long long total_time;
    int longest_task;
    int maximum_time;
};

int write_all(int fd, const void *buffer, size_t size) {
    const char *p = buffer;

    while (size > 0) {
        ssize_t n = write(fd, p, size);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }

        p += n;
        size -= (size_t)n;
    }

    return 0;
}

int read_all(int fd, void *buffer, size_t size) {
    char *p = buffer;

    while (size > 0) {
        ssize_t n = read(fd, p, size);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }

        if (n == 0)
            return -1;

        p += n;
        size -= (size_t)n;
    }

    return 0;
}

int read_positive_int(const char *message) {
    char input[100];
    char *end;
    long value;

    while (1) {
        printf("%s", message);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("Input error.\n");
            exit(EXIT_FAILURE);
        }

        errno = 0;
        value = strtol(input, &end, 10);

        while (*end == ' ' || *end == '\t' ||
               *end == '\n' || *end == '\r')
            end++;

        if (errno == 0 && end != input &&
            *end == '\0' && value > 0 && value <= INT_MAX) {
            return (int)value;
        }

        printf("Please enter a valid positive integer.\n");
    }
}

int main(void) {
    char student_id[100];
    char filename[120];
    char report[1000];

    int task_times[3];
    int pipe1[2], pipe2[2];
    int status;
    pid_t pid;

    struct TaskResult result;

    printf("Enter Student ID: ");

    if (fgets(student_id, sizeof(student_id), stdin) == NULL) {
        printf("Invalid Student ID.\n");
        return 1;
    }

    student_id[strcspn(student_id, "\r\n")] = '\0';

    if (strlen(student_id) == 0 ||
        strpbrk(student_id, "/\\ \t") != NULL) {
        printf("Invalid Student ID.\n");
        return 1;
    }

    for (int i = 0; i < 3; i++) {
        char prompt[60];

        snprintf(prompt, sizeof(prompt),
                 "Enter Task %d Execution Time: ", i + 1);

        task_times[i] = read_positive_int(prompt);
    }

    if (pipe(pipe1) == -1 || pipe(pipe2) == -1) {
        perror("Pipe creation failed");
        return 1;
    }

    pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        return 1;
    }

    if (pid == 0) {
        int received_times[3];
        struct TaskResult child_result;

        close(pipe1[1]);
        close(pipe2[0]);

        if (read_all(pipe1[0], received_times,
                     sizeof(received_times)) == -1) {
            perror("Child read failed");
            _exit(1);
        }

        close(pipe1[0]);

        child_result.total_time =
            (long long)received_times[0] +
            received_times[1] + received_times[2];

        child_result.longest_task = 1;
        child_result.maximum_time = received_times[0];

        for (int i = 1; i < 3; i++) {
            if (received_times[i] >
                child_result.maximum_time) {
                child_result.maximum_time = received_times[i];
                child_result.longest_task = i + 1;
            }
        }

        if (write_all(pipe2[1], &child_result,
                      sizeof(child_result)) == -1) {
            perror("Child write failed");
            _exit(1);
        }

        close(pipe2[1]);
        _exit(0);
    }

    close(pipe1[0]);
    close(pipe2[1]);

    if (write_all(pipe1[1], task_times,
                  sizeof(task_times)) == -1) {
        perror("Parent write failed");
        return 1;
    }

    close(pipe1[1]);

    if (read_all(pipe2[0], &result,
                 sizeof(result)) == -1) {
        perror("Parent read failed");
        return 1;
    }

    close(pipe2[0]);

    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid failed");
        return 1;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        printf("Child process failed.\n");
        return 1;
    }

    snprintf(report, sizeof(report),
        "--- Task Processing Report ---\n"
        "Student ID: %s\n"
        "Task 1: %d units\n"
        "Task 2: %d units\n"
        "Task 3: %d units\n"
        "Total Execution Time: %lld units\n"
        "Longest Task: Task %d\n"
        "Maximum Execution Time: %d units\n",
        student_id,
        task_times[0], task_times[1], task_times[2],
        result.total_time,
        result.longest_task,
        result.maximum_time);

    printf("\n%s", report);

    snprintf(filename, sizeof(filename),
             "%s_report.txt", student_id);

    int file_fd = open(filename,
                       O_WRONLY | O_CREAT | O_TRUNC,
                       0644);

    if (file_fd == -1) {
        perror("Report file creation failed");
        return 1;
    }

    if (write_all(file_fd, report, strlen(report)) == -1) {
        perror("Report writing failed");
        close(file_fd);
        return 1;
    }

    if (close(file_fd) == -1) {
        perror("File closing failed");
        return 1;
    }

    printf("Report saved to: %s\n", filename);

    return 0;
}
