#include "dbg.h"
#include "bot.h"
#include "vkapi.h"
#include "event.h"
#include "json/json.h"
#include <iostream>
#include <boost/thread/mutex.hpp>
#include <fstream>
#include <stdarg.h>
#include <string>
#include <vector>
#include <time.h>

void BotSendMsg(const char* pStr,bool bAsync)
{
	//g_pBot->Send(2000000057,std::string(pStr));
	//const int conv_id = 2000000057;

	BotLog("BotSendMsg %s\n",pStr);
	bot.SetLowPriority(true);
	bot.Send(bot.GetMainConv(),pStr,bAsync);
}

//Log Timer
/*DECLARE_TIMER(LogTimer,Time(15,0))
	std::vector<std::string> m_Log;
EVENT_FIRE_BEGIN()
	if(!m_Log.empty())
	{
		char* pLog = new char[4096];
		pLog[0] = '\0';
		for(std::string& s : m_Log)
			strncat(pLog,s.c_str(),4096);

		bot.Send(bot.GetLogConv(),pLog);
		delete[] pLog;

		m_Log.clear();
	}
EVENT_FIRE_END()

END_TIMER(LogTimer)*/

static boost::mutex s_Mutex;

void BotLog(const char* pFmt,...)
{
	//На линукс хосте нам не нужны логи, пока-что.
#ifndef __linux__
	char szBuf[1024] = {0};
	va_list ap;

	va_start(ap,pFmt);
	vsnprintf(szBuf,1024,pFmt,ap);
	va_end(ap);

	//evt_Timer_LogTimer.m_Log.push_back(std::string(szBuf));
	char szTime[32];
	time_t t = time(NULL);
	struct tm* tm = localtime(&t);
	strftime(szTime,32,"%x-%X",tm);

	s_Mutex.lock();
	std::ofstream log(bot.GetLogFile(),std::ios::out|std::ios::app);
	log << '[' << szTime << "] " << szBuf;
	log.close();

	std::cout << '[' << szTime << "] ";
	ConsoleWriteLine(szBuf);
	s_Mutex.unlock();
#endif
}

void BotError(const char* pFmt,...)
{
	char szBuf[1024] = {0};
	va_list ap;

	va_start(ap,pFmt);
	vsnprintf(szBuf,1024,pFmt,ap);
	va_end(ap);

	//evt_Timer_LogTimer.m_Log.push_back(std::string(szBuf));
	char szTime[32];
	time_t t = time(NULL);
	struct tm* tm = localtime(&t);
	strftime(szTime,32,"%x-%X",tm);

	s_Mutex.lock();
	std::ofstream log(bot.GetLogFile(),std::ios::out|std::ios::app);
	log << '[' << szTime << "] [ERROR] " << szBuf;
	log.close();

	std::cerr << '[' << szTime << "] [ERROR] ";
	ConsoleWriteLine(szBuf,true);
	s_Mutex.unlock();
}

std::string ConsoleReadLine(int uMax)
{
	std::string in;
#ifndef __linux__
	DWORD dwIn;
	wchar_t* pInput = new wchar_t[uMax];
	ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE),pInput,uMax,&dwIn,NULL);
	dwIn -= 2; //remove \r and \n
	int uLen = WideCharToMultiByte(CP_UTF8,0,pInput,dwIn,NULL,0,NULL,NULL);
	char* pBuf = new char[uLen+1];
	WideCharToMultiByte(CP_UTF8,0,pInput,dwIn,pBuf,uLen,NULL,NULL);
	pBuf[uLen] = '\0';
	in = std::string(pBuf);
	delete[] pInput;
	delete[] pBuf;
#else
	std::getline(std::cin,in);
#endif
	return in;
}

void ConsoleWriteLine(std::string text,bool bError)
{
#ifndef __linux__
	DWORD w = 0;
	int uLen = MultiByteToWideChar(CP_UTF8,0,text.c_str(),(int)text.size(),NULL,0);
	wchar_t* pBuf = new wchar_t[uLen];
	MultiByteToWideChar(CP_UTF8,0,text.c_str(),(int)text.size(),pBuf,uLen);
	WriteConsoleW(GetStdHandle(bError ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE),
		pBuf,uLen,&w,NULL);
	delete[] pBuf;
#else
	std::cout << text;
#endif
}
