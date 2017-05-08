#include <stdio.h>
#define MAXLINE 1000
int getline(char line[],int maxline);
void reverse(char s[]);

int main(){
	char line[MAXLINE];
	int i = 0;

	while((i = getline(line,MAXLINE)) > 0) {
		reverse(line);
		printf("%d\n", i);
		printf("%s\n", line);
	}
	return 0;
}

void reverse(char s[])
{
	int i,j;
	char temp;

	i = 0;
	while(s[i] != '\0')
		++i;
	--i;
	if(s[i] == '\n'){
		--i;
	}
	j = 0;
	while(j < i) {
		temp = s[j];
		s[j] = s[i];
		s[i] = temp;
		--i;
		++j;
	}
}
// 读取一行字符串，返回其长度
int getline(char line[],int lim){
	int i,j,c;
	i = 0;
	for (i = 0; (c = getchar()) != EOF && c != '\n'; ++i)
	{
		if(i < lim - 2){
			line[j] = c;
			j++;
		}

	}
	if( c == '\n'){
		line[j] = c;
		i++;
		j++;
	}
	line[j] = '\0';
	return i;
}