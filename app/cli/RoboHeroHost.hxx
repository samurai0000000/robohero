/*
 * RoboHeroHost.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_ROBOHEROHOST_HXX
#define ROBOHERO_ROBOHEROHOST_HXX

#include <ncurses.h>

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "Configuration.hxx"
#include "Mqtt.hxx"

using namespace std;

class RoboHeroHost
{
  public:
    static RoboHeroHost &instance();

    int run(int argc, char **argv);
    void requestExit();

    void log(const string &msg);
    void logf(const char *fmt, ...);

  private:
    RoboHeroHost();
    ~RoboHeroHost();
    RoboHeroHost(const RoboHeroHost &) = delete;
    RoboHeroHost &operator=(const RoboHeroHost &) = delete;

    void initNcurses();
    void cleanupNcurses();
    void createWindows();
    void destroyWindows();
    void handleResize();

    void drawTop();
    void drawMiddle();
    void drawBottom();
    void redrawAll();

    void handleKey(int ch);
    void executeCommand(const string &cmdLine);

    // Built-in commands
    void cmdHelp();
    void cmdStatus();
    void cmdConnect(const vector<string> &args);
    void cmdDisconnect();
    void cmdConfig(const vector<string> &args);
    void cmdPm(const vector<string> &args);
    void cmdPms(const vector<string> &args);
    void cmdPwm(const vector<string> &args);
    void cmdStop();
    void cmdCenter();
    void cmdZero();
    void cmdRelax();
    void cmdVersion();
    void cmdClear();

    static RoboHeroHost _self;
    atomic<bool> _running;
    bool _ncursesActive;

    WINDOW *_winTop;
    WINDOW *_winMid;
    WINDOW *_winBottom;

    int _topHeight;
    int _bottomHeight;

    static constexpr size_t MAX_LOG_LINES = 500;
    deque<string> _logLines;
    size_t _scrollOffset;
    mutable mutex _logMutex;

    string _inputLine;
    size_t _cursorPos;
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
