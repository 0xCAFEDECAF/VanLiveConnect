
#ifndef VanIden_h
#define VanIden_h

#include <VanBusRx.h>

// VAN IDENtifiers
#define VIN_IDEN 0xE24
#define ENGINE_IDEN 0x8A4
#define HEAD_UNIT_STALK_IDEN 0x9C4
#define LIGHTS_STATUS_IDEN 0x4FC
#define DEVICE_REPORT 0x8C4
#define CAR_STATUS1_IDEN 0x564
#define CAR_STATUS2_IDEN 0x524
#define DASHBOARD_IDEN 0x824
#define DASHBOARD_BUTTONS_IDEN 0x664
#define HEAD_UNIT_IDEN 0x554
#define MFD_LANGUAGE_UNITS_IDEN 0x984
#define AUDIO_SETTINGS_IDEN 0x4D4
#define MFD_STATUS_IDEN 0x5E4
#define AIRCON1_IDEN 0x464
#define AIRCON2_IDEN 0x4DC
#define CDCHANGER_IDEN 0x4EC
#define SATNAV_STATUS_1_IDEN 0x54E
#define SATNAV_STATUS_2_IDEN 0x7CE
#define SATNAV_STATUS_3_IDEN 0x8CE
#define SATNAV_GUIDANCE_DATA_IDEN 0x9CE
#define SATNAV_GUIDANCE_IDEN 0x64E
#define SATNAV_REPORT_IDEN 0x6CE
#define MFD_TO_SATNAV_IDEN 0x94E
#define SATNAV_TO_MFD_IDEN 0x74E
#define SATNAV_DOWNLOADING_IDEN 0x6F4
#define SATNAV_DOWNLOADED1_IDEN 0xA44
#define SATNAV_DOWNLOADED2_IDEN 0xAC4
#define WHEEL_SPEED_IDEN 0x744
#define ODOMETER_IDEN 0x8FC
#define COM2000_IDEN 0x450
#define CDCHANGER_COMMAND_IDEN 0x8EC
#define MFD_TO_HEAD_UNIT_IDEN 0x8D4
#define AIR_CONDITIONER_DIAG_IDEN 0xADC
#define AIR_CONDITIONER_DIAG_COMMAND_IDEN 0xA5C
#define ECU_IDEN 0xB0E

inline bool IsSatnavPacket(const TVanPacketRxDesc& pkt)
{
    return
        pkt.DataLen() >= 3 && 
        (
            (pkt.Iden() == DEVICE_REPORT && pkt.Data()[0] == 0x07)
            || pkt.Iden() == SATNAV_STATUS_1_IDEN
            || pkt.Iden() == SATNAV_GUIDANCE_IDEN
            || pkt.Iden() == SATNAV_REPORT_IDEN
            || pkt.Iden() == SATNAV_TO_MFD_IDEN
        );
} // IsSatnavPacket

#endif // VanIden_h
