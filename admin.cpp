#include "admin.h"
#include "vkapi.h"
#include "service.h"
#include "database.h"
#include "bot.h"
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>

struct role_s {
	int m_iPrivelege;
	std::string m_Role;
};

class Admin : public Service, public IAdmin
{
public:
	Admin();

	virtual void Load();
	virtual void Save();
	virtual bool ProcessCommand(const Command& cmd);
	virtual bool ProcessEvent(const json_value& event);

	void ProcessInviteUser(int peer_id,int from,int member);
	void ProcessKickUser(int peer_id,int from,int member);

	virtual int GetPrivelege(int user_id);
	virtual void SetPrivelege(int user_id,int priv);
	virtual void Ban(int user_id,std::string = "");
	virtual void UnBan(int user_id);
	virtual std::string GetPrivelegeName(int priv);
	virtual void Kick(int user_id);

	virtual  bool IsRoot(int user_id){
		return IN_RANGE(GetPrivelege(user_id),StHelen,Leader);
	}
	virtual  bool IsAdmin(int user_id){
		return IN_RANGE(GetPrivelege(user_id),StHelen,HighAdherent);
	}
	virtual  bool IsAdherent(int user_id){
		return IN_RANGE(GetPrivelege(user_id),StHelen,Adherent);
	}
	virtual  bool IsIncorrect(int user_id){
		return IN_RANGE(GetPrivelege(user_id),Incorrect,Banned);
	}
	virtual  bool IsBanned(int user_id){
		return GetPrivelege(user_id) == Banned;
	}

	virtual bool CanRunCommand(int user_id){
		int priv = GetPrivelege(user_id);
		return (priv != Incorrect) && (priv != Banned);
	}

	int FindPrivelege(std::string priv);
	int CreateRole(std::string role,int priv);
	void DeleteRole(int role_id);
	int FindRole(std::string role_name);
	bool IsValidRole(int role);
	void SetRole(int user_id,int role = -1);
	int GetRole(int user_id);
	std::string GetRoleName(int user_id);
private:
	std::map<int,struct role_s> m_Roles;
	std::map<int,int> m_Priveleges;

	std::map<int,int> m_UserRoles;
	std::map<int,std::string> m_BlackList;
};

static Admin _admin;
IAdmin* admin = &_admin;

std::string s_PrivelegeNames[] = {
	"Святая Елена",
	"Предводитель",
	"Высший адепт",
	"Адепт",
	"Пользователь",
	"Неверный",
	"Забаненный"
};

Admin::Admin() : Service("Admin")
{
}

static int role_load(void* map,int,char** argv,char**)
{
	std::map<int,struct role_s>* pMap = (std::map<int,struct role_s>*)map;
	struct role_s role;
	role.m_iPrivelege = atoi(argv[1]);
	role.m_Role = std::string(argv[2]);
	pMap->insert(std::pair<int,struct role_s>(atoi(argv[0]),role));
	return 0;
}

static int admin_load(void* map,int,char** argv,char**)
{
	std::map<int,int>* pMap = (std::map<int,int>*)map;
	pMap->insert(std::pair<int,int>(atoi(argv[0]),atoi(argv[1])));
	return 0;
}

static int blacklist_load(void* map,int,char** argv,char**)
{
	std::map<int,std::string>* pMap = (std::map<int,std::string>*)map;
	pMap->insert(std::pair<int,std::string>(atoi(argv[0]),argv[1]));
	return 0;
}

void Admin::Load()
{
	if(!db.TableExists("roles"))
	{
		db.Execute("CREATE TABLE roles (role_id PRIMARY KEY,privelege,role_name);");
		db.Execute("INSERT INTO roles (role_id,privelege,role_name) VALUES "
			"(0,-1,'Святая Елена');");
	}
	if(!db.TableExists("user_roles"))
	{
		db.Execute("CREATE TABLE user_roles (user_id PRIMARY KEY,role_id);");
		db.Execute("INSERT INTO user_roles (user_id,role_id) VALUES (-1,0);");
	}
	if(!db.TableExists("blacklist"))
	{
		db.Execute("CREATE TABLE blacklist (user_id PRIMARY_KEY,reason);");
	}

	db.Execute("SELECT * FROM roles;",&role_load,&m_Roles);

	db.Execute("SELECT * FROM user_roles;",&admin_load,&m_UserRoles);
	for(auto it = m_UserRoles.begin(); it != m_UserRoles.end(); ++it)
	{
		m_Priveleges.insert(std::pair<int,int>(it->first,
			m_Roles[it->second].m_iPrivelege));
	}

	db.Execute("SELECT * FROM blacklist;",&blacklist_load,&m_BlackList);
	for(auto it = m_BlackList.begin(); it != m_BlackList.end(); ++it)
		m_Priveleges.insert(std::pair<int,int>(it->first,Banned));
}

void Admin::Save()
{
	for(auto it = m_UserRoles.begin(); it != m_UserRoles.end(); ++it)
	{
		db.Execute(boost::str(
			boost::format("INSERT OR REPLACE INTO user_roles (user_id,role_id)"
			" VALUES ('%d','%d');") % it->first % it->second));
	}

	for(auto it = m_BlackList.begin(); it != m_BlackList.end(); ++it)
	{
		db.Execute(boost::str(
			boost::format("INSERT OR REPLACE INTO blacklist (user_id,reason)"
			" VALUES ('%d','%s');") % it->first % sql_str(it->second)));
	}
}

/*bool Admin::ProcessMessage(const VkMessage& msg)
{
	if(!BEGINS_WITH(msg.m_Text.c_str(),"смотритель#")) return false;
	//Доделать обработку команд
}*/

bool Admin::ProcessCommand(const Command& cmd)
{
	if(cmd.GetName() == "ранги")
	{
		std::map<int,int>::iterator it = m_Priveleges.begin();
		for(; it != m_Priveleges.end(); ++it)
		{
			services << "https://vk.com/id" << it->first
			<< " - " << GetPrivelegeName(it->second)
			<< '\n';
		}
		return true;
	}
	else if(cmd.GetName() == "список_рангов")
	{
		for(unsigned int i = 0; i < sizeof(s_PrivelegeNames)/sizeof(std::string); i++)
			services << i-1 << ' ' << s_PrivelegeNames[i] << '\n';
		return true;
	}
	else if(cmd.GetName() == "задать_ранг" && services.CheckAdmin(Leader))
	{
		if(cmd.ArgC() < 2) return false;
		//Лена задать_ранг "ранг"
		if(!services.CheckFwdUser()) return true;
		int priv = FindPrivelege(cmd.Arg(1));
		if(priv == LastPrivelege)
		{
			services << "Неверная привелегия";
			return true;
		}
		if(priv == StHelen)
		{
			services << "Права Св.Елены никто не может иметь";
			return true;
		}

		SetPrivelege(services.GetCommandUser().m_iFwdUser,priv);
		return true;
	}
	else if(cmd.GetName() == "роли")
	{
		for(auto it = m_Roles.begin(); it != m_Roles.end(); ++it)
		{
			services << "ID роли " << it->first
				<< " привелегия " << GetPrivelegeName(it->second.m_iPrivelege)
				<< " описание: " << it->second.m_Role << '\n';
		}
		return true;
	}
	else if(cmd.GetName() == "создать_роль" && services.CheckAdmin(Leader))
	{
		//Лена создать_роль "имя" "привелегия"
		if(cmd.ArgC() < 3) return false;
		int priv = FindPrivelege(cmd.Arg(2));
		if(priv == LastPrivelege)
			services << "Неверная привелегия.";
		else services << "ID новой роли: " << CreateRole(cmd.Arg(1),priv);
		return true;
	}
	else if(cmd.GetName() == "удалить_роль" && services.CheckAdmin(Leader))
	{
		//Лена удалить_роль ID
		if(cmd.ArgC() < 2) return false;
		DeleteRole(boost::lexical_cast<int>(cmd.Arg(1)));
		services << "Роль " << cmd.Arg(1) << " удалена";
		return true;
	}
	else if(cmd.GetName() == "дать_роль" && services.CheckAdmin(Leader))
	{
		//Лена дать_роль "имя роли"
		if(cmd.ArgC() < 2) return false;
		if(!services.CheckFwdUser()) return true;
		int role = FindRole(cmd.Arg(1));
		if(role == -1)
		{
			services << "Роль не найдена.";
			return true;
		}
		SetRole(services.GetFwdUser(),role);
		services << "Теперь у " << services.GetFwdUser() << " роль " << GetRole(services.GetFwdUser());
		return true;
	}
	else if(cmd.GetName() == "забрать_роль" && services.CheckAdmin(Leader))
	{
		if(!services.CheckFwdUser()) return true;
		SetRole(services.GetFwdUser(),-1);
		services << "Роль с " << services.GetFwdUser() << " была снята.";
		return true;
	}
	else if(cmd.GetName() == "статус")
	{
		int user = services.GetFwdUser()
			? services.GetFwdUser() : services.GetCommandUser().m_iUserId;
		if(user == 0)
		{
			services << "Пользователь ненайден!";
			return true;
		}
		services << "ID: " << user << '\n'
			<< "Уровень доступа: "
			<< GetPrivelegeName(GetPrivelege(user)) << '\n'
			<< "Административный доступ: " << (services.IsUserAdmin(user) ? "Да" : "Нет") << '\n'
			<< "Роль: " << GetRoleName(user) << '\n';
		return true;
	}
	else if(cmd.GetName() == "кик" && services.CheckAdmin())
	{
		int user = 0;
		if(cmd.ArgC() > 2) user = boost::lexical_cast<int>(cmd.Arg(1));
		else if(cmd.ArgC() < 2) user = services.GetFwdUser();
		if(!user)
		{
			services << "Пользователь ненайден";
			return true;
		}
		if(GetPrivelege(user) <= GetPrivelege(services.GetCommandUser().m_iUserId))
		{
			services << "Недостаточно прав.";
			return true;
		}
		Kick(user);
		services << "Пользователь " << user << " был кикнут.";
		return true;
	}
	else if(cmd.GetName() == "бан" && services.CheckAdmin())
	{
		int user = 0;
		std::string reason = "";
		if(services.GetFwdUser() != 0)
		{
			user = services.GetFwdUser();
			reason = cmd.Arg(1);
		}
		else
		{
			user = boost::lexical_cast<int>(cmd.Arg(1));
			reason = cmd.Arg(2);
		}
		if(!user)
		{
			services << "Пользователь ненайден";
			return true;
		}
		Ban(user,reason);
		Kick(user);
		services << "Пользователь " << user << " был забанен по причине \""
			<< reason << "\"";
		return true;
	}
	else if(cmd.GetName() == "баны" && services.CheckAdmin())
	{
		for(auto it = m_BlackList.begin(); it != m_BlackList.end(); ++it)
		{
			services << "https://vk.com/id" << it->first << " по причине \""
				<< it->second << "\"";
		}
		return true;
	}
	else if(cmd.GetName() == "разбанить" && services.CheckAdmin())
	{
		if(cmd.ArgC() < 2) return false;
		int user = boost::lexical_cast<int>(cmd.Arg(1));
		UnBan(user);
		services << "Пользователь " << user << " был разбанен";
		return true;
	}
	return false;
}

bool Admin::ProcessEvent(const json_value& event)
{
	//[type,msg_id,flags,peer_id,timestamp,text,[attachments]]
	if((int)event[0] == 4)
	{
		//6
		int peer_id = (int)event[3];
		auto& attachment = event[6];
		if(attachment.type == json_none) return false;
		if(attachment["source_act"].type == json_none) return false;
		std::string source_act = std::string(
			(const char*)attachment["source_act"]);
		if(source_act == "chat_invite_user")
		{
			int source_mid = atoi((const char*)attachment["source_mid"]);
			int from = atoi((const char*)attachment["from"]);
			ProcessInviteUser(peer_id,from,source_mid);
			return true;
		}
		else if(source_act == "chat_kick_user")
		{
			int source_mid = atoi((const char*)attachment["source_mid"]);
			int from = atoi((const char*)attachment["from"]);
			ProcessKickUser(peer_id,from,source_mid);
			return true;
		}
		else if(source_act == "chat_invite_user_by_link")
		{
			int source_mid = atoi((const char*)attachment["source_mid"]);
			ProcessKickUser(peer_id,0,source_mid);
			return true;
		}
	}
	return false;
}

void Admin::ProcessInviteUser(int peer_id,int from,int member)
{
	//Забаненных добавлять нельзя.
	//Их могут добавить только те, у кого ранг Высшего адепта
	if(IsBanned(member) && (from != 0 &&
		(GetPrivelege(from) > HighAdherent && bot.GetAdminUser() != from)))
	{
		Kick(member);
		bot.Send(peer_id,"Данный пользователь забанен");
	}
}

void Admin::ProcessKickUser(int peer_id,int from,int member)
{
}

int Admin::GetPrivelege(int user_id)
{
	auto it = m_Priveleges.find(user_id);
	if(it == m_Priveleges.end()) return User;
	return it->second;
}

void Admin::SetPrivelege(int user_id,int priv)
{
	auto it = m_Priveleges.find(user_id);
	if(it == m_Priveleges.end())
		m_Priveleges.insert(std::pair<int,int>(user_id,priv));
	else it->second = priv;
}

void Admin::Ban(int user_id,std::string reason)
{
	m_BlackList.insert(std::pair<int,std::string>
		(user_id,reason));
	SetRole(user_id); //Снимает роль и дает User
	SetPrivelege(user_id,Banned); //дает Banned

	db.Execute(boost::str(
		boost::format("INSERT OR REPLACE INTO blacklist (user_id,reason)"
		" VALUES ('%d','%s');") % user_id % sql_str(reason)));
}

void Admin::UnBan(int user_id)
{
	auto ij = m_BlackList.find(user_id);
	if(ij == m_BlackList.end()) return;
	m_BlackList.erase(ij);

	auto it = m_Priveleges.find(user_id);
	if(it == m_Priveleges.end()) return;
	m_Priveleges.erase(it);

	db.Execute(boost::str(
		boost::format("DELETE FROM blacklist WHERE user_id='%d';")
			% user_id));
}

std::string Admin::GetPrivelegeName(int priv)
{
	if(priv >= StHelen && priv < LastPrivelege)
		return s_PrivelegeNames[priv+1];
	return "";
}

void Admin::Kick(int user_id)
{
	VkRequest* kick = new VkRequest("messages.removeChatUser");
	kick->SetParam("chat_id",services.GetLocalId());
	kick->SetParam("user_id",user_id);
	services.Request(kick);
}

int Admin::CreateRole(std::string desc,int priv)
{
	struct role_s role;
	role.m_Role = desc;
	role.m_iPrivelege = priv;
	int id = (m_Roles.empty() ? 0 : std::prev(m_Roles.end())->first) + 1;
	m_Roles.insert(std::pair<int,struct role_s>(id,role));

	db.Execute(boost::str(
		boost::format("INSERT OR REPLACE INTO roles (role_id,privelege,role_name)"
			" VALUES ('%d','%d','%s');") % id % role.m_iPrivelege % sql_str(role.m_Role)));
	return id;
}

void Admin::DeleteRole(int role_id)
{
	std::map<int,struct role_s>::iterator it = m_Roles.find(role_id);
	if(it == m_Roles.end()) return;
	m_Roles.erase(it);

	db.Execute(boost::str(
		boost::format("DELETE FROM roles WHERE role_id='%d';")
			% role_id));
}

int Admin::FindPrivelege(std::string priv)
{
	int i;
	for(i = 0; i < LastPrivelege; i++)
		if(s_PrivelegeNames[i] == priv) break;
	if(i == LastPrivelege) return LastPrivelege;
	return i-1;
}

int Admin::FindRole(std::string role_name)
{
	auto it = m_Roles.begin();
	for(; it != m_Roles.end(); ++it)
		if(it->second.m_Role == role_name) break;
	if(it == m_Roles.end()) return -1;
	return it->first;
}

bool Admin::IsValidRole(int role)
{
	return m_Roles.find(role) != m_Roles.end();
}

void Admin::SetRole(int user_id,int role)
{
	auto it = m_UserRoles.find(user_id);
	if(role == -1)
	{
		if(it != m_UserRoles.end())
			m_UserRoles.erase(it);
		SetPrivelege(user_id,User);
		db.Execute(boost::str(
			boost::format("DELETE FROM user_roles WHERE user_id='%d';")
				% user_id));
	}
	else if(IsValidRole(role))
	{
		if(it == m_UserRoles.end())
		{
			m_UserRoles.insert(std::pair<int,int>(user_id,role));
			db.Execute(boost::str(
				boost::format("INSERT INTO user_roles (user_id,role_id) VALUES ('%d','%d');")
					% user_id % role));
		}
		else
		{
			it->second = role;
			db.Execute(boost::str(
			boost::format("UPDATE user_roles SET 'role_id'='%d' WHERE user_id='%d';")
				% role % user_id));
		}
		SetPrivelege(user_id,m_Roles[role].m_iPrivelege);
	}
}

int Admin::GetRole(int user_id)
{
	auto it = m_UserRoles.find(user_id);
	return (it != m_UserRoles.end() ? it->second : -1);
}

std::string Admin::GetRoleName(int user_id)
{
	int role = GetRole(user_id);
	if(!IsValidRole(role)) return "";
	return m_Roles[role].m_Role;
}
