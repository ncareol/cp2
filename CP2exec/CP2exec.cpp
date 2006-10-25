#include		<stdafx.h> //  this gets getch() and kbhit() included ... 
#include        <stdio.h>
#include        <ctype.h>
#include        <string.h>
#include        <stdlib.h>
#include        <math.h>
#include		<conio.h>
#include		<windows.h>
#include <winsock2.h>
#include <mmsystem.h>

#include "stdafx.h"      
#include "CP2exec.h" 

#include "../include/proto.h"

#include "get_julian_day.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define COUNTS_PER_DEGREE 182.04444	// move this to proto.h

static	char *FIFONAME = "/PRQDATA";

#define	FIFOADDRESS  0xA00000

//#define			TIME_TESTING		// define to activate millisecond printout for time of events. 
#ifdef CP2_TESTING		// switch ON test code for CP2 
// test drx data throughput limits by varying data packet size w/o changing DSP computational load:  
#define			DRX_PACKET_TESTING	// define to activate data packet resizing for CP2 throughput testing. 
#endif
#define			CYCLE_HITS	20	// #fifo hits to take from piraq-host shared memory before rotating to next board: CP2
//#define NO_INTEGER_BEAMS // for staggered PRT testing, etc., defeat angle interpolation, etc. 

//#define TESTING_TIMESERIES // compute test diagnostic timeseries data in one of two piraq channels: 
//#define TESTING_TIMESERIES_RANGE	// test dynamic reassignment of timeseries start, end gate using 'U','D'

#define PIRAQ3D_SCALE	1.0/pow(2,31)	// normalize 2^31 data to 1.0 full scale using multiplication

FILE * db_fp; // debug data file 

int keyvalid() 
{
	while(!_kbhit()) { } // no keystroke 
	getch();    // get it
	return 1;
}

/////////////////////////////////////////////////////////////////////////////
using namespace std;

PIRAQ        *piraq;  // allocate pointers for object instantiation
PIRAQ        * piraq1, * piraq2, * piraq3;  // try more 
#ifdef TIMER_PIRAQ
PIRAQ        * timer_piraq; // extra PIRAQ used as substitute for external timer card
#endif
CONFIG       *config;
CONFIG       * config1, * config2, * config3;
FIRFILTER    *FirFilter;

//  pointers to FIR channel allocations
unsigned short * firch1_i; 
unsigned short * firch1_q; 
unsigned short * firch2_i; 
unsigned short * firch2_q; 

#define			ABSCALE		1.E+4
#define			PSCALE		1.0
#define			DBOFFSET	80.0

//for mSec-resolution time tests: 
#include <sys/timeb.h>
#include <time.h>
struct _timeb timebuffer;
char *timeline;

static unsigned short last_millisec, delta_millisec; 

/**
unsigned int BeamsperSec; 
unsigned int mSecperBeam; 
float ExactmSecperBeam; 
__int64 RadarEpochMillisec; 
__int64 SystemEpochMillisec; 
__int64 TimerStartCorrection; // runtime measurement of timer card startup interval in mSec to apply to REMSEc
// fp: 
double fpExactmSecperBeam; 
double fpRadarMillisec; 
double fpSystemMillisec; 
double fpRadarSystemCorrection;
__int64 RadarMillisec; 
__int64 SystemMillisec; 
**/


// set/compute #hits combined by piraq: equal in both piraq executable (CP2_DCCS3_1.out) and CP2exec.exe 
unsigned int Nhits; 
unsigned int packets = 0; 

int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
	int i,j,k,y,outport;
	int idx_tts = 0; // index for test timeseries data
	int cmd1_notifysock, cmd2_notifysock, cmd3_notifysock; 
	int val1, val2, val3; 
	unsigned __int64 temp1, temp2, temp3; 
	int outsock1, outsock2, outsock3; 
	int piraqnum=1, gates_gg;
	char c,name[80];
	FIFO *fifo1, *fifo2, *fifo3; 
	FIFO *cmd1, *cmd2, *cmd3; 
	UDPHEADER *udp1, *udp2, *udp3;
	struct timeval tv1, tv2, tv3;
	fd_set rfd1, rfd2, rfd3;
	PACKET *fifopiraq1, *fifopiraq2, *fifopiraq3;
	PACKET *pkt1, *pkt2, *pkt3, *pn_pkt; 
	config  = new CONFIG;
	config1	= new CONFIG; 
	config2	= new CONFIG; 
	config3	= new CONFIG; 
	char fname1[10]; char fname2[10]; char fname3[10]; // configuration filenames

	float		az = 0, el = 0, pcorrect;
	unsigned int scan = 0, volume = 0; 
	float		az1 = 0, az2 = 0, az3 = 0, el1 = 0, el2 = 0, el3 = 0;
	float		test_ts_power = 0.1f; // multiplier for test timeseries data 
	unsigned int scan1 = 0, scan2 = 0, scan3 = 0, volume1 = 0, volume2 = 0, volume3 = 0; 
	int			test_ts_adjust = 2; // note if 0 test timeseries single frequency gets flat data! 
	unsigned __int64	oldbnum=0; 
	int			send=0;

	int nRetCode = 0; // used by MFC 
	int testnum = 1;  // console output sequential test number 
	int r_c; // return code 
	int nodefault = FALSE; 
	int piraqs = 0;   // board count -- default to single board operation 
	int fifo1_hits, fifo2_hits, fifo3_hits; // cumulative hits per board 
	int cur_fifo1_hits, cur_fifo2_hits, cur_fifo3_hits; // current hits per board 
	int cycle_fifo1_hits, cycle_fifo2_hits, cycle_fifo3_hits; // current hits per cycle 
	cycle_fifo1_hits = cycle_fifo2_hits = cycle_fifo3_hits = 0; // clear hits per cycle     
	int x = 0; 
	unsigned int FIRCount = 32; 
	float *fsrc, *diagiqsrc;
	FILE * dspEXEC; 
	int cmdline_filename = FALSE; 
	int dspl_hits = 100; // fifo hits modulus for updating display 
	char dspl_format = FALSE; 
	unsigned int seq1, seq2, seq3; 
	unsigned int bytespergate; 

	__int64 lastpulsenumber = 0;
	__int64 lastpulsenumber1, lastpulsenumber2, lastpulsenumber3;
	lastpulsenumber1 = lastpulsenumber2 = lastpulsenumber3 = 0;
	__int64 lastbeamnumber = 0;
	__int64 lastbeamnumber1, lastbeamnumber2, lastbeamnumber3;
	lastbeamnumber1 = lastbeamnumber2 = lastbeamnumber3 = 0;
	int  PNerrors1, PNerrors2, PNerrors3; 
	PNerrors1 = PNerrors2 = PNerrors3 = 0; 
	float scale1, scale2, scale3; 
	unsigned int hits, hits1, hits2, hits3; 
	unsigned int gates1, gates2, gates3; 

	unsigned int errors = 0; 
	unsigned int errors1, errors2, errors3; 
	errors1 = errors2 = errors3 = 0;

	int	PIRAQadjustAmplitude = 0; 
	int	PIRAQadjustFrequency = 1; 

	// external timer card:
	TIMER ext_timer; // structure defining external timer parameters 
	unsigned int julian_day; 

	// getsockopt parameters, initialization: 
	int iError;

	// initialize MFC and print and error on failure
	if (!AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0))
	{
		// TODO: change error code to suit your needs
		cerr << _T("Fatal Error: MFC initialization failed") << endl;
		nRetCode = 1;
	}

	// open debug data file: 
	db_fp = fopen("debug.dat","w");
	fprintf(db_fp,"CP2exec.exe results:\n");  

	printf("CP2exec: usage -- \n"); 
	printf("         default output localhost\n"); 
	printf("         default outport 3100\n"); 
	printf("         parameters --\n"); 
	printf("         <DSP filename> <received data format: 'f' floats, 's' unsigned shorts>\n"); 
	if (argc > 1) { // entered a filename 
		if ((dspEXEC = fopen(argv[1],"r")) == NULL) // DSP executable file not found 
		{ printf("Usage: %s <DSP filename> DSP executable file not found\n", argv[0]); exit(0); 
		} 
		cmdline_filename = TRUE; 
		fclose(dspEXEC); // existence test passed; use command-line filename
	}
	printf("file %s will be loaded into piraq\n", argv[1]); 
	if (argc > 2) { // entered a format
		dspl_format = toupper(*argv[2]); 
		if (dspl_format == 'F') { // display short data 
			printf("FLOAT format specified for piraq received data.\n"); 
			printf("data displayed every fifo hit\n"); 
			dspl_hits = 10; // was 10 
		} 
		else { // no data display
			printf("NO DISPLAY of piraq received data\n"); 
		} 
	}
	if (argc > 3) { // entered a piraq BOARD-enable mask (0-7)
		piraqs = atoi(argv[3]); 
		printf("piraq select mask = %d\n", piraqs); 
	}
	if (argc > 4) { // entered a filename
		strcpy(fname1, argv[4]);
		strcpy(fname2, argv[4]);
		strcpy(fname3, argv[4]);
	}
	else {
		strcpy(fname1, "config1");	printf(" config1 filename %s will be used\n", fname1); 
		strcpy(fname2, "config2");	printf(" config2 filename %s will be used\n", fname2); 
		strcpy(fname3, "config3");	printf(" config3 filename %s will be used\n", fname3); 
	}

	printf("\n\nTURN TRANSMITTER OFF for piraq dc offset measurement.\nPress any key to continue.\n"); 
	while(!kbhit())	;
	c = toupper(getch()); // get the character

	// this if/else wraps around the entire main: else {...} contains single-board operation. 
	if (piraqs) { // entered slot pattern: bit set runs board, right-to-left 
		// (right board most significant bit). 
		// use defaults: outport = 3100 outsock opens "localhost"
		outport = 3100; 
		//outport = 21010; //!CP2Scope direct!
		iError = 0; WSASetLastError(iError);
		iError = WSAGetLastError ();
		printf("set/get iError = %d\n",iError);
		//as it was:	if((outsock = open_udp_out("localhost")) ==  ERROR)			/* open one socket */
		int	send_one = 1;

		// open sockets
		if((outsock1 = open_udp_out("192.168.3.255")) ==  ERROR)			/* open one socket */
		{
			printf("%s: Could not open output socket 1\n",name); 
			exit(0);
		}
		printf("udp socket opens; outsock1 = %d\n", outsock1); 
		if((outsock2 = open_udp_out("192.168.3.255")) ==  ERROR)			/* open second socket */
		{
			printf("%s: Could not open output socket 2\n",name); 
			exit(0);
		}
		printf("udp socket opens; outsock2 = %d\n", outsock2); 
		if((outsock3 = open_udp_out("192.168.3.255")) ==  ERROR)			/* open second socket */
		{
			printf("%s: Could not open output socket 3\n",name); 
			exit(0);
		}
		printf("udp socket opens; outsock3 = %d\n", outsock3); 

		iError = WSAGetLastError ();
		printf("open_udp_out(): iError = %d\n",iError);
		printf("set/get iError = %d\n",iError);

		//eof_start_over: 
		timer_stop(&ext_timer); // stop timer card 
		//		config1 = new CONFIG; config2 = new CONFIG; config3 = new CONFIG; 

		piraq1 = new PIRAQ;

		readconfig(fname1,config1);   /* read in fname.dsp, or use configX.dsp if NULL passed. set up all parameters */ 
		readconfig(fname2,config2);    
		readconfig(fname3,config3);   
		if (config1->dataformat == 18) { // CP2 Timeseries 
			bytespergate = 2 * sizeof(float); 
			// CP2: compute #hits combined into one PCI Bus transfer
			Nhits = 65536 / (HEADERSIZE + (config1->gatesa * bytespergate) + BUFFER_EPSILON); 
			if	(Nhits % 2)	//	computed odd #hits
				Nhits--;	//	make it even
		}
		else	{	//	no other dataformats supported
			printf("dataformat != 18 not supported\n"); exit(0); 
		}

		r_c = piraq1->Init(PIRAQ_VENDOR_ID,PIRAQ_DEVICE_ID); 
		printf("piraq1->Init() r_c = %d\n", r_c); 
		if (r_c == -1) { // how to use GetErrorString() 
			char errmsg[256]; 
			piraq1->GetErrorString(errmsg); printf("error: %s\n", errmsg); 
			piraqs &= ~0x0001; goto nop1; 
		}

		/* put the DSP into a known state where HPI reads/writes will work */
		piraq1->ResetPiraq(); // !!!redundant? 
		piraq1->GetControl()->UnSetBit_StatusRegister0(STAT0_SW_RESET);
		Sleep(1);
		piraq1->GetControl()->SetBit_StatusRegister0(STAT0_SW_RESET);
		Sleep(1);
		printf("piraq1 reset\n"); 
		unsigned int EPROM1[128]; 
		piraq1->ReadEPROM(EPROM1);
		for(y = 0; y < 16; y++) {
			printf("%08lX %08lX %08lX %08lX\n",EPROM1[y*4],EPROM1[y*4+1],EPROM1[y*4+2],EPROM1[y*4+3]); 
		}

		// if board 1 selected: 
		if (piraqs & 0x01) { // turn on slot 1
			piraq1->SetCP2PIRAQTestAction(SEND_CHA);	//	send CHA by default; SEND_COMBINED after dynamic-range extension implemented 
			stop_piraq(config1, piraq1);
			fifo1 = (FIFO *)piraq1->GetBuffer(); 

			// CP2: data packets sized at runtime.  + BUFFER_EPSILON
			piraq_fifo_init(fifo1,"/PRQDATA", HEADERSIZE, Nhits * (HEADERSIZE + (config1->gatesa * bytespergate) + BUFFER_EPSILON), PIRAQ_FIFO_NUM); 
			printf("hit size = %d computed Nhits = %d\n", (HEADERSIZE + (config1->gatesa * bytespergate) + BUFFER_EPSILON), Nhits); 

			if (!fifo1) { printf("piraq1 fifo_create failed\n"); exit(0);
			}
			printf("fifo1 = %p, recordsize = %d\n", fifo1, fifo1->record_size); 
			pkt1 = (PACKET *)fifo_get_header_address(fifo1); 
			pkt1->cmd.flag = 0; // Preset the flags just in case
			pn_pkt = pkt1; // set live packet pointer for subsequent UNIX-epoch pulsenumber calculation
			pkt1->data.info.channel = 0;			// set BOARD number
			struct_init(&pkt1->data.info, fname1);   /* initialize the info structure */
			r_c = piraq1->LoadDspCode(argv[1]); // load entered DSP executable filename
			printf("loading %s: piraq1->LoadDspCode returns %d\n", argv[1], r_c);  
			timerset(config1, piraq1); // !note: also programs pll and FIR filter. 
			printf("Opening FIFO /CMD1......"); 
			cmd1 = fifo_create("/CMD1",0,HEADERSIZE,CMD_FIFO_NUM);
			if(!cmd1)	{printf("\nCannot open /CMD1 FIFO buffer\n"); exit(-1);}
			cmd1->port = CMD_RING_PORT+1;
			printf("   done.\n");
			/* make sure command socket is last file descriptor opened */
			if((cmd1_notifysock = open_udp_in(cmd1->port)) == ERROR) { /* open the input socket on port where data will come from */
				printf("%s: Could not open piraq1 notification socket\n",argv[0]); exit(-1);
			}
			printf("cmd1_notifysock = %d \n", cmd1_notifysock); 
			piraq1->SetCP2PIRAQTestAction(SEND_CHA);	//	send CHA by default; SEND_COMBINED after dynamic-range extension implemented 
		} // end if board 1 selected: 
nop1:
		piraq2 = new PIRAQ;

		r_c = piraq2->Init(PIRAQ_VENDOR_ID,PIRAQ_DEVICE_ID);
		printf("piraq2->Init() r_c = %d\n", r_c);
		if (r_c == -1) { 
			char errmsg[256]; 
			piraq2->GetErrorString(errmsg); printf("error: %s\n", errmsg); 
			piraqs &= ~0x0002; goto nop2; 
		} 
		piraq2->ResetPiraq(); 
		piraq2->GetControl()->UnSetBit_StatusRegister0(STAT0_SW_RESET);
		Sleep(1);
		piraq2->GetControl()->SetBit_StatusRegister0(STAT0_SW_RESET);
		Sleep(1);
		printf("piraq2 reset\n"); 
		unsigned int EPROM2[128]; 
		piraq2->ReadEPROM(EPROM2);
		for(y = 0; y < 16; y++) {
			printf("%08lX %08lX %08lX %08lX\n",EPROM2[y*4],EPROM2[y*4+1],EPROM2[y*4+2],EPROM2[y*4+3]); 
		}
		// if board 2 selected: 
		if (piraqs & 0x02) { // turn on slot 2
			stop_piraq(config2, piraq2);
			fifo2 = (FIFO *)piraq2->GetBuffer(); 

			// CP2: data packets sized at runtime. 
			piraq_fifo_init(fifo2,"/PRQDATA", HEADERSIZE, Nhits * (HEADERSIZE + (config2->gatesa * bytespergate) + BUFFER_EPSILON), PIRAQ_FIFO_NUM); 

			if (!fifo2)	{
				printf("piraq2 fifo_create failed\n");      exit(0);
			}
			printf("fifo2 = %p, recordsize = %d\n", fifo2, HEADERSIZE + (config2->gatesa * bytespergate) + BUFFER_EPSILON); 
			pkt2 = (PACKET *)fifo_get_header_address(fifo2);
			pkt2->cmd.flag = 0; // Preset the flags just in case
			pn_pkt = pkt2; // set live packet pointer for subsequent UNIX-epoch pulsenumber calculation
			pkt2->data.info.channel = 1;			// set BOARD number
			struct_init(&pkt2->data.info, fname2);   /* initialize the info structure */
			r_c = piraq2->LoadDspCode(argv[1]); // load entered DSP executable filename
			printf("loading %s: piraq2->LoadDspCode returns %d\n", argv[1], r_c);  
			timerset(config2, piraq2); // !note: also programs pll and FIR filter. 
			printf("Opening FIFO /CMD2......"); 
			cmd2 = fifo_create("/CMD2",0,HEADERSIZE,CMD_FIFO_NUM);
			if(!cmd2) {
				printf("\nCannot open /CMD2 FIFO buffer\n"); exit(-1);
			}
			cmd2->port = CMD_RING_PORT+2;
			printf("   done.\n");
			/* make sure command socket is last file descriptor opened */
			if((cmd2_notifysock = open_udp_in(cmd2->port)) == ERROR) {	/* open the input socket on port where data will come from */
				printf("%s: Could not open piraq2 notification socket\n",argv[0]); exit(-1);
			}
			printf("cmd2_notifysock = %d \n", cmd2_notifysock); 
			piraq2->SetCP2PIRAQTestAction(SEND_CHA);	//	send CHA by default; SEND_COMBINED after dynamic-range extension implemented 
		} // end if board 2 selected: 
nop2: 
		piraq3 = new PIRAQ;

		r_c = piraq3->Init(PIRAQ_VENDOR_ID,PIRAQ_DEVICE_ID);
		printf("piraq3->Init() r_c = %d\n", r_c); 
		if (r_c == -1) { // how to use GetErrorString() 
			char errmsg[256]; 
			piraq3->GetErrorString(errmsg); printf("error: %s\n", errmsg); 
			piraqs &= ~0x0004; goto nop3; 
		} 
		/* put the DSP into a known state where HPI reads/writes will work */
		piraq3->ResetPiraq(); // !!!redundant? 
		piraq3->GetControl()->UnSetBit_StatusRegister0(STAT0_SW_RESET);
		Sleep(1);
		piraq3->GetControl()->SetBit_StatusRegister0(STAT0_SW_RESET);
		Sleep(1);
		printf("piraq3 reset\n"); 
		unsigned int EPROM3[128]; 
		piraq3->ReadEPROM(EPROM3);
		for(y = 0; y < 16; y++) {
			printf("%08lX %08lX %08lX %08lX\n",EPROM3[y*4],EPROM3[y*4+1],EPROM3[y*4+2],EPROM3[y*4+3]); 
		}
		// if board 3 selected: 
		if (piraqs & 0x04) { // turn on slot 3
			stop_piraq(config3, piraq3);
			fifo3 = (FIFO *)piraq3->GetBuffer(); 
			// CP2: data packets sized at runtime. 
			piraq_fifo_init(fifo3,"/PRQDATA", HEADERSIZE, Nhits * (HEADERSIZE + (config3->gatesa * bytespergate) + BUFFER_EPSILON), PIRAQ_FIFO_NUM); 
			if (!fifo3) {
				printf("piraq3 fifo_create failed\n");      exit(0);
			}
			printf("fifo3 = %p, recordsize = %d\n", fifo3, HEADERSIZE + (config3->gatesa * bytespergate) + BUFFER_EPSILON); 
			pkt3 = (PACKET *)fifo_get_header_address(fifo3);
			pkt3->cmd.flag = 0; // Preset the flags just in case
			pn_pkt = pkt3; // set live packet pointer for subsequent UNIX-epoch pulsenumber calculation
			pkt3->data.info.channel = 2;			// set BOARD number
			struct_init(&pkt3->data.info, fname3);   /* initialize the info structure */
			r_c = piraq3->LoadDspCode(argv[1]); // load entered DSP executable filename
			printf("loading %s: piraq3->LoadDspCode returns %d\n", argv[1], r_c);  
			timerset(config3, piraq3); // !note: also programs pll and FIR filter. 
			printf("Opening FIFO /CMD3......"); 
			cmd3 = fifo_create("/CMD3",0,HEADERSIZE,CMD_FIFO_NUM);
			if(!cmd3)	{printf("\nCannot open /CMD3 FIFO buffer\n"); exit(-1);}
			cmd3->port = CMD_RING_PORT+3;
			printf("   done.\n");
			/* make sure command socket is last file descriptor opened */
			if((cmd3_notifysock = open_udp_in(cmd3->port)) == ERROR)	/* open the input socket on port where data will come from */
			{printf("%s: Could not open piraq3 notification socket\n",argv[0]); exit(-1);
			}
			printf("cmd3_notifysock = %d \n", cmd3_notifysock); 
			piraq3->SetCP2PIRAQTestAction(SEND_CHA);	//	send CHA by default; SEND_COMBINED after dynamic-range extension implemented 
		} // end if board 3 selected
nop3:
		// next section waits for PPS edge and then starts external timer card
		// get time, then wait for new second
		time_t now, now_was; 
		__int64 pulsenum, beamnum; 
		float prt = pn_pkt->data.info.prt[0];   // get PRTs from valid packet as determined above. 
		float prt2 = pn_pkt->data.info.prt[1]; 

		//		if (pn_pkt->data.info.dataformat == 17) // staggered PRT
		//			prt += prt2; 
		// from singlepiraq now041505.cpp: 
		if (pn_pkt->data.info.dataformat == 17) { // staggered PRT
			prt += prt2; 
			prt /= 2.0; // staggered PRT: two hits per combined prt
			printf("STAGGER: combined prt = %+8.3e\n", prt); 
		}

		__int64 prf; 
		prf = (__int64)(ceil((double)(1/prt)));
		unsigned int pri; 
		printf("pn_pkt->data.info.prt[0] = %f (float)(1/prt) = %+8.3e\n", pn_pkt->data.info.prt[0], (float)(1/prt)); 
		// fp: 
		double fpprf, fpsuppm; 
		fpprf = ((double)1.0)/((double)prt); 
		//!board-specific for 2,3
		hits = config1->hits; 
		fpsuppm = fpprf/(double)hits; 
		//		fpExactmSecperBeam = ((double)1000.0)/fpsuppm; 
		printf("prt = %+8.3e fpprf = %+8.3e fpsuppm = fpprf/hits = %+8.3e\n", prt, fpprf, fpsuppm); 
		//		printf("fpExactmSecperBeam = %8.3e\n", fpExactmSecperBeam); 

		pri = (unsigned int)(((float)COUNTFREQ)/(float)(1/prt)) + 0.5; 
		printf("pri = %d\n", pri); 
		printf("prf = %I64d\n", prf); 
		printf("prt2 = %8.3e\n", pn_pkt->data.info.prt[1]); 
		//!board-specific for 2,3
		hits = config1->hits; 
#if 1
		printf("hits = %d\n", hits); 
		float suppm; suppm = prf/(float)hits; 
		printf("ceil(prf/hits) = %4.5f, prf/hits = %4.5f\n", ceil(prf/(float)hits), suppm); 
		//		if (ceil(prf/hits) != suppm) {
		//			printf("integral beams/sec required\n"); exit(0); 
		//		} 
#endif
#ifdef NO_INTEGER_BEAMS
		goto no_int_beams; 
#endif
		//		BeamsperSec = (unsigned int)prf/hits; 
		//		mSecperBeam = 1000/BeamsperSec; 
		//		ExactmSecperBeam = 1000.0/suppm; 
		//		printf("ExactmSecperBeam = %4.3f\n", ExactmSecperBeam); 
		//	use integer implementation until full f.p. method is designed: 2-7-05 mp. 
		//		printf("current implementation requires integer #beams/sec\n"); 
		//		if (ceil(ExactmSecperBeam) != mSecperBeam) {
		//			printf("integral milliseconds per beam required\n"); exit(0); 
		//		} 
		//		printf("BeamsperSec = %d mSecperBeam = %d\n", BeamsperSec, mSecperBeam); 
		// get current second and wait for it to pass; 
		now = time(&now); now_was = now;
		while(now == now_was) // current second persists 
			now = time(&now);	//  
		now = time(&now); now_was = now;
		//		_ftime( &timebuffer );
		//		SystemEpochMillisec = (timebuffer.time * (__int64)1000) + (__int64)timebuffer.millitm;
		//		printf("before piraq start():\nSystemEpochMillisec = %I64d\n", SystemEpochMillisec); 
		printf("now WILL BE %I64d\n", timebuffer.time + (__int64)2); 
		pulsenum = ((((__int64)(now+2)) * (__int64)COUNTFREQ) / pri) + 1; 
		beamnum = pulsenum / hits;
		//beamnum += (__int64)(TimerStartCorrection/mSecperBeam); // cannot do this here ... do at interpolation time
		printf("pulsenum=%I64d\n", pulsenum); 
		printf("beamnum=%I64d\n", beamnum); 
		// start the piraqs, waiting for each to indicate functionality: 
		if (piraqs & 0x01) { // turn on slot 1
#if 1	// 0: restore previous beamnum method
			pkt1->data.info.pulse_num = pulsenum;	// set UNIX epoch pulsenum just before starting
			pkt1->data.info.beam_num = beamnum; 
			pkt1->data.info.packetflag = 1;			// set to piraq: get header! 
			printf("board%d: receiver_gain = %4.2f vreceiver_gain = %4.2f \n", pkt1->data.info.channel, pkt1->data.info.receiver_gain, pkt1->data.info.vreceiver_gain); 
			printf("board%d: noise_power = %4.2f vnoise_power = %4.2f \n", pkt1->data.info.channel, pkt1->data.info.noise_power, pkt1->data.info.vnoise_power); 
#endif
			if (!start(config1,piraq1,pkt1)) 		  /* start the PIRAQ: also points the piraq to the fifo structure */ 
			{printf("\npiraq1 DSP program not ready: pkt1->cmd.flag != TRUE (1)\n"); exit(-1);}
		} 
		if (piraqs & 0x02) { // turn on slot 2
#if 1	// 0: restore previous beamnum method
			pkt2->data.info.pulse_num = pulsenum;	// set UNIX epoch pulsenum just before starting
			pkt2->data.info.beam_num = beamnum; 
			pkt2->data.info.packetflag = 1;			// set to piraq: get header! 
			printf("board%d: receiver_gain = %4.2f vreceiver_gain = %4.2f \n", pkt2->data.info.channel, pkt2->data.info.receiver_gain, pkt2->data.info.vreceiver_gain); 
			printf("board%d: noise_power = %4.2f vnoise_power = %4.2f \n", pkt2->data.info.channel, pkt2->data.info.noise_power, pkt2->data.info.vnoise_power); 
#endif
			if (!start(config2,piraq2,pkt2)) 		  /* start the PIRAQ: also points the piraq to the fifo structure */ 
			{printf("\npiraq2 DSP program not ready: pkt2->cmd.flag != TRUE (1)\n"); exit(-1);}
		} 
		if (piraqs & 0x04) { // turn on slot 3
#if 1	// 0: restore previous beamnum method
			pkt3->data.info.pulse_num = pulsenum;	// set UNIX epoch pulsenum just before starting
			pkt3->data.info.beam_num = beamnum; 
			pkt3->data.info.packetflag = 1;			// set to piraq: get header! 
			printf("board%d: receiver_gain = %4.2f vreceiver_gain = %4.2f \n", pkt3->data.info.channel, pkt3->data.info.receiver_gain, pkt3->data.info.vreceiver_gain); 
			printf("board%d: noise_power = %4.2f vnoise_power = %4.2f \n", pkt3->data.info.channel, pkt3->data.info.noise_power, pkt3->data.info.vnoise_power); 
#endif
			if (!start(config3,piraq3,pkt3)) 		  /* start the PIRAQ: also points the piraq to the fifo structure */ 
			{printf("\npiraq3 DSP program not ready: pkt3->cmd.flag != TRUE (1)\n"); exit(-1);}
		} 
		// get current second and wait for it to pass; 
		now = time(&now); now_was = now;
		while(now == now_was) // current second persists 
			now = time(&now);	//  
		now = time(&now); now_was = now;
		// compute pulsenumber when data acquisition will begin: 
		// increment current second to account for wait following
		pulsenum = ((((__int64)(now+1)) * (__int64)COUNTFREQ) / pri) + 1; 
		beamnum = pulsenum / hits;  
		printf("now+1=%d: data collection starts THEN.\n... waiting for this second (now) to expire ... \npulsenumber computed using NEXT second, when timer board is triggered \n", now+1);  
		printf("now=%d\n", now);  
		printf("pri = %d\n", pri+1); 
		printf("pulsenum=%I64d\n", pulsenum); 
		printf("beamnum=%I64d\n", beamnum); 
		// set pulsenum, beamnum computed from PPS-edge second in all boards requested and started;  
#if 0	// 1: restore previous method
		if (piraqs & 0x01) { // piraq1 selected 
			pkt1->data.info.pulse_num = pulsenum;	// set UNIX epoch pulsenum just before starting
			pkt1->data.info.beam_num = beamnum; 
			pkt1->data.info.packetflag = 1;			// set to piraq: get header! 
			pkt1->data.info.channel = 0;			// set BOARD number
		} 
		if (piraqs & 0x02) { // piraq2 selected 
			pkt2->data.info.pulse_num = pulsenum; // set UNIX epoch pulsenum just before starting
			pkt2->data.info.beam_num = beamnum; 
			pkt2->data.info.packetflag = 1;			// set to piraq: get header! 
			pkt2->data.info.channel = 1;			// set BOARD number
		} 
		if (piraqs & 0x04) { // piraq3 selected 
			pkt3->data.info.pulse_num = pulsenum; // set UNIX epoch pulsenum just before starting
			pkt3->data.info.beam_num = beamnum; 
			pkt3->data.info.packetflag = 1;			// set to piraq: get header! 
			pkt3->data.info.channel = 2;			// set BOARD number
		}
#endif
		// time-related parameters initialized -- wait for new second then start timer card and 
		// data acquisition. 
		printf("now=%d: ... still waiting for this second (now) to expire ... then timer will start\n\n", now);  
		while(now == now_was) // current second persists 
			now = time(&now); 
		printf("now=%d: ... now_was=%d\n", now, now_was);  
		// start timer board immediately:
		// !note: operator must synchronize system clock w/Epsilon so PPS edge happens on second change. 
		// !clarify: ? start timer card BEFORE ? does PPS edge require start_timer_card already executed?
		/**
		_ftime( &timebuffer );
		timeline = ctime( & ( timebuffer.time ) );
		printf( "mSec=%hu\n", timebuffer.millitm );
		start_timer_card(&ext_timer, &pn_pkt->data.info); // pn_pkt = PACKET pointer to a live piraq 

		_ftime( &timebuffer );
		timeline = ctime( & ( timebuffer.time ) );
		printf( "timer start correction mSec=%hu\n", timebuffer.millitm );
		TimerStartCorrection = timebuffer.millitm;

		_ftime( &timebuffer );
		SystemEpochMillisec = (timebuffer.time * (__int64)1000) + (__int64)timebuffer.millitm;
		RadarEpochMillisec = mSecperBeam * pn_pkt->data.info.beam_num; // pn_pkt: access a live packet
		//!!!LATER:RadarEpochMillisec = (__int64)(ExactmSecperBeam * ((float)(pn_pkt->data.info.beam_num))); // pn_pkt: access a live packet; (double)?

		_ftime( &timebuffer );
		printf("1:\nRadarEpochMillisec  = %I64d \nSystemEpochMillisec = %I64d\n", RadarEpochMillisec, SystemEpochMillisec); 

		fpSystemMillisec = ((double)timebuffer.time * 1000.0) + (double)timebuffer.millitm;
		fpRadarMillisec = fpExactmSecperBeam * (double)pn_pkt->data.info.beam_num; // pn_pkt: access a live packet
		printf("1:\nfpRadarMillisec          = %+8.12e\n", fpRadarMillisec); 
		printf("1:\nfpSystemMillisec         = %+8.12e\n", fpSystemMillisec); 
		fpRadarSystemCorrection = (fpSystemMillisec - (double)TimerStartCorrection) - fpRadarMillisec; 
		printf("1:\nfpRadarSystemCorrection  = %+8.5e\n", fpRadarSystemCorrection); 
		printf("1:\nfpRadarMillisecCorrected = %+8.12e\n", fpRadarMillisec + fpRadarSystemCorrection); 
		RadarEpochMillisec = (__int64)(fpRadarMillisec + fpRadarSystemCorrection); 
		**/
		// all running -- get data!
		testnum = 0;  fifo1_hits = 0; fifo2_hits = 0; fifo3_hits = 0; // 
		seq1 = seq2 = seq3 = 0; // initialize sequence# for each channel 
		while(1) { // until 'q' 
			julian_day = get_julian_day(); 
			if (piraqs & 0x01) { // turn on slot 1
				tv1.tv_sec = 0;
				tv1.tv_usec = 1000; 
				/* do polling */
				FD_ZERO(&rfd1);
				FD_SET(cmd1_notifysock, &rfd1); //!!!Mitch eliminated but while(1) below depends on it. 
				val1 = select(cmd1_notifysock + 1, &rfd1, 0, 0, &tv1);
				//val1 = 0; // force it
				//val1 = select(cmd1_notifysock, &rfd1, 0, 0, &tv1); // eliminate + 1
				if (val1 < 0) 
				{printf( "select1 val = 0x%x\n", val1 );  continue;}

				else if(val1 == 0) { /* use time out for polling */
					// PIRAQ1:
					// take CYCLE_HITS beams from piraq:
					while(((cur_fifo1_hits = fifo_hit(fifo1)) > 0) && (cycle_fifo1_hits < CYCLE_HITS)) { // fifo1 hits ready: save #hits pending 
						//if ((cur_fifo1_hits % 5) == 0) { printf("b0: hits1 = %d\n", cur_fifo1_hits); } // print often enough ... NCAR-Radar-DRX
						//if (((cur_fifo1_hits % 2) == 0) || (cur_fifo1_hits < 2)) { printf("b0: hits1 = %d\n", cur_fifo1_hits); } // print often enough ... on atd-milan
						cycle_fifo1_hits++; 
						fifopiraq1 = (PACKET *)fifo_get_read_address(fifo1,0); 
#ifdef EOF_DETECT
						if ((int)fifopiraq1->data.info.packetflag == -1) { // piraq detected a hardware out-of-sync condition "EOF" 
							printf("\n\npiraq1 EOF detected. exiting.\n"); 
							if (piraqs & 0x01) { // slot 1 active
								stop_piraq(config1, piraq1); 
							} 
							printf("\n\npiraq1 stopped\n"); 
							if (piraqs & 0x02) { // slot 2 active
								stop_piraq(config2, piraq2); 
							} 
							if (piraqs & 0x04) { // slot 3 active
								stop_piraq(config3, piraq3); 
							} 
							printf("\n\npiraqs stopped\n"); 
							timer_stop(&ext_timer); 
							delete piraq1; delete piraq2; delete piraq3; 
							printf("\n\ngoing to eof_start_over\n"); 
							goto eof_start_over; 
						}
#endif // end #ifdef EOF_DETECT
						hits1 = fifopiraq1->data.info.hits; gates1 = fifopiraq1->data.info.gates; 
						scale1 = (float)(PIRAQ3D_SCALE*PIRAQ3D_SCALE*hits1); // scale fifo1 data 
						udp1 = &fifopiraq1->udp;
						udp1->magic = MAGIC;
						udp1->type = UDPTYPE_PIRAQ_CP2_TIMESERIES; 
						fifopiraq1->data.info.bytespergate = 2 * (sizeof(float)); // Staggered PRT ABPDATA
						fsrc = (float *)fifopiraq1->data.data; 
						// no data scaling in CP2.exe

						temp1 = fifopiraq1->data.info.pulse_num * (unsigned __int64)(prt * (float)COUNTFREQ + 0.5);
						fifopiraq1->data.info.secs = temp1 / COUNTFREQ;
						fifopiraq1->data.info.nanosecs = ((unsigned __int64)10000 * (temp1 % ((unsigned __int64)COUNTFREQ))) / (unsigned __int64)COUNTFREQ;
						fifopiraq1->data.info.nanosecs *= (unsigned __int64)100000; // multiply by 10**5 to get nanoseconds
						az1 += 0.5;  
						if(az1 > 360.0) { // full scan 
							az1 -= 360.0; // restart azimuth angle 
							el1 += 7.0;		// step antenna up 
							scan1++; // increment scan count
							if (el1 >= 21.0) {	// beyond allowed step
								el1 = 0.0; // start over at horizon
								volume1++; // finish volume
							}
						} 
						fifopiraq1->data.info.az = az1;  
						fifopiraq1->data.info.el = el1; // set in packet 
						fifopiraq1->data.info.scan_num = scan1;
						fifopiraq1->data.info.vol_num = volume1;  
						//					printf("fake: az = %4.3f el = %4.3f\n",fifopiraq1->data.info.az,fifopiraq1->data.info.el); 
						// ... to here 
#ifdef TIME_TESTING	
						// for mSec-resolution time tests: 
						_ftime( &timebuffer );
						timeline = ctime( & ( timebuffer.time ) );
						printf( "1: %hu ... fakin it.\n", timebuffer.millitm );
#endif
						fifopiraq1->data.info.julian_day = julian_day; 

#ifdef	DRX_PACKET_TESTING	// define to activate data packet resizing for throughput testing. 
						//					fifopiraq1->data.info.recordlen = (fifopiraq1->data.info.clutter_end[0])*fifopiraq1->data.info.gates*24 + (RECORDLEN(fifopiraq1)); // vary multiplier; note CPU usage in Task Manager. 24 = 2*bytespergate. 
						fifopiraq1->data.info.recordlen = Nhits*(RECORDLEN(fifopiraq1)+BUFFER_EPSILON); /* this after numgates corrected */
#else
						fifopiraq1->data.info.recordlen = RECORDLEN(fifopiraq1); /* this after numgates corrected */
#endif
						// scale data -- offload from piraq: 
						fsrc = (float *)fifopiraq1->data.data; 

#if 1	// test N-hit packet: 0: reduce printing
						__int64 * __int64_ptr, * __int64_ptr2; unsigned int * uint_ptr; float * fsrc2; 
						for (i = 0; i < Nhits; i++) { // all hits in the packet 
							// compute pointer to datum in an individual hit, dereference and print. 
							// CP2 PCI Bus transfer size: Nhits * (HEADERSIZE + (config1->gatesa * bytespergate) + BUFFER_EPSILON)
							__int64_ptr = (__int64 *)((char *)&fifopiraq1->data.info.beam_num + i*((HEADERSIZE + (config1->gatesa * bytespergate) + BUFFER_EPSILON))); 
							beamnum = *__int64_ptr; 
							__int64_ptr2 = (__int64 *)((char *)&fifopiraq1->data.info.pulse_num + i*((HEADERSIZE + (config1->gatesa * bytespergate) + BUFFER_EPSILON))); 
							pulsenum = *__int64_ptr2; 
							uint_ptr = (unsigned int *)((char *)&fifopiraq1->data.info.hits + i*((HEADERSIZE + (config1->gatesa * bytespergate) + BUFFER_EPSILON))); 
							j = *uint_ptr; 
							uint_ptr = (unsigned int *)((char *)&fifopiraq1->data.info.channel + i*((HEADERSIZE + (config1->gatesa * bytespergate) + BUFFER_EPSILON))); 
							k = *uint_ptr; 
							fsrc2 = (float *)((char *)fifopiraq1->data.data + i*((HEADERSIZE + (config1->gatesa * bytespergate) + BUFFER_EPSILON)));
							//printf("hit%d: %+8.3e %+8.3e %+8.3e %+8.3e \n", i, *(fsrc2+0), *(fsrc2+1), *(fsrc2+2), *(fsrc2+3)); 
							//printf("hit%d: %+8.6e %+8.6e %+8.3e %+8.3e \n", i, *(fsrc2+0), *(fsrc2+1), *(fsrc2+2), *(fsrc2+3)); 
							//printf("hit%d: %+8.3e %+8.3e %+8.3e %+8.3e \n", i, *(fsrc2+4), *(fsrc2+5), *(fsrc2+6), *(fsrc2+7)); 
							//printf("hit%d: %+8.3e\n", i, *(fsrc2+0)); 
							//printf("hit%d: %d\n", i, fifopiraq1->cmd.arg[1]); 
							//if (i < 2) { printf("hit%d: %+8.3e\n", i, *(fsrc2+0)); }	//	reduced printing

							if (lastpulsenumber1 != pulsenum - 1) { // PNs not sequential
								printf("hit%d: lastPN = %I64d PN = %I64d\n", i+1, lastpulsenumber1, pulsenum);  PNerrors1++; 
								fprintf(db_fp, "%d:hit%d: lastPN = %I64d PN = %I64d\n", fifopiraq1->data.info.channel, i+1, lastpulsenumber1, pulsenum); 
							}
							//if ((testnum % 100) == 0) *__int64_ptr2++; //	!test flub the pulsenumber
							// test beamnumbers sequential
							//				if (beamnum != pulsenum / j) { // BN != PN/hits
							//					printf("hit%d: computed BN1 = %I64d BN1 = %I64d PN1 = %I64d\n", i+1, pulsenum / j, beamnum, pulsenum); 
							//				} 
							// print data once per fifo hit -- per N-hit packet: 
							//				if (i == 0) { // 
							//					printf("hit%d: BN = %I64d PN = %I64d hits = %d gates = %d fsrc2[0] = %+8.3f\n", i+1, beamnum, pulsenum, j, k, *fsrc2); 
							//				} 
							lastpulsenumber1 = pulsenum; // previous hit PN
							//					printf("hit%d: BN = %I64d PN = %I64d hits = %d gates = %d fsrc2[6] = %+8.3f\n", i+1, beamnum, pulsenum, j, k, *fsrc2); 
							//					printf("%d: hit%02d: BN = %I64d PN = %I64d fsrc2[0] = %+8.3f fsrc2[1] = %+8.3f fsrc2[2] = %+8.3f fsrc2[end] = %+8.3f\n", k, i+1, beamnum, pulsenum, *fsrc2, fsrc2[1], fsrc2[2], fsrc2[(2*fifopiraq1->data.info.gates)-1]); 
						}
						//				printf("\n"); 
#endif
						if ((testnum % dspl_hits) == 0) { // done dspl_hit cycles
							if (dspl_format == 'F') { // display short data
								pcorrect = 1.0; //(float)fifopiraq1->data.info.hits; //pow(10., 0.1*fifopiraq1->data.info.data_sys_sat);
								fsrc = (float *)fifopiraq1->data.data;
								gates_gg = (int)fifopiraq1->data.info.gates*6-6;
								//printf("0:TOTALSIZE = %d fsrc[0] = %+8.3f fsrc[1] = %+8.3f fsrc[2] = %+8.3f fsrc[%d] = %+8.3f testnum = %d\n", TOTALSIZE(fifopiraq1), fsrc[0], fsrc[1], fsrc[2], (2*fifopiraq1->data.info.gates)-1, fsrc[(2*fifopiraq1->data.info.gates)-1], testnum); 
								//printf("\n"); 
								//printf("fsrc2[5] = %+8.3f\n", *fsrc2); 
								//printf("0:TOTALSIZE = %d CurPkt hits = %d CurPkt channel = %d testnum = %d\n", TOTALSIZE(fifopiraq1), fifopiraq1->data.info.clutter_start[0], fifopiraq1->data.info.clutter_end[0], testnum); 
								//printf("                 ts_start = %d ts_end = %d\n", fifopiraq1->data.info.ts_start_gate, fifopiraq1->data.info.ts_end_gate); 
								//printf("                 az = %4.3f el = %4.3f\n",fifopiraq1->data.info.az,fifopiraq1->data.info.el); 
								//							printf("Saturation %8.2f, Pcorrect %8.3f\n", fifopiraq1->data.info.data_sys_sat, pcorrect);
								//							printf("Gate %04d:A0 = %+8.3e B0 = %+8.3e P0 = %+8.3e \n          A1 = %+8.3e B1 = %+8.3e P1 = %+8.3e\n",
								//			  		  0, fsrc[0], fsrc[1], fsrc[2]*pcorrect, fsrc[3], fsrc[4], fsrc[5]*pcorrect); 
							}
						} // end	if ((testnum % ...
						//!!!					printf("fifo1_hits = %d\n",fifo1_hits); 
						testnum++; fifo1_hits++;   
#ifndef DRX_PACKET_TESTING	// define activates data packet resizing for CP2 throughput testing. 
						fifopiraq1->udp.totalsize = TOTALSIZE(fifopiraq1); // ordinary operation
#else	// increase udp-send totalsize to Nhits
						int test_totalsize1 = Nhits*(TOTALSIZE(fifopiraq1) + BUFFER_EPSILON); 
						//works ... int test_totalsize1 = Nhits*(TOTALSIZE(fifopiraq1) + BUFFER_EPSILON) + 6*sizeof(UDPHEADER); // add Nhits-1 udpsend adds 1 more ... fuckinay, man
						fifopiraq1->udp.totalsize = test_totalsize1; // CP2 throughput testing
						packets++; 
						if	(packets == 10)	{printf("packet totalsize1 %d\n", test_totalsize1);}
#endif

						seq1 = send_udp_packet(outsock1, outport, seq1, udp1); 
						fifo_increment_tail(fifo1);
					} // end	while(fifo_hit()
					cycle_fifo1_hits = 0; // clear cycle counter 
				} // end	else if(val == 0) 
			}
			// piraq2: 
			if (piraqs & 0x02) { // turn on slot 2
				tv2.tv_sec = 0;
				tv2.tv_usec = 1000; 
				/* do polling */
				FD_ZERO(&rfd2);
				FD_SET(cmd2_notifysock, &rfd2); //!!!Mitch eliminated but while(1) below depends on it.  
				val2 = select(cmd2_notifysock + 1, &rfd2, 0, 0, &tv2);
				if (val2 < 0) 
				{perror( "select" );  continue;}

				else if(val2 == 0) { /* use time out for polling */
					//printf("1:cur_fifo2_hits = %d\n", cur_fifo2_hits); 
					// PIRAQ2:
					// take CYCLE_HITS beams from piraq:
					while(((cur_fifo2_hits = fifo_hit(fifo2)) > 0) && (cycle_fifo2_hits < CYCLE_HITS)) { // fifo1 hits ready: save #hits pending 
						if ((cur_fifo2_hits % 5) == 0) { printf("b1: hits2 = %d\n", cur_fifo2_hits); } // print often enough ... 
						cycle_fifo2_hits++; 
						fifopiraq2 = (PACKET *)fifo_get_read_address(fifo2,0);
#ifdef EOF_DETECT
						if ((int)fifopiraq2->data.info.packetflag == -1) { // piraq detected a hardware out-of-sync condition "EOF" 
							printf("\n\npiraq2 EOF detected. exiting.\n"); 
							if (piraqs & 0x01) { // slot 1 active
								stop_piraq(config1, piraq1); 
							} 
							printf("\n\npiraq1 stopped\n"); 
							if (piraqs & 0x02) { // slot 2 active
								stop_piraq(config2, piraq2); 
							} 
							if (piraqs & 0x04) { // slot 3 active
								stop_piraq(config3, piraq3); 
							} 
							printf("\n\npiraqs stopped\n"); 
							timer_stop(&ext_timer); 
							delete piraq1; delete piraq2; delete piraq3; 
							printf("\n\ngoing to eof_start_over\n"); 
							goto eof_start_over; 
						}
#endif // end #ifdef EOF_DETECT
						hits2 = fifopiraq2->data.info.hits; gates2 = fifopiraq2->data.info.gates; 
						scale2 = (float)(PIRAQ3D_SCALE*PIRAQ3D_SCALE*hits2); // scale fifo2 data
						udp2 = &fifopiraq2->udp;
						udp2->magic = MAGIC;
						udp2->type = UDPTYPE_PIRAQ_CP2_TIMESERIES; 
						fifopiraq2->data.info.bytespergate = 2 * (sizeof(float)); // Staggered PRT ABPDATA
						fsrc = (float *)fifopiraq2->data.data; 
						// no data scaling
						temp2 = fifopiraq2->data.info.pulse_num * (unsigned __int64)(prt * (float)COUNTFREQ + 0.5);
						fifopiraq2->data.info.secs = temp2 / COUNTFREQ;
						fifopiraq2->data.info.nanosecs = ((unsigned __int64)10000 * (temp2 % ((unsigned __int64)COUNTFREQ))) / (unsigned __int64)COUNTFREQ;
						fifopiraq2->data.info.nanosecs *= (unsigned __int64)100000; // multiply by 10**5 to get nanoseconds

						az2 += 0.5;  
						if(az2 > 360.0) { // full scan 
							az2 -= 360.0; // restart azimuth angle 
							el2 += 7.0;		// step antenna up 
							scan2++; // increment scan count
							if (el2 >= 21.0) {	// beyond allowed step
								el2 = 0.0; // start over at horizon
								volume2++; // finish volume
							}
						} 
						fifopiraq2->data.info.az = az2;  
						fifopiraq2->data.info.el = el2; // set in packet 
						fifopiraq2->data.info.scan_num = scan2;
						fifopiraq2->data.info.vol_num = volume2;
						// ... to here 
#ifdef TIME_TESTING	
						// for mSec-resolution time tests: 
						_ftime( &timebuffer );
						timeline = ctime( & ( timebuffer.time ) );
						printf( "2: %hu ... fakin it.\n", timebuffer.millitm );
#endif
						fifopiraq2->data.info.julian_day = julian_day; 

#ifdef	DRX_PACKET_TESTING	// define to activate data packet resizing for throughput testing. 
						//					fifopiraq2->data.info.recordlen = (fifopiraq2->data.info.clutter_end[0])*fifopiraq2->data.info.gates*24 + (RECORDLEN(fifopiraq2)); // vary multiplier; note CPU usage in Task Manager. 24 = 2*bytespergate. 
						fifopiraq2->data.info.recordlen = Nhits*(RECORDLEN(fifopiraq2)+BUFFER_EPSILON); /* this after numgates corrected */
#else
						fifopiraq2->data.info.recordlen = RECORDLEN(fifopiraq2); /* this after numgates corrected */
#endif

						fsrc = (float *)fifopiraq2->data.data; 
#if 1	// test N-hit packet data: test sequential PNs 
						__int64 * __int64_ptr, * __int64_ptr2; unsigned int * uint_ptr; float * fsrc2; 
						for (i = 0; i < Nhits; i++) { // all hits in the packet 
							//				for (i = 0; i < Nhits+1; i++) { // all hits in the packet + 1
							//				for (i = 0; i < 1; i++) { // first few hits in the packet 
							// compute pointer to datum in an individual hit, dereference and print. 
							// CP2 PCI Bus transfer size: Nhits * (HEADERSIZE + (config2->gatesa * bytespergate) + BUFFER_EPSILON)
							__int64_ptr = (__int64 *)((char *)&fifopiraq2->data.info.beam_num + i*((HEADERSIZE + (config2->gatesa * bytespergate) + BUFFER_EPSILON))); 
							beamnum = *__int64_ptr; 
							__int64_ptr2 = (__int64 *)((char *)&fifopiraq2->data.info.pulse_num + i*((HEADERSIZE + (config2->gatesa * bytespergate) + BUFFER_EPSILON))); 
							pulsenum = *__int64_ptr2; 
							uint_ptr = (unsigned int *)((char *)&fifopiraq2->data.info.hits + i*((HEADERSIZE + (config2->gatesa * bytespergate) + BUFFER_EPSILON))); 
							j = *uint_ptr; 
							uint_ptr = (unsigned int *)((char *)&fifopiraq2->data.info.channel + i*((HEADERSIZE + (config2->gatesa * bytespergate) + BUFFER_EPSILON))); 
							k = *uint_ptr; 
							fsrc2 = (float *)((char *)fifopiraq2->data.data + i*((HEADERSIZE + (config2->gatesa * bytespergate) + BUFFER_EPSILON)));
							fsrc2 += 0; // look at THIS datum
							if (lastpulsenumber2 != pulsenum - 1) { // PNs not sequential
								printf("hit%d: lastPN = %I64d PN = %I64d\n", i+1, lastpulsenumber2, pulsenum);  PNerrors2++; 
								fprintf(db_fp, "%d:hit%d: lastPN = %I64d PN = %I64d\n", fifopiraq2->data.info.channel, i+1, lastpulsenumber2, pulsenum); 
							} 
							lastpulsenumber2 = pulsenum; 
							//					printf("hit%d: BN = %I64d PN = %I64d hits = %d gates = %d fsrc2[6] = %+8.3f\n", i+1, beamnum, pulsenum, j, k, *fsrc2); 
							//					printf("%d: hit%02d: BN = %I64d PN = %I64d fsrc2[0] = %+8.3f fsrc2[1] = %+8.3f fsrc2[2] = %+8.3f fsrc2[end] = %+8.3f\n", k, i+1, beamnum, pulsenum, *fsrc2, fsrc2[1], fsrc2[2], fsrc2[(2*fifopiraq2->data.info.gates)-1]); 
						}
						//				printf("\n"); 
#endif
						if ((testnum % dspl_hits) == 0) { // done dspl_hit cycles
							if (dspl_format == 'F') { // display short data 
								fsrc = (float *)fifopiraq2->data.data;
								gates_gg = (int)fifopiraq2->data.info.gates*6-6;

								if (config2->ts_start_gate >= 0) { 
									diagiqsrc = (float *)(((unsigned char *)fifopiraq2->data.data) + fifopiraq2->data.info.gates*fifopiraq2->data.info.bytespergate*sizeof(float));  
									//								printf("cmd.arg[0..4] 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X\n",
									//										fifopiraq2->cmd.arg[0], fifopiraq2->cmd.arg[1], fifopiraq2->cmd.arg[2],
									//										fifopiraq2->cmd.arg[3], fifopiraq2->cmd.arg[4]);
									//								printf("testiqsrc0= 0x%x testiqsrc1= 0x%x testiqsrc2= 0x%x \ntestiqsrc3= 0x%x testiqsrc4= 0x%x testiqsrc5= 0x%x\n",
									//						      		  testiqsrc[0], testiqsrc[1], testiqsrc[2], testiqsrc[3], testiqsrc[4], testiqsrc[5]); 
								}
							} 
						} // end	if ((testnum % ...
						//!!!					printf("fifo2_hits = %d\n",fifo2_hits); 
						testnum++; fifo2_hits++;  
#ifndef DRX_PACKET_TESTING	// define activates data packet resizing for CP2 throughput testing. 
						fifopiraq2->udp.totalsize = TOTALSIZE(fifopiraq2); // ordinary operation
#else	// increase udp-send totalsize to Nhits
						int test_totalsize2 = Nhits*(TOTALSIZE(fifopiraq2) + BUFFER_EPSILON); // 
						fifopiraq2->udp.totalsize = test_totalsize2; // CP2 throughput testing
#endif

						seq2 = send_udp_packet(outsock2, outport + 1, seq2, udp2);  
						fifo_increment_tail(fifo2);
					} // end	while(fifo_hit()
					cycle_fifo2_hits = 0; 
				} // end	else if(val == 0) 
			}// end piraq2	
			// piraq3: 
			if (piraqs & 0x04) { // turn on slot 3
				tv3.tv_sec = 0;
				tv3.tv_usec = 1000; 
				/* do polling */
				FD_ZERO(&rfd3);
				FD_SET(cmd3_notifysock, &rfd3); //!!!Mitch eliminated but while(1) below depends on it.  
				val3 = select(cmd3_notifysock + 1, &rfd3, 0, 0, &tv3);
				if (val3 < 0) 
				{perror( "select" );  continue;}

				else if(val3 == 0) { /* use time out for polling */
					// PIRAQ3: 
					// take CYCLE_HITS beams from piraq:
					while(((cur_fifo3_hits = fifo_hit(fifo3)) > 0) && (cycle_fifo3_hits < CYCLE_HITS)) { // fifo1 hits ready: save #hits pending 
						if ((cur_fifo3_hits % 5) == 0) { printf("b2: hits3 = %d\n", cur_fifo3_hits); } // print often enough ... 
						cycle_fifo3_hits++; 
#if 0
						printf("piraq3:\n"); piraq3->ReadEPROM(EPROM3);
						for(y = 0; y < 16; y++) {
							printf("%08lX %08lX %08lX %08lX\n",EPROM3[y*4],EPROM3[y*4+1],EPROM3[y*4+2],EPROM3[y*4+3]); 
						}
						exit(0); 
#endif
						fifopiraq3 = (PACKET *)fifo_get_read_address(fifo3,0);
#ifdef EOF_DETECT
						if ((int)fifopiraq3->data.info.packetflag == -1) { // piraq detected a hardware out-of-sync condition "EOF" 
							printf("\n\npiraq3 EOF detected. exiting.\n"); 
							if (piraqs & 0x01) { // slot 1 active
								stop_piraq(config1, piraq1); 
							} 
							printf("\n\npiraq1 stopped\n"); 
							if (piraqs & 0x02) { // slot 2 active
								stop_piraq(config2, piraq2); 
							} 
							if (piraqs & 0x04) { // slot 3 active
								stop_piraq(config3, piraq3); 
							} 
							printf("\n\npiraqs stopped\n"); 
							timer_stop(&ext_timer); 
							delete piraq1; delete piraq2; delete piraq3; 
							printf("\n\ngoing to eof_start_over\n"); 
							goto eof_start_over; 
						}
#endif // end #ifdef EOF_DETECT
						hits3 = fifopiraq3->data.info.hits; gates3 = fifopiraq3->data.info.gates; 
						scale3 = (float)(PIRAQ3D_SCALE*PIRAQ3D_SCALE*hits3); // scale fifo3 data
						udp3 = &fifopiraq3->udp;
						udp3->magic = MAGIC;
						udp3->type = UDPTYPE_PIRAQ_CP2_TIMESERIES; 
						fifopiraq3->data.info.bytespergate = 2 * (sizeof(float)); // Staggered PRT ABPDATA
						fsrc = (float *)fifopiraq3->data.data; 
						// no data scaling
						temp3 = fifopiraq3->data.info.pulse_num * (unsigned __int64)(prt * (float)COUNTFREQ + 0.5);
						fifopiraq3->data.info.secs = temp3 / COUNTFREQ;
						fifopiraq3->data.info.nanosecs = (10000 * (temp3 % COUNTFREQ)) / COUNTFREQ;
						fifopiraq3->data.info.nanosecs = ((unsigned __int64)10000 * (temp3 % ((unsigned __int64)COUNTFREQ))) / (unsigned __int64)COUNTFREQ;
						fifopiraq3->data.info.nanosecs *= (unsigned __int64)100000; // multiply by 10**5 to get nanoseconds

						az3 += 0.5;  
						if(az3 > 360.0) { // full scan 
							az3 -= 360.0; // restart azimuth angle 
							el3 += 7.0;		// step antenna up 
							scan3++; // increment scan count
							if (el3 >= 21.0) {	// beyond allowed step
								el3 = 0.0; // start over at horizon
								volume3++; // finish volume
							}
						}
						fifopiraq3->data.info.az = az3;  
						fifopiraq3->data.info.el = el3; // set in packet 
						fifopiraq3->data.info.scan_num = scan3;
						fifopiraq3->data.info.vol_num = volume3; 
						// ... to here 
#ifdef TIME_TESTING	
						// for mSec-resolution time tests: 
						_ftime( &timebuffer );
						timeline = ctime( & ( timebuffer.time ) );
						printf( "3: %hu ... fakin it.\n", timebuffer.millitm );
#endif
						fifopiraq3->data.info.julian_day = julian_day; 
#ifdef	DRX_PACKET_TESTING	// define to activate data packet resizing for throughput testing. 
						//					fifopiraq3->data.info.recordlen = (fifopiraq3->data.info.clutter_end[0])*fifopiraq3->data.info.gates*24 + (RECORDLEN(fifopiraq3)); // vary multiplier; note CPU usage in Task Manager. 24 = 2*bytespergate. 
						fifopiraq3->data.info.recordlen = Nhits*(RECORDLEN(fifopiraq3) + BUFFER_EPSILON); /* this after numgates corrected */
#else
						fifopiraq3->data.info.recordlen = RECORDLEN(fifopiraq3); /* this after numgates corrected */
#endif

						//!					fifopiraq3->data.info.recordlen = RECORDLEN(fifopiraq3); /* this after numgates corrected */
						fsrc = (float *)fifopiraq3->data.data; 
#if 1	// test N-hit packet: 0: reduce printing
						__int64 * __int64_ptr, * __int64_ptr2; unsigned int * uint_ptr; float * fsrc2; 
						for (i = 0; i < Nhits; i++) { // all hits in the packet 
							// compute pointer to datum in an individual hit, dereference and print. 
							// CP2 PCI Bus transfer size: Nhits * (HEADERSIZE + (config3->gatesa * bytespergate) + BUFFER_EPSILON)
							__int64_ptr = (__int64 *)((char *)&fifopiraq3->data.info.beam_num + i*((HEADERSIZE + (config3->gatesa * bytespergate) + BUFFER_EPSILON))); 
							beamnum = *__int64_ptr; 
							__int64_ptr2 = (__int64 *)((char *)&fifopiraq3->data.info.pulse_num + i*((HEADERSIZE + (config3->gatesa * bytespergate) + BUFFER_EPSILON))); 
							pulsenum = *__int64_ptr2; 
							uint_ptr = (unsigned int *)((char *)&fifopiraq3->data.info.hits + i*((HEADERSIZE + (config3->gatesa * bytespergate) + BUFFER_EPSILON))); 
							j = *uint_ptr; 
							uint_ptr = (unsigned int *)((char *)&fifopiraq3->data.info.channel + i*((HEADERSIZE + (config3->gatesa * bytespergate) + BUFFER_EPSILON))); 
							k = *uint_ptr; 
							fsrc2 = (float *)((char *)fifopiraq3->data.data + i*((HEADERSIZE + (config3->gatesa * bytespergate) + BUFFER_EPSILON)));
							fsrc2 += 0; // look at THIS datum
							if (lastpulsenumber3 != pulsenum - 1) { // PNs not sequential
								printf("hit%d: lastPN = %I64d PN = %I64d\n", i+1, lastpulsenumber3, pulsenum); PNerrors3++; 
								fprintf(db_fp, "%d:hit%d: lastPN = %I64d PN = %I64d\n", fifopiraq3->data.info.channel, i+1, lastpulsenumber3, pulsenum); 
							} 
							lastpulsenumber3 = pulsenum; // previous hit PN
							//					printf("hit%d: BN = %I64d PN = %I64d hits = %d gates = %d fsrc2[6] = %+8.3f\n", i+1, beamnum, pulsenum, j, k, *fsrc2); 
							//					printf("%d: hit%02d: BN = %I64d PN = %I64d fsrc2[0] = %+8.3f fsrc2[1] = %+8.3f fsrc2[2] = %+8.3f fsrc2[end] = %+8.3f\n", k, i+1, beamnum, pulsenum, *fsrc2, fsrc2[1], fsrc2[2], fsrc2[(2*fifopiraq3->data.info.gates)-1]); 
							//					printf("hit%d: &BN = 0x%x\n", i+1, __int64_ptr); 
						}
						//				printf("\n"); 
#endif
						if ((testnum % dspl_hits) == 0) { // done dspl_hit cycles
							if (dspl_format == 'F') { // display short data 
								fsrc = (float *)fifopiraq3->data.data;
								gates_gg = (int)fifopiraq3->data.info.gates*6-6;
								//printf("2:TOTALSIZE = %d testnum = %d\n", TOTALSIZE(fifopiraq3), testnum); 
								//printf("                 az = %4.3f el = %4.3f\n",fifopiraq3->data.info.az,fifopiraq3->data.info.el); 
								//					        printf("Gate %04d:A0 = %+8.3e B0 = %+8.3e P0 = %+8.3e \n          A1 = %+8.3e B1 = %+8.3e P1 = %+8.3e\n",
								//					  		  0, fsrc[0], fsrc[1], fsrc[2], fsrc[3], fsrc[4], fsrc[5]); 
								if (config3->ts_start_gate >= 0) { 
									diagiqsrc = (float *)(((unsigned char *)fifopiraq3->data.data) + fifopiraq3->data.info.gates*fifopiraq3->data.info.bytespergate*sizeof(float));  
								}
							} 
						} // end	if ((testnum % ...
						//!!!					printf("fifo3_hits = %d\n",fifo3_hits); 
						testnum++; fifo3_hits++;  
#ifndef DRX_PACKET_TESTING	// define activates data packet resizing for CP2 throughput testing. 
						fifopiraq3->udp.totalsize = TOTALSIZE(fifopiraq3); // ordinary operation
#else	// increase udp-send totalsize to Nhits
						int test_totalsize3 = Nhits*(TOTALSIZE(fifopiraq3) + BUFFER_EPSILON); 
						fifopiraq3->udp.totalsize = test_totalsize3; // CP2 throughput testing
#endif
						seq3 = send_udp_packet(outsock3, outport + 2, seq3, udp3);  
						fifo_increment_tail(fifo3);
					} // end	while(fifo_hit()
					cycle_fifo3_hits = 0; 
				} // end	else if(val == 0) 
			} // end piraq3	
			if (kbhit()) {
				c = toupper(getch());
#ifdef TESTING_TIMESERIES_RANGE
				if(c == 'U') {
					pn_pkt->data.info.ts_start_gate += 1;
					pn_pkt->data.info.ts_end_gate += 1;
				}
				if(c == 'D') {
					pn_pkt->data.info.ts_start_gate -= 1;
					pn_pkt->data.info.ts_end_gate -= 1;
				}
#endif
				// !!!DO LATER: leave these here but do not use for now. 
				//				if(c == '+')		TimerStartCorrection += (__int64)ExactmSecperBeam; 
				//				if(c == '-')		TimerStartCorrection -= (__int64)ExactmSecperBeam; 
#ifndef TESTING_TIMESERIES
				//				if(c == '+')		TimerStartCorrection += (__int64)mSecperBeam; 
				//				if(c == '-')		TimerStartCorrection -= (__int64)mSecperBeam;
				if(c == 'A')		{ PIRAQadjustAmplitude = 1; PIRAQadjustFrequency = 0; printf("'U','D','+','-' adjust test waveform amplitude\n"); }
				if(c == 'W')		{ PIRAQadjustAmplitude = 0; PIRAQadjustFrequency = 1; printf("'U','D','+','-' adjust test waveform frequency\n"); }
				//	adjust test data freq. up/down fine/coarse
				if(PIRAQadjustFrequency)	{	//	use these keys to adjust the test sine frequency
					if(c == '+')		piraq1->SetCP2PIRAQTestAction(INCREMENT_TEST_SINUSIOD_FINE);	
					if(c == '-')		piraq1->SetCP2PIRAQTestAction(DECREMENT_TEST_SINUSIOD_FINE);
					if(c == 'U')		piraq1->SetCP2PIRAQTestAction(INCREMENT_TEST_SINUSIOD_COARSE);	 
					if(c == 'D')		piraq1->SetCP2PIRAQTestAction(DECREMENT_TEST_SINUSIOD_COARSE);	
				}
				//	adjust test data amplitude: up/down fine/coarse
				if(PIRAQadjustAmplitude)	{	//	use these keys to adjust the test sine amplitude
					if(c == '+')		piraq1->SetCP2PIRAQTestAction(INCREMENT_TEST_AMPLITUDE_FINE);	
					if(c == '-')		piraq1->SetCP2PIRAQTestAction(DECREMENT_TEST_AMPLITUDE_FINE);
					if(c == 'U')		piraq1->SetCP2PIRAQTestAction(INCREMENT_TEST_AMPLITUDE_COARSE);	 
					if(c == 'D')		piraq1->SetCP2PIRAQTestAction(DECREMENT_TEST_AMPLITUDE_COARSE);	
				}
#else
				// adjust test timeseries power: !note requires NOT TESTING_TIMESERIES_RANGE. 
				if(c == 'U')		test_ts_power *= sqrt(10.0); 
				if(c == 'D')		test_ts_power /= sqrt(10.0); // factor of arg in power
				// adjust test timeseries frequency: 
				if(c == '+')		test_ts_adjust += 2; 
				if(c == '-')		test_ts_adjust -= 2; 
#endif

				if(c == 'S')		send ^= 1;	
				//	temporarily use keystrokes '0'-'8' to switch PIRAQ channel mode on 3 boards
				if((c >= '0') && (c <= '8'))	{	// '0'-'2' piraq1, etc.
					if (c == '0')		{	piraq1->SetCP2PIRAQTestAction(SEND_CHA);	//	send CHA
					printf("piraq1->SetCP2PIRAQTestAction(SEND_CHA)\n");	}
					else if (c == '1')	{	piraq1->SetCP2PIRAQTestAction(SEND_CHB);	//	send CHB 
					printf("piraq1->SetCP2PIRAQTestAction(SEND_CHB)\n");	}
					else if (c == '2')	{	piraq1->SetCP2PIRAQTestAction(SEND_COMBINED);	//	send combined data
					printf("piraq1->SetCP2PIRAQTestAction(SEND_COMBINED)\n");	}
					else if (c == '3')	piraq2->SetCP2PIRAQTestAction(SEND_CHA);	//	send CHA
					else if (c == '4')	piraq2->SetCP2PIRAQTestAction(SEND_CHB);	//	send CHB 
					else if (c == '5')	piraq2->SetCP2PIRAQTestAction(SEND_COMBINED);	//	send combined data
					else if (c == '6')	piraq3->SetCP2PIRAQTestAction(SEND_CHA);	//	send CHA
					else if (c == '7')	piraq3->SetCP2PIRAQTestAction(SEND_CHB);	//	send CHB 
					else if (c == '8')	piraq3->SetCP2PIRAQTestAction(SEND_COMBINED);	//	send combined data
				}
				if(c == 'Q' || c == 27)   {printf("sent %d packets, total errors 0:%d 1:%d 2:%d\n", packets, PNerrors1, PNerrors2, PNerrors3 ); printf("\nUser terminated:\n");	break;}
			} // end if (kbhit())  
		} // end	while(1)

		// exit NOW:
		printf("\npress a key to stop piraqs and exit\n"); while(!keyvalid()) ; 
		printf("piraqs stopped.\nexit.\n\n"); 
		if (piraqs & 0x01) { // slot 1 active
			stop_piraq(config1, piraq1); 
		} 
		if (piraqs & 0x02) { // slot 2 active
			stop_piraq(config2, piraq2); 
		} 
		if (piraqs & 0x04) { // slot 3 active
			stop_piraq(config3, piraq3); 
		} 
		// remove for lab testing: keep transmitter pulses active w/o go.exe running. 12-9-04
		timer_stop(&ext_timer); 
		delete piraq1; 
		delete piraq2; 
		delete piraq3;
		fclose(db_fp);
		//		printf("TimerStartCorrection = %dmSec\n", TimerStartCorrection); 
		exit(0); 

	} // end if (piraqs)
	stop_piraq(config1, piraq);	//?
	timer_stop(&ext_timer); 
	delete piraq; 
	printf("\n%d: fifo_hits = %d\n", testnum, testnum); 
	exit(0); 
}

