#include <iostream>
#include "CLockFreeQueue.h"
#include <thread>
#include <fstream>
#include <iomanip>

unsigned int g_data = 0;
unsigned int boolean1 = 0;


CLockFreeQueue<int> g_Queue;
CRITICAL_SECTION cs;

void handler(void)
{
    DWORD curThreadID = GetCurrentThreadId();

    // 출력
    EnterCriticalSection(&cs);

    // 디버깅 정보 출력
    LogClass<int> node;

    std::ofstream outFile{ "debug_error.txt" };

    if (!outFile)
        std::cerr << "파일 열기 실패...\n";

    for (int i = 0; i < CQSIZE; ++i)
    {
        node = g_Queue._queue.Dequeue();

        outFile << std::dec << std::setw(4) << std::setfill('0') << i + 1 << ". EventType: ";

        switch (node._type)
        {
        case EventType::EnqueueFirstCas:
            outFile << "EnqueueFirstCas_______";
            break;
        case EventType::EnqueueFirstCasFail:
            outFile << "EnqueueFirstCasFail___";
            break;
        case EventType::EnqueueSecondCas:
            outFile << "EnqueueSecondCas______";
            break;
        case EventType::EnqueueSecondCasFail:
            outFile << "EnqueueSecondCasFail__";
            break;
        case EventType::DequeueCasFail:
            outFile << "DequeueCasFail________";
            break;
        case EventType::EnqueueFailCas:
            outFile << "EnqueueFailCas________";
            break;
        case EventType::EnqueueFailCasFail:
            outFile << "EnqueueFailCasFail____";
            break;
        case EventType::DequeueCas:
            outFile << "DequeueCas____________";
            break;
        default:
            break;
        }

        outFile << "\tThread: " << std::dec << node._threadID
            << "\tReturnNode: " << std::hex << node._pReturnNode;

        if (!node._pReturnNode)
        {
            outFile << "\t";
        }
        outFile << "\tChangeNode: " << std::hex << node._pChangeNode;

        if (!node._pChangeNode)
        {
            outFile << "\t";
        }
        outFile << "\tLocalNode: " << std::hex << node._pLocalNode;

        if (!node._pLocalNode)
        {
            outFile << "\t";
        }


        outFile << std::endl;
    }

    outFile.close();

    exit(-1);
}







unsigned int __stdcall ThreadFunc(void* arg)
{
    /*while (!boolean1)
    {
    }*/
    try
    {
        while (1)
        {
            int data = InterlockedIncrement(&g_data);
            for (int i = 0; i < 1; i++)
                g_Queue.Enqueue(data);

            for (int i = 0; i < 1; i++)
                if (g_Queue.Dequeue(data))
                {
                    DebugBreak();
                    g_Queue._queue.Stop();
                    handler();
                    return 0;
                }
        }
    }
    catch (const std::exception&)
    {

        handler();
    }
    return 0;
}

int main()
{
    HANDLE _handle[2];
    InitializeCriticalSection(&cs);
    for (int i = 0; i < 2; i++)
    {
        _handle[i] = (HANDLE)_beginthreadex(NULL, 0, ThreadFunc, nullptr, NULL, nullptr);
    }
    WaitForMultipleObjects(2, _handle, true, INFINITE);

    while (true)
    {
    }
}

