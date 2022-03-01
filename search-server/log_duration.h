#pragma once

#include <chrono>
#include <iostream>
#include <string>

#define LOG_DURATION_STREAM(x,out) LogDuration UNIQUE_VAR_NAME_PROFILE(x,out)
#define LOG_DURATION(x) LogDuration UNIQUE_VAR_NAME_PROFILE(x)

#define PROFILE_CONCAT_INTERNAL(X, Y) X ## Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL (X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profile_guard_, __LINE__)

class LogDuration {
public:
    using Clock = std::chrono::steady_clock;
    LogDuration() {

    }
    LogDuration(const std::string name) : 
        name_(name) {

    }
    LogDuration(const std::string name, std::ostream& out) :
        name_(name), out_(out) {

    }

    ~LogDuration() {
        using namespace std::chrono;
        using namespace std::literals;

        const auto end_time = Clock::now();
        const auto dur = end_time - start_time_;
        std::cerr << name_ << ": "s <<
            duration_cast<milliseconds>(dur).count() << " ms"s << std::endl;
    }
private:
    const Clock::time_point start_time_ = Clock::now();
    std::string name_ = "";
    std::ostream& out_ = std::cerr;
};