#ifndef USBPD_H
#define USBPD_H

#include "ch32fun.h"
#include "funconfig.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef PACKED
#define PACKED __attribute__((packed))
#endif

// USB PD spec type definitions
typedef enum PACKED
{
  eUSBPD_PORTDATAROLE_UFP = 0, // Upstream Facing Port
  eUSBPD_PORTDATAROLE_DFP = 1, // Downstream Facing Port
} USBPD_PortDataRole_e;
typedef enum PACKED
{
  eUSBPD_PORTPOWEROLE_SINK = 0,   // Sink Power Role
  eUSBPD_PORTPOWEROLE_SOURCE = 1, // Source Power Role
} USBPD_PortPowerRole_e;

typedef enum PACKED
{
  eUSBPD_REV_10 = 0x00u, // Revision 1.0
  eUSBPD_REV_20 = 0x01u, // Revision 2.0
  eUSBPD_REV_30 = 0x02u, // Revision 3.0
} USBPD_SpecificationRevision_e;

typedef enum PACKED
{
  eUSBPD_CTRL_MSG_GOODCRC = 0x01u,
  eUSBPD_CTRL_MSG_GOTOMIN = 0x02u, // Depracated
  eUSBPD_CTRL_MSG_ACCEPT = 0x03u,
  eUSBPD_CTRL_MSG_REJECT = 0x04u,
  eUSBPD_CTRL_MSG_PING = 0x05u, // Deprecated
  eUSBPD_CTRL_MSG_PS_RDY = 0x06u,
  eUSBPD_CTRL_MSG_GET_SOURCE_CAP = 0x07u,
  eUSBPD_CTRL_MSG_GET_SINK_CAP = 0x08u,
  eUSBPD_CTRL_MSG_DR_SWAP = 0x09u,
  eUSBPD_CTRL_MSG_PR_SWAP = 0x0Au,
  eUSBPD_CTRL_MSG_VCONN_SWAP = 0x0Bu,
  eUSBPD_CTRL_MSG_WAIT = 0x0Cu,
  eUSBPD_CTRL_MSG_SOFT_RESET = 0x0Du,
  eUSBPD_CTRL_MSG_DATA_RESET = 0x0Eu,
  eUSBPD_CTRL_MSG_DATA_RESET_COMPLETE = 0x0Fu,
  eUSBPD_CTRL_MSG_NOT_SUPPORTED = 0x10u,
  eUSBPD_CTRL_MSG_GET_SOURCE_CAPEXT = 0x11u,
  eUSBPD_CTRL_MSG_GET_STATUS = 0x12u,
  eUSBPD_CTRL_MSG_FR_SWAP = 0x13u,
  eUSBPD_CTRL_MSG_GET_PPS_STATUS = 0x14u,
  eUSBPD_CTRL_MSG_GET_COUNTRY_CODES = 0x15u,
  eUSBPD_CTRL_MSG_GET_SINK_CAPEXT = 0x16u,
  eUSBPD_CTRL_MSG_GET_SOURCE_INFO = 0x17u,
  eUSBPD_CTRL_MSG_GET_REVISION = 0x18u,
} USBPD_ControlMessage_e;

typedef enum PACKED
{
  eUSBPD_DATA_MSG_SOURCE_CAP = 0x01u,
  eUSBPD_DATA_MSG_REQUEST = 0x02u,
  eUSBPD_DATA_MSG_BIST = 0x03u,
  eUSBPD_DATA_MSG_SINK_CAP = 0x04u,
  eUSBPD_DATA_MSG_BATTERY_STATUS = 0x05u,
  eUSBPD_DATA_MSG_ALERT = 0x06u,
  eUSBPD_DATA_MSG_GET_COUNTRY_INFO = 0x07u,
  eUSBPD_DATA_MSG_ENTER_USB = 0x08u,
  eUSBPD_DATA_MSG_EPR_REUEST = 0x09u,
  eUSBPD_DATA_MSG_EPR_MODE = 0x0Au,
  eUSBPD_DATA_MSG_SOURCE_INFO = 0x0Bu,
  eUSBPD_DATA_MSG_REVISION = 0x0Cu,
  eUSBPD_DATA_MSG_VENDOR_DEFINED = 0x0Fu
} USBPD_DataMessage_e;

typedef union
{
  uint16_t data;
  struct
  {
    uint16_t MessageType : 5u; // USBPD_ControlMessage_t | USBPD_DataMessage_t
    USBPD_PortDataRole_e PortDataRole : 1u;
    USBPD_SpecificationRevision_e SpecificationRevision : 2u;
    USBPD_PortPowerRole_e PortPowerRole : 1u;
    uint16_t MessageID : 3u;
    uint16_t NumberOfDataObjects : 3u; // 0: Control Message, >0: Data Message
    uint16_t Extended : 1u;
  };
} USBPD_MessageHeader_t;

typedef USBPD_MessageHeader_t USBPD_ControlMessage_t;
static_assert(sizeof(USBPD_MessageHeader_t) == sizeof(uint16_t), "USBPD_MessageHeader_t size mismatch");

typedef enum PACKED
{
  eUSBPD_PDO_FIXED = 0,
  eUSBPD_PDO_BATTERY = 1,
  eUSBPD_PDO_VARIABLE = 2,
  eUSBPD_PDO_AUGMENTED = 3,
} USBPD_PowerDataObject_e;

typedef enum PACKED
{
  eUSBPD_APDO_SPR_PPS = 0, // Standard Power Range Programmable Power Supply
  eUSBPD_APDO_EPR_AVS = 1, // Extended Power Range Adjustable Voltage Supply
  eUSBPD_APDO_SPR_AVS = 2, // Standard Power Range Adjustable Voltage Supply
  eUSBPD_APDO_RESERVED = 3,
} USBPD_AugmentedPDO_e;

typedef enum PACKED
{
  eUSBPD_PEAK_CURRENT_0 = 0, /* Peak Current equals IoC */

  eUSBPD_PEAK_CURRENT_1 = 1, /* 150% IoC for 1ms @ 5% duty cycle
                  125% IoC for 2ms @ 10% duty cycle
                  110% IoC for 10ms @ 50% duty cycle */

  eUSBPD_PEAK_CURRENT_2 = 2, /* 200% IoC for 1ms @ 5% duty cycle
                  150% IoC for 2ms @ 10% duty cycle
                  125% IoC for 10ms @ 50% duty cycle */

  eUSBPD_PEAK_CURRENT_3 = 3, /* 200% IoC for 1ms @ 5% duty cycle
                  175% IoC for 2ms @ 10% duty cycle
                  150% IoC for 10ms @ 50% duty cycle */
} USBPD_PeakCurrent_e;

typedef struct
{
  uint32_t data : 28u;                     // PDO specific data based on PDO type
  USBPD_AugmentedPDO_e AugmentedType : 2u; // shall be eUSBPD_APDO_SPR_PPS
  USBPD_PowerDataObject_e PDOType : 2u;    // shall be eUSBPD_PDO_AUGMENTED
} USBPD_PDOHeader_t;

typedef struct
{
  uint32_t MaxCurrentIn10mA : 10u;
  uint32_t VoltageIn50mV : 10u;
  USBPD_PeakCurrent_e PeakCurrent : 2u;
  uint32_t Reserved_22bit : 1u;
  uint32_t EPRModeCapable : 1u;
  uint32_t UnchunkedExtendedMessage : 1u;
  uint32_t DualRoleData : 1u;
  uint32_t USBCommunicationsCapable : 1u;
  uint32_t UnconstrainedPower : 1u;
  uint32_t USBSuspendSupported : 1u;
  uint32_t DualRolePower : 1u;
  USBPD_PowerDataObject_e PDOType : 2u; // shall be eUSBPD_PDO_FIXED
} USBPD_SourceFixedSupplyPDO_t;

#define FIXED_SUPPLY_FMT               \
  "\nFixed Supply:\n"                  \
  "\tMax Current: %d mA\n"             \
  "\tVoltage: %d mV\n"                 \
  "\tPeak Current: %d\n"               \
  "\tEPR Mode Capable: %s\n"           \
  "\tUnchunked Extended Message: %s\n" \
  "\tDual Role Data: %s\n"             \
  "\tUSB Communications Capable: %s\n" \
  "\tUnconstrained Power: %s\n"        \
  "\tUSB Suspend Supported: %s\n"      \
  "\tDual Role Power: %s\n"

#define FIXED_SUPPLY_FMT_ARGS(pdo)                                                          \
  ((pdo)->FixedSupply.MaxCurrentIn10mA * 10), ((pdo)->FixedSupply.VoltageIn50mV * 50),      \
      ((pdo)->FixedSupply.PeakCurrent), ((pdo)->FixedSupply.EPRModeCapable ? "Yes" : "No"), \
      ((pdo)->FixedSupply.UnchunkedExtendedMessage ? "Yes" : "No"),                         \
      ((pdo)->FixedSupply.DualRoleData ? "Yes" : "No"),                                     \
      ((pdo)->FixedSupply.USBCommunicationsCapable ? "Yes" : "No"),                         \
      ((pdo)->FixedSupply.UnconstrainedPower ? "Yes" : "No"),                               \
      ((pdo)->FixedSupply.USBSuspendSupported ? "Yes" : "No"),                              \
      ((pdo)->FixedSupply.DualRolePower ? "Yes" : "No")

typedef struct
{
  uint32_t MaxCurrentIn10mA : 10u;
  uint32_t MinVoltageIn50mV : 10u;
  uint32_t MaxVoltageIn50mV : 10u;
  USBPD_PowerDataObject_e PDOType : 2u; // shall be eUSBPD_PDO_VARIABLE
} USBPD_VariablePDO_t;

#define VARIABLE_SUPPLY_FMT \
  "\nVariable Supply:\n"    \
  "\tMax Current: %d mA\n"  \
  "\tMin Voltage: %d mV\n"  \
  "\tMax Voltage: %d mV\n"

#define VARIABLE_SUPPLY_FMT_ARGS(pdo)                                                           \
  ((pdo)->VariableSupply.MaxCurrentIn10mA * 10), ((pdo)->VariableSupply.MinVoltageIn50mV * 50), \
      ((pdo)->VariableSupply.MaxVoltageIn50mV * 50)

typedef struct
{
  uint32_t MaxPowerIn250mW : 10u;
  uint32_t MinVoltageIn50mV : 10u;
  uint32_t MaxVoltageIn50mV : 10u;
  USBPD_PowerDataObject_e PDOType : 2u; // shall be eUSBPD_PDO_BATTERY
} USBPD_BatteryPDO_t;

#define BATTERY_SUPPLY_FMT \
  "\nBattery Supply:\n"    \
  "\tMax Power: %d mW\n"   \
  "\tMin Voltage: %d mV\n" \
  "\tMax Voltage: %d mV\n"

#define BATTERY_SUPPLY_FMT_ARGS(pdo)                                                          \
  ((pdo)->BatterySupply.MaxPowerIn250mW * 250), ((pdo)->BatterySupply.MinVoltageIn50mV * 50), \
      ((pdo)->BatterySupply.MaxVoltageIn50mV * 50)

typedef struct
{
  uint32_t MaxCurrentIn50mA : 7u;
  uint32_t Reserved_7bit : 1u; // shall be set to zero
  uint32_t MinVoltageIn100mV : 8u;
  uint32_t Reserved_16bit : 1u; // shall be set to zero
  uint32_t MaxVoltageIn100mV : 8u;
  uint32_t Reserved_25_26bit : 2u; // shall be set to zero
  uint32_t PPSpowerLimited : 1u;
  USBPD_AugmentedPDO_e AugmentedType : 2u; // shall be eUSBPD_APDO_SPR_PPS
  USBPD_PowerDataObject_e PDOType : 2u;    // shall be eUSBPD_PDO_AUGMENTED
} USBPD_SPR_PPS_APDO_t;

#define SPR_PPS_FMT        \
  "\nPPS Supply:\n"        \
  "\tMax Current: %d mA\n" \
  "\tMin Voltage: %d mV\n" \
  "\tMax Voltage: %d mV\n" \
  "\tPPS Power Limited: %s\n"

#define SPR_PPS_FMT_ARGS(pdo)                                                       \
  ((pdo)->SPR_PPS.MaxCurrentIn50mA * 50), ((pdo)->SPR_PPS.MinVoltageIn100mV * 100), \
      ((pdo)->SPR_PPS.MaxVoltageIn100mV * 100), ((pdo)->SPR_PPS.PPSpowerLimited ? "Yes" : "No")

typedef struct
{
  uint32_t PDPIn1W : 8u;
  uint32_t MinVoltageIn100mV : 8u;
  uint32_t Reserved_16bit : 1u; // shall be set to zero
  uint32_t MaxVoltageIn100mV : 9u;
  USBPD_PeakCurrent_e PeakCurrent : 2u;

  USBPD_AugmentedPDO_e AugmentedType : 2u; // shall be eUSBPD_APDO_EPR_AVS
  USBPD_PowerDataObject_e PDOType : 2u;    // shall be eUSBPD_PDO_AUGMENTED
} USBPD_EPR_AVS_APDO_t;

#define EPR_AVS_FMT        \
  "\nEPR AVS Supply:\n"    \
  "\tPDP: %d W\n"          \
  "\tMin Voltage: %d mV\n" \
  "\tMax Voltage: %d mV\n" \
  "\tPeak Current: %d\n"

#define EPR_AVS_FMT_ARGS(pdo)                                         \
  ((pdo)->EPR_AVS.PDPIn1W), ((pdo)->EPR_AVS.MinVoltageIn100mV * 100), \
      ((pdo)->EPR_AVS.MaxVoltageIn100mV * 100), ((pdo)->EPR_AVS.PeakCurrent)

typedef struct
{
  uint32_t MaxCurrent15To20VIn10mA : 10u;
  uint32_t MaxCurrent9to15VIn10mA : 10u;
  uint32_t Reserved_20_25bit : 6u; // shall be set to zero
  USBPD_PeakCurrent_e PeakCurrent : 2u;
  USBPD_AugmentedPDO_e AugmentedType : 2u; // shall be eUSBPD_APDO_SPR_AVS
  USBPD_PowerDataObject_e PDOType : 2u;    // shall be eUSBPD_PDO_AUGMENTED
} USBPD_SPR_AVS_APDO_t;

#define SPR_AVS_FMT               \
  "\nSPR AVS Supply:\n"           \
  "\tMax Current 15-20V: %d mA\n" \
  "\tMax Current 9-15V: %d mA\n"  \
  "\tPeak Current: %d\n"

#define SPR_AVS_FMT_ARGS(pdo)                                                                  \
  ((pdo)->SPR_AVS.MaxCurrent15To20VIn10mA * 10), ((pdo)->SPR_AVS.MaxCurrent9to15VIn10mA * 10), \
      ((pdo)->SPR_AVS.PeakCurrent)

typedef union
{
  USBPD_PDOHeader_t Header;
  USBPD_SourceFixedSupplyPDO_t FixedSupply;
  USBPD_VariablePDO_t VariableSupply;
  USBPD_BatteryPDO_t BatterySupply;
  USBPD_SPR_PPS_APDO_t SPR_PPS;
  USBPD_EPR_AVS_APDO_t EPR_AVS;
  USBPD_SPR_AVS_APDO_t SPR_AVS;
} USBPD_SourcePDO_t;

static_assert(sizeof(USBPD_SourcePDO_t) == sizeof(uint32_t), "USBPD_SourcePDO_t size mismatch");

typedef enum // Do not PACK, messes up alignment
{
  eUSBPD_FAST_ROLE_SWAP_NOT_SUPPORTED = 0,
  eUSBPD_FAST_ROLE_SWAP_DEFAULT = 1,
  eUSBPD_FAST_ROLE_SWAP_1A5 = 2, // 1.5A @ 5V
  eUSBPD_FAST_ROLE_SWAP_3A = 3,  // 3A @ 5V
} USBPD_FastRoleSwapRequiredCurrent_e;

typedef struct
{
  uint32_t CurrentIn10mA : 10u;
  uint32_t VoltageIn50mV : 10u;
  uint32_t Reserved_20_22bit : 3u; // shall be set to zero
  USBPD_FastRoleSwapRequiredCurrent_e FastRoleSwap : 2u;
  uint32_t DualRoleData : 1u;
  uint32_t USBComsCapable : 1u;
  uint32_t UnconstrainedPower : 1u;
  uint32_t HigherCapability : 1u;
  uint32_t DualRolePower : 1u;
  USBPD_PowerDataObject_e PDOType : 2u; // shall be eUSBPD_PDO_FIXED
} USBPD_SinkFixedSupplyPDO_t;

typedef union
{
  USBPD_PDOHeader_t Header;
  USBPD_SinkFixedSupplyPDO_t FixedSupply;
  USBPD_VariablePDO_t VariableSupply;
  USBPD_BatteryPDO_t BatterySupply;
  USBPD_SPR_PPS_APDO_t SPR_PPS;
  USBPD_EPR_AVS_APDO_t EPR_AVS;
  USBPD_SPR_AVS_APDO_t SPR_AVS;
} USBPD_SinkPDO_t;

static_assert(sizeof(USBPD_SourcePDO_t) == sizeof(uint32_t), "USBPD_SourcePDO_t size mismatch");
static_assert(sizeof(USBPD_SinkPDO_t) == sizeof(uint32_t), "USBPD_SinkPDO_t size mismatch");

typedef union
{
  USBPD_SourcePDO_t Source[7];
  USBPD_SinkPDO_t Sink[7];
} USBPD_SPR_CapabilitiesMessage_t;

static_assert(sizeof(USBPD_SPR_CapabilitiesMessage_t) == (7 * sizeof(uint32_t)),
              "USBPD_SPR_CapabilitiesMessage_t size mismatch");

typedef struct
{
  uint32_t MaxCurrentIn10mA : 10u;
  uint32_t OperatingCurrentIn10mA : 10u;
  uint32_t Reserved_20_21bit : 2u; // shall be set to zero
  uint32_t ERPCapable : 1u;
  uint32_t UnchunkedExtendedMessage : 1u;
  uint32_t NoUSBSuspended : 1u;
  uint32_t USBComsCapable : 1u;
  uint32_t CapabilityMissmatch : 1u;
  uint32_t Giveback : 1u;       // Deprecated, shall be set to zero
  uint32_t ObjectPosition : 4u; // Reserved and shall not be used
} USBPD_FixedAndVariableRDO_t;

typedef struct
{
  uint32_t MaxPowerIn250mW : 10u;
  uint32_t OperatingPowerIn250mW : 10u;
  uint32_t Reserved_20_21bit : 2u; // shall be set to zero
  uint32_t ERPCapable : 1u;
  uint32_t UnchunkedExtendedMessage : 1u;
  uint32_t NoUSBSuspended : 1u;
  uint32_t USBComsCapable : 1u;
  uint32_t CapabilityMissmatch : 1u;
  uint32_t Giveback : 1u;       // Deprecated, shall be set to zero
  uint32_t ObjectPosition : 4u; // Reserved and shall not be used
} USBPD_BatteryRDO_t;

typedef struct
{
  uint32_t OperatingCurrentIn50mA : 7u;
  uint32_t Reserved_7_8bit : 2u; // shall be set to zero
  uint32_t OutputVoltageIn20mV : 12u;
  uint32_t Reserved_21bit : 1u; // shall be set to zero
  uint32_t ERPCapable : 1u;
  uint32_t UnchunkedExtendedMessage : 1u;
  uint32_t NoUSBSuspended : 1u;
  uint32_t USBComsCapable : 1u;
  uint32_t CapabilityMissmatch : 1u;
  uint32_t Reserved_27bit : 1u; // Deprecated, shall be set to zero
  uint32_t ObjectPosition : 4u; // Reserved and shall not be used
} USBPD_PPS_RDO_t;

typedef struct
{
  uint32_t OperatingCurrentIn50mA : 7u;
  uint32_t Reserved_7_8bit : 2u;       // shall be set to zero
  uint32_t OutputVoltageIn100mV : 12u; // NOTE: Output voltage in 25mV units, the least two significant bits Shall
                                       // be set to zero making the effective voltage step size 100mV.
  uint32_t Reserved_21bit : 1u;        // shall be set to zero
  uint32_t ERPCapable : 1u;
  uint32_t UnchunkedExtendedMessage : 1u;
  uint32_t NoUSBSuspended : 1u;
  uint32_t USBComsCapable : 1u;
  uint32_t CapabilityMissmatch : 1u;
  uint32_t Reserved_27bit : 1u; // Deprecated, shall be set to zero
  uint32_t ObjectPosition : 4u; // Reserved and shall not be used
} USBPD_AVS_RDO_t;

typedef union
{
  uint32_t data;
  USBPD_FixedAndVariableRDO_t FixedAndVariable;
  USBPD_BatteryRDO_t Battery;
  USBPD_PPS_RDO_t PPS;
} USBPD_RequestDataObject_t;

static_assert(sizeof(USBPD_RequestDataObject_t) == sizeof(uint32_t), "USBPD_RequestDataObject_t size mismatch");

// TODO: Define the rest of the message types sections 6.4.3 -> 6.5.16

typedef enum
{
  eUSBPD_MAX_EXTENDED_MSG_LEN = 260,
  eUSBPD_MAX_EXTENDED_MSG_CHUNK_LEN = 26,
  eUSBPD_MAX_EXTENDED_MSG_LEGACY_LEN = 26,
} USBPD_ValueParameters_t;

typedef enum
{
  eUSBPD_OK = 0,
  eUSBPD_BUSY,
  eUSBPD_ERROR,
  eUSBPD_ERROR_ARGS,
  eUSBPD_ERROR_NOT_SUPPORTED,
  eUSBPD_ERROR_TIMEOUT,
} USBPD_Result_e;

typedef enum
{
  eUSBPD_VCC_3V3 = 0,
  eUSBPD_VCC_5V0 = 1,
} USBPD_VCC_e;

typedef enum
{
  eUSBPD_CCNONE = 0,
  eUSBPD_CC1 = 1,
  eUSBPD_CC2 = 2,
} USBPD_CC_e;

typedef enum
{
  eSTATE_IDLE,
  eSTATE_CABLE_DETECT,
  eSTATE_SOURCE_CAP,
  eSTATE_WAIT_ACCEPT,
  eSTATE_WAIT_PS_RDY,
  eSTATE_PS_RDY,
  eSTATE_MAX,
} USBPD_State_e;

/**
 * @brief  Initialize the USB PD module
 * @param  vcc: VCC voltage level (3.3V or 5V)
 * @return USBPD_Result_e
 */
USBPD_Result_e USBPD_Init(USBPD_VCC_e vcc);

/**
 * @brief  Negotiate with the USB PD Source, must be called periodically
 * @param  None
 * @return USBPD_BUSY if negotiation is in progress, eUSBPD_OK if successful, or an error code
 */
USBPD_Result_e USBPD_SinkNegotiate(void);

/**
 * @brief  Reset the USB PD module
 * @param  None
 * @return None
 */
void USBPD_Reset(void);

/**
 * @brief  Get the current state of the USB PD module
 * @param  None
 * @return USBPD_State_e representing the current state of the module
 */
USBPD_State_e USBPD_GetState(void);

/**
 * @brief  Convert USB PD state to string
 * @param state: USBPD_State_e to convert
 * @return Pointer to a string representing the state
 */
const char *USBPD_StateToStr(USBPD_State_e state);

/**
 * @brief  Convert USB PD result to string
 * @param  result: USBPD_Result_e to convert
 * @return Pointer to a string representing the result
 */
const char *USBPD_ResultToStr(USBPD_Result_e result);

/**
 * @brief  Select a new Power Data Object (PDO) to be used. No re-negotiation is needed.
 * @param  index: Index of the PDO to select (0-based)
 * @param  voltageIn100mV: Desired output voltage in 100mV units (e.g., 500 for 5V) (only applicable for PPS)
 * @return USBPD_Result_e
 */
USBPD_Result_e USBPD_SelectPDO(uint8_t index, uint32_t voltageIn100mV);

/**
 * @brief  Get the capabilities of the USB PD Source
 * @param[out] capabilities: Pointer to a pointer where the capabilities message structure is stored
 * @return Number of Power Data Objects (PDOs) in the capabilities message
 */
size_t USBPD_GetCapabilities(USBPD_SPR_CapabilitiesMessage_t **capabilities);

/**
 * @brief  Check if the Power Data Object is a Programmable Power Supply (PPS)
 * @param[in] pdo: Pointer to the Power Data Object to check
 * @return true if the PDO is a PPS, false otherwise
 */
bool USBPD_IsPPS(const USBPD_SourcePDO_t *pdo);

/**
 * @brief  Get the USB PD Specification Revision
 * @param  None
 * @return USBPD_SpecificationRevision_e representing the USB PD specification revision
 */
USBPD_SpecificationRevision_e USBPD_GetVersion(void);

#endif // USBPD_H