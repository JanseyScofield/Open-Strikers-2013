#ifndef NLOHMANN_JSON_HPP_
#define NLOHMANN_JSON_HPP_

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <istream>
#include <iterator>
#include <map>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace nlohmann {

class json {
public:
    using object_t = std::map<std::string, json>;
    using array_t = std::vector<json>;
    using string_t = std::string;
    using boolean_t = bool;
    using number_integer_t = std::int64_t;
    using number_unsigned_t = std::uint64_t;
    using number_float_t = double;

    enum class value_t : std::uint8_t {
        null,
        object,
        array,
        string,
        boolean,
        number_integer,
        number_unsigned,
        number_float
    };

    class exception : public std::runtime_error {
    public:
        explicit exception(const std::string& msg) : std::runtime_error(msg) {}
    };

    class parse_error : public exception {
    public:
        explicit parse_error(const std::string& msg) : exception("json.exception.parse_error: " + msg) {}
    };

    class type_error : public exception {
    public:
        explicit type_error(const std::string& msg) : exception("json.exception.type_error: " + msg) {}
    };

    class out_of_range : public exception {
    public:
        explicit out_of_range(const std::string& msg) : exception("json.exception.out_of_range: " + msg) {}
    };

private:
    using storage_t = std::variant<
        std::monostate,
        object_t,
        array_t,
        string_t,
        boolean_t,
        number_integer_t,
        number_unsigned_t,
        number_float_t
    >;

    storage_t data_{std::monostate{}};

public:
    json() noexcept = default;
    json(std::nullptr_t) noexcept : data_(std::monostate{}) {}
    json(const object_t& v) : data_(v) {}
    json(object_t&& v) noexcept : data_(std::move(v)) {}
    json(const array_t& v) : data_(v) {}
    json(array_t&& v) noexcept : data_(std::move(v)) {}
    json(const string_t& v) : data_(v) {}
    json(string_t&& v) noexcept : data_(std::move(v)) {}
    json(std::string_view v) : data_(string_t(v)) {}
    json(const char* v) : data_(string_t(v ? v : "")) {}
    json(boolean_t v) noexcept : data_(v) {}

    template <typename IntT, std::enable_if_t<std::is_integral_v<IntT> && !std::is_same_v<IntT, bool> && std::is_signed_v<IntT>, int> = 0>
    json(IntT v) noexcept : data_(static_cast<number_integer_t>(v)) {}

    template <typename UIntT, std::enable_if_t<std::is_integral_v<UIntT> && !std::is_same_v<UIntT, bool> && std::is_unsigned_v<UIntT>, int> = 0>
    json(UIntT v) noexcept : data_(static_cast<number_unsigned_t>(v)) {}

    template <typename FloatT, std::enable_if_t<std::is_floating_point_v<FloatT>, int> = 0>
    json(FloatT v) noexcept : data_(static_cast<number_float_t>(v)) {}

    template <typename T>
    json(const std::vector<T>& vec) {
        array_t arr;
        arr.reserve(vec.size());
        for (const auto& elem : vec) {
            arr.emplace_back(elem);
        }
        data_ = std::move(arr);
    }

    template <typename T, std::size_t N>
    json(const std::array<T, N>& arr_in) {
        array_t arr;
        arr.reserve(N);
        for (const auto& elem : arr_in) {
            arr.emplace_back(elem);
        }
        data_ = std::move(arr);
    }

    static json object() { return json(object_t{}); }
    static json array() { return json(array_t{}); }

    [[nodiscard]] value_t type() const noexcept {
        return static_cast<value_t>(data_.index());
    }

    [[nodiscard]] bool is_null() const noexcept { return std::holds_alternative<std::monostate>(data_); }
    [[nodiscard]] bool is_object() const noexcept { return std::holds_alternative<object_t>(data_); }
    [[nodiscard]] bool is_array() const noexcept { return std::holds_alternative<array_t>(data_); }
    [[nodiscard]] bool is_string() const noexcept { return std::holds_alternative<string_t>(data_); }
    [[nodiscard]] bool is_boolean() const noexcept { return std::holds_alternative<boolean_t>(data_); }
    [[nodiscard]] bool is_number_integer() const noexcept {
        return std::holds_alternative<number_integer_t>(data_) ||
               std::holds_alternative<number_unsigned_t>(data_);
    }
    [[nodiscard]] bool is_number_unsigned() const noexcept {
        if (const auto* u = std::get_if<number_unsigned_t>(&data_)) {
            (void)u;
            return true;
        }
        if (const auto* i = std::get_if<number_integer_t>(&data_)) {
            return *i >= 0;
        }
        return false;
    }
    [[nodiscard]] bool is_number_float() const noexcept { return std::holds_alternative<number_float_t>(data_); }
    [[nodiscard]] bool is_number() const noexcept { return is_number_integer() || is_number_float(); }

    [[nodiscard]] std::size_t size() const noexcept {
        if (const auto* o = std::get_if<object_t>(&data_)) return o->size();
        if (const auto* a = std::get_if<array_t>(&data_)) return a->size();
        if (is_null()) return 0;
        return 1;
    }

    [[nodiscard]] bool empty() const noexcept {
        if (const auto* o = std::get_if<object_t>(&data_)) return o->empty();
        if (const auto* a = std::get_if<array_t>(&data_)) return a->empty();
        return is_null();
    }

    [[nodiscard]] bool contains(std::string_view key) const {
        if (const auto* o = std::get_if<object_t>(&data_)) {
            return o->find(std::string(key)) != o->end();
        }
        return false;
    }

    json& operator[](const std::string& key) {
        if (is_null()) {
            data_ = object_t{};
        }
        auto* o = std::get_if<object_t>(&data_);
        if (!o) {
            throw type_error("cannot use operator[](string) with non-object type");
        }
        return (*o)[key];
    }

    json& operator[](const char* key) {
        return (*this)[std::string(key)];
    }

    const json& operator[](const std::string& key) const {
        return at(key);
    }

    const json& operator[](const char* key) const {
        return at(std::string(key));
    }

    json& operator[](std::size_t idx) {
        auto* a = std::get_if<array_t>(&data_);
        if (!a) {
            throw type_error("cannot use operator[](size_t) with non-array type");
        }
        return (*a)[idx];
    }

    const json& operator[](std::size_t idx) const {
        return at(idx);
    }

    json& at(const std::string& key) {
        auto* o = std::get_if<object_t>(&data_);
        if (!o) {
            throw type_error("cannot use at(string) with non-object type");
        }
        auto it = o->find(key);
        if (it == o->end()) {
            throw out_of_range("key '" + key + "' not found");
        }
        return it->second;
    }

    const json& at(const std::string& key) const {
        const auto* o = std::get_if<object_t>(&data_);
        if (!o) {
            throw type_error("cannot use at(string) with non-object type");
        }
        auto it = o->find(key);
        if (it == o->end()) {
            throw out_of_range("key '" + key + "' not found");
        }
        return it->second;
    }

    json& at(std::size_t idx) {
        auto* a = std::get_if<array_t>(&data_);
        if (!a) {
            throw type_error("cannot use at(size_t) with non-array type");
        }
        if (idx >= a->size()) {
            throw out_of_range("array index " + std::to_string(idx) + " out of range");
        }
        return (*a)[idx];
    }

    const json& at(std::size_t idx) const {
        const auto* a = std::get_if<array_t>(&data_);
        if (!a) {
            throw type_error("cannot use at(size_t) with non-array type");
        }
        if (idx >= a->size()) {
            throw out_of_range("array index " + std::to_string(idx) + " out of range");
        }
        return (*a)[idx];
    }

    void push_back(const json& val) {
        if (is_null()) {
            data_ = array_t{};
        }
        auto* a = std::get_if<array_t>(&data_);
        if (!a) {
            throw type_error("cannot use push_back with non-array type");
        }
        a->push_back(val);
    }

    void push_back(json&& val) {
        if (is_null()) {
            data_ = array_t{};
        }
        auto* a = std::get_if<array_t>(&data_);
        if (!a) {
            throw type_error("cannot use push_back with non-array type");
        }
        a->push_back(std::move(val));
    }

    auto begin() {
        auto* a = std::get_if<array_t>(&data_);
        if (!a) {
            throw type_error("begin() called on non-array json; use items() for objects");
        }
        return a->begin();
    }

    auto end() {
        auto* a = std::get_if<array_t>(&data_);
        if (!a) {
            throw type_error("end() called on non-array json; use items() for objects");
        }
        return a->end();
    }

    auto begin() const {
        const auto* a = std::get_if<array_t>(&data_);
        if (!a) {
            throw type_error("begin() called on non-array json; use items() for objects");
        }
        return a->begin();
    }

    auto end() const {
        const auto* a = std::get_if<array_t>(&data_);
        if (!a) {
            throw type_error("end() called on non-array json; use items() for objects");
        }
        return a->end();
    }

    class items_proxy {
    public:
        struct item_ref {
            const std::string& key_ref;
            const json& val_ref;
            [[nodiscard]] const std::string& key() const noexcept { return key_ref; }
            [[nodiscard]] const json& value() const noexcept { return val_ref; }
        };

        class iterator {
            object_t::const_iterator it_;
        public:
            explicit iterator(object_t::const_iterator it) : it_(it) {}
            item_ref operator*() const { return item_ref{it_->first, it_->second}; }
            iterator& operator++() { ++it_; return *this; }
            bool operator!=(const iterator& other) const { return it_ != other.it_; }
            bool operator==(const iterator& other) const { return it_ == other.it_; }
        };

        explicit items_proxy(const object_t& obj) : obj_(obj) {}
        [[nodiscard]] iterator begin() const { return iterator(obj_.begin()); }
        [[nodiscard]] iterator end() const { return iterator(obj_.end()); }
    private:
        const object_t& obj_;
    };

    [[nodiscard]] items_proxy items() const {
        const auto* o = std::get_if<object_t>(&data_);
        if (!o) {
            throw type_error("items() requires an object value");
        }
        return items_proxy(*o);
    }

    template <typename T>
    [[nodiscard]] T get() const {
        if constexpr (std::is_same_v<T, json>) {
            return *this;
        } else if constexpr (std::is_same_v<T, std::string>) {
            const auto* s = std::get_if<string_t>(&data_);
            if (!s) throw type_error("type must be string");
            return *s;
        } else if constexpr (std::is_same_v<T, bool>) {
            const auto* b = std::get_if<boolean_t>(&data_);
            if (!b) throw type_error("type must be boolean");
            return *b;
        } else if constexpr (std::is_integral_v<T>) {
            if (const auto* i = std::get_if<number_integer_t>(&data_)) {
                return static_cast<T>(*i);
            }
            if (const auto* u = std::get_if<number_unsigned_t>(&data_)) {
                return static_cast<T>(*u);
            }
            if (const auto* f = std::get_if<number_float_t>(&data_)) {
                return static_cast<T>(*f);
            }
            throw type_error("type must be integer number");
        } else if constexpr (std::is_floating_point_v<T>) {
            if (const auto* f = std::get_if<number_float_t>(&data_)) {
                return static_cast<T>(*f);
            }
            if (const auto* i = std::get_if<number_integer_t>(&data_)) {
                return static_cast<T>(*i);
            }
            if (const auto* u = std::get_if<number_unsigned_t>(&data_)) {
                return static_cast<T>(*u);
            }
            throw type_error("type must be number");
        } else if constexpr (is_std_vector<T>::value) {
            const auto* a = std::get_if<array_t>(&data_);
            if (!a) throw type_error("type must be array");
            T result;
            result.reserve(a->size());
            for (const auto& elem : *a) {
                result.push_back(elem.get<typename T::value_type>());
            }
            return result;
        } else if constexpr (is_std_array<T>::value) {
            const auto* a = std::get_if<array_t>(&data_);
            if (!a) throw type_error("type must be array");
            T result{};
            if (a->size() != result.size()) {
                throw out_of_range("array size mismatch in get<std::array>");
            }
            for (std::size_t idx = 0; idx < result.size(); ++idx) {
                result[idx] = (*a)[idx].get<typename T::value_type>();
            }
            return result;
        } else {
            static_assert(sizeof(T) == 0, "Unsupported type in nlohmann::json::get<T>");
        }
    }

    template <typename T>
    [[nodiscard]] T value(const std::string& key, const T& default_value) const {
        const auto* o = std::get_if<object_t>(&data_);
        if (!o) {
            throw type_error("cannot use value() with non-object type");
        }
        auto it = o->find(key);
        if (it == o->end() || it->second.is_null()) {
            return default_value;
        }
        return it->second.get<T>();
    }

    [[nodiscard]] std::string value(const std::string& key, const char* default_value) const {
        return value<std::string>(key, std::string(default_value ? default_value : ""));
    }

    [[nodiscard]] std::string dump(int indent = -1) const {
        std::ostringstream oss;
        dump_to(oss, indent, 0);
        return oss.str();
    }

    static json parse(std::string_view input) {
        parser p(input);
        json result = p.parse_value();
        p.skip_whitespace();
        if (!p.eof()) {
            throw parse_error("unexpected trailing characters at byte " + std::to_string(p.pos()));
        }
        return result;
    }

    static json parse(std::istream& is) {
        std::string content((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
        return parse(std::string_view(content));
    }

    friend std::istream& operator>>(std::istream& is, json& j) {
        j = json::parse(is);
        return is;
    }

    friend std::ostream& operator<<(std::ostream& os, const json& j) {
        os << j.dump();
        return os;
    }

private:
    template <typename T>
    struct is_std_vector : std::false_type {};
    template <typename U, typename Alloc>
    struct is_std_vector<std::vector<U, Alloc>> : std::true_type {};

    template <typename T>
    struct is_std_array : std::false_type {};
    template <typename U, std::size_t N>
    struct is_std_array<std::array<U, N>> : std::true_type {};

    static void escape_string(std::ostream& os, std::string_view sv) {
        os.put('"');
        for (unsigned char c : sv) {
            switch (c) {
                case '"':  os << "\\\""; break;
                case '\\': os << "\\\\"; break;
                case '\b': os << "\\b";  break;
                case '\f': os << "\\f";  break;
                case '\n': os << "\\n";  break;
                case '\r': os << "\\r";  break;
                case '\t': os << "\\t";  break;
                default:
                    if (c < 0x20) {
                        os << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                           << static_cast<int>(c) << std::dec;
                    } else {
                        os.put(static_cast<char>(c));
                    }
                    break;
            }
        }
        os.put('"');
    }

    void dump_to(std::ostream& os, int indent, int depth) const {
        if (is_null()) {
            os << "null";
        } else if (const auto* b = std::get_if<boolean_t>(&data_)) {
            os << (*b ? "true" : "false");
        } else if (const auto* i = std::get_if<number_integer_t>(&data_)) {
            os << *i;
        } else if (const auto* u = std::get_if<number_unsigned_t>(&data_)) {
            os << *u;
        } else if (const auto* f = std::get_if<number_float_t>(&data_)) {
            os << *f;
        } else if (const auto* s = std::get_if<string_t>(&data_)) {
            escape_string(os, *s);
        } else if (const auto* a = std::get_if<array_t>(&data_)) {
            if (a->empty()) {
                os << "[]";
                return;
            }
            os.put('[');
            for (std::size_t idx = 0; idx < a->size(); ++idx) {
                if (idx > 0) os.put(',');
                if (indent >= 0) {
                    os.put('\n');
                    os << std::string(static_cast<std::size_t>((depth + 1) * indent), ' ');
                }
                (*a)[idx].dump_to(os, indent, depth + 1);
            }
            if (indent >= 0) {
                os.put('\n');
                os << std::string(static_cast<std::size_t>(depth * indent), ' ');
            }
            os.put(']');
        } else if (const auto* o = std::get_if<object_t>(&data_)) {
            if (o->empty()) {
                os << "{}";
                return;
            }
            os.put('{');
            bool first = true;
            for (const auto& [k, v] : *o) {
                if (!first) os.put(',');
                first = false;
                if (indent >= 0) {
                    os.put('\n');
                    os << std::string(static_cast<std::size_t>((depth + 1) * indent), ' ');
                }
                escape_string(os, k);
                os.put(':');
                if (indent >= 0) os.put(' ');
                v.dump_to(os, indent, depth + 1);
            }
            if (indent >= 0) {
                os.put('\n');
                os << std::string(static_cast<std::size_t>(depth * indent), ' ');
            }
            os.put('}');
        }
    }

    class parser {
    public:
        explicit parser(std::string_view src) : src_(src), pos_(0) {
            // Skip UTF-8 BOM if present
            if (src_.size() >= 3 &&
                static_cast<unsigned char>(src_[0]) == 0xEF &&
                static_cast<unsigned char>(src_[1]) == 0xBB &&
                static_cast<unsigned char>(src_[2]) == 0xBF) {
                pos_ = 3;
            }
        }

        [[nodiscard]] bool eof() const noexcept { return pos_ >= src_.size(); }
        [[nodiscard]] std::size_t pos() const noexcept { return pos_; }

        void skip_whitespace() noexcept {
            while (pos_ < src_.size()) {
                char c = src_[pos_];
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                    ++pos_;
                } else {
                    break;
                }
            }
        }

        json parse_value() {
            skip_whitespace();
            if (eof()) {
                throw parse_error("unexpected end of input");
            }
            char c = src_[pos_];
            switch (c) {
                case '{': return parse_object();
                case '[': return parse_array();
                case '"': return json(parse_string());
                case 't': return parse_true();
                case 'f': return parse_false();
                case 'n': return parse_null();
                default:
                    if (c == '-' || (c >= '0' && c <= '9')) {
                        return parse_number();
                    }
                    throw parse_error(
                        std::string("unexpected character '") + c + "' at byte " + std::to_string(pos_)
                    );
            }
        }

    private:
        std::string_view src_;
        std::size_t pos_;

        char consume() {
            if (eof()) {
                throw parse_error("unexpected end of input at byte " + std::to_string(pos_));
            }
            return src_[pos_++];
        }

        void expect(char expected) {
            skip_whitespace();
            char actual = consume();
            if (actual != expected) {
                throw parse_error(
                    std::string("expected '") + expected + "' but got '" + actual +
                    "' at byte " + std::to_string(pos_ - 1)
                );
            }
        }

        json parse_object() {
            expect('{');
            object_t obj;
            skip_whitespace();
            if (!eof() && src_[pos_] == '}') {
                ++pos_;
                return json(std::move(obj));
            }
            while (true) {
                skip_whitespace();
                if (eof() || src_[pos_] != '"') {
                    throw parse_error("expected object key string at byte " + std::to_string(pos_));
                }
                std::string key = parse_string();
                expect(':');
                json val = parse_value();
                obj.emplace(std::move(key), std::move(val));

                skip_whitespace();
                if (eof()) {
                    throw parse_error("unterminated object at byte " + std::to_string(pos_));
                }
                if (src_[pos_] == '}') {
                    ++pos_;
                    break;
                }
                expect(',');
            }
            return json(std::move(obj));
        }

        json parse_array() {
            expect('[');
            array_t arr;
            skip_whitespace();
            if (!eof() && src_[pos_] == ']') {
                ++pos_;
                return json(std::move(arr));
            }
            while (true) {
                arr.push_back(parse_value());
                skip_whitespace();
                if (eof()) {
                    throw parse_error("unterminated array at byte " + std::to_string(pos_));
                }
                if (src_[pos_] == ']') {
                    ++pos_;
                    break;
                }
                expect(',');
            }
            return json(std::move(arr));
        }

        static std::uint32_t parse_hex4(std::string_view s) {
            std::uint32_t val = 0;
            for (char c : s) {
                val <<= 4;
                if (c >= '0' && c <= '9') val |= static_cast<std::uint32_t>(c - '0');
                else if (c >= 'a' && c <= 'f') val |= static_cast<std::uint32_t>(c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') val |= static_cast<std::uint32_t>(c - 'A' + 10);
                else throw parse_error("invalid hex digit in unicode escape");
            }
            return val;
        }

        static void append_utf8(std::string& out, std::uint32_t cp) {
            if (cp <= 0x7Fu) {
                out.push_back(static_cast<char>(cp));
            } else if (cp <= 0x7FFu) {
                out.push_back(static_cast<char>(0xC0u | ((cp >> 6) & 0x1Fu)));
                out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
            } else if (cp <= 0xFFFFu) {
                out.push_back(static_cast<char>(0xE0u | ((cp >> 12) & 0x0Fu)));
                out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
                out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
            } else if (cp <= 0x10FFFFu) {
                out.push_back(static_cast<char>(0xF0u | ((cp >> 18) & 0x07u)));
                out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
                out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
                out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
            } else {
                throw parse_error("invalid unicode codepoint");
            }
        }

        std::string parse_string() {
            expect('"');
            std::string out;
            while (!eof()) {
                char c = consume();
                if (c == '"') {
                    return out;
                }
                if (c == '\\') {
                    char esc = consume();
                    switch (esc) {
                        case '"':  out.push_back('"'); break;
                        case '\\': out.push_back('\\'); break;
                        case '/':  out.push_back('/'); break;
                        case 'b':  out.push_back('\b'); break;
                        case 'f':  out.push_back('\f'); break;
                        case 'n':  out.push_back('\n'); break;
                        case 'r':  out.push_back('\r'); break;
                        case 't':  out.push_back('\t'); break;
                        case 'u': {
                            if (pos_ + 4 > src_.size()) {
                                throw parse_error("truncated \\uXXXX escape");
                            }
                            std::uint32_t cp = parse_hex4(src_.substr(pos_, 4));
                            pos_ += 4;
                            if (cp >= 0xD800u && cp <= 0xDBFFu) {
                                if (pos_ + 6 <= src_.size() && src_[pos_] == '\\' && src_[pos_ + 1] == 'u') {
                                    pos_ += 2;
                                    std::uint32_t low = parse_hex4(src_.substr(pos_, 4));
                                    pos_ += 4;
                                    if (low >= 0xDC00u && low <= 0xDFFFu) {
                                        cp = 0x10000u + (((cp - 0xD800u) << 10) | (low - 0xDC00u));
                                    }
                                }
                            }
                            append_utf8(out, cp);
                            break;
                        }
                        default:
                            throw parse_error(std::string("invalid escape sequence \\") + esc);
                    }
                } else {
                    out.push_back(c);
                }
            }
            throw parse_error("unterminated string literal");
        }

        json parse_true() {
            if (src_.substr(pos_, 4) != "true") {
                throw parse_error("invalid literal at byte " + std::to_string(pos_));
            }
            pos_ += 4;
            return json(true);
        }

        json parse_false() {
            if (src_.substr(pos_, 5) != "false") {
                throw parse_error("invalid literal at byte " + std::to_string(pos_));
            }
            pos_ += 5;
            return json(false);
        }

        json parse_null() {
            if (src_.substr(pos_, 4) != "null") {
                throw parse_error("invalid literal at byte " + std::to_string(pos_));
            }
            pos_ += 4;
            return json(nullptr);
        }

        json parse_number() {
            std::size_t start = pos_;
            bool is_negative = false;
            bool is_float = false;

            if (src_[pos_] == '-') {
                is_negative = true;
                ++pos_;
            }
            if (pos_ >= src_.size() || !std::isdigit(static_cast<unsigned char>(src_[pos_]))) {
                throw parse_error("invalid number at byte " + std::to_string(start));
            }
            while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) {
                ++pos_;
            }
            if (pos_ < src_.size() && src_[pos_] == '.') {
                is_float = true;
                ++pos_;
                if (pos_ >= src_.size() || !std::isdigit(static_cast<unsigned char>(src_[pos_]))) {
                    throw parse_error("invalid fractional number at byte " + std::to_string(start));
                }
                while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) {
                    ++pos_;
                }
            }
            if (pos_ < src_.size() && (src_[pos_] == 'e' || src_[pos_] == 'E')) {
                is_float = true;
                ++pos_;
                if (pos_ < src_.size() && (src_[pos_] == '+' || src_[pos_] == '-')) {
                    ++pos_;
                }
                if (pos_ >= src_.size() || !std::isdigit(static_cast<unsigned char>(src_[pos_]))) {
                    throw parse_error("invalid exponent in number at byte " + std::to_string(start));
                }
                while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) {
                    ++pos_;
                }
            }

            std::string token(src_.substr(start, pos_ - start));
            char* endptr = nullptr;
            if (is_float) {
                double d = std::strtod(token.c_str(), &endptr);
                return json(d);
            }
            if (is_negative) {
                long long val = std::strtoll(token.c_str(), &endptr, 10);
                return json(static_cast<number_integer_t>(val));
            }
            unsigned long long uval = std::strtoull(token.c_str(), &endptr, 10);
            if (uval <= static_cast<unsigned long long>(INT64_MAX)) {
                return json(static_cast<number_integer_t>(uval));
            }
            return json(static_cast<number_unsigned_t>(uval));
        }
    };
};

} // namespace nlohmann

#endif // NLOHMANN_JSON_HPP_
