#include "kernel/types.h"
#include "user/user.h"

void process(int parent);

int
main(int argc, char* argv[])
{
    int p[2];
    pipe(p);

    if(fork() == 0){
        close(p[1]);
        process(p[0]);
    }
    else{
        close(p[0]);
        for(int i=2; i<=35; i++)
            write(p[1], &i, 4);
        close(p[1]);
        wait(0);
    }

    exit(0);
}

void process(int parent){
    int prime, digit;
    int p[2];
    if(read(parent, &prime, 4)){
        printf("prime %d\n", prime);
    }
    else{
        close(parent);
        exit(0);
    }

    pipe(p);

    if(fork() == 0){
        close(p[1]);
        process(p[0]);
        // exit(0);
    }
    else{
        close(p[0]);
        while(read(parent, &digit, 4)){
            if(digit%prime != 0)
                write(p[1], &digit, 4);
        }
        close(p[1]);
        wait(0);
    }
}