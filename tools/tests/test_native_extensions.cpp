// SPDX-License-Identifier: MIT
#include "HuboNativeExtensions.h"
#include <cassert>
#include <cstring>
#include <vector>

struct Gain {
    bool actual=true, apply_ok=true, save_ok=true, read_ok=true, restore_fails=false;
    unsigned saves=0, applies=0;
    static bool apply(bool value,void* p) {auto& g=*static_cast<Gain*>(p);if(++g.applies>1 && g.restore_fails)return false;g.actual=value;return g.apply_ok;}
    static bool read(bool& value,void* p) {auto& g=*static_cast<Gain*>(p);value=g.actual;return g.read_ok;}
    static bool save(void* p) {auto& g=*static_cast<Gain*>(p);++g.saves;return g.save_ok;}
};
struct Sender {
    uint8_t* key; bool* unscoped; bool succeed=true; unsigned calls=0;
    static bool send(const HuboNative::ScopedText& request,void* context) {
        auto& s=*static_cast<Sender*>(context);++s.calls;
        assert(request.text_length==6 && !std::memcmp(request.text,"你好",6));
        assert(*s.unscoped==(request.kind==1));
        assert(s.key[0]==(request.kind==2 ? 7 : 0));return s.succeed;
    }
};
int main() {
    int radio=1,pref=1;bool saving=true,restore_ok=true;
    auto apply=[&](){radio=pref=2;return true;};
    auto save=[&](){return saving;};
    auto restore=[&](){pref=1;if(restore_ok)radio=1;return restore_ok;};
    assert(HuboNative::setRadio(apply,save,restore)==HuboNative::RadioResult::Ok && radio==2 && pref==2);
    saving=false;
    assert(HuboNative::setRadio(apply,save,restore)==HuboNative::RadioResult::Storage && radio==1 && pref==1);
    restore_ok=false;
    assert(HuboNative::setRadio(apply,save,restore)==HuboNative::RadioResult::Driver && radio==2 && pref==1);
    restore_ok=true;
    assert(HuboNative::setRadio([&](){radio=3;return false;},save,restore)==HuboNative::RadioResult::Driver && radio==1);
    uint8_t preference=1;Gain gain;
    HuboNative::GainOps ops{Gain::apply,Gain::read,Gain::save,&gain};
    assert(HuboNative::setRxGain("0",preference,ops)==HuboNative::GainResult::Ok);
    assert(!preference && !gain.actual && gain.saves==1);
    gain.save_ok=false;
    assert(HuboNative::setRxGain("1",preference,ops)==HuboNative::GainResult::Storage);
    assert(!preference && !gain.actual);
    gain.apply_ok=false;
    assert(HuboNative::setRxGain("1",preference,ops)==HuboNative::GainResult::Driver);
    assert(!preference && !gain.actual);
    assert(HuboNative::setRxGain("1x",preference,ops)==HuboNative::GainResult::Invalid);
    assert(HuboNative::setRxGain("-1",preference,ops)==HuboNative::GainResult::Invalid);
    gain=Gain{};gain.actual=false;gain.read_ok=false;preference=0;
    assert(HuboNative::setRxGain("0",preference,ops)==HuboNative::GainResult::Driver);
    gain=Gain{};gain.actual=false;gain.save_ok=false;gain.restore_fails=true;preference=0;
    assert(HuboNative::setRxGain("1",preference,ops)==HuboNative::GainResult::Driver);
    assert(gain.actual && !preference);
    std::vector<uint8_t> frame(31);frame[0]=54;frame[1]=2;frame[2]=2;frame[3]=7;frame[20]=0;
    std::memcpy(frame.data()+25,"你好",6);
    HuboNative::ScopedText text;
    assert(HuboNative::parseScopedText(frame.data(),frame.size(),40,text));
    uint8_t old[16];std::memset(old,9,sizeof(old));bool unscoped=true;
    Sender sender{old,&unscoped};
    assert(HuboNative::sendScopedText(text,old,unscoped,Sender::send,&sender));
    assert(old[0]==9 && unscoped && sender.calls==1);
    sender.succeed=false;
    assert(!HuboNative::sendScopedText(text,old,unscoped,Sender::send,&sender));
    assert(old[0]==9 && unscoped);
    for(uint8_t kind : {uint8_t(0),uint8_t(1)}) {
        frame[2]=kind;assert(HuboNative::parseScopedText(frame.data(),frame.size(),40,text));
        sender.succeed=true;assert(HuboNative::sendScopedText(text,old,unscoped,Sender::send,&sender));
        assert(unscoped);for(auto value:old)assert(value==9);
        sender.succeed=false;assert(!HuboNative::sendScopedText(text,old,unscoped,Sender::send,&sender));
        assert(unscoped);for(auto value:old)assert(value==9);
    }
    frame[25]=0;assert(!HuboNative::parseScopedText(frame.data(),frame.size(),40,text));
    frame[25]=0xc0;assert(!HuboNative::parseScopedText(frame.data(),frame.size(),40,text));
    assert(!HuboNative::parseScopedText(frame.data(),24,40,text));
    frame.resize(177);assert(!HuboNative::parseScopedText(frame.data(),frame.size(),40,text));
    frame.assign(176,'a');frame[0]=54;frame[1]=2;frame[2]=2;frame[19]=0;frame[20]=0;
    assert(HuboNative::parseScopedText(frame.data(),frame.size(),40,text) && text.text_length==151);
    frame[20]=40;assert(!HuboNative::parseScopedText(frame.data(),frame.size(),40,text));
}
