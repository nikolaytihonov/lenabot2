#ifndef __VKAPI_H
#define __VKAPI_H

#include "vkrequest.h"
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <exception>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <curl/curl.h>
#include <stdint.h>
#include <time.h>
#include "json/json.h"
#include "vkapicontroller.h"

//#define USE_TOR
#define VKAPI_SLEEP 334000

class Time
{
public:
	Time() : m_Timestamp(0),m_mlSec(0)
	{}
	Time(unsigned int seconds,uint16_t mlSec)
		: m_Timestamp(seconds),m_mlSec(mlSec)
	{}
	explicit Time(unsigned int ms)
		: m_Timestamp(ms/1000),m_mlSec(ms%1000)
	{}
	
	inline bool operator>(const Time& other)
	{
		if(this->m_Timestamp > other.m_Timestamp) return true;
		return ((this->m_Timestamp >= other.m_Timestamp)
			&& this->m_mlSec > other.m_mlSec);
	}
	
	inline bool operator>=(const Time& other)
	{
		if(this->m_Timestamp >= other.m_Timestamp) return true;
		return ((this->m_Timestamp >= other.m_Timestamp)
			&& this->m_mlSec >= other.m_mlSec);
	}
	
	inline bool operator<(const Time& other)
	{
		if(this->m_Timestamp < other.m_Timestamp) return true;
		return ((this->m_Timestamp <= other.m_Timestamp)
			&& this->m_mlSec < other.m_mlSec);
	}
	
	inline bool operator<=(const Time& other)
	{
		if(this->m_Timestamp <= other.m_Timestamp) return true;
		return ((this->m_Timestamp <= other.m_Timestamp)
			&& this->m_mlSec <= other.m_mlSec);
	}
	
	inline Time operator+(const Time& other)
	{
		Time t;
		t.m_mlSec = this->m_mlSec + other.m_mlSec;
		t.m_Timestamp = this->m_Timestamp + other.m_Timestamp
			+ (other.m_mlSec/1000);
		t.m_mlSec %= 1000;
		return t;
	}
	
	inline Time& operator+=(const Time& other)
	{
		this->m_mlSec += other.m_mlSec;
		this->m_Timestamp += other.m_Timestamp +
			(this->m_mlSec/1000);
		this->m_mlSec %= 1000;
		return *this;
	}
	
	inline Time operator-=(const Time& b)
	{
		this->m_Timestamp -= b.m_Timestamp;
		//this->m_mlSec -= b.m_mlSec;
		this->m_mlSec = (b.m_mlSec >= this->m_mlSec) ? 
			(b.m_mlSec - this->m_mlSec) 
			: (this->m_mlSec - b.m_mlSec);
		return *this;
	}
	
	std::string ToString();
	void FromString(const std::string& str);
	
	static Time CurTime();
	static void WaitFor(Time t);
	static uint64_t Measure(const Time& a,const Time& b);
	static Time Add(const Time& a,const Time& b);
	
	time_t m_Timestamp;
	int m_mlSec;
};

class VkException : public std::exception
{
public:
	VkException(boost::shared_ptr<IVkRequest> req)
		: m_pRequest(req){}
	
	virtual const json_value& GetError() const {return json_value_none;}
	virtual int GetErrorCode() const {return 0;}
	virtual bool IsConnectionError() const {return false;}
	virtual bool IsApiError() const {return false;}
	virtual bool IsLongPollError() const {return m_pRequest->IsLongPoll();}
	virtual IVkRequest* GetRequest() const {return m_pRequest.get();}
	
	boost::shared_ptr<IVkRequest> m_pRequest;
};

class VkConnException : public VkException
{
public:
	VkConnException(boost::shared_ptr<IVkRequest> req,CURLcode code)
		: VkException(req),m_Code(code)
	{}
	
	virtual const char* what() const throw();
	virtual int GetErrorCode() const {return m_Code;}
	virtual bool IsConnectionError() const {return true;}
	
	CURLcode m_Code;
};

class VkApiException : public VkException
{
public:
	VkApiException(boost::shared_ptr<IVkRequest> req,json_value* err);
	
	virtual const json_value& GetError() const {return *m_pError.get();}
	virtual int GetErrorCode() const {return m_iError;}
	virtual bool IsApiError() const {return true;}
	const std::string& GetMessage() const {return m_Message;}
	
	virtual const char* what() const throw();
	
	//json_value* m_pError;
	boost::shared_ptr<json_value> m_pError;
	int m_iError;
	std::string m_Message;
};

//простой request - выполнить и подождать 0.3 сек
//chained request - запрос, за которым строго следует другой

//очередь из request - выполнять из очереди, асинхронно ожидая
//0.3 передавая управление в Think

class IVkRequest;

class VkApiWorker
{
public:
	VkApiWorker(VkApi* api)
		: m_Api(api),m_bRunning(false)
	{}
	
	void Start();
	void Stop();
	void AddRequest(IVkRequest* req);
	
	void Run();
	void Exit();
	
	void Dump(std::string& out);
private:
	VkApi* m_Api;
	boost::thread m_Thread;
	boost::mutex m_Mutex;
	std::queue<IVkRequest*> m_Queue;
	bool m_bRunning;
};

typedef struct {
	struct curl_httppost* m_Form;
	struct curl_slist* m_Header;
} curl_free_t;

#define MAX_WORKER IVkRequest::Last

class VkApi
{
public:
	VkApi(std::string token,IVkApiController* controller,
		float apiVersion = 5.101);
	~VkApi();
	
	void Request(IVkRequest* req,json_value** pVal);
	void RequestAsync(IVkRequest* req);
	void DoRequest(IVkRequest* req,json_value** pVal);
	
	virtual void OnWorkerExit();
	
	virtual void OnError(const VkException& e);
	
	const std::string& GetToken(){return m_Token;}
	const float GetAPIVersion(){return m_ApiVersion;}
	VkApiWorker* GetWorker(int i){return m_pWorker[i];}
private:
	std::string m_Token;
	float m_ApiVersion;
	VkApiWorker* m_pWorker[MAX_WORKER];
	Time m_NextRequest;
	boost::mutex m_Mutex;
	IVkApiController* m_pController;
};

#define _VkRequest(out,api,method,name1,val1)	\
	{											\
		VkRequest* req = new VkRequest(method);	\
		req->SetParam(name1,val1);				\
		out = api.Request(req);					\
	}

#endif