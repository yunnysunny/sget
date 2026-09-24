# sget

跨平台 HTTP 下载工具，支持分段（多线程）下载、可视化进度条和断点续传。

## 使用

```sh
sget http://example.com/archive.tar
sget http://example.com/archive.tar downloads -t 4
sget --version
```

Windows 下运行 `sget.exe`。命令格式为 `sget <url> [saveFolder] [-t N]`：

- `url`：必填，目前只接受 `http://` URL。
- `saveFolder`：可选，默认保存到当前目录；不存在时只创建这一级目录，父目录需事先存在。
- `-t N`：下载线程数，范围 1–64，默认 4。
- `--version`：打印版本号后退出，退出码为 `0`。

文件名优先取响应中的 `Content-Disposition`，否则取 URL 的最后一段（没有时用
`index.html`）。进度条显示百分比和已下载字节数；服务器未给出文件大小时，只显示
活动指示符和已下载字节数。进度条使用回车原地刷新，输出重定向到文件时会保留这些回车。

成功退出码为 `0`，下载失败或命令行参数无效为 `1`。失败时显示具体诊断信息；
`Download failed (error 0x...)` 中的错误码主要用于连接、收发和写入失败，
HTTP 状态错误等情形可能显示 `0x00000000`，应以之前的诊断信息为准。

## 断点续传

当服务器支持字节范围、文件大小已知且使用多个线程时，sget 会在目标文件旁创建
`<文件名>.sget.meta`。下载中断后，用相同 URL 和保存目录重新运行即可尝试续传；
完成后元数据文件会被删除。服务器不支持字节范围、大小未知或使用 `-t 1` 时，
改用单连接下载，**不会**创建元数据或续传，重新运行会从头下载。

多线程续传会检查 URL 和文件大小。服务器提供强 ETag 时会保存它，并在分段请求中
发送 `If-Range`；如果本次探测得到不同的强 ETag，就从头下载。若原下载或本次探测
没有强 ETag，程序仍可能复用旧进度，但无法验证远端内容是否已更换。
对完整性要求高时，请在下载后自行校验发布方提供的哈希值。

## 协议限制

目前不支持 HTTPS、代理或重定向。遇到 3xx 时需改用最终 URL；
不支持 `Transfer-Encoding: chunked` 或压缩的响应内容。请求会发送
`Accept-Encoding: identity`，若服务器仍返回不支持的编码，会报错退出。

## 下载安装

从 [Releases](https://github.com/yunnysunny/sget/releases) 下载对应平台的压缩包：

| 平台 | 文件 |
| --- | --- |
| Windows x64 (MinGW) | `sget-<版本>-windows-x64.zip` |
| Linux x64 | `sget-<版本>-linux-x64.tar.gz` |
| macOS arm64 / x64 | `sget-<版本>-macos-arm64.tar.gz`、`sget-<版本>-macos-x64.tar.gz` |

Windows 包内是 `sget.exe`（只依赖系统自带的 `msvcrt.dll` 和 `WS2_32.dll`，无需额外运行库）。
Linux/macOS 包内是保留可执行权限的 `sget`。每个 Release 附 `SHA256SUMS.txt`，
下载后可自行校验。

版本号取 Git tag（如 `v1.0.0`）；`sget --version` 打印的就是构建时的这个值。

## 构建与测试

需要支持 C99 的编译器和 CMake 3.16 或更新版本。CMake 支持 Windows、Linux
和 macOS；仓库 CI 在 Linux (GCC/Clang)、macOS (arm64/x64) 和 Windows
(MSVC/MinGW) 上构建并运行核心测试。

开发构建的产物可在 GitHub Actions 页面的 Artifacts 中下载：`sget-linux-x64`、
`sget-macos-arm64`、`sget-macos-x64`、`sget-windows-x64-msvc` 和
`sget-windows-x64-mingw`。这些是持续集成产物，保留 14 天；对外发布的版本请看 Releases。

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

使用 Visual Studio 多配置生成器时，构建和测试都需指定配置：

```sh
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Linux / macOS / MSYS2 / w64devkit 也可以直接运行 `./build.sh` 构建可执行文件；
设置 `SGET_VERSION` 可写入版本号（`SGET_VERSION=v1.0.0 ./build.sh`）。
旧的 VS2008 `sget.vcproj` 已删除。

打 tag 推送（`v1.0.0` 格式）会触发 `.github/workflows/release.yml`，构建各平台产物、
生成 `SHA256SUMS.txt` 并创建对应的 GitHub Release。

参与开发与提交前检查见 [CONTRIBUTING.md](CONTRIBUTING.md)，许可证见
[LICENSE](LICENSE)。
