#include "command.h"
#include "string.h"
#include <stdint.h>
#include <boost/algorithm/string/join.hpp>
#include "utf8/utf8.h"
#include "dbg.h"

Command::Command()
{
}

std::string Command::Arg(int i) const
{
	if(i < 0 || i >= m_Args.size()) return "";
	return m_Args[i];
}

std::string Command::ArgS() const
{
	return boost::algorithm::join(m_Args," ");
}

bool Command::HasFlag(std::string flag) const
{
	return std::find(m_Flags.begin(),m_Flags.end(),flag)
		!= m_Flags.end();
}

void Command::ParseInternal(std::string _text)
{
	std::vector<int> text;
	std::vector<int> buf;
	utf8::utf8to32(_text.begin(),_text.end(),std::back_inserter(text));
	std::vector<int>::iterator it = text.begin();
	int sep = ' ';
	bool is_flag = false;
	do {
		if(*it == sep)
		{
			std::string arg;
			utf8::utf32to8(buf.begin(),buf.end(),std::back_inserter(arg));
			if(!arg.empty()) (is_flag ? m_Flags : m_Args).push_back(arg);
			buf.clear();
			is_flag = false;
			if(sep == '"') sep = ' ';
		}
		else if(*it == '"') sep = '"';
		else if(*it == '!') is_flag = true;
		else buf.push_back(*it);
		it++;
		if(it == text.end())
		{
			std::string arg;
			utf8::utf32to8(buf.begin(),buf.end(),std::back_inserter(arg));
			if(!arg.empty()) (is_flag ? m_Flags : m_Args).push_back(arg);
		}
	} while(it != text.end());
}

bool Command::Parse(std::string text)
{
	if(text.empty()) return false;
	try {
		ParseInternal(text);
		if(ArgC() < 2) return false;
		if(Arg(0) != "Лена" && Arg(0) != "лена")
			return false;
		m_Args.erase(m_Args.begin());
		return true;
	} catch(std::exception& e) {
		BotError("Command::Parse %s\n",e.what());
	}
	return false;
}