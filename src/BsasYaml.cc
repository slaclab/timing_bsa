#include "BsasYaml.hh"

#include <cpsw_api_builder.h>
#include <cpsw_mmio_dev.h>

#define CPSW_TRY_CATCH(X)       try {   \
        (X);                            \
    } catch (CPSWError &e) {            \
        fprintf(stderr,                 \
                "CPSW Error: %s at %s, line %d\n",     \
                e.getInfo().c_str(),    \
                __FILE__, __LINE__);    \
        throw e;                        \
    }


using namespace Bsas;


BsasControlYaml::BsasControlYaml(Path path)
{
    _path = path;
    CPSW_TRY_CATCH(_count   = IScalVal_RO::create(_path->findByName("count")));
    CPSW_TRY_CATCH(_enable  = IScalVal   ::create(_path->findByName("Enable")));
    CPSW_TRY_CATCH(_rateSel = IScalVal   ::create(_path->findByName("RateSel")));
    CPSW_TRY_CATCH(_destSel = IScalVal   ::create(_path->findByName("DestSel")));

}


void BsasControlYaml::Enable(uint32_t enable)
{
}

void BsasControlYaml::SetFixedRate(uint32_t rate)
{
    uint32_t _rate = (0x000000<<11) | (0x0000000f & rate);
    CPSW_TRY_CATCH(_rateSel->setVal(&_rate));

}

void BsasControlYaml::SetACRate(uint32_t ts_mask, uint32_t rate)
{
    uint32_t _rate = (0x00000001<<11) | (0x0000003f & ts_mask)<< 3 | (0x00000007 & rate);
    CPSW_TRY_CATCH(_rateSel->setVal(&_rate));
}

void BsasControlYaml::SetSeqBit(uint32_t seq_bit)
{
    uint32_t _seq_bit = (0x00000002<<11) | (0x000003ff & seq_bit);
    CPSW_TRY_CATCH(_rateSel->setVal(&_seq_bit));
}

void BsasControlYaml::SetSeqBit(uint32_t seq_num, uint32_t seq_bit)
{
    uint32_t _seq_bit = (seq_num &0x1f) << 4 | (seq_bit & 0xf);
    uint32_t _seq     = (0x00000002<<11) | (0x000003ff & _seq_bit);
    CPSW_TRY_CATCH(_rateSel->setVal(&_seq));
}

void BsasControlYaml::SetInclusionMask(uint32_t dest_mask)
{
    uint32_t _mask = (0x00000000<<16) | (0x0000ffff & dest_mask);
    CPSW_TRY_CATCH(_destSel->setVal(&_mask))
}

void BsasControlYaml::SetExclusionMask(uint32_t dest_mask)
{
    uint32_t _mask = (0x00000001<<16) | (0x0000ffff & dest_mask);
    CPSW_TRY_CATCH(_destSel->setVal(&_mask));

}

void BsasControlYaml::DisableDestination(void)
{
    uint32_t _mask = (0x00000002<<16);
    CPSW_TRY_CATCH(_destSel->setVal(&_mask));
}


BsasModuleYaml::BsasModuleYaml(Path path)
{
    _path = path;
    CPSW_TRY_CATCH(_path_acquire    = _path->findByName("Acquire"));
    CPSW_TRY_CATCH(_path_rowAdvance = _path->findByName("RowAdvance"));
    CPSW_TRY_CATCH(_path_tableReset = _path->findByName("TableReset"));

    CPSW_TRY_CATCH(pAcquire    = new BsasControlYaml(_path_acquire));
    CPSW_TRY_CATCH(pRowAdvance = new BsasControlYaml(_path_rowAdvance));
    CPSW_TRY_CATCH(pTableReset = new BsasControlYaml(_path_tableReset));
}


