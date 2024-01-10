#ifndef TASE2_CLIENT_CONNECTION_H
#define TASE2_CLIENT_CONNECTION_H

#include "datapoint.h"
#include "tase2_client_config.hpp"
#include <gtest/gtest.h>
#include <libtase2/tase2_client.h>
#include <libtase2/tase2_common.h>
#include <mutex>
#include <thread>

class TASE2Client;

class TASE2ClientConnection
{
  public:
    TASE2ClientConnection (TASE2Client* client, TASE2ClientConfig* config,
                           const std::string& ip, int tcpPort, bool tls,
                           OsiParameters* osiParameters);

    ~TASE2ClientConnection ();

    void Start ();
    void Stop ();
    void Activate ();

    void Disconnect ();
    void Connect ();

    bool
    Disconnected () const
    {
        return ((m_connecting == false) && (m_connected == false));
    };

    bool
    Connecting () const
    {
        return m_connecting;
    };

    bool
    Connected () const
    {
        return m_connected;
    };

    bool
    Active () const
    {
        return m_active;
    };

    Tase2_PointValue readValue (Tase2_ClientError* err, const char* ref);

    bool operate (const std::string& ref, DatapointValue value);

    const std::string&
    IP ()
    {
        return m_serverIp;
    };
    int
    Port ()
    {
        return m_tcpPort;
    };

  private:
    bool prepareConnection ();
    bool
    UseTLS ()
    {
        return m_useTls;
    };
    Tase2_Endpoint m_connection = nullptr;
    void executePeriodicTasks ();

    void cleanUp ();

    TASE2Client* m_client;
    TASE2ClientConfig* m_config;

    static void reportCallbackFunction (void* parameter,
                                        Tase2_ClientDSTransferSet report);

    using ConState = enum {
        CON_STATE_IDLE,
        CON_STATE_CONNECTING,
        CON_STATE_CONNECTED,
        CON_STATE_CLOSED,
        CON_STATE_WAIT_FOR_RECONNECT,
        CON_STATE_FATAL_ERROR
    };

    ConState m_connectionState = CON_STATE_IDLE;

    using OperationState = enum {
        CONTROL_IDLE,
        CONTROL_WAIT_FOR_SELECT,
        CONTROL_WAIT_FOR_SELECT_WITH_VALUE,
        CONTROL_SELECTED,
        CONTROL_WAIT_FOR_ACT_CON,
        CONTROL_WAIT_FOR_ACT_TERM
    };

    using ControlObjectStruct = struct
    {
        OperationState state;
        Tase2_PointValue value;
        std::string label;
    };

    std::unordered_map<std::string, ControlObjectStruct*> m_controlObjects;
    std::vector<std::pair<TASE2ClientConnection*, LinkedList>*>
        m_connDataSetDirectoryPairs;
    std::vector<std::pair<TASE2ClientConnection*, ControlObjectStruct*>*>
        m_connControlPairs;

    void m_initialiseControlObjects ();
    void m_configDatasets ();
    void m_configRcb ();
    void m_setVarSpecs ();
    void m_setOsiConnectionParameters ();

    OsiParameters* m_osiParameters;
    int m_tcpPort;
    std::string m_serverIp;
    bool m_connected = false;
    bool m_active = false;
    bool m_connecting = false;
    bool m_started = false;
    bool m_useTls = false;

    TLSConfiguration m_tlsConfig = nullptr;

    std::mutex m_conLock;
    std::mutex m_reportLock;

    uint64_t m_delayExpirationTime;

    uint64_t m_nextPollingTime = 0;

    std::thread* m_conThread = nullptr;
    void _conThread ();

    bool m_connect = false;
    bool m_disconnect = false;

    void sendActCon (const ControlObjectStruct* cos);

    void sendActTerm (const ControlObjectStruct* cos);

    FRIEND_TESTS
};
#endif
