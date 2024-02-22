//////////////////////////////////////////////////////////////////////////////
// This file is part of 'timing_bsa'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'timing_bsa', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
#ifndef BldYaml_hh
#define BldYaml_hh

#include <cpsw_api_builder.h>
#include <stdint.h>
#include <vector>
#include "AcqServiceYaml.hh"

#define NUM_BLD_CHN   4

namespace Bld {
    class BldYaml : public AcqService::AcqServiceYaml {
        public:
            BldYaml(Path bld_path);
            virtual ~BldYaml() {}
            

    }; /* class BldYaml */

} /* namespace Bld */


#endif /* BldYaml_hh */
