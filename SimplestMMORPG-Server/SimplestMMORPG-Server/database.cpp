#include "simplestMMORPG.h"
#include "database.h"
// SQLConnect_ref.cpp  
// compile with: odbc32.lib  

void Database::handleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode) {
	SQLSMALLINT iRec = 0;
	SQLINTEGER iError;
	WCHAR wszMessage[1000];
	WCHAR wszState[SQL_SQLSTATE_SIZE + 1];
	if (RetCode == SQL_INVALID_HANDLE) {
		fwprintf(stderr, L"Invalid handle! n");
		return;
	}
	while (SQLGetDiagRec(hType, hHandle, iRec, wszState, &iError, wszMessage,
		(SQLSMALLINT)(sizeof(wszMessage) / sizeof(WCHAR)), (SQLSMALLINT *)NULL) == SQL_SUCCESS) {
		// Hide data truncated..
		if (wcsncmp(wszState, L"01004", 5))
			fwprintf(stderr, L"[%5.5s] %s (% n", wszState, wszMessage, iError);
	}
}

// 중복이 있으면 false return
// 아이디가 없는데 중복은 없다면 생성
bool Database::login(const std::wstring& query, ClientId& id, int& level, int& xPos, int& yPos, int& str, int& dex, int& hp, int& potion, int& kill, int& death, int& exp)
{
	SQLRETURN retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
	SQLLEN idLen = 0, nameLen = 0, levelLen = 0, xPosLen = 0, yPosLen = 0, strLen = 0, dexLen = 0, hpLen = 0, potionLen = 0, killLen = 0, deathLen = 0, expLen = 0;

	retcode = SQLExecDirect(hstmt, const_cast<SQLWCHAR*>(query.c_str()), SQL_NTS);
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {

		// Bind columns 1, 2, and 3  
		SQLBindCol(hstmt, 1, SQL_SMALLINT, &id, sizeof(ClientId), &idLen);
		SQLBindCol(hstmt, 2, SQL_INTEGER, &level, sizeof(int), &levelLen);
		SQLBindCol(hstmt, 3, SQL_INTEGER, &xPos, sizeof(int), &xPosLen);
		SQLBindCol(hstmt, 4, SQL_INTEGER, &yPos, sizeof(int), &yPosLen);
		SQLBindCol(hstmt, 5, SQL_INTEGER, &str, sizeof(int), &strLen);
		SQLBindCol(hstmt, 6, SQL_INTEGER, &dex, sizeof(int), &dexLen);
		SQLBindCol(hstmt, 7, SQL_INTEGER, &hp, sizeof(int), &hpLen);
		SQLBindCol(hstmt, 8, SQL_INTEGER, &potion, sizeof(int), &potionLen);
		SQLBindCol(hstmt, 9, SQL_INTEGER, &kill, sizeof(int), &killLen);
		SQLBindCol(hstmt, 10, SQL_INTEGER, &death, sizeof(int), &deathLen);
		SQLBindCol(hstmt, 11, SQL_INTEGER, &exp, sizeof(int), &expLen);

		// Fetch and print each row of data. On an error, display a message and exit.  
		for (int i = 0; ; i++) {
			retcode = SQLFetch(hstmt);   // 실제 데이터 각각 뽑는 부분
			if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
				handleDiagnosticRecord(hstmt, SQL_HANDLE_STMT, retcode);
			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
				continue;
			break;
		}
	}
	else {
		return false;
	}

	SQLCancel(hstmt);

	return true;
}

void Database::logout(const std::wstring& query)
{
	SQLRETURN retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
	SQLLEN idLen = 0, nameLen = 0, levelLen = 0, xPosLen = 0, yPosLen = 0;

	retcode = SQLExecDirect(hstmt, const_cast<SQLWCHAR*>(query.c_str()), SQL_NTS);
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		SQLCancel(hstmt);
	}
	else {
		handleDiagnosticRecord(hstmt, SQL_HANDLE_STMT, retcode);
	}
}

std::wstring Database::sqlBuildQueryLogin(wchar_t * strId)
{
	return std::wstring { L"EXEC login " + std::wstring(strId) };
}

std::wstring Database::sqlBuildQueryLogout(short id, short level, short xPos, short yPos, short str, short dex, short hp, short potion, short kill, short death, short exp)
{
	std::wstring query =
		L"EXEC logout "
		+ std::to_wstring(id)
		+ L", "
		+ std::to_wstring(level)
		+ L", "
		+ std::to_wstring(xPos)
		+ L", "
		+ std::to_wstring(yPos)
		+ L", "
		+ std::to_wstring(str)
		+ L", "
		+ std::to_wstring(dex)
		+ L", "
		+ std::to_wstring(hp)
		+ L", "
		+ std::to_wstring(potion)
		+ L", "
		+ std::to_wstring(kill)
		+ L", "
		+ std::to_wstring(death)
		+ L", "
		+ std::to_wstring(exp);
	return query;
}

Database::Database()
{
	SQLRETURN retcode;

	// Allocate environment handle  
	retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);

	// Set the ODBC version environment attribute  
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		retcode = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER*)SQL_OV_ODBC3, 0);

		// Allocate connection handle  
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			retcode = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);

			spdlog::info("Database initialized.");
		}
	}

	hstmt = nullptr;
}

Database::~Database()
{
	SQLCancel(hstmt);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
	SQLFreeHandle(SQL_HANDLE_ENV, henv);
	SQLDisconnect(hdbc);
}

void Database::connect()
{
	SQLRETURN retcode;

	// Set login timeout to 5 seconds  
	SQLSetConnectAttr(hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);

	// Connect to data source  
	retcode = SQLConnect(hdbc, (SQLWCHAR*)L"SimplestMMORPG", SQL_NTS, (SQLWCHAR*)NULL, 0, NULL, 0);

	// Allocate statement handle  
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);

		spdlog::info("Connected to Database.");
	}
}
