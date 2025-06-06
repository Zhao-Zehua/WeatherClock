# WeatherClock

Weather clock built with ESP32-C3 and a 128×128 ST7735 screen. Forked from [ESP32C3_1.44inch](https://github.com/Spotpear/ESP32C3_1.44inch). Main code is fully reconstructed to improve readability and maintainability.

This project is part of the Labor Practice Course for undergraduates at College of Chemistry and Molecular Engineering, Peking University.

Author: [@Zhao-Zehua](https://github.com/Zhao-Zehua)

AI-translated Chinese README: [README_CN.md](https://github.com/Zhao-Zehua/WeatherClock/blob/main/README_zh.md)

## 1 Environment Setup

1. Install [Arduino IDE](https://www.arduino.cc/en/software).
2. Install the ESP32 board support package:
    - Open Arduino IDE, go to **File** → **Preferences**.
    - In the "Additional Board Manager URLs" field, add: `https://dl.espressif.com/dl/package_esp32_index.json`.
    - Go to **Tools** → **Board** → **Boards Manager**, search for "ESP32" and install version `2.0.9`. Other versions may work, but this is the version tested.
3. Copy the libraries from the `libraries` folder of this repository to your Arduino libraries folder (usually located at `~/Documents/Arduino/libraries`).
4. Copy the `WeatherClock` folder to your Arduino sketchbook folder (usually located at `~/Documents/Arduino`).

## 2 Configuration

1. Open `WeatherClock.ino` in Arduino IDE.
2. Set your Wi-Fi credentials:

    ```cpp
    const char* ssid = "your-ssid";
    const char* password = "your-password";
    ```

3. Set your personal motto:

    ```cpp
    // Note: Chinese characters are not fully supported
    String motto = "Your motto";
    ```

## 3 Compiling and Uploading

1. Connect your ESP32-C3 board to your computer and select **ESP32C3 Dev Module** as the board.
2. Click the upload button in Arduino IDE to compile and upload the code to your ESP32-C3 board.

## 4 Further Customization

### 4.1 Change the Animation

The animation size is 30×31 pixels. To change the animation, follow these steps:

1. Obtain your animation file. For reference, you can find animations at [Piskel](https://www.piskelapp.com/).
2. Crop or resize it to 30×31 pixels using [ezgif crop tool](https://ezgif.com/crop) or [ezgif resize tool](https://ezgif.com/resize).
3. Assuming your file is named `my_animation.gif`:
4. Convert the GIF to a C array using [LVGL Image Converter](https://lvgl.io/tools/imageconverter):
    - Select **LGVL V8** tool
    - Set **CF_RAW** as the color format
    - Set **C array** as the output format
    - Click **Convert** and download the generated C file `my_animation.c`
5. Copy `my_animation.c` to the `WeatherClock` folder.
6. In `WeatherClock.ino`, replace the existing animation array with your new one:

    ```cpp
    // Search for LV_IMG_DECLARE and replace the existing declaration
    LV_IMG_DECLARE(my_animation);
    
    // Search for lv_gif_set_src function call and replace it
    lv_gif_set_src(logo_imga, my_animation);
    ```

### 4.2 Change the Icon

The icon size is 18×18 pixels. To change the icon, you have two options:

**Option 1:** Use the Conversion Tool

Use the `image_to_h` function in [`utils.ipynb`](https://github.com/Zhao-Zehua/WeatherClock/blob/main/utils.ipynb).

**Option 2:** Create a Custom Pixel Art Icon

1. Create your icon using [Piskel](https://www.piskelapp.com/):
    - Import your image
    - Set the canvas size to 18×18 and resize the image (it will be automatically pixelated)
    - Edit the image manually if needed
    - Export as a PNG file
1. Convert the PNG to a header file using the `image_to_h` function in [`utils.ipynb`](https://github.com/Zhao-Zehua/WeatherClock/blob/main/utils.ipynb).

**Apply the New Icon:**

1. Copy the generated header file (e.g., `icon.h`) to the `WeatherClock/img` folder.
2. In `WeatherClock.ino`, update the icon references:

    ```cpp
    // Update the include statement
    #include "img/icon.h"
    
    // Update the function call (search for TJpgDec.drawJpg)
    TJpgDec.drawJpg(1, 108, icon, sizeof(icon));
    ```
