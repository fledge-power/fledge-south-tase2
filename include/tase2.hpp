#ifndef INCLUDE_TASE2_H_
#define INCLUDE_TASE2_H_

#include <cstdint>
#include <gtest/gtest.h>
#include <logger.h>
#include <plugin_api.h>
#include <reading.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "tase2_client_config.hpp"
#include "tase2_client_connection.hpp"

#define BACKUP_CONNECTION_TIMEOUT 5000

class TASE2Client;

class TASE2
{
  public:
    using INGEST_CB = void (*) (void*, Reading);

    TASE2 () = default;
    ~TASE2 ();

    void
    setAssetName (const std::string& asset)
    {
        m_asset = asset;
    }
    void setJsonConfig (const std::string& stack_configuration,
                        const std::string& msg_configuration,
                        const std::string& tls_configuration);

    void start ();
    void stop ();

    void ingest (const std::string& assetName,
                 const std::vector<Datapoint*>& points);

    void registerIngest (void* data, void (*cb) (void*, Reading));

    bool operation (const std::string& operation, int count,
                    PLUGIN_PARAMETER** params);

  private:
    TASE2ClientConfig* m_config = new TASE2ClientConfig ();
    ;

    bool m_CommandOperation (int count, PLUGIN_PARAMETER** params);
    bool m_SetPointRealOperation (int count, PLUGIN_PARAMETER** params);
    bool m_SetPointDiscreteOperation (int count, PLUGIN_PARAMETER** params);

    std::string m_asset;

    INGEST_CB m_ingest
        = nullptr; // Callback function used to send data to south service
    void* m_data;  // Ingest function data
    TASE2Client* m_client = nullptr;

    FRIEND_TESTS
};

class TASE2Client
{
  public:
    explicit TASE2Client (TASE2* tase2, TASE2ClientConfig* config);

    ~TASE2Client ();

    void sendData (const std::vector<Datapoint*>& data,
                   const std::vector<std::string>& labels);

    void start ();

    void stop ();

    void prepareConnections ();

    void handleValue (std::string ref, Tase2_PointValue value,
                      uint64_t timestamp, bool ack);
    void handleAllValues ();

    bool handleOperation (Datapoint* operation);

    void logTase2ClientError (Tase2_ClientError err,
                              const std::string& info) const;

    void sendCommandAck (const std::string& label, bool terminated);

    bool sendCommand (std::string domain, std::string name, int value,
                      bool select, long time);

    bool sendSetPointReal (std::string domain, std::string name, float value,
                           bool select, long time);

    bool sendSetPointDiscrete (std::string domain, std::string name, int value,
                               bool select, long time);

  private:
    std::shared_ptr<std::vector<TASE2ClientConnection*> > m_connections
        = nullptr;

    TASE2ClientConnection* m_active_connection = nullptr;
    std::mutex m_activeConnectionMtx;

    enum class ConnectionStatus
    {
        STARTED,
        NOT_CONNECTED
    };

    ConnectionStatus m_connStatus = ConnectionStatus::NOT_CONNECTED;

    void updateConnectionStatus (ConnectionStatus newState);

    std::mutex connectionsMutex;
    std::mutex statusMutex;

    std::thread* m_monitoringThread = nullptr;
    void _monitoringThread ();

    bool m_started = false;

    TASE2ClientConfig* m_config;
    TASE2* m_tase2;

    template <class T>
    Datapoint*
    m_createDatapoint (const std::string& label, const std::string& ref,
                       T value, Tase2_DataFlags quality, uint64_t timestampMs);

    void m_handleMonitoringData (const std::string& objRef,
                                 std::vector<Datapoint*>& datapoints,
                                 const std::string& label, DPTYPE type,
                                 Tase2_PointValue value, uint64_t timestamp);

    Datapoint* m_createDataObject (Tase2_PointValue value,
                                   const std::string& domain,
                                   const std::string& name, uint64_t ts,
                                   DPTYPE type);

    std::unordered_map<std::string, Datapoint*> m_outstandingCommands;

    FRIEND_TESTS
};

#endif // INCLUDE_TASE2_H_
