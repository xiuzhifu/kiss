#ifndef mainthread_h
#define mainthrad_h
#include "socketserver.h"
#define CMD_RECV 1
#define CMD_ACCEPT 2
#define CMD_DISCOONECT 3
struct msgnode{
	char * data;
	uint16_t len;
	uint16_t type;
	Client * client;
	int cmd;
};
bool addMsgNode(msgnode* node);
bool addMsgNodeToFront(msgnode* node);
msgnode * popMsgNode();
msgnode * getMsgNode();
void setLuaCallBack(int callback);
int getLuaCallBack();

int mainthread(const char * config);
#endif