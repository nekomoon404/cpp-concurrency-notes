#!/bin/bash

# 检查build目录是否存在，如果不存在则创建
if [ ! -d "build" ]; then
  mkdir build
fi

# 进入build目录
cd build

# 运行cmake以配置项目
cmake ..

# 运行make以构建项目
make

# 运行构建的可执行文件
./ThreadProject