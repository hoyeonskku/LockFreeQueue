#include <Windows.h>
#include "CLockFreeObjectPool.h"


enum EventType
{
    EnqueueFirstCas,
    EnqueueFirstCasFail,
    EnqueueSecondCas,
    EnqueueSecondCasFail,
    DequeueCas,
};


template <typename T>
struct Node
{
    T _data;
    Node<T>* _next;
};

template <typename T>
class LogClass
{
public:
    LogClass(){}
    LogClass(EventType type, Node<T>* pCurrent, Node<T>* pNext, Node<T>* pTargetNode, DWORD threadID) : _type(type), _pCurrent(pCurrent), _pNext(pNext), _pTargetNode(pTargetNode), _threadID(threadID) { }
    EventType _type;
    Node<T>* _pCurrent = nullptr;
    Node<T>* _pNext = nullptr;
    Node<T>* _pTargetNode = nullptr;
    DWORD _threadID;
};

template <typename T>
class LogQueue
{
public:
    LogQueue() {}
    LogClass<T> _arr[10000];
    unsigned long long _count;

    void Enqueue(LogClass<T> logClass)
    {
        unsigned long long index = InterlockedIncrement(&_count) % 10000;
        _arr[index] = logClass;
    }
};
template <typename T>
class CLockFreeQueue 
{
private:

    long _size;
    CMemoryPool<Node<T>> _pool;

    unsigned long long _headValue;        // 시작노드를 포인트한다.
    //Node<T>* _tailValue;        // 마지막노드를 포인트한다.
    // tailValue + tailNext;
    unsigned long long _tailArray[2];
    unsigned long long _headId = 0;
    unsigned long long _tailId = 0;

    //LogQueue<T> _queue;
public:
    CLockFreeQueue() :_pool(0)
    {
        _size = 0;
        Node<T>* dummy = _pool.Alloc();
        dummy->_next = NULL;
        _headValue = MAKE_VALUE(_headId, dummy);
        _tailArray[0] = MAKE_VALUE(_tailId, dummy);
        _tailArray[1] = NULL;
    }

    void Enqueue(T t)
    {
        Node<T>* node = _pool.Alloc();
        node->_data = t;
        node->_next = NULL;

        unsigned long long nodeValue = MAKE_VALUE(_tailId, node);
        unsigned long long tailArray[2];
        Node<T>* tail;
        Node<T>* next;
        while (true)
        {
           //tailValue = _tailValue;
            tailArray[0] = _tailArray[0];
            tail = (Node<T>*) MAKE_NODE(tailArray[0]);
            next = tail->_next;
            tailArray[1] = (unsigned long long) next;
            if (next == NULL)
            {
               Sleep(0);
                if (InterlockedCompareExchange128((long long*)_tailArray,(unsigned long long)node, (unsigned long long)tailArray[0], (long long*)tailArray))
                {
                     Sleep(0);
                    tail->_next = node;
                     Sleep(0);
                    _queue.Enqueue(LogClass<T>(EnqueueFirstCas, tail, next, node, GetCurrentThreadId()));
                    if ((unsigned long long)InterlockedCompareExchangePointer((PVOID*)&_tailArray[0], (PVOID)nodeValue, (PVOID)tailArray[0]) == tailArray[0]) // << 실패의 경우 그 이유 추적
                    {
                     Sleep(0);
                        _tailArray[0] = nodeValue;
                     Sleep(0);
                        _tailArray[1] = reinterpret_cast<unsigned __int64> (next);
                     Sleep(0);
                        _queue.Enqueue(LogClass<T>(EnqueueSecondCas, tail, next, node, GetCurrentThreadId()));
                        break;
                    }
                    _queue.Enqueue(LogClass<T>(EnqueueSecondCasFail, tail, next, node, GetCurrentThreadId()));
                }
                else
                {
                    _queue.Enqueue(LogClass<T>(EnqueueFirstCasFail, tail, next, node, GetCurrentThreadId()));
                }
            }
        }

        InterlockedExchangeAdd(&_size, 1);
    }

    int Dequeue(T& t)
    {
        if (_size == 0)
            return -1;
        unsigned long long pHeadValue;
        Node<T>* pHead;
        unsigned long long pNextValue;
        Node<T>* pNext;
        while (true)
        {
            pHeadValue = _headValue;
            pHead = (Node<T>*) MAKE_NODE(pHeadValue);
            pNext = pHead->_next;
            pNextValue =  MAKE_VALUE(_headId, pNext);

            if (pNext == NULL)
            {
                return -1;
            }
            else
            {
               Sleep(0);
               if ((unsigned long long)(InterlockedCompareExchangePointer((volatile PVOID*)&_headValue, (PVOID)pNextValue, (PVOID)pHeadValue)) == pHeadValue)
                {
               Sleep(0);
                    t = pNext->_data;
               Sleep(0);
                    _pool.Free(pHead);
               Sleep(0);
                    break;
                }
            }
        }
        _queue.Enqueue(LogClass<T>(DequeueCas, pHead, pNext, pHead, GetCurrentThreadId()));
        InterlockedExchangeAdd(&_size, -1);
        return 0;
    }
};