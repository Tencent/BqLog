package bq.def;
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

/** Holds the text a native call wrote, so the JNI layer can hand a String back. */
public class string_holder {
	/** Constructor; the decoder fills in the text it decoded. */
	public string_holder() {
	}
	/** The text the last native call produced. */
	public String value;
}
