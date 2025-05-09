cmake_minimum_required(VERSION 3.28...3.30)

option(ENABLE_WEBRTC "Enable WebRTC Output support" ON)
if(NOT ENABLE_WEBRTC)
  target_disable(obs-webrtc)
  return()
endif()

find_package(LibDataChannel 0.21 REQUIRED)
find_package(CURL REQUIRED)

add_library(obs-webrtc MODULE)
add_library(OBS::webrtc ALIAS obs-webrtc)

target_sources(
  obs-webrtc
  PRIVATE obs-webrtc.cpp whip-output.cpp whip-output.h whip-service.cpp whip-service.h whip-utils.h
)

target_link_libraries(obs-webrtc PRIVATE OBS::libobs LibDataChannel::LibDataChannel CURL::libcurl)

set_target_properties_obs(obs-webrtc PROPERTIES FOLDER plugins PREFIX "")
