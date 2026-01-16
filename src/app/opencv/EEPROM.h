#pragma once

// OpenCV(host) build stub for the Arduino EEPROM library.
// Enough to satisfy integrated Arduboy game sources.

#include <cstdint>
#include <cstring>
#include <vector>

class EEPROMClass {
public:
    bool begin(std::size_t size) {
        if (size == 0) {
            storage_.clear();
        } else {
            storage_.resize(size, 0);
        }
        return true;
    }

    uint8_t read(int address) const {
        if (address < 0) return 0;
        const std::size_t idx = static_cast<std::size_t>(address);
        if (idx >= storage_.size()) return 0;
        return storage_[idx];
    }

    void write(int address, uint8_t value) {
        if (address < 0) return;
        const std::size_t idx = static_cast<std::size_t>(address);
        if (idx >= storage_.size()) return;
        storage_[idx] = value;
    }

    template <typename T>
    const T& put(int address, const T& value) {
        if (address < 0) return value;
        const std::size_t idx = static_cast<std::size_t>(address);
        if (idx + sizeof(T) > storage_.size()) return value;
        std::memcpy(&storage_[idx], &value, sizeof(T));
        return value;
    }

    template <typename T>
    T& get(int address, T& value) {
        if (address < 0) {
            std::memset(&value, 0, sizeof(T));
            return value;
        }
        const std::size_t idx = static_cast<std::size_t>(address);
        if (idx + sizeof(T) > storage_.size()) {
            std::memset(&value, 0, sizeof(T));
            return value;
        }
        std::memcpy(&value, &storage_[idx], sizeof(T));
        return value;
    }

    bool commit() { return true; }

private:
    std::vector<uint8_t> storage_;
};

inline EEPROMClass EEPROM;
