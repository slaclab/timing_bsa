#ifndef Bsa_AmcCarrier_hh
#define Bsa_AmcCarrier_hh

#include <cpsw_api_builder.h>

#include <stdint.h>
#include <vector>

#include <AmcCarrierBase.hh>
#include <BsaDefs.hh>

namespace Bsa {
  class AmcCarrierCallback {
  public:
    virtual ~AmcCarrierCallback() {}
  public:
    virtual int process   (uint64_t) = 0;
    virtual int processTmo() = 0;
  };

  class AmcCarrier : public AmcCarrierBase {
  public:
    static void        build   (MMIODev);
    static AmcCarrier* instance();
    static AmcCarrier* instance(Path);
  public:
    AmcCarrier(const char* ip,
               bool        lTPG=false);
    AmcCarrier(Path        path);
    ~AmcCarrier();
  public:
    //  BSA buffers
    unsigned nArrays   () const;
    Record*  get       (unsigned array,
                        uint64_t begin,
                        uint64_t* next) const;

    void     dump      () const;
  public:
    //  Raw Diagnostics
    void      initRaw   (unsigned index,
                         uint64_t bufferSize,
                         bool     doneWhenFull=false);
    void      rearm     (unsigned index);
    uint32_t  doneRaw   () const;
    RingState ring      (unsigned array) const;
  public:
    //  Launch BSA locally
    void     start     (unsigned array,
                        unsigned rate,
                        unsigned nacq,
                        unsigned naccum=1,
                        unsigned sevr=3);
    void     start     (unsigned array,
                        BeamSelect, 
                        RateSelect,
                        unsigned nacq,
                        unsigned naccum=1,
                        unsigned sevr=3);
    void     poll      (AmcCarrierCallback&);
    void     clear     (unsigned array);
  public:
    void     handleIrq ();
  private:
    void     _fill     (void*    dst,
                        uint64_t begin,
                        uint64_t end) const;
  private:
    ScalVal    _sCmpl;
    ScalVal_RO _sStat;
  };
};

#endif
