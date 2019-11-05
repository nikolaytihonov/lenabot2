#include "vkrequest.h"
#include "vkapi.h"
#include "dbg.h"
#include "service.h"
#include "bot.h"
#include <sstream>
#include <boost/format.hpp>

VkRequest::VkRequest()
	: m_Method(""),
	m_Form(NULL),m_Header(NULL),
	m_bAsync(false),
	m_Priority(IVkRequest::Standard),
	m_bCustom(false)
{
}

VkRequest::VkRequest(std::string method)
	: m_Method(method),
	m_Form(NULL),m_Header(NULL),
	m_bAsync(false),
	m_Priority(IVkRequest::Standard),
	m_bCustom(false)
{
}

VkRequest::VkRequest(const VkRequest& prev)
{
	m_Method = prev.m_Method;
	m_Params = prev.m_Params;
	m_PostParams = prev.m_PostParams;
	m_Multipart = prev.m_Multipart;
	m_Form = NULL;
	m_Header = NULL;
	m_bAsync = prev.m_bAsync;
	m_bCustom = prev.m_bCustom;
	m_URL = prev.m_URL;
	m_Priority = prev.m_Priority;
}

VkRequest::~VkRequest()
{
	BotLog("VkRequest::~VkRequest %s\n",GetName().c_str());
	if(m_Form) curl_formfree(m_Form);
	if(m_Header) curl_slist_free_all(m_Header);
}

const std::string& VkRequest::GetName() const
{
	return m_Method;
}

void VkRequest::SetParam(std::string param,std::string val,bool bPost)
{
	(bPost ? m_PostParams : m_Params).insert(
		std::pair<std::string,std::string>(param,val));
}

void VkRequest::SetParam(std::string param,int val)
{
	SetParam(param,boost::to_string(val));
}

void VkRequest::SetParam(std::string param,float val)
{
	SetParam(param,boost::to_string(val));
}

void VkRequest::AddMultipart(const VkPostMultipart& multi)
{
	m_Multipart.push_back(multi);
}

std::string VkRequest::Serialize(bool bPost) const
{
	const std::map<std::string,std::string>& params = bPost 
		? m_PostParams : m_Params;
	if(params.empty()) return "";
	std::stringstream out;
	std::map<std::string,std::string>::const_iterator it;
	for(it = params.begin(); it != params.end(); ++it)
	{
		out << boost::format("%s=%s") % it->first % it->second;
		if(std::next(it) != params.end()) out << "&";
	}
	return out.str();
}

void VkRequest::Prepare(VkApi* api)
{
	m_pVk = api;
	if(!IsCustomRequest())
		SetDefaultURL();
	
	if(IsStandartRequest())
	{
		SetParam("access_token",GetAPI()->GetToken());
		SetParam("v",GetAPI()->GetAPIVersion());
		SetParam("lang","ru");
	}
}

void VkRequest::PrepareCURL(CURL* pCurl)
{
	std::string url;
	
	std::string params = Serialize(false);
	url = GetURL() + (params.size() ? "?" : "") + params;
	m_Params.clear();
	
	curl_easy_setopt(pCurl,CURLOPT_URL,url.c_str());
	curl_easy_setopt(pCurl,CURLOPT_USERAGENT,"VkApi::DoRequest");
	BotLog("%s\n",url.c_str());
	
	std::string postParams = Serialize(true);
	if(!postParams.empty())
	{
		curl_easy_setopt(pCurl,CURLOPT_POST,1);
		curl_easy_setopt(pCurl,CURLOPT_POSTFIELDS,postParams.c_str());
		m_PostParams.clear();
	}
	
	if(!m_Multipart.empty())
	{
		struct curl_httppost* last = NULL;
		std::vector<VkPostMultipart>::const_iterator it;
		for(it = m_Multipart.begin(); it != m_Multipart.end(); ++it)
		{
			switch(it->m_Type)
			{
				case VkPostMultipart::Text:
					curl_formadd(&m_Form,&last,
						CURLFORM_COPYNAME,it->m_Name.c_str(),
						CURLFORM_COPYCONTENTS,it->m_Text.c_str(),
						CURLFORM_END);
					break;
				case VkPostMultipart::File:
					curl_formadd(&m_Form,&last,
						CURLFORM_COPYNAME,it->m_Name.c_str(),
						CURLFORM_FILE,it->m_Text.c_str(),
						CURLFORM_END);
					break;
			}
		}
		
		m_Header = curl_slist_append(m_Header, "Content-Type: multipart/form-data");
		
		curl_easy_setopt(pCurl,CURLOPT_HTTPPOST,m_Form);
		curl_easy_setopt(pCurl,CURLOPT_HTTPHEADER,m_Header);
		
		m_Multipart.clear();
	}
}

void VkRequest::OnRequestFinished(json_value& val)
{
	if(IsAsync()) json_value_free(&val);
}

void VkRequest::SetDefaultURL()
{
	if(IsLongPoll())
	{
		VkLongPoll* poll = dynamic_cast<VkLongPoll*>(this);
		m_URL = "https://" + poll->GetServer();
	}
	else if(!IsCustomRequest())
		m_URL = "https://api.vk.com/method/" + GetName();
}

void VkRequest::Clear()
{
	m_Params.clear();
	m_PostParams.clear();
	m_Multipart.clear();
}

VkLinkedRequest::VkLinkedRequest()
	: VkRequest(""),m_iStage(0)
{
}
	
void VkLinkedRequest::OnRequestFinished(json_value& val)
{
	if(GetFinalStage() <= 0 || m_iStage != GetFinalStage())
	{
		if(GetFinalStage() > 0) m_iStage++;
		GetAPI()->RequestAsync(Create());
	}
	VkRequest::OnRequestFinished(val);
}

VkLongPoll::VkLongPoll(std::string server,std::string key,int ts,
	IVkApiController* controller)
	: m_Server(server), m_Key(key), m_iTS(ts),
	m_pController(controller)
{
	m_Method = "LONGPOLL";
	SetPriority(IVkRequest::LongPoll);
}

void VkLongPoll::Prepare(VkApi* vk)
{
	VkRequest::Prepare(vk);
	
	SetParam("act","a_check");
	SetParam("key",m_Key);
	SetParam("ts",m_iTS);
	SetParam("wait",25);
	SetParam("mode",0x7FFFFFFF);
	SetParam("version",3);
}

void VkLongPoll::OnRequestFinished(json_value& val)
{
	m_pController->OnLongPoll(val);
	m_iTS = val["ts"];
	BotLog("Requesting new long poll..\n");
	
	VkLinkedRequest::OnRequestFinished(val);
}

VkUploadPhotoChat::VkUploadPhotoChat(int conv_id,std::string text,int reply_to)
	:
	m_iPhotoCur(0),
	m_Text(text),
	m_iReplyTo(reply_to)
{
	m_bMultiSend = false;
	m_Peer.m_iConvId = conv_id;
}

VkUploadPhotoChat::VkUploadPhotoChat(convtype_t type,std::string text,int reply_to)
	:
	m_iPhotoCur(0),
	m_Text(text),
	m_iReplyTo(reply_to)
{
	m_bMultiSend = true;
	m_Peer.m_ConvType = type;
}

void VkUploadPhotoChat::Prepare(VkApi* api)
{
	switch(m_iStage)
	{
		case 0:
			m_Method = "photos.getMessagesUploadServer";
			SetParam("chat_id",m_bMultiSend ? bot.GetMainConv() : m_Peer.m_iConvId);
			break;
	}
	VkRequest::Prepare(api);
}

void VkUploadPhotoChat::OnRequestFinished(json_value& val)
{
	std::vector<std::string>::iterator it;
	const json_value& r = val["response"];
	//char szAttach[256] = {0};
	std::string attach;
	switch(m_iStage)
	{
		case 0:
			AddMultipart(VkPostMultipart("photo",m_Photos[m_iPhotoCur],VkPostMultipart::File));
			m_URL = std::string((const char*)r["upload_url"]);
			m_bCustom = true;
			m_Method = "";
			SetPriority(IVkRequest::Other);
			break;
		case 1:			
			Clear();
			m_bCustom = false;
			SetParam("server",(int)val["server"]);
			SetParam("photo",std::string((const char*)val["photo"]));
			SetParam("hash",std::string((const char*)val["hash"]));
			m_Method = "photos.saveMessagesPhoto";
			SetPriority(IVkRequest::Standard);
			break;
		case 2:
			Clear();
			attach = boost::str(boost::format("photo%d_%d") 
				%	(int)(r[0])["owner_id"]
				%	(int)(r[0])["id"]);
			m_Attachments.push_back(attach);
			m_iPhotoCur++;
			if(m_iPhotoCur < m_Photos.size())
				Repeat(); //Повторить весь цикл загрузки фото для другого фото
			else
			{
				attach.clear();
				for(it = m_Attachments.begin(); it != m_Attachments.end(); ++it)
				{
					attach += *it;
					if(std::next(it) != m_Attachments.end())
						attach += ',';
				}
				if(m_bMultiSend)
				{
					bot.Send(m_Peer.m_ConvType,m_Text,true,0,Attachment(attach));
					Finish();
				}
				else
				{
					SetParam("peer_id",m_Peer.m_iConvId);
					SetParam("random_id",(int)time(NULL)+rand());
					AddMultipart(VkPostMultipart("message",
						m_Text,VkPostMultipart::Text));
					SetParam("attachment",attach);
					if(m_iReplyTo) SetParam("reply_to",m_iReplyTo);
					m_Method = "messages.send";
					SetPriority(IVkRequest::Standard);
				}
			}
			break;
	}
	VkLinkedRequest::OnRequestFinished(val);
}

void VkUploadPhotoChat::AddPhoto(std::string file)
{
	m_Photos.push_back(file);
}

VkCommandGetMessage::VkCommandGetMessage(int msg_id)
	: VkRequest("messages.getById")
{
	SetParam("message_ids",msg_id);
}

void VkCommandGetMessage::OnRequestFinished(json_value& val)
{
	const json_value& res = val["response"];
	const json_value& items = res["items"];
	const json_value& jmsg = items[0];
	VkMessage msg = VkMessage(
		(int)jmsg["id"],(int)jmsg["peer_id"],
		(int)jmsg["from_id"],(time_t)((int)jmsg["date"]),
		std::string((const char*)jmsg["text"])
	);

	if(jmsg["reply_message"].type != json_none)
	{
		const json_value& reply = jmsg["reply_message"];
		msg.m_FwdText = std::string((const char*)reply["text"]);
		msg.m_iFwdUser = (int)reply["from_id"];
	}
	else
	{
		const json_value& reply = (jmsg["fwd_messages"])[0];	
		msg.m_FwdText = std::string((const char*)reply["text"]);
		msg.m_iFwdUser = (int)reply["from_id"];
	}

	services.FinishCommand(msg);
	VkRequest::OnRequestFinished(val);
}