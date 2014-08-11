#ifndef socketserver_h
#define socketserver_h
#include "iocpSocket.h"
#include "lua.hpp"
#pragma comment(lib,"lua52.lib")

class Client : public CustomSocketContext{
private:
	char _recvBuf[MAX_CLIENT_BUFLEN];
	int _recvlen;
public:
	Client(CustomIocpSocketServer *server);
	~Client();
protected:
	int onRecv(const char * buf, const int buflen);
};

class IocpSocketServer : public CustomIocpSocketServer{
protected:
	CustomSocketContext * getSocketContext();
	Worker *getWorker();
	void onAccept(CustomSocketContext * client);
	void onDeleteSocketContext(CustomSocketContext * client);
public:
	IocpSocketServer() : 
		CustomIocpSocketServer(), 
		_recvcallback(-1), 
		_acceptcallback(-1),
		_disconnectcallback(-1){};
	int _recvcallback;
	int _acceptcallback;
	int _disconnectcallback;
};
int luaopen_iocpsocket(lua_State *L);
#endif