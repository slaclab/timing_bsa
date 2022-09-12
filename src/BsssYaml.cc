#include "BsssYaml.hh"


using namespace Bsss;

BsssYaml::BsssYaml(Path bsss_path)
:AcqServiceYaml(bsss_path, NUM_BSSS_CHN)
{
    for(int i = 0; i < _edef_num; i++) {
        char str_name[80];

        sprintf(str_name, "EdefEnable[%d]",    i); _EdefEnable[i]    = IScalVal::create(_path->findByName(str_name));
        sprintf(str_name, "EdefRateLimit[%d]", i); _EdefRateLimit[i] = IScalVal::create(_path->findByName(str_name));
        sprintf(str_name, "EdefRateSel[%d]",   i); _EdefRateSel[i]   = IScalVal::create(_path->findByName(str_name));
        sprintf(str_name, "EdefDestSel[%d]",   i); _EdefDestSel[i]   = IScalVal::create(_path->findByName(str_name));
    }
}


