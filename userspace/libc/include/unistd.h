#ifndef _UNISTD_H
#define _UNISTD_H

#include <stdint.h>
#include <sys/types.h>

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

ssize_t read   (int fd, void *buf, size_t count);
ssize_t write  (int fd, const void *buf, size_t count);
int     open   (const char *path, int flags, ...);
int     close  (int fd);
pid_t   getpid (void);
pid_t   fork   (void);
pid_t   waitpid(pid_t pid, int *wstatus, int options);
int     execve (const char *path, char *const argv[], char *const envp[]);
void    _exit  (int status) __attribute__((noreturn));

#endif /* _UNISTD_H */
