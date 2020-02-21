@echo off
C:\Chris\MyProjects\tbl2web\release\tbl2web "..\Polymeter ToDo.txt" issues.html issues.txt "Polymeter Issues"
if errorlevel 1 goto err
attrib /s tools\*.* +r
if errorlevel 1 goto err
C:\Chris\tools\navgen template.html .
if errorlevel 1 goto err
attrib /s tools\*.* -r
if errorlevel 1 goto err
C:\Chris\MyProjects\FixSelfUrl\Release\FixSelfUrl *.html
if errorlevel 1 goto err
C:\Chris\tools\navgen gallery/template.html gallery
if errorlevel 1 goto err
C:\Chris\MyProjects\FixSelfUrl\Release\FixSelfUrl gallery/*.html
if errorlevel 1 goto err
rem md Help
rem del /s Help\*.htm
rem C:\Chris\MyProjects\doc2web\release\doc2web /nospaces C:\Chris\MyProjects\Polymeter\Help\help.txt Help Contents.htm C:\Chris\MyProjects\Polymeter\info\PolymeterHelp.htm "Polymeter Help"
rem if errorlevel 1 goto err
rem cd Help
rem md images
rem del /y images\*.*
rem copy C:\Chris\MyProjects\Polymeter\Help\images\*.* images
rem if errorlevel 1 goto err
rem copy ..\helptopic.css content.css
rem C:\Chris\tools\navgen C:\Chris\MyProjects\Polymeter\Help\template.txt .
rem copy ..\helpheader.txt x
rem copy x + Contents.htm
rem echo ^<body^>^<html^> >>x
rem del Contents.htm
rem ren x Contents.htm
rem md printable
rem cd printable
rem move C:\Chris\MyProjects\Polymeter\info\PolymeterHelp.htm .
rem ren PolymeterHelp.htm prnhelp.htm
rem echo y | C:\Chris\tools\fsr prnhelp.htm "../../../images/" "https://polymeter.sourceforge.io/Help/images/"
rem echo y | C:\Chris\tools\fsr prnhelp.htm "../../images/" "https://polymeter.sourceforge.io/Help/images/"
rem echo y | C:\Chris\tools\fsr prnhelp.htm "../images/" "https://polymeter.sourceforge.io/Help/images/"
rem ren prnhelp.htm PolymeterHelp.htm
rem cd ..
rem cd ..
ren issues.html issues.htm
echo y | C:\Chris\tools\fsr issues.htm "<div id=body>" "<div id=widebody>"
ren issues.htm issues.html
goto exit
:err
pause Error!
:exit
