#pragma once

#include <deque>
#include <array>
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
        for (const auto& c : ast.children_)
            retval += S_Expression(*c, brackets) + " ";
        retval.back() = ')';
        return retval;
    }

    template<typename CHECK_, bool LEFT = true>
	std::deque<AST_*> SeparatedList(AST_& head, CHECK_ is_separator)
	{
		std::deque<AST_*> retval;
		auto push = [&](AST_* node) { if constexpr (LEFT) retval.push_front(node); else retval.push_back(node); };
		AST_* current = &head;
        while (is_separator(current->head_) && current->children_.size() == 2)
        {
			push(current->children_[LEFT ? 1 : 0].get());
			current = current->children_[LEFT ? 0 : 1].get();
        }
        push(current);
		return retval;
	}
    // specialize for single-character separators
    template<bool LEFT = true>
    std::deque<AST_*> SeparatedList(AST_& head, char sep = ',')
    {
        auto is_separator = [sep](const Token_& token) { return token == sep; };
        return SeparatedList<decltype(is_separator), LEFT>(head, is_separator);
    }
    
    // additional helper when size is known at compile time
	template<int N>
	std::array<AST_*, N> CommaSeparatedList(AST_& head)
	{
		auto temp = SeparatedList<true>(head, ',');
		if (temp.size() != N)
			throw std::runtime_error("Unexpected number of arguments in SeparatedList, expected " + std::to_string(N) + ", got " + std::to_string(temp.size()));
		std::array<AST_*, N> retval;
		std::copy(temp.begin(), temp.end(), retval.begin());
        return retval;
	}
}  // leave ZOLP
