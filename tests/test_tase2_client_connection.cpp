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
            "ied_name" : "IED1",
            "connections" : [ {
                "srv_ip" : "127.0.0.1",
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
        "application_layer" : { "polling_interval" : 0 }
    }
});

static string protocol_config_1 = QUOTE ({
    "protocol_stack" : {
        "name" : "tase2client",
        "version" : "0.0.1",
        "transport_layer" : {
            "ied_name" : "IED1",
            "connections" : [ {
                "srv_ip" : "127.0.0.1",
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
                    "srv_ip" : "127.0.0.1",
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
                    "srv_ip" : "127.0.0.1",
                    "port" : 10003,
                    "osi" : {
                        "local_ap_title" : "1.1.1.998",
                        "local_ae_qualifier" : 12,
                        "remote_ap_title" : "1.1.1.999",
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
        tase2->stop ();
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

    Tase2_DataModel model = Tase2_DataModel_create();

    Tase2_Server server = Tase2_Server_create (model,nullptr);

    Tase2_Server_start (server);
    tase2->start ();

    Thread_sleep (1000);

    ASSERT_TRUE (Tase2_Endpoint_getState (
                     tase2->m_client->m_active_connection->m_connection)
                 == TASE2_ENDPOINT_STATE_CONNECTED);

    Tase2_Server_stop (server);
    Tase2_Server_destroy (server);
    Tase2_DataModel_destroy (model);
}

TEST_F (ConnectionHandlingTest, SingleConnectionReconnect)
{
    
}

TEST_F (ConnectionHandlingTest, SingleConnectionTLS)
{
   
}

TEST_F (ConnectionHandlingTest, TwoConnectionsBackup)
{
    
}
