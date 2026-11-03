"""Offline integrity/layout check; never connects to or writes a device."""
from pathlib import Path
import hashlib
import json
import struct
import subprocess
import zipfile

ROOT=Path(__file__).resolve().parents[1]
bundle=ROOT/'build-idf62/trader_lab-system-update.irisfw'
app=ROOT/'build-idf62/trader_lab.bin'
digest=lambda data:hashlib.sha256(data).hexdigest()
expected={
    'otadata':(1,0,0x9000,0x2000), 'phy_init':(1,1,0xb000,0x1000),
    'sysmeta':(1,2,0xc000,0x14000), 'vibe_mode':(0,0x20,0x20000,0x1c0000),
    'coredump':(1,3,0x1e0000,0x20000), 'nvs':(1,2,0x200000,0x10000),
    'main_app':(0,0x10,0x210000,0xdf0000),
}
with zipfile.ZipFile(bundle) as archive:
    assert set(archive.namelist())=={'manifest.json','partition-table.bin','ota_0.bin'}
    manifest=json.loads(archive.read('manifest.json'))
    assert manifest['schema']=='esp-iris-system-update/0.2'
    assert manifest['release']=='0.1.0-dev' and manifest['minimum_recovery_version']=='0.2.0'
    assert manifest['target']=={'chip_id':32,'flash_size':0x1000000}
    assert len(manifest['components'])==2
    for component in manifest['components']:
        data=archive.read(component['file'])
        assert len(data)==component['size'] and digest(data)==component['sha256']
        if component['kind']=='application':
            assert component['target_offset']==0x210000 and len(data)<=0xdf0000
            assert data==app.read_bytes()
        else:
            assert component['kind']=='partition_table' and component['target_offset']==0x8000
            assert len(data)==4096 and digest(data)==manifest['target_layout_sha256']
            actual={}
            for at in range(0,len(data),32):
                magic,kind,subtype,offset,size,name,flags=struct.unpack_from('<HBBII16sI',data,at)
                if magic!=0x50aa:break
                actual[name.split(b'\0')[0].decode()]=(kind,subtype,offset,size)
            assert actual==expected
pins={}
for name,expected_commit in {
    'esp-mosaico-utils':'be151ae047014e79a9c7ae0092dd203844499003',
    'esp-mosaico-bsp':'5f88b91e1be055d1d3674364123b2779a1758377',
}.items():
    path=ROOT/'build-deps'/name
    commit=subprocess.check_output(['git','-C',str(path),'rev-parse','HEAD'],text=True).strip()
    assert commit==expected_commit
    assert not subprocess.check_output(['git','-C',str(path),'status','--porcelain'],text=True).strip()
    pins[name]=commit
inputs=sorted(list((ROOT/'main').glob('*.[ch]'))+[ROOT/'sdkconfig.defaults',ROOT/'dependencies.lock',ROOT/'partitions.csv',ROOT/'CMakeLists.txt',ROOT/'main/CMakeLists.txt',ROOT/'main/idf_component.yml'])
record={
    'version':'0.1.0-dev','hardwareValidated':False,'hardwareInstalled':False,
    'bundle':{'file':str(bundle.relative_to(ROOT)).replace('\\','/'),'bytes':bundle.stat().st_size,'sha256':digest(bundle.read_bytes())},
    'application':{'bytes':app.stat().st_size,'sha256':digest(app.read_bytes())},
    'manifest':manifest,'dependencyPins':pins,
    'sourceSha256':{str(p.relative_to(ROOT)).replace('\\','/'):digest(p.read_bytes()) for p in inputs},
}
(ROOT/'docs/build-verification.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'verified':True,'bundleBytes':bundle.stat().st_size,'appBytes':app.stat().st_size,'sha256':record['bundle']['sha256'],'hardwareValidated':False},indent=2))
