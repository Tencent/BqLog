# Copyright (C) 2026 Tencent.
# BQLOG is licensed under the Apache License, Version 2.0.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.

"""
recovery_capability enumeration for BqLog.
"""


class recovery_capability:
    """What this platform offers for crash recovery, and what is missing when it is not available."""
    ready = 0
    no_memory_map = 1
    no_instance_lock = 2
