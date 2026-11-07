// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace HuboNative {
enum class RadioResult { Ok, Driver, Storage };
// The driver may already have partially written registers; if apply or persist fails, the old config is restored.
template<class Apply,class Save,class Restore>
RadioResult setRadio(Apply apply,Save save,Restore restore) {
    if(!apply()){restore();return RadioResult::Driver;}
    if(!save())return restore() ? RadioResult::Storage : RadioResult::Driver;
    return RadioResult::Ok;
}
enum class GainResult { Ok, Invalid, Driver, Storage };
struct GainOps {
    bool (*apply)(bool,void*);
    bool (*read)(bool&,void*);
    bool (*save)(void*);
    void* context;
};
inline GainResult setRxGain(const char* value,uint8_t& preference,const GainOps& ops) {
    if(!value || (value[0]!='0' && value[0]!='1') || value[1]) return GainResult::Invalid;
    const bool wanted=value[0]=='1';bool previous=false;
    if(!ops.read(previous,ops.context)) return GainResult::Driver;
    const uint8_t old_preference=preference;
    if(wanted==previous && preference==uint8_t(wanted)) return GainResult::Ok;
    auto rollback=[&]() {bool actual=false;return ops.apply(previous,ops.context) && ops.read(actual,ops.context) && actual==previous;};
    bool actual=false;
    if(!ops.apply(wanted,ops.context) || !ops.read(actual,ops.context) || actual!=wanted) {
        (void)rollback();return GainResult::Driver;
    }
    preference=wanted;
    if(!ops.save(ops.context)) {
        preference=old_preference;return rollback() ? GainResult::Storage : GainResult::Driver;
    }
    return GainResult::Ok;
}
struct ScopedText {
    uint8_t kind=0,channel=0;
    uint32_t timestamp=0;
    const uint8_t* key=nullptr;
    const uint8_t* text=nullptr;
    size_t text_length=0;
};
inline bool validText(const uint8_t* text,size_t length) {
    for(size_t i=0;i<length;) {
        uint32_t cp=text[i++], minimum=0;unsigned trailing=0;
        if(!cp) return false;
        if(cp<128) continue;
        if(cp>=0xc2 && cp<=0xdf) {cp&=31;trailing=1;minimum=0x80;}
        else if(cp>=0xe0 && cp<=0xef) {cp&=15;trailing=2;minimum=0x800;}
        else if(cp>=0xf0 && cp<=0xf4) {cp&=7;trailing=3;minimum=0x10000;}
        else return false;
        if(trailing>length-i) return false;
        while(trailing--) {
            const uint8_t byte=text[i++];if((byte&0xc0)!=0x80) return false;
            cp=(cp<<6)|(byte&63);
        }
        if(cp<minimum || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) return false;
    }
    return true;
}
inline bool parseScopedText(const uint8_t* frame,size_t size,unsigned channels,ScopedText& out) {
    if(!frame || size<26 || size>176 || frame[0]!=54 || frame[1]!=2 || frame[2]>2 || frame[19]!=0 || frame[20]>=channels) return false;
    if(frame[2]==2) {unsigned any=0;for(unsigned i=3;i<19;++i) any|=frame[i];if(!any) return false;}
    if(!validText(frame+25,size-25)) return false;
    out.kind=frame[2];out.key=frame+3;out.channel=frame[20];out.text=frame+25;out.text_length=size-25;
    out.timestamp=uint32_t(frame[21])|(uint32_t(frame[22])<<8)|(uint32_t(frame[23])<<16)|(uint32_t(frame[24])<<24);
    return true;
}
inline bool sendScopedText(const ScopedText& text,uint8_t key[16],bool& unscoped,
                           bool (*send)(const ScopedText&,void*),void* context) {
    uint8_t previous[16];std::memcpy(previous,key,16);const bool old_unscoped=unscoped;
    std::memset(key,0,16);unscoped=text.kind==1;
    if(text.kind==2) std::memcpy(key,text.key,16);
    const bool result=send(text,context);
    // Done inside the single Mesh task and restored on failure as well; does not change the default scope or config storage.
    std::memcpy(key,previous,16);unscoped=old_unscoped;return result;
}
}
