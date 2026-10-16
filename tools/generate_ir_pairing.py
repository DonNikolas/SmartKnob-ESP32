#!/usr/bin/env python3
"""Generate matching private config headers. Run from anywhere; no extra packages."""
from pathlib import Path
import argparse
import secrets
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--replace',action='store_true',help='Replace existing pairing; reflash BOTH boards afterwards')
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
paths=[root/'firmware/include/ir_link_private/config.h',root/'ir-bridge/SmartKnob_IR_Bridge/config_private.h']
if any(p.exists() for p in paths) and not args.replace:
    parser.error('Pairing already exists. Keep it, or use --replace and reflash BOTH boards.')
text='// Private generated pairing. Do not commit.\n#pragma once\n'
for name,value in [('SSID','SmartKnob-IR-'+secrets.token_hex(2).upper()),('PASSWORD',secrets.token_urlsafe(18)),('KEY',secrets.token_hex(24))]:
    text+=f'constexpr char IR_LINK_{name}[] = "{value}";\n'
for path in paths:
    path.parent.mkdir(parents=True,exist_ok=True);path.write_text(text,encoding='utf-8')
print('Paired headers created. Build and flash both boards. Credentials were not printed.')
