#include "defs.h"



int 
main(int argc, char const *argv[])
{
    //printf("Hello Cmake!\n"); 
    if(boot_fs() < 0){
        printf("file system boot fail!\n");
        goto end;
    }
    printf("file system Init Success!\n");
    boot_shell();
end:
    terminal_fs();
    return 0;
}
