//////////////////////////////////////////////////////////////////////////////
// This file is part of 'timing_bsa'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'timing_bsa', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
#include "BsssYaml.hh"


using namespace Bsss;

BsssYaml::BsssYaml(Path bsss_path, uint32_t num_edef)
:AcqServiceYaml(bsss_path, num_edef, AcqService::bsss)
{
    int i = 0;
    char str_name[80];

    sprintf(str_name, "EdefRateLimit[%d]", i); _EdefRateLimit[i] = IScalVal::create(_path->findByName(str_name));

}


