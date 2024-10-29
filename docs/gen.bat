@echo off
echo regenerate all Polymeter web pages; Ctrl+C to abort
pause
C:\Chris\MyProjects\tbl2web\release\tbl2web "..\Polymeter ToDo.txt" issues.html issues.txt "Polymeter Issues"
if errorlevel 1 goto err
attrib /s tools\*.* +r
if errorlevel 1 goto err
attrib /s Help\*.* +r
if errorlevel 1 goto err
C:\Chris\tools\navgen template.html .
if errorlevel 1 goto err
attrib /s tools\*.* -r
if errorlevel 1 goto err
attrib /s Help\*.* -r
if errorlevel 1 goto err
C:\Chris\MyProjects\FixSelfUrl\Release\FixSelfUrl *.html
if errorlevel 1 goto err
C:\Chris\tools\navgen gallery\template.html gallery
if errorlevel 1 goto err
C:\Chris\MyProjects\FixSelfUrl\Release\FixSelfUrl gallery/*.html
if errorlevel 1 goto err
ren issues.html issues.htm
if errorlevel 1 goto err
echo y | C:\Chris\tools\fsr issues.htm "<div id=body>" "<div id=widebody>"
ren issues.htm issues.html
if errorlevel 1 goto err
goto exit
:err
pause Error!
:exit
