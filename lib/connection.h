#include <winsock2.h>
#include <thread>
#define SEND_BUF 32 * 1024
#define RECV_BUF 64 * 1024
#pragma comment(lib,"mswsock.lib")
#pragma comment(lib,"ws2_32.lib")
class Connection{
private:
	struct sockaddr_in remoteAddr;
	SOCKET _sockRemote;
	char _sendBuf[SEND_BUF];
	char _recvBuf[RECV_BUF];
	int _sendLen;
	int _recvLen;
	bool _isCloseed;
	const char * _ip;
	unsigned short _port;
protected:
	virtual int onRecv(const char* buf, int buflen){
		return 0;
	};
public:
	Connection() :_sendLen(0), _recvLen(0), _ip(nullptr), _port(0){
		WSADATA wsa;
		if (WSAStartup(MAKEWORD(2, 2), &wsa) != NO_ERROR) return;
		ZeroMemory(_sendBuf, sizeof(_sendBuf));
		ZeroMemory(_recvBuf, sizeof(_sendBuf));
	}
	bool isCloseed()const{
		return _isCloseed;
	}
	void doRecv(){
		while (1) {
			int recvlen = recv(_sockRemote, &_recvBuf[_recvLen], RECV_BUF - _recvLen, 0);
			if ((recvlen == SOCKET_ERROR) || (recvlen == INVALID_SOCKET) || (recvlen == 0)) {
				closesocket(_sockRemote);
				_isCloseed = true;
				break;
			}
			_recvLen += recvlen;
			recvlen = onRecv(_recvBuf, _recvLen);
			_recvLen -= recvlen;
		}
	}
	void Listen(unsigned short port){
		_isCloseed = false;
		remoteAddr.sin_family = AF_INET;
		remoteAddr.sin_port = htons(port);
		remoteAddr.sin_addr.S_un.S_addr = INADDR_ANY;
		SOCKET _sockLocal = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		
		if (bind(_sockLocal, (struct sockaddr *)&remoteAddr, sizeof(remoteAddr)) == SOCKET_ERROR) return;
		if (listen(_sockLocal, 1) == SOCKET_ERROR) return;
		int addrlen = sizeof(struct sockaddr);
		_sockRemote = accept(_sockLocal, (struct sockaddr *)&remoteAddr, &addrlen);
		std::thread t(&Connection::doRecv, this);
		t.join();
	}
	bool Connect(const char* host, unsigned short port){
		if (!host) return false;
		_isCloseed = false;
		_ip = host;
		_port = port;
		struct sockaddr_in _name;
		_name.sin_addr.s_addr = inet_addr(host);
		_name.sin_port = htons(port);
		_name.sin_family = AF_INET;
		_sockRemote = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (connect(_sockRemote, (struct sockaddr*)&_name, sizeof(_name)) == 0){
			std::thread t(&Connection::doRecv, this);
			t.detach();
			return true;
		}
		else { 
			return false; }
	}
	bool reConnect(){
		return Connect(_ip, _port);
	}
	bool Send(const char* buf, int buflen){
		if (_sendLen + buflen > SEND_BUF) return false;
		memcpy((void *)&_sendBuf[_sendLen], buf, buflen);
		_sendLen += buflen;
		int sended = send(_sockRemote, _sendBuf, _sendLen, 0);
		if (sended != _sendLen){
			_sendLen -= sended;
			memcpy((void *)&_sendBuf[0], (void *)_sendBuf[sended], _sendLen);
		}
		else{
			_sendLen = 0;
		}
		
		return true;
	}
};