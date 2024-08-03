#pragma once
#include "test_base.h"
#include "bq_common/bq_common.h"

#ifdef BQ_WIN
#pragma warning(push)
#pragma warning(disable : 4324)
#endif

namespace bq {
    class opt_cell {
    public:
        opt_cell() = default;
        opt_cell(int32_t value)
            : _value(value)
        {
        }
        ~opt_cell() { }
        int32_t _value = 0;
        int32_t _values[10];
    };

    class AA {
    public:
        AA() = default;
        AA(const AA& aa)
        {
            (void)aa;
            opt = "AA(const AA& aa)";
        }
        AA(AA&& aa)
        {
            (void)aa;
            opt = "AA(AA&& aa)";
        }
        virtual ~AA() = default;
        bq::string opt = "";
    };

    class BB : public AA {
    };

    namespace test {

        class test_optional : public test_base {
        public:
            virtual test_result test() override
            {
                test_result result;

                optional<int32_t> m = 2;
                m.value();
                optional<int32_t> n = m;
                optional<int32_t> x = n.value();
                optional<int32_t> z = move(m);
                for (int32_t i = 0; i < 2; i++) {
                    optional<opt_cell> objx;
                    int32_t s = sizeof(objx);
                    opt_cell objy = objx.value_or(s);
                }
                string s1 = "";
                optional<string> str(s1);

                bq::optional<int> m1 = 2;
                bq::optional<int> m2 = move(m1);
                bq::optional<int> m3 = m2.value();
                bq::optional<long> m4 = m3;
                m4 = m3;

                AA aa1;
                auto aa2 = move(aa1);
                bq::optional<AA> o1 = AA();
                bq::optional<AA> o2 = move(o1);

                result.add_result(o2.value().opt == "AA(AA&& aa)", "not enter constructional of move");
                bq::optional<AA> o3 = o2;
                result.add_result(o3.value().opt == "AA(const AA& aa)", "not enter constructional of copy");
                o3 = nullopt;
                result.add_result(!o3, "not release by nullopt");
                bq::optional<AA> o4 = nullopt;

                bq::optional<BB*> o5 = bq::nullopt;
                bq::optional<AA*> o6;
                o6 = bq::nullopt;
                o6 = o5;
                return result;
            }
        };
    }
}
#ifdef BQ_WIN
#pragma warning(pop)
#endif
