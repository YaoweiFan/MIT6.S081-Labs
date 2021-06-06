#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    char argv_new[10][6];
    int num;
    char buff[256];
    int n;
    char *start = buff;


    for(num=1; num<argc; num++){
        strcpy(argv_new[num-1], argv[num]);
    }

    if(fork() == 0){
        n = read(0, buff, sizeof(buff));
        if(n == 0){
            printf("No info can be read!\n");
            exit(0);
        }
        for(int j=0; j<n; j++){
            if(buff[j] == '\n' || j == n-1){
                memcpy(argv_new[num], start, buff+j+1-start);

                if(buff[j] != '\n' && j == n-1){
                    argv_new[num][j+1] = '\n';
                    argv_new[num][j+2] = 0;
                }
                else{
                    argv_new[num][j+1] = 0;
                }

                if(fork() == 0)
                    exec(argv[1], (char**)argv_new);
                else
                    wait(0);

                start = start + j + 1;
            }        
        }
    }
    else{
        wait(0);
    }

    exit(0);
}