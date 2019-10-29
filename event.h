#ifndef __EVENT_H
#define __EVENT_H

#include <time.h>
#include <boost/thread.hpp>
#include <string>
#include "vkapi.h"
#include "list.h"
#include "service.h"

#define PER_HOUR 60
#define PER_DAY 1440
#define PER_WEEK (PER_DAY*7)
#define PER_MONTH (PER_DAY*31)
#define PER_YEAR (PER_DAY*365)

class IEvent
{
public:
	enum {
		Timer,
		AlignTimer,
		Event,
		Temp,
	};

	virtual const std::string& GetName() const = 0;
	virtual Time GetNextTime() const = 0;
	virtual void Fire() = 0;
	virtual int GetType() const = 0;
};

class Event : public IEvent
{
public:
	Event(std::string name,Time interval);
	Event(Time interval);
	Event(std::string name,Time interval,
		int alignHour,int alignMinute); //align
	Event(std::string name,int hour,int minute,int period = PER_DAY); //planned
	
	virtual const std::string& GetName() const
	{return m_Name;}
	virtual Time GetNextTime() const
	{return m_NextTime;}
	virtual void Fire();
	virtual int GetType() const
	{return m_iType;}
	
	void CalcNextTime();
	void Register();
	void Remove();
	
	static list_head_t s_List;
private:
	list_node_t m_Node;
public:
	int m_iType;
	std::string m_Name;
	Time m_NextTime;
	Time m_Interval;
	int m_iHour;
	int m_iMinute;
	int m_iPeriod;
};

class EventSystem : public Service
{
public:
	EventSystem() : Service("EventSystem"),
		m_bRunning(false)
	{}

	void Start();
	void Stop();
	
	void Run();
	
	Event* Find(std::string name);
	
	virtual void Load();
	virtual void Save();
private:
	boost::thread m_Thread;
	bool m_bRunning;
};

extern EventSystem events;

#define DECLARE_TIMER(name,interval)			\
	class Timer_##name : public Event			\
	{											\
	public:										\
	Timer_##name() : Event(#name,interval){}
	
#define EVENT_FIRE_BEGIN() virtual void Fire()	\
	{
#define EVENT_FIRE_END()						\
	Event::Fire();								\
	}
#define END_TIMER(name)							\
	} evt_Timer_##name;
	
#define DECLARE_TIMER_ALIGN(name,interval,h,m)	\
	class Timer_##name : public Event			\
	{											\
	public:										\
	Timer_##name() : Event(#name,interval,h,m){}

#define DECLARE_EVENT(name,h,m,p)				\
	class Event_##name : public Event			\
	{											\
	public:										\
	Event_##name() : Event(#name,h,m,p){}
	
#endif