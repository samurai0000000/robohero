/*
 * MinGwLibconfigCompat.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "libconfig.h++"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace ROBOHERO_LIBCONFIG_NAMESPACE {

namespace {

constexpr int kMaxParseDepth = 64;

bool isValidIdentifierStart(char c)
{
    const unsigned char uc = static_cast<unsigned char>(c);
    return std::isalpha(uc) || c == '_' || c == '*';
}

bool isValidIdentifierChar(char c)
{
    const unsigned char uc = static_cast<unsigned char>(c);
    return std::isalnum(uc) || c == '_' || c == '-' || c == '*';
}

std::string toLowerAscii(const std::string &s)
{
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::string escapeString(const std::string &s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
        case '\"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\b':
            out += "\\b";
            break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\x%02x", static_cast<unsigned int>(c));
                out += buf;
            } else {
                out.push_back(static_cast<char>(c));
            }
            break;
        }
    }
    return out;
}

std::string formatFloatValue(double val)
{
    if (!std::isfinite(val)) {
        return "0.0";
    }
    std::ostringstream oss;
    oss << std::setprecision(7) << val;
    std::string s = oss.str();
    if (s.find('.') == std::string::npos &&
        s.find('e') == std::string::npos &&
        s.find('E') == std::string::npos) {
        s += ".0";
    }
    return s;
}

class Parser {
public:
    Parser(const std::string &src, const std::string &file)
        : _src(src)
        , _file(file)
        , _pos(0)
        , _line(1)
    {
    }

    void parseRoot(Setting &root)
    {
        root.clear();
        parseSettingList(root, false, 0);
        skipWhitespaceAndComments();
        if (_pos < _src.size()) {
            throw ParseException(_file, _line, "Unexpected trailing characters");
        }
    }

private:
    char peek() const
    {
        return (_pos < _src.size()) ? _src[_pos] : '\0';
    }

    char get()
    {
        if (_pos >= _src.size()) {
            return '\0';
        }
        char c = _src[_pos++];
        if (c == '\n') {
            _line++;
        }
        return c;
    }

    void skipWhitespaceAndComments()
    {
        while (_pos < _src.size()) {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v') {
                get();
                continue;
            }
            if (c == '#') {
                while (_pos < _src.size() && peek() != '\n') {
                    get();
                }
                continue;
            }
            if (c == '/' && _pos + 1 < _src.size()) {
                char next = _src[_pos + 1];
                if (next == '/') {
                    get();
                    get();
                    while (_pos < _src.size() && peek() != '\n') {
                        get();
                    }
                    continue;
                }
                if (next == '*') {
                    int commentStartLine = _line;
                    get();
                    get();
                    bool closed = false;
                    while (_pos < _src.size()) {
                        if (peek() == '*' && _pos + 1 < _src.size() && _src[_pos + 1] == '/') {
                            get();
                            get();
                            closed = true;
                            break;
                        }
                        get();
                    }
                    if (!closed) {
                        throw ParseException(_file, commentStartLine, "Unterminated block comment");
                    }
                    continue;
                }
            }
            break;
        }
    }

    std::string parseIdentifier()
    {
        skipWhitespaceAndComments();
        if (_pos >= _src.size() || !isValidIdentifierStart(peek())) {
            throw ParseException(_file, _line, "Expected setting name identifier");
        }
        std::string ident;
        while (_pos < _src.size() && isValidIdentifierChar(peek())) {
            ident.push_back(get());
        }
        return ident;
    }

    std::string parseStringLiteral()
    {
        std::string result;
        while (true) {
            skipWhitespaceAndComments();
            if (peek() != '\"') {
                break;
            }
            int startLine = _line;
            get(); // consume opening quote
            bool closed = false;
            while (_pos < _src.size()) {
                char c = get();
                if (c == '\"') {
                    closed = true;
                    break;
                }
                if (c == '\n') {
                    throw ParseException(_file, startLine, "Unterminated string literal before newline");
                }
                if (c == '\\') {
                    if (_pos >= _src.size()) {
                        throw ParseException(_file, startLine, "Unterminated escape sequence in string");
                    }
                    char esc = get();
                    switch (esc) {
                    case '\"':
                        result.push_back('\"');
                        break;
                    case '\\':
                        result.push_back('\\');
                        break;
                    case '/':
                        result.push_back('/');
                        break;
                    case 'n':
                        result.push_back('\n');
                        break;
                    case 'r':
                        result.push_back('\r');
                        break;
                    case 't':
                        result.push_back('\t');
                        break;
                    case 'f':
                        result.push_back('\f');
                        break;
                    case 'b':
                        result.push_back('\b');
                        break;
                    case 'x': {
                        if (_pos + 2 > _src.size()) {
                            throw ParseException(_file, _line, "Invalid hex escape in string");
                        }
                        char h1 = get();
                        char h2 = get();
                        if (!std::isxdigit(static_cast<unsigned char>(h1)) ||
                            !std::isxdigit(static_cast<unsigned char>(h2))) {
                            throw ParseException(_file, _line, "Invalid hex digit in string escape");
                        }
                        char hexBuf[3] = {h1, h2, '\0'};
                        unsigned long val = std::strtoul(hexBuf, nullptr, 16);
                        result.push_back(static_cast<char>(val));
                        break;
                    }
                    default:
                        result.push_back(esc);
                        break;
                    }
                } else {
                    result.push_back(c);
                }
            }
            if (!closed) {
                throw ParseException(_file, startLine, "Unterminated string literal");
            }
            skipWhitespaceAndComments();
            if (peek() != '\"') {
                break;
            }
        }
        return result;
    }

    void parseValueInto(Setting &parent, const std::string &name, int depth)
    {
        if (depth > kMaxParseDepth) {
            throw ParseException(_file, _line, "Maximum group nesting depth exceeded");
        }
        skipWhitespaceAndComments();
        if (_pos >= _src.size()) {
            throw ParseException(_file, _line, "Unexpected end of file while expecting value");
        }

        char c = peek();
        if (c == '{') {
            int braceLine = _line;
            get(); // consume '{'
            Setting &grp = parent.add(name, Setting::TypeGroup);
            parseSettingList(grp, true, depth + 1);
            skipWhitespaceAndComments();
            if (peek() != '}') {
                throw ParseException(_file, braceLine, "Unterminated group '{'");
            }
            get(); // consume '}'
            return;
        }

        if (c == '\"') {
            std::string strVal = parseStringLiteral();
            parent.add(name, Setting::TypeString) = strVal;
            return;
        }

        // Read scalar token up to whitespace or delimiter
        int tokenLine = _line;
        std::string token;
        while (_pos < _src.size()) {
            char tc = peek();
            if (std::isspace(static_cast<unsigned char>(tc)) ||
                tc == ';' || tc == ',' || tc == '}' || tc == '{' || tc == '#' || tc == '/') {
                break;
            }
            token.push_back(get());
        }

        if (token.empty()) {
            throw ParseException(_file, tokenLine, "Expected setting value");
        }

        std::string lowerToken = toLowerAscii(token);
        if (lowerToken == "true") {
            parent.add(name, Setting::TypeBoolean) = true;
            return;
        }
        if (lowerToken == "false") {
            parent.add(name, Setting::TypeBoolean) = false;
            return;
        }

        // Determine whether numeric token is float or integer
        bool isHex = (token.size() > 2 && token[0] == '0' && (token[1] == 'x' || token[1] == 'X'));
        bool hasFloatIndicator = !isHex &&
            (token.find('.') != std::string::npos ||
             token.find('e') != std::string::npos ||
             token.find('E') != std::string::npos);

        if (hasFloatIndicator) {
            char *endPtr = nullptr;
            double dval = std::strtod(token.c_str(), &endPtr);
            if (endPtr == token.c_str() || *endPtr != '\0' || !std::isfinite(dval)) {
                throw ParseException(_file, tokenLine, "Invalid floating-point value: " + token);
            }
            parent.add(name, Setting::TypeFloat) = dval;
            return;
        }

        std::string intToken = token;
        if (!intToken.empty() && (intToken.back() == 'L' || intToken.back() == 'l')) {
            intToken.pop_back();
        }
        char *endPtr = nullptr;
        long long ival = std::strtoll(intToken.c_str(), &endPtr, 0);
        if (endPtr == intToken.c_str() || *endPtr != '\0') {
            throw ParseException(_file, tokenLine, "Invalid setting value: " + token);
        }
        parent.add(name, Setting::TypeInt) = ival;
    }

    void parseSettingList(Setting &group, bool stopOnCloseBrace, int depth)
    {
        if (depth > kMaxParseDepth) {
            throw ParseException(_file, _line, "Maximum group nesting depth exceeded");
        }
        while (true) {
            skipWhitespaceAndComments();
            if (_pos >= _src.size()) {
                if (stopOnCloseBrace) {
                    throw ParseException(_file, _line, "Unexpected end of file inside group");
                }
                break;
            }
            if (stopOnCloseBrace && peek() == '}') {
                break;
            }

            int nameLine = _line;
            std::string name = parseIdentifier();
            if (group.exists(name)) {
                throw ParseException(_file, nameLine, "Duplicate setting name: " + name);
            }

            skipWhitespaceAndComments();
            char sep = peek();
            if (sep == '=' || sep == ':') {
                get();
            } else if (sep != '{') {
                throw ParseException(_file, _line, "Expected '=' or ':' after setting name '" + name + "'");
            }

            parseValueInto(group, name, depth);

            skipWhitespaceAndComments();
            if (peek() == ';' || peek() == ',') {
                get();
            }
        }
    }

    const std::string &_src;
    std::string _file;
    size_t _pos;
    int _line;
};

} // namespace

Setting::Setting(const std::string &name, Type type)
    : _name(name)
    , _type(type)
    , _intVal(0)
    , _floatVal(0.0)
    , _boolVal(false)
    , _strVal()
    , _children()
{
}

Setting::~Setting() = default;

Setting::Type Setting::getType() const noexcept
{
    return _type;
}

const std::string &Setting::getName() const noexcept
{
    return _name;
}

int Setting::getLength() const noexcept
{
    return static_cast<int>(_children.size());
}

bool Setting::exists(const char *name) const
{
    return name != nullptr && exists(std::string(name));
}

bool Setting::exists(const std::string &name) const
{
    if (name.empty()) {
        return false;
    }
    auto dotPos = name.find('.');
    if (dotPos == std::string::npos) {
        for (const auto &child : _children) {
            if (child && child->_name == name) {
                return true;
            }
        }
        return false;
    }
    std::string head = name.substr(0, dotPos);
    std::string tail = name.substr(dotPos + 1);
    for (const auto &child : _children) {
        if (child && child->_name == head) {
            return child->exists(tail);
        }
    }
    return false;
}

Setting &Setting::operator[](const char *name)
{
    if (name == nullptr) {
        throw SettingNotFoundException("");
    }
    return (*this)[std::string(name)];
}

const Setting &Setting::operator[](const char *name) const
{
    if (name == nullptr) {
        throw SettingNotFoundException("");
    }
    return (*this)[std::string(name)];
}

Setting &Setting::operator[](const std::string &name)
{
    auto dotPos = name.find('.');
    if (dotPos == std::string::npos) {
        for (auto &child : _children) {
            if (child && child->_name == name) {
                return *child;
            }
        }
        throw SettingNotFoundException(name);
    }
    std::string head = name.substr(0, dotPos);
    std::string tail = name.substr(dotPos + 1);
    return (*this)[head][tail];
}

const Setting &Setting::operator[](const std::string &name) const
{
    auto dotPos = name.find('.');
    if (dotPos == std::string::npos) {
        for (const auto &child : _children) {
            if (child && child->_name == name) {
                return *child;
            }
        }
        throw SettingNotFoundException(name);
    }
    std::string head = name.substr(0, dotPos);
    std::string tail = name.substr(dotPos + 1);
    return (*this)[head][tail];
}

Setting &Setting::operator[](int index)
{
    if (index < 0 || static_cast<size_t>(index) >= _children.size()) {
        throw SettingNotFoundException(std::to_string(index));
    }
    return *_children[static_cast<size_t>(index)];
}

const Setting &Setting::operator[](int index) const
{
    if (index < 0 || static_cast<size_t>(index) >= _children.size()) {
        throw SettingNotFoundException(std::to_string(index));
    }
    return *_children[static_cast<size_t>(index)];
}

Setting &Setting::add(const char *name, Type type)
{
    return add(std::string(name ? name : ""), type);
}

Setting &Setting::add(const std::string &name, Type type)
{
    if (_type != TypeGroup && _type != TypeList && _type != TypeArray) {
        throw SettingTypeException(_name);
    }
    if (name.empty() || exists(name)) {
        throw SettingNameException(name);
    }
    auto child = std::make_unique<Setting>(name, type);
    Setting &ref = *child;
    _children.push_back(std::move(child));
    return ref;
}

void Setting::remove(const char *name)
{
    if (name == nullptr) {
        return;
    }
    std::string target(name);
    auto it = std::remove_if(_children.begin(), _children.end(),
                             [&target](const std::unique_ptr<Setting> &c) {
                                 return c && c->_name == target;
                             });
    if (it == _children.end()) {
        throw SettingNotFoundException(target);
    }
    _children.erase(it, _children.end());
}

void Setting::clear()
{
    _children.clear();
}

bool Setting::lookupValue(const char *name, int &value) const
{
    if (name == nullptr || !exists(name)) {
        return false;
    }
    const Setting &s = (*this)[name];
    if (s._type == TypeInt || s._type == TypeInt64) {
        value = static_cast<int>(s._intVal);
        return true;
    }
    return false;
}

bool Setting::lookupValue(const char *name, long long &value) const
{
    if (name == nullptr || !exists(name)) {
        return false;
    }
    const Setting &s = (*this)[name];
    if (s._type == TypeInt || s._type == TypeInt64) {
        value = s._intVal;
        return true;
    }
    return false;
}

bool Setting::lookupValue(const char *name, float &value) const
{
    if (name == nullptr || !exists(name)) {
        return false;
    }
    const Setting &s = (*this)[name];
    if (s._type == TypeFloat) {
        value = static_cast<float>(s._floatVal);
        return true;
    }
    if (s._type == TypeInt || s._type == TypeInt64) {
        value = static_cast<float>(s._intVal);
        return true;
    }
    return false;
}

bool Setting::lookupValue(const char *name, double &value) const
{
    if (name == nullptr || !exists(name)) {
        return false;
    }
    const Setting &s = (*this)[name];
    if (s._type == TypeFloat) {
        value = s._floatVal;
        return true;
    }
    if (s._type == TypeInt || s._type == TypeInt64) {
        value = static_cast<double>(s._intVal);
        return true;
    }
    return false;
}

bool Setting::lookupValue(const char *name, bool &value) const
{
    if (name == nullptr || !exists(name)) {
        return false;
    }
    const Setting &s = (*this)[name];
    if (s._type == TypeBoolean) {
        value = s._boolVal;
        return true;
    }
    return false;
}

bool Setting::lookupValue(const char *name, std::string &value) const
{
    if (name == nullptr || !exists(name)) {
        return false;
    }
    const Setting &s = (*this)[name];
    if (s._type == TypeString) {
        value = s._strVal;
        return true;
    }
    return false;
}

Setting &Setting::operator=(int value)
{
    _type = TypeInt;
    _intVal = value;
    _floatVal = static_cast<double>(value);
    return *this;
}

Setting &Setting::operator=(long long value)
{
    _type = TypeInt64;
    _intVal = value;
    _floatVal = static_cast<double>(value);
    return *this;
}

Setting &Setting::operator=(float value)
{
    _type = TypeFloat;
    _floatVal = static_cast<double>(value);
    _intVal = static_cast<long long>(value);
    return *this;
}

Setting &Setting::operator=(double value)
{
    _type = TypeFloat;
    _floatVal = value;
    _intVal = static_cast<long long>(value);
    return *this;
}

Setting &Setting::operator=(bool value)
{
    _type = TypeBoolean;
    _boolVal = value;
    return *this;
}

Setting &Setting::operator=(const std::string &value)
{
    _type = TypeString;
    _strVal = value;
    return *this;
}

Setting &Setting::operator=(const char *value)
{
    _type = TypeString;
    _strVal = (value != nullptr) ? value : "";
    return *this;
}

Setting::operator int() const
{
    if (_type != TypeInt && _type != TypeInt64) {
        throw SettingTypeException(_name);
    }
    return static_cast<int>(_intVal);
}

Setting::operator long long() const
{
    if (_type != TypeInt && _type != TypeInt64) {
        throw SettingTypeException(_name);
    }
    return _intVal;
}

Setting::operator float() const
{
    if (_type == TypeFloat) {
        return static_cast<float>(_floatVal);
    }
    if (_type == TypeInt || _type == TypeInt64) {
        return static_cast<float>(_intVal);
    }
    throw SettingTypeException(_name);
}

Setting::operator double() const
{
    if (_type == TypeFloat) {
        return _floatVal;
    }
    if (_type == TypeInt || _type == TypeInt64) {
        return static_cast<double>(_intVal);
    }
    throw SettingTypeException(_name);
}

Setting::operator bool() const
{
    if (_type != TypeBoolean) {
        throw SettingTypeException(_name);
    }
    return _boolVal;
}

Setting::operator std::string() const
{
    if (_type != TypeString) {
        throw SettingTypeException(_name);
    }
    return _strVal;
}

const char *Setting::c_str() const
{
    if (_type != TypeString) {
        throw SettingTypeException(_name);
    }
    return _strVal.c_str();
}

Config::Config()
    : _root("", Setting::TypeGroup)
{
}

Config::~Config() = default;

Setting &Config::getRoot()
{
    return _root;
}

const Setting &Config::getRoot() const
{
    return _root;
}

bool Config::exists(const char *path) const
{
    return _root.exists(path);
}

void Config::readFile(const char *filename)
{
    if (filename == nullptr || filename[0] == '\0') {
        throw FileIOException();
    }
    std::ifstream ifs(filename, std::ios::in | std::ios::binary);
    if (!ifs.is_open() || !ifs.good()) {
        throw FileIOException();
    }
    std::ostringstream buffer;
    buffer << ifs.rdbuf();
    if (ifs.bad()) {
        throw FileIOException();
    }
    readString(buffer.str(), filename);
}

void Config::readString(const std::string &str, const std::string &filename)
{
    Setting newRoot("", Setting::TypeGroup);
    Parser parser(str, filename);
    parser.parseRoot(newRoot);
    _root = std::move(newRoot);
}

namespace {

void writeSettingRecursive(std::ostream &os, const Setting &s, int indent)
{
    const std::string pad(static_cast<size_t>(indent * 2), ' ');
    for (int i = 0; i < s.getLength(); ++i) {
        const Setting &child = s[i];
        const std::string &name = child.getName();
        switch (child.getType()) {
        case Setting::TypeGroup:
            os << pad << name << " :\n"
               << pad << "{\n";
            writeSettingRecursive(os, child, indent + 1);
            os << pad << "};\n";
            break;
        case Setting::TypeInt:
        case Setting::TypeInt64:
            os << pad << name << " = " << static_cast<long long>(child) << ";\n";
            break;
        case Setting::TypeFloat:
            os << pad << name << " = " << formatFloatValue(static_cast<double>(child)) << ";\n";
            break;
        case Setting::TypeBoolean:
            os << pad << name << " = " << (static_cast<bool>(child) ? "true" : "false") << ";\n";
            break;
        case Setting::TypeString:
            os << pad << name << " = \"" << escapeString(static_cast<std::string>(child)) << "\";\n";
            break;
        default:
            break;
        }
    }
}

} // namespace

std::string Config::writeToString() const
{
    std::ostringstream oss;
    writeSettingRecursive(oss, _root, 0);
    return oss.str();
}

void Config::writeFile(const char *filename)
{
    if (filename == nullptr || filename[0] == '\0') {
        throw FileIOException();
    }
    std::ofstream ofs(filename, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!ofs.is_open() || !ofs.good()) {
        throw FileIOException();
    }
    std::string content = writeToString();
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    ofs.flush();
    if (!ofs.good()) {
        throw FileIOException();
    }
}

} // namespace ROBOHERO_LIBCONFIG_NAMESPACE

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
