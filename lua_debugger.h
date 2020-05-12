

/*******************************************************************
** File Name:       lua_debugger.cpp
** Copy Right:      neil deng
** File creator:    neil deng
** Date:            2020-05-11
** Version:
** Desc:            lua debuger tool
********************************************************************/


#ifndef _LUA_DEBUGGER_H_
#define _LUA_DEBUGGER_H_

extern "C"
{
	#include <lua.h>
	#include <lauxlib.h>
	#include <lualib.h>
}

#include <assert.h>
#include "base_define.h"

#define LUA_MAX_BRKS 64

class CLuaDebugger;
class CSourceMgr;

class CBreakPoint
{
	public:
		CBreadPOint()
		{ 
			Reset(); 
		}

		virtual ~CBreadPOint() {}
	public:
		enum BPType { None = 0, User, Debugger };
		inline void Reset()
		{	
			m_type = None;
			m_iHitCount = 0;
			m_iLineNum = 0;
			memset(m_sName, 0, sizeof(m_sName));
			memset(m_sNameWhat, 0, sizeof(m_sNameWhat));
		}

		inline uint32 GetLine() { return m_iLineNum; }
		inline BPType GetType() { return m_type; }
		inline Hit() { return ++m_iHitCount}
		
		inline void SetName(const char * pStr) { strncpy(m_sName, pStr, 512); }
		const char* GetName() { return m_sName; }
		const char* GetNameWhat() { return m_sNameWhat; }

	private:
		BPType 	m_type;
		char 	m_sName[512];	//function name or filename
		uint32 	m_iLineNum;
		uint32	m_iHitCount;
		char 	m_sNameWhat[7];
};

class CLuaDebugger
{
	public:
		CLuaDebugger( lua_State* L);
		virtual ~CLuaDebugger() {}
	public:
		void HandleCommand(const char* pCmd, lua_Debug* pAr = NULL);
		bool HandleLdbCommand(const char* pCmd, lua_Debug* pAr = NULL);

		uint32 AddBreadPoint(const char* pFineName, uint32 lineNum, CBreakPoint::BPType type = CBreakPoint::User);
		uint32 AddBreakPoint(const char* pFuncName, const char* pNameWhat, CBreakPoint::BPType type = CBreakPoint::User);
		uint32 AddBreadPoint(const char* pFuncName);

		inline uint32 HitBreakPoint(int iBpnum)
		{
			assert(iBpnum >=0 && iBpnum < LUA_MAX_BRKS);
			assert( m_arrBp[iBpnum].GetType() != CBreakPoint::None);
			return m_arrBp[iBpnum].Hit();
		}

		inline void RemoveBreakPoint(int iBpnum)
		{
			assert(iBpnum >=0 && iBpnum < LUA_MAX_BRKS);
			m_arrBp[iBpnum],Reset();
		}

		const CBreakPoint* GetBreakPoint(int iBpnum) const
		{
			assert(iBpnum >=0 && iBpnum < LUA_MAX_BRKS);
			return &m_arrBp[iBpnum];
		}

		int SeachBreaPoint( lua_Debug* pAr ) const;

		void EnableLineHook( bool isEnable);
		void EnableFuncHook( bool isEnable);
		inline void EnableDebug( bool isEnable)
		{
			EnableLineHook(isEnable);
			EnableFuncHook(isEnable);
			m_bIsEnable = isEnable;
		}

		static void line_hook( lua_state* L, lua_Debug* pAr );
		static void func_hook( lua_state* L, lua_Debug* pAr );
		static void all_Hook( lua_state* L, lua_Debug* pAr);

		void OnEvent(int iBpnum, lua_Debug* pAr);
	public:
		void SetSearchPath(const char* pPath);
	protected:
		CBreakPoint m_arrBp[LUA_MAX_BRKS];
		bool 		m_bIsEnable;
		int  		m_iCallDepth;
		bool		m_bIsStep;
		char 		m_sSearchPath[4096];

		lua_State*  m_pLuaState;
		CSourceMgr	m_pSourceMgr;
	private:
		void SingleStep(bool isStep);
		void StepIn();
	private:
		typedef void (CLuaDebugger::*CommandHandler)(const char*, uint32, lua_Debug*);

		struct Entry
		{ 
			const char* cmd;
			CommandHandler handler;
		};
		static Entry m_entries[];

		typedef bool (CLuaDebugger::CommandHandlerLdb)(const char*, const char*, uint32, lua_Debug*);
		struct EntryLdb
		{ 
			const char* cmd;
			CommandHandlerLdb handler;
		};
		static EntryLdb m_entries_ldb[];

	private:
		void HandleCmdBreakPoint(const char* pOp, uint32 iOplen, lua_Debug* pAr);
		void HandleCmdPrint(const char* pOp, uint32 iOplen, lua_Debug* pAr);
		void HandleCmdClear(const char* pOp, uint32 iOplen, lua_Debug* pAr);
		void HandleCmdHelp(const char* pOp, uint32 iOplen, lua_Debug* pAr);
	
	private:
		bool HandleCmdContinue(const char* pCmd, const char* pOp, uint32 iOplen, lua_Debug* pAr);
		bool HandleCmdNext(const char* pCmd, const char* pOp, uint32 iOplen, lua_Debug* pAr);
		bool HandleCmdStep(const char* pCmd, const char* pOp, uint32 iOplen, lua_Debug* pAr);
		bool HandleCmdBackTrace(const char* pCmd, const char* pOp, uint32 iOplen, lua_Debug* pAr);
};

/********************************* file *********************************************/
class CFileSource
{
public:
	CFileSource()
	{
		m_iLineCount = -1;
		m_sFileName = "";
	}
	virtual ~CFileSource() {}

	bool LoadFile( const char* pFileName);
	inline void Reset()
	{
		m_sFileName = "";
		m_iLineCount = -1;
		m_iAccesCount = 0;
		m_lines.m_iNum = 0;
	}

	bool IsLoaded() const { return m_iLineCount != -1; }
	const char* GetLine(int l)
	{
		if(l > 0 && l < m_iLineCount)
		{
			++m_iAccesCount;
			return (const char*)m_lines[l].c_str();
		}
		return NULL;
	}

public:
	std::string m_sFileName;
	int m_iAccesCount;
private:
	int m_iLineCount;
	CAutoPtr<std::string, 4> m_lines;

}

class CSourceMgr
{
	public:
		CSourceMgr();
		virtual ~CSourceMgr() {}

		const char* LoadLine(const char* pFileName, int iLine);
		void ReloadFile();
	private:
		enum { max_num = 4 };
		CFileSource m_files[max_num];
}



template <typename T, int size = 4>
class CAutoPtr
{
	protected:
		T* m_ptr;
	public:
		int m_iSize;
		int m_iNum;
		CAutoPtr()
		{
			m_ptr = NULL;
			m_iSize = 0;
			m_iNum = 0;
		}
		virtual ~CAutoPtr()
		{
			if(m_ptr)
			{
				delete[] m_ptr;
			}
		}

		operator T*() { return m_ptr; }
		T& operator[] (int n)
		{
			assert( n >=0 && n < m_iSize );
			return m_ptr[n];
		}

		void resize();
};

template <typename T, int size>
void CAutoPtr<T, size>::resize()
{
	if ( m_ptr == NULL)
	{
		m_ptr = new T[szie];
		m_iSize = size;
	}
	else
	{
		int newSize = m_iSize + m_iSize / 2;
		T* temp = new T[newSize];
		for(int i = 0; i < m_iNum; ++i)
		{
			temp[i] = m_+ptr[i];
		}

		delete[] m_ptr;
		m_ptr = temp;
		m_iSize = newSize;
	}
}


#endif