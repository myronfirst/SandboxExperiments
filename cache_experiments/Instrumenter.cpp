#include "Instrumenter.h"

#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string_view>
#include <thread>

#include <unistd.h>

namespace {
    auto GetProcessId() -> uint64_t { return static_cast<uint64_t>(getpid()); }
    auto GetThreadId() -> uint64_t {
        uint64_t tid{};
        std::stringstream ss;
        ss << std::this_thread::get_id();
        ss >> tid;
        return tid;
    }
    template<typename T>
    struct TypeName {
        constexpr static auto GetName() -> std::string_view { return typeid(T).name(); }
        constexpr static auto GetSuffix() -> std::string_view { return typeid(T).name(); }
    };
    template<>
    struct TypeName<std::chrono::seconds> {
        constexpr static auto GetName() -> std::string_view { return "seconds"; }
        constexpr static auto GetSuffix() -> std::string_view { return "s"; }
    };
    template<>
    struct TypeName<std::chrono::milliseconds> {
        constexpr static auto GetName() -> std::string_view { return "milliseconds"; }
        constexpr static auto GetSuffix() -> std::string_view { return "ms"; }
    };
    template<>
    struct TypeName<std::chrono::microseconds> {
        constexpr static auto GetName() -> std::string_view { return "microseconds"; }
        constexpr static auto GetSuffix() -> std::string_view { return "us"; }
    };
    template<>
    struct TypeName<std::chrono::nanoseconds> {
        constexpr static auto GetName() -> std::string_view { return "nanoseconds"; }
        constexpr static auto GetSuffix() -> std::string_view { return "ns"; }
    };

    auto operator<<(std::ostream& os, const Trace& obj) -> std::ostream& {
        os << "{"
           << std::quoted("name") << ": " << std::quoted(obj.name) << ", "
           << std::quoted("cat") << ": " << std::quoted(obj.category) << ", "
           << std::quoted("ph") << ": " << std::quoted(obj.type) << ", "
           << std::quoted("ts") << ": " << obj.timestamp << ", "
           //    << std::quoted("tts") << ": " //thread clock timestamp
           << std::quoted("dur") << ": " << obj.duration << ", "
           //    << std::quoted("tts") << ": " //thread clock duration
           << std::quoted("pid") << ": " << obj.processId << ", "
           << std::quoted("tid") << ": " << obj.threadId
           << "}";
        return os;
    }

    [[maybe_unused]] auto SaveAsJSON(const std::string& fileName, const std::vector<Trace>& traceVec) -> void {
        std::ofstream f{ fileName + ".json" };
        f << "{\n"
          << std::quoted("displayTimeUnit") << ": " << std::quoted(TypeName<Timer::Unit>::GetSuffix()) << ",\n"
          << std::quoted("traceEvents") << ": [\n";
        const size_t sz = std::size(traceVec);
        if (sz) {
            for (auto i = 0u; i < sz - 1; ++i)
                f << traceVec.at(i) << ",\n";
            f << traceVec.at(sz - 1) << "\n";
        }
        f << "]\n"
          << "}\n";
    }
    const auto Header = "experiments, " + std::string{ TypeName<Timer::Unit>::GetName() };
    [[maybe_unused]] auto SaveAsCSV(const std::string& fileName, const std::vector<Trace>& traceVec) -> void {
        std::ofstream f{ fileName + ".csv" };
        f << Header << '\n';
        for (const auto& trace : traceVec) {
            f << trace.name << ','
              << trace.duration
              << '\n';
        }
    }
    auto PrefixCSV(const std::string& fileName) -> void {
        std::ofstream f{ fileName + ".csv" };
        f << Header << '\n';
    }
    auto PrefixCSVIfEmpty(const std::string& fileName) -> void {
        {
            std::ifstream f{ fileName + ".csv" };
            std::string line{};
            std::getline(f, line);
            if (line == Header) return;
        }
        PrefixCSV(fileName);
    }
    auto AppendCSV(const std::string& fileName, const Trace& trace) -> void {
        std::ofstream f{ fileName + ".csv", std::ios_base::app };
        f << trace.name << ',' << trace.duration << '\n';
    }

}    // namespace

Trace::Trace(const std::string& _name, uint64_t _timestamp, uint64_t _duration)
    : name{ _name }, category{ "function" }, type{ "X" }, timestamp{ _timestamp }, duration{ _duration }, processId{ GetProcessId() }, threadId{ GetThreadId() } {}

Timer::Timer(const std::string& _id) : id(_id) {
    begin = Clock::now();
}
Timer::~Timer() {
    end = Clock::now();
    assert(begin <= end);
    const uint64_t timestamp = std::chrono::duration_cast<Unit>(begin - TimePoint::min()).count();
    const uint64_t duration = std::chrono::duration_cast<Unit>(end - begin).count();
    Instrumenter::Get().AddTrace({ id, timestamp, duration });
}

auto Instrumenter::Get() -> Instrumenter& {
    static Instrumenter instance{};
    return instance;
}

auto Instrumenter::AddTrace(const Trace& val) -> void {
    std::scoped_lock<std::mutex> lock{ traceVecMutex };
    traceVec.emplace_back(val);
    InvokeTraceCallbacks(traceCallbacks, val);
}
auto Instrumenter::BeginSession(const std::string& _fileName) -> void {
    traceVec.clear();
    fileName = _fileName;
    InvokeBeginSessionCallbacks(beginSessionCallbacks);
}
auto Instrumenter::EndSession() const -> void {
    InvokeEndSessionCallbacks(endSessionCallbacks);
}
auto Instrumenter::InstallBeginSessionCallback(auto&& f) -> void {
    beginSessionCallbacks.emplace_back(f);
}
auto Instrumenter::InstallEndSessionCallback(auto&& f) -> void {
    endSessionCallbacks.emplace_back(f);
}
auto Instrumenter::InstallTraceCallback(auto&& f) -> void {
    traceCallbacks.emplace_back(f);
}

Instrumenter::Instrumenter() { InstallAllCallbacks(); }
auto Instrumenter::InvokeBeginSessionCallbacks(const auto& cbVec) const -> void {
    for (const auto& cb : cbVec) cb(fileName);
}
auto Instrumenter::InvokeEndSessionCallbacks(const auto& cbVec) const -> void {
    for (const auto& cb : cbVec) cb(fileName, traceVec);
}
auto Instrumenter::InvokeTraceCallbacks(const auto& cbVec, const Trace& trace) const -> void {
    for (const auto& cb : cbVec) cb(fileName, trace);
}

auto Instrumenter::InstallAllCallbacks() -> void {
    // Append to file on each trace
    // If file is present, continue where we left off
    InstallBeginSessionCallback(PrefixCSVIfEmpty);
    InstallTraceCallback(AppendCSV);

    // Append to file on each trace
    // InstallBeginSessionCallback(PrefixCSV);
    // InstallTraceCallback(AppendCSV);

    // InstallEndSessionCallback(SaveAsJSON); //Write file on destruction
    // InstallEndSessionCallback(SaveAsCSV); //Write file on destruction
}
