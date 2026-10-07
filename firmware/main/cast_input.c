#include "cast_input.h"
#include <stdio.h>
#include <string.h>
static const char *groups[]={"\babcdefghijklmnopqrstuvwxyz","\bABCDEFGHIJKLMNOPQRSTUVWXYZ","\b0123456789","\b !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"};
static const char *names[]={"小写字母","大写字母","数字","符号和空格"};
void cast_input_begin(cast_input_t *s,const char *initial,size_t limit) {
    memset(s,0,sizeof(*s));s->limit=limit<sizeof(s->value)?limit:sizeof(s->value)-1;s->cursor=1;
    if(initial) {s->length=strlen(initial);if(s->length>s->limit)s->length=s->limit;memcpy(s->value,initial,s->length);}
}
cast_input_result_t cast_input_key(cast_input_t *s,cast_key_t key) {
    size_t count=strlen(groups[s->group]);
    if(key==CAST_BACK) {memset(s,0,sizeof(*s));return INPUT_CANCELLED;}
    if(key==CAST_LONG_DOWN)return INPUT_DONE;
    if(key==CAST_LONG_UP) {s->group=(s->group+1)%4;s->cursor=1;}
    else if(key==CAST_UP)s->cursor=(s->cursor+count-1)%count;
    else if(key==CAST_DOWN)s->cursor=(s->cursor+1)%count;
    else if(key==CAST_OK) {
        char c=groups[s->group][s->cursor];
        if(c=='\b') {if(s->length)s->value[--s->length]=0;}
        else if(s->length<s->limit) {s->value[s->length++]=c;s->value[s->length]=0;}
    }
    return INPUT_EDITING;
}
void cast_input_description(const cast_input_t *s,bool password,char *out,size_t size) {
    char selected[8];char c=groups[s->group][s->cursor];
    if(c=='\b')strcpy(selected,"删除");else if(c==' ')strcpy(selected,"空格");else {selected[0]=c;selected[1]=0;}
    char preview[13]={0};size_t n=s->length<12?s->length:12;
    if(password)memset(preview,'*',n);else memcpy(preview,s->value+s->length-n,n);
    snprintf(out,size,"%s\n%u/%u 字符\n%s  [ %s ]\n上下选择 OK 输入\n长按上键切换字符组\n长按下键完成输入\n长按确定取消",preview,(unsigned)s->length,(unsigned)s->limit,names[s->group],selected);
}
