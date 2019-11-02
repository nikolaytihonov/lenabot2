#include "leto.h"
#include "service.h"
#include "database.h"
#include "vkrequest.h"
#include "bot.h"
#include <boost/format.hpp>

//table leto_groups
//id	name	tag_name	last_post_id

//table leto_group_##id
//id owner_id post_id

//все команды данного сервиса требуют привелегий Leader
//и выполняются синхронно

class LetoService : public Service, public ILeto
{
public:
	LetoService() : Service("LetoService")
	{}

	virtual void Load();
	virtual bool ProcessCommand(const Command& cmd);

	virtual std::string GetRandomArt(std::string tag);
private:
	bool Mirror(std::string id,std::string tags);
	bool MirrorInternal(std::string id,std::string tags,std::vector<int>& last_posts);
} _leto;

ILeto* leto = &_leto;

void LetoService::Load()
{
	if(!db.TableExists("leto_groups"))
		db.Execute("CREATE TABLE leto_groups (id PRIMARY KEY,type,name,tags);");
}

static int leto_load(void* str,int,char** argv,char**)
{
	std::stringstream& out = *(std::stringstream*)str;
	out << "ID " << argv[0] << " Имя \"" 
		<< argv[2] << "\" Теги: \"" 
		<< argv[3] << "\"" << std::endl;
	return 0;
}

bool LetoService::ProcessCommand(const Command& cmd)
{
	std::stringstream out;
	if(cmd.GetName() == "leto_mirror" && services.CheckAdmin(Leader))
	{
		if(cmd.ArgC() < 3) return false;
		Mirror(cmd.Arg(1),cmd.Arg(2));
		return true;
	}
	else if(cmd.GetName() == "leto_list" && services.CheckAdmin(Leader))
	{
		db.Execute("SELECT * FROM leto_groups;",leto_load,&out);
		services.Reply(out.str());
		return true;
	}
	else if(cmd.GetName() == "leto_info" && services.CheckAdmin(Leader))
	{
		if(cmd.ArgC() < 2) return false;
		out << "Количество: " << db.Count(boost::str(
			boost::format("SELECT * FROM leto_group_%s;") % cmd.Arg(1)))
			<< std::endl;
		services.Reply(out.str());
		return true;
	}
	else if(cmd.GetName() == "арт")
	{
		if(cmd.ArgC() < 2) return false;
		std::string photo = GetRandomArt(cmd.Arg(1));
		if(photo.empty())
		{
			out << "Арт ненайден.";
			services.Reply(out.str());
			return true;
		}

		VkRequest* art = new VkRequest("messages.send");
		art->SetParam("peer_id",services.GetCommandUser().m_iConvId);
		art->SetParam("reply_to",services.GetCommandUser().m_iMsgId);
		art->SetParam("random_id",bot.GetMessageRandomId());
		art->SetParam("attachment",photo);
		services.Request(art);
	}
	return false;
}

struct photo_s {
	int id;
	int owner_id;
	int post_id;
};

bool LetoService::Mirror(std::string id,std::string tags)
{
	std::vector<int> last;
	return MirrorInternal(id,tags,last);
}

bool LetoService::MirrorInternal(std::string screen,std::string tags,
	std::vector<int>& last_posts)
{
	json_value* pVal;
	std::string name;
	int id;

	VkRequest* group_info = new VkRequest("groups.getById");
	group_info->SetParam("group_ids",screen);
	bot.GetAPI()->Request(group_info,&pVal);

	auto& group = ((*pVal)["response"])[0];
	if((int)group["is_closed"] == 1) return false;
	name = std::string((const char*)group["name"]);
	id = (int)group["id"];
	json_value_free(pVal);

	services.Reply(boost::str(boost::format("Начинаю клонировать %d") % id));
	std::vector<struct photo_s> photos;

	int offset = 0;
	int count = 100;
	do {
		VkRequest* get = new VkRequest("wall.get");
		get->SetParam("owner_id",-id);
		get->SetParam("offset",offset);
		get->SetParam("count",count);
		bot.GetAPI()->Request(get,&pVal);

		auto& res = (*pVal)["response"];
		auto& posts = res["items"];
		count = (int)posts.u.array.length;

		for(int i = 0; i < count; i++)
		{
			auto& post = posts[i];
			if(post["attachments"].type == json_none) continue;

			if(std::find(last_posts.begin(),last_posts.end(),
				(int)post["id"]) != last_posts.end())
			{
				count = 0;
				break;
			}

			auto& attachments = post["attachments"];
			if(attachments.type != json_array
				|| attachments.u.array.length < 1) continue;
			for(int j = 0; j < attachments.u.array.length; j++)
			{
				auto& attach = attachments[j];
				if(std::string((const char*)attach["type"]) != "photo")
					continue;
				auto& photo = attach["photo"];

				struct photo_s ph;
				ph.id = (int)photo["id"];
				ph.owner_id = (int)photo["owner_id"];
				ph.post_id = (int)post["id"];
				photos.push_back(ph);
			}
		}

		json_value_free(pVal);
		offset += count;
	} while(count >= 100);

	std::reverse(photos.begin(),photos.end());
	
	db.Execute(boost::str(
		boost::format("INSERT OR REPLACE INTO leto_groups (id,type,name,tags)"
		" VALUES ('%d','group','%s','%s');")
			% id % name % tags));

	if(!db.TableExists(boost::str(boost::format("leto_group_%d") % id)))
	{
		db.Execute(boost::str(
			boost::format("CREATE TABLE leto_group_%d (post_id,owner_id,id);")
				% id));
	}
	
	for(auto it = photos.begin(); it != photos.end(); ++it)
	{
		try {
		db.Execute(boost::str(
			boost::format("INSERT INTO leto_group_%d (post_id,owner_id,id)"
				" VALUES ('%d','%d','%d');")
					% id % it->post_id % it->owner_id % it->id));
		} catch(std::exception& e) {
			bot.SendText(services.GetCommandUser().m_iConvId,boost::str(
				boost::format("[Warning] При клонирование произошло исключение: %s")
					% std::string(e.what())),false);
		}
	}

	services.Reply("Клонирование завершено");

	return true;
}

static int leto_find_id(void* pId,int,char** argv,char**)
{
	*(int*)pId = atoi(argv[0]);
	return 0;
}

static int leto_get_photo(void* data,int,char** argv,char**)
{
	*(std::string*)data = boost::str(
		boost::format("photo%s_%s") % argv[1] % argv[2]);
	return 0;
}

std::string LetoService::GetRandomArt(std::string tag)
{
	int id = -1;
	db.Execute(boost::str(
		boost::format("SELECT * FROM leto_groups WHERE tags='%s' ORDER BY RANDOM() LIMIT 1;")
			% tag),leto_find_id,&id);
	if(id == -1) return "";
	std::string photo = "";
	db.Execute(boost::str(
		boost::format("SELECT * FROM leto_group_%d ORDER BY RANDOM() LIMIT 1;")
			% id),leto_get_photo,&photo);

	return photo;
}