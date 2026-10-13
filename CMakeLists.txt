cmake_minimum_required(VERSION 4.1)
project(write)

if (WIN32)
    enable_language(RC)
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS
        Core
        Gui
        Widgets
        Network
        REQUIRED)

add_executable(write main.cpp
        MainWin.cpp
        MainWin.h
        WorkPlaces/workingPaper/Paper.cpp
        WorkPlaces/workingPaper/Paper.h
        WorkPlaces/workingPaper/ArchiveDialog.cpp
        WorkPlaces/workingPaper/ArchiveDialog.h
        WorkPlaces/workingPaper/ImportPreviewDialog.cpp
        WorkPlaces/workingPaper/ImportPreviewDialog.h
        WorkPlaces/workingPaper/ProjectListDelegate.h
        WorkPlaces/WritePaper/Writer.cpp
        WorkPlaces/WritePaper/Writer.h
        WorkPlaces/DesignPaper/designer.cpp
        WorkPlaces/DesignPaper/designer.h
        WorkPlaces/DesignPaper/CreateEntityDialog.cpp
        WorkPlaces/DesignPaper/CreateEntityDialog.h
        WorkPlaces/RelationPaper/Relater.cpp
        WorkPlaces/RelationPaper/Relater.h
        WorkPlaces/RelationPaper/CreateRelationshipDialog.cpp
        WorkPlaces/RelationPaper/CreateRelationshipDialog.h
        WorkPlaces/SettingPaper/Setter.cpp
        WorkPlaces/SettingPaper/Setter.h
        WorkPlaces/SettingPaper/ModelDebugDialog.cpp
        WorkPlaces/SettingPaper/ModelDebugDialog.h
        resources.qrc
        models/Project.h
        storage/ProjectStore.cpp
        storage/ProjectStore.h
        storage/StoragePaths.h
        models/dialogs/CreateProjectDialog.cpp
        models/dialogs/CreateProjectDialog.h
        storage/Chapter/ChapterStore.cpp
        storage/Chapter/ChapterStore.h
        WorkPlaces/WritePaper/WriterWorkspace/WriterWorkspace.cpp
        WorkPlaces/WritePaper/WriterWorkspace/WriterWorkspace.h
        WorkPlaces/WritePaper/WriterWorkspace/TabCloseButton.h
        storage/DocumentAbout/DocumentImporter.cpp
        storage/DocumentAbout/DocumentImporter.h
        storage/SetUp/SetupStore.cpp
        storage/SetUp/SetupStore.h
        models/StorySetup.h
        storage/AppSettings.h
        storage/GlobalWritingConventionStore.h
        storage/GlobalWritingConventionStore.cpp
        models/Entity.h
        models/Relationship.h
        models/WorldData.h
        storage/EntityStore/EntityStore.cpp
        storage/EntityStore/EntityStore.h
        WorkPlaces/ContextBuilder/ContextBuilder.cpp
        WorkPlaces/ContextBuilder/ContextBuilder.h
        storage/DataRootStore.cpp
        storage/DataRootStore.h
        storage/DataRootMigration.cpp
        storage/DataRootMigration.h
        WorkPlaces/Generation/GenerationRequest.h
        WorkPlaces/Generation/ModelRequestLog.cpp
        WorkPlaces/Generation/ModelRequestLog.h
        WorkPlaces/Generation/MockGenerationService.cpp
        WorkPlaces/Generation/MockGenerationService.h
        WorkPlaces/Generation/InlineSuggestionFilter.cpp
        WorkPlaces/Generation/InlineSuggestionFilter.h
        WorkPlaces/Generation/InlinePrompt.cpp
        WorkPlaces/Generation/InlinePrompt.h
        storage/InlineSuggestionSettings.cpp
        storage/InlineSuggestionSettings.h
        storage/InlinePromptSettingsStore.cpp
        storage/InlinePromptSettingsStore.h
        storage/ModelSettingsStore.cpp
        storage/ModelSettingsStore.h
        storage/CloudProfileStore.cpp
        storage/CloudProfileStore.h
        storage/CloudCredentialStore.cpp
        storage/CloudCredentialStore.h
        storage/ActiveModelServiceStore.cpp
        storage/ActiveModelServiceStore.h
        storage/ModelServiceIdentity.h
        WorkPlaces/Generation/GenerationService.h
        WorkPlaces/Generation/CloudChatApi.cpp
        WorkPlaces/Generation/CloudChatApi.h
        WorkPlaces/Generation/CloudGenerationService.cpp
        WorkPlaces/Generation/CloudGenerationService.h
        WorkPlaces/Generation/RoutedGenerationService.cpp
        WorkPlaces/Generation/RoutedGenerationService.h
        WorkPlaces/Generation/OllamaGenerationService/OllamaGenerationService.cpp
        WorkPlaces/Generation/OllamaGenerationService/OllamaGenerationService.h)
if (WIN32)
    # 发布版使用 GUI 子系统，双击启动时不弹出控制台；Debug 保留控制台日志。
    set_target_properties(write PROPERTIES
            WIN32_EXECUTABLE "$<NOT:$<CONFIG:Debug>>")
endif ()
target_link_libraries(write
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Network
)
if (WIN32)
    target_link_libraries(write Advapi32 Dwmapi)
endif ()

if (WIN32)
    target_sources(write PRIVATE app-icon.rc)
    set_source_files_properties(app-icon.rc PROPERTIES
            OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/logo.ico")
endif ()

if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(DEBUG_SUFFIX)
    if (MSVC AND CMAKE_BUILD_TYPE MATCHES "Debug")
        set(DEBUG_SUFFIX "d")
    endif ()
    # Qt6_DIR 由 find_package(Qt6) 确定，不依赖开发机的 CMAKE_PREFIX_PATH。
    get_filename_component(QT_INSTALL_PATH "${Qt6_DIR}/../../.." ABSOLUTE)
    if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
        message(FATAL_ERROR "无法从 Qt6_DIR 找到 Qt 的 bin 目录：${Qt6_DIR}")
    endif ()
    if (EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
    endif ()
    if (EXISTS "${QT_INSTALL_PATH}/plugins/imageformats/qjpeg${DEBUG_SUFFIX}.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/imageformats/")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_INSTALL_PATH}/plugins/imageformats/qjpeg${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/imageformats/")
    endif ()
    # HTTPS 请求需要 Qt 的 TLS 插件。Windows Schannel 使用系统 TLS，
    # 不需要额外部署 OpenSSL DLL。
    if (EXISTS "${QT_INSTALL_PATH}/plugins/tls/qschannelbackend${DEBUG_SUFFIX}.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/tls/")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_INSTALL_PATH}/plugins/tls/qschannelbackend${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/tls/")
    endif ()
    foreach (QT_LIB Core Gui Widgets Network)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:Qt6::${QT_LIB}>"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
    endforeach (QT_LIB)
endif ()
