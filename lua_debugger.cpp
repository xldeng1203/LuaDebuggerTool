

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

/****************************************static function**************************************************/

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
		bool ret = ( strcmp( pName, pFilePath ) == 0 );
		return ret;
	}
	return false;
}

/****************************************File Source***************************************************/
bool CFileSource::LoadFile(const char* pFileName)
{
	FILE* fp = fopen( pFileName, "r" );
	if (!fp)
	{
		return false;
	}
	
	char buf[512];
	std::string line("");
	bool readComplete = false;
	m_iLineCount = 0;
	while ( fgets(buf, sizeof(buf), fp) != NULL )
	{
		for (int i = 0; buf[i] != 0 && i < 512; i++)
		{
			if ( buf[i] == '\r' || buf[i] == '\n')
			{
				buf[i] = 0;
				line = line + buf;
				readComplete = true;
				break;
			}
		}
		readComplete = false;
	}
	
	if (readComplete)
	{
		if (m_iLineCount >= m_lines.m_iSize)
		{
			m_lines.resize();
		}

		m_lines[m_iLineCount] = line;
		m_lines.m_iNum = ++m_iLineCount;
		line = "";
	}
	else
	{
		line = line + buf;
	}
	
	m_sFileName = pFileName;
	fclose(fp);
	return true;
}


const char* CSourceMgr::LoadLine(const char* pFileName, int iLine)
{
	int ie = -1;
	int min = 9999999;
	int im = -1;

	for (int i = 0; i < max_num; i++)
	{
		if (m_files[i].IsLoaded())
		{
			if (m_files[i].m_sFileName == pFileName)
			{
				return m_files[i].GetLine(iLine);
			}
			else
			{
				if ( m_files[i].m_iAccesCount < min )
				{
					im = i;
					min = m_files[i].m_iAccesCount;
				}
			}
		}
		else
		{
			if ( ie < 0 )
			{
				ie = i;
			}
		}			
	}

	if ( ie >= 0 )
	{
		
		if ( !m_filesp[ie].LoadFile(pFileName) )
		{
			return NULL;
		}
		return m_files[ie].GetLine(iLine);	
	}
	
	m_files[im].Reset();

	if ( !m_files[im].LoadFile(pFileName) )
	{
		return NULL;
	}
	
	return m_files[im].GetLine(line);
}

void CSourceMgr::ReloadFile()
{
	for (int i = 0; i < max_num; i++)
	{
		if (m_files[i].m_sFileName != "")
		{
			m_files[i].LoadFile(m_files[i].m_sFileName.c_str());
		}
	}
}

/****************************************lua debugger**************************************************/

CLuaDebugger::Entry CLuaDebugger::m_entries[] = 
{
	{ "b", 		&CLuaDebugger::HandleCmdBreakPoint },
	{ "p", 		&CLuaDebugger::HandleCmdPoint },
	{ "clear", 	&CLuaDebugger::HandleCmdClear },
	{ "help", 	&CLuaDebugger::HandleCmdHelp },
	{ NULL，	NULL },
};

CLuaDebugger::EntryLdb CLuaDebugger::m_entries_ldb[] = 
{
	{ "c", 		&CLuaDebugger::HandleCmdContinue },
	{ "n", 		&CLuaDebugger::HandleCmdNext },
	{ "s", 		&CLuaDebugger::HandleCmdStep },
	{ "bt", 	&CLuaDebugger::HandleCmdBackTrace },
	{ NULL，	NULL },
};

CLuaDebugger::CLuaDebugger(lua_State* L)
{
	m_pLuaState = L;
	m_bIsStep = false;
	m_iCallDepth = -1;
	m_pSourceMgr = new CSourceMgr;
	EnableDebug(true);
}

void CLuaDebugger::SetSearchPath(const char* pPath)
{
	if ( !pPath || *pPath == 0 )
	{
		return;
	}
	
	int len = strlen( pPath );
	char slash = '/';
	if ( pPath[len-1] == slash)
	{
		strncpy( m_sSearchPath, path, 4096);
	}
	else
	{
		strncpy( m_sSearchPath, path, 4096);
		if ( len < 4095)
		{
			m_sSearchPath[len] = slash;
			m_sSearchPath[len+1] = 0;
		}
	}
}

int CLuaDebugger::SeachBreaPoint( lua_Debug* pAr ) const
{
	int i = 0;
	if ( pAr->source[0] == '@' )
	{
		for ( i = 0; i < LUA_MAX_BRKS; i++)
		{
			if ( m_arrBp[i].m_type != CBreakPoint::None
				&& m_arrBp[i].m_iLineNum >= 0
				&& pAr->currentline == (int)m_arrBp[i].m_iLineNum + 1
				&& PathMatch(pAr->source+1, m_arrBp[i].m_sName, m_sSearchPath) )
			{
				return i;
			}			
		}		
	}
	return -1;
}

void CLuaDebugger::EnableLineHook( bool isEnable)
{
	lua_State* L = m_pLuaState;
	int mask = lua_gethookmask(L);
	if ( isEnable )
	{
		int ret = lua_sethook(L, all_hook, mask | LUA_MASKLINE, 0);
		assert( ret != 0);
	}
	else
	{
		int ret = lua_sethook(L, all_hook, mask & ~LUA_MASKLINE, 0);
		assert( ret != 0);
	}
}

void CLuaDebugger::EnableFuncHook( bool isEnable )
{
}

void CLuaDebugger::OnEvent( int iBpnum, lua_Debug* pAr)
{
	lua_State* L = m_pLuaState;
	int depth = GetCallDepth(L);
	if ( iBpnum < 0  && m_iCallDepth != -1 && dpeth > m_iCallDepth )
	{
		return;
	}

	m_iCallDepth = depth;
	if ( iBpnum >= 0 )
	{
		Print("BreakPoint #%d hit!\n", iBpnum);
	}

	SetPropmpt();

	if ( pAr && pAr->source[0] == '@' && pAr->currentline != -1)
	{
		char fn[4096];
		strncpy(fn, m_sSearchPath, 4096);
		fn[4095] = 0;

		int addlen = strlen(fn);
		int offset = pAr->source[1] == '.' ? 3 : 1;

		strncpy(fn+addlen, pAr->source + offset, 4096-addlen);
		fn[4095] = 0;

		const char* pLine = m_pSourceMgr->LoadLine( fn, pAr->currentline-1 );
		if (!pLine)
		{
			Print("[cannot load source %s:%d\n]", pAr->source+1, pAr->currentline);
		}
		else
		{
			Print("[%s:%d]\n%s\n", pAr->source+1, pAr->currentline, pLine);
		}
		
		SetPropmpt();
	}
	
	for (;; SetPropmpt)
	{
		char buffer[256];
		while (1)
		{
			usleep(50000);
			if ( fgets(buffer, 256, stdin) != 0 )
			{
				break;
			}
		}
		
		if ( !HandleLdbCommand(buffer, pAr) )
		{
			break;
		}
	}
}

void CLuaDebugger::all_Hook( lua_state* L, lua_Debug* pAr)
{
	if ( pAr->event == LUA_HOOKLINE )
	{
		line_hook(L, pAr);
	}
	else if ( pAr->event == LUA_HOOKCALL || pAr->event == LUA_HOOKRET )
	{
		func_hook(L, ar);
	}
}

void CLuaDebugger::line_hook( lua_state* L, lua_Debug* pAr )
{
	CLuaDebugger* ldb = g_pstServiceLua->m_lua_debugger;
	if ( NULL == ldb)
	{
		return;
	}
	
	if ( !ldb->m_bIsEnable )
	{
		return;
	}

	if ( !lua_getstack(L, 0, pAr) )
	{
		return;
	}

	if ( lua_getinfo(L, "lnS", pAr) )
	{
		if ( ldb->m_bIsStep )
		{
			ldb->OnEvent(-1, pAr);
		}
		else
		{
			//search break point
			int bpIndex = ldb->SeachBreaPoint(pAr);
			if ( bpIndex < 0 )
			{
				return;
			}
			//find the break point
			ldb->OnEvent(bpIndex, pAr);
		}
	}
}

void CLuaDebugger::func_hook( lua_state* L, lua_Debug* pAr )
{
}

bool CLuaDebugger::HandleLdbCommand(const char* pCmd, lua_Debug* pAr = NULL)
{
	char cmd_new[4096];
	CopyWithBackSpace( cmd_new, cmd);
	const char ws[] = "\t\r\n";
	const char* pCmd = cmd_new;
	int len = 0;

	if (*pCmd == 0)
	{
		return true;
	}

	pCmd += strspn( pCmd, ws );
	len = strlen( pCmd, ws);

	char command[32];
	int min_len = len < 31 ? len : 31;
	strncpy( command, pCmd, min_len);
	command[min_len] = 0;
	for (int i = 0; m_entries_ldb[i].cmd != NULL; i++)
	{
		if ( !strcmp( m_entries_ldb[i].cmd, command) )
		{
			CommandHandlerLdb pFunc = m_entries_ldb[i].handler;
			const char* op = pCmd + len + strspn( pCmd + len, ws);
			int oplen = strcspn(op, ws);
			if ( op == pCmd + len)
			{
				oplen = 0;
			}
			return (this->*(pFunc))(pCmd, op, oplen, pAr);
		}
	}
	
	HandleCommand(pCmd, pAr);
	return true;
}

void CLuaDebugger::HandleCommand(const char* pCmd, lua_Debug* pAr)
{
	int depth = GetCallDepth(m_pLuaState);
	assert( m_iCallDepth >= -1);
	m_iCallDepth = depth;

	char cmd_new[4096];
	CopyWithBackSpace( cmd_new, cmd);
	const char ws[] = "\t\r\n";
	const char* pCmd = cmd_new;
	int len;

	char command[32];
	int min_len = len < 31 ? len : 31;
	strncpy( command, pCmd, min_len);
	command[min_len] = 0;
	for (int i = 0; i < m_entries[i].cmd != NULL; i++)
	{
		if ( !strcmp(m_entries[i].cmd, command) )
		{
			CommandHandler pFunc = m_entries[i].handler;
			const char* op = pCmd + len + strspn( pCmd+len, ws );
			int oplen = strcspn(op, ws);
			if ( op == pCmd+ len )
			{
				oplen = 0;
			}
			
			(this->*(pFunc))(op, oplen, pAr);
			return;
		}
	}
}

int CLuaDebugger::AddBreadPoint(const char* pFineName, uint32 lineNum, CBreakPoint::BPType type = CBreakPoint::User)
{
	assert( lineNum >= 0 );
	for (int i = 0; i < LUA_MAX_BRKS; i++)
	{
		if ( m_arrBp[i].GetType() == CBreakPoint::None )
		{
			m_arrBp[i].Reset();
			m_arrBp[i].SetName(pFineName);
			m_arrBp[i].m_iLineNum = lineNum;
			m_arrBp[i].m_type = type;
			return i;
		}
	}
	return -1;
}

void CLuaDebugger::HandleCmdBreakPoint(const char* pOp, uint32 iOplen, lua_Debug* pAr)
{
	if ( iOplen == 0 )
	{
		Print("b <fileame>:<linenum>\n");
	}

	const char* pDv = (const char*)memchr(pOp, ':', iOplen);
	if (!pDv)
	{
		Print("b <filename>:<linenum>\n");
		return;
	}

	int linenum, sc;
	if ( sscanf(pDv+1, "%d%n", &linenum, &sc) == 0 || sc != iOplen-(pDv-pOp)-1 || linenum <= 0 )
	{
		Print("Invalid line number\n");
		return;
	}

	char fn[4096];

	if ( m_sSearchPath && *pOp != '/')
	{
		// relative path
		strncpy(fn, m_sSearchPath, 4096);
		fn[4095] = 0;
		int addlen = strlen( fn );
		int minlen = 4095 - addlen > pDv - pOp > pDv-Pop : 4095 - addlen;
		strncat( fn + addlen, pOp, minlen);
	}
	else
	{
		//absolute path
		int minlen = 4095 > pDv - pOp ? pDv - pOp : 4095 - addlen;
		strncpy( fn, pOp, minlen);
		fn[minlen] = 0;
	}

	const char* pLine = m_pSourceMgr->LoadLine( fn, linenum - 1);
	if ( !pLine )
	{
		Print("cannot load source %s, line %d!\n", fn, linenum);
		return;
	}

	int bpnum = AddBreadPoint(fn, linenum-1);
	if ( bpnum < 0 )
	{
		Print("add breakpoint failed!\n");
		return;
	}
	
	Print("BreakPoint #%d is set at %.*s, line %d:\n%s\n", bpnum, pDv-Pop, pOp, linenum， line);
	EnableLineHook(true);
}

void CLuaDebugger::HandleCmdClear( const char* pOp, int oplen, lua_Debug* ar)
{
	for (int i = 0; i < LUA_MAX_BRKS; i++)
	{
		m_arrBp[i].Reset();
	}
	
	Print("All breakpoints cleared\n");
}

void CLuaDebugger::HandleCmdHelp(const char* pOp, uint32 iOplen, lua_Debug* pAr)
{
	Print("This is luya debugger instruction\n");
	Print("\n");

	Print("b <filename>:<line>  -- add breakpoint, notice: line num should not be blank line\n");
	Print("p <varname>          -- print var name \n");
	Print("clear                -- clear all breakpoints\n");

	Print("\n");
	Print("---------- belows are available when hit breakpoint ----------\n");
	Print("c                    -- continue\n");
	Print("n                    -- next\n");
	Print("s                    -- step in\n");
	Print("bt                   -- print call stack\n");
}

void CLuaDebugger::HandleCmdPrint(const char* pOp, uint32 iOplen, lua_Debug* pAr)
{
	if ( iOplen == 0 )
	{
		Print("Incorrect use of 'p'\n");
		Print("p <var>\n");
		return;
	}
	
	PrintExpression(m_pLuaState, pOp, pAr);
}

void CLuaDebugger::SingleStep( bool isStep )
{
	if (isStep)
	{
		EnableLineHook(true);
	}
	m_bIsStep = isStep;
}

void CLuaDebugger::StepIn()
{
	if (m_bIsStep)
	{
		SingleStep(true);
	}
	m_iCallDepth = -1;
}

bool CLuaDebugger::HandlecmdContinue(const char* pCmd, const char* pOp, uint32 iOplen, lua_Debug* pAr)
{
	Print("Continue...\n");
	SingleStep(false);
	m_iCallDepth = -1;
	return false;
}

bool CLuaDebugger::HandleCmdNext(const char* pCmd, const char* pOp, uint32 iOplen, lua_Debug* pAr)
{
	if (!m_bIsStep)
	{
		SingleStep(true);
	}
	
	return false;
}

bool CLuaDebugger::HandleCmdStep(const char* pCmd, const char* pOp, uint32 iOplen, lua_Debug* pAr)
{
	StepIn();	
	return false;
}

bool CLuaDebugger::HandleCmdBackTrace(const char* pCmd, const char* pOp, uint32 iOplen, lua_Debug* pAr)
{
	lua_Debug ldb;
	for (int i = 0; lua_getstack(m_pLuaState, i, &ldb); i++)
	{
		lua_getinfo( m_pLuaState, "Slnu", &ldb);
		const char* pName = ldb.name;
		if ( !pName )
		{
			pName = "";
		}
		const char* pFileName = ldb.source;
		print("#%d: %s:'%s', '%s' line %d\n", i+1, ldb.what, pName, pFileName, ldb.currentline);
	}
	return true;
}
