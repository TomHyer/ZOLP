#pragma once

#include "AST.h"
#include "Precedence.h"
#include "Lexer.h"

namespace ZOLP
{
    struct Parser_
    {
        using State_ = AST_::State_;
        AST_ top_;
        AST_* current_;

        OperatorList_ ops_;
        Precedence_ precedence_;
        OpLexer_ opLexer_;
        BracketList_ brackets_;

        Parser_(const OperatorList_& ops, const BracketList_& brackets) : ops_(ops), precedence_(ops), opLexer_(ops), brackets_(brackets) {}

        AST_* Append(Token_ next)
        { 
            if (current_->state_ == State_::EMPTY && next != NONE_TOKEN)
            {
                current_->head_ = next;
                return current_;
            }
            current_->children_.emplace_back(new AST_);
            AST_* retval = current_->children_.back().get();
            retval->parent_ = current_;
            retval->head_ = next;
            return retval;
        }

        void Insert(Token_ next)
        {
            // close-brace
            if (brackets_.ketToBra_.count(next.kind_))
            {
                char bra = brackets_.ketToBra_[next.kind_].value().kind_;
                while (current_ && (!current_->bra_ || brackets_.ketToBra_.count(current_->bra_))) // walk past close-braces, look for open-brace
                {
                    if (current_->state_ == State_::UNARY)
                    {
                        REQUIRE(!current_->children_.empty(), "unary without argument, like '( + )'");
                    }
                    else if (current_->state_ == State_::BINARY)
                    {
                        REQUIRE(current_->children_.size() == 2, "binary without second argument, like 'A + )'");
                    }
                    current_->state_ = State_::ATOM;
                    current_ = current_->parent_;
                }
                REQUIRE(current_, "no open-brace found for " + next.val_);
                REQUIRE(current_->bra_ == bra, string("wrong kind of open-brace found: ") + current_->bra_);
                current_->bra_ = next.kind_;  // mark as closed
                current_->state_ = State_::ATOM;
                return;
            }
            // open-brace
            if (brackets_.braToKet_.count(next.kind_))
            {
                if (current_->state_ == State_::ATOM || current_->state_ == State_::FUNCTION)
                    current_->state_ = State_::FUNCTION;  // this allows a numeric atom to be a function name
                if (current_->state_ != State_::EMPTY || current_->bra_)
                    current_ = Append(NONE_TOKEN);
                current_->state_ = State_::EMPTY;
                current_->bra_ = next.kind_;
                return;
            }
            // name or value
            if (IsAlphanumeric(next.val_[0]))
            {
                REQUIRE(current_->state_ != State_::ATOM, "consecutive atoms, like 'A B'");
                REQUIRE(current_->state_ != State_::FUNCTION, "function then atom, like 'A(3) B'");
                current_ = Append(next);
                current_->state_ = State_::ATOM;
                return;
            }
            // some kind of operator
            if (current_->state_ == State_::UNARY || current_->state_ == State_::BINARY)  // stacked operators, like "3 + - - + +" which is still OK awaiting closure
            {
                current_ = Append(next);
                REQUIRE(ops_.ExistsUnary(next), "can't stack binary operators (postfix not yet supported)");
                current_->state_ = State_::UNARY;
                return;
            }
            // operator applied to a non-operator, at last
            REQUIRE(ops_.ExistsBinary(next), "postfix not yet supported");
            for ( ; ; )
            {
                AST_* lhsOp = current_;
                if (lhsOp && lhsOp->bra_ && lhsOp->state_ == State_::ATOM)
                {   // see if it's the arguments of a function call; remember call can look like f(x)(y)(z)
                    for (AST_* args = lhsOp; args && args->state_ == State_::ATOM && brackets_.ketToBra_.count(args->bra_); args = args->parent_)
                    {
                        if (args->parent_ && args->parent_->state_ == State_::FUNCTION)
                        {
                            lhsOp = args->parent_;
                            break;
                        }
                    }
                }
                while (lhsOp && lhsOp->state_ != State_::UNARY && lhsOp->state_ != State_::BINARY && !brackets_.IsBracket(lhsOp->bra_))
                    lhsOp = lhsOp->parent_;
                if (!lhsOp || (lhsOp->bra_ && (lhsOp->state_ == State_::ATOM || brackets_.ketToBra_.count(lhsOp->bra_))))
                    break;
                if (precedence_.SplitLeft(lhsOp->head_, next))
                    break;  // lhs operator has lower precedence, like "2 + 3 *"
                // next operator has lower precedence, like "2 * 3 +"; roll up the lhs 
                while (current_ != lhsOp)
                {
                    current_ = current_->parent_;
                }
                current_->state_ = State_::ATOM;    // no longer splittable
            }
            // change the existing AST to add this operator
            if (current_->state_ == State_::EMPTY)
            {
                current_->state_ = State_::UNARY;
                current_->head_ = next;
            }
            else
            {
                while (current_->parent_ && current_->state_ == State_::ATOM && brackets_.ketToBra_.count(current_->bra_))
                    current_ = current_->parent_;
                AST_* parent = current_->parent_;
                shared_ptr<AST_> temp(new AST_(*current_));
                temp->parent_ = current_;
                // and take this node for ourselves
                current_->state_ = State_::BINARY;
                temp->bra_ = 0;  // grouping stays up there
                current_->head_ = next;
                current_->parent_ = parent;
                current_->children_ = { temp };
            }
        }

        AST_ operator()(const char* begin, const char* end)
        {
            Lexer_ lex(ops_, brackets_);
            lex.Restart(begin, end);
            top_ = AST_();
            current_ = &top_;
            for ( ; ; )
            {
                Token_ t = lex.Next();
                if (t == NONE_TOKEN)
                    break;
                Insert(t);
            }
            // if the whole expression was parenthesized, cast those off
            while (top_.state_ == State_::EMPTY && top_.children_.size() == 1 && top_.children_[0]->bra_)
            {
                top_.state_ = top_.children_[0]->state_;
                top_.head_ = top_.children_[0]->head_;
                auto newChildren = top_.children_[0]->children_;
                top_.children_.clear();
                top_.children_ = newChildren;
            }
            return top_;
        }
    };
}  // leave ZOLP
