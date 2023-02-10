#include "Processor.hh"
#include "AmcCarrier.hh"
#include "AmcCarrierYaml.hh"
#include "BsaDefs.hh"

#include <cpsw_api_builder.h>

#include <queue>
#include <stdio.h>

static unsigned _nReadout = 1024 * 128;

namespace Bsa {

  class Reader {
  public:
    static void     set_nReadout(unsigned v) {_nReadout=v;} 
    static unsigned get_nReadout() { return _nReadout;}
  public:
    Reader() : _timestamp(0), _next(0), _last(0), _end(0) {}
    ~Reader() {}
  public:
    bool     done () const { return _next==_last; }
    void     reset(PvArray&              array, 
                   const ArrayState&     state,
                   AmcCarrierBase&       hw, 
                   uint64_t              timestamp)
    {
      unsigned iarray = array.array();
      IndexRange rng(iarray);
      uint64_t start,end;
      hw._startAddr->getVal(&start,1,&rng);
      hw._endAddr  ->getVal(&end  ,1,&rng);

      _timestamp = state.timestamp;
      _next = state.wrap ? state.wrAddr : start;
      _last = state.wrAddr;
      _end  = end;

      array.reset(timestamp>>32,timestamp&0xffffffff);
    }
    Record* next(PvArray& array, AmcCarrierBase& hw)
    {
      unsigned n = _nReadout;
      //  _next : start of current read
      //  _last : end of final read
      //  _end  : end of buffer where we need to wrap
      uint64_t next = _next + n*sizeof(Entry);  // end of current read
      uint64_t nnext = next;                    // start of next read

      if (next > _end) {  // truncate at the wrap point
        next = _end;
        n = (next - _next) / sizeof(Entry);
        IndexRange rng(array.array());
        hw._startAddr->getVal(&nnext,1,&rng);
      }
      else if (_next < _last && _last < next) {  // truncate to _last
        n = (_last - _next) / sizeof(Entry);
        nnext = _last;
        next  = _last;
      }

      Record* record = new Record;
      record->entries.resize(n);

      hw._fill(record->entries.data(), _next, next);
      _next = nnext;

      if (done()) {
        hw   .reset(array.array());
        array.set(_timestamp>>32, _timestamp&0xffffffff);
      }

      return record;
    }
  private:
    uint64_t _timestamp;
    uint64_t _next;
    uint64_t _last;
    uint64_t _end;
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

#if 0
  printf("current[%d]: wrAddr %016llx  next %016llx  clear %u  wrap %u  nacq %u\n",
         iarray,current.wrAddr,current.next,current.clear,current.wrap,current.nacq);
  printf("state  [%d]: wrAddr %016llx  next %016llx  clear %u  wrap %u  nacq %u\n",
         iarray,_state[iarray].wrAddr,_state[iarray].next,_state[iarray].clear,_state[iarray].wrap,_state[iarray].nacq);
#endif

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
    unsigned ifltb = array.array()-HSTARRAY0;
    if (_readerQueue.empty()) {
      _readerQueue.push(ifltb);
      record = new Record;
    }
    else if (_readerQueue.front()==ifltb) {
      Reader& reader = _reader[ifltb];
      if (reader.done()) {
        //  A new fault was latched
        current.nacq = 0;
        reader.reset(array,current,_hw,current.timestamp-(1ULL<<32));
        record = reader.next(array,_hw);
      }
      else {
        //  A previous fault readout is in progress
        record = reader.next(array,_hw);
      }

      if (reader.done())
        _readerQueue.pop();
    }
    else {  // Some other fault buffer readout is in progress
      record = new Record;
    }
  }

  for(unsigned i=0; i<record->entries.size(); i++) {
    // Process next entry (i.e. pulse ID)
    const Entry& entry = record->entries[i];

    //  Fill pulseid waveform
    array.append(entry.pulseId());

    //  Fill channel data waveforms
    for(unsigned j=0; j<numChannelData; j++)
    {
      // Adding new call to procChannelData() here that will do the partitioning of the channel data.
      // Two new parameters are passed: the number of user-defined BSA channels (i.e. pvs.size())
      // and a boolean to indicate if we are done sending all the channel data for the current pulse. 
      // Note the data are sent as 32-bit chunks and those are splitted as needed in the Asyn driver.
      array.procChannelData(entry.channel_data[j].n(),
                            entry.channel_data[j].mean(),
                            entry.channel_data[j].rms2(),
                            (j == (numChannelData - 1) || j == (pvs.size() - 1)));
    }
  }

  current.nacq += record->entries.size();
  delete record;
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
