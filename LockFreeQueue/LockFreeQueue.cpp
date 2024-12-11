#include <iostream>
#include "CLockFreeQueue.h"
#include <thread>

unsigned int g_data = 0;
unsigned int boolean1 = 0;
CLockFreeQueue<int> g_Queue;

unsigned int __stdcall ThreadFunc(void* arg)
{
	/*while (!boolean1)
	{
	}*/
	{
		while (1)
		{
			int data = InterlockedIncrement(&g_data);
			for (int i = 0; i < 5; i++)
				g_Queue.Enqueue(data);

			for (int i = 0; i < 5; i++)
				g_Queue.Dequeue(data);
		}
	}
}

int main()
{
	HANDLE _handle[5];
	for (int i = 0; i < 5; i++)
	{
		_handle[i] = (HANDLE)_beginthreadex(NULL, 0, ThreadFunc, nullptr, NULL, nullptr);
	}
	WaitForMultipleObjects(5, _handle, true, INFINITE);

	while (true)
	{
	}
}

