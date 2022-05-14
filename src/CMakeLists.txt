set(${PROJECT_NAME}_SOURCES
    main.c
    log.c
    event.c
    server.c
    list.c
    client.c
    utils.c
    protocol.c
    packet.c)

list(TRANSFORM ${PROJECT_NAME}_SOURCES PREPEND src/)

set(${PROJECT_NAME}_SOURCES ${${PROJECT_NAME}_SOURCES} PARENT_SCOPE)
