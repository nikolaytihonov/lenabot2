#include "database.h"
#include <stdio.h>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

DataBase db;

#include "dbg.h"

void DataBase::Open()
{
	BotLog("DataBase::Open\n");
	int rc;
	if((rc=sqlite3_open(DB_FILE,&m_pDb)) != SQLITE_OK)
	{
		sqlite3_close(m_pDb);
		throw SqlException(sqlite3_errstr(rc),rc);
	}
}

void DataBase::Close()
{
	BotLog("DataBase::Close\n");
	sqlite3_close(m_pDb);
}

int DataBase::Execute(std::string sql,sql_callback_t clk,void* data)
{
	BotLog("DataBase::Execute SQL %s\n",sql.c_str());
	char* errMsg = NULL;
	int rc = sqlite3_exec(m_pDb,sql.c_str(),clk,data,&errMsg);
	BotLog("DataBase::Execute %d %s\n",rc,errMsg);
	if(rc < 0 || errMsg)
	{
		SqlException ex = SqlException(errMsg,rc);
		sqlite3_free(errMsg);
		throw ex;
	}
	return rc;
}

int count_callback(void* data,int,char** argv,char**)
{
	*(int*)data = atoi(argv[0]);
	return 0;
}

int DataBase::Count(std::string sql)
{
	int count = 0;
	Execute(sql,count_callback,&count);
	return count;
}

bool DataBase::TableExists(std::string table)
{
	return Count(boost::str(
		boost::format("SELECT count(*) FROM sqlite_master WHERE type='table' AND name='%s';") % table
		)) > 0;
}	

std::string sql_str(std::string sql)
{
	boost::replace_all(sql,"'","");
	return sql;
}