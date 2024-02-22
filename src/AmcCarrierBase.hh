//////////////////////////////////////////////////////////////////////////////
// This file is part of 'timing_bsa'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'timing_bsa', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
#ifndef Bsa_AmcCarrierBase_hh
#define Bsa_AmcCarrierBase_hh

#include <cpsw_api_builder.h>

#include <stdint.h>
#include <vector>

#include <BsaDefs.hh>

namespace Bsa {
  class AmcCarrierBase {
  public:
    AmcCarrierBase();
    virtual ~AmcCarrierBase() {}
  public:
    //  BSA buffers
    Record*  get       (unsigned array,
                        uint64_t begin,
                        uint64_t* next) const;
  private:
    void     _fill     (void*    dst,
                        uint64_t begin,
                        uint64_t end) const;
  public:
    void     initialize();
    void     reset     (unsigned array);
    void     ackClear  (unsigned array);
    uint64_t inprogress() const;
    uint64_t done      () const;
    bool     done      (unsigned array) const;
    uint32_t status    (unsigned array) const;
    ArrayState state   (unsigned array) const;
    const std::vector<ArrayState>& state   ();

    Record*  getRecord (unsigned array) const;
    Record*  getRecord (unsigned array,
                        uint64_t* next) const;
    Record*  getRecord (unsigned array,
                        uint64_t begin) const;
    uint8_t* getBuffer (uint64_t begin,
                        uint64_t end  ) const;
    virtual  RingState ring  (unsigned array) const = 0;
  protected:
    void    _printBuffer(Path path, ScalVal_RO ts, unsigned i,
                         uint64_t done , uint64_t full, 
                         uint64_t empty, uint64_t error) const;
    void    _printBuffer(Path path, unsigned i,
                         uint64_t done , uint64_t full, 
                         uint64_t empty, uint64_t error) const;
  protected:
    std::vector<ArrayState> _state;
    std::vector<uint64_t>   _begin;
    std::vector<uint64_t>   _end;
    mutable Record          _record;
    Path       _path;
    Path       _bpath;
    Path       _wpath0;
    Path       _wpath1;
    ScalVal_RO _sStat;
    ScalVal_RO _sEmpt;
    ScalVal_RO _sDone;
    ScalVal_RO _sFull;
    ScalVal    _sEnabled;
    ScalVal    _sMode;
    ScalVal    _sInit;
    ScalVal_RO _sError;
    ScalVal_RO _sStatus;
    ScalVal_RO _tstamp;
    ScalVal    _sClear;
    ScalVal    _startAddr;
    ScalVal    _endAddr;
    ScalVal_RO _wrAddr;
    ScalVal_RO _trAddr;
    ScalVal_RO _dram;
    uint64_t   _memEnd;

    friend class Reader;
    friend class ProcessorImpl;
  };
};

#endif
