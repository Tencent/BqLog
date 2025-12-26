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
#if !UNITY_2017_1_OR_NEWER && !UNITY_5_3_OR_NEWER
        private static bool was_compiling_;
#endif
        private static bq.log.type_console_callback s_console_callback;
        static bq_log_editor_bootstrap()
        {
            // Try to register once on next editor update (after domain reload)
            EditorApplication.delayCall += register_callback;

#if UNITY_2017_1_OR_NEWER
            AssemblyReloadEvents.afterAssemblyReload -= on_after_assembly_reload;
            AssemblyReloadEvents.afterAssemblyReload += on_after_assembly_reload;
            EditorApplication.wantsToQuit += on_quit;
            EditorApplication.playModeStateChanged -= on_play_mode_changed;
            EditorApplication.playModeStateChanged += on_play_mode_changed;
#elif UNITY_5_3_OR_NEWER
            // 5.3+：Use DidReloadScripts
            // delayCall has already been used to register once after domain reload
            EditorApplication.quitting += on_quit;
            #pragma warning disable 618
            EditorApplication.playmodeStateChanged -= on_play_mode_changed_legacy;
            EditorApplication.playmodeStateChanged += on_play_mode_changed_legacy;
            #pragma warning restore 618
#else
            // 5.0–5.2：no hook for domain reload, poll compilation state
            was_compiling_ = EditorApplication.isCompiling;
            EditorApplication.update -= poll_compilation_and_register;
            EditorApplication.update += poll_compilation_and_register;
            EditorApplication.quitting += on_quit;
#endif
        }


#if UNITY_2017_1_OR_NEWER
        private static void on_play_mode_changed(PlayModeStateChange state)
        {
            if (state == PlayModeStateChange.ExitingEditMode ||
                state == PlayModeStateChange.EnteredEditMode)
                bq.log.register_console_callback(null);
            else if (state == PlayModeStateChange.EnteredPlayMode)
            {
                Debug.Log($"[BqLog] Re-register on PlayMode: {state}");
                register_callback();
            }
        }
        private static bool on_quit()
        {
            Debug.Log("[BqLog] Try To Register Console Callback By Assembly on_quit");
            bq.log.register_console_callback(null);
            return true;
        }
        private static void on_after_assembly_reload()
        {
            register_callback();
        }
#elif UNITY_5_3_OR_NEWER
        private static void on_quit(){
            bq.log.register_console_callback(null);
        }
        [UnityEditor.Callbacks.DidReloadScripts]
        private static void on_after_assembly_reload()
        {
            register_callback();
        }
        private static void on_play_mode_changed_legacy()
        {
            //need process for user
            bq.log.register_console_callback(null);
        }
#else
        private static void on_quit(){
            bq.log.register_console_callback(null);
        }
        private static void poll_compilation_and_register()
        {
            bool now = EditorApplication.isCompiling;
            if (was_compiling_ && !now)
            {
                register_callback();
            }
            was_compiling_ = now;
        }
#endif

        private static void register_callback()
        {
            Debug.Log("[BqLog] Try To Register Console Callback By Assembly Reload");
            s_console_callback = new bq.log.type_console_callback(console_callback);
            bq.log.register_console_callback(s_console_callback);//
        }

        private static void console_callback(ulong log_id, int category_idx, bq.def.log_level level, string content)
        {
            switch (level)
            {
                case bq.def.log_level.verbose:
                case bq.def.log_level.debug:
                case bq.def.log_level.info:
                    log_no_stack(LogType.Log, content);
                    break;
                case bq.def.log_level.warning:
                    log_no_stack(LogType.Warning, content);
                    break;
                case bq.def.log_level.error:
                    log_no_stack(LogType.Error, content);
                    break;
                case bq.def.log_level.fatal:
                    log_no_stack(LogType.Exception, content);
                    break;
                default:
                    log_no_stack(LogType.Log, content);
                    break;
            }
        }

        private static void log_no_stack(LogType type, string msg)
        {
#if UNITY_2017_1_OR_NEWER
            Debug.LogFormat(type, LogOption.NoStacktrace, null, "bq {0}", msg);
#else
            switch (type)
            {
                case LogType.Warning:
                    Debug.LogWarning(msg);
                    break;
                case LogType.Error:
                    Debug.LogError(msg);
                    break;
                case LogType.Exception:
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