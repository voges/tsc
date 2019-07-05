set(source_files
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
        range.c
        samcodec.c
        samparser.c
        str.c
        tsc.c
        tscfmt.c
        zlib-wrap.c
)

add_executable(tsc ${source_files})

target_link_libraries(tsc z)
