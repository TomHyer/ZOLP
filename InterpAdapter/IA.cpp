
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <iostream>

#include "Parser_.h"
#include "AST.h"
#include "Utils.h"

using namespace std;

// base class
class Interp1_
{
public:
	virtual ~Interp1_() = default;

	virtual double operator()(double x) const = 0;
};

// example non-scripted Interp
class Interp1Linear_ : public Interp1_
{
	vector<double> x_, y_;

public:
	Interp1Linear_(vector<double>&& x, vector<double>&& y) : x_(std::move(x)), y_(std::move(y))
	{  // x_ must be sorted
		if (x_.size() != y_.size())
			throw std::runtime_error("x and y must have the same size");
		if (std::adjacent_find(x_.begin(), x_.end(), std::greater_equal<>()) != x_.end())
			throw std::runtime_error("x must be sorted in non-decreasing order");
	}

	double operator()(double x) const override
	{
		auto pGE = std::lower_bound(x_.begin(), x_.end(), x);
		if (pGE == x_.end())
			return y_.back();
		if (pGE == x_.begin())
			return y_.front();
		auto iGE = pGE - x_.begin(), iLT = iGE - 1;
		double loFrac = (*pGE - x) / (*pGE - x_[iLT]);
		return y_[iGE] + loFrac * (y_[iLT] - y_[iGE]);
	}
};


struct Scope_
{
	vector<shared_ptr<const Interp1_>> bases_;
	unordered_map<string, double> vals_;
};

class Node_
{
public:
	virtual ~Node_() = default;

	virtual double Eval(const Scope_& scope) const = 0;
};

// a Node_ plus predefined variables makes an interp
class Interp1Node_ : public Interp1_
{
	shared_ptr<const Node_> node_;
	Scope_ scope_;  // predefined variables and other interps

public:
	Interp1Node_(shared_ptr<const Node_> node, const Scope_& predefined) : node_(node), scope_(predefined) {}

	double operator()(double x) const override
	{
		Scope_ newScope = scope_;
		newScope.vals_["x"] = x;
		return node_->Eval(newScope);
	}
};


// Various expressions all have their own nodes
struct NodeConst_ : Node_
{
	double val_;
	NodeConst_(double val) : val_(val) {}

	double Eval(const Scope_&) const override { return val_; }
};

template<typename OP_>
struct NodeUnary_ : Node_
{
	shared_ptr<const Node_> arg_;
	OP_ op_;
	NodeUnary_(shared_ptr<const Node_> arg, OP_ op) : arg_(arg), op_(op) {}

	double Eval(const Scope_& scope) const override
	{
		return op_(arg_->Eval(scope));
	}
};

template<typename OP_>
struct NodeArithmetic_ : Node_
{
	shared_ptr<const Node_> lhs_, rhs_;
	OP_ op_;
	NodeArithmetic_(shared_ptr<const Node_> lhs, shared_ptr<const Node_> rhs, OP_ op) : lhs_(lhs), rhs_(rhs), op_(op) {}

	double Eval(const Scope_& scope) const override
	{
		return op_(lhs_->Eval(scope), rhs_->Eval(scope));
	}
};

// operators; use std::plus, minus, multiplies
double Div(double a, double b) {
	if (b == 0.0)
		throw std::runtime_error("Division by zero");
	return a / b;
}
double GT(double a, double b) { return a > b ? 1.0 : 0.0; }
double LT(double a, double b) { return a < b ? 1.0 : 0.0; }
double GE(double a, double b) { return a >= b ? 1.0 : 0.0; }
double LE(double a, double b) { return a <= b ? 1.0 : 0.0; }
double EQ(double a, double b) { return a == b ? 1.0 : 0.0; }
double NE(double a, double b) { return a != b ? 1.0 : 0.0; }

template<class OP_> shared_ptr<const Node_> MakeArithmetic(shared_ptr<const Node_> lhs, shared_ptr<const Node_> rhs, OP_ op)
{
	// easy optimization: precompute if both sides are constants
	if (auto lConst = dynamic_cast<const NodeConst_*>(lhs.get()), rConst = dynamic_cast<const NodeConst_*>(rhs.get()); lConst && rConst) 
		return shared_ptr<const Node_>(new NodeConst_(op(lConst->val_, rConst->val_)));

	return make_shared<NodeArithmetic_<OP_>>(lhs, rhs, op);
}

// hand-code short-circuiting
struct NodeLogical_ : Node_
{
	shared_ptr<const Node_> lhs_, rhs_;
	bool isOr_;  // true for ||, false for &&
	NodeLogical_(shared_ptr<const Node_> lhs, shared_ptr<const Node_> rhs, bool isOr) : lhs_(lhs), rhs_(rhs), isOr_(isOr) {}

	double Eval(const Scope_& scope) const override
	{
		double lhs = lhs_->Eval(scope);
		if (isOr_)
			return lhs || rhs_->Eval(scope);
		else
			return lhs && rhs_->Eval(scope);
	}
};
shared_ptr<const Node_> MakeLogical(shared_ptr<const Node_> lhs, shared_ptr<const Node_> rhs, bool is_or)
{
	// a simple optimization to give the flavor
	if (auto lConst = dynamic_cast<const NodeConst_*>(lhs.get()))
	{  // lhs is known at "compile" time
		if (is_or)
			return lConst->val_ ? shared_ptr<const Node_>(new NodeConst_(1.0)) : rhs;
		else
			return lConst->val_ ? rhs : shared_ptr<const Node_>(new NodeConst_(0.0));
	}
	return make_shared<NodeLogical_>(lhs, rhs, is_or);
}


struct NodeVar_ : Node_
{
	string name_;
	double Eval(const Scope_& scope) const override
	{
		auto it = scope.vals_.find(name_);
		if (it == scope.vals_.end())
			throw std::runtime_error("Undefined variable: " + name_);
		return it->second;
	}
};

// we interpret zero as false, anything else as true
struct NodeIf_ : Node_
{
	shared_ptr<const Node_> cond_, then_, else_;
	double Eval(const Scope_& scope) const override
	{
		return cond_->Eval(scope) ? then_->Eval(scope) : else_->Eval(scope);
	}
};

// with(x=expr, body) creates a new scope
struct NodeWith_ : Node_
{
	string var_;
	shared_ptr<const Node_> expr_, body_;
	NodeWith_(const string& var, shared_ptr<const Node_> expr, shared_ptr<const Node_> body) : var_(var), expr_(expr), body_(body) {}

	double Eval(const Scope_& scope) const override
	{
		Scope_ newScope = scope;
		newScope.vals_[var_] = expr_->Eval(scope);
		return body_->Eval(newScope);
	}
};

// we can call existing interps to do complicated work
struct NodeF_ : Node_
{
	shared_ptr<const Node_> which_, arg_;
	NodeF_(const shared_ptr<const Node_>& which, const shared_ptr<const Node_>& arg) : which_(which), arg_(arg) {}

	double Eval(const Scope_& scope) const override
	{
		double d_which = which_->Eval(scope);
		int which = static_cast<int>(d_which);
		if (d_which != which)
			throw std::runtime_error("Non-integer function index");
		if (which < 0 || which >= scope.bases_.size())
			throw std::runtime_error("Function index out of range");
		double d_arg = arg_->Eval(scope);
		return (*scope.bases_[which])(d_arg);
	}
};

// turn an AST into a Node_ tree
shared_ptr<const Node_> Activate(const ZOLP::AST_& ast);

shared_ptr<const Node_> ActivateInterp(const ZOLP::AST_& ast)
{
	using namespace ZOLP;
	if (ast.children_.size() == 1)
	{  // input like f(x)
		static const shared_ptr<const Node_> ZERO = make_shared<NodeConst_>(NodeConst_{ 0.0 });
		REQUIRE(ast.children_[0]->bra_ != '(', "Expected f(...) for one-argument form of 'f'");
		auto arg = ast.children_[0];
		return make_shared<NodeF_>(NodeF_{ ZERO, Activate(*arg) });
	}
	else if (ast.children_.size() == 2)
	{  // input like f[1](x)
		auto which = ast.children_[0];
		REQUIRE(ast.children_[0]->bra_ != '[' && ast.children_[1]->bra_ != '(', "Expected f[...](...) for two-argument form of 'f'");
		return make_shared<NodeF_>(NodeF_{ Activate(*ast.children_[0]), Activate(*ast.children_[1])});
	}
	else
		throw std::runtime_error("Unexpected AST structure for f; need [index](arg) or (arg)");
}

shared_ptr<const Node_> ActivateIf(const ZOLP::AST_& ast)
{
	using namespace ZOLP;
	if (ast.children_.size() != 1 || ast.children_[0]->head_.kind_ != ',')
		throw std::runtime_error("Expected comma-separated arguments in if(cond, v_true, v_false)");
	if (ast.children_[0]->bra_ != ')')
		throw std::runtime_error("Expected parentheses in if(cond, v_true, v_false)");
	auto args = CommaSeparatedList<3>(ast.children_[0]);
	unique_ptr<NodeIf_> retval(new NodeIf_);
	retval->cond_ = Activate(*args[0]);
	retval->then_ = Activate(*args[1]);
	retval->else_ = Activate(*args[2]);
	return shared_ptr<const Node_>(retval.release());
}

shared_ptr<const Node_> ActivateWith(const ZOLP::AST_& ast)
{
	using namespace ZOLP;
	if (ast.bra_ != '(')
		throw std::runtime_error("Expected parentheses in with(<name>=<expr>, <body>)");
	auto args = CommaSeparatedList<2>(ast.children_[0]);
	auto assign = args[0];
	if (assign->state_ != AST_::State_::BINARY || assign->head_.kind_ != '=')
		throw std::runtime_error("Expected assignment in with(<name>=<expr>, <body>)");
	auto name = assign->children_[0];
	if (name->state_ != AST_::State_::ATOM || name->head_.kind_ != 'v')
		throw std::runtime_error("Expected variable name in with(<name>=<expr>, <body>)");
	if (name->head_.val_ == "f" || name->head_.val_ == "if" || name->head_.val_ == "with")
		throw std::runtime_error("Cannot assign to reserved names");
	auto expr = Activate(*args[1]);
	auto body = Activate(*ast.children_[1]);

	return make_shared<NodeWith_>(NodeWith_(name->head_.val_, expr, body));
}

shared_ptr<const Node_> ActivateExpression(const ZOLP::AST_& ast)
{
	if (ast.head_.kind_ == ZOLP::NUMERIC_KIND)  // number
	{
		double v = stod(ast.head_.val_);
		return shared_ptr<const Node_>(new NodeConst_(v));
	}
	else if (ast.head_.kind_ == ZOLP::ALPHA_KIND)  // variable
	{
		auto node = make_shared<NodeVar_>();
		node->name_ = ast.head_.val_;
		return node;
	}
	else if (ast.children_.size() == 1)  // unary
	{
		auto argNode = Activate(*ast.children_[0]);
		if (ast.head_.kind_ == '-')
			return shared_ptr<const Node_>(new NodeUnary_<std::negate<double>>(argNode, std::negate<double>()));
		throw std::runtime_error("Unexpected unary operator: " + ast.head_.val_);
	}
	else if (ast.children_.size() == 2)  // binary
	{
		auto lhsNode = Activate(*ast.children_[0]);
		auto rhsNode = Activate(*ast.children_[1]);
		// handle single-character operators
		switch (ast.head_.kind_)
		{
		case '+':
			return MakeArithmetic(lhsNode, rhsNode, std::plus<double>());
		case '-':
			return MakeArithmetic(lhsNode, rhsNode, std::minus<double>());
		case '*':
			return MakeArithmetic(lhsNode, rhsNode, std::multiplies<double>());
		case '/':
			return MakeArithmetic(lhsNode, rhsNode, Div);
		case '>':
			return MakeArithmetic(lhsNode, rhsNode, GT);
		case '<':
			return MakeArithmetic(lhsNode, rhsNode, LT);
		default:
			if (ast.head_.val_.size() == 1)
				throw std::runtime_error("Unexpected binary operator: " + ast.head_.val_);
		}
		// multi-character tokens
		if (ast.head_.val_ == ">=")
			return MakeArithmetic(lhsNode, rhsNode, GE);
		else if (ast.head_.val_ == "<=")
			return MakeArithmetic(lhsNode, rhsNode, LE);
		else if (ast.head_.val_ == "==")
			return MakeArithmetic(lhsNode, rhsNode, EQ);
		else if (ast.head_.val_ == "!=")
			return MakeArithmetic(lhsNode, rhsNode, NE);
		else if (ast.head_.val_ == "&&")
			return MakeLogical(lhsNode, rhsNode, false);
		else if (ast.head_.val_ == "||")
			return MakeLogical(lhsNode, rhsNode, true);
		else
			throw std::runtime_error("Unexpected binary operator: " + ast.head_.val_);
	}
	else
		throw std::runtime_error("Unexpected atom: " + ast.head_.val_);
}

shared_ptr<const Node_> ActivateFunction(const ZOLP::AST_& ast)
{
	if (ast.head_.kind_ == ZOLP::ALPHA_KIND)  // name
	{
		if (ast.head_.val_ == "f")
		{
			return ActivateInterp(ast);
		}
		if (ast.head_.val_ == "with")
		{
			return ActivateWith(ast);
		}
		if (ast.head_.val_ == "if")
		{
			return ActivateIf(ast);
		}
	}
	throw std::runtime_error("Unexpected function: " + ast.head_.val_);
}

shared_ptr<const Node_> Activate(const ZOLP::AST_& ast)
{
	using namespace ZOLP;
	// handle the various cases based on the AST node type
	switch (ast.state_)
	{
	case AST_::State_::ATOM:
		return ActivateExpression(ast);

	case AST_::State_::FUNCTION:
		return ActivateFunction(ast);

	default:
		throw std::runtime_error("Unexpected AST state (incomplete parse?)");
	}
}

shared_ptr<Interp1_> StringToInterp(const string& s, const Scope_& predefined)
{
	using namespace ZOLP;
	OperatorList_ ops;
	ops.Unary()('-')
		.Left()('*')('/')
		.Left()('+')('-')
		.Left()('<')("<=")('>')(">=")
		.Left()("==")("!=")
		.Left()('&')
		.Left()('|')
		.Right()('=')
		.Left()(',');
	BracketList_ brackets(vector<Bracket_>({ { '(', ')' }, {'[', ']'} }));
	Parser_ parse(ops, brackets);

	auto ast = parse(&s[0], &s[0] + s.size());
	auto node = Activate(ast);
	return shared_ptr<Interp1_>(new Interp1Node_(node, predefined));
}

int main()
{
	try
	{
		string formula_1 = "if(x > -1, if(x < 1, x, 1), -1)";
		Scope_ empty;
		auto f_1 = StringToInterp(formula_1, empty);
		cout << (*f_1)(0.5) << "\n";  // should print 0.5
		cout << (*f_1)(1.5) << "\n";  // should print 1

		shared_ptr<Interp1_> fLinear(new Interp1Linear_(std::vector<double>{0.0, 1.0, 2.0}, std::vector<double>{-1.0, 3.0, -1.0}));
		empty.bases_.push_back(fLinear);  // now f[0] should invoke fLinear
		string formula_2 = "if(x > -1, if(x < 1, x, 1), -f[0](-x/3))";
		auto f_2 = StringToInterp(formula_2, empty);
		cout << (*f_2)(0.5) << "\n";  // should print 0.5
		cout << (*f_2)(1.5) << "\n";  // should print 1
		cout << (*f_2)(-1.5) << "\n";  // should print -f[0](0.5) = -1.0
		cout << (*f_2)(-2.5) << "\n";  // should print -f[0](5/6) = -7/3
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << "\n";
		return 1;
	}
	return 0;
}
