prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=${prefix}
libdir=${exec_prefix}/lib@LIB_SUFFIX@
includedir=${prefix}/include/direct_bt

Name: direct_bt
Description: Tiny BLE HCI library
Version: @tinyb_VERSION_STRING@

Libs: -L${libdir} -ltinyb
Cflags: -I${includedir}
