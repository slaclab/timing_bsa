#include "BsssYaml.hh"


using namespace Bsss;

BsssYaml::BsssYaml(Path bsss_path)
:AcqServiceYaml(bsss_path, NUM_BSSS_CHN, AcqService::bsss)
{
    int i = 0;
    char str_name[80];

    sprintf(str_name, "EdefRateLimit[%d]", i); _EdefRateLimit[i] = IScalVal::create(_path->findByName(str_name));

}


