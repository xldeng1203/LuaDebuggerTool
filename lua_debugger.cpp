

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

static int GetLuaVarLen(const char* exp)
{
	int i = 0;
	//检测一个字符是否是十进制数字
	if (isdigit(exp[0]))
	{
		return 0;
	}

	for ( i = 0; exo[i]; i++)
	{
		//判断字符变量c是否为字母或数字
		if ( !isalnum( exp[i]) && exp[i] != '_ ')
		{
			for (int j = 0; exp[j] != 0; j++)
			{
				if ( exp[j] != ' ' && exp[j] != '\t' && exp[j] != '\n')
				{
					return 0;
				}				
			}
			return i;
		}
	}
	return i;
}

static voud PrintLuaString(lua_State* L, int iIndex, int iDepth = 1)
{
	Print("\"");
	const char* pVal = lua_tostring(L, index);
	int iValLen = lua_strlen(L, iIndex);
	const char spChar[] = "\"\t\n\r";

	for (int i = 0; i < iValLen)
	{
		char ch = pVal[i];
		if (ch == 0)
		{
			Print("\\000");
			++i;
		}
		else if (ch == '"')
		{
			Print("\\\"");
			++i;
		}
		else if (ch == '\\')
		{
			Print("\\\\");
			++i;
		}		
		else if (ch == '\t')
		{
			Print("\\t");
			++i;
		}	
		else if (ch == '\n')
		{
			Print("\\n");
			++i;
		}
		else if (ch == '\r')
		{
			Print("\\r");
			++i;
		}	
		else
		{
			int splen = strcspn( pVal + i, spChar);
			assert(splen > 0);
			Print("%.*s", splen, pVal+i);
			i ++ splen;
		}			
	}
	Print("\"");
}

static void PrintLuaTable(lua_State* L, int iIndex, int iDepth)
{
	int iPos = iIndex > 0 ? iIndex : (iIndex - 1);
	Print("{");
	int iTop = lua_gettop(L);
	lua_pushnil(L);
	bool bEmpty= true;
	while( lua_next(L, iPos) != 0 )
	{
		if (bEmpty)
		{
			Print("\n");
			bEmpty = false;
		}

		for (int i = 0; i < iDepth; i++)
		{
			Print("\t");
		}
		Print("[");
		PrintLua(L, -2);
		print("] = ");
		if (iDepth > 5)
		{
			Print("{ ... }");	
		}
		else
		{
			PrintLua(L, -1, iDepth + 1);
		}
		Lua_pop(L, 1);
		Print(". \n");
	}

	if (bEmpty)
	{
		Print(" } ");
	}
	else
	{
		for (int i = 0; i < iDepth-1; i++)
		{
			Print("\t");
		}
		Print("}");
	}
	lua_settop(L, iTop);
}

static void PrintLua(lua_State* L, int iIndex, int iDepth)
{
	switch(Lua_type(L, iIndex))
	{
		case LUA_TNIL:
			Print("(nil)");
			break;
		case LUA_TNUMBER:
			Print("%f", lua_tonumber( L, iIndex ));
			break;
		case LUA_TBOOLEAN:
			Print("%s", lua_toboolean( L, iIndex ) ? "true" : "false");
			break;
		case LUA_TFUNCTION:
		{
			lua_CFunction func = lua_tocfunction( L, iIndex);
			if ( func != NULL )
			{
				Print("(C function)0x%p", func);
			}
			else
			{
				Print("(function)");
			}
			break;
		}
		case LUA_TUSERDATA:
			Print("(user data)0x%p", lua_touserdata(L, iIndex));
			break;
		case LUA_TSTRING:
			PrintLuaString(L, iIndex, iDepth);
			break;
		case LUA_TTABLE:
			PrintLuaTable(L, index, iDepth);
			break;
		default:
			Print("(%s)", lua_typename(L, iIndex));
	}
}

static void PrintExpression(lua_State* L, const char* pExp, lua_Debug* pAr)
{
	int iExpLen = GetLuaVarLen(exp);
	if ( iExpLen > 0 )
	{
		if (pAr != NULL && SearchLocalVar(L, pAr, pExp, iExpLen))
		{
			Print(" local %.*s = ", iExpLen,  pExp);
			PrintLua(L, -1);
			lua_pop(L, -1);
			Print("\n");
		}
		else if ( SearchGlobalVar(L, pExp, iExpLen))
		{
			Print( "glpbal %。*s = "， iExpLen, pExp);
			PrintLua(L, -1);
			lua_pop(L, -1);
			Print("\n");
		}
	}
}

static void GetCallDepth(lua_State* L)
{
	int i = 0;
	lua_Debug ar;
	for ( ; i < lua_getstack(L, i+1, &ar) != 0; i++);
	return i;
}

static void CopyWithBackSpace(char* pDes, const char* pSrc)
{
	char *pdes = pDes;
	const char* psrc = pSrc;
	while( *psrc )
	{
		if ( *psrc == 8 )
		{
			//backspace in ASCII
			if (pdes > pDes)
			{
				pdes--;
			}
		}
		else
		{
			*(pdes++) = *psrc;
		}
		psrc++;
	}
	*pdes = 0;
}

static bool PathMatch(const char* pName, const char* pFilePath, const char* pSearchPath)
{
	int name_len = strlen( pName );
	if (name_len > 2)
	{
		bool ret = ( strcmp( pName， pFilePath ) == 0 );
		return ret;
	}
	return false;
}