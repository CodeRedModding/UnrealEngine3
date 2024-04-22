cd %1
echo Copying %2LiveConfig.xml to %3.exe.cfg
copy %2LiveConfig.xml %3.exe.cfg
"%VS80COMNTOOLS%\bin\makecat.exe" -v %2%3.exe.cdf
"%VS80COMNTOOLS%\bin\makecert.exe" -sv XLiveTestSign.pvk -n "CN=XLiveTestSignCert" XLiveTestSign.cer
"%VS80COMNTOOLS%\bin\pvk2pfx.exe" -pvk XLiveTestSign.pvk -pi %3 -spc XLiveTestSign.cer -pfx XLiveTestSign.pfx -f
"%VS80COMNTOOLS%\bin\signtool.exe" sign /f XLiveTestSign.pfx /p %3 /v %3.exe.cat