
#ifndef LIBANT_CHANNEL_STATISTICS_H
#define LIBANT_CHANNEL_STATISTICS_H

#include <list>
#include <chrono>
#include <memory>
#include <cstdint>
#include <memory>

const size_t lru_limit = 100;
#define STAT_PERIOD 1000  // ms
#define NO_STAT_DATA -1
typedef long long stat_time_ms;

template<class T>
class least_recently_used  {

public:
	void push_back(typename std::list<T>::value_type const& item) {
		if(_list.size()>=lru_limit)
			_list.pop_front();
		_list.push_back(item);
	}

	void emplace_back(T&& new_obj) {
		if(_list.size()>=lru_limit)
			_list.pop_front();
		_list.emplace_back(new_obj);
	}

	typename std::list<T>::const_iterator begin() const noexcept
		{return _list.begin(); }

	typename std::list<T>::const_iterator end() const noexcept
		{return _list.end(); }

	T const& back() const noexcept
		{return _list.back(); }

	size_t size() const noexcept
		{ return _list.size(); }

	bool empty() const noexcept
		{ return _list.empty(); }

	 void clear() noexcept
	 	{ _list.clear();  }

protected:
	std::list<T> _list;
};

static stat_time_ms start_stats = std::chrono::duration_cast<std::chrono::milliseconds >(std::chrono::system_clock::now().time_since_epoch()).count();

inline stat_time_ms now()
{
	std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds >(std::chrono::system_clock::now().time_since_epoch());
	return now.count() - start_stats;
}

struct stat_event {
	stat_event() = delete;
	stat_event(size_t a_value, stat_time_ms& a_time)
		: _time(a_time)
		, _value(a_value)
		{}
	stat_time_ms _time;
	size_t _value;
};

class channel_statistics {
public:
	typedef std::shared_ptr<channel_statistics> ptr;
	void clear();

	void push_sent_event(size_t a_sent_data, stat_time_ms &&a_time = now());
	void push_sending_event(size_t a_sending_data, stat_time_ms &&a_time = now());
	void push_buffer_event(size_t a_buffer_size, stat_time_ms &&a_time = now());

	void dump();
#ifdef ANT_UNIT_TESTS
	void tests();
#endif

	// return bytes/second
	// return NO_STAT_DATA if there is no one stat event for this period
	int sma_sent(stat_time_ms const& period = STAT_PERIOD, stat_time_ms&& now_ts = now()) const;
	// return bytes/second
	// return NO_STAT_DATA if there is no one stat event for this period
	int sma_sending(stat_time_ms const& period = STAT_PERIOD, stat_time_ms&& now_ts = now()) const;
	// return bytes
	// return NO_STAT_DATA if there is no one stat event for this period
	int min_buffer(stat_time_ms const& period = STAT_PERIOD, stat_time_ms&& now_ts = now()) const;
	// return bytes
	// return NO_STAT_DATA if there is no one stat event for this period
	int max_buffer(stat_time_ms const& period = STAT_PERIOD, stat_time_ms&& now_ts = now()) const;

protected:

	// return NO_STAT_DATA if there is no one stat event for this period
	int simple_moving_average(least_recently_used<stat_event> const& a_data, stat_time_ms const& period, stat_time_ms& now_ts) const;
	enum EType {
		min = 0,
		max = 1
	};
	// return NO_STAT_DATA if there is no one stat event for this period
	int extremum(least_recently_used<stat_event> const& a_data, EType type, stat_time_ms const& period, stat_time_ms& now_ts) const;

	least_recently_used<stat_event> _sending; // sent / buffered by Ant
	least_recently_used<stat_event> _sent; // sent/buffered by UTP
	least_recently_used<stat_event> _out_buffer;
};


#endif //LIBANT_CHANNEL_STATISTICS_H
