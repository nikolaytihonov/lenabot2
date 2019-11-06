#include "leto.h"
#include "service.h"
#include "database.h"
#include "vkrequest.h"
#include "bot.h"
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

//table leto_groups
//id	name	tag_name	last_post_id

//table leto_group_##id
//id owner_id post_id

//все команды данного сервиса требуют привелегий Leader
//и выполняются синхронно

struct photo_s {
	int id;
	int owner_id;
	int post_id;

	inline bool operator==(const struct photo_s& other)
	{
		return this->id == other.id;
	}

	inline bool operator==(int post_id)
	{
		return this->post_id == post_id;
	}
};

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
	bool MirrorInternal(std::string id,std::string tags,
		std::vector<struct photo_s>& last_posts);
	void Update();

	bool ScanWall(int id,
		std::vector<struct photo_s>& out,
		std::vector<struct photo_s>& last);
	void UpdateWall(int id);

	void OnNewPhoto(int group,const struct photo_s& photo,std::string tags);
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
		<< argv[3] << "\"" << '\n';
	return 0;
}

bool LetoService::ProcessCommand(const Command& cmd)
{
	if(cmd.GetName() == "leto_mirror" && services.CheckAdmin(Leader))
	{
		if(cmd.ArgC() < 3) return false;
		Mirror(cmd.Arg(1),cmd.Arg(2));
		return true;
	}
	else if(cmd.GetName() == "leto_list" && services.CheckAdmin(Leader))
	{
		std::stringstream out;
		db.Execute("SELECT * FROM leto_groups;",leto_load,&out);
		services.Reply(out.str());
		return true;
	}
	else if(cmd.GetName() == "leto_info" && services.CheckAdmin(Leader))
	{
		if(cmd.ArgC() < 2) return false;
		services << "Количество: " << db.Count(boost::str(
			boost::format("SELECT * FROM leto_group_%s;") % sql_str(cmd.Arg(1))))
			<< '\n';
		return true;
	}
	else if(cmd.GetName() == "арт")
	{
		if(cmd.ArgC() < 2) return false;
		std::string photo = GetRandomArt(cmd.Arg(1));
		if(photo.empty())
		{
			services << "Арт ненайден.";
			return true;
		}

		VkRequest* art = new VkRequest("messages.send");
		art->SetParam("peer_id",services.GetPeerId());
		art->SetParam("reply_to",services.GetCommandUser().m_iMsgId);
		art->SetParam("random_id",bot.GetMessageRandomId());
		art->SetParam("attachment",photo);
		services.Request(art);
	}
	/*else if(cmd.GetName() == "leto_update" && services.CheckAdmin(Leader))
	{
		Update();
		return true;
	}*/
	return false;
}

bool LetoService::Mirror(std::string id,std::string tags)
{
	std::vector<struct photo_s> last;
	return MirrorInternal(id,tags,last);
}

bool LetoService::MirrorInternal(std::string screen,std::string tags,
		std::vector<struct photo_s>& last_posts)
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

	if(!ScanWall(id,photos,last_posts)) return false;

	db.Execute(boost::str(
		boost::format("INSERT OR REPLACE INTO leto_groups (id,type,name,tags)"
		" VALUES ('%d','group','%s','%s');")
			% id % sql_str(name) % tags));

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
			bot.Send(services.GetPeerId(),boost::str(
				boost::format("[Warning] При клонирование произошло исключение: %s")
					% std::string(e.what())),false);
		}
	}

	services.Reply("Клонирование завершено");
}

//bool LetoService::MirrorInternal(std::string screen,std::string tags,
//	std::vector<int>& last_posts)
bool LetoService::ScanWall(int id,
	std::vector<struct photo_s>& out,
	std::vector<struct photo_s>& last)
{
	json_value* pVal;
	std::string name;

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

			if(std::find(last.begin(),last.end(),
				(int)post["id"]) != last.end())
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
				out.push_back(ph);
			}
		}

		json_value_free(pVal);
		offset += count;
	} while(count >= 100);

	std::reverse(out.begin(),out.end());
	return true;
}

static int leto_load_photo(void* data,int,char** argv,char**)
{
	struct photo_s photo;
	photo.post_id = atoi(argv[0]);
	photo.owner_id = atoi(argv[1]);
	photo.id = atoi(argv[2]);
	((std::vector<struct photo_s>*)data)->push_back(photo);
	return 0;
}

//CREATE TABLE leto_groups (id PRIMARY KEY,type,name,tags);

static int leto_get_tags(void* data,int,char** argv,char**)
{
	*(std::string*)data = std::string(argv[3]);
	return 0;
}

void LetoService::UpdateWall(int id)
{
	std::vector<struct photo_s> last;
	std::vector<struct photo_s> update;

	try {
		db.Execute(boost::str(
			boost::format("SELECT * FROM leto_group_%d"
			" ORDER BY CAST(post_id as INTEGER) DESC LIMIT 5;")
				% id),&leto_load_photo,&last);
		ScanWall(id,update,last);
		
		std::string tags;
		db.Execute(boost::str(
			boost::format("SELECT * FROM leto_groups WHERE id='%d' LIMIT 1;")
				% id),leto_get_tags,&tags);

		for(auto it = update.begin(); it != update.end(); ++it)
		{
			OnNewPhoto(id,*it,tags);

			/*db.Execute(boost::str(
				boost::format("INSERT INTO leto_group_%d (post_id,owner_id,id)"
				" VALUES ('%d','%d','%d');") % id % it->post_id % it->owner_id % it->id));*/
		}
	} catch(std::exception& e) {
		bot.Send(ConvMainChat,boost::str(
			boost::format("LetoService::UpdateWall исключение: %s")
				% std::string(e.what())),false);
	}
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
			% sql_str(tag)),leto_find_id,&id);
	if(id == -1) return "";
	std::string photo = "";
	db.Execute(boost::str(
		boost::format("SELECT * FROM leto_group_%d ORDER BY RANDOM() LIMIT 1;")
			% id),leto_get_photo,&photo);

	return photo;
}

//CREATE TABLE leto_groups (id PRIMARY KEY,type,name,tags);

struct leto_group_s {
	int id;
	std::string type;
	std::string name;
	std::string tags;
};

static int leto_load_groups(void* data,int,char** argv,char**)
{
	struct leto_group_s g;
	g.id = atoi(argv[0]);
	g.type = std::string(argv[1]);
	g.name = std::string(argv[2]);
	g.tags = std::string(argv[3]);
	((std::vector<struct leto_group_s>*)data)->push_back(g);
	return 0;
}

void LetoService::Update()
{
	std::vector<struct leto_group_s> groups;
	db.Execute("SELECT * FROM leto_groups WHERE type='group';",
		leto_load_groups,&groups);
	for(auto it = groups.begin(); it != groups.end(); ++it)
		UpdateWall(it->id);
}

void LetoService::OnNewPhoto(int group,const struct photo_s& photo,std::string _tags)
{
	std::stringstream text;
	std::vector<std::string> tags;
	text << "#БесконечноеЛето #Бесконечное_Лето #Совёнок #Летосфера";
	boost::split(tags,_tags,boost::is_any_of(","));
	for(auto it = tags.begin(); it != tags.end(); ++it)
	{
		std::string tag = *it;
		if(tag == "Комми-Лена") tag = "Лена";
		else if(tag == "Цитадель") tag = "Лена";
		else if(tag == "БС") continue;
		text << " #" << tag;
	}

	VkRequest* post = new VkRequest("wall.post");
	post->SetParam("owner_id",-bot.GetMirrorGroup());
	post->SetParam("from_group",1);
	post->SetParam("attachments",boost::str(
		boost::format("photo%d_%d") % photo.owner_id % photo.id));
	post->AddMultipart(VkPostMultipart("message",text.str(),VkPostMultipart::Text));
	bot.GetAPI()->RequestAsync(post);
}