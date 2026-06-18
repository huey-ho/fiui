#pragma once

#include "fiui/diagnostics.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace fiui {

class DiagnosticsHub {
public:
    DiagnosticsHub();

    void configure(const DiagnosticsConfig& config);
    [[nodiscard]] DebugMode mode() const;
    void flush();
    void event(const char* category,
               const char* action,
               ObjectId object_id,
               std::uint32_t generation,
               std::uint64_t frame_id,
               const char* path,
               const char* detail);
    void frame_json(const std::string& json);
    void leak(ObjectId object_id,
              std::uint32_t generation,
              std::uint64_t frame_id,
              const char* path,
              const char* detail);
    [[nodiscard]] const std::string& session_id() const noexcept;

private:
    mutable std::mutex mutex_;
    DiagnosticsConfig config_;
    std::vector<std::string> trace_;
    std::vector<std::string> leaks_;
    std::string frame_ = "{}\n";
    std::string session_id_;
};

void diagnostics_event(const char* category,
                       const char* action,
                       ObjectId object_id,
                       const char* path,
                       const char* detail);
void diagnostics_event_ex(const char* category,
                          const char* action,
                          ObjectId object_id,
                          std::uint32_t generation,
                          std::uint64_t frame_id,
                          const char* path,
                          const char* detail);
void diagnostics_frame_json(const std::string& json);
void diagnostics_leak(ObjectId object_id, const char* path, const char* detail);
void diagnostics_leak_ex(ObjectId object_id,
                         std::uint32_t generation,
                         std::uint64_t frame_id,
                         const char* path,
                         const char* detail);

} // namespace fiui
