#include <map>
#include "PMDG_NGX_SDK.h"
#include "SimConnect.h"
#include "Radio.h"
#include "Joystick.h"

enum DATA_REQUEST_ID {
	DATA_REQUEST,
	CONTROL_REQUEST,
	RADIO_REQUEST,
};

using namespace std;

class NgxInterface {
private:
	PMDG_NGX_Control control;
	HANDLE hSimConnect = NULL;
	HANDLE pollForDataThread;
	map<unsigned int, unsigned int> currValues;
public:
	int connected;
	PMDG_NGX_Data data;
	FSX_Radio_Data radio;

	NgxInterface() {};
	~NgxInterface() {};

	void connect();
	void pollForData();

	HRESULT send(unsigned int eventId, unsigned int parameter) {
		if (hSimConnect == NULL) return -1;

		control.Event = eventId;
		control.Parameter = parameter;
		HRESULT hr = SimConnect_SetClientData(hSimConnect, PMDG_NGX_CONTROL_ID,	PMDG_NGX_CONTROL_DEFINITION, 0, 0, sizeof(PMDG_NGX_Control), &control);

		if (FAILED(hr)) {
			printf("NgxInterface.send: %x, %x failed with error code %d\n", eventId, parameter, hr);
		} else {
			printf("NgxInterface.send: %x, %x succeeded\n", eventId, parameter);
		}

		return hr;
	}

	HRESULT send(unsigned int eventId, unsigned int parameter, unsigned int currValue) {
		if (parameter != currValue) return send(eventId, parameter);
		return 0;
	}

	void pressButton(unsigned int eventId, unsigned int parameter) {
		auto item = currValues.find(eventId);
		if (item != currValues.end() && item->second == parameter) return;

		auto hr = send(eventId, parameter ? MOUSE_FLAG_LEFTSINGLE : MOUSE_FLAG_LEFTRELEASE);
		if (!hr) currValues[eventId] = parameter;
	}

	void adjust(EVENT_ID eventId, char value) {
		int flag = value > 0 ? MOUSE_FLAG_WHEEL_UP : MOUSE_FLAG_WHEEL_DOWN;
		for (unsigned char i = 0; i < min(10, abs(value*value)); i++)
			SimConnect_TransmitClientEvent(hSimConnect, 0, eventId, flag, SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
	}

	void setData(SIMCONNECT_RECV_CLIENT_DATA *pData) {
		if (pData->dwRequestID == DATA_REQUEST) {
			auto pS = (PMDG_NGX_Data*)&pData->dwData;
			data = *pS;
		} /*else if (pObjData->dwRequestID == CONTROL_REQUEST) {
			// keep the present state of Control area to know if the server had received and reset the command
			PMDG_NGX_Control *pS = (PMDG_NGX_Control*)&pObjData->dwData;
			printf("Received control: %d %d\n", pS->Event, pS->Parameter);
		}*/
	}
};

static void HandleSimEvent(NgxInterface* ngx, SIMCONNECT_RECV_EVENT* evt) {
	switch (evt->uEventID) {
		case EVENT_INPUT_SW1:
			if (evt->dwData) {
				ngx->send(EVT_CONTROL_STAND_ENG1_START_LEVER, 0);
			}
			break;

		case EVENT_INPUT_SW2:
			if (evt->dwData) {
				ngx->send(EVT_CONTROL_STAND_ENG1_START_LEVER, 1);
			}
			break;

		case EVENT_INPUT_SW3:
			if (evt->dwData) {
				ngx->send(EVT_CONTROL_STAND_ENG2_START_LEVER, 0);
			}
			break;

		case EVENT_INPUT_SW4:
			if (evt->dwData) {
				ngx->send(EVT_CONTROL_STAND_ENG2_START_LEVER, 1);
			}
			break;

		case EVENT_INPUT_SW5:
			ngx->send(EVT_FIRE_DETECTION_TEST_SWITCH, evt->dwData ? 2 : 1);
			break;

		case EVENT_INPUT_SW6:
			ngx->send(EVT_FIRE_DETECTION_TEST_SWITCH, evt->dwData ? 0 : 1);
			break;

		case EVENT_INPUT_MODE_SW:
			ngx->send(EVT_CONTROL_STAND_PARK_BRAKE_LEVER, evt->dwData);
			break;

		case EVENT_INPUT_FLAPS_UP:
			if (evt->dwData) {
				ngx->send(EVT_CONTROL_STAND_FLAPS_LEVER_0, MOUSE_FLAG_LEFTSINGLE);
			}
			break;

		case EVENT_INPUT_FLAPS_1:
			if (evt->dwData) {
				ngx->send(EVT_CONTROL_STAND_FLAPS_LEVER_1, MOUSE_FLAG_LEFTSINGLE);
			}
			break;

		case EVENT_INPUT_FLAPS_2:
			if (evt->dwData) {
				ngx->send(EVT_CONTROL_STAND_FLAPS_LEVER_2, MOUSE_FLAG_LEFTSINGLE);
			}
			break;

		case EVENT_INPUT_FLAPS_5:
			if (evt->dwData) {
				ngx->send(EVT_CONTROL_STAND_FLAPS_LEVER_5, MOUSE_FLAG_LEFTSINGLE);
			}
			break;

		case EVENT_INPUT_FLAPS_10:
			if (evt->dwData) {
				ngx->send(EVT_CONTROL_STAND_FLAPS_LEVER_10, MOUSE_FLAG_LEFTSINGLE);
			}
			break;

		case EVENT_INPUT_FLAPS_15:
			if (evt->dwData) {
				ngx->send(EVT_CONTROL_STAND_FLAPS_LEVER_15, MOUSE_FLAG_LEFTSINGLE);
			}
			break;

		case EVENT_INPUT_FLAPS_25:
			if (evt->dwData) {
				ngx->send(EVT_CONTROL_STAND_FLAPS_LEVER_25, MOUSE_FLAG_LEFTSINGLE);
			}
			break;

		case EVENT_INPUT_FLAPS_30:
			if (evt->dwData) {
				ngx->send(EVT_CONTROL_STAND_FLAPS_LEVER_30, MOUSE_FLAG_LEFTSINGLE);
			}
			break;
	}
}

static void CALLBACK MyDispatchProc(SIMCONNECT_RECV* pData, DWORD cbData, void *pContext) {
	auto ngx = (NgxInterface*)pContext;

	switch (pData->dwID) {
	case SIMCONNECT_RECV_ID_EXCEPTION: {
		SIMCONNECT_RECV_EXCEPTION * except = (SIMCONNECT_RECV_EXCEPTION*)pData;
		printf("***** EXCEPTION=%d  SendID=%d  Index=%d  cbData=%d\n", except->dwException, except->dwSendID, except->dwIndex, cbData);
		break;
	}

	case SIMCONNECT_RECV_ID_OPEN:
		printf("MyDispatchProc.Received: SIMCONNECT_RECV_ID_OPEN\n");
		break;

	case SIMCONNECT_RECV_ID_QUIT:
		printf("MyDispatchProc.Received: SIMCONNECT_RECV_ID_QUIT\n");
		ngx->connected = 0;
		break;

	case SIMCONNECT_RECV_ID_EVENT:
		HandleSimEvent(ngx, (SIMCONNECT_RECV_EVENT*)pData);
		break;

	case SIMCONNECT_RECV_ID_CLIENT_DATA: {
		ngx->setData((SIMCONNECT_RECV_CLIENT_DATA*)pData);
		break;
	}

	case SIMCONNECT_RECV_ID_SIMOBJECT_DATA_BYTYPE: {
		SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE *pObjData = (SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE*)pData;

		if (pObjData->dwRequestID == RADIO_REQUEST) {
			DWORD ObjectID = pObjData->dwObjectID;
			RadioInterface::setData(&ngx->radio, (FSX_Radio_Data*)&pObjData->dwData);
		}
		break;
	}

	default:
		printf("MyDispatchProc.Received:%d\n", pData->dwID);
		break;
	}

	fflush(stdout);
}

DWORD WINAPI PollForData(LPVOID lpParam) {
	auto ngx = (NgxInterface*)lpParam;
	ngx->pollForData();
	return 0;
}

void NgxInterface::connect() {
	HRESULT hr = SimConnect_Open(&hSimConnect, "PMDGWrapper", NULL, 0, 0, 0);

	if (FAILED(hr)) return;

	printf("Connected to Flight Simulator\n");
	Joystick_MapEvents(hSimConnect);
	RadioInterface::connect(hSimConnect);

	// Associate an ID with the PMDG data area name
	SimConnect_MapClientDataNameToID(hSimConnect, PMDG_NGX_DATA_NAME, PMDG_NGX_DATA_ID);

	// Define the data area structure - this is a required step
	SimConnect_AddToClientDataDefinition(hSimConnect, PMDG_NGX_DATA_DEFINITION, 0, sizeof(PMDG_NGX_Data), 0, 0);

	// Sign up for notification of data change.
	SimConnect_RequestClientData(hSimConnect, PMDG_NGX_DATA_ID, DATA_REQUEST, PMDG_NGX_DATA_DEFINITION, SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET, SIMCONNECT_CLIENT_DATA_REQUEST_FLAG_CHANGED, 0, 0, 0);

	// Associate an ID with the PMDG control area name
	SimConnect_MapClientDataNameToID(hSimConnect, PMDG_NGX_CONTROL_NAME, PMDG_NGX_CONTROL_ID);

	// Define the control area structure - this is a required step
	SimConnect_AddToClientDataDefinition(hSimConnect, PMDG_NGX_CONTROL_DEFINITION, 0, sizeof(PMDG_NGX_Control), 0, 0);

	// Sign up for notification of control change
	SimConnect_RequestClientData(hSimConnect, PMDG_NGX_CONTROL_ID, CONTROL_REQUEST, PMDG_NGX_CONTROL_DEFINITION, SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET, SIMCONNECT_CLIENT_DATA_REQUEST_FLAG_CHANGED, 0, 0, 0);

	// Map client events to PMDG events (THIRD_PARTY_EVENT_ID_MIN = 0x11000 = 69632)
	SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_COURSE_SELECTOR_L, "#70008");
	SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_SPEED_SELECTOR, "#70016");
	SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_HEADING_SELECTOR, "#70022");
	SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_ALTITUDE_SELECTOR, "#70032");
	SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_VERTSPEED_SELECTOR, "#70033");
	SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_COURSE_SELECTOR_R, "#70041");
	SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_BARO_SELECTOR_L, "#69997");
	SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_MINS_SELECTOR_L, "#69987");

	connected = 1;
	pollForDataThread = CreateThread(NULL, 0, PollForData, this, 0, NULL);
}

void NgxInterface::pollForData() {
	while (connected) {
		SimConnect_CallDispatch(hSimConnect, MyDispatchProc, this);
		//SimConnect_RequestDataOnSimObjectType(hSimConnect, RADIO_REQUEST, RADIO_DEF, 0, SIMCONNECT_SIMOBJECT_TYPE_USER);
		Sleep(10);
	}
}