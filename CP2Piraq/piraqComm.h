#ifndef _PROTO_H_
#define _PROTO_H_

#include "dd_types.h"

#define PCIBASE		0x3000000

#define	MAGIC		0x12345678

#define STRUCTURE_ALIGNMENT 8 // must be a power of 2

#define	PIRAQ_DEVICE_ID 	0x9054
#define	PIRAQ_VENDOR_ID		0x10B5   

#define	true	1
#define	false	0

void
dmaTransfer(int channel, 
			int *src, 
			int *dst, 
			int transferCount,
			int globalReload); 

#define	PUDPSENDSIZE	(unsigned int)65536	//	size of N-hit packets in system: PIRAQ through Qt applications

// 3 modes of data transfer from PIRAQ to host: first 2 are diagnostic
#define	SEND_CHA		0		// send CHA
#define	SEND_CHB		1		// send CHB
#define	SEND_COMBINED	2		// execute dynamic-range extension algorithm; send resulting combined data
#define	CHMODE_MASK		0x03	// extract channel mode bits

// 4 PIRAQ test-sinusoid frequency adjustments
#define	INCREMENT_TEST_SINUSIOD_COARSE	4
#define	INCREMENT_TEST_SINUSIOD_FINE	8
#define	DECREMENT_TEST_SINUSIOD_COARSE	0x0c
#define	DECREMENT_TEST_SINUSIOD_FINE	0x10
#define	FREQADJ_MASK	0x1c	// extract frequency-adjust bits

// 4 PIRAQ test-sinusoid amplitude adjustments
#define	INCREMENT_TEST_AMPLITUDE_COARSE	0x20
#define	INCREMENT_TEST_AMPLITUDE_FINE	0x40
#define	DECREMENT_TEST_AMPLITUDE_COARSE	0x60
#define	DECREMENT_TEST_AMPLITUDE_FINE	0x80
#define	AMPLADJ_MASK	0xe0	// extract amplitude-adjust bits

#define		PI				3.1415926

#define	PMAXPACKET	1200
#define	PMAXGATES	400		// Added to test speed; most you can do w/clutterfilter

#pragma pack(4)
typedef	struct 
	{
	char	name[80];	/* name that identifies the type of FIFO buffer */
	int	magic;	/* magic number indicating the FIFO has been initialized */
	int	size;		/* total size of allocated memory for this FIFO */
	int	header_off;	/* offset to the user header (can't use absolute address here) */
	int	fifobuf_off;	/* offset to fifo base address */
	int	record_size;	/* size in bytes of each FIFO record */
	int	record_num;	/* number of records in FIFO buffer */
	int	head,tail;	/* indexes to the head and tail records */
	int	destroy_flag;	/* destroy-when-empty flag */
	int sock;				/* socket file descriptor for notification communications */
    int port;				/* UDP (datagram) socket port for notification */
    int clients;
	} PFIFO;

typedef struct pudp_header{
    int totalsize;      /* total amount of data only (don't count the size of this header) */
} PUDPHEADER;
#pragma STRUCT_ALIGN (PUDPHEADER, 8);

typedef struct pcommand {
	int		type;
	int		flag;		/* done, new, old, handshake, whatever, ..... */
	}	PCOMMAND;
#pragma STRUCT_ALIGN (PCOMMAND, 8);
		
typedef struct pinfoheader
{   
	char desc[4];			/* "DWLX" */
//    uint4 recordlen;        /* total length of record - must be the second field */
    uint4 channel;          ///< board number
//    uint4 rev;		        /* format revision #-from RADAR structure */
//    uint4 one;			    /* always set to the value 1 (endian flag) */
//    uint4 byte_offset_to_data;
    uint4 dataformat;

//    uint4 typeof_compression;
    
    /*
      Pulsenumber (pulse_num) is the number of transmitted pulses
since Jan 1970. It is a 64 bit number. It is assumed
that the first pulse (pulsenumber = 0) falls exactly
at the midnight Jan 1,1970 epoch. To get unix time,
multiply by the PRT. The PRT is a rational number a/b.
More specifically N/Fc where Fc is the counter clock (PIRAQ_CLOCK_FREQ),
and N is the divider number. So you can get unix time
without roundoff error by:
secs = pulsenumber * N / Fc. The
computations is done with 64 bit arithmatic. No
rollover will occur.

The 'nanosecs' field is derived without roundoff
error by: 100 * (pulsenumber * N % Fc).

Beamnumber is the number of beams since Jan 1,1970.
The first beam (beamnumber = 0) was completed exactly
at the epoch. beamnumber = pulsenumber / hits. 
*/
    
#ifdef _TMS320C6X   /* TI doesn't support long long */
    uint4 pulse_num_low;
    uint4 pulse_num_high;
    uint4 beam_num_low;
    uint4 beam_num_high;
#else
    uint8 pulse_num;   /*  keep this field on an 8 byte boundary */
    uint8 beam_num;	   /*  keep this field on an 8 byte boundary */
#endif
    uint4 gates;
    uint4 start_gate;
    uint4 hits;
    uint4 ctrlflags; /* equivalent to packetflag below?  */
    uint4 bytespergate; 
    float4 rcvr_pulsewidth;
#define PX_NUM_PRT 4
    float4 prt[PX_NUM_PRT];
    float4 meters_to_first_gate;  
    uint4 num_segments;  /* how many segments are we using */
#define PX_MAX_SEGMENTS 8
    float4 gate_spacing_meters[PX_MAX_SEGMENTS];
    uint4 gates_in_segment[PX_MAX_SEGMENTS]; /* how many gates in this segment */
#define PX_NUM_CLUTTER_REGIONS 4
    uint4 clutter_start[PX_NUM_CLUTTER_REGIONS]; /* start gate of clutter filtered region */
    uint4 clutter_end[PX_NUM_CLUTTER_REGIONS];  /* end gate of clutter filtered region */
    uint4 clutter_type[PX_NUM_CLUTTER_REGIONS]; /* type of clutter filtering applied */
/* following fields are computed from pulse_num by host */
    uint4 secs;     /* Unix standard - seconds since 1/1/1970
                       = pulse_num * N / ClockFrequency */
    uint4 nanosecs;  /* within this second */
    float4 az;   /* azimuth: referenced to 9550 MHz. possibily modified to be relative to true North. */
    float4 az_off_ref;   /* azimuth offset off reference */ 
    float4 el;		/* elevation: referenced to 9550 MHz.  */ 
    float4 el_off_ref;   /* elevation offset off reference */ 
    float4 radar_longitude;
    float4 radar_latitude;
    float4 radar_altitude;
#define PX_MAX_GPS_DATUM 8
    char gps_datum[PX_MAX_GPS_DATUM]; /* e.g. "NAD27" */
    
    uint4 ts_start_gate;   /* starting time series gate , set to 0 for none */
    uint4 ts_end_gate;     /* ending time series gate , set to 0 for none */
    
    float4 ew_velocity;

    float4 ns_velocity;
    float4 vert_velocity;

    float4 fxd_angle;		/* in degrees instead of counts */
    float4 true_scan_rate;	/* degrees/second */
    uint4 scan_type;
    uint4 scan_num;
    uint4 vol_num;

    uint4 transition;
    float4 xmit_power;

    float4 yaw;
    float4 pitch;
    float4 roll;
    float4 track;
    float4 gate0mag;  /* magnetron sample amplitude */
    float4 dacv;
    uint4  packetflag; 
    /*
    // items from the depricated radar "RHDR" header
    // do not set "radar->recordlen"
    */
    uint4 year;             /* e.g. 2003 */
    uint4 julian_day;
    
#define PPX_MAX_RADAR_NAME 16
    char radar_name[PPX_MAX_RADAR_NAME];
#define PPX_MAX_CHANNEL_NAME 16
    char channel_name[PPX_MAX_CHANNEL_NAME];
#define PPX_MAX_PROJECT_NAME 16
    char project_name[PPX_MAX_PROJECT_NAME];
#define PPX_MAX_OPERATOR_NAME 12
    char operator_name[PPX_MAX_OPERATOR_NAME];
#define PPX_MAX_SITE_NAME 12
    char site_name[PPX_MAX_SITE_NAME];
    
    uint4 polarization;
    float4 test_pulse_pwr;
    float4 test_pulse_frq;
    float4 frequency;

    float4 noise_figure;
    float4 noise_power;
    float4 receiver_gain;
    float4 E_plane_angle;  /* offsets from normal pointing angle */
    float4 H_plane_angle;
    

    float4 data_sys_sat;
    float4 antenna_gain;
    float4 H_beam_width;
    float4 V_beam_width;

    float4 xmit_pulsewidth;
    float4 rconst;
    float4 phaseoffset;

    float4 zdr_fudge_factor;

    float4 mismatch_loss;
    float4 rcvr_const;

    float4 test_pulse_rngs_km[2];
    float4 antenna_rotation_angle;   /* S-Pol 2nd frequency antenna may be 30 degrees off vertical */
    
#define PPX_SZ_COMMENT 64
    char comment[PPX_SZ_COMMENT];
    float4 i_norm;  /* normalization for timeseries */
    float4 q_norm;
    float4 i_compand;  /* companding (compression) parameters */
    float4 q_compand;
    float4 transform_matrix[2][2][2];
    float4 stokes[4]; 
    float4 spare[20];

    /*
    // always append new items so the alignment of legacy variables
    // won't change
    */
} PINFOHEADER;
#pragma STRUCT_ALIGN (PINFOHEADER, 8);

typedef struct pdatablock {		/* data that's in the PIRAQ1 FIFO */
	PINFOHEADER		info;
	float				data[PMAXGATES * 12];
	} PDATABLOCK;
#pragma STRUCT_ALIGN (PDATABLOCK, 8);

typedef struct ppacket {
	PUDPHEADER	udp;
	PCOMMAND		cmd;
	PDATABLOCK	data;
    } PPACKET;
#pragma STRUCT_ALIGN (PPACKET, 8);

typedef struct ppacket_header {			/* this structure must match the non-data portion of the PACKET structure */
	PUDPHEADER	udp;
	PCOMMAND		cmd;
	PINFOHEADER	info;
	} PPACKETHEADER;
#pragma STRUCT_ALIGN (PPACKETHEADER, 8);
	
#define	PHEADERSIZE		sizeof(PPACKETHEADER)
#define	PRECORDLEN(a)  (sizeof(PINFOHEADER) + (PDATASIZE(a)))
#define	PDATASIZE(a)   (a->data.info.gates * a->data.info.bytespergate)
//void createSineTestWaveform(float freq);	//	create test sine waveform of freq; store to SINstore
//void ChannelSelect(int ngates, float * restrict src, float * restrict dst, unsigned int channelMode);


#ifdef __cplusplus
extern "C" {  // only need to export C interface if
              // used by C++ source code
#endif
PFIFO *pfifo_create(char *name, int headersize, int recordsize, int recordnum);
PFIFO *pfifo_open(char *name);
int    pfifo_close(PFIFO *fifo);
void  *pfifo_get_last_address(PFIFO *fifo);
int    pfifo_increment_head(PFIFO *fifo);
void   pfifo_notify(PFIFO *fifo);
void  *pfifo_get_read_address(PFIFO *fifo, int offset);
void  *pfifo_get_write_address(PFIFO *fifo);
int    pfifo_increment_tail(PFIFO *fifo);
int    pfifo_hit(PFIFO *fifo);
int    pfifo_exhist(char *name);
void  *pfifo_get_header_address(PFIFO *fifo);
#ifdef __cplusplus
}
#endif

#endif