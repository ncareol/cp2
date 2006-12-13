/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
 ** Copyright UCAR (c) 1992 - 1999
 ** University Corporation for Atmospheric Research(UCAR)
 ** National Center for Atmospheric Research(NCAR)
 ** Research Applications Program(RAP)
 ** P.O.Box 3000, Boulder, Colorado, 80307-3000, USA
 ** All rights reserved. Licenced use only.
 ** Do not copy or distribute without authorization
 ** 1999/03/14 14:18:54
 *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/
///////////////////////////////////////////////////////////////
// MomentsCompute.cc
//
// MomentsCompute object
//
// Mike Dixon, RAP, NCAR, P.O.Box 3000, Boulder, CO, 80307-3000, USA
//
// May 2006
//
///////////////////////////////////////////////////////////////
//
// MomentsCompute reads raw time-series data, and
// computes the moments.
//
////////////////////////////////////////////////////////////////

#include <cerrno>
#include <cmath>
#include <iostream>
#include <iomanip>
#include "MomentsCompute.hh"
using namespace std;

const double MomentsCompute::_missingDbl = -9999.0;

////////////////////////////////////////////////////
// Constructor

MomentsCompute::MomentsCompute(int argc, char **argv)

{

  _progName = "MomentsCompute";

  _momentsMgr = NULL;

  _nSamples = 64;
  _maxPulseQueueSize = 0;

  _midIndex1 = 0;
  _midIndex2 = 0;
  _countSinceBeam = 0;

  _pulseSeqNum = 0;

  _az = 0.0;
  _prevAz = -1.0;
  _el = 0.0;
  _prevEl = -180;
  _time = 0;

  _nGatesPulse = 0;
  _nGatesOut = 0;
  
  isOK = true;

  // get command line args
  
  if (_args.parse(argc, argv, _params)) {
    cerr << "ERROR: " << _progName << endl;
    cerr << "Problem with command line args" << endl;
    isOK = false;
    return;
  }

  // set up moments objects
  // This initializes the FFT package to the set number of samples.
  
  for (int i = 0; i < _params.n_moments_params; i++) {
    MomentsMgr *mgr = new MomentsMgr(_params, _params.moments_params[i]);
    _momentsMgrArray.push_back(mgr);
  }
  if (_momentsMgrArray.size() < 1) {
    cerr << "ERROR: MomentsCompute::MomentsCompute." << endl;
    cerr << "  No algorithm geometry specified."; 
    cerr << "  The param moments_menuetry must have at least 1 entry."
	 << endl;
    isOK = false;
    return;
  }

  // compute pulse queue size, set to 2 * maxNsamples

  int maxNsamples = 0;
  for (int i = 0; i < _params.n_moments_params; i++) {
    if (maxNsamples < _params.moments_params[i].n_samples) {
      maxNsamples = _params.moments_params[i].n_samples;
    }
  }
  _maxPulseQueueSize = maxNsamples * 2 + 2;
  if (_params.debug >= Params::DEBUG_VERBOSE) {
    cerr << "_maxPulseQueueSize: " << _maxPulseQueueSize << endl;
  }
  
  return;

}

//////////////////////////////////////////////////////////////////
// destructor

MomentsCompute::~MomentsCompute()

{

  for (size_t ii = 0; ii < _momentsMgrArray.size(); ii++) {
    delete _momentsMgrArray[ii];
  }
  _momentsMgrArray.clear();

  for (size_t ii = 0; ii < _pulseQueue.size(); ii++) {
    if (_pulseQueue[ii]->removeClient("MomentsCompute destructor") == 0) {
      delete _pulseQueue[ii];
    }
  } // ii
  _pulseQueue.clear();

}

//////////////////////////////////////////////////
// Run

int MomentsCompute::Run ()
{
  
  // process pulses as they arrive

  int pulseSeqNum = 0;

  bool done = false;
  while (!done) {

    // at this point you need to acquire another pulse
    // instead of using the following hard-coded values

    int nGates = 1000;
    double now = (double) time(NULL);
    double prt = 1000.0;
    double el = 1.0;
    double az = pulseSeqNum % 360;
    bool isHoriz = pulseSeqNum % 2;
    float *iqc = new float[nGates * 2];
    float *iqx = NULL;
    
    // Create a new pulse object and save a pointer to it in the
    // _pulseBuffer array.  _pulseBuffer is a FIFO, with elements
    // added at the end and dropped off the beginning. So if we have a
    // full buffer delete the first element before shifting the
    // elements to the left.
    
    Pulse *pulse = new Pulse(_params, pulseSeqNum, nGates, now,
                             prt, el, az, isHoriz, iqc, iqx);

    delete[] iqc;
    
    // add pulse to queue, managing memory appropriately
    
    _addPulseToQueue(pulse);
    
    // prepare for moments computations
    // also sets _momentsMgr as appropriate
    
    _prepareForMoments(pulse);
  
    // is a beam ready?
    
    if (_momentsMgr != NULL && _beamReady()) {
	
      _countSinceBeam = 0;
      
      // create new beam
      
      Beam *beam = new Beam(_params, _pulseQueue, _az, _momentsMgr);
      _nGatesOut = beam->getNGatesOut();
      
      // compute beam moments
      
      _computeBeamMoments(beam);
      
      // write out beam moments here
      
      // writeBeam();
      
      delete beam;
      
    }

    pulseSeqNum++;

  } // while (true)

  return 0;

}

/////////////////////////////////////////////////
// prepare for moments computations
    
void MomentsCompute::_prepareForMoments(Pulse *pulse)
  
{

  // set properties from pulse
  
  _nGatesPulse = pulse->getNGates();
  double prf = 1.0 / pulse->getPrt();

  // find moments manager, based on PRF
  
  if (fabs(prf - _prevPrfForMoments) > 0.1) {
    _momentsMgr = NULL;
    for (int i = 0; i < (int) _momentsMgrArray.size(); i++) {
      if (prf >= _momentsMgrArray[i]->getLowerPrf() &&
	  prf <= _momentsMgrArray[i]->getUpperPrf()) {
	_momentsMgr = _momentsMgrArray[i];
	_nSamples = _momentsMgr->getNSamples();
	break;
      }
    }
    _prevPrfForMoments = prf;
  } // if (fabs(prf ...

}
    
/////////////////////////////////////////////////
// are we ready for a beam?
//
// Side effects: sets _az, _midIndex1, _midIndex2

bool MomentsCompute::_beamReady()
  
{
  
  _countSinceBeam++;
  _az = 0.0;
  
  // enough data in the queue?

  int minPulses = _nSamples;
  if (_momentsMgr->getMode() == Params::DUAL_FAST_ALT) {
    // need one extra pulse because we sometimes need to search
    // backwards for horizontal pulse
    minPulses++;
  }

  if ((int) _pulseQueue.size() < minPulses) {
    return false;
  }
  
  // check we have constant nGates
  
  int nGates = _pulseQueue[0]->getNGates();
  for (int i = 1; i < _nSamples; i++) {
    if (_pulseQueue[i]->getNGates() != nGates) {
      return false;
    }
  }
  
  // check we have constant prt
  
  double prt = _pulseQueue[0]->getPrt();
  for (int i = 1; i < _nSamples; i++) {
    if (fabs(_pulseQueue[i]->getPrt() - prt) > 0.001) {
      return false;
    }
  }

  // compute the indices at the middle of the beam.
  // index1 is just short of the midpt
  // index2 is just past the midpt
  
  _midIndex1 = _nSamples / 2;
  _midIndex2 = _midIndex1 - 1;

  // compute azimuths which need to straddle the center of the beam
  
  double midAz1 = _pulseQueue[_midIndex1]->getAz();
  double midAz2 = _pulseQueue[_midIndex2]->getAz();

  if (_params.index_beams_in_azimuth) {

    // compute target azimiuth by rounding the azimuth at the
    // center of the data to the closest suitable az
    
    _az = ((int) (midAz1 / _params.azimuth_resolution + 0.5)) *
      _params.azimuth_resolution;
    
    if (_az >= 360.0) {
      _az -= 360;
    } else if (_az < 0) {
      _az += 360.0;
    }
    
    // Check if the azimuths at the center of the data straddle
    // the target azimuth
    
    if (midAz1 <= _az && midAz2 >= _az) {
      
      // az1 is below and az2 above - clockwise rotation
      return true;
      
    } else if (midAz1 >= _az && midAz2 <= _az) {
      
      // az1 is above and az2 below - counterclockwise rotation
      return true;
      
    } else if (_az == 0.0) {
      
      if (midAz1 > 360.0 - _params.azimuth_resolution &&
	  midAz2 < _params.azimuth_resolution) {
	
	// az1 is below 0 and az2 above 0 - clockwise rotation
	return true;
	
      } else if (midAz2 > 360.0 - _params.azimuth_resolution &&
		 midAz1 < _params.azimuth_resolution) {
	
	// az1 is above 0 and az2 below 0 - counterclockwise rotation
	return true;
	
      }
      
    } else if (_countSinceBeam > (_nSamples * 16)) {
      
      // antenna moving very slowly, we have waited long enough
      return true;
      
    }
    
  } else {
    
    // do not index - only check we have enough data
    
    if (_countSinceBeam >= _nSamples) {
      _az = midAz1;
      if (_az >= 360.0) {
	_az -= 360;
      } else if (_az < 0) {
	_az += 360.0;
      }
      return true;
    }
    
  }
  
  return false;

}

/////////////////////////////////////////////////
// compute moments for the beam
    
int MomentsCompute::_computeBeamMoments(Beam *beam)
  
{
  
  // compute moments
  
  beam->computeMoments();

  // check for missing az

  bool azMissing = false;
  if (_prevAz >= 0) {
    double deltaAz = fabs(beam->getAz() - _prevAz);
    if (deltaAz > 180) {
      deltaAz = fabs(deltaAz - 360.0);
    }
    if (deltaAz > _params.azimuth_resolution) {
      azMissing = true;
    }
  }

  if (_params.debug) {
    if (azMissing) {
      cerr << "Azimuths missing, prev: " << _prevAz
           << ", this: " << beam->getAz() << endl;
    }
  }
  
  _prevAz = beam->getAz();
  _prevEl = beam->getEl();
  
  return 0;

}
	
/////////////////////////////////////////////////
// add the pulse to the pulse queue
    
void MomentsCompute::_addPulseToQueue(Pulse *pulse)
  
{

  // manage the size of the pulse queue, popping off the back
  
  if ((int) _pulseQueue.size() > _maxPulseQueueSize) {
    Pulse *oldest = _pulseQueue.back();
    if (oldest->removeClient("Deleting from queue") == 0) {
      delete oldest;
    }
    _pulseQueue.pop_back();
  }
  
  // push pulse onto front of queue
  
  pulse->addClient("Adding to queue");
  _pulseQueue.push_front(pulse);

  // print missing pulses if requested
  
  if ((int) pulse->getSeqNum() != _pulseSeqNum + 1) {
    if (_params.debug >= Params::DEBUG_VERBOSE && _pulseSeqNum != 0) {
      cout << "**************** Missing seq num: " << _pulseSeqNum
	   << " to " <<  pulse->getSeqNum() << " **************" << endl;
    }
  }
  _pulseSeqNum = pulse->getSeqNum();

}

