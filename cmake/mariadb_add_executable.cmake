# Add MariaDB symlinks
MACRO(CREATE_MARIADB_SYMLINK filepath sympath)
    install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${filepath} ${sympath})" COMPONENT symlinks)
    install(CODE "message(\"-- Created symlink: ${CMAKE_BINARY_DIR}/${sympath} -> ${INSTALL_BINDIR}/${filepath}\")" COMPONENT symlinks)
    install(FILES ${CMAKE_BINARY_DIR}/${sympath} DESTINATION ${INSTALL_BINDIR} COMPONENT symlinks)
ENDMACRO(CREATE_MARIADB_SYMLINK)
