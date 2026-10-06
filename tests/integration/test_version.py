import json, subprocess, sys
p=subprocess.run([sys.argv[1],'--version'],capture_output=True,text=True)
assert p.returncode==0,p.stderr
assert json.loads(p.stdout)=={'contract':'bruce-version-v1','version':sys.argv[2]}
assert subprocess.run([sys.argv[1],'--version','extra'],capture_output=True).returncode==2
