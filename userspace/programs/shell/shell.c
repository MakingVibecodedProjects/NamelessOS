#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#define LINE_MAX    256
#define ARG_MAX     16
#define PATH_MAX    128

/* ── readline ────────────────────────────────────────────────────── */
static int readline(char *buf, int max) {
    int i = 0;
    char c;
    while (i < max - 1) {
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) return -1;
        if (c == '\n') break;
        if (c == '\r') continue;
        buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}

/* ── parse — split line into argv, return argc ───────────────────── */
static int parse(char *line, char *argv[], int max_args) {
    int argc = 0;
    char *p = line;
    while (*p && argc < max_args - 1) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
    argv[argc] = (char *)0;
    return argc;
}

/* ── built-in: cd ────────────────────────────────────────────────── */
/* No chdir syscall yet — stub that prints a message */
static void builtin_cd(char *argv[]) {
    if (!argv[1]) { printf("cd: missing argument\n"); return; }
    /* chdir is Phase 7 — print placeholder */
    printf("cd: %s (not yet implemented)\n", argv[1]);
}

/* ── built-in: ls ────────────────────────────────────────────────── */
/* VFS readdir not wired to syscalls yet — stub */
static void builtin_ls(char *argv[]) {
    (void)argv;
    printf("ls: (not yet implemented)\n");
}

/* ── built-in: cat ───────────────────────────────────────────────── */
/* open/read not fully wired yet — stub */
static void builtin_cat(char *argv[]) {
    if (!argv[1]) { printf("cat: missing argument\n"); return; }
    printf("cat: %s (not yet implemented)\n", argv[1]);
}

/* ── run external program ────────────────────────────────────────── */
static void run(char *argv[]) {
    pid_t pid = fork();
    if (pid < 0) { printf("shell: fork failed\n"); return; }
    if (pid == 0) {
        execve(argv[0], argv, (char *[]){ (char *)0 });
        printf("shell: exec failed: %s\n", argv[0]);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
}

/* ── main ────────────────────────────────────────────────────────── */
int main(void) {
    char line[LINE_MAX];
    char *argv[ARG_MAX];

    printf("NamelessOS shell - type 'exit' to quit\n");

    for (;;) {
        write(STDOUT_FILENO, "$ ", 2);

        if (readline(line, sizeof(line)) < 0) break;
        if (line[0] == '\0') continue;

        int argc = parse(line, argv, ARG_MAX);
        if (argc == 0) continue;

        if (strcmp(argv[0], "exit") == 0) break;
        else if (strcmp(argv[0], "cd")  == 0) builtin_cd(argv);
        else if (strcmp(argv[0], "ls")  == 0) builtin_ls(argv);
        else if (strcmp(argv[0], "cat") == 0) builtin_cat(argv);
        else run(argv);
    }

    return 0;
}
