package bq;
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
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.*;

import bq.def.*;
import bq.impl.*;

/**
 * @author pippocao
 *
 */
public class log {
	static {
		try {
			System.loadLibrary(bq.lib_def.lib_name);
		}catch(Exception e)
		{
			System.err.println("Failed to Load " + bq.lib_def.lib_name);
			System.err.println(e.getMessage());
		}
	}

    @FunctionalInterface
    public interface console_callbck_delegate{
        void callback(long log_id, int category_idx, int log_level, String content);
    }
    
	private static log_category_base default_category_ = new log_category_base() {
		@SuppressWarnings("unused")
		protected long index = 0L;
	};
    private static console_callbck_delegate console_callbck_delegate_;
	
	private long log_id_ = 0;
    private String name_ = "";
    private ByteBuffer merged_log_level_bitmap_ = null;
    private ByteBuffer categories_mask_array_ = null;
    private ByteBuffer print_stack_level_bitmap_ = null;
    protected List<String> categories_name_array_ = null;
    private log_context context_ = null;
    
    protected static log get_log_by_id(long log_id)
    {
        log log = new log();
        String name = log_invoker.__api_get_log_name_by_id(log_id);
        if (null == name)
        {
            return log;
        }
        log.name_ = name;
        log.merged_log_level_bitmap_ = log_invoker.__api_get_log_merged_log_level_bitmap_by_log_id(log_id);
        log.merged_log_level_bitmap_.order(ByteOrder.LITTLE_ENDIAN);
        log.categories_mask_array_ = log_invoker.__api_get_log_category_masks_array_by_log_id(log_id);
        log.categories_mask_array_.order(ByteOrder.LITTLE_ENDIAN);
        log.print_stack_level_bitmap_ = log_invoker.__api_get_log_print_stack_level_bitmap_by_log_id(log_id);
        log.print_stack_level_bitmap_.order(ByteOrder.LITTLE_ENDIAN);

        long category_count = log_invoker.__api_get_log_categories_count(log_id);
        log.categories_name_array_ = new ArrayList<String>((int)category_count);
        for (long i = 0; i < category_count; ++i)
        {
            String category_item_name = log_invoker.__api_get_log_category_name_by_index(log_id, i);
            if (null != category_item_name)
            {
                log.categories_name_array_.add(category_item_name);
            }
        }
        log.log_id_ = log_id;
        log.context_ = new bq.impl.log_context(log);
        return log;
    }

    @SuppressWarnings("unused")
	private static void native_console_callbck(long log_id, int category_idx, int log_level, String content)
    {
        console_callbck_delegate_.callback(log_id, category_idx, log_level, content);
    }
    private boolean is_enable_for(log_category_base category, log_level level)
    {
    	if((merged_log_level_bitmap_.getInt(0) & (1 << level.ordinal())) == 0 
    			|| categories_mask_array_.get((int)log_category_base.get_index(category)) == 0)
        {
            return false;
        }
        return true;
    }

    protected boolean do_log(log_category_base category, log_level level, String log_format_content, Object... args)
    {
        if(!is_enable_for(category, level))
        {
            return false;
        }
        long param_storage_size = 0;
        for(Object o : args)
        {
        	param_storage_size += context_.get_param_storage_size_no_optimized(o);
        }
        if((print_stack_level_bitmap_.getInt(0) & (1 << level.ordinal())) != 0)
        {
        	StringBuffer sb = new StringBuffer(log_format_content);
        	StackTraceElement[] stack_trace_elements = Thread.currentThread().getStackTrace();
            for(int i = 2; i < stack_trace_elements.length; ++i)
            {
            	sb.append('\n');
            	sb.append(stack_trace_elements[i]);
            }
            log_format_content = sb.toString();
        }
    	Map.Entry<long[], ByteBuffer> handle = context_.begin_copy(this, category, level, log_format_content, param_storage_size);
        if(null == handle)
        {
            return false;
        }
        ByteBuffer ring_buffer = handle.getValue();
        for (Object o : args)
        {
        	context_.add_param_no_optimized(ring_buffer, o);
        }
        context_.end_copy(this, handle);
        return true;
    }
    
    /**
     * Get bqLog lib version
     * @return
     */
    public static String get_version()
    {
    	return log_invoker.__api_get_log_version();
    }
    
    /**
     * If bqLog is asynchronous, a crash in the program may cause the logs in the buffer not to be persisted to disk. 
	 * If this feature is enabled, bqLog will attempt to perform a forced flush of the logs in the buffer in the event of a crash. However, 
	 * this functionality does not guarantee success.
     */
    public static void enable_auto_crash_handle()
    {
        log_invoker.__api_enable_auto_crash_handler();
    }

    /**
     * If bqLog is stored in a relative path, it will choose whether the relative path is within the sandbox or not.
     * This will return the absolute paths corresponding to both scenarios.
     * @param is_in_sandbox
     * @return
     */
    public static String get_file_base_dir(boolean is_in_sandbox)
    {
        return log_invoker.__api_get_file_base_dir(is_in_sandbox);
    }
    
    /**
     * Create a log object
     * @param name 
     * 		  	If the log name is an empty string, bqLog will automatically assign you a unique log name. 
     * 			If the log name already exists, it will return the previously existing log object and overwrite the previous configuration with the new config.
     * @param config
     * 			Log config string
     * @return
     * 			A log object, if create failed, the is_valid() method of it will return false
     */
    public static log create_log(String name, String config)
    {
        if (config == null || config.length() == 0)
        {
            return new log();
        }
        long log_handle = log_invoker.__api_create_log(name, config, 0, null);
        log result = get_log_by_id(log_handle);
        return result;
    }

    /**
     * Get a log object by it's name
     * @param log_name
     * 			Name of the log you want to find
     * @return
     * 			A log object, if the log object with specific name was not found, the is_valid() method of it will return false
     */
    public static log get_log_by_name(String log_name)
    {
    	if(log_name == null || log_name.length() == 0)
    	{
    		return new log();
    	}
        long log_count = log_invoker.__api_get_logs_count();
        for (long i = 0; i < log_count; ++i)
        {
            long id = log_invoker.__api_get_log_id_by_index(i);
            String name = log_invoker.__api_get_log_name_by_id(id);
            if (log_name.equals(name))
            {
                return get_log_by_id(id);
            }
        }
        return new log();
    }
    
    /**
     * Synchronously flush the buffer of all log objects
     * to ensure that all data in the buffer is processed after the call.
     */
    public static void force_flush_all_logs()
    {
        log_invoker.__api_force_flush(0);
    }

    /**
     * Register a callback that will be invoked whenever a console log message is output. 
     * This can be used for an external system to monitor console log output.
     * @param callback
     */
    public static void register_console_callback(console_callbck_delegate callback)
    {
        console_callbck_delegate_ = callback;
        log_invoker.__api_set_console_callback(true);
    }

    /**
     * @param Unregister console callback.
     */
    public static void unregister_console_callback()
    {
        console_callbck_delegate_ = null;
        log_invoker.__api_set_console_callback(false);
    }
    
    /**
     * Output to console with log_level.
     * Important: This is not log entry, and can not be caught by console callback with was registered by register_console_callback 
     * @param level
     * @param str
     */
    public static void console(log_level level, String str)
    {
    	log_invoker.__api_log_device_console(level.ordinal(), str);
    }
    
    protected log()
    {
    	
    }
    
    /**
     * copy constructor
     * @param rhs
     */
    public log(log rhs)
    {
    	merged_log_level_bitmap_ = rhs.merged_log_level_bitmap_;
        name_ = rhs.name_;
        log_id_ = rhs.log_id_;
        merged_log_level_bitmap_ = log_invoker.__api_get_log_merged_log_level_bitmap_by_log_id(log_id_);
        merged_log_level_bitmap_.order(ByteOrder.LITTLE_ENDIAN);
        categories_mask_array_ = log_invoker.__api_get_log_category_masks_array_by_log_id(log_id_);
        categories_mask_array_.order(ByteOrder.LITTLE_ENDIAN);
        print_stack_level_bitmap_ = log_invoker.__api_get_log_print_stack_level_bitmap_by_log_id(log_id_);
        print_stack_level_bitmap_.order(ByteOrder.LITTLE_ENDIAN);

        long category_count = log_invoker.__api_get_log_categories_count(log_id_);
        categories_name_array_ = new ArrayList<String>((int)category_count);
        for (long i = 0; i < category_count; ++i)
        {
            String category_item_name = log_invoker.__api_get_log_category_name_by_index(log_id_, i);
            if (null != category_item_name)
            {
                categories_name_array_.add(category_item_name);
            }
        }
        context_ = new bq.impl.log_context(this);
    }
    
    /**
     * Modify the log configuration, but some fields, such as buffer_size, cannot be modified.
     * @param config
     */
    public void reset_config(String config)
    {
        if (config == null || config.length() == 0)
        {
            return;
        }
        log_invoker.__api_log_reset_config(name_, config);
    }

    /**
     * Temporarily disable or enable a specific Appender.
     * @param appender_name
     * @param enable
     */
    public void set_appenders_enable(String appender_name, boolean enable)
    {
        log_invoker.__api_set_appenders_enable(log_id_, appender_name, enable);
    }
    
    /**
     * Synchronously flush the buffer of this log object
     * to ensure that all data in the buffer is processed after the call.
     */
    public void force_flush()
    {
    	log_invoker.__api_force_flush(log_id_);
    }

    /**
     * Get id of this log object
     * @return
     */
    public long get_id()
    {
        return log_id_;
    }
    
    /**
     * Whether a log object is valid
     * @return
     */
    public boolean is_valid()
    {
        return get_id() > 0;
    }

    /**
     * Get the name of a log
     * @return
     */
    public String get_name()
    {
        return name_;
    }

	/**
	 * Enable snapshot capability. Once enabled, the log object will continuously retain a copy of the buffer data, 
	 * containing the latest buffer data. This is used for generating a log snapshot string with the take_snapshot() function.
	 * @param snapshot_buffer_size
	 * 			Size of snapshot buffer
	 */
	public void enable_snapshot(long snapshot_buffer_size)
	{
		bq.impl.log_invoker.__api_enable_snapshot(log_id_, snapshot_buffer_size);
	}

	/**
	 * Works only when enable_snapshot(true) is called.
	 * It will decode the snapshot buffer to text.
	 * @param use_gmt_time
	 * 			Whether the timestamp of each log is GMT time or local time
	 * @return
	 * 			The decoded snapshot buffer
	 */
	public String take_snapshot(boolean use_gmt_time)
	{
		return bq.impl.log_invoker.__api_take_snapshot_string(log_id_, use_gmt_time);
	}

    @Override
    public boolean equals(Object obj)
    {
        if(obj == null || !(obj instanceof log))
        {
            return false;
        }
        return log_id_ == ((log)obj).get_id();
    }

    @Override
    public int hashCode(){  
    	return Long.hashCode(log_id_);
    }


	///Core log functions, there are 6 log levels:
	///verbose, debug, info, warning, error, fatal
    public boolean verbose(String log_format_content, Object... args)
    {
        return do_log(default_category_, log_level.verbose, log_format_content, args);
    }
    public boolean debug(String log_format_content, Object... args)
    {
        return do_log(default_category_, log_level.debug, log_format_content, args);
    }
    public boolean info(String log_format_content, Object... args)
    {
        return do_log(default_category_, log_level.info, log_format_content, args);
    }
    public boolean warning(String log_format_content, Object... args)
    {
        return do_log(default_category_, log_level.warning, log_format_content, args);
    }
    public boolean error(String log_format_content, Object... args)
    {
        return do_log(default_category_, log_level.error, log_format_content, args);
    }
    public boolean fatal(String log_format_content, Object... args)
    {
        return do_log(default_category_, log_level.fatal, log_format_content, args);
    }
}
