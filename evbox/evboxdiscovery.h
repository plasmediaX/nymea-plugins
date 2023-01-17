#ifndef EVBOXDISCOVERY_H
#define EVBOXDISCOVERY_H

#include <QObject>
#include "evboxport.h"

class EVBoxDiscovery : public QObject
{
    Q_OBJECT
public:
    explicit EVBoxDiscovery(EVBoxPort *evboxPort, QObject *parent = nullptr);

signals:


private:
    EVBoxPort *m_port = nullptr;

};

#endif // EVBOXDISCOVERY_H
