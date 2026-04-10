#pragma once

#include <memory>
#include "Token.h"

namespace ZOLP
{
    using std::shared_ptr;

    struct LexerBase_
    {
        virtual ~LexerBase_() {}
    };
    struct OpLexer_ : LexerBase_
    {
        struct RHS_
        {
            Token_ halt_ = NONE_TOKEN;
            shared_ptr<LexerBase_> continue_;
        };
        map<char, RHS_> vals_;

        void Emplace(const Token_& t, int offset = 0)
        {
            RHS_& child = vals_[t.val_[offset]];
            if (t.val_.size() == offset + 1)
                child.halt_ = t;
            else
            {
                if (!child.continue_)
                    child.continue_ = std::make_shared<OpLexer_>();
                static_cast<OpLexer_*>(child.continue_.get())->Emplace(t, offset + 1);
            }
        }

        OpLexer_() {}
        OpLexer_(const OperatorList_& ops)
        {
            for (const auto& [a, v] : ops.vals_)
                for (const auto& t : v)
                    Emplace(t);
        }

        Token_ operator()(const BracketList_& brackets, const char*& p, const char* end) const
        {   // only call for non-alphanumeric
            const auto* layer = &vals_;
            const Token_* candidate = nullptr;
            const char* candidateEnd = nullptr;
            for ( ; ; )
            {
                auto pr = layer->find(*p);
                if (pr == layer->end())
                {
                    REQUIRE(candidate, "no such operator from " + string(1, *p));
                    p = candidateEnd;
                    return *candidate;
                }
                ++p;
                if (pr->second.halt_ != NONE_TOKEN)
                {
                    candidate = &pr->second.halt_;
                    candidateEnd = p;
                }
                if (p == end || IsAlphanumeric(*p) || IsWhite(*p) || brackets.IsBracket(*p) || !pr->second.continue_.get())
                {   // token is over
                    REQUIRE(candidate, "no such operator at " + string(1, *p));
                    p = candidateEnd;
                    return *candidate;
                }
                layer = &static_cast<const OpLexer_*>(pr->second.continue_.get())->vals_;
                // ... and go around again
            }
        }
    };


    struct Lexer_
    {
        OperatorList_ ops_;
        BracketList_ brackets_;
        OpLexer_ opLex_;
        const char* loc_;
        const char* end_;

        Lexer_(const OperatorList_& ops, const BracketList_& brackets)
            : ops_(ops)
            , brackets_(brackets)
            , opLex_(ops)
        {
            // learn to recognize all the tokens
            for (const auto& [_, tt] : ops.vals_)
                for (const Token_& t : tt)
                    opLex_.Emplace(t);
        }

        void Restart(const char* begin, const char* end)
        {
            loc_ = begin;
            end_ = end;
        }

        Token_ Number()
        {
            const char* start = loc_;
            auto ffwd = [&]() { ++loc_;  while (loc_ != end_ && IsNumeric(*loc_)) ++loc_; };
            // consume digits
            ffwd();
            // allowed one decimal point
            if (loc_ != end_ && *loc_ == '.')
                ffwd();
            // allowed one exponent, which could have a sign attached
            if (loc_ != end_ && (*loc_ == 'e' || *loc_ == 'E'))
            {
                if (loc_ + 1 != end_ && (*(loc_ + 1) == '+' || *(loc_ + 1) == '-'))
                    ++loc_;
                ffwd();
            }
            return Token_{ string(start, loc_), NUMERIC_KIND };
        }

        Token_ Next()
        {
            while (loc_ != end_ && IsWhite(*loc_))
                ++loc_;
            if (loc_ == end_ || !*loc_)
                return NONE_TOKEN;
            if (IsAlphanumeric(*loc_))
            {
                if (IsNumeric(*loc_))
                    return Number();
                // some kind of name -- read alphanumerics until we run out
                const char* start = loc_;
                while (loc_ != end_ && IsAlphanumeric(*loc_))
                    ++loc_;
                return Token_(string(start, loc_), ALPHA_KIND);
            }
            if (brackets_.IsBracket(*loc_))
                return Token_(*loc_++);

            // must be an operator
            return opLex_(brackets_, loc_, end_);
        }
    };
}  // leave ZOLP
