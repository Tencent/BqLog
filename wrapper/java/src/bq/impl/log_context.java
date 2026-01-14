package bq.impl;
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

/*
 * Author : pippocao
 */

import bq.log;
import bq.def.*;
import bq.utils.param;

import java.nio.ByteBuffer;
import java.util.Map;


public class log_context {
     private static long align4(long n)
     {
         return n == 0 ? n : (((n - 1) & (~((long)4 - 1))) + 4);
     }
 	 
 	 
 	 public log_context()
 	 {
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
        	 return get_param_storage_size(((StringBuffer)obj).toString());
         }else
         {
        	 return get_param_storage_size(obj.toString());
         }
     }


     //string in Java and C# is immutable, it is thread safe, length() is always equal to real string size.
     //we don't need save string length to avoid crash like c++ wrapper.
     public ByteBuffer begin_copy(log target_log, log_category_base target_category, log_level target_level, String log_format_content, long param_storage_size)
     {
         if(log_format_content == null)
         {
        	 log_format_content = "null";
         }
         long log_format_content_len = (long)log_format_content.length() << 1;
         long fmt_string_storage_size = align4(log_format_content_len);
    	 
         ByteBuffer[] result = log_invoker.__api_log_buffer_alloc(target_log.get_id(), fmt_string_storage_size + param_storage_size, (short)target_level.ordinal(), log_category_base.get_index(target_category), log_format_content, log_format_content_len);
         
         if (null != result)
         {
        	 result[1].position(0);
        	 result[0].position(result[1].getInt());
        	 return result[0];
         }
         return null;
     }

     public void add_param(ByteBuffer ring_buffer, String value)
     {
         //utf-16 String
    	 if (null == value)
         {
    		 ring_buffer.putInt(log_arg_type_enum.null_type.ordinal()); //Works in Little-Endian
         }else {
        	 long str_size = ((long)value.length() << 1);
        	 ring_buffer.putInt(log_arg_type_enum.string_utf16_type.ordinal());  //Works in Little-Endian
        	 ring_buffer.putInt((int)str_size);
        	 ring_buffer.asCharBuffer().put(value);
        	 ring_buffer.position(ring_buffer.position() + (int)align4(str_size));
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
    	 ring_buffer.putShort((short)log_arg_type_enum.bool_type.ordinal());
    	 ring_buffer.putShort((short)(value ? 1 : 0));
     }
     public void add_param(ByteBuffer ring_buffer, Character value)
     {
    	 ring_buffer.putShort((short)log_arg_type_enum.char16_type.ordinal());
    	 ring_buffer.putShort((short)(int)value);
     }

     public void add_param(ByteBuffer ring_buffer, Byte value)
     {
    	 ring_buffer.putShort((short)log_arg_type_enum.int8_type.ordinal());
    	 ring_buffer.putShort((short)value);
     }

     public void add_param(ByteBuffer ring_buffer, Short value)
     {
    	 ring_buffer.putShort((short)log_arg_type_enum.int16_type.ordinal());
    	 ring_buffer.putShort(value);
     }

     public void add_param(ByteBuffer ring_buffer, Integer value)
     {
    	 ring_buffer.putInt(log_arg_type_enum.int32_type.ordinal());
    	 ring_buffer.putInt(value);
     }

     public void add_param(ByteBuffer ring_buffer, Long value)
     {
    	 ring_buffer.putInt(log_arg_type_enum.int64_type.ordinal());
    	 ring_buffer.putLong(value);
     }
     
     public void add_param(ByteBuffer ring_buffer, Float value)
     {
    	 ring_buffer.putInt(log_arg_type_enum.float_type.ordinal());
    	 ring_buffer.putFloat(value);
     }
     public void add_param(ByteBuffer ring_buffer, Double value)
     {
    	 ring_buffer.putInt(log_arg_type_enum.double_type.ordinal());
    	 ring_buffer.putDouble(value);
     }
     
     public <T> void add_param(ByteBuffer ring_buffer, T value)
     {
         if (value == null)
         {
        	 ring_buffer.putInt(log_arg_type_enum.null_type.ordinal());
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
        	 ring_buffer.putInt(log_arg_type_enum.null_type.ordinal());
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

     public void end_copy(log target_log)
     {
         log_invoker.__api_log_buffer_commit(target_log.get_id());
     }
}
