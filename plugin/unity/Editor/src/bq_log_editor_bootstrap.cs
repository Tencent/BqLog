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
//
//  Redirect log console output to Unity Editor Console in Editor Mode
//  Created by pippocao on 2025/10/10.

#if UNITY_EDITOR
using bq.def;
using bq;
using UnityEditor;
using UnityEngine;

namespace BqLog.Editor
{
    [InitializeOnLoad]
    public static class bq_log_editor_bootstrap
    {
        static bq_log_editor_bootstrap()
        {
            EditorApplication.update -= on_editor_update;
            EditorApplication.update += on_editor_update;
            bq.log.set_console_buffer_enable(true);
        }


        static void on_editor_update()
        {
            while (bq.log.fetch_and_remove_console_buffer((ulong log_id, int category_idx, bq.def.log_level log_level, string content) => {
                log_no_stack(log_level, content);
            }))
            {

            }
        }

        private static void log_no_stack(bq.def.log_level log_level, string msg)
        {
#if UNITY_2017_1_OR_NEWER
            switch (log_level)
            {
                case log_level.verbose:
                    Debug.LogFormat(LogType.Log, LogOption.NoStacktrace, null, "[Bq] {0}", msg);
                    break;
                case log_level.debug:
                    Debug.LogFormat(LogType.Log, LogOption.NoStacktrace, null, "[Bq] {0}", msg);
                    break;
                case log_level.info:
                    Debug.LogFormat(LogType.Log, LogOption.NoStacktrace, null, "[Bq] {0}", msg);
                    break;
                case log_level.warning:
                    Debug.LogFormat(LogType.Warning, LogOption.NoStacktrace, null, "[Bq] {0}", msg);
                    break;
                case log_level.error:
                    Debug.LogFormat(LogType.Error, LogOption.NoStacktrace, null, "[Bq] {0}", msg);
                    break;
                case log_level.fatal:
                    Debug.LogFormat(LogType.Assert, LogOption.NoStacktrace, null, "[Bq] {0}", msg);
                    break;
                default:
                    Debug.Log(msg);
                    break;
            }
#else
            switch (log_level)
            {
                case log_level.verbose:
                    Debug.Log(msg);
                    break;
                case log_level.debug:
                    Debug.Log(msg);
                    break;
                case log_level.info:
                    Debug.Log(msg);
                    break;
                case log_level.warning:
                    Debug.LogWarning(msg);
                    break;
                case log_level.error:
                    Debug.LogError(msg);
                    break;
                case log_level.fatal:
                    Debug.LogError(msg);
                    break;
                default:
                    Debug.Log(msg);
                    break;
            }
#endif
        }
    }
}
#endif