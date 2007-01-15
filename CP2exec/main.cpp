#include <winsock2.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <conio.h>
#include <windows.h>
#include <mmsystem.h>
#include <sys/timeb.h>
#include <time.h>
#include <iostream>
#include <qapplication.h>

// local includes
#include "CP2exec.h" 
#include "CP2PIRAQ.h"
#include "CP2ExecThread.h"

// from CP2Lib
#include "timerlib.h"
#include "config.h"
#include "pci_w32.h"

// from CP2Piraq
#include "piraqComm.h"

// from PiraqIII_RevD_Driver
#include "piraq.h"
#include "plx.h"
#include "control.h"
#include "HPIB.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/// Initialize the windows network interface. closeNetwork() 
/// must be called the same number of times that this routine is
/// called, because WSAstartup() mantains a reference count. See
/// the windows documentation.
/// @param ipName The network name that datagrams will be sent to
/// @param port the destination port.
/// @param sockAddr A sockAddr structure will be initialized here, so that
///  it can be used later for the sendto() call.
/// @return The socke file descriptor, or -1 if failure.
int initNetwork(char* ipName, int port, struct sockaddr_in& sockAddr)
{
	WORD wVersionRequested;
	WSADATA wsaData;

	// startup 
	wVersionRequested = MAKEWORD( 2, 2 );	 
	if (WSAStartup( wVersionRequested, &wsaData )){
		printf ("WARNING - WSAStartup failed - networking may not be avaiable\n");
	}
	int sock = WSASocket(AF_INET,SOCK_DGRAM,IPPROTO_UDP,NULL,0,WSA_FLAG_OVERLAPPED);	//	was dwFlags = 0
	if (sock <0)
		return -1;

	/// initialize sockAddr
	struct hostent* hent = gethostbyname(ipName);
	int inetAddr = *(long*)hent->h_addr;
	sockAddr.sin_port = htons(port);
	sockAddr.sin_addr.s_addr = inetAddr;
	sockAddr.sin_family = AF_INET;

	// ask for a large socket send buffer
	int sockbufsize = 50000000;

	int result = setsockopt (sock,
		SOL_SOCKET,
		SO_SNDBUF,
		(char *) &sockbufsize,
		sizeof sockbufsize);
	if (result) {
		printf("Set send buffer size for socket failed\n");
		exit(1); 
	}



	return sock;

}

void
closeNetwork()
{
	WSACleanup( );
}

/////////////////////////////////////////////////////////////////////////////
int keyvalid() 
{
	while(!_kbhit()) { } // no keystroke 
	getch();    // get it
	return 1;
}

/////////////////////////////////////////////////////////////////////////////
unsigned int
findPMACdpram() 
{
	PCI_CARD*	pcicard;
	unsigned int reg_base;

	pcicard = find_pci_card(0x1172,1,0);

	if(!pcicard)
		return(0);

	reg_base = pcicard->phys2;

	return reg_base;
}
/////////////////////////////////////////////////////////////////////////////
int 
main(int argc, char* argv[], char* envp[])
{
	CP2PIRAQ* piraq1 = 0;
	CP2PIRAQ* piraq2 = 0;
	CP2PIRAQ* piraq3 = 0;

	CONFIG *config1, *config2, *config3;
	char fname1[100]; char fname2[100]; char fname3[100]; // configuration filenames
	char* destIP = "192.168.3.255";
	//destIP = "127.0.0.1";

	unsigned int packetsPerPciXfer; 
	int outport;
	char c;
	int piraqs = 0;   // board count -- default to single board operation 
	FILE * dspEXEC; 

	long long pulsenum;
	unsigned int PMACphysAddr;

	int	PIRAQadjustAmplitude = 0; 
	int	PIRAQadjustFrequency = 1; 
	TIMER ext_timer; // structure defining external timer parameters 

	config1	= new CONFIG; 
	config2	= new CONFIG; 
	config3	= new CONFIG; 

	if (argc < 4) {
		printf("CP2exec <DSP filename> <rcv data format: 'f' floats, 's' unsigned shorts> <piraq mask> [config file]\n"); 
		exit(1);
	}

	if ((dspEXEC = fopen(argv[1],"r")) == NULL) // DSP executable file not found 
	{ 
		char* dspFileName = argv[1];
		printf("Usage: %s <DSP filename> DSP executable file not found\n", argv[0]); exit(0); 
	} 
	fclose(dspEXEC); // existence test passed; use command-line filename

	printf("file %s will be loaded into piraq\n", argv[1]); 

	piraqs = atoi(argv[3]); 

	if (argc > 4) { // entered a filename
		strcpy(fname1, argv[4]);
		strcpy(fname2, argv[4]);
		strcpy(fname3, argv[4]);
	}
	else {
		strcpy(fname1, "config1");	 
		strcpy(fname2, "config2");	 
		strcpy(fname3, "config3");	 
	}

	printf(" config1 filename %s will be used\n", fname1);
	printf(" config2 filename %s will be used\n", fname2);
	printf(" config3 filename %s will be used\n", fname3);
//	printf("\n\nTURN TRANSMITTER OFF for piraq dc offset measurement.\nPress any key to continue.\n"); 
//	while(!kbhit())	;
//	c = toupper(getch()); // get the character

	// Initialize the network
	outport = 3100; 
	struct sockaddr_in sockAddr;
	int sock = initNetwork(destIP, outport, sockAddr);

	// stop timer card
	cp2timer_stop(&ext_timer);  

	// read in fname.dsp, or use configX.dsp if NULL passed. set up all parameters
	readconfig(fname1, config1);    
	readconfig(fname2, config2);    
	readconfig(fname3, config3);   

	/// NOTE- packetsPerPciXfer is computed here from the size of the PPACKET packet, such that
	/// it will be smaller than 64K. This must hold true for the PCI 
	/// bus transfers. 
	int blocksize = sizeof(PINFOHEADER)+
		config1->gatesa * 2 * sizeof(float);
	packetsPerPciXfer = 65536 / blocksize; 
	if	(packetsPerPciXfer % 2)	//	computed odd #hits
		packetsPerPciXfer--;	//	make it even

	// find the PMAC card
	PMACphysAddr = findPMACdpram();
	if (PMACphysAddr == 0) {
		printf("unable to locate PMAC dpram\n");
		exit(-1);
	}
	printf("PMAC DPRAM base addr is 0x%08x\n", PMACphysAddr);

	///////////////////////////////////////////////////////////////////////////
	//
	//    Create piraqs. They all have to be created, so that all boards
	//    are found in succesion, even if we will not be collecting data 
	//    from all of them.

	piraq1 = new CP2PIRAQ(sockAddr, sock, destIP, outport,   fname1, argv[1], packetsPerPciXfer, PMACphysAddr, 0, SHV);
	piraq2 = new CP2PIRAQ(sockAddr, sock, destIP, outport+1, fname2, argv[1], packetsPerPciXfer, PMACphysAddr, 1, XH);
	piraq3 = new CP2PIRAQ(sockAddr, sock, destIP, outport+2, fname3, argv[1], packetsPerPciXfer, PMACphysAddr, 2, XV);

	///////////////////////////////////////////////////////////////////////////
	//
	//     Calculate starting beam and pulse numbers. 
	//     These are passed on to the piraqs, so that they all
	//     start with the same beam and pulse number.

	float prt;
	prt = piraq3->prt();
	unsigned int pri; 
	pri = (unsigned int)((((float)COUNTFREQ)/(float)(1/prt)) + 0.5); 
	time_t now = time(&now);
	pulsenum = ((((long long)(now+2)) * (long long)COUNTFREQ) / pri) + 1; 
	printf("pulsenum = %I64d\n", pulsenum); 

	///////////////////////////////////////////////////////////////////////////
	//
	//      start the piraqs, waiting for each to indicate they are ready

	if (piraqs & 0x01) { // turn on slot 1
		if (piraq1->start(pulsenum)) 
			exit(-1);
	} 
	if (piraqs & 0x02) { // turn on slot 2
		if (piraq2->start(pulsenum)) 
			exit(-1);
	} 
	if (piraqs & 0x04) { // turn on slot 3
		if (piraq3->start(pulsenum)) 
			exit(-1);
	} 

	printf("\nAll piraqs have been started. %d pulses will be \ntransmitted for each PCI bus transfer\n\n",
		packetsPerPciXfer);

	///////////////////////////////////////////////////////////////////////////
	//
	// start timer board immediately

	PINFOHEADER info;
	info = piraq3->info();
	cp2timer_config(&ext_timer, &info, config3->prt, config3->xmit_pulsewidth);
	cp2timer_set(&ext_timer);		// put the timer structure into the timer DPRAM 
	cp2timer_reset(&ext_timer);	// tell the timer to initialize with the values from DPRAM 
	cp2timer_start(&ext_timer);	// start timer 

	QApplication app( argc, argv );

	CP2ExecThread* pThread = new CP2ExecThread(piraqs, piraq1, piraq2, piraq3);

	// create our main window. It may contain a PPI sometime, and 
	// other buttons etc.
	CP2Exec cp2exec(pThread);

	// if we don't show() the  dialog, nothing appears!
	cp2exec.show();

 	// This tells cp2ppi to stop running when the main window
	// closes.
	app.setMainWidget(&cp2exec);
	
	return app.exec();

// exit NOW:
	printf("\npress a key to stop piraqs and exit\n"); while(!keyvalid()) ; 
	printf("piraqs stopped.\nexit.\n\n"); 

	// remove for lab testing: keep transmitter pulses active w/o go.exe running. 12-9-04
	cp2timer_stop(&ext_timer); 

	if (piraq1)
		delete piraq1; 
	if (piraq2)
		delete piraq2; 
	if (piraq3)
		delete piraq3;

	closeNetwork();

	exit(0); 

}

