@echo off
rem Aplica o Zelda (mapa F7, Randomizer, tracker...) no Hookshot: acha o jogo e o builder, junta com a versao atual e recompila.
rem Para escolher o hookshot.exe numa janela: Aplicar-Zelda.bat --escolher
cd /d "%~dp0"
python aplicar.py %*
pause
