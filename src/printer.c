#include "defs.h"
#include "filetype.h"


void 
print_dir_item(char *hints, struct dir_item* dir_item){
    printf("%s %s\n", dir_item->name, dir_item->type == FILE ? "\e[35;1mFILE\e[0m" : "\e[36;1mDIR\e[0m");
}

void
print_dir_info(int size){
    printf("\n\e[36;1mtotal: %d items\e[30;1m \n",size);
}

void                        
print_super_blk(struct super_block* su_blk){
    
}

void
print_command_info(char *hints, char *info){
    printf("\e[31m%s\e[0m \e[32;1m%s\e[0;1m \n", hints, info);
}