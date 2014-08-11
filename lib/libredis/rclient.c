#include "rclient.h"
#include <string.h>

#pragma comment(lib, "WS2_32.lib")

/***
Get the position of the 'order' th occurrence of 'sym' value in 'src' string
src: the source string
sym: the string to find
order: the a few th times
***/
int indexofstr(const char *src, const char *sym, const int order)
{
	int i = 0, time = 0;

	const char * sourcestr = src;

	const char *flag;

	while(*sourcestr != '\0')
	{
		flag = strstr(sourcestr, sym);

		if(NULL == flag)
		{
			return -1;
		}

		if(sourcestr == flag)
		{
			time += 1;

			if(order == time)
			{
				return i;
			}
		}

		sourcestr ++;

		i ++;
	}

	return -1;
}


/***
Get sub string
src: the source string
start: the start index
end: the end index
dst: the dest string
***/
char* substring(const char *src, size_t start, size_t end, char *dst, size_t size)  
{
	size_t index = 0, len = 0;

	if(start > strlen(src))
		return NULL;  

	if(end > strlen(src))
		end = strlen(src);

	len = end - start;

	if(len > size)
	{
		len = size;
	}

	while(index < len)  
	{     
		dst[index] = src[index + start];  

		index++;
	}  

	dst[index]='\0';

	return dst;
}


/***
Relinquish resource of the rhnd
***/
static void r_delete(PREDIS* phnd)
{
	if(NULL == (*phnd))
	{
		return;
	}

	if((*phnd)->buf.data)
	{
		free((*phnd)->buf.data);
	}

	if((*phnd)->bulk.data)
	{
		free((*phnd)->bulk.data);
	}

	if(INVALID_SOCKET != (*phnd)->sock)
	{
		closesocket((*phnd)->sock);
	}

	free((*phnd));

	(*phnd) = NULL;
}

/***
Initialize resource
***/
static PREDIS r_new()
{
	PREDIS rhnd;

	if(NULL == (rhnd = (PREDIS)calloc(1, sizeof(r_client))) ||
		NULL == (rhnd->buf.data = (char*)malloc(R_BUFFER_SIZE)) ||
		NULL == (rhnd->bulk.data = (char*)malloc(sizeof(r_bulk)*R_BULK_COUNT))
		)
	{
		r_delete(&rhnd);

		return NULL;
	}

	memset(rhnd->buf.data, 0, R_BUFFER_SIZE);

	memset(rhnd->bulk.data, 0, sizeof(r_bulk)*R_BULK_COUNT);

	rhnd->buf.maxsize = R_BUFFER_SIZE;
	rhnd->buf.len = 0;

	rhnd->bulk.maxcount = R_BULK_COUNT;
	rhnd->bulk.num = 0;

	return rhnd;
}

/***
Verify if socket is writable
***/
static int r_select_write(SOCKET sock, unsigned short timeout)
{
	FD_SET fds;

	TIMEVAL tmv;
	tmv.tv_sec = timeout;
	tmv.tv_usec = 1;

	FD_ZERO(&fds);
	FD_SET(sock, &fds);

	return select(0, NULL, &fds, NULL, &tmv);
}

/***
Verify if socket is readable
***/
static int r_select_read(SOCKET sock, unsigned short timeout)
{
	FD_SET fds;

	TIMEVAL tmv;
	tmv.tv_sec = timeout;
	tmv.tv_usec = 1;

	FD_ZERO(&fds);
	FD_SET(sock, &fds);

	return select(0, &fds, NULL, NULL, &tmv);
}

/***
Send data
***/
static int r_send(SOCKET sock, char *data, int len, unsigned short timeout)
{
	int res = 0;
	int sent_size = 0;
	BOOL bsucc = TRUE;

	while (sent_size < len)
	{

		res = r_select_write(sock, timeout);

		if(0 < res)
		{
			res = send(sock, data + sent_size, len, 0);

			if(0 > res)
			{
				return res;
			}

			sent_size += res;

		}
		else
		{
			return res;
		}
	}

	return sent_size;
}

/***
Recv data
***/
static int r_receive(SOCKET sock, char *data, int size, unsigned short timeout)
{
	int res = 0;

	res = r_select_read(sock, timeout);

	if(0 < res)
	{
		return recv(sock, data, size, 0);
	}

	return res;
}

/***
Connect to the redis server
***/
PREDIS r_connect_server(const char *host, unsigned short port, unsigned short timeout)
{
	SOCKET sock;
	WSADATA wsdata;
	int keepalive = 1, nodelay = 1, mode = 1;
	PREDIS rhnd;

	SOCKADDR_IN sa;


	if(0 != WSAStartup(MAKEWORD(2,2), &wsdata))
	{
		printf("WSAStartup error\n");

		return NULL;
	}

	if(NULL == (rhnd = r_new()))
	{
		printf("r_new error\n");

		return NULL;
	}

	if(NULL == host)
	{
		host = "127.0.0.1";
	}

	if(port <= 0)
	{
		port = 6379;
	}

	if((INVALID_SOCKET == (sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))) ||
		(-1 == setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (const char*)&keepalive, sizeof(keepalive))) ||
		(-1 == setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay)))
		)
	{
		printf("sock error error : %ld\n", WSAGetLastError());

		goto error;
	}

	if(0 != ioctlsocket(sock, FIONBIO, (u_long*)&mode))
	{
		printf("ioctlsocket error : %ld\n", WSAGetLastError());

		goto error;
	}

	sa.sin_addr.S_un.S_addr = inet_addr(host);
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	memset(sa.sin_zero, 0, sizeof(sa.sin_zero));

	if(0 != connect(sock, (SOCKADDR*)&sa, sizeof(sa))
		&& WSAGetLastError() != 10035)
	{
		printf("connect error : %ld\n", WSAGetLastError());

		goto error;
	}

	if(r_select_write(sock, timeout) <= 0)
	{
		printf("r_select_write error\n");

		goto error;
	}

	strncpy_s(rhnd->ip, 32, host, 32);
	rhnd->port = port;
	rhnd->sock = sock;
	rhnd->timeout = timeout;
	rhnd->err = 0;


	printf("connect server :%s %d succ\n", host, port);

	return rhnd;

error:
	if(INVALID_SOCKET != sock)
	{
		closesocket(sock);
	}
	r_delete(&rhnd);
	return NULL;
}

/***
Reconnect to the redis server
***/
int r_reconnect_server(PREDIS rhnd)
{
	SOCKET sock;
	SOCKADDR_IN sa;
	int keepalive = 1, nodelay = 1, mode = 1;

	if(NULL == rhnd)
		return -1;

	if((INVALID_SOCKET == (sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))) ||
		(-1 == setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (const char*)&keepalive, sizeof(keepalive))) ||
		(-1 == setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay)))
		)
	{
		return -2;
	}

	if(0 != ioctlsocket(sock, FIONBIO, (u_long*)&mode))
	{
		return -2;
	}

	sa.sin_addr.S_un.S_addr = inet_addr(rhnd->ip);
	sa.sin_family = AF_INET;
	sa.sin_port = htons(rhnd->port);
	memset(sa.sin_zero, 0, sizeof(sa.sin_zero));

	if(0 != connect(sock, (SOCKADDR*)&sa, sizeof(sa))
		&& WSAGetLastError() != 10035)
	{
		return -2;
	}

	if(r_select_write(sock, rhnd->timeout) <= 0)
	{
		return -2;
	}

	rhnd->sock = sock;

	return 0;
}

/***
Close the connection
***/
void r_close_connect(PREDIS* phnd)
{
	if(NULL == (*phnd))
	{
		return;
	}

	closesocket((*phnd)->sock);

	r_delete(phnd);
}



/***
Send and recv data
***/
int r_send_and_recv(PREDIS rhnd, char *szsend, int sendsize)
{
	int res = 0;

	if(-1 != r_send(rhnd->sock, szsend, sendsize, rhnd->timeout))
	{
		memset(rhnd->buf.data, 0, R_BUFFER_SIZE);

		res = r_receive(rhnd->sock, rhnd->buf.data, rhnd->buf.maxsize, rhnd->timeout);
	}

	if(-1 == res || 0 == res)
	{
		return -1;
	}

	return res;
}

/***
Analyze set res info
***/
static int r_analyze_set_res(char *data, const char *sym)
{
	char buff[4];

	strncpy_s(buff, 4, data, 3);

	if(NULL != strstr(buff, sym))
	{
		return 0;
	}

	return -1;
}

/***
Execute expire
***/
int r_expire(PREDIS rhnd, const char *key, const unsigned short expiremin)
{
	int len = 0;

	int res = 0;

	if(NULL == rhnd)
		return -1;

	rhnd->err = 0;

	len = sprintf_s(rhnd->buf.data, rhnd->buf.maxsize, "expire %s %u\r\n", key, 60*expiremin);

	if(0 < (res = r_send_and_recv(rhnd, rhnd->buf.data, len)))
	{
		if(-1 != (res = r_analyze_set_res(rhnd->buf.data, ":")))
		{
			return 0;
		}
		else
		{
			rhnd->err = -1;

			strncpy_s(rhnd->errstr, 64, rhnd->buf.data, 63);
		}
	}
	else if(-1 == res)
	{
		return -2;
	}

	return 1;
}

/***
Execute hset
***/
int r_hset(PREDIS rhnd, const char *key, const char *field, const char *value, unsigned short expiremin)
{
	int len = 0;

	int res = 0;

	if(NULL == rhnd)
		return -1;

	rhnd->err = 0;


	len = sprintf_s(rhnd->buf.data, rhnd->buf.maxsize, "hset %s %s %s\r\n", key, field, value);

	if(0 < (res = r_send_and_recv(rhnd, rhnd->buf.data, len)))
	{
		if(-1 != (res = r_analyze_set_res(rhnd->buf.data, ":")))
		{
			if(0 < expiremin)
			{
				return r_expire(rhnd, key, expiremin);
			}

			return 0;
		}
		else
		{
			rhnd->err = -1;

			strncpy_s(rhnd->errstr, 64, rhnd->buf.data, 63);
		}
	}
	else if(-1 == res)
	{
		return -2;
	}

	return 1;
}

/***
Initialize hmset
***/
int r_init_hmset(PREDIS rhnd, const char *key)
{
	if(NULL == rhnd)
		return -1;

	rhnd->buf.len = sprintf_s(rhnd->buf.data, rhnd->buf.maxsize, "hmset %s ", key);

	return 0;
}

/***
Set hmset field value
***/
int r_add_hmset_info(PREDIS rhnd, char *field, char *value)
{
	char szbuf[1024];

	int flen = 0;

	memset(szbuf, 0, 1024);

	if(NULL == rhnd)
		return -1;

	flen = sprintf_s(szbuf, 1024, "%s %s ", field, value);

	if(rhnd->buf.len + flen > rhnd->buf.maxsize - 2)
	{
		return -1;
	}

	strncat_s(rhnd->buf.data, rhnd->buf.maxsize, szbuf, flen);

	rhnd->buf.len += flen;

	return 0;
}

/***
Execute hmset
***/
int r_hmset(PREDIS rhnd, const char *key, unsigned short expiremin)
{
	int res = 0;

	char szend[] = "\r\n";

	if(NULL == rhnd)
		return -1;

	rhnd->err = 0;

	strncat_s(rhnd->buf.data, rhnd->buf.maxsize, szend, sizeof(szend));

	rhnd->buf.len += 2;

	if(0 < (res = r_send_and_recv(rhnd, rhnd->buf.data, rhnd->buf.len)))
	{
		if(-1 != (res = r_analyze_set_res(rhnd->buf.data, "+OK")))
		{
			if(0 < expiremin)
			{
				return r_expire(rhnd, key, expiremin);
			}

			return 0;
		}
		else
		{
			rhnd->err = -1;

			strncpy_s(rhnd->errstr, 64, rhnd->buf.data, 63);
		}
	}
	else if(-1 == res)
	{
		return -2;
	}

	return 1;
}

/***
Execute set
***/
int r_set(PREDIS rhnd, const char *key, const char *value, unsigned short expiremin)
{
	int len = 0;

	int res = 0;

	if(NULL == rhnd)
		return -1;

	rhnd->err = 0;

	len = sprintf_s(rhnd->buf.data, rhnd->buf.maxsize, "set %s %s\r\n", key, value);

	if(0 < (res = r_send_and_recv(rhnd, rhnd->buf.data, len)))
	{
		if(-1 != (res = r_analyze_set_res(rhnd->buf.data, "+OK")))
		{
			if(0 < expiremin)
			{
				return r_expire(rhnd, key, expiremin);
			}

			return 0;
		}
		else
		{
			rhnd->err = -1;

			strncpy_s(rhnd->errstr, 64, rhnd->buf.data, 63);
		}
	}
	else if(-1 == res)
	{
		return -2;
	}

	return 1;
}

/***
Analyze hgetall res info
***/
static int r_analyze_hgetall_res(PREDIS rhnd, char *data)
{
	int num = 0, i = 0, fieldlen = 0, valuelen = 0;

	int fsidx = 0, feidx = 0, vsidx = 0, veidx = 0;

	char *info, field[160], value[80];

	r_bulk *curbulk;

	sscanf_s(data, "*%d", &num);

	num /= 2;

	if(num > rhnd->bulk.maxcount)
	{
		num = rhnd->bulk.maxcount;
	}

	rhnd->bulk.num = num;

	if(0 == num)
	{
		return 0;
	}

	info = strstr(data, "\r\n");

	info += 2;

	curbulk = (r_bulk*)rhnd->bulk.data;

	for(i = 0; i < num*4; i+=4)
	{
		fsidx = indexofstr(info, "\r\n", i + 1);

		feidx = indexofstr(info, "\r\n", i + 2);

		strcpy_s(curbulk->field, 160, substring(info, fsidx + 2, feidx, field, 160));

		vsidx = indexofstr(info, "\r\n", i + 3);

		veidx = indexofstr(info, "\r\n", i + 4);

		strcpy_s(curbulk->value, 80, substring(info, vsidx + 2, veidx, value, 80));

		curbulk += 1;

	}

	return 0;
}

/***
Execute hgetall
***/
int r_hgetall(PREDIS rhnd, const char *key)
{
	int len = 0;

	int res = 0;

	if(NULL == rhnd)
		return -1;

	rhnd->err = 0;

	len = sprintf_s(rhnd->buf.data, rhnd->buf.maxsize, "hgetall %s\r\n", key);

	if(0 < (res = r_send_and_recv(rhnd, rhnd->buf.data, len)))
	{
		if(-1 != (res = r_analyze_hgetall_res(rhnd, rhnd->buf.data)))
		{
			return 0;
		}
	}
	else if(-1 == res)
	{
		return -2;
	}

	return 1;
}

/***
Analyze get res info
***/
static int r_analyze_get_res(PREDIS rhnd, char *data)
{
	int num = 0, veidx = 0;

	char *info, value[80];

	r_bulk *curbulk;

	sscanf_s(data, "$%d", &num);

	if(num > rhnd->bulk.maxcount)
	{
		num = rhnd->bulk.maxcount;
	}

	rhnd->bulk.num = num;

	if(-1 == num)
	{
		rhnd->bulk.num = 0;

		return 0;
	}

	info = strstr(data, "\r\n");

	info += 2;

	curbulk = (r_bulk*)rhnd->bulk.data;

	veidx = indexofstr(info, "\r\n", 1);

	strcpy_s(curbulk->value, 80, substring(info, 0, veidx, value, 80));

	return 0;
}

/***
Execute get
***/
int r_get(PREDIS rhnd, const char *key)
{
	int len = 0;

	int res = 0;

	if(NULL == rhnd)
		return -1;

	rhnd->err = 0;

	len = sprintf_s(rhnd->buf.data, rhnd->buf.maxsize, "get %s\r\n", key);

	if(0 < (res = r_send_and_recv(rhnd, rhnd->buf.data, len)))
	{
		if(-1 != (res = r_analyze_get_res(rhnd, rhnd->buf.data)))
		{
			return 0;
		}
	}
	else if(-1 == res)
	{
		return -2;
	}

	return 1;
}

/***
Analyze hmget res info
***/
static int r_analyze_hmget_res(PREDIS rhnd, char *data)
{
	int num = 0, vsidx = 0, veidx = 0;

	char *info, value[80];

	r_bulk *curbulk;

	sscanf_s(data, "*%d", &num);

	if(num > rhnd->bulk.maxcount)
	{
		num = rhnd->bulk.maxcount;
	}

	rhnd->bulk.num = num;

	info = strstr(data, "\r\n");

	info += 2;

	if(0 == strcmp("$-1\r\n", info))
	{
		rhnd->bulk.num = 0;

		return 0;
	}

	curbulk = (r_bulk*)rhnd->bulk.data;

	vsidx = indexofstr(info, "\r\n", 1);

	veidx = indexofstr(info, "\r\n", 2);

	strcpy_s(curbulk->value, 80, substring(info, vsidx + 2, veidx, value, 80));

	return 0;
}

/***
Execute hmget
***/
int r_hmget(PREDIS rhnd, const char *key, const char *field)
{
	int len = 0;

	int res = 0;

	if(NULL == rhnd)
		return -1;

	rhnd->err = 0;

	len = sprintf_s(rhnd->buf.data, rhnd->buf.maxsize, "hmget %s %s\r\n", key, field);

	if(0 < (res = r_send_and_recv(rhnd, rhnd->buf.data, len)))
	{
		if(-1 != (res = r_analyze_hmget_res(rhnd, rhnd->buf.data)))
		{
			return 0;
		}
	}
	else if(-1 == res)
	{
		return -2;
	}

	return 1;
}

/***
Execute del
***/
int r_del(PREDIS rhnd, const char *key)
{
	int len = 0;

	int res = 0;

	if(NULL == rhnd)
		return -1;

	rhnd->err = 0;

	len = sprintf_s(rhnd->buf.data, rhnd->buf.maxsize, "del %s\r\n", key);

	if(0 < (res = r_send_and_recv(rhnd, rhnd->buf.data, len)))
	{
		if(-1 != (res = r_analyze_set_res(rhnd->buf.data, ":")))
		{
			return 0;
		}
	}
	else if(-1 == res)
	{
		return -2;
	}

	return 1;
}
