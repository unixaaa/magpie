#pragma once

#include <iostream>

#include "Macros.h"
#include "MagpieString.h"
#include "Memory.h"

namespace magpie
{
  // The different types of Tokens that make up Magpie source code.
  enum TokenType
  {
    // Punctuators.
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACKET,
    TOKEN_RIGHT_BRACKET,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_COMMA,
    TOKEN_EQUALS,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_LESS_THAN,
    
    // Keywords.
    TOKEN_AND,
    TOKEN_CASE,
    TOKEN_CATCH,
    TOKEN_DEF,
    TOKEN_DO,
    TOKEN_END,
    TOKEN_ELSE,
    TOKEN_FALSE,
    TOKEN_FOR,
    TOKEN_IF,
    TOKEN_IS,
    TOKEN_MATCH,
    TOKEN_NOT,
    TOKEN_NOTHING,
    TOKEN_OR,
    TOKEN_RETURN,
    TOKEN_THEN,
    TOKEN_THROW,
    TOKEN_TRUE,
    TOKEN_VAL,
    TOKEN_VAR,
    TOKEN_WHILE,
    TOKEN_XOR,

    TOKEN_FIELD,
    TOKEN_NAME,
    TOKEN_NUMBER,
    TOKEN_STRING,

    TOKEN_LINE,
    TOKEN_ERROR,
    TOKEN_EOF,
    
    TOKEN_NUM_TYPES
  };

  // A span of source code associated with some Token or AST.
  class SourcePos
  {
  public:
    SourcePos(const char* file, int startLine, int startCol,
              int endLine, int endCol);
    
    const char* file() const { return file_; }
    int startLine() const { return startLine_; }
    int startCol() const { return startCol_; }
    int endLine() const { return endLine_; }
    int endCol() const { return endCol_; }
    
    SourcePos spanTo(const SourcePos& end) const;

  private:
    // The name of the source file containing this position.
    const char* file_;
    
    int startLine_;
    int startCol_;
    int endLine_;
    int endCol_;
  };
  
  // A single meaningful Token of source code. Generated by the Lexer, and
  // consumed by the Parser.
  class Token : public Managed
  {
  public:
    Token(TokenType type, gc<String> text, const SourcePos& pos);
    
    static const char* typeString(TokenType type);
    
    TokenType         type() const { return type_; }
    gc<String>        text() const { return text_; }
    const SourcePos&  pos()  const { return pos_; }
    
    // Gets whether this token is of the given type.
    bool is(TokenType type) const { return type_ == type; }
    
    virtual void reach();
    virtual void trace(std::ostream& out) const;
    
  private:
    TokenType   type_;
    gc<String>  text_;
    SourcePos   pos_;
  };
}

