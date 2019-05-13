//
// Created by alvid on 26.04.17.
//
#ifndef VOIP_TOOL_STAT_H
#define VOIP_TOOL_STAT_H

#include <vector>
#include <cassert>
#include <chrono>
#include <mutex>
#include <exception>

using std::chrono::system_clock;
using std::chrono::steady_clock;

template <typename T>
class Moving_average
{
public:
    class e_no_data : public std::exception {};

    Moving_average() = delete;

    Moving_average(const T& value, int ave_interval_ms, const std::string &name = "") noexcept
            : interval(ave_interval_ms), name_v(name) {
        history.reserve(10);
        init(value);
        min_v = max_v = value;
    }

    void add_val(const T& value) noexcept {
        auto cur_time = steady_clock::now();
        if (value < min_v)
            min_v = value;
        if (max_v < value)
            max_v = value;
        if (cur_time - last_stat_time < interval) {
            sum_v += value;
            ++count;
        } else {
            std::lock_guard<std::mutex> guard(mt);

            ave_v = sum_v / count;
            history.push_back(ave_v);

            init(value);
        }
    }

    T ave_val(int minus_interval_num) {
        assert(minus_interval_num <= 0);
        std::lock_guard<std::mutex> guard(mt);
        if (!history.size())
            return sum_v/count;
        if (history.size() > abs(minus_interval_num))
            return history.at(history.size() - 1 + minus_interval_num);
        throw (e_no_data());
    }
    inline T min_val() {
        return min_v;
    }
    inline T max_val() {
        return max_v;
    }
    inline int intervals_count() {
        std::lock_guard<std::mutex> guard(mt);
        return history.size();
    }

    void print(int indent, int max_intervals_count) {
        for (auto i = 0; i < indent; ++i)
            putchar(' ');
        printf("[%s]\n", name_v.c_str());

        for (auto i = 0; i < indent+2; ++i)
            putchar(' ');
        printf("min value: %d\n", min_val());

        for (auto i = 0; i < indent+2; ++i)
            putchar(' ');
        printf("ave value: ");
        auto cnt = intervals_count() - 1;
        cnt = cnt < max_intervals_count ? cnt : max_intervals_count;
        for (int i = cnt; i >= 0; --i) {
            try {
                if (i)
                    printf("%d sec ago: %d, ", -(1+i) * interval.count() / 1000, ave_val(-i));
                else
                    printf("%d sec ago: %d", -(1+i) * interval.count() / 1000, ave_val(-i));
            } catch (std::exception e) {
                break;
            };
        }
        printf("\n");

        for (auto i = 0; i < indent+2; ++i)
            putchar(' ');
        printf("max value: %d\n", max_val());
    }

private:
    void init(const T& value) {
        last_stat_time = steady_clock::now();
        ave_v = value;
        sum_v = value;
        count = 1;
    }

private:
    std::chrono::duration<int, std::milli> interval;
    std::mutex mt;
    steady_clock::time_point last_stat_time;
    T sum_v {0};
    T min_v {0};
    T max_v {0};
    T ave_v {0};
    T count {0};
    std::vector<T> history;
    std::string name_v;
};

void moving_average_unittest()
{
    const auto interval = 50;
    std::vector<int> i_values = { 1, 25, -32, 111, 55,
                                  78, 4, 0, -168, 334,
                                  -78, 34, 40, -18, 84,
                                  -222, 90, 0, -8, 133,
                                  323, 444, -992, 1, 5 };

    std::vector<int>::const_iterator itr = i_values.begin();
    Moving_average<int> *i_val_average = new Moving_average<int>(*itr, interval);
    printf("add val: %d\n", *itr);

    for (++itr; itr!=i_values.end(); ++itr) {
        i_val_average->add_val(*itr);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        printf("add val: %d, min: %d, ave: %d, max: %d\n",
               *itr, i_val_average->min_val(), i_val_average->ave_val(0), i_val_average->max_val());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(interval*2));

    printf("\nstored ave: ");
    for (int i = i_val_average->intervals_count()-1; i >= 0; --i)
        try {
            printf("%d, ", i_val_average->ave_val(-i));
        } catch (std::exception e) {
            printf("none");
            break;
        };
    printf("\n");
    fflush(stdout);
}

#endif //VOIP_TOOL_STAT_H
