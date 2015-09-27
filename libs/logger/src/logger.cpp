/*
 * (C) Copyright 2015 ETH Zurich Systems Group (http://www.systems.ethz.ch/) and others.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Contributors:
 *     Markus Pilman <mpilman@inf.ethz.ch>
 *     Simon Loesing <sloesing@inf.ethz.ch>
 *     Thomas Etter <etterth@gmail.com>
 *     Kevin Bocksrocker <kevin.bocksrocker@gmail.com>
 *     Lucas Braun <braunl@inf.ethz.ch>
 */
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
