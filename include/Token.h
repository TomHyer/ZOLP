#pragma once

#include <string>
#include <optional>
#include <map>
#include <vector>
#include <utility>
#include <set>

#define REQUIRE(cond, msg) if (cond) ; else throw std::runtime_error(msg);

namespace ZOLP
{
    using std::string;
	using std::map;
    using std::optional;
    using std::vector;
    using std::pair;
	using std::set;

    inline bool IsAlphanumeric(char c)
    {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }
    inline bool IsNumeric(char c)
    {
        return c >= '0' && c <= '9';
    }
    inline bool IsWhite(char c)
    {
        return c == ' ' || c == '\t' || c == '\n';
    }

    struct Token_
    {
        char kind_;  // for single-character tokens, kind_==val_; otherwise non-printable
        string val_;

        Token_(char c) : kind_(c), val_(1, c) {}
        Token_(const string& s, char kind) : kind_(kind), val_(s) {}
    };
    const Token_ NONE_TOKEN = { string(), 0};
    constexpr char ALPHA_KIND = 16, NUMERIC_KIND = 17;

    inline bool operator==(const Token_& lhs, const Token_& rhs) { return lhs.val_ == rhs.val_; }
    inline bool operator!=(const Token_& lhs, const Token_& rhs) { return lhs.val_ != rhs.val_; }
    inline bool operator<(const Token_ & lhs, const Token_ & rhs) { return lhs.val_ < rhs.val_; }

    struct Bracket_
    {
        char bra_;
        char ket_;
    };

    struct BracketList_
    {
        map<char, std::optional<Token_>> braToKet_;
        map<char, std::optional<Token_>> ketToBra_;

        BracketList_(const vector<Bracket_>& pairs)
        {
            for (Bracket_ b : pairs)
            {
                braToKet_[b.bra_] = Token_(b.ket_);
                ketToBra_[b.ket_] = Token_(b.bra_);
            }
        }

        bool IsBracket(char c) const { return braToKet_.count(c) || ketToBra_.count(c); }
    };

    struct OperatorList_
    {
        enum class Assoc_ { LEFT, RIGHT, UNARY };  // kinds of associativity
        vector<pair<Assoc_, vector<Token_>>> vals_;

        map<string, char> multi_;
        set<Token_> unary_, binary_;

        Token_ Register(char c, bool binary)
        {
            set<Token_>& existing = binary ? binary_ : unary_;
            Token_ retval(c);
            REQUIRE(!existing.count(retval), "multiple declarations of " + string(1, c));
            existing.emplace(retval);
            return retval;
        }
        Token_ Register(const string& s, bool binary)
        {
            if (s.size() == 1)
                return Register(s[0], binary);
            if (!multi_.count(s))
                multi_.emplace(s, -static_cast<char>(1 + multi_.size()));
            Token_ retval(s, multi_[s]);
            set<Token_>& existing = binary ? binary_ : unary_;
            REQUIRE(!existing.count(retval), "multiple declarations of " + s);
            existing.emplace(retval);
            return retval;
        }

        struct Append_
        {
            OperatorList_& parent_;
            template<class T_> Append_& operator()(T_ t) { parent_.vals_.back().second.push_back(parent_.Register(t, parent_.vals_.back().first != Assoc_::UNARY)); return *this; }
            Append_& Left() { parent_.vals_.emplace_back(Assoc_::LEFT, vector<Token_>()); return *this; }
            Append_& Right() { parent_.vals_.emplace_back(Assoc_::RIGHT, vector<Token_>()); return *this; }
            Append_& Unary() { parent_.vals_.emplace_back(Assoc_::UNARY, vector<Token_>()); return *this; }
        };

        Append_ Left() { Append_ retval{ *this }; return retval.Left(); }
        Append_ Right() { Append_ retval{ *this };  return retval.Right(); }
        Append_ Unary() { Append_ retval{ *this };  return retval.Unary(); }

        bool ExistsUnary(Token_ t) const { return !!unary_.count(t); }
        bool ExistsBinary(Token_ t) const { return !!binary_.count(t); }
    };
}  // leave ZOLP
