// SPDX-License-Identifier: MIT
#include "persistence_stubs.h"
#include "DataStore.h"
#include "HuboNativeExtensions.h"
#include <cassert>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <type_traits>
#include <sys/stat.h>
#include <unistd.h>
#ifdef _WIN32
#include <direct.h>
#endif

static std::string fault;
unsigned logged_failures=0;
static bool new_file(FILE* file);
static std::vector<FILE*> pending_files;
static FILE* injected_open(const char* path, const char* mode) {
  const bool pending = std::string(path).find(".new") != std::string::npos && mode[0]=='w';
  if (pending && fault=="open") {errno=EACCES;return nullptr;}
  FILE* f=fopen(path,mode);
  if (f && pending) pending_files.push_back(f);
  return f;
}
static bool new_file(FILE* file) {return std::find(pending_files.begin(),pending_files.end(),file)!=pending_files.end();}
static size_t injected_write(const void* bytes,size_t size,size_t count,FILE* file) {
  if (new_file(file) && fault=="write") return fwrite(bytes,size,count?count-1:0,file);
  return fwrite(bytes,size,count,file);
}
static int injected_close(FILE* file) {
  const bool fail=new_file(file) && fault=="close";
  pending_files.erase(std::remove(pending_files.begin(),pending_files.end(),file),pending_files.end());
  const int result=fclose(file);
  return fail ? EOF : result;
}
static int injected_rename(const char* from,const char* to) {
  if (fault=="commit" && std::string(from).find(".new")!=std::string::npos) {errno=EACCES;return -1;}
  return rename(from,to);
}
static int host_mkdir(const char* path,int) {
#ifdef _WIN32
  return _mkdir(path);
#else
  return mkdir(path,0700);
#endif
}
#define fopen injected_open
#define fwrite injected_write
#define fclose injected_close
#include "native_fs.inc"
#undef fopen
#undef fwrite
#undef fclose

bool IdentityStore::save(const char*,const mesh::LocalIdentity&) {return false;}
bool IdentityStore::load(const char*,mesh::LocalIdentity&) {return false;}

struct Host : DataStoreHost {
  std::vector<ContactInfo> contacts;
  std::vector<ChannelDetails> channels;
  bool onContactLoaded(const ContactInfo& c) override {contacts.push_back(c);return true;}
  bool getContactForSave(uint32_t i,ContactInfo& c) override {if(i>=contacts.size())return false;c=contacts[i];return true;}
  bool onChannelLoaded(uint8_t i,const ChannelDetails& c) override {if(i>=channels.size())channels.resize(i+1);channels[i]=c;return true;}
  bool getChannelForSave(uint8_t i,ChannelDetails& c) override {if(i>=channels.size())return false;c=channels[i];return true;}
};
template<class F> bool checked(F fn) {
  if constexpr(std::is_void_v<decltype(fn())>) {fn();return false;}
  else return fn();
}
static ContactInfo contact(unsigned key,const char* name) {
  ContactInfo c{};c.id.pub_key[0]=key;c.type=ADV_TYPE_CHAT;c.flags=1;c.out_path_len=2;
  strcpy(c.name,name);c.out_path[0]=42;c.lastmod=111;c.sync_since=23;return c;
}
static ChannelDetails channel(const char* name) {
  ChannelDetails c{};strcpy(c.name,name);c.channel.secret[0]=9;return c;
}
static std::string bytes(const std::string& path) {
  std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};
}
struct Capture {std::vector<std::vector<uint8_t>> frames;bool isConnected(){return true;}void writeFrame(const uint8_t* b,int n){frames.emplace_back(b,b+n);}};
struct SensorBoundary {
  double node_lat=11,node_lon=22;
  std::string gps="0",interval="60";
  bool setSettingValue(const char* n,const char* v) {if(!strcmp(n,"gps"))gps=v;else if(!strcmp(n,"gps_interval"))interval=v;return true;}
  const char* getSettingValue(int i) {return i==0?gps.c_str():interval.c_str();}
  const char* getSettingName(int i) {return i==0?"gps":"gps_interval";}
  int getNumSettings() {return 2;}
} sensors;
struct RadioSettings {float frequency_mhz,bandwidth_khz;uint8_t spreading_factor,coding_rate;int8_t chip_power_dbm;};
struct DriverBoundary {
  RadioSettings settings{433,125,7,5,1};bool gain=false,restore_ok=true;
  bool applySettings(RadioSettings s) {settings=s;return restore_ok;}
  bool setRxBoostedGainMode(bool v) {gain=v;return true;}
  bool getRxBoostedGainModeChecked(bool& v) {v=gain;return true;}
} radio_driver;
static bool hubo_radio_validate(RadioSettings) {return true;}
struct BoardBoundary {bool rebooted=false;void reboot(){rebooted=true;}} board;
struct StrHelper {
  static void strncpy(char* out,const char* in,size_t n) {::strncpy(out,in,n-1);out[n-1]=0;}
};
#define MAX_FRAME_SIZE 255
#define MAX_LORA_TX_POWER 10
#define MAX_CONTACTS 3
#define MAX_ANON_CONTACTS 8
#define MAX_GROUP_CHANNELS 2
#define ENV_INCLUDE_GPS 1

class MyMesh : public Host {
public:
  NodePrefs _prefs;
  DataStore* _store;
  Capture capture;Capture* _serial=&capture;
  unsigned long dirty_contacts_expiry=0;
  uint8_t cmd_frame[MAX_FRAME_SIZE+1]{},out_frame[MAX_FRAME_SIZE+1]{};
  mesh::RTCClock clock;
  explicit MyMesh(DataStore& store):_store(&store) {
    strcpy(_prefs.node_name,"old");_prefs.freq=433;_prefs.bw=125;_prefs.sf=7;_prefs.cr=5;_prefs.tx_power_dbm=1;
    _prefs.node_lat=sensors.node_lat;_prefs.node_lon=sensors.node_lon;
    _prefs.gps_interval=60;strcpy(_prefs.default_scope_name,"oldscope");_prefs.default_scope_key[0]=2;
    contacts={contact(1,"one"),contact(2,"two")};channels={channel("old"),channel("second")};
  }
  bool savePrefsChecked();
  void savePrefs() {(void)savePrefsChecked();}
  bool saveContacts();
  bool saveChannels() {return checked([&]{return _store->saveChannels(this);});}
  mesh::RTCClock* getRTCClock() {return &clock;}
  bool isValidClientRepeatFreq(uint32_t) const {return true;}
  unsigned long futureMillis(unsigned long n) {return n+100;}
  bool millisHasNowPassed(unsigned long) {return true;}
  int getNumContacts() const {return static_cast<int>(contacts.size());}
  int getTotalContactSlots() const {return static_cast<int>(contacts.size());}
  bool getContactByIdx(uint32_t i,ContactInfo& c) {return i>=MAX_ANON_CONTACTS && getContactForSave(i-MAX_ANON_CONTACTS,c);}
  bool shouldOverwriteWhenFull() const {return (_prefs.autoadd_config&1)!=0;}
  void onContactOverwrite(const uint8_t*);
  ContactInfo transient=contact(99,"temporary");
  ContactInfo* allocateContactSlot(bool) {return &transient;}
  ContactInfo* lookupContactByPubKey(const uint8_t* key,int n) {for(auto& c:contacts)if(!memcmp(c.id.pub_key,key,n))return &c;return nullptr;}
  bool addContact(const ContactInfo& c) {if(c.type==ADV_TYPE_NONE){transient=c;return true;}if(contacts.size()>=MAX_CONTACTS)return false;contacts.push_back(c);return true;}
  bool removeContact(ContactInfo& c) {const auto key=c.id.pub_key[0];for(auto i=contacts.begin();i!=contacts.end();++i)if(i->id.pub_key[0]==key){contacts.erase(i);return true;}return false;}
  bool getChannel(int i,ChannelDetails& c) {if(i<0 || size_t(i)>=channels.size())return false;c=channels[i];return true;}
  bool setChannel(int i,const ChannelDetails& c) {if(i<0 || size_t(i)>=channels.size())return false;channels[i]=c;return true;}
  void handleCmdFrame(size_t);
  void retryContacts();
  void updateContactFromFrame(ContactInfo&,uint32_t&,const uint8_t*,int);
  void writeOKFrame();void writeErrFrame(uint8_t);
  void request(std::vector<uint8_t> frame) {capture.frames.clear();memset(cmd_frame,0,sizeof(cmd_frame));memcpy(cmd_frame,frame.data(),frame.size());handleCmdFrame(frame.size());}
};
#include "commands.inc"

class MemoryStream : public Stream {
public:
  std::string result;
  int available() override{return 0;}int read() override{return -1;}int peek() override{return -1;}
  size_t write(uint8_t b) override{result.push_back(b);return 1;}
  size_t write(const uint8_t* b,size_t n) override{result.append(reinterpret_cast<const char*>(b),n);return n;}
  void flush() override{}
};
static std::string prefsBytes(NodePrefs& prefs) {MemoryStream stream;assert(prefs.saveSerial(stream));return stream.result;}
static void error(const MyMesh& mesh) {assert(mesh.capture.frames.size()==1);assert(mesh.capture.frames[0]==std::vector<uint8_t>({1,5}));}
static void ok(const MyMesh& mesh) {assert(mesh.capture.frames.size()==1);assert(mesh.capture.frames[0]==std::vector<uint8_t>({0}));}
static std::vector<uint8_t> contactFrame(unsigned key) {
  std::vector<uint8_t> f(148);f[0]=9;f[1]=key;f[33]=ADV_TYPE_CHAT;f[34]=0;f[35]=1;strcpy(reinterpret_cast<char*>(f.data()+100),"new");return f;
}
static void commands(DataStore& store,const std::string& root,const std::string& name) {
  store.begin();
  MyMesh mesh(store);mesh.contacts.reserve(16);assert(store.savePrefs(mesh._prefs));
  checked([&]{return store.saveContacts(&mesh);});checked([&]{return store.saveChannels(&mesh);});
  if(name=="prefs-commands" || name=="radio-commands") {
    std::vector<std::vector<uint8_t>> frames;
    if(name=="prefs-commands") {
      frames={{8,'n','e','w'},{14,0,0,0,0,0,0,0,0},{21,1,0,0,0,2,0,0,0},{38,1,21,1,2},{61,0,1},{37,0,0,0,0},{58,15,64},{63}};
      std::vector<uint8_t> scope(48);scope[0]=63;strcpy(reinterpret_cast<char*>(scope.data()+1),"newscope");scope[32]=99;frames.push_back(scope);
      frames.push_back({41,'g','p','s',':','1'});frames.push_back({41,'g','p','s','_','i','n','t','e','r','v','a','l',':','9','0'});
    } else frames={{12,0},{11,0x68,0x9b,6,0,0x48,0xe8,1,0,9,6,1},{41,'r','a','d','i','o','.','r','x','g','a','i','n',':','1'}};
    for(const auto& f:frames) for(const char* mode:{"open","write","close","commit"}) {
      const auto before=prefsBytes(mesh._prefs),disk=bytes(root+"/prefs.json");
      const auto lat=sensors.node_lat,lon=sensors.node_lon;const auto gps=sensors.gps,interval=sensors.interval;
      const auto driver=radio_driver.settings;const auto gain=radio_driver.gain;
      fault=mode;mesh.request(f);fault.clear();error(mesh);
      assert(prefsBytes(mesh._prefs)==before);assert(bytes(root+"/prefs.json")==disk);
      assert(sensors.node_lat==lat && sensors.node_lon==lon && sensors.gps==gps && sensors.interval==interval);
      assert(radio_driver.settings.frequency_mhz==driver.frequency_mhz && radio_driver.settings.bandwidth_khz==driver.bandwidth_khz);
      assert(radio_driver.settings.spreading_factor==driver.spreading_factor && radio_driver.settings.coding_rate==driver.coding_rate);
      assert(radio_driver.settings.chip_power_dbm==driver.chip_power_dbm && radio_driver.gain==gain);
    }
  } else if(name=="channel-command") {
    std::vector<uint8_t> f(50);f[0]=32;strcpy(reinterpret_cast<char*>(f.data()+2),"new");f[34]=10;
    for(const char* mode:{"open","write","close","commit"}) {
      const auto before=mesh.channels[0];const auto disk=bytes(root+"/channels2");fault=mode;mesh.request(f);fault.clear();error(mesh);
      assert(!memcmp(&before,&mesh.channels[0],sizeof(before)));assert(bytes(root+"/channels2")==disk);
    }
  } else if(name=="contact-commands") {
    uint8_t key[32]={1},blob[2]={42,43};assert(store.putBlobByKey(key,32,blob,2));
    const auto advert=bytes(root+"/bl/0100000000000000");
    std::vector<uint8_t> remove(33);remove[0]=15;remove[1]=1;
    std::vector<uint8_t> reset(33);reset[0]=13;reset[1]=1;
    for(const auto& f:{remove,reset,contactFrame(1),contactFrame(3)}) for(const char* mode:{"open","write","close","commit"}) {
      const auto before=mesh.contacts;const auto disk=bytes(root+"/contacts3");fault=mode;mesh.request(f);fault.clear();error(mesh);
      assert(mesh.contacts.size()==before.size());assert(!memcmp(mesh.contacts.data(),before.data(),before.size()*sizeof(ContactInfo)));
      assert(bytes(root+"/contacts3")==disk);
      assert(bytes(root+"/bl/0100000000000000")==advert);
    }
  } else if(name=="command-cold-read") {
    mesh.request({8,'n','e','w'});ok(mesh);mesh.request(contactFrame(1));ok(mesh);
    std::vector<uint8_t> f(50);f[0]=32;strcpy(reinterpret_cast<char*>(f.data()+2),"new");f[34]=10;mesh.request(f);ok(mesh);
    Host cold;NodePrefs prefs;store.loadPrefs(prefs);store.loadContacts(&cold);store.loadChannels(&cold);
    assert(!strcmp(prefs.node_name,"new") && !strcmp(cold.contacts[0].name,"new") && !strcmp(cold.channels[0].name,"new"));
  } else if(name=="contact-full-commands") {
    uint8_t key[32]={1},blob[2]={42,43};assert(store.putBlobByKey(key,32,blob,2));
    const auto advert=bytes(root+"/bl/0100000000000000");
    mesh.contacts.push_back(contact(3,"three"));mesh.contacts[0].flags=0;mesh._prefs.autoadd_config=1;
    assert(store.saveContacts(&mesh));const auto before=mesh.contacts;const auto disk=bytes(root+"/contacts3");
    for(const char* mode:{"open","write","close","commit"}) {
      fault=mode;mesh.request(contactFrame(4));fault.clear();error(mesh);
      assert(mesh.contacts.size()==before.size());assert(!memcmp(mesh.contacts.data(),before.data(),before.size()*sizeof(ContactInfo)));
      assert(bytes(root+"/contacts3")==disk && bytes(root+"/bl/0100000000000000")==advert);
    }
    mesh.request(contactFrame(4));assert(mesh.contacts[0].id.pub_key[0]==4);
    assert(mesh.capture.frames.size()==2 && mesh.capture.frames[0][0]==0x8f && mesh.capture.frames[1]==std::vector<uint8_t>({0}));
    assert(!std::filesystem::exists(root+"/bl/0100000000000000"));
    assert(mesh.contacts[0].sync_since==0 && !mesh.contacts[0].shared_secret_valid);
  } else if(name=="background-commands") {
    mesh.dirty_contacts_expiry=1;const auto disk=bytes(root+"/contacts3");fault="open";mesh.retryContacts();fault.clear();
    assert(mesh.dirty_contacts_expiry>1 && logged_failures==1 && bytes(root+"/contacts3")==disk && mesh.capture.frames.empty());
    mesh.retryContacts();assert(mesh.dirty_contacts_expiry==0 && mesh.capture.frames.empty());
  } else if(name=="reboot-commands") {
    mesh.dirty_contacts_expiry=1;fault="open";mesh.request({19,'r','e','b','o','o','t'});fault.clear();error(mesh);
    assert(!board.rebooted && mesh.dirty_contacts_expiry==1);mesh.request({19,'r','e','b','o','o','t'});assert(board.rebooted);
  } else if(name=="transient-commands") {
    mesh.contacts.push_back(contact(3,"three"));mesh.contacts[0].flags=0;mesh._prefs.autoadd_config=1;
    auto f=contactFrame(4);f[33]=ADV_TYPE_NONE;const auto previous=mesh.transient;
    fault="open";mesh.request(f);fault.clear();error(mesh);assert(!memcmp(&mesh.transient,&previous,sizeof(previous)));
  }
}
int main(int argc,char** argv) {
  assert(argc==3);const std::string name=argv[1],root=argv[2];fs::FS fs(root.c_str());mesh::RTCClock clock;DataStore store(fs,clock);
  if(name.find("commands")!=std::string::npos || name=="channel-command" || name=="command-cold-read") {commands(store,root,name);return 0;}
  Host host;host.contacts={contact(1,"old")};host.channels={channel("old")};NodePrefs prefs;strcpy(prefs.node_name,"old");
  if(name=="cold-read") {
    assert(checked([&]{return store.saveContacts(&host);}));assert(checked([&]{return store.saveChannels(&host);}));assert(store.savePrefs(prefs));
    fs::FS cold_fs(root.c_str());DataStore cold_store(cold_fs,clock);Host cold;NodePrefs cold_prefs;
    cold_store.loadContacts(&cold);cold_store.loadChannels(&cold);cold_store.loadPrefs(cold_prefs);
    assert(cold.contacts.size()==1 && !strcmp(cold.contacts[0].name,"old") && cold.contacts[0].out_path[0]==42);
    assert(cold.channels.size()==1 && cold.channels[0].channel.secret[0]==9 && !strcmp(cold_prefs.node_name,"old"));return 0;
  }
  const bool prefs_case=name=="prefs-failures",contacts=name.find("contacts-")==0;
  const std::string path=prefs_case?"/prefs.json":contacts?"/contacts3":"/channels2";
  const auto save=[&] {if(prefs_case)return store.savePrefs(prefs);if(contacts)return checked([&]{return store.saveContacts(&host);});return checked([&]{return store.saveChannels(&host);});};
  save();const auto old=bytes(root+path);assert(!old.empty());
  strcpy(host.contacts[0].name,"new");strcpy(host.channels[0].name,"new");strcpy(prefs.node_name,"new");
  std::vector<std::string> modes=prefs_case?std::vector<std::string>{"open","write","close","commit"}:std::vector<std::string>{name.substr(name.find('-')+1)};
  for(const auto& mode:modes) {
    fault=mode;const bool result=save();fault.clear();assert(!result);assert(bytes(root+path)==old);
    Host cold;NodePrefs cold_prefs;DataStore cold_store(fs,clock);
    if(prefs_case){cold_store.loadPrefs(cold_prefs);assert(!strcmp(cold_prefs.node_name,"old"));}
    else if(contacts){cold_store.loadContacts(&cold);assert(cold.contacts.size()==1 && !strcmp(cold.contacts[0].name,"old"));}
    else{cold_store.loadChannels(&cold);assert(cold.channels.size()==1 && !strcmp(cold.channels[0].name,"old"));}
  }
}
