@echo off
setlocal

python "%~dp0run.py" ^
  --config "%~dp0targets.example.json" ^
  --target local-docker ^
  --build-image ^
  --interactive ^
  --container-workdir /gaia-ecs/vm ^
  -- bash

exit /b %errorlevel%