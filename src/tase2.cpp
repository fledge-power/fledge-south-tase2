#include "tase2.hpp"

TASE2::~TASE2 ()
{
    stop ();
    delete m_config;
}

void
TASE2::setJsonConfig (const std::string& stack_configuration,
                      const std::string& msg_configuration,
                      const std::string& tls_configuration)
{
    delete m_config;

    m_config = new TASE2ClientConfig ();

    m_config->importProtocolConfig (stack_configuration);
    m_config->importExchangeConfig (msg_configuration);
    m_config->importTlsConfig (tls_configuration);
}

void
TASE2::start ()
{
    Tase2Utility::log_info ("Starting iec61850");
    // LCOV_EXCL_START
    switch (m_config->LogLevel ())
    {
    case 1:
        Logger::getLogger ()->setMinLevel ("debug");
        break;
    case 2:
        Logger::getLogger ()->setMinLevel ("info");
        break;
    case 3:
        Logger::getLogger ()->setMinLevel ("warning");
        break;
    default:
        Logger::getLogger ()->setMinLevel ("error");
        break;
    }
    // LCOV_EXCL_STOP

    m_client = new TASE2Client (this, m_config);

    // LCOV_EXCL_START
    if (!m_client)
    {
        Tase2Utility::log_error ("Can't start, client is null");
        return;
    }
    // LCOV_EXCL_STOP

    m_client->start ();
}

void
TASE2::stop ()
{
    if (!m_client)
        return;

    m_client->stop ();

    delete m_client;
    m_client = nullptr;
}

void
TASE2::ingest (const std::string& assetName,
               const std::vector<Datapoint*>& points)
{
    if (m_ingest)
    {
        m_ingest (m_data, Reading (assetName, points));
    }
}

void
TASE2::registerIngest (void* data, void (*cb) (void*, Reading))
{
    m_ingest = cb;
    m_data = data;
}

enum CommandParameters
{
    TYPE,
    SCOPE,
    DOMAIN,
    NAME,
    VALUE,
    SELECT,
    TS
};

bool
TASE2::m_CommandOperation (int count, PLUGIN_PARAMETER** params)
{
    if (count > 6)
    {
        // common address of the asdu
        std::string domain = params[DOMAIN]->value;

        // information object address
        std::string name = params[NAME]->value;

        // command state to send, must be a boolean
        // 0 = off, 1 otherwise
        int value = atoi (params[VALUE]->value.c_str ());

        // select or execute, must be a boolean
        // 0 = execute, otherwise = select
        bool select
            = static_cast<bool> (atoi (params[SELECT]->value.c_str ()));

        long time = 0;

        time = stol (params[TS]->value);

        Tase2Utility::log_debug ("operate: command - Domain: %s Name: "
                                 "%s value: %i select: %i timestamp: %i",
                                 domain.c_str (), name.c_str (), value, select,
                                 time);

        return m_client->sendCommand (domain, name, value, select, time);
    }
    else
    {
        Tase2Utility::log_error ("operation parameter missing");
        return false;
    }
}
bool
TASE2::m_SetPointRealOperation (int count, PLUGIN_PARAMETER** params)
{
    if (count > 6)
    {
        // common address of the asdu
        std::string domain = params[DOMAIN]->value;

        // information object address
        std::string name = params[NAME]->value;

        // command state to send, must be a boolean
        // 0 = off, 1 otherwise
        float value = atof (params[VALUE]->value.c_str ());

        // select or execute, must be a boolean
        // 0 = execute, otherwise = select
        bool select
            = static_cast<bool> (atoi (params[SELECT]->value.c_str ()));

        long time = 0;

        time = stol (params[TS]->value);

        Tase2Utility::log_debug ("operate: setpoint real - Domain: %s Name: "
                                 "%s value: %f select: %i timestamp: %i",
                                 domain.c_str (), name.c_str (), value, select,
                                 time);

        return m_client->sendSetPointReal (domain, name, value, select, time);
    }
    else
    {
        Tase2Utility::log_error ("operation parameter missing");
        return false;
    }
}
bool
TASE2::m_SetPointDiscreteOperation (int count, PLUGIN_PARAMETER** params)
{
    if (count > 6)
    {
        // common address of the asdu
        std::string domain = params[DOMAIN]->value;

        // information object address
        std::string name = params[NAME]->value;

        // command state to send, must be a boolean
        // 0 = off, 1 otherwise
        int value = atoi (params[VALUE]->value.c_str ());

        // select or execute, must be a boolean
        // 0 = execute, otherwise = select
        bool select
            = static_cast<bool> (atoi (params[SELECT]->value.c_str ()));

        long time = 0;

        time = stol (params[TS]->value);

        Tase2Utility::log_debug (
            "operate: setpoint discrete - Domain: %s Name: "
            "%s value: %i select: %i timestamp: %i",
            domain.c_str (), name.c_str (), value, select, time);

        return m_client->sendSetPointDiscrete (domain, name, value, select,
                                               time);
    }
    else
    {
        Tase2Utility::log_error ("operation parameter missing");
        return false;
    }
}

bool
TASE2::operation (const std::string& operation, int count,
                  PLUGIN_PARAMETER** params)
{
    if (m_client == nullptr)
    {
        Tase2Utility::log_error (
            "operation called but plugin is not yet initialized");
        return false;
    }
    if (operation == "TASE2Command")
    {
        std::string type = params[0]->value;

        if (type[0] == '"')
        {
            type = type.substr (1, type.length () - 2);
        }

        int typeID = TASE2ClientConfig::getDpTypeFromString (type);

        switch (typeID)
        {
        case COMMAND:
            return m_CommandOperation (count, params);
        case SETPOINTREAL:
            return m_SetPointRealOperation (count, params);
        case SETPOINTDISCRETE:
            return m_SetPointDiscreteOperation (count, params);
        default:
            Tase2Utility::log_error ("Unrecognised command type %s",
                                     type.c_str ());
            return false;
        }
    }

    Tase2Utility::log_error ("Unrecognised operation %s", operation.c_str ());

    return false;
}
