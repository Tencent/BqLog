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
import { test_result } from "./test_result";

export abstract class test_base {
    private name: string;
    constructor(name: string) {
        this.name = name;
    }
    public get_name(): string { return this.name; }
    public abstract test(): Promise<test_result>;
}
