# 参与开发

## 构建与测试

修改代码前请先确认本地可以构建。项目使用 C99，CMake 至少为 3.16。
从仓库根目录运行：

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

使用 Visual Studio 多配置生成器时，构建和测试加上 `--config Release` 和
`-C Release`。Linux / macOS 也可以运行 `./build.sh` 检查命令行构建。
CI 在 Linux (GCC、Clang) 和 Windows (MSVC、MinGW) 上执行构建与核心测试。

## 提交改动

- 为行为修复补充聚焦的测试；核心测试位于 `tests/test_core.c`，由 CTest 运行。
- 修改 HTTP 下载流程时，检查单连接和多线程路径，以及 Range、续传和错误响应。
- 提交前运行上述构建与测试，并执行 `git diff --check`。
- 自动化测试不要依赖公开下载站；外网下载可能因网络或服务端变化而不稳定。
- 更新命令行行为或支持范围时，同步更新 `readme.md`。

如需报告问题，请提供操作系统、编译器、复现命令、输出和预期行为；
若涉及私有 URL，先移除地址中的凭证或其他敏感信息。
