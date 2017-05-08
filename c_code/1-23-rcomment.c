// 去掉c语言中的注释语句（/**/），注释不允许嵌套
// 
#include <stdio.h>

void rcomment(int c);
void in_comment(void);
void echo_quote(int c);

int main(){
	int c,d;

	while((c = getchar()) != EOF)
		rcomment(c);
	return 0;
}
// 搜索起始标志
void rcomment(int c){
	int d;

	if(c == '/'){
		if((d = getchar()) == '*')
			// 搜索结束标志
			in_comment();
		else if(d == '/'){
			putchar(c);
			rcomment(d);
		} else {
			putchar(c);
			putchar(d);
		}
	} 
	// 当碰到单双引号时保证其不会被误认为是注释
	else if(  c == '\'' || c == '"')
		echo_quote(c);
	else
		putchar(c);
}

void in_comment(void){
	int c,d;

	c = getchar(); // prev character
	d = getchar(); // curr character
	while( c != '*' || d != '/'){
		c = d;
		d = getchar();
	}
}

void echo_quote(int c){
	int d;

	putchar(c);
	while((d = getchar()) != c){
		putchar(d);
		if(d == '\\') // 忽略反斜杠后的引号
			putchar(getchar());
	}
	putchar(d);
}