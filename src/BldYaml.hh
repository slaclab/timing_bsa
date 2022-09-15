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
