rm *.obj
rm *.exe
rc.exe IRC.rc
for /f %%i in ('git rev-parse --short HEAD') do echo #define GIT_COMMIT_HASH "%%i" > "version.h"
cl /EHsc /std:c++17 /MD /D_AFXDLL /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0601 /I zipkit IRC.cpp IRC.res zipkit\*.c /DZLIB_COMPAT /DHAVE_ZLIB /DHAVE_WZAES /DHAVE_CRYPT_BACKEND /link /SUBSYSTEM:WINDOWS ws2_32.lib bcrypt.lib

