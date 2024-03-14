//////////////////////////////////////////////////////////////////////////////
// This file is part of 'timing_bsa'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'timing_bsa', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
#ifndef BsaDef_hh
#define BsaDef_hh

#include <stdint.h>
#include <vector>

namespace Bsa {

  enum { NBSAARRAYS =44 };
  enum { HSTARRAY0  =44 };
  enum { HSTARRAYN  =48 };

  const int numChannelData = 31;

  class ChannelData {
  public:
    //  Arithmetic exception during accumulation
    bool      excpt() const { return data[0]&(3<<13); }
    unsigned  n   () const { return data[0]&0x1fff; }
    double    mean() const;
    double    rms2() const;
    //  Data is not accumulated, only raw value is needed
    bool      fixed() const { return data[0]&(1<<15); }
    unsigned  raw  () const { return (data[0]>>16) | (data[1]<<16); }
  public:
    uint32_t  data[3];
  };

  class Entry {
  public:
    unsigned nchannels() const;
    uint64_t pulseId  () const;
  private:
    uint32_t  data[3];
  public:
    ChannelData channel_data[numChannelData];
  };

  class Record {
  public:
    Record(unsigned nreserve=1) { entries.reserve(nreserve); }
  public:
    unsigned  buffer;
    unsigned  time_secs;
    unsigned  time_nsecs;
    std::vector<Entry> entries;
  };

  class ArrayState {
  public:
    ArrayState() : timestamp(0), wrAddr(0), next(0), clear(0), wrap(0), nacq(0) {}
    bool operator!=(const ArrayState& o) const { return timestamp!=o.timestamp || wrAddr!=o.wrAddr; }
  public:
    uint64_t timestamp;
    uint64_t wrAddr;
    uint64_t next;
    unsigned clear;
    unsigned wrap;
    unsigned nacq;
  };

  class RingState {
  public:
    uint64_t begAddr;
    uint64_t endAddr;
    uint64_t nxtAddr;
  };

  class BeamSelect {
  public:
    enum Destinations { InjDLine=1, D10Dump=2, SXU=4 };
    BeamSelect();  // Don't Care
    BeamSelect(unsigned mask, bool excl=false);  // mask is OR of Destinations
    operator unsigned() const;
  private:
    unsigned _value;
  };

  class RateSelect {
  public:
    enum FixedRate { _929kHz, _71kHz, _10kHz, _1kHz, _100Hz, _10Hz, _1Hz };
    enum ACRate    { _60HzA, _30HzA, _10HzA, _5HzA, _1HzA };
    RateSelect();
    RateSelect(FixedRate frate);                // Fixed Rate
    RateSelect(ACRate    arate, unsigned ats);  // AC Rate
    RateSelect(unsigned  idx  , unsigned bit);  // Sequence
    operator unsigned() const;
  private:
    unsigned _value;
  };
};

#endif
