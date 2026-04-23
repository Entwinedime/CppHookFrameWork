#pragma once

#include "framework/common.hpp"
#include "framework/hook_arg_info.hpp"
#include "framework/pmu_recorder.hpp"
#include "framework/trace_logger.hpp"
#include <initializer_list>
#include <sstream>
#include <stdint.h>
#include <string>
#include <sys/syscall.h>
#include <time.h>
#include <type_traits>
#include <unistd.h>
#include <vector>

namespace HookFrameWork {

class ScopedTimer {
public:
    template <typename... Args>
    explicit ScopedTimer(const std::string & name, std::initializer_list<HookArgInfo> trace_args, Args &&... args) : name_(name),
                                                                                                                     start_us_(GetRealtimeUs()) {

        pmu_snapshot_ok_ = PmuRecorder::Get().ReadSnapshot(&start_pmu_);
        function_args_num_ = sizeof...(Args);

        std::ostringstream oss;
        oss << "\"Function-Args\": {";

        if (function_args_num_ > 0 && trace_args.size() > 0) { ProcessArgs(oss, trace_args, std::forward<Args>(args)...); }

        function_args_str_ = oss.str();
        if (function_args_str_[function_args_str_.length() - 1] == '{') { function_args_str_ += "}"; }
        else { function_args_str_ = function_args_str_.substr(0, function_args_str_.length() - 2) + "}"; }
    }

    ~ScopedTimer() {
        const pid_t tid{ static_cast<pid_t>(syscall(SYS_gettid)) };
        const uint64_t dur_us{ GetRealtimeUs() - start_us_ };
        const std::string args_str{ "\"args\": {" + function_args_str_ + ", " + ProcessPMUSnapshot() + "}" };

        TraceLogger::Get().LogEvent(name_, start_us_, dur_us, tid, args_str);
    }

private:
    static uint64_t GetRealtimeUs() {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1'000'000ULL + static_cast<uint64_t>(ts.tv_nsec) / 1'000ULL;
    }

    std::string ProcessPMUSnapshot() {
        PmuSnapshot end_pmu;

        std::ostringstream oss;
        oss << "\"Perf-Events\": {";

        const bool can_emit_pmu{ pmu_snapshot_ok_ && PmuRecorder::Get().ReadSnapshot(&end_pmu) && end_pmu.IsCompatibleWith(start_pmu_) };
        if (can_emit_pmu) {
            std::vector<long long> pmu_deltas = end_pmu.DeltaFrom(start_pmu_);
            if (pmu_deltas.size() != end_pmu.event_names.size()) { return "PMU data error"; }
            for (size_t i = 0; i < end_pmu.event_names.size(); ++i) {
                oss << "\"" << EscapeJsonString(end_pmu.event_names[i]) << "\": " << pmu_deltas[i] << ", ";
            }
        }

        std::string result = oss.str();
        if (result[result.length() - 1] == '{') { result += "}"; }
        else { result = result.substr(0, result.length() - 2) + "}"; }
        return result;
    }

    template <typename T> std::string ToString(T && val) {
        if constexpr (std::is_convertible_v<T, std::string>) { return std::string(std::forward<T>(val)); }
        else if constexpr (std::is_arithmetic_v<std::remove_reference_t<T>>) { return std::to_string(val); }
        else { return "unknown_type"; }
    }

    template <typename T, typename... Rest>
    void ProcessArgs(std::ostringstream & oss, std::initializer_list<HookArgInfo> trace_args, T && value, Rest &&... rest) {
        size_t current_index = function_args_num_ - 1 - sizeof...(Rest);

        for (auto & arg_info : trace_args) {
            if (arg_info.arg_position == current_index) {
                oss << "\"" << arg_info.arg_name << "\": \"" << EscapeJsonString(ToString(std::forward<T>(value))) << "\", ";
                break;
            }
        }

        if constexpr (sizeof...(Rest) > 0) { ProcessArgs(oss, trace_args, std::forward<Rest>(rest)...); }
    }

    void ProcessArgs(std::ostringstream & oss, std::initializer_list<HookArgInfo> trace_args) {}

    const std::string name_;
    const uint64_t start_us_;
    PmuSnapshot start_pmu_;
    bool pmu_snapshot_ok_{ false };
    size_t function_args_num_{ 0 };
    std::string function_args_str_;
};

} // namespace HookFrameWork
