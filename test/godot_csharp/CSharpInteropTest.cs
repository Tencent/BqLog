/* Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 */
using System;
using Godot;

public partial class CSharpInteropTest : Node
{
    public override void _Ready()
    {
        GD.Print("=== BqLog Godot C# + GDScript Shared Singleton Test Start ===");
        int passed = 0;
        int failed = 0;

        void AssertTrue(bool cond, string msg)
        {
            if (cond) passed++;
            else { failed++; GD.PrintErr("[FAIL] " + msg); }
        }

        // Disable editor console redirect during headless test.
        var gdHelper = ResourceLoader.Load<GDScript>("res://gd_interop_helper.gd").New();
        gdHelper.Call("disable_redirect");

        string cfg = @"
            appenders_config.console.type=console
            appenders_config.console.levels=[all]
            log.thread_mode=async
            snapshot.buffer_size=65536
            snapshot.levels=[all]
        ";
        var csLog = bq.log.create_log("godot_cs_gd_shared", cfg);
        AssertTrue(csLog != null && csLog.is_valid(), "C# create_log should succeed");

        csLog.info("from_csharp: value={}", 12345);
        bool gdOk = (bool)gdHelper.Call("write_from_gdscript", "godot_cs_gd_shared", 67890);
        AssertTrue(gdOk, "GDScript should locate C#-created log by name via shared singleton");

        csLog.force_flush();
        string snap = csLog.take_snapshot("UTC+0");
        AssertTrue(snap.Contains("from_csharp: value=12345"), "snapshot should contain C# log entry");
        AssertTrue(snap.Contains("from_gdscript: value=67890"), "snapshot should contain GDScript log entry");

        bq.log.force_flush_all_logs();
        GD.Print($"=== BqLog Godot C# + GDScript Test Done: passed={passed}, failed={failed} ===");
        GetTree().Quit(failed == 0 ? 0 : 1);
    }
}
