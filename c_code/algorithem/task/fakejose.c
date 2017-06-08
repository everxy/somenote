/*理解有误，该解法不是约瑟夫环*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
struct dl {
    int     value;
    struct dl *next;
    struct dl *pre;
};

void josep(int m,int n);

int main(int argc, char* argv[])
{
    int m,n;
    //scanf("%d,%d",&m,&n);
    m = *argv[1] - '0';
    n = *argv[2] - '0';
    printf("m=%d,n=%d\n",m,n);
    josep(m,n);
    return 0;
}

void josep(int m,int n)
{
    struct dl *p,*l,*temp,*del,*dp;
    int t = m;
    p = (struct dl *)malloc(sizeof(struct dl));
    p->value = 0;
    p->next = NULL;
    l = p;
    for(int i = 1;i <= n; i++) {
        temp = (struct dl *)malloc(sizeof(struct dl));
        temp->value = i;
        temp->next = NULL;
        temp->pre = NULL;
        p->next = temp;
        temp->pre = p;
        p = p->next;
    }
    l->next->pre = p;
    p->next = l->next;
    p = l->next;
    while((n -= t) > 0) {
        temp = p;
        m = t;
        p = p->next;
        while(m-- > 0) {
            printf("del:%d\t",p->value);
            del = p;
            p = p->next;
            p->pre = del->pre;
            del->pre->next = p;
            free(del);
            printf("the p :%d\n",p->value);
        }
        dp = p;
        for(int j = 0;j < 11 ;j++){
            printf("->%d",dp->value);
            dp = dp->next;
        }
    }
}
