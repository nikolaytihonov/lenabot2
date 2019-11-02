#ifndef __BOT_H
#define __BOT_H

#include <string>
#include <string.h>
#include "json/json.h"
#include "vkapi.h"

#define BEGINS_WITH(text,with) (!memcmp(text,with,sizeof(with)-1))

class Bot : public IVkApiController
{
public:
	Bot() : m_bRunning(false),
		m_iMsgSent(0),
		m_iErrLongPoll(0)
	{}
	
	void Start(std::string config_file);
	void Stop();
	
	void Run();
	
	virtual void PrepareLongPoll(std::string server,std::string key,int ts);
	virtual bool PrepareLongPoll();
	void StartLongPoll();
	int GetMessageRandomId();
	void Send(int conv_id,std::string text,bool bAsync = true,int reply = 0);
	void SendText(int conv_id,std::string text,bool bAsync = true,int reply = 0); //Для большого текста
	void ProcessEvent(const json_value& event);
	void ProcessMessage(const VkMessage& msg);
		
	VkApi* GetAPI(){return m_pVk;}
	
	void SetLowPriority(bool bVal){m_bInLowPriority = bVal;}
	
	int GetMainConv(){return m_iMainConv;}
	const std::string& GetLogFile(){return m_LogFile;}
	int GetAdminUser(){return m_iAdminUser;}
	
	void OnRequestPreAdd(IVkRequest* req);
	
	virtual void OnLongPoll(json_value& val);
	virtual void OnError(const VkException& e);
	virtual void OnRequestFinished(IVkRequest* req);
	
	virtual void OnLongPollError(const VkException& e);
private:
	std::string m_Token;
	std::string m_LogFile;
	int m_iMainConv;
	int m_iAdminUser;
	VkApi* m_pVk;
	bool m_bRunning;
	bool m_bInLowPriority;
	
	int m_iMsgSent;
	int m_iErrLongPoll;
};

extern Bot bot;

#endif