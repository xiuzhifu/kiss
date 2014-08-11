#ifndef _iocpSocket_h
#define _iocpSocket_h

#define  MAX_SOCKET_P  16
#define  MAX_SOCKET  65535
#define MAX_SENDBUF 1024 * 1024
  // ���������� (1024*8)
  // ֮����Ϊʲô����8K��Ҳ��һ�������ϵľ���ֵ
  // ���ȷʵ�ͻ��˷�����ÿ�����ݶ��Ƚ��٣���ô�����õ�СһЩ��ʡ�ڴ�
#define  MAX_BUFFER_LEN  8192
  // Ĭ�϶˿�
#define  DEFAULT_PORT  12345
  // Ĭ��IP��ַ
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
		OVERLAPPED overlapped; // ÿһ���ص�����������ص��ṹ(���ÿһ��Socket��ÿһ����������Ҫ��һ��)
		SOCKET socket; // ������������ʹ�õ�Socket
		WSABUF wsabuf; // WSA���͵Ļ����������ڸ��ص�������������
		char buf[MAX_BUFFER_LEN]; // �����WSABUF�������ַ��Ļ�����
		operationtype optype; // ��ʶ�������������(��Ӧ�����ö��)
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
		LPFN_ACCEPTEX _lpfnAcceptEx;                // AcceptEx �� GetAcceptExSockaddrs �ĺ���ָ�룬���ڵ�����������չ����
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
