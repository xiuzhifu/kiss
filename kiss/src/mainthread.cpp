#include "dispatchmessage.h"
#include<list>
int _callback = -1;
int luaopen_serialize(lua_State *L);
using  std::string;
static std::list<msgnode*> _msgqueue;
static unsigned long _lock;
#define LOCK(l) while(InterlockedCompareExchange(&l, 1, 0) == 1) {};
#define UNLOCK(l) InterlockedExchange(&l, 0);
bool addMsgNode(msgnode* node){
	LOCK(_lock)
	_msgqueue.push_back(node);
	UNLOCK(_lock)
	return true;
}

bool addMsgNodeToFront(msgnode* node){
	LOCK(_lock)
		_msgqueue.push_front(node);
	UNLOCK(_lock)
		return true;
}

msgnode * popMsgNode(){
	msgnode* node = nullptr;
	LOCK(_lock)
	if (!_msgqueue.empty()){
		node = _msgqueue.front();
		_msgqueue.pop_front();
	}
	UNLOCK(_lock)
	return node;
}

void setLuaCallBack(int callback){
	_callback = callback;
}

int getLuaCallBack(){
	return _callback;
}

bool _call(lua_State *L, int argc, int retn){
	int err = lua_pcall(L, argc, retn, 0);
	if (err) {
		fprintf(stderr, "%s\n", lua_tostring(L, -1));
		lua_close(L);
		return false;
	}
	return true;
}

int worker_thread(const char * path, const char * config){
	lua_State *L = nullptr;
	L = luaL_newstate();
	luaL_openlibs(L);
	luaL_requiref(L, "socketdriver.c", luaopen_iocpsocket, 0);
	luaL_requiref(L, "serialize", luaopen_serialize, 0);
	lua_settop(L, 0);
	int err = luaL_loadfile(L, config);
	if (err != LUA_OK) {
		fprintf(stderr, "%s\n", lua_tostring(L, -1));
		lua_close(L);
		return 1;
	}
	lua_pushlstring(L, path, strlen(path));
	err = lua_pcall(L, 1, 0, 0);
	if (err != LUA_OK) {
		fprintf(stderr, "%s\n", lua_tostring(L, -1));
		lua_close(L);
		return 1;
	}
	lua_settop(L, 0);
	lua_gc(L, LUA_GCRESTART, 0);
	for (;;){
		size_t tick = GetTickCount64();
		msgnode * msg = popMsgNode();
		if (msg){
			if (msg->cmd == CMD_RECV){
				if (!dispatch_message(msg))
					dispatch_message_lua(L, msg);
			}
			else if (msg->cmd == CMD_ACCEPT){
				IocpSocketServer * server = (IocpSocketServer *)msg->client->_server;
				if (server->_acceptcallback != -1){
					lua_rawgeti(L, LUA_REGISTRYINDEX, server->_acceptcallback);
					int id = msg->client->getId();
					lua_pushinteger(L, id);
				}
				_call(L, 1, 0);
				delete msg;
			}
			else if (msg->cmd == CMD_DISCOONECT){
				IocpSocketServer * server = (IocpSocketServer *)msg->client->_server;
				if (server->_disconnectcallback != -1){
					lua_rawgeti(L, LUA_REGISTRYINDEX, server->_disconnectcallback);
					lua_pushinteger(L, msg->client->getId());
					_call(L, 1, 0);
				}
				delete msg->client;
				delete msg;
			}
		}
		if (poll_message(tick)) continue;
		if (_callback != -1){
			lua_rawgeti(L, LUA_REGISTRYINDEX, _callback);
			lua_pushinteger(L, tick);
			_call(L, 1, 1);
			int close = lua_toboolean(L, -1);
			lua_remove(L, -1);//删除返回值
			if (!close){
				break;
			}
		}
	}
	lua_close(L);
	return 0;
}

int mainthread(const char * config){
	lua_State * ls = luaL_newstate();
	char path[256];
	GetModuleFileNameA(NULL, path, 256); //得到当前模块路径
	(strrchr(path, '\\'))[1] = '\0';
	string s(path);
	s.append("config\\");
	if (config) s.append(config); else s.append("config.txt");
	luaopen_base(ls);
	luaL_openlibs(ls);
	int err = luaL_dofile(ls, s.c_str());
	if (err) {
		fprintf(stderr, "%s\n", lua_tostring(ls, -1));
		lua_close(ls);
		return 1;
	}
	lua_getglobal(ls, "startfile");
	const char* startfile = lua_tostring(ls, -1);
	s.clear();
	s.append(path);
	s.append(startfile);
	lua_close(ls);
	return worker_thread(path, s.c_str());
}