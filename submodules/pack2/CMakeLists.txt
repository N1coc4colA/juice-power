cmake_minimum_required(VERSION 3.16)

project(pack2 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(pack2 SHARED
  cCut.cpp
  cCut.h
  cShape.cpp
  pack2.cpp
  pack2.h
)

target_compile_definitions(pack2 PRIVATE PACK2_LIBRARY)
