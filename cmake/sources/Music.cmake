target_sources(
    advanced_platformer_music
    PRIVATE
        ${PROJECT_SOURCE_DIR}/app/music/main.cpp
        ${PROJECT_SOURCE_DIR}/app/audio/audio_device.cpp
        ${PROJECT_SOURCE_DIR}/app/audio/audio_playback.cpp
        ${PROJECT_SOURCE_DIR}/app/audio/wave_file.cpp
)
