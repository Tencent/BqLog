/* Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
using System;
using System.Text;
using Godot;

public partial class csharp_interop_test : Node
{
    private static readonly object cb_lock_ = new object();
    private static readonly StringBuilder cb_captured_ = new StringBuilder();

    private static void on_console_callback(ulong log_id, int category_idx, bq.def.log_level level, string content)
    {
        lock (cb_lock_)
        {
            cb_captured_.AppendLine(content);
        }
    }

    public override void _Ready()
    {
        GD.Print("=== BqLog Godot C# + GDScript Shared Singleton Test Start ===");
        int passed = 0;
        int failed = 0;

        void assert_true(bool cond, string msg)
        {
            if (cond) passed++;
            else { failed++; GD.PrintErr("[FAIL] " + msg); }
        }

        // Disable editor console redirect during headless test.
        var gd_helper = ResourceLoader.Load<GDScript>("res://gd_interop_helper.gd").New().AsGodotObject();
        gd_helper.Call("disable_redirect");

        string cfg = @"
            appenders_config.console.type=console
            appenders_config.console.levels=[all]
            log.thread_mode=async
            snapshot.buffer_size=65536
            snapshot.levels=[all]
        ";
        var cs_log = bq.log.create_log("godot_cs_gd_shared", cfg);
        assert_true(cs_log != null && cs_log.is_valid(), "C# create_log should succeed");

        bq.log.set_console_buffer_enable(true);

        cs_log.info("from_csharp: value={}", 12345);
        bool gd_ok = (bool)gd_helper.Call("write_from_gdscript", "godot_cs_gd_shared", 67890);
        assert_true(gd_ok, "GDScript should locate C#-created log by name via shared singleton");

        cs_log.force_flush();
        while (bq.log.fetch_and_remove_console_buffer(on_console_callback))
        {
        }
        bq.log.set_console_buffer_enable(false);

        string captured;
        lock (cb_lock_)
        {
            captured = cb_captured_.ToString();
        }
        assert_true(captured.Contains("from_csharp: value=12345"), "console callback should contain C# log entry");
        assert_true(captured.Contains("from_gdscript: value=67890"), "console callback should contain GDScript log entry");

        string snap = cs_log.take_snapshot("UTC+0");
        assert_true(snap.Contains("from_csharp: value=12345"), "snapshot should contain C# log entry");
        assert_true(snap.Contains("from_gdscript: value=67890"), "snapshot should contain GDScript log entry");

        bq.log.force_flush_all_logs();
        GD.Print($"=== BqLog Godot C# + GDScript Test Done: passed={passed}, failed={failed} ===");
        GetTree().Quit(failed == 0 ? 0 : 1);
    }
}
