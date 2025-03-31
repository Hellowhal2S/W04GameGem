
#include "Array.h"

#include "Define.h"

template <typename T, typename Allocator>
T& TArray<T, Allocator>::Last()
{
    (!IsEmpty());
    return ContainerPrivate.back();
}

template <typename T, typename Allocator>
const T& TArray<T, Allocator>::Last() const
{
    check(!IsEmpty());
    return ContainerPrivate.back();
}

template <typename T, typename Allocator>
bool TArray<T, Allocator>::Pop(T& OutItem)
{
    if (IsEmpty())
        return false;
    OutItem = std::move(ContainerPrivate.back());
    ContainerPrivate.pop_back();
    return true;
}

template <typename T, typename Allocator>
T TArray<T, Allocator>::Pop()
{
    check(!IsEmpty());
    T Temp = std::move(ContainerPrivate.back());
    ContainerPrivate.pop_back();
    return Temp;
}

template <typename T, typename Allocator>
void TArray<T, Allocator>::Push(const T& Item)
{
    Add(Item);
}

template <typename T, typename Allocator>
void TArray<T, Allocator>::Push(T&& Item)
{
    Add(std::move(Item));
}

template <typename T, typename Allocator>
T& TArray<T, Allocator>::Top()
{
    return Last();
}

template <typename T, typename Allocator>
const T& TArray<T, Allocator>::Top() const
{
    return Last();
}

template <typename T, typename Allocator>
T TArray<T, Allocator>::Dequeue()
{
    check(!IsEmpty());
    T Temp = std::move(ContainerPrivate.front());
    RemoveAt(0);
    return Temp;
}

template <typename T, typename Allocator>
void TArray<T, Allocator>::Enqueue(const T& Item)
{
    Add(Item);
}

template <typename T, typename Allocator>
void TArray<T, Allocator>::Enqueue(T&& Item)
{
    Add(std::move(Item));
}
