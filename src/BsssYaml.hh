#ifndef BsssYaml_hh
#define BsssYaml_hh

#include <cpsw_api_builder.h>
#include <stdint.h>
#include <vector>
#include "AcqServiceYaml.hh"

#define NUM_BSSS_CHN   9

namespace Bsss {
    class BsssYaml : AcqService::AcqServiceYaml {
        public:
            BsssYaml(Path bsss_path);
            virtual ~BsssYaml() {}
            

    }; /* class BsssYaml */

} /* namespace Bsss */


#endif /* BsssYaml_hh */
