#pragma once
#include <mutex>
#include <queue>

template<typename T>
class Queue {
	std::queue<T> q;
public:
	//void push(const T &item) {
	//	q.push(item);
	//}

	void push(std::mutex &m, const T &item) {
		std::lock_guard lock(m);
		q.push(item);
	}

	T front() {
		T item = q.front();
		q.pop();
		return item;
	}

	bool empty() {
		return q.empty();
	}
};