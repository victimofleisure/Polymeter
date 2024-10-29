@echo off
xcopy /s /y C:\Chris\MyProjects\Polymeter\web\*.* .
copy C:\Chris\MyProjects\Polymeter\notes.txt
copy downloadGitHub.html download.html
del Help\polymeter_help.svg
rem C:\Chris\MyProjects\tbl2web\release\tbl2web "..\Polymeter ToDo.txt" issues.html issues.txt "Polymeter Issues"
rem if errorlevel 1 goto err
attrib /s tools\*.* +r
attrib /s Help\*.* +r
if errorlevel 1 goto err
C:\Chris\tools\navgen templateGitHub.html .
if errorlevel 1 goto err
attrib /s tools\*.* -r
attrib /s Help\*.* -r
if errorlevel 1 goto err
C:\Chris\MyProjects\FixSelfUrl\Release\FixSelfUrl *.html
if errorlevel 1 goto err
C:\Chris\tools\navgen gallery/templateGitHub.html gallery
if errorlevel 1 goto err
C:\Chris\MyProjects\FixSelfUrl\Release\FixSelfUrl gallery/*.html
if errorlevel 1 goto err
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
ren translate.html translate.htm
echo y | C:\Chris\tools\fsr translate.htm "\"https://polymeter-sourceforge-io.translate.goog/\"" "\"https://victimofleisure-github-io.translate.goog/Polymeter/\""
ren translate.htm translate.html
copy C:\Chris\MyProjects\Polymeter\Help\help.txt Help
copy C:\Chris\MyProjects\Polymeter\Help\Polymeter.hhp Help
if errorlevel 1 goto err
goto exit
:err
pause Error!
:exit
