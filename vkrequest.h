#ifndef __VKREQUEST_H
#define __VKREQUEST_H

#include <string>
#include <vector>
#include <map>
#include <curl/curl.h>
#include "json/json.h"
#include "vkapicontroller.h"

typedef enum {
	ConvMainChat = -1,
	ConvUser,
	ConvChat,
	ConvGroup,
	ConvEmail
} convtype_t;

typedef struct conv_s {
	int id;
	convtype_t type;
	int local_id;

	bool operator==(const struct conv_s& cv)
	{
		return this->id == cv.id;
	}

	bool operator==(int peer_id)
	{
		return this->id == peer_id;
	}
} conv_t;

class VkApi;

namespace VkMethods
{
	std::string DownloadPhoto(VkApi* api,int owner,int id,std::string path);
}

class VkPostMultipart
{
public:
	typedef enum {
		Text,
		File
	} mimetype_t;
	
	VkPostMultipart(std::string name,
		std::string text,
		mimetype_t type)
		: m_Name(name),
		m_Text(text),
		m_Type(type)
	{}
	
	std::string m_Name;
	std::string m_Text;
	mimetype_t m_Type;
};

class IVkRequest
{
public:
	typedef enum {
		Standard,
		LongPoll,
		Other,
		Last
	} priority_t;
	
	virtual ~IVkRequest(){}
	
	virtual const std::string& GetName() const = 0;
	virtual void SetParam(std::string param,std::string val,bool bPost = false) = 0;
	virtual void SetParam(std::string param,int val) = 0;
	virtual void SetParam(std::string param,float val) = 0;
	virtual void AddMultipart(const VkPostMultipart& multi) = 0;
	
	virtual void Prepare(VkApi*) = 0;
	virtual void PrepareCURL(CURL*) = 0;
	virtual VkApi* GetAPI() const = 0;
	
	virtual void SetAsync(bool bVal) = 0;
	virtual bool IsAsync() = 0;
	virtual bool IsCustomRequest() = 0;
	virtual bool IsLongPoll() = 0;
	virtual const std::string& GetURL() = 0;
	virtual void SetPriority(priority_t pr) = 0;
	virtual priority_t GetPriority() const = 0;
	
	virtual void OnRequestFinished(json_value& val) = 0;
};

class VkRequest : public IVkRequest
{
public:
	VkRequest();
	VkRequest(std::string method);
	VkRequest(const VkRequest&);
	virtual ~VkRequest();
	
	virtual const std::string& GetName() const;
	virtual void SetParam(std::string param,std::string val,bool bPost = false);
	virtual void SetParam(std::string param,int val);
	virtual void SetParam(std::string param,float val);
	virtual void AddMultipart(const VkPostMultipart& multi);
	
	virtual void Prepare(VkApi*);
	virtual void PrepareCURL(CURL*);
	virtual VkApi* GetAPI() const {return m_pVk;}
	
	virtual void SetAsync(bool bVal){m_bAsync = bVal;}
	virtual bool IsAsync(){return m_bAsync;}
	virtual bool IsCustomRequest(){return m_bCustom;}
	virtual bool IsLongPoll(){return false;}
	virtual const std::string& GetURL(){return m_URL;}
	virtual priority_t GetPriority() const {return m_Priority;}
	virtual void SetPriority(IVkRequest::priority_t pr){m_Priority = pr;}
	
	virtual void OnRequestFinished(json_value& val);
	
	std::string Serialize(bool bPost = false) const; //потом нужно вызвать free
	void SetDefaultURL();
	void Clear();
	inline bool IsStandartRequest(){return (!IsLongPoll() && !IsCustomRequest());}
protected:
	VkApi* m_pVk;
	std::string m_Method;
	std::map<std::string,std::string> m_Params;
	std::map<std::string,std::string> m_PostParams;
	std::vector<VkPostMultipart> m_Multipart;
	struct curl_httppost* m_Form;
	struct curl_slist* m_Header;
	bool m_bCustom;
	bool m_bAsync;
	std::string m_URL;
	IVkRequest::priority_t m_Priority;
};

class IVkLinkedRequest
{
public:
	virtual IVkRequest* Create() = 0;
	virtual int GetFinalStage() = 0;
};

class VkLinkedRequest : public VkRequest, public IVkLinkedRequest
{
public:
	VkLinkedRequest();
	virtual ~VkLinkedRequest(){
		printf("%s STAGE %d %p LINKED REQUEST DESTRUCTOR\n",m_Method.c_str(),m_iStage,this);
	}
	
	virtual void OnRequestFinished(json_value& val);
protected:
	void Repeat(){m_iStage = -1;}
	void Finish(){m_iStage = GetFinalStage();}
	
	int m_iStage;
};

class VkLongPoll : public VkLinkedRequest
{
public:
	VkLongPoll(std::string server,std::string key,int ts,
		IVkApiController* pControl);
	virtual ~VkLongPoll(){printf("VkLongPoll::~VkLongPoll\n");}
	
	virtual IVkRequest* Create(){return new VkLongPoll(*this);}
	virtual void OnRequestFinished(json_value& val);
	virtual int GetFinalStage(){return 0;} //Повторять этот запрос всегда.
	
	virtual void Prepare(VkApi*);
	virtual bool IsLongPoll(){return true;}
	
	const std::string& GetServer() const {return m_Server;}
	const std::string& GetKey() const {return m_Key;}
	int GetTS() const {return m_iTS;}
private:
	std::string m_Server;
	std::string m_Key;
	int m_iTS;
	IVkApiController* m_pController;
};

class VkMessage
{
public:
	VkMessage(){}
	VkMessage(int msg,int conv,int user,
		time_t timestamp,std::string text)
		: m_iMsgId(msg),
		m_iConvId(conv),m_iUserId(user),
		m_Timestamp(timestamp),m_Text(text)
	{}

	int m_iMsgId;
	int m_iConvId;
	int m_iUserId;
	time_t m_Timestamp;
	std::string m_Text;
	std::string m_Attachments;
	int m_iFwdUser;
	std::string m_FwdText;
};


//0 photos.getMessagesUploadServer + multipart upload
//1 photos.saveMessagesPhoto
//2 messages.send
class VkUploadPhotoChat : public VkLinkedRequest
{
public:
	VkUploadPhotoChat(int conv_id,std::string text = "",int reply_to = 0);
	VkUploadPhotoChat(convtype_t conv,std::string text = "",int reply_to = 0);
	
	virtual IVkRequest* Create(){return new VkUploadPhotoChat(*this);}
	virtual void Prepare(VkApi*);
	virtual void OnRequestFinished(json_value& val);
	virtual int GetFinalStage(){return 3;}
	
	void AddPhoto(std::string file);
	
private:
	bool m_bMultiSend;
	union {
		int m_iConvId;
		convtype_t m_ConvType;
	} m_Peer;
	int m_iPhotoCur;
	int m_iReplyTo;
	std::string m_Text;
	std::vector<std::string> m_Photos;
	std::vector<std::string> m_Attachments;
};

//0 - get message
//1 - get fwd if needed
//2 - stop

class VkCommandGetMessage : public VkRequest
{
public:
	VkCommandGetMessage(int iMsgId);

	virtual void OnRequestFinished(json_value& val);
};

#endif
