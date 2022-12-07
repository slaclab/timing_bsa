#include "Processor.hh"
#include "AmcCarrier.hh"
#include "AmcCarrierYaml.hh"

#include <stdio.h>

namespace Bsa {
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
    AmcCarrierBase& _hw;
    ArrayState  _state[64];
    bool        _debug;
  };
};

using namespace Bsa;

enum { BSA_FAULT_DIAG = 60 };

AmcCarrierBase *ProcessorImpl::getHardware()
{
    return &_hw;
}

uint64_t ProcessorImpl::pending()
{
  const std::vector<ArrayState>& s = _hw.state();

  uint64_t r = 0;
  for(unsigned i=0; i<64; i++)
    if (s[i] != _state[i])
      r |= 1ULL<<i;

  uint64_t done = _hw.done();
  done |= (1ULL<<60)-1;

  r &= done;

  return r;
}

int ProcessorImpl::update(PvArray& array)
{
  unsigned      iarray = array.array();
  std::vector<Pv*> pvs = array.pvs();
  ArrayState current(_hw.state(iarray));

  if (current != _state[iarray]) {

    //
    //  Fault diagnostics are large and should only be accessed when latched
    //
    if (array.array() >= BSA_FAULT_DIAG &&
        !_hw.done(array.array()))
      return 0;

    Record* record;

    current.nacq = _state[iarray].nacq;

    if (array.array() < BSA_FAULT_DIAG) {
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
    else {
      //
      //  For fault diagnostics, just replace the entire waveform with new data.
      //
      array.reset(current.timestamp>>32,
                  current.timestamp&0xffffffff);
      if (current.wrap)
        record = _hw.getRecord(iarray,current.wrAddr);
      else
        record = _hw.getRecord(iarray);
    }

    if (_debug) {
      printf("Record [%p]:  buffer [%u], time %u.%09u  entries [%u]\n",
             record, record->buffer, record->time_secs, record->time_nsecs, record->entries.size());
#if 0
      printf("%16.16s %8.8s %26.26s %26.26s\n",
             "PulseID","Channels","Channel0","Channel1");
      for(unsigned i=0; i<record->entries.size(); i++) {
        printf("%016llx %08x %08x:%08x:%08x %08x:%08x:%08x\n",
               record->entries[i].pulseId(),
               record->entries[i].nchannels(),
               record->entries[i].channel_data[0].data[0],
               record->entries[i].channel_data[0].data[1],
               record->entries[i].channel_data[0].data[2],
               record->entries[i].channel_data[1].data[0],
               record->entries[i].channel_data[1].data[1],
               record->entries[i].channel_data[1].data[2]);
      }
#else
      for(unsigned i=0; i<record->entries.size(); i++) {
        printf("Hdr: %016llx %08x\n", 
               record->entries[i].pulseId(), 
               record->entries[i].nchannels());
        for(unsigned j=0; j<31; j++) {
          printf(" %08x:%08x:%08x",
                 record->entries[i].channel_data[j].data[0],
                 record->entries[i].channel_data[j].data[1],
                 record->entries[i].channel_data[j].data[2]);
        }
        printf("\n");
      }
#endif
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
    delete record;
  }

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
