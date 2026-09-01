/*
 * RoboHero.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_HXX
#define ROBOHERO_HXX

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "Config.hxx"

enum RhCmdType {
    RH_CMD_PM = 1,
    RH_CMD_PMS,
    RH_CMD_STOP,
    RH_CMD_CENTER,
    RH_CMD_ZERO,
};

typedef void (*RoboHeroWifiUpFn)(void);

class RoboHero
{
  public:
    static RoboHero &instance();

    void onWifiUp(RoboHeroWifiUpFn fn);
    void start();
    bool applyNetif();
    bool wifiReady() const;

    bool isBusy() const;
    bool isLowVoltage() const;
    void resetLowVoltage();
    int getVoltage() const;
    int engineeringModel() const;
    bool sendVoltage(int voltage);

    void handleWifiKind(int kind);

    bool submitPm(int id);
    bool submitPms(int id);
    bool requestStop();
    void submitCenter();
    void submitZero();

    static void consoleBegin();
    static void terminalReset();

  private:
    RoboHero();
    RoboHero(const RoboHero &);
    RoboHero &operator=(const RoboHero &);
    static RoboHero _self;

    struct MotionCmd {
        uint8_t type;
        int id;
    };

    static void motionTask(void *arg);

    void notifyWifiUp();
    void checkVoltage();
    void executePm(int prog);
    void executePms(int prog);
    bool runThenCenter(const int matrix[][ALLMATRIX], int steps);
    bool interruptibleDelay(int ms);
    void enqueueCmd(uint8_t type, int id);
    bool setupWifi();
    void startSoftAp(const char *ssid, const char *pass, uint8_t channel);
    void probeEngineering();
    static bool uartEscapePending();
    static bool yieldCancel(void *ctx);
    static int mapInt(int x, int inMin, int inMax, int outMin, int outMax);
    static void makeDefaultApSsid(char *out, size_t outlen);

    void *_cmdQueue;
    void *_wifiEvents;
    volatile bool _busy;
    volatile bool _cancel;
    volatile int _voltageLow;
    int _voltage;
    int _engineering;
    int _voltageCab;
    bool _wifiReady;
    RoboHeroWifiUpFn _wifiUpFn;
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
