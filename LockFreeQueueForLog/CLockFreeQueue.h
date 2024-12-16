#include <Windows.h>
#include "CLockFreeObjectPool.h"

#pragma once

#define CQSIZE 10000

#include <Windows.h>

template <typename T>
class LogQueue {
public:
    // ������
    LogQueue(UINT32 size = CQSIZE) : capacity(size), count(0) {
        //queue = new T[capacity];  // ���� �迭 �Ҵ�
    }

    // �Ҹ���
    ~LogQueue() {
        //delete[] queue;  // ���� �迭 ����
    }

    // ť�� ������ �߰� (enqueue)
    void Enqueue(const T& data) {
        if (bStop)
            return;

        UINT32 inc = InterlockedIncrement(&count);

        UINT32 index = inc % capacity;  // ���� �迭 ó��
        queue[index] = data;
    }

    // ������ ����� �� ���ϰ� �Ϸ��� ����ϴ� �Լ�. ���ÿ� ������� ����. ������ ����� ��Ȳ���� ����ϱ� ���϶� ���.
    const T& Dequeue(void)
    {
        if (!bDequeue)
        {
            bDequeue = true;
            deqeueCnt = count;
        }

        deqeueCnt++;
        deqeueCnt %= capacity;

        return queue[deqeueCnt % capacity];
    }

    UINT32 GetCount(void) { return count; }

    void Stop(void) { bStop = true; }

private:
    T queue[CQSIZE];           // ť �迭

    UINT32 count;
    UINT32 capacity;    // ť�� �ִ� ũ��

    bool bDequeue = false;
    int deqeueCnt = -1;

    bool bStop = false;
};

enum EventType
{
    EnqueueFirstCas,
    EnqueueFirstCasFail,
    EnqueueSecondCas,
    EnqueueSecondCasFail,
    DequeueCas,
    DequeueCasFail,
    EnqueueFailCas,
    EnqueueFailCasFail,
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
    LogClass() {}
    LogClass(EventType type, Node<T>* pReturnNode, Node<T>* pChangeNode, Node<T>* pLocalNode,  DWORD threadID) : _type(type), _pReturnNode(pReturnNode), _pChangeNode(pChangeNode), _pLocalNode(pLocalNode), _threadID(threadID) { }
    EventType _type;
    Node<T>* _pChangeNode = nullptr;
    Node<T>* _pLocalNode = nullptr;
    Node<T>* _pReturnNode = nullptr;
    DWORD _threadID;
};


template <typename T>
class CLockFreeQueue
{
private:

    long _size;
    CMemoryPool<Node<T>> _pool;

    unsigned long long _headValue;        // ���۳�带 ����Ʈ�Ѵ�.
    //Node<T>* _tailValue;        // ��������带 ����Ʈ�Ѵ�.
    // tailValue + tailNext;
    unsigned long long _tailValue;
    unsigned long long _headId = 0;
    unsigned long long _tailId = 0;

public:
    LogQueue<LogClass<T>> _queue;
    CLockFreeQueue() :_pool(0)
    {
        _size = 0;
        Node<T>* dummy = _pool.Alloc();
        dummy->_next = NULL;
        _headValue = MAKE_VALUE(_headId, dummy);
        _tailValue = MAKE_VALUE(_tailId, dummy);
    }

    void Enqueue(T t)
    {
        Node<T>* node = _pool.Alloc();
        node->_data = t;
        node->_next = NULL;

        unsigned long long nodeValue = MAKE_VALUE(_tailId, node);
        unsigned long long tailValue;
        Node<T>* tail;
        Node<T>* next;
        while (true)
        {
            //tailValue = _tailValue;
            tailValue = _tailValue;
            tail = (Node<T>*) MAKE_NODE(tailValue);
            if (tail == node)
            {
                DebugBreak();
                throw std::runtime_error("error");
            }
            next = tail->_next;

            // 2�� cas ������ ��츦 ���� tail�� �Ű��� �� ������ �Ű���.
            if (next != NULL)
            {
                unsigned long long nextValue = MAKE_VALUE(_tailId, next);
                InterlockedCompareExchangePointer((PVOID*)&_tailValue, (PVOID)nextValue, (PVOID)tailValue);
                continue;
            }

                    Sleep(0);
            {
                Node<T>* retNode = (Node<T>*)InterlockedCompareExchangePointer((volatile PVOID*)&tail->_next, (PVOID)node, (PVOID)next);
                if (retNode == NULL)
                {
                    _queue.Enqueue(LogClass<int>(EnqueueFirstCas, (Node<T>*) retNode, (Node<T>*) nodeValue, (Node<T>*) tailValue, GetCurrentThreadId()));
                    Sleep(0);
                    unsigned long long retValue = (unsigned long long)InterlockedCompareExchangePointer((PVOID*)&_tailValue, (PVOID)nodeValue, (PVOID)tailValue);
                    if (retValue == tailValue) // << ������ ��� �� ���� ����
                    {
                        _queue.Enqueue(LogClass<int>(EnqueueSecondCas, (Node<T>*)retValue, (Node<T>*)nodeValue, (Node<T>*) tailValue, GetCurrentThreadId()));
                        break;
                    }
                    else
                    {
                        _queue.Enqueue(LogClass<int>(EnqueueSecondCasFail, (Node<T>*) retValue, (Node<T>*) nodeValue, (Node<T>*) tailValue, GetCurrentThreadId()));
                        break;
                    }
                }
                else
                {
                    _queue.Enqueue(LogClass<int>(EnqueueFirstCasFail, (Node<T>*) retNode, (Node<T>*) nodeValue, (Node<T>*)  next, GetCurrentThreadId()));
                    continue;
                }
            }
        }

        InterlockedExchangeAdd(&_size, 1);
    }

    int Dequeue(T& t)
    {
        int size = InterlockedDecrement(&_size);
        
        if (size < 0)
        {
            InterlockedIncrement(&_size);
            DebugBreak();
            return -1;
        }

        unsigned long long pHeadValue;
        Node<T>* pHead;

        unsigned long long pTailValue;
        Node<T>* pTail;

        unsigned long long pHeadNextValue;
        Node<T>* pHeadNext;

        unsigned long long pTailNextValue;
        Node<T>* pTailNext;

        while (true)
        {
            pHeadValue = _headValue;
            pHead = (Node<T>*) MAKE_NODE(pHeadValue);
            pTailValue = _tailValue;
            pTail = (Node<T>*) MAKE_NODE(pTailValue);
            pHeadNext = pHead->_next;

            if (pHeadNext == NULL)
            {
                continue;
            }

            // head�� tail�� ���� ��� 
            if (pHead == pTail)
            {
                pTailNext = pTail->_next;
                if (pTailNext == NULL)
                    continue;

                pTailNextValue = MAKE_NODE(pTailNext);
                InterlockedCompareExchangePointer((PVOID*)&_tailValue, (PVOID)pTailNextValue, (PVOID)pTailValue);
                continue;
            }
            else
            {
                pHeadNextValue = MAKE_VALUE(_headId, pHeadNext);
                Sleep(0);
                t = pHeadNext->_data;
                unsigned long long retValue = (unsigned long long)(InterlockedCompareExchangePointer((volatile PVOID*)&_headValue, (PVOID)pHeadNextValue, (PVOID)pHeadValue));
                if (retValue == pHeadValue)
                {
                    Sleep(0);
                    Sleep(0);
                    _pool.Free(pHead);
                    Sleep(0);

                    _queue.Enqueue(LogClass<int>(DequeueCas, (Node<T>*)retValue, (Node<T>*)pHeadNextValue, (Node<T>*)pHeadValue, GetCurrentThreadId()));
                    break;
                }
                _queue.Enqueue(LogClass<int>(DequeueCasFail, (Node<T>*)retValue, (Node<T>*)pHeadNextValue, (Node<T>*)pHeadValue, GetCurrentThreadId()));
            }
        }
        return 0;
    }
};