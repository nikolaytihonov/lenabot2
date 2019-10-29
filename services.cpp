#include "service.h"
#include "event.h"
#include "bot.h"
#include "vkrequest.h"
#include <string>
#include <vector>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <boost/filesystem.hpp>

using namespace boost::filesystem;

//
//ArtService lena("LenaService","Лена","D:/clublenochki");
//ArtService alisa("AlisaService","Алиса","D:/alisa");

class ArtService : public Service, public Event
{
public:
	ArtService() : Service("ArtService"),
		Event("ArtService",Time(60*60,0))
	{}

	virtual void Load()
	{
		AddFolder("Лена","D:/clublenochki");
		AddFolder("Алиса","D:/alisa");
	}

	virtual bool ProcessMessage(const VkMessage& msg)
	{
		if(m_Arts.find(msg.m_Text) != m_Arts.end())
		{
			SendArt(msg.m_iConvId,msg.m_Text,msg.m_iMsgId);
			return true;
		}
		return false;
	}

	virtual void Fire()
	{
		SendArt(bot.GetMainConv(),"Лена");
		Event::Fire();
	}

	void AddFolder(std::string artName,std::string dir)
	{
		std::vector<std::string> files;

		path p(dir);
		directory_iterator end_it;
		for(directory_iterator it(p); it != end_it; ++it)
			if(is_regular_file(it->path())) files.push_back(it->path().string());
		m_Arts.insert(std::make_pair<std::string,std::vector<std::string>>(artName,files));
	}

	void SendArt(int conv_id,std::string artName,int reply_to = 0)
	{
		auto it = m_Arts.find(artName);
		if(it == m_Arts.end()) return;
		if(it->second.empty()) return;

		srand((unsigned int)time(NULL));
		VkUploadPhotoChat* art = new VkUploadPhotoChat(conv_id,"",reply_to);
		art->AddPhoto(it->second[rand()%it->second.size()]);
		services.Request(art);
	}
private:
	std::map<std::string,std::vector<std::string>> m_Arts;
} artservice;

//class BotService : public Service
//{
//public:
//	BotService() : Service("BotService")
//	{}
//
//	virtual bool ProcessCommand(const Command& cmd)
//	{
//		if(cmd.GetName() == "тест_кмд")
//		{
//			std::stringstream out;
//			out << "Вы ввели команду " << cmd.GetName() << std::endl
//				<< "Её аргументы: ";
//			for(int i = 1; i < cmd.ArgC(); i++)
//				out << '"' << cmd.Arg(i) << "\" ";
//			out << std::endl
//				<< "Её флаги: ";
//			for(int i = 0; i < cmd.m_Flags.size(); i++)
//				out << '"' << cmd.m_Flags[i] << "\" ";
//			out << std::endl;
//			Reply(out.str());
//			return true;
//		}
//		return false;
//	}
//} botSvc;