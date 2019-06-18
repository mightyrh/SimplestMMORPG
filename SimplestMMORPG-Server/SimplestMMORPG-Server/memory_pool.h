#pragma once

#include "simplestMMORPG.h"
#include <atomic>
#include <cassert>
#include <numeric>


// 인덱스0 ~ PoolSize-2의 PoolSiz-1개 만큼의 데이터를 담을 수 있는 Memory pool
// 인덱스PoolSize-1는 해당 메모리가 사용중임을 나타내는 값으로 사용하기 위해 reserve
template<class T, size_t PoolSize = 1024>
class MemoryPool
{
	// PoolSize는 2의 n승이어야 합니다.
	static_assert((PoolSize & PoolSize - 1) == 0 && PoolSize >= 4, "PoolSize must be power of 2.(At least 4)");
private:
	std::vector<T> mBuffer;
	std::vector<uint16_t> mFreeList;
	std::atomic_uint16_t mTail, mHead;				// 초기값 tail = 0, head = 1
	std::atomic_int32_t mCount;						// 할당받을 수 있는 메모리의 수
	static const uint16_t mMask = PoolSize - 1;
	MemoryPool<T, PoolSize>* mNextPool;
	std::mutex mExtendLock;
	size_t mPoolNumber;

public:
	//TODO: MemoryPool 구현
	MemoryPool(size_t poolNumber = 0) :
		mTail(0),
		mHead(1),
		mCount(PoolSize - 1),
		mNextPool(nullptr),
		mPoolNumber(poolNumber)
	{
		mBuffer.resize(PoolSize - 1);
		mFreeList.resize(PoolSize);
		// mFreeList는 0부터 쭉 채워놓는다
		std::iota(&mFreeList[1], &mFreeList[PoolSize - 1], 0);
		mFreeList[0] = PoolSize - 1;

		spdlog::info("New memory pool allocated. Element buffer size: {0}, Index buffer size: {1}.", mBuffer.size() * sizeof(T), mFreeList.size() * sizeof(uint16_t));
	}

	// 해당 메모리블럭의 인덱스가 필요 없는경우 사용.
	T* alloc()
	{
		// 할당 가능한 메모리가 없으면 새로 만들어줌.
		// 새로만든 풀에서 alloc() 한걸 리턴함.
		if (mCount <= 0) {
			return allocExtend();
		}

		int32_t count = atomicDecr(mCount);
		if (mCount <= 0) {
			atomicIncr(mCount);	// undo.
			return allocExtend();
		}

		uint16_t head = atomicIncr(mHead);
		uint16_t& index = mFreeList[head];
		while (index == 0xffff) { ; }

		T* mem = &mBuffer[index];
		// 이 인덱스가 가리키는 메모리가 사용중임을 표시.
		index = 0xffff;

		return mem;
	}

	// 해당 메모리블럭의 인덱스가 필요한 경우에 사용.
	size_t alloc(T*& memory)
	{
		// 할당 가능한 메모리가 없으면 새로 만들어줌.
		// 새로만든 풀에서 alloc() 한걸 리턴함.
		if (mCount <= 0) {
			return allocExtend(memory);
		}

		int32_t count = atomicDecr(mCount);
		if (mCount <= 0) {
			atomicIncr(mCount);	// undo.
			return allocExtend(memory);
		}

		uint16_t head = atomicIncr(mHead);
		uint16_t& index = mFreeList[head];
		while (index == 0xffff) { ; }

		memory = &mBuffer[index];
		// 해당 메모리의 index를 리턴하기 위해 저장.
		size_t returnVal = index + (PoolSize - 1) * mPoolNumber;
		// 이 인덱스가 가리키는 메모리가 사용중임을 표시.
		index = 0xffff;

		return returnVal;
	}

	void dealloc(T* mem)
	{
		// 해당 메모리가 담긴 풀을 찾고 거기서 해제해준다.
		if (mem < &mBuffer.front() || mem > &mBuffer.back()) {
			if (mNextPool) {
				mNextPool->dealloc(mem);
			}
			return;
		}

		// 해제하려는 메모리의 index를 구합니다.
		uint16_t index = static_cast<uint16_t>(mem - &mBuffer.front());

		uint16_t tail = atomicIncr(mTail);
		mFreeList[tail] = index;

		atomicIncr(mCount);

	}

	// 해당 인덱스가 가리키는 메모리를 리턴한다.
	T* unsafeAt(size_t index)
	{
		// index가 범위를 넘어섰는데 mNextPool이 nullptr이라면 access violation.
		if (index >= PoolSize - 1) {
			if (mNextPool == nullptr) {
				return nullptr;
			}
			else {
				return unsafeAtExtend(index - (PoolSize - 1));
			}
		}


		return &mBuffer[index];
	}

private:
	T* allocExtend()
	{
		// 추가로 생성된 풀이 있으면 그 풀에서 할당받는다.
		if (mNextPool == nullptr) {
			// 없으면 락걸고 새로 만들고 거기서 할당받는다.
			{
				// 공간이 부족할때 새로운 풀이 여러번 생기는걸 방지하기 위해 락을 사용한다.
				std::scoped_lock<std::mutex> lock(mExtendLock);
				if (mNextPool == nullptr) {
					mNextPool = new MemoryPool<T, PoolSize>(mPoolNumber + 1);
					spdlog::warn("New memory pool created. Consider modifying pool size.");
				}
			}
			if (mNextPool == nullptr) {
				return nullptr;
			}
		}
		return mNextPool->alloc();
	}

	size_t allocExtend(T*& memory)
	{
		// 추가로 생성된 풀이 있으면 그 풀에서 할당받는다.
		if (mNextPool == nullptr) {
			// 없으면 락걸고 새로 만들고 거기서 할당받는다.
			{
				// 공간이 부족할때 새로운 풀이 여러번 생기는걸 방지하기 위해 락을 사용한다.
				std::scoped_lock<std::mutex> lock(mExtendLock);
				if (mNextPool == nullptr) {
					mNextPool = new MemoryPool<T, PoolSize>(mPoolNumber + 1);
					spdlog::warn("New memory pool created. Consider modifying pool size.");
				}
			}
			//assert(mNextPool == nullptr);
			if (mNextPool == nullptr) {
				memory = nullptr;
				return 0;
			}
		}
		return mNextPool->alloc(memory);
	}

	T* unsafeAtExtend(size_t index)
	{
		return mNextPool->unsafeAt(index);
	}

	uint16_t atomicIncr(std::atomic_uint16_t& index)
	{
		uint16_t returnVal = index.fetch_add(1);
		index &= mMask;
		returnVal &= mMask;
		if (returnVal == 1024) {
			std::cout << "???????????" << std::endl;
		}
		return returnVal;
	}

	int32_t atomicIncr(std::atomic_int32_t& count)
	{
		int32_t returnVal = count.fetch_add(1);
		return returnVal;
	}

	int32_t atomicDecr(std::atomic_int32_t& count)
	{
		int32_t returnVal = count.fetch_sub(1);
		return returnVal;
	}
};