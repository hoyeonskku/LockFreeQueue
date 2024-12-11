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
    LogClass(EventType type, Node<T>* pCurrent, Node<T>* pNext) : _type(type), _pCurrent(pCurrent), _pNext(pNext) { }
    EventType _type;
    Node<T>* _pCurrent = nullptr;
    Node<T>* _pNext = nullptr;
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

    Node<T>* _headValue;        // 시작노드를 포인트한다.
    Node<T>* _tailValue;        // 마지막노드를 포인트한다.
    unsigned long long _headId = 0;
    unsigned long long _tailId = 0;

    LogQueue<T> _queue;
public:
    CLockFreeQueue() :_pool(0)
    {
        _size = 0;
        Node<T>* dummy = _pool.Alloc();
        dummy->_next = NULL;
        _headValue = (Node<T>*) MAKE_VALUE(_headId, dummy);
        _tailValue = (Node<T>*) MAKE_VALUE(_tailId, dummy);
    }

    void Enqueue(T t)
    {
        Node<T>* node = _pool.Alloc();
        node->_data = t;
        node->_next = NULL;

        Node<T>* nodeValue = (Node<T>*) MAKE_VALUE(_tailId, node);
        Node<T>* tailValue;
        Node<T>* tail;
        Node<T>* next;
        while (true)
        {
           tailValue = _tailValue;
           tail = (Node<T>*) MAKE_NODE(tailValue);
           next = tail->_next;
            if (next == NULL)
            {
               Sleep(0);
                if (InterlockedCompareExchangePointer((PVOID*)&tail->_next, node, NULL) == next)
                {
                    _queue.Enqueue(LogClass(EnqueueFirstCas, tail, next));
                     Sleep(0);
                    if (InterlockedCompareExchangePointer((PVOID*)&_tailValue, nodeValue, tailValue) == tailValue) // << 실패의 경우 그 이유 추적
                    {
                        _queue.Enqueue(LogClass(EnqueueSecondCas, tail, next));
                        break;
                    }
                    _queue.Enqueue(LogClass(EnqueueSecondCasFail, tail, next));
                }
                else
                {
                    _queue.Enqueue(LogClass(EnqueueFirstCasFail, tail, next));
                }
            }
        }

        InterlockedExchangeAdd(&_size, 1);
    }

    int Dequeue(T& t)
    {
        if (_size == 0)
            return -1;
        Node<T>* pHeadValue;
        Node<T>* pHead;
        Node<T>* pNextValue;
        Node<T>* pNext;
        while (true)
        {
            pHeadValue = _headValue;
            pHead = (Node<T>*) MAKE_NODE(pHeadValue);
            pNext = pHead->_next;
            pNextValue = (Node<T>*) MAKE_VALUE(_headId, pNext);

            if (pNext == NULL)
            {
                return -1;
            }
            else
            {
               Sleep(0);
                if (InterlockedCompareExchangePointer((PVOID*)&_headValue, pNextValue, pHeadValue) == pHeadValue)
                {
                    t = pNext->_data;
                    _pool.Free(pHead);
                    break;
                }
            }
        }
        _queue.Enqueue(LogClass(DequeueCas, pHead, pNext));
        InterlockedExchangeAdd(&_size, -1);
        return 0;
    }
};