#ifndef Bsa_Processor_hh
#define Bsa_Processor_hh

#include "BsaField.hh"
#include "AmcCarrierBase.hh"

#include <vector>
#include <stdint.h>

#include <cpsw_api_user.h>

#define BLOCK_WIDTH_2   2
#define BLOCK_WIDTH_16  16
#define BLOCK_WIDTH_32  32

#define KEEP_LSB_2    0x00000003 
#define KEEP_LSB_16   0x0000ffff 
#define DEFAULT_MASK  0xffffffff

#define M_PI_DEGREES 180.0

namespace Bsa {
  typedef enum {
    uint2,
    uint16,
    int32,
    uint32,
    uint64,
    float32,
    fault,
    llrfAmp,
    llrfPhase
  } bsaDataType_t;
  //
  //  Class that defines the interface from the BSA processor to a PV record
  //
  class Pv {
  public:
    virtual ~Pv() {}
  public:
    //
    //  Field that this record represents
    //
    //    virtual const Field& field() const = 0;
  public:
    //
    //  Update the timestamp of the record
    //
    virtual void setTimestamp(unsigned sec, 
                              unsigned nsec) = 0;
    //
    //  Clear the data contents of the record
    //
    virtual void clear() = 0;
    //
    //  Append an entry to the record
    //
    //    virtual void append() = 0;  // exception during acquisition
    virtual void append(unsigned n,   // could be zero
                        double   mean,
                        double   rms2) = 0;
    //
    //  Flush the data out
    //
    virtual void flush() = 0;
    //
    // Return BSA datatype for this PV 
    //  
    //
    virtual bsaDataType_t * get_p_type() const { return _p_type; }
 protected:
    bsaDataType_t *_p_type;
};

  //
  //  Class that consists of the PV records for one BSA array
  //
  class PvArray {
  public:
    virtual ~PvArray() {}
  public:
    //
    //  The BSA array number service by these PVs
    //
    virtual unsigned array() const = 0;
    //
    //  Clear array and start with new timestamp
    //
    virtual void reset(uint32_t sec,
                       uint32_t nsec) = 0;
    //
    //  Update timestamp
    //
    virtual void set(uint32_t sec,
                     uint32_t nsec) = 0;
    //
    //
    //
    virtual void append(uint64_t pulseId) = 0;
    //
    //  The PV records assigned to this array number
    //  (element order matches diagnostic bus element order)
    //
    virtual std::vector<Pv*> pvs() = 0;
    //
    // Add the capability to preprocess the data prior to assigning to PVs 
    //  
    //
    virtual void procChannelData(unsigned, double, double, bool) = 0;
};

  //
  //  The interface for updating PV records from the AmcCarrier buffers
  //
  class Processor {
  public:
    virtual ~Processor() {}
    //
    //  Create the interface to the AmcCarrier with paths to register and ram access
    //
    static Processor* create(Path reg, Path ram, bool lInit=false);
    //
    //  Create the interface to the AmcCarrier with IP address "ip"
    //
    static Processor* create(const char* ip, bool lInit=false, bool lDebug=false);
    //
    //  Share the interface to the AmcCarrier
    //
    static Processor* create();
  public:
    //
    //  Fetch a bit mask of arrays with pending data
    //
    virtual uint64_t pending() = 0;
    //
    //  Update the array of PV records for one BSA buffer
    //  Return value indicates if any records were changed
    //
    virtual int update(PvArray&) = 0;
    //
    //  
    //
    virtual AmcCarrierBase *getHardware() = 0;
  };
};

#endif
