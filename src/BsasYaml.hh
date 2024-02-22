//////////////////////////////////////////////////////////////////////////////
// This file is part of 'timing_bsa'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'timing_bsa', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
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

            void getCount(uint32_t *count);
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

            void Enable(uint32_t enable);
            void SetChannelMask(uint32_t mask);
            void SetChannelMask(int chn, uint32_t mask);
            void SetChannelSeverity(uint64_t sevr);
            void SetChannelSeverity(int chn, uint64_t sevr);

            BsasControlYaml *pAcquire;
            BsasControlYaml *pRowAdvance;
            BsasControlYaml *pTableReset;

        protected:
            Path _path;
            Path _path_bsasStream;
            Path _path_acquire;
            Path _path_rowAdvance;
            Path _path_tableReset;

            ScalVal  _enable;          // BSAS stream control, enable and disable
            ScalVal  _channelMask;     // BSAS stream control, enable and disable the data channel
            ScalVal  _channelSevr;     // BSAS stream control, severity filtering
    };


} /* namespace Bsas */

#endif  /* BsasYaml_hh */
