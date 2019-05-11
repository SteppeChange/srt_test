//
// Created by Oleg Golosovskiy on 14/11/2018.
//

#include "channel_statistics.h"
#include <thread>
#include <algorithm>
#include <cassert>
#include "logger.h"
#include <limits.h>

#ifdef ANT_UNIT_TESTS
# include <gtest/gtest.h>
#endif


#ifdef ANT_UNIT_TESTS

TEST(Statistics, network_bandwith) {
	channel_statistics st;
	st.tests();
}

void channel_statistics::tests()
{


	// test accuracy
	clear();
	std::this_thread::sleep_for(std::chrono::seconds(1));
	push_sent_event(50);
	std::this_thread::sleep_for(std::chrono::seconds(1));
	push_sent_event(60);
	std::this_thread::sleep_for(std::chrono::seconds(1));
	push_sent_event(70);
	size_t sent1 = sma_sent(2000);
	size_t sent2 = sma_sent(3000);
	assert(sent1==65);
	assert(sent2==60);


	// test diff args
	clear();
	push_sent_event(2764, 26877);
	push_sent_event(2419, 26877);
	push_sending_event(32823, 27130);
	push_buffer_event(10000, 27130);
	push_sent_event(16584 ,  27130);
	push_sent_event(16239, 27132);
	// ....
	push_sending_event(32823, 27384);
	push_buffer_event(10000, 27384);
	push_sent_event(19348 ,  27384);
	push_sent_event(13475, 27385);
	push_sending_event(32823, 27637);
	push_buffer_event(10000, 27637);
	push_sent_event(16584 ,  27637);
	push_sent_event(4146, 27638);
	push_sent_event(5528, 27638);
	push_sent_event(6565, 27638);
	push_sending_event(32823, 27892);
	push_buffer_event(10000, 27892);
	push_sent_event(22112 ,  27892);
	push_sent_event(10711, 27893);
	push_sending_event(32823, 28148);
	push_buffer_event(10000, 28148);
	push_sent_event(13820 ,  28148);
	push_sent_event(11056, 28149);
	push_sent_event(5528, 28149);
	push_sent_event(2419, 28149);
	push_sending_event(32823, 28401);
	push_buffer_event(10000, 28401);
	push_sent_event(22112 ,  28401);
	push_sent_event(10711, 28402);
	push_sending_event(32824, 28657);
	push_buffer_event(10000, 28657);
	push_sent_event(15202, 28657);
	push_sent_event(9674, 28658);
	push_sent_event(7948, 28658);
	push_sending_event(32824, 28912);
	push_buffer_event(10000, 28912);
	push_sent_event(22112, 28912);
	push_sent_event(9674, 28913);
	push_sent_event(1038, 28913);
	push_sending_event(32824, 29167);
	push_buffer_event(5000, 29167);
	push_sent_event(16584, 29167);
	push_sent_event(15202, 29168);
	push_sent_event(1038, 29168);
	size_t sending = sma_sending(2000, 29200);
	size_t sent = sma_sent(2000, 29200);
	assert(sending == sent);
	size_t buf = min_buffer(2000, 29200);
	assert(buf = 5000);

	// test limit
	clear();
	for(int i = 0; i<lru_limit*2; ++i)
		push_sent_event(100, 100);
	assert(_sent.size()==lru_limit);

}

#endif

void channel_statistics::clear() {
	_sent.clear();
	_sending.clear();
}

int channel_statistics::simple_moving_average(least_recently_used<stat_event> const& a_data, stat_time_ms const& period, stat_time_ms& now_ts) const {

	if(a_data.empty())
		return NO_STAT_DATA;

	stat_time_ms period_ts = now_ts - period;
	if(period_ts <= 0)
		return NO_STAT_DATA;

	stat_time_ms sum = 0;
	bool found_data = false;
	for(auto it = a_data.end(), end = a_data.begin(); end!=it; )
	{
		--it;
		if(it->_time > period_ts) {
			assert(it->_time <= now_ts);
			sum += it->_value;
			found_data = true;
		}
		else
			break;
	}

	if(!found_data)
		return NO_STAT_DATA;

	size_t cma = sum / (period/1000.0); // per second
	return cma;
}

int channel_statistics::extremum(least_recently_used<stat_event> const& a_data,  EType type, stat_time_ms const& period, stat_time_ms& now_ts) const {

	stat_time_ms period_ts = now_ts - period;
	if(period_ts <= 0)
		return NO_STAT_DATA;
	int val = type==min ? INT_MAX : 0;
	bool found_data = false;
	for(auto it = a_data.end(), end = a_data.begin(); end!=it; )
	{
		--it;
		if(it->_time > period_ts) {
			found_data = true;
			assert(it->_time <= now_ts);
			if(min==type)
				val = std::min(val, (int)(it->_value));
			if(max==type)
				val = std::max(val, (int)(it->_value));
		}
		else
			break;
	}
	if(!found_data)
		return NO_STAT_DATA;
	return val;
}

int channel_statistics::sma_sent(stat_time_ms const& period, stat_time_ms&& now_ts) const {
	return simple_moving_average(_sent, period, now_ts);
}

int channel_statistics::sma_sending(stat_time_ms const& period, stat_time_ms&& now_ts) const {
	return simple_moving_average(_sending, period, now_ts);
}

int channel_statistics::min_buffer(stat_time_ms const& period, stat_time_ms&& now_ts) const {
	return extremum(_out_buffer, channel_statistics::min, period, now_ts);
}

int channel_statistics::max_buffer(stat_time_ms const& period, stat_time_ms&& now_ts) const {
	return extremum(_out_buffer, channel_statistics::max, period, now_ts);
}

void channel_statistics::push_sent_event(size_t a_sent_data, stat_time_ms &&a_time) {
	_sent.emplace_back(stat_event(a_sent_data, a_time));
}

void channel_statistics::push_sending_event(size_t a_sending_data, stat_time_ms &&a_time) {
	_sending.emplace_back(stat_event(a_sending_data, a_time));
}

void channel_statistics::push_buffer_event(size_t a_buffer_size, stat_time_ms &&a_time) {
	_out_buffer.emplace_back(stat_event(a_buffer_size, a_time));
}

void channel_statistics::dump() {
	 // to do union of all events sorted by time
	for(auto it = _sent.begin(), end = _sent.end(); end!=it; ++it) {
		LOG(ant::Log::EDebug, ant::Log::EAnt, "net stat dump: sent(%d, %d);\n", it->_time, it->_value);
	}
}
