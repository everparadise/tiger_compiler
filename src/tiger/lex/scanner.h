#ifndef TIGER_LEX_SCANNER_H_
#define TIGER_LEX_SCANNER_H_

#include <algorithm>
#include <cstdarg>
#include <iostream>
#include <string>

#include "scannerbase.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/parse/parserbase.h"

class Scanner : public ScannerBase {
public:
  Scanner() = delete;
  explicit Scanner(std::string_view fname, std::ostream &out = std::cout)
      : ScannerBase(std::cin, out), comment_level_(1), char_pos_(1),
        errormsg_(std::make_unique<err::ErrorMsg>(fname)) {
    switchStreams(errormsg_->infile_, out);
  }

  /**
   * Output an error
   * @param message error message
   */
  void Error(int pos, std::string message, ...) {
    va_list ap;
    va_start(ap, message);
    errormsg_->Error(pos, message, ap);
    va_end(ap);
  }

  /**
   * Getter for `tok_pos_`
   */
  [[nodiscard]] int GetTokPos() const { return errormsg_->GetTokPos(); }

  /**
   * Transfer the ownership of `errormsg_` to the outer scope
   * @return unique pointer to errormsg
   */
  [[nodiscard]] std::unique_ptr<err::ErrorMsg> TransferErrormsg() {
    return std::move(errormsg_);
  }

  int lex();

private:
  int comment_level_;
  std::string string_buf_;
  int char_pos_;
  std::unique_ptr<err::ErrorMsg> errormsg_;

  /**
   * NOTE: do not change all the funtion signature below, which is used by
   * flexc++ internally
   */
  int lex_();
  int executeAction_(size_t ruleNr);

  void print();
  void preCode();
  void postCode(PostEnum_ type);
  void adjust();
  void adjustStr();
  void ifNewLine(int i);
};

inline int Scanner::lex() { return lex_(); }

inline void Scanner::preCode() {
  // Optionally replace by your own code
}

inline void Scanner::postCode(PostEnum_ type) {
  // Optionally replace by your own code
}

inline void Scanner::print() { print_(); }

inline void Scanner::adjust() {
  //std::cout << matched() << ":" << char_pos_  << "+ " << length() << std::endl; 
  errormsg_->tok_pos_ = char_pos_;
  char_pos_ += length();
}

inline void Scanner::adjustStr() { 
  //std::cout << matched() << ":" << char_pos_  << "+ " << length() << std::endl;
  char_pos_ += length();
}

inline void Scanner::ifNewLine(int i = 0){
    if(matched()[i] == '\n') errormsg_->Newline();
}

#endif // TIGER_LEX_SCANNER_H_
