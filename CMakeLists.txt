cmake_minimum_required(VERSION 3.16)
project(Survival2D LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Сначала ищем установленную SFML 2.x, иначе скачиваем и собираем сами
find_package(SFML 2.5 COMPONENTS graphics window system QUIET)
if(NOT SFML_FOUND)
    include(FetchContent)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG 2.6.1)
    FetchContent_MakeAvailable(SFML)
endif()

add_executable(survival main.cpp)
target_link_libraries(survival PRIVATE sfml-graphics sfml-window sfml-system)

if(MSVC)
    target_compile_options(survival PRIVATE /utf-8)
    # Запуск из Visual Studio: рабочая папка = папка проекта (там лежит textures/)
    set_target_properties(survival PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

# Копируем textures/ (и assets/) рядом с exe, если они есть
foreach(dir textures assets)
    if(EXISTS ${CMAKE_SOURCE_DIR}/${dir})
        add_custom_command(TARGET survival POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/${dir} $<TARGET_FILE_DIR:survival>/${dir})
    endif()
endforeach()
