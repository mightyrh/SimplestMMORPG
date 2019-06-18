#pragma once

#include "simplestMMORPG.h"
#include <sqlext.h>
#include "client.h"
#include "id_table_def.h"
#include "protocol.h"

#define PHONE_LEN 5

class Database
{
private:
	SQLHENV henv;
	SQLHDBC hdbc;
	SQLHSTMT hstmt;

public:
	Database();
	~Database();
	void connect();
	void handleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode);

	bool login(const std::wstring& query, ClientId& id, int& level, int& xPos, int& yPos, int& str, int& dex, int& hp, int& potion, int& kill, int& death, int& exp);
	void logout(const std::wstring& query);
	
	std::wstring sqlBuildQueryLogin(wchar_t* strId);
	std::wstring sqlBuildQueryLogout(short id, short level, short xPos, short yPos, short str, short dex, short hp, short potion, short kill, short death, short exp);
	
};


enum class QueryType
{
	Login,
	Logout
};

struct QueryMessage
{
	ClientId clientId;
	QueryType type;
	std::wstring query;
};