#ifndef __LETO_H
#define __LETO_H

#include <string>

class ILeto
{
public:
	virtual std::string GetRandomArt(std::string tag,int* group_id = NULL) = 0;
	virtual void Update() = 0;
};

extern ILeto* leto;

#endif
