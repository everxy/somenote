#include <stdio.h>

long Fib(int n);


int main(void)
{
    int n;

    printf("Enter a positive integer (q to quit): ");
    while(scanf("%d", &n) == 1)
    {
            printf("\nThe %dth number in the fibonacci numbers is %ld\n",n, Fib(n));
    }
    puts("\nDone!");

    return 0;
}

/*unsigned long Fib(int n)
{
    int a,b;
    a = b = 1;
    unsigned long t = 0;
    if(n <= 2)
        return (unsigned long)a;
    else {
        for(int i = 2;i < n;i++) {
            t = a + b;
            a = b;
            b = t;
        }
        return t;
    }
}*/

long Fib(int n)
{
    int a = 1;
    static long array[100];
    //long array[n + 1];
    if(n <= 1)
        return (long)a;
    else {
        if(array[n] != 0)
            return array[n];
        else
            array[n] = Fib(n-1) + Fib(n-2);
    }
    return array[n];
}


