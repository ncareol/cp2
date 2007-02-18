#include "CP2UdpSocket.h"
#include <string>
#include <QList>
#include <QNetworkInterface>
#include <QNetworkAddressEntry>
#include <winsock2.h>


///////////////////////////////////////////////////////////
CP2UdpSocket::CP2UdpSocket(
						   std::string network, 
						   int port,
						   bool broadcast, 
						   int sndBufferSize, 
						   int rcvBufferSize):
_network(network),
_port(port),
_broadcast(broadcast),
_sndBufferSize(sndBufferSize),
_rcvBufferSize(rcvBufferSize),
_ok(false)
{
	QList<QNetworkInterface> allIfaces = QNetworkInterface::allInterfaces();

	QNetworkAddressEntry addrEntry;
	int i;
	bool found = false;
	for (i = 0; i < allIfaces.size(); i++) {
		QNetworkInterface iface = allIfaces[i];
		QList<QNetworkAddressEntry> addrs = iface.addressEntries();
		for (int j = 0; j < addrs.size(); j++) {
			std::string thisIp = addrs[j].ip().toString().toAscii();
			if (thisIp.find(_network)!= std::string::npos) {
				addrEntry = addrs[j];
				found = true;
				break;
			}
		}
	}

	if (!found) {
		_errorMsg += "Unaable to find interface for network ";
		_errorMsg += _network;
		_errorMsg += "\n";
		return;
	}

	if (!broadcast) {
		_hostAddress = addrEntry.ip();
	} else {
		_hostAddress = addrEntry.broadcast();
	}

	// bind socket to port/network
	int optval = 1;
	if (!bind(_hostAddress, _port)) {
		_errorMsg += "Unable to bind datagram port ";
		_errorMsg += _hostAddress.toString().toAscii().constData();
		_errorMsg += ":";
		_errorMsg += QString("%1").arg(_port).toAscii().constData();
		_errorMsg += "\n";
		return;
	}

	int result = setsockopt(socketDescriptor(), 
		SOL_SOCKET, 
		SO_REUSEADDR, 
		(const char*)&optval, 
		sizeof(optval)); 

	if (_sndBufferSize) {
		// set the system send buffer size
		int sockbufsize = _sndBufferSize;
		result = setsockopt (socketDescriptor(),
			SOL_SOCKET,
			SO_SNDBUF,
			(char *) &sockbufsize,
			sizeof sockbufsize);
		if (result) {
			_errorMsg += "Set send buffer size for socket failed\n";
		}
	}

	if (_rcvBufferSize) {
		// set the system receive buffer size
		int sockbufsize = _rcvBufferSize;
		result = setsockopt (socketDescriptor(),
			SOL_SOCKET,
			SO_RCVBUF,
			(char *) &sockbufsize,
			sizeof sockbufsize);
		if (result) {
			_errorMsg += "Set receive buffer size for socket failed\n";
		}
	}

	_ok = true;
}

///////////////////////////////////////////////////////////
CP2UdpSocket::~CP2UdpSocket()
{
}

///////////////////////////////////////////////////////////
bool
CP2UdpSocket::ok()
{
	return _ok;
}
///////////////////////////////////////////////////////////
std::string
CP2UdpSocket::errorMsg() {
	return _errorMsg;
}

///////////////////////////////////////////////////////////
std::string
CP2UdpSocket::toString()
{
	return _hostAddress.toString().toAscii().constData();
}

///////////////////////////////////////////////////////////
QHostAddress
CP2UdpSocket::hostAddress()
{
	return _hostAddress;
}

