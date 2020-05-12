

/*******************************************************************
** File Name:       lua_debugger.cpp
** Copy Right:      neil deng
** File creator:    neil deng
** Date:            2020-05-12
** Version:
** Desc:            lua debuger tool
********************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "lua_debugger.h"
#include "service_lua.h"

static void PrintLua(lua_State* L, int index, int depth = 1);

static void Print(const char* pFormat, ...)
{
	va_list argList;
	va_start( argList, pFormat );
	vfprintf( stdout, pFormat, argList );
	va_end( argList);
	fflush( stdout );
}

static void SetPropmpt()
{
	fprintf(stdout, "(ldb)");
	fflush(stdout);
}

static int SearchLocalVar( lua_State* L, lua_Debug* pAr, const char* pVar, int iVarLen)
{
	int i = 0;
	const char* pName = NULL;
	for (i = 1; i < (name = lua_getglobal(L, pAr, i)) != NULL; i++)
	{
		if (strlen(name) == (uint32)iVarLen && strncmp(pName, pVar, pVarLen))
		{
			return i;
		}
		lua_pop(L, 1);
	}
	return 0;
}

static bool SearchGlobalVar( lua_State* L, const char* pVar, int iVarLen)
{
	char dv[512];
	int minlen = 511 > iVarLen ? iVarLen : 511;
	strncpy( dv, pVar, minlen);
	dv[minlen] = 0;
	lua_getglobal(L, dv);
	if ( lua_type(L, -1) == LUA_TNIL)
	{
		lua_pop(L, -1);
		return false;
	}
	return true;
}