@echo off
SET ti=%TIME: =0%
SET TAG=%date:~10,4%%date:~7,2%%date:~4,2%%ti:~0,2%%ti:~3,2%%ti:~6,2%


powershell -Command "(gc ..\duckstation-qt\duckstation-qt.rc) -replace '1,0,0,1', '1,0,"%TAG%",0' | Out-File -encoding ASCII ..\duckstation-qt\duckstation-qt.rc"
powershell -Command "(gc ..\duckstation-qt\duckstation-qt.rc) -replace '1.0.0.1', '1.0."%TAG%"' | Out-File -encoding ASCII ..\duckstation-qt\duckstation-qt.rc"

powershell -Command "(gc ..\duckstation-nogui\duckstation-nogui.rc) -replace '1,0,0,1', '1,0,"%TAG%",0' | Out-File -encoding ASCII ..\duckstation-nogui\duckstation-nogui.rc"
powershell -Command "(gc ..\duckstation-nogui\duckstation-nogui.rc) -replace '1.0.0.1', '1.0."%TAG%"' | Out-File -encoding ASCII ..\duckstation-nogui\duckstation-nogui.rc"
