#pragma once

#include <array>
#include <string>
#include <ostream>

#include "Parser_.h"
#include "Utils.h"

namespace ZOLP
{
    using std::array;
	using std::string;

    namespace Test
    {
        array<array<const char*, 2>, 29> IO = { {
            { ("2+3+4"), ("(+ (+ 2 3) 4)") },
            { ("2*3+4*5"), ("(+ (* 2 3) (* 4 5))") },
            { ("2-3*4-5"), ("(- (- 2 (* 3 4)) 5)") },
            { ("x[7]"), ("(x[] 7)") },
            { ("func(a+3)"), ("(func() (+ a 3))") },
            { ("2*(3+4)*5"), ("(* (* 2 (+ 3 4)) 5)") },
            { ("func(3+4+5)"), ("(func() (+ (+ 3 4) 5))") },
            { ("func(a, b+7)"), ("(func() (, a (+ b 7)))") },
            { ("2 + 3 + 4"), ("(+ (+ 2 3) 4)") },
            { ("with(x=4, with(z=x/v(f), 1+z*z))"), ("(with() (, (= x 4) (with() (, (= z (/ x (v() f))) (+ 1 (* z z))))))") },
            { ("2=3,4"), ("(, (= 2 3) 4)") },
            { ("2,3=4"), ("(, 2 (= 3 4))") },
            { ("(2=3, 4)"), ("(, (= 2 3) 4)") },
            { ("2*(3+4*5)"), ("(* 2 (+ 3 (* 4 5)))") },
            { ("2*(3*4+5)"), ("(* 2 (+ (* 3 4) 5))") },
            { ("3*4+5"), ("(+ (* 3 4) 5)") },
            { ("func(a+3)"), ("(func() (+ a 3))") },
            { ("func(a) + 3"), ("(+ (func() a) 3)") },
            { ("(3+4)*5"), ("(* (+ 3 4) 5)") },
            { ("with(x=f(0), 1)"), ("(with() (, (= x (f() 0)) 1))") },
            { ("(3+4+5)"), ("(+ (+ 3 4) 5)") },
            { ("([{4+5+6}])"), ("([] ({} (+ (+ 4 5) 6)))") },
            { ("[5+6*7]"), ("(+ 5 (* 6 7))") },
            { ("2 + 3 - 4 + 5"), ("(+ (- (+ 2 3) 4) 5)") },
            { ("-2"), ("(- 2)") },
            { ("-2 -3"), ("(- (- 2) 3)") },
            { ("-2 - -3"), ("(- (- 2) (- 3))") },
            { ("+-- 2"), ("(+ (-- 2))") },
            { ("-+-2"), ("(- (+ (- 2)))") }
        } };

        array<array<const char*, 2>, 3> IE = { {
            { ("9*(8//7)"), ("can't stack binary operators") },
            { ("f g h"), ("consecutive atoms") },
            { ("3 ~ 3"), ("no such operator") }
        } };


        bool Run(std::ostream& err)
        {
            // define the test grammar
            OperatorList_ ops;
            ops.Left()("::").Left()('.')("->").Unary()("++")("--")('+')('-')('*')('&')('!').Left()(".*")("->*")
                .Left()('*')('/')('%').Left()('+')('-').Left()("<<")(">>").Left()("<=>")
                .Left()('<')("<=")('>')(">=").Left()("==")("!=").Left()('&').Left()('^').Left()('|')
                .Left()("&&").Left()("||").Right()('?')(':')
                .Right()('=')("+=")("-=")("*=")("/=")("%=")("<<=")(">>=")("&=")("^=")("?=")
                .Left()(',');

            BracketList_ brackets(vector<Bracket_>({ { '(', ')' }, {'{', '}'}, {'[', ']'} }));
            Parser_ parse(ops, brackets);

            for (const auto& [input, expected] : IO)
            {
                try
                {
                    AST_ ast = parse(input, input + strlen(input));
                    string actual = S_Expression(ast, brackets);
                    if (actual != string(expected))
                    {
						err << "Test failed: input '" << input << "', expected '" << expected << "' but got '" << actual << "'\n";
                        return false;
                    }
                }
                catch (const std::exception& ex)
                {
                    err << "Test failed: input '" << input << "' threw exception '" << ex.what() << "'\n";
                    return false;
                }
            }

            for (const auto& [input, error] : IE)
            {
                try
                {
                    AST_ ast = parse(input, input + strlen(input));
                    string actual = S_Expression(ast, brackets);  // shouldn't get here
                    err << "Test failed: input '" << input << "', expected error but got '" << actual << "'\n";
                    return false;
                }
                catch (const std::exception& ex)
                {
                    auto find = string(ex.what()).find(error);
                    if (find == string::npos)
                    {
                        err << "Test failed: input '" << input << "' threw exception '" << ex.what() << "', expected '" << error << "'\n";
                        return false;
                    }
                }
            }

            return true;
        }
    }
}  // leave ZOLP
