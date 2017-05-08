#include "apue/include/apue.3e"
#include <pthread.h>
#include <syslog.h>

sigset_t 		mask;
extern int already_running(void);

void
reread(void)
{

}

void *
thr_fn(void *arg)
{
	int err,signo;

	for(;;) {
		err = sigwait(&mask,&signo);
		if(err != 0) {
			syslog(LOG_ERR,"sigwait failed");
			exit(1);
		}

		switch(signo) {
			case SIGHUP:
				syslog(LOG_INFO,"re-reading file");
				reread();
				break;

			case SIGTERM:
				syslog(LOG_INFO,"GOT SIGTERM");
				exit(0);

			default:
				syslog(LOG_INFO,"unexpected %d\n",signo);
		}
	}

	return(0);
}

int main(int argc, char const *argv[])
{
	/* code */
	int err;
	pthread_t  tid;
	char *cmd;
	struct sigaction sa;

	if((cmd = strrchr(argv[0],'/')) == NULL)
		cmd = argv[0];
	else
		cmd++;

	daemonize(cmd);

	if(already_running()) {
		syslog(LOG_INFO,"daemon running");
		exit(1);
	}


	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sigemptyset(&sa.sa_mask,SIGHUP);
	sa.sa_flags = 0;
	if(sigaction(SIGTERM,&sa,NULL) < 0) {
		syslog(LOG_ERR,"can't catch SIGTERM: %s".strerror(errno));
		exit(1);
	}
	sa.sa_handler = sighup;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask,SIGTERM);
	sa.sa_flags = 0;
	if(sigaction(SIGHUP,&sa,NULL) < 0) {
		syslog(LOG_ERR,"can't catch SIGTERM: %s".strerror(errno))
		exit(1);
	}

	return 0;
}