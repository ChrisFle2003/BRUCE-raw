import hashlib,json,subprocess,sys,tempfile,unittest
from pathlib import Path
BIN=str(Path(sys.argv.pop(1)).resolve())
class ReplaceTests(unittest.TestCase):
 def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
 def tearDown(self):self.tmp.cleanup()
 def call(self,*args,ok=True):
  p=subprocess.run([BIN,*map(str,args)],capture_output=True,text=True,timeout=30)
  self.assertEqual(p.returncode,0 if ok else 2,p.stderr)
  return json.loads(p.stdout if ok else p.stderr)
 def source(self,name,hold=3,start=0):
  t=self.root/(name+'.csv');h=self.root/(name+'h.csv');p=self.root/name
  t.write_text('x,y\n'+''.join(f'{x},{x*x}\n' for x in range(start,start+3)));h.write_text(f'x,y\n{hold},{hold*hold}\n')
  self.call('research','--strategy','portfolio-rational','--max-degree',2,'--min',-2,'--max',2,'--max-power',2,'--max-denominator',1,'--train',t,'--holdout',h,'--output',p)
  return p
 def initial(self):
  p=self.root/'store';r=self.call('experience','add','--from-run',self.source('first'),'--output',p);return p,r
 def replace(self,store,target,source,out,ok=True,reason='new holdout'):
  return self.call('experience','replace','--store',store,'--id',target,'--from-run',source,'--reason',reason,'--output',out,ok=ok)
 def test_zero_budget_error_preserves_cli_lifetime(self):
  missing=self.root/'missing-source';output=self.root/'not-published'
  error=self.call('experience','replace','--id','0'*64,'--reason','probe','--from-run',missing,
                  '--max-transfer-checkpoints',0,'--output',output,ok=False)
  self.assertEqual(error,{'error':'TransferCheckpointBudgetExceeded: checkpoints=0 limit=0'})
  self.assertFalse(output.exists())
 def test_chain_votes_and_selfcontained(self):
  store,r=self.initial();original=store.read_bytes();target=r['records'][0]['id']
  for n in range(2):
   child=self.root/f'child{n}';r=self.replace(store,target,self.source(f'next{n}',4+n),child)
   self.assertEqual(r['engine'],'bruce-experience-v13');self.assertEqual(r['active_records'],1)
   self.assertEqual(r['records'][-1]['replaces'],target)
   self.assertEqual(r['corrections'][-1]['replacement'],r['records'][-1]['id'])
   self.assertFalse(r['records'][-2]['active']);target=r['records'][-1]['id'];store=child
  self.assertEqual((self.root/'store').read_bytes(),original)
  train=self.root/'later.csv';hold=self.root/'laterh.csv';train.write_text('x,y\n4,16\n5,25\n6,36\n');hold.write_text('x,y\n7,49\n')
  result=self.root/'result';learned=self.call('research','--strategy','portfolio-rational','--max-degree',2,'--min',-2,'--max',2,'--max-power',2,'--max-denominator',1,'--train',train,'--holdout',hold,'--planner','experience-order','--experience',store,'--output',result)
  self.assertEqual(learned['experience_records_used'],1)
  self.assertEqual(learned['experience_votes'],r['records'][-1]['votes'])
  revoked=self.root/'revoked';rr=self.call('experience','revoke','--store',store,'--id',target,'--reason','withdraw','--output',revoked);self.assertEqual(rr['active_records'],0)
  for p in self.root.iterdir():
   if p!=result:p.unlink()
  self.assertEqual(self.call('replay',result),learned)
 def test_invalid_replacements_leave_source_unchanged(self):
  store,r=self.initial();before=store.read_bytes();target=r['records'][0]['id'];new=self.source('new',4)
  for id,source,reason in [(target,self.root/'first','same'),('missing',new,'unknown'),(target,new,''),(target,new,'a\nb')]:
   self.replace(store,id,source,self.root/'bad',ok=False,reason=reason);self.assertFalse((self.root/'bad').exists())
  child=self.root/'child';out=self.replace(store,target,new,child)
  self.replace(child,target,self.source('third',5),self.root/'bad',ok=False)
  other=self.source('other',7,4);mixed=self.root/'mixed';self.call('experience','add','--store',store,'--from-run',other,'--output',mixed)
  self.replace(mixed,target,other,self.root/'bad',ok=False)
  self.assertEqual(store.read_bytes(),before)
  header,_,body=child.read_text().split('\n',2);changed=body.replace('target '+target,'target '+'0'*64,1)
  child.write_text(header+'\n'+hashlib.sha256(changed.encode()).hexdigest()+'\n'+changed)
  self.call('experience','inspect',child,ok=False)
 def test_grid_replacement_and_ineligible_source(self):
  t=self.root/'gridtrain';self.call('grid-batch','--paths','0,2;1,2;2,2','--output',t)
  sources=[]
  for n in (3,4):
   h=self.root/f'hold{n}';p=self.root/f'grid{n}'
   self.call('grid-batch','--paths',f'{n},2','--output',h)
   self.call('research','--strategy','portfolio-rational','--max-degree',2,'--min',-2,'--max',2,'--max-power',2,'--max-denominator',1,'--grid-train',t,'--grid-holdout',h,'--output',p);sources.append(p)
  store=self.root/'gridstore';r=self.call('experience','add','--from-run',sources[0],'--output',store);target=r['records'][0]['id']
  child=self.root/'child';report=self.replace(store,target,sources[1],child)
  self.assertTrue(all(r['grid_origin'] for r in report['records']))
  self.assertEqual(report['records'][-1]['training_fingerprint'],report['records'][0]['training_fingerprint'])
  self.replace(store,target,t,self.root/'bad',ok=False);self.assertFalse((self.root/'bad').exists())
  for p in self.root.iterdir():
   if p!=child:p.unlink()
  self.assertEqual(self.call('experience','inspect',child),report)
 def test_historical_record_limit(self):
  store,r=self.initial();target=r['records'][-1]['id']
  for n in range(7):
   child=self.root/f's{n}';r=self.replace(store,target,self.source(f'r{n}',4+n),child);store=child;target=r['records'][-1]['id']
  self.assertEqual(len(r['records']),8)
  self.replace(store,target,self.source('last',20),self.root/'bad',ok=False);self.assertFalse((self.root/'bad').exists())
 def test_controlled_replace_preserves_bytes_and_prevents_publication(self):
  def unpublished(path):
   self.assertFalse(path.exists())
   self.assertFalse(list(self.root.glob(path.name+'.tmp.*')))
  store,initial=self.initial();target=initial['records'][0]['id'];source=self.source('controlled',4)
  plain_output=self.root/'plain-replace';plain=self.replace(store,target,source,plain_output)
  bounded_output=self.root/'bounded-replace'
  bounded=self.call('experience','replace','--store',store,'--id',target,'--reason','new holdout','--from-run',source,
                    '--max-transfer-checkpoints',1000000,'--output',bounded_output)
  self.assertEqual(bounded['contract'],'controlled-transfer-v1');self.assertEqual(bounded['result'],plain)
  self.assertEqual(bounded_output.read_bytes(),plain_output.read_bytes())
  none_output=self.root/'none-replace'
  none=self.call('experience','replace','--store',store,'--id',target,'--reason','new holdout','--from-run',source,
                 '--transfer-cancellation','none','--output',none_output)
  self.assertEqual(none['contract'],'controlled-transfer-v1');self.assertEqual(none['result'],plain)
  self.assertEqual(none_output.read_bytes(),plain_output.read_bytes())
  used=bounded['usage']['checkpoints'];exact_output=self.root/'exact-replace'
  exact=self.call('experience','replace','--store',store,'--id',target,'--reason','new holdout','--from-run',source,
                  '--max-transfer-checkpoints',used,'--output',exact_output)
  self.assertEqual(exact['result'],plain);self.assertEqual(exact_output.read_bytes(),plain_output.read_bytes())
  stopped=self.root/'stopped-replace'
  error=self.call('experience','replace','--store',store,'--id',target,'--reason','new holdout','--from-run',source,
                  '--max-transfer-checkpoints',used-1,'--output',stopped,ok=False)
  self.assertIn('TransferCheckpointBudgetExceeded',error['error']);unpublished(stopped)
  missing_source=self.root/'missing-source';no_source=self.root/'no-source-replace'
  error=self.call('experience','replace','--id',target,'--reason','new holdout','--from-run',missing_source,
                  '--max-transfer-checkpoints',0,'--output',no_source,ok=False)
  self.assertIn('TransferCheckpointBudgetExceeded',error['error']);unpublished(no_source)
  missing_store=self.root/'missing-store';no_store=self.root/'no-store-replace'
  error=self.call('experience','replace','--store',missing_store,'--id',target,'--reason','new holdout','--from-run',source,
                  '--max-transfer-checkpoints',0,'--output',no_store,ok=False)
  self.assertIn('TransferCheckpointBudgetExceeded',error['error']);unpublished(no_store)
  existing=self.root/'existing-replace';existing.write_bytes(b'keep')
  error=self.call('experience','replace','--store',store,'--id',target,'--reason','new holdout','--from-run',source,
                  '--max-transfer-checkpoints',1000000,'--output',existing,ok=False)
  self.assertIn('ExportExistsOrUnavailable',error['error']);self.assertEqual(existing.read_bytes(),b'keep')
  self.assertFalse(list(self.root.glob(existing.name+'.tmp.*')))
if __name__=='__main__':unittest.main()
