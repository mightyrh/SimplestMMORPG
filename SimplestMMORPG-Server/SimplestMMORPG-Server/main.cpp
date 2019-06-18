#include "simplestMMORPG.h"
#include "server.h"


/************************************************************************
/*
HandleDiagnosticRecord : display error/warning information
/*
/* Parameters:
/*
hHandle ODBC handle
/*
hType Type of handle (SQL_HANDLE_STMT, SQL_HANDLE_ENV, SQL_HANDLE_DBC)
/*
RetCode Return code of failing command
/************************************************************************/

int main()
{
	setlocale(LC_ALL, "");
	std::wcout.imbue(std::locale("korean"));
	Server server;

	system("pause");
}