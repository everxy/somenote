
/*
 * 两个多项式相乘,用链表实现，需要保证幂的顺序
 * 注意用链表需要合并同类项
 * */
struct poly{
    int     power;
    int     coeff;
    struct poly *next;
}
/*方法a：先每个项相乘，计算每一个项的时候要检查幂的大小，然后合并同类项
 *
 * */
struct poly *multi(poly *p1,poly *p2)
{
    struct poly *res;
    struct poly *temp,*pres;
    res->next = pres = NULL;
    temp->next = NULL;
    while(p1 != NULL) {
        while(p2 != NULL) {
            temp = (struct *)malloc(sizeof(poly));
            temp.power = p1.power + p2.power;
            temp.coeff = p1.coeff * p2.coeff;
            temp->next = NULL;
            if(pres == NULL) {
                pres = temp;
            } else if(pres->power >= temp->power){
                while(pres->next != NULL && pres->next->power > temp.power) {
                        pres = pres->next;
                }
                if(pres->next == NULL)
                    pres->next = temp;
                if(pres.next.power < temp.power) {
                    temp.next = temp.next;
                    pres.next = temp.next;
                } else if(pres.next.power = pres.next.power) {
                    pres.next.coeff += temp.coeff;
                }

                pres = res.next;
            } else {
                temp.next = pres;
                res.next = temp;
                pres = temp;
            }
            p2 = p2.next;
        }
        p1 = p1.next;
    }
}


