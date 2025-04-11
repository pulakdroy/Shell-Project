#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_ARGS 100
#define MAX_HISTORY 100

char *history[MAX_HISTORY];
int history_count = 0;

void sigint_handler(int signo) {
    write(STDOUT_FILENO, "\nsh> ", 5);
}

void parse_input(char *input, char **args) {
    int i = 0;
    args[i] = strtok(input, " \t\n");
    while (args[i] != NULL) {
        i++;
        args[i] = strtok(NULL, " \t\n");
    }
    args[i] = NULL;
}

void execute_command(char **args) {
    if (strcmp(args[0], "history") == 0) {
        for (int i = 0; i < history_count; i++) {
            printf("%d: %s", i + 1, history[i]);
        }
        return;
    }

    pid_t pid = fork();

    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        int in = -1, out = -1;
        for (int i = 0; args[i]; i++) {
            if (strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0) {
                out = open(args[i+1], strcmp(args[i], ">>") == 0 ? O_WRONLY|O_CREAT|O_APPEND : O_WRONLY|O_CREAT|O_TRUNC, 0644);
                dup2(out, STDOUT_FILENO);
                args[i] = NULL;
                break;
            } else if (strcmp(args[i], "<") == 0) {
                in = open(args[i+1], O_RDONLY);
                dup2(in, STDIN_FILENO);
                args[i] = NULL;
                break;
            }
        }
        execvp(args[0], args);
        perror("exec failed");
        exit(1);
    } else {
        wait(NULL);
    }
}

void execute_pipeline(char *input) {
    char *commands[10];
    int num_cmds = 0;

    char *token = strtok(input, "|");
    while (token != NULL && num_cmds < 10) {
        commands[num_cmds++] = strdup(token);
        token = strtok(NULL, "|");
    }

    int prev_fd = -1;
    for (int i = 0; i < num_cmds; i++) {
        int pipefd[2];
        if (i < num_cmds - 1)
            pipe(pipefd);

        pid_t pid = fork();
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            if (prev_fd != -1) {
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }
            if (i < num_cmds - 1) {
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }
            char *args[MAX_ARGS];
            parse_input(commands[i], args);
            execvp(args[0], args);
            perror("exec failed");
            exit(1);
        }

        if (prev_fd != -1) close(prev_fd);
        if (i < num_cmds - 1) {
            close(pipefd[1]);
            prev_fd = pipefd[0];
        }

        wait(NULL);
    }

    for (int i = 0; i < num_cmds; i++) {
        free(commands[i]);
    }
}

void process_input_line(char *input) {
    char *tokens[20];
    int token_count = 0;

    char *op = NULL;
    if (strstr(input, "&&")) op = "&&";
    else if (strstr(input, ";")) op = ";";

    if (!op) {
        if (strchr(input, '|')) {
            execute_pipeline(input);
        } else {
            char *args[MAX_ARGS];
            parse_input(input, args);
            if (args[0] != NULL) {
                execute_command(args);
            }
        }
        return;
    }

    char *token = strtok(input, op);
    while (token && token_count < 20) {
        tokens[token_count++] = strdup(token);
        token = strtok(NULL, op);
    }

    for (int i = 0; i < token_count; i++) {
        while (*tokens[i] == ' ') tokens[i]++;
        char *newline = strchr(tokens[i], '\n');
        if (newline) *newline = '\0';

        int status;
        pid_t pid = fork();

        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            if (strchr(tokens[i], '|')) {
                execute_pipeline(tokens[i]);
            } else {
                char *args[MAX_ARGS];
                parse_input(tokens[i], args);
                execvp(args[0], args);
            }
            perror("exec failed");
            exit(1);
        } else {
            wait(&status);
            if (strcmp(op, "&&") == 0 && status != 0)
                break;
        }
        free(tokens[i]);
    }
}

int main() {
    signal(SIGINT, sigint_handler);

    char input[1024];
    while (1) {
        printf("sh> ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("\n");
            break;
        }

        if (strlen(input) > 1) {
            if (history_count < MAX_HISTORY)
                history[history_count++] = strdup(input);
        }

        process_input_line(input);
    }
    return 0;
}
