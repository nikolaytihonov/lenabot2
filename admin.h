#ifndef __ADMIN_H
#define __ADMIN_H

#include <string>

#define IN_RANGE(c,a,b) (c >= a && c <= b)

typedef enum {
	StHelen = -1,
	Leader,
	HighAdherent,
	Adherent,
	User,
	Incorrect,
	Banned,
	LastPrivelege,
} privelege_t;

class IAdmin
{
public:
	virtual int GetPrivelege(int user_id) = 0;
	virtual void SetPrivelege(int user_id,int priv) = 0;
	virtual void Ban(int user_id,std::string reason = "") = 0;
	virtual void UnBan(int user_id) = 0;
	virtual std::string GetPrivelegeName(int priv) = 0;
	virtual void Kick(int user_id) = 0;
	
	virtual bool IsRoot(int user_id) = 0;
	virtual bool IsAdmin(int user_id) = 0;
	virtual bool IsAdherent(int user_id) = 0;
	virtual bool IsIncorrect(int user_id) = 0;
	virtual bool IsBanned(int user_id) = 0;
};

extern IAdmin* admin;

#endif