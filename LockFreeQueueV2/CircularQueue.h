#pragma once

#define CQSIZE 10000

#include <Windows.h>

template <typename T>
class CircularQueue {
public:
    // 생성자
    CircularQueue(UINT32 size = CQSIZE) : capacity(size), count(0) {
        //queue = new T[capacity];  // 동적 배열 할당
    }

    // 소멸자
    ~CircularQueue() {
        //delete[] queue;  // 동적 배열 해제
    }

    // 큐에 데이터 추가 (enqueue)
    void enqueue(const T& data) {
        if (bStop)
            return;

        UINT32 inc = InterlockedIncrement(&count);

        UINT32 index = inc % capacity;  // 원형 배열 처리
        queue[index] = data;
    }

    // 내용을 출력할 때 편하게 하려고 사용하는 함수. 평상시엔 사용하지 않음. 오로지 디버그 상황에서 출력하기 편하라도 사용.
    const T& dequeue(void)
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