C:\Chris\MyProjects\tbl2web\release\tbl2web "..\Polymeter ToDo.txt" issues.html issues.txt "Polymeter Issues"
navgen template.html . issues.html
ren issues.html issues.htm
echo y | fsr issues.htm "<div id=body>" "<div id=widebody>"
ren issues.htm issues.html

