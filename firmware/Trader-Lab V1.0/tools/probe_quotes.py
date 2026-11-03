"""Read public quote endpoints; no credentials, no order API, bounded responses.

Optional --save writes ignored fixtures for the exact C parser regression probe.
"""
import argparse
import json
import urllib.request
from pathlib import Path
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--save', action='store_true')
args = parser.parse_args()
sources = {
    'eastmoney': ('https://push2.eastmoney.com/api/qt/ulist.np/get?fltt=2&invt=2&secids=1.510300,0.159915&fields=f12,f14,f2,f3,f18,f124', 'https://quote.eastmoney.com/'),
    'tencent': ('https://qt.gtimg.cn/q=sh510300,sz159915', 'https://gu.qq.com/'),
    'tencent-global': ('https://qt.gtimg.cn/q=sh000001,sz399001,sz399006,hkHSI,usINX,usIXIC,usDJI', 'https://gu.qq.com/'),
    'tencent-global-gz': ('https://qt.gtimg.cn/q=gzN225,gzGDAXI,gzFTSE,gzFCHI', 'https://gu.qq.com/'),
    'tencent-global-regional': ('https://qt.gtimg.cn/q=gzKS11', 'https://gu.qq.com/'),
}
for name, (url, referer) in sources.items():
    try:
        request = urllib.request.Request(url, headers={'User-Agent':'Mozilla/5.0','Referer':referer,'Accept-Encoding':'identity'})
        with urllib.request.urlopen(request, timeout=8) as response:
            body = response.read(65537)
            if len(body)>65536:
                raise ValueError('oversized response')
            print(name, response.status, len(body), datetime.now(timezone.utc).isoformat())
        if args.save:
            (ROOT/'tests/fixtures.local').mkdir(exist_ok=True)
            (ROOT/f'tests/fixtures.local/{name}.txt').write_bytes(body)
        if name == 'eastmoney':
            print(json.dumps(json.loads(body), ensure_ascii=False)[:1200])
        else:
            for line in body.decode('gbk').splitlines():
                fields = line.split('~')
                if len(fields)>32:
                    print(json.dumps({'code':fields[2],'name':fields[1],'price':fields[3],'previousClose':fields[4],'quoteTime':fields[30],'changePercent':fields[32]},ensure_ascii=False))
                elif len(fields)>=7:
                    print(json.dumps({'quoteTime':fields[2],'price':fields[3],'changePoints':fields[4],'changePercent':fields[5]},ensure_ascii=False))
    except Exception as error:
        print(name, type(error).__name__, str(error)[:180])
