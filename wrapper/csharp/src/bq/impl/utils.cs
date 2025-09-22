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
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace bq.impl
{
    public class utf8_encoder
    {
        public unsafe static byte[] get_utf8_array(string s)
        {
            if (s == null)
            {
                byte[] result = new byte[1];
                result[0] = 0;
                return result;
            }
            else
            {
                byte[] result_bytes = System.Text.Encoding.UTF8.GetBytes(s);
                List<byte> result_list = new List<byte>(result_bytes);
                result_list.Add(0);
                return result_list.ToArray();
            }
        }
        public unsafe static byte* alloc_utf8_fixed_str(string s)
        {
            if (s == null)
            {
                System.IntPtr ptr = Marshal.AllocHGlobal(1);
                byte* result = (byte*)ptr.ToPointer();
                result[0] = 0;
                return result;
            }
            else
            {
                byte[] encoded = System.Text.Encoding.UTF8.GetBytes(s);
                System.IntPtr ptr = Marshal.AllocHGlobal(encoded.Length + 1);
                byte* result = (byte*)ptr.ToPointer();
                for (int i = 0; i < encoded.Length; ++i)
                {
                    result[i] = encoded[i];
                }
                result[encoded.Length] = 0;
                return result;
            }
        }

        public unsafe static void release_utf8_fixed_str(byte* s)
        {
            System.IntPtr ptr = new System.IntPtr(s);
            Marshal.FreeHGlobal(ptr);
        }
    }

}

internal class utils
{
    public static uint align4(uint n)
    {
        return n == 0 ? n : (((n - 1) & (~((uint)4 - 1))) + 4);
    }
    public static ulong align4(ulong n)
    {
        return n == 0 ? n : (((n - 1) & (~((ulong)4 - 1))) + 4);
    }


    public static unsafe void memcpy(byte* dest, byte* src, uint len)
    {
        //inspired from .Net Framework System.Buffer.memcpyimpl
        //but the high performance is based on src and dest is at least
        //4 bytes aligned.
        if (len >= 16)
        {
            do
            {
                *(long*)dest = *(long*)src;
                *(long*)(dest + 8) = *(long*)(src + 8);
                dest += 16;
                src += 16;
            }
            while ((len -= 16) >= 16);
        }

        if (len > 0)
        {
            if ((len & 8u) != 0)
            {
                *(long*)dest = *(long*)src;
                dest += 8;
                src += 8;
            }

            if ((len & 4u) != 0)
            {
                *(int*)dest = *(int*)src;
                dest += 4;
                src += 4;
            }

            if ((len & 2u) != 0)
            {
                *(short*)dest = *(short*)src;
                dest += 2;
                src += 2;
            }

            if ((len & (true ? 1u : 0u)) != 0)
            {
                *(dest++) = *(src++);
            }
        }
    }

    /*
     * str_memcpy_high_performance uses "fixed (char*  = string)" statement,
     * which will cause JIT vreg crash on old version mono.
     */
    enum enum_use_high_performance_str_cpy
    {
        pendding,
        can_use,
        can_not_use
    }
    private static enum_use_high_performance_str_cpy can_use_high_performance_str_cpy_ = enum_use_high_performance_str_cpy.pendding;

    private static bool can_use_high_performance_str_cpy
    {
        get
        {
            if (can_use_high_performance_str_cpy_ == enum_use_high_performance_str_cpy.pendding)
            {
                bool is_mono = System.Type.GetType("Mono.Runtime") != null;
                if (is_mono && System.Environment.Version.Major <= 2)
                {
                    can_use_high_performance_str_cpy_ = enum_use_high_performance_str_cpy.can_not_use;
                }
                else
                {
                    can_use_high_performance_str_cpy_ = enum_use_high_performance_str_cpy.can_use;
                }
            }
            return false; // can_use_high_performance_str_cpy_ == enum_use_high_performance_str_cpy.can_use;
        }
    }

    public static unsafe void str_memcpy(byte* dest, string src_str, uint len)
    {
#if ENABLE_IL2CPP
            fixed (char* src_c_style = src_str)
            {
                byte* src = (byte*)src_c_style;
                memcpy(dest, src, len);
            }
#else
        bool can_use_memcpy = can_use_high_performance_str_cpy;
        if (can_use_memcpy)
        {
            str_memcpy_high_performance(dest, src_str, len);
        }
        else
        {
            str_memcpy_low_performance(dest, src_str, len);
        }
#endif
    }

    private static unsafe void str_memcpy_low_performance(byte* dest, string src_str, uint len)
    {
        char* dest_char = (char*)dest;
        uint char_len = len >> 1;
        for (uint i = 0; i < char_len; ++i)
        {
            dest_char[i] = src_str[(int)i];
        }
    }

    private static unsafe void str_memcpy_high_performance(byte* dest, string src_str, uint len)
    {
        fixed (char* src_c_style = src_str)
        {
            byte* src = (byte*)src_c_style;
            memcpy(dest, src, len);
        }
    }


}