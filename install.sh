#!/bin/bash

# Obtener el directorio del script para hacerlo portable
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
# Directorio de música por defecto en el HOME del usuario
MUSIC_DIR="$HOME/Música"
BUILD_DIR="$PROJECT_DIR/build"

echo "--- Iniciando Script de Instalación de YtDlpWrapper ---"

# --- 1) Verificar dependencias ---
echo "1) Verificando dependencias..."

DEPENDENCIES=(
    "yt-dlp:yt-dlp (paquete de Python para descargas de YouTube)"
    "ffmpeg:ffmpeg (herramienta para procesamiento de audio/video, requerida por yt-dlp)"
    "cmake:cmake (generador de sistema de construcción)"
    "g++:g++ (compilador de C++)"
    "make:make (herramienta de automatización de construcción)"
)

MISSING_DEPS=()

for dep_cmd_info in "${DEPENDENCIES[@]}"; do
    IFS=':' read -r cmd_name description <<< "$dep_cmd_info"
    if ! command -v "$cmd_name" &> /dev/null; then
        MISSING_DEPS+=("$description (comando: $cmd_name)")
    fi
done

# Verificación especial para los archivos de desarrollo de ncurses
# CMake usualmente fallará si estos faltan, pero una advertencia es útil.
# Esto es una heurística, no una verificación definitiva.
if ! dpkg -s libncurses-dev &> /dev/null && ! rpm -q ncurses-devel &> /dev/null && ! pacman -Q ncurses &> /dev/null; then
    echo "Advertencia: No se detectaron los archivos de desarrollo de ncurses." 
    echo "  Para Debian/Ubuntu: sudo apt install libncurses-dev"
    echo "  Para Fedora: sudo dnf install ncurses-devel"
    echo "  Para Arch Linux: sudo pacman -S ncurses"
fi


if [ ${#MISSING_DEPS[@]} -gt 0 ]; then
    echo "Las siguientes dependencias no están instaladas o no se encontraron en el PATH:"
    for dep in "${MISSING_DEPS[@]}"; do
        echo "  - $dep"
    done
    echo ""
    echo "Por favor, instale las dependencias faltantes y vuelva a ejecutar el script."
    echo "Sugerencias de instalación:"
    echo "  yt-dlp: pip install yt-dlp (o consulte la documentación oficial para su sistema)"
    echo "  ffmpeg, cmake, g++, make: Use su gestor de paquetes (ej. sudo apt install ffmpeg cmake g++ make)"
    exit 1
elif [ ${#MISSING_DEPS[@]} -eq 0 ]; then
    echo "Todas las dependencias de comandos encontradas."
fi

echo ""

# --- 2) Crear carpeta Música ---
echo "2) Creando la carpeta '$MUSIC_DIR' si no existe..."
if [ ! -d "$MUSIC_DIR" ]; then
    mkdir -p "$MUSIC_DIR"
    if [ $? -eq 0 ]; then
        echo "Carpeta '$MUSIC_DIR' creada exitosamente."
    else
        echo "Error: No se pudo crear la carpeta '$MUSIC_DIR'. Verifique los permisos."
        exit 1
    fi
else
    echo "La carpeta '$MUSIC_DIR' ya existe."
fi

echo ""

# --- 3) Compilar el programa ---
echo "3) Compilando el programa..."

# Crear directorio de construcción si no existe
mkdir -p "$BUILD_DIR"

# Navegar al directorio de construcción y compilar
pushd "$BUILD_DIR" > /dev/null # Guarda el directorio actual y suprime la salida
cmake -DMUSIC_DIR="$MUSIC_DIR" ..
if [ $? -ne 0 ]; then
    echo "Error: CMake falló. Verifique la configuración de su entorno y las dependencias de desarrollo (ej. libncurses-dev)."
    popd > /dev/null # Vuelve al directorio original
    exit 1
fi

make
if [ $? -ne 0 ]; then
    echo "Error: Make falló. Verifique los errores de compilación."
    popd > /dev/null # Vuelve al directorio original
    exit 1
fi

popd > /dev/null # Vuelve al directorio original

echo "Compilación completada exitosamente."
echo ""
echo "--- Instalación Finalizada ---"
echo "El ejecutable se encuentra en: $BUILD_DIR/yt-dlp-tui"
echo "Puede ejecutarlo con: $BUILD_DIR/yt-dlp-tui"