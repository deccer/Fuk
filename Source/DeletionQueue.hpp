#pragma once

#include <deque>
#include <functional>

struct DeletionQueue
{
public:
   	void Push(std::function<void()>&& function)
    {
		_deletors.push_back(function);
	}

	void flush()
    {
		for (auto it = _deletors.rbegin(); it != _deletors.rend(); it++)
        {
			(*it)();
		}

		_deletors.clear();
	}
private:
	std::deque<std::function<void()>> _deletors;
};