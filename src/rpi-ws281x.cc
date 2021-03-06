#include <nan.h>

#include <v8.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>

extern "C" {
  #include "rpi_ws281x/ws2811.h"
}

using namespace v8;

#define DEFAULT_TARGET_FREQ     800000
#define DEFAULT_CH0_GPIO_PIN        18
#define DEFAULT_CH1_GPIO_PIN        13
#define DEFAULT_DMANUM          5

ws2811_t ledstring;
ws2811_channel_t
  channel0data,
  channel1data;
int activeChannel;


/**
 * exports.render(Uint32Array data) - sends the data to the LED-strip,
 *   if data is longer than the number of leds, remaining data will be ignored.
 *   Otherwise, data
 */
void render(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  if(info.Length() != 1) {
    Nan::ThrowTypeError("render(): missing argument.");
    return;
  }

  if(!node::Buffer::HasInstance(info[0])) {
    Nan::ThrowTypeError("render(): expected argument to be a Buffer.");
    return;
  }

  Local<Object> buffer = info[0]->ToObject();

  int numBytes = std::min((int)node::Buffer::Length(buffer),
      4 * ledstring.channel[activeChannel].count);

  uint32_t* data = (uint32_t*) node::Buffer::Data(buffer);
  memcpy(ledstring.channel[activeChannel].leds, data, numBytes);

  ws2811_wait(&ledstring);
  ws2811_render(&ledstring);

  info.GetReturnValue().SetUndefined();
}


/**
 * exports.init(Number ledCount [, Object config]) - setup the configuration and initialize the library.
 */
void init(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  activeChannel = 0;
  
  ledstring.freq    = DEFAULT_TARGET_FREQ;
  ledstring.dmanum  = DEFAULT_DMANUM;

  channel0data.gpionum = 0;
  channel0data.invert = 0;
  channel0data.count = 0;
  channel0data.brightness = 255;

  channel1data.gpionum = 0;
  channel1data.invert = 0;
  channel1data.count = 0;
  channel1data.brightness = 255;

  ledstring.channel[0] = channel0data;
  ledstring.channel[1] = channel1data;

  if(info.Length() < 1) {
    return Nan::ThrowTypeError("init(): expected at least 1 argument");
  }

  // first argument is a number
  if(!info[0]->IsNumber()) {
    return Nan::ThrowTypeError("init(): argument 0 is not a number");
  }

  // second (optional) an Object
  if(info.Length() >= 2 && info[1]->IsObject()) {
    Local<Object> config = info[1]->ToObject();

    Local<String>
        symFreq = Nan::New<String>("frequency").ToLocalChecked(),
        symDmaNum = Nan::New<String>("dmaNum").ToLocalChecked(),
        symPwmChannel = Nan::New<String>("pwmChannel").ToLocalChecked(),
		symGpioPin = Nan::New<String>("gpioPin").ToLocalChecked(),
        symInvert = Nan::New<String>("invert").ToLocalChecked(),
        symBrightness = Nan::New<String>("brightness").ToLocalChecked();

    if(config->HasOwnProperty(symFreq)) {
      ledstring.freq = config->Get(symFreq)->Uint32Value();
    }

    if(config->HasOwnProperty(symDmaNum)) {
      ledstring.dmanum = config->Get(symDmaNum)->Int32Value();
    }
	
    if(config->HasOwnProperty(symPwmChannel)) {
	  int pwmChannel = config->Get(symPwmChannel)->Int32Value();
	  if (pwmChannel == 1) {
		activeChannel = 1;
      } else if (pwmChannel != 0) {
		return Nan::ThrowTypeError("init(): invalid pwmChannel (has to be 0 or 1)");
	  }
    }
	
	if(config->HasOwnProperty(symGpioPin)) {
		ledstring.channel[activeChannel].gpionum = config->Get(symGpioPin)->Int32Value();
    } else {
		ledstring.channel[activeChannel].gpionum = DEFAULT_CH0_GPIO_PIN;
	}

	if(config->HasOwnProperty(symInvert)) {
      ledstring.channel[activeChannel].invert = config->Get(symInvert)->Int32Value();
    }

    if(config->HasOwnProperty(symBrightness)) {
      ledstring.channel[activeChannel].brightness = config->Get(symBrightness)->Int32Value();
    }
  }

  ledstring.channel[activeChannel].count = info[0]->Int32Value();       

  // FIXME: handle errors, throw JS-Exception
  int err = ws2811_init(&ledstring);

  if(err) {
      return Nan::ThrowError("init(): initialization failed. sorry – no idea why.");
  }
  info.GetReturnValue().SetUndefined();
}


/**
 * exports.setBrightness()
 */
void setBrightness(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  if(info.Length() != 1) {
      return Nan::ThrowError("setBrightness(): no value given");
  }

  // first argument is a number
  if(!info[0]->IsNumber()) {
    return Nan::ThrowTypeError("setBrightness(): argument 0 is not a number");
  }

  ledstring.channel[activeChannel].brightness = info[0]->Int32Value();

  info.GetReturnValue().SetUndefined();
}


/**
 * exports.reset() – blacks out the LED-strip and finalizes the library
 * (disable PWM, free DMA-pages etc).
 */
void reset(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  memset(ledstring.channel[activeChannel].leds, 0, sizeof(*ledstring.channel[activeChannel].leds) * ledstring.channel[activeChannel].count);

  ws2811_render(&ledstring);
  ws2811_wait(&ledstring);
  ws2811_fini(&ledstring);

  info.GetReturnValue().SetUndefined();
}


/**
 * initializes the module.
 */
void initialize(Handle<Object> exports) {
  NAN_EXPORT(exports, init);
  NAN_EXPORT(exports, reset);
  NAN_EXPORT(exports, render);
  NAN_EXPORT(exports, setBrightness);
}

NODE_MODULE(rpi_ws281x, initialize)

// vi: ts=2 sw=2 expandtab
