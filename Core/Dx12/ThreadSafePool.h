#pragma once

#include <queue>
#include <mutex>

namespace Core
{
	template<class Type_>
	class ThreadSafePool
	{
	public:
		ThreadSafePool() = default;
		ThreadSafePool(ThreadSafePool const& other);

		void Push(Type_ value);

		bool TryPop(Type_& value);

		bool Empty() const;

		size_t Size() const;

		~ThreadSafePool() = default;

	private:
		std::queue<Type_> m_queue;
		mutable std::mutex m_mutex;
	};

	template<class Type_>
	inline ThreadSafePool<Type_>::ThreadSafePool(ThreadSafePool const& other)
	{
		std::lock_guard<std::mutex> lock(this->m_mutex);
		this->m_queue = other.m_queue;
	}

	template<class Type_>
	inline void ThreadSafePool<Type_>::Push(Type_ value)
	{
		std::lock_guard<std::mutex> lock(this->m_mutex);
		this->m_queue.push(std::move(value));
	}

	template<class Type_>
	inline bool ThreadSafePool<Type_>::TryPop(Type_& value)
	{
		std::lock_guard<std::mutex> lock(this->m_mutex);
		if (this->m_queue.empty())
		{
			return false;
		}

		value = this->m_queue.front();
		this->m_queue.pop();

		return true;
	}

	template<class Type_>
	inline bool ThreadSafePool<Type_>::Empty() const
	{
		std::lock_guard<std::mutex> lock(this->m_mutex);
		return this->m_queue.empty();
	}

	template<class Type_>
	inline size_t ThreadSafePool<Type_>::Size() const
	{
		std::lock_guard<std::mutex> lock(this->m_mutex);
		return this->m_queue.size();
	}
}

