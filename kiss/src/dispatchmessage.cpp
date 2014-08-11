#include "dispatchmessage.h"
bool dispatch_message(struct msgnode * msg){

	return false;
}

bool dispatch_message_lua(lua_State *L, struct msgnode * msg){
	IocpSocketServer * server = (IocpSocketServer *)msg->client->_server;
	if (server->_recvcallback == -1) return false;
	lua_rawgeti(L, LUA_REGISTRYINDEX, server->_recvcallback);
	lua_pushinteger(L, msg->client->getId());
	lua_pushinteger(L, msg->type);
	int argc = 0;
	void * tmp;
	switch (msg->type){
	case MESSAGE_TYPE_DEFINE:
		lua_pushinteger(L, (uint8_t)msg->data[0]);
		delete[] msg->data;
		argc = 3;
		break;
	case MESSAGE_TYPE_LUA:
		tmp = lua_newuserdata(L, msg->len);
		memcpy(tmp, msg->data, msg->len);
		delete[]msg->data;
		argc = 3;
		break;
	case MESSAGE_TYPE_PB:
		lua_pushlstring(L, msg->data, msg->len);
		delete[]msg->data;
		argc = 3;
		break;
	case MESSAGE_TYPE_RAW:
		lua_pushlstring(L, msg->data, msg->len);
		delete[]msg->data;
		argc = 3;
		break;
	default:
		argc = 2;
	}
	delete msg;
	int err = lua_pcall(L, argc, 0, 0);
	if (err) {
		fprintf(stderr, "%s\n", lua_tostring(L, -1));
		lua_close(L);
	}
	return false;
}

bool poll_message(size_t tick){
	return false;
}