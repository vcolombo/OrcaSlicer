// Local control API: global GUI+API slice lease (spec §4).
//
// Print::process keeps process-global state, so only one slice — GUI
// background or API job — may run per process. This is a pure state
// machine: threads, cancellation, and the FIFO wait itself belong to the
// owner. Never throws.

#pragma once

#include <cstddef>
#include <deque>

namespace Slic3r {
namespace LocalAPI {

enum class LeaseHolder { None, Gui, Api };

enum class AcquireResult {
    Acquired,  // caller owns the lease now
    Queued,    // API job waits; position counts ahead of it (0-based)
    PreemptGui,// API job takes the lease; the owner must cancel the GUI slice
    Busy,      // GUI must wait: an API job owns the lease
    Full,      // queue at capacity; caller answers LEASE_HELD
};

struct Acquire {
    AcquireResult result;
    size_t position = 0; // meaningful only for Queued
};

class SliceLease {
public:
    // max_queued bounds the waiting FIFO (active job excluded).
    explicit SliceLease(size_t max_queued = 8) : m_max_queued(max_queued) {}

    // API slice submit. Re-submitting the active id is idempotent.
    Acquire acquire_api(int job_id);
    // GUI background slice. Re-acquiring while held is idempotent.
    AcquireResult acquire_gui();

    void release_api(); // completes/cancels the active job, promotes head
    void release_gui(); // releases a GUI-held lease; always clears intent

    // Removes a waiting (not active) job. False when absent or active.
    bool cancel_queued(int job_id);

    LeaseHolder holder() const { return m_holder; }
    int active_job() const { return m_active_job; } // -1 when none
    size_t queue_length() const { return m_queue.size(); }
    size_t queue_position(int job_id) const; // 0-based, npos when absent
    static constexpr size_t npos = static_cast<size_t>(-1);

    // True when the GUI asked for the lease while API-owned (Busy) or was
    // preempted by an API submit: draining the API queue hands the lease
    // back to Gui so the restartable background slice resumes.
    bool gui_waiting() const { return m_gui_waiting; }

private:
    LeaseHolder m_holder = LeaseHolder::None;
    int m_active_job = -1;
    std::deque<int> m_queue;
    size_t m_max_queued;
    bool m_gui_waiting = false;
};

} // namespace LocalAPI
} // namespace Slic3r
