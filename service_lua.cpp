#include "service_lua.h"
#include "string_utils.h"
#include "fate_log.h"
#include "log.h"
#include "core_byte_buffer.h"
#include "timer_axis.h"
#include "core_new.h"
#include "core_random.h"
#include "lua_cmsgpack.h"
#include "luna_value.h"
#include "core_global_memory.h"
#include "core_properties.h"
#include "lua_debugger.h"

#include <sys/types.h>
#include <dirent.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "tolua++.h"
#include "lualib.h"
	int luaopen_protobuf_c(lua_State *L);
	int luaopen_sproto_core(lua_State *L);
	int luaopen_lpeg(lua_State *L);
	//int luaopen_socket_core(lua_State* L);
	int snapshot(lua_State *L);
	void tolua_openuint64(lua_State *L);
	void tolua_openint64(lua_State* L);

#ifdef __cplusplus
}
#endif
  
using namespace fate;
 
void lua_register_function(lua_State* L, const char* name, lua_CFunction func)
{
	lua_pushstring(L, name);
	lua_pushcfunction(L, func);
	lua_rawset(L, -3);
}

int get_string_for_print(lua_State * L, std::string* out)
{
	int n = lua_gettop(L);  /* number of arguments */
	int i;

    lua_Debug ar;
    if (lua_getstack(L, 3, &ar)) {
        lua_getinfo(L, "Sl", &ar);
    }

    static StrVec vec_str;
    vec_str.clear();
    StringTool::split(ar.source, '/', vec_str);

    char buf[1024];
    int last = vec_str.size() - 1;
    snprintf(buf, 1024,"[%s:%d] ", vec_str[last].c_str(), ar.currentline);
    *out += buf; 

	lua_getglobal(L, "tostring");
	for (i = 2; i <= n; i++) {
		const char*  s;
		lua_pushvalue(L, -1);  /* function to be called */
		lua_pushvalue(L, i);   /* value to print */
		lua_call(L, 1, 1);
		size_t sz;
		s = lua_tolstring(L, -1, &sz);  /* get result */
		if (s == NULL)
			return luaL_error(L, LUA_QL("tostring") " must return a string to "
				LUA_QL("print"));
		if (i > 2) out->append("\t");
		out->append(s, sz);
		lua_pop(L, 1);  /* pop result */
	}
	return 0;
}

typedef std::vector<std::string>	VStringList;
VStringList								global_lua_string;
VStringList								global_lua_error_string; 
 
 
int lua_str_register(lua_State* L)
{
	if (!lua_isstring(L, 1))
	{
		lua_pushnumber(L, 0);
		return 0;
	}

	const char* data = lua_tostring(L, -1); 
	global_lua_string.push_back(data); 
	std::string& format_data = global_lua_string[global_lua_string.size() - 1];
	CStringUtils::Replace(format_data, "%d", "%s");
	lua_pushnumber(L, global_lua_string.size() - 1);
	return 1;
}


int lua_error_str_register(lua_State* L)
{
	if (!lua_isstring(L, 1))
	{
		lua_pushnumber(L, 0);
		return 0;
	}

	const char* data = lua_tostring(L, -1);
	global_lua_error_string.push_back(data);
	std::string& format_data = global_lua_error_string[global_lua_error_string.size() - 1];
	CStringUtils::Replace(format_data, "%d", "%s");
	format_data += "\nerror stack:[%s]";
	lua_pushnumber(L, global_lua_error_string.size() - 1);
	return 1;
}



const char* lua_value_to_char(lua_State* L, int index)
{
	if (lua_isstring(L, index))
	{
		return lua_tostring(L, index);
	}
	
	else if(lua_isnumber(L, index))
	{
		long int value = lua_tonumber(L, index);
		static char buf[64];
		snprintf(buf, 64, "%ld", value);
		return buf;
	}
	else
	{
		return "nil";
	}
}


int lua_write_bill_log(lua_State* L)
{
	int nargs = lua_gettop(L);
	if (nargs < 2)
	{
		ERROR_LOG("lua_write_bill_log write error![%d]\n", nargs);
		return 0;
	}

	if (!lua_isnumber(L, 1))
	{
		ERROR_LOG("lua_write_bill_log type is not number\n");
		return 0;
	}

	static BufferCmdQueue bill_log_buf;
	if (bill_log_buf.max_size() == 0)
	{
		bill_log_buf.reset();
		bill_log_buf.wr_resize(1024 * 1024);
	}
	
	bill_log_buf.reset();
	const char* buf = lua_value_to_char(L, -2);
	bill_log_buf.put(buf, strlen(buf));
	int top_index = lua_gettop(L);
	lua_pushnil(L); 
	while (lua_next(L, top_index))
	{
		bill_log_buf.set_char('|');
		int sub_index = lua_gettop(L);
		lua_pushnil(L);
		while (lua_next(L, sub_index))
		{ 
			const char* key = lua_value_to_char(L, -2);
			const char* value = lua_value_to_char(L, -1);
			bill_log_buf.put(key, strlen(key));
			bill_log_buf.set_char('=');
			bill_log_buf.put(value, strlen(value));
			bill_log_buf.set_char(',');
			lua_pop(L, 1);
		} 
		lua_pop(L, 1);
	}
	lua_pop(L, 1);

	bill_log_buf.set_char('\0');

	lua_Debug ar;
	if (lua_getstack(L, 4, &ar)) {
		lua_getinfo(L, "Sl", &ar);
	}

	LUABILL(ar.short_src, ar.currentline, bill_log_buf.get_rd_buf());

	return 0;
}


int lua_write_error_log(lua_State* L)
{
	int nargs = lua_gettop(L);
	 
	if (!lua_isnumber(L, 1))
	{
		return 0;
	}
	 
	int format_index = lua_tonumber(L, 0 - nargs);

	if (uint32(format_index) >= global_lua_error_string.size())
	{
		return 0;
	}
	std::string& format_data = global_lua_error_string[format_index];

	
	std::string error_data;
	error_data.clear();
	int first_index = 4;
	for (int i = first_index; i <= 10; ++i)
	{
		lua_Debug step;
		if (lua_getstack(L, i, &step))
		{
			lua_getinfo(L, "Sl", &step);
		}
		else
		{
			break;
		}

		if (i == first_index)
		{
			CStringUtils::Format(error_data, "step:[%d]: file:[%s:%d]", i - first_index, step.short_src, step.currentline);
		}
		else
		{
			CStringUtils::Format(error_data, "%s\nstep:[%d]: file:[%s:%d]", error_data.c_str(), i - first_index, step.short_src, step.currentline);
		}
	}

	lua_Debug ar;
	if (lua_getstack(L, 4, &ar)) {
		lua_getinfo(L, "Sl", &ar);
	}


	switch (nargs - 1)
	{
	case 0:
		LUAERROR(ar.short_src, ar.currentline, format_data.c_str(), error_data.c_str());
		break;
	case 1:
		LUAERROR(ar.short_src, ar.currentline, format_data.c_str(), lua_value_to_char(L, -1), error_data.c_str());
		break;
	case 2:
		LUAERROR(ar.short_src, ar.currentline, format_data.c_str(), lua_value_to_char(L, -2), lua_value_to_char(L, -1), error_data.c_str());
		break;
	case 3:
		LUAERROR(ar.short_src, ar.currentline, format_data.c_str(), lua_value_to_char(L, -3), lua_value_to_char(L, -2), lua_value_to_char(L, -1), error_data.c_str());
		break;
	case 4:
		LUAERROR(ar.short_src, ar.currentline, format_data.c_str(), lua_value_to_char(L, -4), lua_value_to_char(L, -3), lua_value_to_char(L, -2), lua_value_to_char(L, -1), error_data.c_str());
		break;
	case 5:
		LUAERROR(ar.short_src, ar.currentline, format_data.c_str(), lua_value_to_char(L, -5), lua_value_to_char(L, -4), lua_value_to_char(L, -3), lua_value_to_char(L, -2), lua_value_to_char(L, -1), error_data.c_str());
		break;
	case 6:
		LUAERROR(ar.short_src, ar.currentline, format_data.c_str(), lua_value_to_char(L, -6), lua_value_to_char(L, -5), lua_value_to_char(L, -4), lua_value_to_char(L, -3), lua_value_to_char(L, -2), lua_value_to_char(L, -1), error_data.c_str());
		break;
	case 7:
		LUAERROR(ar.short_src, ar.currentline, format_data.c_str(), lua_value_to_char(L, -7), lua_value_to_char(L, -6), lua_value_to_char(L, -5), lua_value_to_char(L, -4), lua_value_to_char(L, -3), lua_value_to_char(L, -2), lua_value_to_char(L, -1), error_data.c_str());
		break;
	default:

		break;
	}
	return 0;
}



int lua_write_model_log(lua_State* L)
{
	int nargs = lua_gettop(L);

	if (!lua_isstring(L, 1))
	{
		return 0;
	}

	if (!lua_isnumber(L, 2))
	{
		return 0;
	} 
	
	const char* model_name = lua_tostring(L, 0 - nargs);
	int format_index = lua_tonumber(L, 0 - nargs + 1); 

	if (uint32(format_index) >= global_lua_string.size())
	{
		return 0;
	}
	std::string& format_data = global_lua_string[format_index];
	 
	lua_Debug ar;
	if (lua_getstack(L, 4, &ar)) {
		lua_getinfo(L, "Sl", &ar);
	}

	switch (nargs - 2)
	{
	case 0:
		LUAMODEL(model_name, ar.short_src, ar.currentline, format_data.c_str());
		break;
	case 1:
		LUAMODEL(model_name, ar.short_src, ar.currentline, format_data.c_str(), lua_value_to_char(L, -1));
		break;
	case 2:
		LUAMODEL(model_name, ar.short_src, ar.currentline, format_data.c_str(), lua_value_to_char(L, -2), lua_value_to_char(L, -1));
		break;
	case 3:
		LUAMODEL(model_name, ar.short_src, ar.currentline, format_data.c_str(), lua_value_to_char(L, -3), lua_value_to_char(L, -2), lua_value_to_char(L, -1));
		break;
	case 4:
		LUAMODEL(model_name, ar.short_src, ar.currentline, format_data.c_str(), lua_value_to_char(L, -4), lua_value_to_char(L, -3), lua_value_to_char(L, -2), lua_value_to_char(L, -1));
		break;
	case 5:
		LUAMODEL(model_name, ar.short_src, ar.currentline, format_data.c_str(), lua_value_to_char(L, -5), lua_value_to_char(L, -4), lua_value_to_char(L, -3), lua_value_to_char(L, -2), lua_value_to_char(L, -1));
		break;
	case 6:
		LUAMODEL(model_name, ar.short_src, ar.currentline, format_data.c_str(), lua_value_to_char(L, -6), lua_value_to_char(L, -5), lua_value_to_char(L, -4), lua_value_to_char(L, -3), lua_value_to_char(L, -2), lua_value_to_char(L, -1));
		break;
	case 7:
		LUAMODEL(model_name, ar.short_src, ar.currentline, format_data.c_str(), lua_value_to_char(L, -7), lua_value_to_char(L, -6), lua_value_to_char(L, -5), lua_value_to_char(L, -4), lua_value_to_char(L, -3), lua_value_to_char(L, -2), lua_value_to_char(L, -1));
		break;
	default:
		
		break;
	}
	return 0;
}

int lua_write_log(lua_State* L)
{
	if (!lua_isnumber(L, 1))
	{
		ERROR_LOG("lua_write_log failed!!lua_isnumber \n");
		return 0;
	}
	  
	int log_level = lua_tonumber(L, 1);
	std::string t;
	get_string_for_print(L, &t);

	t += "\n";
	 
	switch (log_level)
	{
	case LOG_LEVEL_DEBUG: DEBUG_LEVEL_LOG(LOG_LEVEL_DEBUG, "%s\n", t.c_str()); break;
	case LOG_LEVEL_TRACE: DEBUG_LEVEL_LOG(LOG_LEVEL_TRACE, "%s\n", t.c_str()); break;
	case LOG_LEVEL_ERR: ERROR_LOG("%s\n", t.c_str()); break;
	default:
		DEBUG_LEVEL_LOG(LOG_LEVEL_DEBUG, "%s", t.c_str());
		break;
	} 
	return 0;
}
 
int lua_check_dir_list(lua_State* L)
{
	string file_list = "";
	if (!lua_isstring(L, -1))
	{
		lua_pushstring(L, file_list.c_str());
		return 1;
	}

	DIR * dir = opendir(lua_tostring(L, -1));
	if (dir == NULL)
	{
		lua_pushstring(L, file_list.c_str());
		return 1;
	}
	struct dirent *ent = readdir(dir);
	while (ent != NULL)
	{
		if (ent->d_type == 8)
		{
			file_list += ent->d_name;
			file_list += "|";
		}
		ent = readdir(dir);
	}

	closedir(dir);
	lua_pushstring(L, file_list.c_str());
	return 1;
}


//static std::string lua_script_dir = "";
static char lua_script_dir[128] = { 0 };
//返回脚本的路径
int lua_get_script_path(lua_State* L)
{ 
	lua_pushstring(L, lua_script_dir);
	return 1;
}

const luaL_Reg global_functions[] =
{
	{ "print", lua_write_log },
	{ NULL, NULL }
};

int lua_ref_object(lua_State* L)
{
	if (lua_istable(L, 1)) 
	{
		int ref = lua_ref(L, true);
		lua_pushnumber(L, ref);
		return 1;
	}
	return 0;
}

int lua_unref_object(lua_State* L)
{
	if (lua_isnumber(L, 1)) {
		int ref = lua_tonumber(L, -1);
		lua_unref(L, ref);
		return 1;
	}
	return 0;
}

int lua_get_ref(lua_State* L) 
{
	if (lua_isnumber(L, 1)) {
		int ref = lua_tonumber(L, -1);
		lua_getref(L, ref);
		return 1;
	}
	return 0;
}

int lua_create_map(lua_State* L)
{
    static LuaMapData luaMap;
    luaMap.reset();
    tolua_pushusertype(L, (void*)&luaMap, "LuaMapData");
    return 1;
}

class LuaTimer : public TimerHandler
{
private:
	int		m_ref;
public:
	LuaTimer(int ref)
	{
		m_ref = ref;
	}
	 
	void onTimer(uint32 timer_id, uint32 real_interval)
	{
		lua_State* L = g_pstServiceLua->getLuaState();
		lua_getref(L, m_ref);
		if (!lua_istable(L, -1))
		{
			ERROR_LOG("%s:%d 对象绑定已经找不到", __FUNCTION__, __LINE__);
			return;
		}

		lua_getfield(L, -1, "onTimer");
		if (!lua_isfunction(L, -1))
		{
			ERROR_LOG("%s:%d 对象找不到这个方法:onTimer", __FUNCTION__, __LINE__);
			return;
		}
		lua_pushvalue(L, -2);
		lua_pushnumber(L, timer_id);
		lua_pushnumber(L, real_interval); 

		//没有返回值
        if (lua_pcall(L, 3, 0, 0) != 0)
        {
            lua_fail(L, __FILE__, __LINE__);
        }
	}
};
 

typedef std::map<int, LuaTimer*>	MLuaTimerList;

class LuaTimerManager : public Singleton<LuaTimerManager>
{
private:
	MLuaTimerList		m_timer_list;

public: 
	LuaTimerManager()
	{
		m_timer_list.clear();
	}

	LuaTimer * getTimer(int ref)
	{
		MLuaTimerList::iterator iter_timer = m_timer_list.find(ref);
		if (iter_timer == m_timer_list.end())
		{
			return NULL;
		}
		return iter_timer->second;
	}


	void bindTimer(int ref)
	{
		MLuaTimerList::iterator iter_timer = m_timer_list.find(ref);
		if (iter_timer != m_timer_list.end())
		{
			return;
		}
		m_timer_list[ref] = MMNEW LuaTimer(ref);
	}

	void releaseTimer(int ref)
	{
		MLuaTimerList::iterator iter_timer = m_timer_list.find(ref);
		if (iter_timer != m_timer_list.end())
		{
			delete iter_timer->second;
			m_timer_list.erase(iter_timer);
		}
	}

	void startTimer(int timer_handler, int timer_id, int tick, int times, const char* file_path)
	{
		LuaTimer* lua_timer = getTimer(timer_handler);
		if (lua_timer)
		{
			g_TimerAxis->setTimer(timer_id, tick, lua_timer, times, file_path);
		}
	}
	
	void stopTimer(int timer_handler, int timer_id)
	{
		LuaTimer* lua_timer = getTimer(timer_handler);
		if (lua_timer)
		{
			g_TimerAxis->killTimer(timer_id, lua_timer);
		}
	}
	 
};

#define g_luaTimerAxis		LuaTimerManager::get_singleton_ptr()

int lua_kill_timer(lua_State* L)
{
	if (!lua_isnumber(L, 1))
	{
		return 0;
	}

	if (!lua_isnumber(L, 2))
	{
		return 0;
	}

	int timer_handler = lua_tonumber(L, -2);
	int timer_id = lua_tonumber(L, -1);

	g_luaTimerAxis->stopTimer(timer_handler, timer_id);
	return 1;
}
  
//对象绑定
int lua_bind_timer(lua_State* L)
{
	if (lua_istable(L, 1))
	{
		int ref = lua_ref(L, true);
		lua_pushnumber(L, ref);
		g_luaTimerAxis->bindTimer(ref);
		return 1;
	}
	return 0;
}

int lua_release_timer(lua_State* L)
{
	if (lua_isnumber(L, 1)) {
		int ref = lua_tonumber(L, -1);
		lua_unref(L, ref);
		g_luaTimerAxis->releaseTimer(ref);
		return 1;
	}
	return 0;
}

int lua_math_random(lua_State* L)
{
	if (!lua_isnumber(L, 1))
	{
		return 0;
	}
	if (!lua_isnumber(L, 2))
	{
		return 0;
	}

	int min_number = lua_tonumber(L, -2);
	int max_number = lua_tonumber(L, -1);

	int random_data = randBetween(min_number, max_number);
	lua_pushnumber(L, random_data);
	return 1;
}



int lua_register_timer(lua_State* L)
{
	if (!lua_isnumber(L, 1))
	{
		return 0;
	}
	if (!lua_isnumber(L, 2))
	{
		return 0;
	}
	if (!lua_isnumber(L, 3))
	{
		return 0;
	}
	if (!lua_isnumber(L, 4))
	{
		return 0;
	}
	if (!lua_isstring(L, 5))
	{
		ERROR_LOG("register is error");
		return 0;
	}


	int timer_handler = lua_tonumber(L, -5);
	int timer_id = lua_tonumber(L,-4);
	int tick = lua_tonumber(L, -3);
	int times = lua_tonumber(L, -2);
	const char* file_path = lua_tostring(L, -1);

	g_luaTimerAxis->startTimer(timer_handler, timer_id, tick, times, file_path);
	return 1;
}
 
void fate_lua_open(lua_State* L, std::string& script_dir)
{
	strcpy(lua_script_dir, script_dir.c_str());
	if (script_dir.empty())
	{
		DEBUG("[luadir is empty\n]");
		return;
	}

	luaL_register(L, "_G", global_functions);

	int top = lua_gettop(L);
	lua_getglobal(L, "package");
	lua_getfield(L, -1, "path");
	std::string searchpath(lua_tostring(L, -1));
	std::string cpath = searchpath + ";" + script_dir + "?.lua";
	lua_getglobal(L, "package");
	lua_pushlstring(L, cpath.c_str(), cpath.size());
	lua_setfield(L, -2, "path");
	lua_settop(L, top);

	lua_guard g(L); //栈平衡保护
	tolua_open(L);

	lua_getglobal(L, "package");
	lua_pushstring(L, "loaded");
	lua_rawget(L, -2);
	lua_rawseti(L, LUA_REGISTRYINDEX, LUA_RIDX_LOADED);
	lua_pop(L, 1);
	tolua_openuint64(L);
	tolua_openint64(L);
	lua_pushvalue(L, LUA_GLOBALSINDEX);

	lua_register_function(L, "lua_ref_object", lua_ref_object);
	lua_register_function(L, "lua_unref_object", lua_unref_object);
	lua_register_function(L, "lua_get_object", lua_get_ref);
	lua_register_function(L, "lua_create_map", lua_create_map);
	lua_register_function(L, "write_log", lua_write_log);
	lua_register_function(L, "write_model_log", lua_write_model_log);
	lua_register_function(L, "write_error_log", lua_write_error_log);
	lua_register_function(L, "write_bill_log", lua_write_bill_log);
	lua_register_function(L, "str_register", lua_str_register);
	lua_register_function(L, "error_str_register", lua_error_str_register);  
	lua_register_function(L, "get_script_path", lua_get_script_path);
	lua_register_function(L, "lua_register_timer", lua_register_timer);
	lua_register_function(L, "math_random", lua_math_random);
	lua_register_function(L, "lua_bind_timer", lua_bind_timer);
	lua_register_function(L, "lua_kill_timer", lua_kill_timer);
	lua_register_function(L, "lua_release_timer", lua_release_timer);
	lua_register_function(L, "my_snap_shot", snapshot); 
	lua_register_function(L, "check_dir_list", lua_check_dir_list);
} 

static luaL_Reg luax_exts[] =
{
	//{ "socket.core", luaopen_socket_core },
	{ NULL, NULL }
};
 

void luaopen_lua_extensions(lua_State *L)
{
	// load extensions
	luaL_Reg* lib = luax_exts;
	lua_getglobal(L, "package");
	lua_getfield(L, -1, "preload");
	for (; lib->func; lib++)
	{
		lua_pushcfunction(L, lib->func);
		lua_setfield(L, -2, lib->name);
	}
	lua_pop(L, 2);

	//luaopen_luasocket_scripts(L);

	luaopen_cmsgpack_safe(L);
}

int tolua_Server_open(lua_State* tolua_S);

bool ServiceLua::ReloadLua(const char* root_dir, const char* server_dir )
{
    std::string script_dir = root_dir;
    std::string strict_init = "";
    strict_init = script_dir + "base/LuaPatch.lua";

    ANY_LOG("ServiceLua::reload strict_init:%s\n", strict_init.c_str());

    if( luaL_dofile(m_service_lua, strict_init.c_str()) > 0 )
    {
        lua_fail(m_service_lua, __FILE__, __LINE__);
        return false;
    }
    else
    {
        return true;
    }
}

bool ServiceLua::bindLuaVirtual(const char* root_dir, const char* server_dir, TSvrDefine* server_info, int group_id, time_t open_time)
{
	if (strlen(root_dir) <= 0 || strlen(server_dir) <= 0 || server_info == NULL)
	{
		ERROR_LOG("ServiceLua::bindLuaVirtual failed, because root_dir:%s server_dir:%s is null\n", root_dir, server_dir);
		return false;
	}
	ANY_LOG("ServiceLua::bindLuaVirtual beg root_dir:%s server_dir:%s, m_iSvrID:%d m_cSvrType:%d m_iGroupID:%d, m_lOpenTime:%lu\n", root_dir, server_dir,
		server_info->m_iSvrID, server_info->m_cSvrType, group_id, open_time);
	
	std::string script_dir = root_dir;
	std::string strict_init = "";
	//初始化新lua信息 
	m_service_lua = luaL_newstate();
	luaL_openlibs(m_service_lua); 
	fate_lua_open(m_service_lua, script_dir);
	luaopen_protobuf_c(m_service_lua);
	luaopen_sproto_core(m_service_lua);
	luaopen_lpeg(m_service_lua);
	luaopen_lua_extensions(m_service_lua);
	tolua_Server_open(m_service_lua);
	strict_init = script_dir + "init.lua";
	ANY_LOG("ServiceLua::bindLuaVirtual strict_init:%s\n", strict_init.c_str());

#ifdef _DEBUG
	m_lua_debugger = new CLuaDebugger(m_service_lua);
	m_lua_debugger->SetSearchPatch(root_dir);
#endif
	
	luaL_loadfile(m_service_lua, strict_init.c_str());
	int status = lua_pcall(m_service_lua, 0, 0, 0);
	if (status != 0)
    {
		ERROR_LOG("ServiceLua::bindLuaVirtual call init:%s script failed \n", strict_init.c_str());
        lua_fail(m_service_lua, __FILE__, __LINE__);
        return false;
    } 
		
	Properties::PropertyMap_ConstIter iter_xml = CoreLib::global.m_properties.begin();
	while (iter_xml != CoreLib::global.m_properties.end())
	{
		lua_getglobal(m_service_lua, "gFunc_initSetupConfig");
		tolua_pushstring(m_service_lua, iter_xml->first.c_str());
		tolua_pushstring(m_service_lua, (const char*)iter_xml->second);
        if (lua_pcall(m_service_lua, 2, 0, 0) != 0)
        {
            ERROR_LOG("ServiceLua::bindLuaVirtual faild, call gFunc_initSetupConfig function failed!\n");
            lua_fail(m_service_lua, __FILE__, __LINE__);
            return false;
        }
		++iter_xml;
	}

	//初始化lua日志
	unsigned int ui_filter = 1;//LZ_NLOG->GetFilter();
	//for (int i = 1; i <= fate::ETrace;)
	{
		//if (ui_filter & i)
		{
			lua_getglobal(m_service_lua, "gFunc_setServiceLog");
			tolua_pushnumber(m_service_lua, ui_filter);//i);
            if (lua_pcall(m_service_lua, 1, 0, 0) != 0)
            {
                ERROR_LOG("ServiceLua::bindLuaVirtual faild, call gFunc_setServiceLog function failed!\n" );
                lua_fail(m_service_lua, __FILE__, __LINE__);
                return false;
            }
		}

		//i = i * 2;
	}

	lua_getglobal(m_service_lua, "gFunc_initServer");		
	tolua_pushstring(m_service_lua, server_dir);
	tolua_pushnumber(m_service_lua, server_info->m_iSvrID);
	tolua_pushnumber(m_service_lua, server_info->m_cSvrType);
	tolua_pushnumber(m_service_lua, group_id);
	tolua_pushnumber(m_service_lua, open_time);
	
    if (lua_pcall(m_service_lua, 5, 0, 0) != 0)
    {
        ERROR_LOG("ServiceLua::bindLuaVirtual faild , call gFunc_initServer function failed! \n" );
        lua_fail(m_service_lua, __FILE__, __LINE__);
        return false;
    }
	ANY_LOG("ServiceLua::bindLuaVirtual finished!\n");
	return true;
}


bool ServiceLua::resetLuaState(const char* root_dir, const char* server_dir, TSvrDefine* server_info, int group_id, time_t open_time)
{
	if (m_service_lua)
	{
		lua_close(m_service_lua);
	}

	bindLuaVirtual(root_dir, server_dir, server_info, group_id, open_time);
	return true;
}

bool ServiceLua::serviceInit()
{
	if (!m_service_lua)
	{
		return true;
	}
	lua_guard g(m_service_lua);
	/*lua_getglobal(m_service_lua, "gFunc_initServerIpPort");
	tolua_pushstring(m_service_lua, getServerInfo()->ip);
	tolua_pushnumber(m_service_lua, getServerInfo()->port);
	if (lua_pcall(m_service_lua, 2, 0, 0) != 0)
	{
		ERROR_LOG(lua_tostring(m_service_lua, -1));
		return false;
	}*/
	return true;
}






