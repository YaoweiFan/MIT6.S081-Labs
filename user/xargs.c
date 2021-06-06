#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    char *argv_new[MAXARG];
    int num;
    char buff[256];
    char *start = buff;

    for(num=1; num<argc; num++){
        argv_new[num-1] = argv[num];
    }
    // printf("argc = %d, num = %d\n", argc, num);
    if(fork() == 0){
        read(0, buff, sizeof(buff));
        // printf("*******************************\n");
        // printf("%s", buff);
        // printf("*******************************\n");
        for(int j=0; j<strlen(buff); j++){
            if(buff[j] == '\n'){
                char buf[12];
                memcpy(buf, start, buff+j-start);
                buf[buff+j-start] = 0;
                argv_new[num-1] = buf;
                argv_new[num] = 0;
                // printf("***************\n");
                // printf("%s", argv_new[num-1]);
                // printf("***************\n");

                if(fork() == 0){
                    // printf("%s, %s, %s, %s\n", argv_new[0], argv_new[1], argv_new[2], argv_new[3]);
                    exec(argv_new[0], (char**)argv_new);
                    // exec 要返回的
                    exit(0);
                }
                else
                    wait(0);

                start = buff + j + 1;
            }        
        }
    }
    else{
        wait(0);
    }

    exit(0);
}