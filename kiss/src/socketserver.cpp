#include "socketserver.h"
#include "mainthread.h"


Client::Client(CustomIocpSocketServer *server) :CustomSocketContext(server)
{
}
Client::~Client()
{

}
int Client::onRecv(const char * buf, const int buflen)
{
	if (this->_mode!= MESSAGE_TYPE_RAW){
		if (buflen < 4) return 0;
		uint8_t lv = (uint8_t)buf[0];
		uint8_t hv = (uint8_t)buf[1];
		uint16_t datalen = hv << 8 | lv;
		if (datalen <= 0 || buflen < datalen + 4) return 0;
		msgnode * node = new msgnode;
		lv = (uint8_t)buf[2];
		hv = (uint8_t)buf[3];
		uint16_t type = hv << 8 | lv;
		node->data = new char[datalen];
		node->len = datalen;
		node->type = type;
		node->client = this;
		node->cmd = CMD_RECV;
		memcpy(node->data, &buf[4], datalen);
		addMsgNode(node);
		return datalen + 4;
	}
	else{
		msgnode * node = new msgnode;
		node->data = new char[buflen];
		node->len = buflen;
		node->type = MESSAGE_TYPE_RAW;
		node->client = this;
		node->cmd = CMD_RECV;
		memcpy(node->data, &buf[0], buflen);
		addMsgNode(node);
		return buflen;
	}
}


CustomSocketContext * IocpSocketServer::getSocketContext()
{
	return new Client(this);
}
Worker *IocpSocketServer::getWorker()
{
	return new Worker();
}

void IocpSocketServer::onAccept(CustomSocketContext * client){
	if (this->_acceptcallback){
		msgnode * node = new msgnode;
		node->client = (Client *)client;
		node->cmd = CMD_ACCEPT;
		addMsgNodeToFront(node);
	}
}

void IocpSocketServer::onDeleteSocketContext(CustomSocketContext * client){
	if (this->_disconnectcallback){
		msgnode * node = new msgnode;
		node->client = (Client *)client;
		node->cmd = CMD_DISCOONECT;
		addMsgNode(node);
	}

}

static int lcreatesocket(lua_State *L) {

	IocpSocketServer * iocp = new IocpSocketServer;
	lua_pushlightuserdata(L, iocp);
	return 1;
}

static int lfreesocket(lua_State *L) {
	IocpSocketServer * iocp = (IocpSocketServer *)lua_touserdata(L, -1);
	delete iocp;
	return 0;
}


static int lconnect(lua_State *L) {
	int num = lua_gettop(L);
	if (num != 3) return 0;
	IocpSocketServer * iocp = (IocpSocketServer *)lua_touserdata(L, 1);
	char* ip = (char *)lua_tostring(L, 2);
	unsigned short port = (unsigned short)lua_tointeger(L, 3);
	CustomSocketContext * client = iocp->Connect(ip, port);
	if (client){
		lua_pushinteger(L, client->getId());
		return 1;
	}
	else {
		return 0;
	}

}

static int llisten(lua_State *L){
	IocpSocketServer * iocp = (IocpSocketServer *)lua_touserdata(L, 1);
	int num = lua_gettop(L);
	if (num == 2){
		unsigned short port = (unsigned short)lua_tointeger(L, 2);
		bool ret = iocp->Listen(nullptr, port);
		lua_pushboolean(L, ret);
		return 1;
	}
	else if (num == 2){
		const char* ip = lua_tostring(L, 2);
		unsigned short port = (unsigned short)(L, 3);
		iocp->Listen(ip, port);
		bool ret = iocp->Listen(nullptr, port);
		lua_pushboolean(L, ret);
		return 1;
	}
	else {
		return 0;
	}

}

static int lsend(lua_State *L){
	int num = lua_gettop(L);
	if (num > 3) {
		IocpSocketServer * iocp = (IocpSocketServer *)lua_touserdata(L, 1);
		int id = (int)lua_tointeger(L, 2);
		CustomSocketContext * client = iocp->getSocket(id);	
		if (!client) {
			lua_pushboolean(L, false);
			return 1;
		}
		const char * buf;	
		int len;
		if (client->_mode == MESSAGE_TYPE_RAW){
			if (num != 4) return 0;
			buf = lua_tostring(L, 3);
			len = (int)lua_tointeger(L, 4);
			lua_pushboolean(L, client->Send(buf, len));
			return 1;
		}
		uint8_t i;
		int type = (int)lua_tointeger(L, 3);
		switch (type){
			case MESSAGE_TYPE_DEFINE:  
				if (num != 4) return 0;
				i = (uint8_t)lua_tointeger(L, 4);
				buf = (const char *)&i;
				len = 1;
				break;
			case MESSAGE_TYPE_LUA: 
				if (num != 5) return 0;
				buf = (const char *)lua_touserdata(L, 4);  
				len = (int)lua_tointeger(L, 5);
				break;
			case MESSAGE_TYPE_PB:  
				if (num != 5) return 0;
				buf = lua_tostring(L, 4); 
				len = (int)lua_tointeger(L, 5);
				break;
			default:
				buf = nullptr;
				len = 0;
				return 0;
		}		
		lua_pushboolean(L, client->Send(buf, len, type));
		return 1;
	}
	return 0;
}

static int lrecv(lua_State *L){
	IocpSocketServer * iocp = (IocpSocketServer *)lua_touserdata(L, 1);
	lua_remove(L, 1);
	if (lua_isfunction(L, 1)) {
		int i = luaL_ref(L, LUA_REGISTRYINDEX);
		iocp->_recvcallback = i;
	}	
	return 0;
}

static int lstart(lua_State *L){
	int num = lua_gettop(L);
	if (num == 1){
		IocpSocketServer * iocp = (IocpSocketServer *)lua_touserdata(L, 1);
		iocp->start();
	}
	return 0;
}

static int lpoll(lua_State *L){
	if (lua_isfunction(L, 1)) {
		int i = luaL_ref(L, LUA_REGISTRYINDEX);
		setLuaCallBack(i);
	}
	return 0;
}

static int lstop(lua_State *L){
	IocpSocketServer * iocp = (IocpSocketServer *)lua_touserdata(L, 1);
	if (!iocp) return 0;
	iocp->stop();
	if (getLuaCallBack() != -1)
		luaL_unref(L, LUA_REGISTRYINDEX, getLuaCallBack());
	return 0;
}

static int lgetsocketaddr(lua_State *L){
	int num = lua_gettop(L);
	if (num != 2) return 0;
	IocpSocketServer * iocp = (IocpSocketServer *)lua_touserdata(L, 1);
	int id = (int)lua_tointeger(L, 2);
	CustomSocketContext * client = iocp->getSocket(id);
	if (!client) return 0;	
	lua_pushstring(L, client->getAddr());
	return 1;
}

static int lonaccept(lua_State *L){
	int num = lua_gettop(L);
	IocpSocketServer * iocp = (IocpSocketServer *)lua_touserdata(L, 1);
	lua_remove(L, 1);
	if (lua_isfunction(L, 1)) {
		int i = luaL_ref(L, LUA_REGISTRYINDEX);
		iocp->_acceptcallback = i;
	}
	return 0;
}

static int lkick(lua_State * L){
	int num = lua_gettop(L);
	if (num != 2) return 0;
	IocpSocketServer * iocp = (IocpSocketServer *)lua_touserdata(L, 1);
	int id = (int)lua_tointeger(L, 2);
	CustomSocketContext *client = iocp->getSocket(id);
	if (!client) return 0;
	iocp->deleteSocket(client);
	return 0;
}

static int londisconnect(lua_State * L){
	IocpSocketServer * iocp = (IocpSocketServer *)lua_touserdata(L, 1);
	lua_remove(L, 1);
	if (lua_isfunction(L, 1)) {
		int i = luaL_ref(L, LUA_REGISTRYINDEX);
		iocp->_disconnectcallback = i;
	}
	return 0;
}

static int lsetmode(lua_State * L){
	int num = lua_gettop(L);
	if (num != 3) return 0;
	IocpSocketServer * iocp = (IocpSocketServer *)lua_touserdata(L, 1);
	int fd = (int)lua_tointeger(L, 2);
	int mode = (int)lua_tointeger(L, 3);
	CustomSocketContext *client = iocp->getSocket(fd);
	if (!client) return 0;
	client->_mode = mode;
	return 0;
}

int luaopen_iocpsocket(lua_State *L){
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "createsocket", lcreatesocket },
		{ "freesocket", lfreesocket },
		{ "connect", lconnect },
		{ "listen", llisten },
		{ "send", lsend },
		{ "recv", lrecv },
		{ "start", lstart },
		{ "stop", lstop },
		{ "kick", lkick },
		{ "poll", lpoll},
		{ "getsocketaddr", lgetsocketaddr},
		{ "onaccept", lonaccept },
		{ "setmode", lsetmode },
		{ "ondisconnect", londisconnect },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}

