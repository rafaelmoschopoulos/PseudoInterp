﻿/* parser.cpp */

#include "parser.h"
#include "errors.h"
#include "object.h"

Parser::Parser() = default;

CodeBlock* Parser::getAST(const std::string& inputStr)
{
	// Returns full AST based on inputStr
	lexer.setInput(inputStr); // Convert str to tokens
	lexer.lexInput();
	CodeBlock* mainBlock = parseBlock();
	// Consider the whole program to be in a block
	if (lexer.getCurrToken().getType() != Lexer::TokenType::EOFILE)
	{
		// If there are unparsed characters in the end, something's wrong
		throw ParsingError("", lexer.getCurrToken().getPos());
	}
	return mainBlock;
}

// Note: (this->*(currGroup + 1)->parserFunc)(currGroup + 1) calls the appropriate parser
// function for operators of the next (higher) precedence level.

// Note: in grammar definitions, T refers to an expression of higher precedence than E
// The notation {A} means repetition of A (or nothing) (i.e. "", "A", "AA")
// The notaion A|B|C means either one or the other.
// Hence {A|B|C} may be "AA", "ABBC", "C", "", "CCCAAA", etc.
// ε is the empty string ""
// [op] denotes an operator (i.e. +)
// A block is a sequence of statements, with an equal number of leading tabs (identation).
// To parse a block, the current token must be at the beginning of the line
// (not at the beginning of the statement).
CodeBlock* Parser::parseBlock()
{
	blockLevel++; // New block => level up
	// Used to know how many tabs to expect in the current block
	const auto currBlock = new CodeBlock();
	while (lexer.getCurrToken().getType() != Lexer::TokenType::EOFILE)
	{
		// A block is defined by statements having equal indentation.
		if (int nTabs; lessTabs(nTabs)) break;
		// If less tabs than expected, we're out of the block

		while (lexer.getCurrToken().getType() == Lexer::TokenType::TAB) lexer.
			scanToken(); // Skip all the tabs

		Statement* currStatement = nullptr;
		switch (lexer.getCurrToken().getType())
		{
		// Parse depending on the statement
		case Lexer::TokenType::WHILE:
			currStatement = parseWhile();
			break;
		case Lexer::TokenType::IF:
			currStatement = parseIf();
			break;
		case Lexer::TokenType::FOR:
			currStatement = parseFor();
			break;
		case Lexer::TokenType::RETURN_TOK:
			currStatement = parseReturn();
			break;
		case Lexer::TokenType::FUNCTION_DEF:
			currStatement = parseFunctionDef();
			break;
		default: // Else, it is just a normal expression statement
			currStatement = parseExpr();
			break;
		}
		currBlock->addStatement(currStatement); // Add statement to sequence
	}
	blockLevel--; // Block has ended => level down
	return currBlock;
}

bool Parser::lessTabs(int& i)
// Checks for identation errors, or if we have exited a block
{
	for (i = 0; lexer.lookForw(i).getType() == Lexer::TokenType::TAB; i++);
	if (i < blockLevel)
	{
		// Less tabs than the current block level -> current block ended
		return true;
	}
	if (i > blockLevel)
	{
		// Excessive tabs yield indentation error
		throw ParsingError("Indentation error.", lexer.getCurrToken().getPos());
	}
	return false;
}

void Parser::checkNewLine() // Runs when a newline is expected
{
	if (lexer.getCurrToken().getType() != Lexer::TokenType::NEWLINE)
		// If it doesn't end in newline, something's wrong
		throw ParsingError("Newline expected.", lexer.getCurrToken().getPos());
	lexer.scanToken();
}


Statement* Parser::parseReturn()
// Simply parses expression after keyword "return"
{
	const size_t pos = lexer.getCurrToken().getPos();
	// pos always holds the position of the statement/operator/expression and is stored
	// for error reporting
	lexer.scanToken();
	Statement* returnStatement = new ReturnStatement(
		(this->*precedenceTab[0].parserFunc)(precedenceTab), pos);
	checkNewLine(); // Proceeds to new line - if more code in same line throw error
	return returnStatement;
}

Statement* Parser::parseExpr()
// Simply parses an expression and returns the result 
{
	const size_t pos = lexer.getCurrToken().getPos();
	Statement* exprStatement = new ExprStatement(
		(this->*precedenceTab[0].parserFunc)(precedenceTab), pos);
	checkNewLine(); // Go to next line
	return exprStatement;
}

Statement* Parser::parseIf()
{
	const auto statement = new IfStatement(lexer.getCurrToken().getPos());
	Lexer::TokenType currToken;
	while ((currToken = lexer.getCurrToken().getType()) == Lexer::TokenType::IF
		|| currToken ==
		Lexer::TokenType::ELIF || currToken ==
		Lexer::TokenType::ELSE) // While the current token is if/elif/else
	{
		// To parse the whole if-elif-else chain
		lexer.scanToken();
		ASTNode* condition = nullptr;

		if (currToken != Lexer::TokenType::ELSE) // If it isn't 'else'
		{
			// Else doesn't have a condition
			condition = (this->*precedenceTab[0].parserFunc)(precedenceTab);
			// Parse the expression
			if (lexer.getCurrToken().getType() == Lexer::TokenType::THEN)
			{
				// Check if 'then' exists after the condition
				lexer.scanToken();
			}
			else throw ParsingError("\'then\' token expected.",
									lexer.getCurrToken().getPos());
		}
		else
		{
			condition = new LiteralNode(true, 0);
			// Create a dummy condition that is always true
		}

		checkNewLine(); // Go to next line

		CodeBlock* block = parseBlock(); // Parse block and add it
		statement->addCase(condition, block);

		if (currToken == Lexer::TokenType::ELSE) break;
		// If we're on an else, we have finished

		int nTabs = 0;
		if (lessTabs(nTabs)) return statement;
		// Count the tabs in nTabs. If less than expected, we are done

		if (Lexer::TokenType nextTok; (nextTok = lexer.lookForw(nTabs).
				getType()) == Lexer::TokenType::ELIF || nextTok ==
			Lexer::TokenType::ELSE)
		{
			// If the next statement after the tabs is elif or else
			while (lexer.getCurrToken().getType() == Lexer::TokenType::TAB)
				lexer.scanToken();
			// Skip tabs, and continue the loop
		}
		else break;
	}

	return statement;
}


Statement* Parser::parseWhile()
{
	const size_t pos = lexer.getCurrToken().getPos();
	lexer.scanToken();
	ASTNode* condition = (this->*precedenceTab[0].parserFunc)(precedenceTab);
	// Get the condition expression
	checkNewLine();
	CodeBlock* block = parseBlock(); // Get the block

	return new WhileStatement(condition, block, pos);
}

Statement* Parser::parseFor()
{
	const size_t pos = lexer.getCurrToken().getPos();
	lexer.scanToken();
	// This is the dummy counter variable of the loop
	if (lexer.getCurrToken().getType() != Lexer::TokenType::ID)
		throw ParsingError("Token is not an identifier.",
		                   lexer.getCurrToken().getPos());
	ASTNode* counterNode = new IDNode(lexer.getCurrToken().getLexeme(),
	                                  lexer.getCurrToken().getPos());
	lexer.scanToken();

	if (lexer.getCurrToken().getType() == Lexer::TokenType::FROM)
	{
		// Check if the limits are separated by 'from' and 'to'
		lexer.scanToken();
	}
	else throw ParsingError("\'from\' - lower limit delimiter expected.",
	                        lexer.getCurrToken().getPos());
	
	// Parse the lower limit expression 
	ASTNode* lowerNode = (this->*precedenceTab[0].parserFunc)(precedenceTab);

	if (lexer.getCurrToken().getType() == Lexer::TokenType::TO)
	{
		lexer.scanToken(); // Go to next token
	}
	else throw ParsingError("\'to\' - upper limit delimiter expected.",
	                        lexer.getCurrToken().getPos());

	// Parse the upper limit expression
	ASTNode* upperNode = (this->*precedenceTab[0].parserFunc)(precedenceTab);

	checkNewLine();

	CodeBlock* block = parseBlock(); // Parse 'for' block

	return new ForStatement(counterNode, lowerNode, upperNode, block, pos);
}

ASTNode* Parser::parseUnary(precedenceGroup* currGroup)
{
	// Parses right associative unary operators (E -> T, E -> [op]E)
	const Lexer::TokenType currToken = lexer.getCurrToken().getType();
	if (currGroup->findOp.contains(currToken))
	{
		// If current token is in the token group we are examining
		const size_t pos = lexer.getCurrToken().getPos();
		lexer.scanToken(); // Proceed to next token
		ASTNode* child = parseUnary(currGroup); // E -> [op]E
		return new UnaryNode(child, currGroup->findOp[currToken], pos);
		// Create unary node, assign the corresponding operator
	}
	return (this->*(currGroup + 1)->parserFunc)(currGroup + 1); // E -> T
}

ASTNode* Parser::parseBinLeft(precedenceGroup* currGroup)
{
	// Parses binary left associative operators. ( E -> E + T, hence E -> T {[op]T} )
	ASTNode* nodeA = (this->*(currGroup + 1)->parserFunc)(currGroup + 1);
	// Get left operand
	while (true)
	{
		// As long as we have repeated terms (i.e. 5 + 2 + 3 + 8), continue parsing
		const Lexer::TokenType currToken = lexer.getCurrToken().getType();
		if (currGroup->findOp.contains(currToken))
		{
			const size_t pos = lexer.getCurrToken().getPos();
			lexer.scanToken();
			ASTNode* nodeB = (this->*(currGroup + 1)->
				parserFunc)(currGroup + 1); // Get right operand
			ASTNode* tmpNode = new BinaryNode(nodeA, nodeB,
			                                  currGroup->findOp[currToken],
			                                  pos);
			// Use temporary node to fix left recursion problem
			nodeA = tmpNode;
		}
		else
			return nodeA;
	}
}

ASTNode* Parser::parseBinRight(precedenceGroup* currGroup)
{
	// Parses binary right associative operators. ( E -> T + E )
	ASTNode* nodeA = (this->*(currGroup + 1)->parserFunc)(currGroup + 1);
	// Get left operand
	const Lexer::TokenType currToken = lexer.getCurrToken().getType();
	if (currGroup->findOp.contains(currToken))
	{
		// If current token is in the group we're examining
		const size_t pos = lexer.getCurrToken().getPos();
		lexer.scanToken();
		ASTNode* nodeB = (this->*(currGroup)->parserFunc)(currGroup);
		// E -> T + E (call E again)
		return new BinaryNode(nodeA, nodeB, currGroup->findOp[currToken], pos);
	}
	return nodeA;
}

// Note: the function call, square brackets and dot operator are parsed differently, but
// have equal precedence and are both left associative. Hence, they must be parsed by
// the same function.
// Grammar: E -> T{. | (V) | [V]}
//			V -> ε | E{,E}
ASTNode* Parser::parseParenthAndDot(precedenceGroup* currGroup)
{
	ASTNode* node = (this->*(currGroup + 1)->parserFunc)(currGroup + 1);
	while (true)
	{
		if (currGroup->findOp.contains(lexer.getCurrToken().getType()))
		{
			const Lexer::TokenType currToken = lexer.getCurrToken().getType();
			const Lexer::TokenType closingToken = lexer.getCurrToken().
				getOppositeType(); // ) if (, ] if [
			const size_t pos = lexer.getCurrToken().getPos();
			std::vector<ASTNode*> nOperands;
			if (lexer.lookForw(1).getType() != closingToken)
			// For the case where the argument list is empty "()"
			{
				do
				{
					lexer.scanToken();
					nOperands.push_back(
						// Parse an expression, but parse above the comma precedence
						// (comma has another meaning here - it separates expressions
						// and it isn't an operator)
						(this->*precedenceTab[COMMA_PRECEDENCE + 1].parserFunc)(
							precedenceTab + COMMA_PRECEDENCE + 1));
				}
				while (lexer.getCurrToken().getType() ==
					Lexer::TokenType::COMMA);
				// If there's another comma, there are more
				if (lexer.getCurrToken().getType() != closingToken)
					throw ParsingError( // If parenthesis didn't close
						(closingToken == Lexer::TokenType::R_PAREN)
							? (")")
							: ("]") + std::string(" expected."),
						lexer.getCurrToken().getPos());
				lexer.scanToken();
			}
			else lexer.scanToken(2);
			// If we have "()", just skip both parentheses
			node = new nAryNode(node, currGroup->findOp[currToken], nOperands,
			                    pos);
		}
		else if (lexer.getCurrToken().getType() == Lexer::TokenType::DOT)
		{
			// This is just like parsebinleft
			const size_t pos = lexer.getCurrToken().getPos();
			lexer.scanToken();
			ASTNode* nodeB = (this->*(currGroup + 1)->
				parserFunc)(currGroup + 1);
			ASTNode* tmpNode = new BinaryNode(node, nodeB,
			                                  OperatorType::MEMBER_ACCESS, pos);
			node = tmpNode;
		}
		else
			return node;
	}
}

Statement* Parser::parseFunctionDef()
{
	// Follows the grammar:
	// function [ID]( ε | [ID]{, [ID]} ) ( i.e. foo(), or func(a, b) )
	const size_t pos = lexer.getCurrToken().getPos();
	lexer.scanToken();
	// The function name is an ID node
	if (lexer.getCurrToken().getType() != Lexer::TokenType::ID)
		throw ParsingError("Token is not an identifier.",
		                   lexer.getCurrToken().getPos());
	// Store the node
	ASTNode* funcIdNode = new IDNode(lexer.getCurrToken().getLexeme(),
	                                 lexer.getCurrToken().getPos());
	lexer.scanToken();

	// Left parenthesis expected before parameter listing
	if (lexer.getCurrToken().getType() != Lexer::TokenType::L_PAREN)
		throw ParsingError("( expected.", lexer.getCurrToken().getPos());

	std::vector<ASTNode*> paramVec; // Will hold nodes of parameters
	if (lexer.lookForw(1).getType() != Lexer::TokenType::R_PAREN)
	{
		do
		{
			lexer.scanToken();
			// Get parameter ID nodes and put them in vector
			if (lexer.getCurrToken().getType() != Lexer::TokenType::ID)
				throw ParsingError("Token is not an identifier.",
				                   lexer.getCurrToken().getPos());
			paramVec.push_back(new IDNode(lexer.getCurrToken().getLexeme(),
			                              lexer.getCurrToken().getPos()));
			lexer.scanToken();
		}
		// Continue while there are more commas
		while (lexer.getCurrToken().getType() == Lexer::TokenType::COMMA);
	}
	else lexer.scanToken();
	
	// Should close with ')'
	if (lexer.getCurrToken().getType() != Lexer::TokenType::R_PAREN)
		throw ParsingError(") expected - matching parentheses not found.",
		                   lexer.getCurrToken().getPos());
	lexer.scanToken();
	checkNewLine();
	CodeBlock* block = parseBlock(); // This is the actual function code
	return new FunctionDefStatement(funcIdNode, paramVec, block, pos);
}


ASTNode* Parser::parseUnaryPostfix(precedenceGroup* currGroup)
{
	// Parses unary postfix operation with production
	// { E -> E[op], E -> T }, hence E -> T{[op]}
	// Parse operand
	ASTNode* node = (this->*(currGroup + 1)->parserFunc)(currGroup + 1);
	while (true) // While there are consecutive unary postfix operators 
	{
		Lexer::TokenType currToken = lexer.getCurrToken().getType();
		if (currGroup->findOp.contains(currToken)) 
		{ // If current token corresponds to such an operator
			const size_t pos = lexer.getCurrToken().getPos();
			lexer.scanToken();
			// Old node becomes child
			node = new UnaryNode(node, currGroup->findOp[currToken], pos);
		}
		else
			return node;
	}
}

ASTNode* Parser::parsePrimary(precedenceGroup*)
{
	// Parses literals, identifiers, and parentheses
	ASTNode* node;
	const size_t pos = lexer.getCurrToken().getPos();
	// This lambda is used for array list initialization
	auto listParser = [&pos, this]() 
	{
		std::vector<ASTNode*> nOperands;
		if (lexer.lookForw(1).getType() != Lexer::TokenType::R_SQ_BRACKET)
		// For the case where the list is empty "[]"
		{
			do
			{
				lexer.scanToken();
				nOperands.push_back(
					// Parse an expression, but parse above the comma precedence (comma
					// has another meaning here - it separates expressions and it isn't
					// an operator)
					(this->*precedenceTab[COMMA_PRECEDENCE + 1].parserFunc)(
						precedenceTab + COMMA_PRECEDENCE + 1));
			}
			while (lexer.getCurrToken().getType() == Lexer::TokenType::COMMA);
			// If there's another comma, there are more
			if (lexer.getCurrToken().getType() !=
				Lexer::TokenType::R_SQ_BRACKET)
				throw ParsingError(std::string("] expected."),
				                   lexer.getCurrToken().getPos());
			lexer.scanToken();
		}
		else lexer.scanToken(2); // If we have "[]", just skip both brackets
		return nOperands;
	};

	switch (lexer.getCurrToken().getType())
	{ // Create a node and load it according to the token's lexeme
	case Lexer::TokenType::TRUE_LIT:
		node = new LiteralNode(true, pos);
		lexer.scanToken();
		break;
	case Lexer::TokenType::FALSE_LIT:
		node = new LiteralNode(false, pos);
		lexer.scanToken();
		break;
	case Lexer::TokenType::INT_LIT: // If number literal, convert lexeme to int
		node = new LiteralNode(std::stoi(lexer.getCurrToken().getLexeme()),
		                       pos);
		lexer.scanToken();
		break;
	case Lexer::TokenType::FLOAT_LIT:
		// If number literal, convert lexeme to int
		node = new LiteralNode(std::stof(lexer.getCurrToken().getLexeme()),
		                       pos);
		lexer.scanToken();
		break;
	case Lexer::TokenType::CHAR_LIT: // If number literal, convert lexeme to int
		node = new LiteralNode(lexer.getCurrToken().getLexeme()[0], pos);
		lexer.scanToken();
		break;

	case Lexer::TokenType::STRING_LIT:
		node = new LiteralNode( // Strings are saved in StringContainers
			std::make_shared<StringContainer>(lexer.getCurrToken().getLexeme()),
			pos);
		lexer.scanToken();
		break;
	case Lexer::TokenType::L_PAREN:
		lexer.scanToken();
		node = (this->*precedenceTab[0].parserFunc)(precedenceTab);
	// Go back to lowest precedence
		if (node)
		{
			node->setForceRval(true);
			// (myVar) = 5 should not be valid, even if myVar = 5 is
		}
		if (lexer.getCurrToken().getType() == Lexer::TokenType::R_PAREN)
		{
			lexer.scanToken();
		}
		else
		{
			// If it doesn't end with a matching parenthesis, error
			node = nullptr;
			throw ParsingError(") expected - matching parentheses not found.",
			                   lexer.getCurrToken().getPos());
		}
		break;
	case Lexer::TokenType::L_SQ_BRACKET:
		node = new nAryNode(nullptr, OperatorType::LIST_INIT, listParser(),
		                    pos);
		break;
	case Lexer::TokenType::ID: // For object identifiers
		node = new IDNode(lexer.getCurrToken().getLexeme(), pos);
		lexer.scanToken();
		break;
	default:
		node = nullptr;
		break;
	}
	if (!node) // If the node pointer is invalid, something is terribly wrong
	{
		throw ParsingError("", lexer.getCurrToken().getPos());
	}
	return node;
}
