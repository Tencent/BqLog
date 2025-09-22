package bq;
/*
 * Copyright (C) 2024 Tencent.
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
 * @author pippocao
 *
 */
public class category_log extends log{
    protected category_log()
    {
        super();
    }
    
    protected category_log(log child_inst)
    {
        super(child_inst);
    }
    
    /**
     * Get log categories count
     * @return
     */
    public long get_categories_count()
    {
        return (long)categories_name_array_.size();
    }

    /**
     * Get names of all categories
     * @return
     */
    public List<String> get_categories_name_array()
    {
        return categories_name_array_;
    }
}
