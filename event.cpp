#include "event.h"
#include <time.h>
#include "database.h"

LIST_ZERO_INIT(Event::s_List);

Event::Event(std::string name,Time interval)
	: m_Name(name),m_Interval(interval),
	//m_NextTime(Time::CurTime()),
	m_iType(IEvent::Timer)
{
	m_NextTime = Time::Add(Time::CurTime(),m_Interval);
	Register();
}

Event::Event(Time interval)
	: m_iType(IEvent::Timer)
{
	char szBuf[32];
	sprintf(szBuf,"Event#%p",this);
	m_Name = std::string(szBuf);
	m_Interval = interval;
	m_NextTime = Time::Add(Time::CurTime(),m_Interval);
	Register();
}

static Time AlignTime(int hour,int minute,int period = PER_DAY)
{
	time_t t = time(NULL);
	struct tm* tm = localtime(&t);
	int dMin;
	int aMin = (hour*60) + minute;
	int cMin = ((int)tm->tm_hour*60) + tm->tm_min;
	if(cMin >= aMin)
		dMin = period + aMin - cMin;
	else dMin = aMin - cMin;
	
	return Time(dMin*60,0);
}

//Align
Event::Event(std::string name,Time interval,
	int alignHour,int alignMinute)
	: m_Name(name),m_Interval(interval),
	m_iHour(alignHour),m_iMinute(alignMinute),
	m_iType(IEvent::AlignTimer)
{	
	m_NextTime = Time::CurTime()
		+ AlignTime(m_iHour,m_iMinute);
	
	Register();
}

//Event
Event::Event(std::string name,int hour,int minute,int period)
	: m_Name(name),
	m_iHour(hour),m_iMinute(minute),
	m_iPeriod(period),
	m_iType(IEvent::Event)
{	
	m_NextTime = Time::CurTime()
		+ AlignTime(m_iHour,m_iMinute,m_iPeriod);
	
	printf("m_NextTime %u %d\n",
		m_NextTime.m_Timestamp,
		m_NextTime.m_mlSec);
	
	Register();
}

/*
m_iHour = alignHour;
	m_iMinute = alignMinute;
	m_NextTime = Time::CurTime() + AlignTime(m_iHour,m_iMinute);
	
	//Align time
	m_bAlign = !m_bPlanned;
	
	printf("m_NextTime %u %d\n",
		m_NextTime.m_Timestamp,
		m_NextTime.m_mlSec);
*/

#include "dbg.h"

void Event::Fire()
{
	CalcNextTime();
	if(m_iType == IEvent::Temp)
	{
		Remove();
		delete this;
		return;
	}
}

void Event::CalcNextTime()
{
	Time curTime = Time::CurTime();
	switch(m_iType)
	{
		case IEvent::AlignTimer:
			m_iType = IEvent::Timer;
		case IEvent::Timer:
			m_NextTime = Time::Add(curTime,m_Interval);
			break;
		case IEvent::Event:
			m_NextTime = curTime 
				+ AlignTime(m_iHour,m_iMinute,m_iPeriod);
			break;
	}
	
	printf("m_NextTime (%d) %u %d\n",
		m_iType,
		m_NextTime.m_Timestamp,
		m_NextTime.m_mlSec);
}

void Event::Register()
{
	list_add(&s_List,&m_Node,this);
}

void Event::Remove()
{
	list_remove(&s_List,&m_Node);
}

static void* eventsystem_thread(void* arg)
{
	((EventSystem*)arg)->Run();
	return NULL;
}

void EventSystem::Start()
{
	m_bRunning = true;
	m_Thread = boost::thread(&EventSystem::Run,this);
}

void EventSystem::Stop()
{
	m_bRunning = false;
}

//DEADLOCKS!!!
void EventSystem::Run()
{
	while(m_bRunning)
	{
		try {
			boost::this_thread::sleep(
				boost::posix_time::seconds(1));
			
			LIST_ITER_BEGIN(Event::s_List)
				Event* event = (Event*)LIST_DATA(Event::s_List,item);
				Time curTime = Time::CurTime();
				/*printf("%s will fire in (NextTime %u %d) %u ms\n",
					event->GetName().c_str(),
					event->GetNextTime().m_Timestamp,
					event->GetNextTime().m_mlSec,
					Time::Measure(event->GetNextTime(),curTime));
				printf("CurTime %u %d\n",curTime.m_Timestamp,
					curTime.m_mlSec);*/
				if(curTime>=event->GetNextTime())
				{
					BotLog("Fire %s\n",event->GetName().c_str());
					event->Fire();
				}
			LIST_ITER_END()
		} catch(std::exception& e) {
			BotError("EventSystem exception %s\n",e.what());
		}
	}
}

Event* EventSystem::Find(std::string name)
{
	LIST_ITER_BEGIN(Event::s_List)
		Event* event = (Event*)LIST_DATA(Event::s_List,item);
		if(event->GetName() == name)
			return event;
	LIST_ITER_END()
	return NULL;
}

static int events_load(void*,int,char** argv,char**)
{
	Event* evt = events.Find(argv[0]);
	if(!evt) return 0;
	
	evt->m_iType = atoi(argv[1]);
	evt->m_NextTime.FromString(argv[2]);
	evt->m_Interval.FromString(argv[3]);
	evt->m_iHour = atoi(argv[4]);
	evt->m_iMinute = atoi(argv[5]);
	evt->m_iPeriod = atoi(argv[6]);
	
	if(evt->m_NextTime < Time::CurTime())
		evt->CalcNextTime();
	return 0;
}

void EventSystem::Load()
{
	if(!db.TableExists("events"))
	{
		BotLog("Таблицы events не существует!");
		db.Execute(
			"CREATE TABLE events (name PRIMARY KEY,"
			"type,nextTime,interval,hour,minute,period);");
	}
	db.Execute("SELECT * FROM events;",events_load);
}

#include "dbg.h"
#include <boost/format.hpp>

void EventSystem::Save()
{	
	LIST_ITER_BEGIN(Event::s_List)
		Event* event = (Event*)LIST_DATA(Event::s_List,item);
		if(event->GetType() == IEvent::Temp) continue;
		
		db.Execute(boost::str(
			boost::format("INSERT OR REPLACE INTO events (name,type,nextTime,interval,hour,minute,period)"
				" VALUES ('%s',%d,'%s','%s',%d,%d,%d);")
			% event->GetName() % event->GetType()
			% event->GetNextTime().ToString()
			% event->m_Interval.ToString()
			% event->m_iHour % event->m_iMinute
			% event->m_iPeriod
		));
	LIST_ITER_END()
}

EventSystem events;
