#include "service.h"
#include <boost/algorithm/string.hpp>
#include "vkrequest.h"
#include "bot.h"
#include <boost/format.hpp>

LIST_ZERO_INIT(Service::s_List);

Service::Service(std::string name)
	: m_Name(name)
{
	list_add(&s_List,&m_Node,this);
}

bool Service::ProcessEvent(const json_value& event)
{
	const json_value& from = event[6];
	switch((int)event[0])
	{
		case 4:
			if(&from == &json_value_none) return true;
			return ProcessMessage(VkMessage(
				(int)event[1],
				(int)event[3],
				(int)from["from"],
				(int)event[4],
				std::string((const char*)event[5])));
			break;
	}
	return false;
}

bool Service::ProcessMessage(const VkMessage& msg)
{
	return false;
}

void ServiceSystem::Reply(std::string text)
{
	bot.Send(GetPeerId(),text,
		IsAsyncMode(),m_Msg.m_iMsgId);
}

bool ServiceSystem::IsUserAdmin(int user_id,int priv)
{
	return (admin->GetPrivelege(user_id) <= priv 
		|| user_id == bot.GetAdminUser());
}

bool ServiceSystem::IsUserAdmin()
{
	return IsUserAdmin(GetCommandUser().m_iUserId,HighAdherent);
}

bool ServiceSystem::CheckAdmin(int priv)
{
	if(!IsUserAdmin(GetCommandUser().m_iUserId,priv))
	{
		Reply("Недостаточный уровень прав.");
		return false;
	}
	return true;
}

bool ServiceSystem::CheckFwdUser()
{
	if(GetFwdUser() == 0)
	{
		Reply("Пользователь не найден. "
			"Чтобы указать пользователя, перешлите и "
			"ответьте на его сообщения этой командой");
		return false;
	}
	return true;
}

ServiceSystem services;

void ServiceSystem::LoadServices()
{
	LIST_ITER_BEGIN(Service::s_List)
		IService* svc = dynamic_cast<IService*>(
			(Service*)LIST_DATA(Service::s_List,item));
		svc->Load();
	LIST_ITER_END()
}

void ServiceSystem::SaveServices()
{
	LIST_ITER_BEGIN(Service::s_List)
		IService* svc = dynamic_cast<IService*>(
			(Service*)LIST_DATA(Service::s_List,item));
		svc->Save();
	LIST_ITER_END()
}

void ServiceSystem::ProcessEvent(const json_value& event)
{
	const json_value& from = event[6];
	//peer_id = (int)event[3]
	switch((int)event[0])
	{
		case 4:
			if(&from == &json_value_none) break;
			if(ParseMessage((int)event[1],(int)event[4],
				std::string((const char*)event[5]))) return;
			break;
	}

	LIST_ITER_BEGIN(Service::s_List)
		IService* svc = dynamic_cast<IService*>(
			(Service*)LIST_DATA(Service::s_List,item));
		if(svc->ProcessEvent(event)) break;
	LIST_ITER_END()
}

bool ServiceSystem::ParseMessage(int msg_id,int user_id,std::string text)
{
	Command cmd;
	boost::replace_all(text,"&quot;","\"");
	if(cmd.Parse(text) && admin->CanRunCommand(user_id))
	{
		bot.GetAPI()->RequestAsync(
			new VkCommandGetMessage(msg_id)
		);
		//m_Cmds[msg_id] = cmd;
		m_Cmds.insert(std::pair<int,Command>(msg_id,cmd));
		return true;
	}
	return false;
}

void ServiceSystem::FinishCommand(const VkMessage& msg)
{
	std::map<int,Command>::iterator it = m_Cmds.find(msg.m_iMsgId);
	if(it == m_Cmds.end()) return;
	m_Msg = msg;
	m_Cmd = it->second;
	m_Cmds.erase(it);

	m_bSync = false;
	if(m_Cmd.HasFlag("!sync") && CheckAdmin(Leader))
		m_bSync = true;

	LIST_ITER_BEGIN(Service::s_List)
		IService* svc = dynamic_cast<IService*>(
			(Service*)LIST_DATA(Service::s_List,item));
		try {
			m_Reply.str("");
			bool bOk = svc->ProcessCommand(m_Cmd);
			if(!m_Reply.str().empty()) Reply(m_Reply.str());
			m_Reply.str("");
			if(bOk) break;
		} catch(std::exception& e) {
			services.Reply(boost::str(
				boost::format("Во время выполнения команды %s произошло исключение: %s")
					% m_Cmd.GetName() % std::string(e.what())));
		}
	LIST_ITER_END()
}

int ServiceSystem::GetLocalId()
{
	return bot.GetLocalId(GetPeerId());
}

void ServiceSystem::Request(IVkRequest* req)
{
	if(IsAsyncMode()) bot.GetAPI()->RequestAsync(req);
	else
	{
		json_value* pVal;
		bot.GetAPI()->Request(req,&pVal);
		json_value_free(pVal);
	}
}
