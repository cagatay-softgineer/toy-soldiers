cmake_minimum_required(VERSION 3.22)
project(toy_soldiers
	VERSION 1.2.0
	DESCRIPTION "Oyuncak Asker Masa Savasi — Box3D game jam prototype"
	LANGUAGES C CXX
)

# v1.2 #161: sokol_impl.m (Metal/macOS backend) needs the Objective-C frontend.
if(APPLE)
	enable_language(OBJC)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ---------------------------------------------------------------------------
# Box3D (sibling checkout: ../box3d)
# ---------------------------------------------------------------------------
set(TOY_BOX3D_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../box3d" CACHE PATH "Path to Box3D source tree")
if(NOT EXISTS "${TOY_BOX3D_DIR}/CMakeLists.txt")
	message(FATAL_ERROR "Box3D not found at ${TOY_BOX3D_DIR}. Clone/build box3d next to this project.")
endif()

# Keep FetchContent downloads shared with the box3d tree when possible.
if(NOT FETCHCONTENT_BASE_DIR)
	set(FETCHCONTENT_BASE_DIR "${TOY_BOX3D_DIR}/.fetchcontent-cache"
		CACHE PATH "Shared FetchContent populate cache")
endif()

set(BOX3D_SAMPLES OFF CACHE BOOL "" FORCE)
set(BOX3D_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX3D_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(BOX3D_DOCS OFF CACHE BOOL "" FORCE)
set(BOX3D_VALIDATE OFF CACHE BOOL "" FORCE)

add_subdirectory("${TOY_BOX3D_DIR}" "${CMAKE_BINARY_DIR}/box3d" EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# ImGui (card HUD)
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(imgui
	GIT_REPOSITORY https://github.com/ocornut/imgui.git
	GIT_TAG        v1.92.7
	GIT_SHALLOW    TRUE
	GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
	"${imgui_SOURCE_DIR}/imgui.cpp"
	"${imgui_SOURCE_DIR}/imgui_draw.cpp"
	"${imgui_SOURCE_DIR}/imgui_tables.cpp"
	"${imgui_SOURCE_DIR}/imgui_widgets.cpp"
	"${imgui_SOURCE_DIR}/imgui_demo.cpp"
)
target_include_directories(imgui SYSTEM PUBLIC "${imgui_SOURCE_DIR}")
set_target_properties(imgui PROPERTIES
	CXX_STANDARD 20
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
)

# ---------------------------------------------------------------------------
# Sokol (vendored from Box3D extern)
# ---------------------------------------------------------------------------
set(SOKOL_DIR "${TOY_BOX3D_DIR}/extern/sokol")
if(NOT EXISTS "${SOKOL_DIR}/sokol_app.h")
	message(FATAL_ERROR "sokol headers not found under ${SOKOL_DIR}")
endif()

add_library(sokol INTERFACE)
target_include_directories(sokol SYSTEM INTERFACE "${SOKOL_DIR}")
# v0.9: sokol_audio.h vendored separately (box3d's extern set has no audio header).
target_include_directories(sokol SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/extern/sokol")
if(WIN32)
	target_compile_definitions(sokol INTERFACE SOKOL_D3D11)
	target_link_libraries(sokol INTERFACE d3d11 dxgi dxguid d3dcompiler user32 gdi32 shell32 ole32)
elseif(APPLE)
	target_compile_definitions(sokol INTERFACE SOKOL_METAL)
	target_link_libraries(sokol INTERFACE
		"-framework Foundation" "-framework Cocoa" "-framework QuartzCore"
		"-framework Metal" "-framework MetalKit" "-framework AudioToolbox")
else()
	target_compile_definitions(sokol INTERFACE SOKOL_GLCORE)
	find_package(OpenGL REQUIRED)
	find_package(Threads REQUIRED)
	find_package(X11 REQUIRED)
	target_link_libraries(sokol INTERFACE OpenGL::GL Threads::Threads X11 Xcursor Xi asound ${CMAKE_DL_LIBS})
endif()

# ---------------------------------------------------------------------------
# Game library (rules + physics + net — headless-safe)
# ---------------------------------------------------------------------------
add_library(toy_game STATIC
	src/game/types.h
	src/game/cards.cpp
	src/game/cards.h
	src/game/rules.cpp
	src/game/rules.h
	src/game/match.cpp
	src/game/match.h
	src/game/snapshot.cpp
	src/game/snapshot.h
	src/game/events.cpp
	src/game/events.h
	src/game/cosmetics.cpp
	src/game/cosmetics.h
	src/game/summary.cpp
	src/game/summary.h
	src/game/play_reason.cpp
	src/game/play_reason.h
	src/game/replay.cpp
	src/game/replay.h
	src/app/settings.cpp
	src/app/settings.h
	src/app/i18n.cpp
	src/app/i18n.h
	src/app/session_log.cpp
	src/app/session_log.h
	src/app/crash_handler.cpp
	src/app/crash_handler.h
	src/physics/table_scene.cpp
	src/physics/table_scene.h
	src/net/socket.cpp
	src/net/socket.h
	src/net/protocol.cpp
	src/net/protocol.h
	src/net/transport.cpp
	src/net/transport.h
	src/net/session.cpp
	src/net/session.h
	extern/lz4/lz4.c
	extern/lz4/lz4.h
)
target_include_directories(toy_game PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
# v1.2 #95: vendored lz4 for snapshot compression.
target_include_directories(toy_game SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/extern/lz4")
target_link_libraries(toy_game PUBLIC box3d)
if(WIN32)
	target_link_libraries(toy_game PUBLIC ws2_32)
endif()
if(MSVC)
	target_compile_options(toy_game PRIVATE /W4 /wd4201)
else()
	target_compile_options(toy_game PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# Headless smoke tests
# ---------------------------------------------------------------------------
add_executable(toy_sim src/sim_main.cpp)
target_link_libraries(toy_sim PRIVATE toy_game)
if(MSVC)
	set_property(TARGET toy_sim PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

add_executable(toy_net_test src/net_test_main.cpp)
target_link_libraries(toy_net_test PRIVATE toy_game)
if(MSVC)
	set_property(TARGET toy_net_test PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

add_executable(toy_event_test src/event_test_main.cpp)
target_link_libraries(toy_event_test PRIVATE toy_game)
if(MSVC)
	set_property(TARGET toy_event_test PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

add_executable(toy_cosmetic_test src/cosmetic_test_main.cpp)
target_link_libraries(toy_cosmetic_test PRIVATE toy_game)
if(MSVC)
	set_property(TARGET toy_cosmetic_test PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

add_executable(toy_balance_report src/balance_report_main.cpp)
target_link_libraries(toy_balance_report PRIVATE toy_game)
if(MSVC)
	set_property(TARGET toy_balance_report PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

add_executable(toy_mode_test src/mode_test_main.cpp)
target_link_libraries(toy_mode_test PRIVATE toy_game)
if(MSVC)
	set_property(TARGET toy_mode_test PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

add_executable(toy_content_test src/content_test_main.cpp)
target_link_libraries(toy_content_test PRIVATE toy_game)
if(MSVC)
	set_property(TARGET toy_content_test PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

add_executable(toy_golden_seed_test src/game/golden_seed_test_main.cpp)
target_link_libraries(toy_golden_seed_test PRIVATE toy_game)
if(MSVC)
	set_property(TARGET toy_golden_seed_test PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

add_executable(toy_replay_test src/replay_main.cpp)
target_link_libraries(toy_replay_test PRIVATE toy_game)
if(MSVC)
	set_property(TARGET toy_replay_test PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

add_executable(toy_lockstep_test src/lockstep_test_main.cpp)
target_link_libraries(toy_lockstep_test PRIVATE toy_game)
if(MSVC)
	set_property(TARGET toy_lockstep_test PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

# ---------------------------------------------------------------------------
# Graphical prototype
# ---------------------------------------------------------------------------
# v1.2 #161: sokol_impl.c can't build Metal/AppKit on Apple (needs Objective-C) — see sokol_impl.m.
if(APPLE)
	set(TOY_SOKOL_IMPL src/sokol_impl.m)
else()
	set(TOY_SOKOL_IMPL src/sokol_impl.c)
endif()

add_executable(toy_soldiers
	src/main.cpp
	${TOY_SOKOL_IMPL}
	src/sokol_imgui_impl.cpp
	src/app/audio.cpp
	src/app/audio.h
	src/app/capture.cpp
	src/app/capture.h
	src/render/camera.cpp
	src/render/camera.h
	src/render/draw3d.cpp
	src/render/draw3d.h
	src/render/qr_texture.cpp
	src/render/qr_texture.h
	src/render/ui.cpp
	src/render/ui.h
	extern/qrcodegen/qrcodegen.c
	extern/qrcodegen/qrcodegen.h
)
target_include_directories(toy_soldiers PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
# v1.2 #77/#188/#179: vendored msf_gif single-header (GIF burst + press-tour capture).
target_include_directories(toy_soldiers SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/extern/msf_gif")
# v1.2 #90: vendored qrcodegen (room-code QR in the host lobby).
target_include_directories(toy_soldiers SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/extern/qrcodegen")
target_link_libraries(toy_soldiers PRIVATE toy_game sokol imgui)
if(MSVC)
	target_compile_options(toy_soldiers PRIVATE /W4 /wd4201)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT toy_soldiers)
	set_property(TARGET toy_soldiers PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
else()
	target_compile_options(toy_soldiers PRIVATE -Wall -Wextra)
endif()
if(WIN32)
	# capture.cpp PNG path (#179/#188) uses GDI+.
	target_link_libraries(toy_soldiers PRIVATE gdiplus)
endif()

message(STATUS "Toy Soldiers → Box3D at ${TOY_BOX3D_DIR}")
