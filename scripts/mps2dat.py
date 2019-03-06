import sys

fields = [{'input' : 'PIDU'      , 'output' : [{'field' : 'PIDU' , 'low' :  0, 'high' : 32}]},
          {'input' : 'PIDL'      , 'output' : [{'field' : 'PIDL' , 'low' :  0, 'high' : 32}]},
          {'input' : 'RATES'     , 'output' : [{'field' : 'FIXED', 'low' :  0, 'high' : 10},
                                               {'field' : 'AC'   , 'low' : 10, 'high' : 16},
                                               {'field' : 'TS'   , 'low' : 16, 'high' : 19},
                                               {'field' : 'TSPH' , 'low' : 19, 'high' : 32}]},
          {'input' : 'BEAMREQ'   , 'output' : [{'field' : 'BEAM' , 'low' :  0, 'high' :  1},
                                               {'field' : 'DST'  , 'low' :  4, 'high' :  8},
                                               {'field' : 'CHG'  , 'low' : 16, 'high' : 32}]},
          {'input' : 'BEAMNRG01' , 'output' : [{'field' : 'NRG0' , 'low' :  0, 'high' : 16},
                                               {'field' : 'NRG1' , 'low' : 16, 'high' : 32}]},
          {'input' : 'BEAMNRG23' , 'output' : [{'field' : 'NRG2' , 'low' :  0, 'high' : 16},
                                               {'field' : 'NRG3' , 'low' : 16, 'high' : 32}]},
          {'input' : 'PHWAVELEN' , 'output' : [{'field' : 'PHW0' , 'low' :  0, 'high' : 16},
                                               {'field' : 'PHW1' , 'low' : 16, 'high' : 32}]},
          {'input' : 'MPSSTAT'   , 'output' : [{'field' : 'SYNC' , 'low' :  0, 'high' :  1},
                                               {'field' : 'MPSV' , 'low' :  1, 'high' :  2},
                                               {'field' : 'BCSF' , 'low' :  2, 'high' :  3},
                                               {'field' : 'MPSL' , 'low' : 16, 'high' : 32}]},
          {'input' : 'MPSCLASS01', 'output' : [{'field' : 'MPSC0', 'low' :  0, 'high' :  4},
                                               {'field' : 'MPSC1', 'low' :  4, 'high' :  8},
                                               {'field' : 'MPSC2', 'low' :  8, 'high' : 12},
                                               {'field' : 'MPSC3', 'low' : 12, 'high' : 16},
                                               {'field' : 'MPSC4', 'low' : 16, 'high' : 20},
                                               {'field' : 'MPSC5', 'low' : 20, 'high' : 24},
                                               {'field' : 'MPSC6', 'low' : 24, 'high' : 28},
                                               {'field' : 'MPSC7', 'low' : 28, 'high' : 32}]},
          {'input' : 'MPSCLASS23', 'output' : [{'field' : 'MPSC8', 'low' :  0, 'high' :  4},
                                               {'field' : 'MPSC9', 'low' :  4, 'high' :  8},
                                               {'field' : 'MPSCA', 'low' :  8, 'high' : 12},
                                               {'field' : 'MPSCB', 'low' : 12, 'high' : 16},
                                               {'field' : 'MPSCC', 'low' : 16, 'high' : 20},
                                               {'field' : 'MPSCD', 'low' : 20, 'high' : 24},
                                               {'field' : 'MPSCE', 'low' : 24, 'high' : 28},
                                               {'field' : 'MPSCF', 'low' : 28, 'high' : 32}]}]

if __name__ == "__main__":

    g = open(sys.argv[1],"rb")
    f = open(sys.argv[2],"w")

    f.write('#INDEX\tPulseId\tvalid\ttag\ttimeStamp')
    for i in range(16):
        f.write('\tPC%d'%i)
    f.write('\n')

    try:
        idx=0
        while True:
            line = g.read(192)

            pulseId = line[4:12]
            strobe = line[14]&1
            latch  = (line[14]&2)>>1
            tag    = line[26:28]
            timestamp = line[28:30]
            pclass = []
            for i in range(16):
                k = 12*(i/4)+38
                pclass.append(line[k:k+2])

            f.write(idx)
            f.write('\t%llu'%PulseId)
            f.write('\t%d'%valid)
            f.write('\t%d'%latch)
            f.write('\t%d'%tag)
            f.write('\t%d'%timestamp)
            for i in range(16):
                f.write('\t%d'%pclass[i])
            f.write('\n')

            idx += 1
    except:
        g.close()
        f.close()
