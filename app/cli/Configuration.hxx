/*
 * Configuration.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_CONFIGURATION_HXX
#define ROBOHERO_CONFIGURATION_HXX

#include <libconfig.h++>
#include <string>

using namespace std;

class Configuration
{
  public:
    static Configuration &instance();

    bool init(const string &configPath = "");
    bool load(const string &configPath = "");
    bool save(const string &configPath = "");

    const string &getConfigPath() const;

    // MQTT Broker Parameters
    string getMqttHost() const;
    int getMqttPort() const;
    string getMqttUsername() const;
    string getMqttPassword() const;
    string getMqttClientId() const;
    string getMqttRobotId() const;
    int getMqttKeepalive() const;

    void setMqttHost(const string &host);
    void setMqttPort(int port);
    void setMqttUsername(const string &user);
    void setMqttPassword(const string &pass);
    void setMqttClientId(const string &cid);
    void setMqttRobotId(const string &rid);
    void setMqttKeepalive(int keepalive);

    // Generic setting helpers
    bool lookupString(const string &path, string &val) const;
    bool lookupInt(const string &path, int &val) const;
    bool lookupBool(const string &path, bool &val) const;

    static string expandHome(const string &path);
    static string getDefaultConfigPath();

  private:
    Configuration();
    Configuration(const Configuration &) = delete;
    Configuration &operator=(const Configuration &) = delete;

    void populateDefaults();
    void syncFromConfig();
    void syncToConfig();

    static Configuration _self;
    libconfig::Config _cfg;
    string _configPath;
    bool _loaded;

    string _mqttHost;
    int _mqttPort;
    string _mqttUsername;
    string _mqttPassword;
    string _mqttClientId;
    string _mqttRobotId;
    int _mqttKeepalive;
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
