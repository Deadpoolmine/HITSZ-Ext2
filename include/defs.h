#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "disk.h"
#include "ext2.h"
/* 
struct inode;
struct super_block;
struct dir_item;
struct inode_block_map; */


/* shell.c */
void                        boot_shell();

/* utils.c */
int                         gets(char *buf, int max);
int                         convertAa(char *str);
void                        panic(char *str);
char*                       peek_path(char** path); //去掉path的第一个'/'，并返回token
char*                       join(char *s1, char *s2);   
char*                       get_file_name(char *path);

/* ext2.c */
int                         boot_fs();
void                        terminal_fs();
struct inode*               create_file(char *path);
struct inode*               create_dir(char *path);
void                        check_dir(char *path);
void                        swith_current_dir(char *path);
int                         copy_to(char *from_path, char *to_path);
char*                       read_file(char *path);
void                        write_file(char *path);
void                        observe_fs_structure(char *path, int level);
char*                       get_current_path();
