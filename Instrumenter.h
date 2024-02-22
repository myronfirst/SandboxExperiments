#ifndef INSTRUMENTER_H
#define INSTRUMENTER_H

#include <functional>
#include <mutex>
#include <string>
#include <vector>

struct Trace {
    std::string name{};
    std::string category{};
    std::string type{};
    uint64_t timestamp{};
    uint64_t duration{};
    uint64_t processId{};
    uint64_t threadId{};
    Trace(const std::string& _name, uint64_t _timestamp, uint64_t _duration);
};

class Timer {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Unit = std::chrono::milliseconds;
    Timer(const Timer&) = delete;
    Timer(Timer&) = delete;
    Timer& operator=(const Timer&) = delete;
    Timer& operator=(Timer&&) = delete;

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
    Instrumenter& operator=(const Instrumenter&) = delete;
    Instrumenter& operator=(Instrumenter&&) = delete;

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
