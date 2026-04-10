#pragma once

#include <map>
#include "Token.h"

namespace ZOLP
{
    using std::map;
	using std::string;

    struct Precedence_
    {
        map<char, int> u1_, l1_, r1_;
        map<string, int> u2_, l2_, r2_;

        Precedence_(const OperatorList_& ops)
        {
            int score = 0;
            for (const auto& [a, v] : ops.vals_)
            {
                for (const Token_& t : v)
                {
                    if (a == OperatorList_::Assoc_::UNARY)
                        u1_[t.kind_] = u2_[t.val_] = score;
                    else
                    {
                        l1_[t.kind_] = l2_[t.val_] = score + (a == OperatorList_::Assoc_::LEFT ? -1 : 1);
                        r1_[t.kind_] = r2_[t.val_] = score - (a == OperatorList_::Assoc_::LEFT ? -1 : 1);
                    }
                }
                score += 10;
            }
        }

        bool SplitLeft(const Token_& left, const Token_& right) const
        {
            if (auto pl = l1_.find(left.kind_), pr = r1_.find(right.kind_); pl != l1_.end() && pr != r1_.end())
                return pl->second > pr->second;
            REQUIRE(false, "precedence only supports binary-binary comparison");
        }
    };
}  // leave ZOLP
