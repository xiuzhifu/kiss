#define _CRTDBG_MAP_ALLOC
#include<stdlib.h>  
#include<crtdbg.h> 
#include <thread>
#include "mainthread.h"


int main(int argc, char * argv[])
{
	char * config = nullptr;
	if (argc > 1) config = argv[1];
	std::thread t(mainthread, config);
	t.join();
	_CrtDumpMemoryLeaks();
	return 0;
}