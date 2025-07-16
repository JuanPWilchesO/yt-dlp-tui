cmake_minimum_required(VERSION 3.10)

project(YtDlpWrapper)

# Busca el paquete ncurses
find_package(Curses REQUIRED)

# Define MUSIC_DIR si se pasa desde CMake (ej. cmake -DMUSIC_DIR="/ruta/a/musica")
# Si no se pasa, se puede definir un valor por defecto aquí, aunque el script de instalación lo manejará.
if(NOT DEFINED MUSIC_DIR)
    set(MUSIC_DIR "${CMAKE_SOURCE_DIR}/Music") # Valor por defecto si no se especifica
endif()

# Pasa MUSIC_DIR como una definición de preprocesador al código fuente
add_definitions(-DMUSIC_DIR="${MUSIC_DIR}")

add_executable(yt-dlp-tui main.cpp)

# Enlaza la biblioteca ncurses, pthread y filesystem
target_link_libraries(yt-dlp-tui ${CURSES_LIBRARIES} pthread stdc++fs)

# Establece el estándar de C++ (opcional pero buena práctica)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)