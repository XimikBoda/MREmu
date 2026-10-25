#pragma once
#include <vector>

class History {
	std::vector<int> v;
public:
	int push(int id) {
        auto it = std::find(v.begin(), v.end(), id);

        if (it != v.end())
            v.erase(it);

        v.push_back(id);

        return id;
	}

    int pop(int id) {
        auto it = std::find(v.begin(), v.end(), id);

        if (it != v.end())
            v.erase(it);

        return last();
    }

    int last() const {
        return get_from_last(0);
    }

    int get_from_last(int n) const {
        if (n < v.size())
            return v[v.size() - 1 - n];
        return -1;
    }

    int first() const {
        return get_from_first(0);
    }

    int get_from_first(int n) const {
        if (n < v.size())
            return v[n];
        return -1;
    }

    bool empty() const {
        return v.empty();
    }
};