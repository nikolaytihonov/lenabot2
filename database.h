#ifndef __DATABASE_H
#define __DATABASE_H

#include <string>
#include <exception>
#include <sqlite3.h>
#include <stddef.h>
#include <string>

#define DB_FILE "lenabot.db"

class SqlException : public std::exception
{
public:
	SqlException(std::string errMsg,int code)
		: m_What(errMsg),m_iCode(code)
	{}
	~SqlException() throw(){}
	
	virtual const char* what() const throw(){return m_What.c_str();}
	
	std::string m_What;
	int m_iCode;
};

typedef int (*sql_callback_t)(void*,int,char**,char**);

class DataBase
{
public:
	DataBase()
		: m_pDb(NULL)
	{}
	
	void Open();
	void Close();
	
	int Execute(std::string sql,sql_callback_t clk = NULL,void* data = NULL);
	int Count(std::string sql);
	bool TableExists(std::string table);
	//void LoadTable(const char* pTable,const char* pFmt,...);
	
	sqlite3* GetDB(){return m_pDb;}
private:
	sqlite3* m_pDb;
};

extern DataBase db;

#endif