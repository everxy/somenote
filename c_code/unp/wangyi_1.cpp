#include <stdio.h>
#include <iostream>
#include <deque>
#include <string.h>
#include <stdlib.h>
using namespace std;

int main()
{
    char a[100000];
    int n;
    while(scanf("%d",&n) != EOF) {
        gets(a);
        std::deque<int>  c;
        int where = 1;
        int back  = 0;
        int front = 1;
        for(int i = 0; i < strlen(a); i++) {
            a[i] = atoi(&a[i]);
            if(where == front) {
                c.push_front(a[i]);
                where = back;
                continue;
            }
            if(where == back) {
                c.push_back(a[i]);
                where = front;
                continue;
            }
        }
        
        while (!c.empty())
  		{
    		cout << c.front()  << ' ';
    		c.pop_front();
 		}
    }
    
    return 0;
}