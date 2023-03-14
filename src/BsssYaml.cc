#include "BsssYaml.hh"


using namespace Bsss;

BsssYaml::BsssYaml(Path bsss_path, uint32_t num_edef)
:AcqServiceYaml(bsss_path, num_edef, AcqService::bsss)
{
    int i = 0;
    char str_name[80];

    sprintf(str_name, "EdefRateLimit[%d]", i); _EdefRateLimit[i] = IScalVal::create(_path->findByName(str_name));

}


