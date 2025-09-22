package bq.impl;
/*
 * Copyright (C) 2024 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */

/*
 * The Java implementation of log_context differs significantly from C#, 
 * and it appears counter intuitive. This is all in the name of performance. Because:

 * 1. Java doesn't have "Struct" + "Implicit Conversions" to avoid heap allocation.
 * 2. If you directly place a log_context object inside Java's ThreadLocal, 
 * 	  then in an environment with a thread pool + multiple web applications, 
 *    it might cause memory leaks, preventing the application from unloading cleanly.
 * 3. We try to avoid passing custom objects to JNI as much as possible because the C++ side 
 *    would then have to use reflection to retrieve values. And caching reflections isn't safe enough 
 *    in situations with a single JVM and multiple classLoaders.
 * 4. ThreadLocal.get actually involves a hash map lookup, so we try to call it as infrequently as possible.
 * 
 * 
 * Author : pippocao
 */

import bq.log;
import bq.def.*;
import bq.utils.param;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Map;
import java.util.AbstractMap.SimpleEntry;


public class log_context {
	 enum log_jni_long_params_enum
	 {
		 data_offset,
		 params_count
	 }
	 
     private static final int _log_head_def_size = 24;
     private bq.log parent_log_;
 	 private ThreadLocal<Map.Entry<long[], ByteBuffer>> thread_local_pool = null;


     private static long align4(long n)
     {
         return n == 0 ? n : (((n - 1) & (~((long)4 - 1))) + 4);
     }
 	 
 	 
 	 public log_context(bq.log parent_log)
 	 {
 		parent_log_ = parent_log;
 		thread_local_pool = new ThreadLocal<Map.Entry<long[], ByteBuffer>>(){
 	 		 @Override
 	 	    protected Map.Entry<long[], ByteBuffer> initialValue() {
 	 	         ByteBuffer thread_local_ring_buffer_ref = log_invoker.__api_get_log_ring_buffer(parent_log.get_id());
 	 	 		 thread_local_ring_buffer_ref.order(ByteOrder.LITTLE_ENDIAN);
 	 	         return new SimpleEntry<long[], ByteBuffer>(new long[log_jni_long_params_enum.params_count.ordinal()], thread_local_ring_buffer_ref);
 	 	    }
 	 	 };
 	 }
     
     public long get_param_storage_size(Map.Entry<int[], long[]> obj)
     {
    	 return obj.getValue()[1];
     }

     public long get_param_storage_size(Boolean obj)
     {
         return 4;
     }
     
     public long get_param_storage_size(String obj)
     {
         if (null == obj)
         {
             return 4;
         }
         //utf-16 String
         return align4(4/*sizeof typeinfo*/ + 4/*sizeof(uint32_t)*/ + ((long)obj.length() << 1));
     }

     public long get_param_storage_size(Character obj)
     {
         return 4;
     }

     public long get_param_storage_size(Byte obj)
     {
         return 4;
     }
     public long get_param_storage_size(Short obj)
     {
         return 4;
     }
     public long get_param_storage_size(Integer obj)
     {
         return 8;
     }
     public long get_param_storage_size(Long obj)
     {
         return 12;
     }
     public long get_param_storage_size(Float obj)
     {
         return 8;
     }
     public long get_param_storage_size(Double obj)
     {
         return 12;
     }

     public <T> long get_param_storage_size(T obj)
     {
         if (null == obj)
         {
             return 4;
         }
         return get_param_storage_size(obj.toString());
     }

     @SuppressWarnings("unchecked")
	public long get_param_storage_size_no_optimized(Object obj)
     {
         if (null == obj)
         {
             return 4;
         }
         Class<? extends Object> cls = obj.getClass();
         
         //The order of the if-else statements is determined by experience, 
         //with those used more frequently generally placed at the beginning.
         if(cls == constants.cls_param_wrapper)
         {
        	 return get_param_storage_size((Map.Entry<int[], long[]>)obj);
         }else if(cls == Integer.class)
         {
        	 return get_param_storage_size((Integer)obj);
         }else if(cls == Float.class)
         {
        	 return get_param_storage_size((Float)obj);
         }else if(cls == Boolean.class)
         {
        	 return get_param_storage_size((Boolean)obj);
         }
         else if(cls == String.class)
         {
        	 return get_param_storage_size((String)obj);
         }else if(cls == Long.class)
         {
        	 return get_param_storage_size((Long)obj);
         }else if(cls == Double.class)
         {
        	 return get_param_storage_size((Double)obj);
         }else if(cls == Character.class)
         {
        	 return get_param_storage_size((Character)obj);
         }else if(cls == Byte.class)
         {
        	 return get_param_storage_size((Byte)obj);
         }else if(cls == Short.class)
         {
        	 return get_param_storage_size((Short)obj);
         }else if(cls == StringBuffer.class)
         {
        	 return get_param_storage_size(((StringBuffer)obj).chars());
         }else
         {
        	 return get_param_storage_size(obj.toString());
         }
     }


     //string in Java and C# is immutable, it is thread safe, length() is always equal to real string size.
     //we don't need save string length to avoid crash like c++ wrapper.
     public Map.Entry<long[], ByteBuffer> begin_copy(log target_log, log_category_base target_category, log_level target_level, String log_format_content, long param_storage_size)
     {
    	 Map.Entry<long[], ByteBuffer> thread_cache_obj = thread_local_pool.get();
    	 long[] long_params = thread_cache_obj.getKey();
    	 ByteBuffer ring_buffer = thread_cache_obj.getValue();
    	 
         if(log_format_content == null)
         {
        	 log_format_content = "null";
         }
         long log_format_content_len = (long)log_format_content.length() << 1;
         long fmt_string_storage_size = align4(4/*sizeof(uint32_t)*/ + log_format_content_len);
    	 
         long data_offset = log_invoker.__api_log_buffer_alloc(target_log.get_id(), (long)_log_head_def_size + fmt_string_storage_size + param_storage_size, (short)target_level.ordinal(), log_category_base.get_index(target_category), log_format_content, log_format_content_len);
         
         if (data_offset < 0)
         {
             return null;
         }
         long_params[log_jni_long_params_enum.data_offset.ordinal()] = data_offset;
         ring_buffer.position((int)((data_offset >> 32) + (data_offset & 0xFFFFFFFF)));
         return thread_cache_obj;
     }

     public void add_param(ByteBuffer ring_buffer, String value)
     {
         //utf-16 String
    	 int position = ring_buffer.position();
    	 if (null == value)
         {
        	 ring_buffer.put((byte)log_arg_type_enum.null_type.ordinal());
        	 ring_buffer.position(position + 4);
         }else {
        	 long str_size = ((long)value.length() << 1);
        	 bq.impl.log_invoker.__api_log_arg_push_utf16_string(parent_log_.get_id(), (long)position, value, str_size);
             ring_buffer.position(position + (int)align4(4 + 4 + str_size));
         }
     }
     
	 public void add_param(ByteBuffer ring_buffer, Map.Entry<int[], long[]> value)
	 {
    	 int position = ring_buffer.position();
    	 int type_info = value.getKey()[0];
    	 ring_buffer.put((byte)type_info);
    	 long pod_value = value.getValue()[0];
    	 long total_size = value.getValue()[1];
    	 //todo use switch
    	 if(total_size <= 4)
    	 {
        	 ring_buffer.position(position + 2);
    		 if(type_info == log_arg_type_enum.bool_type.ordinal()
    				 || type_info == log_arg_type_enum.int8_type.ordinal())
    		 {
    			 ring_buffer.put((byte)pod_value);
    		 }else if(type_info == log_arg_type_enum.char16_type.ordinal())
    		 {
    	    	 ring_buffer.putChar((char)pod_value);
    		 }else if(type_info == log_arg_type_enum.int16_type.ordinal())
    		 {
    	    	 ring_buffer.putShort((short)pod_value);
    		 }else {
    			 throw new RuntimeException("unkown type " + type_info + " with storage size:" + total_size);
    		 }
        	 ring_buffer.position(position + 4);
    	 }else if(total_size == 8)
    	 {
        	 ring_buffer.position(position + 4);
        	 ring_buffer.putInt((int)pod_value);
    	 }else if(total_size == 12)
    	 {
        	 ring_buffer.position(position + 4);
        	 ring_buffer.putLong(pod_value);
    	 }else {
			 throw new RuntimeException("unkown type " + type_info + " with storage size:" + total_size);
		 }
    	 param.return_param_wrapper_to_pool(value);
	 }

     public void add_param(ByteBuffer ring_buffer, Boolean value)
     {
    	 int position = ring_buffer.position();
    	 ring_buffer.put((byte)log_arg_type_enum.bool_type.ordinal());
    	 ring_buffer.position(position + 2);
    	 ring_buffer.put((byte)(value ? 1 : 0));
    	 ring_buffer.position(position + 4);
     }
     public void add_param(ByteBuffer ring_buffer, Character value)
     {
    	 int position = ring_buffer.position();
    	 ring_buffer.put((byte)log_arg_type_enum.char16_type.ordinal());
    	 ring_buffer.position(position + 2);
    	 ring_buffer.putChar(value);
    	 ring_buffer.position(position + 4);
     }

     public void add_param(ByteBuffer ring_buffer, Byte value)
     {
    	 int position = ring_buffer.position();
    	 ring_buffer.put((byte)log_arg_type_enum.int8_type.ordinal());
    	 ring_buffer.position(position + 2);
    	 ring_buffer.put(value);
    	 ring_buffer.position(position + 4);
     }

     public void add_param(ByteBuffer ring_buffer, Short value)
     {
    	 int position = ring_buffer.position();
    	 ring_buffer.put((byte)log_arg_type_enum.int16_type.ordinal());
    	 ring_buffer.position(position + 2);
    	 ring_buffer.putShort(value);
     }

     public void add_param(ByteBuffer ring_buffer, Integer value)
     {
    	 int position = ring_buffer.position();
    	 ring_buffer.put((byte)log_arg_type_enum.int32_type.ordinal());
    	 ring_buffer.position(position + 4);
    	 ring_buffer.putInt(value);
     }

     public void add_param(ByteBuffer ring_buffer, Long value)
     {
    	 int position = ring_buffer.position();
    	 ring_buffer.put((byte)log_arg_type_enum.int64_type.ordinal());
    	 ring_buffer.position(position + 4);
    	 ring_buffer.putLong(value);
     }
     
     public void add_param(ByteBuffer ring_buffer, Float value)
     {
    	 int position = ring_buffer.position();
    	 ring_buffer.put((byte)log_arg_type_enum.float_type.ordinal());
    	 ring_buffer.position(position + 4);
    	 ring_buffer.putFloat(value);
     }
     public void add_param(ByteBuffer ring_buffer, Double value)
     {
    	 int position = ring_buffer.position();
    	 ring_buffer.put((byte)log_arg_type_enum.double_type.ordinal());
    	 ring_buffer.position(position + 4);
    	 ring_buffer.putDouble(value);
     }
     
     public <T> void add_param(ByteBuffer ring_buffer, T value)
     {
    	 int position = ring_buffer.position();
         if (value == null)
         {
        	 ring_buffer.put((byte)log_arg_type_enum.null_type.ordinal());
        	 ring_buffer.position(position + 4);
         }
         else
         {
             add_param(ring_buffer, value.toString());
         }
     }

     @SuppressWarnings("unchecked")
	public void add_param_no_optimized(ByteBuffer ring_buffer, Object value)
     {
         if (value == null)
         {
        	 int position = ring_buffer.position();
        	 ring_buffer.put((byte)log_arg_type_enum.null_type.ordinal());
        	 ring_buffer.position(position + 4);
             return;
         }
         Class<? extends Object> cls = value.getClass();
       //The order of the if-else statements is determined by experience, 
         //with those used more frequently generally placed at the beginning.
         if(cls == constants.cls_param_wrapper)
         {
        	 add_param(ring_buffer, (Map.Entry<int[], long[]>)value);
         }else if(cls == Integer.class)
         {
        	 add_param(ring_buffer, (Integer)value);
         }else if(cls == Float.class)
         {
        	 add_param(ring_buffer, (Float)value);
         }else if(cls == Boolean.class)
         {
        	 add_param(ring_buffer, (Boolean)value);
         }
         else if(cls == String.class)
         {
        	 add_param(ring_buffer, (String)value);
         }else if(cls == Long.class)
         {
        	 add_param(ring_buffer, (Long)value);
         }else if(cls == Double.class)
         {
        	 add_param(ring_buffer, (Double)value);
         }else if(cls == Character.class)
         {
        	 add_param(ring_buffer, (Character)value);
         }else if(cls == Byte.class)
         {
        	 add_param(ring_buffer, (Byte)value);
         }else if(cls == Short.class)
         {
        	 add_param(ring_buffer, (Short)value);
         }else if(cls == StringBuffer.class)
         {
        	 add_param(ring_buffer, ((StringBuffer)value).chars());
         }else
         {
        	 add_param(ring_buffer, value.toString());
         }
     }

     public void end_copy(log target_log, Map.Entry<long[], ByteBuffer> handle)
     {
         log_invoker.__api_log_buffer_commit(target_log.get_id(), handle.getKey()[log_jni_long_params_enum.data_offset.ordinal()]);
     }
}
