#pragma once

#include <iostream>
#include "test_base.h"
#include "bq_common/bq_common.h"

namespace bq {
    namespace test {
        class test_property : public test_base {
        private:
            uint32_t result_idx = 0;
            test_result result;
            void add_test_result(bool check_result)
            {
                result.add_result(check_result, "property_test %d", ++result_idx);
            }

        public:
            virtual test_result test() override
            {
                property properties;
                bq::string json_text;
                string context = R"(
						#this is commentary
						empty=
						==2
						special\=t1=su=
						special\:t2:su=
						special\:t3=su:
						!this is commentary
						hello=1.1 
						world=2 
						name=abc
						json_style:json......
						open=true
						object.f1=a
						object.f2=b
						deep.obj.notice=c=0+6=1+5=2+4=3+3
						deep.obj.d2=1.0
						configs=[name,object,deep]
						newtype=I want to go to the Park!\n\
								I will fly a kite!\n\
								\
								bybyby! 
				)";
                file_manager::write_all_text(TO_ABSOLUTE_PATH("property.properties", false), context);
                bool success = properties.load("property.properties", false);
                string serialize = properties.serialize();
                add_test_result(success);
                add_test_result(properties.get("hello") == "1.1");

                property_value pv = property_value::create_from_string(serialize);
                add_test_result(pv.object_size() == 13);
                add_test_result(pv.has_object_key("object"));
                add_test_result(pv.has_object_key("deep"));

                string a = (string)(pv["object"]["f1"]);
                int64_t d2 = (int64_t)(pv["deep"]["obj"]["d2"]);
                add_test_result(a == "a");
                add_test_result(d2 == 1);

                string wpv = pv.serialize();
                file_manager::write_all_text(TO_ABSOLUTE_PATH("property.properties", false), wpv);
                auto context2 = file_manager::read_all_text(TO_ABSOLUTE_PATH("property.properties", false));
                pv = property_value::create_from_string(context2);

                add_test_result(pv.object_size() == 13);
                add_test_result(pv.has_object_key("object"));
                add_test_result(pv.has_object_key("deep"));

                a = (string)(pv["object"]["f1"]);
                d2 = (int64_t)(pv["deep"]["obj"]["d2"]);
                add_test_result(a == "a");
                add_test_result(d2 == 1);

                pv["configs"][6] = "nice";
                add_test_result((string)pv["configs"][0] == "name");
                add_test_result((string)pv["configs"][1] == "object");
                add_test_result(pv["configs"][5].is_null());

                add_test_result(pv["configs"].is_array());
                add_test_result(pv["configs"].array_size() == 7);

                return result;
            }
        };
    }
}
