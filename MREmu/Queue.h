#pragma once
#include <mutex>
#include <queue>

template<typename T>
class Queue {
	std::queue<T> q;
	std::mutex m;
public:
	void push(const T &item) {
		std::lock_guard lock(m);
		q.push(item);
	}

	T front() {
		std::lock_guard lock(m);
		T item = q.front();
		q.pop();
		return item;
	}

	size_t size() {
		std::lock_guard lock(m);
		return q.size();
	}
};