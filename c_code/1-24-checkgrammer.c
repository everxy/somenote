
// 只是检查(){}[]是否是匹配的。
#include <stdio.h>

int brace,brack,paren;

void in_quote(int c);
void in_comment(void);
void search(int c);

int main(){

    int c;
    extern int brace,brack,paren;
    while((c = getchar()) != EOF) {
        if(c == '/') {
            if((c = getchar()) == '*')
                in_comment();
            else
                search(c);
        } else if(c == '/' || c == '"')
            in_quote(c);
        else
            search(c);
       // 只有右半部分的情况 ) } ] 
        if(brace < 0){
            printf("unbalanced braces 1 \n");
            brace = 0;
        } else if( brack < 0){
            printf("unbalanced bracks 1 \n");
            brack = 0;
        } else if(paren < 0){
            printf("unbalanced paren 1 \n");
            paren = 0;
        }
    //只有左半部分的情况 ( { [ 
    // 如果在这里则会在换行后就直接输出两次，因为它要一直查询到输入完成才能判断
    // 但是如果直接是只有右边的情况就肯定是错的，可以直接输出 
    // if(brace > 0) 
    //     printf("unbalanced braces \n");
    // if(brack > 0) 
    //     printf("unbalanced bracks \n");
    // if(paren > 0) 
    //     printf("unbalanced paren \n");
    }
    if(brace > 0) 
        printf("unbalanced braces \n");
    if(brack > 0) 
        printf("unbalanced bracks \n");
    if(paren > 0) 
        printf("unbalanced paren \n");
    return 0;
}

void search(int c){
   extern int brack,brace,paren;

    if(c == '{')
        ++brace;
    else if (c == '}')
        --brace;
    else if (c == '[')
        ++brack;
    else if (c == ']')
        --brack;
    else if (c == '(')
        ++paren;
    else if (c == ')')
        --paren;
}

void in_comment(void){
    int c, d;

    c = getchar();
    d = getchar();
    while( c != '*' || d != '/'){
        c = d;
        d = getchar();
    }
}

void in_quote(int c){
    int d;
    while((d = getchar()) != c){
        if(d == '\\')
            getchar();
    }
}
