#ifndef BsasYaml_hh
#define BsasYaml_hh


#include <cpsw_api_builder.h>
#include <stdint.h>
#include <vector>

#define  NUM_BSAS_MODULES   4
#define  NUM_BSAS_TABLES    4

namespace Bsas {
    class BsasControlYaml {
        public:
            BsasControlYaml(Path path);
            virtual ~BsasControlYaml() {}

            void Enable(uint32_t enable);
            void SetFixedRate(uint32_t rate);
            void SetACRate(uint32_t ts_mask, uint32_t rate);
            void SetSeqBit(uint32_t seq_bit);
            void SetSeqBit(uint32_t seq_num, uint32_t seq_bit);
            void SetInclusionMask(uint32_t mask);
            void SetExclusionMask(uint32_t mask);
            void DisableDestination(void);

        protected:
            Path        _path;      // path for control section
            ScalVal_RO  _count;     // register for count
            ScalVal     _enable;    // register for enable
            ScalVal     _rateSel;   // register for rateSel
            ScalVal     _destSel;   // register for destSel
    }; /* class BsasYaml */


    class BsasModuleYaml {
        public:
            BsasModuleYaml(Path path);
            virtual ~BsasModuleYaml() {}

            BsasControlYaml *pAcquire;
            BsasControlYaml *pRowAdvance;
            BsasControlYaml *pTableReset;

        protected:
            Path _path;
            Path _path_acquire;
            Path _path_rowAdvance;
            Path _path_tableReset;
    };


} /* namespace Bsas */

#endif  /* BsasYaml_hh */
