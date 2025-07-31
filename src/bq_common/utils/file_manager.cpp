/*
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#include <sys/stat.h>
#include <inttypes.h>
#include "bq_common/bq_common.h"

// todo use read-write lock to improve the concurrency performance

#ifndef BQ_UNIT_TEST
#define FILE_MANAGER_LOG(...) bq::util::log_device_console(__VA_ARGS__)
#else
#define FILE_MANAGER_LOG(...)
#endif
namespace bq {
    static BQ_TLS int32_t file_manager_file_error_no_ = 0;

    file_manager::file_manager()
        : mutex(true)
    {
        seq_generator = 0;
        FILE_MANAGER_LOG(log_level::info, "file_manager is constructed");
        FILE_MANAGER_LOG(log_level::info, "internal storage path:%s", get_base_dir(true).c_str());
        FILE_MANAGER_LOG(log_level::info, "external storage path:%s", get_base_dir(false).c_str());
    }

    file_manager::~file_manager()
    {
        flush_all_opened_files();
        FILE_MANAGER_LOG(log_level::info, "file_manager is destructed");
    }

    file_manager& file_manager::instance()
    {
        return *common_global_vars::get().file_manager_inst_;
    }

    const bq::string& file_manager::get_base_dir(bool in_sand_box)
    {
        return bq::platform::get_base_dir(in_sand_box);
    }

    string file_manager::trans_process_relative_path_to_absolute_path(const bq::string& relative_path, bool in_sand_box)
    {
        string absolute_path = relative_path;
        if (!is_absolute(absolute_path)) {
            absolute_path = combine_path(get_base_dir(in_sand_box), relative_path);
        }
        absolute_path = get_lexically_path(absolute_path);
        return absolute_path;
    }

    string file_manager::trans_process_absolute_path_to_relative_path(const bq::string& absolute_path, bool in_sand_box)
    {
        if (!is_absolute(absolute_path)) {
            return absolute_path;
        }
        string relative_path = file_manager::get_base_dir(in_sand_box);
        relative_path = file_manager::get_lexically_path(relative_path);
        relative_path = get_lexically_path(absolute_path).replace(relative_path, "");
        return relative_path;
    }

    bool file_manager::is_handle_valid(const file_handle& handle) const
    {
        if (!handle) {
            return false;
        }
        int32_t desc_idx = get_file_descriptor_index_by_handle(handle);
        return desc_idx >= 0;
    }

    //---------------------------------------------tool functions begin-----------------------------------------------------//

    bool file_manager::create_directory(const bq::string& path)
    {
        if (platform::is_dir(path.c_str())) {
            return true;
        }
        int32_t error_code = platform::make_dir(path.c_str());
        if (error_code != 0) {
            FILE_MANAGER_LOG(bq::log_level::error, "create_directory failed, directory:%s, error code:%d", path.c_str(), error_code);
            return false;
        }
        return true;
    }

    bq::string file_manager::get_directory_from_path(const bq::string& path)
    {
        bq::string::size_type last_pos1 = path.find_last("/");
        bq::string::size_type last_pos2 = path.find_last("\\");
        auto last_split_pos = bq::min_ref(last_pos1, last_pos2);
        if (last_split_pos == bq::string::npos) {
            return "./";
        }
        bq::string result = path.substr(0, last_split_pos);
        return result.replace("\\", "/");
    }

    bq::string file_manager::get_file_name_from_path(const bq::string& path)
    {
        bq::string::size_type last_pos1 = path.find_last("/");
        bq::string::size_type last_pos2 = path.find_last("\\");
        const auto last_split_pos = bq::min_ref(last_pos1, last_pos2);
        if (last_split_pos == bq::string::npos) {
            return path;
        }
        if (last_split_pos == path.size() - 1) {
            return "";
        }
        return path.substr(last_split_pos + 1);
    }

    bq::string file_manager::combine_path(const bq::string& path1, const bq::string& path2)
    {
        bq::string result;
        result.set_capacity(path1.size() + path2.size() + 1);
        bq::array<bq::string> split1 = path1.replace("\\", "/").split("/");
        bq::array<bq::string> split2 = path2.replace("\\", "/").split("/");
        decltype(split1)::size_type total_parts = split1.size() + split2.size();
        decltype(total_parts) current_parts = 0;
        for (auto part : split1) {
            if (current_parts != 0) {
                result += "/";
            }
            result += part;
            ++current_parts;
        }
        for (auto part : split2) {
            if (current_parts != 0) {
                result += "/";
            }
            result += part;
            ++current_parts;
        }
        if (path1.size() > 0 && path1[0] == '/')
            result = "/" + result;
        return result;
    }

    bq::string file_manager::get_lexically_path(const bq::string& original_path)
    {
        return bq::platform::get_lexically_path(original_path);
    }

    bool file_manager::is_absolute(const string& path)
    {
        return bq::platform::is_absolute(path);
    }

    bool file_manager::is_dir(const bq::string& path)
    {
        bq::string real_path = get_lexically_path(path);
        return bq::platform::is_dir(real_path.c_str());
    }

    bool file_manager::is_file(const bq::string& path)
    {
        bq::string real_path = get_lexically_path(path);
        return bq::platform::is_regular_file(real_path.c_str());
    }

    bool file_manager::remove_file_or_dir(const bq::string& path)
    {
        bq::string real_path = get_lexically_path(path);

        int32_t error_code = bq::platform::remove_dir_or_file(real_path.c_str());
        if (error_code != 0) {
            switch (error_code) {
            case EACCES:
                FILE_MANAGER_LOG(bq::log_level::error, "remove_file_or_dir failed, directory:%s, error code:%s", real_path.c_str(), "EACCES");
                break;
            default:
                FILE_MANAGER_LOG(bq::log_level::error, "remove_file_or_dir failed, directory:%s, error code:%d", real_path.c_str(), error_code);
                break;
            }
            return false;
        }
        return true;
    }

    bq::array<bq::string> file_manager::get_sub_dirs_and_files_name(const bq::string& path)
    {
        bq::string real_path = get_lexically_path(path);
        return platform::get_all_sub_names(real_path.c_str());
    }

    void file_manager::get_all_files(const bq::string& path, bq::array<string>& out_list, bool recursion)
    {
        auto temp_files = bq::file_manager::get_sub_dirs_and_files_name(path);
        for (auto& info : temp_files) {
            string curr_path = combine_path(path, info);
            if (bq::file_manager::is_file(curr_path)) {
                out_list.push_back(curr_path);
            } else {
                out_list.push_back(curr_path + "/");
                if (recursion)
                    get_all_files(curr_path, out_list);
            }
        }
    }

    uint64_t file_manager::get_file_last_modified_epoch_ms(const bq::string& path)
    {
        return bq::platform::get_file_last_modified_epoch_ms(path.c_str());
    }

    size_t file_manager::get_file_size(const bq::string& path)
    {
        auto file_handle = file_manager::instance().open_file(path, file_open_mode_enum::read);
        if (!file_handle) {
            return 0;
        }
        return file_manager::instance().get_file_size(file_handle);
    }

    string file_manager::read_all_text(const bq::string& path)
    {
        auto file_handle = bq::file_manager::instance().open_file(path, file_open_mode_enum::read);
        if (!file_handle.is_valid()) {
            return "";
        }
        string text = bq::file_manager::instance().read_text(file_handle);
        return text;
    }

    void file_manager::append_all_text(const bq::string& path, string content)
    {
        auto& file_manager_inst = bq::file_manager::instance();
        auto file_handle = file_manager_inst.open_file(path, file_open_mode_enum::auto_create | file_open_mode_enum::read_write);
        if (!file_handle.is_valid()) {
            FILE_MANAGER_LOG(bq::log_level::error, "failed to load file %s", path.c_str());
            return;
        }
        file_manager_inst.write_file(file_handle, content.begin(), content.size());
        file_manager_inst.flush_file(file_handle);
    }

    void file_manager::write_all_text(const bq::string& path, string content)
    {
        auto& file_manager_inst = bq::file_manager::instance();
        auto file_handle = file_manager_inst.open_file(path, file_open_mode_enum::auto_create | file_open_mode_enum::read_write);
        if (!file_handle.is_valid()) {
            FILE_MANAGER_LOG(bq::log_level::error, "failed to load file %s", path.c_str());
            return;
        }
        file_manager_inst.truncate_file(file_handle, 0);
        file_manager_inst.write_file(file_handle, content.begin(), content.size());
        file_manager_inst.flush_file(file_handle);
    }

    int32_t file_manager::get_and_clear_last_file_error()
    {
        int32_t value = file_manager_file_error_no_;
        file_manager_file_error_no_ = 0;
        return value;
    }

    //----------------------------------------------tool functions end------------------------------------------------------//

    bq::file_handle file_manager::open_file(const bq::string& path, file_open_mode_enum open_mode)
    {
        bq::string real_path = get_lexically_path(path);
        file_handle handle;
        int32_t first_empty_idx = -1;
        bq::platform::scoped_mutex lock(mutex);
        for (decltype(file_descriptors)::size_type i = 0; i < file_descriptors.size(); ++i) {
            auto& desc = file_descriptors[i];
            if (first_empty_idx == -1 && desc.is_empty()) {
                first_empty_idx = (int32_t)i;
            }
        }

        // open or create file
        bq::string file_name = get_file_name_from_path(path);
        if ((int32_t)(open_mode & file_open_mode_enum::auto_create) && file_name.size() < path.size()) {
            bq::string dir = get_directory_from_path(real_path);
            if (!create_directory(dir)) {
                return handle;
            }
        }
        bq::platform::platform_file_handle platform_handle;
        int32_t open_file_errno = bq::platform::open_file(real_path.c_str(), (bq::platform::file_open_mode_enum)open_mode, platform_handle);
        if ((!bq::platform::is_platform_handle_valid(platform_handle)) || open_file_errno != 0) {
            file_manager_file_error_no_ = open_file_errno;
            FILE_MANAGER_LOG(bq::log_level::error, "open_or_create_file failed, file:\"%s\", error code:%d", real_path.c_str(), open_file_errno);
            return handle;
        }
        bq::platform::seek_file(platform_handle, platform::file_seek_option::end, 0);

        platform::platform_file_handle* handle_ptr = (platform::platform_file_handle*)malloc(sizeof(platform::platform_file_handle));
        *handle_ptr = platform_handle;
        handle.handle_ptr_ = handle_ptr;
        handle.file_path_ = real_path;
        if (first_empty_idx >= 0) {
            handle.idx_ = static_cast<uint32_t>(first_empty_idx);
        } else {
            file_descriptors.emplace_back(file_descriptor());
            handle.idx_ = static_cast<uint32_t>(file_descriptors.size() - 1);
        }
        auto& desc = file_descriptors[handle.idx_];
        assert(desc.is_empty());
        desc.seq = ++seq_generator;
        desc.handle_ptr = handle_ptr;
        desc.file_path = real_path;
        desc.inc_ref();

        handle.seq_ = file_descriptors[handle.idx_].seq;
#ifndef NDEBUG
        FILE_MANAGER_LOG(bq::log_level::info, "open  file:%s success, idx:%" PRIu32 ", seq:%" PRIu32 ", counter:%" PRId32 "", real_path.c_str(), handle.idx_, handle.seq_, desc.ref_cout.load());
#endif
        return handle;
    }

    size_t file_manager::write_file(const file_handle& handle, const void* data, size_t length, seek_option opt /* = seek_option::current*/, int64_t seek_offset /* = 0*/)
    {
        if (!data || length == 0) {
            return 0;
        }
        if (!seek(handle, opt, seek_offset)) {
            return 0;
        }
        size_t total_written_count = 0;
        int32_t error_code = bq::platform::write_file(handle.platform_handle(), data, length, total_written_count);
        if (error_code != 0) {
            file_manager_file_error_no_ = error_code;
            FILE_MANAGER_LOG(bq::log_level::error, "write file failed, errno:%" PRId32 ", path:%s, need_write_count:%" PRIu64 ", real_write_count:%" PRIu64,
                error_code, handle.file_path_.c_str(), static_cast<uint64_t>(length), static_cast<uint64_t>(total_written_count));
            return total_written_count;
        }
        return total_written_count;
    }

    bool file_manager::truncate_file(const file_handle& handle, size_t offset)
    {
        auto truncate_errno = platform::truncate_file(handle.platform_handle(), offset);
        bool result = (truncate_errno == 0);
        if (truncate_errno != 0) {
            FILE_MANAGER_LOG(bq::log_level::error, "truncate file failed, file:\"%s\", error code:%d", handle.file_path_.c_str(), truncate_errno);
        }
        size_t current_file_size = 0;
        int32_t get_file_size_error_no = bq::platform::get_file_size(handle.platform_handle(), current_file_size);
        if (get_file_size_error_no != 0) {
            current_file_size = offset;
        }
        platform::flush_file(handle.platform_handle());
        seek(handle, seek_option::end, 0);
        return result;
    }

    size_t file_manager::read_file(const file_handle& handle, void* dest_data, size_t length, seek_option opt /* = seek_option::current*/, int64_t seek_offset /* = 0*/)
    {
        if (!seek(handle, opt, seek_offset)) {
            return 0;
        }
        int32_t max_try_count = 10;
        size_t total_read_count = 0;
        while (max_try_count-- > 0 && total_read_count < length) {
            size_t need_read_count = length - total_read_count;
            size_t tmp_count;
            int32_t error_code = bq::platform::read_file(handle.platform_handle(), (uint8_t*)dest_data + total_read_count, need_read_count, tmp_count);
            if (error_code != 0) {
                file_manager_file_error_no_ = error_code;
                FILE_MANAGER_LOG(bq::log_level::error, "read file failed, errno:%d, path:%s, need_read_count:%" PRIu64 ", real_read_count:%" PRIu64,
                    error_code, handle.file_path_.c_str(), static_cast<uint64_t>(length), static_cast<uint64_t>(total_read_count);
                break;
            }
            assert(tmp_count <= need_read_count && "read_count <= length");
            total_read_count += tmp_count;
        }
        return total_read_count;
    }

    bq::string file_manager::get_file_absolute_path(const file_handle& handle)
    {
        bq::platform::scoped_mutex lock(mutex);
        int32_t desc_idx = get_file_descriptor_index_by_handle(handle);
        if (desc_idx < 0) {
            return bq::string();
        }
        return file_descriptors[desc_idx].file_path;
    }

    bool file_manager::seek(const file_handle& handle, seek_option opt, int64_t offset)
    {
        if (seek_option::current == opt && 0 == offset) {
            // performance optimize
            return true;
        }
        auto result = bq::platform::seek_file(handle.platform_handle(), (bq::platform::file_seek_option)opt, offset);
        if (0 != result) {
            FILE_MANAGER_LOG(bq::log_level::error, "seek(seek_option::current) file failed, errno:%d, file:%s, offset:%" PRId64 "",
                result, handle.file_path_.c_str(), offset);
            return false;
        }
        return true;
    }

    bool file_manager::lock_file(const file_handle& handle)
    {
        return bq::platform::lock_file(handle.platform_handle());
    }

    bq::string file_manager::read_text(const file_handle& handle)
    {
        size_t size = get_file_size(handle);
        if (size == 0) {
            return "";
        }
        seek(handle, seek_option::begin, 0);
        bq::string content(size);
        content.fill_uninitialized(size);
        auto real_read_size = read_file(handle, (bq::string::value_type*)content.begin(), size);
        if (real_read_size < size) {
            content.erase(content.begin() + real_read_size, size - real_read_size);
        }
        return content;
    }

    bool file_manager::flush_file(const file_handle& file)
    {
        int32_t err_code = platform::flush_file(file.platform_handle());
        if (err_code == 0) {
            return true;
        }
        file_manager_file_error_no_ = err_code;
        FILE_MANAGER_LOG(bq::log_level::error, "flush file failed, errno:%d,path:%s",
            err_code, file.file_path_.c_str());
        return false;
    }

    bool file_manager::flush_all_opened_files()
    {
        bq::platform::scoped_mutex lock(mutex);
        bool result = true;
        for (decltype(file_descriptors)::size_type i = 0; i < file_descriptors.size(); ++i) {
            auto& desc = file_descriptors[i];
            if (!desc.is_empty()) {
                int32_t err_code = platform::flush_file(*desc.handle_ptr);
                if (err_code == 0) {
                    continue;
                }
                file_manager_file_error_no_ = err_code;
                FILE_MANAGER_LOG(bq::log_level::error, "flush file failed, errno:%d, path:%s",
                    err_code, desc.file_path.c_str());
                result = false;
            }
        }
        return result;
        ;
    }

    bool file_manager::close_file(file_handle& handle)
    {
        if (!handle) {
            return false;
        }
        bq::platform::scoped_mutex lock(mutex);
        int32_t desc_idx = get_file_descriptor_index_by_handle(handle);
        if (desc_idx < 0) {
            return false;
        }
        auto& desc = file_descriptors[desc_idx];
#ifndef NDEBUG
        FILE_MANAGER_LOG(bq::log_level::info, "close file:%s idx:%d, seq:%d, ref_count:%d", desc.file_path.c_str(), desc_idx, handle.seq_, desc.ref_cout.load() - 1);
#endif
        desc.dec_ref();
        handle.clear();
        return true;
    }

    size_t file_manager::get_file_size(const file_handle& handle) const
    {
        bq::platform::flush_file(handle.platform_handle());
        size_t current_file_size;
        int32_t err_code = bq::platform::get_file_size(handle.platform_handle(), current_file_size);
        if (err_code == 0) {
            return current_file_size;
        }
        file_manager_file_error_no_ = err_code;
        switch (err_code) {
        case ENOENT:
            FILE_MANAGER_LOG(bq::log_level::error, "get_file_size file not found: %s", handle.file_path_.c_str());
            break;
        case EACCES:
            FILE_MANAGER_LOG(bq::log_level::error, "get_file_size permission is denied for: %s", handle.file_path_.c_str());
            break;
        case EISDIR:
            FILE_MANAGER_LOG(bq::log_level::error, "get_file_size path is dir: %s", handle.file_path_.c_str());
            break;
        case ENOTDIR:
            FILE_MANAGER_LOG(bq::log_level::error, "get_file_size path invalid: %s", handle.file_path_.c_str());
            break;
        default:
            FILE_MANAGER_LOG(bq::log_level::error, "get_file_size path failed: %s, error code:%" PRId32 "", handle.file_path_.c_str(), err_code);
            break;
        }
        return 0;
    }

    int32_t file_manager::get_file_descriptor_index_by_handle(const file_handle& handle) const
    {
        bq::platform::scoped_mutex lock(const_cast<bq::platform::mutex&>(mutex));
        if ((decltype(file_descriptors)::size_type)handle.idx_ >= file_descriptors.size()) {
            // FILE_MANAGER_LOG(bq::log_level::error, "get_file_descriptor_index_by_handle failed, invalid handle idx:%d, seq:%d", handle.idx, handle.seq);
            return -1;
        }
        if (handle.seq_ != file_descriptors[handle.idx_].seq) {
            // FILE_MANAGER_LOG(bq::log_level::error, "get_file_descriptor_index_by_handle failed, invalid handle idx:%d, seq:%d, real seq:%d", handle.idx, handle.seq, file_descriptors[handle.idx].seq);
            return -1;
        }
        return static_cast<int32_t>(handle.idx_);
    }

    void file_manager::inc_ref(const file_handle& handle)
    {
        bq::platform::scoped_mutex lock(const_cast<bq::platform::mutex&>(mutex));
        int32_t desc_idx = get_file_descriptor_index_by_handle(handle);
        if (desc_idx < 0) {
            return;
        }
        auto& desc = file_descriptors[desc_idx];
        desc.inc_ref();
    }

    void file_manager::file_descriptor::clear()
    {
        bq::platform::close_file(*handle_ptr);
        free(handle_ptr);
        handle_ptr = nullptr;
        seq = 0;
        ref_cout = 0;
        file_path.clear();
    }

    file_manager::file_descriptor::file_descriptor()
    {
        handle_ptr = nullptr;
        seq = 0;
        ref_cout = 0;
    }

    bool file_manager::file_descriptor::is_empty() const
    {
        return (!handle_ptr || !bq::platform::is_platform_handle_valid(*handle_ptr)) && seq == 0;
    }

    void file_manager::file_descriptor::inc_ref()
    {
        ref_cout.add_fetch_seq_cst(1);
    }

    void file_manager::file_descriptor::dec_ref()
    {
        ref_cout.add_fetch_seq_cst(-1);
        assert(ref_cout.load() >= 0 && "file_descriptor ref_cout < 0");
        if (ref_cout.load() == 0) {
            clear();
        }
    }

    file_handle::file_handle(const file_handle& rhs)
    {
        idx_ = rhs.idx_;
        seq_ = rhs.seq_;
        handle_ptr_ = rhs.handle_ptr_;
        file_path_ = rhs.file_path_;
        bq::file_manager::instance().inc_ref(rhs);
    }

    file_handle::file_handle(file_handle&& rhs) noexcept
    {
        idx_ = rhs.idx_;
        seq_ = rhs.seq_;
        handle_ptr_ = rhs.handle_ptr_;
        file_path_ = bq::move(rhs.file_path_);
        rhs.clear();
    }

    file_handle::~file_handle()
    {
        bq::file_manager::instance().close_file(*this);
    }

    bq::file_handle& file_handle::operator=(const file_handle& rhs)
    {
        idx_ = rhs.idx_;
        seq_ = rhs.seq_;
        handle_ptr_ = rhs.handle_ptr_;
        file_path_ = rhs.file_path_;
        bq::file_manager::instance().inc_ref(rhs);
        return *this;
    }

    file_handle& file_handle::operator=(file_handle&& rhs) noexcept
    {
        file_manager::instance().close_file(*this);
        idx_ = rhs.idx_;
        seq_ = rhs.seq_;
        handle_ptr_ = rhs.handle_ptr_;
        file_path_ = bq::move(rhs.file_path_);
        rhs.clear();
        return *this;
    }

    void file_handle::invalid()
    {
        file_manager::instance().close_file(*this);
        clear();
    }

    bool file_handle::is_valid() const
    {
        return bq::file_manager::instance().is_handle_valid(*this);
    }

}
