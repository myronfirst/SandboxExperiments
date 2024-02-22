
#include <array>
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

#include <pthread.h>
#include <signal.h>
#include <unistd.h>

constexpr auto WORKER_NUM = 1;
constexpr auto THREAD_NUM = WORKER_NUM + 2;
std::array<pthread_t, THREAD_NUM> threadIds{};

using namespace std::chrono_literals;

auto SigUsrHandler(int sig, siginfo_t* info, void* uContext) -> void {
    (void)uContext;
    const auto id = pthread_self();
    std::cout << id << " handles " << sig << '\n';
    std::cout << "sig: " << sig << ", signo: " << info->si_signo << ", errno: " << info->si_errno << ", code: " << info->si_code << '\n';
}

auto WorkerFunc(int idx) -> void {
    threadIds.at(idx) = pthread_self();
    std::cout << threadIds.at(idx) << " started " << '\n';
    while (1) {
        std::this_thread::sleep_for(1s);
    }
}

auto SignalFunc(int idx) -> void {
    threadIds.at(idx) = pthread_self();
    while (1) {
        std::this_thread::sleep_for(4s);
        std::cout << threadIds.at(idx) << " sends " << SIGUSR1 << '\n';
        int ret = 0;
        ret = pthread_kill(threadIds.at(1), SIGUSR1);
        assert(ret == 0);
    }
}

auto main() -> int {
    threadIds.fill(0LU);
    threadIds.at(0) = pthread_self();
    struct sigaction act;
    int ret = 0;
    ret = sigemptyset(&act.sa_mask);
    assert(ret == 0);
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = SigUsrHandler;
    ret = sigaction(SIGUSR1, &act, NULL);
    assert(ret == 0);
    std::vector<std::jthread> threadVec{ WORKER_NUM };
    size_t idx = 1;
    for (auto& thread : threadVec) {
        thread = std::jthread{ WorkerFunc, idx };
        idx += 1;
    }
    std::jthread signalThread{ SignalFunc, idx };
    return 0;
}
