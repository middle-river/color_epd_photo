// Library for 5.65 inch 7-color e-Paper Display Module.
// 2022-04-17  T. Nakagawa

#include <SPI.h>

class EPD {
public:
  static constexpr int WIDTH = 600;
  static constexpr int HEIGHT = 448;

  EPD(int pin_din, int pin_sck, int pin_cs, int pin_dc, int pin_rst, int pin_busy) : pin_cs_(pin_cs), pin_dc_(pin_dc), pin_rst_(pin_rst), pin_busy_(pin_busy) {
    SPI.begin(pin_sck, -1, pin_din);
    SPI.setDataMode(SPI_MODE0);
    SPI.setBitOrder(MSBFIRST);
    SPI.setFrequency(4000000);
    pinMode(pin_cs_, OUTPUT);
    pinMode(pin_dc_, OUTPUT);
    pinMode(pin_rst_, OUTPUT);
    pinMode(pin_busy_, INPUT_PULLUP);
    digitalWrite(pin_rst_, HIGH);
    digitalWrite(pin_cs_, HIGH);
  }

  void begin() {
    init();
    send_cmd(0x61);
    send_dat(0x02);
    send_dat(0x58);
    send_dat(0x01);
    send_dat(0xc0);
    send_cmd(0x10);
  }

  void transfer(uint8_t *data, int size) {
    send_dat(data, size);
  }

  void end() {
    send_cmd(0x04);
    busy(HIGH);
    send_cmd(0x12);
    busy(HIGH);
    send_cmd(0x02);
    busy(LOW);
    delay(200);
    // Sleep.
    send_cmd(0x07);
    send_dat(0xa5);
  }

private:
  int pin_cs_;
  int pin_dc_;
  int pin_rst_;
  int pin_busy_;

  void send_cmd(uint8_t cmd) {
    digitalWrite(pin_dc_, LOW);
    digitalWrite(pin_cs_, LOW);
    SPI.transfer(cmd);
    digitalWrite(pin_cs_, HIGH);
  }

  void send_dat(uint8_t dat) {
    digitalWrite(pin_dc_, HIGH);
    digitalWrite(pin_cs_, LOW);
    SPI.transfer(dat);
    digitalWrite(pin_cs_, HIGH);
  }

  void send_dat(uint8_t *dat, int size) {
    digitalWrite(pin_dc_, HIGH);
    digitalWrite(pin_cs_, LOW);
    SPI.transfer(dat, size);
    digitalWrite(pin_cs_, HIGH);
  }

  void busy(int wait) {
    while (digitalRead(pin_busy_) != wait) delay(1);
  }

  void reset() {
    digitalWrite(pin_rst_, LOW);
    delay(1);
    digitalWrite(pin_rst_, HIGH);
    delay(200);
  }

  void init() {
    reset();
    busy(HIGH);
    send_cmd(0x00);
    send_dat(0xef);
    send_dat(0x08);
    send_cmd(0x01);
    send_dat(0x37);
    send_dat(0x00);
    send_dat(0x23);
    send_dat(0x23);
    send_cmd(0x03);
    send_dat(0x00);
    send_cmd(0x06);
    send_dat(0xc7);
    send_dat(0xc7);
    send_dat(0x1d);
    send_cmd(0x30);
    send_dat(0x3c);
    send_cmd(0x41);
    send_dat(0x00);
    send_cmd(0x50);
    send_dat(0x37);
    send_cmd(0x60);
    send_dat(0x22);
    send_cmd(0x61);
    send_dat(0x02);
    send_dat(0x58);
    send_dat(0x01);
    send_dat(0xc0);
    send_cmd(0xe3);
    send_dat(0xaa);
    delay(100);
    send_cmd(0x50);
    send_dat(0x37);
  }
};
