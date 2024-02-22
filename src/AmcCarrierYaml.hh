//////////////////////////////////////////////////////////////////////////////
// This file is part of 'timing_bsa'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'timing_bsa', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
#ifndef Bsa_AmcCarrierYaml_hh
#define Bsa_AmcCarrierYaml_hh

#include <cpsw_api_builder.h>

#include <stdint.h>
#include <vector>

#include <AmcCarrierBase.hh>
#include <BsaDefs.hh>

namespace Bsa {
  class AmcCarrierYaml : public AmcCarrierBase {
  public:
    AmcCarrierYaml(Path        mmio,
                   Path        dram);
    ~AmcCarrierYaml();
  public:
    //  BSA buffers
    unsigned nArrays   () const;
    Record*  get       (unsigned array,
                        uint64_t begin,
                        uint64_t* next) const;

    void     dump      () const;
  public:
    //  Raw Diagnostics
    void      initializ_(unsigned index,
                         uint64_t bufferSize,
                         bool     doneWhenFull=false);
    void      rearm     (unsigned index);
    uint32_t  doneRaw   () const;
    RingState ring      (unsigned array) const;
    uint8_t*  get       (uint64_t begin,
                         uint64_t end  ) const;
  public:
    void      clear     (unsigned array);
  private:
    void      _fill     (void*    dst,
                         uint64_t begin,
                         uint64_t end) const;
  };
};

#endif
