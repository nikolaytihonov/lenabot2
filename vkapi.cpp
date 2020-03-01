#include "vkapi.h"
#include <stdio.h>
#include <stdlib.h>
#include <iterator>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>
#include "stats.h"

#ifndef __linux__
int gettimeofday(struct timeval * tp, struct timezone * tzp)
{
    // Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
    // This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
    // until 00:00:00 January 1, 1970
    static const uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);

    SYSTEMTIME  system_time;
    FILETIME    file_time;
    uint64_t    time;

    GetSystemTime( &system_time );
    SystemTimeToFileTime( &system_time, &file_time );
    time =  ((uint64_t)file_time.dwLowDateTime )      ;
    time += ((uint64_t)file_time.dwHighDateTime) << 32;

    tp->tv_sec  = (long) ((time - EPOCH) / 10000000L);
    tp->tv_usec = (long) (system_time.wMilliseconds * 1000);
    return 0;
}
#else
#include <sys/time.h>
#endif

Time Time::CurTime()
{
	Time t;
	struct timeval tv;

	gettimeofday(&tv,NULL);
	t.m_Timestamp = tv.tv_sec;
	t.m_mlSec = tv.tv_usec/1000;

	return t;
}

void Time::WaitFor(Time t)
{
	//Time curTime = Time::CurTime();
	if(t <= Time::CurTime()) return;
	boost::this_thread::sleep(
		boost::posix_time::microsec(
		Time::Measure(t,CurTime())*1000));
}

uint64_t Time::Measure(const Time& a,const Time& b)
{
	Time t = a;
	t -= b;
	return ((uint64_t)t.m_Timestamp)*1000 + t.m_mlSec;
}

Time Time::Add(const Time& a,const Time& b)
{
	Time c;
	c.m_mlSec = a.m_mlSec + b.m_mlSec;
	c.m_Timestamp = a.m_Timestamp + b.m_Timestamp
		+ (c.m_mlSec/1000);
	c.m_mlSec %= 1000;
	return c;
}

std::string Time::ToString()
{
	return (boost::format("%u.%d") % m_Timestamp % m_mlSec).str();
}

void Time::FromString(const std::string& str)
{
	std::vector<std::string> nums;
	nums.reserve(2);
	boost::algorithm::split(nums,str,boost::is_any_of("."));
	m_Timestamp = boost::lexical_cast<unsigned long>(nums[0]);
	m_mlSec = boost::lexical_cast<unsigned int>(nums[1]);
}

const char* VkConnException::what() const throw()
{
	static std::string s_Err;
	s_Err = boost::str(
		boost::format("%s - CURL error %d")
			% m_pRequest->GetName()
			% m_Code);
	return s_Err.c_str();
}

VkApiException::VkApiException(boost::shared_ptr<IVkRequest> req,json_value* err)
	: VkException(req),m_pError(err,json_value_free)
{
	if(m_pRequest->IsLongPoll())
	{
		m_iError = (int)GetError()["failed"];
		m_Message = "";
	}
	else
	{
		json_value _err = GetError()["error"];
		m_iError = _err["error_code"];
		m_Message = std::string((const char*)_err["error_msg"]);
	}
}

const char* VkApiException::what() const throw()
{
	static std::string s_Err;
	s_Err = boost::str(
		boost::format("VK %s Error %d - %s")
			% std::string(m_pRequest->IsLongPoll() ? "Long Poll" : "API")
			% m_iError
			% m_Message);
	return s_Err.c_str();
}

void VkApiWorker::AddRequest(IVkRequest* req)
{
	m_Mutex.lock();
	m_Queue.push(req);
	m_Mutex.unlock();
}

void VkApiWorker::Start()
{
	m_bRunning = true;
	m_Thread = boost::thread(&VkApiWorker::Run,this);
}

void VkApiWorker::Stop()
{
	m_bRunning = false;
}

#include "dbg.h"

void VkApiWorker::Run()
{
	while(m_bRunning)
	{
		while(m_Queue.empty())
			boost::this_thread::sleep(boost::posix_time::microsec(VKAPI_SLEEP));
		try {
			if(!m_bRunning) break;
			m_Mutex.lock();
			IVkRequest* req = m_Queue.front();
			m_Queue.pop();
			m_Mutex.unlock();

			m_Api->DoRequest(req,NULL);
		} catch(VkConnException& c){
			BotError("VkConnException - %s\n",c.what());
			m_Api->OnError(c);
		} catch(VkApiException& api){
			BotError("VkApiException - %s\n",api.what());
			m_Api->OnError(api);
		} catch(std::exception& e){
			BotError("std::exception - %s\n",e.what());
		}
	}
	Exit();
}

void VkApiWorker::Exit()
{
	m_Api->OnWorkerExit();
}

void VkApiWorker::Dump(std::string& out)
{
	for(unsigned int i = 0; i < m_Queue.size(); i++)
	{
		IVkRequest* req = std::move(m_Queue.front());
		m_Queue.pop();
		out += boost::str(
			boost::format("запрос %p %s\n")
				% req % req->GetName());
		m_Queue.push(std::move(req));
	}
}

VkApi::VkApi(std::string token,IVkApiController* controller,
	float apiVersion)
	: m_Token(token),m_pController(controller),
	m_ApiVersion(apiVersion)
{
	for(int i = 0; i < MAX_WORKER; i++)
		m_pWorker[i] = NULL;
}

VkApi::~VkApi()
{
	for(int i = 0; i < MAX_WORKER; i++)
	{
		if(m_pWorker[i])
		{
			m_pWorker[i]->Stop();
			delete m_pWorker[i];
			m_pWorker[i] = NULL;
		}
	}
}

void VkApi::Request(IVkRequest* req,json_value** pVal)
{
	DoRequest(req,pVal);
}

void VkApi::RequestAsync(IVkRequest* req)
{
	int idx = (int)req->GetPriority();

	if(!m_pWorker[idx])
	{
		m_pWorker[idx] = new VkApiWorker(this);
		m_pWorker[idx]->Start();
	}

	req->SetAsync(true);
	m_pWorker[idx]->AddRequest(req);
}

static size_t write_string(void* contents, size_t size, size_t nmemb, void* s)
{
	size_t newLength = size*nmemb;
	try
	{
		((std::string*)s)->append((char*)contents, newLength);
	}
	catch(std::bad_alloc&)
	{
		return 0;
	}
    return newLength;
}

void VkApi::DoRequest(IVkRequest* pReq,json_value** pVal)
{
	boost::shared_ptr<IVkRequest> req(pReq);
	std::string result;

	req->Prepare(this);
	BotLog("DoRequest req %p %s pVal %p\n",req.get(),req->GetName().c_str(),pVal);

	m_Mutex.lock();
	stats.CountRequest(req.get());

	Time::WaitFor(m_NextRequest);
	m_NextRequest = Time::CurTime() + Time(334);
	m_Mutex.unlock();

	//pCurl = curl_easy_init();
	boost::shared_ptr<CURL> curl(curl_easy_init(),curl_easy_cleanup);
	req->PrepareCURL(curl.get());

	curl_easy_setopt(curl.get(),CURLOPT_WRITEFUNCTION,write_string);
	curl_easy_setopt(curl.get(),CURLOPT_WRITEDATA,&result);
#ifdef USE_TOR
	curl_easy_setopt(curl.get(),CURLOPT_PROXY,"socks5h://127.0.0.1:9050");
#endif

	CURLcode res = curl_easy_perform(curl.get());

	if(res != CURLE_OK)
	{
		throw VkConnException(req,res);
	}

	//создадим лонг полл ошибку
	/*if(req->IsLongPoll())
	{
		puts("FAKE EXCEPTION LONG POLL!\n");
		result = "{\"failed\":2}";
	}*/

	puts(result.c_str());

	json_value* pJson = json_parse(result.c_str(),result.size());
	if(!pJson) throw std::runtime_error("Bad JSON response");
	json_value& val = *pJson;

	if(val["error"].type != json_none || val["failed"].type != json_none)
	{
		throw VkApiException(req,pJson);
	}

	req->OnRequestFinished(val);
	if(m_pController)
		m_pController->OnRequestFinished(req.get());
	if(pVal) *pVal = &val;
}

void VkApi::OnWorkerExit()
{
	//delete m_pWorker;
	//m_pWorker = NULL;
	BotError("#!!! WORKER EXIT!\n");
}

void VkApi::OnError(const VkException& e)
{
	if(m_pController)
		m_pController->OnError(e);
	else BotError("[VkApi::OnError] m_pController == NULL!\n");
}
