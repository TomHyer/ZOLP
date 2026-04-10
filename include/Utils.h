#pragma once

#include "AST.h"

namespace ZOLP
{
	using std::string;

    string S_Expression(const AST_& ast, const BracketList_& brackets)
    {
        if (ast.children_.empty())
            return ast.head_.val_;
        string retval = "(" + ast.head_.val_;
        if (char ket = ast.children_.empty() ? 0 : ast.children_[0]->bra_)
        {
            if (auto p = brackets.ketToBra_.find(ket); p != brackets.ketToBra_.end())
                retval += p->second->kind_;
            retval += ket;
        }
        retval += " ";
        for (auto c : ast.children_)
            retval += S_Expression(*c, brackets) + " ";
        retval.back() = ')';
        return retval;
    }
}  // leave ZOLP
