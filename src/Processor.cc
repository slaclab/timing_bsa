#include "Processor.hh"
#include "AmcCarrier.hh"
#include "AmcCarrierYaml.hh"
#include "BsaDefs.hh"

#include <cpsw_api_builder.h>

#include <cmath>
#include <math.h>
#include <queue>
#include <stdio.h>
#include <algorithm>
#include <cstddef>

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

    void procChannelData   (const Entry*, Pv*, Pv*, bool&, bool);
    void llrfPerformChecks (const Bsa::Pv*, const Bsa::Pv*, int);
    void llrfCalcPhaseAmp  (signed short, signed short, double&, double&);
  };

};

using namespace std;
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

void ProcessorImpl::llrfPerformChecks(const Bsa::Pv* pv, const Bsa::Pv* pvN, int bitIndex)
{
    // Check if channel pointers are nullptr
    if (pv == NULL || pvN == NULL)
    {
        printf("BsaPvArray::llrfPerformChecks(): ERROR - LLRF BSA channels are nullptr!!\n");
        printf("BsaPvArray::llrfPerformChecks(): ERROR - Check LLRF BSA channels!!\n");
        printf("BsaPvArray::llrfPerformChecks(): ERROR - Exiting ...\n");
        exit(EXIT_FAILURE);
    }
    // Ensure bit index points to the start of the channel
    if (bitIndex != 0)
    {
        printf("BsaPvArray::llrfPerformChecks(): ERROR - LLRF BSA channel not aligned with LSB!!\n");
        printf("BsaPvArray::llrfPerformChecks(): ERROR - Check LLRF BSA channel order!!\n");
        printf("BsaPvArray::llrfPerformChecks(): ERROR - Exiting ...\n");
        exit(EXIT_FAILURE);
    }
    // Ensure the types of consecutive channels are valid (phase->amp or amp->phase)
    if ((*pv->get_p_type() == llrfAmp   && *pvN->get_p_type() == llrfPhase) ||
        (*pv->get_p_type() == llrfPhase && *pvN->get_p_type() == llrfAmp  )   ){}
    else
    {
        printf("BsaPvArray::llrfPerformChecks(): ERROR - Did NOT find consecutive Amplitude and Phase channels!!\n");
        printf("BsaPvArray::llrfPerformChecks(): ERROR - Check LLRF BSA channel types!!\n");
        printf("BsaPvArray::llrfPerformChecks(): ERROR - Exiting ...\n");
        exit(EXIT_FAILURE);
    }
}

void ProcessorImpl::llrfCalcPhaseAmp(signed short i, signed short q, double& amp, double& phase)
{
    // Calculate amplitude
    amp = (!isnan(i) && !isnan(q))?sqrt(pow((double)i,2) + pow((double)q,2)):0.0;
    // Calculate phase
    phase = (!isnan(i) && !isnan(q) && i != 0)?atan2((double)q, (double)i) * M_PI_DEGREES / M_PI:0.0;
}

void ProcessorImpl::procChannelData(const Entry* entry, Pv* pv, Pv* pvN, bool& skipNextPV, bool done)
{
    uint32_t mask, val, iVal, qVal;
    double   amp, phase, quant1, quant2;

    // Keep track of bit boundaries
    static unsigned bitSum = 0;

    // Keep track of hardware channel index
    static unsigned wordIndex = 0;
    
    // Get the data type of the PV (32-bit or less)
    bsaDataType_t *type = pv->get_p_type();
            
    // Partition 32-bit data and distribute to new PVs as needed
    switch(*type){
      case uint2:
        // Extract the 2-bit block
        val = (uint32_t)entry->channel_data[wordIndex].mean();
        mask = KEEP_LSB_2;val >>= bitSum;val &= mask;
        bitSum += BLOCK_WIDTH_2;
        break; 
      case uint16:
        // Extract the 16-bit block
        val = (uint32_t)entry->channel_data[wordIndex].mean();
        mask = KEEP_LSB_16;val >>= bitSum;val &= mask;
        bitSum += BLOCK_WIDTH_16;
        break;
      case llrfAmp:
      case llrfPhase:
        // Perform checks to ensure type validity
        llrfPerformChecks(const_cast<Bsa::Pv*>(pv),const_cast<Bsa::Pv*>(pvN),bitSum);
        // Extract lower 16 bits
        iVal = (uint32_t)entry->channel_data[wordIndex].mean();
        mask = KEEP_LSB_16;iVal >>= bitSum;iVal &= mask;
        // Extract upper 16 bits
        qVal = (uint32_t)entry->channel_data[wordIndex].mean();
        mask = KEEP_LSB_16;qVal >>= BLOCK_WIDTH_16;qVal &= mask;
        // Compute phase & amplitude
        llrfCalcPhaseAmp(static_cast<signed short>(iVal),static_cast<signed short>(qVal),amp,phase);                
        // Append computed values to PVs
        quant1 = (*type == llrfAmp)?amp:phase;
        quant2 = (quant1 == amp   )?phase:amp;
        pv->append  (entry->channel_data[wordIndex].n(), 
                     quant1,
                     entry->channel_data[wordIndex].rms2());
        pvN->append (entry->channel_data[wordIndex].n(), 
                     quant2,
                     entry->channel_data[wordIndex].rms2());
        skipNextPV = true;
        bitSum += 2 * BLOCK_WIDTH_16;
        break;
      case int32:
      case uint32:
      case float32:
      default:
        // Send data as is (no partition required)
        val = (uint32_t)entry->channel_data[wordIndex].mean();
        bitSum += BLOCK_WIDTH_32;
    }
    // Append the value to the corresponding PV history
    if (*type != llrfAmp && *type != llrfPhase)
      pv->append(entry->channel_data[wordIndex].n(), 
                 (double)val, 
                 entry->channel_data[wordIndex].rms2());
            
    // Check if the 32-bit boundary has been violated
    if (bitSum == BLOCK_WIDTH_32)
    {
      // All good, move on to the next 32-bit word
      bitSum = 0;
      // Increment index to next channel
      wordIndex++;
    }
    else if (bitSum > BLOCK_WIDTH_32)
    {
      printf("ProcessorImpl::procChannelData(): ERROR - Please ensure BSA channels do not violate 32-bit boundaries!!\n");
      printf("ProcessorImpl::procChannelData(): ERROR - Check BSA channel type!!\n");
      printf("ProcessorImpl::procChannelData(): ERROR - Exiting ...\n");
      exit(EXIT_FAILURE);
    }

    // Reset hardware channel index if done with all hardware channels
    wordIndex = (done)?0:wordIndex;
}

int ProcessorImpl::update(PvArray& array)
{
  unsigned      iarray  = array.array();
  std::vector<Pv*> pvs  = array.pvs();
  unsigned    numOfPVs  = pvs.size();

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

  unsigned numOfEntries = record->entries.size();
  
  for(unsigned i = 0; i < numOfEntries; i++) 
  {
    // Process next entry (i.e. pulse ID)
    const Entry& entry = record->entries[i];

    // Fill pulseid waveform
    array.append(entry.pulseId());

    // Fill channel data waveforms
/*
    // Method 1: Call Bsa::PvArray::procChannelData() 
    // Push data to a vector in bsaDriver and extract data in there
    for(unsigned j = 0; j < std::min(numChannelData, (const int&)numOfPVs); j++)
    {
      // Adding new call to procChannelData() here that will do the extraction of the channel data.
      // A boolean argument indicates if we are done sending all the channel data for the current pulse. 
      // Note the data are sent as 32-bit chunks and those are splitted as needed in the Asyn driver.
      array.procChannelData(entry.channel_data[j].n(),
                            entry.channel_data[j].mean(),
                            entry.channel_data[j].rms2(),
                            (j == std::min(numChannelData - 1, (const int&)numOfPVs - 1)));
    }
*/
    // Method 2: Call ProcessorImpl::procChannelData() 
    // Loop over PVs, extract data from hardware channels and assign to PVs 
    for(unsigned int j = 0; j < numOfPVs; j++)
    {
      // Call function to do the extraction
      Pv* pv  = pvs[j]; bool skip = false;
      Pv* pvN = ((j+1) < numOfPVs)?pvs[j+1]:pv;
      if (pv && pvN) procChannelData(&entry, pv, pvN, skip, j == (numOfPVs - 1));
      j = (skip)?j+1:j;
    }
  }

  current.nacq += numOfEntries;
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
