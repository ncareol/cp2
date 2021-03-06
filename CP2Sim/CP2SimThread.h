#ifndef CP2SIMTHREADH_
#define CP2SIMTHREADH_

#include <qthread.h>
#include <qmutex.h>
#include <qwaitcondition.h>
#include <vector>

// CP2 timeseries network transfer protocol.
#include "CP2Net.h"
using namespace CP2Net;

// Configuration management
#include "CP2Config.h"

// UDP socket support
#include "CP2UdpSocket.h"

// Angle simulator
#include "SimAngles.h"

class CP2SimThread :
	public QThread
{
public:
	CP2SimThread();
	virtual ~CP2SimThread(void);
	virtual void run();
	/// @param runstate Set true to produce output, false otherwise
	void setRunState(bool runState);
	/// Call to terminate the thread
	void end();
	/// @return The cumulative pulse count, in thousands
	int getPulseCount();
	/// @return The host IP address
	std::string hostAddressToString();


protected:
	/// initialize the socket for outgoing pulses.
	void initializeSocket(); 
	/// Send out the next set of pulses
	void nextPulses();
	// send the data on the pulse socket
	int sendData(int size, void* data);
	// create a SimAngles from the configuration
	SimAngles createSimAngles();
	/// The configuration
	CP2Config _config;
	/// set true if pulses are to be calculated; false otherwise.
	bool _run;
	/// set true when its time to exit the thread
	bool _quitThread;
	/// The time series raw data socket.
	CP2UdpSocket*   _pPulseSocket;
	/// The port number for outgoing pulses.
	int	_pulsePort;
	/// The maximum message size that we can send
	/// on UDP.
	int _soMaxMsgSize;
	/// Sband data fill area
	std::vector<float> _sData;
	/// xhband data fill area
	std::vector<float> _xhData;
	/// xvband data fill area
	std::vector<float> _xvData;
	/// number of gates
	int _gates;
	/// Number of pulses to put in each datagram
	int _pulsesPerDatagram;
	/// The number of resends on the datagram
	int _resendCount;
	/// The cumulative pulse count
	int _pulseCount;

	CP2Packet _sPacket;
	CP2Packet _xhPacket;
	CP2Packet _xvPacket;

	SimAngles _simAngles;
};

#endif

