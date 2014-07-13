#include <RGBLed.h>
#include <Backlight.h>
#include <AdapterBoard.h>
#include <Arduino.h>
#include "usb_commands.h"

bool isUsb = false;
unsigned long lastPowerChange = 0;
unsigned long lastBacklightChange = 0;

AdapterBoard::AdapterBoard()
{
}

void AdapterBoard::init()
{
  //Initialize the led, set to 'standby'
  led.init(LED_R, LED_G, LED_B);
  led.set(STANDBY_COLOUR);

  //Setup backlight, restore previous brightness, but don't enable
  backlight.init(BACKLIGHT_PIN, SUPPLY_EN);
  backlight.off();

  //Initialize the switches on board
  initSwitches();
}

void AdapterBoard::poll()
{
  //Check the switches, execute action if necessary
  pollSwitches();

  //Handle any USB commands waiting
  handleUSB();
}

void AdapterBoard::initSwitches()
{
  pinMode(SW_ON, INPUT);
  pinMode(SW_UP, INPUT);
  pinMode(SW_DOWN, INPUT);

  prev_swOn1 = HIGH;
  prev_swUp1 = HIGH;
  prev_swDown1 = HIGH;
  prev_swOn2 = HIGH;
  prev_swUp2 = HIGH;
  prev_swDown2 = HIGH;
  switchDelay = millis();
}

void AdapterBoard::pollSwitches()
{
  if((millis() - switchDelay) < 10)
    return;
  switchDelay = millis();

  int swOn = digitalRead(SW_ON);
  int swUp = digitalRead(SW_UP);
  int swDown = digitalRead(SW_DOWN);

  if(swOn == LOW && prev_swOn1 == LOW && prev_swOn2 == HIGH)
    togglePower();

  if((millis()) - lastBacklightChange > 150)
  {
    lastBacklightChange = millis();
    if(swUp == LOW && prev_swUp1 == LOW && prev_swUp2 == HIGH)
      backlight.up();
    if(swDown == LOW && prev_swDown1 == LOW && prev_swDown2 == HIGH)
      backlight.down();
  }

  prev_swOn2 = prev_swOn1;
  prev_swUp2 = prev_swUp1;
  prev_swDown2 = prev_swDown1;

  prev_swOn1 = swOn;
  prev_swUp1 = swUp;
  prev_swDown1 = swDown;
}

void AdapterBoard::togglePower()
{
  if((millis() - lastPowerChange) < 300)
    return;
  lastPowerChange = millis();

  if(backlight.isOn())
  {
    led.set(STANDBY_COLOUR);
    backlight.set(backlight.get());
    backlight.off();
  }
  else
  {
    if(isUsb)
      led.set(ONUSB_COLOUR);
    else
      led.set(ON_COLOUR);

    backlight.setLast();
    backlight.on();
  }
}

char buf[EP_LEN];

void AdapterBoard::handleUSB()
{
  if(usb.isEnumerated())
  {
    isUsb = true;
    if(backlight.isOn())
      led.set(ONUSB_COLOUR);

    if(usb.hasData())
    {
      char resp[EP_LEN];
      bool unknown = false;
      bool needsAck = true;
      usb.read(buf, EP_LEN);

      //Command type specifier stored in byte 0
      switch(buf[0])
      {
        case CMD_BL_ON:
          led.set(ONUSB_COLOUR);
          backlight.on();
          break;
        case CMD_BL_OFF:
          led.set(STANDBY_COLOUR);
          backlight.off();
          break;
        case CMD_BL_LEVEL:
          backlight.silentSet(buf[1]);
          break;
        case CMD_BL_UP:
          backlight.up();
          break;
        case CMD_BL_DOWN:
          backlight.down();
          break;
        case CMD_BL_GET_STATE:
          needsAck = false;
          //Send state response
          resp[0] = CMD_RESP;
          resp[1] = CMD_BL_GET_STATE;
          resp[2] = backlight.isOn();
          resp[3] = backlight.get();
          while(!usb.canSend());
          usb.write(resp, EP_LEN);
          break;

        case CMD_RGB_SET:
          led.set(buf[1], buf[2], buf[3]);
          break;
        case CMD_RGB_GET:
          needsAck = false;
          //Send response
          resp[0] = CMD_RESP;
          resp[1] = CMD_RGB_GET;
          resp[2] = led.r;
          resp[2] = led.g;
          resp[3] = led.b;
          while(!usb.canSend());
          usb.write(resp, EP_LEN);
          break;

        default:
          unknown = true;
          break;
      }

      if(!unknown && needsAck)
      {
        //Send ACK back to PC
        resp[0] = CMD_ACK;
        resp[1] = buf[0];
        while(!usb.canSend());
        usb.write(resp, EP_LEN);
      }
    }
  }
  else
  {
    isUsb = false;
    if(backlight.isOn())
      led.set(ON_COLOUR);
  }
}
