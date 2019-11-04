#include "bot.h"
#include <stdint.h>
#include <fstream>
#include <algorithm>
#include <boost/thread.hpp>
#include <boost/algorithm/minmax.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include "event.h"
#include "service.h"
#include "database.h"
#include "utf8/utf8.h"
#include "dbg.h"

Bot bot;

void Bot::Start(std::string config_file)
{
	//Загрузить конфиг
	boost::filesystem::path p(config_file);
	if(!boost::filesystem::exists(p))
	{
		BotError("config.json not found!\n");
		return;
	}

	std::string json;
	std::ifstream config(config_file,std::ios::in);
	//Читаем через getline построчно, т.к он убирает любые переносы LF / (CR LF)
	//Потом сами добавляем нужный перенос LF. От (CR LF) json_parse ломается
	while(!config.eof())
	{
		std::string line;
		std::getline(config,line);
		json += line + '\n';
	}
	config.close();

	json_value* pConf = json_parse(json.c_str(),json.size());
	json_value& conf = *pConf;
	if(!pConf) throw std::exception("Bad config JSON");

	m_Token = std::string((const char*)conf["token"]);
	m_LogFile = std::string((const char*)conf["logfile"]);
	m_iMainConv = (int)conf["mainconv"];
	m_iAdminUser = (int)conf["adminuser"];

	json_value_free(pConf);

	//Запуск CURL и VkApi
	curl_global_init(CURL_GLOBAL_ALL);
	
	m_pVk = new VkApi(m_Token,this);
	
	//Загрузка бесед
	AddConversation(GetMainConv());
	LoadConversations();

	//Запуск всех остальных систем
	db.Open();
	services.LoadServices();
	events.Start();
	
	m_bRunning = true;
	PrepareLongPoll();
	
	Run();
}

void Bot::Stop()
{
	m_bRunning = false;
	//Остановка всех систем
	events.Stop();
	services.SaveServices();
	db.Close();
	
	delete m_pVk;
	m_pVk = NULL;
	curl_global_cleanup();
}

void Bot::Run()
{
	while(m_bRunning)
	{
		ProcessMessage(VkMessage(-1,GetMainConv(),-1,
			time(NULL),ConsoleReadLine()));
	}
}

void Bot::LoadConversations()
{
	int offset = 0;
	int count = 0;
	
	try {
		do {
			json_value* pVal;

			VkRequest* get = new VkRequest("messages.getConversations");
			get->SetParam("offset",offset);
			get->SetParam("count",200);
			m_pVk->Request(get,&pVal);
			{
				boost::shared_ptr<json_value> val(pVal,json_value_free);
				auto& res = (*val)["response"];
				auto& items = res["items"];
				count = (int)res["count"];

				if(items.type != json_array)
					throw std::exception("items.type != json_array");
				for(int i = 0; i < items.u.array.length; i++)
				{
					auto& conv = items[i]["conversation"];
					auto& peer = conv["peer"];
					if(peer.type == json_none)
						throw std::exception("peer.type == json_none");
					conv_t cv;
					cv.id = (int)peer["id"];
					cv.local_id = (int)peer["local_id"];
					std::string type = std::string((const char*)peer["type"]);
					if(IsValidConversation(cv.id)) continue;
					if(cv.id == GetMainConv()) cv.type = ConvMainChat;
					else if(type == "user") cv.type = ConvUser;
					else if(type == "chat") cv.type = ConvChat;
					else if(type == "group") cv.type = ConvGroup;
					else if(type == "email") cv.type = ConvEmail;
					m_Conversations.push_back(cv);
				}

				offset += items.u.array.length;
			}
		} while(offset < count);
	} catch(std::exception& e) {
		Send(ConvMainChat,boost::str(
			boost::format("Ошибка загрузки бесед: %s")
				% std::string(e.what())),false);
	}
}

void Bot::AddConversation(int peer_id)
{
	try {
		json_value* pVal;
		VkRequest* get = new VkRequest("messages.getConversationsById");
		get->SetParam("peer_ids",peer_id);
		m_pVk->Request(get,&pVal);

		conv_t cv;
		auto& res = (*pVal)["response"];
		auto& peer = res["items"][0]["peer"];
		cv.id = (int)peer["id"];
		cv.local_id = (int)peer["local_id"];
		std::string type = std::string((const char*)peer["type"]);
		if(cv.id == GetMainConv()) cv.type = ConvMainChat;
		else if(type == "user") cv.type = ConvUser;
		else if(type == "chat") cv.type = ConvChat;
		else if(type == "group") cv.type = ConvGroup;
		else if(type == "email") cv.type = ConvEmail;
		m_Conversations.push_back(cv);

		json_value_free(pVal);
	} catch(std::exception& e) {
		Send(ConvMainChat,boost::str(
			boost::format("Ошибка загрузка беседы %d: %s")
				% peer_id % std::string(e.what())),false);
	}
}

bool Bot::IsValidConversation(int peer_id)
{
	return std::find(m_Conversations.begin(),
		m_Conversations.end(),peer_id) != m_Conversations.end();
}

int Bot::GetLocalId(int peer_id)
{
	auto it = std::find(m_Conversations.begin(),
		m_Conversations.end(),peer_id);
	if(it == m_Conversations.end())
	{
		AddConversation(peer_id);
		it = std::find(m_Conversations.begin(),
			m_Conversations.end(),peer_id);
		if(it == m_Conversations.end())
			return -1;
	}
	return it->local_id;
}

void Bot::PrepareLongPoll(std::string server,std::string key,int ts)
{
	VkLongPoll* poll = new VkLongPoll(server,key,ts,this);
	m_pVk->RequestAsync(poll);
}

bool Bot::PrepareLongPoll()
{
	BotLog("%s\n",__FUNCTION__);
	try {
		VkRequest* req = new VkRequest("messages.getLongPollServer");
		req->SetParam("lp_version","3");
		
		json_value* pVal;
		m_pVk->Request(req,&pVal);
		boost::shared_ptr<json_value> val(pVal,json_value_free);
	
		const json_value& res = (*val)["response"];
		PrepareLongPoll(
			std::string((const char*)res["server"]),
			std::string((const char*)res["key"]),
			(int)res["ts"]);
	} catch(std::exception& e) {
		BotError("[PrepareLongPoll] Exception %s\n",e.what());
		return false;
	}
	return true;
}

void Bot::StartLongPoll()
{
	while(!PrepareLongPoll())
	{
		boost::this_thread::sleep(
			boost::posix_time::minutes(1));
	}
}

int Bot::GetMessageRandomId()
{
	return (int)time(NULL)+m_iMsgSent++;
}

void Bot::SendMessage(int peer_id,std::string text,bool bAsync,int reply,std::string attach)
{
	json_value* val;
	BotLog("%s %d\n",__FUNCTION__,peer_id);
	VkRequest* msg = new VkRequest("messages.send");
	msg->SetParam("peer_id",peer_id);
	msg->SetParam("random_id",(int)GetMessageRandomId());
	if(reply) msg->SetParam("reply_to",reply);
	if(!attach.empty()) msg->SetParam("attachment",attach);
	msg->AddMultipart(VkPostMultipart("message",
		text,VkPostMultipart::Text));
	OnRequestPreAdd(msg);
	if(bAsync) m_pVk->RequestAsync(msg);
	else
	{
		m_pVk->Request(msg,&val);
		json_value_free(val);
	}
}

void Bot::Send(int peer_id,std::string text,bool bAsync,int reply,std::string attach)
{
	int len = utf8::distance(text.begin(),text.end());
	if(len <= 4096)
	{
		SendMessage(peer_id,text,bAsync,reply,attach);
		return;
	}
	
	std::vector<uint32_t> text32;
	utf8::utf8to32(text.begin(),text.end(),std::back_inserter(text32));
	int pos = 0;
	while(len > 0)
	{
		std::string block;
		int d = boost::minmax<int>(4096,len).get<0>();
		len -= d;
		utf8::utf32to8(text32.begin()+pos,text32.begin()+pos+d,std::back_inserter(block));
		SendMessage(peer_id,block,bAsync,reply,len > 0 ? "" : attach);
		pos += d;
	}
}

void Bot::Send(convtype_t type,std::string text,bool bAsync,int reply,std::string attach)
{
	for(auto it = m_Conversations.begin(); 
		it != m_Conversations.end(); ++it)
	{
		if(it->type == type || (type == ConvChat && it->type == ConvMainChat))
			Send(it->id,text,bAsync,reply,attach);
	}
}

void Bot::ProcessEvent(const json_value& event)
{
	services.ProcessEvent(event);
	
	auto& from = event[6];
	int peer_id;
	switch((int)event[0])
	{
		case 4:
			if(&from == &json_value_none) return;
			int peer_id = (int)event[3];
			if(!IsValidConversation(peer_id))
				AddConversation(peer_id);
			ProcessMessage(VkMessage(
				(int)event[1],
				peer_id,
				(int)from["from"],
				(int)event[4],
				std::string((const char*)event[5])));
			break;
	}
	//#4
	//msg_id = event[1]
	//conv_id = event[3]
	//timestamp = event[4]
	//text = event[5]
}

void Bot::ProcessMessage(const VkMessage& msg)
{
	//strcasestr
	BotLog("%s \"%s\"\n",__FUNCTION__,msg.m_Text.c_str());
	//if(strcasestr(msg.m_Text.c_str(),"Лена"))
	//	Send(msg.m_iConvId,"Слава Св.Елене!");
	if(msg.m_Text == "смотритель#очередь")
	{
		std::string dump = "";
		char szLine[256];
		for(int i = 0; i < 3; i++)
		{
			sprintf(szLine,"Очередь приоритета %d (%p)\n\n",i,m_pVk->GetWorker(i));
			dump += szLine;
			if(m_pVk->GetWorker(i))
				m_pVk->GetWorker(i)->Dump(dump);
		}
		
		Send(msg.m_iConvId,std::string(dump),false);
	}
	else if(msg.m_Text == "смотритель#сохранить_сервисы")
	{
		Send(msg.m_iConvId,"Сохраняю сервисы..");
		services.SaveServices();
	}
	else if(msg.m_Text == "смотритель#throw_exception")
		throw std::runtime_error("письку ебал");
	else if(msg.m_Text == "смотритель#фото")
	{
		VkUploadPhotoChat* photo = new VkUploadPhotoChat(msg.m_iConvId,"");
		photo->AddPhoto("lena1.jpg");
		photo->AddPhoto("lena2.jpg");
		m_pVk->RequestAsync(photo);
	}
	else if(msg.m_Text == "смотритель#спам")
	{
		for(int i = 0; i < 32; i++)
			Send(msg.m_iConvId,"спам_сообщение_проверка_14");
	}
	else if(msg.m_Text == "лена10")
	{
		for(int i = 0; i < 10; i++)
		{
			ProcessMessage(VkMessage(msg.m_iMsgId,msg.m_iConvId,msg.m_iUserId,time(NULL)+i,"смотритель#фото"));
		}
	}
	else if(msg.m_Text == "лена256")
	{
		for(int i = 0; i < 256; i++)
		{
			VkUploadPhotoChat* photo = new VkUploadPhotoChat(msg.m_iConvId,"");
			photo->AddPhoto("lena1.jpg");
			m_pVk->RequestAsync(photo);
		}
	}
	//else if(!strcmp(msg.m_Text.c_str(),"смотритель#выпилиться"))
	//	*(long*)0x1 = 2;
}

void Bot::OnRequestPreAdd(IVkRequest* req)
{
	if(m_bInLowPriority)
	{
		req->SetPriority(IVkRequest::Other);
		m_bInLowPriority = false;
	}
}

void Bot::OnLongPoll(json_value& val)
{
	try {
		BotLog("%s\n",__FUNCTION__);
		const json_value& updates = val["updates"];
		for(unsigned int i = 0; i < updates.u.array.length; i++)
		{
			const json_value& update = updates[i];
			printf("update %d\n",(int)update[0]);
			ProcessEvent(update);
		}
	} catch(VkConnException& c){
		BotError("[OnLongPoll] VkConnException - %s\n",c.what());
		OnError(c);
	} catch(VkApiException& api){
		BotError("[OnLongPoll] VkApiException - %s\n",api.what());
		OnError(api);
	} catch(std::exception& e){
		BotError("[OnLongPoll] std::exception - %s\n",e.what());
	}
}

void Bot::OnError(const VkException& e)
{
	if(e.IsApiError())
	{
		switch(e.GetErrorCode())
		{
			case 14:
				BotError("[OnVkError] Captcha needed. Slow down\n");
				boost::this_thread::sleep(
					boost::posix_time::minutes(1));
				break;
		}
	}
	else if(e.IsConnectionError())
	{
		BotError("[OnVkError] CURL Error %d\n",e.GetErrorCode());
		switch(e.GetErrorCode())
		{
			case 7:
				BotError("[OnVkError] Couldn't connect() to host. 5 minute slow down\n");
				boost::this_thread::sleep(
					boost::posix_time::minutes(5));
				break;
		}
	}
	
	if(e.IsLongPollError())
		OnLongPollError(e);
}

void Bot::OnRequestFinished(IVkRequest* req)
{
	if(req->IsLongPoll()) m_iErrLongPoll = 0;
}

void Bot::OnLongPollError(const VkException& e)
{
	if(m_iErrLongPoll == 25)
	{
		BotError("EXCEPTION RECURSION PREVENTED\n");
		BotSendMsg("Внимание! В боте произошел непоправимый сбой.\n"
			"Дальнейшая обработка команд может быть невозможна.\n");
		return;
	}
	m_iErrLongPoll++;
	
	if(e.IsConnectionError())
		StartLongPoll(); //Перезапустить long poll после обрыва подключения.
	else
	{
		BotError("[OnLongPollError] LongPoll Error %d\n",e.GetErrorCode());
		boost::this_thread::sleep(boost::posix_time::seconds(5));
		VkLongPoll* poll = dynamic_cast<VkLongPoll*>(e.GetRequest());
		switch(e.GetErrorCode())
		{
			case 1:
				PrepareLongPoll(poll->GetServer(),poll->GetKey(),(int)e.GetError()["ts"]);
				break;
			case 2:
			case 3:
				StartLongPoll();
				break;
			case 4:
				BotError("[OnLongPollError] Long Poll Wrong version! Termination\n");
				break;
		}
	}
}