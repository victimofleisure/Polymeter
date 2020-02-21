@echo off
xcopy /s /y C:\Chris\MyProjects\Polymeter\web\*.* .
copy downloadGitHub.html download.html
C:\Chris\MyProjects\tbl2web\release\tbl2web "..\Polymeter ToDo.txt" issues.html issues.txt "Polymeter Issues"
if errorlevel 1 goto err
attrib /s tools\*.* +r
if errorlevel 1 goto err
C:\Chris\tools\navgen templateGitHub.html .
if errorlevel 1 goto err
attrib /s tools\*.* -r
if errorlevel 1 goto err
C:\Chris\MyProjects\FixSelfUrl\Release\FixSelfUrl *.html
if errorlevel 1 goto err
C:\Chris\tools\navgen gallery/templateGitHub.html gallery
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
ren links.html links.htm
echo y | C:\Chris\tools\fsr links.htm "\"http://chordease.sourceforge.net/\"" "\"https://victimofleisure.github.io/ChordEase/\""
echo y | C:\Chris\tools\fsr links.htm "\"http://ffrend.sourceforge.net/\"" "\"https://victimofleisure.github.io/FFRend/\""
echo y | C:\Chris\tools\fsr links.htm "\"http://fractice.sourceforge.net/\"" "\"https://victimofleisure.github.io/Fractice/\""
echo y | C:\Chris\tools\fsr links.htm "\"http://mixere.sourceforge.net/\"" "\"https://victimofleisure.github.io/Mixere/\""
echo y | C:\Chris\tools\fsr links.htm "\"http://polymeter.sourceforge.net/\"" "\"https://victimofleisure.github.io/Polymeter/\""
echo y | C:\Chris\tools\fsr links.htm "\"http://potterdraw.sourceforge.net/\"" "\"https://victimofleisure.github.io/PotterDraw/\""
echo y | C:\Chris\tools\fsr links.htm "\"http://sourceforge.net/projects/triplight/\"" "\"https://github.com/victimofleisure/TripLight/\""
echo y | C:\Chris\tools\fsr links.htm "\"http://waveshop.sourceforge.net/\"" "\"https://victimofleisure.github.io/WaveShop/\""
echo y | C:\Chris\tools\fsr links.htm "\"http://whorld.sourceforge.net/\"" "\"https://victimofleisure.github.io/Whorld/\""
echo y | C:\Chris\tools\fsr links.htm "\"http://whorld.org/\"" "\"https://victimofleisure.github.io/Whorld/\""
ren links.htm links.html
goto exit
:err
pause Error!
:exit
