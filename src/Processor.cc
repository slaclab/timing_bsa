#include "Processor.hh"
#include "AmcCarrier.hh"
#include "AmcCarrierYaml.hh"
#include "BsaDefs.hh"

#include <cpsw_api_builder.h>

#include <queue>
#include <stdio.h>
#include <time.h>

#define DONE_WORKAROUND

static unsigned _nReadout = 1024 * 128;
static const unsigned MAXREADOUT = 1<<20;

static char* timestr() 
{
  time_t t = time(NULL);
  return asctime(localtime(&t));
}

namespace Bsa {

  class Reader {
  public:
    static void     set_nReadout(unsigned v) {_nReadout=v;} 
    static unsigned get_nReadout() { return _nReadout;}
  public:
    Reader() : _timestamp(0), _next(0), _last(0), _end(0), _preset(0), _record(MAXREADOUT) {}
    ~Reader() {}
  public:
    bool     done () const { return _next==_last; }
    void     preset (const ArrayState&    state) { _preset = state.wrAddr; }
    bool     reset(PvArray&              array, 
                   const ArrayState&     state,
                   AmcCarrierBase&       hw, 
                   uint64_t              timestamp)
    {
      //  Check if wrAddr pointer has moved.  If so, the acquisition isn't done
      if (state.wrAddr != _preset) {
        printf("%s:  %s:%-4d [reset]: not ready 0x%016llx  wrAddr 0x%016llx\n",
               timestr(),__FILE__,__LINE__,_preset,state.wrAddr);
        return false;
      }

      unsigned iarray = array.array();
      IndexRange rng(iarray);
      uint64_t start,end;
      hw._startAddr->getVal(&start,1,&rng);
      hw._endAddr  ->getVal(&end  ,1,&rng);

      _timestamp = state.timestamp;
      //  If wrap is set, the buffer is complete from wrAddr -> end; start -> wrAddr
      //  Else, if the buffer was read before, continue from where we ended
      //  Else read from the start
      _next = state.wrap ? state.wrAddr : (_last ? _last : start);
      _last = state.wrAddr;
      _end  = end;

      //  Check if wrAddr is properly aligned to the record size.
      unsigned n0 = _last / sizeof(Entry);
      if (n0*sizeof(Entry) != _last) {
        const Entry& e = *reinterpret_cast<const Entry*>(_last);
        printf("%s:  %s:%-4d [reset] misaligned _last 0x%016llx  nch %u  pid 0x%016llx\n",
               timestr(),__FILE__,__LINE__,_last,e.nchannels(),e.pulseId());
        return false;
      }

      array.reset(timestamp>>32,timestamp&0xffffffff);

      uint64_t hw_done = hw.done();
      printf("%s:  %s:%-4d []  array %u  _timestamp 0x%016llx  _next 0x%016llx  _last 0x%016llx  __end 0x%016llx  done 0x%016llx\n",
             timestr(),__FILE__,__LINE__,iarray,_timestamp,_next,_last,_end,hw_done);

      return true;
    }
    Record* next(PvArray& array, AmcCarrierBase& hw)
    {
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
        IndexRange rng(array.array());
        hw._startAddr->getVal(&nnext,1,&rng);
      }

      if (n > MAXREADOUT) {
        printf("ERROR: Reader::next allocating record with %u entries\n", n);
      }

      Record& record = _record;
      record.entries.resize(n);

      hw._fill(record.entries.data(), _next, next);

      //  Detect mis-aligned entries
      unsigned n0 = _next / sizeof(Entry);
      if (n0*sizeof(Entry) != _next) {
        const Entry& e = record.entries[0];
        printf("%s:  %s:%-4d [Misaligned record]  _next 0x%016llx  nch %u  pid 0x%016llx\n",
               timestr(),__FILE__,__LINE__,_next,e.nchannels(),e.pulseId());
      }
      
      //  _last or _end dont occur at an Entry boundary
      if (_next + n*sizeof(Entry) != next) {
        printf("%s:  %s:%-4d [Truncated record]  _next 0x%016llx  next 0x%016llx  _last 0x%016llx  _end 0x%016llx  _next+n 0x%016llx  n %u\n",
               timestr(),__FILE__,__LINE__,_next,next,_last,_end,_next+n*sizeof(Entry),n);
      }

      _next = nnext;

      if (done()) {
        hw   .reset(array.array());
        array.set(_timestamp>>32, _timestamp&0xffffffff);
        uint64_t hw_done = hw.done();
        printf("%s:%-4d [done]:  array %u  hw.done 0x%016llx\n",__FILE__,__LINE__,array.array(),hw_done);
      }

      return &record;
    }
  private:
    uint64_t _timestamp;
    uint64_t _next;
    uint64_t _last;
    uint64_t _end;
    uint64_t _preset;
    Record   _record;
  };

  class ProcessorImpl : public Processor {
  public:
    ProcessorImpl(Path reg,
                  Path ram,
                  bool lInit) : _hw(*new AmcCarrierYaml(reg,ram)), _debug(false)
    { if (lInit) _hw.initialize(); }
    ProcessorImpl(const char* ip,
                  bool lInit,
                  bool lDebug) : _hw(*new AmcCarrier(ip)), _debug(lDebug)
    { if (lInit) _hw.initialize(); }
    ProcessorImpl() : _hw(*AmcCarrier::instance()), _debug(false)
    {}
    ~ProcessorImpl();
  public:
    uint64_t pending();
    int      update(PvArray&);
    AmcCarrierBase *getHardware();
  private:
    AmcCarrierBase&      _hw;
    ArrayState           _state [HSTARRAYN];
    Reader               _reader[HSTARRAYN-HSTARRAY0];
    std::queue<unsigned> _readerQueue;
    bool                 _debug;
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

  if (array.array() < HSTARRAY0) {
    if (current != _state[iarray]) {
      current.nacq = _state[iarray].nacq;
      if (current.clear) {
        //
        //  New acquisition;  start from the beginning of the circular buffer
        //
        _hw.ackClear(iarray);

        if(_debug) {
          printf("NEW TS [%u] [%u.%09u -> %u.%09u]\n",
                 iarray,
                 _state[iarray].timestamp>>32,
                 _state[iarray].timestamp&0xffffffff,
                 current.timestamp>>32,
                 current.timestamp&0xffffffff);
        }
        array.reset(current.timestamp>>32,
                    current.timestamp&0xffffffff);
        current.nacq = 0; 
        record = _hw.getRecord(iarray,&current.next);
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

#if 1
    printf("%s:  %s:%-4d [current %d]: wrAddr %016llx  next %016llx  clear %u  wrap %u  nacq %u\n",
           timestr(),__FILE__,__LINE__,iarray,current.wrAddr,current.next,current.clear,current.wrap,current.nacq);
#endif

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
  return current.nacq;
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
