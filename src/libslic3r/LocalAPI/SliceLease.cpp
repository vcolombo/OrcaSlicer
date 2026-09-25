#include "SliceLease.hpp"

#include <algorithm>

namespace Slic3r {
namespace LocalAPI {

Acquire SliceLease::acquire_api(int job_id)
{
    if (m_holder == LeaseHolder::None) {
        m_holder = LeaseHolder::Api;
        m_active_job = job_id;
        return {AcquireResult::Acquired, 0};
    }
    if (m_holder == LeaseHolder::Gui) {
        // The GUI slice is restartable and cache-assisted; the API job
        // takes the lease and the owner cancels the GUI worker. The
        // interrupted slice will want to resume on drain.
        m_holder = LeaseHolder::Api;
        m_active_job = job_id;
        m_gui_waiting = true;
        return {AcquireResult::PreemptGui, 0};
    }
    if (job_id == m_active_job)
        return {AcquireResult::Acquired, 0};
    auto it = std::find(m_queue.begin(), m_queue.end(), job_id);
    if (it != m_queue.end())
        return {AcquireResult::Queued, static_cast<size_t>(it - m_queue.begin())};
    if (m_queue.size() >= m_max_queued)
        return {AcquireResult::Full, 0};
    m_queue.push_back(job_id);
    return {AcquireResult::Queued, m_queue.size() - 1};
}

AcquireResult SliceLease::acquire_gui()
{
    if (m_holder == LeaseHolder::None) {
        m_holder = LeaseHolder::Gui;
        return AcquireResult::Acquired;
    }
    if (m_holder == LeaseHolder::Api) {
        m_gui_waiting = true; // GUI waits; it resumes on drain.
        return AcquireResult::Busy;
    }
    // GUI restarts route through cancel-then-acquire; re-entry is the
    // same holder continuing.
    return AcquireResult::Acquired;
}

void SliceLease::release_api()
{
    if (m_holder != LeaseHolder::Api)
        return;
    if (!m_queue.empty()) {
        m_active_job = m_queue.front();
        m_queue.pop_front();
        return;
    }
    // Hand a waiting GUI back its lease so the background slice resumes.
    m_holder = m_gui_waiting ? LeaseHolder::Gui : LeaseHolder::None;
    m_gui_waiting = false;
    m_active_job = -1;
}

void SliceLease::release_gui()
{
    m_gui_waiting = false;
    if (m_holder == LeaseHolder::Gui)
        m_holder = LeaseHolder::None;
}

bool SliceLease::cancel_queued(int job_id)
{
    auto it = std::find(m_queue.begin(), m_queue.end(), job_id);
    if (it == m_queue.end())
        return false;
    m_queue.erase(it);
    return true;
}

size_t SliceLease::queue_position(int job_id) const
{
    auto it = std::find(m_queue.begin(), m_queue.end(), job_id);
    return it == m_queue.end() ? npos : static_cast<size_t>(it - m_queue.begin());
}

} // namespace LocalAPI
} // namespace Slic3r
