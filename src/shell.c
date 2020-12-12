#include "defs.h"
#include "params.h"

/********************** definition *************************/
char whitespace[] = " \t\r\n\v";

typedef enum op{
    ls,
    mkdir,
    touch,
    cp,
    cd,
    tee,
    cat,
    shutdown,
    undefine
} op_t;

typedef struct cmd{
    op_t op;
    char args[MAXARGS][MAXPATH];
} cmd_t;

/********************** Code *************************/
int
gettoken(char **ps, char *es, int startpos, char *parsedtoken)
{
    char *s;
    int pos = startpos;
    s = *ps + startpos;
    char *token, *endoftoken;
    /* 清理所有s的空格 trim */
    while(s < es && strchr(whitespace, *s)){
        s++;
        pos++;
    }
    token = s;
    while (s < es && !strchr(whitespace, *s))
    {
        /* code */
        s++;
        pos++;
    }
    endoftoken = s;

    s = token;
    for (; s < endoftoken; s++)
    {
        *(parsedtoken++) = *s;
        //printf("%c", *s);
    }
    *parsedtoken = '\0';
    return pos;
} 

op_t
translate_op(char *token){
    op_t op = undefine;
    if(!strcmp(token, "ls")){
        op = ls;
    }
    else if (!strcmp(token, "mkdir"))
    {
        op = mkdir;
    }
    else if (!strcmp(token, "touch"))
    {
        op = touch;
    }
    else if (!strcmp(token, "cp"))
    {
        op = cp;
    }   
    else if (!strcmp(token, "cd"))
    {
        op = cd;
    }
    else if (!strcmp(token, "tee"))
    {
        op = tee;
    }
    else if (!strcmp(token, "cat"))
    {
        op = cat;
    }
    else if (!strcmp(token, "shutdown"))
    {
        op = shutdown;
    }
    return op;
}
void 
print_cmd(cmd_t cmd){
    printf("exec_cmd(): \n");
    printf("op(): %d \n", cmd.op);
    for (int i = 0; i < MAXARGS; i++)
    {
        printf("arg %d: %s , %ld\n", i+1, cmd.args[i], strlen(cmd.args[i]));
    }
}
void
exec_cmd(cmd_t cmd){
    //printf("exec_cmd() op: %d, from_path: %s, to_path: %s\n", cmd.op, cmd.from_path, cmd.to_path);
    //print_cmd(cmd);
    switch (cmd.op)
    {
    case ls:
        for (int i = 0; i < MAXARGS; i++)
        {
            if(cmd.args[i][0] != 0)
                check_dir(cmd.args[i]);
            if(cmd.args[i][0] == 0 && i == 0){
                check_dir(".");
            }
        }
        break;
    case mkdir:
        for (int i = 0; i < MAXARGS; i++)
        {
            if(cmd.args[i][0] != 0)
                create_dir(cmd.args[i]);
        }
        break;
    case touch:
        for (int i = 0; i < MAXARGS; i++)
        {
            if(cmd.args[i][0] != 0)
                create_file(cmd.args[i]);
        }
        break;
    case cp:
        copy_to(cmd.args[0], cmd.args[1]);
        break;
    case cd:
        swith_current_dir(cmd.args[0]);
        break;
    case tee:
        for (int i = 0; i < MAXARGS; i++)
        {
            if(cmd.args[i][0] != 0)
                write_file(cmd.args[i]);
        }
        break;
    case cat:
        for (int i = 0; i < MAXARGS; i++)
        {
            if(cmd.args[i][0] != 0){
                char *contents = read_file(cmd.args[i]);
                printf("\e[32;1m%s\e[0;1m",contents);
            }
        }
        break;
    case shutdown:
        printf("\e[32;1mshut down...welcome next time !\n");
        sleep(1);
        exit(0);
        break;
    case undefine:
        printf("\e[32;1mundefined command, we'll add that later \n");
        break;
    default:
        break;
    }
}

void 
parse_cmd(char *cmd, int cmdlen){
    printf("parse_cmd(): %s \n", cmd);
    int startpos = 0;
    int count = 0;
    cmd_t *_cmd = (cmd_t *)malloc(sizeof(cmd_t));
    _cmd->op = undefine;
    for (int i = 0; i < MAXARGS; i++)
    {
        memset(_cmd->args[i], 0, MAXPATH); 
    }
    while (startpos < cmdlen)
    {
        count++;
        char *token = (char *)malloc(MAXPATH);
        startpos = gettoken(&cmd, cmd + cmdlen, startpos, token);
        if(count == 1){
            convertAa(token);
            _cmd->op = translate_op(token);
        }
        else
        {
            if(strlen(token) != 0)
                memmove(_cmd->args[count - 2], token, strlen(token));
        }
        printf("parse_cmd(): get token: %s, length: %ld\n", token, strlen(token));
    }
    exec_cmd(*_cmd);
}



void 
boot_shell(){
    char cmd[MAXCMD];
    int cmdlen;
    printf("booting shell... \n");
    printf("success!\n");
    while (1)
    {
        char* current_dir = get_current_path();
        
        printf("\e[31;1mdeadpool&star:\e[0m\e[34;1m~%s\e[0m$ ", current_dir);
        memset(cmd, 0, MAXCMD);
        if((cmdlen = gets(cmd, MAXCMD)) == 0){
            //printf("exit shell!\n");
            //break;
        }
        parse_cmd(cmd, cmdlen);
        printf("\n");
    }
}
