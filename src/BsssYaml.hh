#ifndef BsssYaml_hh
#define BsssYaml_hh

#include <cpsw_api_builder.h>
#include <stdint.h>
#include <vector>


#define NUM_BSSS_CHN   9

namespace Bsss {
    class BsssYaml {
        public:
            BsssYaml(Path bsss_path);
            virtual ~BsssYaml() {}

            void getCurrPacketSize(uint32_t *size);
            void getCurrPacketState(uint32_t *status);
            void getCurrPulseIdL(uint32_t *pulseIdL);
            void getCurrTimeStampL(uint32_t *timeStampL);
            void getCurrDelta(uint32_t *delta);
            void getPacketCount(uint32_t *count);
            void getPaused(uint32_t *paused);
            void getDiagnClockRate(uint32_t *rate);
            void getDiagnStrobeRate(uint32_t *rate);
            void getEventSel0Rate(uint32_t *rate);

            // channel number corresponds to the  BSSS EDEF, rate controls
            void setDestInclusion(int chn, uint32_t dest_mask);
            void setDestExclusion(int chn, uint32_t dest_mask);
            void setDestDisable(int chn);
            void setFixedRate(int chn, uint32_t rate);
            void setACRate(int chn, uint32_t ts_mask, uint32_t rate);
            void setSeqRate(int chn, uint32_t seq_num, uint32_t seq_bit);
            void setRateLimit(int chn, uint32_t rate_limit);
            void setEdefEnable(int chn, uint32_t enable);

            // channel number correspnds to the BSSS data channels
            void setChannelSevr(int chn, uint32_t sevr);
            void setChannelMask(int chn, uint32_t enable);
            void setPacketSize(uint32_t size);
            void enablePacket(uint32_t enable);

        protected:
            Path _path;
            ScalVal _packetSize;                     // maximum size of packets in 32bit word
            ScalVal _enable;                         // enable packets
            ScalVal _channelMask;                    // mask of enabled channels
            ScalVal _channelSevr;                    // maximum channel severity, 2bits per each channel

            ScalVal_RO _currPacketSize;              // current packet size in 32bit word
            ScalVal_RO _currPacketState;             // current packet fill state
            ScalVal_RO _currPulseIdL;                // current packet pulse id lower word
            ScalVal_RO _currTimeStampL;              // current packet timestamp lower word
            ScalVal_RO _currDelta;                   // current compressed timestamp/pulse id
            ScalVal_RO _packetCount;                 // pcacket count
            ScalVal_RO _paused;                      // stream paused
            ScalVal_RO _diagnClockRate;              // diagnostic bus clock rate
            ScalVal_RO _diagnStrobeRate;             // diagnostic bus strobe rate
            ScalVal_RO _eventSel0Rate;               // event select0 rate

            ScalVal _EdefEnable[NUM_BSSS_CHN];       // enable edef
            ScalVal _EdefRateLimit[NUM_BSSS_CHN];    // rate limit
            ScalVal _EdefRateSel[NUM_BSSS_CHN];      // rate select
            ScalVal _EdefDestSel[NUM_BSSS_CHN];      // destination select
    }; /* class BsssYaml */

} /* namespace Bsss */


#endif /* BsssYaml_hh */
