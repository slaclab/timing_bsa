#
#  Copy and paste tcpdump output to get an analysis of BSA fetches or RSSI/SRP transactions in general
#
#  Example tcpdump command is:
#  tcpdump -x -s64 -i<INTF> dst <ATCA_IP> and port <RSSI_PORT>
#  tcpdump -x -s64 -ieth5 dst 10.0.1.103 and port 8194   (memory stream reads)
#  tcpdump -x -s64 -ieth5 dst 10.0.1.103 and port 8193   (register reads)
#
from sys import stdin

start_time = None
last_time  = None
start_addr = None
last_addr  = None
start_tid  = None
last_tid   = None
pkt_time   = None

#  Rearrange 4-bytes into a word
def tou32(words,start):
    w = words[start:start+2]
    return w[1][2:4]+w[1][:2]+w[0][2:4]+w[0][:2]

#  Map address into a BSA array number
def tobsa(addr):
    return int( addr / ((1<<15)*384) ) if addr < 0x21000000 else int( (addr - 0x21000000) / ((1<<20)*384) + 44)

#  Parse the time field into seconds
def tosec(the_time):
    hrs = the_time[:2]
    mns = the_time[3:5]
    scs = the_time[6:15]
    return int(hrs)*3600 + int(mns)*60 + float(scs)

#  Dump the complete transaction
def dump():
    duration  = tosec(last_time)-tosec(start_time)
    start_bsa = tobsa(start_addr)
    last_bsa  = tobsa(last_addr)
    bytes     = last_addr - start_addr
    entries   = int(bytes/384)
    print(f' [{start_time} ({duration:6.6f})] [0x{start_tid:08x}:{last_tid-start_tid+1:04d}]  [0x{start_addr:09x}:{bytes:10d}:{entries:5d}]    [{start_bsa}:{last_bsa}]')

def header():
    print(f'    start time     duration    start TID : #      start addr :  bytes   :entries     BSA')

#  Parse the RSSI command from the words and address within the packet    
def parse_rssi(addr,words):
    global start_time
    global start_addr
    global start_tid
    global last_time
    global last_addr
    global last_tid
    global pkt_time

#    if addr == '0x0010':
#        if words[6]!='4008':
#            print(f'Wrong header word {words[6]}/=4008')
#            return
#        w = words[7]
#        print(f'SEQ   [{w[:2]}] ACK [{w[2:4]}]')
#    elif addr == '0x0020':
#        if len(words)==8:
#            print(f'CKSUM [{words[1]}] SRPV[{words[6][:2]}]')
#    elif addr == '0x0030':
    if addr == '0x0030':
        tid   = int(tou32(words,0),16)
        maddr = int('0x'+tou32(words,4)+tou32(words,2),16)
        reqsz = int('0x'+tou32(words,6),16)+1

        if not (last_tid and tid == last_tid+1 and maddr == last_addr):
            # dump the previous transaction
            if start_addr:
                dump()
            # start the new transaction
            start_tid = tid
            start_time = pkt_time
            start_addr = maddr
            last_addr  = maddr

        last_tid   = tid
        last_time  = pkt_time
        last_addr += reqsz

header()

#  Loop over all lines in the input
for line in stdin:
    if line == '':
        break
    sline = line.strip()
    fields = sline.split(':')
    if len(fields)>2:
        pkt_time = sline.split(' ')[0]
        continue
    elif len(fields)==2:
        addr, body = sline.split(':')[:2]
        words = body.strip().split(' ')
        parse_rssi(addr,words)
    else:
        dump()
        exit()


