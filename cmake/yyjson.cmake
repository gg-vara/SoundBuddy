# yyjson : JSON reading and writing (settings, SoundVitrine requests and responses)
#
# built from sources, as a static library linked to appExec

SET(YYJSON_VERSION 0.13.0)

include(FetchContent)
FetchContent_Declare(yyjson
    URL https://github.com/ibireme/yyjson/archive/refs/tags/${YYJSON_VERSION}.tar.gz
    URL_HASH SHA256=34e0f62a2bc11ab20d601e8ca1cc2b2079503aa45119a19133d89d19b94a0fae
    DOWNLOAD_EXTRACT_TIMESTAMP ON
)
FetchContent_MakeAvailable(yyjson)

target_link_libraries(appExec PRIVATE
    yyjson
)
