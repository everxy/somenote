#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
	int t = 10;
	if(argc == 2) {
		t = atoi(argv[1]);
	}
//	printf("%d\n", t);
	system("/var/cos/cvm/autostart.php &");
	sleep(t);
	system("/var/cos/cvm/autostart.php &");
	sleep(t);
	system("/var/cos/cvm/autostart.php &");
	return 0;
}
