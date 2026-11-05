/* Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#pragma once

#include <string>
#include "test_base.h"
#include "bq_common/bq_common.h"
#include "bq_log/bq_log.h"
#include "bq_log/log/appender/appender_file_base.h"
#include "bq_log/log/log_manager.h"
#include "bq_log/types/storage/instance_storage.h"
#include "bq_log/types/storage/log_storage.h"

namespace bq {
    namespace test {

        // Copies one file byte for byte. Returns false when the source cannot be read, which for a
        // snapshot means "this file was not there at that instant" and is reported separately.
        // open_file leaves the cursor at end of file, so the read has to ask for begin explicitly -
        // reading from the current position returns zero bytes and looks like a missing file.
        inline bool copy_file_bytes(const bq::string& from, const bq::string& to)
        {
            auto src = bq::file_manager::instance().open_file(from, bq::file_open_mode_enum::read);
            if (!src) {
                return false;
            }
            const uint64_t file_size = bq::file_manager::instance().get_file_size(src);
            // The copy goes through a buffer, so it needs a size the buffer can hold.
            if (file_size > static_cast<uint64_t>(BQ_SIZE_MAX)) {
                return false;
            }
            const size_t size = static_cast<size_t>(file_size);
            bq::array<uint8_t> data;
            data.fill_uninitialized(size);
            const size_t read_size = bq::file_manager::instance().read_file(src, data.begin(), size,
                bq::file_manager::seek_option::begin, 0);
            bq::file_manager::instance().close_file(src);
            if (read_size != size) {
                return false;
            }
            bq::file_manager::remove_file_or_dir(to);
            auto dst = bq::file_manager::instance().open_file(to,
                bq::file_open_mode_enum::auto_create | bq::file_open_mode_enum::read_write);
            if (!dst) {
                return false;
            }
            const size_t written = bq::file_manager::instance().write_file(dst, data.begin(), size);
            bq::file_manager::instance().close_file(dst);
            return written == size;
        }

        // Directory holding one log's internal files (log buffer mmap, appender write cache mmap).
        inline bq::string log_internal_dir(const bq::string& log_name)
        {
            auto* inst = bq::instance_storage::get();
            if (!inst) {
                return bq::string();
            }
            return inst->logs_dir() + "/" + bq::log_storage::escape_name(log_name);
        }

        // Crash-recovery tests need the exact on-disk state that exists at one instant of a flush.
        // A real kill -9 cannot hit that instant twice, so the flush hook copies a fixed file list at
        // the hook point and the test restarts the logger on the copy instead. The list is explicit
        // because the appender's file set is known: write cache mmap, log buffer mmap, log output.
        // Only flushes of appender_name are captured; an empty name captures any appender.
        class scoped_flush_snapshot {
        public:
            scoped_flush_snapshot(const bq::array<bq::string>& source_files, const bq::string& appender_name,
                const bq::string& snapshot_dir, bq::appender_file_base::flush_hook_point point)
                : source_files_(source_files)
                , appender_name_(appender_name)
                , snapshot_dir_(snapshot_dir)
                , point_(point)
                , captured_(false)
                , fire_count_(0)
            {
                bq::appender_file_base::set_flush_hook(&scoped_flush_snapshot::on_flush, this);
            }

            ~scoped_flush_snapshot()
            {
                bq::appender_file_base::set_flush_hook(nullptr, nullptr);
            }

            bool captured() const { return captured_; }
            uint32_t fire_count() const { return fire_count_; }
            const bq::array<bq::string>& source_files() const { return source_files_; }

            // True when the file existed at the captured instant and was copied.
            bool has(const bq::string& source_file) const
            {
                for (size_t i = 0; i < source_files_.size(); ++i) {
                    if (source_files_[i] == source_file) {
                        return i < captured_files_.size() && captured_files_[i];
                    }
                }
                return false;
            }

            // Puts the captured state back. The caller must have destroyed the log object first.
            bool restore() const
            {
                bool ok = true;
                for (size_t i = 0; i < source_files_.size(); ++i) {
                    if (i < captured_files_.size() && captured_files_[i]) {
                        ok = copy_file_bytes(snapshot_path(i), source_files_[i]) && ok;
                    }
                }
                return ok;
            }

        private:
            bq::string snapshot_path(size_t index) const
            {
                return snapshot_dir_ + "/" + bq::file_manager::get_file_name_from_path(source_files_[index]);
            }

            static void on_flush(const bq::appender_file_base* sender, bq::appender_file_base::flush_hook_point point, void* user)
            {
                auto* self = static_cast<scoped_flush_snapshot*>(user);
                if (point != self->point_ || self->captured_) {
                    return;
                }
                if (!self->appender_name_.is_empty() && (!sender || sender->get_name() != self->appender_name_)) {
                    return;
                }
                ++self->fire_count_;
                bq::file_manager::create_directory(self->snapshot_dir_);
                self->captured_files_.clear();
                bool any = false;
                for (size_t i = 0; i < self->source_files_.size(); ++i) {
                    const bool copied = copy_file_bytes(self->source_files_[i], self->snapshot_path(i));
                    self->captured_files_.push_back(copied);
                    any = any || copied;
                }
                self->captured_ = any;
            }

            bq::array<bq::string> source_files_;
            bq::array<bool> captured_files_;
            bq::string appender_name_;
            bq::string snapshot_dir_;
            bq::appender_file_base::flush_hook_point point_;
            bool captured_;
            uint32_t fire_count_;
        };

        // Resets every process-global test hook on both construction and destruction, so a failure in
        // the middle of a case cannot leak the injected state into the next one.
        struct scoped_fault_and_hook_guard {
            scoped_fault_and_hook_guard()
            {
                bq::platform::test_inject::set_short_write_limit(static_cast<size_t>(-1));
                bq::appender_file_base::set_flush_hook(nullptr, nullptr);
            }
            ~scoped_fault_and_hook_guard()
            {
                bq::platform::test_inject::set_short_write_limit(static_cast<size_t>(-1));
                bq::appender_file_base::set_flush_hook(nullptr, nullptr);
            }
        };

        // A producer can reach the write cache while a flush is in flight; what it leaves past the
        // finished cursor is the tail that flush has to carry along with the bytes it could not
        // write. A single-threaded test never produces that overlap on its own, so the bytes are
        // appended from the flush hook instead - the one instant where the mix really exists.
        class scoped_cache_tail_writer {
        public:
            scoped_cache_tail_writer(const bq::string& appender_name, const void* tail, size_t size)
                : appender_name_(appender_name)
                , tail_(static_cast<const uint8_t*>(tail))
                , size_(size)
                , fired_(false)
            {
                bq::appender_file_base::set_flush_hook(&scoped_cache_tail_writer::on_flush, this);
            }

            ~scoped_cache_tail_writer()
            {
                bq::appender_file_base::set_flush_hook(nullptr, nullptr);
            }

            bool fired() const { return fired_; }

        private:
            static void on_flush(const bq::appender_file_base* sender, bq::appender_file_base::flush_hook_point point, void* user)
            {
                auto* self = static_cast<scoped_cache_tail_writer*>(user);
                if (point != bq::appender_file_base::flush_hook_point::after_write || self->fired_) {
                    return;
                }
                if (!sender || sender->get_name() != self->appender_name_) {
                    return;
                }
                const_cast<bq::appender_file_base*>(sender)->test_append_write_cache_tail(self->tail_, self->size_);
                self->fired_ = true;
            }

            bq::string appender_name_;
            const uint8_t* tail_;
            size_t size_;
            bool fired_;
        };

        class test_security_flush : public test_base {
        private:
            static const char* appender_pub_key()
            {
                return "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCwv3QtDXB/fQN+FonyOHuS2uC6IZc16bfd6qQk4ykBOt3nTfBFcNr8ZWvvcf4H0hFkrpMtQ0AJO057GhVTQCCfnvfStSq2Yra+O5VGpI5Q6NLrUuVERimjNgwtxbXt3P8Nw87jEIJiY/8m2FUXhZEPwoA7t+2/953cNE1itJskJtojwaUlMN0dXBJxs4NP8MfBPPZQ5vNV8xgEf1SCQzQBAJsofy1kPHHqJNBXUBsNA44SP5H95JOz+r0oaNkYxT88Zk4tbk5N3hk5aXyZVp49OqhrXCPf5owDa4Lqk4UzVTk9EimxvtSuiUTzr7IJhHYy7jsGnSgq6dH0xlUfxKeX pippocao@PIPPOCAO-PC6";
            }

            static const char* appender_name() { return "sec"; }

            // Stands in for the CSPRNG a console application would inject. It only has to be used,
            // not to be unpredictable - the test checks that the library takes it, not that it is
            // good.
            static bool injected_random_source(void* buf, size_t size)
            {
                uint8_t* out = static_cast<uint8_t*>(buf);
                for (size_t i = 0; i < size; ++i) {
                    out[i] = static_cast<uint8_t>(0xA5 ^ static_cast<uint8_t>(i * 31));
                }
                return true;
            }

            // Only the recovery case needs to read a file back, and that one writes encrypted data.
            static const bq::string& appender_priv_key()
            {
                static const bq::string k = bq::string("-----BEGIN RSA PRIVATE KEY-----\n")
                    + "MIIEpAIBAAKCAQEAsL90LQ1wf30DfhaJ8jh7ktrguiGXNem33eqkJOMpATrd503w\n"
                    + "RXDa/GVr73H+B9IRZK6TLUNACTtOexoVU0Agn5730rUqtmK2vjuVRqSOUOjS61Ll\n"
                    + "REYpozYMLcW17dz/DcPO4xCCYmP/JthVF4WRD8KAO7ftv/ed3DRNYrSbJCbaI8Gl\n"
                    + "JTDdHVwScbODT/DHwTz2UObzVfMYBH9UgkM0AQCbKH8tZDxx6iTQV1AbDQOOEj+R\n"
                    + "/eSTs/q9KGjZGMU/PGZOLW5OTd4ZOWl8mVaePTqoa1wj3+aMA2uC6pOFM1U5PRIp\n"
                    + "sb7UrolE86+yCYR2Mu47Bp0oKunR9MZVH8SnlwIDAQABAoIBACtaWpmuYTi0JkYo\n"
                    + "Kx/hoNXtoA+nq5pKwJHLOwXdPjKSCNnycQvnWZ9tFSN/V2r9qMyEUY9ZnnxlMqPZ\n"
                    + "Sv/Hi/j7Ghhx3Y8s+VwB62SPemT4JrwX8ipj91SULjqP80br3Re4PqfNZd3SX0Rc\n"
                    + "7co+Nc2izKdZPxTGHM9leNHMMP2VrVbZeSlQBnqlFVMqi2g9ukMGZG10vPdIJV7z\n"
                    + "5dqWaKuW/2F8dp/o6i/uUDWAH4fITLD1PLqx5/kP8ohOXup8wxYaY8jhKlvswuCh\n"
                    + "qlY773SamnrIGctQWe63Fe9q7hzs3vcCOfciFYsVX2qfHPvGORzd8DgMwe3TFbrM\n"
                    + "nmUijcECgYEA5zyarIPQfbEsmMtKgY7OSz+M1SaOE9r/I0Hl2dz7f0r7JocZ6KUj\n"
                    + "NinbX/zvJ7AMzDUefbptjf9F4vNa0eBZmSZDAiWm3byvMKS5uLboFNrKnYptIc3c\n"
                    + "0CzDzC+nMi4NrPoAZZrUyJw1Emr7gWVVG2FW2NatOVqfPq3XBbk9dncCgYEAw60H\n"
                    + "FqcvrSTAVSk9L+TB8Fn92p5YQtaV7CSZj9GQdvfs+pIkPlq+jYvSH7nYjK2Qlq+h\n"
                    + "sn+3YcVVczbGuhSLuq4bHPd46HOjya3rbAq39RxlpFRFjZ4hici4XIFKnB7Kylta\n"
                    + "Ph6nNq9m5tdFZ1SurgLlOaxg2AwLAia2V5L5/+ECgYAzmhmmP/Ap7HzYSB2DVfwB\n"
                    + "XNgvxN/V3HwtQQprGN5i5LexPFryyM9XyfVzsT0pbScd9wir5AuIsZvF7qqoxVkZ\n"
                    + "TSmM9BwNxYqO32O2rdKSvNSUXYzHC2qoZiT3jvbPwuk4Xb3y7p9neTx6tLcVhCh+\n"
                    + "6LT5xMZ5UxjQYvjmBRWLNwKBgQC5Ta9rzXHB5w7Y1w/hviHHKoHTOabdzPb3RQXD\n"
                    + "g7LqZwkdla4K+sZ/pwybDNU9C9TkTnizYG1agpTUYeg6KeDVLbHxcY4nm/Nct349\n"
                    + "t7zTu0uqHkArx7d9Ev88Yxgz1pk2nuJL951klSC+tNg97Zzqn0VSo6KmlmkKZXzC\n"
                    + "XCayIQKBgQC+hPlGE7T6WLKvsmaH3IJI/TAjbuSu25o+aar7ecPoeax6YnSgyF/8\n"
                    + "Xx7f0BFCYEzprftfqDBcfa6D8GKkcsqAMDeJRz8meD+55o3qdL6LKFkjCKvmRxuv\n"
                    + "OmO+42HqCD4mqxMU1rgcWOn+LLW3HSqbE5kYA9XDwEtxCnMiKqP7sA==\n"
                    + "-----END RSA PRIVATE KEY-----\n";
                return k;
            }

            static bq::string make_log_conf(const char* file_name, bool encrypted, const char* thread_mode = "sync")
            {
                bq::string conf = "log.recovery=true\n"
                                  "log.thread_mode=";
                conf += thread_mode;
                conf += "\n"
                        "appenders_config.sec.type=compressed_file\n"
                        "appenders_config.sec.levels=[all]\n"
                        "appenders_config.sec.max_file_size=0\n"
                        "appenders_config.sec.enable_rolling_log_file=false\n"
                        "appenders_config.sec.file_name=";
                conf += file_name;
                conf += "\n";
                if (encrypted) {
                    conf += "appenders_config.sec.pub_key=";
                    conf += appender_pub_key();
                    conf += "\n";
                }
                return conf;
            }

            static bq::string output_dir() { return TO_ABSOLUTE_PATH("security_flush_out", 0); }

            static bq::string snapshot_dir() { return TO_ABSOLUTE_PATH("security_flush_snapshot", 0); }

            // The appender's output file is named after the configured base name, and a crash replica
            // has to restore exactly the file the run under test was writing.
            static bq::array<bq::string> snapshot_sources(const bq::string& log_name, const bq::string& file_base)
            {
                bq::array<bq::string> files;
                files.push_back(log_internal_dir(log_name) + "/appenders/" + appender_name() + ".mmap");
                files.push_back(log_internal_dir(log_name) + "/lp.mmap");
                files.push_back(output_dir() + "/" + bq::file_manager::get_file_name_from_path(file_base) + "_1.logcompr");
                return files;
            }

            // logs_dir() only exists once something asked for the instance. Without this, a standalone
            // run resolves an empty path and silently cleans nothing, leaving a stale mmap behind that
            // the next run would mistake for recoverable data.
            static void ensure_instance_storage()
            {
                if (!bq::instance_storage::get()) {
                    bq::log::force_flush_all_logs();
                }
            }

            static void clean_all(const bq::string& log_name)
            {
                ensure_instance_storage();
                bq::file_manager::remove_file_or_dir(output_dir());
                bq::file_manager::remove_file_or_dir(snapshot_dir());
                const bq::string internal_dir = log_name.is_empty() ? bq::string() : log_internal_dir(log_name);
                if (!internal_dir.is_empty()) {
                    bq::file_manager::remove_file_or_dir(internal_dir);
                }
            }

            // One log name per hook point. Re-creating a log under the same name right after its
            // internal directory was removed leaves the new log without any of the files the snapshot
            // needs - the meta registry still knows the old one - so the two runs must not share a name.
            static const char* snapshot_log_name(int32_t point)
            {
                return point == 0 ? "security_flush_snapshot_transform" : "security_flush_snapshot_write";
            }

            // The output file needs its own base for the same reason, and on Windows for one more:
            // removing the directory can come back with EACCES while the previous appender's file is
            // still held, and a second run that finds that file treats the log as one to recover
            // instead of one to write. It then flushes through the recovery path, so the after-write
            // hook never fires and the case would fail for a reason that has nothing to do with it.
            static const char* snapshot_file_base(int32_t point)
            {
                return point == 0 ? "security_flush_out/snapshot_transform" : "security_flush_out/snapshot_write";
            }

            // The short-write hook is the only way to reach the appender's partial-write path without
            // filling a real disk, so it has to be trustworthy before anything relies on it.
            void case_short_write_injection(test_result& result)
            {
                scoped_fault_and_hook_guard guard;
                const bq::string path = output_dir() + "/short_write.bin";
                bq::file_manager::create_directory(output_dir());
                bq::file_manager::remove_file_or_dir(path);
                auto handle = bq::file_manager::instance().open_file(path,
                    bq::file_open_mode_enum::auto_create | bq::file_open_mode_enum::read_write);
                result.add_result(static_cast<bool>(handle), "short write: open file");

                uint8_t payload[4096];
                memset(payload, 0x5a, sizeof(payload));

                bq::platform::test_inject::set_short_write_limit(100);
                const size_t first = bq::file_manager::instance().write_file(handle, payload, sizeof(payload));
                result.add_result(first == 100, "short write: injection limits the write to 100 bytes (got %d)",
                    static_cast<int32_t>(first));

                bq::platform::test_inject::set_short_write_limit(static_cast<size_t>(-1));
                const size_t second = bq::file_manager::instance().write_file(handle, payload + 100, sizeof(payload) - 100);
                result.add_result(second == sizeof(payload) - 100, "short write: disarming restores full writes");
                bq::file_manager::instance().close_file(handle);
                bq::file_manager::remove_file_or_dir(path);
            }

            // Both hook points have to fire during one flush, and the snapshot has to contain the
            // files the recovery path reads. A cold log has none of those files yet - the appender
            // opens its write cache mmap and its output file lazily - so the log is warmed up with one
            // flushed write before the hook is armed. Arming it earlier would fire on a flush whose
            // source list is still absent, leaving nothing to snapshot.
            void case_flush_snapshot(test_result& result)
            {
                // The snapshot exists to replicate a crash: it copies the two mappings recovery reads.
                // A platform without a cross-process file lock keeps recovery off, so those mappings
                // are never created and there is nothing to capture.
                if (!recovery_available()) {
                    skip_recovery_tests("flush hook snapshot");
                    return;
                }
                scoped_fault_and_hook_guard guard;

                for (int32_t point = 0; point < 2; ++point) {
                    const bq::string log_name = snapshot_log_name(point);
                    const char* file_base = snapshot_file_base(point);
                    clean_all(log_name);
                    // The case needs a cold appender: one that finds a file of its own name first
                    // replays it as a recovery instead of writing to it, and the after-write hook
                    // never runs. Saying so out loud beats watching the hook assertions fail for a
                    // reason that belongs to the cleanup above.
                    const bq::string output_file = output_dir() + "/"
                        + bq::file_manager::get_file_name_from_path(file_base) + "_1.logcompr";
                    result.add_result(!bq::file_manager::is_file(output_file),
                        "flush hook: no output file left behind (point %d)", point);
                    {
                        bq::log log_obj = bq::log::create_log(log_name.c_str(),
                            make_log_conf(file_base, true).c_str());
                        result.add_result(log_obj.is_valid(), "flush hook: encrypted log created");

                        log_obj.info("warm up line");
                        bq::log::force_flush_all_logs();

                        const bq::array<bq::string> sources = snapshot_sources(log_name, file_base);
                        for (size_t i = 0; i < sources.size(); ++i) {
                            result.add_result(bq::file_manager::is_file(sources[i]),
                                "flush hook: %s exists after warm up (point %d)",
                                bq::file_manager::get_file_name_from_path(sources[i]).c_str(), point);
                        }

                        scoped_flush_snapshot snapshot(sources, appender_name(), snapshot_dir(),
                            static_cast<bq::appender_file_base::flush_hook_point>(point));
                        result.add_result(!snapshot.captured(), "flush hook: not captured before any flush");
                        for (int32_t i = 0; i < 256; ++i) {
                            log_obj.info("snapshot line {}, payload {}", i, 3.14159f);
                        }
                        bq::log::force_flush_all_logs();
                        result.add_result(snapshot.fire_count() > 0,
                            "flush hook: fired during flush (point %d, %d times)", point,
                            static_cast<int32_t>(snapshot.fire_count()));
                        result.add_result(snapshot.captured(),
                            "flush hook: snapshot captured (point %d, fires %d)", point,
                            static_cast<int32_t>(snapshot.fire_count()));
                        for (size_t i = 0; i < sources.size(); ++i) {
                            result.add_result(snapshot.has(sources[i]),
                                "flush hook: captured %s (point %d, fires %d)",
                                bq::file_manager::get_file_name_from_path(sources[i]).c_str(), point,
                                static_cast<int32_t>(snapshot.fire_count()));
                        }
                    }
                }
                for (int32_t point = 0; point < 2; ++point) {
                    clean_all(snapshot_log_name(point));
                }
            }

        public:
            // D-6: when write() takes fewer bytes than the cache holds, everything it did not take
            // stays pending, and the bytes a producer appended meanwhile line up behind them. The
            // fixup has to slide both down by the number of bytes that landed, in one move. Copying
            // the unwritten part to the front and then moving the tail there as well puts the tail
            // exactly where the records that were never written used to be, so those records are
            // lost - and with encryption on, the revert then runs over the wrong bytes as well.
            //
            // A sync log flushes on every write, so the only way to have a pending window is to make
            // that flush come up short: the record that was not fully written is what the next flush
            // has to salvage. The tail is the record the flush hook appends while the write is in
            // flight - the same bytes a producer thread would put there.
            void case_short_write_keeps_tail(test_result& result)
            {
                scoped_fault_and_hook_guard guard;
                const char* log_name = "security_flush_short_write_tail";
                const bq::string file_path = output_dir() + "/tail_1.logcompr";
                const int32_t before_count = 8;
                const size_t short_write_size = 16;
                clean_all(log_name);
                {
                    bq::log log_obj = bq::log::create_log(log_name, make_log_conf("security_flush_out/tail", false).c_str());
                    result.add_result(log_obj.is_valid(), "short write tail: log created");

                    log_obj.info("tail warm up");
                    bq::log_manager::instance().test_force_flush(log_obj.get_id());
                    for (int32_t i = 0; i < before_count; ++i) {
                        log_obj.info("tail before {} {}", i, 3.5f);
                    }
                    bq::log_manager::instance().test_force_flush(log_obj.get_id());
                    result.add_result(bq::file_manager::is_file(file_path), "short write tail: output file created");

                    static const uint8_t partial_record[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
                    const uint64_t size_before = bq::file_manager::get_file_size(file_path);
                    {
                        scoped_cache_tail_writer tail_writer(appender_name(), partial_record, sizeof(partial_record));
                        bq::platform::test_inject::set_short_write_limit(short_write_size);
                        // The record this write carries is left half written in the cache.
                        log_obj.info("tail pending record {} with a payload long enough to survive compression", 42);
                        bq::platform::test_inject::set_short_write_limit(static_cast<size_t>(-1));
                        result.add_result(tail_writer.fired(), "short write tail: bytes were appended during the write");
                    }
                    const uint64_t size_after = bq::file_manager::get_file_size(file_path);
                    // Without this the case would quietly pass on a full write, where the two layouts
                    // agree and nothing is at stake.
                    result.add_result(size_after - size_before == short_write_size,
                        "short write tail: the write took exactly %d bytes (%d before, %d after)",
                        static_cast<int32_t>(short_write_size), static_cast<int32_t>(size_before), static_cast<int32_t>(size_after));

                    // The rest of the cache now flushes in full. Whatever the fixup dropped is gone
                    // for good, so the file has to be complete for the decoder to find it.
                    bq::log_manager::instance().test_force_flush(log_obj.get_id());
                }

                bq::tools::log_decoder decoder(file_path, "");
                bq::string decoded;
                bq::appender_decode_result first_error = bq::appender_decode_result::success;
                int32_t entry_count = 0;
                for (;;) {
                    const bq::appender_decode_result r = decoder.decode();
                    if (r == bq::appender_decode_result::eof) {
                        break;
                    }
                    if (r != bq::appender_decode_result::success) {
                        first_error = r;
                        break;
                    }
                    decoded += decoder.get_last_decoded_log_entry();
                    decoded += "\n";
                    if (++entry_count > 100000) {
                        break;
                    }
                }

                // The injected tail is not a record, so the decoder is expected to stop on it - but
                // only after the record the short write left behind, which is what these two say.
                const int32_t expected_count = 2 + before_count;
                result.add_result(entry_count >= expected_count,
                    "short write tail: every record before the tail decodes (%d of %d)",
                    entry_count, expected_count);
                result.add_result(decoded.find("tail pending record 42 with a payload") != bq::string::npos,
                    "short write tail: the record the write could not take is still in the file (decode stopped with %d)",
                    static_cast<int32_t>(first_error));

                clean_all(log_name);
            }

            // Encryption happens in place inside a flush window, so a crash between the transform
            // and the accounting can leave the write cache holding ciphertext. Recovery cannot tell
            // that apart from plaintext by looking at the buffer, and appending it to a fresh segment
            // would transform it a second time. v11 records that a window is open and discards
            // whatever the window still holds, so what this asserts is the invariant that survives
            // either way: bytes that never reached the file, or that reached it only halfway, must
            // never be written a second time.
            void case_recovery_does_not_replay_transform_window(test_result& result)
            {
                scoped_fault_and_hook_guard guard;
                const char* log_name = "security_flush_recover_transform";
                const bq::string file_base = "security_flush_out/recover";
                const bq::string file_path = output_dir() + "/recover_1.logcompr";
                const int32_t record_count = 64;
                ensure_instance_storage();
                clean_all(log_name);

                bq::log log_obj = bq::log::create_log(log_name, make_log_conf(file_base.c_str(), true, "async").c_str());
                result.add_result(log_obj.is_valid(), "D-2: encrypted async recovery log created");
                if (!log_obj.is_valid()) {
                    clean_all(log_name);
                    return;
                }
                log_obj.info("mt warm up");
                bq::log::force_flush_all_logs();

                // The source list needs the log's internal directory, which only exists once the
                // instance has been created - so the snapshot is armed after the log, not before it.
                const bq::array<bq::string> sources = snapshot_sources(log_name, file_base);
                scoped_flush_snapshot snapshot(sources, appender_name(), snapshot_dir(),
                    bq::appender_file_base::flush_hook_point::after_transform);
                for (int32_t i = 0; i < record_count; ++i) {
                    log_obj.info("mt record {}", i);
                }
                bq::log::force_flush_all_logs();
                result.add_result(snapshot.captured(), "D-2: the transform window was captured before the write");
                if (!snapshot.captured()) {
                    bq::log_manager::instance().test_crash_log(log_obj.get_id());
                    clean_all(log_name);
                    return;
                }
                // Everything from here is a crash: the cache holds ciphertext, the file does not.
                bq::log_manager::instance().test_crash_log(log_obj.get_id());

                result.add_result(snapshot.restore(), "D-2: the pre-write instant was put back");
                if (auto* inst = bq::instance_storage::get()) {
                    inst->test_begin_new_run(); // the relaunch
                }
                bq::log recovered = bq::log::create_log(log_name, make_log_conf(file_base.c_str(), true, "async").c_str());
                result.add_result(recovered.is_valid(), "D-2: the log reopens on the crash state");
                if (recovered.is_valid()) {
                    recovered.force_flush();
                }

                bq::tools::log_decoder decoder(file_path, appender_priv_key());
                bq::appender_decode_result first_error = bq::appender_decode_result::success;
                bq::array<bq::string> entries;
                for (;;) {
                    const bq::appender_decode_result r = decoder.decode();
                    if (r == bq::appender_decode_result::eof) {
                        break;
                    }
                    if (r != bq::appender_decode_result::success) {
                        first_error = r;
                        break;
                    }
                    entries.push_back(decoder.get_last_decoded_log_entry());
                    if (entries.size() > 100000) {
                        break;
                    }
                }
                int32_t garbage = 0;
                for (size_t i = 0; i < entries.size(); ++i) {
                    garbage += entries[i].find("mt ") == bq::string::npos ? 1 : 0;
                }
                result.add_result(first_error == bq::appender_decode_result::success,
                    "D-2: recovery leaves nothing unreadable in the file (decode stopped with %d, %d entries)",
                    static_cast<int32_t>(first_error), static_cast<int32_t>(entries.size()));
                result.add_result(garbage == 0,
                    "D-2: every recovered record is one that was written (%d of %d are not)",
                    garbage, static_cast<int32_t>(entries.size()));
                result.add_result(entries.size() >= 1, "D-2: the record flushed before the crash survived");

                if (recovered.is_valid()) {
                    bq::log_manager::instance().test_crash_log(recovered.get_id());
                }
                clean_all(log_name);
            }

            test_result d2_repro()
            {
                test_result result;
                case_recovery_does_not_replay_transform_window(result);
                return result;
            }

            // The segment encryption head is written with a struct copy and read back with one,
            // so a round trip through a byte buffer only means something if the bytes really sit where
            // the format table says. The static_asserts in the header cover the offsets; this covers
            // the values, and the fact that the payload base follows the head exactly.
            void case_segment_encryption_head_layout(test_result& result)
            {
                using head_type = bq::appender_file_binary::appender_segment_encryption_head;
                head_type written {};
                written.rsa_key_fingerprint = UINT64_C(0x0102030405060708);
                written.chacha_rounds = bq::appender_file_binary::default_chacha_rounds;
                memset(written.reserved, 0, sizeof(written.reserved));
                for (size_t i = 0; i < sizeof(written.kem_ciphertext); ++i) {
                    written.kem_ciphertext[i] = static_cast<uint8_t>(i & 0xFFU);
                }

                uint8_t bytes[sizeof(head_type)];
                memcpy(bytes, &written, sizeof(bytes));
                head_type read_back;
                memcpy(&read_back, bytes, sizeof(bytes));
                result.add_result(read_back.rsa_key_fingerprint == written.rsa_key_fingerprint
                        && read_back.chacha_rounds == written.chacha_rounds
                        && memcmp(read_back.kem_ciphertext, written.kem_ciphertext, sizeof(written.kem_ciphertext)) == 0,
                    "segment head: round trip through bytes preserves every field");
                // Segment-relative offsets, from the bytes: fingerprint at +0x0C, rounds at +0x14,
                // ciphertext starting at +0x18. Little-endian is the format's stated convention.
                result.add_result(bytes[0] == 0x08 && bytes[7] == 0x01
                        && bytes[8] == bq::appender_file_binary::default_chacha_rounds
                        && bytes[9] == 0 && bytes[12] == 0,
                    "segment head: fields land at +0x0C / +0x14 / +0x18, little-endian");
                result.add_result(bq::appender_file_binary::encrypted_payload_base_offset == 280
                        && bq::appender_file_binary::plaintext_payload_base_offset == 12,
                    "segment head: payload starts at segment +0x118 when encrypted, +0x0C when not");
            }

            // The state word sits in the recovery mapping, which is shared, so reading the file sees
            // what the appender just stored without unmapping anything. Returns UINT32_MAX when the
            // mapping is not a file at all, which is what turns a missing recovery mapping into a
            // visible failure rather than a silent pass.
            static uint32_t read_flush_state_word(const bq::string& log_name)
            {
                const bq::string path = log_internal_dir(log_name) + "/appenders/" + appender_name() + ".mmap";
                auto file = bq::file_manager::instance().open_file(path, bq::file_open_mode_enum::read);
                if (!file) {
                    return UINT32_MAX;
                }
                uint32_t value = UINT32_MAX;
                const size_t read_size = bq::file_manager::instance().read_file(file, &value, sizeof(value),
                    bq::file_manager::seek_option::begin,
                    static_cast<int64_t>(bq::appender_file_base::test_flush_state_offset()));
                bq::file_manager::instance().close_file(file);
                return read_size == sizeof(value) ? value : UINT32_MAX;
            }

            // Reads the word at every point of one flush window. The window is the only interval in
            // which the cache is not plaintext, so observing it from outside means observing it from
            // inside a hook.
            struct flush_state_probe {
                bq::string appender_name;
                uint32_t at_transform = UINT32_MAX;
                uint32_t at_write = UINT32_MAX;
                int32_t hits = 0;
            };

            static void probe_flush_state(const bq::appender_file_base* sender,
                bq::appender_file_base::flush_hook_point point, void* user)
            {
                auto* self = static_cast<flush_state_probe*>(user);
                if (!sender || sender->get_name() != self->appender_name) {
                    return;
                }
                // Asked through the sender, not through a second file handle on the mapping: read() of a
                // live shared mapping only sees the page where the platform keeps the two coherent, and
                // that is not every platform. This is the same word recovery reads out of its own mapping.
                const uint32_t value = sender->test_read_flush_state();
                ++self->hits;
                if (point == bq::appender_file_base::flush_hook_point::after_transform) {
                    self->at_transform = value;
                } else {
                    self->at_write = value;
                }
            }

            // The state word is what recovery reads instead of guessing, so its value at each
            // instant is part of the contract, not an implementation detail. Inside the window it is
            // transforming; outside it is idle, and a plaintext appender never moves it at all.
            void case_flush_state_word(test_result& result)
            {
                // The state word lives in the recovery mapping, which only exists where recovery does.
                if (!recovery_available()) {
                    skip_recovery_tests("flush state word");
                    return;
                }
                scoped_fault_and_hook_guard guard;
                const char* plain_log = "security_flush_state_plain";
                const char* enc_log = "security_flush_state_enc";
                ensure_instance_storage();
                clean_all(plain_log);
                clean_all(enc_log);

                bq::log plain = bq::log::create_log(plain_log,
                    make_log_conf("security_flush_out/state_plain", false).c_str());
                result.add_result(plain.is_valid(), "state word: plaintext log created");
                if (plain.is_valid()) {
                    result.add_result(read_flush_state_word(plain_log) == 0,
                        "state word: a plaintext appender's fresh mapping is idle");
                    plain.info("plain record");
                    bq::log_manager::instance().test_force_flush(plain.get_id());
                    result.add_result(read_flush_state_word(plain_log) == 0,
                        "state word: a plaintext appender leaves it idle after a flush");
                }

                bq::log enc = bq::log::create_log(enc_log,
                    make_log_conf("security_flush_out/state_enc", true).c_str());
                result.add_result(enc.is_valid(), "state word: encrypted log created");
                if (enc.is_valid()) {
                    result.add_result(read_flush_state_word(enc_log) == 0,
                        "state word: an encrypted appender's fresh mapping is idle");

                    flush_state_probe probe;
                    probe.appender_name = appender_name();
                    bq::appender_file_base::set_flush_hook(&probe_flush_state, &probe);
                    for (int32_t i = 0; i < 32; ++i) {
                        enc.info("state word record {}", i);
                    }
                    bq::log_manager::instance().test_force_flush(enc.get_id());
                    bq::appender_file_base::set_flush_hook(nullptr, nullptr);

                    result.add_result(probe.hits >= 2, "state word: both flush points were observed (%d)", probe.hits);
                    result.add_result(probe.at_transform == 1,
                        "state word: transforming while the buffer is ciphertext (read %d)",
                        static_cast<int32_t>(probe.at_transform));
                    result.add_result(probe.at_write == 1,
                        "state word: still transforming after write() and before the accounting (read %d)",
                        static_cast<int32_t>(probe.at_write));
                    result.add_result(read_flush_state_word(enc_log) == 0,
                        "state word: back to idle once the window has closed");
                }

                clean_all(plain_log);
                clean_all(enc_log);
            }

            // D-3, revised: without a CSPRNG the appender used to refuse to be created. It now says
            // so once and keeps writing - a log that stops is worse than one whose segment keys are
            // weaker than they should be - so what has to hold is that the records really are
            // encrypted, not that the appender is gone.
            void case_encryption_without_secure_random_still_encrypts(test_result& result)
            {
                struct scoped_secure_random_guard {
                    scoped_secure_random_guard()
                        : previous_(bq::log::has_secure_random_source())
                    {
                        bq::log::set_secure_random_source(nullptr);
                        bq::platform::test_inject::set_disable_secure_random(false);
                    }
                    ~scoped_secure_random_guard()
                    {
                        bq::log::set_secure_random_source(nullptr);
                        bq::platform::test_inject::set_disable_secure_random(false);
                    }
                    bool previous_;
                } random_guard;
                (void)random_guard;

                clean_all("security_flush_no_csprng");
                clean_all("security_flush_no_csprng_plain");
                clean_all("security_flush_injected_csprng");

                bq::platform::test_inject::set_disable_secure_random(true);
                result.add_result(!bq::log::has_secure_random_source(),
                    "no CSPRNG: the library reports that no secure source exists");
                const char* marker = "weak-key-marker-C0FFEE";
                bq::log degraded = bq::log::create_log("security_flush_no_csprng",
                    make_log_conf("security_flush_out/nocsprng", true).c_str());
                result.add_result(degraded.is_valid(), "no CSPRNG: the encrypted log is still created");
                if (degraded.is_valid()) {
                    degraded.info(marker);
                    bq::log_manager::instance().test_force_flush(degraded.get_id());
                }
                const bq::string degraded_file = output_dir() + "/nocsprng_1.logcompr";
                result.add_result(bq::file_manager::is_file(degraded_file),
                    "no CSPRNG: the encrypted appender wrote a file");

                // A file that still shows the marker means the transform was skipped rather than
                // applied with a weaker key, which is the failure this case exists to catch.
                bool marker_visible = false;
                auto degraded_handle = bq::file_manager::instance().open_file(degraded_file, bq::file_open_mode_enum::read);
                if (degraded_handle) {
                    const uint64_t degraded_size = bq::file_manager::instance().get_file_size(degraded_handle);
                    if (degraded_size > 0 && degraded_size <= static_cast<uint64_t>(BQ_SIZE_MAX)) {
                        bq::array<uint8_t> bytes;
                        bytes.fill_uninitialized(static_cast<size_t>(degraded_size));
                        const size_t read_size = bq::file_manager::instance().read_file(degraded_handle,
                            bytes.begin(), bytes.size(), bq::file_manager::seek_option::begin, 0);
                        const size_t marker_len = strlen(marker);
                        if (read_size == bytes.size() && marker_len <= bytes.size()) {
                            for (size_t i = 0; i + marker_len <= bytes.size(); ++i) {
                                if (memcmp(bytes.begin() + static_cast<ptrdiff_t>(i), marker, marker_len) == 0) {
                                    marker_visible = true;
                                    break;
                                }
                            }
                        }
                    }
                    bq::file_manager::instance().close_file(degraded_handle);
                }
                result.add_result(!marker_visible, "no CSPRNG: the records on disk are still encrypted");
                bq::log plain = bq::log::create_log("security_flush_no_csprng_plain",
                    make_log_conf("security_flush_out/nocsprng_plain", false).c_str());
                result.add_result(plain.is_valid(), "no CSPRNG: a plaintext log is unaffected");
                if (plain.is_valid()) {
                    plain.info("plain still works");
                    bq::log_manager::instance().test_force_flush(plain.get_id());
                }

                bq::log::set_secure_random_source(&injected_random_source);
                result.add_result(bq::log::has_secure_random_source(),
                    "injected source: the library reports one exists");
                bq::log injected = bq::log::create_log("security_flush_injected_csprng",
                    make_log_conf("security_flush_out/injected", true).c_str());
                result.add_result(injected.is_valid(), "injected source: the encrypted log is created");
                if (injected.is_valid()) {
                    injected.info("encrypted still works");
                    bq::log_manager::instance().test_force_flush(injected.get_id());
                }
                bq::log::set_secure_random_source(nullptr);
                bq::platform::test_inject::set_disable_secure_random(false);
                result.add_result(bq::log::has_secure_random_source() == random_guard.previous_,
                    "clearing the injected source returns to the previous state");

                clean_all("security_flush_no_csprng");
                clean_all("security_flush_no_csprng_plain");
                clean_all("security_flush_injected_csprng");
            }

            virtual test_result test() override
            {
                test_result result;
                case_segment_encryption_head_layout(result);
                case_short_write_injection(result);
                case_flush_snapshot(result);
                case_short_write_keeps_tail(result);
                case_flush_state_word(result);
                case_recovery_does_not_replay_transform_window(result);
                case_encryption_without_secure_random_still_encrypts(result);
                return result;
            }
        };
    }
}
