#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Xinput.h>
#include <ViGEm/Client.h>
#include <cmath>
#include <iostream>
#include <algorithm>

#pragma comment(lib, "xinput.lib")
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
}

int main() {
	const DWORD controllerIndex = findController();
	if (controllerIndex == XUSER_MAX_COUNT) {
		std::cerr << "No XInput controller found. Make sure DS4Windows exposes the controller through XInput.\n";
		return 1;
	}

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

	std::cout << "Physical XInput controller: " << controllerIndex << "\n";
	std::cout << "Virtual Xbox controller ready. Bind USC knobs to virtual Lx and Rx.\n";
	std::cout << "Press Ctrl+C to stop.\n";

	constexpr double deadzone = DEFAULT_DEADZONE;
	while (true) {
		XINPUT_STATE state{};
		if (XInputGetState(controllerIndex, &state) != ERROR_SUCCESS) {
			std::cerr << "Physical controller disconnected.\n";
			break;
		}

		XUSB_REPORT report{};
		report.sThumbLX = angleAxis(state.Gamepad.sThumbLX, state.Gamepad.sThumbLY, deadzone);
		report.sThumbRX = angleAxis(state.Gamepad.sThumbRX, state.Gamepad.sThumbRY, deadzone);
		report.wButtons = state.Gamepad.wButtons;
		report.bLeftTrigger = state.Gamepad.bLeftTrigger;
		report.bRightTrigger = state.Gamepad.bRightTrigger;
		vigem_target_x360_update(client, target, report);
		Sleep(1);
	}

	vigem_target_remove(client, target);
	vigem_target_free(target);
	vigem_disconnect(client);
	vigem_free(client);
	return 0;
}
