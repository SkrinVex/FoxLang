// This named section is located and patched through the ELF/PE section table.
// Volatile + a live reference keep the descriptor even with LTO/section GC.
#if defined(_MSC_VER)
#pragma section(".foxbndl", read)
__declspec(allocate(".foxbndl"))
#elif defined(__GNUC__)
__attribute__((section(".foxbndl"), used))
#endif
extern const volatile unsigned char foxlangBundleDescriptor[40] = {
    'F','O','X','S','T','U','B',0, 1,0,0,0
};

bool foxlangHasBundleDescriptor() {
    return foxlangBundleDescriptor[0] == 'F';
}
