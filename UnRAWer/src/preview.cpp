#include "pch.h"

#include "preview.h"

#include <cmath>

static constexpr uint16_t kHalfOneBits = 0x3C00u;

static bool
MakeThumbnailRGBA16F(const std::string& file_path, int target_square_size, PreviewThumbnailRGBA16F* out)
{
    if (out == nullptr) {
        return false;
    }
    if (target_square_size <= 0) {
        return false;
    }

    spdlog::trace("Preview: thumbnail request for '{}' target_square_size={}", file_path, target_square_size);

    OIIO::ImageBuf src(file_path);
    if (!src.init_spec(file_path, 0, 0)) {
        spdlog::debug("Preview: init_spec failed for {}: {}", file_path, src.geterror());
        return false;
    }

    const int native_nchannels = src.spec().nchannels;
    const int chend_to_read    = std::max(1, std::min(3, native_nchannels));
    spdlog::trace("Preview: file '{}' spec {}x{} nchannels={} -> reading chend={}",
                  file_path, src.spec().width, src.spec().height, native_nchannels, chend_to_read);
    if (!src.read(0, 0, 0, chend_to_read, true, OIIO::TypeDesc::HALF)) {
        spdlog::debug("Preview: read failed for {}: {}", file_path, src.geterror());
        return false;
    }

    const int src_w = src.spec().width;
    const int src_h = src.spec().height;
    if (src_w <= 0 || src_h <= 0) {
        return false;
    }

    int dst_w = 1;
    int dst_h = 1;
    if (src_w >= src_h) {
        dst_w = target_square_size;
        dst_h = static_cast<int>(std::lround((double)src_h * (double)target_square_size / (double)src_w));
    } else {
        dst_h = target_square_size;
        dst_w = static_cast<int>(std::lround((double)src_w * (double)target_square_size / (double)src_h));
    }
    dst_w = std::max(1, std::min(dst_w, target_square_size));
    dst_h = std::max(1, std::min(dst_h, target_square_size));

    spdlog::debug("Preview: '{}' {}x{} -> {}x{} (half RGBA)",
                  file_path, src_w, src_h, dst_w, dst_h);

    PreviewThumbnailRGBA16F thumb;
    thumb.width       = dst_w;
    thumb.height      = dst_h;
    thumb.source_path = file_path;
    thumb.pixels_rgba16f.resize(static_cast<size_t>(dst_w) * static_cast<size_t>(dst_h) * 4u);

    OIIO::ImageSpec dst_spec(dst_w, dst_h, 4, OIIO::TypeDesc::HALF);
    OIIO::ImageBuf dst(dst_spec, thumb.pixels_rgba16f.data());

    const OIIO::ImageBuf* src_for_resample = &src;
    OIIO::ImageBuf src_tmp;
    if (src.nchannels() < 3) {
        const int last = std::max(0, src.nchannels() - 1);
        std::array<int, 3> order { 0, std::min(1, last), last };
        spdlog::trace("Preview: expanding channels {} -> 3 using order [{}, {}, {}]",
                      src.nchannels(), order[0], order[1], order[2]);
        if (!OIIO::ImageBufAlgo::channels(src_tmp, src, 3, order, {}, {}, false, 1)) {
            spdlog::debug("Preview: channels() failed for {}", file_path);
            return false;
        }
        src_for_resample = &src_tmp;
    }

    const OIIO::ROI roi(0, dst_w, 0, dst_h, 0, 1, 0, 3);
    if (!OIIO::ImageBufAlgo::resample(dst, *src_for_resample, true, roi, 1)) {
        spdlog::debug("Preview: resample() failed for {}: {}", file_path, dst.geterror());
        return false;
    }

    const size_t pixels = static_cast<size_t>(dst_w) * static_cast<size_t>(dst_h);
    for (size_t i = 0; i < pixels; ++i) {
        thumb.pixels_rgba16f[i * 4u + 3u] = kHalfOneBits;
    }

    *out = std::move(thumb);
    spdlog::trace("Preview: thumbnail ready for '{}'", file_path);
    return true;
}

PreviewQueue::PreviewQueue() = default;

PreviewQueue::~PreviewQueue()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = true;
    }
    m_cv.notify_all();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void
PreviewQueue::SetEnabled(bool enabled)
{
    m_enabled.store(enabled);
    spdlog::debug("Preview: enabled={}", enabled ? "true" : "false");
    if (!enabled) {
        Clear();
    }
}

bool
PreviewQueue::IsEnabled() const
{
    return m_enabled.load();
}

void
PreviewQueue::SetTargetSquareSize(int size_px)
{
    m_targetSquareSize.store(size_px);
}

int
PreviewQueue::GetTargetSquareSize() const
{
    return m_targetSquareSize.load();
}

void
PreviewQueue::SetRequestQueueMaxSize(int max_items)
{
    max_items = std::max(0, max_items);
    m_requestQueueMax.store(max_items);

    std::lock_guard<std::mutex> lock(m_mutex);
    if (max_items == 0) {
        m_requests.clear();
        return;
    }

    while (static_cast<int>(m_requests.size()) > max_items) {
        m_requests.pop_front();
    }
}

int
PreviewQueue::GetRequestQueueMaxSize() const
{
    return m_requestQueueMax.load();
}

void
PreviewQueue::SetReadyQueueMaxSize(int max_items)
{
    max_items = std::max(0, max_items);
    m_readyQueueMax.store(max_items);

    std::lock_guard<std::mutex> lock(m_readyMutex);
    if (max_items == 0) {
        m_readyQueue.clear();
        return;
    }

    while (static_cast<int>(m_readyQueue.size()) > max_items) {
        m_readyQueue.pop_front();
    }
}

int
PreviewQueue::GetReadyQueueMaxSize() const
{
    return m_readyQueueMax.load();
}

void
PreviewQueue::EnsureStartedLocked()
{
    if (m_started) {
        return;
    }
    m_started = true;
    m_thread  = std::thread(&PreviewQueue::WorkerLoop, this);
}

void
PreviewQueue::EnqueuePath(const char* out_file_path)
{
    EnqueuePath(out_file_path, 0, 0);
}

void
PreviewQueue::EnqueuePath(const char* out_file_path, int file_index1, int total_files)
{
    if (out_file_path == nullptr || out_file_path[0] == '\0') {
        return;
    }
    if (!m_enabled.load()) {
        return;
    }

    spdlog::trace("Preview: enqueue '{}' ({}/{})", out_file_path, file_index1, total_files);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stop) {
            return;
        }
        EnsureStartedLocked();
        const int max_req = m_requestQueueMax.load();
        if (max_req <= 0) {
            return;
        }
        if (static_cast<int>(m_requests.size()) >= max_req) {
            spdlog::debug("Preview: request queue full ({}), dropping oldest", max_req);
            m_requests.pop_front();
        }
        m_requests.push_back(Request { std::string(out_file_path), file_index1, total_files });
    }
    m_cv.notify_one();
}

bool
PreviewQueue::TryConsume(PreviewThumbnailRGBA16F* out)
{
    if (out == nullptr) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_readyMutex);
    if (m_readyQueue.empty()) {
        return false;
    }
    *out = std::move(m_readyQueue.front());
    m_readyQueue.pop_front();
    return true;
}

void
PreviewQueue::Clear()
{
    spdlog::debug("Preview: clear queues");
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_requests.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_readyMutex);
        m_readyQueue.clear();
    }
}

void
PreviewQueue::WorkerLoop()
{
    for (;;) {
        Request req;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return m_stop || !m_requests.empty(); });
            if (m_stop && m_requests.empty()) {
                return;
            }
            req = std::move(m_requests.front());
            m_requests.pop_front();
        }

        if (!m_enabled.load()) {
            continue;
        }

        const int target_size = m_targetSquareSize.load();
        if (target_size <= 0) {
            continue;
        }

        spdlog::trace("Preview: worker processing '{}'", req.path);
        PreviewThumbnailRGBA16F thumb;
        if (!MakeThumbnailRGBA16F(req.path, target_size, &thumb)) {
            continue;
        }
        thumb.file_index1 = req.file_index1;
        thumb.total_files = req.total_files;

        std::lock_guard<std::mutex> lock(m_readyMutex);
        const int max_ready = m_readyQueueMax.load();
        if (max_ready <= 0) {
            continue;
        }

        if (static_cast<int>(m_readyQueue.size()) >= max_ready) {
            spdlog::debug("Preview: ready queue full ({}), dropping oldest", max_ready);
            m_readyQueue.pop_front();
        }
        m_readyQueue.push_back(std::move(thumb));
    }
}
