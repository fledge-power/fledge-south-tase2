#include "tase2_client_connection.hpp"
#include "tase2_client_config.hpp"
#include <algorithm>
#include <libtase2/hal_thread.h>
#include <libtase2/tase2_client.h>
#include <map>
#include <string>
#include <tase2.hpp>
#include <utils.h>
#include <vector>

TASE2ClientConnection::TASE2ClientConnection (TASE2Client* client,
                                              TASE2ClientConfig* config,
                                              const std::string& ip,
                                              const int tcpPort, bool tls,
                                              OsiParameters* osiParameters)
    : m_client (client), m_config (config), m_osiParameters (osiParameters),
      m_tcpPort (tcpPort), m_serverIp (ip), m_useTls (tls)
{
}

TASE2ClientConnection::~TASE2ClientConnection () { Stop (); }

static uint64_t
GetCurrentTimeInMs ()
{
    struct timeval now;

    gettimeofday (&now, nullptr);

    return ((uint64_t)now.tv_sec * 1000LL) + (now.tv_usec / 1000);
}

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

void
TASE2ClientConnection::m_setOsiConnectionParameters ()
{
    const OsiParameters& osiParams = *m_osiParameters;

    // set Remote 'AP Title' and 'AE Qualifier'
    if (!osiParams.remoteApTitle.empty ())
    {
        Tase2Utility::log_debug ("Set remote ap title to %s:%d",
                                 osiParams.remoteApTitle.c_str (),
                                 osiParams.remoteAeQualifier);
        Tase2_Endpoint_setRemoteApTitle (m_endpoint,
                                         osiParams.remoteApTitle.c_str (),
                                         osiParams.remoteAeQualifier);
    }

    // set Local 'AP Title' and 'AE Qualifier'
    if (!osiParams.localApTitle.empty ())
    {
        Tase2Utility::log_debug ("Set local ap title to %s:%d",
                                 osiParams.localApTitle.c_str (),
                                 osiParams.localAeQualifier);
        Tase2_Endpoint_setLocalApTitle (m_endpoint,
                                        osiParams.localApTitle.c_str (),
                                        osiParams.localAeQualifier);
    }

    /* change parameters for presentation, session and transport layers */
    Tase2_Endpoint_setRemoteAddresses (m_endpoint, osiParams.remotePSelector,
                                       osiParams.remoteSSelector,
                                       osiParams.localTSelector);
    Tase2_Endpoint_setLocalAddresses (m_endpoint, osiParams.localPSelector,
                                      osiParams.localSSelector,
                                      osiParams.remoteTSelector);
}

void
TASE2ClientConnection::m_configDatasets ()
{
    for (const auto& pair : m_config->getDatasets ())
    {
        Tase2_ClientError error;
        std::shared_ptr<Dataset> dataset = pair.second;

        if (dataset->dynamic)
        {
            Tase2Utility::log_debug ("Create new dataset %s",
                                     dataset->datasetRef.c_str ());
            LinkedList newDataSetEntries = LinkedList_create ();

            if (newDataSetEntries == nullptr)
            {
                continue;
            }

            for (const auto& entry : dataset->entries)
            {
                Tase2Utility::log_debug (
                    "Add datapoint %s to dataset %s:%s", entry.c_str (),
                    dataset->domain.c_str (), dataset->datasetRef.c_str ());
                char* strCopy
                    = static_cast<char*> (malloc (entry.length () + 1));
                if (strCopy != nullptr)
                {
                    std::strcpy (strCopy, entry.c_str ());
                    LinkedList_add (newDataSetEntries,
                                    static_cast<void*> (strCopy));
                }
            }

            Tase2_Client_createDataSet (
                m_tase2client, &error, dataset->domain.c_str (),
                dataset->datasetRef.c_str (), newDataSetEntries);

            if (error != TASE2_CLIENT_ERROR_OK)
            {
                Tase2Utility::log_error (
                    "Error in dataset creation (Dataset "
                    "Name : %s, Domain : %s, Error Code : %d",
                    dataset->datasetRef.c_str (), dataset->domain.c_str (),
                    error);
            }

            LinkedList_destroyDeep (newDataSetEntries, free);
        }
    }
}

/* callback handler that is called twice for each received transfer set report
 */
void
TASE2ClientConnection::dsTransferSetReportHandler (
    void* parameter, bool finished, uint32_t seq,
    Tase2_ClientDSTransferSet transferSet)
{
    if (finished)
    {
        Tase2Utility::log_debug ("--> (%i) report processing finished", seq);
    }
    else
    {
        Tase2Utility::log_debug ("New report received with seq no: %u", seq);
    }
}

/* callback handler that is called for each data point of a received transfer
 * set report */
void
TASE2ClientConnection::dsTransferSetValueHandler (
    void* parameter, Tase2_ClientDSTransferSet transferSet,
    const char* domainName, const char* pointName, Tase2_PointValue pointValue)
{
    Tase2Utility::log_debug ("  Received value for %s:%s", domainName,
                             pointName);

    auto connection = (TASE2ClientConnection*)parameter;

    connection->m_client->handleValue (
        std::string (domainName) + ":" + std::string (pointName), pointValue,
        GetCurrentTimeInMs (), false);
}

void
TASE2ClientConnection::m_configDsts ()
{
    for (const auto& pair : m_config->getDsTranferSets ())
    {
        std::shared_ptr<DatasetTransferSet> dsts = pair.second;
        Tase2_ClientError err;
        Tase2_ClientDSTransferSet ts
            = Tase2_Client_getNextDSTransferSet (m_tase2client, "icc1", &err);

        if (ts)
        {
            Tase2_ClientDataSet dataSet = Tase2_Client_getDataSet (
                m_tase2client, &err, dsts->domain.c_str (),
                dsts->datasetRef.c_str ());

            if (!dataSet)
            {
                Tase2Utility::log_error ("Could not find dataset %s:%s",
                                         dsts->domain.c_str (),
                                         dsts->datasetRef.c_str ());
                continue;
            }

            m_datasets.push_back (dataSet);

            Tase2_ClientDSTransferSet_setDataSet (ts, dataSet);

            Tase2Utility::log_debug ("DSTransferSet %s:%s",
                                     Tase2_ClientDSTransferSet_getDomain (ts),
                                     Tase2_ClientDSTransferSet_getName (ts));

            Tase2_ClientDSTransferSet_readValues (ts, m_tase2client);

            Tase2Utility::log_debug (
                "  data-set: %s:%s",
                Tase2_ClientDSTransferSet_getDataSetDomain (ts),
                Tase2_ClientDSTransferSet_getDataSetName (ts));

            Tase2_ClientDSTransferSet_setDataSetName (
                ts, dsts->domain.c_str (), dsts->datasetRef.c_str ());

            Tase2_ClientDSTransferSet_setInterval (ts, dsts->interval);

            Tase2_ClientDSTransferSet_setRBE (ts, dsts->rbe);

            Tase2_ClientDSTransferSet_setCritical (ts, dsts->critical);

            Tase2_ClientDSTransferSet_setBufferTime (ts, dsts->bufferTime);

            Tase2_ClientDSTransferSet_setIntegrityCheck (ts, dsts->rbe);

            Tase2_ClientDSTransferSet_setStartTime (ts, dsts->startTime);

            Tase2_ClientDSTransferSet_setAllChangesReported (
                ts, dsts->allChangesReported);

            Tase2_ClientDSTransferSet_setDSConditionsRequested (
                ts, dsts->dsConditions);

            Tase2Utility::log_debug ("Start DSTransferSet %s",
                                     dsts->dstsRef.c_str ());

            Tase2_ClientDSTransferSet_setStatus (ts, true);

            if (Tase2_ClientDSTransferSet_writeValues (ts, m_tase2client)
                != TASE2_CLIENT_ERROR_OK)
            {
                Tase2Utility::log_error ("Failed to write dsTs values");
                Tase2_ClientDSTransferSet_destroy(ts);
            }
            else{
                m_dsts.push_back (ts);
            }
        }
        else
            Tase2Utility::log_error ("GetNextDSTransferSet operation failed!");
    }
}

void
TASE2ClientConnection::executePeriodicTasks ()
{
    uint64_t currentTime = getMonotonicTimeInMs ();
    if (m_config->getPollingInterval () > 0
        && currentTime >= m_nextPollingTime)
    {
        m_client->handleAllValues ();
        m_nextPollingTime = currentTime + m_config->getPollingInterval ();
    }
}

void TASE2ClientConnection::_conThread()
{
    try
    {
        while (m_started)
        {
            if (m_connect)
            {
                processConnectionState();
            }
            Thread_sleep(50); 
        }
        std::lock_guard<std::mutex> lock(m_conLock);
        cleanUp();
    }
    catch (const std::exception& e)
    {
        Tase2Utility::log_error("Exception caught in _conThread: %s", e.what());
    }
}

void TASE2ClientConnection::processConnectionState()
{
    Tase2_Endpoint_State newState;

    switch (m_connectionState)
    {
    case CON_STATE_IDLE:
        if (prepareConnection () && m_tase2client != nullptr)
        {
            std::lock_guard<std::mutex> lock(m_conLock);
            Tase2_Endpoint_connect (m_endpoint);

            Tase2Utility::log_info ("Connecting to %s:%d",
                                    m_serverIp.c_str (),
                                    m_tcpPort);

            m_connectionState = CON_STATE_CONNECTING;
            m_connecting = true;
            m_delayExpirationTime
                = getMonotonicTimeInMs () + 10000;
        }
        else
        {
            Tase2Utility::log_error("Fatal configuration error");
            m_connectionState = CON_STATE_FATAL_ERROR;
        }
        break;

    case CON_STATE_CONNECTING:
        newState = Tase2_Endpoint_getState(m_endpoint);
        if (newState == TASE2_ENDPOINT_STATE_CONNECTED)
        {
            std::lock_guard<std::mutex> lock(m_conLock);
            m_connectionState = CON_STATE_CONNECTED;
            m_connected = true;
            m_connecting = false;
            postConnectionSetup(); 
        }
        else if (getMonotonicTimeInMs() > m_delayExpirationTime)
        {
            std::lock_guard<std::mutex> lock(m_conLock);
            Tase2Utility::log_warn("Timeout while connecting %d", m_tcpPort);
            Disconnect();
        }
        break;

    case CON_STATE_CONNECTED:
      {
        newState = Tase2_Endpoint_getState(m_endpoint);
        std::lock_guard<std::mutex> lock(m_conLock);
        if (newState != TASE2_ENDPOINT_STATE_CONNECTED)
        {
            Tase2Utility::log_warn("Lost connection to %s:%d", m_serverIp.c_str(), m_tcpPort);
            Disconnect();
        }
        else
        {
            executePeriodicTasks();
        }
        break;
      }

    case CON_STATE_FATAL_ERROR:
        break;
    }
}

void TASE2ClientConnection::postConnectionSetup()
{
    m_configDatasets();
    m_configDsts();
    Tase2_Client_installDSTransferSetReportHandler(m_tase2client, dsTransferSetReportHandler, nullptr);
    Tase2_Client_installDSTransferSetValueHandler(m_tase2client, dsTransferSetValueHandler, this);
    Tase2Utility::log_info("Connected to %s:%d", m_serverIp.c_str(), m_tcpPort);
}
void
TASE2ClientConnection::Start ()
{
    if (!m_started)
    {
        Tase2Utility::log_debug("Starting connection %s:%d", m_serverIp.c_str(), m_tcpPort);
        m_started = true;

        m_conThread
            = new std::thread (&TASE2ClientConnection::_conThread, this);
    }
}

void
TASE2ClientConnection::cleanUp ()
{
    if (!m_dsts.empty ())
    {
        for (const auto& entry : m_dsts)
        {
            Tase2_ClientDSTransferSet_destroy (entry);
        }
    }
    if (!m_datasets.empty ())
    {
        for (const auto& entry : m_datasets)
        {
            Tase2_ClientDataSet_destroy (entry);
        }
    }
    if (!m_connDataSetDirectoryPairs.empty ())
    {
        for (const auto& entry : m_connDataSetDirectoryPairs)
        {
            LinkedList dataSetDirectory = entry->second;
            LinkedList_destroy (dataSetDirectory);
            delete entry;
        }
        m_connDataSetDirectoryPairs.clear ();
    }

    Tase2_ClientError err;

    if (m_endpoint)
    {
        if (m_tase2client)
        {
            Tase2_Client_disconnect (m_tase2client);
            Tase2_Client_destroy (m_tase2client);
        }
        Tase2_Endpoint_destroy (m_endpoint);
        m_endpoint = nullptr;
        m_tase2client = nullptr;
    }

    if (m_tlsConfig != nullptr)
    {
        TLSConfiguration_destroy (m_tlsConfig);
    }
    m_tlsConfig = nullptr;
}

void
TASE2ClientConnection::Stop ()
{
    if (!m_started)
        return;

    {
        std::lock_guard<std::mutex> lock (m_conLock);
        m_started = false;
    }
    if (m_conThread)
    {
        m_conThread->join ();
        delete m_conThread;
        m_conThread = nullptr;
    }
}

bool
TASE2ClientConnection::prepareConnection ()
{
    if (m_endpoint != nullptr)
    {
        Tase2_Endpoint_destroy (m_endpoint);
        m_endpoint = nullptr;
    }

    Tase2Utility::log_debug("Preparing connection %s:%d", m_serverIp.c_str(), m_tcpPort);
    

    if (UseTLS ())
    {
        TLSConfiguration tlsConfig = TLSConfiguration_create ();

        bool tlsConfigOk = true;

        std::string certificateStore
            = getDataDir () + std::string ("/etc/certs/");
        std::string certificateStorePem
            = getDataDir () + std::string ("/etc/certs/pem/");

        if (m_config->GetOwnCertificate ().length () == 0
            || m_config->GetPrivateKey ().length () == 0)
        {
            Tase2Utility::log_error (
                "No private key and/or certificate configured for client");
            tlsConfigOk = false;
        }
        else
        {
            std::string privateKeyFile
                = certificateStore + m_config->GetPrivateKey ();

            if (access (privateKeyFile.c_str (), R_OK) == 0)
            {
                if (TLSConfiguration_setOwnKeyFromFile (
                        tlsConfig, privateKeyFile.c_str (), nullptr)
                    == false)
                {
                    Tase2Utility::log_error (
                        "Failed to load private key file: %s",
                        privateKeyFile.c_str ());
                    tlsConfigOk = false;
                }
            }
            else
            {
                Tase2Utility::log_error (
                    "Failed to access private key file: %s",
                    privateKeyFile.c_str ());
                tlsConfigOk = false;
            }

            std::string clientCert = m_config->GetOwnCertificate ();
            bool isPemClientCertificate
                = clientCert.rfind (".pem") == clientCert.size () - 4;

            std::string clientCertFile;

            if (isPemClientCertificate)
                clientCertFile = certificateStorePem + clientCert;
            else
                clientCertFile = certificateStore + clientCert;

            if (access (clientCertFile.c_str (), R_OK) == 0)
            {
                if (TLSConfiguration_setOwnCertificateFromFile (
                        tlsConfig, clientCertFile.c_str ())
                    == false)
                {
                    Tase2Utility::log_error (
                        "Failed to load client certificate file: %s",
                        clientCertFile.c_str ());
                    tlsConfigOk = false;
                }
            }
            else
            {
                Tase2Utility::log_error (
                    "Failed to access client certificate file: %s",
                    clientCertFile.c_str ());
                tlsConfigOk = false;
            }
        }

        if (!m_config->GetRemoteCertificates ().empty ())
        {
            TLSConfiguration_setAllowOnlyKnownCertificates (tlsConfig, true);

            for (const std::string& remoteCert :
                 m_config->GetRemoteCertificates ())
            {
                bool isPemRemoteCertificate
                    = remoteCert.rfind (".pem") == remoteCert.size () - 4;

                std::string remoteCertFile;

                if (isPemRemoteCertificate)
                    remoteCertFile = certificateStorePem + remoteCert;
                else
                    remoteCertFile = certificateStore + remoteCert;

                if (access (remoteCertFile.c_str (), R_OK) == 0)
                {
                    if (TLSConfiguration_addAllowedCertificateFromFile (
                            tlsConfig, remoteCertFile.c_str ())
                        == false)
                    {
                        Tase2Utility::log_warn (
                            "Failed to load remote certificate file: %s -> "
                            "ignore certificate",
                            remoteCertFile.c_str ());
                    }
                }
                else
                {
                    Tase2Utility::log_warn (
                        "Failed to access remote certificate file: %s -> "
                        "ignore certificate",
                        remoteCertFile.c_str ());
                }
            }
        }
        else
        {
            TLSConfiguration_setAllowOnlyKnownCertificates (tlsConfig, false);
        }

        if (m_config->GetCaCertificates ().size () > 0)
        {
            TLSConfiguration_setChainValidation (tlsConfig, true);

            for (const std::string& caCert : m_config->GetCaCertificates ())
            {
                bool isPemCaCertificate
                    = caCert.rfind (".pem") == caCert.size () - 4;

                std::string caCertFile;

                if (isPemCaCertificate)
                    caCertFile = certificateStorePem + caCert;
                else
                    caCertFile = certificateStore + caCert;

                if (access (caCertFile.c_str (), R_OK) == 0)
                {
                    if (TLSConfiguration_addCACertificateFromFile (
                            tlsConfig, caCertFile.c_str ())
                        == false)
                    {
                        Tase2Utility::log_warn (
                            "Failed to load CA certificate file: %s -> ignore "
                            "certificate",
                            caCertFile.c_str ());
                    }
                }
                else
                {
                    Tase2Utility::log_warn (
                        "Failed to access CA certificate file: %s -> ignore "
                        "certificate",
                        caCertFile.c_str ());
                }
            }
        }
        else
        {
            TLSConfiguration_setChainValidation (tlsConfig, false);
        }

        if (tlsConfigOk)
        {

            TLSConfiguration_setRenegotiationTime (tlsConfig, 60000);

            m_endpoint = Tase2_Endpoint_create (tlsConfig, m_passive);

            if (m_endpoint)
            {
                m_tlsConfig = tlsConfig;
            }
            else
            {
                Tase2Utility::log_error ("TLS configuration failed");
                TLSConfiguration_destroy (tlsConfig);
            }
        }
        else
        {
            Tase2Utility::log_error ("TLS configuration failed");
        }
    }
    else
    {
        m_endpoint = Tase2_Endpoint_create (nullptr, m_passive);
    }

    if (m_endpoint == nullptr)
    {
        return false;
    }

    if (!m_passive)
    {
        Tase2_Endpoint_setRemoteIpAddress (m_endpoint, m_serverIp.c_str ());

        Tase2_Endpoint_setRemoteTcpPort (m_endpoint, m_tcpPort);
    }

    if (m_passive)
    {
        Tase2_Endpoint_setLocalTcpPort (m_endpoint, m_tcpPort);
    }

    m_setOsiConnectionParameters ();

    m_tase2client = Tase2_Client_createEx (m_endpoint);

    return m_tase2client != nullptr;
}

void TASE2ClientConnection::Connect()
{
    {
        std::lock_guard<std::mutex> lock(m_conLock);
        m_connect = true; 
    }
}

void TASE2ClientConnection::Disconnect()
{        
    if (!m_started)  
        return;

    m_connecting = false;
    m_connected = false;
    m_connect = false;
    m_connectionState = CON_STATE_IDLE;

    cleanUp();
}


Tase2_PointValue
TASE2ClientConnection::readValue (Tase2_ClientError* error, const char* domain,
                                  const char* name)
{
    Tase2_PointValue value
        = Tase2_Client_readPointValue (m_tase2client, error, domain, name);
    return value;
}

bool
TASE2ClientConnection::sendCommand (std::string domain, std::string name,
                                    int value, bool select, long time)
{
    Tase2_ClientError err;
    if (select)
    {
        return Tase2_Client_selectDevice (m_tase2client, &err, domain.c_str (),
                                          name.c_str ())
               != 0;
    }
    return Tase2_Client_sendCommand (m_tase2client, &err, domain.c_str (),
                                     name.c_str (), value);
}
bool
TASE2ClientConnection::sendSetPointReal (std::string domain, std::string name,
                                         float value, bool select, long time)
{
    Tase2_ClientError err;
    if (select)
    {
        return Tase2_Client_selectDevice (m_tase2client, &err, domain.c_str (),
                                          name.c_str ())
               != 0;
    }
    return Tase2_Client_sendRealSetPoint (m_tase2client, &err, domain.c_str (),
                                          name.c_str (), value);
}
bool
TASE2ClientConnection::sendSetPointDiscrete (std::string domain,
                                             std::string name, int value,
                                             bool select, long time)
{
    Tase2_ClientError err;
    if (select)
    {
        return Tase2_Client_selectDevice (m_tase2client, &err, domain.c_str (),
                                          name.c_str ())
               != 0;
    }
    return Tase2_Client_sendDiscreteSetPoint (
        m_tase2client, &err, domain.c_str (), name.c_str (), value);
}
