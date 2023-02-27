from epics import caget, caput, cainfo, camonitor
import datetime
from subprocess import *
import argparse

parser = argparse.ArgumentParser(description='fault buffer acknowlege')
parser.add_argument('--tpg', help='TPG PV base', default='TPG:SYS0:2')
parser.add_argument('--nodes', help='Client PV base', nargs='+', default=['TPG:SYS0:2'])
args = parser.parse_args()

#  Returns buffer number and timestamp
def gen_tpgts():
    camonitor_args = ['camonitor','-f0']
    for i in range(4):
        camonitor_args.append(f'{args.tpg}:FLTBUF{i}:TPGTS_STR')
    print(f'camonitor_args {camonitor_args}')
    with Popen(camonitor_args, stdout=PIPE) as proc:
        while(True):
            line = proc.stdout.readline().decode('utf-8').rstrip()
            nbuf = int(line.split(':')[3][-1])
            sdate = ' '.join(line.split(' ')[-2:])
            tts = datetime.datetime.fromisoformat(sdate)
            yield nbuf,tts

def fltb_rdy(pv,buf,ts):
    camonitor_args = ['camonitor','-f0','-# 1',f'{pv}:PIDHSTFLTB{buf}']
    proc = Popen(camonitor_args, stdout=PIPE)
    while(True):
        line  = ' '.join(proc.stdout.readline().decode('utf-8').split())
        sdate = ' '.join(line.split(' ')[1:3])
        val   = line.split(' ')[-1]
        tts = datetime.datetime.fromisoformat(sdate)
        v = (tts - ts) / datetime.timedelta(microseconds = 1)
        #print(f'ts [{ts}]  tts [{tts}]  {v}')
        if v > -5e5:
            #print(f'  {node} {tts} {val} {v}')
            return


for nbuf,ts in gen_tpgts():
    start = datetime.datetime.now()
    print(f'{start}:  FLTBUF{nbuf} at {ts} latched')
    for node in args.nodes:
        fltb_rdy(node,nbuf,ts)
    stop  = datetime.datetime.now()
    print(f'  {stop}:  FLTBUF{nbuf} at {ts} complete  {(stop-start)/datetime.timedelta(microseconds=1)*1.e-6}')
    
