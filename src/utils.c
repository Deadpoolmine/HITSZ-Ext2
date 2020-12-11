#include "defs.h"
/* Copy From XV6 */
int
gets(char *buf, int max)
{
  int i;
  char c;

  for(i=0; i+1 < max; ){
    c = getchar();
    if(c < 0)
      break;
    if(c == '\n' || c == '\r')
      break;
    buf[i++] = c;
  }
  buf[i] = '\0';
  return strlen(buf);
}

int 
convertAa(char *str){
    int len = strlen(str);
    for (size_t i = 0; i < len; i++)
    {
        if(str[i] >= 'A' && str[i] <= 'Z'){
            str[i] += 32;
        }
    }
    return 1;
}

void
panic(char * str){
  printf("%s \n", str);
  while (1);
}

char*
peek_path(char **path){
  char *ps, *es;
  ps = *path;
  if(ps[0] == '/')
    ps++;
  es = ps;
  while (*es != '/' && *es != '\0')
  {
    es++;
  }
  int token_len = es - ps;
  char *token = (char *)malloc(token_len);
  for (int i = 0; i < token_len; i++)
  {
    token[i] = ps[i];
  }
  *path = es; 
  return token; 
}

