#ifndef PCITIMERINC_
#define PCITIMERINC_

#include <vector>

#define	PCITIMER_DEVICE_ID 	0x8504
#define	PCITIMER_VENDOR_ID	0x10E8   
#define	COUNTFREQ	6000000
#define	TIMER_RESET	0
#define	TIMER_STOP	1
#define	TIMER_START	2

#define	TIMER_RQST	1

#define	MAXSEQUENCE  38

typedef 
union TIMERHIMEDLO
{	
	unsigned int		himedlo;
	struct 
	{
		unsigned char		lo,med,hi,na;
	} byte;
} TIMERHIMEDLO;

typedef 
union TIMERHILO {	
	unsigned short	hilo;
	struct
	{
		unsigned char	lo,hi;
	} byte;
} TIMERHILO;

//typedef	struct TESTPULSE {
//	HILO		delay;
//	HILO		width;
//} TESTPULSE;

//typedef	struct TIMER {
//	SEQUENCE    seq[MAXSEQUENCE];
//	TESTPULSE   tp[8];
//	HILO        seqdelay,sync;
//	int         timingmode,seqlen;
//	float       clockfreq,reffreq,phasefreq;
//	char*       base;
//}	TIMER;
//typedef	struct SEQUENCE {
//	HILO		period;
//	int		pulseenable;
//	int		polarization;
//	int		phasedata;
//	int		index;
//} SEQUENCE;

/// A configuration class for the PciTimer. An instance is created and
/// filled, and then passed in the constructor to a PciTimer.
class PciTimerConfig {
	friend class PciTimer;
public:
	/// Constructor
	PciTimerConfig(int systemClock, ///< The system clock, in Hz.
		int timingMode              ///< The timimg mode. 0 - generate the PRF onboard, 1 - the PRF comes from an external trigger
		);
	/// Destructor
	virtual ~PciTimerConfig();
	/// Add a new sequence step to the end of the sequence list.
	void addSequence(int length, ///< The length of this sequence, in counts
		unsigned char pulseMask, ///< A mask which defines which BPULSE signals are enabled during this sequence step.
		int polarization,        ///< Polarization value for this sequence step. Not clear what it does.
		int phase                ///< Phase value for this sequence step. Not sure what it does.
		);
	/// Add a new sequence step to the end of the sequence list.
	void addPrt(float prt,       ///< The length of this sequence, seconds.
		unsigned char pulseMask, ///< A mask which defines which BPULSE signals are enabled during this sequence step.
		int polarization,        ///< Polarization value for this sequence step. Not clear what it does.
		int phase                ///< Phase value for this sequence step. Not sure what it does.
		);
	/// Define a BPULSE, which is a pulse of a fixed delay and length.
	void setBpulse(int index, unsigned short width, unsigned short delay);

protected:
	/// There are six BPULSE signals, which can be triggered at the beginning of
	/// each sequence. However, a sequeence can selectively choose which BPULSES to
	/// trigger. Each BPULSE can have a width and a delay.
	struct Bpulse {
		TIMERHILO width;   ///< The BPULSE width in counts.
		TIMERHILO delay;   ///< The BPULSE delay in counts.
	} _bpulse[6];
	/// Specify the parameters for a sequence. 
	struct Sequence {
		TIMERHILO length;       ///< The length of this sequence in counts.
		unsigned char bpulseMask;  ///< Enable a BPULSE by setting bits 0-5.
		int polarization; ///< Not sure what this is for.
		int phase;        ///< Not sure what this is for.
	};
	/// Collect all sequences. These sequences will be executed
	/// in the order that they are defined.
	std::vector<Sequence> _sequences;
	/// The system clock rate, in Hz. It is used in various timng calculations.
	int _systemClock;
	/// The timing mode: 0 - internal prf generation, 1 - external prf generation.
	int _timingMode;
};


/////////////////////////////////////////////////////////////////////////

/// The NCAR PCI based timer card is managed by this class. The timer is
/// reset and intialized during construction. The card may be started and 
/// stopped after that.
///
/// To use:
/// <ul>
/// <li>Create a PciTimerConfig
/// <li>Configure timer behavior via PciTimerConfig::setBpulse(), PciTimerConfig::addSequence() and PciTimerConfig::addPrt()
/// <li>Create a PciTimer, passing the PciTimerConfig to the constructor.
/// <li>Call PciTimer::start() to start the timer.
/// <li>Call PciTimer::stop() to stop the timer.
/// <li>The timer will be stopped and reset if the PciTimer destructor is called.
/// </ul>
class PciTimer {
public:
	/// Reset, confgure and intialize the timer card during construction.
	PciTimer(PciTimerConfig config);
	/// Destructor.
	~PciTimer();
	/// Stop the timer,and reset it.
	void reset();
	/// Start the timer
	void start();
	/// Stop the timer
	void stop();
	/// return the current error flag.
	bool error();

protected:
	/// Set the timer configuration registers.
	void set();
	/// Test the timer. Not sure what this accomplishes.
	int	 test();

protected:
	/// The PCI address of the timer base register
	char* _base;
	/// The configuration for this timer card.
	PciTimerConfig _config;
	/// The clock frequency, in Hz.
	float _clockfreq;
	/// The reference clock frequency, in Hz.
	float _reffreq;
	/// The phase frequency. in Hz.
	float _phasefreq;
	// The sync configuration word. What does this do?
	TIMERHILO _sync;
	/// The seq delay. What does this do?
	TIMERHILO _seqdelay;
	/// Set true if there has been a timer error
	bool _error;


};

#endif