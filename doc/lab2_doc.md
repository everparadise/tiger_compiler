
# lab2 document
- [lab2 document](#lab2-document)
    - [handle comments](#handle-comments)
    - [handle strings](#handle-strings)
    - [handle error](#handle-error)
    - [handle EOF](#handle-eof)
### handle comments

[back to header](#lab2-document)
1. 当在**INITIAL**状态匹配到/\*时进入**COMMENT**状态。
2. 为了支持嵌套comment，需要使用变量**comment_level**维护嵌套深度
   1. 每次遇到/\*时增加嵌套深度
   2. 遇到\*/时降低嵌套深度。
   3. 嵌套深度为0时恢复**INITIAL**状态
3. 在文件结束时仍处于**COMMENT**状态，表明comment未匹配，报错并退出
4. 对于\n换行符控制errormsg_换行，维持文件行数匹配
5. 其余任何字符均不作处理，简单的移动位置
```
<INITIAL>R"(/*)" {
    adjust();
    comment_level_=1; 
    begin(StartCondition_::COMMENT);
}
<COMMENT>{
    \n {adjustStr(); errormsg_->Newline();}
    R"(/*)" {adjustStr(); comment_level_++;}
    R"(*/)" {
        adjustStr(); 
        if(--comment_level_ == 0){ 
            begin(StartCondition_::INITIAL); 
        }
    }
    <<EOF>> {
        Error(errormsg_->tok_pos_, "unterminated comment symbol"); 
        return 0;
    }
    .   {adjustStr();}
}
```
<a name="handle-string"></a>
### handle strings
[back to header](#lab2-document)
在**INITIAL**状态下，匹配到"进入**STR**模式进行字符串匹配，需要注意要对string_buf_进行清空，避免上次匹配的数据重复混乱
在**STR**模式下除可打印字符外，特殊字符如下
* \n \t \" \\转换为对应的转义字符并添加到string_buf_中(\n需要保证errormsg_换行,使用工具函数ifNewLine更新行数)
* \^X 控制字符，根据A-Z转换为ASCII 1-26的字符,\@转换为(char)0
* \ddd 3位十进制数stoi转换为对应的char字符
* \<\<EOF\>\>时报错并退出
* 其余正常字符将其添加到string_buf_中
* " 结束时返回**INITIAL**状态并解析为Parser::STRING类型
* \之后跟空白字符时进入**IGNORE**状态，忽略其中所有字符(不加入string_buf_,仍需注意\n需要触发errormsg_NewLine())
  * **IGNORE**状态中只需要对\n触发NewLine()，其余任何字符不做处理，直到\重新出现返回**STR**状态
```
<STR>{
  R"(\n)" { adjustStr(); string_buf_+='\n'; ifNewLine();}
  R"(\t)" {adjustStr(); string_buf_+='\t';}
  \\\^[A-Z] { adjustStr(); char matchedChar = matched()[2] - 'A' + 1; string_buf_+=matchedChar;}
  \\[0-9]{3} {adjustStr(); string_buf_ += char(std::stoi(matched().substr(1,3)));}
  R"(\@)" {adjustStr(); string_buf_+= (char)(0);}
  \\[\"\\] {adjustStr(); string_buf_ += matched()[1];}
  \\[ \n\t] {adjustStr(); begin(StartCondition_::IGNORE); ifNewLine(1);}
  \" {adjustStr(); setMatched(string_buf_); begin(StartCondition_::INITIAL);return Parser::STRING;}
  <<EOF>> {Error(errormsg_->tok_pos_, "unclosed string"); return 0;}
  . {adjustStr(); string_buf_.append(matched());}
}

<IGNORE>{
  [ \t\n] {adjustStr(); ifNewLine();}
  R"(\)" {adjustStr(); begin(StartCondition_::STR);}
  <<EOF>> {Error(errormsg_->tok_pos_,"unclosed ignore sequence");return 0;}
  . { Error(errormsg_->tok_pos_, "illegal character in");}
}

```
<a name="handle-error"></a>
### handle error
[back to header](#lab2-document)
遇到错误时调用Error方法报告错误并继续匹配，错误类型包括
* illegal token
* unterminated comment symbol
* unclosed string
* unclosed ignore sequence
* illegal token in **IGNORE** sequence

例如
```
. {adjust(); Error(errormsg_->tok_pos_, "illegal token");}
```

<a name="handle-eof"></a>
### handle EOF
[back to header](#lab2-document)
**INITIAL**状态下，在EOF时检查errormsg_->AnyError()是否遇到过错误，如果发生过错误则exit并返回错误码，否则return 0正常退出。
```
<<EOF>> {if(errormsg_->AnyErrors()) exit(1);return 0;}
```
其余状态下，EOF均为unclosed / unterminated state，报错并exit返回错误码
例如**COMMENT**状态下
```
<COMMENT>{
  \n {adjustStr(); errormsg_->Newline();}
  R"(/*)"  {adjustStr(); comment_level_++;}
  R"(*/)"  {adjustStr(); if(--comment_level_ == 0){ begin(StartCondition_::INITIAL); }}
  <<EOF>> {Error(errormsg_->tok_pos_, "unterminated comment symbol"); exit(1);}
  .   {adjustStr();}
}
```