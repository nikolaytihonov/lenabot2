#include "crash.h"
#include "dbg.h"
#include "bot.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#if __LINUX__
static void signal_handler(int sig,siginfo_t* info,void*)
{
	static char szBuf[4096] = {0};
	
	snprintf(szBuf,4096,
		"=== %s ===\n"
		"si_signo %d\n"
		"si_errno %d\n"
		"si_value.sival_ptr %p\n",
		(info->si_signo == SIGSEGV ? "CRASH" : "SIGNAL"),
		info->si_signo,
		info->si_errno,
		info->si_value.sival_ptr
	);
	
	BotError("%s\n",szBuf);
	BotSendMsg(szBuf,false);
	
	BotSendMsg("Прекращение работы бота. Сохранение..\n",false);
	BotError("Прекращение работы бота. Сохранение..\n",false);
	bot.Stop();
	exit(1);
}

void InstallCrashHandler()
{
	struct sigaction sa;
	
	memset(&sa,'\0',sizeof(sa));
	sigemptyset(&sa.sa_mask);
	
	sa.sa_flags = SA_SIGINFO|SA_NODEFER;
	sa.sa_sigaction = signal_handler;
	
	sigaction(SIGSEGV,&sa,NULL);
	sigaction(SIGILL,&sa,NULL);
	sigaction(SIGTERM,&sa,NULL);
}
#endif

void InstallCrashHandler()
{
}