#ifndef _iocpSocket_h
#define _iocpSocket_h

#define  MAX_SOCKET_P  16
#define  MAX_SOCKET  65535
#define MAX_SENDBUF 1024 * 1024
  // 缓冲区长度 (1024*8)
  // 之所以为什么设置8K，也是一个江湖上的经验值
  // 如果确实客户端发来的每组数据都比较少，那么就设置得小一些，省内存
#define  MAX_BUFFER_LEN  8192
  // 默认端口
#define  DEFAULT_PORT  12345
  // 默认IP地址
#define  DEFAULT_IP "127.0.0.1"
#define  MAX_POST_ACCEPT 1
#define  SHUTDOWN 0
#define  DISCONNECT ~1
#define MAX_CLIENT_BUFLEN 64 * 1024

#define MESSAGE_TYPE_DEFINE 1
#define MESSAGE_TYPE_LUA 2
#define MESSAGE_TYPE_PB 3
#define MESSAGE_TYPE_RAW 4

#include <winsock2.h>
#include <mswsock.h>
#include <queue>
#include <thread>
using std::vector;
#pragma comment(lib,"mswsock.lib")
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"libiocpsocket.lib")

enum  operationtype {op_null, op_read, op_write, op_accept};
struct SendBuffer {
	char * buf;
	int buflen;
};
class CustomSocketContext;
	struct IoContext {
		OVERLAPPED overlapped; // 每一个重叠网络操作的重叠结构(针对每一个Socket的每一个操作，都要有一个)
		SOCKET socket; // 这个网络操作所使用的Socket
		WSABUF wsabuf; // WSA类型的缓冲区，用于给重叠操作传参数的
		char buf[MAX_BUFFER_LEN]; // 这个是WSABUF里具体存字符的缓冲区
		operationtype optype; // 标识网络操作的类型(对应上面的枚举)
		void reset()
		{
			memset(&overlapped, 0, sizeof(OVERLAPPED));
			memset(buf, 0, MAX_BUFFER_LEN);
			socket= INVALID_SOCKET;
			wsabuf.buf = buf;
			wsabuf.len = MAX_BUFFER_LEN;
			optype = op_null;
		};
	};
	class CustomIocpSocketServer;
	class CustomSocketContext{
	private:
		SOCKET socket;
		SOCKADDR_IN addr;
		char *_addrs;
		int _id;
		std::queue<SendBuffer*> _sendList;
		IoContext _ioctx;
		int _sendCount;
		bool _sending;
		char _recvbuf[MAX_CLIENT_BUFLEN];
		int _recvbuflen;
		unsigned long _lock;
		int _mode;
		void doRecv(const char * buf, const int buflen);
	protected:
		virtual int onRecv(const char * buf, const int buflen) = 0;
	public:
		CustomIocpSocketServer *_server;
		void reset()
		{
			socket = 0;
			memset(&addr, 0, sizeof(addr));
			_lock = 0;
		}
		CustomSocketContext(CustomIocpSocketServer *server){
			_server = server;
			_sending = false;
			_sendCount = 0;
		    _recvbuflen = 0;
		}
		virtual ~CustomSocketContext(){ closeSocket();};
		bool Send(const char * buf, uint16_t buflen, uint16_t type);
		void doSend(IoContext *ioctx);
		int getId(){ return _id; }
		char * getAddr() {return _addrs;}
		int getMode(){ return _mode; }
		void closeSocket(){ if (socket) closesocket(socket);}
		friend  class CustomIocpSocketServer;
	};

	class Worker {
	private:
		CustomIocpSocketServer *Iocpss;
		std::thread * m_threads;
		int m_threadCount;
		int getCpuCount();
	protected:
		virtual int getWorkThreadCount();
	public:
		bool start(CustomIocpSocketServer *iocpss);
		void stop();
		void doWork();
		void Join();
	};

	class SingleWorker :public Worker {
	protected:
		int getWorkThreadCount() {
			return 1;
		}
	};

	class CustomIocpSocketServer{
	private:
		CustomSocketContext *_slot[MAX_SOCKET];
		CustomSocketContext *_listenContext;
		LPFN_ACCEPTEX _lpfnAcceptEx;                // AcceptEx 和 GetAcceptExSockaddrs 的函数指针，用于调用这两个扩展函数
		LPFN_GETACCEPTEXSOCKADDRS _lpfnGetAcceptExSockAddrs;
		Worker *_worker;
		HANDLE _iocp;
		char * _ip;
		bool _stoped;
		unsigned short _port;
		bool postRead(IoContext *ioctx);
		bool postAccept(IoContext *ioctx);
		bool postWrite(IoContext *ioctx);
		bool insertSocket(CustomSocketContext *s);
	public:
		CustomIocpSocketServer();
		virtual ~CustomIocpSocketServer();
		bool Listen(const char * host, const int port);
		void doAccept(IoContext *ioctx);
		bool doRead(CustomSocketContext *s, IoContext *ioctx);
		void doWrite(CustomSocketContext *s, IoContext *ioctx);
		CustomSocketContext* Connect(char * ip, unsigned short port, CustomSocketContext* client = nullptr);
		void deleteSocket(CustomSocketContext *s);
		CustomSocketContext * getSocket(int id);
		void start();
		void stop();
	protected:
		virtual CustomSocketContext * getSocketContext() = 0;
		virtual Worker *getWorker() = 0;
		virtual void onAccept(CustomSocketContext * client){};
		virtual void onDeleteSocketContext(CustomSocketContext * client){};
		friend class  CustomSocketContext;
	friend class Worker;
  };

#endif
