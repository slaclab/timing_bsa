//////////////////////////////////////////////////////////////////////////////
// This file is part of 'timing_bsa'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'timing_bsa', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
#include "BldYaml.hh"


using namespace Bld;

BldYaml::BldYaml(Path bld_path)
:AcqServiceYaml(bld_path, NUM_BLD_CHN, AcqService::bld)
{
    for(int i = 0; i < _edef_num; i++) {
        char str_name[80];

        sprintf(str_name, "EdefEnable[%d]",    i); _EdefEnable[i]    = IScalVal::create(_path->findByName(str_name));
        sprintf(str_name, "EdefRateLimit[%d]", i); _EdefRateLimit[i] = IScalVal::create(_path->findByName(str_name));
        sprintf(str_name, "EdefRateSel[%d]",   i); _EdefRateSel[i]   = IScalVal::create(_path->findByName(str_name));
        sprintf(str_name, "EdefDestSel[%d]",   i); _EdefDestSel[i]   = IScalVal::create(_path->findByName(str_name));
    }
}


