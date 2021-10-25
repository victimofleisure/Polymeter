@echo off
rd /q /s Help
if errorlevel 1 goto err
md Help
if errorlevel 1 goto err
C:\Chris\MyProjects\doc2web\release\doc2web /nospaces C:\Chris\MyProjects\Polymeter\Help\help.txt Help Contents.htm C:\Chris\MyProjects\Polymeter\info\PolymeterHelp.htm "Polymeter Help"
if errorlevel 1 goto err
cd Help
md images
if errorlevel 1 goto err
copy C:\Chris\MyProjects\Polymeter\web\images\polymeter-32.png images
if errorlevel 1 goto err
rem copy C:\Chris\MyProjects\Polymeter\Help\images\*.* images
rem if errorlevel 1 goto err
copy ..\helptopic.css content.css
if errorlevel 1 goto err
C:\Chris\tools\navgen C:\Chris\MyProjects\Polymeter\Help\template.txt .
if errorlevel 1 goto err
copy ..\helpheader.txt x
if errorlevel 1 goto err
copy x + Contents.htm
if errorlevel 1 goto err
echo ^<body^>^<html^> >>x
del Contents.htm
ren x Contents.htm
md printable
if errorlevel 1 goto err
cd printable
move C:\Chris\MyProjects\Polymeter\info\PolymeterHelp.htm .
if errorlevel 1 goto err
ren PolymeterHelp.htm prnhelp.htm
if errorlevel 1 goto err
echo y | C:\Chris\tools\fsr prnhelp.htm "../../../images/" "https://polymeter.sourceforge.io/Help/images/"
echo y | C:\Chris\tools\fsr prnhelp.htm "../../images/" "https://polymeter.sourceforge.io/Help/images/"
echo y | C:\Chris\tools\fsr prnhelp.htm "../images/" "https://polymeter.sourceforge.io/Help/images/"
ren prnhelp.htm PolymeterHelp.htm
if errorlevel 1 goto err
cd ..
del /s /q *.tmp
cd ..
"C:\Chris\MyProjects\doc2web graph\Debug\doc2web.exe" /nospaces "C:\Chris\MyProjects\Polymeter\Help\help.txt" C:\Temp\polymeter_help.gv
if errorlevel 1 goto err
rem "C:\Program Files\Graphviz\bin\dot.exe" -Tsvg -Kdot -o"C:\Chris\MyProjects\Polymeter\web\Help\polymeter_help.svg" C:\Temp\polymeter_help.gv
rem if errorlevel 1 goto err
rem del C:\Temp\polymeter_help.gv
goto exit
:err
pause Error!
:exit
