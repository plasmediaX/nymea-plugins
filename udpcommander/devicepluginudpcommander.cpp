/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2015 Simon Stürz <simon.stuerz@guh.io>                   *
 *  Copyright (C) 2017 Bernhard Trinnes <bernhard.trinnes@guh.io>          *
 *                                                                         *
 *  This file is part of nymea.                                            *
 *                                                                         *
 *  This library is free software; you can redistribute it and/or          *
 *  modify it under the terms of the GNU Lesser General Public             *
 *  License as published by the Free Software Foundation; either           *
 *  version 2.1 of the License, or (at your option) any later version.     *
 *                                                                         *
 *  This library is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *  Lesser General Public License for more details.                        *
 *                                                                         *
 *  You should have received a copy of the GNU Lesser General Public       *
 *  License along with this library; If not, see                           *
 *  <http://www.gnu.org/licenses/>.                                        *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*!
    \page udpcommander.html
    \title UDP Commander
    \brief Plugin for catching UDP commands from the network.

    \ingroup plugins
    \ingroup nymea-plugins-maker

    This plugin allows to receive UDP packages over a certain UDP port and generates an \l{Event} if the message content matches
    the \l{Param} command.

    \note This plugin is ment to be combined with a \l{nymeaserver::Rule}.

    \section3 Example

    If you create an UDP Commander on port 2323 and with the command \c{"Light 1 ON"}, following command will trigger an \l{Event} in nymea
    and allows you to connect this \l{Event} with a \l{nymeaserver::Rule}.

    \note In this example nymea is running on \c localhost

    \code
    $ echo "Light 1 ON" | nc -u localhost 2323
    OK
    \endcode

    This allows you to execute \l{Action}{Actions} in your home automation system when a certain UDP message will be sent to nymea.

    If the command will be recognized from nymea, the sender will receive as answere a \c{"OK"} string.

    \chapter Plugin properties
    Following JSON file contains the definition and the description of all available \l{DeviceClass}{DeviceClasses}
    and \l{Vendor}{Vendors} of this \l{DevicePlugin}.

    For more details how to read this JSON file please check out the documentation for \l{The plugin JSON File}.

    \quotefile plugins/deviceplugins/udpcommander/devicepluginudpcommander.json
*/

#include "devicepluginudpcommander.h"
#include "plugin/device.h"
#include "plugininfo.h"

DevicePluginUdpCommander::DevicePluginUdpCommander()
{

}

DeviceManager::DeviceSetupStatus DevicePluginUdpCommander::setupDevice(Device *device)
{
    // check port
    // TODO ports used by an input should be available for outputs and vice versa
    if ((device->deviceClassId() == udpInputDeviceClassId) || (device->deviceClassId() == udpOutputDeviceClassId)) {
        bool portOk = false;
        int port = device->paramValue(portParamTypeId).toInt(&portOk);
        if (!portOk || port <= 0 || port > 65535) {
            qCWarning(dcUdpCommander) << device->name() << ": invalid port:" << device->paramValue(portParamTypeId).toString() << ".";
            return DeviceManager::DeviceSetupStatusFailure;
        }

        QUdpSocket *udpSocket = new QUdpSocket(this);

        if (!udpSocket->bind(QHostAddress::Any, port)) {
            qCWarning(dcUdpCommander) << device->name() << "can't bind to port" << port << ".";
            delete udpSocket;
            return DeviceManager::DeviceSetupStatusFailure;
        }

        connect(udpSocket, SIGNAL(readyRead()), this, SLOT(readPendingDatagrams()));
        m_commanderList.insert(udpSocket, device);

        return DeviceManager::DeviceSetupStatusSuccess;
    }
    return DeviceManager::DeviceSetupStatusFailure;
}


DeviceManager::DeviceError DevicePluginUdpCommander::executeAction(Device *device, const Action &action) {

    if (device->deviceClassId() == udpOutputDeviceClassId) {

        if (action.actionTypeId() == outputDataActionTypeId) {
            QUdpSocket *udpSocket = m_commanderList.key(device);
            int port = device->paramValue(portParamTypeId).toInt();
            QHostAddress address = QHostAddress(device->paramValue(ipv4AddressParamTypeId).toString());
            QByteArray data = action.param(outputDataAreaParamTypeId).value().toByteArray();
            qDebug(dcUdpCommander()) << "Send UDP datagram:" << data << "address:" << address.toIPv4Address() << "port:" << port;
            udpSocket->writeDatagram(data, address, port);

            return DeviceManager::DeviceErrorNoError;
        }
        return DeviceManager::DeviceErrorActionTypeNotFound;
    }
    return DeviceManager::DeviceErrorDeviceClassNotFound;
}

void DevicePluginUdpCommander::deviceRemoved(Device *device)
{
    if ((device->deviceClassId() == udpInputDeviceClassId) || (device->deviceClassId() == udpOutputDeviceClassId)){

        QUdpSocket *socket = m_commanderList.key(device);
        m_commanderList.remove(socket);

        socket->close();
        socket->deleteLater();
    }
}

void DevicePluginUdpCommander::readPendingDatagrams()
{
    QUdpSocket *socket= qobject_cast<QUdpSocket*>(sender());
    Device *device = m_commanderList.value(socket);

    QByteArray datagram;
    QHostAddress sender;
    quint16 senderPort;

    while (socket->hasPendingDatagrams()) {
        datagram.resize(socket->pendingDatagramSize());
        socket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
    }

    device->setStateValue(inputDataStateTypeId, datagram);

    //TO CHECK remove command param and event, comparison may be done in the rule engine
    if (datagram == device->paramValue(commandParamTypeId).toByteArray() ||
            datagram == device->paramValue(commandParamTypeId).toByteArray() + "\n") {
        qCDebug(dcUdpCommander) << device->name() << " got command from" << sender.toString() << senderPort;
        emit emitEvent(Event(commanderCommandReceivedEventTypeId, device->id()));
        socket->writeDatagram("OK\n", sender, senderPort);
    }
}
