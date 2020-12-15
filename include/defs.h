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

/* 开启debug模式 */
//#define DEBUG

/* shell.c */
void                        boot_shell();

/* utils.c */
int                         gets(char *buf, int max);
int                         convertAa(char *str);
void                        panic(char *str);
char*                       peek_path(char** path); //去掉path的第一个'/'，并返回token
char*                       join(char *s1, char *s2);   
char*                       get_file_name(char *path);

/* raise.c */
void                        raise_path_not_exist(char *hints, char *path);
void                        raise_path_exist(char *hints, char *path);
void                        raise_path_type_error(char *hints, char *path, int file_type);
void                        raise_common_error(char *hints, char *info);

/* printer.c */
void                        print_dir_item(char *hints, struct dir_item* dir_item);
void                        print_dir_info(int size);
void                        print_super_blk(struct super_block* su_blk);
void                        print_command_info(char *hints, char *info);

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
void                        observe_fs_structure(char *path, int level, int show_detail);
void                        remove_path(char *path);
void                        move_to(char *from_path, char *to_path);
char*                       get_current_path();
