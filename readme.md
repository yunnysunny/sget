# sget

跨平台 HTTP 下载工具，支持分段（多线程）下载、指定线程数，以及通过 `.sget.meta` 断点续传。

## 使用

```sh
sget http://example.com/archive.tar
sget http://example.com/archive.tar downloads -t 4
```

`-t` 接受 1–64 个线程，默认 4。服务器不支持字节范围（`Accept-Ranges: bytes`）或文件
过小时会自动退回单连接下载。中断后再次运行相同命令即可续传；续传时若服务器返回的
强 ETag 与上次不一致（资源被替换），会自动放弃旧进度重新下载。

目前只支持 `http://`。不支持 HTTPS、代理和重定向——遇到 3xx 会提示改用最终 URL，
不会静默跟随。也不支持 `Transfer-Encoding: chunked` 与压缩内容：请求发送
`Accept-Encoding: identity`，若服务器仍返回这些编码会报错退出，而不是写入错误数据。

## 构建与测试

Linux / macOS：

```sh
./build.sh
```

或者使用 CMake（Windows/Linux/macOS）：

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

需要支持 C99 的编译器。Windows 下可用 w64devkit / MinGW：

```sh
gcc -std=c99 -DWIN32 -D_CRT_SECURE_NO_WARNINGS -Wall -Wextra -O2 \
    -o sget.exe log.c win2linux.c common_socket.c sget_thread.c metadata.c download.c utest.c -lws2_32
```

旧的 `sget.vcproj` 是 VS2008 工程文件，已删除；请使用 CMake 或上面的命令行构建。
