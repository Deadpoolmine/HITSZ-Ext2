#include "defs.h"
#include "params.h"
#include "filetype.h"

struct super_block *su_blk;
struct inode *root_dir_node;
struct dir_item *root_dir_item;
/********************** Constructor *************************/
struct inode*
new_inode(uint16 file_type){
    struct inode* inode = (struct inode *)malloc(INODESZ);
    inode->file_type = file_type;
    inode->size = 0;
    inode->link = 1;
    for (int i = 0; i < SINGLE + SIGNLELINK; i++)
    {
        inode->block_point[i] = INVALID_INODE_POINT; 
    }
    return inode;
}

struct dir_item*
new_dir_item(uint8 type, uint32 node_id, char *name){
    struct dir_item* dir_item = (struct dir_item*)malloc(DIRITEMSZ);
    dir_item->inode_id = node_id;
    dir_item->type = type;
    dir_item->valid = 1;
    memmove(dir_item->name, name, strlen(name));
    return dir_item;
}
/********************** Path *************************/
struct path_stack_t path_stack;

void
init_stack(){
    path_stack.top = -1;
}

int
is_empty(){
    return path_stack.top == -1 ? 1 : 0;
}

char*
get_current_path(){
    char *s = "";
    for (int i = 0; i <= path_stack.top; i++)
    {
        s = join(s, path_stack.stack[i].name);
        if(i != path_stack.top && i != 0)
            s = join(s, "/");
    }
    return s;
}

struct dir_item*
top(int n){
    if(is_empty()){
        return (struct dir_item*)0;
    }
    
    return path_stack.top - n >= 0 ?
            &path_stack.stack[path_stack.top - n] : &path_stack.stack[0];
}

struct dir_item*
pop(){
    if(is_empty()){
        return (struct dir_item*)0;
    }
    struct dir_item* item = &path_stack.stack[path_stack.top];
    path_stack.top--;
    return item;
}

int
push(struct dir_item* dir_item){
    path_stack.top++;
    if(path_stack.top > MAXARGS - 1){
        printf("push path() path_stack is too long");
        return -1;     
    }
    path_stack.stack[path_stack.top].inode_id = dir_item->inode_id;
    memmove(path_stack.stack[path_stack.top].name, dir_item->name, 121);
    path_stack.stack[path_stack.top].type = dir_item->type;
    path_stack.stack[path_stack.top].valid = dir_item->valid;
    return 0;
}

struct dir_item*
get_current_dir_item(){
    return top(0);
}

struct dir_item*
get_last_dir_item(){
    return top(1);
}
/********************** Utils *************************/
void
sync_super_blk(){
    disk_write_block(0, (char *)su_blk);
    disk_write_block(1, (char *)su_blk + DEVICE_BLOCK_SIZE);
}

uint32
get_free_block(){
    for (uint32 i = 0; i < 128; i++)
    {
        int hasfreeblk = 0;
        for (uint32 j = 0; j < 32; j++)
        {
            if(((su_blk->block_map[i] >> j) & 1) == 0){
                printf("find one block: %d\n", i*32 + j);
                return i * 32 + j;
            }
        }
        
    }   
}

uint32
get_free_inode(){
    for (uint32 i = 0; i < 32; i++)
    {
        int hasfreeblk = 0;
        for (uint32 j = 0; j < 32; j++)
        {
            if(((su_blk->inode_map[i] >> j) & 1) == 0){
                printf("find one inode: %d\n", i*32 + j);
                return i * 32 + j;
            }
        }   
    }
}



/* 分配一个inode */
void
alloc_inode(uint32 inode_num){
    int index = inode_num / 32 ;
    int32 off = inode_num % 32;
    uint32 mask = 1 << off;
    su_blk->inode_map[index] |= mask;
    su_blk->free_inode_count--;
    sync_super_blk();
}

/* 释放一个inode */
void
release_inode(uint32 inode_num){
    int index = inode_num / 32 ;
    int32 off = inode_num % 32;
    uint32 mask = 1 << off;
    su_blk->inode_map[index] &= mask;
    su_blk->free_inode_count++;
    sync_super_blk();
}

/* 分配一个数据块 */
void 
alloc_block(uint32 block_num){
    int index = block_num / 32;
    int32 off = block_num % 32;
    uint32 mask = 1 << off;
    su_blk->block_map[index] |= mask;
    su_blk->free_block_count--;
    sync_super_blk();
}

/* 释放一个数据块 */
void
rlease_block(uint32 block_num){
    int index = block_num / 32;
    int32 off = block_num % 32;
    uint32 mask = 1 << off;
    su_blk->block_map[index] &= ~mask;
    su_blk->free_block_count++;
    sync_super_blk();
}

/* 数据块相关 */
/**
 * 废弃的方法
 * */
void
write_blocks(char *buf, int size){
    /* 所需要的盘块 */
    int num_of_block = size / DEVICE_BLOCK_SIZE + 1;
    /* 一次分配一个data block数据块 */
    for (int i = 0; i < num_of_block; i += 2)
    {
        uint32 free_block_num = get_free_block();
        alloc_block(free_block_num);
        disk_write_block(free_block_num * 2, buf);
        buf += DEVICE_BLOCK_SIZE;
        disk_write_block(free_block_num * 2 + 1, buf);
        buf += DEVICE_BLOCK_SIZE;
    }
}

char*
read_block(uint32 block_num){
    char *buf = (char *)malloc(DATABLKSZ);
    memset(buf, 0, DATABLKSZ);
    disk_read_block(block_num * 2, buf);
    disk_read_block(block_num * 2 + 1, buf + DEVICE_BLOCK_SIZE);
    return buf;
}

/**
 * offset 为字节偏移量
 */
int
write_block(uint32 block_num, char *buf, int sz, int offset){
    if(offset + sz > DATABLKSZ){
        printf("write_block(): impossible");
        return -1;
    }
    alloc_block(block_num);
    char *old_buf = read_block(block_num);
    memmove(old_buf + offset, buf, sz);
    disk_write_block(block_num * 2, old_buf);
    disk_write_block(block_num * 2 + 1, old_buf + DEVICE_BLOCK_SIZE);
    return 0;
}



/* inode相关 */
uint32
create_inode(struct inode *inode){
    uint32 free_inode_index = get_free_inode();
    int block_num = free_inode_index / NINODE_PER_BLK + 1; //第几个数据块
    int off_index = free_inode_index % NINODE_PER_BLK;       //数据块内偏移
    alloc_inode(free_inode_index);
    write_block(block_num, (char *)inode, INODESZ, off_index * INODESZ);
    return free_inode_index;
}

/**
 * 
 * inode_num：要更新的inode编号
 * update_inode：要更新的inode
 */
int
sync_inode(uint32 inode_num, struct inode* update_inode){
    int block_num = inode_num / NINODE_PER_BLK + 1; //第几个数据块
    int off_index = inode_num % NINODE_PER_BLK;   //数据块内偏移
    write_block(block_num, (char *)update_inode, INODESZ, off_index * INODESZ);
}

struct inode*
read_inode(uint32 inode_num){
    int block_num = inode_num / NINODE_PER_BLK + 1; //第几个数据块
    int off_index = inode_num % NINODE_PER_BLK;   //数据块内偏移
    char *buf;
    //memset(buf, 0, DEVICE_BLOCK_SIZE);
    struct inode* inode;
    buf = read_block(block_num);
    inode = (struct inode *)(buf + off_index * INODESZ);
    /* 一个盘块只能装16个512B 32B */
    return inode;
}

/* dir_item相关 */
int
create_dir_item(struct inode* dir_node, struct dir_item* dir_item){
    if(dir_node->file_type == FILE)
        return -1;
    int sz = dir_node->size;
    int blk_index = sz / DATABLKSZ;
    int blk_off = sz % DATABLKSZ;
    if(blk_index > SINGLE + SIGNLELINK){
        printf("create_dir_item() can't alloc more data block \n");
        return -1;
    }
    int block_num = 0;
    if(dir_node->block_point[blk_index] == INVALID_INODE_POINT){
        block_num = get_free_block();
        dir_node->block_point[blk_index] = block_num; 
    }
    else{
        block_num = dir_node->block_point[blk_index];
    }
    write_block(block_num, (char *)dir_item, DIRITEMSZ, blk_off);
    dir_node->size += DIRITEMSZ;
    return 0;
}

struct dir_item*
read_dir_item(struct inode* dir_node, int blk_index, int off_index){
    if(off_index >= NDIRITEM_PER_BLK){
        printf("read_dir_item() cannot read num greater than 8 \n");
        return 0;
    }
    char *buf = read_block(dir_node->block_point[blk_index]);
    return (struct dir_item*)(buf + off_index * DIRITEMSZ);
}

/**
 * 
 * 在制定inode下寻找dir_item，仅需名字即可
 * 
 * footprint：代表需不需要输出查找过程
 * 
 * 
 * 
 * 目前暂时不允许文件夹与文件重名
 */
struct dir_item*
lookup_dir_item(struct inode* dir_inode, char *dir_name, int *blk_index, int *blk_off, int footprint ){
    int total_dir_item = dir_inode->size / DIRITEMSZ; 
    *blk_off = 0;
    for (int i = 0; i < SINGLE + SIGNLELINK; i++)
    {
        if(dir_inode->block_point[i] != INVALID_INODE_POINT){
            total_dir_item -= NDIRITEM_PER_BLK;
            int dir_item_per_blk = total_dir_item >= 0 ? NDIRITEM_PER_BLK : total_dir_item + NDIRITEM_PER_BLK;
            int is_last = total_dir_item < 0 ? 1 : 0;
            for (int j = 0; j < dir_item_per_blk; j++)
            {
                struct dir_item* dir_item = read_dir_item(dir_inode, i, j);
                if(dir_item->valid && footprint){
                    printf("%s %s\n", dir_item->name, dir_item->type == FILE ? "\e[35;1mFILE\e[0m" : "\e[36;1mDIR\e[0m");
                }
                if(strcmp(dir_item->name,dir_name) == 0){
                    *blk_index = i;
                    *blk_off = j * DIRITEMSZ;
                    return dir_item;
                }
            }
            *blk_off += dir_item_per_blk * DIRITEMSZ;
            if(is_last){
                *blk_index = i;
            }
        }
    }
    return (struct dir_item*)0;
}

int
sync_dir_item(struct inode* dir_node, struct dir_item* update_dir_item) {
    int blk_index = 0;
    int block_off = 0;
    if(lookup_dir_item(dir_node, update_dir_item->name,&blk_index, &block_off, 0)){
        write_block(dir_node->block_point[blk_index], (char *)update_dir_item, 
                                                        DIRITEMSZ, block_off);
        return 0;
    }
    return -1;
    
}



/**
 * 根据path，找到相应的dir_item
 * ../adadad/adad
 * ./adad/ad
 * addd/add
 * 
 * /dad/ad
 * 
 * path: 路径
 * dir_name：如果没有
 * current_dir_item:用于返回找到的dir_item
 * last_dir_item：用于返回dir_item的上一级dir_item
 * is_follow：是否跟踪路径
 * 
 * return：
 * 没有找到返回-1，否则返回0
 */
int
find_dir_item(char *path, char **dir_name, struct dir_item** current_dir_item, struct dir_item** last_dir_item, int is_follow){
    if(strcmp(path, "/") == 0){
        /* / */
        *current_dir_item = root_dir_item;
        *last_dir_item = root_dir_item;
        *dir_name = "/";
        if(is_follow){
            init_stack();
            push(root_dir_item);
        }
        return 0;
    }
    if(path[0] == '.' && path[1] == '.'){
        /* ../awdawdawd/awdaw */
        *current_dir_item = get_last_dir_item();
        peek_path(&path);
        if(is_follow && (memcmp(top(0)->name, "/", sizeof("/")) != 0))
            pop();
    }
    else if(path[0] == '/')
    {
        /* /dawdawd/awdaw */
        *current_dir_item = root_dir_item;
        if(is_follow){
            init_stack();
            push(root_dir_item);
        }
    }
    /* ./adad/adad */
    else if(path[0] == '.')
    {
        peek_path(&path);
        *current_dir_item = get_current_dir_item();
    }
    /* asdad/adad */
    else
    {
        *current_dir_item = get_current_dir_item();
    }
    
    while (*dir_name = peek_path(&path))
    {
        if(strlen(*dir_name) == 0){
            break;
        }
        printf("find_dir_item() path %s, dirname %s , len %ld\n", path, *dir_name, strlen(*dir_name));
        int blk_index = 0;
        int block_off = 0;
        struct inode* current_dir_node = read_inode((*current_dir_item)->inode_id);
        if(current_dir_node->file_type == FILE){
            printf("find_inode() error: %s is not directory \n", (*current_dir_item)->name);
            return -1;
        }
        struct dir_item* next_dir_item = 
                        lookup_dir_item(current_dir_node, *dir_name, &blk_index, &block_off, 0);
        if(!next_dir_item){
            printf("find_inode() directory %s is not exist!\n", *dir_name);
            return -1;
        }
        *last_dir_item = *current_dir_item;
        *current_dir_item = next_dir_item;
        if(next_dir_item->type == FILE){
            printf("find_inde() %s is a file , stop here!\n", *dir_name);
            return 0;
        }
        if(is_follow)
            push(next_dir_item);
    }
    return 0;
}
/********************** Codes *************************/
void 
terminal_fs(){
    close_disk();
}

void
swith_current_dir(char *path){
    struct dir_item* current_dir_item;
    struct dir_item* last_dir_item;
    struct inode* current_inode;
    char *dir_name;
    if(find_dir_item(path, &dir_name, &current_dir_item, &last_dir_item, 1) < 0){
        printf("path %s is not a directory", path);
        return;
    }
    if(current_dir_item->type == FILE)
        printf("\e[35m%s\e[0m \e[31mis not a directory \e[0m\n", path);
    char* current_path = get_current_path();
    printf("switch() %s \n", current_path);
}

void
check_dir(char *path){
    struct dir_item* current_dir_item;
    struct dir_item* last_dir_item;
    struct inode* current_inode;
    char *dir_name;
    find_dir_item(path, &dir_name, &current_dir_item, &last_dir_item, 0);
    current_inode = read_inode(current_dir_item->inode_id);
    int blk_index = 0;
    int block_off = 0;
    /* 查找 "/" 表明不查找任何匹配文件 */
    lookup_dir_item(current_inode, "/", &blk_index, &block_off, 1);
    printf("check_dir() blk_index: %d, block_off: %d dir_name: %s\n", blk_index ,block_off, dir_name);
}

struct inode*
create_file(char *path){
    struct dir_item* current_dir_item;
    struct dir_item* last_dir_item;
    struct inode* current_inode;
    struct inode* next_inode;
    char *dir_name;
    char *file_name = get_file_name(path);
    printf("path %s \n", path);
    if(find_dir_item(path, &dir_name, &current_dir_item, &last_dir_item, 0) < 0){
        if(memcmp(dir_name, file_name, strlen(file_name)) != 0){
            printf("create_dir() directory %s is not exist!\n", dir_name);
            return (struct inode*)0;    
        }
        
        printf("create_dir() not find path: %s\n", path);
        printf("create_dir() now we'll create one for you !\n");
        /* 创建不存在的目录 */
        struct inode* next_inode = new_inode(FILE);
        uint32 inode_index = create_inode(next_inode);
        struct dir_item* dir_item = new_dir_item(FILE, inode_index, dir_name);
        /* 刷新当前节点 */
        current_inode = read_inode(current_dir_item->inode_id); 
        create_dir_item(current_inode, dir_item);
        sync_inode(current_dir_item->inode_id, current_inode);            

        return next_inode;
    }
    printf("create_file() file %s is already exists !\n", file_name);
}

struct inode*
create_dir(char *path){
    su_blk->dir_inode_count++;
    /* 根目录 */
    if(strcmp(path, "/") == 0){
        struct inode *dir_node = new_inode(DIR);
        if(root_dir_node){
            printf("create_dir() can not create another root directory! \n");
            return (struct inode*)0;
        }
        struct dir_item* current_dir_item = new_dir_item(DIR, 0, ".");
        struct dir_item* last_dir_item = new_dir_item(DIR, 0, "..");
        create_dir_item(dir_node, current_dir_item);
        create_dir_item(dir_node, last_dir_item);
        create_inode(dir_node);
        sync_super_blk();
        return dir_node;
    }
    /* 非根目录 */
    else
    {
        /**
         * TODO
         * 
         * 基本逻辑：
         * 找到上一级dir_item，顺便找到它的inode，为其创建dir_item，
         * 
         * 记得每次更新 inode 和 
         */
        /* 测试，创建一个DIR */
        struct dir_item* current_dir_item;
        struct dir_item* last_dir_item;
        struct inode* current_inode;
        struct inode* next_inode;
        char *dir_name;
        printf("path %s \n", path);
        if(find_dir_item(path, &dir_name, &current_dir_item, &last_dir_item, 0) < 0){
            printf("create_dir() not find path: %s\n", path);
            printf("create_dir() now we'll create one for you !\n");

            /**
             *  
             * 创建不存在的目录
             * 
             * dir_item -> block -> dir .
             *                   -> dir ..
             * */
            struct inode* next_inode = new_inode(DIR);
            uint32 inode_index = create_inode(next_inode);
            struct dir_item* dir_item = new_dir_item(DIR, inode_index, dir_name);
            create_dir_item(next_inode, new_dir_item(DIR, dir_item->inode_id, "."));
            create_dir_item(next_inode, new_dir_item(DIR, current_dir_item->inode_id, ".."));
            sync_inode(dir_item->inode_id, next_inode);
            /* 刷新当前节点 */
            /**
             * 刷新current_inode即可
             * current_dir_item -> current_inode -> dir_item -> next_inode -> dir .
             *                                                             -> dir ..
             */
            current_inode = read_inode(current_dir_item->inode_id); 
            create_dir_item(current_inode, dir_item);
            sync_inode(current_dir_item->inode_id, current_inode);            

            next_inode = create_dir(path);
            sync_super_blk();
            return next_inode;
        }
    }
    
    /** test read write inode : success */
    /* struct inode *j;
    j = read_inode(0);
    printf("create_dir() dir_node.type: %d, link: %d \n", j->file_type, j->link);
    j->file_type = FILE;
    create_inode(j);
    j = read_inode(1);
    printf("create_dir() dir_node.type: %d, link: %d \n", j->file_type, j->link); */
}
int                         
copy_to(char *from_path, char *to_path){
    
    struct dir_item* from_current_dir_item;
    struct dir_item* from_last_dir_item;
    struct inode* from_current_inode;
    
    struct dir_item* to_current_dir_item;
    struct dir_item* to_last_dir_item;
    struct inode* to_current_inode;

    char *from_dir_name;
    char *to_dir_name;

    printf("from_path %s , to_path %s\n", from_path, to_path);

    if(find_dir_item(from_path, &from_dir_name, &from_current_dir_item, &from_last_dir_item, 0) < 0){
        printf("copy_to() source file not exist!\n");
        return -1;
    }
    if(find_dir_item(to_path, &to_dir_name, &to_current_dir_item, &to_last_dir_item, 0) == 0){
        printf("copy_to() dest file exists!\n");
        return -1;
    }
    from_current_inode = read_inode(from_current_dir_item->inode_id);
    to_current_inode = create_file(to_path);
    if(!to_current_inode){
        printf("copy_to() dest directory not exists!\n");
        return -1;
    }
    find_dir_item(to_path, &to_dir_name, &to_current_dir_item, &to_last_dir_item, 0);
    for (int i = 0; i < SINGLE + SIGNLELINK; i++)
    {
        uint32 origin_block_num = from_current_inode->block_point[i];
        if(origin_block_num != INVALID_INODE_POINT){
            uint32 block_num = get_free_block();
            to_current_inode->block_point[i] = block_num;
            char *buf = read_block(origin_block_num);
            write_block(block_num, buf, DATABLKSZ, 0);
        }
    }
    //sync_dir_item(to_current_dir_item,)
    sync_inode(to_current_dir_item->inode_id, to_current_inode);
    return 0;
}



/** 
 * 文件系统共4MB，共4MB / 1KB = 4096个数据块
 * 
 * 默认数据块0用于存储super block
 * 下标为0的inode默认为根目录
 * 
 * 数据块1 ~ 32 用于存储inode，顺序分配
 * 剩余数据块待分配
 * */
int
boot_fs(){
    if(open_disk() < 0){
        printf("Cannot open disk\n");
        return -1;
    }
    char *buf;
    buf = read_block(SUPERBLKLOC);
    su_blk = (struct super_block *)buf;
    if(su_blk->magic_num != MAGICNUM){
        
        printf("Oops! you have no file system !\n");
        printf("...\n");
        printf("Now we'll create one for you\n");

        su_blk->magic_num = MAGICNUM;
        su_blk->dir_inode_count = 0; 
        su_blk->free_inode_count = 32 * 32; //1024
        su_blk->free_block_count = 128 * 32;
        memset(su_blk->block_map, 0, sizeof(su_blk->block_map));
        memset(su_blk->inode_map, 0, sizeof(su_blk->inode_map));
        /* 数据块0标识superblk，将block 1~32标记为已被inode占用，get_free_block将不再返回它们 */
        for (int i = 0; i <= 32; i++)
        {
            alloc_block(i);
        }
        sync_super_blk();
        root_dir_node = create_dir("/");
    }
    /* 下标为0的inode默认为根目录项 */
    root_dir_node = read_inode(0);
    root_dir_item = new_dir_item(DIR, 0, "/");
    init_stack();
    push(root_dir_item);
    check_dir("/");
    /* current dir应该是dir_item */
    return 0;
}