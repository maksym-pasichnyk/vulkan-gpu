//
// Created by Maksym Pasichnyk on 07.04.2023.
//

#pragma once

class ManagedObject {
protected:
    explicit ManagedObject() : _refs(1) {}
    virtual ~ManagedObject() = default;

public:
    void retain() {
        _refs.fetch_add(1);
    }

    void release() {
        if (_refs.fetch_sub(1) == 1) {
            delete this;
        }
    }

private:
    std::atomic_uint64_t _refs;
};