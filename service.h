#ifndef __SERVICE_H
#define __SERVICE_H

#include <string.h>
#include <string>
#include <sstream>
#include <map>
#include "vkapi.h"
#include "list.h"
#include "json/json.h"
#include "command.h"
#include "admin.h"

class IService
{
public:
	virtual const std::string& GetName() const = 0;
	virtual void Load() = 0;
	virtual void Save() = 0;
	virtual bool ProcessEvent(const json_value& event) = 0;
	virtual bool ProcessMessage(const VkMessage& msg) = 0;
	virtual bool ProcessCommand(const Command& cmd) = 0;
};

class Service : public IService
{
public:
	Service(std::string name);
	
	virtual const std::string& GetName() const
	{return m_Name;}
	virtual void Load(){}
	virtual void Save(){}
	virtual bool ProcessEvent(const json_value& event);
	virtual bool ProcessMessage(const VkMessage& msg);
	virtual bool ProcessCommand(const Command& cmd){return false;}

private:
	list_node_t m_Node;
	std::string m_Name;
public:
	static list_head_t s_List;
};

class ServiceSystem
{
public:
	void LoadServices();
	void SaveServices();
	void ProcessEvent(const json_value& event);
	bool ParseMessage(int msg_id,int user_id,std::string text);
	void FinishCommand(const VkMessage& msg);

	const VkMessage& GetCommandUser(){return m_Msg;}
	int GetPeerId(){return GetCommandUser().m_iConvId;}
	int GetLocalId();
	bool IsUserAdmin(int user_id,int priv = HighAdherent);
	bool IsUserAdmin();
	bool IsAsyncMode(){return !m_bSync;}
	void Reply(std::string text);
	bool CheckAdmin(int priv = HighAdherent);
	inline int GetFwdUser(){return GetCommandUser().m_iFwdUser;}
	bool CheckFwdUser();

	void Request(IVkRequest* req);

	template<typename T>
	inline ServiceSystem& operator<<(const T& obj)
	{
		m_Reply << obj;
		return *this;
	}
private:
	std::map<int,Command> m_Cmds;
	Command m_Cmd;
	VkMessage m_Msg;
	bool m_bSync;
	std::stringstream m_Reply;
};

extern ServiceSystem services;

#endif