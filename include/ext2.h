#include "types.h"
#include "params.h"

/* 超级块 */
struct super_block {
    int32 magic_num;                  // 幻数
    int32 free_block_count;           // 空闲数据块数
    int32 free_inode_count;           // 空闲inode数
    int32 dir_inode_count;            // 目录inode数
    uint32 block_map[128];            // 数据块占用位图，共128 * 32 = 4K个数据块
    uint32 inode_map[32];             // inode占用位图 32 * 32 = 1024个inode节点，与inode共占1k * 32B = 32KB
    uint32 inode_block_map[32];
};

#define SINGLE 5
#define SIGNLELINK 1

/* 文件inode 共 32B，一个数据块可分配1KB，故可分配1KB/32B=32个inode，共需1K/32 = 32个数据块，顺序分配即可*/
struct inode {
    uint32 size;              // 文件大小 4B
    uint16 file_type;         // 文件类型（文件/文件夹） 2B
    uint16 link;              // 连接数 2B
    uint32 block_point[SINGLE + SIGNLELINK];    // 数据块指针 6 * 4B = 24B
};


/* 目录项 共121 + 4 + 2 + 1 = 128B，一个数据块可分配1KB，故可分配1KB/128B=8个，1个盘块分配4个*/
struct dir_item {               // 目录项一个更常见的叫法是 dirent(directory entry)
    uint32 inode_id;            // 当前目录项表示的文件/目录的对应inode   4B
    uint16 valid;               // 当前目录项是否有效                    2B
    uint8 type;                 // 当前目录项类型（文件/目录）            1B
    char name[121];             // 目录项表示的文件/目录的文件名/目录名    121 * 1B = 121 B  
};

/* 路径栈 */
struct path_stack_t
{
    struct dir_item stack[MAXARGS]; 
    int top;
} path_stack_t;