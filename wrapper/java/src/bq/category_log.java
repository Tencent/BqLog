package bq;
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
import java.util.List;

/**
 * A log whose entries carry a category, so that appenders can filter on it; the classes the
 * category log generator writes for an application extend this one.
 * @author pippocao
 *
 */
public class category_log extends log{
    /** Creates an empty category log instance. */
    protected category_log()
    {
        super();
    }
    
    /**
     * Creates a category log from another log instance.
     * @param child_inst The source log instance.
     */
    protected category_log(log child_inst)
    {
        super(child_inst);
    }
    
    /**
     * Get log categories count
     * @return The number of categories in this log.
     */
    public long get_categories_count()
    {
        return (long)categories_name_array_.size();
    }

    /**
     * Get names of all categories
     * @return The names of all categories in this log.
     */
    public List<String> get_categories_name_array()
    {
        return categories_name_array_;
    }
}
