section .userblob progbits alloc align=16

global hello_elf_start
global hello_elf_end
hello_elf_start:
incbin "build/user/hello.elf"
hello_elf_end:

align 16
global dynhello_start
global dynhello_end
dynhello_start:
incbin "build/user/dynhello"
dynhello_end:

align 16
global ldlinux_start
global ldlinux_end
ldlinux_start:
incbin "build/user/ldlinux.so"
ldlinux_end:

align 16
global libc_start
global libc_end
libc_start:
incbin "build/user/libc.so"
libc_end:

align 16
global libdl_start
global libdl_end
libdl_start:
incbin "build/user/libdl.so"
libdl_end:

align 16
global libpthread_start
global libpthread_end
libpthread_start:
incbin "build/user/libpthread.so"
libpthread_end:

align 16
global libz_start
global libz_end
libz_start:
incbin "build/user/libz.so"
libz_end:

align 16
global libm_start
global libm_end
libm_start:
incbin "build/user/libm.so"
libm_end:

align 16
global libgcc_start
global libgcc_end
libgcc_start:
incbin "build/user/libgcc_s.so"
libgcc_end:

align 16
global libstdcxx_start
global libstdcxx_end
libstdcxx_start:
incbin "build/user/libstdcxx.so"
libstdcxx_end:

align 16
global librt_start
global librt_end
librt_start:
incbin "build/user/librt.so"
librt_end:

align 16
global appruntime_start
global appruntime_end
appruntime_start:
incbin METTA_APPRUNTIME
appruntime_end:

%include "build/user/hostlibs.S"
