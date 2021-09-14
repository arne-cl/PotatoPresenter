
// Generated from potato.g4 by ANTLR 4.9.2

#pragma once


#include "antlr4-runtime.h"
#include "potatoListener.h"


/**
 * This class provides an empty implementation of potatoListener,
 * which can be extended to create a listener which only needs to handle a subset
 * of the available methods.
 */
class  potatoBaseListener : public potatoListener {
public:

  virtual void enterPotato(potatoParser::PotatoContext * /*ctx*/) override { }
  virtual void exitPotato(potatoParser::PotatoContext * /*ctx*/) override { }

  virtual void enterBox(potatoParser::BoxContext * /*ctx*/) override { }
  virtual void exitBox(potatoParser::BoxContext * /*ctx*/) override { }

  virtual void enterCommand(potatoParser::CommandContext * /*ctx*/) override { }
  virtual void exitCommand(potatoParser::CommandContext * /*ctx*/) override { }

  virtual void enterProperty(potatoParser::PropertyContext * /*ctx*/) override { }
  virtual void exitProperty(potatoParser::PropertyContext * /*ctx*/) override { }

  virtual void enterProperty_entry(potatoParser::Property_entryContext * /*ctx*/) override { }
  virtual void exitProperty_entry(potatoParser::Property_entryContext * /*ctx*/) override { }

  virtual void enterValue(potatoParser::ValueContext * /*ctx*/) override { }
  virtual void exitValue(potatoParser::ValueContext * /*ctx*/) override { }

  virtual void enterParagraph(potatoParser::ParagraphContext * /*ctx*/) override { }
  virtual void exitParagraph(potatoParser::ParagraphContext * /*ctx*/) override { }

  virtual void enterText(potatoParser::TextContext * /*ctx*/) override { }
  virtual void exitText(potatoParser::TextContext * /*ctx*/) override { }

  virtual void enterText_colon(potatoParser::Text_colonContext * /*ctx*/) override { }
  virtual void exitText_colon(potatoParser::Text_colonContext * /*ctx*/) override { }


  virtual void enterEveryRule(antlr4::ParserRuleContext * /*ctx*/) override { }
  virtual void exitEveryRule(antlr4::ParserRuleContext * /*ctx*/) override { }
  virtual void visitTerminal(antlr4::tree::TerminalNode * /*node*/) override { }
  virtual void visitErrorNode(antlr4::tree::ErrorNode * /*node*/) override { }

};

