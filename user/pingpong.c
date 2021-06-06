#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char* argv[])
{
    int p1[2], p2[2];

    pipe(p1);
    pipe(p2);

    if(fork() == 0){
        //p1[0]-->0: read
        close(0);
        dup(p1[0]);
        close(p1[0]);
        close(p1[1]);
        //p2[1]-->1: write
        // close(1);
        // dup(p2[1]);
        // close(p2[1]);
        close(p2[0]);

        char buff[12];
        read(0, buff, 1);
        printf("%d: received ping\n", getpid());
        write(p2[1], " ", 1);
        // close(p2[1]);
        // close(p1[0]);
    }else{
        //p1[1]-->1: write
        // close(1);
        // dup(p1[1]);
        // close(p1[1]);
        close(p1[0]);
        //p2[0]-->0: read
        close(0);
        dup(p2[0]);
        close(p2[0]);
        close(p2[1]);

        char buff[12];
        write(p1[1], " ", 1);
        read(0, buff, 1);
        printf("%d: received pong\n", getpid());
        // close(p1[1]);
        // close(p2[0]);
        // wait(0);
    }

    exit(0);
}