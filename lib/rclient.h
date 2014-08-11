#ifndef _REDIS_CLIENT_H_
#define _REDIS_CLIENT_H_

#include<stdio.h>
#include<WinSock2.h>
#include<Windows.h>


#define R_BUFFER_SIZE 10240

#define R_BULK_COUNT 1000

typedef struct _r_buffer
{
	unsigned short maxsize;
	unsigned short len;
	char *data;
}r_buffer;

typedef struct _r_bulk
{
	char field[160];
	char value[80];
}r_bulk;

typedef struct _r_bulk_buffer
{
	unsigned short maxcount;
	unsigned short num;
	char *data;
}r_bulk_buffer;

typedef struct _r_client
{
	SOCKET sock;
	char ip[32];
	unsigned short port;
	unsigned short timeout;
	int err;
	char errstr[64];
	r_buffer buf;
	r_bulk_buffer bulk;
}r_client;


typedef r_client* PREDIS;


#ifdef __cplusplus
extern "C"{
#endif


/***
In concurrent programming environment, each connected thread must create individual  handle
This will connect to the redis server and init resource
***/
PREDIS r_connect_server(const char *host, unsigned short port, unsigned short timeout);

/***
This will reconnect to the redis server
***/
int r_reconnect_server(PREDIS rhnd);

/***
Close the connection
This will close the socket and dispose resource
***/
void r_close_connect(PREDIS* phnd);


/***
Return values
0 : success
1 : error
-1: rhnd is null
-2: socket was disconnected
***/

/***
Wrap of hset
expiremin: 0 means do not set an expiration time
***/
int r_hset(PREDIS rhnd, const char *key, const char *field, const char *value, unsigned short expiremin);

/***
Wrap of hmset
expiremin: 0 means do not set an expiration time
***/
int r_init_hmset(PREDIS rhnd, const char *key);

int r_add_hmset_info(PREDIS rhnd, char *field, char *value);

int r_hmset(PREDIS rhnd, const char *key, unsigned short expiremin);


/***
Wrap of hmget
***/
int r_hmget(PREDIS rhnd, const char *key, const char *field);

/***
Wrap of hgetall
***/
int r_hgetall(PREDIS rhnd, const char *key);

/***
Wrap of set
expiremin: 0 means do not set an expiration time
***/
int r_set(PREDIS rhnd, const char *key, const char *value, unsigned short expiremin);

/***
Wrap of get
***/
int r_get(PREDIS rhnd, const char *key);

/***
Wrap of expire
***/
int r_expire(PREDIS rhnd, const char *key, const unsigned short expiremin);

/***
Wrap of del
***/
int r_del(PREDIS rhnd, const char *key);

#ifdef __cplusplus
}
#endif

#endif
