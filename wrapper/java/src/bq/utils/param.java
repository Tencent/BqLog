package bq.utils;
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
import bq.def.*;

import java.util.*;
import java.util.AbstractMap.SimpleEntry;

public class param {
	
	/*
	 * Java does not have implicit type conversions or value-type structs like C#, 
	 * which are used to handle the boxing and unboxing issues that can lead to GC overhead. 
	 * To address this, a pooling technique is employed for the reuse of such objects. 
	 * Given that BqLog may be used within web containers capable of running multiple applications, 
	 * and these applications can be unloaded at any time, the pool stores only standard JDK types 
	 * to avoid potential issues during application unloading.

	 * Usage example:
	 * log.info("log text, param:{}, param:{}", bq.utils.param.no_boxing(5.65f), bq.utils.param.no_boxing(false));
	 */
	private static ThreadLocal<Stack<Map.Entry<int[], long[]>>> pool = new ThreadLocal<>();
	
	
	private static Map.Entry<int[], long[]> get_param_wrapper_from_pool() throws RuntimeException
	{
		Stack<Map.Entry<int[], long[]>> list_local = pool.get();
		if(null == list_local)
		{
			list_local = new Stack<>();
			pool.set(list_local);
		}
		if(list_local.size() > 0)
		{
			Map.Entry<int[], long[]> wrapper_obj = list_local.pop();
			if(wrapper_obj.getKey()[0] != log_arg_type_enum.unsupported_type.ordinal())
			{
				throw new RuntimeException("please don't cache bq log unbox object!");
			}
			return wrapper_obj;
		}
		Map.Entry<int[], long[]> wrapper_obj = create_new_param_wrapper_obj();
		wrapper_obj.getKey()[0] = log_arg_type_enum.unsupported_type.ordinal();
		return wrapper_obj;
	}
	
    
    private static Map.Entry<int[], long[]> create_new_param_wrapper_obj()
    {
    	return new SimpleEntry<int[], long[]>(new int[1], new long[2]);
    }
	
	public static void return_param_wrapper_to_pool(Map.Entry<int[], long[]> wrapper_obj)
	{
		Stack<Map.Entry<int[], long[]>> list_local = pool.get();
		wrapper_obj.getKey()[0] = log_arg_type_enum.unsupported_type.ordinal();	
		list_local.push(wrapper_obj);
	}

	
	public static Map.Entry<int[], long[]> no_boxing(boolean obj)
    {
		Map.Entry<int[], long[]> wrapper_obj = get_param_wrapper_from_pool();
		wrapper_obj.getKey()[0] = log_arg_type_enum.bool_type.ordinal();
		wrapper_obj.getValue()[0] = obj ? 1 : 0;
		wrapper_obj.getValue()[1] = 4;
		return wrapper_obj;
    }

    public static Map.Entry<int[], long[]> no_boxing(char obj)
    {
		Map.Entry<int[], long[]> wrapper_obj = get_param_wrapper_from_pool();
		wrapper_obj.getKey()[0] = log_arg_type_enum.char16_type.ordinal();
		wrapper_obj.getValue()[0] = obj;
		wrapper_obj.getValue()[1] = 4;
		return wrapper_obj;
    }

    public static Map.Entry<int[], long[]> no_boxing(byte obj)
    {
		Map.Entry<int[], long[]> wrapper_obj = get_param_wrapper_from_pool();
		wrapper_obj.getKey()[0] = log_arg_type_enum.int8_type.ordinal();
		wrapper_obj.getValue()[0] = obj;
		wrapper_obj.getValue()[1] = 4;
		return wrapper_obj;
    }
    
    public static Map.Entry<int[], long[]> no_boxing(short obj)
    {
		Map.Entry<int[], long[]> wrapper_obj = get_param_wrapper_from_pool();
		wrapper_obj.getKey()[0] = log_arg_type_enum.int16_type.ordinal();
		wrapper_obj.getValue()[0] = obj;
		wrapper_obj.getValue()[1] = 4;
		return wrapper_obj;
    }
    
    public static Map.Entry<int[], long[]> no_boxing(int obj)
    {
		Map.Entry<int[], long[]> wrapper_obj = get_param_wrapper_from_pool();
		wrapper_obj.getKey()[0] = log_arg_type_enum.int32_type.ordinal();
		wrapper_obj.getValue()[0] = obj;
		wrapper_obj.getValue()[1] = 8;
		return wrapper_obj;
    }
    
    public static Map.Entry<int[], long[]> no_boxing(long obj)
    {
		Map.Entry<int[], long[]> wrapper_obj = get_param_wrapper_from_pool();
		wrapper_obj.getKey()[0] = log_arg_type_enum.int64_type.ordinal();
		wrapper_obj.getValue()[0] = obj;
		wrapper_obj.getValue()[1] = 12;
		return wrapper_obj;
    }
    
    public static Map.Entry<int[], long[]> no_boxing(float obj)
    {
		Map.Entry<int[], long[]> wrapper_obj = get_param_wrapper_from_pool();
		wrapper_obj.getKey()[0] = log_arg_type_enum.float_type.ordinal();
		wrapper_obj.getValue()[0] = Float.floatToRawIntBits(obj);
		wrapper_obj.getValue()[1] = 8;
		return wrapper_obj;
    }
    
    public static Map.Entry<int[], long[]> no_boxing(double obj)
    {
		Map.Entry<int[], long[]> wrapper_obj = get_param_wrapper_from_pool();
		wrapper_obj.getKey()[0] = log_arg_type_enum.double_type.ordinal();
		wrapper_obj.getValue()[0] = Double.doubleToRawLongBits(obj);
		wrapper_obj.getValue()[1] = 12;
		return wrapper_obj;
    }
}
