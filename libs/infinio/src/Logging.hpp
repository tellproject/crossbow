#pragma once

#ifdef INFINIO_DEBUG

#include <boost/format.hpp>

#include <iostream>

template<class... Args>
struct LogFormatter;

template<class Head, class... Tail>
struct LogFormatter<Head, Tail...> {
    void format(boost::format& f, Head h, Tail... tail) const {
        f % h;
        LogFormatter<Tail...>().format(f, tail...);
    }
};

template<>
struct LogFormatter<> {
    void format(boost::format&) const {
        return;
    }
};

template<class... Args>
static void infinioLog(const std::string& message, Args... args) {
    boost::format formatter(message);
    LogFormatter<Args...> fmt;
    fmt.format(formatter, args...);
    std::cout << formatter.str() << std::endl;
}

#define INFINIO_LOG(...) infinioLog(__VA_ARGS__)
#else
#define INFINIO_LOG(...)
#endif
