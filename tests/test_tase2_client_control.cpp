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

static const string protocol_config = QUOTE ({
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

static const string exchanged_data = QUOTE ({
    "exchanged_data" : {
        "datapoints" : [
            {
                "pivot_id" : "TC1",
                "label" : "TC1",
                "protocols" : [ {
                    "name" : "tase2",
                    "ref" : "icc1:Command",
                    "typeid" : "Command"
                } ]
            },
            {
                "pivot_id" : "TC2",
                "label" : "TC2",
                "protocols" : [ {
                    "name" : "tase2",
                    "ref" : "icc1:SetPointReal",
                    "typeid" : "SetPointReal"
                } ]
            },
            {
                "pivot_id" : "TC3",
                "label" : "TC3",
                "protocols" : [ {
                    "name" : "tase2",
                    "ref" : "icc1:SetPointDiscrete",
                    "typeid" : "SetPointDiscrete"
                } ]
            }
        ]
    }
});

static const string tls_config = QUOTE ({
    "tls_conf" : {
        "private_key" : "server-key.pem",
        "own_cert" : "server.cer",
        "ca_certs" : [ { "cert_file" : "root.cer" } ]
    }
});

class ControlTest : public testing::Test
{
  protected:
    TASE2* tase2 = nullptr;
    int ingestCallbackCalled = 0;
    Reading* storedReading = nullptr;
    int clockSyncHandlerCalled = 0;
    std::vector<Reading*> storedReadings;

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
        storedReadings.clear ();
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
        auto self = (ControlTest*)parameter;

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

    void
    verifyDatapoint (Datapoint* parent, const std::string& childName,
                     const int* expectedValue)
    {
        ASSERT_NE (parent, nullptr) << "Parent Datapoint is nullptr";

        Datapoint* child = getChild (*parent, childName);
        ASSERT_NE (child, nullptr)
            << "Child Datapoint '" << childName << "' is nullptr";

        if (expectedValue != nullptr)
        {
            ASSERT_EQ (child->getData ().toInt (), *expectedValue)
                << "Int value of Datapoint '" << childName
                << "' does not match expected value";
        }
    }

    void
    verifyDatapoint (Datapoint* parent, const std::string& childName,
                     const double* expectedValue)
    {
        ASSERT_NE (parent, nullptr) << "Parent Datapoint is nullptr";

        Datapoint* child = getChild (*parent, childName);
        ASSERT_NE (child, nullptr)
            << "Child Datapoint '" << childName << "' is nullptr";

        if (expectedValue != nullptr)
        {
            ASSERT_NEAR (child->getData ().toDouble (), *expectedValue, 0.0001)
                << "Double value of Datapoint '" << childName
                << "' does not match expected value";
        }
    }

    void
    verifyDatapoint (Datapoint* parent, const std::string& childName,
                     const std::string* expectedValue)
    {
        ASSERT_NE (parent, nullptr) << "Parent Datapoint is nullptr";

        Datapoint* child = getChild (*parent, childName);
        ASSERT_NE (child, nullptr)
            << "Child Datapoint '" << childName << "' is nullptr";

        if (expectedValue != nullptr)
        {
            ASSERT_EQ (child->getData ().toStringValue (), *expectedValue)
                << "String value of Datapoint '" << childName
                << "' does not match expected value";
        }
    }

    void
    verifyDatapoint (Datapoint* parent, const std::string& childName)
    {
        ASSERT_NE (parent, nullptr) << "Parent Datapoint is nullptr";

        Datapoint* child = getChild (*parent, childName);
        ASSERT_NE (child, nullptr)
            << "Child Datapoint '" << childName << "' is nullptr";
    }
};

TEST_F (ControlTest, operateDirect)
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

    Tase2_ControlPoint command = Tase2_Domain_addControlPoint (
        icc, "Command", TASE2_CONTROL_TYPE_COMMAND, TASE2_DEVICE_CLASS_DIRECT,
        false, 123);

    Tase2_ControlPoint setPointReal = Tase2_Domain_addControlPoint (
        icc, "SetPointReal", TASE2_CONTROL_TYPE_SETPOINT_REAL,
        TASE2_DEVICE_CLASS_DIRECT, false, 124);

    Tase2_ControlPoint setPointDiscrete = Tase2_Domain_addControlPoint (
        icc, "SetPointDiscrete", TASE2_CONTROL_TYPE_SETPOINT_DESCRETE,
        TASE2_DEVICE_CLASS_DIRECT, false, 125);

    Tase2_BilateralTable_addControlPoint (blt, command, 123, true, true, true,
                                          true);
    Tase2_BilateralTable_addControlPoint (blt, setPointReal, 124, true, true,
                                          true, true);
    Tase2_BilateralTable_addControlPoint (blt, setPointDiscrete, 125, true,
                                          true, true, true);

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

    auto params = new PLUGIN_PARAMETER*[7];
    params[0] = new PLUGIN_PARAMETER;
    params[0]->name = std::string ("co_type");
    params[0]->value = std::string ("Command");
    params[1] = new PLUGIN_PARAMETER;
    params[1]->name = std::string ("co_scope");
    params[1]->value = std::string ("domain");
    params[2] = new PLUGIN_PARAMETER;
    params[2]->name = std::string ("co_domain");
    params[2]->value = std::string ("icc1");
    params[3] = new PLUGIN_PARAMETER;
    params[3]->name = std::string ("co_name");
    params[3]->value = std::string ("Command");
    params[4] = new PLUGIN_PARAMETER;
    params[4]->name = std::string ("co_value");
    params[4]->value = std::string ("1");
    params[5] = new PLUGIN_PARAMETER;
    params[5]->name = std::string ("co_se");
    params[5]->value = std::string ("0");
    params[6] = new PLUGIN_PARAMETER;
    params[6]->name = std::string ("co_ts");
    params[6]->value = std::string ("10000");
    ASSERT_TRUE (tase2->operation ("TASE2Command", 7, params));

    Thread_sleep (100);
    delete params[0];
    delete params[1];
    delete params[2];
    delete params[3];
    delete params[4];
    delete params[5];
    delete params[6];

    params[0] = new PLUGIN_PARAMETER;
    params[0]->name = std::string ("co_type");
    params[0]->value = std::string ("SetPointReal");
    params[1] = new PLUGIN_PARAMETER;
    params[1]->name = std::string ("co_scope");
    params[1]->value = std::string ("domain");
    params[2] = new PLUGIN_PARAMETER;
    params[2]->name = std::string ("co_domain");
    params[2]->value = std::string ("icc1");
    params[3] = new PLUGIN_PARAMETER;
    params[3]->name = std::string ("co_name");
    params[3]->value = std::string ("SetPointReal");
    params[4] = new PLUGIN_PARAMETER;
    params[4]->name = std::string ("co_value");
    params[4]->value = std::string ("1.2");
    params[5] = new PLUGIN_PARAMETER;
    params[5]->name = std::string ("co_se");
    params[5]->value = std::string ("0");
    params[6] = new PLUGIN_PARAMETER;
    params[6]->name = std::string ("co_ts");
    params[6]->value = std::string ("10000");
    ASSERT_TRUE (tase2->operation ("TASE2Command", 7, params));

    Thread_sleep (100);
    delete params[0];
    delete params[1];
    delete params[2];
    delete params[3];
    delete params[4];
    delete params[5];
    delete params[6];

    params[0] = new PLUGIN_PARAMETER;
    params[0]->name = std::string ("co_type");
    params[0]->value = std::string ("SetPointDiscrete");
    params[1] = new PLUGIN_PARAMETER;
    params[1]->name = std::string ("co_scope");
    params[1]->value = std::string ("domain");
    params[2] = new PLUGIN_PARAMETER;
    params[2]->name = std::string ("co_domain");
    params[2]->value = std::string ("icc1");
    params[3] = new PLUGIN_PARAMETER;
    params[3]->name = std::string ("co_name");
    params[3]->value = std::string ("SetPointDiscrete");
    params[4] = new PLUGIN_PARAMETER;
    params[4]->name = std::string ("co_value");
    params[4]->value = std::string ("1.2");
    params[5] = new PLUGIN_PARAMETER;
    params[5]->name = std::string ("co_se");
    params[5]->value = std::string ("0");
    params[6] = new PLUGIN_PARAMETER;
    params[6]->name = std::string ("co_ts");
    params[6]->value = std::string ("10000");
    ASSERT_TRUE (tase2->operation ("TASE2Command", 7, params));

    delete params[0];
    delete params[1];
    delete params[2];
    delete params[3];
    delete params[4];
    delete params[5];
    delete params[6];
    delete[] params;

    auto timeout = std::chrono::seconds (3);
    auto start = std::chrono::high_resolution_clock::now ();
    while (ingestCallbackCalled < 3)
    {
        auto now = std::chrono::high_resolution_clock::now ();
        if (now - start > timeout)
        {
            tase2->stop ();
            Tase2_Endpoint_destroy (endpoint);
            Tase2_Server_stop (server);
            Tase2_Server_destroy (server);
            Tase2_DataModel_destroy (model);
            FAIL () << "Callback not called within timeout";
        }
        Thread_sleep (10);
    }

    tase2->stop ();
    Tase2_Endpoint_destroy (endpoint);
    Tase2_Server_stop (server);
    Tase2_Server_destroy (server);
    Tase2_DataModel_destroy (model);
}
