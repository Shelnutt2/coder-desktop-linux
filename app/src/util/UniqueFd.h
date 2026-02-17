#ifndef UNIQUEFD_H
#define UNIQUEFD_H

#include <unistd.h>
#include <utility>

/// RAII wrapper for a Linux file descriptor.
/// Closes the fd on destruction. Move-only (not copyable).
class UniqueFd {
public:
    constexpr UniqueFd() noexcept = default;
    constexpr explicit UniqueFd(int fd) noexcept : m_fd(fd) {}
    ~UniqueFd() { reset(); }

    // Move-only
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    UniqueFd(UniqueFd&& other) noexcept : m_fd(std::exchange(other.m_fd, -1)) {}
    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            reset();
            m_fd = std::exchange(other.m_fd, -1);
        }
        return *this;
    }

    [[nodiscard]] constexpr int get() const noexcept { return m_fd; }
    [[nodiscard]] constexpr bool isValid() const noexcept { return m_fd >= 0; }
    constexpr explicit operator bool() const noexcept { return isValid(); }

    /// Release ownership and return the raw fd.
    [[nodiscard]] int release() noexcept { return std::exchange(m_fd, -1); }

    /// Close the fd and reset to -1.
    void reset(int fd = -1) noexcept {
        if (m_fd >= 0) ::close(m_fd);
        m_fd = fd;
    }

private:
    int m_fd = -1;
};

#endif  // UNIQUEFD_H
