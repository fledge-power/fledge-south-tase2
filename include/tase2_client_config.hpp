#ifndef TASE2_CLIENT_CONFIG_H
#define TASE2_CLIENT_CONFIG_H

#include "libtase2/tase2_client.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "tase2_utility.hpp"
#include <gtest/gtest.h>
#include <logger.h>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#define FRIEND_TESTS                                                          \
    FRIEND_TEST (ConnectionHandlingTest, SingleConnection);                   \
    FRIEND_TEST (ConnectionHandlingTest, SingleConnectionTLS);                \
    FRIEND_TEST (ConnectionHandlingTest, SingleConnectionReconnect);          \
    FRIEND_TEST (ConnectionHandlingTest, TwoConnectionsBackup);               \
    FRIEND_TEST (SpontDataTest, PollingAllType);                              \
    FRIEND_TEST (ControlTest, operateDirect);                                 \
    FRIEND_TEST (ReportingTest, ReportingAllType);                            \
    FRIEND_TEST (ReportingTest, ReportingAllTypeDynamicDataset);              \
    FRIEND_TEST (ControlTest, operateSelect);                                 \
    FRIEND_TEST (ConnectionHandlingTest, SingleConnectionTryBeforeServerStarts);

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
    Tase2_TSelector localTSelector;
    Tase2_TSelector remoteTSelector;
    Tase2_SSelector localSSelector;
    Tase2_SSelector remoteSSelector;
    Tase2_PSelector localPSelector;
    Tase2_PSelector remotePSelector;
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
    DPTYPE type;
    std::string label;
};

struct DatasetTransferSet
{
    std::string domain;
    std::string dstsRef;
    std::string datasetRef;
    int dsConditions;
    int startTime;
    int interval;
    int tle;
    int bufferTime;
    int integrityCheck;
    bool critical;
    bool rbe;
    bool allChangesReported;
};

struct Dataset
{
    std::string domain;
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

    static DPTYPE getDpTypeFromString (const std::string& type);

    void importProtocolConfig (const std::string& protocolConfig);
    void importJsonConnectionOsiConfig (const rapidjson::Value& connOsiConfig,
                                        RedGroup& iedConnectionParam);
    void
    importJsonConnectionOsiSelectors (const rapidjson::Value& connOsiConfig,
                                      OsiParameters* osiParams);
    OsiSelectorSize parseOsiPSelector (std::string& inputOsiSelector,
                                       Tase2_PSelector* pselector);
    OsiSelectorSize parseOsiSSelector (std::string& inputOsiSelector,
                                       Tase2_SSelector* sselector);
    OsiSelectorSize parseOsiTSelector (std::string& inputOsiSelector,
                                       Tase2_TSelector* tselector);
    OsiSelectorSize parseOsiSelector (std::string& inputOsiSelector,
                                      uint8_t* selectorValue,
                                      const uint8_t selectorSize);
    void importExchangeConfig (const std::string& exchangeConfig);
    void importTlsConfig (const std::string& tlsConfig);

    static std::pair<std::string, std::string>
    splitExchangeRef (std::string ref);

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
