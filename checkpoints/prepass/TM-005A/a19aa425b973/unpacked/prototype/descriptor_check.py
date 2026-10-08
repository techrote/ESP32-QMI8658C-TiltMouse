#!/usr/bin/env python3
"""Small short-item checker for THIS retained descriptor; not a general HID parser."""
import re,json,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
source=(root/'evidence/assessed/components/usb_hid_mouse/usb_hid_mouse.c').read_text()
block=source.split('s_hid_report_descriptor[] = {',1)[1].split('};',1)[0]
block=re.sub(r'/\*.*?\*/','',block,flags=re.S)
data=bytes(int(s,16) for s in re.findall(r'0x([0-9A-Fa-f]+)',block))
def parse(data):
    pos=offset=size=count=page=0; fields=[]
    while pos<len(data):
        item=data[pos];pos+=1
        assert item!=0xfe,'long item unsupported'
        n=[0,1,2,4][item&3];typ=(item>>2)&3;tag=item>>4
        assert pos+n<=len(data)
        value=int.from_bytes(data[pos:pos+n],'little');pos+=n
        if typ==1:
            if tag==0:page=value
            elif tag==7:size=value
            elif tag==9:count=value
            elif tag in (10,11):raise AssertionError('PUSH/POP unsupported')
        elif typ==0 and tag==8:
            fields.append({'bit_offset':offset,'bits':size*count,'count':count,
                           'size':size,'page':page,'constant':bool(value&1),
                           'relative':bool(value&4)})
            offset+=size*count
    return {'input_bits':offset,'fields':fields,
            'button_data_bits':sum(f['bits'] for f in fields if f['page']==9 and not f['constant'])}
actual=parse(data)
assert actual['input_bits']==22 and actual['button_data_bits']==0
needle=bytes.fromhex('95 02 75 01')
assert data.count(needle)==1
candidate=data.replace(needle,needle+bytes.fromhex('81 02'),1)
fixed=parse(candidate)
assert fixed['input_bits']==24 and fixed['button_data_bits']==2
result={'assessed_descriptor_hex':data.hex(' '),'assessed':actual,
        'candidate_two_byte_insertion':fixed,
        'finding':'Missing Input(Data,Variable,Absolute) after two 1-bit buttons. Only 6 padding bits precede X/Y; descriptor declares 22 bits, while payload is 24 bits.',
        'scope':'Static short-item parse of Git-blob-verified source. Candidate is NOT applied; no enumeration claim.'}
out=Path(sys.argv[1]) if len(sys.argv)>1 else root/'results'
out.mkdir(parents=True,exist_ok=True)
(out/'descriptor_results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
