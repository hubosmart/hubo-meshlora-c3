// SPDX-License-Identifier: MIT
#pragma once
#ifndef HUBO_PERSISTENCE_STUBS_H
#define HUBO_PERSISTENCE_STUBS_H
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
using std::min;
using std::max;
class Stream {
public:
  virtual ~Stream() = default;
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t*, size_t) = 0;
  virtual void flush() = 0;
  size_t print(const char* s) { return write(reinterpret_cast<const uint8_t*>(s), strlen(s)); }
  size_t print(char c) { return write(static_cast<uint8_t>(c)); }
  size_t print(int v, int = 10) { return number("%d", v); }
  size_t print(unsigned v, int = 10) { return number("%u", v); }
  size_t print(long v, int = 10) { return number("%ld", v); }
  size_t print(unsigned long v, int = 10) { return number("%lu", v); }
  size_t print(double v, int p = 2) { char b[80]; snprintf(b,sizeof(b),"%.*f",p,v);return print(b); }
private:
  template<class T> size_t number(const char* f, T v) { char b[40];snprintf(b,sizeof(b),f,v);return print(b); }
};
#define PUB_KEY_SIZE 32
#define MAX_PATH_SIZE 64
#define ADV_TYPE_NONE 0
#define ADV_TYPE_CHAT 1
#define MESH_DEBUG_PRINTLN(...) ((void)0)
extern unsigned logged_failures;
#define ESP_LOGE(...) (++logged_failures)
namespace mesh {
class RTCClock { public: uint32_t value=123; uint32_t getCurrentTime() {return value;} };
class Identity {
public:
  uint8_t pub_key[32]{};
  Identity() = default;
  Identity(const uint8_t* key) {memcpy(pub_key,key,32);}
  bool matches(const Identity& id) const {return !memcmp(pub_key,id.pub_key,32);}
};
class LocalIdentity: public Identity { public: void calcSharedSecret(uint8_t*, const uint8_t*) const {} };
struct GroupChannel { uint8_t secret[32]{}, hash[8]{}; };
struct Utils {
  static void toHex(char* out,const uint8_t* in,int n) {for(int i=0;i<n;++i)sprintf(out+2*i,"%02X",in[i]);out[2*n]=0;}
  static void fromHex(uint8_t* out,size_t n,const char* in) {for(size_t i=0;i<n && in[i*2] && in[i*2+1];++i){unsigned v=0;sscanf(in+2*i,"%2x",&v);out[i]=v;}}
  static void printHex(Stream& stream,const uint8_t* bytes,size_t n) {for(size_t i=0;i<n;++i){char b[3];sprintf(b,"%02X",bytes[i]);stream.print(b);}}
};
}
using esp_err_t = int;
#define ESP_OK 0
inline int esp_spiffs_info(const char*,size_t*,size_t*) {return -1;}
inline int esp_spiffs_format(const char*) {return -1;}
inline int nvs_flash_erase() {return -1;}
template<class T,class L,class H> T constrain(T v,L lo,H hi) {return v<lo?lo:v>hi?hi:v;}
#endif
