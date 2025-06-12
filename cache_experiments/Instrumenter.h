#ifndef INSTRUMENTER_H
#define INSTRUMENTER_H

#include <functional>
#include <mutex>
#include <string>
#include <vector>

using Unit = std::chrono::milliseconds;

struct Trace {
    std::string name{};
    std::string category{};
    std::string type{};
    Unit timestamp{};
    Unit duration{};
    uint64_t processId{};
    uint64_t threadId{};
    Trace(const std::string& _name, Unit _timestamp, Unit _duration);
};

class Timer {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    Timer(const Timer&) = delete;
    Timer(Timer&) = delete;
    auto operator=(const Timer&) = delete;
    auto operator=(Timer&&) -> Timer& = delete;

public:
    Timer(const std::string& _id);
    ~Timer();

private:
    std::string id{};
    TimePoint begin{};
    TimePoint end{};
};

class Instrumenter {
public:
    using BeginSessionCallbackVector = std::vector<std::function<void(const std::string&)>>;
    using EndSessionCallbackVector = std::vector<std::function<void(const std::string&, const std::vector<Trace>&)>>;
    using TraceCallbackVector = std::vector<std::function<void(const std::string&, const Trace&)>>;
    static auto Get() -> Instrumenter&;
    ~Instrumenter() = default;
    Instrumenter(const Instrumenter&) = delete;
    Instrumenter(const Instrumenter&&) = delete;
    auto operator=(const Instrumenter&) -> Instrumenter& = delete;
    auto operator=(Instrumenter&&) -> Instrumenter& = delete;

public:
    auto AddTrace(const Trace&) -> void;
    auto BeginSession(const std::string&) -> void;
    auto EndSession() const -> void;
    auto InstallBeginSessionCallback(auto&&) -> void;
    auto InstallEndSessionCallback(auto&&) -> void;
    auto InstallTraceCallback(auto&&) -> void;

private:
    auto InvokeBeginSessionCallbacks(const auto&) const -> void;
    auto InvokeEndSessionCallbacks(const auto&) const -> void;
    auto InvokeTraceCallbacks(const auto&, const Trace&) const -> void;
    auto InstallAllCallbacks() -> void;
    Instrumenter();
    std::string fileName{};
    std::vector<Trace> traceVec{};
    std::mutex traceVecMutex{};
    BeginSessionCallbackVector beginSessionCallbacks{};
    EndSessionCallbackVector endSessionCallbacks{};
    TraceCallbackVector traceCallbacks{};
};

#define __CAT(X, Y) X##Y
#define _CAT(X, Y) __CAT(X, Y)

#define INSTRUMENT_BEGIN_SESSION(fileName) Instrumenter::Get().BeginSession(fileName)
#define INSTRUMENT_END_SESSION() Instrumenter::Get().EndSession()
#define TIME_SCOPE(id) Timer _CAT(_timer_, __LINE__)(id)
#define TIME_FUNCTION() TIME_SCOPE(__FUNCTION__)

#endif
