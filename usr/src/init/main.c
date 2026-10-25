#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

char *argv[] = { "/bin/sh", 0 };
char *envp[] = { "PATH=/bin", "TZ=JST-9", "TEST_ENV=FROM_INIT", 0 };

int
main()
{
    int pid, wpid;

    if (open("/dev/console", O_RDWR) < 0) {
        mknod("/dev/console", S_IFCHR, 0x0100);
        open("/dev/console", O_RDWR);
    }
    dup(0);                     // stdout
    dup(0);                     // stderr

    while (1) {
        //printf("init: starting sh\n");
        pid = fork();
        if (pid < 0) {
            printf("init: fork failed\n");
            exit(1);
        }
        if (pid == 0) {
            execve("/bin/sh", argv, envp);
            //printf("init: exec sh failed\n");
            exit(1);
        }
        while ((wpid = wait(NULL)) >= 0 && wpid != pid)
            printf("zombie!\n");
    }

    return 0;
}
