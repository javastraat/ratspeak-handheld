#pragma once

#include "StorageContract.h"
#include "AlignedStorageMemory.h"
#include <Arduino.h>
#include <atomic>
#include <cstdlib>

namespace handheld::storage {

// Cardputer reserves this block during setup, before the worker and networking
// start. Normal/legacy documents and key persistence share it under the storage
// lease. Display pixels stay independent so slow I/O never prevents drawing.
class LegacyMessageArena {
public:
    bool reserve() {
        if (_memory) return true;
        if (!Budget::canReserveLegacy(ESP.getFreeHeap(), ESP.getMaxAllocHeap())) return false;
        void* memory = allocateAlignedStorage(Budget::LegacyScratch);
        if (!memory) return false;
        if (ESP.getFreeHeap() < Budget::CardHeapFloor ||
            ESP.getMaxAllocHeap() < Budget::CardLargestBlockFloor) {
            freeAlignedStorage(memory); return false;
        }
        _memory = memory;
        return true;
    }
    bool available() const { return _memory != nullptr; }
    void* borrow() {
        // Callers hold StorageLease before borrowing. Refuse nested/reentrant
        // use instead of overwriting live bytes or waiting for ourselves.
        if (!_memory || _borrowed.test_and_set(std::memory_order_acquire)) return nullptr;
        return _memory;
    }
    void release() { _borrowed.clear(std::memory_order_release); }

private:
    void* _memory = nullptr;
    std::atomic_flag _borrowed = ATOMIC_FLAG_INIT;
};

inline LegacyMessageArena& legacyMessageArena() {
    static LegacyMessageArena arena;
    return arena;
}

// For synchronous non-document users. Declare after StorageLease so the
// scratch is returned before unlocking, including on exceptions/early returns.
class LegacyMessageBorrow {
public:
    explicit LegacyMessageBorrow(bool acquire = true)
        : _memory(acquire ? legacyMessageArena().borrow() : nullptr) {}
    ~LegacyMessageBorrow() { if (_memory) legacyMessageArena().release(); }
    LegacyMessageBorrow(const LegacyMessageBorrow&) = delete;
    LegacyMessageBorrow& operator=(const LegacyMessageBorrow&) = delete;
    void* data() const { return _memory; }
private:
    void* _memory;
};

} // namespace handheld::storage
