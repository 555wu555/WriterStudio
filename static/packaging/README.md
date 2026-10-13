# Windows 安装包准备

安装目录与作品库目录相互独立：安装向导选择程序安装位置；新用户第一次启动时选择作品库存放位置，也可以打开已有作品库。之后可在「设置 → 文件与数据 → 作品库位置」搬迁。卸载程序不会自动删除作品库和用户设置。

## 构建与打包

1. 用与 Qt 6.5.3 MinGW 64 位版兼容的编译器配置单独的 Release 构建目录。配置 CMake 时传入自己的 Qt 安装路径，例如 `-DCMAKE_PREFIX_PATH=<Qt 安装目录>/6.5.3/mingw_64`，不要把该路径写回项目源码。构建目标为 `write`。
2. 执行 `packaging/Prepare-Release.ps1 -BuildDir <Release 构建目录> -QtBin <Qt 安装目录>/6.5.3/mingw_64/bin`。脚本会清空并重建 `dist/app`，只从 Release 可执行文件和运行依赖生成暂存目录。它会先使用 `windeployqt`；若当前 Qt 工具无法识别平台，则扫描 DLL 依赖并部署本项目用到的插件。
3. 确认 `dist/app` 只有程序、DLL、Qt 插件和 `qt.conf`。不要把 `WriterStudio`、构建目录、个人配置、API Key、小说文件、日志或测试数据放进 `dist/app`。
4. 在你自己的 Inno Setup 脚本中，仅将 `dist/app` 作为程序文件来源，并启用安装目录选择页（`DisableDirPage=no`）。不要把项目根目录或整个构建目录作为来源。
5. 在没有 Qt 开发环境的 Windows 用户账户或虚拟机中测试首次启动、作品库选择、云端 HTTPS、升级和卸载。

首次安装如果已经有旧作品库，选择**包含 `projects` 文件夹的目录**；程序不会自动迁移或删除旧数据。
