#include "tase2.hpp"

TASE2::~TASE2 (){
    stop();
    delete m_config;
}

void TASE2::setJsonConfig (const std::string& stack_configuration,
                    const std::string& msg_configuration,
                    const std::string& tls_configuration){}

void TASE2::start (){}
void TASE2::stop (){}

void TASE2::ingest (const std::string& assetName,
             const std::vector<Datapoint*>& points){}

void TASE2::registerIngest (void* data, void (*cb) (void*, Reading)){}

bool TASE2::operation (const std::string& operation, int count,
                PLUGIN_PARAMETER** params){}
