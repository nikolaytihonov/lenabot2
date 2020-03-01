#include "leto.h"
#include "service.h"
#include "database.h"
#include "vkrequest.h"
#include "bot.h"
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/filesystem.hpp>

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

struct album_s {
	int album_id;
	int clone_id;
	std::string tags;

	inline bool operator==(const struct album_s& other)
	{
		return this->clone_id == other.clone_id;
	}

	inline bool operator==(int clone_id)
	{
		return this->clone_id == clone_id;
	}
};

class LetoService : public Service, public ILeto
{
public:
	LetoService() : Service("LetoService")
	{}

	virtual void Load();
	virtual bool ProcessCommand(const Command& cmd);

	virtual std::string GetRandomArt(std::string tag,int* group_id = NULL);

	bool Mirror(std::string id,std::string tags);
	bool MirrorInternal(std::string id,std::string tags,
		std::vector<struct photo_s>& last_posts);
	virtual void Update();

	bool ScanWall(int id,
		std::vector<struct photo_s>& out,
		std::vector<struct photo_s>& last);
	size_t UpdateWall(int id);

	std::string ConvertTag(std::string tag);
	void OnNewPhoto(int group,const struct photo_s& photo,std::string tags);
	std::string MirrorPhoto(int group,const struct photo_s& photo);
private:
	std::vector<struct album_s> m_Albums;
} _leto;

ILeto* leto = &_leto;

//mirror_albums
//album_id	clone_id	tags

static int leto_load_albums(void* data,int,char** argv,char**)
{
	struct album_s album;
	album.album_id = atoi(argv[0]);
	album.clone_id = atoi(argv[1]);
	album.tags = std::string(argv[2]);
	((std::vector<struct album_s>*)data)->push_back(album);
	return 0;
}

void LetoService::Load()
{
	if(!db.TableExists("leto_groups"))
		db.Execute("CREATE TABLE leto_groups (id PRIMARY KEY,type,name,tags);");
	if(!db.TableExists("mirror_albums"))
		db.Execute("CREATE TABLE mirror_albums (album_id PRIMARY KEY,clone_id,tags);");
	db.Execute("SELECT * FROM mirror_albums;",leto_load_albums,&m_Albums);
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
		int group_id;
		std::string photo = GetRandomArt(cmd.Arg(1),&group_id);
		if(photo.empty())
		{
			services << "Арт ненайден.";
			return true;
		}

		VkRequest* art = new VkRequest("messages.send");
		art->AddMultipart(VkPostMultipart("message",
			boost::str(boost::format("Арт взят из группы https://vk.com/club%d") % group_id),
		VkPostMultipart::Text));
		art->SetParam("peer_id",services.GetPeerId());
		art->SetParam("reply_to",services.GetCommandUser().m_iMsgId);
		art->SetParam("random_id",bot.GetMessageRandomId());
		art->SetParam("attachment",photo);
		services.Request(art);
	}
	else if(cmd.GetName() == "leto_photo" && services.CheckAdmin(Leader))
	{
		struct photo_s photo;
		photo.owner_id = atoi(cmd.Arg(1).c_str());
		photo.id = atoi(cmd.Arg(2).c_str());
		MirrorPhoto(0,photo);
	}
	else if(cmd.GetName() == "leto_update" && services.CheckAdmin(Leader))
		Update();
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
	return true;
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
			for(unsigned int j = 0; j < attachments.u.array.length; j++)
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

size_t LetoService::UpdateWall(int id)
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
			//OnNewPhoto(id,*it,tags);

			db.Execute(boost::str(
				boost::format("INSERT INTO leto_group_%d (post_id,owner_id,id)"
				" VALUES ('%d','%d','%d');") % id % it->post_id % it->owner_id % it->id));
		}
		
		return update.size();
	} catch(std::exception& e) {
		bot.Send(ConvMainChat,boost::str(
			boost::format("LetoService::UpdateWall исключение: %s")
				% std::string(e.what())),false);
	}
	return 0;
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

std::string LetoService::GetRandomArt(std::string tag,int* group_id)
{
	int id = -1;
	db.Execute(boost::str(
		boost::format("SELECT * FROM leto_groups WHERE tags='%s' ORDER BY RANDOM() LIMIT 1;")
			% sql_str(tag)),leto_find_id,&id);
	if(id == -1) return "";
	if(group_id) *group_id = id;
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
	bot.Send(ConvMainChat,"Запускаю обновление базы артов..",false);
	for(auto it = groups.begin(); it != groups.end(); ++it)
	{
		bot.Send(ConvMainChat,boost::str(
			boost::format("%u: %u новых постов добавлено\n")
				% it->id % UpdateWall(it->id))
		);
	}
}

std::string LetoService::ConvertTag(std::string tag)
{
	if(tag == "Комми-Лена") return "Лена";
	else if(tag == "Цитадель") return "Лена";
	else if(tag == "БС") return "";
	return tag;
}

void LetoService::OnNewPhoto(int group,const struct photo_s& photo,std::string _tags)
{
	std::stringstream text;
	std::vector<std::string> tags;
	text << "#БесконечноеЛето #Бесконечное_Лето #Совёнок #Летосфера";
	boost::split(tags,_tags,boost::is_any_of(","));
	for(auto it = tags.begin(); it != tags.end(); ++it)
	{
		std::string tag = ConvertTag(*it);
		if(tag.empty()) continue;
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

//1. Download photo
//1.1. If album doesn't exists, then create it
//2. Upload to main mirror album

std::string LetoService::MirrorPhoto(int group,const struct photo_s& photo)
{
	std::string path = boost::str(
		boost::format("leto/%d/") % group);
	boost::filesystem::create_directories(
		boost::filesystem::path(path));

	std::string file = VkMethods::DownloadPhoto(
		bot.GetAPI(),photo.owner_id,photo.id,path);
	if(file.empty())
		return "photo0_0";

	//Проверка на альбом
	auto it = std::find(m_Albums.begin(),m_Albums.end(),group);
	if(it == m_Albums.end())
	{
		//Создать альбом
		struct album_s album;

		//Найти тэги группы
		std::string tags = "";
		db.Execute(boost::str(
			boost::format("SELECT * FROM leto_groups WHERE id='%d';")
				% group),leto_get_tags,&tags);

		VkRequest* req = new VkRequest("photos.createAlbum");
		req->SetParam("title",boost::str(
			boost::format("%s-##%08X-04X")
				% ConvertTag(tags) % group % rand()));
		req->SetParam("description",ConvertTag(tags));
		req->SetParam("upload_by_admins_only",1);

		json_value* pVal;
		bot.GetAPI()->Request(req,&pVal);
		{
			boost::shared_ptr<json_value> val(pVal,json_value_free);

			auto& res = (*val)["response"];
			album.album_id = (int)res["id"];
			album.clone_id = group;
			album.tags = tags;
			m_Albums.push_back(album);

			db.Execute(boost::str(
				boost::format("INSERT INTO mirror_albums (album_id,clone_id,tags)"
					" VALUES ('%d','%d','%s');")
						% album.album_id
						% album.clone_id
						% sql_str(album.tags)));
		}

		it = std::find(m_Albums.begin(),m_Albums.end(),group);
	}

	std::string upload_url;
	{
		json_value* pVal;
		VkRequest* get = new VkRequest("photos.getUploadServer");
		get->SetParam("album_id",it->album_id);
		get->SetParam("group_id",bot.GetMirrorGroup());
		bot.GetAPI()->Request(get,&pVal);
		boost::shared_ptr<json_value> val(pVal,json_value_free);

		upload_url = std::string((const char*)(*(val.get()))["upload_url"]);
	}

	/*std::string server,photos_list,hash;
	int aid;
	{
		json_value* pVal;
		VkRequest* upload = new VkRequest("");

	}*/
	return "";
}
