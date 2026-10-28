#!/usr/bin/env python3
"""Generate the ignored firmware credentials without logging them."""
import json
import time
from pathlib import Path

root = Path(__file__).resolve().parents[1]
data = json.loads((root / 'wifi.local.json').read_text())
ssid, password = data.get('ssid', ''), data.get('password', '')
if not isinstance(ssid, str) or not isinstance(password, str):
    raise SystemExit('ssid y password deben ser texto.')
if not 1 <= len(ssid.encode()) <= 32 or '\x00' in ssid:
    raise SystemExit('Completa ssid en wifi.local.json (1–32 bytes).')
if '\x00' in password or (password and not 8 <= len(password.encode()) <= 63):
    raise SystemExit('La contraseña WPA debe tener 8–63 bytes, o estar vacía para una red abierta.')

def literal(value):
    # Octal byte escapes avoid C++ unicode, quoting and hex escape ambiguity.
    return '"' + ''.join('\\%03o' % byte for byte in value.encode()) + '"'

dest = root / 'UsdMxnMonitor' / 'secrets.h'
dest.write_text('#pragma once\nstatic const char WIFI_SSID[] = ' + literal(ssid) + ';\n'
                'static const char WIFI_PASSWORD[] = ' + literal(password) + ';\n')
dest.chmod(0o600)
(root / 'wifi.local.json').chmod(0o600)
(root / 'UsdMxnMonitor' / 'BuildTime.h').write_text('#pragma once\nconstexpr uint32_t BUILD_UNIX_TIME = '+str(int(time.time()))+'UL;\n')
print('Configuración Wi-Fi preparada; credenciales ocultas.')
