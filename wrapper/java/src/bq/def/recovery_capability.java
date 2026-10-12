package bq.def;
/*
 * Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */

/**
 * What this platform offers for crash recovery, and what is missing when it is not available:
 * recovery needs a memory map and a lock that keeps the other processes out.
 */
public enum recovery_capability {
    ready,
    no_memory_map,
    no_instance_lock,
}
