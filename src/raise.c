#include "defs.h"
#include "filetype.h"

void
raise_path_not_exist(char *hints, char *path){
    printf("\e[31m%s\e[0m: \e[34mpath %s does not exist\e[0m\n",hints, path);
}

void
raise_path_exist(char *hints, char *path){
    printf("\e[31m%s\e[0m: \e[34mpath %s exists\e[0m\n",hints, path);
}

void
raise_path_type_error(char *hints, char *path, int file_type){
    printf("\e[35m%s\e[0m: \e[34mpath %s is not a \e[0m",hints, path);
    if(file_type == DIR)
        printf(" \e[34mdirectory!\e[0m\n");
    else
        printf(" \e[34mfile!\e[0m\n");
}

void
raise_common_error(char *hints, char *info){
    printf("\e[31m%s: %s\e[0m\n",hints, info);
}