#include "AcqServiceYaml.hh"

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

#define DEST_MODE_BITLOC        16        // bit location for destination mode
#define DEST_MASK               0xffff    // mask for destination
#define DEST_INCLUSION          0x0       // destination for inclusion mode
#define DEST_EXCLUSION          0x1       // destination for exclusion mode
#define DEST_DISABLE            0x2       // destination for disable

#define RATE_MODE_BITLOC        11        // bit location for rate mode
#define RATE_FIXED              0x0       // rate mode - fixed mode
#define RATE_AC                 0x1       // rate mode - AC mode
#define RATE_SEQ                0x2       // rate mode - Seq mode
#define RATE_FIXED_MASK         0xf       // rate mask for fixed rate
#define RATE_AC_MASK            0x7       // rate mask for ac rate
#define RATE_AC_TS_MASK         0x3f      // timeslot mask for ac rate
#define RATE_AC_TS_BITLOC       3         // bit location for timeslot mask
#define RATE_SEQ_MASK           0xf       // rate mask for seq mode
#define RATE_SEQ_NUM_MASK       0x1f      // seq num mask
#define RATE_SEQ_NUM_BITLOC     4         // bit location for seq num


using namespace AcqService;

AcqServiceYaml::AcqServiceYaml(Path AcqService_path, uint32_t edef_num)
{
    // Make sure no overflow will occur
    assert ( edef_num <= MAX_EDEFS );

    _path        = AcqService_path;
    _packetSize  = IScalVal::create(_path->findByName("packetSize"));
    _enable      = IScalVal::create(_path->findByName("enable"));
    _channelMask = IScalVal::create(_path->findByName("channelMask"));
    
    _currPacketSize  = IScalVal_RO::create(_path->findByName("currPacketSize"));
    _currPacketState = IScalVal_RO::create(_path->findByName("currPacketState"));
    _currPulseIdL    = IScalVal_RO::create(_path->findByName("currPulseIdL"));
    _currTimeStampL  = IScalVal_RO::create(_path->findByName("currTimeStampL"));
    _currDelta       = IScalVal_RO::create(_path->findByName("currDelta"));
    _packetCount     = IScalVal_RO::create(_path->findByName("packetCount"));
    _paused          = IScalVal_RO::create(_path->findByName("paused"));
    _diagnClockRate  = IScalVal_RO::create(_path->findByName("diagnClockRate"));
    _diagnStrobeRate = IScalVal_RO::create(_path->findByName("diagnStrobeRate"));
    _eventSel0Rate   = IScalVal_RO::create(_path->findByName("eventSel0Rate"));

    _edef_num = edef_num;

}

void AcqServiceYaml::getCurrPacketSize(uint32_t *size)
{
    CPSW_TRY_CATCH(_currPacketSize->getVal(size));
}

void AcqServiceYaml::getCurrPacketState(uint32_t *state)
{
    CPSW_TRY_CATCH(_currPacketState->getVal(state));
}

void AcqServiceYaml::getCurrPulseIdL(uint32_t *pulseIdL)
{
    CPSW_TRY_CATCH(_currPulseIdL->getVal(pulseIdL));
}

void AcqServiceYaml::getCurrTimeStampL(uint32_t *timeStampL)
{
    CPSW_TRY_CATCH(_currTimeStampL->getVal(timeStampL));
}

void AcqServiceYaml::getCurrDelta(uint32_t *delta)
{
    CPSW_TRY_CATCH(_currDelta->getVal(delta));
}

void AcqServiceYaml::getPacketCount(uint32_t *count)
{
    CPSW_TRY_CATCH(_packetCount->getVal(count));
}

void AcqServiceYaml::getPaused(uint32_t *paused)
{
    CPSW_TRY_CATCH(_paused->getVal(paused));
}

void AcqServiceYaml::getDiagnClockRate(uint32_t *rate)
{
    CPSW_TRY_CATCH(_diagnClockRate->getVal(rate));
}

void AcqServiceYaml::getDiagnStrobeRate(uint32_t *rate)
{
    CPSW_TRY_CATCH(_diagnStrobeRate->getVal(rate));
}

void AcqServiceYaml::getEventSel0Rate(uint32_t *rate)
{
    CPSW_TRY_CATCH(_eventSel0Rate->getVal(rate));
}


void AcqServiceYaml::setDestInclusion(int chn, uint32_t dest_mask)
{
    uint32_t destSel = (DEST_INCLUSION<<DEST_MODE_BITLOC) | (DEST_MASK & dest_mask);
    CPSW_TRY_CATCH(_EdefDestSel[chn]->setVal(destSel));
}

void AcqServiceYaml::setDestExclusion(int chn, uint32_t dest_mask)
{
    uint32_t destSel = (DEST_EXCLUSION<<DEST_MODE_BITLOC) | (DEST_MASK & dest_mask);
    CPSW_TRY_CATCH(_EdefDestSel[chn]->setVal(destSel));
}

void AcqServiceYaml::setDestDisable(int chn)
{
    uint32_t destSel = (DEST_DISABLE<<DEST_MODE_BITLOC);
    CPSW_TRY_CATCH(_EdefDestSel[chn]->setVal(destSel));
}


void AcqServiceYaml::setFixedRate(int chn, uint32_t rate)
{
    uint32_t rateSel = (RATE_FIXED<<RATE_MODE_BITLOC)
                       | (RATE_FIXED_MASK & rate);
    CPSW_TRY_CATCH(_EdefRateSel[chn]->setVal(rateSel));
}

void AcqServiceYaml::setACRate(int chn, uint32_t ts_mask, uint32_t rate)
{
    uint32_t rateSel = (RATE_AC<<RATE_MODE_BITLOC)
                       | (RATE_AC_TS_MASK & ts_mask) << RATE_AC_TS_BITLOC
                       | (RATE_AC_MASK & rate);
    CPSW_TRY_CATCH(_EdefRateSel[chn]->setVal(rateSel));
}

void AcqServiceYaml::setSeqRate(int chn, uint32_t seq_num, uint32_t seq_bit)
{
    uint32_t rateSel = (RATE_SEQ<<RATE_MODE_BITLOC)
                       | (RATE_SEQ_NUM_MASK & seq_num) << RATE_SEQ_NUM_BITLOC
                       | (RATE_SEQ_MASK & seq_bit);
    CPSW_TRY_CATCH(_EdefRateSel[chn]->setVal(rateSel));
}

void AcqServiceYaml::setRateLimit(int chn, uint32_t rate_limit)
{
    CPSW_TRY_CATCH(_EdefRateLimit[chn]->setVal(rate_limit));
}

void AcqServiceYaml::setEdefEnable(int chn, uint32_t enable)
{
    CPSW_TRY_CATCH(_EdefEnable[chn]->setVal(enable?(uint32_t) 1: (uint32_t) 0));
}

void AcqServiceYaml::setChannelMask(int chn, uint32_t enable)
{
    uint32_t channelMask;

    CPSW_TRY_CATCH(_channelMask->getVal(&channelMask));
    channelMask &= ~(0x1 << chn);                  /* clear mask */
    channelMask |= (0x1 & enable?0x1:0x0) << chn;  /* set mask */
    CPSW_TRY_CATCH(_channelMask->setVal(channelMask));
}

void AcqServiceYaml::setChannelMask(uint32_t mask)
{
    CPSW_TRY_CATCH(_channelMask->setVal(mask));
}

void AcqServiceYaml::setPacketSize(uint32_t size)
{
    CPSW_TRY_CATCH(_packetSize->setVal(size));
}

void AcqServiceYaml::enablePacket(uint32_t enable)
{
    CPSW_TRY_CATCH(_enable->setVal(enable?(uint32_t) 1: (uint32_t) 0));
}



