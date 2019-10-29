#include "bot.h"
#include "event.h"
#include "crash.h"
#include <stdio.h>
#include <string.h>
#include <string>

#define CONFIG_FILE "config.json"

#include "command.h"
#include "dbg.h"

int main()
{
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
#endif
	try {
		InstallCrashHandler();
		bot.Start(CONFIG_FILE);
	} catch(VkConnException& c){
		printf("VkConnException - %s\n",c.what());
	} catch(VkApiException& api){
		printf("VkApiException - %s\n",api.what());
	} catch(std::exception& e){
		printf("std::exception - %s\n",e.what());
	}
	
	return 0;
}