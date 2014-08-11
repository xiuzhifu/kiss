#include"iocpSocket.h"
#include<iostream>
#include<assert.h>
#include <algorithm>

#define LOCK(l) while(InterlockedCompareExchange(&l, 1, 0) == 1) {};
#define UNLOCK(l) InterlockedExchange(&l, 0);

//只会在主线程调用
bool CustomSocketContext::Send(const char * buf, uint16_t buflen, uint16_t type)
{
	SendBuffer * sb = new SendBuffer;
	if (_mode != MESSAGE_TYPE_RAW){
		sb->buf = new char[buflen + 4];
		uint8_t lv = (uint8_t)buflen;
		uint8_t hv = (uint16_t)buflen >> 8;
		sb->buf[0] = (char)lv;
		sb->buf[1] = (char)hv;
		lv = (uint8_t)type;
		hv = (uint16_t)type >> 8;
		sb->buf[2] = lv;
		sb->buf[3] = hv;
		memcpy((void*)&sb->buf[4], buf, buflen);
		sb->buflen = buflen + 4;
	}
	else {
		sb->buf = new char[buflen];
		sb->buflen = buflen;
		memcpy((void*)&sb->buf[0], buf, buflen);
	}
	_sendList.push(sb);
	if (!_sending){
		_sending = true;
		_ioctx.reset();
		_ioctx.socket = this->socket;
		doSend(&_ioctx);
	}
	return true;
}

void CustomSocketContext::doSend(IoContext *ioctx){
	
	if (!_sendList.empty()){
		SendBuffer * sb = _sendList.front();
		_sendList.pop();
		memcpy(&ioctx->buf, sb->buf, sb->buflen);
		ioctx->wsabuf.len = sb->buflen;
		delete[] sb->buf;
		delete sb;
		if (!_server->postWrite(ioctx)){
			_server->deleteSocket(this);
		}
	}
	else{
		_sending = false;
	}
}

void CustomSocketContext::doRecv(const char * buf, const int buflen){
	LOCK(_lock);
	if (buflen + _recvbuflen < MAX_CLIENT_BUFLEN)
	{
		memcpy(_recvbuf, buf, buflen);
		_recvbuflen += buflen;
	}
	else
	{
		memcpy(_recvbuf, buf, MAX_CLIENT_BUFLEN - _recvbuflen);
		_recvbuflen = MAX_CLIENT_BUFLEN;
	}
	
	_recvbuflen -= onRecv((const char*)&_recvbuf, _recvbuflen);
	//if (_recvbuflen < 0) {
	//	int i = 100;
	//}
	UNLOCK(_lock); 
}


CustomIocpSocketServer::CustomIocpSocketServer()
{
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != NO_ERROR) return;
	_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	memset(_slot, NULL, sizeof(_slot));
	_worker = nullptr;
	_lpfnAcceptEx = nullptr;
	_lpfnGetAcceptExSockAddrs = nullptr;
}

CustomIocpSocketServer::~CustomIocpSocketServer()
{
	CloseHandle(_iocp);
	WSACleanup();
}

//要考虑多线程
bool CustomIocpSocketServer::insertSocket(CustomSocketContext *s)
{
	for(int i = 1; i < MAX_SOCKET; i++)
		if (InterlockedCompareExchange((LONG*)&_slot[i], (LONG)s,NULL) == NULL) {
			s->_id = i;
			return true;
		}
	return false;
}

void CustomIocpSocketServer::deleteSocket(CustomSocketContext *s)
{
	InterlockedExchange((LONG*)&_slot[s->_id % MAX_SOCKET], NULL);
	onDeleteSocketContext(s);	
	PostQueuedCompletionStatus(_iocp, 0, (ULONG_PTR)DISCONNECT, (LPOVERLAPPED)s);
}

CustomSocketContext * CustomIocpSocketServer::getSocket(int id){
	return _slot[id % MAX_SOCKET];
}

bool CustomIocpSocketServer::postAccept(IoContext *ioctx)
{
	ioctx->reset();
	ioctx->optype = op_accept;
	ioctx->socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == ioctx->socket) return false;
	DWORD dwbyte = 0;
	if (_lpfnAcceptEx(_listenContext->socket, ioctx->socket, ioctx->wsabuf.buf,
		ioctx->wsabuf.len - ((sizeof(SOCKADDR_IN)+16) * 2), sizeof(SOCKADDR_IN)+16,
		sizeof(SOCKADDR_IN)+16, &dwbyte, &ioctx->overlapped) == false){
		if (WSAGetLastError() != WSA_IO_PENDING){
			closesocket(ioctx->socket);
			return false;
			//      writeln('投递 AcceptEx 请求失败，错误代码', WSAGetLastError())
		}
	}
	return true;
}

void CustomIocpSocketServer::doAccept(IoContext *ioctx)
{
	int remoteLen , localLen;

	SOCKADDR_IN* pClientAddr , pLocalAddr;
	_lpfnGetAcceptExSockAddrs(ioctx->wsabuf.buf, MAX_BUFFER_LEN - ((sizeof(SOCKADDR_IN)+16) * 2),
		sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, (LPSOCKADDR*)&pLocalAddr, &localLen, (LPSOCKADDR*)&pClientAddr, &remoteLen);
	CustomSocketContext *s = getSocketContext();
	s->reset();
	s->socket = ioctx->socket;
	s->addr = *pClientAddr;
	s->_addrs = inet_ntoa(s->addr.sin_addr);	
	if (CreateIoCompletionPort((HANDLE)s->socket, _iocp, ULONG_PTR(s), 0) == NULL) {
		closesocket(ioctx->socket);
		delete s;
		return;
	}
	insertSocket(s);
	onAccept(s);
	s->doRecv(ioctx->wsabuf.buf, ioctx->wsabuf.len);
	IoContext *m_ictx = new IoContext;
	m_ictx->reset();
	m_ictx->socket = s->socket;
	if (!postRead(m_ictx)) {
		delete m_ictx;
		return;
	}
	assert(postAccept(ioctx));
}

bool CustomIocpSocketServer::postRead(IoContext *ioctx)
{
	ioctx->optype = op_read;
	DWORD dwFlags = 0;
	ioctx->wsabuf.len = MAX_BUFFER_LEN;
	int irecv = WSARecv(ioctx->socket, &ioctx->wsabuf, 1, &ioctx->wsabuf.len, &dwFlags, &ioctx->overlapped, NULL);
	if ((SOCKET_ERROR == irecv) && (WSA_IO_PENDING != WSAGetLastError()))
		return false;
	else
		return true;
}

bool CustomIocpSocketServer::doRead(CustomSocketContext *s, IoContext *ioctx)
{
	s->doRecv(ioctx->wsabuf.buf, ioctx->wsabuf.len);
	return postRead(ioctx);
}

bool CustomIocpSocketServer::postWrite(IoContext *ioctx)
{
	DWORD flags = 0;
	ioctx->optype = op_write;
	int irecv = WSASend(ioctx->socket, &ioctx->wsabuf, 1, &ioctx->wsabuf.len, flags, &ioctx->overlapped, NULL);
	if ((SOCKET_ERROR == irecv) && (WSA_IO_PENDING != WSAGetLastError()))
		return false;
	else
		return true;
}

void CustomIocpSocketServer::doWrite(CustomSocketContext *s, IoContext *ioctx)
{
	s->doSend(ioctx);
}

CustomSocketContext* CustomIocpSocketServer::Connect(char * ip, unsigned short port, CustomSocketContext* client)
{
	if (!ip) return nullptr;
	_ip = ip;
	_port = port;
	struct sockaddr_in _name;
	_name.sin_addr.s_addr = inet_addr(ip);
	_name.sin_port = htons(port);
	_name.sin_family = AF_INET;
	SOCKET _sockRemote = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (connect(_sockRemote, (struct sockaddr*)&_name, sizeof(_name)) == 0){
		CustomSocketContext *s = client;
		if (s == nullptr)
			s = getSocketContext();
		s->reset();
		s->socket = _sockRemote;
		s->addr = _name;

		if (CreateIoCompletionPort((HANDLE)s->socket, _iocp, ULONG_PTR(s), 0) == NULL) {
			delete s;
			return false;
		}
		insertSocket(s);
		IoContext *m_ictx = new IoContext;
		m_ictx->reset();
		m_ictx->socket = s->socket;
		if (!postRead(m_ictx)) {
			delete m_ictx;
			return nullptr;
		}
		return s;
	}
	else {
		return nullptr;
	}
}

bool CustomIocpSocketServer::Listen(const char * host, const int port)
{
	GUID GuidAcceptEx = WSAID_ACCEPTEX;
	GUID GuidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;
	DWORD dwBytes = 0;
	SOCKET socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (socket == INVALID_SOCKET) return false;
	_listenContext = getSocketContext();
    _listenContext->socket = socket;
	if (CreateIoCompletionPort((HANDLE)socket, _iocp, ULONG_PTR(&_listenContext), 0) == NULL) goto _failed;
	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.S_un.S_addr = INADDR_ANY;
	if (host) addr.sin_addr.S_un.S_addr = inet_addr(host);
	if (bind(socket, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) goto _failed;

	if (listen(socket, SOMAXCONN) == SOCKET_ERROR) goto _failed;

	// 获取AcceptEx函数指针
	if (!_lpfnAcceptEx)
		if(SOCKET_ERROR == WSAIoctl( _listenContext->socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidAcceptEx,
			sizeof(GuidAcceptEx), &_lpfnAcceptEx, sizeof(_lpfnAcceptEx), &dwBytes, NULL, NULL)) goto _failed;

	// 获取GetAcceptExSockAddrs函数指针，也是同理
	if (!_lpfnGetAcceptExSockAddrs)
		if(SOCKET_ERROR == WSAIoctl(_listenContext->socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidGetAcceptExSockAddrs,
			sizeof(GuidGetAcceptExSockAddrs), &_lpfnGetAcceptExSockAddrs,
				sizeof(_lpfnGetAcceptExSockAddrs), &dwBytes, NULL, NULL)) goto _failed;

	for(int i = 0; i<MAX_POST_ACCEPT; i++)
	{
		IoContext * ioctx = new IoContext;
		if(!postAccept(ioctx)) { delete ioctx; goto _failed;}
	}
	return true;
	_failed:
	closesocket(socket);
	return false;
}

void CustomIocpSocketServer::start()
{
	_worker = new Worker;
	_worker->start(this);
	_worker->Join();
	_stoped = false;
}

void CustomIocpSocketServer::stop()
{
	if (!_stoped){
		_stoped = true;
		_worker->stop();
	}
}

bool Worker::start(CustomIocpSocketServer *iocpss)
{
	Iocpss = iocpss;
	m_threadCount = getWorkThreadCount();
	m_threads = new std::thread[m_threadCount];
	for (int i = 0; i < m_threadCount; i++) {
		m_threads[i] = std::thread(&Worker::doWork, this);
	}
	return true;
}

void Worker::stop()
{
	PostQueuedCompletionStatus(Iocpss->_iocp, 0, DWORD(SHUTDOWN), NULL);
}

void Worker::Join()
{
	for (int i = 0; i <m_threadCount; i++) {
		m_threads[i].detach();
	}
}

void Worker::doWork()
{
	DWORD dwBytesTransfered = 0;
	CustomSocketContext *s = nullptr;
	IoContext	*ioctx;
	ULONG_PTR key = NULL;
	for(; ;)
	{
		BOOL ret = GetQueuedCompletionStatus(Iocpss->_iocp, &dwBytesTransfered, &key,(LPOVERLAPPED*)&ioctx, INFINITE);
		if (key == SHUTDOWN) {
			PostQueuedCompletionStatus(Iocpss->_iocp, 0, DWORD(SHUTDOWN), NULL);
			break;
		}
		else if (key == DISCONNECT){
			s = (CustomSocketContext*)ioctx;
			s->closeSocket();
			continue;
		}
		if (ret)
		{
			s = (CustomSocketContext*)key;
			if ((dwBytesTransfered ==0)&&((ioctx->optype == op_read)||(ioctx->optype == op_write))) {
				Iocpss->deleteSocket(s);
				continue;
			}
			else
			{
				ioctx->wsabuf.len = dwBytesTransfered;
				switch(ioctx->optype){
					case op_accept: Iocpss->doAccept(ioctx); break;
					case op_read: if (!Iocpss->doRead(s, ioctx)) Iocpss->deleteSocket(s); break;
					case op_write: Iocpss->doWrite(s, ioctx); break;
				default:
	//            socket_server_error('ic.optype error!')
					;
				}
			}
		}
	}
	printf("thread id: %d exit\n", GetCurrentThreadId());
}

int Worker::getCpuCount()
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return si.dwNumberOfProcessors;
}

int Worker::getWorkThreadCount()
{
	return getCpuCount();
}



