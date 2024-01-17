#include "tase2_client_config.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <regex>
#include <unordered_set>
#include <vector>

#define JSON_PROTOCOL_STACK "protocol_stack"
#define JSON_TRANSPORT_LAYER "transport_layer"
#define JSON_APPLICATION_LAYER "application_layer"
#define JSON_DATASETS "datasets"
#define JSON_CONNECTIONS "connections"
#define JSON_IP "ip_addr"
#define JSON_PORT "port"
#define JSON_TLS "tls"
#define JSON_DATASET_REF "dataset_ref"
#define JSON_DATASET_ENTRIES "entries"
#define JSON_POLLING_INTERVAL "polling_interval"
#define JSON_DATASET_TRANSFER_SETS "dataset_transfer_sets"
#define JSON_NAME "name"
#define JSON_DSTS_CON "dsConditions"
#define JSON_INTERVAL "interval"
#define JSON_INTEGRITY "integrity"
#define JSON_CHANGE "change"
#define JSON_OPERATOR_REQUEST "operator_request"
#define JSON_EXTERNAL_EVENT "external_event"
#define JSON_START_TIME "startTime"
#define JSON_TLE "tle"
#define JSON_BUFFER_TIME "bufTm"
#define JSON_INTEGRITY_CHECK "integrityCheck"
#define JSON_CRITICAL "critical"
#define JSON_RBE "rbe"
#define JSON_ALL_CHANGES_REPORTED "allChangesReported"
#define JSON_OSI "osi"

#define JSON_LOCAL_AP "local_ap_title"
#define JSON_LOCAL_AE "local_ae_qualifier"
#define JSON_REMOTE_AP "remote_ap_title"
#define JSON_REMOTE_AE "remote_ae_qualifier"
#define JSON_LOCAL_PSEL "local_psel"
#define JSON_LOCAL_SSEL "local_ssel"
#define JSON_LOCAL_TSEL "local_tsel"
#define JSON_REMOTE_PSEL "remote_psel"
#define JSON_REMOTE_SSEL "remote_ssel"
#define JSON_REMOTE_TSEL "remote_tsel"

#define JSON_DOMAIN "domain"
#define JSON_EXCHANGED_DATA "exchanged_data"
#define JSON_DATAPOINTS "datapoints"
#define JSON_PROTOCOLS "protocols"
#define JSON_LABEL "label"

#define PROTOCOL_TASE2 "tase2"
#define JSON_PROT_NAME "name"
#define JSON_PROT_REF "ref"
#define JSON_TYPE_ID "typeid"

using namespace rapidjson;

static const std::unordered_map<std::string, int> dsConditions
    = { { JSON_INTERVAL, TASE2_DS_CONDITION_INTERVAL },
        { JSON_INTEGRITY, TASE2_DS_CONDITION_INTEGRITY },
        { JSON_CHANGE, TASE2_DS_CONDITION_CHANGE },
        { JSON_OPERATOR_REQUEST, TASE2_DS_CONDITION_OPERATOR_REQUESTED },
        { JSON_EXTERNAL_EVENT, TASE2_DS_CONDITION_EXTERNAL_EVENT } };

static const std::unordered_map<std::string, Type> expectedTypeMap
    = { { JSON_PROTOCOL_STACK, kObjectType },
        { JSON_TRANSPORT_LAYER, kObjectType },
        { JSON_APPLICATION_LAYER, kObjectType },
        { JSON_DATASETS, kArrayType },
        { JSON_CONNECTIONS, kArrayType },
        { JSON_IP, kStringType },
        { JSON_PORT, kNumberType },
        { JSON_TLS, kTrueType },
        { JSON_DATASET_REF, kStringType },
        { JSON_DATASET_ENTRIES, kArrayType },
        { JSON_POLLING_INTERVAL, kNumberType },
        { JSON_DATASET_TRANSFER_SETS, kArrayType },
        { JSON_NAME, kStringType },
        { JSON_DSTS_CON, kArrayType },
        { JSON_INTERVAL, kNumberType },
        { JSON_TLE, kNumberType },
        { JSON_BUFFER_TIME, kNumberType },
        { JSON_INTEGRITY_CHECK, kNumberType },
        { JSON_CRITICAL, kTrueType },
        { JSON_RBE, kTrueType },
        { JSON_ALL_CHANGES_REPORTED, kTrueType },
        { JSON_OSI, kObjectType },
        { JSON_LOCAL_AP, kStringType },
        { JSON_LOCAL_AE, kNumberType },
        { JSON_REMOTE_AP, kStringType },
        { JSON_REMOTE_AE, kNumberType },
        { JSON_LOCAL_PSEL, kStringType },
        { JSON_LOCAL_SSEL, kStringType },
        { JSON_LOCAL_TSEL, kStringType },
        { JSON_REMOTE_PSEL, kStringType },
        { JSON_REMOTE_SSEL, kStringType },
        { JSON_REMOTE_TSEL, kStringType } };

const static std::unordered_map<std::string, DPTYPE> dpTypeMap
    = { { "State", STATE },
        { "StateQ", STATEQ },
        { "StateQTime", STATEQTIME },
        { "StateQTimeExt", STATEQTIMEEXT },
        { "Real", REAL },
        { "RealQ", REALQ },
        { "RealQTime", REALQTIME },
        { "RealQTimeExt", REALQTIMEEXT },
        { "Discrete", DISCRETE },
        { "DiscreteQ", DISCRETEQ },
        { "DiscreteQTime", DISCRETEQTIME },
        { "DiscreteQTimeExt", DISCRETEQTIMEEXT },
        { "StateSup", STATESUP },
        { "StateSupQ", STATESUPQ },
        { "StateSupQTime", STATESUPQTIME },
        { "StateSupQTimeExt", STATESUPQTIMEEXT },
        { "Command", COMMAND },
        { "SetPointReal", SETPOINTREAL },
        { "SetPointDiscrete", SETPOINTDISCRETE } };

DPTYPE
TASE2ClientConfig::getDpTypeFromString (const std::string& type)
{
    auto it = dpTypeMap.find (type);
    if (it != dpTypeMap.end ())
    {
        return it->second;
    }
    return DP_TYPE_UNKNOWN;
}

static bool
isCorrectType (const std::string& parameter, const Value& value)
{
    auto it = expectedTypeMap.find (parameter);

    if (it == expectedTypeMap.end ())
    {
        return true;
    }

    if (value.IsBool ())
    {
        return it->second == kTrueType;
    }

    return it->second == value.GetType ();
}

bool
TASE2ClientConfig::isValidIPAddress (const std::string& addrStr)
{
    // see
    // https://stackoverflow.com/questions/318236/how-do-you-validate-that-a-string-is-a-valid-ipv4-address-in-c
    struct sockaddr_in sa;
    int result = inet_pton (AF_INET, addrStr.c_str (),
                            &(sa.sin_addr)); // LCOV_EXCL_LINE

    return (result == 1);
}

void
TASE2ClientConfig::deleteExchangeDefinitions ()
{
    m_exchangeDefinitions.clear ();
    m_exchangeDefinitionsRef.clear ();
    m_polledDatapoints.clear ();
}

TASE2ClientConfig::~TASE2ClientConfig () { deleteExchangeDefinitions (); }

std::pair<std::string, std::string>
TASE2ClientConfig::splitExchangeRef (std::string ref)
{
    size_t colonPos = ref.find (':');
    std::string domainRef = ref.substr (0, colonPos);
    std::string dpRef = ref.substr (colonPos + 1);
    return std::pair<std::string, std::string> (domainRef, dpRef);
}

bool
traverseAndCheckTypes (const Value& value)
{
    bool result = true;
    std::vector<std::pair<std::string, const Value*> > stack;
    stack.emplace_back ("", &value);

    while (!stack.empty ())
    {
        auto [parent, currentValue] = stack.back ();
        stack.pop_back ();

        if (currentValue->IsObject ())
        {
            for (Value::ConstMemberIterator itr = currentValue->MemberBegin ();
                 itr != currentValue->MemberEnd (); ++itr)
            {
                std::string key = parent.empty ()
                                      ? itr->name.GetString ()
                                      : parent + "." + itr->name.GetString ();
                stack.emplace_back (key, &itr->value);
            }
        }
        else if (currentValue->IsArray ())
        {
            for (SizeType i = 0; i < currentValue->Size (); ++i)
            {
                std::string index = parent + "[" + std::to_string (i) + "]";
                stack.emplace_back (index, &(*currentValue)[i]);
            }
        }
        else
        {
            if (!isCorrectType (parent, *currentValue))
            {
                Tase2Utility::log_error ("Wrong data type in protocol stack "
                                         "configuration: Parameter - %s",
                                         parent.c_str ());
                result = false;
            }
        }
    }

    return result;
}

void
TASE2ClientConfig::importProtocolConfig (const std::string& protocolConfig)
{
    m_protocolConfigComplete = false;

    Document document;

    if (document.Parse (protocolConfig.c_str ()).HasParseError ())
    {
        Tase2Utility::log_fatal ("Parsing error in protocol configuration");
        return;
    }

    if (!document.IsObject ())
    {
        return; // LCOV_EXCL_LINE
    }

    if (!traverseAndCheckTypes (document))
    {
        return;
    }

    if (!document.HasMember (JSON_PROTOCOL_STACK)
        || !document[JSON_PROTOCOL_STACK].IsObject ())
    {
        return;
    }

    const Value& protocolStack = document[JSON_PROTOCOL_STACK];

    if (!protocolStack.HasMember (JSON_TRANSPORT_LAYER))
    {
        Tase2Utility::log_fatal ("transport layer configuration is missing");
        return;
    }

    const Value& transportLayer = protocolStack[JSON_TRANSPORT_LAYER];

    if (!transportLayer.HasMember (JSON_CONNECTIONS))
    {
        Tase2Utility::log_fatal ("no connections are configured");
        return;
    }

    const Value& connections = transportLayer[JSON_CONNECTIONS];

    for (const Value& connection : connections.GetArray ())
    {
        if (connection.HasMember (JSON_IP))
        {
            std::string srvIp = connection[JSON_IP].GetString ();
            if (!isValidIPAddress (srvIp))
            {
                Tase2Utility::log_error ("Invalid Ip address %s",
                                         srvIp.c_str ());
                continue;
            }
            if (connection.HasMember (JSON_PORT))
            {
                int portVal = connection[JSON_PORT].GetInt ();
                if (portVal <= 0 || portVal >= 65636)
                {
                    Tase2Utility::log_error ("Invalid port %d", portVal);
                    continue;
                }
            }

            auto group = std::make_shared<RedGroup> ();

            group->ipAddr = connection[JSON_IP].GetString ();
            group->tcpPort = connection[JSON_PORT].GetInt ();
            group->tls = false;

            if (connection.HasMember ("osi"))
            {
                importJsonConnectionOsiConfig (connection["osi"], *group);
            }

            if (connection.HasMember ("tls"))
            {
                if (connection["tls"].IsBool ())
                {
                    group->tls = (connection["tls"].GetBool ());
                }
                else
                {
                    printf (
                        "connection.tls has invalid type -> not using TLS\n");
                    Tase2Utility::log_warn (
                        "connection.tls has invalid type -> not using TLS");
                }
            }

            m_connections.push_back (group);
        }
    }

    if (transportLayer.HasMember ("backupTimeout"))
    {
        m_backupConnectionTimeout = transportLayer["backupTimeout"].GetInt ();
    }

    if (!protocolStack.HasMember (JSON_APPLICATION_LAYER))
    {
        Tase2Utility::log_fatal ("transport layer configuration is missing");
        return;
    }

    const Value& applicationLayer = protocolStack[JSON_APPLICATION_LAYER];

    if (applicationLayer.HasMember (JSON_POLLING_INTERVAL))
    {
        int intVal = applicationLayer[JSON_POLLING_INTERVAL].GetInt ();
        if (intVal < 0)
        {
            Tase2Utility::log_error ("polling_interval must be positive");
            return;
        }
        pollingInterval = intVal;
    }

    if (applicationLayer.HasMember (JSON_DATASETS))
    {
        for (const auto& datasetVal :
             applicationLayer[JSON_DATASETS].GetArray ())
        {
            if (!datasetVal.IsObject ()
                || !datasetVal.HasMember (JSON_DATASET_REF)
                || !datasetVal[JSON_DATASET_REF].IsString ())
                continue;

            std::string domainName = "vcc";

            if (datasetVal.HasMember (JSON_DOMAIN))
            {
                domainName = datasetVal[JSON_DOMAIN].GetString ();
            }

            std::string datasetRef = datasetVal[JSON_DATASET_REF].GetString ();
            auto dataset = std::make_shared<Dataset> ();
            dataset->datasetRef = datasetRef;
            dataset->domain = domainName;
            if (datasetVal.HasMember (JSON_DATASET_ENTRIES)
                && datasetVal[JSON_DATASET_ENTRIES].IsArray ())
            {
                for (const auto& entryVal :
                     datasetVal[JSON_DATASET_ENTRIES].GetArray ())
                {
                    if (entryVal.IsString ())
                    {
                        std::string name = entryVal.GetString ();

                        const std::shared_ptr<DataExchangeDefinition> def
                            = getExchangeDefinitionByRef (domainName + ":"
                                                          + name);

                        dataset->entries.push_back (name);

                        if (def)
                        {
                            m_polledDatapoints.erase (domainName + ":" + name);
                        }
                    }
                }
            }
            if (datasetVal.HasMember ("dynamic")
                && datasetVal["dynamic"].IsBool ())
            {
                dataset->dynamic = datasetVal["dynamic"].GetBool ();
            }
            else
            {
                Tase2Utility::log_warn (
                    "Dataset %s has no dynamic value -> defaulting to static",
                    dataset->datasetRef.c_str ());
                dataset->dynamic = false;
            }
            m_datasets.insert ({ datasetRef, dataset });
        }
    }

    if (applicationLayer.HasMember (JSON_DATASET_TRANSFER_SETS))
    {
        for (const auto& dstsVal :
             applicationLayer[JSON_DATASET_TRANSFER_SETS].GetArray ())
        {
            if (!dstsVal.IsObject ())
                continue;
            auto dsts = std::make_shared<DatasetTransferSet> ();

            if (dstsVal.HasMember (JSON_DOMAIN))
            {
                dsts->domain = dstsVal[JSON_DOMAIN].GetString ();
            }
            if (dstsVal.HasMember (JSON_NAME))
            {
                dsts->dstsRef = dstsVal[JSON_NAME].GetString ();
            }
            else
            {
                continue;
            }

            if (dstsVal.HasMember (JSON_DATASET_REF))
            {
                dsts->datasetRef = dstsVal[JSON_DATASET_REF].GetString ();
            }
            else
            {
                dsts->datasetRef = "";
            }

            if (dstsVal.HasMember (JSON_DSTS_CON))
            {
                for (const auto& dsConVal : dstsVal[JSON_DSTS_CON].GetArray ())
                {
                    if (dsConVal.IsString ())
                    {
                        auto it = dsConditions.find (dsConVal.GetString ());
                        if (it == dsConditions.end ())
                            continue; // LCOV_EXCL_LINE
                        dsts->dsConditions |= it->second;
                    }
                }
            }
            else
            {
                dsts->dsConditions = 0;
            }

            if (dstsVal.HasMember (JSON_INTERVAL))
            {
                dsts->interval = dstsVal[JSON_INTERVAL].GetInt ();
            }

            if (dstsVal.HasMember (JSON_TLE))
            {
                dsts->tle = dstsVal[JSON_TLE].GetInt ();
            }

            if (dstsVal.HasMember (JSON_INTEGRITY_CHECK))
            {
                dsts->integrityCheck = dstsVal[JSON_INTEGRITY_CHECK].GetInt ();
            }

            if (dstsVal.HasMember (JSON_CRITICAL))
            {
                dsts->critical = dstsVal[JSON_CRITICAL].GetBool ();
            }

            if (dstsVal.HasMember (JSON_RBE))
            {
                dsts->rbe = dstsVal[JSON_RBE].GetBool ();
            }

            if (dstsVal.HasMember (JSON_BUFFER_TIME))
            {
                dsts->bufferTime = dstsVal[JSON_BUFFER_TIME].GetInt ();
            }
            else
            {
                dsts->bufferTime = 0;
            }

            if (dstsVal.HasMember (JSON_START_TIME))
            {
                dsts->startTime = dstsVal[JSON_START_TIME].GetInt ();
            }
            else
            {
                dsts->startTime = 0;
            }

            if (dstsVal.HasMember (JSON_ALL_CHANGES_REPORTED))
            {
                dsts->allChangesReported
                    = dstsVal[JSON_ALL_CHANGES_REPORTED].GetBool ();
            }

            m_dsTranferSets.insert ({ dsts->dstsRef, std::move (dsts) });
        }
    }

    m_protocolConfigComplete = true;
}

void
TASE2ClientConfig::importJsonConnectionOsiConfig (
    const rapidjson::Value& connOsiConfig, RedGroup& iedConnectionParam)
{
    // Preconditions
    if (!connOsiConfig.IsObject ())
    {
        throw ConfigurationException ("'OSI' section is not valid");
    }

    OsiParameters* osiParams = &iedConnectionParam.osiParameters;

    // AE qualifiers
    if (connOsiConfig.HasMember ("local_ae_qualifier"))
    {
        if (!connOsiConfig["local_ae_qualifier"].IsInt ())
        {
            throw ConfigurationException (
                "bad format for 'local_ae_qualifier'");
        }

        osiParams->localAeQualifier
            = connOsiConfig["local_ae_qualifier"].GetInt ();
    }

    if (connOsiConfig.HasMember ("remote_ae_qualifier"))
    {
        if (!connOsiConfig["remote_ae_qualifier"].IsInt ())
        {
            throw ConfigurationException (
                "bad format for 'remote_ae_qualifier'");
        }

        osiParams->remoteAeQualifier
            = connOsiConfig["remote_ae_qualifier"].GetInt ();
    }

    // AP Title
    if (connOsiConfig.HasMember ("local_ap_title"))
    {
        if (!connOsiConfig["local_ap_title"].IsString ())
        {
            throw ConfigurationException ("bad format for 'local_ap_title'");
        }

        osiParams->localApTitle = connOsiConfig["local_ap_title"].GetString ();
        std::replace (osiParams->localApTitle.begin (),
                      osiParams->localApTitle.end (), ',', '.');

        // check 'localApTitle' contains digits and dot only
        std::string strToCheck = osiParams->localApTitle;
        strToCheck.erase (
            std::remove (strToCheck.begin (), strToCheck.end (), '.'),
            strToCheck.end ());

        if (!std::regex_match (strToCheck, std::regex ("[0-9]*")))
        {
            throw ConfigurationException ("'local_ap_title' is not valid");
        }
    }

    if (connOsiConfig.HasMember ("remote_ap_title"))
    {
        if (!connOsiConfig["remote_ap_title"].IsString ())
        {
            throw ConfigurationException ("bad format for 'remote_ap_title'");
        }

        osiParams->remoteApTitle
            = connOsiConfig["remote_ap_title"].GetString ();
        std::replace (osiParams->remoteApTitle.begin (),
                      osiParams->remoteApTitle.end (), ',', '.');
        // check 'remoteApTitle' contains digits and dot only
        std::string strToCheck = osiParams->remoteApTitle;
        strToCheck.erase (
            std::remove (strToCheck.begin (), strToCheck.end (), '.'),
            strToCheck.end ());

        if (!std::regex_match (strToCheck, std::regex ("[0-9]*")))
        {
            throw ConfigurationException ("'remote_ap_title' is not valid");
        }
    }

    // Selector
    importJsonConnectionOsiSelectors (connOsiConfig, osiParams);
    iedConnectionParam.isOsiParametersEnabled = true;
}

void
TASE2ClientConfig::importJsonConnectionOsiSelectors (
    const rapidjson::Value& connOsiConfig, OsiParameters* osiParams)
{
    if (connOsiConfig.HasMember ("local_psel"))
    {
        if (!connOsiConfig["local_psel"].IsString ())
        {
            throw ConfigurationException ("bad format for 'local_psel'");
        }

        std::string inputOsiSelector
            = connOsiConfig["local_psel"].GetString ();
        osiParams->localPSelector.size
            = parseOsiPSelector (inputOsiSelector, &osiParams->localPSelector);
    }

    if (connOsiConfig.HasMember ("local_ssel"))
    {
        if (!connOsiConfig["local_ssel"].IsString ())
        {
            throw ConfigurationException ("bad format for 'local_ssel'");
        }

        std::string inputOsiSelector
            = connOsiConfig["local_ssel"].GetString ();
        osiParams->localSSelector.size
            = parseOsiSSelector (inputOsiSelector, &osiParams->localSSelector);
    }

    if (connOsiConfig.HasMember ("local_tsel"))
    {
        if (!connOsiConfig["local_tsel"].IsString ())
        {
            throw ConfigurationException ("bad format for 'local_tsel'");
        }

        std::string inputOsiSelector
            = connOsiConfig["local_tsel"].GetString ();
        osiParams->localTSelector.size
            = parseOsiTSelector (inputOsiSelector, &osiParams->localTSelector);
    }

    if (connOsiConfig.HasMember ("remote_psel"))
    {
        if (!connOsiConfig["remote_psel"].IsString ())
        {
            throw ConfigurationException ("bad format for 'remote_psel'");
        }

        std::string inputOsiSelector
            = connOsiConfig["remote_psel"].GetString ();
        osiParams->remotePSelector.size = parseOsiPSelector (
            inputOsiSelector, &osiParams->remotePSelector);
    }

    if (connOsiConfig.HasMember ("remote_ssel"))
    {
        if (!connOsiConfig["remote_ssel"].IsString ())
        {
            throw ConfigurationException ("bad format for 'remote_ssel'");
        }

        std::string inputOsiSelector
            = connOsiConfig["remote_ssel"].GetString ();
        osiParams->remoteSSelector.size = parseOsiSSelector (
            inputOsiSelector, &osiParams->remoteSSelector);
    }

    if (connOsiConfig.HasMember ("remote_tsel"))
    {
        if (!connOsiConfig["remote_tsel"].IsString ())
        {
            throw ConfigurationException ("bad format for 'remote_tsel'");
        }

        std::string inputOsiSelector
            = connOsiConfig["remote_tsel"].GetString ();
        osiParams->remoteTSelector.size = parseOsiTSelector (
            inputOsiSelector, &osiParams->remoteTSelector);
    }
}

OsiSelectorSize
TASE2ClientConfig::parseOsiPSelector (std::string& inputOsiSelector,
                                      Tase2_PSelector* pselector)
{
    return parseOsiSelector (inputOsiSelector, pselector->value, 16);
}

OsiSelectorSize
TASE2ClientConfig::parseOsiSSelector (std::string& inputOsiSelector,
                                      Tase2_SSelector* sselector)
{
    return parseOsiSelector (inputOsiSelector, sselector->value, 16);
}

OsiSelectorSize
TASE2ClientConfig::parseOsiTSelector (std::string& inputOsiSelector,
                                      Tase2_TSelector* tselector)
{
    return parseOsiSelector (inputOsiSelector, tselector->value, 4);
}

OsiSelectorSize
TASE2ClientConfig::parseOsiSelector (std::string& inputOsiSelector,
                                     uint8_t* selectorValue,
                                     const uint8_t selectorSize)
{
    char* tokenContext = nullptr;
    const char* nextToken
        = strtok_r (&inputOsiSelector[0], " ,.-", &tokenContext);
    uint8_t count = 0;

    while (nullptr != nextToken)
    {
        if (count >= selectorSize)
        {
            throw ConfigurationException (
                "bad format for 'OSI Selector' (too many bytes)");
        }

        int base = 10;

        if (0 == strncmp (nextToken, "0x", 2))
        {
            base = 16;
        }

        unsigned long ul = 0;

        try
        {
            ul = std::stoul (nextToken, nullptr, base);
        }
        catch (std::invalid_argument&)
        {
            throw ConfigurationException (
                "bad format for 'OSI Selector' (not a byte)");
        }
        catch (std::out_of_range&)
        {
            throw ConfigurationException (
                "bad format for 'OSI Selector (exceed an int)'");
        }

        if (ul > 255)
        {
            throw ConfigurationException (
                "bad format for 'OSI Selector' (exceed a byte)");
        }

        selectorValue[count] = static_cast<uint8_t> (ul);
        count++;
        nextToken = strtok_r (nullptr, " ,.-", &tokenContext);
    }

    return count;
}

void
TASE2ClientConfig::importExchangeConfig (const std::string& exchangeConfig)
{
    m_exchangeConfigComplete = false;

    Document document;

    if (document.Parse (const_cast<char*> (exchangeConfig.c_str ()))
            .HasParseError ())
    {
        Tase2Utility::log_fatal (
            "Parsing error in data exchange configuration");

        return;
    }

    if (!document.IsObject ())
        return;

    if (!document.HasMember (JSON_EXCHANGED_DATA)
        || !document[JSON_EXCHANGED_DATA].IsObject ())
    {
        return;
    }

    const Value& exchangeData = document[JSON_EXCHANGED_DATA];

    if (!exchangeData.HasMember (JSON_DATAPOINTS)
        || !exchangeData[JSON_DATAPOINTS].IsArray ())
    {
        return;
    }

    const Value& datapoints = exchangeData[JSON_DATAPOINTS];

    for (const Value& datapoint : datapoints.GetArray ())
    {

        if (!datapoint.IsObject ())
            return;

        if (!datapoint.HasMember (JSON_LABEL)
            || !datapoint[JSON_LABEL].IsString ())
            return;

        std::string label = datapoint[JSON_LABEL].GetString ();

        if (!datapoint.HasMember (JSON_PROTOCOLS)
            || !datapoint[JSON_PROTOCOLS].IsArray ())
            return;

        for (const Value& protocol : datapoint[JSON_PROTOCOLS].GetArray ())
        {

            if (!protocol.HasMember (JSON_PROT_NAME)
                || !protocol[JSON_PROT_NAME].IsString ())
                return;

            std::string protocolName = protocol[JSON_PROT_NAME].GetString ();

            if (!protocol.HasMember (JSON_PROT_REF)
                || !protocol[JSON_PROT_REF].IsString ())
                return;

            std::string protocolRef = protocol[JSON_PROT_REF].GetString ();

            if (!protocol.HasMember (JSON_TYPE_ID)
                || !protocol[JSON_TYPE_ID].IsString ())
                return;

            std::string type = protocol[JSON_TYPE_ID].GetString ();

            if (getDpTypeFromString (type) == DP_TYPE_UNKNOWN)
            {
                Tase2Utility::log_error (
                    "Invalid datapoint type label: %s, type: %s",
                    label.c_str (), type.c_str ());
                continue;
            }

            size_t colonPos = protocolRef.find (':');

            if (colonPos != std::string::npos)
            {
                Tase2Utility::log_debug ("Add dp to Exchange Def %s",
                                         protocolRef.c_str ());

                auto def = std::make_shared<DataExchangeDefinition> ();
                def->ref = protocolRef;
                def->label = label;
                def->type = getDpTypeFromString (type);

                m_exchangeDefinitions[label] = def;
                m_exchangeDefinitionsRef[protocolRef] = def;
                if (def->type < COMMAND)
                {
                    m_polledDatapoints[protocolRef] = def;
                }
            }
            else
            {
                Tase2Utility::log_warn ("Invalid ref %s",
                                        protocolRef.c_str ());
                continue;
            }
        }
    }
}

void
TASE2ClientConfig::importTlsConfig (const std::string& tlsConfig)
{
    Document document;

    if (document.Parse (const_cast<char*> (tlsConfig.c_str ()))
            .HasParseError ())
    {
        Tase2Utility::log_fatal ("Parsing error in TLS configuration");

        return;
    }

    if (!document.IsObject ())
        return;

    if (!document.HasMember ("tls_conf") || !document["tls_conf"].IsObject ())
    {
        return;
    }

    const Value& tlsConf = document["tls_conf"];

    if (tlsConf.HasMember ("private_key")
        && tlsConf["private_key"].IsString ())
    {
        m_privateKey = tlsConf["private_key"].GetString ();
    }

    if (tlsConf.HasMember ("own_cert") && tlsConf["own_cert"].IsString ())
    {
        m_ownCertificate = tlsConf["own_cert"].GetString ();
    }

    if (tlsConf.HasMember ("ca_certs") && tlsConf["ca_certs"].IsArray ())
    {

        const Value& caCerts = tlsConf["ca_certs"];

        for (const Value& caCert : caCerts.GetArray ())
        {
            if (caCert.HasMember ("cert_file"))
            {
                if (caCert["cert_file"].IsString ())
                {
                    std::string certFileName
                        = caCert["cert_file"].GetString ();

                    m_caCertificates.push_back (certFileName);
                }
            }
        }
    }

    if (tlsConf.HasMember ("remote_certs")
        && tlsConf["remote_certs"].IsArray ())
    {

        const Value& remoteCerts = tlsConf["remote_certs"];

        for (const Value& remoteCert : remoteCerts.GetArray ())
        {
            if (remoteCert.HasMember ("cert_file"))
            {
                if (remoteCert["cert_file"].IsString ())
                {
                    std::string certFileName
                        = remoteCert["cert_file"].GetString ();

                    m_remoteCertificates.push_back (certFileName);
                }
            }
        }
    }
}

std::shared_ptr<DataExchangeDefinition>
TASE2ClientConfig::getExchangeDefinitionByRef (const std::string& ref)
{
    auto it = m_exchangeDefinitionsRef.find (ref);
    if (it != m_exchangeDefinitionsRef.end ())
    {
        return it->second;
    }
    return nullptr;
}
