//////////////////////////////////////////////////////////////////////////////
// This file is part of 'timing_bsa'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'timing_bsa', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
#ifndef BsssYaml_hh
#define BsssYaml_hh

#include <cpsw_api_builder.h>
#include <stdint.h>
#include <vector>
#include "AcqServiceYaml.hh"

#define NUM_BSSS_CHN   9

namespace Bsss {
    class BsssYaml : public AcqService::AcqServiceYaml {
        public:
            BsssYaml(Path bsss_path, uint32_t num_edef);
            virtual ~BsssYaml() {}
            

    }; /* class BsssYaml */

} /* namespace Bsss */


#endif /* BsssYaml_hh */
