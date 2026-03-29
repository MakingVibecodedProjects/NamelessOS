#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#define SHELL_PATH  "/bin/shell"
#define HTTPD_PATH  "/bin/httpd"

static char *shell_argv[] = { SHELL_PATH, (char *)0 };
static char *shell_envp[] = { (char *)0 };
static char *httpd_argv[] = { HTTPD_PATH, (char *)0 };

int main(void) {
    printf("init: PID %d starting\n", (int)getpid());

    /* Launch httpd as a background daemon — fork and do NOT waitpid */
    pid_t hpid = fork();
    if (hpid == 0) {
        execve(HTTPD_PATH, httpd_argv, shell_envp);
        printf("init: httpd execve failed\n");
        _exit(1);
    }
    if (hpid > 0)
        printf("init: httpd launched (pid %d)\n", (int)hpid);

    /* Shell respawn loop */
    for (;;) {
        pid_t pid = fork();
        if (pid < 0) {
            printf("init: fork failed\n");
            for (volatile int i = 0; i < 1000000; i++);
            continue;
        }

        if (pid == 0) {
            execve(SHELL_PATH, shell_argv, shell_envp);
            printf("init: execve failed\n");
            _exit(1);
        }

        int status = 0;
        waitpid(pid, &status, 0);
        printf("init: shell exited, respawning\n");
    }
}
