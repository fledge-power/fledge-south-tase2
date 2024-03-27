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
        "application_layer" : {
            "polling_interval" : 0,
            "datasets" : [],
            "dataset_transfer_sets" : [ {
                "domain" : "icc1",
                "name" : "dsts1",
                "dataset_ref" : "DataSet1",
                "dsConditions" : [ "interval", "change" ],
                "startTime" : 0,
                "interval" : 5,
                "bufTm" : 5,
                "integrityCheck" : 60,
                "critical" : false,
                "rbe" : false,
                "allChangesReported" : true
            } ]
        }
    }
});

static const string protocol_config_1 = QUOTE ({
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
        "application_layer" : {
            "polling_interval" : 0,
            "datasets" : [ {
                "domain" : "icc1",
                "dataset_ref" : "DataSet1",
                "entries" : [
                    "icc1/datapointReal", "icc1/datapointRealQ",
                    "icc1/datapointRealQTime", "icc1/datapointRealQTimeExt",
                    "icc1/datapointState", "icc1/datapointStateQ",
                    "icc1/datapointStateQTime", "icc1/datapointStateQTimeExt",
                    "icc1/datapointDiscrete", "icc1/datapointDiscreteQ",
                    "icc1/datapointDiscreteQTime",
                    "icc1/datapointDiscreteQTimeExt", "icc1/datapointStateSup",
                    "icc1/datapointStateSupQ", "icc1/datapointStateSupQTime",
                    "icc1/datapointStateSupQTimeExt"
                ],
                "dynamic" : true
            } ],
            "dataset_transfer_sets" : [ {
                "domain" : "icc1",
                "name" : "dsts1",
                "dataset_ref" : "DataSet1",
                "dsConditions" : [ "interval", "change" ],
                "startTime" : 0,
                "interval" : 5,
                "bufTm" : 5,
                "integrityCheck" : 60,
                "critical" : false,
                "rbe" : false,
                "allChangesReported" : true
            } ]
        }
    }
});

static const string exchanged_data = QUOTE ({
    "exchanged_data" : {
        "datapoints" : [
            {
                "pivot_id" : "TS3",
                "label" : "TS3",
                "protocols" : [ {
                    "name" : "tase2",
                    "ref" : "icc1:datapointReal",
                    "typeid" : "Real"
                } ]
            },
            {
                "pivot_id" : "TS4",
                "label" : "TS4",
                "protocols" : [ {
                    "name" : "tase2",
                    "ref" : "icc1:datapointRealQ",
                    "typeid" : "RealQ"
                } ]
            },
            {
                "pivot_id" : "TS5",
                "label" : "TS5",
                "protocols" : [ {
                    "name" : "tase2",
                    "ref" : "icc1:datapointRealQTime",
                    "typeid" : "RealQTime"
                } ]
            },
            {
                "pivot_id" : "TS6",
                "label" : "TS6",
                "protocols" : [ {
                    "name" : "tase2",
                    "ref" : "icc1:datapointRealQTimeExt",
                    "typeid" : "RealQTimeExt"
                } ]
            },
            {
                "pivot_id" : "TS7",
                "label" : "TS7",
                "protocols" : [ {
                    "name" : "tase2",
                    "ref" : "icc1:datapointState",
                    "typeid" : "State"
                } ]
            },
            {
                "pivot_id" : "TS8",
                "label" : "TS8",
                "protocols" : [ {
                    "name" : "tase2",
                    "ref" : "icc1:datapointStateQ",
                    "typeid" : "StateQ"
                } ]
            },
            {
                "pivot_id" : "TS9",
                "label" : "TS9",
                "protocols" : [ {
                    "name" : "tase2",
                    "ref" : "icc1:datapointStateQTime",
                    "typeid" : "StateQTime"
                } ]
            },
            {
                "pivot_id" : "TS10",
                "label" : "TS10",
                "protocols" : [ {
                    "name" : "tase2",
                    "ref" : "icc1:datapointStateQTimeExt",
                    "typeid" : "StateQTimeExt"
                } ]
            },
            {
                "pivot_id" : "TS11",
                "label" : "TS11",
                "protocols" : [ {
                    "name" : "tase2",
                    "ref" : "icc1:datapointDiscrete",
                    "typeid" : "Discrete"
                } ]
            },
            {
                "pivot_id" : "TS12",
                "label" : "TS12",
                "protocols" : [ {
                    "name" : "tase2",
                    "ref" : "icc1:datapointDiscreteQ",
                    "typeid" : "DiscreteQ"
                } ]
            },
            {
                "pivot_id" : "TS13",
                "label" : "TS13",
                "protocols" : [ {
                    "name" : "tase2",
                    "ref" : "icc1:datapointDiscreteQTime",
                    "typeid" : "DiscreteQTime"
                } ]
            },
            {
                "pivot_id" : "TS14",
                "label" : "TS14",
                "protocols" : [ {
                    "name" : "tase2",
                    "ref" : "icc1:datapointDiscreteQTimeExt",
                    "typeid" : "DiscreteQTimeExt"
                } ]
            },
            {
                "pivot_id" : "TS15",
                "label" : "TS15",
                "protocols" : [ {
                    "name" : "tase2",
                    "ref" : "icc1:datapointStateSup",
                    "typeid" : "StateSup"
                } ]
            },
            {
                "pivot_id" : "TS16",
                "label" : "TS16",
                "protocols" : [ {
                    "name" : "tase2",
                    "ref" : "icc1:datapointStateSupQ",
                    "typeid" : "StateSupQ"
                } ]
            },
            {
                "pivot_id" : "TS17",
                "label" : "TS17",
                "protocols" : [ {
                    "name" : "tase2",
                    "ref" : "icc1:datapointStateSupQTime",
                    "typeid" : "StateSupQTime"
                } ]
            },
            {
                "pivot_id" : "TS18",
                "label" : "TS18",
                "protocols" : [ {
                    "name" : "tase2",
                    "ref" : "icc1:datapointStateSupQTimeExt",
                    "typeid" : "StateSupQTimeExt"
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

class ReportingTest : public testing::Test
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
        auto self = (ReportingTest*)parameter;

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

TEST_F (ReportingTest, ReportingAllType)
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

    Tase2_IndicationPoint datapointReal = Tase2_Domain_addIndicationPoint (
        icc, "datapointReal", TASE2_IND_POINT_TYPE_REAL, TASE2_NO_QUALITY,
        TASE2_NO_TIMESTAMP, false, true);

    Tase2_IndicationPoint datapointRealQ = Tase2_Domain_addIndicationPoint (
        icc, "datapointRealQ", TASE2_IND_POINT_TYPE_REAL, TASE2_QUALITY,
        TASE2_NO_TIMESTAMP, false, true);

    Tase2_IndicationPoint datapointRealQTime
        = Tase2_Domain_addIndicationPoint (
            icc, "datapointRealQTime", TASE2_IND_POINT_TYPE_REAL,
            TASE2_QUALITY, TASE2_TIMESTAMP, false, true);

    Tase2_IndicationPoint datapointRealQTimeExt
        = Tase2_Domain_addIndicationPoint (
            icc, "datapointRealQTimeExt", TASE2_IND_POINT_TYPE_REAL,
            TASE2_QUALITY, TASE2_TIMESTAMP_EXTENDED, false, true);

    Tase2_IndicationPoint datapointState = Tase2_Domain_addIndicationPoint (
        icc, "datapointState", TASE2_IND_POINT_TYPE_STATE, TASE2_NO_QUALITY,
        TASE2_NO_TIMESTAMP, false, true);

    Tase2_IndicationPoint datapointStateQ = Tase2_Domain_addIndicationPoint (
        icc, "datapointStateQ", TASE2_IND_POINT_TYPE_STATE, TASE2_QUALITY,
        TASE2_NO_TIMESTAMP, false, true);

    Tase2_IndicationPoint datapointStateQTime
        = Tase2_Domain_addIndicationPoint (
            icc, "datapointStateQTime", TASE2_IND_POINT_TYPE_STATE,
            TASE2_QUALITY, TASE2_TIMESTAMP, false, true);

    Tase2_IndicationPoint datapointStateQTimeExt
        = Tase2_Domain_addIndicationPoint (
            icc, "datapointStateQTimeExt", TASE2_IND_POINT_TYPE_STATE,
            TASE2_QUALITY, TASE2_TIMESTAMP_EXTENDED, false, true);

    Tase2_IndicationPoint datapointDiscrete = Tase2_Domain_addIndicationPoint (
        icc, "datapointDiscrete", TASE2_IND_POINT_TYPE_DISCRETE,
        TASE2_NO_QUALITY, TASE2_NO_TIMESTAMP, false, true);

    Tase2_IndicationPoint datapointDiscreteQ
        = Tase2_Domain_addIndicationPoint (
            icc, "datapointDiscreteQ", TASE2_IND_POINT_TYPE_DISCRETE,
            TASE2_QUALITY, TASE2_NO_TIMESTAMP, false, true);

    Tase2_IndicationPoint datapointDiscreteQTime
        = Tase2_Domain_addIndicationPoint (
            icc, "datapointDiscreteQTime", TASE2_IND_POINT_TYPE_DISCRETE,
            TASE2_QUALITY, TASE2_TIMESTAMP, false, true);

    Tase2_IndicationPoint datapointDiscreteQTimeExt
        = Tase2_Domain_addIndicationPoint (
            icc, "datapointDiscreteQTimeExt", TASE2_IND_POINT_TYPE_DISCRETE,
            TASE2_QUALITY, TASE2_TIMESTAMP_EXTENDED, false, true);

    Tase2_IndicationPoint datapointStateSup = Tase2_Domain_addIndicationPoint (
        icc, "datapointStateSup", TASE2_IND_POINT_TYPE_STATE_SUPPLEMENTAL,
        TASE2_NO_QUALITY, TASE2_NO_TIMESTAMP, false, true);

    Tase2_IndicationPoint datapointStateSupQ
        = Tase2_Domain_addIndicationPoint (
            icc, "datapointStateSupQ", TASE2_IND_POINT_TYPE_STATE_SUPPLEMENTAL,
            TASE2_QUALITY, TASE2_NO_TIMESTAMP, false, true);

    Tase2_IndicationPoint datapointStateSupQTime
        = Tase2_Domain_addIndicationPoint (
            icc, "datapointStateSupQTime",
            TASE2_IND_POINT_TYPE_STATE_SUPPLEMENTAL, TASE2_QUALITY,
            TASE2_TIMESTAMP, false, true);

    Tase2_IndicationPoint datapointStateSupQTimeExt
        = Tase2_Domain_addIndicationPoint (
            icc, "datapointStateSupQTimeExt",
            TASE2_IND_POINT_TYPE_STATE_SUPPLEMENTAL, TASE2_QUALITY,
            TASE2_TIMESTAMP_EXTENDED, false, true);

    Tase2_Domain_addDSTransferSet (icc, "dsts1");

    Tase2_DataSet dataSet = Tase2_Domain_addDataSet (icc, "DataSet1");

    Tase2_DataSet_addEntry (dataSet, icc, "datapointReal");
    Tase2_DataSet_addEntry (dataSet, icc, "datapointRealQ");
    Tase2_DataSet_addEntry (dataSet, icc, "datapointRealQTime");
    Tase2_DataSet_addEntry (dataSet, icc, "datapointRealQTimeExt");
    Tase2_DataSet_addEntry (dataSet, icc, "datapointState");
    Tase2_DataSet_addEntry (dataSet, icc, "datapointStateQ");
    Tase2_DataSet_addEntry (dataSet, icc, "datapointStateQTime");
    Tase2_DataSet_addEntry (dataSet, icc, "datapointStateQTimeExt");
    Tase2_DataSet_addEntry (dataSet, icc, "datapointDiscrete");
    Tase2_DataSet_addEntry (dataSet, icc, "datapointDiscreteQ");
    Tase2_DataSet_addEntry (dataSet, icc, "datapointDiscreteQTime");
    Tase2_DataSet_addEntry (dataSet, icc, "datapointDiscreteQTimeExt");
    Tase2_DataSet_addEntry (dataSet, icc, "datapointStateSup");
    Tase2_DataSet_addEntry (dataSet, icc, "datapointStateSupQ");
    Tase2_DataSet_addEntry (dataSet, icc, "datapointStateSupQTime");
    Tase2_DataSet_addEntry (dataSet, icc, "datapointStateSupQTimeExt");

    Tase2_BilateralTable_addDataPoint (blt, (Tase2_DataPoint)datapointReal,
                                       true, false);
    Tase2_BilateralTable_addDataPoint (blt, (Tase2_DataPoint)datapointRealQ,
                                       true, false);
    Tase2_BilateralTable_addDataPoint (
        blt, (Tase2_DataPoint)datapointRealQTime, true, false);
    Tase2_BilateralTable_addDataPoint (
        blt, (Tase2_DataPoint)datapointRealQTimeExt, true, false);

    Tase2_BilateralTable_addDataPoint (blt, (Tase2_DataPoint)datapointState,
                                       true, false);
    Tase2_BilateralTable_addDataPoint (blt, (Tase2_DataPoint)datapointStateQ,
                                       true, false);
    Tase2_BilateralTable_addDataPoint (
        blt, (Tase2_DataPoint)datapointStateQTime, true, false);
    Tase2_BilateralTable_addDataPoint (
        blt, (Tase2_DataPoint)datapointStateQTimeExt, true, false);

    Tase2_BilateralTable_addDataPoint (blt, (Tase2_DataPoint)datapointDiscrete,
                                       true, false);
    Tase2_BilateralTable_addDataPoint (
        blt, (Tase2_DataPoint)datapointDiscreteQ, true, false);
    Tase2_BilateralTable_addDataPoint (
        blt, (Tase2_DataPoint)datapointDiscreteQTime, true, false);
    Tase2_BilateralTable_addDataPoint (
        blt, (Tase2_DataPoint)datapointDiscreteQTimeExt, true, false);

    Tase2_BilateralTable_addDataPoint (blt, (Tase2_DataPoint)datapointStateSup,
                                       true, false);
    Tase2_BilateralTable_addDataPoint (
        blt, (Tase2_DataPoint)datapointStateSupQ, true, false);
    Tase2_BilateralTable_addDataPoint (
        blt, (Tase2_DataPoint)datapointStateSupQTime, true, false);
    Tase2_BilateralTable_addDataPoint (
        blt, (Tase2_DataPoint)datapointStateSupQTimeExt, true, false);

    Tase2_Server server = Tase2_Server_createEx (model, endpoint);

    Tase2_Server_addBilateralTable (server, blt);

    Tase2_Server_start (server);
    tase2->start ();

    ASSERT_TRUE (tase2->m_config->m_protocolConfigComplete);

    Thread_sleep (500);

    TASE2Client* client = tase2->m_client;
    TASE2ClientConnection* connection = client->m_active_connection;
    Tase2_Endpoint clientEndpoint = connection->m_endpoint;

    ASSERT_TRUE (Tase2_Endpoint_getState (clientEndpoint)
                 == TASE2_ENDPOINT_STATE_CONNECTED);

    Thread_sleep (1000);

    auto timeout = std::chrono::seconds (10);
    auto start = std::chrono::high_resolution_clock::now ();
    while (ingestCallbackCalled < 16)
    {
        auto now = std::chrono::high_resolution_clock::now ();
        if (now - start > timeout)
        {
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

TEST_F (ReportingTest, ReportingAllTypeDynamicDataset)
{
    tase2->setJsonConfig (protocol_config_1, exchanged_data, tls_config);

    Tase2_DataModel model = Tase2_DataModel_create ();

    Tase2_Domain icc = Tase2_DataModel_addDomain (model, "icc1");

    Tase2_BilateralTable blt
        = Tase2_BilateralTable_create ("blt1", icc, "1.1.1.998", 12);

    Tase2_Endpoint endpoint = Tase2_Endpoint_create (nullptr, true);

    Tase2_Endpoint_setLocalIpAddress (endpoint, "0.0.0.0");
    Tase2_Endpoint_setLocalTcpPort (endpoint, 10002);

    Tase2_Endpoint_setLocalApTitle (endpoint, "1.1.1.999", 12);

    Tase2_IndicationPoint datapointReal = Tase2_Domain_addIndicationPoint (
        icc, "datapointReal", TASE2_IND_POINT_TYPE_REAL, TASE2_NO_QUALITY,
        TASE2_NO_TIMESTAMP, false, true);

    Tase2_IndicationPoint datapointRealQ = Tase2_Domain_addIndicationPoint (
        icc, "datapointRealQ", TASE2_IND_POINT_TYPE_REAL, TASE2_QUALITY,
        TASE2_NO_TIMESTAMP, false, true);

    Tase2_IndicationPoint datapointRealQTime
        = Tase2_Domain_addIndicationPoint (
            icc, "datapointRealQTime", TASE2_IND_POINT_TYPE_REAL,
            TASE2_QUALITY, TASE2_TIMESTAMP, false, true);

    Tase2_IndicationPoint datapointRealQTimeExt
        = Tase2_Domain_addIndicationPoint (
            icc, "datapointRealQTimeExt", TASE2_IND_POINT_TYPE_REAL,
            TASE2_QUALITY, TASE2_TIMESTAMP_EXTENDED, false, true);

    Tase2_IndicationPoint datapointState = Tase2_Domain_addIndicationPoint (
        icc, "datapointState", TASE2_IND_POINT_TYPE_STATE, TASE2_NO_QUALITY,
        TASE2_NO_TIMESTAMP, false, true);

    Tase2_IndicationPoint datapointStateQ = Tase2_Domain_addIndicationPoint (
        icc, "datapointStateQ", TASE2_IND_POINT_TYPE_STATE, TASE2_QUALITY,
        TASE2_NO_TIMESTAMP, false, true);

    Tase2_IndicationPoint datapointStateQTime
        = Tase2_Domain_addIndicationPoint (
            icc, "datapointStateQTime", TASE2_IND_POINT_TYPE_STATE,
            TASE2_QUALITY, TASE2_TIMESTAMP, false, true);

    Tase2_IndicationPoint datapointStateQTimeExt
        = Tase2_Domain_addIndicationPoint (
            icc, "datapointStateQTimeExt", TASE2_IND_POINT_TYPE_STATE,
            TASE2_QUALITY, TASE2_TIMESTAMP_EXTENDED, false, true);

    Tase2_IndicationPoint datapointDiscrete = Tase2_Domain_addIndicationPoint (
        icc, "datapointDiscrete", TASE2_IND_POINT_TYPE_DISCRETE,
        TASE2_NO_QUALITY, TASE2_NO_TIMESTAMP, false, true);

    Tase2_IndicationPoint datapointDiscreteQ
        = Tase2_Domain_addIndicationPoint (
            icc, "datapointDiscreteQ", TASE2_IND_POINT_TYPE_DISCRETE,
            TASE2_QUALITY, TASE2_NO_TIMESTAMP, false, true);

    Tase2_IndicationPoint datapointDiscreteQTime
        = Tase2_Domain_addIndicationPoint (
            icc, "datapointDiscreteQTime", TASE2_IND_POINT_TYPE_DISCRETE,
            TASE2_QUALITY, TASE2_TIMESTAMP, false, true);

    Tase2_IndicationPoint datapointDiscreteQTimeExt
        = Tase2_Domain_addIndicationPoint (
            icc, "datapointDiscreteQTimeExt", TASE2_IND_POINT_TYPE_DISCRETE,
            TASE2_QUALITY, TASE2_TIMESTAMP_EXTENDED, false, true);

    Tase2_IndicationPoint datapointStateSup = Tase2_Domain_addIndicationPoint (
        icc, "datapointStateSup", TASE2_IND_POINT_TYPE_STATE_SUPPLEMENTAL,
        TASE2_NO_QUALITY, TASE2_NO_TIMESTAMP, false, true);

    Tase2_IndicationPoint datapointStateSupQ
        = Tase2_Domain_addIndicationPoint (
            icc, "datapointStateSupQ", TASE2_IND_POINT_TYPE_STATE_SUPPLEMENTAL,
            TASE2_QUALITY, TASE2_NO_TIMESTAMP, false, true);

    Tase2_IndicationPoint datapointStateSupQTime
        = Tase2_Domain_addIndicationPoint (
            icc, "datapointStateSupQTime",
            TASE2_IND_POINT_TYPE_STATE_SUPPLEMENTAL, TASE2_QUALITY,
            TASE2_TIMESTAMP, false, true);

    Tase2_IndicationPoint datapointStateSupQTimeExt
        = Tase2_Domain_addIndicationPoint (
            icc, "datapointStateSupQTimeExt",
            TASE2_IND_POINT_TYPE_STATE_SUPPLEMENTAL, TASE2_QUALITY,
            TASE2_TIMESTAMP_EXTENDED, false, true);

    Tase2_Domain_addDSTransferSet (icc, "dsts1");

    Tase2_BilateralTable_addDataPoint (blt, (Tase2_DataPoint)datapointReal,
                                       true, false);
    Tase2_BilateralTable_addDataPoint (blt, (Tase2_DataPoint)datapointRealQ,
                                       true, false);
    Tase2_BilateralTable_addDataPoint (
        blt, (Tase2_DataPoint)datapointRealQTime, true, false);
    Tase2_BilateralTable_addDataPoint (
        blt, (Tase2_DataPoint)datapointRealQTimeExt, true, false);

    Tase2_BilateralTable_addDataPoint (blt, (Tase2_DataPoint)datapointState,
                                       true, false);
    Tase2_BilateralTable_addDataPoint (blt, (Tase2_DataPoint)datapointStateQ,
                                       true, false);
    Tase2_BilateralTable_addDataPoint (
        blt, (Tase2_DataPoint)datapointStateQTime, true, false);
    Tase2_BilateralTable_addDataPoint (
        blt, (Tase2_DataPoint)datapointStateQTimeExt, true, false);

    Tase2_BilateralTable_addDataPoint (blt, (Tase2_DataPoint)datapointDiscrete,
                                       true, false);
    Tase2_BilateralTable_addDataPoint (
        blt, (Tase2_DataPoint)datapointDiscreteQ, true, false);
    Tase2_BilateralTable_addDataPoint (
        blt, (Tase2_DataPoint)datapointDiscreteQTime, true, false);
    Tase2_BilateralTable_addDataPoint (
        blt, (Tase2_DataPoint)datapointDiscreteQTimeExt, true, false);

    Tase2_BilateralTable_addDataPoint (blt, (Tase2_DataPoint)datapointStateSup,
                                       true, false);
    Tase2_BilateralTable_addDataPoint (
        blt, (Tase2_DataPoint)datapointStateSupQ, true, false);
    Tase2_BilateralTable_addDataPoint (
        blt, (Tase2_DataPoint)datapointStateSupQTime, true, false);
    Tase2_BilateralTable_addDataPoint (
        blt, (Tase2_DataPoint)datapointStateSupQTimeExt, true, false);

    Tase2_Server server = Tase2_Server_createEx (model, endpoint);

    Tase2_Server_addBilateralTable (server, blt);

    Tase2_Server_start (server);
    tase2->start ();

    ASSERT_TRUE (tase2->m_config->m_protocolConfigComplete);

    Thread_sleep (500);

    TASE2Client* client = tase2->m_client;
    TASE2ClientConnection* connection = client->m_active_connection;
    Tase2_Endpoint clientEndpoint = connection->m_endpoint;

    ASSERT_TRUE (Tase2_Endpoint_getState (clientEndpoint)
                 == TASE2_ENDPOINT_STATE_CONNECTED);

    Thread_sleep (1000);

    auto timeout = std::chrono::seconds (10);
    auto start = std::chrono::high_resolution_clock::now ();
    while (ingestCallbackCalled < 16)
    {
        auto now = std::chrono::high_resolution_clock::now ();
        if (now - start > timeout)
        {
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
