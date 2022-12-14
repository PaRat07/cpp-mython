#pragma once

#include <iosfwd>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>

namespace parse {

    namespace token_type {
        struct Number {  // Лексема «число»
            int value;   // число
        };

        struct Id {             // Лексема «идентификатор»
            std::string value;  // Имя идентификатора
        };

        struct Char {    // Лексема «символ»
            char value;  // код символа
        };

        struct String {  // Лексема «строковая константа»
            std::string value;
        };

        struct Class {};    // Лексема «class»
        struct Return {};   // Лексема «return»
        struct If {};       // Лексема «if»
        struct Else {};     // Лексема «else»
        struct Def {};      // Лексема «def»
        struct Newline {};  // Лексема «конец строки»
        struct Print {};    // Лексема «print»
        struct Indent {};  // Лексема «увеличение отступа», соответствует двум пробелам
        struct Dedent {};  // Лексема «уменьшение отступа»
        struct Eof {};     // Лексема «конец файла»
        struct And {};     // Лексема «and»
        struct Or {};      // Лексема «or»
        struct Not {};     // Лексема «not»
        struct Eq {};      // Лексема «==»
        struct NotEq {};   // Лексема «!=»
        struct LessOrEq {};     // Лексема «<=»
        struct GreaterOrEq {};  // Лексема «>=»
        struct None {};         // Лексема «None»
        struct True {};         // Лексема «True»
        struct False {};        // Лексема «False»
    }  // namespace token_type

    using TokenBase
            = std::variant<token_type::Number, token_type::Id, token_type::Char, token_type::String,
            token_type::Class, token_type::Return, token_type::If, token_type::Else,
            token_type::Def, token_type::Newline, token_type::Print, token_type::Indent,
            token_type::Dedent, token_type::And, token_type::Or, token_type::Not,
            token_type::Eq, token_type::NotEq, token_type::LessOrEq, token_type::GreaterOrEq,
            token_type::None, token_type::True, token_type::False, token_type::Eof>;

    struct Token : TokenBase {
        using TokenBase::TokenBase;

        template <typename T>
        [[nodiscard]] bool Is() const {
            return std::holds_alternative<T>(*this);
        }

        template <typename T>
        [[nodiscard]] const T& As() const {
            return std::get<T>(*this);
        }

        template <typename T>
        [[nodiscard]] const T* TryAs() const {
            return std::get_if<T>(this);
        }
    };

    bool operator==(const Token& lhs, const Token& rhs);
    bool operator!=(const Token& lhs, const Token& rhs);

    std::ostream& operator<<(std::ostream& os, const Token& rhs);

    class LexerError : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    class Lexer {
    public:
        explicit Lexer(std::istream& input);

        // Возвращает ссылку на текущий токен или token_type::Eof, если поток токенов закончился
        [[nodiscard]] const Token& CurrentToken() const;

        // Возвращает следующий токен, либо token_type::Eof, если поток токенов закончился
        Token NextToken();

        // Если текущий токен имеет тип T, метод возвращает ссылку на него.
        // В противном случае метод выбрасывает исключение LexerError
        template <typename T>
        const T& Expect() const {
            if (current_token_.Is<T>()) {
                return current_token_.As<T>();
            }
            else {
                throw LexerError("incorrect token type");
            }
        }

        // Метод проверяет, что текущий токен имеет тип T, а сам токен содержит значение value.
        // В противном случае метод выбрасывает исключение LexerError
        template <typename T, typename U>
        void Expect(const U& value) const {
            if (!current_token_.Is<T>()) {
                throw LexerError("incorrect token type");
            }
            else if (current_token_.As<T>().value != value) {
                throw LexerError("incorrect token value");
            }
        }

        // Если следующий токен имеет тип T, метод возвращает ссылку на него.
        // В противном случае метод выбрасывает исключение LexerError
        template <typename T>
        const T& ExpectNext() {
            NextToken();
            if (current_token_.Is<T>()) {
                return current_token_.As<T>();
            }
            else {
                throw LexerError("incorrect token type");
            }
        }

        // Метод проверяет, что следующий токен имеет тип T, а сам токен содержит значение value.
        // В противном случае метод выбрасывает исключение LexerError
        template <typename T, typename U>
        void ExpectNext(const U& value) {
            NextToken();
            if (current_token_.Is<T>()) {
                if (current_token_.As<T>().value != value) {
                    throw LexerError("incorrect token value");
                }
            }
            else {
                throw LexerError("incorrect token type");
            }
        }

    private:
        Token current_token_;
        std::istream &in_;
        int indents_in_prev_line = 0;
        int current_indents_ = 0;
        Token ReadInt();

        Token ReadString();

        Token ReadIdOrKeyWord() {
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

        Token ReadCompOpOrChar();
    };

}  // namespace parse