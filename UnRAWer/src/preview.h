#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

struct PreviewThumbnailRGBA16F {
    int width  = 0;
    int height = 0;
    std::vector<uint16_t> pixels_rgba16f;
    std::string source_path;
    int file_index1 = 0;
    int total_files = 0;
};

class PreviewQueue {
public:
    PreviewQueue();
    ~PreviewQueue();

    PreviewQueue(const PreviewQueue&)            = delete;
    PreviewQueue& operator=(const PreviewQueue&) = delete;

    void SetEnabled(bool enabled);
    bool IsEnabled() const;

    void SetTargetSquareSize(int size_px);
    int GetTargetSquareSize() const;

    void SetRequestQueueMaxSize(int max_items);
    int GetRequestQueueMaxSize() const;

    void SetReadyQueueMaxSize(int max_items);
    int GetReadyQueueMaxSize() const;

    void EnqueuePath(const char* out_file_path);
    void EnqueuePath(const char* out_file_path, int file_index1, int total_files);
    bool TryConsume(PreviewThumbnailRGBA16F* out);

    void Clear();

private:
    struct Request {
        std::string path;
        int file_index1 = 0;
        int total_files = 0;
    };

    void EnsureStartedLocked();
    void WorkerLoop();

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<Request> m_requests;
    bool m_started = false;
    bool m_stop    = false;

    std::atomic<int> m_targetSquareSize { 0 };
    std::atomic<bool> m_enabled { true };
    std::atomic<int> m_requestQueueMax { 100 };
    std::atomic<int> m_readyQueueMax { 100 };

    std::mutex m_readyMutex;
    std::deque<PreviewThumbnailRGBA16F> m_readyQueue;

    std::thread m_thread;
};
