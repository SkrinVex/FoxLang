// This named section is located and patched through the ELF/PE section table.
// Volatile + a live reference keep the descriptor even with LTO/section GC.
#if defined(_MSC_VER)
#pragma section(".skmbndl", read)
__declspec(allocate(".skmbndl"))
#elif defined(__GNUC__)
__attribute__((section(".skmbndl"), used))
#endif
extern const volatile unsigned char skrumiBundleDescriptor[40] = {
    'S','K','M','S','T','U','B',0, 1,0,0,0
};

bool skrumiHasBundleDescriptor() {
    return skrumiBundleDescriptor[0] == 'S';
}

// `skrumi build` sets the mode word (bytes 12..15) in the file; the loaded image carries
// it, so a plain skrumi knows without reading its whole executable from disk.
bool skrumiHasBundlePayload() {
    return skrumiBundleDescriptor[12] || skrumiBundleDescriptor[13] || skrumiBundleDescriptor[14] ||
           skrumiBundleDescriptor[15];
}
