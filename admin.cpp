#include "admin.h"
#include "vkapi.h"
#include "service.h"
#include "database.h"
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
			" VALUES ('%d','%s');") % it->first % it->second));
	}
}

/*bool Admin::ProcessMessage(const VkMessage& msg)
{
	if(!BEGINS_WITH(msg.m_Text.c_str(),"смотритель#")) return false;
	//Доделать обработку команд
}*/

bool Admin::ProcessCommand(const Command& cmd)
{
	std::stringstream out;
	if(cmd.GetName() == "ранги")
	{
		std::map<int,int>::iterator it = m_Priveleges.begin();
		for(; it != m_Priveleges.end(); ++it)
		{
			out << "https://vk.com/id" << it->first
			<< " - " << GetPrivelegeName(it->second)
			<< std::endl;
		}
		services.Reply(out.str());
		return true;
	}
	else if(cmd.GetName() == "список_рангов")
	{
		for(int i = 0; i < sizeof(s_PrivelegeNames)/sizeof(std::string); i++)
			out << i-1 << ' ' << s_PrivelegeNames[i] << std::endl;
		services.Reply(out.str());
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
			out << "Неверная привелегия";
			services.Reply(out.str());
			return true;
		}
		if(priv == StHelen)
		{
			out << "Права Св.Елены никто не может иметь";
			services.Reply(out.str());
			return true;
		}
		
		SetPrivelege(services.GetCommandUser().m_iFwdUser,priv);
		return true;
	}
	else if(cmd.GetName() == "роли")
	{
		for(auto it = m_Roles.begin(); it != m_Roles.end(); ++it)
		{
			out << "ID роли " << it->first
				<< " привелегия " << GetPrivelegeName(it->second.m_iPrivelege)
				<< " описание: " << it->second.m_Role << std::endl;
		}
		services.Reply(out.str());
		return true;
	}
	else if(cmd.GetName() == "создать_роль" && services.CheckAdmin(Leader))
	{
		//Лена создать_роль "имя" "привелегия"
		if(cmd.ArgC() < 3) return false;
		int priv = FindPrivelege(cmd.Arg(2));
		if(priv == LastPrivelege)
			out << "Неверная привелегия.";
		else out << "ID новой роли: " << CreateRole(cmd.Arg(1),priv);
		services.Reply(out.str());
		return true;
	}
	else if(cmd.GetName() == "удалить_роль" && services.CheckAdmin(Leader))
	{
		//Лена удалить_роль ID
		if(cmd.ArgC() < 2) return false;
		DeleteRole(boost::lexical_cast<int>(cmd.Arg(1)));
		out << "Роль " << cmd.Arg(1) << " удалена";
		services.Reply(out.str());
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
			out << "Роль не найдена.";
			services.Reply(out.str());
			return true;
		}
		SetRole(services.GetFwdUser(),role);
		out << "Теперь у " << services.GetFwdUser() << " роль " << GetRole(services.GetFwdUser());
		services.Reply(out.str());
		return true;
	}
	else if(cmd.GetName() == "забрать_роль" && services.CheckAdmin(Leader))
	{
		if(!services.CheckFwdUser()) return true;
		SetRole(services.GetFwdUser(),-1);
		out << "Роль с " << services.GetFwdUser() << " была снята.";
		services.Reply(out.str());
		return true;
	}
	else if(cmd.GetName() == "статус")
	{
		int user = services.GetFwdUser() 
			? services.GetFwdUser() : services.GetCommandUser().m_iUserId;
		if(user == 0)
		{
			out << "Пользователь ненайден!";
			services.Reply(out.str());
			return true;
		}
		out << "ID: " << user << std::endl
			<< "Уровень доступа: " 
			<< GetPrivelegeName(GetPrivelege(user)) << std::endl
			<< "Административный доступ: " << (services.IsUserAdmin(user) ? "Да" : "Нет") << std::endl
			<< "Роль: " << GetRoleName(user) << std::endl;
		services.Reply(out.str());
		return true;
	}
	else if(cmd.GetName() == "кик" && services.CheckAdmin())
	{
		int user = 0;
		if(cmd.ArgC() > 2) user = boost::lexical_cast<int>(cmd.Arg(1));
		else if(cmd.ArgC() < 2) user = services.GetFwdUser();
		if(!user)
		{
			out << "Пользователь ненайден";
			services.Reply(out.str());
			return true;
		}
		if(GetPrivelege(user) <= GetPrivelege(services.GetCommandUser().m_iUserId))
		{
			out << "Недостаточно прав.";
			services.Reply(out.str());
			return true;
		}
		Kick(user);
		out << "Пользователь " << user << " был кикнут.";
		services.Reply(out.str());
		return true;
	}
	return false;
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
	SetPrivelege(user_id,Banned);
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
	priv++;
	if(priv >= 0 && priv < LastPrivelege)
		return s_PrivelegeNames[priv];
	return "";
}

void Admin::Kick(int user_id)
{
	VkRequest* kick = new VkRequest("messages.removeChatUser");
	kick->SetParam("chat_id",services.GetCommandUser().m_iConvId);
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
			" VALUES ('%d','%d','%s');") % id % role.m_iPrivelege % role.m_Role));
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