#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_CMD_LEN 1024
#define MAX_ARGS 100
#define HISTORY_SIZE 100

char *history[HISTORY_SIZE];
int history_count = 0;

void add_to_history(char *cmd) {
    if (history_count < HISTORY_SIZE) {
        history[history_count++] = strdup(cmd);
    }
}

void sigint_handler(int sig) {
    printf("\nCaught signal %d. Use 'exit' to quit.\nsh> ", sig);
    fflush(stdout);
}

void execute(char *cmd);
void handle_pipes(char *cmd);
void parse_execute(char *cmd);

void redirect_io(char **args) {
    for (int i = 0; args[i]; i++) {
        if (strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0) {
            int fd;
            if (args[i+1] == NULL) return;
            if (strcmp(args[i], ">") == 0)
                fd = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            else
                fd = open(args[i+1], O_WRONLY | O_CREAT | O_APPEND, 0644);
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

void parse_execute(char *cmd) {
    char *args[MAX_ARGS];
    int i = 0;
    args[i] = strtok(cmd, " ");
    while (args[i] != NULL) args[++i] = strtok(NULL, " ");

    if (args[0] == NULL) return;

    // Handle built-in: cd
    if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL) {
            fprintf(stderr, "cd: missing argument\n");
        } else {
            if (chdir(args[1]) != 0) {
                perror("cd");
            }
        }
        return;
    }

    // Handle built-in: exit
    if (strcmp(args[0], "exit") == 0) {
        int code = args[1] ? atoi(args[1]) : 0;
        exit(code);
    }

    // Handle built-in: history
    if (strcmp(args[0], "history") == 0) {
        for (int j = 0; j < history_count; j++) {
            printf("%d: %s\n", j + 1, history[j]);
        }
        return;
    }

    // External commands
    pid_t pid = fork();
    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        redirect_io(args);
        execvp(args[0], args);
        perror("exec");
        exit(1);
    } else {
        wait(NULL);
    }
}


void handle_pipes(char *cmd) {
    char *commands[10];
    int i = 0;
    commands[i] = strtok(cmd, "|");
    while (commands[i] != NULL) commands[++i] = strtok(NULL, "|");
    int num_cmds = i;

    int in_fd = 0;
    int fd[2];
    for (int j = 0; j < num_cmds; j++) {
        pipe(fd);
        if (fork() == 0) {
            dup2(in_fd, 0);
            if (j < num_cmds - 1) dup2(fd[1], 1);
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

void execute(char *cmd) {
    if (strchr(cmd, '|')) {
        handle_pipes(cmd);
        return;
    }
    parse_execute(cmd);
}

int main() {
    char command[MAX_CMD_LEN];
    signal(SIGINT, sigint_handler);

    while (1) {
        printf("sh> ");
        if (!fgets(command, MAX_CMD_LEN, stdin)) break;
        command[strcspn(command, "\n")] = 0;

        if (strcmp(command, "exit") == 0) break;
        if (strlen(command) == 0) continue;

        add_to_history(command);

        char *sequence = strtok(command, ";");
        while (sequence != NULL) {
            char *logic = strstr(sequence, "&&");
            if (logic) {
                *logic = '\0';
                logic += 2;
                if (system(sequence) == 0) execute(logic);
            } else {
                execute(sequence);
            }
            sequence = strtok(NULL, ";");
        }
    }
    return 0;
}


