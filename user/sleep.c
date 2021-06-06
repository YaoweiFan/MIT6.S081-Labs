#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char* argv[])
{
    if(argc == 1)
        write(1,"Error!\n",7);
    else{
        sleep(atoi(argv[1]));
    }
    exit(0);
}