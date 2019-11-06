#ifndef __BOT_H
#define __BOT_H

#include <string>
#include <string.h>
#include <vector>
#include "json/json.h"
#include "vkapi.h"

#define BEGINS_WITH(text,with) (!memcmp(text,with,sizeof(with)-1))

class Attachment
{
public:
	explicit Attachment(std::string attach)
		: m_Attach(attach)
	{}

	virtual std::string GetAttachment() const {return m_Attach;}
protected:
	std::string m_Attach;
};

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
	void LoadConversations();
	void AddConversation(int peer_id);
	bool IsValidConversation(int peer_id);
	int GetLocalId(int peer_id);
	void StartLongPoll();
	int GetMessageRandomId();
	void SendMessage(int peer_id,std::string text,bool bAsync = true,int reply = 0,
		const Attachment& attach = Attachment(""));
	void Send(int peer_id,std::string text,bool bAsync = true,int reply = 0,
		const Attachment& attach = Attachment("")); //Для большого текста
	void Send(convtype_t type,std::string text,bool bAsync = true,int reply = 0,
		const Attachment& attach = Attachment(""));
	void ProcessEvent(const json_value& event);
	void ProcessMessage(const VkMessage& msg);
		
	VkApi* GetAPI(){return m_pVk;}
	
	void SetLowPriority(bool bVal){m_bInLowPriority = bVal;}
	
	int GetMainConv(){return m_iMainConv;}
	const std::string& GetLogFile(){return m_LogFile;}
	int GetAdminUser(){return m_iAdminUser;}
	int GetMainGroup(){return m_iMainGroup;}
	int GetMirrorGroup(){return m_iMirrorGroup;}
	
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
	int m_iMainGroup;
	int m_iMirrorGroup;
	VkApi* m_pVk;
	bool m_bRunning;
	bool m_bInLowPriority;
	
	int m_iMsgSent;
	int m_iErrLongPoll;

	std::vector<conv_t> m_Conversations;
};

extern Bot bot;

#endif