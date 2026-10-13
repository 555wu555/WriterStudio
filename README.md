# WriterStudio

WriterStudio 是一款面向中文小说写作的 Windows 桌面应用，使用 C++20 和 Qt 6 Widgets 开发。它把作品、章节、写作设定和人物关系放在同一个工作台中；AI 行内建议是可选功能，不影响离线写作。

## 主要功能

- **作品与章节**：管理多部作品；新建、排序和编辑章节；归档及恢复作品；导入、导出 TXT。
- **写作工作台**：多作品标签、自动保存、手动保存、上一版正文备份、专注模式。
- **作品设定**：记录故事大纲、写作约定，以及人物、地点、组织和物件资料。
- **关系图谱**：为资料建立带双向称呼的关系，并在可拖动、可缩放的圆点画布中查看。
- **AI 行内短语建议**：光标停顿后给出简短补全，按 `Tab` 接受、`Esc` 忽略；支持本机 Ollama 和 OpenAI 兼容的云端服务。

## 使用与数据

首次启动时可以选择作品库目录；作品和章节保存在所选位置，不需要放入程序安装目录。离线写作不要求配置模型。

启用 AI 时，可在设置页选择本机 Ollama 模型，或添加兼容的云端服务。云端服务的 API Key 保存在 Windows 凭据管理器；使用云端模型时，生成建议所需的写作上下文会发送到所选服务。请自行确认服务商的数据处理规则。

安装包通过 [Releases](https://github.com/555wu555/WriterStudio/releases) 页面单独发布；如果页面尚无附件，表示暂未提供安装包。构建目录和作品数据不属于源码；下载源码需要自行编译，不能直接运行。

## 从源码构建

需要 Windows、Qt 6.5.3 MinGW 64 位、与该 Qt 套件兼容的 C++20 编译器、CMake 4.1 或更新版本，以及 Ninja。在项目根目录运行：

```powershell
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<Qt 安装目录>/6.5.3/mingw_64"
cmake --build build-release
```

`<Qt 安装目录>` 需要替换为自己电脑上的实际路径。构建完成后运行 `build-release/write.exe`。发布安装包时，还需部署 Qt 与编译器的运行依赖；直接复制单个 EXE 不足以在普通 Windows 电脑上运行。

如需制作 Windows 安装包，请参阅[打包说明](static/packaging/README.md)。仓库中的 `packaging/Prepare-Release.ps1` 用于将 Release 程序及依赖整理到 `dist/app`；Inno Setup 脚本需按自己的发布信息和安装路径配置。

## 许可

本项目使用 [GNU GPL 第 3 版](LICENSE)。仓库中的许可证适用于项目自身代码；随安装包分发的 Qt 库及其他第三方组件仍遵循各自的许可证。
