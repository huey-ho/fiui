#include "fiui/diagnostics.h"

#include "diagnostics/diagnostics_internal.h"
#include "runtime/runtime.h"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fiui {
namespace {

std::uint64_t timestamp_ms()
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

std::string escape_json(const char* input)
{
    std::string out;
    if (input == nullptr) {
        return out;
    }
    for (const char ch : std::string(input)) {
        switch (ch) {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out += ch;
            break;
        }
    }
    return out;
}

void write_text_file(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
}

std::string make_session_id()
{
    std::ostringstream id;
    id << "fiui-session-" << timestamp_ms();
    return id.str();
}

bool is_failure_action(const char* action) noexcept
{
    return action != nullptr &&
           (std::strstr(action, "failed") != nullptr ||
            std::strstr(action, "failure") != nullptr ||
            std::strstr(action, "lost") != nullptr ||
            std::strstr(action, "exception") != nullptr);
}

bool basic_trace_allowed(const char* category, const char* action) noexcept
{
    if (is_failure_action(action)) {
        return true;
    }
    if (category == nullptr || action == nullptr) {
        return false;
    }
    if (std::strcmp(category, "diagnostics") == 0 || std::strcmp(category, "app") == 0 ||
        std::strcmp(category, "lifecycle") == 0 || std::strcmp(category, "render") == 0) {
        return true;
    }
    if (std::strcmp(category, "platform") == 0) {
        return std::strcmp(action, "window_created") == 0 ||
               std::strcmp(action, "window_destroyed") == 0 ||
               std::strcmp(action, "render_paint") == 0 ||
               std::strcmp(action, "render_after_input") == 0;
    }
    if (std::strcmp(category, "scheduler") == 0) {
        return std::strcmp(action, "complete_frame") == 0;
    }
    return false;
}

} // namespace

DiagnosticsHub::DiagnosticsHub()
    : session_id_(make_session_id())
{
}

void DiagnosticsHub::configure(const DiagnosticsConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

DebugMode DiagnosticsHub::mode() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.mode;
}

void DiagnosticsHub::flush()
{
    std::lock_guard<std::mutex> lock(mutex_);
    const std::filesystem::path directory =
        config_.output_directory == nullptr ? "." : config_.output_directory;
    std::filesystem::create_directories(directory);

    std::ostringstream trace;
    for (const std::string& event : trace_) {
        trace << event << '\n';
    }
    write_text_file(directory / "fiui-trace.jsonl", trace.str());
    write_text_file(directory / "fiui-frame.json", frame_);

    std::ostringstream leaks;
    leaks << "{\n";
    leaks << "  \"schema\": \"fiui.leaks.v0\",\n";
    leaks << "  \"session_id\": \"" << escape_json(session_id_.c_str()) << "\",\n";
    leaks << "  \"leaks\": [\n";
    for (std::size_t index = 0; index < leaks_.size(); ++index) {
        leaks << leaks_[index];
        if (index + 1 < leaks_.size()) {
            leaks << ',';
        }
        leaks << '\n';
    }
    leaks << "  ]\n}\n";
    write_text_file(directory / "fiui-leaks.json", leaks.str());
}

void DiagnosticsHub::event(const char* category,
                           const char* action,
                           ObjectId object_id,
                           std::uint32_t generation,
                           std::uint64_t frame_id,
                           const char* path,
                           const char* detail)
{
    const DebugMode current_mode = mode();
    if (current_mode == DebugMode::Off) {
        return;
    }
    if (current_mode == DebugMode::Basic && !basic_trace_allowed(category, action)) {
        return;
    }

    std::ostringstream json;
    json << "{\"schema\":\"fiui.trace.v0\",";
    json << "\"session_id\":\"" << escape_json(session_id_.c_str()) << "\",";
    json << "\"frame_id\":" << frame_id << ',';
    json << "\"timestamp_ms\":" << timestamp_ms() << ',';
    json << "\"category\":\"" << escape_json(category).c_str() << "\",";
    json << "\"action\":\"" << escape_json(action).c_str() << "\",";
    json << "\"object_id\":" << object_id << ',';
    json << "\"generation\":" << generation << ',';
    json << "\"path\":\"" << escape_json(path).c_str() << "\",";
    json << "\"detail\":\"" << escape_json(detail).c_str() << "\"}";

    std::lock_guard<std::mutex> lock(mutex_);
    constexpr std::size_t max_events = 4096;
    if (trace_.size() >= max_events) {
        trace_.erase(trace_.begin());
    }
    trace_.push_back(json.str());
}

void DiagnosticsHub::frame_json(const std::string& json)
{
    std::lock_guard<std::mutex> lock(mutex_);
    frame_ = json;
}

void DiagnosticsHub::leak(ObjectId object_id,
                          std::uint32_t generation,
                          std::uint64_t frame_id,
                          const char* path,
                          const char* detail)
{
    std::ostringstream json;
    json << "    {\"session_id\":\"" << escape_json(session_id_.c_str()) << "\",";
    json << "\"frame_id\":" << frame_id << ',';
    json << "\"object_id\":" << object_id << ',';
    json << "\"generation\":" << generation << ',';
    json << "\"path\":\"" << escape_json(path).c_str()
         << "\",\"detail\":\"" << escape_json(detail).c_str() << "\"}";
    std::lock_guard<std::mutex> lock(mutex_);
    leaks_.push_back(json.str());
}

const std::string& DiagnosticsHub::session_id() const noexcept
{
    return session_id_;
}

void configure_diagnostics(const DiagnosticsConfig& config)
{
    default_runtime().diagnostics().configure(config);
    diagnostics_event("diagnostics", "configure", 0, "", "configuration updated");
}

DebugMode diagnostics_mode()
{
    return default_runtime().diagnostics().mode();
}

void flush_diagnostics()
{
    default_runtime().diagnostics().flush();
}

void diagnostics_event(const char* category,
                       const char* action,
                       ObjectId object_id,
                       const char* path,
                       const char* detail)
{
    diagnostics_event_ex(category, action, object_id, 0, default_runtime().current_frame_id(), path,
                         detail);
}

void diagnostics_event_ex(const char* category,
                          const char* action,
                          ObjectId object_id,
                          std::uint32_t generation,
                          std::uint64_t frame_id,
                          const char* path,
                          const char* detail)
{
    default_runtime().diagnostics().event(category, action, object_id, generation, frame_id, path,
                                          detail);
}

void diagnostics_frame_json(const std::string& json)
{
    default_runtime().diagnostics().frame_json(json);
}

void diagnostics_leak(ObjectId object_id, const char* path, const char* detail)
{
    diagnostics_leak_ex(object_id, 0, default_runtime().current_frame_id(), path, detail);
}

void diagnostics_leak_ex(ObjectId object_id,
                         std::uint32_t generation,
                         std::uint64_t frame_id,
                         const char* path,
                         const char* detail)
{
    default_runtime().diagnostics().leak(object_id, generation, frame_id, path, detail);
}

} // namespace fiui
