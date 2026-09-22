package bq;
/*
 * Copyright (C) 2025 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
import java.io.File;
import java.io.InputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import bq.def.*;
import bq.impl.*;

/**
 * @author pippocao
 *
 */
public class log {
	static {
		try {
			// On Android the .so is extracted by the OS installer from the AAR,
			// so the standard loadLibrary path always works.
			boolean is_android = System.getProperty("java.vm.name", "").contains("Dalvik")
					|| System.getProperty("java.vendor", "").toLowerCase().contains("android");
			if (is_android) {
				System.loadLibrary(bq.lib_def.lib_name);
			} else {
				// For fat-JAR (Maven) distribution: try to locate the bundled native
				// library under /natives/<os_token>-<arch_token>/<lib_file_name> inside
				// the JAR, extract it to a temp file, and load it from there.
				// Falls back to System.loadLibrary so that developers who build
				// locally and manage java.library.path themselves are unaffected.
				boolean loaded = false;
				String os_name  = System.getProperty("os.name",  "").toLowerCase();
				String os_arch  = System.getProperty("os.arch",  "").toLowerCase();

				// Normalise arch to match CI staging directory names
				String arch_token;
				if      (os_arch.equals("amd64")   || os_arch.equals("x86_64"))                      arch_token = "x86_64";
				else if (os_arch.equals("aarch64") || os_arch.equals("arm64"))                        arch_token = "arm64";
				else if (os_arch.equals("x86")     || os_arch.equals("i386") || os_arch.equals("i686")) arch_token = "x86";
				else                                                                                   arch_token = os_arch;

				// Normalise OS and choose the library file name
				String os_token;
				String lib_file_name;
				if (os_name.contains("win")) {
					os_token      = "windows";
					lib_file_name = bq.lib_def.lib_name + ".dll";
				} else if (os_name.contains("mac") || os_name.contains("darwin")) {
					os_token      = "macos";
					lib_file_name = "lib" + bq.lib_def.lib_name + ".dylib";
				} else if (os_name.contains("freebsd")) {
					os_token      = "freebsd";
					lib_file_name = "lib" + bq.lib_def.lib_name + ".so";
				} else if (os_name.contains("openbsd")) {
					os_token      = "openbsd";
					lib_file_name = "lib" + bq.lib_def.lib_name + ".so";
				} else if (os_name.contains("netbsd")) {
					os_token      = "netbsd";
					lib_file_name = "lib" + bq.lib_def.lib_name + ".so";
				} else if (os_name.contains("dragonfly")) {
					os_token      = "dragonfly";
					lib_file_name = "lib" + bq.lib_def.lib_name + ".so";
				} else if (os_name.contains("sunos") || os_name.contains("solaris")) {
					os_token      = "sunos";
					lib_file_name = "lib" + bq.lib_def.lib_name + ".so";
				} else {
					// Linux and any other POSIX-like system
					os_token      = "linux";
					lib_file_name = "lib" + bq.lib_def.lib_name + ".so";
				}

				// Suffix for the temp file so the OS links the right extension
				String lib_suffix = lib_file_name.substring(lib_file_name.lastIndexOf('.'));
				String resource_path = "/natives/" + os_token + "-" + arch_token + "/" + lib_file_name;

				try (InputStream in = log.class.getResourceAsStream(resource_path)) {
					if (in != null) {
						File tmp = File.createTempFile(bq.lib_def.lib_name + "_", lib_suffix);
						tmp.deleteOnExit();
						Files.copy(in, tmp.toPath(), StandardCopyOption.REPLACE_EXISTING);
						System.load(tmp.getAbsolutePath());
						loaded = true;
					}
				} catch (IOException e) {
					// Extraction failed; fall through to loadLibrary
				}

				if (!loaded) {
					System.loadLibrary(bq.lib_def.lib_name);
				}
			}
			Runtime.getRuntime().addShutdownHook(new Thread() {
				@Override
				public void run() {
					log_invoker.__api_mark_jvm_destroyed();
				}
			});
		} catch(Exception e) {
			System.err.println("Failed to Load " + bq.lib_def.lib_name);
			System.err.println(e.getMessage());
		}
	}

	/** Receives console log entries. */
    @FunctionalInterface
    public interface console_callback_delegate{
        void callback(long log_id, int category_idx, bq.def.log_level log_level, String content);
    }
    
	private static log_category_base default_category_ = new log_category_base() {
		@SuppressWarnings("unused")
		protected long index = 0L;
	};
    private static console_callback_delegate console_callback_delegate_ = null;

	private long log_id_ = 0;
    private String name_ = "";
    private ByteBuffer merged_log_level_bitmap_ = null;
    private ByteBuffer categories_mask_array_ = null;
    private ByteBuffer print_stack_level_bitmap_ = null;
	/** Category names configured for this log. */
    protected List<String> categories_name_array_ = null;
    private log_context context_ = null;
    
    protected static log get_log_by_id(long log_id)
    {
        log log_inst = new log();
        String name = log_invoker.__api_get_log_name_by_id(log_id);
        if (null == name)
        {
            return log_inst;
        }
        log_inst.name_ = name;
        log_inst.merged_log_level_bitmap_ = log_invoker.__api_get_log_merged_log_level_bitmap_by_log_id(log_id);
        log_inst.categories_mask_array_ = log_invoker.__api_get_log_category_masks_array_by_log_id(log_id);
        log_inst.print_stack_level_bitmap_ = log_invoker.__api_get_log_print_stack_level_bitmap_by_log_id(log_id);

        long category_count = log_invoker.__api_get_log_categories_count(log_id);
        log_inst.categories_name_array_ = new ArrayList<String>((int)category_count);
        for (long i = 0; i < category_count; ++i)
        {
            String category_item_name = log_invoker.__api_get_log_category_name_by_index(log_id, i);
            if (null != category_item_name)
            {
                log_inst.categories_name_array_.add(category_item_name);
            }
        }
        log_inst.log_id_ = log_id;
        log_inst.context_ = new bq.impl.log_context();
        return log_inst;
    }

    @SuppressWarnings("unused")
	private static void native_console_callback(long log_id, int category_idx, int log_level, String content)
    {
        if(console_callback_delegate_ != null){
            console_callback_delegate_.callback(log_id, category_idx, bq.def.log_level.values()[log_level], content);
        }
    }
    @SuppressWarnings("unused")
	private static void native_console_buffer_fetch_and_remove_callback(console_callback_delegate callback_obj, long log_id, int category_idx, int log_level, String content)
    {
		callback_obj.callback(log_id, category_idx, bq.def.log_level.values()[log_level], content);
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

    
    /**
     * Get bqLog lib version
     * @return The bqLog library version.
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
     * If bqLog is stored in a relative path, the base dir is determined by the value of base_dir_type.
     * This will return the absolute paths corresponding to both scenarios.
     * @param base_dir_type The base directory type.
     * @return The corresponding absolute base directory path.
     */
    public static String get_file_base_dir(int base_dir_type)
    {
        return log_invoker.__api_get_file_base_dir(base_dir_type);
    }
	
	/**
	 * Reset the base dir
	 * @param base_dir_type The base directory type.
	 * @param dir The new base directory path.
	 */
	public static void reset_base_dir(int base_dir_type, String dir)
	{
		bq.impl.log_invoker.__api_reset_base_dir(base_dir_type, dir);
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
     * @param callback The callback to register, or {@code null} to disable callbacks.
     */
    public static void register_console_callback(console_callback_delegate callback)
    {
        console_callback_delegate_ = callback;
        if(null != console_callback_delegate_){
            log_invoker.__api_set_console_callback(true);
        }else {
            log_invoker.__api_set_console_callback(false);
        }
    }

    /**
     * Unregister a previously registered console callback.
     * @param callback The callback to unregister.
     */
    public static void unregister_console_callback(console_callback_delegate callback)
    {
        if(console_callback_delegate_ == callback){
            console_callback_delegate_ = null;
            log_invoker.__api_set_console_callback(false);
        }
    }
    
    /**
     * Enable or disable the console appender buffer. 
     * Since our wrapper may run in both C# and Java virtual machines, and we do not want to directly invoke callbacks from a native thread, 
     * we can enable this option. This way, all console outputs will be saved in the buffer until we fetch them.
     * @param enable Whether console appender buffering is enabled.
     */
    public static void set_console_buffer_enable(boolean enable)
    {
    	log_invoker.__api_set_console_buffer_enable(enable);
    }
    
    /**
     * Fetch and remove a log entry from the console appender buffer in a thread-safe manner. 
     * If the console appender buffer is not empty, the on_console_callback function will be invoked for this log entry. 
     * Please ensure not to output synchronized BQ logs within the callback function.
     * @param on_console_callback
     *        A callback function to be invoked for the fetched log entry if the console appender buffer is not empty
     * @return
     *        True if the console appender buffer is not empty and a log entry is fetched; otherwise False is returned.
     */
    public static boolean fetch_and_remove_console_buffer(console_callback_delegate on_console_callback)
    {
    	return log_invoker.__api_fetch_and_remove_console_buffer(on_console_callback);
    }
    
    /**
     * Output to console with log_level.
     * Important: This is not log entry, and can not be caught by console callback which was registered by register_console_callback or fetch_and_remove_console_buffer
     * @param level The console log level.
     * @param str The text to output.
     */
    public static void console(log_level level, String str)
    {
    	log_invoker.__api_log_device_console(level.ordinal(), str);
    }
    
	/** Creates an empty log instance. */
	protected log()
    {
    	
    }
    
    /**
     * copy constructor
     * @param rhs The log object to copy.
     */
    protected log(log rhs)
    {
    	merged_log_level_bitmap_ = rhs.merged_log_level_bitmap_;
        name_ = rhs.name_;
        log_id_ = rhs.log_id_;
        merged_log_level_bitmap_ = log_invoker.__api_get_log_merged_log_level_bitmap_by_log_id(log_id_);
        categories_mask_array_ = log_invoker.__api_get_log_category_masks_array_by_log_id(log_id_);
        print_stack_level_bitmap_ = log_invoker.__api_get_log_print_stack_level_bitmap_by_log_id(log_id_);

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
        context_ = new bq.impl.log_context();
    }
    
    /**
     * Modify the log configuration, but some fields, such as buffer_size, cannot be modified.
     * @param config The new log configuration string.
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
     * @param appender_name The appender name.
     * @param enable Whether the appender is enabled.
     */
    public void set_appender_enable(String appender_name, boolean enable)
    {
        log_invoker.__api_set_appender_enable(log_id_, appender_name, enable);
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
     * @return The log object identifier.
     */
    public long get_id()
    {
        return log_id_;
    }
    
    /**
     * Whether a log object is valid
     * @return {@code true} if this log object is valid; otherwise {@code false}.
     */
    public boolean is_valid()
    {
        return get_id() != 0;
    }

    /**
     * Get the name of a log
     * @return The log name.
     */
    public String get_name()
    {
        return name_;
    }

	/**
	 * Works only when snapshot is configured.
	 * It will decode the snapshot buffer to text.
	 * @param time_zone_config
	 * 			Use this to specify the time display of log text.
	 *          such as : "localtime", "gmt", "Z", "UTC", "UTC+8", "UTC-11", "utc+11:30"
	 * @return
	 * 			The decoded snapshot buffer
	 */
	public String take_snapshot(String time_zone_config)
	{
		return bq.impl.log_invoker.__api_take_snapshot_string(log_id_, time_zone_config);
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


    //log methods for param count 0
    protected boolean do_log(log_category_base category, log_level level, String log_format_content)
    {
        if(!is_enable_for(category, level))
        {
            return false;
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
    	ByteBuffer ring_buffer = context_.begin_copy(this, category, level, log_format_content, 0);
        if(null == ring_buffer)
        {
            return false;
        }
        return true;
    }

    public boolean verbose(String log_format_content)
    {
        return do_log(default_category_, log_level.verbose, log_format_content);
    }
    /**
     * Writes a debug log without format arguments.
     * @param log_format_content The log content.
     * @return Whether the log was written successfully.
     */
    public boolean debug(String log_format_content)
    {
        return do_log(default_category_, log_level.debug, log_format_content);
    }
    public boolean info(String log_format_content)
    {
        return do_log(default_category_, log_level.info, log_format_content);
    }
    public boolean warning(String log_format_content)
    {
        return do_log(default_category_, log_level.warning, log_format_content);
    }
    public boolean error(String log_format_content)
    {
        return do_log(default_category_, log_level.error, log_format_content);
    } 
    public boolean fatal(String log_format_content)
    {
        return do_log(default_category_, log_level.fatal, log_format_content);
    }

    //log methods for param count 1
    @SuppressWarnings("unchecked")
    protected boolean do_log(log_category_base category, log_level level, String log_format_content, Object p1)
    {
        if(!is_enable_for(category, level))
        {
            if(null != p1 && p1.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p1);
            }
            return false;
        }
        long param_storage_size = context_.get_param_storage_size_no_optimized(p1);
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
    	ByteBuffer ring_buffer = context_.begin_copy(this, category, level, log_format_content, param_storage_size);
        if(null == ring_buffer)
        {
            return false;
        }
        context_.add_param_no_optimized(ring_buffer, p1);
        context_.end_copy(this);
        return true;
    }
    public boolean verbose(String log_format_content, Object p1)
    {
        return do_log(default_category_, log_level.verbose, log_format_content, p1);
    }
    public boolean debug(String log_format_content, Object p1)
    {
        return do_log(default_category_, log_level.debug, log_format_content, p1);
    }
    public boolean info(String log_format_content, Object p1)
    {
        return do_log(default_category_, log_level.info, log_format_content, p1);
    }
    public boolean warning(String log_format_content, Object p1)
    {
        return do_log(default_category_, log_level.warning, log_format_content, p1);
    }
    public boolean error(String log_format_content, Object p1)
    {
        return do_log(default_category_, log_level.error, log_format_content, p1);
    }
    public boolean fatal(String log_format_content, Object p1)
    {
        return do_log(default_category_, log_level.fatal, log_format_content, p1);
    }

    //log methods for param count 2
    @SuppressWarnings("unchecked")
    protected boolean do_log(log_category_base category, log_level level, String log_format_content, Object p1, Object p2)
    {
        if(!is_enable_for(category, level))
        {
            if(null != p1 && p1.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p1);
            }
            if(null != p2 && p2.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p2);
            }
            return false;
        }
        long param_storage_size = context_.get_param_storage_size_no_optimized(p1) + context_.get_param_storage_size_no_optimized(p2);
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
    	ByteBuffer ring_buffer = context_.begin_copy(this, category, level, log_format_content, param_storage_size);
        if(null == ring_buffer)
        {
            return false;
        }
        context_.add_param_no_optimized(ring_buffer, p1);
        context_.add_param_no_optimized(ring_buffer, p2);
        context_.end_copy(this);
        return true;
    }
    public boolean verbose(String log_format_content, Object p1, Object p2)
    {
        return do_log(default_category_, log_level.verbose, log_format_content, p1, p2);
    }
    public boolean debug(String log_format_content, Object p1, Object p2)
    {
        return do_log(default_category_, log_level.debug, log_format_content, p1, p2);
    }
    public boolean info(String log_format_content, Object p1, Object p2)
    {
        return do_log(default_category_, log_level.info, log_format_content, p1, p2);
    }
    public boolean warning(String log_format_content, Object p1, Object p2)
    {
        return do_log(default_category_, log_level.warning, log_format_content, p1, p2);
    }
    public boolean error(String log_format_content, Object p1, Object p2)
    {
        return do_log(default_category_, log_level.error, log_format_content, p1, p2);
    }
    public boolean fatal(String log_format_content, Object p1, Object p2)
    {
        return do_log(default_category_, log_level.fatal, log_format_content, p1, p2);
    }

    //log methods for param count 3
    @SuppressWarnings("unchecked")
    protected boolean do_log(log_category_base category, log_level level, String log_format_content, Object p1, Object p2, Object p3)
    {
        if(!is_enable_for(category, level))
        {
            if(null != p1 && p1.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p1);
            }
            if(null != p2 && p2.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p2);
            }
            if(null != p3 && p3.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p3);
            }
            return false;
        }
        long param_storage_size = context_.get_param_storage_size_no_optimized(p1) + context_.get_param_storage_size_no_optimized(p2) + context_.get_param_storage_size_no_optimized(p3);
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
    	ByteBuffer ring_buffer = context_.begin_copy(this, category, level, log_format_content, param_storage_size);
        if(null == ring_buffer)
        {
            return false;
        }
        context_.add_param_no_optimized(ring_buffer, p1);
        context_.add_param_no_optimized(ring_buffer, p2);
        context_.add_param_no_optimized(ring_buffer, p3);
        context_.end_copy(this);
        return true;
    }
    public boolean verbose(String log_format_content, Object p1, Object p2, Object p3)
    {
        return do_log(default_category_, log_level.verbose, log_format_content, p1, p2, p3);
    }
    public boolean debug(String log_format_content, Object p1, Object p2, Object p3)
    {
        return do_log(default_category_, log_level.debug, log_format_content, p1, p2, p3);
    }
    public boolean info(String log_format_content, Object p1, Object p2, Object p3)
    {
        return do_log(default_category_, log_level.info, log_format_content, p1, p2, p3);
    }
    public boolean warning(String log_format_content, Object p1, Object p2, Object p3)
    {
        return do_log(default_category_, log_level.warning, log_format_content, p1, p2, p3);
    }
    public boolean error(String log_format_content, Object p1, Object p2, Object p3)
    {
        return do_log(default_category_, log_level.error, log_format_content, p1, p2, p3);
    }
    public boolean fatal(String log_format_content, Object p1, Object p2, Object p3)
    {
        return do_log(default_category_, log_level.fatal, log_format_content, p1, p2, p3);
    }

    //log methods for param count 4
    @SuppressWarnings("unchecked")
    protected boolean do_log(log_category_base category, log_level level, String log_format_content, Object p1, Object p2, Object p3, Object p4)
    {
        if(!is_enable_for(category, level))
        {
            if(null != p1 && p1.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p1);
            }
            if(null != p2 && p2.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p2);
            }
            if(null != p3 && p3.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p3);
            }
            if(null != p4 && p4.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p4);
            }
            return false;
        }
        long param_storage_size = context_.get_param_storage_size_no_optimized(p1) + context_.get_param_storage_size_no_optimized(p2) + context_.get_param_storage_size_no_optimized(p3) + context_.get_param_storage_size_no_optimized(p4);
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
    	ByteBuffer ring_buffer = context_.begin_copy(this, category, level, log_format_content, param_storage_size);
        if(null == ring_buffer)
        {
            return false;
        }
        context_.add_param_no_optimized(ring_buffer, p1);
        context_.add_param_no_optimized(ring_buffer, p2);
        context_.add_param_no_optimized(ring_buffer, p3);
        context_.add_param_no_optimized(ring_buffer, p4);
        context_.end_copy(this);
        return true;
    }
    public boolean verbose(String log_format_content, Object p1, Object p2, Object p3, Object p4)
    {
        return do_log(default_category_, log_level.verbose, log_format_content, p1, p2, p3, p4);
    }
    public boolean debug(String log_format_content, Object p1, Object p2, Object p3, Object p4)
    {
        return do_log(default_category_, log_level.debug, log_format_content, p1, p2, p3, p4);
    }
    public boolean info(String log_format_content, Object p1, Object p2, Object p3, Object p4)
    {
        return do_log(default_category_, log_level.info, log_format_content, p1, p2, p3, p4);
    }
    public boolean warning(String log_format_content, Object p1, Object p2, Object p3, Object p4)
    {
        return do_log(default_category_, log_level.warning, log_format_content, p1, p2, p3, p4);
    }
    public boolean error(String log_format_content, Object p1, Object p2, Object p3, Object p4)
    {
        return do_log(default_category_, log_level.error, log_format_content, p1, p2, p3, p4);
    }
    public boolean fatal(String log_format_content, Object p1, Object p2, Object p3, Object p4)
    {
        return do_log(default_category_, log_level.fatal, log_format_content, p1, p2, p3, p4);
    }

    //log methods for param count 5
    @SuppressWarnings("unchecked")
    protected boolean do_log(log_category_base category, log_level level, String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5)
    {
        if(!is_enable_for(category, level))
        {
            if(null != p1 && p1.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p1);
            }
            if(null != p2 && p2.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p2);
            }
            if(null != p3 && p3.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p3);
            }
            if(null != p4 && p4.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p4);
            }
            if(null != p5 && p5.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p5);
            }
            return false;
        }
        long param_storage_size = context_.get_param_storage_size_no_optimized(p1) + context_.get_param_storage_size_no_optimized(p2) + context_.get_param_storage_size_no_optimized(p3) + context_.get_param_storage_size_no_optimized(p4) + context_.get_param_storage_size_no_optimized(p5);
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
    	ByteBuffer ring_buffer = context_.begin_copy(this, category, level, log_format_content, param_storage_size);
        if(null == ring_buffer)
        {
            return false;
        }
        context_.add_param_no_optimized(ring_buffer, p1);
        context_.add_param_no_optimized(ring_buffer, p2);
        context_.add_param_no_optimized(ring_buffer, p3);
        context_.add_param_no_optimized(ring_buffer, p4);
        context_.add_param_no_optimized(ring_buffer, p5);
        context_.end_copy(this);
        return true;
    }
    public boolean verbose(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5)
    {
        return do_log(default_category_, log_level.verbose, log_format_content, p1, p2, p3, p4, p5);
    }
    public boolean debug(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5)
    {
        return do_log(default_category_, log_level.debug, log_format_content, p1, p2, p3, p4, p5);
    }
    public boolean info(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5)
    {
        return do_log(default_category_, log_level.info, log_format_content, p1, p2, p3, p4, p5);
    }
    public boolean warning(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5)
    {
        return do_log(default_category_, log_level.warning, log_format_content, p1, p2, p3, p4, p5);
    }
    public boolean error(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5)
    {
        return do_log(default_category_, log_level.error, log_format_content, p1, p2, p3, p4, p5);
    }
    public boolean fatal(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5)
    {
        return do_log(default_category_, log_level.fatal, log_format_content, p1, p2, p3, p4, p5);
    }

    //log methods for param count 6
    @SuppressWarnings("unchecked")
    protected boolean do_log(log_category_base category, log_level level, String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6)
    {
        if(!is_enable_for(category, level))
        {
            if(null != p1 && p1.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p1);
            }
            if(null != p2 && p2.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p2);
            }
            if(null != p3 && p3.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p3);
            }
            if(null != p4 && p4.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p4);
            }
            if(null != p5 && p5.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p5);
            }
            if(null != p6 && p6.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p6);
            }
            return false;
        }
        long param_storage_size = context_.get_param_storage_size_no_optimized(p1) + context_.get_param_storage_size_no_optimized(p2) + context_.get_param_storage_size_no_optimized(p3) + context_.get_param_storage_size_no_optimized(p4) + context_.get_param_storage_size_no_optimized(p5) + context_.get_param_storage_size_no_optimized(p6);
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
    	ByteBuffer ring_buffer = context_.begin_copy(this, category, level, log_format_content, param_storage_size);
        if(null == ring_buffer)
        {
            return false;
        }
        context_.add_param_no_optimized(ring_buffer, p1);
        context_.add_param_no_optimized(ring_buffer, p2);
        context_.add_param_no_optimized(ring_buffer, p3);
        context_.add_param_no_optimized(ring_buffer, p4);
        context_.add_param_no_optimized(ring_buffer, p5);
        context_.add_param_no_optimized(ring_buffer, p6);
        context_.end_copy(this);
        return true;
    }
    public boolean verbose(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6)
    {
        return do_log(default_category_, log_level.verbose, log_format_content, p1, p2, p3, p4, p5, p6);
    }
    public boolean debug(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6)
    {
        return do_log(default_category_, log_level.debug, log_format_content, p1, p2, p3, p4, p5, p6);
    }
    public boolean info(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6)
    {
        return do_log(default_category_, log_level.info, log_format_content, p1, p2, p3, p4, p5, p6);
    }
    public boolean warning(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6)
    {
        return do_log(default_category_, log_level.warning, log_format_content, p1, p2, p3, p4, p5, p6);
    }
    public boolean error(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6)
    {
        return do_log(default_category_, log_level.error, log_format_content, p1, p2, p3, p4, p5, p6);
    }
    public boolean fatal(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6)
    {
        return do_log(default_category_, log_level.fatal, log_format_content, p1, p2, p3, p4, p5, p6);
    }

    //log methods for param count 7
    @SuppressWarnings("unchecked")
    protected boolean do_log(log_category_base category, log_level level, String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7)
    {
        if(!is_enable_for(category, level))
        {
            if(null != p1 && p1.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p1);
            }
            if(null != p2 && p2.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p2);
            }
            if(null != p3 && p3.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p3);
            }
            if(null != p4 && p4.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p4);
            }
            if(null != p5 && p5.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p5);
            }
            if(null != p6 && p6.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p6);
            }
            if(null != p7 && p7.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p7);
            }
            return false;
        }
        long param_storage_size = context_.get_param_storage_size_no_optimized(p1) + context_.get_param_storage_size_no_optimized(p2) + context_.get_param_storage_size_no_optimized(p3) + context_.get_param_storage_size_no_optimized(p4) + context_.get_param_storage_size_no_optimized(p5) + context_.get_param_storage_size_no_optimized(p6) + context_.get_param_storage_size_no_optimized(p7);
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
    	ByteBuffer ring_buffer = context_.begin_copy(this, category, level, log_format_content, param_storage_size);
        if(null == ring_buffer)
        {
            return false;
        }
        context_.add_param_no_optimized(ring_buffer, p1);
        context_.add_param_no_optimized(ring_buffer, p2);
        context_.add_param_no_optimized(ring_buffer, p3);
        context_.add_param_no_optimized(ring_buffer, p4);
        context_.add_param_no_optimized(ring_buffer, p5);
        context_.add_param_no_optimized(ring_buffer, p6);
        context_.add_param_no_optimized(ring_buffer, p7);
        context_.end_copy(this);
        return true;
    }
    public boolean verbose(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7)
    {
        return do_log(default_category_, log_level.verbose, log_format_content, p1, p2, p3, p4, p5, p6, p7);
    }
    public boolean debug(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7)
    {
        return do_log(default_category_, log_level.debug, log_format_content, p1, p2, p3, p4, p5, p6, p7);
    }
    public boolean info(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7)
    {
        return do_log(default_category_, log_level.info, log_format_content, p1, p2, p3, p4, p5, p6, p7);
    }
    public boolean warning(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7)
    {
        return do_log(default_category_, log_level.warning, log_format_content, p1, p2, p3, p4, p5, p6, p7);
    }
    public boolean error(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7)
    {
        return do_log(default_category_, log_level.error, log_format_content, p1, p2, p3, p4, p5, p6, p7);
    }
    public boolean fatal(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7)
    {
        return do_log(default_category_, log_level.fatal, log_format_content, p1, p2, p3, p4, p5, p6, p7);
    }

    //log methods for param count 8
    @SuppressWarnings("unchecked")
    protected boolean do_log(log_category_base category, log_level level, String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8)
    {
        if(!is_enable_for(category, level))
        {
            if(null != p1 && p1.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p1);
            }
            if(null != p2 && p2.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p2);
            }
            if(null != p3 && p3.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p3);
            }
            if(null != p4 && p4.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p4);
            }
            if(null != p5 && p5.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p5);
            }
            if(null != p6 && p6.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p6);
            }
            if(null != p7 && p7.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p7);
            }
            if(null != p8 && p8.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p8);
            }
            return false;
        }
        long param_storage_size = context_.get_param_storage_size_no_optimized(p1) + context_.get_param_storage_size_no_optimized(p2) + context_.get_param_storage_size_no_optimized(p3) + context_.get_param_storage_size_no_optimized(p4) + context_.get_param_storage_size_no_optimized(p5) + context_.get_param_storage_size_no_optimized(p6) + context_.get_param_storage_size_no_optimized(p7) + context_.get_param_storage_size_no_optimized(p8);
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
    	ByteBuffer ring_buffer = context_.begin_copy(this, category, level, log_format_content, param_storage_size);
        if(null == ring_buffer)
        {
            return false;
        }
        context_.add_param_no_optimized(ring_buffer, p1);
        context_.add_param_no_optimized(ring_buffer, p2);
        context_.add_param_no_optimized(ring_buffer, p3);
        context_.add_param_no_optimized(ring_buffer, p4);
        context_.add_param_no_optimized(ring_buffer, p5);
        context_.add_param_no_optimized(ring_buffer, p6);
        context_.add_param_no_optimized(ring_buffer, p7);
        context_.add_param_no_optimized(ring_buffer, p8);
        context_.end_copy(this);
        return true;
    }
    public boolean verbose(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8)
    {
        return do_log(default_category_, log_level.verbose, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8);
    }
    public boolean debug(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8)
    {
        return do_log(default_category_, log_level.debug, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8);
    }
    public boolean info(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8)
    {
        return do_log(default_category_, log_level.info, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8);
    }
    public boolean warning(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8)
    {
        return do_log(default_category_, log_level.warning, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8);
    }
    public boolean error(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8)
    {
        return do_log(default_category_, log_level.error, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8);
    }
    public boolean fatal(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8)
    {
        return do_log(default_category_, log_level.fatal, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8);
    }

    //log methods for param count 9
    @SuppressWarnings("unchecked")
    protected boolean do_log(log_category_base category, log_level level, String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9)
    {
        if(!is_enable_for(category, level))
        {
            if(null != p1 && p1.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p1);
            }
            if(null != p2 && p2.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p2);
            }
            if(null != p3 && p3.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p3);
            }
            if(null != p4 && p4.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p4);
            }
            if(null != p5 && p5.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p5);
            }
            if(null != p6 && p6.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p6);
            }
            if(null != p7 && p7.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p7);
            }
            if(null != p8 && p8.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p8);
            }
            if(null != p9 && p9.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p9);
            }
            return false;
        }
        long param_storage_size = context_.get_param_storage_size_no_optimized(p1) + context_.get_param_storage_size_no_optimized(p2) + context_.get_param_storage_size_no_optimized(p3) + context_.get_param_storage_size_no_optimized(p4) + context_.get_param_storage_size_no_optimized(p5) + context_.get_param_storage_size_no_optimized(p6) + context_.get_param_storage_size_no_optimized(p7) + context_.get_param_storage_size_no_optimized(p8) + context_.get_param_storage_size_no_optimized(p9);
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
    	ByteBuffer ring_buffer = context_.begin_copy(this, category, level, log_format_content, param_storage_size);
        if(null == ring_buffer)
        {
            return false;
        }
        context_.add_param_no_optimized(ring_buffer, p1);
        context_.add_param_no_optimized(ring_buffer, p2);
        context_.add_param_no_optimized(ring_buffer, p3);
        context_.add_param_no_optimized(ring_buffer, p4);
        context_.add_param_no_optimized(ring_buffer, p5);
        context_.add_param_no_optimized(ring_buffer, p6);
        context_.add_param_no_optimized(ring_buffer, p7);
        context_.add_param_no_optimized(ring_buffer, p8);
        context_.add_param_no_optimized(ring_buffer, p9);
        context_.end_copy(this);
        return true;
    }
    public boolean verbose(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9)
    {
        return do_log(default_category_, log_level.verbose, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8, p9);
    }
    public boolean debug(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9)
    {
        return do_log(default_category_, log_level.debug, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8, p9);
    }
    public boolean info(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9)
    {
        return do_log(default_category_, log_level.info, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8, p9);
    }
    public boolean warning(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9)
    {
        return do_log(default_category_, log_level.warning, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8, p9);
    }
    public boolean error(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9)
    {
        return do_log(default_category_, log_level.error, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8, p9);
    }
    public boolean fatal(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9)
    {
        return do_log(default_category_, log_level.fatal, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8, p9);
    }

    //log methods for param count 10
    @SuppressWarnings("unchecked")
    protected boolean do_log(log_category_base category, log_level level, String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9, Object p10)
    {
        if(!is_enable_for(category, level))
        {
            if(null != p1 && p1.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p1);
            }
            if(null != p2 && p2.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p2);
            }
            if(null != p3 && p3.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p3);
            }
            if(null != p4 && p4.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p4);
            }
            if(null != p5 && p5.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p5);
            }
            if(null != p6 && p6.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p6);
            }
            if(null != p7 && p7.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p7);
            }
            if(null != p8 && p8.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p8);
            }
            if(null != p9 && p9.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p9);
            }
            if(null != p10 && p10.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p10);
            }
            return false;
        }
        long param_storage_size = context_.get_param_storage_size_no_optimized(p1) + context_.get_param_storage_size_no_optimized(p2) + context_.get_param_storage_size_no_optimized(p3) + context_.get_param_storage_size_no_optimized(p4) + context_.get_param_storage_size_no_optimized(p5) + context_.get_param_storage_size_no_optimized(p6) + context_.get_param_storage_size_no_optimized(p7) + context_.get_param_storage_size_no_optimized(p8) + context_.get_param_storage_size_no_optimized(p9) + context_.get_param_storage_size_no_optimized(p10);
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
    	ByteBuffer ring_buffer = context_.begin_copy(this, category, level, log_format_content, param_storage_size);
        if(null == ring_buffer)
        {
            return false;
        }
        context_.add_param_no_optimized(ring_buffer, p1);
        context_.add_param_no_optimized(ring_buffer, p2);
        context_.add_param_no_optimized(ring_buffer, p3);
        context_.add_param_no_optimized(ring_buffer, p4);
        context_.add_param_no_optimized(ring_buffer, p5);
        context_.add_param_no_optimized(ring_buffer, p6);
        context_.add_param_no_optimized(ring_buffer, p7);
        context_.add_param_no_optimized(ring_buffer, p8);
        context_.add_param_no_optimized(ring_buffer, p9);
        context_.add_param_no_optimized(ring_buffer, p10);
        context_.end_copy(this);
        return true;
    }
    public boolean verbose(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9, Object p10)
    {
        return do_log(default_category_, log_level.verbose, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
    }
    public boolean debug(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9, Object p10)
    {
        return do_log(default_category_, log_level.debug, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
    }
    public boolean info(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9, Object p10)
    {
        return do_log(default_category_, log_level.info, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
    }
    public boolean warning(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9, Object p10)
    {
        return do_log(default_category_, log_level.warning, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
    }
    public boolean error(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9, Object p10)
    {
        return do_log(default_category_, log_level.error, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
    }
    public boolean fatal(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9, Object p10)
    {
        return do_log(default_category_, log_level.fatal, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
    }

    //log methods for param count 11
    @SuppressWarnings("unchecked")
    protected boolean do_log(log_category_base category, log_level level, String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9, Object p10, Object p11)
    {
        if(!is_enable_for(category, level))
        {
            if(null != p1 && p1.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p1);
            }
            if(null != p2 && p2.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p2);
            }
            if(null != p3 && p3.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p3);
            }
            if(null != p4 && p4.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p4);
            }
            if(null != p5 && p5.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p5);
            }
            if(null != p6 && p6.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p6);
            }
            if(null != p7 && p7.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p7);
            }
            if(null != p8 && p8.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p8);
            }
            if(null != p9 && p9.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p9);
            }
            if(null != p10 && p10.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p10);
            }
            if(null != p11 && p11.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p11);
            }
            return false;
        }
        long param_storage_size = context_.get_param_storage_size_no_optimized(p1) + context_.get_param_storage_size_no_optimized(p2) + context_.get_param_storage_size_no_optimized(p3) + context_.get_param_storage_size_no_optimized(p4) + context_.get_param_storage_size_no_optimized(p5) + context_.get_param_storage_size_no_optimized(p6) + context_.get_param_storage_size_no_optimized(p7) + context_.get_param_storage_size_no_optimized(p8) + context_.get_param_storage_size_no_optimized(p9) + context_.get_param_storage_size_no_optimized(p10) + context_.get_param_storage_size_no_optimized(p11);
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
    	ByteBuffer ring_buffer = context_.begin_copy(this, category, level, log_format_content, param_storage_size);
        if(null == ring_buffer)
        {
            return false;
        }
        context_.add_param_no_optimized(ring_buffer, p1);
        context_.add_param_no_optimized(ring_buffer, p2);
        context_.add_param_no_optimized(ring_buffer, p3);
        context_.add_param_no_optimized(ring_buffer, p4);
        context_.add_param_no_optimized(ring_buffer, p5);
        context_.add_param_no_optimized(ring_buffer, p6);
        context_.add_param_no_optimized(ring_buffer, p7);
        context_.add_param_no_optimized(ring_buffer, p8);
        context_.add_param_no_optimized(ring_buffer, p9);
        context_.add_param_no_optimized(ring_buffer, p10);
        context_.add_param_no_optimized(ring_buffer, p11);
        context_.end_copy(this);
        return true;
    }
    public boolean verbose(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9, Object p10, Object p11)
    {
        return do_log(default_category_, log_level.verbose, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);
    }
    public boolean debug(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9, Object p10, Object p11)
    {
        return do_log(default_category_, log_level.debug, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);
    }
    public boolean info(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9, Object p10, Object p11)
    {
        return do_log(default_category_, log_level.info, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);
    }
    public boolean warning(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9, Object p10, Object p11)
    {
        return do_log(default_category_, log_level.warning, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);
    }
    public boolean error(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9, Object p10, Object p11)
    {
        return do_log(default_category_, log_level.error, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);
    }
    public boolean fatal(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9, Object p10, Object p11)
    {
        return do_log(default_category_, log_level.fatal, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);
    }

    //log methods for param count 12
    @SuppressWarnings("unchecked")
    protected boolean do_log(log_category_base category, log_level level, String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9, Object p10, Object p11, Object p12)
    {
        if(!is_enable_for(category, level))
        {
            if(null != p1 && p1.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p1);
            }
            if(null != p2 && p2.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p2);
            }
            if(null != p3 && p3.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p3);
            }
            if(null != p4 && p4.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p4);
            }
            if(null != p5 && p5.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p5);
            }
            if(null != p6 && p6.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p6);
            }
            if(null != p7 && p7.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p7);
            }
            if(null != p8 && p8.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p8);
            }
            if(null != p9 && p9.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p9);
            }
            if(null != p10 && p10.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p10);
            }
            if(null != p11 && p11.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p11);
            }
            if(null != p12 && p12.getClass() == constants.cls_param_wrapper)
            {
                bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)p12);
            }
            return false;
        }
        long param_storage_size = context_.get_param_storage_size_no_optimized(p1) + context_.get_param_storage_size_no_optimized(p2) + context_.get_param_storage_size_no_optimized(p3) + context_.get_param_storage_size_no_optimized(p4) + context_.get_param_storage_size_no_optimized(p5) + context_.get_param_storage_size_no_optimized(p6) + context_.get_param_storage_size_no_optimized(p7) + context_.get_param_storage_size_no_optimized(p8) + context_.get_param_storage_size_no_optimized(p9) + context_.get_param_storage_size_no_optimized(p10) + context_.get_param_storage_size_no_optimized(p11) + context_.get_param_storage_size_no_optimized(p12);
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
    	ByteBuffer ring_buffer = context_.begin_copy(this, category, level, log_format_content, param_storage_size);
        if(null == ring_buffer)
        {
            return false;
        }
        context_.add_param_no_optimized(ring_buffer, p1);
        context_.add_param_no_optimized(ring_buffer, p2);
        context_.add_param_no_optimized(ring_buffer, p3);
        context_.add_param_no_optimized(ring_buffer, p4);
        context_.add_param_no_optimized(ring_buffer, p5);
        context_.add_param_no_optimized(ring_buffer, p6);
        context_.add_param_no_optimized(ring_buffer, p7);
        context_.add_param_no_optimized(ring_buffer, p8);
        context_.add_param_no_optimized(ring_buffer, p9);
        context_.add_param_no_optimized(ring_buffer, p10);
        context_.add_param_no_optimized(ring_buffer, p11);
        context_.add_param_no_optimized(ring_buffer, p12);
        context_.end_copy(this);
        return true;
    }
    public boolean verbose(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9, Object p10, Object p11, Object p12)
    {
        return do_log(default_category_, log_level.verbose, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12);
    }
    public boolean debug(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9, Object p10, Object p11, Object p12)
    {
        return do_log(default_category_, log_level.debug, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12);
    }
    public boolean info(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9, Object p10, Object p11, Object p12)
    {
        return do_log(default_category_, log_level.info, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12);
    }
    public boolean warning(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9, Object p10, Object p11, Object p12)
    {
        return do_log(default_category_, log_level.warning, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12);
    }
    public boolean error(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9, Object p10, Object p11, Object p12)
    {
        return do_log(default_category_, log_level.error, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12);
    }
    public boolean fatal(String log_format_content, Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8, Object p9, Object p10, Object p11, Object p12)
    {
        return do_log(default_category_, log_level.fatal, log_format_content, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12);
    }

	///Core log functions, there are 6 log levels:
	///verbose, debug, info, warning, error, fatal
	///
    //log methods for variable param count
    @SuppressWarnings("unchecked")
    protected boolean do_log(log_category_base category, log_level level, String log_format_content, Object... args)
    {
        if(!is_enable_for(category, level))
        {
            for(Object o : args)
            {
                if(null != o && o.getClass() == constants.cls_param_wrapper)
                {
                    bq.utils.param.return_param_wrapper_to_pool((Map.Entry<int[], long[]>)o);
                }
            }
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
    	ByteBuffer ring_buffer = context_.begin_copy(this, category, level, log_format_content, param_storage_size);
        if(null == ring_buffer)
        {
            return false;
        }
        if(param_storage_size > 0) {
            for (Object o : args)
            {
            	context_.add_param_no_optimized(ring_buffer, o);
            }
            context_.end_copy(this);
        }
        return true;
    }
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
