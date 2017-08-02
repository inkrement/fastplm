#include "CrashQueue.h"

CrashQueue* mainQueue = nullptr;

CrashQueue::CrashQueue(std::size_t threadCount): aliveWorkers(threadCount) {
    for (std::size_t i = 0; i < threadCount; i ++) {
        workers.emplace_back([this]() -> void {
            while (running) {
                void* payload;
                {
                    std::unique_lock<std::mutex> lock(taskFetchingMutex);
                    if (payloads.empty()) {
                        aliveWorkers.fetch_sub(1);
                        sleepClock.notify_one();

                        alarmClock.wait(lock);
                        continue;
                    }
                    payload = payloads.front();
                    payloads.pop();
                }

                function(payload);
            }
        });
    }

    // Make sure all workers finished running.
    while (aliveWorkers.load() > 0);
    std::unique_lock<std::mutex> lock(taskFetchingMutex);
}

CrashQueue::~CrashQueue() {
    running = false;
    alarmClock.notify_all();
    for (auto& worker : workers)
        worker.join();
}

void CrashQueue::crash() {
    this->aliveWorkers = workers.size();
    alarmClock.notify_all();

    while (true) {
        std::unique_lock<std::mutex> lock(taskFetchingMutex);
        if (aliveWorkers.load() == 0)
            break;
        sleepClock.wait(lock);
    }
}

void CrashQueue::commit(CrashQueue::FunctionType&& function,
                        std::queue<CrashQueue::PayloadType>&& payloads) {
    this->function = std::move(function);
    this->payloads = std::move(payloads);
}
