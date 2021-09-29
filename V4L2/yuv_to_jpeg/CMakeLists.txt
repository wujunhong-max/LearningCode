#****************************This is Project Info****************************
project(ProJ)	# 文件名
cmake_minimum_required(VERSION 2.8.12)	#最低版本号要求

#****************************This is Include File/Directories****************************
# 添加编译器用于查找头文件的文件夹
# ${CMAKE_CURRENT_SOURCE_DIR}这是当前处理的CMakeLists.txt所在的目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/)

#****************************This is Source File/Directories****************************
aux_source_directory(./src source_src)		#查找当前目录下的所有源文件，将所有的源文件存储在source_src变量中
set(DIR_SRCS ${source_src})	   				

#****************************This is Library Directories****************************
LINK_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/lib/)	#添加需要链接的库文件目录

#****************************This is 3Party Library****************************
LINK_LIBRARIES(/usr/local/lib/libjpeg.so.9)				#添加需要链接的库文件目录

#**********************CPLUSPLUS FAGS**************************************
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O2")

#****************************This is Output****************************

add_executable(${PROJECT_NAME} ${DIR_SRCS})			#指定生成目标
target_link_libraries(${PROJECT_NAME})				#设置要链接的库文件的名称
