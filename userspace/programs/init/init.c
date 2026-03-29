#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#define SHELL_PATH  "/bin/shell"

static char *shell_argv[] = { SHELL_PATH, (char *)0 };
static char *shell_envp[] = { (char *)0 };

int main(void) {
    printf("init: PID %d starting\n", (int)getpid());

    /* Shell respawn loop — httpd is launched directly by the kernel */
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
