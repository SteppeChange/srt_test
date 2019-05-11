#pragma once

#include <queue>
#include <mutex>

template<typename Item>
class multithread_queue
{
private:
  std::queue<Item> _queue;

  mutable std::mutex _mutex;
  typedef std::lock_guard<std::mutex> lock_t;

public:

  void push(Item const& item)
  {
    lock_t lock(_mutex);
    _queue.push(item);
  }

  bool empty() const
  {
    lock_t lock(_mutex);
    return _queue.empty();
  }

  void clear()
  {
	lock_t lock(_mutex);
	std::queue<Item> _empty_queue;
	std::swap( _queue, _empty_queue );
  }

  bool try_pop(Item& item)
  {
    lock_t lock(_mutex);
    if(_queue.empty())
    {
      return false;
    }

    item=_queue.front();
    _queue.pop();
    return true;
  }
};
