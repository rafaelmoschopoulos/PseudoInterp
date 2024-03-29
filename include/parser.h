/* parser.h */

#pragma once
enum class OperatorType
{ // Enumeration for all operators. Used to denote the operator in the AST nodes
	COMMA,
	UNARY_PLUS,
	UNARY_NEGATION,
	ADDITION,
	SUBTRACTION,
	MULTIPLICATION,
	DIVISION,
	MODULO,
	DIV,
	ADDITION_ASSIGN,
	SUBTRACTION_ASSIGN,
	MULTIPLICATION_ASSIGN,
	DIVISION_ASSIGN,
	MODULO_ASSIGN,
	DIV_ASSIGN,
	OR,
	AND,
	NOT,
	BIT_AND,
	BIT_OR,
	BIT_NOT,
	BIT_XOR,
	BIT_LSHIFT,
	BIT_RSHIFT,
	EQUAL,
	NOT_EQUAL,
	GREATER,
	LESS,
	GRE_EQ,
	LESS_EQ,
	ASSIGNMENT,
	PRE_INCR,
	PRE_DECR,
	POST_INCR,
	POST_DECR,
	FUNCTION_CALL,
	SUBSCRIPT,
	MEMBER_ACCESS,
	LIST_INIT,
	UNKNOWN
};

#include <map>
#include "lexer.h"
#include "AST.h"

class CodeBlock;
class Statement;
class ASTNode;
class Object;
class Scope;

#define MAX_GROUPS 15 // The maximum amount of operator groups

using OT = OperatorType;
using TT = Lexer::TokenType;

class Parser
{
public:
	Parser();
	CodeBlock* getAST(const std::string&);

private:
	Lexer lexer;

	// Each precedence group consists of a map linking tokens to their corresponding
	// operators, and a pointer to a function to parse those operators
	// I know this is a hot mess, but I didn't know any better.
	struct precedenceGroup
	{
		std::map<Lexer::TokenType, OperatorType> findOp{};
		ASTNode* (Parser::* parserFunc)(precedenceGroup*){};
	};

#ifndef COMMA_PRECEDENCE // The precedence level of comma.
#define COMMA_PRECEDENCE 0 // Used by the parseParenthAndDot function
#endif

	// TT is tokentype, OT is operator type
	precedenceGroup precedenceTab[MAX_GROUPS] = {
		// Precedence = 0
		{{{TT::COMMA, OT::COMMA}}, &Parser::parseBinLeft},

		// Precedence = 1
		{
			{
				{TT::EQ, OT::ASSIGNMENT},
				// 'equals' token ("=") is linked to assignment operator.
				{TT::PLUS_EQ, OT::ADDITION_ASSIGN},
				{TT::MINUS_EQ, OT::SUBTRACTION_ASSIGN},
				{TT::STAR_EQ, OT::MULTIPLICATION_ASSIGN},
				{TT::FORW_SLASH_EQ, OT::DIVISION_ASSIGN},
				{TT::PERCENT_EQ, OT::MODULO_ASSIGN},
				{TT::DIV_EQ, OT::DIV_ASSIGN}
			},
			&Parser::parseBinRight
			// Parser function used for that group is parseBinRight
		},

		// Precedence = 2
		{
			{
				{TT::DOUBLE_VERT_SLASH, OT::OR},
				{TT::OR, OT::OR}
			},
			&Parser::parseBinLeft
		},

		// Precedence = 3
		{
			{
				{TT::DOUBLE_AMP, OT::AND},
				{TT::AND, OT::AND}
			},
			&Parser::parseBinLeft
		},

		// Precedence = 4
		{
			{
				{TT::DOUBLE_EQ, OT::EQUAL},
				{TT::NOT_EQ, OT::NOT_EQUAL}
			},
			&Parser::parseBinLeft
		},

		// Precedence = 5
		{
			{
				{TT::LESS, OT::LESS},
				{TT::LESS_EQ, OT::LESS_EQ},
				{TT::GRE, OT::GREATER},
				{TT::GRE_EQ, OT::GRE_EQ}
			},
			&Parser::parseBinLeft
		},

		// Precedence = 6
		{
			{
				{TT::PLUS, OT::ADDITION},
				{TT::MINUS, OT::SUBTRACTION}
			},
			&Parser::parseBinLeft
		},

		{
			{
				{TT::STAR, OT::MULTIPLICATION},
				{TT::FORW_SLASH, OT::DIVISION},
				{TT::PERCENT, OT::MODULO},
				{TT::MOD, OT::MODULO},
				{TT::DIV, OT::DIV}
			},
			&Parser::parseBinLeft
		},

		{
			{
				{TT::PLUS, OT::UNARY_PLUS},
				{TT::MINUS, OT::UNARY_NEGATION},
				{TT::EXMARK, OT::NOT},
				{TT::NOT, OT::NOT},
				{TT::DOUBLE_PLUS, OT::PRE_INCR},
				{TT::DOUBLE_MINUS, OT::PRE_DECR}
			},
			&Parser::parseUnary
		},

		{
			{
				{TT::DOUBLE_PLUS, OT::POST_INCR},
				{TT::DOUBLE_MINUS, OT::POST_DECR}
			},
			&Parser::parseUnaryPostfix
		},

		{
			{
				{TT::L_SQ_BRACKET, OT::SUBSCRIPT},
				{TT::L_PAREN, OT::FUNCTION_CALL}
			},
			&Parser::parseParenthAndDot
		},

		{{}, &Parser::parsePrimary}
	};

	CodeBlock* parseBlock(); // Parses a block
	Statement* parseWhile(); // Parses a while statement
	Statement* parseIf(); // Parses an if statement
	Statement* parseFor(); // Parses a for statement
	Statement* parseExpr(); // Parses an expression
	Statement* parseReturn(); // Parses a return statement
	Statement* parseFunctionDef(); // Parses a function definition
	ASTNode* parseUnary(precedenceGroup*); // Parses unary prefix operators
	ASTNode* parseBinLeft(precedenceGroup*); // For binary left associative ops.
	ASTNode* parseBinRight(precedenceGroup*); // For binary right associative ops.
	ASTNode* parseUnaryPostfix(precedenceGroup*); // Parses unary postfix ops.
	ASTNode* parsePrimary(precedenceGroup*); // Parses IDs, literals, list inits.
	ASTNode* parseParenthAndDot(precedenceGroup*); /* Parses parenth and dot
				operators. These are left associative and have same precedence */
	int blockLevel = -1;
	// Each block increases this by one. So if the main block (which contains everything)
	// is level 0, then blockLevel is initially -1.
	bool lessTabs(int&); /* Check the subsequuent tabs at any point, to determine
							if we've exited a block, or identation errors */
	void checkNewLine(); /* Proceed to newline, or throw errors if newline is
							expected but there are more tokens */
};
