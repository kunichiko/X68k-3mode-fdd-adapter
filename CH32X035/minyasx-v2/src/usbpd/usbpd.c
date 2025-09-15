/*	USB PD Library
 *	2025-06-21 Bogdan Ionescu
 *	Configuration:
 *		- USBPD_IMPLEMENTATION: Enable USB PD implementation
 *		- FUNCONF_USBPD_NO_STR: Disable string conversion functions
 *	Notes:
 *		- This library is based on the USB Power Delivery Specification.
 *			https://www.usb.org/document-library/usb-power-delivery
 *		- Packed bitfield structs are used for de/serialization of USB PD messages and
 *			are taken directly from the spec above.
 *		- Not all messages are implemented.
 *		- Formatting macros are provided next to the struct deffinitions.
 *	Basic usage:
 *		USBPD_VCC_e vcc = eUSBPD_VCC_5V0; // set the VCC voltage
 *		USBPD_Result_e result = USBPD_Init( vcc ); // initialize the peripheral
 *
 *		// wait for negotiation to complete.
 *		while ( eUSBPD_BUSY == ( result = USBPD_SinkNegotiate() ) );
 *
 *		USBPD_SPR_CapabilitiesMessage_t *capabilities;
 *		const size_t count = USBPD_GetCapabilities( &capabilities );
 *		USBPD_SelectPDO( count - 1, voltage ); // select the last supply (voltage is only used for PPS)
 *
 *	The above is not a complete example, check the funtion declarations below for more details.
 */

#include "usbpd/usbpd.h"

#include <string.h>

#include "oled/ssd1306_txt.h"

typedef struct
{
	uint32_t ccCount;
	volatile USBPD_State_e state;
	USBPD_SpecificationRevision_e pdVersion;
	USBPD_CC_e lastCCLine;
	USBPD_SPR_CapabilitiesMessage_t caps;
	// uint8_t messageID;
	uint8_t pdoCount;
	bool gotSourceGoodCRC;
	volatile bool sinkGoodCRCOver;
	uint8_t tx_msgid;	   // 送信用 MessageIDCounter (0..7)
	bool rx_have_last;	   // 直前の受信IDを保持しているか
	uint8_t rx_last_msgid; // 直前に受理した相手の MessageID (0..7)
} USBPD_Instance_t;

static __attribute__((aligned(4))) uint8_t s_buffer[34];
static USBPD_Instance_t s_instance = {
	.pdVersion = eUSBPD_REV_30,
};

static USBPD_CC_e GetActiveCCLine(void);
static void SwitchRXMode(void);
static void SendMessage(uint8_t size);
static void ParsePacket(void);

USBPD_Result_e USBPD_Init(USBPD_VCC_e vcc)
{
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO;
	RCC->AHBPCENR |= RCC_USBPD;

	GPIOC->CFGHR &= ~(0xf << ((14 & 7) << 2));
	GPIOC->CFGHR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_OD) << ((14 & 7) << 2);
	GPIOC->CFGHR &= ~(0xf << ((15 & 7) << 2));
	GPIOC->CFGHR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_OD) << ((15 & 7) << 2);

	AFIO->CTLR |= USBPD_IN_HVT;
	if (vcc == eUSBPD_VCC_3V3)
	{
		AFIO->CTLR |= USBPD_PHY_V33;
	}

	USBPD->DMA = (uint32_t)s_buffer;
	USBPD->CONFIG = IE_RX_ACT | IE_RX_RESET | IE_TX_END | PD_DMA_EN | PD_FILT_EN;
	USBPD->STATUS = BUF_ERR | IF_RX_BIT | IF_RX_BYTE | IF_RX_ACT | IF_RX_RESET | IF_TX_END;

	// disable CC comparators
	//	USBPD->PORT_CC1 &= ~(CC_CMP_MASK | PA_CC_AI);
	//	USBPD->PORT_CC2 &= ~(CC_CMP_MASK | PA_CC_AI);
	// set CC comparator voltage
	//	USBPD->PORT_CC1 |= CC_CMP_66;
	//	USBPD->PORT_CC2 |= CC_CMP_66;

	USBPD->PORT_CC1 = CC_CMP_66 | CC_PD;
	USBPD->PORT_CC2 = CC_CMP_66 | CC_PD;

	return eUSBPD_OK;
}

USBPD_Result_e USBPD_SinkNegotiate(void)
{
	switch (s_instance.state)
	{
	case eSTATE_IDLE:;
		const uint8_t ccLine = GetActiveCCLine();
		if (ccLine == eUSBPD_CCNONE)
		{
			s_instance.ccCount = 0;
			s_instance.lastCCLine = eUSBPD_CCNONE;
			break;
		}

		if (s_instance.lastCCLine != ccLine)
		{
			s_instance.lastCCLine = ccLine;
			s_instance.ccCount = 0;
		}
		else
		{
			s_instance.ccCount++;
		}

		if (s_instance.ccCount > 10)
		{
			if (ccLine == eUSBPD_CC2)
			{
				USBPD->CONFIG |= CC_SEL;
			}
			else
			{
				USBPD->CONFIG &= ~CC_SEL;
			}

			s_instance.ccCount = 0;
			s_instance.state = eSTATE_CABLE_DETECT;

			SwitchRXMode();
			NVIC_SetPriority(USBPD_IRQn, 0x00); // TODO: Is this needed?
			NVIC_EnableIRQ(USBPD_IRQn);
		}
		break;

	case eSTATE_SOURCE_CAP:
		// 直前に返した GoodCRC の送信完了（TX_END → ISR で set）を待つ
		if (!s_instance.sinkGoodCRCOver)
		{
			break; // まだ送信中。次回のポーリングで再評価
		}
		// NVIC_DisableIRQ(USBPD_IRQn);
		// OLED_print("USBPD_SelectPDO\n");
		// OLED_printf("PDO count: %d\n", s_instance.pdoCount);
		// Delay_Ms(1000);
		USBPD_SelectPDO(0, 0); // Select the first PDO by default
		s_instance.state = eSTATE_WAIT_ACCEPT;
		// NVIC_EnableIRQ(USBPD_IRQn);
		break;

	case eSTATE_PS_RDY:
		return eUSBPD_OK;

	default:
		break;
	}

	return eUSBPD_BUSY;
}

void USBPD_Reset(void)
{
	NVIC_DisableIRQ(USBPD_IRQn);
	s_instance = (USBPD_Instance_t){
		.pdVersion = eUSBPD_REV_30,
	};
}

USBPD_State_e USBPD_GetState(void)
{
	return s_instance.state;
}

#if FUNCONF_USBPD_NO_STR
const char *USBPD_StateToStr(USBPD_State_e state)
{
	(void)state;
	return "";
}

const char *USBPD_ResultToStr(USBPD_Result_e result)
{
	(void)result;
	return "";
}
#else
const char *USBPD_StateToStr(USBPD_State_e state)
{
	switch (state)
	{
	case eSTATE_IDLE:
		return "Idle";
	case eSTATE_CABLE_DETECT:
		return "Cable Detected";
	case eSTATE_SOURCE_CAP:
		return "Got Source Capabilities";
	case eSTATE_WAIT_ACCEPT:
		return "Waiting for Accept";
	case eSTATE_WAIT_PS_RDY:
		return "Waiting for PS_Ready";
	case eSTATE_PS_RDY:
		return "Power Supply Ready";
	default:
		return "Unknown State";
	}
};

const char *USBPD_ResultToStr(USBPD_Result_e result)
{
	switch (result)
	{
	case eUSBPD_OK:
		return "OK";
	case eUSBPD_BUSY:
		return "Busy";
	case eUSBPD_ERROR:
		return "Error";
	case eUSBPD_ERROR_ARGS:
		return "Error Args";
	case eUSBPD_ERROR_NOT_SUPPORTED:
		return "Error Not Supported";
	case eUSBPD_ERROR_TIMEOUT:
		return "Error Timeout";
	default:
		return "Unknown Result";
	}
}
#endif // FUNCONF_USBPD_NO_STR

USBPD_Result_e USBPD_SelectPDO(uint8_t index, uint32_t voltageIn100mV)
{
	if (index >= s_instance.pdoCount)
	{
		return eUSBPD_ERROR_ARGS;
	}

	const USBPD_SourcePDO_t *const pdo = &s_instance.caps.Source[index];

	USBPD_MessageHeader_t header;
	header.data = 0u; // union の全ビットを 0 に初期化
	header.MessageID = (s_instance.tx_msgid & 0x7);
	header.MessageType = eUSBPD_DATA_MSG_REQUEST;
	header.NumberOfDataObjects = 1u;
	header.SpecificationRevision = s_instance.pdVersion;
	header.PortPowerRole = eUSBPD_PORTPOWEROLE_SINK;
	header.PortDataRole = eUSBPD_PORTDATAROLE_UFP;

	*(USBPD_MessageHeader_t *)&s_buffer[0] = header;
	// USBPD_RequestDataObject_t *const rdo = (USBPD_RequestDataObject_t *)&s_buffer[sizeof(USBPD_MessageHeader_t)];
	USBPD_RequestDataObject_t *const rdo = (USBPD_RequestDataObject_t *)&s_buffer[2];

	if (USBPD_IsPPS(pdo))
	{
		// Clamp voltage to min/max NOTE: Maybe we should return an error if the voltage is out of range?
		const uint32_t minVoltage = pdo->SPR_PPS.MinVoltageIn100mV;
		const uint32_t maxVoltage = pdo->SPR_PPS.MaxVoltageIn100mV;
		voltageIn100mV = voltageIn100mV > maxVoltage ? maxVoltage : voltageIn100mV;
		voltageIn100mV = voltageIn100mV < minVoltage ? minVoltage : voltageIn100mV;

		*rdo = (USBPD_RequestDataObject_t){
			.PPS =
				{
					.ObjectPosition = index + 1,
					.OutputVoltageIn20mV = voltageIn100mV * 5,
					.OperatingCurrentIn50mA = pdo->SPR_PPS.MaxCurrentIn50mA,
					.NoUSBSuspended = 1u,
					.USBComsCapable = 1u, // TODO: Should have these are arguments or define
				},
		};
	}
	else
	{
		const uint16_t want_mA = 500; // 試験用に 500mA など
		const uint16_t want_10mA = want_mA / 10;
		(*rdo).data = 0u;
		*rdo = (USBPD_RequestDataObject_t){
			.FixedAndVariable =
				{
					.ObjectPosition = index + 1,
					.MaxCurrentIn10mA = pdo->FixedSupply.MaxCurrentIn10mA,
					.OperatingCurrentIn10mA = pdo->FixedSupply.MaxCurrentIn10mA,
					/*					.OperatingCurrentIn10mA = (want_10mA < pdo->FixedSupply.MaxCurrentIn10mA)
																	  ? want_10mA
																	  : pdo->FixedSupply.MaxCurrentIn10mA,*/
					.USBComsCapable = 1u,
					.NoUSBSuspended = 1u,
				},
		};
	}

	SendMessage(6);

	return eUSBPD_OK;
}

/**
 * @brief  Get the capabilities of the USB PD Source
 * @param[out] capabilities: pointer to the capabilities message structure
 * @return Number of Power Data Objects (PDOs) in the capabilities message
 */
size_t USBPD_GetCapabilities(USBPD_SPR_CapabilitiesMessage_t **capabilities)
{
	if (s_instance.pdoCount == 0)
	{
		return 0;
	}

	if (capabilities)
	{
		*capabilities = &s_instance.caps;
	}

	return s_instance.pdoCount;
}

bool USBPD_IsPPS(const USBPD_SourcePDO_t *pdo)
{
	return (pdo->Header.PDOType == eUSBPD_PDO_AUGMENTED) && (pdo->Header.AugmentedType == eUSBPD_APDO_SPR_PPS);
}

USBPD_SpecificationRevision_e USBPD_GetVersion(void)
{
	return s_instance.pdVersion;
}

/**
 * @brief  Check CC line status
 * @param  None
 * @return USBPD_CC_t
 */
static USBPD_CC_e GetActiveCCLine(void)
{
	// Switch to CC1
	USBPD->CONFIG &= ~CC_SEL;
	Delay_Us(1);
	// check if CC1 is connected
	if (USBPD->PORT_CC1 & PA_CC_AI)
	{
		return eUSBPD_CC1;
	}

	// Switch to CC2
	USBPD->CONFIG |= CC_SEL;
	Delay_Us(1);
	if (USBPD->PORT_CC2 & PA_CC_AI)
	{
		return eUSBPD_CC2;
	}

	return eUSBPD_CCNONE;
}

/**
 * @brief  Switch to RX mode
 * @param  None
 * @return None
 */
static void SwitchRXMode(void)
{
	USBPD->BMC_CLK_CNT = UPD_TMR_RX;
	USBPD->CONTROL = (USBPD->CONTROL & ~PD_TX_EN) | BMC_START;
}

/**
 * @brief  Begin transmission of PD message
 * @param  size: size of the message in bytes
 * @return None
 */
static void SendMessage(uint8_t size)
{
	// OLED_print("TX kick\n");
	//  Drive amplitude: enable LVE on the active CC line
	if (USBPD->CONFIG & CC_SEL)
	{
		USBPD->PORT_CC2 |= CC_LVE; // 例: ヘッダ定義により USBPD_CC_LVE / CC_LVE を使い分け
	}
	else
	{
		USBPD->PORT_CC1 |= CC_LVE;
	}
	// 1) 古い TX_END を明示クリア（重要）
	// USBPD->STATUS |= IF_TX_END;

	// 2) TX 用のタイミングとフレーム長を先にセット
	USBPD->BMC_CLK_CNT = UPD_TMR_TX;
	USBPD->TX_SEL = UPD_SOP0;
	USBPD->BMC_TX_SZ = size;
	USBPD->STATUS = 0; // クリア

	// 3) BMC をいったん停止してから…
	//	USBPD->CONTROL &= ~(BMC_START | PD_TX_EN);
	//	Delay_Us(1); // 最低 1us 程度（環境に合わせて）

	// 4) TX 有効化→BMC_START 立ち上げ（0→1のエッジを作る）
	USBPD->CONTROL |= PD_TX_EN | BMC_START;
}

void PD_memcpy(uint8_t *dest, const uint8_t *src, uint8_t n)
{
	while (n--)
		*dest++ = *src++;
}

/**
 * @brief  Parse the received packet
 * @param  None
 * @return None
 */
static void ParsePacket(void)
{
	bool sendGoodCRC = true;
	USBPD_MessageHeader_t message = *(USBPD_MessageHeader_t *)s_buffer;

	USBPD_State_e nextState = s_instance.state;

	// 重複検出＋受信IDの保存---
	/*	const bool is_ctrl = (message.NumberOfDataObjects == 0u);
		const bool is_goodcrc = is_ctrl && ((USBPD_ControlMessage_e)message.MessageType == eUSBPD_CTRL_MSG_GOODCRC);

		bool drop = false; // 重複なら true にして内容処理をスキップ（GoodCRC は返す）
		if (!is_goodcrc)
		{
			if (s_instance.rx_have_last && message.MessageID == s_instance.rx_last_msgid)
			{
				drop = true; // ← 重複フレーム。GoodCRC だけ返して内容処理は捨てる
			}
			else
			{
				s_instance.rx_last_msgid = message.MessageID; // ★ここが質問の2行
				s_instance.rx_have_last = true;				  // ★ここが質問の2行
			}
		}
		// ---*/

	if (message.Extended == 0u)
	{
		if (message.NumberOfDataObjects == 0u)
		{
			//			if (!drop)
			{
				switch ((USBPD_ControlMessage_e)message.MessageType)
				{
				case eUSBPD_CTRL_MSG_GOODCRC:
					// 受け取った GoodCRC の MessageID が自分の tx_msgid と一致するか確認
					if (message.MessageID == (s_instance.tx_msgid & 0x7))
					{
						s_instance.tx_msgid = (s_instance.tx_msgid + 1) & 0x7; // ← ここでのみ進める
					}
					// 不一致なら再送側に回すのが正道だが、まずは何もしない（再送未実装なら据え置き）
					sendGoodCRC = false;
					s_instance.sinkGoodCRCOver = true;
					break;

				case eUSBPD_CTRL_MSG_ACCEPT:
					nextState = eSTATE_WAIT_PS_RDY;
					break;

				case eUSBPD_CTRL_MSG_REJECT:
					nextState = eSTATE_SOURCE_CAP;
					break;

				case eUSBPD_CTRL_MSG_PS_RDY:
					nextState = eSTATE_PS_RDY;
					break;

				default:
					break;
				}
			}
		}
		else
		{
			//			if (!drop)
			{
				switch ((USBPD_DataMessage_e)message.MessageType)
				{

				case eUSBPD_DATA_MSG_SOURCE_CAP:
					nextState = eSTATE_SOURCE_CAP;
					s_instance.pdoCount = message.NumberOfDataObjects;
					s_instance.pdVersion = message.SpecificationRevision;
					// memcpy(&s_instance.caps, &s_buffer[2], sizeof(USBPD_SPR_CapabilitiesMessage_t));
					PD_memcpy((uint8_t *)&s_instance.caps, &s_buffer[2], 28);
					break;

				default:
					break;
				}
			}
		}
	}

	if (message.Extended || sendGoodCRC)
	{
		Delay_Us(30);
		//  Delay_Ms(1000); // 試験用に長めに待つ
		USBPD_ControlMessage_t reply;
		reply.data = 0u;
		reply.MessageID = message.MessageID; // 受け取った MessageID をそのまま返す
		reply.MessageType = eUSBPD_CTRL_MSG_GOODCRC;
		reply.SpecificationRevision = s_instance.pdVersion;
		reply.PortPowerRole = eUSBPD_PORTPOWEROLE_SINK;
		reply.PortDataRole = eUSBPD_PORTDATAROLE_UFP;

		*(uint16_t *)&s_buffer[0] = reply.data;
		s_instance.sinkGoodCRCOver = false;
		SendMessage(sizeof(reply));
	}

	s_instance.state = nextState;
}

void USBPD_IRQHandler(void) __attribute__((interrupt));
void USBPD_IRQHandler(void)
{
	// Receive complete interrupt
	if (USBPD->STATUS & IF_RX_ACT)
	{
		// Check if we received a SOP0 packet
		if (((USBPD->STATUS & BMC_AUX_MASK) == BMC_AUX_SOP0) && (USBPD->BMC_BYTE_CNT >= 6))
		{
			ParsePacket();
			//			OLED_print("Parse Done\n");
		}
		USBPD->STATUS |= IF_RX_ACT;
	}

	// Transmit complete interrupt (GoodCRC only)
	if (USBPD->STATUS & IF_TX_END)
	{
		// OLED_print("TX_END\n");
		//  Drop LVE on both CC pins (safe)
		USBPD->PORT_CC1 &= ~CC_LVE; // ヘッダによっては USBPD_CC_LVE
		USBPD->PORT_CC2 &= ~CC_LVE;

		// GoodCRC送信完了
		s_instance.sinkGoodCRCOver = true;
		SwitchRXMode();
		USBPD->STATUS |= IF_TX_END;
	}

	// Reset interrupt
	if (USBPD->STATUS & IF_RX_RESET)
	{
		USBPD->STATUS |= IF_RX_RESET;
	}
}
