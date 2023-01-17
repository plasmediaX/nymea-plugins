#include "evboxdiscovery.h"

EVBoxDiscovery::EVBoxDiscovery(EVBoxPort *evboxPort, QObject *parent)
    : QObject{parent},
      m_port(evboxPort)
{

}

