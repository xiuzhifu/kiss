#include "mainthread.h"
bool dispatch_message(struct msgnode * msg);
bool dispatch_message_lua(lua_State *L, struct msgnode * msg);
bool poll_message(size_t tick);