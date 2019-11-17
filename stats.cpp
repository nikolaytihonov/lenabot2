#include "stats.h"
#include "dbg.h"

Stats stats;

Stats::Stats() : Service("Stats"),
	Event("stats",Time(60*60*5,0))
{
	m_iRequests = 0;
	m_iMessages = 0;
}

bool Stats::ProcessMessage(const VkMessage& msg)
{
	if(msg.m_Text == "смотритель#статистика")
	{
		std::stringstream stat;
		stat << "статистика ==\n"
			<< "Запросов (IVkRequest): " << m_iRequests << "\n"
			<< "Сообщений отправлено: " << m_iMessages << "\n";
		bot.Send(msg.m_iConvId,stat.str());
		return true;
	}
	return false;
}

void Stats::Fire()
{
	std::stringstream stat;
	stat << "статистика ==\n"
		<< "Запросов (IVkRequest): " << m_iRequests << "\n"
		<< "Сообщений отправлено: " << m_iMessages << "\n";
	BotSendMsg(stat.str().c_str());
	
	Event::Fire();
}

void Stats::CountRequest(IVkRequest* req)
{
	m_iRequests++;
	if(req->GetName() == "messages.send")
		m_iMessages++;
}