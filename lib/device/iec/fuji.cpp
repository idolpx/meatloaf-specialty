#ifdef BUILD_IEC

#include "fuji.h"
#include "../../../include/cbm_defines.h"

#include <driver/ledc.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <utility>

#include "string_utils.h"
#include "../../../include/debug.h"

#include "fnSystem.h"
#include "fnConfig.h"
#include "fsFlash.h"
#include "fnWiFi.h"
#include "network.h"
#include "led.h"
#include "siocpm.h"
#include "clock.h"
#include "utils.h"
#include "status_error_codes.h"

#define ADDITIONAL_DETAILS_BYTES 10
#define FF_DIR 0x01
#define FF_TRUNC 0x02

//iecFuji theFuji; // global fuji device object
// iecNetwork sioNetDevs[MAX_NETWORK_DEVICES];

bool _validate_host_slot(uint8_t slot, const char *dmsg = nullptr);
bool _validate_device_slot(uint8_t slot, const char *dmsg = nullptr);

bool _validate_host_slot(uint8_t slot, const char *dmsg)
{
    if (slot < MAX_HOSTS)
        return true;

    if (dmsg == NULL)
    {
        Debug_printf("!! Invalid host slot %hu\r\n", slot);
    }
    else
    {
        Debug_printf("!! Invalid host slot %hu @ %s\r\n", slot, dmsg);
    }

    return false;
}

bool _validate_device_slot(uint8_t slot, const char *dmsg)
{
    if (slot < MAX_DISK_DEVICES)
        return true;

    if (dmsg == NULL)
    {
        Debug_printf("!! Invalid device slot %hu\r\n", slot);
    }
    else
    {
        Debug_printf("!! Invalid device slot %hu @ %s\r\n", slot, dmsg);
    }

    return false;
}

static std::string dataToHexString(uint8_t *data, size_t len)
{
  std::string res;
  char buf[10];
  
  for(size_t i=0; i<len; i++)
    {
      sprintf(buf, "%02X ", data[i]);
      res += buf;
    }
  
  return res;
}


// Constructor
iecFuji::iecFuji() //: IECDevice()
{
    // Helpful for debugging
    for (int i = 0; i < MAX_HOSTS; i++)
        _fnHosts[i].slotid = i;

    state = DEVICE_IDLE;
}

// Initializes base settings and adds our devices to the SIO bus
void iecFuji::setup(systemBus *bus)
{
    //Debug_printf("iecFuji::setup()\r\n");

    _populate_slots_from_config();

    FileSystem *ptrfs = fnSDFAT.running() ? (FileSystem *)&fnSDFAT : (FileSystem *)&fsFlash;
//    iecPrinter::printer_type ptype = Config.get_printer_type(0);
    iecPrinter::printer_type ptype = iecPrinter::printer_type::PRINTER_COMMODORE_MPS803; // temporary
    Debug_printf("Creating a default printer using %s storage and type %d\r\n", ptrfs->typestring(), ptype);
    iecPrinter *ptr = new iecPrinter(4, ptrfs, ptype);
    fnPrinters.set_entry(0, ptr, ptype, Config.get_printer_port(0));

    // 04-07 Printers / Plotters
    if (bus->attachDevice(ptr))
        Debug_printf("Attached printer device #%d\r\n", 4);

    // 08-15 Drives
    for (int i = 0; i < MAX_DISK_DEVICES; i++)
    {
        _fnDisks[i].disk_dev.setDeviceNumber(BUS_DEVICEID_DISK+i);
        if (bus->attachDevice(&_fnDisks[i].disk_dev))
        {
            bool enabled = Config.get_device_slot_enable(i+1);
            Debug_printf("Attached drive device #%d %s\r\n", BUS_DEVICEID_DISK+i, (enabled ? "":"[disabled]"));
            _fnDisks[i].disk_dev.setActive(enabled);
        }
    }

    // 16-19 Network Devices
    if (bus->attachDevice(new iecNetwork(16)))     // 16-19 Network Devices
        Debug_printf("Attached network device #%d\r\n", 16);

    //Serial.print("CPM "); bus->addDevice(new iecCpm(), 20);             // 20-29 Other
    if (bus->attachDevice(new iecClock(29)))
        Debug_printf("Attached clock device #%d\r\n", 29);

    // // FujiNet
    // setDeviceNumber(30); 
    // if (bus->attachDevice(this))
    //     Debug_printf("Attached Meatloaf device #%d\r\n", 30);
}

void logResponse(const void* data, size_t length)
{
    // ensure we don't flood the logs with debug, and make it look pretty using util_hexdump
    uint8_t debug_len = length;
    bool is_truncated = false;
    if (debug_len > 64) {
        debug_len = 64;
        is_truncated = true;
    }

    std::string msg = util_hexdump(data, debug_len);
    Debug_printf("Sending:\r\n%s\r\n", msg.c_str());
    if (is_truncated) {
        Debug_printf("[truncated from %d]\r\n", length);
    }

    // Debug_printf("  ");
    // // ASCII Text representation
    // for (int i=0;i<length;i++)
    // {
    //     char c = petscii2ascii(data[i]);
    //     Debug_printf("%c", c<0x20 || c>0x7f ? '.' : c);
    // }

}

#if 0
void iecFuji::talk(uint8_t secondary)
{
  // only talk on channel 15
  if( (secondary & 0x0F)==15 )
    {
      state = DEVICE_TALK;
      responsePtr = 0;
    }
}


void iecFuji::listen(uint8_t secondary)
{
  // only listen on channel 15
  if( (secondary & 0x0F)==15 )
    {
      state = DEVICE_LISTEN;
      payload.clear();
    }
}


void iecFuji::untalk()
{
  state = DEVICE_IDLE;
}


void iecFuji::unlisten()
{
  if( state == DEVICE_LISTEN )
    state = DEVICE_ACTIVE;
}


int8_t iecFuji::canWrite()
{
  return state==DEVICE_LISTEN ? 1 : 0;
}


int8_t iecFuji::canRead()
{
  if( state == DEVICE_TALK )
    return std::min((size_t) 2, responseV.size()-responsePtr);
  else
    return 0;
}


void iecFuji::write(uint8_t data, bool eoi)
{
  payload += char(data);
}


uint8_t iecFuji::read()
{
  // we should never get here if responsePtr>=responseV.size() because
  // then canRead would have returned 0, but better safe than sorry
  return responsePtr < responseV.size() ? responseV[responsePtr++] : 0;
}


void iecFuji::task()
{
  // this gets called whenever the IEC bus is NOT in a time-sensitive state.
  // Any possibly time-comsuming tasks should be processed within here.

  // first call the underlying class task function
  IECDevice::task();
  
  if( state==DEVICE_ACTIVE )
    {
      if( payload.size()>0 ) process_cmd();
      state = DEVICE_IDLE;
    }
}


void iecFuji::reset()
{
  IECDevice::reset();
  current_fuji_cmd = -1;
  last_command = -1;
  response.clear();
  responseV.clear();
  state = DEVICE_IDLE;
}
#endif


void iecFuji::process_cmd()
{
  responseV.clear();
  response.clear();
  is_raw_command = false;
  if (current_fuji_cmd == -1) {
    // this is a new command being sent
    is_raw_command = (payload.size() == 2 && payload[0] == 0x01); // marker uint8_t
    if (is_raw_command) {
      Debug_printv("RAW command: %s", dataToHexString((uint8_t *) payload.data(), payload.size()).c_str());
      
      if (!is_supported(payload[1])) {
        Debug_printv("ERROR: Unsupported cmd: x%02x, ignoring\r\n", payload[1]);
        last_command = payload[1];
        set_fuji_iec_status(DEVICE_ERROR, "Unrecognised command");
        return;
      }
      // Debug_printf("RAW COMMAND - trying immediate action on it\r\n");
      // if it is an immediate command (no parameters), current_fuji_cmd will be reset to -1,
      // otherwise, it stays set until further data is received to process it
      current_fuji_cmd = payload[1];
      last_command = current_fuji_cmd;
      process_immediate_raw_cmds();
    } else if (payload.size() > 0) {
      // "IEC: [EF] (E0 CLOSE  15 CHANNEL)" happens with an UNLISTEN, which has no command, so we can skip it to save trying BASIC commands
      Debug_printv("BASIC command: %s", payload.c_str());
      process_basic_commands();
    }
  } else {
    // we're in the middle of some data, let's continue
    // Debug_printf("IN CMD, processing data\r\n");
    Debug_printv("RAW data: %s", dataToHexString((uint8_t *) payload.data(), payload.size()).c_str());
    process_raw_cmd_data();
  }

  if (!responseV.empty() && is_raw_command) {
    // only send raw back for a raw command, thus code can set "response", but we won't send it back as that's BASIC response
    Debug_printv("RAW response: %s", dataToHexString(responseV.data(), responseV.size()).c_str());
  }
  else if(!response.empty() && !is_raw_command) {
    // only send string response back for basic command
    Debug_printv("BASIC response: %s", response.c_str());
    responseV.assign(response.begin(), response.end());
    response.clear();
  }
  else {
    Debug_printv("NO response");
  }
}


// COMMODORE SPECIFIC CONVENIENCE COMMANDS /////////////////////

void iecFuji::local_ip()
{
    fnSystem.Net.get_ip4_info(cfg.localIP, cfg.netmask, cfg.gateway);
    std::ostringstream ss;
    ss << cfg.localIP[0] << "." << cfg.localIP[1] << "." << cfg.localIP[2] << "." << cfg.localIP[3];
    set_fuji_iec_status(0, ss.str());
}


void iecFuji::process_basic_commands()
{
    // required for appkey in BASIC
    payloadRaw = payload;
    payload = mstr::toUTF8(payload);
    pt = util_tokenize(payload, ',');

    if (payload.find("adapterconfig") != std::string::npos)
        get_adapter_config_basic();
    else if (payload.find("setssid") != std::string::npos)
        net_set_ssid_basic();
    else if (payload.find("getssid") != std::string::npos)
        net_get_ssid_basic();
    else if (payload.find("reset") != std::string::npos)
        reset_device();
    else if (payload.find("scanresult") != std::string::npos)
        net_scan_result_basic();
    else if (payload.find("scan") != std::string::npos)
        net_scan_networks_basic();
    else if (payload.find("wifienable") != std::string::npos)
      { Config.store_wifi_enabled(true); Config.save(); }
    else if (payload.find("wifidisable") != std::string::npos)
      { Config.store_wifi_enabled(false); Config.save(); }
    else if (payload.find("wifistatus") != std::string::npos)
        net_get_wifi_status_basic();
    else if (payload.find("mounthost") != std::string::npos)
        mount_host_basic();
    else if (payload.find("unmountdrive") != std::string::npos)
        disk_image_umount_basic();
    else if (payload.find("mountdrive") != std::string::npos)
        disk_image_mount_basic();
    else if (payload.find("opendir") != std::string::npos)
        open_directory_basic();
    else if (payload.find("readdir") != std::string::npos)
        read_directory_entry_basic();
    else if (payload.find("closedir") != std::string::npos)
        close_directory_basic();
    else if (payload.find("gethost") != std::string::npos ||
             payload.find("flh") != std::string::npos)
        read_host_slots_basic();
    else if (payload.find("puthost") != std::string::npos ||
             payload.find("fhost") != std::string::npos)
        write_host_slots_basic();
    else if (payload.find("getdrive") != std::string::npos)
        read_device_slots_basic();
    else if (payload.find("unmounthost") != std::string::npos)
        unmount_host_basic();
    else if (payload.find("getdirpos") != std::string::npos)
        get_directory_position_basic();
    else if (payload.find("setdirpos") != std::string::npos)
        set_directory_position_basic();
    else if (payload.find("setdrivefilename") != std::string::npos)
        set_device_filename_basic();
    else if (payload.find("writeappkey") != std::string::npos)
        write_app_key_basic();
    else if (payload.find("readappkey") != std::string::npos)
        read_app_key_basic();
    else if (payload.find("openappkey") != std::string::npos)
        open_app_key_basic();
    else if (payload.find("closeappkey") != std::string::npos)
        close_app_key_basic();
    else if (payload.find("drivefilename") != std::string::npos)
        get_device_filename_basic();
    else if (payload.find("bootconfig") != std::string::npos)
        set_boot_config_basic();
    else if (payload.find("bootmode") != std::string::npos)
        set_boot_mode_basic();
    else if (payload.find("mountall") != std::string::npos)
        mount_all();
    else if (payload.find("fujistatus") != std::string::npos)
        get_status_basic();
    else if (payload.find("localip") != std::string::npos)
        local_ip();
    else if (payload.find("enable") != std::string::npos)
        enable_device_basic();
    else if (payload.find("disable") != std::string::npos)
        disable_device_basic();
    else if (payload.find("bptiming") != std::string::npos)
    {
        if ( pt.size() < 3 ) 
            return;

        //IEC.setBitTiming(pt[1], atoi(pt[2].c_str()), atoi(pt[3].c_str()), atoi(pt[4].c_str()), atoi(pt[5].c_str()));
    }
}

bool iecFuji::is_supported(uint8_t cmd)
{
    bool result = false;

    switch (cmd)
    {
    case FUJICMD_CLOSE_APPKEY:
    case FUJICMD_CLOSE_DIRECTORY:
    case FUJICMD_CONFIG_BOOT:
    case FUJICMD_GET_ADAPTERCONFIG_EXTENDED:
    case FUJICMD_GET_ADAPTERCONFIG:
    case FUJICMD_GET_DEVICE_FULLPATH:
    case FUJICMD_GET_DIRECTORY_POSITION:
    case FUJICMD_GET_SCAN_RESULT:
    case FUJICMD_GET_SSID:
    case FUJICMD_GET_WIFI_ENABLED:
    case FUJICMD_GET_WIFISTATUS:
    case FUJICMD_HASH_CLEAR:
    case FUJICMD_HASH_COMPUTE_NO_CLEAR:
    case FUJICMD_HASH_COMPUTE:
    case FUJICMD_HASH_INPUT:
    case FUJICMD_HASH_LENGTH:
    case FUJICMD_HASH_OUTPUT:
    case FUJICMD_MOUNT_ALL:
    case FUJICMD_MOUNT_HOST:
    case FUJICMD_MOUNT_IMAGE:
    case FUJICMD_OPEN_APPKEY:
    case FUJICMD_OPEN_DIRECTORY:
    case FUJICMD_READ_APPKEY:
    case FUJICMD_READ_DEVICE_SLOTS:
    case FUJICMD_READ_DIR_ENTRY:
    case FUJICMD_READ_HOST_SLOTS:
    case FUJICMD_RESET:
    case FUJICMD_SCAN_NETWORKS:
    case FUJICMD_SET_BOOT_MODE:
    case FUJICMD_SET_DEVICE_FULLPATH:
    case FUJICMD_SET_DIRECTORY_POSITION:
    case FUJICMD_SET_SSID:
    case FUJICMD_STATUS:
    case FUJICMD_UNMOUNT_HOST:
    case FUJICMD_UNMOUNT_IMAGE:
    case FUJICMD_WRITE_APPKEY:
    case FUJICMD_WRITE_DEVICE_SLOTS:
    case FUJICMD_WRITE_HOST_SLOTS:
        result = true;
        break;
    }

    return result;
}

/*
 * During this phase, we will process any data sent to us in the follow up parameter data after the initial "open".
 * At the end, we can unset the current_fuji_cmd to show we have finished, and set any iecStatus value.
 */
void iecFuji::process_raw_cmd_data()
{
    bool was_processed = true;

    switch (current_fuji_cmd)
    {
    case FUJICMD_GET_SCAN_RESULT:
        net_scan_result_raw();
        break;
    case FUJICMD_SET_SSID:
        net_set_ssid_raw();
        break;
    case FUJICMD_MOUNT_HOST:
        mount_host_raw();
        break;
    case FUJICMD_MOUNT_IMAGE:
        disk_image_mount_raw();
        break;
    case FUJICMD_OPEN_DIRECTORY:
        open_directory_raw();
        break;
    case FUJICMD_READ_DIR_ENTRY:
        read_directory_entry_raw();
        break;
    case FUJICMD_WRITE_DEVICE_SLOTS:
        write_device_slots_raw();
        break;
    case FUJICMD_UNMOUNT_IMAGE:
        disk_image_umount_raw();
        break;
    case FUJICMD_UNMOUNT_HOST:
        unmount_host_raw();
        break;
    case FUJICMD_SET_DIRECTORY_POSITION:
        set_directory_position_raw();
        break;
    case FUJICMD_SET_DEVICE_FULLPATH:
        set_device_filename_raw();
        break;
    case FUJICMD_WRITE_APPKEY:
        write_app_key_raw();
        break;
    case FUJICMD_OPEN_APPKEY:
        open_app_key_raw();
        break;
    case FUJICMD_GET_DEVICE_FULLPATH:
        get_device_filename_raw();
        break;
    case FUJICMD_CONFIG_BOOT:
        set_boot_config_raw();
        break;
    case FUJICMD_SET_BOOT_MODE:
        set_boot_mode_raw();
        break;
    case FUJICMD_HASH_COMPUTE_NO_CLEAR:
        hash_compute_raw(false);
        break;
    case FUJICMD_HASH_COMPUTE:
        hash_compute_raw(true);
        break;
    case FUJICMD_HASH_INPUT:
        hash_input_raw();
        break;
    case FUJICMD_HASH_LENGTH:
        hash_length_raw();
        break;
    case FUJICMD_HASH_OUTPUT:
        hash_output_raw();
        break;
    default:
        was_processed = false;
    }

    // TODO: should this be done by the commands after they have finished? Doing it here means they can only get one blast of data
    // For now, leave it here, and see if any commands ever need to have multiple data items sent over multiple write commands
    if (was_processed) {
        // Debug_printf("--- Processed! Resetting current_fuji_cmd\r\n");
        current_fuji_cmd = -1;
    // } else {
    //     Debug_printf("xxx Not Processed, current_fuji_cmd staying as: %d\r\n", current_fuji_cmd);
    }
}

/*
 * Immediate commands do not expect any further data as part of the command, so can execute immediately
 * Once complete, the current_fuji_cmd is then unset.
 */
void iecFuji::process_immediate_raw_cmds()
{
    bool was_immediate_cmd = true;

    switch (current_fuji_cmd)
    {
    case FUJICMD_RESET:
        reset_device();
        break;
    case FUJICMD_GET_SSID:
        net_get_ssid_raw();
        break;
    case FUJICMD_SCAN_NETWORKS:
        net_scan_networks_raw();
        break;
    case FUJICMD_GET_WIFISTATUS:
        net_get_wifi_status_raw();
        break;
    case FUJICMD_GET_WIFI_ENABLED:
        net_get_wifi_enabled_raw();
        break;
    case FUJICMD_CLOSE_DIRECTORY:
        close_directory_raw();
        break;
    case FUJICMD_READ_HOST_SLOTS:
        read_host_slots_raw();
        break;
    case FUJICMD_WRITE_HOST_SLOTS:
        write_host_slots_raw();
        break;
    case FUJICMD_READ_DEVICE_SLOTS:
        read_device_slots_raw();
        break;
    case FUJICMD_GET_ADAPTERCONFIG:
        get_adapter_config_raw();
        break;
    case FUJICMD_GET_ADAPTERCONFIG_EXTENDED:
        get_adapter_config_extended_raw();
        break;
    case FUJICMD_GET_DIRECTORY_POSITION:
        get_directory_position_raw();
        break;
    case FUJICMD_READ_APPKEY:
        read_app_key_raw();
        break;
    case FUJICMD_CLOSE_APPKEY:
        close_app_key_raw();
        break;
    case FUJICMD_STATUS:
        get_status_raw();
        break;
    case FUJICMD_MOUNT_ALL:
        mount_all();
        break;
    case FUJICMD_HASH_CLEAR:
        hash_clear();
        break;
    default:
        // not an immediate command, so exit without changing current_fuji_cmd, as we need to be sent data
        was_immediate_cmd = false;
        break;
    }

    if (was_immediate_cmd)
    {
        // unset the current command, as we are not expecting any data
        // Debug_printf("Immediate CMD processed! Resetting current_fuji_cmd\r\n");
        current_fuji_cmd = -1;
    // } else {
    //     Debug_printf("Not Immediate command, will wait for more data for cmd: %02x\r\n", current_fuji_cmd);
    }
}

void iecFuji::get_status_raw()
{
    // convert iecStatus to a responseV for the host to read
    responseV = iec_status_to_vector();
    // don't set the status!!
    // set_fuji_iec_status(0, "");
}

void iecFuji::get_status_basic()
{
    std::ostringstream oss;
    oss << "err=" << iecStatus.error << ",conn=" << iecStatus.connected << ",chan=" << iecStatus.channel << ",msg=" << iecStatus.msg;
    response = oss.str();
    set_fuji_iec_status(0, response);
}

void iecFuji::reset_device()
{
    fnSystem.reboot();
}

void iecFuji::net_scan_networks_basic()
{
    net_scan_networks();
    response = std::to_string(_countScannedSSIDs);
    set_fuji_iec_status(0, "ok");
}

void iecFuji::net_scan_networks_raw()
{
    net_scan_networks();
    responseV.push_back(_countScannedSSIDs);
    set_fuji_iec_status(0, "");
}


void iecFuji::net_scan_result_basic()
{
    if (pt.size() != 2) {
        return;
    }
    util_remove_spaces(pt[1]);
    int i = atoi(pt[1].c_str());
    scan_result_t result = net_scan_result(i);
    response = std::to_string(result.rssi) + ",\"" + std::string(result.ssid) + "\"";
    set_fuji_iec_status(0, "ok");
}

void iecFuji::net_scan_result_raw()
{
    int n = payload[0];
    scan_result_t result = net_scan_result(n);
    responseV.assign(reinterpret_cast<const uint8_t*>(&result), reinterpret_cast<const uint8_t*>(&result) + sizeof(result));
    // Debug_printf("assigned scan_result to responseV, size: %d\r\n%s\r\n", responseV.size(), util_hexdump(&responseV.data()[0], responseV.size()).c_str());
    set_fuji_iec_status(0, "");
}

void iecFuji::net_get_ssid_basic()
{
    net_config_t net_config = net_get_ssid();
    response = std::string(net_config.ssid);
    set_fuji_iec_status(0, "ok");
}

void iecFuji::net_get_ssid_raw()
{
    net_config_t net_config = net_get_ssid();
    responseV.assign(reinterpret_cast<const uint8_t*>(&net_config), reinterpret_cast<const uint8_t*>(&net_config) + sizeof(net_config));
    set_fuji_iec_status(0, "");
}


void iecFuji::net_set_ssid_basic(bool store)
{
    if (pt.size() != 2)
    {
        Debug_printv("error: bad args");
        response = "BAD SSID ARGS";
        return;
    }

    // Strip off the SETSSID: part of the command
    pt[0] = pt[0].substr(8, std::string::npos);
    if (mstr::isNumeric(pt[0])) {
        // Find SSID by CRC8 Number
        pt[0] = fnWiFi.get_network_name_by_crc8(std::stoi(pt[0]));
    }

    // Debug_printv("pt1[%s] pt2[%s]", pt[1].c_str(), pt[2].c_str());

    // URL Decode SSID/PASSWORD. This will convert any %xx values to their equivalent char for the xx code. e.g. "%20" is a space
    // It does NOT replace '+' with a space as that's frankly just insane.
    // NOTE: if your password or username has a deliberate % followed by 2 digits, (e.g. the literal string "%69") you will have to type "%2569" to "escape" the percentage.
    std::string ssid = mstr::urlDecode(pt[0], false);
    std::string passphrase = mstr::urlDecode(pt[1], false);

    if (ssid.length() > MAX_SSID_LEN || passphrase.length() > MAX_PASSPHRASE_LEN || ssid.length() == 0 || passphrase.length() == 0) {
        response = "ERROR: BAD SSID/PASSPHRASE";
        return;
    }

    net_config_t net_config;
    memset(&net_config, 0, sizeof(net_config_t));

    strncpy(net_config.ssid, ssid.c_str(), ssid.length());
    strncpy(net_config.password, passphrase.c_str(), passphrase.length());
    Debug_printv("ssid[%s] pass[%s]", net_config.ssid, net_config.password);

    net_set_ssid(store, net_config);
}

void iecFuji::enable_device_basic(std::string ids)
{
    if(ids.empty()) {
        // Strip off the ENABLE: part of the command
        pt[0] = pt[0].substr(7, std::string::npos);
    } else {
        pt = util_tokenize(ids, ',');
    }

    // Enable devices
    for (int i = 0; i < pt.size(); i++) {
        uint8_t device = atoi(pt[i].c_str());
        auto d = IEC.findDevice(device, true);
        if (d) {
            d->setActive(true);
            Config.store_device_slot_enable((device - 7), true);
            Debug_printv("Enable Device #%d [%d]", device, d->isActive());
        }
    }
    Config.save();
}

void iecFuji::disable_device_basic(std::string ids)
{
    if(ids.empty()) {
        // Strip off the DISABLE: part of the command
        pt[0] = pt[0].substr(8, std::string::npos);
    } else {
        pt = util_tokenize(ids, ',');
    }

    // Disable devices
    for (int i = 0; i < pt.size(); i++) {
        uint8_t device = atoi(pt[i].c_str());
        auto d = IEC.findDevice(device, true);
        if (d) {
            d->setActive(false);
            Config.store_device_slot_enable((device - 7), false);
            Debug_printv("Disable Device #%d [%d]", device, d->isActive());
        }
    }
    Config.save();
}

void iecFuji::net_set_ssid_raw(bool store)
{
    net_config_t net_config;
    memset(&net_config, 0, sizeof(net_config_t));

    // the data is coming to us packed not in struct format, so depack it into the NetConfig
    // Had issues with it not sending password after large number of \0 padding ssid.
    uint8_t sent_ssid_len = strlen(&payload[0]);
    uint8_t sent_pass_len = strlen(&payload[1 + sent_ssid_len]);
    strncpy((char *)&net_config.ssid[0], (const char *) &payload[0], sent_ssid_len);
    std::string password = mstr::toPETSCII2((const char *) &payload[1 + sent_ssid_len]);
    strncpy((char *)&net_config.password[0], (const char *) &password[0], sent_pass_len);
    Debug_printv("ssid[%s] pass[%s]", net_config.ssid, net_config.password);

    net_set_ssid(store, net_config);
}

void iecFuji::net_get_wifi_status_raw()
{
    responseV.push_back(net_get_wifi_status());
    set_fuji_iec_status(0, "");
}

void iecFuji::net_get_wifi_status_basic()
{
    response = net_get_wifi_status() == 3 ? "connected" : "disconnected";
    set_fuji_iec_status(0, "ok");
}

void iecFuji::net_get_wifi_enabled_raw()
{
    responseV.push_back(net_get_wifi_enabled());
    set_fuji_iec_status(0, "");
}

uint8_t iecFuji::net_get_wifi_enabled()
{
    return Config.get_wifi_enabled() ? 1 : 0;
}

void iecFuji::unmount_host_basic()
{
    if (pt.size() < 2)
    {
        response = "invalid # of parameters";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    int hs = atoi(pt[1].c_str());

    if (!_validate_host_slot(hs, "unmount_host"))
    {
        response = "invalid host slot";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }
    if (!unmount_host(hs)) {
        response = "error unmounting host";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    response="ok";
    set_fuji_iec_status(0, response);
}

void iecFuji::unmount_host_raw()
{
    uint8_t hs = payload[0];
    if (!_validate_host_slot(hs, "unmount_host"))
    {
        set_fuji_iec_status(DEVICE_ERROR, "invalid host slot");
        return;
    }
    if (!unmount_host(hs)) {
        set_fuji_iec_status(DEVICE_ERROR, "error unmounting host");
        return;
    }

    set_fuji_iec_status(0, "");
}

bool iecFuji::unmount_host(uint8_t hs)
{
    return _fnHosts[hs].umount();
}

void iecFuji::mount_host_raw()
{
    int hs = payload[0];
    if (hs < 0 || hs >= MAX_HOSTS)
    {
        set_fuji_iec_status(DEVICE_ERROR, "Invalid host slot parameter");
        return;
    }

    if (!mount_host(hs))
    {
        set_fuji_iec_status(DEVICE_ERROR, "Failed to mount host slot");
    }
}

void iecFuji::mount_host_basic()
{
    if (pt.size() < 2)
    {
        response = "INVALID # OF PARAMETERS.";
        return;
    }

    int hs = atoi(pt[1].c_str());
    if (hs < 0 || hs >= MAX_HOSTS) {
        response = "INVALID HOST #";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    if (mount_host(hs)) {
      std::string hns = _fnHosts[hs].get_hostname();
        hns = mstr::toPETSCII2(hns);
        response = hns + " MOUNTED.";
        set_fuji_iec_status(0, response);
    } else {
        response = "UNABLE TO MOUNT HOST SLOT #";
        set_fuji_iec_status(DEVICE_ERROR, response);
    }
}


void iecFuji::disk_image_mount_basic()
{
    _populate_slots_from_config();
    
    if (pt.size() < 3)
    {
        response = "invalid # of parameters";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    uint8_t ds = atoi(pt[1].c_str());
    uint8_t mode = atoi(pt[2].c_str());

    if (!disk_image_mount(ds, mode))
    {
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }
    set_fuji_iec_status(0, response);
}

void iecFuji::disk_image_mount_raw()
{
    _populate_slots_from_config();
    if (!disk_image_mount(payload[0], payload[1]))
    {
        set_fuji_iec_status(DEVICE_ERROR, "Failed to mount disk image");
    }
    set_fuji_iec_status(0, "");
}

// Toggle boot config on/off, aux1=0 is disabled, aux1=1 is enabled
void iecFuji::set_boot_config_basic()
{
    if (pt.size() < 2)
    {
        Debug_printf("Invalid # of parameters.\r\n");
        response = "invalid # of parameters";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    set_boot_config(atoi(pt[1].c_str()) != 0);
    response = "ok";
    set_fuji_iec_status(0, response);
}

void iecFuji::set_boot_config_raw()
{
    set_boot_config(payload[0] != 0);
    set_fuji_iec_status(0, "");
}

void iecFuji::set_boot_config(bool should_boot_config)
{
    boot_config = should_boot_config;
}


// Do SIO copy
void iecFuji::copy_file()
{
    // TODO IMPLEMENT
}

// TODO: refactor this into BASIC/RAW, for now, leaving it with the response value and using "is_raw_command"
void iecFuji::mount_all()
{
    // Check at the end if no disks are in a slot and disable config
    bool nodisks = true;

    for (int i = 0; i < MAX_DISK_DEVICES; i++)
    {
        fujiDisk &disk = _fnDisks[i];
        fujiHost &host = _fnHosts[disk.host_slot];
        char flag[3] = {'r', 0, 0};

        if (disk.access_mode == DISK_ACCESS_MODE_WRITE)
            flag[1] = '+';

        if (disk.host_slot != INVALID_HOST_SLOT && strlen(disk.filename) > 0)
        {
            nodisks = false; // We have a disk in a slot

            if (!host.mount())
            {
                std::string slotno = std::to_string(i);
                response = "error: unable to mount slot " + slotno + "\r\n";
                set_fuji_iec_status(DEVICE_ERROR, response);
                return;
            }

            Debug_printf("Selecting '%s' from host #%u as %s on D%u:\r\n",
                         disk.filename, disk.host_slot, flag, i + 1);

            disk.fileh = host.file_open(disk.filename, disk.filename, sizeof(disk.filename), flag);

            if (disk.fileh == nullptr)
            {
                std::string slotno = std::to_string(i);
                response = "error: invalid file handle for slot " + slotno + "\r\n";
                set_fuji_iec_status(DEVICE_ERROR, response);
                return;
            }

            // We've gotten this far, so make sure our bootable CONFIG disk is disabled
            boot_config = false;

            // We need the file size for loading XEX files and for CASSETTE, so get that too
            disk.disk_size = host.file_size(disk.fileh);

            // Set the host slot for high score mode
            // TODO: Refactor along with mount disk image.
            disk.disk_dev.m_host = &host;

            // And now mount it
            disk.disk_type = disk.disk_dev.mount(disk.fileh, disk.filename, disk.disk_size);
        }
    }

    if (nodisks)
    {
        // No disks in a slot, disable config
        boot_config = false;
    }

    response = "ok";
    if (is_raw_command) {
        set_fuji_iec_status(0, "");
    } else {
        set_fuji_iec_status(0, response);
    }
}

// Set boot mode
void iecFuji::set_boot_mode_basic()
{
    if (pt.size() < 2)
    {
        Debug_printf("SET BOOT CONFIG MODE - Invalid # of parameters.\r\n");
        response = "invalid # of parameters";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    set_boot_mode(atoi(pt[1].c_str()), true);

    response = "ok";
    set_fuji_iec_status(0, response);
}

void iecFuji::set_boot_mode_raw()
{
    set_boot_mode(payload[0], true);
    set_fuji_iec_status(0, "");
}

void iecFuji::set_boot_mode(uint8_t boot_device, bool should_boot_config)
{
    insert_boot_device(boot_device);
    boot_config = should_boot_config;
}

char *_generate_appkey_filename(appkey *info)
{
    static char filenamebuf[30];

    snprintf(filenamebuf, sizeof(filenamebuf), "/FujiNet/%04hx%02hhx%02hhx.key", info->creator, info->app, info->key);
    return filenamebuf;
}

/*
 Opens an "app key".  This just sets the needed app key parameters (creator, app, key, mode)
 for the subsequent expected read/write command. We could've added this information as part
 of the payload in a WRITE_APPKEY command, but there was no way to do this for READ_APPKEY.
 Requiring a separate OPEN command makes both the read and write commands behave similarly
 and leaves the possibity for a more robust/general file read/write function later.
*/
void iecFuji::open_app_key_basic()
{
    unsigned int val;

    if (pt.size() < 5)
    {
        Debug_printf("Incorrect number of parameters in open_app_key_basic.\r\n");
        response = "invalid # of parameters";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    // We're only supporting writing to SD, so return an error if there's no SD mounted
    if (!fnSDFAT.running())
    {
        Debug_println("No SD mounted - returning error");
        response = "no sd card mounted";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    sscanf(pt[1].c_str(), "%x", &val);
    uint16_t creator = (uint16_t) val;
    sscanf(pt[2].c_str(), "%x", &val);
    uint8_t app = (uint8_t)val;
    sscanf(pt[3].c_str(), "%x", &val);
    uint8_t key = (uint8_t)val;
    sscanf(pt[4].c_str(), "%x", &val);
    appkey_mode mode = (appkey_mode)val;

    // Basic check for valid data
    if (_current_appkey.creator == 0 || _current_appkey.mode == APPKEYMODE_INVALID)
    {
        Debug_println("Invalid app key data");
        response = "invalid app key data";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    open_app_key(creator, app, key, mode, 0);

    response = "ok";
    set_fuji_iec_status(0, response);
}

void iecFuji::open_app_key_raw()
{
    Debug_print("Fuji cmd: OPEN APPKEY\r\n");
    if (!fnSDFAT.running())
    {
        Debug_println("No SD mounted - returning error");
        set_fuji_iec_status(DEVICE_ERROR, "no sd card mounted");
        return;
    }
    
    uint16_t creator = payload[0] | (payload[1] << 8);
    uint8_t app = payload[2];
    uint8_t key = payload[3];
    appkey_mode mode = (appkey_mode) payload[4];
    uint8_t reserved = payload[5];

    open_app_key(creator, app, key, mode, reserved);
    set_fuji_iec_status(0, "");
}

void iecFuji::open_app_key(uint16_t creator, uint8_t app, uint8_t key, appkey_mode mode, uint8_t reserved)
{
    _current_appkey.creator = creator;
    _current_appkey.app = app;
    _current_appkey.key = key;
    _current_appkey.mode = mode;
    _current_appkey.reserved = reserved;

    Debug_printf("App key creator = 0x%04hx, app = 0x%02hhx, key = 0x%02hhx, mode = %hhu, filename = \"%s\"\r\n",
                 _current_appkey.creator, _current_appkey.app, _current_appkey.key, _current_appkey.mode,
                 _generate_appkey_filename(&_current_appkey));

}

void iecFuji::close_app_key_basic()
{
    close_app_key();
    response = "ok";
    set_fuji_iec_status(0, response);
}

void iecFuji::close_app_key_raw()
{
    close_app_key();
    set_fuji_iec_status(0, "");
}

/*
  The app key close operation is a placeholder in case we want to provide more robust file
  read/write operations. Currently, the file is closed immediately after the read or write operation.
*/
void iecFuji::close_app_key()
{
    Debug_print("Fuji cmd: CLOSE APPKEY\r\n");
    _current_appkey.creator = 0;
    _current_appkey.mode = APPKEYMODE_INVALID;
}

bool iecFuji::check_appkey_creator(bool check_is_write)
{
    return !(_current_appkey.creator == 0 || (check_is_write && _current_appkey.mode != APPKEYMODE_WRITE));
}

void iecFuji::write_app_key_basic()
{
    // In BASIC, the key length is specified in the parameters.
    if (pt.size() <= 2)
    {
        response = "invalid # of parameters";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    if (!check_appkey_creator(true))
    {
        Debug_println("Invalid app key metadata - aborting");
        response = "malformed appkey data.";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    if (!fnSDFAT.running())
    {
        Debug_println("No SD mounted - can't write app key");
        response = "no sd card mounted";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    // Tokenize the raw payload to save to the file - WE MAKE NO CHANGES TO DATA, the host sends raw values
    std::vector<std::string> ptRaw;
    ptRaw = tokenize_basic_command(payloadRaw);

    // do we need keylen anymore?
    int keylen = atoi(pt[1].c_str());
    
    // Bounds check
    if (keylen > MAX_APPKEY_LEN)
        keylen = MAX_APPKEY_LEN;

    std::vector<uint8_t> key_data(ptRaw[2].begin(), ptRaw[2].end());
    int write_size = key_data.size();

    int count = write_app_key(std::move(key_data));
    if (count != write_size) {
        int e = errno;
        std::ostringstream oss;
        oss << "error: only wrote " << count << " bytes of expected " << write_size << ", errno=" << e;
        response = oss.str();
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    response = "ok";
    set_fuji_iec_status(0, response);

}

void iecFuji::write_app_key_raw()
{
    if (!check_appkey_creator(true))
    {
        set_fuji_iec_status(DEVICE_ERROR, "creator information missing");
        return;
    }
    if (!fnSDFAT.running())
    {
        set_fuji_iec_status(DEVICE_ERROR, "sd filesystem not running");
        return;
    }

    // we can't write more than the appkey_size, which is set by the mode.
    // May have to change this later as per Eric's comments in discord.
    size_t write_size = payload.size();
    if (write_size > appkey_size)
    {
        Debug_printf("ERROR: key data sent was larger than keysize. Aborting rather than potentially corrupting existing data.");
        set_fuji_iec_status(DEVICE_ERROR, "too much data for appkey");
        return;
    }

    std::vector<uint8_t> key_data(payload.begin(), payload.end());
    Debug_printf("key_data: \r\n%s\r\n", util_hexdump(key_data.data(), key_data.size()).c_str());
    int count = write_app_key(std::move(key_data));
    if (count != write_size) {
        int e = errno;
        std::ostringstream oss;
        oss << "error: only wrote " << count << " bytes of expected " << write_size << ", errno=" << e;
        set_fuji_iec_status(DEVICE_ERROR, oss.str());
        return;
    }

    set_fuji_iec_status(0, "");
}

int iecFuji::write_app_key(std::vector<uint8_t>&& value)
{
    char *filename = _generate_appkey_filename(&_current_appkey);

    // Reset the app key data so we require calling APPKEY OPEN before another attempt
    _current_appkey.creator = 0;
    _current_appkey.mode = APPKEYMODE_INVALID;

    Debug_printf("Writing appkey to \"%s\"\r\n", filename);

    // Make sure we have a "/FujiNet" directory, since that's where we're putting these files
    fnSDFAT.create_path("/FujiNet");

    FILE *fOut = fnSDFAT.file_open(filename, FILE_WRITE);
    if (fOut == nullptr)
    {
        Debug_printf("Failed to open/create output file: errno=%d\r\n", errno);
        return -1;
    }
    size_t count = fwrite(value.data(), 1, value.size(), fOut);
    fclose(fOut);
    return count;
}

/*
 Read an "app key" from SD (ONLY!) storage
*/
void iecFuji::read_app_key_basic()
{
    Debug_println("Fuji cmd: READ APPKEY");

    // Make sure we have an SD card mounted
    if (!fnSDFAT.running())
    {
        Debug_println("No SD mounted - can't read app key");
        response = "no sd mounted";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    // Make sure we have valid app key information
    if (_current_appkey.creator == 0 || _current_appkey.mode != APPKEYMODE_READ)
    {
        Debug_println("Invalid app key metadata - aborting");
        response = "invalid appkey metadata";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    char *filename = _generate_appkey_filename(&_current_appkey);
    Debug_printf("Reading appkey from \"%s\"\r\n", filename);

    std::vector<uint8_t> response_data;
    if (read_app_key(filename, response_data) == -1) {
        Debug_println("Failed to read appkey file");
        response = "failed to read appkey file";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

// use ifdef to guard against calling hexdump if we're not using debug
#ifdef DEBUG
	Debug_printf("appkey data:\r\n%s\r\n", util_hexdump(response_data.data(), response_data.size()).c_str());
#endif

    response.assign(response_data.begin(), response_data.end());
    set_fuji_iec_status(0, "ok");
}


void iecFuji::read_app_key_raw()
{
    if (!fnSDFAT.running())
    {
        Debug_println("No SD mounted - can't read app key");
        set_fuji_iec_status(DEVICE_ERROR, "sd card not mounted");
        return;
    }

    // Make sure we have valid app key information
    if (_current_appkey.creator == 0 || _current_appkey.mode != APPKEYMODE_READ)
    {
        Debug_println("Invalid app key metadata - aborting");
        set_fuji_iec_status(DEVICE_ERROR, "invalid appkey meta data");
        return;
    }

    char *filename = _generate_appkey_filename(&_current_appkey);
    Debug_printf("Reading appkey from \"%s\"\r\n", filename);

    if (read_app_key(filename, responseV) == -1) {
        Debug_println("Failed to read appkey file");
        set_fuji_iec_status(DEVICE_ERROR, "failed to read appkey file");
        return;
    }

// use ifdef to guard against calling hexdump if we're not using debug
#ifdef DEBUG
	Debug_printf("appkey data:\r\n%s\r\n", util_hexdump(responseV.data(), responseV.size()).c_str());
#endif

    set_fuji_iec_status(0, "");
}

int iecFuji::read_app_key(char *filename, std::vector<uint8_t>& file_data)
{
    FILE *fIn = fnSDFAT.file_open(filename, "r");
    if (fIn == nullptr)
    {
        std::ostringstream oss;
        oss << "Failed to open input file: errno=" << errno;
        response = oss.str();
        set_fuji_iec_status(DEVICE_ERROR, response);
        return -1;
    }

    file_data.resize(appkey_size);
    size_t count = fread(file_data.data(), 1, file_data.size(), fIn);
    file_data.resize(count);
    Debug_printf("Read %u bytes from input file\r\n", (unsigned)count);
    fclose(fIn);

    return count;
}

void iecFuji::disk_image_umount_basic()
{
    uint8_t deviceSlot = atoi(pt[1].c_str());
    if (!disk_image_umount(deviceSlot)) {
        response = "invalid device slot";
        set_fuji_iec_status(DEVICE_ERROR, "invalid device slot");
    } else {
        response = "ok";
        set_fuji_iec_status(0, "");
    }
}

void iecFuji::disk_image_umount_raw()
{
    uint8_t deviceSlot = payload[0];
    if (!disk_image_umount(deviceSlot)) {
        set_fuji_iec_status(DEVICE_ERROR, "invalid device slot");
    } else {
        set_fuji_iec_status(0, "");
    }
}

bool iecFuji::disk_image_umount(uint8_t deviceSlot)
{
    Debug_printf("Fuji cmd: UNMOUNT IMAGE 0x%02X\r\n", deviceSlot);

    bool is_success = false;
    if (deviceSlot < MAX_DISK_DEVICES)
    {
        _fnDisks[deviceSlot].disk_dev.unmount();
        //_fnDisks[deviceSlot].disk_dev.device_active = false;
        _fnDisks[deviceSlot].reset();
        is_success = true;
    }
    return is_success;
}

// Disk Image Rotate
/*
  We rotate disks my changing their disk device ID's. That prevents
  us from having to unmount and re-mount devices.
*/
void iecFuji::image_rotate()
{
    // TODO IMPLEMENT
}

std::pair<std::string, std::string> split_at_delim(const std::string& input, char delim) {
    // Find the position of the first occurrence of delim in the string
    size_t pos = input.find(delim);

    std::string firstPart, secondPart;
    if (pos != std::string::npos) {
        firstPart = input.substr(0, pos);
        // Check if there's content beyond the delim for the second part
        if (pos + 1 < input.size()) {
            secondPart = input.substr(pos + 1);
        }
    } else {
        // If delim is not found, the entire input is the first part
        firstPart = input;
    }

    // Remove trailing slash from firstPart, if present
    if (!firstPart.empty() && firstPart.back() == '/') {
        firstPart.pop_back();
    }

    return {firstPart, secondPart};
}

void iecFuji::open_directory_basic()
{
    Debug_println("Fuji cmd: OPEN DIRECTORY");

    if (pt.size() < 3)
    {
        response = "invalid # of parameters.";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    uint8_t host_slot = atoi(pt[1].c_str());
    auto [dirpath, pattern] = split_at_delim(pt[2], '~');

    if (!open_directory(host_slot, dirpath, pattern)) {
        set_fuji_iec_status(DEVICE_ERROR, "");
        return;
    }

    set_fuji_iec_status(0, "ok");
}

void iecFuji::open_directory_raw()
{
    Debug_println("Fuji cmd: OPEN DIRECTORY");
    if (payload.size() < 1)
    {
        Debug_printf("ERROR: open_directory_raw, payload too short\r\n");
        set_fuji_iec_status(DEVICE_ERROR, "Bad parameters to open directory");
        return;
    }

    uint8_t host_slot = payload[0];
    auto [dirpath, pattern] = split_at_delim(payload.substr(1),  '\0');

    if (!open_directory(host_slot, dirpath, pattern)) {
        set_fuji_iec_status(DEVICE_ERROR, "Failed to open directory");
        return;
    }
    set_fuji_iec_status(0, "");
}


bool iecFuji::validate_parameters_and_setup(uint8_t& maxlen, uint8_t& addtlopts) {
    if (pt.size() < 2) {
        return false;
    }
    maxlen = atoi(pt[1].c_str());
    addtlopts = atoi(pt[2].c_str());
    return true;
}

bool iecFuji::validate_directory_slot() {
    return _current_open_directory_slot != -1;
}

void iecFuji::read_directory_entry_basic() {
    uint8_t maxlen, addtlopts;
    if (!validate_parameters_and_setup(maxlen, addtlopts)) {
        response = "invalid # of parameters";
        set_fuji_iec_status(DEVICE_ERROR, "Invalid parameters");
        return;
    }
    if (!validate_directory_slot()) {
        response = "no currently open directory";
        set_fuji_iec_status(DEVICE_ERROR, "No currently open directory");
        return;
    }
    std::string entry = read_directory_entry(maxlen, addtlopts);
    response = mstr::toPETSCII2(entry);
    set_fuji_iec_status(0, "");
}

void iecFuji::read_directory_entry_raw() {
    if (!validate_directory_slot()) {
        set_fuji_iec_status(DEVICE_ERROR, "No current open directory");
        return;
    }

    uint8_t maxlen = payload[0];
    uint8_t addtlopts = payload[1];

    std::string entry = read_directory_entry(maxlen, addtlopts);
    responseV.assign(entry.begin(), entry.end());
    set_fuji_iec_status(0, "");
}


void iecFuji::get_directory_position_basic()
{
    Debug_println("Fuji cmd: GET DIRECTORY POSITION");

    if (!validate_directory_slot())
    {
        response = "no currently open directory";
        Debug_println(response.c_str());
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    uint16_t pos = get_directory_position();
    if (pos == FNFS_INVALID_DIRPOS)
    {
        response = "invalid directory position";
        Debug_println(response.c_str());
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    response = std::to_string(pos);
    set_fuji_iec_status(0, "ok");
}

void iecFuji::get_directory_position_raw()
{
    if (!validate_directory_slot())
    {
        set_fuji_iec_status(DEVICE_ERROR, "no currently open directory");
        return;
    }

    uint16_t pos = get_directory_position();
    responseV.assign(reinterpret_cast<const uint8_t*>(&pos), reinterpret_cast<const uint8_t*>(&pos) + sizeof(pos));
    set_fuji_iec_status(0, "");
}

uint16_t iecFuji::get_directory_position()
{
    return _fnHosts[_current_open_directory_slot].dir_tell();
}

void iecFuji::set_directory_position_basic()
{
    Debug_println("Fuji cmd: SET DIRECTORY POSITION");
    uint16_t pos = 0;
    if (pt.size() < 2)
    {
        response = "error: invalid directory position";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    pos = atoi(pt[1].c_str());

    if (!validate_directory_slot())
    {
        Debug_print("No currently open directory\r\n");
        response = "error: no currently open directory";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    bool result = _fnHosts[_current_open_directory_slot].dir_seek(pos);
    if (!result)
    {
        response = "error: unable to perform directory seek";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }
    response = "ok";
    set_fuji_iec_status(0, response);
}

void iecFuji::set_directory_position_raw()
{
    Debug_println("Fuji cmd: SET DIRECTORY POSITION");

    if (!validate_directory_slot())
    {
        Debug_print("No currently open directory\r\n");
        set_fuji_iec_status(DEVICE_ERROR, "error: no currently open directory");
        return;
    }

    uint16_t pos = payload[0] | (payload[1] << 8);
    bool result = _fnHosts[_current_open_directory_slot].dir_seek(pos);
    if (!result)
    {
        set_fuji_iec_status(DEVICE_ERROR, "error: unable to perform directory seek");
        return;
    }
    set_fuji_iec_status(0, "");
}

bool iecFuji::set_directory_position(uint16_t pos)
{
    return _fnHosts[_current_open_directory_slot].dir_seek(pos);
}

void iecFuji::close_directory_basic()
{
    close_directory();
    response = "ok";
    set_fuji_iec_status(0, response);
}

void iecFuji::close_directory_raw()
{
    close_directory();
    set_fuji_iec_status(0, "");
}

void iecFuji::close_directory()
{
    Debug_println("Fuji cmd: CLOSE DIRECTORY");

    if (_current_open_directory_slot != -1)
        _fnHosts[_current_open_directory_slot].dir_close();

    _current_open_directory_slot = -1;
}

void iecFuji::get_adapter_config_basic()
{
    get_adapter_config();
    response = "use localip netmask gateway dnsip bssid hostname version";
    set_fuji_iec_status(0, "ok");
}

void iecFuji::get_adapter_config_raw()
{
    get_adapter_config();
    responseV.assign(reinterpret_cast<const uint8_t*>(&cfg), reinterpret_cast<const uint8_t*>(&cfg) + sizeof(AdapterConfig));
    set_fuji_iec_status(0, "");
}

void iecFuji::get_adapter_config_extended_raw()
{
    AdapterConfigExtended cfg = get_adapter_config_extended();
    responseV.assign(reinterpret_cast<const uint8_t*>(&cfg), reinterpret_cast<const uint8_t*>(&cfg) + sizeof(AdapterConfigExtended));
    set_fuji_iec_status(0, "");
}

void iecFuji::get_adapter_config()
{
    // This reads the current configuration from the adapter into memory.
    memset(&cfg, 0, sizeof(cfg));

    strlcpy(cfg.fn_version, fnSystem.get_fujinet_version(true), sizeof(cfg.fn_version));

    if (!fnWiFi.connected())
    {
        strlcpy(cfg.ssid, "NOT CONNECTED", sizeof(cfg.ssid));
        strlcpy(cfg.hostname, "NOT CONNECTED", sizeof(cfg.hostname));
    }
    else
    {
        strlcpy(cfg.hostname, fnSystem.Net.get_hostname().c_str(), sizeof(cfg.hostname));
        strlcpy(cfg.ssid, fnWiFi.get_current_ssid().c_str(), sizeof(cfg.ssid));
        fnWiFi.get_current_bssid(cfg.bssid);
        fnSystem.Net.get_ip4_info(cfg.localIP, cfg.netmask, cfg.gateway);
        fnSystem.Net.get_ip4_dns_info(cfg.dnsIP);
    }

    fnWiFi.get_mac(cfg.macAddress);
}

AdapterConfigExtended iecFuji::get_adapter_config_extended()
{
    // This reads the current configuration from the adapter into memory.
    AdapterConfigExtended cfg;
    memset(&cfg, 0, sizeof(cfg));

    strlcpy(cfg.fn_version, fnSystem.get_fujinet_version(true), sizeof(cfg.fn_version));

    if (!fnWiFi.connected())
    {
        strlcpy(cfg.ssid, "NOT CONNECTED", sizeof(cfg.ssid));
        strlcpy(cfg.hostname, "NOT CONNECTED", sizeof(cfg.hostname));
    }
    else
    {
        strlcpy(cfg.hostname, fnSystem.Net.get_hostname().c_str(), sizeof(cfg.hostname));
        strlcpy(cfg.ssid, fnWiFi.get_current_ssid().c_str(), sizeof(cfg.ssid));
        fnWiFi.get_current_bssid(cfg.bssid);
        fnSystem.Net.get_ip4_info(cfg.localIP, cfg.netmask, cfg.gateway);
        fnSystem.Net.get_ip4_dns_info(cfg.dnsIP);
    }

    fnWiFi.get_mac(cfg.macAddress);

    // convert fields to strings
    strlcpy(cfg.sLocalIP, fnSystem.Net.get_ip4_address_str().c_str(), 16);
    strlcpy(cfg.sGateway, fnSystem.Net.get_ip4_gateway_str().c_str(), 16);
    strlcpy(cfg.sDnsIP,   fnSystem.Net.get_ip4_dns_str().c_str(),     16);
    strlcpy(cfg.sNetmask, fnSystem.Net.get_ip4_mask_str().c_str(),    16);

    sprintf(cfg.sMacAddress, "%02X:%02X:%02X:%02X:%02X:%02X", cfg.macAddress[0], cfg.macAddress[1], cfg.macAddress[2], cfg.macAddress[3], cfg.macAddress[4], cfg.macAddress[5]);
    sprintf(cfg.sBssid,      "%02X:%02X:%02X:%02X:%02X:%02X", cfg.bssid[0], cfg.bssid[1], cfg.bssid[2], cfg.bssid[3], cfg.bssid[4], cfg.bssid[5]);
    return cfg;
}

//  Make new disk and shove into device slot
void iecFuji::new_disk()
{
    // TODO: Implement when we actually have a good idea of
    // media types.
}

void iecFuji::read_host_slots_basic()
{
    Debug_println("Fuji cmd: READ HOST SLOTS");

    if (pt.size() < 2)
    {
        response = "host slot # required";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    util_remove_spaces(pt[1]);

    int selected_hs = atoi(pt[1].c_str());
    std::string hn = std::string(_fnHosts[selected_hs].get_hostname());

    response = mstr::toPETSCII2(hn.empty() ? "<empty>" : hn);
    set_fuji_iec_status(0, "ok");
}

// RAW version doesn't take a parameter for the host slot, BASIC does. I didn't make the rules
void iecFuji::read_host_slots_raw()
{
    Debug_println("Fuji cmd: READ HOST SLOTS");
    responseV.resize(MAX_HOSTS * MAX_HOSTNAME_LEN, 0);
    for (size_t i = 0; i < MAX_HOSTS; i++) {
        const char* hostname = _fnHosts[i].get_hostname();
        size_t offset = i * MAX_HOSTNAME_LEN;
        std::memcpy(&responseV[offset], hostname, std::min(std::strlen(hostname), static_cast<size_t>(MAX_HOSTNAME_LEN)));
    }
    set_fuji_iec_status(0, "");
}


// Read and save host slot data from computer, again, BASIC differs from RAW by taking parameters for which one to save.
void iecFuji::write_host_slots_basic()
{
    Debug_println("FUJI CMD: WRITE HOST SLOTS");
    if (pt.size() < 2)
    {
        response = "no host slot #";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    int hostSlot = atoi(pt[1].c_str());
    if (!_validate_host_slot(hostSlot))
    {
        response = "invalid host slot #";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    std::string hostname = (pt.size() == 3) ? pt[2] : "";

    // Debug_printf("Setting host slot %u to %s\r\n", hostSlot, hostname.c_str());
    _fnHosts[hostSlot].set_hostname(hostname.c_str());

    _populate_config_from_slots();
    Config.save();

    response = "ok";
    set_fuji_iec_status(0, response);
}

void iecFuji::write_host_slots_raw()
{
    if (payload.size() != MAX_HOSTS * MAX_HOSTNAME_LEN) {
        Debug_printv("Did not receive correct payload size, was %d, expected %d\r\n", payload.size(), MAX_HOSTS * MAX_HOSTNAME_LEN);
        set_fuji_iec_status(DEVICE_ERROR, "payload size incorrect");
        return;
    }

    for (size_t i = 0; i < MAX_HOSTS; i++)
    {
        size_t offset = i * MAX_HOSTNAME_LEN;
        char hostnameBuffer[MAX_HOSTNAME_LEN] = {0};
        std::copy(payload.begin() + offset, payload.begin() + offset + MAX_HOSTNAME_LEN - 1, hostnameBuffer);
        _fnHosts[i].set_hostname(hostnameBuffer);
    }
    _populate_config_from_slots();
    Config.save();
    set_fuji_iec_status(0, "");
}

void iecFuji::set_host_prefix()
{
    // TODO IMPLEMENT
}

void iecFuji::get_host_prefix()
{
    // TODO IMPLEMENT
}

void iecFuji::read_device_slots_basic()
{
    Debug_println("Fuji cmd: READ DEVICE SLOT");

    if (pt.size() < 2)
    {
        response = "device slot # required";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    util_remove_spaces(pt[1]);

    int selected_ds = atoi(pt[1].c_str());
    std::string filename = _fnDisks[selected_ds].filename;

    // equivalent of basename without the headaches
    size_t pos = filename.find_last_of("/\\");
    if (pos != std::string::npos) {
        filename = filename.substr(pos + 1);
    }

    response = filename;
    set_fuji_iec_status(0, "ok");
}


// RAW version doesn't take a parameter for the device slot, BASIC does. I didn't make the rules
void iecFuji::read_device_slots_raw()
{
    Debug_println("Fuji cmd: READ DEVICE SLOTS");
    responseV.reserve(MAX_DISK_DEVICES * (MAX_DISPLAY_FILENAME_LEN + 2));

    for (size_t i = 0; i < MAX_DISK_DEVICES; ++i) {
        responseV.push_back(_fnDisks[i].host_slot);
        responseV.push_back(_fnDisks[i].access_mode);
        for (size_t j = 0; j < MAX_DISPLAY_FILENAME_LEN; ++j) {
            responseV.push_back(static_cast<uint8_t>(_fnDisks[i].filename[j]));
        }
    }

    set_fuji_iec_status(0, "");
}

void iecFuji::write_device_slots_basic()
{
    Debug_println("Fuji cmd: WRITE DEVICE SLOTS");

    // from BASIC
    if (pt.size() < 4)
    {
        response = "need file mode";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }
    else if (pt.size() < 3)
    {
        response = "need filename";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }
    else if (pt.size() < 2)
    {
        response = "need host slot";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }
    else if (pt.size() < 1)
    {
        response = "need device slot";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    unsigned char ds = atoi(pt[1].c_str());
    unsigned char hs = atoi(pt[2].c_str());
    std::string filename = pt[3];
    unsigned char m = atoi(pt[4].c_str());

    _fnDisks[ds].reset(filename.c_str(),hs,m);
    strncpy(_fnDisks[ds].filename,filename.c_str(),256);

    write_device_slots();
    set_fuji_iec_status(0, "ok");
}

void iecFuji::write_device_slots_raw()
{
    union _diskSlots
    {
        struct
        {
            uint8_t hostSlot;
            uint8_t mode;
            char filename[MAX_DISPLAY_FILENAME_LEN];
        } diskSlots[MAX_DISK_DEVICES];
        char rawData[304]; // 38 * 8
    } diskSlots;

    strncpy(diskSlots.rawData, &payload.c_str()[0], 304);

    // Load the data into our current device array
    for (int i = 0; i < MAX_DISK_DEVICES; i++) {
        _fnDisks[i].reset(diskSlots.diskSlots[i].filename, diskSlots.diskSlots[i].hostSlot, diskSlots.diskSlots[i].mode);
    }
    write_device_slots();
    set_fuji_iec_status(0, "");
}

void iecFuji::write_device_slots()
{
    // it is assumed the data has been parsed at this point
    _populate_config_from_slots();
    Config.save();
}

// Temporary(?) function while we move from old config storage to new
void iecFuji::_populate_slots_from_config()
{
    // Debug_printf("_populate_slots_from_config()\r\n");
    for (int i = 0; i < MAX_HOSTS; i++)
    {
        if (Config.get_host_type(i) == fnConfig::host_types::HOSTTYPE_INVALID)
            _fnHosts[i].set_hostname("");
        else
            _fnHosts[i].set_hostname(Config.get_host_name(i).c_str());
    }

    for (int i = 0; i < MAX_DISK_DEVICES; i++)
    {
        _fnDisks[i].reset();

        if (Config.get_mount_host_slot(i) != HOST_SLOT_INVALID)
        {
            if (Config.get_mount_host_slot(i) >= 0 && Config.get_mount_host_slot(i) <= MAX_HOSTS)
            {
                strlcpy(_fnDisks[i].filename, Config.get_mount_path(i).c_str(), sizeof(fujiDisk::filename));
                _fnDisks[i].host_slot = Config.get_mount_host_slot(i);
                if (Config.get_mount_mode(i) == fnConfig::mount_modes::MOUNTMODE_WRITE)
                    _fnDisks[i].access_mode = DISK_ACCESS_MODE_WRITE;
                else
                    _fnDisks[i].access_mode = DISK_ACCESS_MODE_READ;
            }
        }
    }
}

// Temporary(?) function while we move from old config storage to new
void iecFuji::_populate_config_from_slots()
{
    for (int i = 0; i < MAX_HOSTS; i++)
    {
        fujiHostType htype = _fnHosts[i].get_type();
        const char *hname = _fnHosts[i].get_hostname();

        if (hname[0] == '\0')
        {
            Config.clear_host(i);
        }
        else
        {
            Config.store_host(
                i, 
                hname,
                htype == HOSTTYPE_TNFS ? fnConfig::host_types::HOSTTYPE_TNFS : fnConfig::host_types::HOSTTYPE_SD
            );
        }
    }

    for (int i = 0; i < MAX_DISK_DEVICES; i++)
    {
        if (_fnDisks[i].host_slot >= MAX_HOSTS || _fnDisks[i].filename[0] == '\0')
            Config.clear_mount(i);
        else
            Config.store_mount(i, _fnDisks[i].host_slot, _fnDisks[i].filename,
                               _fnDisks[i].access_mode == DISK_ACCESS_MODE_WRITE ? fnConfig::mount_modes::MOUNTMODE_WRITE : fnConfig::mount_modes::MOUNTMODE_READ);
    }
}

void iecFuji::set_device_filename_basic()
{
    if (pt.size() < 4)
    {
        Debug_printf("not enough parameters.\r\n");
        response = "error: invalid # of parameters";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    uint8_t slot = atoi(pt[0].c_str());
    uint8_t host = atoi(pt[1].c_str());
    uint8_t mode = atoi(pt[2].c_str());
    std::string filename = pt[3];

    if (!_validate_device_slot(slot, "set_device_filename_basic")) {
        response = "invalid device slot";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    if (!_validate_host_slot(host, "set_device_filename_basic")) {
        response = "invalid host slot";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    if (filename.size() > MAX_FILENAME_LEN) {
        response = "invalid filename - too long";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    Debug_printf("Fuji cmd: SET DEVICE SLOT 0x%02X/%02X/%02X FILENAME: %s\r\n", slot, host, mode, filename.c_str());

    set_device_filename(slot, host, mode, filename);
    response = "ok";
    set_fuji_iec_status(0, response);
}

void iecFuji::set_device_filename_raw()
{
    Debug_print("Fuji cmd: SET DEVICE FILENAME\r\n");

    uint8_t slot = payload[0];
    uint8_t host = payload[1];
    uint8_t mode = payload[2];
    std::string filename = payload.substr(3);

    if (!_validate_device_slot(slot, "set_device_filename_raw")) {
        set_fuji_iec_status(DEVICE_ERROR, "invalid device slot");
        return;
    }

    if (!_validate_host_slot(host, "set_device_filename_raw")) {
        set_fuji_iec_status(DEVICE_ERROR, "invalid host slot");
        return;
    }

    if (filename.size() > MAX_FILENAME_LEN) {
        set_fuji_iec_status(DEVICE_ERROR, "invalid filename - too long");
        return;
    }

    Debug_printf("Fuji cmd: SET DEVICE SLOT 0x%02X/%02X/%02X FILENAME: %s\r\n", slot, host, mode, filename.c_str());

    set_device_filename(slot, host, mode, filename);
    set_fuji_iec_status(0, "");
}

void iecFuji::set_device_filename(uint8_t slot, uint8_t host, uint8_t mode, std::string filename)
{
    std::strncpy(_fnDisks[slot].filename, filename.c_str(), sizeof(_fnDisks[slot].filename) - 1);
    _fnDisks[slot].filename[sizeof(_fnDisks[slot].filename) - 1] = '\0';

    // If the filename is empty, mark this as an invalid host, so that mounting will ignore it too
    if (filename.size() == 0) {
        _fnDisks[slot].host_slot = INVALID_HOST_SLOT;
    } else {
        _fnDisks[slot].host_slot = host;
    }
    _fnDisks[slot].access_mode = mode;
    _populate_config_from_slots();

    Config.save();
}

void iecFuji::get_device_filename_basic()
{
    Debug_println("Fuji CMD: get device filename");
    if (pt.size() < 2)
    {
        Debug_printf("Incorrect # of parameters.\r\n");
        response = "invalid # of parameters";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    uint8_t ds = atoi(pt[1].c_str());
    if (!_validate_device_slot(ds, "get_device_filename"))
    {
        Debug_printf("Invalid device slot: %u\r\n", ds);
        response = "invalid device slot";
        set_fuji_iec_status(DEVICE_ERROR, response);
        return;
    }

    response = get_device_filename(ds);
    set_fuji_iec_status(0, "ok");
}

void iecFuji::get_device_filename_raw()
{
    uint8_t ds = payload[0];
    if (!_validate_device_slot(ds, "get_device_filename"))
    {
        Debug_printf("Invalid device slot: %u\r\n", ds);
        set_fuji_iec_status(DEVICE_ERROR, "Invalid device slot");
        return;
    }

    std::string result = get_device_filename(ds);
    Debug_printv("result = >%s<\r\n", result.c_str());
    if (result == "") {
        Debug_printf("Adding zero byte to responseV\r\n");
        responseV.push_back(0);
    } else {
        responseV.assign(result.begin(), result.end());
    }
    set_fuji_iec_status(0, "");
}

std::string iecFuji::get_device_filename(uint8_t ds)
{
    char *fp = &_fnDisks[ds].filename[0];
    if (*fp == 0) {
        return std::string();
    }
    return std::string(_fnDisks[ds].filename);
}

// Mounts the desired boot disk number
void iecFuji::insert_boot_device(uint8_t d)
{
    // TODO IMPLEMENT
}


iecDrive *iecFuji::bootdisk()
{
    return &_bootDisk;
}

int iecFuji::get_disk_id(int drive_slot)
{
    return _fnDisks[drive_slot].disk_dev.id();
}

std::string iecFuji::get_host_prefix(int host_slot)
{
    return _fnHosts[host_slot].get_prefix();
}

/* @brief Tokenizes the payload command and parameters.
 Example: "COMMAND:Param1,Param2" will return a vector of [0]="COMMAND", [1]="Param1",[2]="Param2"
 Also supports "COMMAND,Param1,Param2"
*/
std::vector<std::string> iecFuji::tokenize_basic_command(std::string command)
{
    Debug_printf("Tokenizing basic command: %s\r\n", command.c_str());

    // Replace the first ":" with "," for easy tokenization. 
    // Assume it is fine to change the payload at this point.
    // Technically, "COMMAND,Param1,Param2" will work the smae, if ":" is not in a param value
    size_t endOfCommand = command.find(':');
    if (endOfCommand != std::string::npos)
        command.replace(endOfCommand,1,",");
    
    std::vector<std::string> result =  util_tokenize(command, ',');
    return result;

}


void iecFuji::net_set_ssid(bool store, net_config_t& net_config)
{
    Debug_println("Fuji cmd: SET SSID");
    std::string msg;

    int test_result = fnWiFi.test_connect(net_config.ssid, net_config.password);
    if (test_result != 0)
    {
        Debug_println("Could not connect to target SSID. Aborting save.");
        msg = "ssid not set";
    }
    else {
        // Only save these if we're asked to, otherwise assume it was a test for connectivity
        if (store) {
            fnWiFi.store_wifi(net_config.ssid, net_config.password);
        }
        msg = "ssid set";
    }

    Debug_println("Restarting WiFiManager");
    fnWiFi.start();

    // give it a few seconds to restart the WiFi before we return to the client, who may immediately start checking status if this is CONFIG
    // and get errors if we're not up yet.
    fnSystem.delay(4000);

    set_fuji_iec_status(
        // this was using NETWORK_ERROR_SUCCESS (1) for a success, but in iec_status, 0 is success, anything else is an error
        // I don't think we should use network codes for FUJI device success status, same goes for the ERROR really...
        test_result == 0 ? 0 : NETWORK_ERROR_NOT_CONNECTED,
        msg
    );
}

void iecFuji::net_scan_networks()
{
    _countScannedSSIDs = fnWiFi.scan_networks();
    // this causes the wifi to disconnect and reconnect, so we have a race condition on the results being passed back.
    // what does any good programmer do for a race condition? PLASTER IT WITH A PAUSE
    Debug_printf("Scan performed. Pausing for WiFi to reestablish\r\n");
    fnSystem.delay(5000);
}

scan_result_t iecFuji::net_scan_result(int scan_num)
{
    scan_result_t result;
    memset(&result.ssid[0], 0, sizeof(scan_result_t));
    fnWiFi.get_scan_result(scan_num, result.ssid, &result.rssi);
    Debug_printf("SSID: %s RSSI: %u\r\n", result.ssid, result.rssi);
    return result;
}

net_config_t iecFuji::net_get_ssid()
{
    net_config_t net_config;
    memset(&net_config, 0, sizeof(net_config));

    std::string s = Config.get_wifi_ssid();
    memcpy(net_config.ssid, s.c_str(), s.length() > sizeof(net_config.ssid) ? sizeof(net_config.ssid) : s.length());

    s = Config.get_wifi_passphrase();
    memcpy(net_config.password, s.c_str(), s.length() > sizeof(net_config.password) ? sizeof(net_config.password) : s.length());

    return net_config;
}


uint8_t iecFuji::net_get_wifi_status()
{
    return fnWiFi.connected() ? 3 : 6;
}

bool iecFuji::mount_host(int hs)
{
    _populate_slots_from_config();
    return _fnHosts[hs].mount();
}

bool iecFuji::disk_image_mount(uint8_t ds, uint8_t mode)
{
    char flag[3] = {'r', 0, 0};

    if (mode == DISK_ACCESS_MODE_WRITE)
        flag[1] = '+';

    if (!_validate_device_slot(ds))
    {
        response = "invalid device slot.";
        return false;
    }

    // A couple of reference variables to make things much easier to read...
    fujiDisk &disk = _fnDisks[ds];
    fujiHost &host = _fnHosts[disk.host_slot];

    Debug_printf("Selecting '%s' from host #%u as %s on D%u:\r\n",
                 disk.filename, disk.host_slot, flag, ds + 1);

    // TODO: Refactor along with mount disk image.
    disk.disk_dev.m_host = &host;

    disk.fileh = host.file_open(disk.filename, disk.filename, sizeof(disk.filename), flag);

    if (disk.fileh == nullptr)
    {
        response = "no file handle";
        return false;
    }

    // We've gotten this far, so make sure our bootable CONFIG disk is disabled
    boot_config = false;

    // We need the file size for loading XEX files and for CASSETTE, so get that too
    disk.disk_size = host.file_size(disk.fileh);

    // And now mount it
    disk.disk_type = disk.disk_dev.mount(disk.fileh, disk.filename, disk.disk_size);
    response = "mounted";
    return true;
}

bool iecFuji::open_directory(uint8_t hs, std::string dirpath, std::string pattern)
{
    if (!_validate_host_slot(hs))
    {
        response = "invalid host slot #";
        return false;
    }

    // If we already have a directory open, close it first
    if (_current_open_directory_slot != -1)
    {
        Debug_print("Directory was already open - closing it first\r\n");
        _fnHosts[_current_open_directory_slot].dir_close();
        _current_open_directory_slot = -1;
    }

    Debug_printf("Opening directory: \"%s\", pattern: \"%s\"\r\n", dirpath.c_str(), pattern.c_str());

    if (_fnHosts[hs].dir_open(dirpath.data(), pattern.empty() ? nullptr : pattern.c_str(), 0))
    {
        _current_open_directory_slot = hs;
        response = "ok";
        return true;
    }
    else
    {
        response = "unable to open directory";
        return false;
    }
}


void _set_additional_direntry_details(fsdir_entry_t *f, uint8_t *dest, uint8_t maxlen)
{
    // File modified date-time
    struct tm *modtime = localtime(&f->modified_time);

    // std::string fTime = mstr::toHex((const uint8_t *) &(f->modified_time), sizeof(time_t));
    // std::string mTime = mstr::toHex((const uint8_t *) modtime, sizeof(struct tm));

    // Debug_printf("file: '%s'\r\nfTim: [%s]\r\n mT1: [%s]\r\n", f->filename, fTime.c_str(), mTime.c_str());

    dest[0] = modtime->tm_year;          // +1900 = year
    dest[1] = modtime->tm_mon + 1;       // month is 0 indexed
    dest[2] = modtime->tm_mday;          // day of month is 1 indexed
    dest[3] = modtime->tm_hour;          // the local hour with TZ offset of device applied
    dest[4] = modtime->tm_min;
    dest[5] = modtime->tm_sec;

    // File size
    uint16_t fsize = f->size;
    dest[6] = LOBYTE_FROM_UINT16(fsize);
    dest[7] = HIBYTE_FROM_UINT16(fsize);

    dest[8] = f->isDir ? FF_DIR : 0;

    maxlen -= 10; // Adjust the max return value with the number of additional bytes we're copying
    if (f->isDir) // Also subtract a byte for a terminating slash on directories
        maxlen--;
    if (strlen(f->filename) >= maxlen)
        dest[8] |= FF_TRUNC;

    // File type
    dest[9] = MediaType::discover_mediatype(f->filename);

    // what was generated... and altered :(
    // std::string dData = mstr::toHex((const uint8_t *) dest, 10);
    // mTime = mstr::toHex((const uint8_t *) modtime, sizeof(struct tm));
    // Debug_printf(" mT2: [%s]\r\ndata: [%s]\r\n", mTime.c_str(), dData.c_str());
}

std::string iecFuji::process_directory_entry(uint8_t maxlen, uint8_t addtlopts) {
    fsdir_entry_t *f = _fnHosts[_current_open_directory_slot].dir_nextfile();
    if (f == nullptr) {
        Debug_println("Reached end of of directory");
        return std::string(2, char(0x7F));
    }

    Debug_printf("::read_direntry \"%s\"\r\n", f->filename);

    std::string entry;
    if (addtlopts & 0x80) {
        uint8_t extra[10];
        _set_additional_direntry_details(f, extra, maxlen);
        entry.append(reinterpret_cast<char*>(extra), sizeof(extra));
    }

    size_t maxFilenameSize = maxlen - entry.size() - (f->isDir ? 1 : 0); // Reserve space for '/' if directory
    std::string ellipsizedFilename = util_ellipsize_string(f->filename, maxFilenameSize);

    entry += ellipsizedFilename;
    if (f->isDir) entry += '/';

    return entry;
}

std::string iecFuji::read_directory_entry(uint8_t maxlen, uint8_t addtlopts) {
    return process_directory_entry(maxlen, addtlopts);    
}

void iecFuji::hash_input_raw()
{
    hash_input(payload);
    set_fuji_iec_status(0, "");
}

void iecFuji::hash_input(std::string input)
{
    Debug_printf("FUJI: HASH INPUT\r\n");
    hasher.add_data(input);
}

    void iecFuji::hash_compute_raw(bool clear_data)
    {
        Hash::Algorithm alg = Hash::to_algorithm(payload[0]);
        hash_compute(clear_data, alg);
        set_fuji_iec_status(0, "");
    }

    void iecFuji::hash_compute(bool clear_data, Hash::Algorithm alg)
    {
        Debug_printf("FUJI: HASH COMPUTE\r\n");
        algorithm = alg;
        hasher.compute(algorithm, clear_data);
    }

void iecFuji::hash_length_raw()
{
    uint8_t is_hex = payload[0] == 1;
    uint8_t r = hash_length(is_hex);
    responseV.push_back(r);
    set_fuji_iec_status(0, "");
}

uint8_t iecFuji::hash_length(bool is_hex)
{
    Debug_printf("FUJI: HASH LENGTH\r\n");
    return hasher.hash_length(algorithm, is_hex);
}

void iecFuji::hash_output_raw()
{
    if (payload.size() != 1) {
        std::string msg = "Input should be 1 byte, got " + std::to_string(payload.size());
        set_fuji_iec_status(DEVICE_ERROR, "Input should be 1 uint8_t.");
        return;
    }
    responseV = hash_output(payload[0] == 1);
    set_fuji_iec_status(0, "");
}

std::vector<uint8_t> iecFuji::hash_output(bool is_hex)
{
    Debug_printf("FUJI: HASH OUTPUT\r\n");
    if (is_hex) {
        std::string data = hasher.output_hex();
        return std::vector<uint8_t>(data.begin(), data.end());
    } else {
        return hasher.output_binary();
    }
}

void iecFuji::hash_clear_raw()
{
    hash_clear();
    set_fuji_iec_status(0, "");
}

void iecFuji::hash_clear()
{
    Debug_printf("FUJI: HASH CLEAR\r\n");
    hasher.clear();
}


#endif /* BUILD_IEC */
