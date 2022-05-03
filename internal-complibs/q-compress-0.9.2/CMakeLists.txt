include(ExternalProject)

get_filename_component(QCO_LOCAL_DIR ./ ABSOLUTE)

ExternalProject_Add(
    qco_blosc
    DOWNLOAD_COMMAND ""
    CONFIGURE_COMMAND ""
    BUILD_COMMAND cargo build COMMAND cargo build --release
    BINARY_DIR "${QCO_LOCAL_DIR}"
    INSTALL_COMMAND ""
    LOG_BUILD ON)
