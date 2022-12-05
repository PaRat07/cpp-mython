#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>
#include <iostream>
#include <cassert>

using namespace std;

namespace parse {

    bool operator==(const Token& lhs, const Token& rhs) {
        using namespace token_type;

        if (lhs.index() != rhs.index()) {
            return false;
        }
        if (lhs.Is<Char>()) {
            return lhs.As<Char>().value == rhs.As<Char>().value;
        }
        if (lhs.Is<Number>()) {
            return lhs.As<Number>().value == rhs.As<Number>().value;
        }
        if (lhs.Is<String>()) {
            return lhs.As<String>().value == rhs.As<String>().value;
        }
        if (lhs.Is<Id>()) {
            return lhs.As<Id>().value == rhs.As<Id>().value;
        }
        return true;
    }

    bool operator!=(const Token& lhs, const Token& rhs) {
        return !(lhs == rhs);
    }

    std::ostream& operator<<(std::ostream& os, const Token& rhs) {
        using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

        VALUED_OUTPUT(Number);
        VALUED_OUTPUT(Id);
        VALUED_OUTPUT(String);
        VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

        UNVALUED_OUTPUT(Class);
        UNVALUED_OUTPUT(Return);
        UNVALUED_OUTPUT(If);
        UNVALUED_OUTPUT(Else);
        UNVALUED_OUTPUT(Def);
        UNVALUED_OUTPUT(Newline);
        UNVALUED_OUTPUT(Print);
        UNVALUED_OUTPUT(Indent);
        UNVALUED_OUTPUT(Dedent);
        UNVALUED_OUTPUT(And);
        UNVALUED_OUTPUT(Or);
        UNVALUED_OUTPUT(Not);
        UNVALUED_OUTPUT(Eq);
        UNVALUED_OUTPUT(NotEq);
        UNVALUED_OUTPUT(LessOrEq);
        UNVALUED_OUTPUT(GreaterOrEq);
        UNVALUED_OUTPUT(None);
        UNVALUED_OUTPUT(True);
        UNVALUED_OUTPUT(False);
        UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

        return os << "Unknown token :("sv;
    }

    Lexer::Lexer(std::istream& input)
        : in_(input)
    {
        do {
            NextToken();
        } while (current_token_.Is<token_type::Newline>() && !current_token_.Is<token_type::Eof>());
    }

    const Token& Lexer::CurrentToken() const {
        return current_token_;
    }

    Token Lexer::NextToken() {
        if (current_token_.Is<token_type::Eof>()) {
            return current_token_;
        }

        const char next_sym = in_.peek();

        if (in_.peek() == -1) {
            if (current_token_.Is<token_type::Newline>() || current_token_.Is<token_type::Dedent>()) {
                if (current_indents_ > indents_in_prev_line) {
                    ++indents_in_prev_line;
                    current_token_ = token_type::Indent{};
                }
                else if (current_indents_ < indents_in_prev_line) {
                    --indents_in_prev_line;
                    current_token_ = token_type::Dedent{};
                }
                else {
                    current_token_ = token_type::Eof{};
                }
            }
            else {
                current_token_ = token_type::Eof{};
                return token_type::Newline{};
            }
        }
        else if (current_indents_ > indents_in_prev_line) {
            ++indents_in_prev_line;
            current_token_ = token_type::Indent{};
        }
        else if (current_indents_ < indents_in_prev_line) {
            --indents_in_prev_line;
            current_token_ = token_type::Dedent{};
        }
        else if (next_sym == '=') {
            in_.get();
            if (in_.peek() == '=') {
                in_.get();
                current_token_ = token_type::Eq{};
            }
            else {
                current_token_ = token_type::Char{'='};
            }
        }
        else if (in_.peek() =='\'' || in_.peek() =='"') {
            current_token_ = ReadString();
        }
        else if (next_sym == '\n') {
            while (in_.peek() == '\n') {
                in_.get();
            }
            current_token_ = token_type::Newline{};

            int indents_count = 0;
            for (; in_.peek() == ' '; in_.get(), in_.get(), ++indents_count) {}
            current_indents_ = indents_count;
        }
        else if (next_sym == '\r') {
            in_.get();
            return NextToken();
        }
        else if (next_sym == '\t') {
            in_.get();
            NextToken();
        }
        else if (next_sym == ' ') {
            in_.get();
            NextToken();
        }
        else if ((next_sym >= 'a' && next_sym <= 'z') || (next_sym >= 'A' && next_sym <= 'Z') || next_sym == '_'){
            current_token_ = ReadIdOrKeyWord();
        }
        else if (next_sym <= '9' && next_sym >= '0') {
            current_token_ = ReadInt();
        }
        else if (next_sym == '#') {
            string comment;
            getline(in_, comment);
//            in_.get();
            while (in_.peek() == '\n') {
                in_.get();
            }
            if (in_.peek() == -1) {
                current_token_ = token_type::Eof{};
            }
            else {
                current_token_ = token_type::Newline{};
            }
        }
        else {
            current_token_ = ReadCompOpOrChar();
        }
        return current_token_;
    }

    Token Lexer::ReadCompOpOrChar() {
        char sym;
        in_ >> sym;
        if (in_.peek() == '=') {
            switch (sym) {
                case '=':
                    in_.get();
                    return token_type::Eq{};
                case '!':
                    in_.get();
                    return token_type::NotEq{};
                case '<':
                    in_.get();
                    return token_type::LessOrEq{};
                case '>':
                    in_.get();
                    return token_type::GreaterOrEq{};
                default:
                    return token_type::Char{sym};
            }
        }
        return token_type::Char{sym};
    }

    Token Lexer::ReadInt() {
        std::string ans;
        for (char sym; std::isdigit(in_.peek()) && in_ >> sym;) {
            ans.push_back(sym);
        }
        return token_type::Number{stoi(ans)};
    }

    Token Lexer::ReadString() {
        std::string ans;

        char sym;
        if (in_.get() == '\'') {
            bool is_previous_backslash = false;
            while (in_.peek() != '\'' || is_previous_backslash) {
                sym = in_.get();
                if (is_previous_backslash) {
                    is_previous_backslash = false;
                    switch (sym) {
                        case '\\':
                            ans.push_back('\\');
                            break;
                        case 'n':
                            ans.push_back('\n');
                            break;
                        case 'r':
                            ans.push_back('\r');
                            break;
                        case 't':
                            ans.push_back('\t');
                            break;
                        case '\'':
                            ans.push_back('\'');
                            break;
                        case '"':
                            ans.push_back('"');
                            break;
                    }
                }
                else {
                    if (sym == '\\') {
                        is_previous_backslash = true;
                    }
                    else {
                        ans.push_back(sym);
                    }
                }
            }
        }
        else {
            bool is_previous_backslash = false;
            while (in_.peek() != '"' || is_previous_backslash) {
                sym = in_.get();
                if (is_previous_backslash) {
                    is_previous_backslash = false;
                    switch (sym) {
                        case '\\':
                            ans.push_back('\\');
                            break;
                        case 'n':
                            ans.push_back('\n');
                            break;
                        case 'r':
                            ans.push_back('\r');
                            break;
                        case 't':
                            ans.push_back('\t');
                            break;
                        case '"':
                            ans.push_back('"');
                            break;
                        case '\'':
                            ans.push_back('\'');
                            break;
                    }
                }
                else {
                    if (sym == '\\') {
                        is_previous_backslash = true;
                    }
                    else {
                        ans.push_back(sym);
                    }
                }
            }
        }
        in_.get();
        return token_type::String{ans};
    }

    Token Lexer::ReadIdOrKeyWord() {
        std::string command;
        for (char next_sym = in_.peek(); (in_.peek() != -1) && ((next_sym >= 'a' && next_sym <= 'z') || (next_sym >= 'A' && next_sym <= 'Z') || next_sym == '_' || (next_sym >= '0' && next_sym <= '9')); next_sym = in_.peek()) {
            command.push_back(in_.get());
        }
        if (command == "class") {
            return {parse::token_type::Class{}};
        }
        else if (command == "return") {
            return {parse::token_type::Return{}};
        }
        else if (command == "if") {
            return {parse::token_type::If{}};
        }
        else if (command == "else") {
            return {parse::token_type::Else{}};
        }
        else if (command == "def") {
            return {parse::token_type::Def{}};
        }
        else if (command == "print") {
            return {parse::token_type::Print{}};
        }
        else if (command == "or") {
            return {parse::token_type::Or{}};
        }
        else if (command == "None") {
            return {parse::token_type::None{}};
        }
        else if (command == "and") {
            return {parse::token_type::And{}};
        }
        else if (command == "not") {
            return {parse::token_type::Not{}};
        }
        else if (command == "True") {
            return {parse::token_type::True{}};
        }
        else if (command == "False") {
            return {parse::token_type::False{}};
        }
        else {
            return {token_type::Id{command}};
        }
    }

}  // namespace parse