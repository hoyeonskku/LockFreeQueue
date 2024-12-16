#include <Windows.h>
#include "CLockFreeObjectPool.h"

#pragma once

#define CQSIZE 10000

#include <Windows.h>

template <typename T>
class LogQueue {
public:
    // 생성자
    LogQueue(UINT32 size = CQSIZE) : capacity(size), count(0) {
        //queue = new T[capacity];  // 동적 배열 할당
    }

    // 소멸자
    ~LogQueue() {
        //delete[] queue;  // 동적 배열 해제
    }

    // 큐에 데이터 추가 (enqueue)
    void Enqueue(const T& data) {
        if (bStop)
            return;

        UINT32 inc = InterlockedIncrement(&count);

        UINT32 index = inc % capacity;  // 원형 배열 처리
        queue[index] = data;
    }

    // 내용을 출력할 때 편하게 하려고 사용하는 함수. 평상시엔 사용하지 않음. 오로지 디버그 상황에서 출력하기 편하라도 사용.
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
    T queue[CQSIZE];           // 큐 배열

    UINT32 count;
    UINT32 capacity;    // 큐의 최대 크기

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

    unsigned long long _headValue;        // 시작노드를 포인트한다.
    //Node<T>* _tailValue;        // 마지막노드를 포인트한다.
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

            // 2번 cas 실패한 경우를 위해 tail을 옮겨줄 수 있으면 옮겨줌.
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
                    if (retValue == tailValue) // << 실패의 경우 그 이유 추적
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

            // head와 tail이 같은 경우 
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