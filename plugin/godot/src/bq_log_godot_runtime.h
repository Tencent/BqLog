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
#pragma once

namespace bq_godot {

    // Special Godot GDExtension workaround: keeps the loaded BqLog core shared library
    // mapped until process exit while allowing BqLogGodot itself to unload normally.
    void retain_bqlog_runtime();

}
