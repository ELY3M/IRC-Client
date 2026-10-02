rc.exe IRC.rc
cl /EHsc /std:c++17 /MD /D_AFXDLL /DUNICODE /D_UNICODE IRC.cpp IRC.res /link /SUBSYSTEM:WINDOWS ws2_32.lib
