#ifndef MZ_CONFIG_H
#define MZ_CONFIG_H

/* Hand-written in place of CMake's generated mz_config.h, for a minimal Windows build of minizip-ng
   (store+deflate+AES only -- no bzip2/lzma/zstd/ppmd/pkcrypt/posix). See mz_config.h.cmakein in the
   minizip-ng source for what these mean; the HAVE_ZLIB/HAVE_WZAES/HAVE_CRYPT_BACKEND feature-selection
   macros (as opposed to these header/function-availability ones) are passed as /D compiler flags instead,
   matching how minizip-ng's own CMakeLists.txt does it. */

#define HAVE_STDINT_H    1   /* MSVC has had <stdint.h> since VS2010 */
#define HAVE_INTTYPES_H  1   /* and <inttypes.h> since VS2013 */
#define HAVE_DIRENT_H    0   /* POSIX-only; mz_os_win32.c/mz_strm_os_win32.c don't need it */
#define HAVE_SYS_DIRENT_H 0
#define HAVE_PDIR        0
#define HAVE_FSEEKO      0
#define HAVE_SYMLINK     0
#define HAVE_READLINK    0

#endif
