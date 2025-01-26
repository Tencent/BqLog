#include "bq_log/types/buffer/log_buffer.h"

namespace bq {
    log_buffer::log_buffer(const log_buffer_config& config)
    {
        type_ = config.type;
        switch (type_) {
        case log_buffer_type::normal:
            buffer_impl_ = new miso_ring_buffer(config);
            break;
        case log_buffer_type::high_performance:
            buffer_impl_ = new group_list(config);
            break;
        default:
            assert(false && "unknown log_buffer_type type");
            break;
        }
    }

    /// <summary>
    ///
    /// </summary>
    log_buffer::~log_buffer()
    {
        switch (type_) {
        case log_buffer_type::normal:
            delete ((miso_ring_buffer*)buffer_impl_);
            break;
        case log_buffer_type::high_performance:
            delete ((group_list*)buffer_impl_);
            break;
        default:
            assert(false && "unknown log_buffer_type type");
            break;
        }
        buffer_impl_ = nullptr;
    }
}