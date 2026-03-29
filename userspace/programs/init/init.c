#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

/* Path to the shell binary — loaded by the kernel from the disk image */
#define SHELL_PATH  "/bin/shell"

static char *shell_argv[] = { SHELL_PATH, (char *)0 };
static char *shell_envp[] = { (char *)0 };

int main(void) {
    printf("init: PID %d starting\n", (int)getpid());

    for (;;) {
        pid_t pid = fork();
        if (pid < 0) {
            printf("init: fork failed\n");
            /* back off and retry */
            for (volatile int i = 0; i < 1000000; i++);
            continue;
        }

        if (pid == 0) {
            /* child: exec the shell */
            execve(SHELL_PATH, shell_argv, shell_envp);
            /* execve only returns on failure */
            printf("init: execve failed\n");
            _exit(1);
        }

        /* parent: wait for shell to exit, then respawn */
        int status = 0;
        waitpid(pid, &status, 0);
        printf("init: shell exited, respawning\n");
    }
}
