
#include <array>
#include <iostream>
#include <assert.h>

#include "Parser_.h"
#include "Utils.h"
#include "Tests.h"

namespace
{
    using std::array;

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

    array<array<const char*, 2>, 5> IE = { {
        { ("9*(8//7)"), ("can't stack binary operators") },
        { ("f g h"), ("consecutive atoms") },
        { ("9 * ( * + 7"), ("precedence only supports binary-binary comparison") },
        { ("9 * ( 8 + 7"), ("Unmatched opening bracket '('") },
        { ("3 ~ 3"), ("no such operator") }
    } };

	std::vector<int> ParseCSList(const char* begin, const char* end)
	{
        using namespace ZOLP;

        // the risible example "grammar" from Boost Spirit
        OperatorList_ ops;
        ops.Right()(',');
        Parser_ parse(ops, BracketList_({}));

        AST_ ast = parse(begin, end);
        std::vector<int> retval;
		auto consume = [&](const AST_& node)
			{
				REQUIRE(node.head_.kind_ == NUMERIC_KIND, "Non-numeric argument");
				double temp = stod(node.head_.val_);
				int value = static_cast<int>(temp);
				REQUIRE(temp == value, "Non-integer numeric argument");
                retval.push_back(value);
			};

        for (const AST_* node = &ast; ; )
        {
            if (node->head_.kind_ == ',')
            {
				assert(node->children_.size() == 2);
                consume(*node->children_[0]);
				node = node->children_[1].get();
            }
            else
            {
                consume(*node);
				break;
            }
        }
		return retval;
	}
}

bool ZOLP::Test::RunAll(std::ostream& err)
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

	string intList = "1,2,3,44, 65536.0"; 
	std::vector<int> expectedList = { 1, 2, 3, 44, 65536 };
    try {
        if (ParseCSList(intList.c_str(), intList.c_str() + intList.size()) != expectedList)
        {
            err << "Test failed: ParseCSList('" << intList << "') did not return the expected list\n";
            return false;
        }
	}
	catch (const std::exception& ex)
	{
		err << "Test failed: ParseCSList('" << intList << "') threw exception '" << ex.what() << "'\n";
		return false;
	}

    return true;
}
