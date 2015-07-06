#include <crossbow/logger.hpp>

#include <unordered_map>

namespace crossbow {
namespace logger {

namespace {

const std::unordered_map<crossbow::string, LogLevel> gLogLevelNames = {
    std::make_pair(crossbow::string("TRACE"), LogLevel::TRACE),
    std::make_pair(crossbow::string("DEBUG"), LogLevel::DEBUG),
    std::make_pair(crossbow::string("INFO"), LogLevel::INFO),
    std::make_pair(crossbow::string("WARN"), LogLevel::WARN),
    std::make_pair(crossbow::string("ERROR"), LogLevel::ERROR),
    std::make_pair(crossbow::string("FATAL"), LogLevel::FATAL)
};

} // anonymous namespace

Logger logger;

LoggerT::~LoggerT() {
    for (auto& fun : config.destructFunctions) {
        fun();
    }
}

LogLevel logLevelFromString(const crossbow::string& s) {
    return gLogLevelNames.at(s);
}

} // namespace logger
} // namespace crossbow
