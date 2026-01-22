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
// Define an interface for the bq library shape to satisfy type checking
// without importing the value directly.
import type { bq as BqType } from "../../../../../wrapper/typescript/src/index";

export let bq: typeof BqType;

export function set_bq_lib(lib: typeof BqType) {
    bq = lib;
}
