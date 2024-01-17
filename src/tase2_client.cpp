#include "datapoint.h"
#include "tase2_client_config.hpp"
#include "tase2_client_connection.hpp"
#include <libtase2/hal_thread.h>
#include <libtase2/tase2_client.h>
#include <libtase2/tase2_common.h>
#include <tase2.hpp>
#include <utility>

static uint64_t
getMonotonicTimeInMs ()
{
    uint64_t timeVal = 0;

    struct timespec ts;

    if (clock_gettime (CLOCK_MONOTONIC, &ts) == 0)
    {
        timeVal = ((uint64_t)ts.tv_sec * 1000LL) + (ts.tv_nsec / 1000000);
    }

    return timeVal;
}

static bool
hasQuality (DPTYPE type)
{
    return type % 4 != 0 && type < COMMAND;
}

static bool
hasTimestamp (DPTYPE type)
{
    return type % 4 >= 2 && type < COMMAND;
}

static uint64_t
GetCurrentTimeInMs ()
{
    struct timeval now;

    gettimeofday (&now, nullptr);

    return ((uint64_t)now.tv_sec * 1000LL) + (now.tv_usec / 1000);
}

static Datapoint*
createDp (const std::string& name)
{
    auto* datapoints = new std::vector<Datapoint*>;

    DatapointValue dpv (datapoints, true);

    auto* dp = new Datapoint (name, dpv);

    return dp;
}

template <class T>
static Datapoint*
createDpWithValue (const std::string& name, const T value)
{
    DatapointValue dpv (value);

    auto* dp = new Datapoint (name, dpv);

    return dp;
}

template <class T>
static Datapoint*
createDatapoint (const std::string& dataname, const T value)
{
    DatapointValue dp_value = DatapointValue (value);
    return new Datapoint (dataname, dp_value);
}

static Datapoint*
addElement (Datapoint* dp, const std::string& name)
{
    DatapointValue& dpv = dp->getData ();

    std::vector<Datapoint*>* subDatapoints = dpv.getDpVec ();

    Datapoint* element = createDp (name);

    if (element)
    {
        subDatapoints->push_back (element);
    }

    return element;
}

template <class T>
static Datapoint*
addElementWithValue (Datapoint* dp, const std::string& name, const T value)
{
    DatapointValue& dpv = dp->getData ();

    std::vector<Datapoint*>* subDatapoints = dpv.getDpVec ();

    Datapoint* element = createDpWithValue (name, value);

    if (element)
    {
        subDatapoints->push_back (element);
    }

    return element;
}

const static std::unordered_map<DPTYPE, std::string> DpTypeMap
    = { { STATE, "State" },
        { STATEQ, "StateQ" },
        { STATEQTIME, "StateQTime" },
        { STATEQTIMEEXT, "StateQTimeExt" },
        { REAL, "Real" },
        { REALQ, "RealQ" },
        { REALQTIME, "RealQTime" },
        { REALQTIMEEXT, "RealQTimeExt" },
        { DISCRETE, "Discrete" },
        { DISCRETEQ, "DiscreteQ" },
        { DISCRETEQTIME, "DiscreteQTime" },
        { DISCRETEQTIMEEXT, "DiscreteQTimeExt" },
        { STATESUP, "StateSup" },
        { STATESUPQ, "StateSupQ" },
        { STATESUPQTIME, "StateSupQTime" },
        { STATESUPQTIMEEXT, "StateSupQTimeExt" },
        { COMMAND, "Command" },
        { SETPOINTREAL, "SetPointReal" },
        { SETPOINTDISCRETE, "SetPointDiscrete" } };

static std::string
dpTypeToStr (DPTYPE type)
{
    return DpTypeMap.at (type);
}

TASE2Client::TASE2Client (TASE2* tase2, TASE2ClientConfig* tase2_client_config)
    : m_config (tase2_client_config), m_tase2 (tase2)
{
}

TASE2Client::~TASE2Client () { stop (); }

void
TASE2Client::stop ()
{
    if (!m_started)
    {
        return;
    }

    m_started = false;

    if (m_monitoringThread != nullptr)
    {
        m_monitoringThread->join ();
        delete m_monitoringThread;
        m_monitoringThread = nullptr;
    }
}

void
TASE2Client::start ()
{
    if (m_started)
        return;

    prepareConnections ();
    m_started = true;
    m_monitoringThread
        = new std::thread (&TASE2Client::_monitoringThread, this);
}

void
TASE2Client::prepareConnections ()
{
    std::lock_guard<std::mutex> lock (connectionsMutex);
    m_connections = std::make_shared<std::vector<TASE2ClientConnection*> > ();
    for (const auto redgroup : m_config->GetConnections ())
    {
        Tase2Utility::log_info ("Add connection: %s",
                                redgroup->ipAddr.c_str ());
        OsiParameters* osiParameters = nullptr;
        if (redgroup->isOsiParametersEnabled)
            osiParameters = &redgroup->osiParameters;
        auto connection = new TASE2ClientConnection (
            this, m_config, redgroup->ipAddr, redgroup->tcpPort, redgroup->tls,
            osiParameters);

        m_connections->push_back (connection);
    }
}

void
TASE2Client::updateConnectionStatus (ConnectionStatus newState)
{
    std::lock_guard<std::mutex> lock (statusMutex);
    if (m_connStatus == newState)
        return;

    m_connStatus = newState;
}

void
TASE2Client::_monitoringThread ()
{
    if (m_started)
    {
        std::lock_guard<std::mutex> lock (m_activeConnectionMtx);
        for (auto clientConnection : *m_connections)
        {
            clientConnection->Start ();
        }
    }

    updateConnectionStatus (ConnectionStatus::NOT_CONNECTED);

    uint64_t backupConnectionStartTime
        = Hal_getTimeInMs () + BACKUP_CONNECTION_TIMEOUT;

    while (m_started)
    {
        std::lock_guard<std::mutex> lock (m_activeConnectionMtx);

        if (m_active_connection == nullptr
            || m_active_connection->Disconnected ())
        {
            bool foundOpenConnections = false;
            for (auto clientConnection : *m_connections)
            {
                backupConnectionStartTime
                    = Hal_getTimeInMs () + BACKUP_CONNECTION_TIMEOUT;

                foundOpenConnections = true;

                clientConnection->Connect ();

                m_active_connection = clientConnection;

                Tase2Utility::log_debug ("Trying connection %s:%d",
                                         clientConnection->IP ().c_str (),
                                         clientConnection->Port ());

                auto start = std::chrono::high_resolution_clock::now ();
                auto timeout = std::chrono::milliseconds (
                    m_config->backupConnectionTimeout ());

                while (!clientConnection->Connected ())
                {
                    auto now = std::chrono::high_resolution_clock::now ();
                    if (now - start > timeout)
                    {
                        clientConnection->Disconnect ();
                        m_active_connection = nullptr;
                        break;
                    }
                    Thread_sleep (10);
                }

                if (m_active_connection)
                {
                    break;
                }
            }
        }
        else
        {
            backupConnectionStartTime
                = Hal_getTimeInMs () + BACKUP_CONNECTION_TIMEOUT;
        }

        Thread_sleep (100);
    }

    for (auto& clientConnection : *m_connections)
    {
        std::lock_guard<std::mutex> lock (m_activeConnectionMtx);
        delete clientConnection;
    }

    m_connections->clear ();
}

bool
TASE2Client::sendCommand (std::string domain, std::string name, int value,
                          bool select, long time)
{
    // send single command over active connection
    bool success = false;

    // check if the data point is in the exchange configuration
    if (m_config->getExchangeDefinitionByRef (domain + ":" + name) == nullptr)
    {
        Tase2Utility::log_error (
            "Failed to send command - no such data point");

        return false;
    }

    if (m_active_connection != nullptr)
    {
        success = m_active_connection->sendCommand (domain, name, value,
                                                    select, time);
    }

    if (success)
    {
        Tase2_PointValue pointvalue = Tase2_PointValue_createDiscrete (value);
        handleValue (domain + ":" + name, pointvalue, GetCurrentTimeInMs (),
                     true);
    }

    return success;
}
bool
TASE2Client::sendSetPointReal (std::string domain, std::string name,
                               float value, bool select, long time)
{
    bool success = false;

    // check if the data point is in the exchange configuration
    if (m_config->getExchangeDefinitionByRef (domain + ":" + name) == nullptr)
    {
        Tase2Utility::log_error (
            "Failed to send setpointreal - no such data point");

        return false;
    }

    if (m_active_connection != nullptr)
    {
        success = m_active_connection->sendSetPointReal (domain, name, value,
                                                         select, time);
    }

    if (success)
    {
        Tase2_PointValue pointvalue = Tase2_PointValue_createReal (value);
        handleValue (domain + ":" + name, pointvalue, GetCurrentTimeInMs (),
                     true);
    }

    return success;
}
bool
TASE2Client::sendSetPointDiscrete (std::string domain, std::string name,
                                   int value, bool select, long time)
{
    // send single command over active connection
    bool success = false;

    // check if the data point is in the exchange configuration
    if (m_config->getExchangeDefinitionByRef (domain + ":" + name) == nullptr)
    {
        Tase2Utility::log_error (
            "Failed to send setpointdiscrete - no such data point");

        return false;
    }

    if (m_active_connection != nullptr)
    {
        success = m_active_connection->sendSetPointDiscrete (
            domain, name, value, select, time);
    }

    if (success)
    {
        Tase2_PointValue pointvalue = Tase2_PointValue_createDiscrete (value);
        handleValue (domain + ":" + name, pointvalue, GetCurrentTimeInMs (),
                     true);
    }

    return success;
}

void
TASE2Client::sendData (const std::vector<Datapoint*>& datapoints,
                       const std::vector<std::string>& labels)
{
    int i = 0;

    for (Datapoint* item_dp : datapoints)
    {
        std::vector<Datapoint*> points;
        points.push_back (item_dp);

        m_tase2->ingest (labels.at (i), points);
        i++;
    }
}

void
TASE2Client::handleValue (std::string ref, Tase2_PointValue value,
                          uint64_t timestamp, bool ack)
{
    std::vector<std::string> labels;
    std::vector<Datapoint*> datapoints;
    const std::shared_ptr<DataExchangeDefinition> def
        = m_config->getExchangeDefinitionByRef (ref);

    if (!def)
    {
        Tase2Utility::log_error ("Datapoint %s not in exchanged data ",
                                 ref.c_str ());
        return;
    }

    DPTYPE typeId = def->type;

    labels.push_back (def->label);

    m_handleMonitoringData (def->ref, datapoints, def->label, typeId, value,
                            timestamp);

    sendData (datapoints, labels);

    if (ack)
    {
        Tase2_PointValue_destroy (value);
    }
}

void
TASE2Client::handleAllValues ()
{
    std::vector<std::string> labels;
    std::vector<Datapoint*> datapoints;

    for (const auto& pair : m_config->polledDatapoints ())
    {
        const std::shared_ptr<DataExchangeDefinition> def = pair.second;

        DPTYPE typeId = def->type;

        labels.push_back (def->label);

        m_handleMonitoringData (def->ref, datapoints, def->label, typeId,
                                nullptr, GetCurrentTimeInMs ());
    }
    sendData (datapoints, labels);
}

void
TASE2Client::m_handleMonitoringData (const std::string& ref,
                                     std::vector<Datapoint*>& datapoints,
                                     const std::string& label, DPTYPE type,
                                     Tase2_PointValue value,
                                     uint64_t timestamp)
{
    Tase2_ClientError error;

    auto def = m_config->getExchangeDefinitionByRef (ref);
    if (!def)
    {
        Tase2Utility::log_error ("Invalid definition for %s", ref.c_str ());
        return;
    }

    uint64_t ts = timestamp;

    std::pair<std::string, std::string> domainNamePair
        = TASE2ClientConfig::splitExchangeRef (ref);

    Tase2_PointValue pointvalue;

    if (!value)
    {
        pointvalue = m_active_connection->readValue (
            &error, domainNamePair.first.c_str (),
            domainNamePair.second.c_str ());
    }
    else
    {
        pointvalue = value;
    }

    if (!pointvalue)
    {
        Tase2Utility::log_error ("Couldn't get value for %s", ref.c_str ());
        return;
    }

    datapoints.push_back (m_createDataObject (
        pointvalue, domainNamePair.first, domainNamePair.second, ts, type));

    if (!value)
    {
        Tase2_PointValue_destroy (pointvalue);
    }
}

static std::string
validityToString (Tase2_DataFlags flags)
{
    if (flags | TASE2_DATA_FLAGS_VALIDITY_VALID)
    {
        return "valid";
    }
    else if (flags | TASE2_DATA_FLAGS_VALIDITY_HELD)
    {
        return "held";
    }
    else if (flags | TASE2_DATA_FLAGS_VALIDITY_SUSPECT)
    {
        return "suspect";
    }
    else if (flags | TASE2_DATA_FLAGS_VALIDITY_NOTVALID)
    {
        return "invalid";
    }
    return "";
}

static std::string
currentSourceToString (Tase2_DataFlags flags)
{
    if (flags | TASE2_DATA_FLAGS_CURRENT_SOURCE_TELEMETERED)
    {
        return "telemetered";
    }
    else if (flags | TASE2_DATA_FLAGS_CURRENT_SOURCE_ENTERED)
    {
        return "entered";
    }
    else if (flags | TASE2_DATA_FLAGS_CURRENT_SOURCE_CALCULATED)
    {
        return "calculated";
    }
    else if (flags | TASE2_DATA_FLAGS_CURRENT_SOURCE_ESTIMATED)
    {
        return "estimated";
    }
    return "";
}

static std::string
normalValueToString (Tase2_DataFlags flags)
{
    if (flags | TASE2_DATA_FLAGS_NORMAL_VALUE)
    {
        return "normal";
    }
    return "abnormal";
}

Datapoint*
TASE2Client::m_createDataObject (Tase2_PointValue value,
                                 const std::string& domain,
                                 const std::string& name, uint64_t ts,
                                 DPTYPE type)
{
    auto datapoints = new std::vector<Datapoint*>;

    datapoints->push_back (createDatapoint ("do_type", dpTypeToStr (type)));
    datapoints->push_back (createDatapoint ("do_domain", domain));
    datapoints->push_back (createDatapoint ("do_name", name));

    switch (type)
    {
    case COMMAND:
    case SETPOINTDISCRETE:
    case DISCRETE:
    case DISCRETEQ:
    case DISCRETEQTIME:
    case DISCRETEQTIMEEXT: {
        datapoints->push_back (createDatapoint (
            "do_value", (int64_t)Tase2_PointValue_getValueDiscrete (value)));
        break;
    }
    case STATESUP:
    case STATESUPQ:
    case STATESUPQTIME:
    case STATESUPQTIMEEXT: {
        datapoints->push_back (createDatapoint (
            "do_value",
            (int64_t)Tase2_PointValue_getValueStateSupplemental (value)));
        break;
    }
    case STATE:
    case STATEQ:
    case STATEQTIME:
    case STATEQTIMEEXT: {
        datapoints->push_back (createDatapoint (
            "do_value", (int64_t)Tase2_PointValue_getValueState (value)));
        break;
    }
    case SETPOINTREAL:
    case REAL:
    case REALQ:
    case REALQTIME:
    case REALQTIMEEXT: {
        datapoints->push_back (createDatapoint (
            "do_value", (double)Tase2_PointValue_getValueReal (value)));
        break;
    }
    default: {
        break;
    }
    }

    if (hasQuality (type))
    {
        Tase2_DataFlags flags = Tase2_PointValue_getFlags (value);
        datapoints->push_back (
            createDatapoint ("do_validity", validityToString (flags)));
        datapoints->push_back (
            createDatapoint ("do_cs", currentSourceToString (flags)));
        datapoints->push_back (createDatapoint ("do_quality_normal_value",
                                                normalValueToString (flags)));
    }

    if (hasTimestamp (type))
    {
        datapoints->push_back (createDatapoint ("do_ts", (long)ts));
        datapoints->push_back (createDatapoint ("do_ts_validity", "valid"));
    }

    DatapointValue dpv (datapoints, true);

    Datapoint* dp = new Datapoint ("data_object", dpv);

    return dp;
}
