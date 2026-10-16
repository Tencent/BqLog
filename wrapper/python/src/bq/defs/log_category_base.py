# Copyright (C) 2025 Tencent.
# BQLOG is licensed under the Apache License, Version 2.0.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.

"""
log_category_base for BqLog category log support.
"""


class log_category_base:
    """Base class for log category items. Each subclass represents a category."""

    def __init__(self, index):
        self._index = index

    @property
    def index(self):
        """The index this category was created with."""
        return self._index
