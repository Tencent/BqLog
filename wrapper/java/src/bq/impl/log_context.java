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


/**
 * Packs one log entry: it first measures how much room the arguments need, then writes them into
 * the buffer the library reserved. One instance is kept per log object and reused.
 */
public class log_context {
     private static long align4(long n)
     {
         return n == 0 ? n : (((n - 1) & (~((long)4 - 1))) + 4);
     }
 	 
 	 
 	 /** Created once per log object. */
 	 public log_context()
 	 {
 	 }
     
     /**
      * Storage size of a parameter bq.utils.param has already packed.
      * @param obj The packed parameter.
      * @return Its size in the argument area, in bytes.
      */
     public long get_param_storage_size(Map.Entry<int[], long[]> obj)
     {
    	 return obj.getValue()[1];
     }

     /**
      * Storage size of a boolean.
      * @param obj The value to log.
      * @return Four bytes.
      */
     public long get_param_storage_size(Boolean obj)
     {
         return 4;
     }
     
     /**
      * Storage size of a string, which packs as UTF-16 and is padded to four bytes.
      * @param obj The value to log.
      * @return Its size in the argument area, in bytes.
      */
     public long get_param_storage_size(String obj)
     {
         if (null == obj)
         {
             return 4;
         }
         //utf-16 String
         return align4(4/*sizeof typeinfo*/ + 4/*sizeof(uint32_t)*/ + ((long)obj.length() << 1));
     }

     /**
      * Storage size of a char.
      * @param obj The value to log.
      * @return Four bytes.
      */
     public long get_param_storage_size(Character obj)
     {
         return 4;
     }

     /**
      * Storage size of a byte.
      * @param obj The value to log.
      * @return Four bytes.
      */
     public long get_param_storage_size(Byte obj)
     {
         return 4;
     }
     /**
      * Storage size of a short.
      * @param obj The value to log.
      * @return Four bytes.
      */
     public long get_param_storage_size(Short obj)
     {
         return 4;
     }
     /**
      * Storage size of an int.
      * @param obj The value to log.
      * @return Eight bytes.
      */
     public long get_param_storage_size(Integer obj)
     {
         return 8;
     }
     /**
      * Storage size of a long.
      * @param obj The value to log.
      * @return Twelve bytes.
      */
     public long get_param_storage_size(Long obj)
     {
         return 12;
     }
     /**
      * Storage size of a float.
      * @param obj The value to log.
      * @return Eight bytes.
      */
     public long get_param_storage_size(Float obj)
     {
         return 8;
     }
     /**
      * Storage size of a double.
      * @param obj The value to log.
      * @return Twelve bytes.
      */
     public long get_param_storage_size(Double obj)
     {
         return 12;
     }

     /**
      * Storage size of any other object, which packs as its toString().
      * @param <T> The type of the value.
      * @param obj The value to log.
      * @return Its size in the argument area, in bytes.
      */
     public <T> long get_param_storage_size(T obj)
     {
         if (null == obj)
         {
             return 4;
         }
         return get_param_storage_size(obj.toString());
     }

	/**
	 * Storage size of a value whose type is only known at run time.
	 * @param obj The value to log.
	 * @return Its size in the argument area, in bytes.
	 */
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
     /**
      * Reserves the room one entry needs and returns the buffer to pack it into.
      * @param target_log The log to write to.
      * @param target_category Category of the entry.
      * @param target_level Level of the entry.
      * @param log_format_content The format string.
      * @param param_storage_size Room the arguments need, or 0 when the entry has no arguments.
      * @return The buffer to pack into, or null when the library dropped the entry.
      */
     public ByteBuffer begin_copy(log target_log, log_category_base target_category, log_level target_level, String log_format_content, long param_storage_size)
     {
         if(log_format_content == null)
         {
        	 log_format_content = "null";
         }
         long log_format_content_len = (long)log_format_content.length() << 1;
         ByteBuffer[] result = log_invoker.__api_log_write_begin(target_log.get_id(), (byte)target_level.ordinal(), log_category_base.get_index(target_category), log_format_content_len , log_format_content, param_storage_size, param_storage_size == 0);
         
         if (null != result)
         {
        	 result[1].position(0);
        	 result[0].position(result[1].getInt());
        	 return result[0];
         }
         return null;
     }

     /**
      * Writes a string, or a null, into the argument area.
      * @param log_buffer Buffer begin_copy() returned.
      * @param value The value to write.
      */
     public void add_param(ByteBuffer log_buffer, String value)
     {
         //utf-16 String
    	 if (null == value)
         {
    		 log_buffer.putInt(log_arg_type_enum.null_type.ordinal()); //Works in Little-Endian
         }else {
        	 long str_size = ((long)value.length() << 1);
        	 log_buffer.putInt(log_arg_type_enum.string_utf16_type.ordinal());  //Works in Little-Endian
        	 log_buffer.putInt((int)str_size);
        	 int pos_prev = log_buffer.position();
        	 //Believe JIT optimization
        	 for(int i = 0; i < value.length(); ++i) {
        		 log_buffer.putChar(value.charAt(i));
        	 }
        	 log_buffer.position(pos_prev + (int)align4(str_size));
         }
     }
     
	 /**
	  * Writes a parameter bq.utils.param packed, and gives its wrapper back to the pool.
	  * @param log_buffer Buffer begin_copy() returned.
	  * @param value The packed value to write.
	  */
	 public void add_param(ByteBuffer log_buffer, Map.Entry<int[], long[]> value)
	 {
    	 int position = log_buffer.position();
    	 int type_info = value.getKey()[0];
    	 log_buffer.put((byte)type_info);
    	 long pod_value = value.getValue()[0];
    	 long total_size = value.getValue()[1];
    	 //todo use switch
    	 if(total_size <= 4)
    	 {
        	 log_buffer.position(position + 2);
    		 if(type_info == log_arg_type_enum.bool_type.ordinal()
    				 || type_info == log_arg_type_enum.int8_type.ordinal())
    		 {
    			 log_buffer.put((byte)pod_value);
    		 }else if(type_info == log_arg_type_enum.char16_type.ordinal())
    		 {
    	    	 log_buffer.putChar((char)pod_value);
    		 }else if(type_info == log_arg_type_enum.int16_type.ordinal())
    		 {
    	    	 log_buffer.putShort((short)pod_value);
    		 }else {
    			 throw new RuntimeException("unkown type " + type_info + " with storage size:" + total_size);
    		 }
        	 log_buffer.position(position + 4);
    	 }else if(total_size == 8)
    	 {
        	 log_buffer.position(position + 4);
        	 log_buffer.putInt((int)pod_value);
    	 }else if(total_size == 12)
    	 {
        	 log_buffer.position(position + 4);
        	 log_buffer.putLong(pod_value);
    	 }else {
			 throw new RuntimeException("unkown type " + type_info + " with storage size:" + total_size);
		 }
    	 param.return_param_wrapper_to_pool(value);
	 }

     /**
      * Writes a boolean into the argument area.
      * @param log_buffer Buffer begin_copy() returned.
      * @param value The value to write.
      */
     public void add_param(ByteBuffer log_buffer, Boolean value)
     {
    	 log_buffer.putShort((short)log_arg_type_enum.bool_type.ordinal());
    	 log_buffer.putShort((short)(value ? 1 : 0));
     }
     /**
      * Writes a char into the argument area.
      * @param log_buffer Buffer begin_copy() returned.
      * @param value The value to write.
      */
     public void add_param(ByteBuffer log_buffer, Character value)
     {
    	 log_buffer.putShort((short)log_arg_type_enum.char16_type.ordinal());
    	 log_buffer.putShort((short)(int)value);
     }

     /**
      * Writes a byte into the argument area.
      * @param log_buffer Buffer begin_copy() returned.
      * @param value The value to write.
      */
     public void add_param(ByteBuffer log_buffer, Byte value)
     {
    	 log_buffer.putShort((short)log_arg_type_enum.int8_type.ordinal());
    	 log_buffer.putShort((short)value);
     }

     /**
      * Writes a short into the argument area.
      * @param log_buffer Buffer begin_copy() returned.
      * @param value The value to write.
      */
     public void add_param(ByteBuffer log_buffer, Short value)
     {
    	 log_buffer.putShort((short)log_arg_type_enum.int16_type.ordinal());
    	 log_buffer.putShort(value);
     }

     /**
      * Writes an int into the argument area.
      * @param log_buffer Buffer begin_copy() returned.
      * @param value The value to write.
      */
     public void add_param(ByteBuffer log_buffer, Integer value)
     {
    	 log_buffer.putInt(log_arg_type_enum.int32_type.ordinal());
    	 log_buffer.putInt(value);
     }

     /**
      * Writes a long into the argument area.
      * @param log_buffer Buffer begin_copy() returned.
      * @param value The value to write.
      */
     public void add_param(ByteBuffer log_buffer, Long value)
     {
    	 log_buffer.putInt(log_arg_type_enum.int64_type.ordinal());
    	 log_buffer.putLong(value);
     }
     
     /**
      * Writes a float into the argument area.
      * @param log_buffer Buffer begin_copy() returned.
      * @param value The value to write.
      */
     public void add_param(ByteBuffer log_buffer, Float value)
     {
    	 log_buffer.putInt(log_arg_type_enum.float_type.ordinal());
    	 log_buffer.putFloat(value);
     }
     /**
      * Writes a double into the argument area.
      * @param log_buffer Buffer begin_copy() returned.
      * @param value The value to write.
      */
     public void add_param(ByteBuffer log_buffer, Double value)
     {
    	 log_buffer.putInt(log_arg_type_enum.double_type.ordinal());
    	 log_buffer.putDouble(value);
     }
     
     /**
      * Writes any other object as its toString().
      * @param <T> The type of the value.
      * @param log_buffer Buffer begin_copy() returned.
      * @param value The value to write.
      */
     public <T> void add_param(ByteBuffer log_buffer, T value)
     {
         if (value == null)
         {
        	 log_buffer.putInt(log_arg_type_enum.null_type.ordinal());
         }
         else
         {
             add_param(log_buffer, value.toString());
         }
     }

	/**
	 * Writes a value whose type is only known at run time.
	 * @param log_buffer Buffer begin_copy() returned.
	 * @param value The value to write.
	 */
     @SuppressWarnings("unchecked")
	public void add_param_no_optimized(ByteBuffer log_buffer, Object value)
     {
         if (value == null)
         {
        	 log_buffer.putInt(log_arg_type_enum.null_type.ordinal());
             return;
         }
         Class<? extends Object> cls = value.getClass();
       //The order of the if-else statements is determined by experience, 
         //with those used more frequently generally placed at the beginning.
         if(cls == constants.cls_param_wrapper)
         {
        	 add_param(log_buffer, (Map.Entry<int[], long[]>)value);
         }else if(cls == Integer.class)
         {
        	 add_param(log_buffer, (Integer)value);
         }else if(cls == Float.class)
         {
        	 add_param(log_buffer, (Float)value);
         }else if(cls == Boolean.class)
         {
        	 add_param(log_buffer, (Boolean)value);
         }
         else if(cls == String.class)
         {
        	 add_param(log_buffer, (String)value);
         }else if(cls == Long.class)
         {
        	 add_param(log_buffer, (Long)value);
         }else if(cls == Double.class)
         {
        	 add_param(log_buffer, (Double)value);
         }else if(cls == Character.class)
         {
        	 add_param(log_buffer, (Character)value);
         }else if(cls == Byte.class)
         {
        	 add_param(log_buffer, (Byte)value);
         }else if(cls == Short.class)
         {
        	 add_param(log_buffer, (Short)value);
         }else if(cls == StringBuffer.class)
         {
        	 add_param(log_buffer, ((StringBuffer)value).toString());
         }else
         {
        	 add_param(log_buffer, value.toString());
         }
     }

     /**
      * Tells the library the entry is complete, so it can be published.
      * @param target_log The log the entry was written to.
      */
     public void end_copy(log target_log)
     {
         log_invoker.__api_log_write_finish(target_log.get_id());
     }
}
