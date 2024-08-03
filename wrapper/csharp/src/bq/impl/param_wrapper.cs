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
/********************************************************************
	created:	2023/05/29
	created:	29:5:2023   11:25
	filename: 	BqLog\wrapper\csharp\lib\bq\impl\param_wrapper.cs
	file path:	BqLog\wrapper\csharp\lib\bq\impl
	file base:	param_wrapper
	file ext:	cs
	author:		Author
	
	purpose:	Bq logging system is focused on high-performance implementation, and GC memory allocation is unacceptable. 
				use param_wrapper as log parameter type instead of object or generic types, can resolve the GC issue caused by auto-boxing.
*********************************************************************/
using System;
using bq.def;
namespace bq.impl
{
    public struct param_wrapper
    {
		private log_arg_type_enum type_;
        private long pod_value_;
        private string string_value_;
        private uint storage_size_;
        private uint aligned_size_;

        public uint storage_size
        {
            get { return storage_size_; }
        }
        public uint aligned_size
        {
            get { return aligned_size_; }
        }

        public string string_value
        {
            get { return string_value_; }
        }

        public unsafe void write(byte* addr)
        {
            *addr = (byte)type_;
            fixed (long* value_ptr = &pod_value_)
            {
                switch (type_)
                {
                    case log_arg_type_enum.bool_type:
                        addr += 2;
                        *(Boolean*)(addr) = (pod_value_ != 0);
                        break;
                    case log_arg_type_enum.char16_type:
                        addr += 2;
                        *(Char*)(addr) = (Char)pod_value_;
                        break;
                    case log_arg_type_enum.int8_type:
                        addr += 2;
                        *(SByte*)(addr) = (SByte)pod_value_;
                        break;
                    case log_arg_type_enum.uint8_type:
                        addr += 2;
                        *(Byte*)(addr) = (Byte)pod_value_;
                        break;
                    case log_arg_type_enum.int16_type:
                        addr += 2;
                        *(Int16*)(addr) = (Int16)pod_value_;
                        break;
                    case log_arg_type_enum.uint16_type:
                        addr += 2;
                        *(UInt16*)(addr) = (UInt16)pod_value_;
                        break;
                    case log_arg_type_enum.int32_type:
                        addr += 4;
                        *(Int32*)(addr) = (Int32)pod_value_;
                        break;
                    case log_arg_type_enum.uint32_type:
                        addr += 4;
                        *(UInt32*)(addr) = (UInt32)pod_value_;
                        break;
                    case log_arg_type_enum.int64_type:
                        addr += 4;
                        *(Int64*)(addr) = (Int64)pod_value_;
                        break;
                    case log_arg_type_enum.uint64_type:
                        addr += 4;
                        *(UInt64*)(addr) = (UInt64)pod_value_;
                        break;
                    case log_arg_type_enum.float_type:
                        addr += 4;
                        *(float*)(addr) = *((float*)value_ptr);
                        break;
                    case log_arg_type_enum.double_type:
                        addr += 4;
                        *(Double*)(addr) = *((Double*)value_ptr);
                        break;
                    case log_arg_type_enum.null_type:
                        break;
                    case log_arg_type_enum.string_utf16_type:
                        addr += 4;
                        UInt32 str_size = storage_size_ - 4 - sizeof(UInt32);
                        *(UInt32*)(addr) = str_size;
                        addr += 4;
                        if(string_value_ != null)
                        {
                            utils.str_memcpy(addr, string_value_, str_size);
                        }
                        break;
                    default:
                        bq.log.console(log_level.error, "Unknown parameter type:" + type_.ToString());
                        break;

                }
            }
        }

        public static implicit operator param_wrapper(bool obj)
        {
            param_wrapper param = new param_wrapper();
            param.type_ = log_arg_type_enum.bool_type;
            param.pod_value_ = obj ? 1 : 0;
            param.storage_size_ = 4;
            param.aligned_size_ = param.storage_size_;
            return param;
        }

        public static implicit operator param_wrapper(string obj)
        {
            param_wrapper param = new param_wrapper();
            param.string_value_ = obj;
            if (obj != null)
            {
                param.type_ = log_arg_type_enum.string_utf16_type;
                param.storage_size_ = 4 + sizeof(UInt32);//typeinfo + length;
                param.storage_size_ += ((uint)obj.Length << 1);
                param.aligned_size_ = utils.align4(param.storage_size_);
            }
            else
            {
                param.type_ = log_arg_type_enum.null_type;
                param.storage_size_ = 4;
                param.aligned_size_ = param.storage_size_;
            }
            return param;
        }

        public static implicit operator param_wrapper(char obj)
        {
            param_wrapper param = new param_wrapper();
            param.type_ = log_arg_type_enum.char16_type;
            param.pod_value_ = obj;
            param.storage_size_ = 4;
            param.aligned_size_ = param.storage_size_;
            return param;
        }

        public static implicit operator param_wrapper(sbyte obj)
        {
            param_wrapper param = new param_wrapper();
            param.type_ = log_arg_type_enum.int8_type;
            param.pod_value_ = obj;
            param.storage_size_ = 4;
            param.aligned_size_ = param.storage_size_;
            return param;
        }

        public static implicit operator param_wrapper(byte obj)
        {
            param_wrapper param = new param_wrapper();
            param.type_ = log_arg_type_enum.uint8_type;
            param.pod_value_ = obj;
            param.storage_size_ = 4;
            param.aligned_size_ = param.storage_size_;
            return param;
        }

        public static implicit operator param_wrapper(short obj)
        {
            param_wrapper param = new param_wrapper();
            param.type_ = log_arg_type_enum.int16_type;
            param.pod_value_ = obj;
            param.storage_size_ = 4;
            param.aligned_size_ = param.storage_size_;
            return param;
        }

        public static implicit operator param_wrapper(ushort obj)
        {
            param_wrapper param = new param_wrapper();
            param.type_ = log_arg_type_enum.uint16_type;
            param.pod_value_ = obj;
            param.storage_size_ = 4;
            param.aligned_size_ = param.storage_size_;
            return param;
        }

        public static implicit operator param_wrapper(int obj)
        {
            param_wrapper param = new param_wrapper();
            param.type_ = log_arg_type_enum.int32_type;
            param.pod_value_ = obj;
            param.storage_size_ = 8;
            param.aligned_size_ = param.storage_size_;
            return param;
        }

        public static implicit operator param_wrapper(uint obj)
        {
            param_wrapper param = new param_wrapper();
            param.type_ = log_arg_type_enum.uint32_type;
            param.pod_value_ = obj;
            param.storage_size_ = 8;
            param.aligned_size_ = param.storage_size_;
            return param;
        }

        public static implicit operator param_wrapper(long obj)
        {
            param_wrapper param = new param_wrapper();
            param.type_ = log_arg_type_enum.int64_type;
            param.pod_value_ = obj;
            param.storage_size_ = 12;
            param.aligned_size_ = param.storage_size_;
            return param;
        }

        public static implicit operator param_wrapper(ulong obj)
        {
            param_wrapper param = new param_wrapper();
            param.type_ = log_arg_type_enum.uint64_type;
            param.pod_value_ = (long)obj;
            param.storage_size_ = 12;
            param.aligned_size_ = param.storage_size_;
            return param;
        }

        public unsafe static implicit operator param_wrapper(float obj)
        {
            param_wrapper param = new param_wrapper();
            param.type_ = log_arg_type_enum.float_type;
            float* pod_ptr = (float*)&param.pod_value_;
            *pod_ptr = obj;
            param.storage_size_ = 8;
            param.aligned_size_ = param.storage_size_;
            return param;
        }

        public unsafe static implicit operator param_wrapper(double obj)
        {
            param_wrapper param = new param_wrapper();
            param.type_ = log_arg_type_enum.double_type;
            double* pod_ptr = (double*)&param.pod_value_;
            *pod_ptr = obj;
            param.storage_size_ = 12;
            param.aligned_size_ = param.storage_size_;
            return param;
        }

        public unsafe static implicit operator param_wrapper(System.Enum obj)
        {
            param_wrapper param = new param_wrapper();
            param.type_ = log_arg_type_enum.int32_type;
            int* pod_ptr = (int*)&param.pod_value_;
            *pod_ptr = obj.GetHashCode();
            param.storage_size_ = 8;
            param.aligned_size_ = param.storage_size_;
            return param;
        }
    }
}