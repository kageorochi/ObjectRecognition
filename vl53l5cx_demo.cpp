#include <stdio.h>

#include <string.h>
#include <math.h>
#include <vector>
#include <cstdlib>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "vl53l5cx.hpp"
#include "src/vl53l5cx_firmware.h"

#include "common/pimoroni_i2c.hpp"
#include "pico_display.hpp"
#include "drivers/st7789/st7789.hpp"
#include "libraries/pico_graphics/pico_graphics.hpp"
#include "rgbled.hpp"
#include "button.hpp"
#include "SEFR.h"

using namespace pimoroni;

#define POL_COL (15)
#define POL_ROW (15)
#define FEATURES (64)    // number of features
#define LABELS (4)       // number of labels
#define SAMPLES (50)      // samples per class
#define DATASET_MAXSIZE (SAMPLES * LABELS)


ST7789 st7789(PicoDisplay::WIDTH, PicoDisplay::HEIGHT, ROTATE_0, false, get_spi_pins(BG_SPI_FRONT));
//PicoGraphics_PenRGB332 graphics(st7789.width, st7789.height, nullptr);
PicoGraphics_PenRGB565 graphics(st7789.width, st7789.height, nullptr);

RGBLED led(PicoDisplay::LED_R, PicoDisplay::LED_G, PicoDisplay::LED_B);
Button button_a(PicoDisplay::A);
Button button_b(PicoDisplay::B);
Button button_x(PicoDisplay::X);
Button button_y(PicoDisplay::Y);

int16_t pol_mtx[POL_ROW][POL_COL] = {0};

typedef enum {
  Em_run_mode_rang = 0,
  Em_run_mode_tran = 1,
  Em_run_mode_infr = 2,
} EM_RUN_MODE;

SEFR sefr;


void LightLED(uint8_t label, uint8_t bright)
{
  RGB ledrgb;
  switch (label) {
    case 0:
    {
      // No object(WHITE)
      ledrgb = RGB(bright, bright, bright);
      break;
    }
    case 1:
    {
      // object 1(RED)
      ledrgb = RGB(bright, 0, 0);
      break;
    }
    case 2:
    {
      // object 2(GREEN)
      ledrgb = RGB(0, bright, 0);
      break;
    }
    case 3:
    {
      // object 3(BLUE)
      ledrgb = RGB(0, 0, bright);
      break;
    }
  }
  led.set_rgb(ledrgb.r, ledrgb.g, ledrgb.b);
}


int main() {

  stdio_init_all();

  EM_RUN_MODE e_run_mode = Em_run_mode_rang;

  sefr.setup(FEATURES, LABELS);

  st7789.set_backlight(255);
  Pen BG = graphics.create_pen(0, 200, 200);
  led.set_rgb(0, 80, 80);
  graphics.set_pen(BG);
  graphics.clear();
  st7789.update(&graphics);

  I2C i2c(4, 5);
  VL53L5CX vl53l5cx(&i2c, (uint8_t *)&vl53l5cx_firmware_bin);

  bool result = vl53l5cx.init();
  if(!result) {
      printf("Error initializing...\n");
  }
  vl53l5cx.set_ranging_mode(VL53L5CX::RANGING_MODE_CONTINUOUS);
  vl53l5cx.set_resolution(VL53L5CX::RESOLUTION_8X8);
  vl53l5cx.set_ranging_frequency_hz(15);
  vl53l5cx.start_ranging();

  Point vcenter_s1(67, 67 - 10);
  Point vcenter_e1(67, 67 - 5);
  Point vcenter_s2(67, 67 + 5);
  Point vcenter_e2(67, 67 + 10);
  Point hcenter_s1(67 - 10, 67);
  Point hcenter_e1(67 - 5, 67);
  Point hcenter_s2(67 + 5, 67);
  Point hcenter_e2(67 + 10, 67);

  Point text_location(138, 2);
  Point text_location2(138, 42);
  Point text_location3(138, 62);
  int loop_cnt = 0;
  int train_cnt_sample = 0;
  int train_cnt_label = 0;
  bool isdemo = false;

  float** X_train = new float*[DATASET_MAXSIZE];
  uint8_t* Y_train = new uint8_t[DATASET_MAXSIZE];
  float* infer_target = new float[FEATURES];
  char msg[256];
  for (int j = 0; j < DATASET_MAXSIZE; j++) {
    X_train[j] = nullptr;
  }

printf("\033[2J");

  while(true) {
    if(button_a.raw()) {
      e_run_mode = Em_run_mode_tran;
      train_cnt_sample = 0;
      train_cnt_label = 0;
      led.set_rgb(0, 0, 0);
      for (int j = 0; j < DATASET_MAXSIZE; j++) {
        if (X_train[j]) delete[] X_train[j];
      }
    }
    if(button_b.raw()) {
      e_run_mode = Em_run_mode_rang;
      led.set_rgb(0, 80, 80);
    }
    if(button_x.raw()) {
      isdemo = true;
      BG = graphics.create_pen(255, 165, 0);
      led.set_rgb(255/2, 165/2, 0);
    }
    if(button_y.raw()) {

    }

    if(vl53l5cx.data_ready()) {
      VL53L5CX::ResultsData result;
      if(vl53l5cx.get_data(&result)) {

        graphics.set_pen(BG);
        graphics.clear();

        for (int cntx = 0; cntx < POL_ROW; cntx++) {
          for (int cnty = 0; cnty < POL_COL; cnty++) {
            if (cntx % 2) {
              if (cnty % 2) {
                pol_mtx[cntx][cnty] = 
                  ( pol_mtx[cntx - 1][cnty    ] +
                    pol_mtx[cntx    ][cnty - 1] +
                    result.distance_mm[((cntx/2 + 1) * 8) + 7 - cnty/2] ) / 3;
              }
              else {
                pol_mtx[cntx][cnty] = 
                  ( result.distance_mm[((cntx/2    ) * 8) + 7 - cnty/2] +
                    result.distance_mm[((cntx/2 + 1) * 8) + 7 - cnty/2] ) / 2;
              }
            }
            else {
              if (cnty % 2) {
                pol_mtx[cntx][cnty] = 
                  ( result.distance_mm[(cntx/2 * 8) + 7 - (cnty/2    )] +
                    result.distance_mm[(cntx/2 * 8) + 7 - (cnty/2 + 1)] ) / 2;
              }
              else{
                pol_mtx[cntx][cnty] = result.distance_mm[(cntx/2 * 8) + 7 - cnty/2];
              }
            }
          }
        }

        for (int cntx = 0; cntx < POL_ROW; cntx++) {
          for (int cnty = 0; cnty < POL_COL; cnty++) {
            uint8_t depth = 255 - (pol_mtx[cntx][cnty] < 256 ? pol_mtx[cntx][cnty] : 255);
            if (isdemo) {
              graphics.set_pen(graphics.create_pen(depth, depth*165/255, 0));
            }
            else {
              graphics.set_pen(graphics.create_pen(depth, depth, depth));
            }
            Rect pxrect(cnty * 9, cntx * 9, 9, 9);
            graphics.rectangle(pxrect);
          }
        }

        printf("\033[%d;%dH", 1, 1);
        printf("temp:%3d", result.silicon_temp_degc);
        for (int cntx = 0; cntx < 8; cntx++) {
          for (int cnty = 0; cnty < 8; cnty++) {
            printf("\033[%d;%dH", cntx + 2, cnty*7 + 1);
            printf("%4dmm", result.distance_mm[(cntx * 8) + cnty]);
          }
        }

        if (e_run_mode == Em_run_mode_infr) {
          // Inferrence mode
          graphics.set_pen(graphics.create_pen(255, 255, 255));
          graphics.set_font(&font14_outline);
          graphics.text("Infer", text_location, 240);

          for (int j = 0; j < FEATURES; j++) {
            infer_target[j] = float(result.distance_mm[j]);
          }
          // Infer
          uint8_t c = sefr.predict(infer_target);
          LightLED(c, 100);
          switch (c) {
            case 0:
              sprintf(msg, "NO OBJECT");
              break;
            default:
              sprintf(msg, "OBJECT %d", c);
              break;
          }
          if (isdemo) {
            graphics.set_font(&font14_outline);
            sprintf(msg, "ORANGE");
            led.set_rgb(255/2, 165/2, 0);
            Point text_demo(140, 53);

            graphics.text(msg, text_demo, 240);
          }
          else {
            graphics.set_font(&font8);
            graphics.text(msg, text_location3, 240);
          }
        }
        else if (e_run_mode == Em_run_mode_tran) {
          // Training mode
          graphics.set_pen(graphics.create_pen(255, 255, 255));
          graphics.set_font(&font14_outline);
          graphics.text("Train", text_location, 240);
          sprintf(msg, "LABEL: %d", train_cnt_label);
          graphics.set_font(&font8);
          graphics.text(msg, text_location2, 240);
          sprintf(msg, "COUNT: %2d",  SAMPLES - train_cnt_sample);
          graphics.set_font(&font8);
          graphics.text(msg, text_location3, 240);

          if (train_cnt_sample == 0) {
            st7789.update(&graphics);
            led.set_rgb(0, 0, 0);
            sleep_ms(2000);
          }

          LightLED(train_cnt_label, train_cnt_sample * 50);

          if (loop_cnt % 2 == 0) {
            X_train[train_cnt_label * SAMPLES + train_cnt_sample] = new float[FEATURES];
            for (int j = 0; j < FEATURES; j++) {
              X_train[train_cnt_label * SAMPLES + train_cnt_sample][j] = float(result.distance_mm[j]);
            }
            Y_train[train_cnt_label * SAMPLES + train_cnt_sample] = train_cnt_label;
            train_cnt_sample++;
            if (train_cnt_sample >= SAMPLES) {
              train_cnt_sample = 0;
              train_cnt_label++;
              if (train_cnt_label >= LABELS) {
                // Training data Completed
                train_cnt_sample = 0;
                train_cnt_label = 0;
                e_run_mode = Em_run_mode_infr;
                // Exec training
                sefr.fit(X_train, Y_train, DATASET_MAXSIZE);
              }
            }
          }
        }
        else {
          // Ranging mode
          graphics.set_pen(graphics.create_pen(255, 255, 255));
          graphics.set_font(&font14_outline);
          graphics.text("Ranging", text_location, 240);
          sprintf(msg, "TEMP:%3d", result.silicon_temp_degc);
          graphics.set_font(&font8);
          graphics.text(msg, text_location3, 240);
          sprintf(msg, "%4dmm", result.distance_mm[8*3+4]);
          graphics.set_font(&font8);
          graphics.text(msg, text_location2, 240, 2.0f, 0.0f, 1, true);
          uint8_t depth = 255 - (result.distance_mm[8*3+4] < 256 ? result.distance_mm[8*3+4] : 255);
          led.set_rgb(0, depth/2, depth/2);
        }

        graphics.set_pen(graphics.create_pen(255, 0, 0));
        graphics.thick_line(vcenter_s1, vcenter_e1, 2);
        graphics.thick_line(vcenter_s2, vcenter_e2, 2);
        graphics.thick_line(hcenter_s1, hcenter_e1, 2);
        graphics.thick_line(hcenter_s2, hcenter_e2, 2);

        st7789.update(&graphics);
        loop_cnt++;
      }
    }
    sleep_ms(20);
  }

  return 0;
}