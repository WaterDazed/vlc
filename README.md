# 如何使用

1. 获取针对`VLC 3.0.x`版本的Docker镜像，Create容器并将Windows目录挂载到容器
```cmd
docker pull registry.videolan.org/vlc-debian-win64-3.0:20260504124015
docker create --name vlc -v 你的Windows目录:~/projects/vlc registry.videolan.org/vlc-debian-win64-3.0:20260504124015
```

2. 启动容器，将项目Clone到容器，在项目根目录运行以完成构建
```bash
docker start vlc
mkdir vlc
cd vlc
git clone -b 3.0.x https://github.com/WaterDazed/vlc.git
mkdir build
cd build
../vlc/extras/package/win32/build.sh -a x86_64 -m
cd win64
make -j22 clean
bear --output /tmp/compile_commands.json -- make -j22 all && mv /tmp/compile_commands.json ./compile_commands.json
make -j22 package-win-common
```

3. 将`win64/vlc-3.0.23`目录置于Windows下启动
```cmd
vlc.exe --adaptive-logic=mylogic
```