/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_MEMORY_POOL_HPP
#define HYPERION_MEMORY_POOL_HPP

#include <core/containers/LinkedList.hpp>
#include <core/containers/FixedArray.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Mutex.hpp>
#include <core/threading/DataRaceDetector.hpp>
#include <core/threading/Threads.hpp>

#include <core/memory/Memory.hpp>

#include <core/memory/allocator/Allocator.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/utilities/IdGenerator.hpp>

#include <Types.hpp>

namespace hyperion {
namespace memory {

template <class T>
struct MemoryPoolInitInfo
{
    static_assert(sizeof(T) <= 1024 * 1024, "Element size must be less than or equal to 1 MiB");

    static constexpr uint32 numElementsPerBlock = 1024 * 1024 / sizeof(T); // 1 MiB per block
    static constexpr uint32 numInitialElements = numElementsPerBlock;
};

template <class ElementType, class TInitInfo, void (*OnBlockAllocated)(void* ctx, ElementType* elements, uint32 startIndex, uint32 count) = nullptr>
struct MemoryPoolBlock
{
    static constexpr uint32 numElementsPerBlock = TInitInfo::numElementsPerBlock;

    ValueStorage<ubyte, numElementsPerBlock * sizeof(ElementType), alignof(ElementType)> buffer;
    AtomicVar<uint32> numElements { 0 };

#ifdef HYP_ENABLE_MT_CHECK
    FixedArray<DataRaceDetector, numElementsPerBlock> dataRaceDetectors;
#endif

    MemoryPoolBlock(void* ctx, uint32 blockIndex)
    {
        Memory::MemSet(buffer.GetPointer(), 0, numElementsPerBlock * sizeof(ElementType));

        // Allow overloading assignment of elements
        if (OnBlockAllocated != nullptr)
        {
            OnBlockAllocated(ctx, reinterpret_cast<ElementType*>(buffer.GetPointer()), blockIndex * numElementsPerBlock, numElementsPerBlock);
        }
        else
        {
            if constexpr (!isPodType<ElementType>)
            {
                // For non-POD types, default assign each element
                for (uint32 i = 0; i < numElementsPerBlock; i++)
                {
                    new (reinterpret_cast<ElementType*>(buffer.GetPointer()) + i) ElementType();

                    // if constexpr (std::is_copy_assignable_v<ElementType> || std::is_move_assignable_v<ElementType>)
                    // {
                    //     *(reinterpret_cast<ElementType*>(buffer.GetPointer()) + i) = ElementType {};
                    // }
                }
            }
        }
    }

    MemoryPoolBlock(const MemoryPoolBlock& other) = delete;
    MemoryPoolBlock& operator=(const MemoryPoolBlock& other) = delete;

    MemoryPoolBlock(MemoryPoolBlock&& other) noexcept = delete;
    MemoryPoolBlock& operator=(MemoryPoolBlock&& other) noexcept = delete;

    ~MemoryPoolBlock()
    {
        // Call destructors for each element in the block
        if constexpr (!isPodType<ElementType>)
        {
            for (uint32 i = 0; i < numElementsPerBlock; i++)
            {
                reinterpret_cast<ElementType*>(buffer.GetPointer())[i].~ElementType();
            }
        }
    }

    HYP_FORCE_INLINE bool IsEmpty() const
    {
        return numElements.Get(MemoryOrder::ACQUIRE) == 0;
    }
};

class MemoryPoolManager;

extern HYP_API MemoryPoolManager& GetMemoryPoolManager();
extern HYP_API void CalculateMemoryPoolUsage(Array<SizeType>& outBytesPerPool);

class HYP_API MemoryPoolBase
{
public:
    MemoryPoolBase(const MemoryPoolBase& other) = delete;
    MemoryPoolBase& operator=(const MemoryPoolBase& other) = delete;
    MemoryPoolBase(MemoryPoolBase&& other) noexcept = delete;
    MemoryPoolBase& operator=(MemoryPoolBase&& other) noexcept = delete;
    ~MemoryPoolBase(); // non-virtual intentionally

protected:
    MemoryPoolBase(ThreadId ownerThreadId, SizeType (*getNumAllocatedBytes)(MemoryPoolBase*));

    ThreadId m_ownerThreadId;
    IdGenerator m_idGenerator;

private:
    void RegisterMemoryPool();
};

template <class ElementType, class TInitInfo = MemoryPoolInitInfo<ElementType>, void (*OnBlockAllocated)(void* ctx, ElementType* elements, uint32 startIndex, uint32 count) = nullptr>
class MemoryPool : MemoryPoolBase
{
public:
    using InitInfo = TInitInfo;

    static constexpr uint32 numElementsPerBlock = InitInfo::numElementsPerBlock;

protected:
    using Block = MemoryPoolBlock<ElementType, TInitInfo, OnBlockAllocated>;

protected:
    static SizeType CalculateMemoryUsage(MemoryPoolBase* memoryPool)
    {
        return static_cast<MemoryPool*>(memoryPool)->NumAllocatedBytes();
    }

    void CreateInitialBlocks()
    {
        m_numBlocks.Set(m_initialNumBlocks, MemoryOrder::RELEASE);

        for (uint32 i = 0; i < m_initialNumBlocks; i++)
        {
            m_blocks.EmplaceBack(m_blockInitCtx, i);
        }
    }

public:
    static constexpr uint32 s_invalidIndex = ~0u;

    MemoryPool(uint32 initialCount = InitInfo::numInitialElements, bool createInitialBlocks = true, void* blockInitCtx = nullptr)
        : MemoryPoolBase(ThreadId::Current(), &CalculateMemoryUsage),
          m_initialNumBlocks((initialCount + numElementsPerBlock - 1) / numElementsPerBlock),
          m_numBlocks(0),
          m_blockInitCtx(blockInitCtx)
    {
        if (createInitialBlocks)
        {
            CreateInitialBlocks();
        }
    }

    ~MemoryPool() = default;

    HYP_FORCE_INLINE SizeType NumAllocatedElements() const
    {
        return m_numBlocks.Get(MemoryOrder::ACQUIRE) * numElementsPerBlock;
    }

    HYP_FORCE_INLINE SizeType NumAllocatedBytes() const
    {
        return m_numBlocks.Get(MemoryOrder::ACQUIRE) * sizeof(Block);
    }

    uint32 AcquireIndex(ElementType** outElementPtr = nullptr)
    {
        HYP_SCOPE;

        const uint32 index = m_idGenerator.Next() - 1;

        const uint32 blockIndex = index / numElementsPerBlock;

        if (blockIndex < m_initialNumBlocks)
        {
            Block& block = m_blocks[blockIndex];
            block.numElements.Increment(1, MemoryOrder::RELEASE);

            if (outElementPtr != nullptr)
            {
                *outElementPtr = &reinterpret_cast<ElementType*>(block.buffer.GetPointer())[index % numElementsPerBlock];
            }
        }
        else
        {
            Mutex::Guard guard(m_blocksMutex);

            if (index < numElementsPerBlock * m_numBlocks.Get(MemoryOrder::ACQUIRE))
            {
                Block& block = m_blocks[blockIndex];
                block.numElements.Increment(1, MemoryOrder::RELEASE);

                if (outElementPtr != nullptr)
                {
                    *outElementPtr = &reinterpret_cast<ElementType*>(block.buffer.GetPointer())[index % numElementsPerBlock];
                }
            }
            else
            {
                // Add blocks until we can insert the element
                uint32 currentBlockIndex = uint32(m_blocks.Size());

                while (index >= numElementsPerBlock * m_numBlocks.Get(MemoryOrder::ACQUIRE))
                {
                    m_blocks.EmplaceBack(m_blockInitCtx, currentBlockIndex);

                    m_numBlocks.Increment(1, MemoryOrder::RELEASE);

                    ++currentBlockIndex;
                }

                Block& block = m_blocks[blockIndex];
                block.numElements.Increment(1, MemoryOrder::RELEASE);

                if (outElementPtr != nullptr)
                {
                    *outElementPtr = &reinterpret_cast<ElementType*>(block.buffer.GetPointer())[index % numElementsPerBlock];
                }
            }
        }

        return index;
    }

    void ReleaseIndex(uint32 index)
    {
        HYP_SCOPE;

        m_idGenerator.ReleaseId(index + 1);

        const uint32 blockIndex = index / numElementsPerBlock;

        if (blockIndex < m_initialNumBlocks)
        {
            Block& block = m_blocks[blockIndex];

            block.numElements.Decrement(1, MemoryOrder::RELEASE);
        }
        else
        {
            Mutex::Guard guard(m_blocksMutex);

            HYP_CORE_ASSERT(index < numElementsPerBlock * m_numBlocks.Get(MemoryOrder::ACQUIRE));

            Block& block = m_blocks[blockIndex];
            block.numElements.Decrement(1, MemoryOrder::RELEASE);
        }
    }

    /*! \brief Ensure that the pool has enough capacity for the given index
     * After calling, you'll need to ensure that the blocks have numElements properly set,
     * or else the next call to RemoveEmptyBlocks() will just remove the newly added blocks. */
    void EnsureCapacity(uint32 index)
    {
        HYP_SCOPE;

        HYP_CORE_ASSERT(index != s_invalidIndex);

        const uint32 requiredBlocks = (index + numElementsPerBlock) / numElementsPerBlock;

        if (requiredBlocks <= m_numBlocks.Get(MemoryOrder::ACQUIRE))
        {
            return; // already has enough blocks
        }

        Mutex::Guard guard(m_blocksMutex);

        uint32 currentBlockIndex = uint32(m_blocks.Size());

        while (requiredBlocks > m_numBlocks.Get(MemoryOrder::ACQUIRE))
        {
            m_blocks.EmplaceBack(m_blockInitCtx, currentBlockIndex);
            m_numBlocks.Increment(1, MemoryOrder::RELEASE);

            ++currentBlockIndex;
        }
    }

    ElementType& GetElement(uint32 index)
    {
        HYP_SCOPE;

        HYP_CORE_ASSERT(index < NumAllocatedElements());

        const uint32 blockIndex = index / numElementsPerBlock;
        const uint32 elementIndex = index % numElementsPerBlock;

        if (blockIndex < m_initialNumBlocks)
        {
            Block& block = m_blocks[blockIndex];
            HYP_MT_CHECK_READ(block.dataRaceDetectors[elementIndex]);

            return reinterpret_cast<ElementType*>(block.buffer.GetPointer())[elementIndex];
        }
        else
        {
            Mutex::Guard guard(m_blocksMutex);

            Block& block = m_blocks[blockIndex];
            HYP_MT_CHECK_READ(block.dataRaceDetectors[elementIndex]);

            return reinterpret_cast<ElementType*>(block.buffer.GetPointer())[elementIndex];
        }
    }

    void SetElement(uint32 index, const ElementType& value)
    {
        HYP_SCOPE;

        HYP_CORE_ASSERT(index < NumAllocatedElements());

        const uint32 blockIndex = index / numElementsPerBlock;
        const uint32 elementIndex = index % numElementsPerBlock;

        if (blockIndex < m_initialNumBlocks)
        {
            Block& block = m_blocks[blockIndex];
            HYP_MT_CHECK_RW(block.dataRaceDetectors[elementIndex]);

            reinterpret_cast<ElementType*>(block.buffer.GetPointer())[elementIndex] = value;
        }
        else
        {
            Mutex::Guard guard(m_blocksMutex);

            HYP_CORE_ASSERT(blockIndex < m_numBlocks.Get(MemoryOrder::ACQUIRE));

            Block& block = m_blocks[blockIndex];
            HYP_MT_CHECK_RW(block.dataRaceDetectors[elementIndex]);

            reinterpret_cast<ElementType*>(block.buffer.GetPointer())[elementIndex] = value;
        }
    }

    /*! \brief Remove empty blocks from the back of the list */
    void RemoveEmptyBlocks()
    {
        HYP_SCOPE;
        // // Must be on the owner thread to remove empty blocks.
        // Threads::AssertOnThread(m_ownerThreadId);

        if (m_numBlocks.Get(MemoryOrder::ACQUIRE) <= m_initialNumBlocks)
        {
            return;
        }

        Mutex::Guard guard(m_blocksMutex);

        typename LinkedList<Block>::Iterator beginIt = m_blocks.Begin();
        typename LinkedList<Block>::Iterator endIt = m_blocks.End();

        Array<typename LinkedList<Block>::Iterator> toRemove;

        for (uint32 blockIndex = 0; blockIndex < m_numBlocks.Get(MemoryOrder::ACQUIRE) && beginIt != endIt; ++blockIndex, ++beginIt)
        {
            if (blockIndex < m_initialNumBlocks)
            {
                continue;
            }

            if (beginIt->IsEmpty())
            {
                toRemove.PushBack(beginIt);
            }
            else
            {
                toRemove.Clear();
            }
        }

        if (toRemove.Any())
        {
            m_numBlocks.Decrement(toRemove.Size(), MemoryOrder::RELEASE);

            while (toRemove.Any())
            {
                HYP_CORE_ASSERT(&m_blocks.Back() == &*toRemove.Back());

                m_blocks.Erase(toRemove.PopBack());
            }
        }
    }

    void ClearUsedIndices()
    {
        HYP_SCOPE;

        // // Must be on the owner thread to reset indices.
        // Threads::AssertOnThread(m_ownerThreadId);

        m_idGenerator.Reset();
    }

protected:
    uint32 m_initialNumBlocks;

    LinkedList<Block> m_blocks;
    AtomicVar<uint32> m_numBlocks;
    // Needs to be locked when accessing blocks beyond initialNumBlocks or adding/removing blocks.
    Mutex m_blocksMutex;

    void* m_blockInitCtx;
};

// struct MemoryPoolAllocator : Allocator<MemoryPoolAllocator>
// {

// };

} // namespace memory

using memory::CalculateMemoryPoolUsage;
using memory::MemoryPool;
using memory::MemoryPoolInitInfo;

} // namespace hyperion

#endif