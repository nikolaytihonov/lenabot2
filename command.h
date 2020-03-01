#ifndef __COMMAND_H
#define __COMMAND_H

#include <string>
#include <vector>
#include "vkrequest.h"

class Command
{
public:
	Command();

	std::string GetName() const {return Arg(0);}
	size_t ArgC() const {return m_Args.size();}
	std::string Arg(unsigned int i) const;
	std::string ArgS() const;

	bool HasFlag(std::string flag) const;

	bool Parse(std::string text);
protected:
	void ParseInternal(std::string text);
private:
	std::vector<std::string> m_Args;
	std::vector<std::string> m_Flags;
};

#endif
