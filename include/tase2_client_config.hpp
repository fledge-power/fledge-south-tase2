#ifndef TASE2_CLIENT_CONFIG_H
#define TASE2_CLIENT_CONFIG_H

#include "tase2_utility.hpp"
#include "libtase2/tase2_client.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include <gtest/gtest.h>
#include <logger.h>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#define FRIEND_TESTS                                                          \
    FRIEND_TEST (ConnectionHandlingTest, SingleConnection);                   \
    FRIEND_TEST (ConnectionHandlingTest, SingleConnectionTLS);                \
    FRIEND_TEST (ConnectionHandlingTest, SingleConnectionReconnect);          


typedef enum
{
    REAL,
    REALQ,
    REALQTIME,
    REALQTIMEEXT,
    STATE,
    STATEQ,
    STATEQTIME,
    STATEQTIMEEXT,
    DISCRETE,
    DISCRETEQ,
    DISCRETEQTIME,
    DISCRETEQTIMEEXT,
    STATESUP,
    STATESUPQ,
    STATESUPQTIME,
    STATESUPQTIMEEXT,
    COMMAND,
    SETPOINTREAL,
    SETPOINTDISCRETE,
    DP_TYPE_UNKNOWN = -1
} DPTYPE;


class ConfigurationException : public std::logic_error
{
  public:
    explicit ConfigurationException (std::string const& msg)
        : std::logic_error ("Configuration exception: " + msg)
    {
    }
};

using OsiSelectorSize = uint8_t;
struct OsiParameters
{
    std::string localApTitle{ "" };
    int localAeQualifier{ 0 };
    std::string remoteApTitle{ "" };
    int remoteAeQualifier{ 0 };
    TSelector localTSelector;
    TSelector remoteTSelector;
    SSelector localSSelector;
    SSelector remoteSSelector;
    PSelector localPSelector;
    PSelector remotePSelector;
};

struct RedGroup
{
    std::string ipAddr;
    int tcpPort;
    OsiParameters osiParameters;
    bool isOsiParametersEnabled;
    bool tls;
};

struct DataExchangeDefinition
{
    std::string ref;
    DPTYPE cdcType;
    std::string label;
    std::string id;
};

struct DatasetTransferSet
{
    std::string dstsRef;
    std::string datasetRef;
    int trgops;
};

struct Dataset
{
    std::string datasetRef;
    std::vector<std::string> entries;
    bool dynamic;
};

class TASE2ClientConfig
{
  public:
    TASE2ClientConfig () { m_exchangeDefinitions.clear (); };
    ~TASE2ClientConfig ();

    int
    LogLevel () const
    {
        return 1;
    };

    void importProtocolConfig (const std::string& protocolConfig);
    void importJsonConnectionOsiConfig (const rapidjson::Value& connOsiConfig,
                                        RedGroup& iedConnectionParam);
    void
    importJsonConnectionOsiSelectors (const rapidjson::Value& connOsiConfig,
                                      OsiParameters* osiParams);
    OsiSelectorSize parseOsiPSelector (std::string& inputOsiSelector,
                                       PSelector* pselector);
    OsiSelectorSize parseOsiSSelector (std::string& inputOsiSelector,
                                       SSelector* sselector);
    OsiSelectorSize parseOsiTSelector (std::string& inputOsiSelector,
                                       TSelector* tselector);
    OsiSelectorSize parseOsiSelector (std::string& inputOsiSelector,
                                      uint8_t* selectorValue,
                                      const uint8_t selectorSize);
    void importExchangeConfig (const std::string& exchangeConfig);
    void importTlsConfig (const std::string& tlsConfig);

    std::vector<std::shared_ptr<RedGroup> >&
    GetConnections ()
    {
        return m_connections;
    };
    std::string&
    GetPrivateKey ()
    {
        return m_privateKey;
    };
    std::string&
    GetOwnCertificate ()
    {
        return m_ownCertificate;
    };
    std::vector<std::string>&
    GetRemoteCertificates ()
    {
        return m_remoteCertificates;
    };
    std::vector<std::string>&
    GetCaCertificates ()
    {
        return m_caCertificates;
    };

    static bool isValidIPAddress (const std::string& addrStr);

    static int getCdcTypeFromString (const std::string& cdc);

    std::unordered_map<std::string, std::shared_ptr<DataExchangeDefinition> >&
    ExchangeDefinition ()
    {
        return m_exchangeDefinitions;
    };

    static int GetTypeIdByName (const std::string& name);

    std::string* checkExchangeDataLayer (int typeId, std::string& objRef);

    std::shared_ptr<DataExchangeDefinition>
    getExchangeDefinitionByLabel (const std::string& label);
    std::shared_ptr<DataExchangeDefinition>
    getExchangeDefinitionByRef (const std::string& objRef);

    const std::unordered_map<std::string,
                             std::shared_ptr<DatasetTransferSet> >&
    getDsTranferSets () const
    {
        return m_dsTranferSets;
    };
    const std::unordered_map<std::string, std::shared_ptr<Dataset> >&
    getDatasets () const
    {
        return m_datasets;
    };
    const std::unordered_map<std::string,
                             std::shared_ptr<DataExchangeDefinition> >&
    polledDatapoints () const
    {
        return m_polledDatapoints;
    };

    long
    getPollingInterval () const
    {
        return pollingInterval;
    }

    uint64_t
    backupConnectionTimeout ()
    {
        return m_backupConnectionTimeout;
    };

  private:
    static bool isMessageTypeMatching (int expectedType, int rcvdType);

    std::vector<std::shared_ptr<RedGroup> > m_connections;

    void deleteExchangeDefinitions ();

    std::unordered_map<std::string, std::shared_ptr<DataExchangeDefinition> >
        m_polledDatapoints;
    std::unordered_map<std::string, std::shared_ptr<Dataset> > m_datasets;
    std::unordered_map<std::string, std::shared_ptr<DataExchangeDefinition> >
        m_exchangeDefinitions;
    std::unordered_map<std::string, std::shared_ptr<DataExchangeDefinition> >
        m_exchangeDefinitionsRef;

    std::unordered_map<std::string, std::shared_ptr<DatasetTransferSet> >
        m_dsTranferSets;

    bool m_protocolConfigComplete = false;
    bool m_exchangeConfigComplete = false;

    std::string m_privateKey = "";
    std::string m_ownCertificate = "";
    std::vector<std::string> m_remoteCertificates;
    std::vector<std::string> m_caCertificates;

    uint64_t m_backupConnectionTimeout = 5000;

    long pollingInterval = 0;

    FRIEND_TESTS
};

#endif /* TASE2_CLIENT_CONFIG_H */
