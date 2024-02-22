//////////////////////////////////////////////////////////////////////////////
// This file is part of 'timing_bsa'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'timing_bsa', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
#include "Processor.hh"
#include "AmcCarrier.hh"
#include "AmcCarrierYaml.hh"
#include "BsaDefs.hh"

#include <cpsw_api_builder.h>

#include <queue>
#include <stdio.h>
#include <time.h>
#include <syslog.h>

#define DONE_WORKAROUND

static unsigned _nReadout = 1024 * 128;
static const unsigned MAXREADOUT = 1<<20;

static char* timestr() 
{
  time_t t = time(NULL);
  char* v = asctime(localtime(&t));
  *strchr(v,'\n')=0; // strip the carriage return
  return v;
}

namespace Bsa {

  class Reader {
  public:
    static void     set_nReadout(unsigned v) {_nReadout=v;} 
    static unsigned get_nReadout() { return _nReadout;}
  public:
    Reader() : _timestamp(0), _next(0), _last(0), _end(0), _preset(0), 
               _abort(false), _record(MAXREADOUT) {}
    ~Reader() {}
  public:
    void     abort() { _abort = true; }
    bool     done () const { return _next==_last; }
    void     preset (const ArrayState&    state) { _preset = state.wrAddr; }
    bool     reset(PvArray&              array, 
                   const ArrayState&     state,
                   AmcCarrierBase&       hw,
                   uint64_t              timestamp)
    {
      if (_abort) {
        _next = _last = state.wrAddr;
        _abort = false;
        return false;
      }

      //  Check if wrAddr pointer has moved.  If so, the acquisition isn't done
      if (state.wrAddr != _preset) {
        syslog(LOG_WARNING,"<W> %s:  %s:%-4d [reset]: not ready 0x%09llx  wrAddr 0x%09llx",
               timestr(),__FILE__,__LINE__,_preset,state.wrAddr);
        return false;
      }

      _timestamp = state.timestamp;
      //  If wrap is set, the buffer is complete from wrAddr -> end; start -> wrAddr
      //  Else, if the buffer was read before, continue from where we ended
      //  Else read from the start
      unsigned iarray = array.array();
      _start = hw._begin[iarray];
      _end   = hw._end  [iarray];
      _next = state.wrap ? state.wrAddr : (_last ? _last : _start);
      _last = state.wrAddr;

      //  Check if wrAddr is properly aligned to the record size.
      unsigned n0 = _last / sizeof(Entry);
      if (n0*sizeof(Entry) != _last) {
        syslog(LOG_WARNING,"<W> %s:  %s:%-4d [reset] misaligned _last 0x%09llx",
               timestr(),__FILE__,__LINE__,_last);
        return false;
      }

      array.reset(timestamp>>32,timestamp&0xffffffff);

      uint64_t hw_done = hw.done();
      syslog(LOG_DEBUG,"<D> %s:  %s:%-4d []  array %u  _timestamp 0x%016llx  _next 0x%09llx  _last 0x%09llx  __end 0x%09llx  done 0x%09llx",
             timestr(),__FILE__,__LINE__,array.array(),_timestamp,_next,_last,_end,hw_done);

      return true;
    }

    Record* next(PvArray& array, AmcCarrierBase& hw)
    {
      if (_abort) {
        _next = _last;
        _record.entries.resize(0);
        return &_record;
      }

      unsigned n = _nReadout;
      //  _next : start of current read
      //  _last : end of final read
      //  _end  : end of buffer where we need to wrap
      uint64_t next = _next + n*sizeof(Entry);  // end of current read
      uint64_t nnext = next;                    // start of next read

      //  What are the possibilities?
      //    (start)                   (_last)                (_end)
      //     1.      _next      next                                        read _next:next
      //     2.      _next                      next                        read _next:_last
      //     3.      _next                                           next   read _next:_last
      //     4.                                _next    next                read _next:next
      //     5.                                _next                 next   read _next:_end; _next=start
      if ((_next <  _last && next < _last) ||
          (_next >= _last && next < _end)) {
        //  read _next:next
      }
      else if (_next < _last) {
        //  read _next:_last
        n = (_last - _next) / sizeof(Entry);
        nnext = _last;
        next  = _last;
      }
      else {
        //  read _next:_end
        next = _end;
        n = (next - _next) / sizeof(Entry);
        nnext = _start;
      }

      if (n > MAXREADOUT) {
        syslog(LOG_ERR,"<E> Reader::next allocating record with %u entries", n);
        throw(std::string("Too many entries"));
      }

      Record& record = _record;
      record.entries.resize(n);

      hw._fill(record.entries.data(), _next, next);

      //  Detect mis-aligned entries
      unsigned n0 = _next / sizeof(Entry);
      if (n0*sizeof(Entry) != _next) {
        const Entry& e = record.entries[0];
        syslog(LOG_ERR,"<E> %s  %s:%-4d [Misaligned record]  _next 0x%09llx  nch %u  pid 0x%09llx",
               timestr(),__FILE__,__LINE__,_next,e.nchannels(),e.pulseId());
      }
      
      //  _last or _end dont occur at an Entry boundary
      if (_next + n*sizeof(Entry) != next) {
        syslog(LOG_ERR,"<E> %s:  %s:%-4d [Truncated record]  _next 0x%09llx  next 0x%09llx  _last 0x%09llx  _end 0x%09llx  _next+n 0x%09llx  n %u",
               timestr(),__FILE__,__LINE__,_next,next,_last,_end,_next+n*sizeof(Entry),n);
      }

      _next = nnext;

      if (done()) {
        hw   .reset(array.array());
        array.set(_timestamp>>32, _timestamp&0xffffffff);
        uint64_t hw_done = hw.done();
        syslog(LOG_DEBUG,"<D> %s:%-4d [done]:  array %u  hw.done 0x%09llx",__FILE__,__LINE__,array.array(),hw_done);
      }

      return &record;
    }
  private:
    AmcCarrierBase* _hw;
    uint64_t _start;
    uint64_t _end;
    uint64_t _timestamp;
    uint64_t _next;
    uint64_t _last;
    uint64_t _preset;
    bool     _abort;
    Record   _record;
  };

  class ProcessorImpl : public Processor {
  public:
    ProcessorImpl(Path reg,
                  Path ram,
                  bool lInit) : _hw(*new AmcCarrierYaml(reg,ram))
    {
      if (lInit) _hw.initialize();
      for(unsigned i=0; i<HSTARRAYN; i++) {
	_state[i].next = _hw._begin[i];
	syslog(LOG_DEBUG,"<D> %s:  %s:%-4d [ProcessorImpl] next[%u] 0x%09llx",
	       timestr(),__FILE__,__LINE__, i,_state[i].next);
      }
    }
    ProcessorImpl(const char* ip,
		  bool lInit,
		  bool lDebug) : _hw(*new AmcCarrier(ip))
    {
      if (lInit) _hw.initialize();
      for(unsigned i=0; i<HSTARRAYN; i++)
	_state[i].next = _hw._begin[i];
    }
    ProcessorImpl() : _hw(*AmcCarrier::instance())
    {
      syslog(LOG_WARNING,"<W> %s:  %s:%-4d [ProcessorImpl]",
	     timestr(),__FILE__,__LINE__);
    }
    ~ProcessorImpl();
  public:
    uint64_t pending();
    int      update(PvArray&);
    AmcCarrierBase *getHardware();
  private:
    void     abort (PvArray&);
    AmcCarrierBase&      _hw;
    ArrayState           _state [HSTARRAYN];
    Reader               _reader[HSTARRAYN-HSTARRAY0];
    std::queue<unsigned> _readerQueue;
    Record               _emptyRecord;
  };

};

using namespace Bsa;

AmcCarrierBase *ProcessorImpl::getHardware()
{
    return &_hw;
}

uint64_t ProcessorImpl::pending()
{
  const std::vector<ArrayState>& s = _hw.state();

  uint64_t r = 0;
  for(unsigned i=0; i<HSTARRAY0; i++)
    if (s[i] != _state[i])
      r |= 1ULL<<i;

  r |= ~((1ULL<<HSTARRAY0)-1);

  uint64_t done = _hw.done();
  done |= (1ULL<<HSTARRAY0)-1;
  r &= done;

  return r;
}

int ProcessorImpl::update(PvArray& array)
{
  unsigned      iarray = array.array();
  std::vector<Pv*> pvs = array.pvs();
  ArrayState current(_hw.state(iarray));

  Record* record;

  try {

    // syslog(LOG_DEBUG,"<W> %s:  %s:%-4d []: array %u  next 0x%09llx",
    // 	   timestr(),__FILE__,__LINE__,iarray,_state[iarray].next);

    if (array.array() < HSTARRAY0) {
      if (current != _state[iarray]) {
        current.nacq = _state[iarray].nacq;
        if (current.clear) {
          //
          //  New acquisition;  start from the beginning of the circular buffer
          //
          _hw.ackClear(iarray);

          syslog(LOG_DEBUG,"<D> %s:  %s:%-4d [current %d]: NEW TS [%u.%09u -> %u.%09u]  wrAddr %09llx\n",
                 timestr(),__FILE__,__LINE__,iarray,
                 _state[iarray].timestamp>>32,
                 _state[iarray].timestamp&0xffffffff,
                 current.timestamp>>32,
                 current.timestamp&0xffffffff,
                 current.wrAddr);

          array.reset(current.timestamp>>32,
                      current.timestamp&0xffffffff);
          current.nacq = 0; 
          record = _hw.getRecord(iarray,&current.next);  // read from beginning
        }
        else {
          //
          //  Incremental update
          //
          array.set(current.timestamp>>32,
                    current.timestamp&0xffffffff);
          record = _hw.get(iarray,_state[iarray].next,&current.next);
        }

      }
    }
    else {  // >= HSTARRAY0

      syslog(LOG_DEBUG,"<D> %s:  %s:%-4d [current %d]: wrAddr %09llx  next %09llx  clear %u  wrap %u  nacq %u",
             timestr(),__FILE__,__LINE__,iarray,current.wrAddr,current.next,current.clear,current.wrap,current.nacq);

      unsigned ifltb = array.array()-HSTARRAY0;
      Reader& reader = _reader[ifltb];
      if (_readerQueue.empty()) {
        reader.preset(current);  // prepare check for erroneous hw.done signal
        _readerQueue.push(ifltb);
        record = &_emptyRecord;
      }
      else if (_readerQueue.front()==ifltb) {
        if (reader.done()) {
          //  A new fault was latched
          current.nacq = 0;
          if (reader.reset(array,current,_hw,current.timestamp-(1ULL<<32))) {
            record = reader.next(array,_hw);
          }
          else {
            // We got the wrong done signal.  Find the correct one and queue it.
            _hw.reset(iarray);
            _readerQueue.pop();
            return 0;  // don't try to correct anything, just skip
          }
        }
        else {
          //  A previous fault readout is in progress
          record = reader.next(array,_hw);
        }

        if (reader.done())
          _readerQueue.pop();
      }
      else {  // Some other fault buffer readout is in progress
        record = &_emptyRecord;
      }
    }

    // syslog(LOG_DEBUG,"<W> %s:  %s:%-4d []: array %u  entries %u",
    // 	   timestr(),__FILE__,__LINE__,iarray,record->entries.size());

    for(unsigned i=0; i<record->entries.size(); i++) {
      const Entry& entry = record->entries[i];
      //  fill pulseid waveform
      array.append(entry.pulseId());
      //  fill channel data waveforms
      for(unsigned j=0; j<pvs.size(); j++)
        pvs[j]->append(entry.channel_data[j].n(),
                       entry.channel_data[j].mean(),
                       entry.channel_data[j].rms2());
    }

    current.nacq += record->entries.size();
    _state[iarray] = current;
  }
  catch(...) {
    // Something bad happened.  Abort this acquisition.
    syslog(LOG_ERR,"<E> %s:  %s:%-4d [current %d]: caught exception. abort. next 0x%09llx  wrAddr 0x%09llx  ts 0x%016llx",
           timestr(),__FILE__,__LINE__,iarray,_state[iarray].next,_state[iarray].wrAddr,_state[iarray].timestamp);
    abort(array);
    return 0;
  }

  return current.nacq;
}

//
//  Clear the state of this acquisition so it doesn't retry later.
//
void ProcessorImpl::abort(PvArray& array)
{
  int iarray = array.array();
  
  ArrayState current(_hw.state(iarray));
  array.reset(current.timestamp>>32,
              current.timestamp&0xffffffff);

  if (iarray < HSTARRAY0) {
  }
  else {
    unsigned ifltb = iarray-HSTARRAY0;
    Reader& reader = _reader[ifltb];
    if (!_readerQueue.empty() &&
        _readerQueue.front()==ifltb) {
      Reader& reader = _reader[ifltb];
      reader.abort();
    }
    else {
      // nothing to clear
    }
  }

  //  Still a firmware bug that allows WrAddr to go out of bounds.  Let's reset upon detection.
  _hw.reset(iarray);
  _state[iarray].next = _hw._begin[iarray];
}

Processor* Processor::create(Path reg,
                             Path ram,
                             bool lInit)
{
  return new ProcessorImpl(reg,ram,lInit);
}

Processor* Processor::create(const char* ip,
                             bool lInit,
                             bool lDebug)
{
  return new ProcessorImpl(ip,lInit,lDebug);
}

Processor* Processor::create()
{
  return new ProcessorImpl();
}

ProcessorImpl::~ProcessorImpl()
{
}
