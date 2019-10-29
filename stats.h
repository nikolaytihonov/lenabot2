#ifndef __STATS_H
#define __STATS_H

#include "vkapi.h"
#include "event.h"
#include "service.h"
#include "bot.h"

class Stats : public Service, public Event
{
public:
	Stats();
	
	virtual bool ProcessMessage(const VkMessage& msg);
	virtual void Fire();
	
	void CountRequest(IVkRequest* req);
private:
	int m_iRequests;
	int m_iMessages;
};

extern Stats stats;

#endif