/*
 * RoboHeroShell.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_SHELL_HXX
#define ROBOHERO_SHELL_HXX

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <Arduino.h>

class RoboHeroApp;

class RoboHeroShell {

public:

    RoboHeroShell(RoboHeroApp *app = NULL);
    ~RoboHeroShell();

    void setApp(RoboHeroApp *app);

    inline void setBanner(const char *banner) {
        _banner = banner;
    }
    inline void setVersion(const char *version) {
        _version = version;
    }
    inline void setBuilt(const char *built) {
        _built = built;
    }
    inline void setCopyright(const char *copyright) {
        _copyright = copyright;
    }

    inline const char *banner(void) const {
        return _banner ? _banner : "";
    }
    inline const char *version(void) const {
        return _version ? _version : "";
    }
    inline const char *built(void) const {
        return _built ? _built : "";
    }
    inline const char *copyright(void) const {
        return _copyright ? _copyright : "";
    }

    inline void setNoEcho(bool noEcho) {
        _noEcho = noEcho;
    }

    virtual void showWelcome(void);
    virtual int process(void);

protected:

    RoboHeroApp *_app;

    virtual int tx_write(const uint8_t *buf, size_t size);
    virtual int printf(const char *format, ...);
    virtual int vprintf(const char *format, va_list ap);
    virtual int rx_ready(void) const;
    virtual int rx_read(uint8_t *buf, size_t size);

    virtual int exec(char *cmdline);
    virtual int help(int argc, char **argv);
    virtual int version(int argc, char **argv);
    virtual int reboot(int argc, char **argv);
    virtual int eeprom(int argc, char **argv);
    virtual int wifi(int argc, char **argv);
    virtual int net(int argc, char **argv);
    virtual int pwm(int argc, char **argv);
    virtual int unknown_command(int argc, char **argv);

    const char *_banner;
    const char *_version;
    const char *_built;
    const char *_copyright;

protected:

#define CMDLINE_SIZE 256

    bool _noEcho;
    bool _lastWasCr;

    struct inproc {
        char cmdline[CMDLINE_SIZE];
        unsigned int i;
    };

    struct inproc _inproc;

};

#endif

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
