#include "tase2.hpp"
#include <config_category.h>
#include <logger.h>
#include <plugin_api.h>
#include <version.h>

#include <fstream>
#include <iostream>
#include <string>

using namespace std;

typedef void (*INGEST_CB) (void*, Reading);

#define PLUGIN_NAME "tase2"

/**
 * Default configuration
 */
static const char* default_config = QUOTE ({
    "plugin" : {
        "description" : "tase2 south plugin",
        "type" : "string",
        "default" : PLUGIN_NAME,
        "readonly" : "true"
    },
    "asset" : {
        "description" : "Asset name",
        "type" : "string",
        "default" : "tase2",
        "displayName" : "Asset Name",
        "order" : "1",
        "mandatory" : "true"
    },
    "protocol_stack" : {
        "description" : "protocol stack parameters",
        "type" : "JSON",
        "displayName" : "Protocol stack parameters",
        "order" : "2",
        "default" : QUOTE ({
            "protocol_stack" : {
                "name" : "tase2client",
                "version" : "0.0.1",
                "transport_layer" : {
                    "ied_name" : "IED1",
                    "connections" : [ {
                        "ip_addr" : "127.0.0.1",
                        "port" : 102,
                        "tls" : false
                    } ]
                },
                "application_layer" : { "polling_interval" : 10000 }
            }
        })
    },
    "exchanged_data" : {
        "description" : "Exchanged datapoints configuration",
        "type" : "JSON",
        "displayName" : "Exchanged datapoints",
        "order" : "3",
        "default" : QUOTE ({
            "exchanged_data" : {
                "datapoints" : [ {
                    "label" : "TS1",
                    "pivot_id" : "TS1",
                    "protocols" : [ {
                        "name" : "tase2",
                        "objref" : "DER_Scheduler_Control/ActPow_GGIO1.AnOut1",
                        "cdc" : "ApcTyp"
                    } ]
                } ]
            }
        })
    },
    "tls_conf" : {
        "description" : "TLS configuration",
        "type" : "JSON",
        "displayName" : "TLS Configuration",
        "order" : "4",
        "default" : QUOTE ({
            "tls_conf" : {
                "private_key" : "iec104_server.key",
                "own_cert" : "iec104_server.cer",
                "ca_certs" : [
                    { "cert_file" : "iec104_ca.cer" },
                    { "cert_file" : "iec104_ca2.cer" }
                ],
                "remote_certs" :
                    [ { "cert_file" : "iec104_client.cer" } ]
            }
        })
    }
});

/**
 * The 61850 plugin interface
 */
extern "C"
{
    static PLUGIN_INFORMATION info = {
        PLUGIN_NAME,           // Name
        VERSION,               // Version (automaticly generated by mkversion)
        SP_ASYNC | SP_CONTROL, // Flags - added control
        PLUGIN_TYPE_SOUTH,     // Type
        "1.0.0",               // Interface version
        default_config         // Default configuration
    };

    /**
     * Return the information about this plugin
     */
    PLUGIN_INFORMATION*
    plugin_info ()
    {
        Tase2Utility::log_info ("61850 Config is %s", info.config);
        return &info;
    }

    /**
     * Initialise the plugin, called to get the plugin handle
     */
    PLUGIN_HANDLE
    plugin_init (ConfigCategory* config)
    {
        TASE2* tase2 = nullptr;
        Tase2Utility::log_info ("Initializing the plugin");

        tase2 = new TASE2 ();

        if (tase2)
        {
            if (config->itemExists ("asset"))
                tase2->setAssetName (config->getValue ("asset"));
            else
                tase2->setAssetName ("tase 2");

            if (config->itemExists ("protocol_stack")
                && config->itemExists ("exchanged_data")
                && config->itemExists ("tls_conf"))
                tase2->setJsonConfig (config->getValue ("protocol_stack"),
                                      config->getValue ("exchanged_data"),
                                      config->getValue ("tls_conf"));
        }

        return (PLUGIN_HANDLE)tase2;
    }

    /**
     * Start the Async handling for the plugin
     */
    void
    plugin_start (PLUGIN_HANDLE* handle)
    {
        if (!handle)
            return;

        Tase2Utility::log_info ("Starting the plugin");

        auto* tase2 = reinterpret_cast<TASE2*> (handle);
        tase2->start ();
    }

    /**
     * Register ingest callback
     */
    void
    plugin_register_ingest (PLUGIN_HANDLE* handle, INGEST_CB cb, void* data)
    {
        if (!handle)
            throw exception ();

        auto* tase2 = reinterpret_cast<TASE2*> (handle);
        tase2->registerIngest (data, cb);
    }

    /**
     * Poll for a plugin reading
     */
    Reading
    plugin_poll (PLUGIN_HANDLE* handle)
    {
        throw runtime_error (
            "IEC_61850 is an async plugin, poll should not be called");
    }

    /**
     * Reconfigure the plugin
     */
    void
    plugin_reconfigure (PLUGIN_HANDLE* handle, std::string& newConfig)
    {
        ConfigCategory config ("newConfig", newConfig);
        auto* tase2 = reinterpret_cast<TASE2*> (*handle);

        tase2->stop ();

        if (config.itemExists ("protocol_stack")
            && config.itemExists ("exchanged_data")
            && config.itemExists ("tls"))
            tase2->setJsonConfig (config.getValue ("protocol_stack"),
                                  config.getValue ("exchanged_data"),
                                  config.getValue ("tls_conf"));

        if (config.itemExists ("asset"))
        {
            tase2->setAssetName (config.getValue ("asset"));
            Tase2Utility::log_info (
                "61850 plugin restart after reconfigure asset");
            tase2->start ();
        }
        else
        {
            Tase2Utility::log_error ("61850 plugin restart failed");
        }
    }

    /**
     * Shutdown the plugin
     */
    void
    plugin_shutdown (PLUGIN_HANDLE* handle)
    {
        auto* tase2 = reinterpret_cast<TASE2*> (handle);

        tase2->stop ();
        delete tase2;
    }

    /**
     * plugin plugin_write entry point
     * NOT USED
     */
    bool
    plugin_write (PLUGIN_HANDLE* handle, std::string& name, std::string& value)
    {
        return false;
    }

    /**
     * plugin plugin_operation entry point
     */
    bool
    plugin_operation (PLUGIN_HANDLE* handle, std::string& operation, int count,
                      PLUGIN_PARAMETER** params)
    {
        if (!handle)
            throw exception ();

        auto* tase2 = reinterpret_cast<TASE2*> (handle);

        return tase2->operation (operation, count, params);
    }

} // extern "C"
