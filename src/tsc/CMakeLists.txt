set(src_files
        auxcodec.c
        cbufint64.c
        cbufstr.c
        common.c
        crc64.c
        fio.c
        idcodec.c
        log.c
        mem.c
        nuccodec.c
        paircodec.c
        qualcodec.c
        samcodec.c
        samparser.c
        str.c
        tsc.c
        tscfmt.c
        zlib_wrap.c
)

add_executable(tsc ${src_files})

target_link_libraries(tsc range)
target_link_libraries(tsc z)
