#pragma once

#include <cstddef>
#include <utility>

namespace dbnet {
constexpr std::size_t MAX_INCOMING_EVENTS = 256;
constexpr std::size_t MAX_INCOMING_MESSAGE_BYTES = 1024u * 1024u;

template<class Queue, class Value>
bool pushBoundedIncoming(Queue& queue, Value&& value) {
    const bool dropped = queue.size() >= MAX_INCOMING_EVENTS;
    if (dropped) queue.pop_front();
    queue.push_back(std::forward<Value>(value));
    return dropped;
}
}
