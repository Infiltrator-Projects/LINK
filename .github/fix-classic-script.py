from pathlib import Path
p = Path('.github/apply-classic-bluetooth.py')
s = p.read_text()
s = s.replace(r"device[0] == '\0'", r"device[0] == '\\0'")
p.write_text(s)
