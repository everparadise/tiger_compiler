%filenames = "scanner"

 /*
  * Please don't modify the lines above.
  */

 /* You can add lex definitions here. */
 /* TODO: Put your lab2 code here */

%x COMMENT STR IGNORE

%%

 /*
  * Below is examples, which you can wipe out
  * and write regular expressions and actions of your own.
  *
  * All the tokens:
  *   Parser::ID  √
  *   Parser::STRING
  *   Parser::INT .
  *   Parser::COMMA √
  *   Parser::COLON √
  *   Parser::SEMICOLON .
  *   Parser::LPAREN √
  *   Parser::RPAREN √
  *   Parser::LBRACK √
  *   Parser::RBRACK √
  *   Parser::LBRACE √
  *   Parser::RBRACE √
  *   Parser::DOT √
  *   Parser::PLUS √
  *   Parser::MINUS √
  *   Parser::TIMES .
  *   Parser::DIVIDE . 
  *   Parser::EQ .
  *   Parser::NEQ .
  *   Parser::LT .
  *   Parser::LE .
  *   Parser::GT .
  *   Parser::GE . 
  *   Parser::AND .
  *   Parser::OR .
  *   Parser::ASSIGN .
  *   Parser::ARRAY .
  *   Parser::IF. 
  *   Parser::THEN . 
  *   Parser::ELSE .
  *   Parser::WHILE.
  *   Parser::FOR .
  *   Parser::TO .
  *   Parser::DO .
  *   Parser::LET .
  *   Parser::IN .
  *   Parser::END .
  *   Parser::OF .
  *   Parser::BREAK  .
  *   Parser::NIL .
  *   Parser::FUNCTION .
  *   Parser::VAR .
  *   Parser::TYPE .
  */

 /* reserved words */
 /* TODO: Put your lab2 code here */

<INITIAL>{
  if {adjust(); return Parser::IF;}
  to {adjust(); return Parser::TO;}
  in {adjust(); return Parser::IN;}
  do {adjust(); return Parser::DO;}
  of {adjust(); return Parser::OF;}
  var {adjust(); return Parser::VAR;}
  end {adjust(); return Parser::END;}
  nil {adjust(); return Parser::NIL;}
  for {adjust(); return Parser::FOR;}
  let {adjust(); return Parser::LET;}
  type {adjust(); return Parser::TYPE;}
  else {adjust(); return Parser::ELSE;}
  then {adjust(); return Parser::THEN;}
  break {adjust(); return Parser::BREAK;}
  array {adjust(); return Parser::ARRAY;}
  while {adjust(); return Parser::WHILE;}
  function {adjust(); return Parser::FUNCTION;}
  , {adjust(); return Parser::COMMA;}
  : {adjust(); return Parser::COLON;}
  "(" {adjust(); return Parser::LPAREN;}
  ")" {adjust(); return Parser::RPAREN;}
  ; {adjust(); return Parser::SEMICOLON;}
  R"([)" {adjust(); return Parser::LBRACK;}
  R"(])" {adjust(); return Parser::RBRACK;}
  R"({)" {adjust(); return Parser::LBRACE;}
  R"(})" {adjust(); return Parser::RBRACE;}
  R"(.)" {adjust(); return Parser::DOT;}
  [a-zA-Z][a-z0-9A-Z_]* {adjust(); return Parser::ID;}
  R"(+)" {adjust(); return Parser::PLUS;}
  R"(-)" {adjust(); return Parser::MINUS;}
  R"(*)" {adjust(); return Parser::TIMES;}
  R"(/)" {adjust(); return Parser::DIVIDE;}
  = {adjust(); return Parser::EQ;}
  := {adjust(); return Parser::ASSIGN;}
  R"(&)" {adjust(); return Parser::AND;}
  R"(|)" {adjust(); return Parser::OR;}
  R"(>=)" {adjust(); return Parser::GE;}
  R"(<=)" {adjust(); return Parser::LE;}
  R"(<>)" {adjust(); return Parser::NEQ;}
  R"(<)" {adjust(); return Parser::LT;}
  R"(>)" {adjust(); return Parser::GT;}
  \" {adjust(); begin(StartCondition_::STR); string_buf_.clear();}
  R"(/*)" {adjust();comment_level_=1; begin(StartCondition_::COMMENT);}
  [0-9]+ {adjust(); return Parser::INT;}
  <<EOF>> {if(errormsg_->AnyErrors()) exit(1);return 0;}
  \n {adjust(); errormsg_->Newline();}
}

<IGNORE>{
  [ \t\n] {adjustStr(); ifNewLine();}
  R"(\)" {adjustStr(); begin(StartCondition_::STR);}
  <<EOF>> {Error(errormsg_->tok_pos_,"unclosed ignore sequence"); exit(1);}
  . { Error(errormsg_->tok_pos_, "illegal character in ignore sequence");}
}

<STR>{
  R"(\n)" { adjustStr(); string_buf_+='\n'; ifNewLine();}
  R"(\t)" {adjustStr(); string_buf_+='\t';}
  \\\^[A-Z] { adjustStr(); char matchedChar = matched()[2] - 'A' + 1; string_buf_+=matchedChar;}
  \\[0-9]{3} {adjustStr(); string_buf_ += char(std::stoi(matched().substr(1,3)));}
  R"(\@)" {adjustStr(); string_buf_+= (char)(0);}
  \\[\"\\] {adjustStr(); string_buf_ += matched()[1];}
  \\[ \n\t] {adjustStr(); begin(StartCondition_::IGNORE); ifNewLine(1);}
  \" {adjustStr(); setMatched(string_buf_); begin(StartCondition_::INITIAL);return Parser::STRING;}
  <<EOF>> {Error(errormsg_->tok_pos_, "unclosed string"); exit(1);}
  . {adjustStr(); string_buf_.append(matched());}
}

/* comment condition
  handle nested comment by operate <comment_level_>
  which represents the number of unmatched /*
*/
<COMMENT>{
  \n {adjustStr(); errormsg_->Newline();}
  R"(/*)"  {adjustStr(); comment_level_++;}
  R"(*/)"  {adjustStr(); if(--comment_level_ == 0){ begin(StartCondition_::INITIAL); }}
  <<EOF>> {Error(errormsg_->tok_pos_, "unterminated comment symbol"); exit(1);}
  .   {adjustStr();}
}
 /*
  * skip white space chars.
  * space, tabs and LF
  */
[ \t]+ {adjust();}


 /* illegal input */
. {adjust(); Error(errormsg_->tok_pos_, "illegal token");}
