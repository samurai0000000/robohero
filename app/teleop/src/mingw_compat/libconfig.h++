/*
 * libconfig.h++
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_TELEOP_MINGW_LIBCONFIG_HXX
#define ROBOHERO_TELEOP_MINGW_LIBCONFIG_HXX

#include <exception>
#include <memory>
#include <string>
#include <vector>

#ifndef ROBOHERO_LIBCONFIG_NAMESPACE
#define ROBOHERO_LIBCONFIG_NAMESPACE libconfig
#endif

namespace ROBOHERO_LIBCONFIG_NAMESPACE {

class ConfigException : public std::exception {
public:
    const char *what() const noexcept override
    {
        return "ConfigException";
    }
};

class FileIOException : public ConfigException {
public:
    const char *what() const noexcept override
    {
        return "FileIOException";
    }
};

class SettingException : public ConfigException {
public:
    explicit SettingException(const std::string &path = "")
        : _path(path)
    {
    }

    const char *getPath() const noexcept
    {
        return _path.c_str();
    }

    const char *what() const noexcept override
    {
        return "SettingException";
    }

private:
    std::string _path;
};

class SettingNotFoundException : public SettingException {
public:
    explicit SettingNotFoundException(const std::string &path = "")
        : SettingException(path)
    {
    }

    const char *what() const noexcept override
    {
        return "SettingNotFoundException";
    }
};

class SettingTypeException : public SettingException {
public:
    explicit SettingTypeException(const std::string &path = "")
        : SettingException(path)
    {
    }

    const char *what() const noexcept override
    {
        return "SettingTypeException";
    }
};

class SettingNameException : public SettingException {
public:
    explicit SettingNameException(const std::string &path = "")
        : SettingException(path)
    {
    }

    const char *what() const noexcept override
    {
        return "SettingNameException";
    }
};

class ParseException : public ConfigException {
public:
    ParseException(const std::string &file, int line, const std::string &error)
        : _file(file)
        , _line(line)
        , _error(error)
    {
    }

    const char *getFile() const noexcept
    {
        return _file.c_str();
    }

    int getLine() const noexcept
    {
        return _line;
    }

    const char *getError() const noexcept
    {
        return _error.c_str();
    }

    const char *what() const noexcept override
    {
        return _error.c_str();
    }

private:
    std::string _file;
    int _line;
    std::string _error;
};

class Setting {
public:
    enum Type {
        TypeNone = 0,
        TypeInt,
        TypeInt64,
        TypeFloat,
        TypeString,
        TypeBoolean,
        TypeGroup,
        TypeArray,
        TypeList
    };

    Setting(const std::string &name = "", Type type = TypeGroup);
    Setting(const Setting &) = delete;
    Setting &operator=(const Setting &) = delete;
    Setting(Setting &&) noexcept = default;
    Setting &operator=(Setting &&) noexcept = default;
    ~Setting();

    Type getType() const noexcept;
    const std::string &getName() const noexcept;
    int getLength() const noexcept;

    bool exists(const char *name) const;
    bool exists(const std::string &name) const;

    Setting &operator[](const char *name);
    const Setting &operator[](const char *name) const;
    Setting &operator[](const std::string &name);
    const Setting &operator[](const std::string &name) const;
    Setting &operator[](int index);
    const Setting &operator[](int index) const;

    Setting &add(const char *name, Type type);
    Setting &add(const std::string &name, Type type);
    void remove(const char *name);
    void clear();

    bool lookupValue(const char *name, int &value) const;
    bool lookupValue(const char *name, long long &value) const;
    bool lookupValue(const char *name, float &value) const;
    bool lookupValue(const char *name, double &value) const;
    bool lookupValue(const char *name, bool &value) const;
    bool lookupValue(const char *name, std::string &value) const;

    Setting &operator=(int value);
    Setting &operator=(long long value);
    Setting &operator=(float value);
    Setting &operator=(double value);
    Setting &operator=(bool value);
    Setting &operator=(const std::string &value);
    Setting &operator=(const char *value);

    operator int() const;
    operator long long() const;
    operator float() const;
    operator double() const;
    operator bool() const;
    operator std::string() const;
    const char *c_str() const;

private:
    friend class Config;

    std::string _name;
    Type _type;
    long long _intVal;
    double _floatVal;
    bool _boolVal;
    std::string _strVal;
    std::vector<std::unique_ptr<Setting>> _children;
};

class Config {
public:
    Config();
    ~Config();

    void readFile(const char *filename);
    void writeFile(const char *filename);
    void readString(const std::string &str, const std::string &filename = "<string>");
    std::string writeToString() const;

    Setting &getRoot();
    const Setting &getRoot() const;
    bool exists(const char *path) const;

private:
    Setting _root;
};

} // namespace ROBOHERO_LIBCONFIG_NAMESPACE

#endif // ROBOHERO_TELEOP_MINGW_LIBCONFIG_HXX

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
