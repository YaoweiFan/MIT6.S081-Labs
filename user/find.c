#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h"
#include "kernel/fs.h"

void find(char *path, char *file){
    int fd;
    char buff[DIRSIZ+1];
    char buf[512];
    struct dirent de;
    struct stat st;
    char *p;

    if((fd = open(path, 0)) < 0){
        fprintf(2, "find: can not open %s\n", path);
        return;
    }
    
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';

    while(read(fd, &de, sizeof(de)) == sizeof(de)){
        if(de.inum == 0)
            continue;
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;

        if(stat(buf, &st) < 0){
            fprintf(2, "find: can not stat %s\n", buf);
            continue;
        }

        memmove(buff, de.name, DIRSIZ);
        buff[DIRSIZ] = 0;

        switch (st.type)
        {
        case T_FILE:
            if(strcmp(buff, file) == 0)
                printf("%s\n", buf);
            break;
        case T_DIR:
            if(strcmp(buff, ".") == 0 || strcmp(buff, "..") == 0)
                continue;
            find(buf, file);
            break;
        }
    }
    close(fd);
}

int
main(int argc, char *argv[])
{
    char *path = argv[1];
    char *file = argv[2];
    find(path, file);
    exit(0);
}