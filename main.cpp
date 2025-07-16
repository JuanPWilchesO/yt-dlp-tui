#include <ncurses.h>
#include <string>
#include <vector>
#include <memory>
#include <cstdio>
#include <array>
#include <thread>
#include <mutex>
#include <atomic>
#include <filesystem>
#include <algorithm>
#include <stdexcept> // For std::stoi

namespace fs = std::filesystem;

// MUSIC_DIR will be defined by CMake
#ifndef MUSIC_DIR
#define MUSIC_DIR "/tmp/Music" // Fallback for local testing if not defined by CMake
#endif

// Enum para manejar el estado de la aplicación
enum class AppState {
    WAITING_FOR_SONG,
    DOWNLOADING,
    ORGANIZING,
    CREATING_FOLDER
};
std::atomic<AppState> current_state = AppState::WAITING_FOR_SONG;

// Variables globales para la UI y el estado
WINDOW *output_win, *input_win, *border_win;
std::vector<std::string> output_lines;
std::mutex output_mutex;
std::string last_downloaded_file;
std::vector<std::string> music_dirs;

// Función para añadir una línea a la ventana de salida de forma segura
void add_to_output(const std::string& line) {
    std::lock_guard<std::mutex> lock(output_mutex);
    output_lines.push_back(line);
}

// Limpia y muestra el menú de organización
void show_organization_menu() {
    std::lock_guard<std::mutex> lock(output_mutex);
    output_lines.clear(); // Limpia la salida anterior
    output_lines.push_back("--- Descarga Finalizada ---");
    output_lines.push_back("Archivo: " + fs::path(last_downloaded_file).filename().string());
    output_lines.push_back("¿Dónde deseas mover el archivo?");
    
    music_dirs.clear();
    std::string music_path = MUSIC_DIR;
    try {
        for (const auto& entry : fs::directory_iterator(music_path)) {
            if (entry.is_directory()) {
                music_dirs.push_back(entry.path().filename().string());
            }
        }
    } catch (const fs::filesystem_error& e) {
        add_to_output("Error al leer los directorios de música: " + std::string(e.what()));
        current_state = AppState::WAITING_FOR_SONG;
        return;
    }
    std::sort(music_dirs.begin(), music_dirs.end());

    for (size_t i = 0; i < music_dirs.size(); ++i) {
        output_lines.push_back(std::to_string(i + 1) + ". " + music_dirs[i]);
    }
    output_lines.push_back("-----------------------------");
    output_lines.push_back("N. Crear nueva carpeta");
    output_lines.push_back("Q. Dejar en " + std::string(MUSIC_DIR));
    output_lines.push_back("Introduce una opción y presiona Enter:");
}

// Mueve el archivo a la carpeta seleccionada
void move_file(const std::string& dest_folder) {
    std::string dest_path_str = std::string(MUSIC_DIR) + "/" + dest_folder;
    fs::path dest_path(dest_path_str);
    
    try {
        if (!fs::exists(dest_path)) {
            fs::create_directory(dest_path);
            add_to_output("Carpeta creada: " + dest_folder);
        }
        
        fs::path source_path(last_downloaded_file);
        fs::path new_file_path = dest_path / source_path.filename();
        
        std::error_code ec;
        fs::rename(source_path, new_file_path, ec);
        if (ec) {
            add_to_output("Error al mover el archivo: " + ec.message());
        } else {
            add_to_output("Archivo movido a: " + dest_folder);
        }
    } catch (const fs::filesystem_error& e) {
        add_to_output("Error del sistema de archivos: " + std::string(e.what()));
    }
}

// Función que se ejecuta en un hilo para descargar la canción con yt-dlp
void run_download(const std::string& song_name) {
    current_state = AppState::DOWNLOADING;
    last_downloaded_file.clear();

    // Comando para buscar y descargar el audio en formato mp3.
    // El '2>&1' al final redirige stderr a stdout para capturar los mensajes de error también.
    // Las comillas están escapadas para el shell.
    std::string command = "yt-dlp -x --audio-format mp3 -o \"" + std::string(MUSIC_DIR) + "/%(title)s.%(ext)s\" \"ytsearch:" + song_name + "\" 2>&1";
    add_to_output("Ejecutando: " + command);

    std::array<char, 512> buffer;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        add_to_output("Error: no se pudo ejecutar el comando yt-dlp.");
        current_state = AppState::WAITING_FOR_SONG;
        return;
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        std::string line = buffer.data();
        if (!line.empty() && line.back() == '\n') {
            line.pop_back();
        }
        add_to_output(line);
        
        // Captura el nombre del archivo final desde la salida de yt-dlp
        std::string dest_marker = "[ExtractAudio] Destination: ";
        auto pos = line.find(dest_marker);
        if (pos != std::string::npos) {
            last_downloaded_file = line.substr(pos + dest_marker.length());
        }
    }
    
    if (!last_downloaded_file.empty() && fs::exists(last_downloaded_file)) {
        show_organization_menu();
        current_state = AppState::ORGANIZING;
    } else {
        add_to_output("--- No se pudo determinar el archivo descargado. Volviendo al inicio. ---");
        current_state = AppState::WAITING_FOR_SONG;
    }
}

// Dibuja la interfaz completa
void draw_ui(const std::string& input_str) {
    werase(border_win);
    box(border_win, 0, 0);
    mvwprintw(border_win, 0, 2, " Cliente de Descarga de Música ");
    wrefresh(border_win);

    werase(output_win);
    {
        std::lock_guard<std::mutex> lock(output_mutex);
        int max_y, max_x;
        getmaxyx(output_win, max_y, max_x);
        int start_line = output_lines.size() > max_y ? output_lines.size() - max_y : 0;
        for (int i = 0; i < max_y && (start_line + i) < output_lines.size(); ++i) {
            mvwprintw(output_win, i, 0, "%.*s", max_x, output_lines[start_line + i].c_str());
        }
    }
    wrefresh(output_win);

    werase(input_win);
    mvwprintw(input_win, 0, 0, "> %s", input_str.c_str());
    wmove(input_win, 0, 2 + input_str.length());
    wrefresh(input_win);
}

void handle_organization_input(const std::string& input) {
    if (input == "q" || input == "Q") {
        add_to_output("Archivo conservado en " + std::string(MUSIC_DIR) + ".");
    } else if (input == "n" || input == "N") {
        add_to_output("Por favor, introduce el nombre de la nueva carpeta:");
        current_state = AppState::CREATING_FOLDER;
        return; // Espera a la siguiente entrada
    } else {
        try {
            int choice = std::stoi(input);
            if (choice > 0 && choice <= music_dirs.size()) {
                move_file(music_dirs[choice - 1]);
            } else {
                add_to_output("Opción inválida.");
            }
        } catch (const std::invalid_argument&) {
            add_to_output("Entrada inválida. Por favor, elige un número, 'N', o 'Q'.");
        } catch (const std::out_of_range&) {
            add_to_output("Opción fuera de rango.");
        }
    }
    
    add_to_output("-----------------------------");
    add_to_output("Introduce un nuevo nombre de canción o presiona 'q' para salir.");
    current_state = AppState::WAITING_FOR_SONG;
}

void handle_create_folder_input(const std::string& folder_name) {
    if (!folder_name.empty()) {
        move_file(folder_name);
    } else {
        add_to_output("El nombre de la carpeta no puede estar vacío.");
    }
    add_to_output("-----------------------------");
    add_to_output("Introduce un nuevo nombre de canción o presiona 'q' para salir.");
    current_state = AppState::WAITING_FOR_SONG;
}


int main() {
    initscr();
    cbreak();
    noecho();
    curs_set(1);

    int height, width;
    getmaxyx(stdscr, height, width);

    border_win = newwin(height, width, 0, 0);
    output_win = newwin(height - 5, width - 4, 2, 2);
    input_win = newwin(3, width - 4, height - 3, 2);
    keypad(input_win, TRUE);
    wtimeout(input_win, 100);

    add_to_output("Bienvenido. Escribe el nombre de una canción y presiona Enter.");
    add_to_output("Escribe 'q' y presiona Enter para salir.");

    std::string input_str;
    int ch;

    while (true) {
        draw_ui(input_str);

        ch = wgetch(input_win);

        if (ch != ERR) {
            if (ch == '\n') { // Tecla Enter
                if (input_str == "q" || input_str == "Q") {
                     if (current_state == AppState::WAITING_FOR_SONG) break;
                     else { // Si está en otro estado, 'q' nos devuelve al inicio
                         current_state = AppState::WAITING_FOR_SONG;
                         input_str.clear();
                         add_to_output("Operación cancelada. Introduce un nombre de canción.");
                         continue;
                     }
                }

                if (!input_str.empty()) {
                    if (current_state == AppState::WAITING_FOR_SONG) {
                        std::thread(run_download, input_str).detach();
                    } else if (current_state == AppState::ORGANIZING) {
                        handle_organization_input(input_str);
                    } else if (current_state == AppState::CREATING_FOLDER) {
                        handle_create_folder_input(input_str);
                    }
                    input_str.clear();
                }
            } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') { // Retroceso
                if (!input_str.empty()) {
                    input_str.pop_back();
                }
            } else if (ch >= 32 && ch <= 126) { // Caracteres imprimibles
                input_str += ch;
            }
        }
    }

    endwin();
    return 0;
}