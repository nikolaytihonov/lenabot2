#ifndef __DBG_H
#define __DBG_H

#include <string>

void BotSendMsg(const char* pStr,bool bAsync = true);
void BotLog(const char* pFmt,...);
void BotError(const char* pFmt,...);

std::string ConsoleReadLine(int uMax = 256);
void ConsoleWriteLine(std::string text,bool bError = false);

#endif