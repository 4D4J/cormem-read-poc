cmake_minimum_required(VERSION 3.20)
project(cordrv_exploit CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources
add_executable(cordrv_exploit
    main.cpp
    cordrv.cpp
    cordrv.hpp
)

# Windows libs nécessaires
target_link_libraries(cordrv_exploit PRIVATE
    ntdll
)

# Warnings utiles
target_compile_options(cordrv_exploit PRIVATE
    /W3
    /WX-          # warnings non bloquants
    /permissive-  # mode conforme au standard
)

# x64 uniquement (le code suppose un espace d'adressage 64 bits)
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Ce projet doit être compilé en 64 bits (x64).")
endif()
