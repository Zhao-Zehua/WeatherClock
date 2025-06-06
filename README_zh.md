# WeatherClock 天气时钟

一个基于 ESP32-C3 和 128×128 ST7735 屏幕制作的天气时钟。Fork 自 [ESP32C3_1.44inch](https://github.com/Spotpear/ESP32C3_1.44inch)。完全重构了主要代码，以提高可读性和可维护性。

本项目是北京大学化学与分子工程学院本科生劳动实践课的一部分。

作者：[@Zhao-Zehua](https://github.com/Zhao-Zehua)

本文档为[README.md](https://github.com/Zhao-Zehua/WeatherClock/blob/main/README.md)的中文版本，由AI自动翻译。

## 1 环境配置

1. 安装 [Arduino IDE](https://www.arduino.cc/en/software)。
2. 安装 ESP32 开发板支持包：
    - 打开 Arduino IDE，进入 **文件** → **首选项**。
    - 在“附加开发板管理器网址”字段中，添加：`https://dl.espressif.com/dl/package_esp32_index.json`。
    - 进入 **工具** → **开发板** → **开发板管理器**，搜索 “ESP32” 并安装 `2.0.9` 版本。其他版本可能也可用，但这是经过测试的版本。
3. 将本仓库 `libraries` 文件夹中的库复制到您的 Arduino 库文件夹中 (通常位于 `~/Documents/Arduino/libraries`)。
4. 将 `WeatherClock` 文件夹复制到您的 Arduino 项目文件夹中 (通常位于 `~/Documents/Arduino`)。

## 2 配置

1. 在 Arduino IDE 中打开 `WeatherClock.ino`。
2. 设置您的 Wi-Fi 凭据：

    ```cpp
    const char* ssid = "your-ssid";
    const char* password = "your-password";
    ```

3. 设置下方个性文本：

    ```cpp
    // 注意：不完全支持中文字符
    String motto = "Your motto";
    ```

## 3 编译和上传

1. 将您的 ESP32-C3 开发板连接到电脑，并选择 **ESP32C3 Dev Module** 作为开发板。
2. 在 Arduino IDE 中点击上传按钮，编译代码并将其上传到您的 ESP32-C3 开发板。

## 4 进阶定制

### 4.1 更改动画

动画尺寸为 30×31 像素。要更改动画，请按照以下步骤操作：

1. 获取你的动画文件。作为参考，你可以在 [Piskel](https://www.piskelapp.com/) 找到动画。
2. 使用 [ezgif 裁剪工具](https://ezgif.com/crop) 或 [ezgif 缩放工具](https://ezgif.com/resize) 将其裁剪或缩放至 30×31 像素。
3. 假设你的文件名为 `my_animation.gif`：
4. 使用 [LVGL 图像转换器](https://lvgl.io/tools/imageconverter) 将 GIF 转换为 C 数组：
    - 选择 **LGVL V8** 工具
    - 将颜色格式设置为 **CF_RAW**
    - 将输出格式设置为 **C array**
    - 点击 **Convert** 并下载生成的 C 文件 `my_animation.c`
5. 将 `my_animation.c` 复制到 `WeatherClock` 文件夹中。
6. 在 `WeatherClock.ino` 中，用你的新数组替换现有的动画数组：

    ```cpp
    // 搜索 LV_IMG_DECLARE 并替换现有的声明
    LV_IMG_DECLARE(my_animation);

    // 搜索 lv_gif_set_src 函数调用并替换它
    lv_gif_set_src(logo_imga, my_animation);
    ```

### 4.2 更改图标

图标尺寸为 18×18 像素。要更改图标，您有两个选项：

**选项 1：** 使用转换工具

使用 [`utils.ipynb`](https://github.com/Zhao-Zehua/WeatherClock/blob/main/utils.ipynb) 中的 `image_to_h` 函数。

**选项 2：** 创建自定义像素风格图标

1. 使用 [Piskel](https://www.piskelapp.com/) 创建你的图标：
    - 导入您的图片
    - 设置画布大小为 18×18 并调整图片大小
    - 如果需要，可以手动编辑图像
    - 导出为 PNG 文件
2. 使用 [`utils.ipynb`](https://github.com/Zhao-Zehua/WeatherClock/blob/main/utils.ipynb) 中的 `image_to_h` 函数将 PNG 转换为头文件。

**应用新图标：**

1. 将生成的头文件 (例如 `icon.h`) 复制到 `WeatherClock/img` 文件夹中。
2. 在 `WeatherClock.ino` 中，更新图标引用：

    ```cpp
    // 更新 include 语句
    #include "img/icon.h"

    // 更新函数调用 (搜索 TJpgDec.drawJpg)
    TJpgDec.drawJpg(1, 108, icon, sizeof(icon));
    ```
