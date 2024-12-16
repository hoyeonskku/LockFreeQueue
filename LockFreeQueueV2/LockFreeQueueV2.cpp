#include <iostream>
#include "CLockFreeQueue.h"
#include "CircularQueue.h"
#include <thread>
#include <fstream>
#include <iomanip>

unsigned int g_data = 0;
unsigned int boolean1 = 0;


CLockFreeQueue<int> g_Queue;


unsigned int __stdcall ThreadFunc(void* arg)
{

	while (1)
	{
		int data = InterlockedIncrement(&g_data);
		for (int i = 0; i < 5; i++)
			g_Queue.Enqueue(data);

		for (int i = 0; i < 5; i++)
            if (g_Queue.Dequeue(data))
            {
                DebugBreak();
            };
	}
	return 0;
}

int main()
{
	HANDLE _handle[2];
	for (int i = 0; i < 1; i++)
	{
		_handle[i] = (HANDLE)_beginthreadex(NULL, 0, ThreadFunc, nullptr, NULL, nullptr);
	}
	WaitForMultipleObjects(1, _handle, true, INFINITE);

	while (true)
	{
	}
}

