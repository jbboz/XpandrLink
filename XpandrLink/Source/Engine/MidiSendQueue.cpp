#include "MidiSendQueue.h"

// All bodies below were moved verbatim from MidiEngine.cpp (Phase 4 extraction).
// The only changes are field renames (trailing underscore) and routing the drain
// loop's send side-effect through onSendSysex instead of calling sendSysex directly.

void MidiSendQueue::enqueueMessage(std::vector<uint8_t> bytes, int delayAfterMs)
{
    queue_.push_back({ std::move(bytes), {}, delayAfterMs });
}

void MidiSendQueue::enqueueAction(std::function<void()> action, int delayAfterMs)
{
    queue_.push_back({ {}, std::move(action), delayAfterMs });
}

void MidiSendQueue::enqueuePageSelectIfNeeded(int page, int mode, int settleMs, uint8_t sysexId, bool force)
{
    if (!force && lastQueuedPage_ == page && lastQueuedMode_ == mode) return;
    std::vector<uint8_t> ps = {
        0xF0, 0x10, sysexId,
        0x0B, (uint8_t)page, (uint8_t)mode,
        0xF7
    };
    enqueueMessage(std::move(ps), settleMs);
    lastQueuedPage_ = page;
    lastQueuedMode_ = mode;
}

void MidiSendQueue::drain(juce::uint32 now)
{
    while (!queue_.empty())
    {
        // Use signed subtraction to handle uint32 wraparound (~49-day period).
        if ((juce::int32)(now - nextSendTime_) < 0) break;

        auto msg = std::move(queue_.front());
        queue_.pop_front();

        if (!msg.bytes.empty())
        {
            // MidiEngine's onSendSysex reproduces the old drain body exactly: it calls
            // sendSysex(bytes) and then the page-select / mod-routing byte-inspection that
            // updates lastSentPage/lastSentMode/lastModSentTime_ (see MidiEngine ctor).
            if (onSendSysex) onSendSysex(msg.bytes, now);
        }
        else if (msg.action)
        {
            msg.action();
        }

        nextSendTime_ = now + (juce::uint32)msg.delayAfterMs;
        if (msg.delayAfterMs > 0) break;  // settle before next message
    }
}

void MidiSendQueue::reset()
{
    queue_.clear();
    lastQueuedPage_ = -1;
    lastQueuedMode_ = -1;
}
