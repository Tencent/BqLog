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

using UnityEditor;
using UnityEngine;
using bq;
using static Codice.CM.WorkspaceServer.WorkspaceTreeDataStore;

namespace BqLog.Editor
{
#if UNITY_2017_1_OR_NEWER
    [InitializeOnLoad]
    public static class bq_log_editor_bootstrap
    {
        static bq_log_editor_bootstrap()
        {
            AssemblyReloadEvents.afterAssemblyReload -= OnAfterAssemblyReload;
            AssemblyReloadEvents.afterAssemblyReload += OnAfterAssemblyReload;
        }

        private static void bq_log_console_callback(ulong log_id, int category_idx, bq.def.log_level log_level, string content)
        {
            switch (log_level)
            {
                case bq.def.log_level.verbose:
                    Debug.LogFormat(LogType.Log, LogOption.NoStacktrace, null, content);
                    break;
                case bq.def.log_level.debug:
                    Debug.LogFormat(LogType.Log, LogOption.NoStacktrace, null, content);
                    break;
                case bq.def.log_level.info:
                    Debug.LogFormat(LogType.Log, LogOption.NoStacktrace, null, content);
                    break;
                case bq.def.log_level.warning:
                    Debug.LogFormat(LogType.Warning, LogOption.NoStacktrace, null, content);
                    break;
                case bq.def.log_level.error:
                    Debug.LogFormat(LogType.Error, LogOption.NoStacktrace, null, content);
                    break;
                case bq.def.log_level.fatal:
                    Debug.LogFormat(LogType.Exception, LogOption.NoStacktrace, null, content);
                    break;
            }
        }

        private static void OnAfterAssemblyReload()
        {
            Debug.LogFormat(LogType.Log, LogOption.NoStacktrace, null, "[BqLog]Register console callback by Assembly Reload");
            bq.log.register_console_callback(new log.type_console_callback(bq_log_console_callback));
        }
    }
#endif
}