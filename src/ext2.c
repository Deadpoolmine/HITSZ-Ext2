#include "defs.h"
#include "params.h"
#include "filetype.h"
#include "flags.h"



struct super_block *su_blk;
struct inode *root_dir_node;
struct dir_item *root_dir_item;
struct lookup_options
{
    int footprint;
    char *dir_name;
} ;

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
#ifdef DEBUG
    printf("push path() path_stack is too long");
#endif // debug
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
                #ifdef DEBUG
                    printf("find one block: %d\n", i*32 + j);
                #endif // DEBUG
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
                #ifdef DEBUG
                    printf("find one inode: %d\n", i*32 + j);
                #endif // DEBUG
                return i * 32 + j;
            }
        }   
    }
}



/* 分配一个inode */

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
release_block(uint32 block_num){
    int index = block_num / 32;
    int32 off = block_num % 32;
    uint32 mask = 1 << off;
    su_blk->block_map[index] &= ~mask;
    su_blk->free_block_count++;
    sync_super_blk();
}

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
    int index = inode_num / NINODE_PER_BLK;
    //int inode_index = su_blk->inode_block_map[inode_map_index]; //第几个数据块
    int off_index = inode_num % NINODE_PER_BLK;       //数据块内偏移
    uint32 mask = 1 << off_index;
    su_blk->inode_map[index] &= ~mask;
    su_blk->free_inode_count++;
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
        #ifdef DEBUG
             printf("write_block(): impossible");
        #endif // DEBUG
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
    int inode_map_index = free_inode_index / NINODE_PER_BLK;
    if(su_blk->inode_block_map[inode_map_index] == INVALID_INODE_BLK_MAP){
        su_blk->inode_block_map[inode_map_index] = get_free_block();
    }
    int block_num = su_blk->inode_block_map[inode_map_index]; //第几个数据块
    int off_index = free_inode_index % NINODE_PER_BLK;       //数据块内偏移
    alloc_inode(free_inode_index);
    write_block(block_num, (char *)inode, INODESZ, off_index * INODESZ);
    sync_super_blk();
    return free_inode_index;
}

/**
 * 
 * inode_num：要更新的inode编号
 * update_inode：要更新的inode
 */
int
sync_inode(uint32 inode_num, struct inode* update_inode){
    int block_num = su_blk->inode_block_map[inode_num / NINODE_PER_BLK]; //第几个数据块
    int off_index = inode_num % NINODE_PER_BLK;   //数据块内偏移
    write_block(block_num, (char *)update_inode, INODESZ, off_index * INODESZ);
}

struct inode*
read_inode(uint32 inode_num){
    int block_num = su_blk->inode_block_map[inode_num / NINODE_PER_BLK]; //第几个数据块
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
        #ifdef DEBUG
            printf("create_dir_item() can't alloc more data block \n");
        #endif // DEBUG
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
        #ifdef DEBUG
            printf("read_dir_item() cannot read num greater than 8 \n");
        #endif // DEBUG
        return 0;
    }
    char *buf = read_block(dir_node->block_point[blk_index]);
    return (struct dir_item*)(buf + off_index * DIRITEMSZ);
}

/**
 * 
 * 在指定inode下寻找dir_item，仅需名字即可
 * 
 * footprint：代表需不需要输出查找过程
 * 如果footprint非0，则1代表打印信息前不输出Tab，2代表输出一个Tab，以此类推
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
                if(!dir_item->valid)
                    continue;
                if(footprint){
                    for (int i = 1; i < footprint; i++)
                    {
                        printf("    ");
                    }
                    //printf("%s %s\n", dir_item->name, dir_item->type == FILE ? "\e[35;1mFILE\e[0m" : "\e[36;1mDIR\e[0m");
                    print_dir_item("",dir_item);
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
    if(lookup_dir_item(dir_node, update_dir_item->name,&blk_index, &block_off, UN_FOOT_PRINT)){
        write_block(dir_node->block_point[blk_index], (char *)update_dir_item, 
                                                        DIRITEMSZ, block_off);
        return 0;
    }
    return -1;
}


/**
 * 如果dest_inode的blk_index没有，则重新创建一个block，并把buf写进去，暂时考虑覆写整个blk
 * 
 */
void
write_inode_block(struct inode* dest_inode, char *buf, int sz, int blk_offset,int blk_index){
    uint32 block_num;
    if(dest_inode->block_point[blk_index] == INVALID_INODE_POINT){
        block_num = get_free_block();
        char*  buf = (char *)malloc(DATABLKSZ);
        memset(buf, 0 , DATABLKSZ);
        write_block(block_num, buf, DATABLKSZ, 0);
    }
    else
        block_num = dest_inode->block_point[blk_index];
    dest_inode->block_point[blk_index] = block_num;
    dest_inode->size += sz;
    write_block(block_num, buf, sz, blk_offset);
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
find_dir_item(char *path, char **dir_name, struct dir_item** current_dir_item, struct dir_item** last_dir_item, int is_follow, int is_remove){
    *last_dir_item = NULL;
    *current_dir_item = NULL;
    struct dir_item *last_last_dir_item = NULL;
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
        
        //printf("find_dir_item() path %s, dirname %s , len %ld\n", path, *dir_name, strlen(*dir_name));
        int blk_index = 0;
        int block_off = 0;
        struct inode* current_dir_node = read_inode((*current_dir_item)->inode_id);
        if(current_dir_node->file_type == FILE){
            #ifdef DEBUG
                printf("find_inode() error: %s is not directory \n", (*current_dir_item)->name);
            #endif // DEBUG
            return -1;
        }
        struct dir_item* next_dir_item = 
                        lookup_dir_item(current_dir_node, *dir_name, &blk_index, &block_off, UN_FOOT_PRINT);
        if(!next_dir_item){
            #ifdef DEBUG
                printf("find_inode() directory %s is not exist!\n", *dir_name);
            #endif // DEBUG
            return -1;
        }
        last_last_dir_item = *last_dir_item;
        *last_dir_item = *current_dir_item;
        *current_dir_item = next_dir_item;
        if(next_dir_item->type == FILE){
            #ifdef DEBUG
                printf("find_inde() %s is a file , stop here!\n", *dir_name);
            #endif // DEBUG
            return 0;
        }
        if(!strcmp(*dir_name, ".")){
            //do nothing
        }
        //如果是要移除".."目录的化，就不判断了；
        else if(!is_remove && !strcmp(*dir_name, "..")){
            if(*last_dir_item == NULL)
                *last_dir_item = root_dir_item;
            if(last_last_dir_item == NULL)
                last_last_dir_item = root_dir_item;
            *current_dir_item = *last_dir_item;
            *last_dir_item = last_last_dir_item;
            if(is_follow) {
                struct dir_item* item = pop();
                if(!strcmp(item->name, "/"))
                    push(root_dir_item);
            }
        }
        else
        {
            if(is_follow)
                push(next_dir_item);
        }   
    }
    return 0;
}

int
judge_path_type(char *path){
    struct dir_item* current_dir_item;
    struct dir_item* last_dir_item;
    struct inode* current_inode;
    char *dir_name;

    if(find_dir_item(path, &dir_name, &current_dir_item, &last_dir_item, NOT_FOLLOW, NOT_REMOVE) < 0){
        #ifdef DEBUG
            printf("judge_path_type() source not exist!\n");
            printf("\e[35mcp: source file/directory is not exist\e[0m\n");
        #endif // DEBUG
        return -1;
    }

    return current_dir_item->type;
}

/* 紧缩inode下的所有 */
void
shrink_dir_items(uint32 inode_id, struct inode* inode){
    int valid_dir_item_count = 0;
    int total_dir_item = inode->size / DIRITEMSZ;
    struct dir_item* chache_dir_items = (struct dir_item*)malloc(inode->size);  
    for (int i = 0; i < SINGLE + SIGNLELINK; i++)
    {
        if(inode->block_point[i] != INVALID_INODE_POINT){
            total_dir_item -= NDIRITEM_PER_BLK;
            int dir_item_per_blk = total_dir_item >= 0 ? NDIRITEM_PER_BLK : total_dir_item + NDIRITEM_PER_BLK;
            int is_find = 0;
            for (int j = 0; j < dir_item_per_blk; j++)
            {
                struct dir_item* dir_item = read_dir_item(inode, i, j);
                if(dir_item->valid){
                    is_find = 1;
                    chache_dir_items[valid_dir_item_count++] = *dir_item;
                }
            }
            if(!is_find){
                release_block(inode->block_point[i]);
                inode->block_point[i] = INVALID_INODE_POINT;
            }
        }
    }
    inode->size = 0;
    for (int i = 0; i < valid_dir_item_count; i++)
    {
        create_dir_item(inode, &chache_dir_items[i]);
    }
    sync_inode(inode_id, inode);
}
/********************** Codes *************************/

void 
terminal_fs(){
    close_disk();
}
/********************** cd related *************************/
void
swith_current_dir(char *path){
    struct dir_item* current_dir_item;
    struct dir_item* last_dir_item;
    struct inode* current_inode;
    char *dir_name;
    if(find_dir_item(path, &dir_name, &current_dir_item, &last_dir_item, NOT_FOLLOW, NOT_REMOVE) < 0){
        /* printf("path %s is not a directory", path); */
        raise_path_not_exist("cd", path);
        return;
    }
    find_dir_item(path, &dir_name, &current_dir_item, &last_dir_item, FOLLOW, NOT_REMOVE);
    if(current_dir_item->type == FILE)
        raise_path_type_error("cd", path, DIR);
        //printf("\e[35m%s\e[0m \e[31mis not a directory \e[0m\n", path);
    char* current_path = get_current_path();
    #ifdef DEBUG
        printf("switch() %s \n", current_path);
    #endif // DEBUG
}

/********************** ls related *************************/
void
check_dir(char *path){
    struct dir_item* current_dir_item;
    struct dir_item* last_dir_item;
    struct inode* current_inode;
    char *dir_name;
    find_dir_item(path, &dir_name, &current_dir_item, &last_dir_item, NOT_FOLLOW, NOT_REMOVE);
    current_inode = read_inode(current_dir_item->inode_id);
    int blk_index = 0;
    int block_off = 0;
    /* 查找 "/" 表明不查找任何匹配文件 */
    lookup_dir_item(current_inode, "/", &blk_index, &block_off, FOOT_PRINT);
    //printf("check_dir() blk_index: %d, block_off: %d dir_name: %s\n", blk_index ,block_off, dir_name);
    print_dir_info(block_off / DIRITEMSZ);
}
/********************** stru related *************************/
void
observe_fs_structure(char *path, int level, int show_detail){
    struct dir_item* current_dir_item;
    struct dir_item* last_dir_item;
    struct inode* current_inode;
    char *dir_name;
    if(find_dir_item(path, &dir_name, &current_dir_item, &last_dir_item, NOT_FOLLOW, NOT_REMOVE) < 0){
        //printf("\e[35mcp: path \e[0m\e[0m%s\e[0m\e[35m is not exist\e[0m\n", path);
        raise_path_not_exist("stru", path);
        return;
    }
    current_inode = read_inode(current_dir_item->inode_id);
    int total_dir_item = current_inode->size / DIRITEMSZ;
    for (int i = 0; i < SINGLE + SIGNLELINK; i++)
    {
        if(current_inode->block_point[i] != INVALID_INODE_POINT){
            total_dir_item -= NDIRITEM_PER_BLK;
            int dir_item_per_blk = total_dir_item >= 0 ? NDIRITEM_PER_BLK : total_dir_item + NDIRITEM_PER_BLK;
            for (int j = 0; j < dir_item_per_blk; j++)
            {
                struct dir_item* dir_item = read_dir_item(current_inode, i, j);
                if(!dir_item->valid)
                    continue;
                for (int i = 0; i < level; i++)
                {
                    printf("    ");
                }
                print_dir_item("", dir_item);
                if(dir_item->type == DIR)
                {
                    char *new_path = memcmp(path, "/", sizeof("/")) == 0 ? path : join(path, "/");
                    new_path = join(new_path, dir_item->name);
                    if(memcmp(dir_item->name, ".", sizeof(".")) != 0 && 
                                memcmp(dir_item->name, "..", sizeof("..")) != 0)
                        observe_fs_structure(new_path, level + 1, show_detail);
                }
            }
        }
    }
    if(level == 0 && show_detail){
        printf("--------------------- block map --------------------\n");
        for (int i = 0; i < 128; i++)
        {
            printf("%08x\n", su_blk->block_map[i]);
        }
        printf("--------------------- inode map --------------------\n");
        for (int i = 0; i < 32; i++)
        {
            printf("%08x\n", su_blk->inode_map[i]);
        }
    }
    
}
/********************** touch related *************************/
struct inode*
create_file(char *path){
    struct dir_item* current_dir_item;
    struct dir_item* last_dir_item;
    struct inode* current_inode;
    struct inode* next_inode;
    char *dir_name;
    char *file_name = get_file_name(path);
    #ifdef DEBUG
        printf("path %s \n", path);
    #endif //DEBUG
    if(find_dir_item(path, &dir_name, &current_dir_item, &last_dir_item, NOT_FOLLOW, NOT_REMOVE) < 0){
        #ifdef DEBUG
            printf("create_dir() not find path: %s\n", path);
            printf("create_dir() now we'll create one for you !\n");
        #endif // DEBUG
        /* 创建不存在的文件 */
        struct inode* next_inode = new_inode(FILE);
        uint32 inode_index = create_inode(next_inode);
        struct dir_item* dir_item = new_dir_item(FILE, inode_index, dir_name);
        /* 刷新当前节点 */
        current_inode = read_inode(current_dir_item->inode_id); 
        create_dir_item(current_inode, dir_item);
        sync_inode(current_dir_item->inode_id, current_inode);            

        return next_inode;
    }
    //printf("create_file() file %s is already exists !\n", file_name);
    raise_path_exist("touch", path);
}

/********************** mkdir related *************************/
struct inode*
recurse_create_dir(char *path){
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
    #ifdef DEBUG
        printf("path %s \n", path);
    #endif // DEBUG
    if(find_dir_item(path, &dir_name, &current_dir_item, &last_dir_item, NOT_FOLLOW, NOT_REMOVE) < 0){
        #ifdef DEBUG
            printf("create_dir() not find path: %s\n", path);
            printf("create_dir() now we'll create one for you !\n");
        #endif // DEBUG
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

        next_inode = recurse_create_dir(path);
        sync_super_blk();
        return next_inode;
    }
    
}
struct inode*
create_dir(char *path){
    struct dir_item* current_dir_item;
    struct dir_item* last_dir_item;
    char *dir_name;
    su_blk->dir_inode_count++;
    /* 根目录 */
    if(strcmp(path, "/") == 0){
        struct inode *dir_node = new_inode(DIR);
        if(root_dir_node){
            raise_common_error("mkdir", "can not create another root directory!");
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
    if(find_dir_item(path, &dir_name, &current_dir_item, &last_dir_item, NOT_FOLLOW, NOT_REMOVE) == 0){
        raise_path_exist("mkdir", path);
        return read_inode(current_dir_item->inode_id);
    }
    /* 非根目录 */
    else
    {   
        return recurse_create_dir(path);
    }
}

/********************** cp related *************************/

/** 判断path1和path2对应的是否是同一个文件/目录 */
//TODO 凭借字符串匹配来做，减少开销
int
check_path_same(char *path1, char *path2){
    struct dir_item* current_dir_item1;
    struct dir_item* last_dir_item1;
    
    struct dir_item* current_dir_item2;
    struct dir_item* last_dir_item2;

    char *dir_name1;
    char *dir_name2;

    find_dir_item(path1, &dir_name1, &current_dir_item1, &last_dir_item1, NOT_FOLLOW, NOT_REMOVE);
    find_dir_item(path2, &dir_name2, &current_dir_item2, &last_dir_item2, NOT_FOLLOW, NOT_REMOVE);
    if(strcmp(dir_name1, dir_name2) != 0)
        return 0;
    if(current_dir_item1->inode_id != current_dir_item2->inode_id)
        return 0;
    if(strcmp(current_dir_item1->name, current_dir_item2->name) != 0)
        return 0;
    if(current_dir_item1->type != current_dir_item2->type)
        return 0;
    if(current_dir_item1->valid != current_dir_item2->valid)
        return 0;
    return 1;
}

int
copy_file(char *from_file_path, char *to_file_path){
    struct dir_item* from_current_dir_item;
    struct dir_item* from_last_dir_item;
    
    struct dir_item* to_current_dir_item;
    struct dir_item* to_last_dir_item;

    char *from_dir_name;
    char *to_dir_name;
    #ifdef DEBUG
        printf("from_path %s , to_path %s\n", from_file_path, to_file_path);
    #endif // DEBUG
    char *to_file_name = get_file_name(to_file_path);
    int to_file_name_len = strlen(to_file_name);
    int to_file_path_len = strlen(to_file_path);
    char *pre_to_file_path = (char *)malloc(to_file_path_len - to_file_name_len);
    for (int i = 0; i < to_file_path_len - to_file_name_len; i++)
    {
        pre_to_file_path[i] = to_file_path[i];
    }
    
    if(find_dir_item(pre_to_file_path, &from_dir_name, &from_current_dir_item, &from_last_dir_item, NOT_FOLLOW,NOT_REMOVE) < 0){
        //printf("copy_file() source file not exist!\n");
        raise_path_not_exist("cp - destionation - directory", pre_to_file_path);
        return -1;
    }

    if(find_dir_item(from_file_path, &from_dir_name, &from_current_dir_item, &from_last_dir_item, NOT_FOLLOW,NOT_REMOVE) < 0){
        //printf("copy_file() source file not exist!\n");
        raise_path_not_exist("cp - source", from_file_path);
        return -1;
    }
    if(find_dir_item(to_file_path, &to_dir_name, &to_current_dir_item, &to_last_dir_item, NOT_FOLLOW, NOT_REMOVE) == 0){
        //printf("copy_file() dest file exists!\n");
        raise_path_exist("cp - destination", to_file_path);
        return -1;
    }
    
    struct inode* src_inode = read_inode(from_current_dir_item->inode_id);
    struct inode* dest_inode = create_file(to_file_path);
    if(!dest_inode){
        //printf("copy_file() dest directory not exists!\n");
        //raise_path_not_exist("cp - destination", to_file_path);
        return -1;
    }
    find_dir_item(to_file_path, &to_dir_name, &to_current_dir_item, &to_last_dir_item, NOT_FOLLOW, NOT_REMOVE);
    for (int i = 0; i < SINGLE + SIGNLELINK; i++)
    {
        uint32 origin_block_num = src_inode->block_point[i];
        if(origin_block_num != INVALID_INODE_POINT){
            char *buf = read_block(origin_block_num);
            uint32 block_num = get_free_block();
            dest_inode->block_point[i] = block_num;
            write_block(block_num, buf, DATABLKSZ, 0);
        }
    }
    //sync_dir_item(to_current_dir_item,)
    sync_inode(to_current_dir_item->inode_id, dest_inode);
}

int
copy_directory(char *from_dir_path, char *to_dir_path, int is_init, char *origin_to_dir_path){
    struct dir_item* from_current_dir_item;
    struct dir_item* from_last_dir_item;
    struct dir_item* to_current_dir_item;
    struct dir_item* to_last_dir_item;
    char *from_dir_name;
    char *to_dir_name;
    if(is_init){
        origin_to_dir_path = to_dir_path;
    }
    
    if(find_dir_item(to_dir_path, &to_dir_name, &to_current_dir_item, &to_last_dir_item, NOT_FOLLOW, NOT_REMOVE) == 0){
        //printf("copy_directory() dest directory exists!\n");
        if(memcmp(get_file_name(to_dir_path), ".", sizeof(".")) 
        && memcmp(get_file_name(to_dir_path), "..", sizeof("..")))
            raise_path_exist("cp - destination", to_dir_path);
        return -1;
    }
    struct inode* dest_dir_node = create_dir(to_dir_path);
    /* 找到源目录对应的 目录的 dir */
    find_dir_item(from_dir_path, &from_dir_name, &from_current_dir_item, &from_last_dir_item, NOT_FOLLOW, NOT_REMOVE);
    struct inode* src_dir_node = read_inode(from_current_dir_item->inode_id);
    int total_dir_item = src_dir_node->size / DIRITEMSZ;
    /* 遍历源目录，调用copy file和递归调用copy directory */
    for (int i = 0; i < SIGNLELINK + SINGLE; i++)
    {
        if(src_dir_node->block_point[i] != INVALID_INODE_POINT){
            total_dir_item -= NDIRITEM_PER_BLK;
            int dir_item_per_blk = total_dir_item >= 0 ? NDIRITEM_PER_BLK : total_dir_item + NDIRITEM_PER_BLK;
            for (int j = 0; j < dir_item_per_blk; j++)
            {
                struct dir_item* dir_item = read_dir_item(src_dir_node, i, j);
                char *src_path = memcmp(from_dir_name, "/", sizeof("/")) == 0 ? 
                                            from_dir_path : join(from_dir_path, "/");
                src_path = join(src_path, dir_item->name);
                /* 保证不重复复制同一个文件 */
                if(check_path_same(src_path, origin_to_dir_path))
                    continue;
                char *dest_path = join(to_dir_path, "/");
                dest_path = join(dest_path, dir_item->name);
                if(dir_item->type == FILE){
                    copy_file(src_path, dest_path);
                }
                else if(dir_item->type == DIR){
                    copy_directory(src_path, dest_path, 0, origin_to_dir_path);
                }
            }
        }
    }
}

int                         
copy_to(char *from_path, char *to_path){
    /* 判断源类型 */
    int path_type = judge_path_type(from_path);
    if(path_type < 0){
        raise_path_not_exist("cp", from_path);
        return -1;
    }
    if(path_type == FILE){
        copy_file(from_path, to_path);
    }
    else if(path_type== DIR)
    {
        copy_directory(from_path, to_path, 1, NULL);
    }
    return 0;
}
/********************** cat related *************************/
char*
read_file(char *path){
    struct dir_item* current_dir_item;
    struct dir_item* last_dir_item;
    struct inode* current_inode;
    char *dir_name;
    char *file_name = get_file_name(path);
    if(find_dir_item(path, &dir_name, &current_dir_item, &last_dir_item, NOT_FOLLOW, NOT_REMOVE) < 0){
        /* printf("read_file() source file not exist!\n");
        printf("\e[35mcp: source file is not exist\e[0m\n"); */
        raise_path_not_exist("cat", path);
        return NULL;
    }
    if(current_dir_item->type == DIR){
        raise_path_type_error("cat", path, FILE);
        return NULL;
    }
    current_inode = read_inode(current_dir_item->inode_id);
    char *contents = (char *)malloc(current_inode->size);
    for (int i = 0; i < SINGLE + SIGNLELINK; i++)
    {
        if(current_inode->block_point[i] != INVALID_INODE_POINT){
            uint32 block_num = current_inode->block_point[i];
            char *buf = read_block(block_num);
            contents = join(contents, buf);
            //printf("%s", buf);
        }
    }
    return contents;
}
/********************** tee related *************************/
void
write_file(char *path){
    struct dir_item* current_dir_item;
    struct dir_item* last_dir_item;
    struct inode* current_inode;
    char *dir_name;
    char *file_name = get_file_name(path);
    if(find_dir_item(path, &dir_name, &current_dir_item, &last_dir_item, NOT_FOLLOW, NOT_REMOVE) < 0){
        /* printf("write_file() source file not exist!\n");
        printf("\e[35mcp: source file is not exist\e[0m\n"); */
        raise_path_not_exist("tee", path);
        return;
    }
    if(current_dir_item->type == DIR){
        /* printf("write_file() %s is not a file!\n", path);
        printf("\e[35mtee: %s is a directroy !\e[0m\n", path); */
        raise_path_type_error("tee", path, FILE);
        return;
    }
    /* 清空 */
    current_inode = read_inode(current_dir_item->inode_id);
    for (int i = 0; i < SINGLE + SIGNLELINK; i++)
    {
        current_inode->block_point[i] = INVALID_INODE_POINT;
    }
    char buf[DATABLKSZ];
    memset(buf, 0, DATABLKSZ);
    char c;
    int len = 0, total_len = 0, blk_index = 0;
    while (1)
    {
        memset(buf, 0, DATABLKSZ);
        fgets(buf,DATABLKSZ,stdin);
        int blk_index = total_len / DATABLKSZ;
        int offset = total_len % DATABLKSZ;
        len = strlen(buf);
        total_len += len;
        int sz = len, left_sz = -1;
        if(offset + len > DATABLKSZ){
            left_sz = offset + len - DATABLKSZ;
            sz -= left_sz;
        }
        write_inode_block(current_inode, buf, sz, offset, blk_index);
        if(total_len % DATABLKSZ == 0)
            blk_index++;
        if(left_sz != -1)
            write_inode_block(current_inode, buf, left_sz, 0, blk_index);
        if(len == 1){
            break;
        }
    }
    sync_inode(current_dir_item->inode_id, current_inode);
}
/********************** remove related *************************/
int
remove_file(char *path){
    struct dir_item* current_dir_item;
    struct dir_item* last_dir_item;
    struct inode* current_inode;
    struct inode* last_inode;
    char *dir_name;

    if(find_dir_item(path, &dir_name, &current_dir_item, &last_dir_item, NOT_FOLLOW, REMOVE) < 0){
        /* printf("remove_file() source not exist!\n");
        printf("\e[35mcp: source file/directory is not exist\e[0m\n"); */
        raise_path_not_exist("rm", path);
        return -1;
    }
    current_dir_item->valid = 0;
    current_inode = read_inode(current_dir_item->inode_id);
    last_inode = read_inode(last_dir_item->inode_id);
    if(current_dir_item->type == DIR){
        /* 修改了就得同步，此时对应的情况应该是remove . .. */
        sync_dir_item(last_inode, current_dir_item);
        return -1;
    }
    for (int i = 0; i < SINGLE + SIGNLELINK; i++)
    {
        if(current_inode->block_point[i] != INVALID_INODE_POINT){
            release_block(current_inode->block_point[i]);
            current_inode->block_point[i] = INVALID_INODE_POINT;
        }   
    }
    current_inode->size = 0;
    sync_dir_item(last_inode, current_dir_item);
    release_inode(current_dir_item->inode_id);
    sync_inode(current_dir_item->inode_id, current_inode);
    shrink_dir_items(last_dir_item->inode_id, last_inode);
    return 0;
}

int
remove_directory(char *path){
    struct dir_item* current_dir_item;
    struct dir_item* last_dir_item;
    struct inode* current_inode;
    struct inode* last_inode;
    char *dir_name;
    if(find_dir_item(path, &dir_name, &current_dir_item, &last_dir_item, NOT_FOLLOW, REMOVE) < 0){
        //printf("remove_file() source not exist!\n");
        //printf("\e[35msource file/directory is not exist\e[0m\n");
        raise_path_not_exist("rm", path);
        return -1;
    }
    current_dir_item->valid = 0;
    last_inode = read_inode(last_dir_item->inode_id);
    current_inode = read_inode(current_dir_item->inode_id);
    int total_dir_item = current_inode->size / DIRITEMSZ;
    /* 遍历源目录，调用copy file和递归调用copy directory */
    for (int i = 0; i < SIGNLELINK + SINGLE; i++)
    {
        if(current_inode->block_point[i] != INVALID_INODE_POINT){
            total_dir_item -= NDIRITEM_PER_BLK;
            int dir_item_per_blk = total_dir_item >= 0 ? NDIRITEM_PER_BLK : total_dir_item + NDIRITEM_PER_BLK;
            for (int j = 0; j < dir_item_per_blk; j++)
            {
                struct dir_item* dir_item = read_dir_item(current_inode, i, j);
                char *src_path;
                src_path = join(path, "/");
                src_path = join(src_path, dir_item->name);
                if(!strcmp(dir_item->name, ".") || !strcmp(dir_item->name, "..")){
                    remove_file(src_path);
                    continue;
                }
                if(dir_item->type == FILE){
                    remove_file(src_path);
                }
                else if(dir_item->type == DIR){
                    remove_directory(src_path);
                }
            }
            release_block(current_inode->block_point[i]);
            current_inode->block_point[i] = INVALID_INODE_POINT;
        }
    }
    current_inode->size = 0;
    sync_dir_item(last_inode, current_dir_item);
    release_inode(current_dir_item->inode_id);
    sync_inode(current_dir_item->inode_id, current_inode);
    shrink_dir_items(last_dir_item->inode_id, last_inode);
    return 0;
}

void
remove_path(char *path){
    if(!strcmp(path, "..") || !strcmp(path, ".") || !strcmp(path, "/")){
        //printf("\e[35mcan't remove current or previous directory\e[0m\n");
        raise_common_error("rm","can't remove current or previous directory");
        return;
    }
    int path_type = judge_path_type(path);
    if(path_type < 0){
        raise_common_error("rm","can't remove a path doesn't exists");
        return;
    }
    if(path_type == FILE){
        remove_file(path);
    }
    else
    {
        remove_directory(path);
    }
}
/********************** move related *************************/
void
move_to(char *from_path, char *to_path){
    int file_type = judge_path_type(to_path);
    if(file_type != -1){
        //printf("\e[35mcan't move to a file/directory that exists\e[0m\n");
        raise_common_error("mv", "can't move to a file/directory that exists");
        return;
    }
    copy_to(from_path, to_path);
    remove_path(from_path);
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
        
        printf("\e[31mOops! you have no file system !\n");
        printf("...\n");
        printf("Now we'll create one for you\n");
        printf("Please wait...\e[0m\n");
        sleep(1);
        su_blk->magic_num = MAGICNUM;
        su_blk->dir_inode_count = 0; 
        su_blk->free_inode_count = 32 * 32; //1024
        su_blk->free_block_count = 128 * 32;
        memset(su_blk->block_map, 0, sizeof(su_blk->block_map));
        memset(su_blk->inode_map, 0, sizeof(su_blk->inode_map));
        /* 数据块0标识superblk，将block 1~32标记为已被inode占用，get_free_block将不再返回它们 */
        /* for (int i = 0; i <= 32; i++)
        {
            alloc_block(i);
        } */
        alloc_block(SUPERBLKLOC);
        for (int i = 0; i < 32; i++)
        {
            su_blk->inode_block_map[i] = INVALID_INODE_BLK_MAP;
        }
        sync_super_blk();
        root_dir_node = create_dir("/");
    }
    /* 下标为0的inode默认为根目录项 */
    root_dir_node = read_inode(0);
    root_dir_item = new_dir_item(DIR, 0, "/");
    init_stack();
    push(root_dir_item);
    #ifdef DEBUG
        check_dir("/");
    #endif // DEBUG
    /* current dir应该是dir_item */
    return 0;
}