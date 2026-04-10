#pragma once

#include "Token.h"

namespace ZOLP
{
    using std::shared_ptr;
    using std::vector;

    struct AST_
    {
        enum class State_
        {
            EMPTY,
            ATOM,
            UNARY,
            BINARY,
            FUNCTION
        } state_;
        char bra_;  // 0 if not parenthesized
        Token_ head_;
        AST_* parent_;
        vector<shared_ptr<AST_>> children_;

        AST_() : head_(NONE_TOKEN), parent_(nullptr), bra_(0), state_(State_::EMPTY) {}
    };
}  // leave ZOLP

