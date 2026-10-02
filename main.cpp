#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Xinput.h>
#include <dinput.h>
#include <ViGEm/Client.h>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <string>

#pragma comment(lib, "xinput.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "ViGEmClient.lib")

namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double DEFAULT_DEADZONE = 0.15;

short angleAxis(SHORT x, SHORT y, double deadzone) {
	const double normalizedX = static_cast<double>(x) / 32767.0;
	const double normalizedY = static_cast<double>(y) / 32767.0;
	const double magnitude = std::hypot(normalizedX, normalizedY);
	if (magnitude < deadzone) {
		return 0;
	}

	const double theta = std::atan2(normalizedY, normalizedX);
	const double normalizedAngle = std::clamp(theta / PI, -1.0, 1.0);
	return static_cast<short>(normalizedAngle * 32767.0);
}

DWORD findController() {
	XINPUT_STATE state{};
	for (DWORD index = 0; index < XUSER_MAX_COUNT; ++index) {
		if (XInputGetState(index, &state) == ERROR_SUCCESS) {
			return index;
		}
	}
	return XUSER_MAX_COUNT;
}

class DirectInputController {
public:
	DirectInputController() = default;
	~DirectInputController() {
		close();
	}

	bool connect() {
		close();

		if (FAILED(DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION,
			IID_IDirectInput8, reinterpret_cast<void**>(&directInput_), nullptr))) {
			return false;
		}

		const HRESULT result = directInput_->EnumDevices(
			DI8DEVCLASS_GAMECTRL,
			[](const DIDEVICEINSTANCEW* instance, void* context) -> BOOL {
				return static_cast<DirectInputController*>(context)->tryDevice(*instance);
			},
			this,
			DIEDFL_ATTACHEDONLY);

		if (FAILED(result) || !device_) {
			close();
			return false;
		}

		std::wcout << L"DirectInput controller connected: " << name_ << L"\n";
		return true;
	}

	bool read(SHORT& leftX, SHORT& leftY, SHORT& rightX, SHORT& rightY) {
		if (!device_) {
			return false;
		}

		DIJOYSTATE2 state{};
		HRESULT result = device_->GetDeviceState(sizeof(state), &state);
		if (result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED) {
			device_->Acquire();
			result = device_->GetDeviceState(sizeof(state), &state);
		}
		if (FAILED(result)) {
			return false;
		}

		// This is the common DualShock 4 DirectInput layout used by the Sentakki wrapper:
		// X/Y are the left stick and Z/Rz are the right stick. DirectInput Y axes increase down.
		leftX = normalizeAxis(state.lX);
		leftY = normalizeAxis(65535L - state.lY);
		rightX = normalizeAxis(state.lZ);
		rightY = normalizeAxis(65535L - state.lRz);
		return true;
	}

	void close() {
		if (device_) {
			device_->Unacquire();
			device_->Release();
			device_ = nullptr;
		}
		if (directInput_) {
			directInput_->Release();
			directInput_ = nullptr;
		}
		name_.clear();
	}

private:
	static SHORT normalizeAxis(LONG value) {
		const double normalized = (static_cast<double>(value) - 32767.5) / 32767.5;
		return static_cast<SHORT>(std::clamp(normalized, -1.0, 1.0) * 32767.0);
	}

	BOOL tryDevice(const DIDEVICEINSTANCEW& instance) {
		if (FAILED(directInput_->CreateDevice(instance.guidInstance, &device_, nullptr))) {
			return DIENUM_CONTINUE;
		}
		if (FAILED(device_->SetDataFormat(&c_dfDIJoystick2)) ||
			FAILED(device_->SetCooperativeLevel(GetConsoleWindow(), DISCL_NONEXCLUSIVE | DISCL_BACKGROUND)) ||
			FAILED(device_->Acquire())) {
			device_->Release();
			device_ = nullptr;
			return DIENUM_CONTINUE;
		}

		name_ = instance.tszProductName;
		return DIENUM_STOP;
	}

	LPDIRECTINPUT8 directInput_ = nullptr;
	LPDIRECTINPUTDEVICE8 device_ = nullptr;
	std::wstring name_;
};
}

int main() {
	PVIGEM_CLIENT client = vigem_alloc();
	if (!client || !VIGEM_SUCCESS(vigem_connect(client))) {
		std::cerr << "Could not connect to ViGEmClient. Install ViGEmBus and configure the ViGEmClient SDK.\n";
		if (client) vigem_free(client);
		return 1;
	}

	PVIGEM_TARGET target = vigem_target_x360_alloc();
	if (!target || !VIGEM_SUCCESS(vigem_target_add(client, target))) {
		std::cerr << "Could not create the virtual Xbox controller.\n";
		if (target) vigem_target_free(target);
		vigem_disconnect(client);
		vigem_free(client);
		return 1;
	}

	std::cout << "Virtual Xbox controller ready. Bind USC knobs to virtual Lx and Rx.\n";
	std::cout << "Press Ctrl+C to stop.\n";

	constexpr double deadzone = DEFAULT_DEADZONE;
	DWORD controllerIndex = XUSER_MAX_COUNT;
	DirectInputController directInput;
	bool useDirectInput = false;
	while (true) {
		if (!useDirectInput && controllerIndex == XUSER_MAX_COUNT) {
			controllerIndex = findController();
			if (controllerIndex == XUSER_MAX_COUNT) {
				if (directInput.connect()) {
					useDirectInput = true;
				} else {
					std::cerr << "No XInput or DirectInput controller found. Retrying...\n";
					Sleep(1000);
					continue;
				}
			}
			if (!useDirectInput) {
				std::cout << "Physical XInput controller connected: " << controllerIndex << "\n";
			}
		}

		XUSB_REPORT report{};
		if (useDirectInput) {
			SHORT leftX = 0;
			SHORT leftY = 0;
			SHORT rightX = 0;
			SHORT rightY = 0;
			if (!directInput.read(leftX, leftY, rightX, rightY)) {
				std::cerr << "DirectInput controller disconnected. Retrying...\n";
				directInput.close();
				useDirectInput = false;
				controllerIndex = XUSER_MAX_COUNT;
				continue;
			}
			report.sThumbLX = angleAxis(leftX, leftY, deadzone);
			report.sThumbRX = angleAxis(rightX, rightY, deadzone);
		} else {
			XINPUT_STATE state{};
			if (XInputGetState(controllerIndex, &state) != ERROR_SUCCESS) {
				std::cerr << "Physical XInput controller disconnected.\n";
				controllerIndex = XUSER_MAX_COUNT;
				continue;
			}
			report.sThumbLX = angleAxis(state.Gamepad.sThumbLX, state.Gamepad.sThumbLY, deadzone);
			report.sThumbRX = angleAxis(state.Gamepad.sThumbRX, state.Gamepad.sThumbRY, deadzone);
			report.wButtons = state.Gamepad.wButtons;
			report.bLeftTrigger = state.Gamepad.bLeftTrigger;
			report.bRightTrigger = state.Gamepad.bRightTrigger;
		}
		vigem_target_x360_update(client, target, report);
		Sleep(1);
	}

	vigem_target_remove(client, target);
	vigem_target_free(target);
	vigem_disconnect(client);
	vigem_free(client);
	return 0;
}
