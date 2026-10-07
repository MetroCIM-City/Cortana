#pragma once

template <class T>
class ComPtr {
public:
    ComPtr() = default;
    explicit ComPtr(T* pointer) : pointer_(pointer) {}
    ~ComPtr() { Reset(); }

    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    ComPtr(ComPtr&& other) noexcept : pointer_(other.pointer_) { other.pointer_ = nullptr; }
    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            Reset();
            pointer_ = other.pointer_;
            other.pointer_ = nullptr;
        }
        return *this;
    }

    T* Get() const { return pointer_; }
    T* operator->() const { return pointer_; }
    explicit operator bool() const { return pointer_ != nullptr; }

    T** Put() {
        Reset();
        return &pointer_;
    }

    void Reset() {
        if (pointer_) {
            pointer_->Release();
            pointer_ = nullptr;
        }
    }

private:
    T* pointer_ = nullptr;
};
