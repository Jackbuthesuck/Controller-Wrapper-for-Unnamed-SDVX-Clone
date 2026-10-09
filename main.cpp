#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Xinput.h>
#include <dinput.h>
#include <ViGEm/Client.h>
#include <cmath>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <conio.h>
#include <string>
#include <vector>

#pragma comment(lib, "xinput.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "ViGEmClient.lib")

namespace {
constexpr double PI = 3.14159265358979323846;
// A stick's angle is undefined near its center. Use a larger controller
// deadzone, with hysteresis, so center noise cannot become knob movement.
constexpr double CONTROLLER_DEADZONE = 0.35;
constexpr double CONTROLLER_DEADZONE_RELEASE = 0.27;
constexpr double OUTER_RING_ENTER = 0.80;
constexpr double OUTER_RING_EXIT = 0.70;
constexpr double CENTER_ROTATION_SPEED = 2.0 * PI * 2.0;
struct AngleTracker {
	bool initialized = false;
	bool active = false;
	bool outerMode = false;
	double referenceAngle = 0.0;
	double phase = 0.0;
	double lastAxis = 0.0;
};

double wrapAngle(double angle) {
	while (angle > PI) angle -= 2.0 * PI;
	while (angle < -PI) angle += 2.0 * PI;
	return angle;
}

short hybridAngleAxis(SHORT x, SHORT y, AngleTracker& tracker,
	std::chrono::steady_clock::duration elapsed) {
	const double normalizedX = static_cast<double>(x) / 32767.0;
	const double normalizedY = static_cast<double>(y) / 32767.0;
	const double magnitude = std::hypot(normalizedX, normalizedY);
	if ((!tracker.active && magnitude < CONTROLLER_DEADZONE) ||
		(tracker.active && magnitude < CONTROLLER_DEADZONE_RELEASE)) {
		tracker.active = false;
		tracker.outerMode = false;
		return static_cast<short>(tracker.lastAxis * 32767.0);
	}
	tracker.active = true;

	const double currentAngle = std::atan2(normalizedY, normalizedX);
	if (!tracker.initialized) {
		tracker.referenceAngle = currentAngle;
		tracker.initialized = true;
		return 0;
	}

	const double elapsedSeconds = std::clamp(
		std::chrono::duration<double>(elapsed).count(), 0.0, 0.1);

	if (!tracker.outerMode && magnitude >= OUTER_RING_ENTER) {
		// Re-anchor the polar reference so entering the outer ring preserves
		// the current phase instead of snapping to a new angle.
		tracker.referenceAngle = currentAngle + tracker.phase;
		tracker.outerMode = true;
	}

	if (tracker.outerMode) {
		if (magnitude < OUTER_RING_EXIT) {
			tracker.outerMode = false;
		} else {
			// atan2() increases counter-clockwise. Negate the phase so
			// clockwise rotation is positive for USC.
			tracker.phase = wrapAngle(-(currentAngle - tracker.referenceAngle));
		}
	}

	if (!tracker.outerMode) {
		// In the inner region, use the stick's horizontal displacement as a
		// spring-centered rotation-speed control.
		const double normalizedX = static_cast<double>(x) / 32767.0;
		tracker.phase = wrapAngle(
			tracker.phase + normalizedX * CENTER_ROTATION_SPEED * elapsedSeconds);
	}

	tracker.lastAxis = std::clamp(tracker.phase / PI, -1.0, 1.0);
	return static_cast<short>(tracker.lastAxis * 32767.0);
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

DWORD selectXInputController() {
	std::vector<DWORD> controllers;
	XINPUT_STATE state{};
	for (DWORD index = 0; index < XUSER_MAX_COUNT; ++index) {
		if (XInputGetState(index, &state) == ERROR_SUCCESS) {
			controllers.push_back(index);
		}
	}

	if (controllers.empty()) return XUSER_MAX_COUNT;
	if (controllers.size() == 1) {
		std::cout << "Auto-selecting XInput controller " << (controllers[0] + 1) << "\n";
		return controllers[0];
	}

	std::cout << "\n=== XINPUT CONTROLLER SELECTION ===\n";
	for (size_t i = 0; i < controllers.size(); ++i) {
		std::cout << "[" << (i + 1) << "] XInput controller "
			<< (controllers[i] + 1) << "\n";
	}
	std::cout << "Select controller (1-" << controllers.size() << "): " << std::flush;

	while (true) {
		const int key = _getch();
		if (key >= '1' && key <= '9') {
			const size_t selection = static_cast<size_t>(key - '1');
			if (selection < controllers.size()) {
				std::cout << key << "\n";
				return controllers[selection];
			}
		}
	}
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
	DWORD controllerIndex = selectXInputController();

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

	DirectInputController directInput;
	AngleTracker leftAngle;
	AngleTracker rightAngle;
	auto previousPoll = std::chrono::steady_clock::now();
	auto previousDiagnostic = std::chrono::steady_clock::now();
	bool useDirectInput = false;
	while (true) {
		const auto pollNow = std::chrono::steady_clock::now();
		const auto elapsed = pollNow - previousPoll;
		previousPoll = pollNow;
		if (!useDirectInput && controllerIndex == XUSER_MAX_COUNT) {
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

		SHORT leftX = 0;
		SHORT leftY = 0;
		SHORT rightX = 0;
		SHORT rightY = 0;
		XUSB_REPORT report{};
		if (useDirectInput) {
			if (!directInput.read(leftX, leftY, rightX, rightY)) {
				std::cerr << "DirectInput controller disconnected. Retrying...\n";
				directInput.close();
				useDirectInput = false;
				controllerIndex = XUSER_MAX_COUNT;
				continue;
			}
			report.sThumbLX = hybridAngleAxis(leftX, leftY, leftAngle, elapsed);
			report.sThumbRX = hybridAngleAxis(rightX, rightY, rightAngle, elapsed);
		} else {
			XINPUT_STATE state{};
			if (XInputGetState(controllerIndex, &state) != ERROR_SUCCESS) {
				std::cerr << "Physical XInput controller disconnected.\n";
				controllerIndex = XUSER_MAX_COUNT;
				continue;
			}
			leftX = state.Gamepad.sThumbLX;
			leftY = state.Gamepad.sThumbLY;
			rightX = state.Gamepad.sThumbRX;
			rightY = state.Gamepad.sThumbRY;
			report.sThumbLX = hybridAngleAxis(state.Gamepad.sThumbLX, state.Gamepad.sThumbLY, leftAngle, elapsed);
			report.sThumbRX = hybridAngleAxis(state.Gamepad.sThumbRX, state.Gamepad.sThumbRY, rightAngle, elapsed);
			report.wButtons = state.Gamepad.wButtons;
			report.bLeftTrigger = state.Gamepad.bLeftTrigger;
			report.bRightTrigger = state.Gamepad.bRightTrigger;
		}
		vigem_target_x360_update(client, target, report);

		if (std::chrono::duration<double>(pollNow - previousDiagnostic).count() >= 0.25) {
			const auto magnitude = [](SHORT x, SHORT y) {
				return std::hypot(static_cast<double>(x) / 32767.0,
					static_cast<double>(y) / 32767.0);
			};
			const auto angle = [](SHORT x, SHORT y) {
				return std::atan2(static_cast<double>(y), static_cast<double>(x));
			};
			std::cout << std::fixed << std::setprecision(3)
				<< "[diag] " << (useDirectInput ? "DInput" : "XInput")
				<< " L raw=(" << leftX << "," << leftY << ")"
				<< " mag=" << magnitude(leftX, leftY)
				<< " angle=" << angle(leftX, leftY)
				<< " -> Lx=" << report.sThumbLX
				<< " | R raw=(" << rightX << "," << rightY << ")"
				<< " mag=" << magnitude(rightX, rightY)
				<< " angle=" << angle(rightX, rightY)
				<< " -> Rx=" << report.sThumbRX << '\n';
			previousDiagnostic = pollNow;
		}
		Sleep(1);
	}

	vigem_target_remove(client, target);
	vigem_target_free(target);
	vigem_disconnect(client);
	vigem_free(client);
	return 0;
}
