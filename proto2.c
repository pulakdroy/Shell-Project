#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_CMD_LEN 1000
#define MAX_ARGS 100
#define HISTORY_SIZE 100

char *history[HISTORY_SIZE];
int history_count = 0;

void add_to_history(const char *cmd) {
    if (history_count < HISTORY_SIZE) {
        history[history_count++] = strdup(cmd);
    }
}

void sigint_handler(int sig) {
    printf("\nCaught signal %d. Use 'exit' to quit.\nsh> ", sig);
    fflush(stdout);
}

void redirect_io(char **args) {
    for (int i = 0; args[i]; i++) {
        if (strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0) {
            int fd;
            if (args[i+1] == NULL) return;
            fd = (strcmp(args[i], ">") == 0)
                ? open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644)
                : open(args[i+1], O_WRONLY | O_CREAT | O_APPEND, 0644);
            dup2(fd, STDOUT_FILENO);
            close(fd);
            args[i] = NULL;
        } else if (strcmp(args[i], "<") == 0) {
            if (args[i+1] == NULL) return;
            int fd = open(args[i+1], O_RDONLY);
            dup2(fd, STDIN_FILENO);
            close(fd);
            args[i] = NULL;
        }
    }
}

int parse_execute(char *cmd) {
    char *args[MAX_ARGS];
    int i = 0;
    args[i] = strtok(cmd, " ");
    while (args[i] != NULL) args[++i] = strtok(NULL, " ");
    if (args[0] == NULL) return 0;
    if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL) fprintf(stderr, "cd: missing argument\n");
        else if (chdir(args[1]) != 0) perror("cd");
        return 0;
    }
    if (strcmp(args[0], "exit") == 0) {
        int code = args[1] ? atoi(args[1]) : 0;
        exit(code);
    }
    if (strcmp(args[0], "history") == 0) {
        for (int j = 0; j < history_count; j++) {
            printf("%d: %s\n", j + 1, history[j]);
        }
        return 0;
    }
    pid_t pid = fork();
    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        redirect_io(args);
        execvp(args[0], args);
        perror("exec");
        exit(1);
    } else {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
}

void handle_pipes(char *cmd) {
    char *commands[10];
    int i = 0;
    commands[i] = strtok(cmd, "|");
    while (commands[i] != NULL) commands[++i] = strtok(NULL, "|");
    int in_fd = 0, fd[2];
    for (int j = 0; j < i; j++) {
        pipe(fd);
        if (fork() == 0) {
            dup2(in_fd, STDIN_FILENO);
            if (j < i - 1) dup2(fd[1], STDOUT_FILENO);
            close(fd[0]);
            parse_execute(commands[j]);
            exit(0);
        } else {
            wait(NULL);
            close(fd[1]);
            in_fd = fd[0];
        }
    }
}

int execute(char *cmd) {
    if (strchr(cmd, '|')) {
        handle_pipes(cmd);
        return 0;
    }
    return parse_execute(cmd);
}

void process_input(const char *input) {
    char buffer[MAX_CMD_LEN];
    strncpy(buffer, input, MAX_CMD_LEN);
    buffer[MAX_CMD_LEN - 1] = '\0';
    char *saveptr1;
    char *sequence = strtok_r(buffer, ";", &saveptr1);
    while (sequence != NULL) {
        char sequence_copy[MAX_CMD_LEN];
        strncpy(sequence_copy, sequence, MAX_CMD_LEN);
        sequence_copy[MAX_CMD_LEN - 1] = '\0';

        char *saveptr2;
        char *cmd = strtok_r(sequence_copy, "&&", &saveptr2);
        int last_status = 0;

        while (cmd != NULL) {
            while (*cmd == ' ') cmd++;
            last_status = execute(cmd);
            if (last_status != 0) break;
            cmd = strtok_r(NULL, "&&", &saveptr2);
        }
        sequence = strtok_r(NULL, ";", &saveptr1);
    }
}

int main() {
    char command[MAX_CMD_LEN];
    signal(SIGINT, sigint_handler);
    while (1) {
        printf("sh> ");
        if (!fgets(command, MAX_CMD_LEN, stdin)) break;
        command[strcspn(command, "\n")] = 0;
        if (strlen(command) == 0) continue;
        if (strcmp(command, "exit") == 0) break;
        add_to_history(command);
        process_input(command);
    }
    return 0;
}
