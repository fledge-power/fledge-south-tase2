#include "libtase2/tase2_server.h"
#include <config_category.h>
#include <gtest/gtest.h>
#include <plugin_api.h>
#include <string.h>
#include <tase2.hpp>

#include <boost/thread.hpp>
#include <libtase2/hal_thread.h>
#include <utility>
#include <vector>

using namespace std;

// PLUGIN DEFAULT PROTOCOL STACK CONF
static string protocol_config = QUOTE ({
    "protocol_stack" : {
        "name" : "tase2client",
        "version" : "0.0.1",
        "transport_layer" : {
            "connections" : [ {
                "ip_addr" : "127.0.0.1",
                "port" : 10002,
                "osi" : {
                    "local_ap_title" : "1.1.1.998",
                    "local_ae_qualifier" : 12,
                    "remote_ap_title" : "1.1.1.999",
                    "remote_ae_qualifier" : 12
                },
                "tls" : false
            } ]
        },
        "application_layer" : { "polling_interval" : 1000 }
    }
});

static string protocol_config_1 = QUOTE ({
    "protocol_stack" : {
        "name" : "tase2client",
        "version" : "0.0.1",
        "transport_layer" : {
            "connections" : [ {
                "ip_addr" : "127.0.0.1",
                "port" : 10002,
                "osi" : {
                    "local_ap_title" : "1.1.1.998",
                    "local_ae_qualifier" : 12,
                    "remote_ap_title" : "1.1.1.999",
                    "remote_ae_qualifier" : 12
                },
                "tls" : true
            } ]

        },
        "application_layer" : { "polling_interval" : 0 }
    }
});

static string protocol_config_2 = QUOTE ({
    "protocol_stack" : {
        "name" : "tase2client",
        "version" : "0.0.1",
        "transport_layer" : {
            "ied_name" : "IED1",
            "connections" : [
                {
                    "ip_addr" : "127.0.0.1",
                    "port" : 10002,
                    "osi" : {
                        "local_ap_title" : "1.1.1.998",
                        "local_ae_qualifier" : 12,
                        "remote_ap_title" : "1.1.1.999",
                        "remote_ae_qualifier" : 12
                    },
                    "tls" : false
                },
                {
                    "ip_addr" : "127.0.0.1",
                    "port" : 10003,
                    "osi" : {
                        "local_ap_title" : "1.1.1.996",
                        "local_ae_qualifier" : 12,
                        "remote_ap_title" : "1.1.1.997",
                        "remote_ae_qualifier" : 12
                    },
                    "tls" : false
                }
            ]
        },
        "application_layer" : { "polling_interval" : 0 }
    }
});

// PLUGIN DEFAULT EXCHANGED DATA CONF

static string exchanged_data
    = QUOTE ({ "exchanged_data" : { "datapoints" : [] } });

// PLUGIN DEFAULT TLS CONF
static string tls_config = QUOTE ({
    "tls_conf" : {
        "private_key" : "server-key.pem",
        "own_cert" : "server.cer",
        "ca_certs" : [ { "cert_file" : "root.cer" } ]
    }
});

static string tls_config_2 = QUOTE ({
    "tls_conf" : {
        "private_key" : "tase2_client.key",
        "own_cert" : "tase2_client.cer",
        "ca_certs" : [ { "cert_file" : "tase2_ca.cer" } ],
        "remote_certs" : [ { "cert_file" : "tase2_server.cer" } ]
    }
});

class ConnectionHandlingTest : public testing::Test
{
  protected:
    TASE2* tase2 = nullptr;
    int ingestCallbackCalled = 0;
    Reading* storedReading = nullptr;
    int clockSyncHandlerCalled = 0;
    std::vector<Reading*> storedReadings;

    int asduHandlerCalled = 0;
    Tase2_Endpoint lastConnection = nullptr;
    int lastOA = 0;
    int openConnections = 0;
    int activations = 0;
    int deactivations = 0;
    int maxConnections = 0;

    void
    SetUp () override
    {
        tase2 = new TASE2 ();

        tase2->registerIngest (this, ingestCallback);
    }

    void
    TearDown () override
    {
        delete tase2;

        for (auto reading : storedReadings)
        {
            delete reading;
        }
    }

    static bool
    hasChild (Datapoint& dp, std::string childLabel)
    {
        DatapointValue& dpv = dp.getData ();

        auto dps = dpv.getDpVec ();

        for (auto sdp : *dps)
        {
            if (sdp->getName () == childLabel)
            {
                return true;
            }
        }

        return false;
    }

    static Datapoint*
    getChild (Datapoint& dp, std::string childLabel)
    {
        DatapointValue& dpv = dp.getData ();

        auto dps = dpv.getDpVec ();

        for (Datapoint* childDp : *dps)
        {
            if (childDp->getName () == childLabel)
            {
                return childDp;
            }
        }

        return nullptr;
    }

    static int64_t
    getIntValue (Datapoint* dp)
    {
        DatapointValue dpValue = dp->getData ();
        return dpValue.toInt ();
    }

    static std::string
    getStrValue (Datapoint* dp)
    {
        return dp->getData ().toStringValue ();
    }

    static bool
    hasObject (Reading& reading, std::string label)
    {
        std::vector<Datapoint*> dataPoints = reading.getReadingData ();

        for (Datapoint* dp : dataPoints)
        {
            if (dp->getName () == label)
            {
                return true;
            }
        }

        return false;
    }

    static Datapoint*
    getObject (Reading& reading, std::string label)
    {
        std::vector<Datapoint*> dataPoints = reading.getReadingData ();

        for (Datapoint* dp : dataPoints)
        {
            if (dp->getName () == label)
            {
                return dp;
            }
        }

        return nullptr;
    }

    static void
    ingestCallback (void* parameter, Reading reading)
    {
        auto self = (ConnectionHandlingTest*)parameter;

        printf ("ingestCallback called -> asset: (%s)\n",
                reading.getAssetName ().c_str ());

        std::vector<Datapoint*> dataPoints = reading.getReadingData ();

        for (Datapoint* sdp : dataPoints)
        {
            printf ("name: %s value: %s\n", sdp->getName ().c_str (),
                    sdp->getData ().toString ().c_str ());
        }
        self->storedReading = new Reading (reading);

        self->storedReadings.push_back (self->storedReading);

        self->ingestCallbackCalled++;
    }
};

TEST_F (ConnectionHandlingTest, SingleConnection)
{
    tase2->setJsonConfig (protocol_config, exchanged_data, tls_config);

    Tase2_DataModel model = Tase2_DataModel_create ();

    Tase2_Domain icc = Tase2_DataModel_addDomain (model, "icc1");

    Tase2_BilateralTable blt
        = Tase2_BilateralTable_create ("blt1", icc, "1.1.1.998", 12);

    Tase2_Endpoint endpoint = Tase2_Endpoint_create (nullptr, true);

    Tase2_Endpoint_setLocalIpAddress (endpoint, "0.0.0.0");
    Tase2_Endpoint_setLocalTcpPort (endpoint, 10002);

    Tase2_Endpoint_setLocalApTitle (endpoint, "1.1.1.999", 12);

    Tase2_Server server = Tase2_Server_createEx (model, endpoint);

    Tase2_Server_addBilateralTable (server, blt);

    Tase2_Server_start (server);
    tase2->start ();

    ASSERT_TRUE (tase2->m_config->m_protocolConfigComplete);

    Thread_sleep (1000);

    TASE2Client* client = tase2->m_client;
    TASE2ClientConnection* connection = client->m_active_connection;
    Tase2_Endpoint clientEndpoint = connection->m_endpoint;

    ASSERT_TRUE (Tase2_Endpoint_getState (clientEndpoint)
                 == TASE2_ENDPOINT_STATE_CONNECTED);

    Tase2_Endpoint_destroy (endpoint);

    Tase2_Server_stop (server);
    Tase2_Server_destroy (server);
    Tase2_DataModel_destroy (model);
}

TEST_F (ConnectionHandlingTest, SingleConnectionReconnect)
{
    tase2->setJsonConfig (protocol_config, exchanged_data, tls_config);

    Tase2_DataModel model = Tase2_DataModel_create ();

    Tase2_Domain icc = Tase2_DataModel_addDomain (model, "icc1");

    Tase2_BilateralTable blt
        = Tase2_BilateralTable_create ("blt1", icc, "1.1.1.998", 12);

    Tase2_Endpoint endpoint = Tase2_Endpoint_create (nullptr, true);

    Tase2_Endpoint_setLocalIpAddress (endpoint, "0.0.0.0");
    Tase2_Endpoint_setLocalTcpPort (endpoint, 10002);

    Tase2_Endpoint_setLocalApTitle (endpoint, "1.1.1.999", 12);

    Tase2_Server server = Tase2_Server_createEx (model, endpoint);

    Tase2_Server_addBilateralTable (server, blt);

    Tase2_Server_start (server);
    tase2->start ();
    Thread_sleep (1000);

    TASE2Client* client = tase2->m_client;
    TASE2ClientConnection* connection = client->m_active_connection;
    Tase2_Endpoint clientEndpoint = connection->m_endpoint;

    auto start = std::chrono::high_resolution_clock::now ();
    auto timeout = std::chrono::seconds (10);
    while (Tase2_Endpoint_getState (clientEndpoint)
           != TASE2_ENDPOINT_STATE_CONNECTED)
    {
        auto now = std::chrono::high_resolution_clock::now ();
        if (now - start > timeout)
        {
            Tase2_Endpoint_destroy (endpoint);
            Tase2_Server_stop (server);
            Tase2_Server_destroy (server);
            Tase2_DataModel_destroy (model);
            FAIL () << "Connection not established within timeout";
            break;
        }
        Thread_sleep (10);
    }

    Tase2_Server_stop (server);
    cout << "Stopping server";
    Thread_sleep (2000);

    Tase2_Server_start (server);
    cout << "Starting server";
    
    start = std::chrono::high_resolution_clock::now ();
    timeout = std::chrono::seconds (20);
    while (!tase2->m_client->m_active_connection
           || !tase2->m_client->m_active_connection->m_endpoint
           || tase2->m_client->m_active_connection->m_connectionState != TASE2ClientConnection::ConState::CON_STATE_CONNECTED)
    {
        auto now = std::chrono::high_resolution_clock::now ();
        if (now - start > timeout)
        {
            Tase2_Endpoint_destroy (endpoint);
            Tase2_Server_stop (server);
            Tase2_Server_destroy (server);
            Tase2_DataModel_destroy (model);
            FAIL () << "Connection not established within timeout";
            break;
        }
        Thread_sleep (10);
    }

    Tase2_Endpoint_destroy (endpoint);
    Tase2_Server_stop (server);
    Tase2_Server_destroy (server);
    Tase2_DataModel_destroy (model);
}

TEST_F (ConnectionHandlingTest, SingleConnectionTLS)
{
    tase2->setJsonConfig (protocol_config_1, exchanged_data, tls_config_2);

    setenv ("FLEDGE_DATA", "../tests/data", 1);

    TLSConfiguration tlsConfig = TLSConfiguration_create ();

    TLSConfiguration_addCACertificateFromFile (
        tlsConfig, "../tests/data/etc/certs/tase2_ca.cer");
    TLSConfiguration_setOwnCertificateFromFile (
        tlsConfig, "../tests/data/etc/certs/tase2_server.cer");
    TLSConfiguration_setOwnKeyFromFile (
        tlsConfig, "../tests/data/etc/certs/tase2_server.key", NULL);
    TLSConfiguration_addAllowedCertificateFromFile (
        tlsConfig, "../tests/data/etc/certs/tase2_client.cer");
    TLSConfiguration_setChainValidation (tlsConfig, true);
    TLSConfiguration_setAllowOnlyKnownCertificates (tlsConfig, true);

    Tase2_DataModel model = Tase2_DataModel_create ();

    Tase2_Domain icc = Tase2_DataModel_addDomain (model, "icc1");

    Tase2_BilateralTable blt
        = Tase2_BilateralTable_create ("blt1", icc, "1.1.1.998", 12);

    Tase2_Endpoint endpoint = Tase2_Endpoint_create (tlsConfig, true);

    Tase2_Endpoint_setLocalIpAddress (endpoint, "0.0.0.0");
    Tase2_Endpoint_setLocalTcpPort (endpoint, 10002);

    Tase2_Endpoint_setLocalApTitle (endpoint, "1.1.1.999", 12);

    Tase2_Server server = Tase2_Server_createEx (model, endpoint);
    Tase2_Server_addBilateralTable (server, blt);

    Tase2_Server_start (server);

    tase2->start ();

    Thread_sleep (1000);

    TASE2Client* client = tase2->m_client;
    TASE2ClientConnection* connection = client->m_active_connection;
    Tase2_Endpoint clientEndpoint = connection->m_endpoint;

    ASSERT_TRUE (Tase2_Endpoint_getState (clientEndpoint)
                 == TASE2_ENDPOINT_STATE_CONNECTED);

    Tase2_Server_stop (server);
    Tase2_Server_destroy (server);
    Tase2_DataModel_destroy (model);
    TLSConfiguration_destroy (tlsConfig);
    Tase2_Endpoint_destroy (endpoint);
}

TEST_F (ConnectionHandlingTest, TwoConnectionsBackup)
{
    tase2->setJsonConfig (protocol_config_2, exchanged_data, tls_config);

    Tase2_DataModel model1 = Tase2_DataModel_create ();
    Tase2_DataModel model2 = Tase2_DataModel_create ();

    Tase2_Domain icc1 = Tase2_DataModel_addDomain (model1, "icc1");
    Tase2_Domain icc2 = Tase2_DataModel_addDomain (model2, "icc2");

    Tase2_BilateralTable blt1
        = Tase2_BilateralTable_create ("blt1", icc1, "1.1.1.998", 12);
    Tase2_BilateralTable blt2
        = Tase2_BilateralTable_create ("blt2", icc2, "1.1.1.996", 12);

    Tase2_Endpoint endpoint1 = Tase2_Endpoint_create (nullptr, true);
    Tase2_Endpoint endpoint2 = Tase2_Endpoint_create (nullptr, true);

    Tase2_Endpoint_setLocalIpAddress (endpoint1, "0.0.0.0");
    Tase2_Endpoint_setLocalTcpPort (endpoint1, 10002);
    Tase2_Endpoint_setLocalApTitle (endpoint1, "1.1.1.999", 12);

    Tase2_Endpoint_setLocalIpAddress (endpoint2, "0.0.0.0");
    Tase2_Endpoint_setLocalTcpPort (endpoint2, 10003);
    Tase2_Endpoint_setLocalApTitle (endpoint2, "1.1.1.997", 12);

    Tase2_Server server1 = Tase2_Server_createEx (model1, endpoint1);
    Tase2_Server server2 = Tase2_Server_createEx (model2, endpoint2);

    Tase2_Server_addBilateralTable (server1, blt1);
    Tase2_Server_addBilateralTable (server2, blt2);

    Tase2_Server_start (server1);
    Tase2_Server_start (server2);

    tase2->start ();

    Thread_sleep (1000);

    auto start = std::chrono::high_resolution_clock::now ();
    auto timeout = std::chrono::seconds (10);
    while (Tase2_Endpoint_getState (
               tase2->m_client->m_active_connection->m_endpoint)
           != TASE2_ENDPOINT_STATE_CONNECTED)
    {
        auto now = std::chrono::high_resolution_clock::now ();
        if (now - start > timeout)
        {
            Tase2_Endpoint_destroy (endpoint1);
            Tase2_Endpoint_destroy (endpoint2);

            Tase2_Server_stop (server1);
            Tase2_Server_destroy (server1);
            Tase2_Server_stop (server2);
            Tase2_Server_destroy (server2);
            Tase2_DataModel_destroy (model1);
            Tase2_DataModel_destroy (model2);

            FAIL () << "Connection not established within timeout";
            break;
        }
        Thread_sleep (10);
    }

    ASSERT_EQ (tase2->m_client->m_active_connection->m_tcpPort, 10002);

    Tase2_Server_stop (server1);

    while (tase2->m_client->m_active_connection
           && tase2->m_client->m_active_connection->m_tcpPort == 10002)
    {
        Thread_sleep (10);
    }

    start = std::chrono::high_resolution_clock::now ();
    timeout = std::chrono::seconds (20);
    while (!tase2->m_client->m_active_connection
           || !tase2->m_client->m_active_connection->m_endpoint
           || Tase2_Endpoint_getState (
                  tase2->m_client->m_active_connection->m_endpoint)
                  != TASE2_ENDPOINT_STATE_CONNECTED)
    {
        auto now = std::chrono::high_resolution_clock::now ();
        if (now - start > timeout)
        {
            Tase2_Endpoint_destroy (endpoint1);
            Tase2_Endpoint_destroy (endpoint2);
            Tase2_Server_stop (server1);
            Tase2_Server_destroy (server1);
            Tase2_Server_stop (server2);
            Tase2_Server_destroy (server2);
            Tase2_DataModel_destroy (model1);
            Tase2_DataModel_destroy (model2);

            FAIL () << "Connection not established within timeout";
            break;
        }
        Thread_sleep (10);
    }

    Tase2_Server_stop (server2);

    Tase2_Server_start (server1);

    Thread_sleep (1000);

    ASSERT_TRUE (Tase2_Server_isRunning (server1));

    Thread_sleep (20000);

    ASSERT_NE (tase2->m_client->m_active_connection, nullptr);
    ASSERT_NE (tase2->m_client->m_active_connection->m_endpoint, nullptr);

    ASSERT_EQ (tase2->m_client->m_active_connection->m_connectionState, TASE2ClientConnection::ConState::CON_STATE_CONNECTED);

    ASSERT_EQ (tase2->m_client->m_active_connection->m_tcpPort, 10002);

    Tase2_Endpoint_destroy (endpoint1);
    Tase2_Endpoint_destroy (endpoint2);
    Tase2_Server_stop (server1);
    Tase2_Server_destroy (server1);
    Tase2_Server_stop (server2);
    Tase2_Server_destroy (server2);
    Tase2_DataModel_destroy (model1);
    Tase2_DataModel_destroy (model2);
}

TEST_F(ConnectionHandlingTest, SingleConnectionTryBeforeServerStarts) {
    tase2->setJsonConfig(protocol_config, exchanged_data, tls_config);

    Tase2_DataModel model = Tase2_DataModel_create();
    Tase2_Domain icc = Tase2_DataModel_addDomain(model, "icc1");
    Tase2_BilateralTable blt = Tase2_BilateralTable_create("blt1", icc, "1.1.1.998", 12);

    auto endpointDeleter = [](Tase2_Endpoint ep) {
        Tase2_Endpoint_destroy(ep);
    };
    
    std::unique_ptr<std::remove_pointer<Tase2_Endpoint>::type, decltype(endpointDeleter)> endpoint(
        Tase2_Endpoint_create(nullptr, true),
        endpointDeleter
    );

    Tase2_Endpoint_setLocalIpAddress(endpoint.get(), "0.0.0.0");
    Tase2_Endpoint_setLocalTcpPort(endpoint.get(), 10002);
    Tase2_Endpoint_setLocalApTitle(endpoint.get(), "1.1.1.999", 12);

    Tase2_Server server = Tase2_Server_createEx(model, endpoint.get());
    Tase2_Server_addBilateralTable(server, blt);

    tase2->start();
    Thread_sleep(20000);  

    TASE2Client* client = tase2->m_client;
    ASSERT_NE(client, nullptr);

    if (client->m_active_connection && client->m_active_connection->m_endpoint) {
        auto clientEndpoint = client->m_active_connection->m_endpoint;
        ASSERT_NE(Tase2_Endpoint_getState(clientEndpoint), TASE2_ENDPOINT_STATE_CONNECTED);
    }

    Tase2_Server_start(server);

    auto start = std::chrono::high_resolution_clock::now();
    auto timeout = std::chrono::seconds(20);
    bool connected = false;
    while (!connected) {
        if (client->m_active_connection && client->m_active_connection->m_endpoint) {
            if (Tase2_Endpoint_getState(client->m_active_connection->m_endpoint) == TASE2_ENDPOINT_STATE_CONNECTED) {
                connected = true;
            }
        }
        auto now = std::chrono::high_resolution_clock::now();
        if (now - start > timeout) {
            Tase2_Server_stop(server);
            Tase2_Server_destroy(server);
            Tase2_DataModel_destroy(model);
            FAIL() << "Connection not established within timeout";
            break;
        }
        Thread_sleep(10);
    }

    Tase2_Server_stop(server);
    Tase2_Server_destroy(server);
    Tase2_DataModel_destroy(model);
}
