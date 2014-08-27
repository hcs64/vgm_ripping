REM Batch handling of a very specific arrangement

aix2adx %1.aix
if errorlevel 1 goto end
adxloop %100000.adx %101000.adx %1000loop.adx
adxloop %100001.adx %101001.adx %1001loop.adx
adxloop %100002.adx %101002.adx %1002loop.adx
del %100000.adx %101000.adx %100001.adx %101001.adx %100002.adx %101002.adx
:end
