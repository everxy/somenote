
/*
 * 两个多项式相加,用链表实现
 * */
struct poly{
    int     power;
    int     coeff;
    struct poly *next;
}

struct poly  *poly_add(poly *p1,poly *p2)
{
    struct poly *res;

    while(p1->next != NULL || p2->next != NULL) {
        if(p1->power > p2->power) {
            res->power = p1->power;
            res->coeff = p1->coeff;
            p1 = p1->next;
        } else if(p1->power == p2->power) {
            res->power = p1->power;
            res->coeff = p1->coeff + p2->coeff;
            p1 = p1->next;
            p2 = p2->next;
        } else {
            res->power = p2->power;
            res->coeff = p2->coeff;
            p2 = p2->next;
        }
    }
    return res;
}
