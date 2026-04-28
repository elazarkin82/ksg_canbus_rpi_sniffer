#include "utils/LedControllerUtil.hpp"
#include <stdio.h>
#include <unistd.h> // For sleep
#include <string.h> // For strncpy
#include <thread>   // For std::this_thread::sleep_for
#include <chrono>   // For std::chrono::seconds

// Define LED names for Raspberry Pi
#define RPI_LED_ACT "ACT"
#define RPI_LED_PWR "PWR"

int main()
{
    // Declare variables at the beginning of the block
    utils::LedControllerUtil& ledController = utils::LedControllerUtil::getInstance();
    int result;

    printf("--- LedControllerUtil Tester ---\n");

    // 1. Take Control
    printf("Step 1: Taking control of ACT and PWR LEDs...\n");
    result = ledController.takeControl(RPI_LED_ACT);
    if (result < 0) {
        fprintf(stderr, "Failed to take control of ACT LED. Exiting.\n");
        return 1;
    }
    result = ledController.takeControl(RPI_LED_PWR);
    if (result < 0) {
        fprintf(stderr, "Failed to take control of PWR LED. Exiting.\n");
        return 1;
    }
    printf("   (LEDs should now be off or static)\n");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 2. Solid ON
    printf("Step 2: Turning ACT and PWR LEDs ON for 2 seconds...\n");
    ledController.turnOn(RPI_LED_ACT);
    ledController.turnOn(RPI_LED_PWR);
    printf("   (Both LEDs should be solid ON)\n");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 3. Solid OFF
    printf("Step 3: Turning ACT and PWR LEDs OFF for 2 seconds...\n");
    ledController.turnOff(RPI_LED_ACT);
    ledController.turnOff(RPI_LED_PWR);
    printf("   (Both LEDs should be solid OFF)\n");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 4. Timer Test (different rates)
    printf("Step 4: ACT fast blink (100/100ms), PWR slow blink (1000/1000ms) for 5 seconds...\n");
    ledController.setTimer(RPI_LED_ACT, 100, 100);
    ledController.setTimer(RPI_LED_PWR, 1000, 1000);
    printf("   (ACT should blink fast, PWR should blink slow)\n");
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 5. Error Simulation (short ON, long OFF)
    printf("Step 5: Simulating error state (200ms ON / 1500ms OFF) for 5 seconds...\n");
    ledController.setTimer(RPI_LED_ACT, 200, 1500);
    ledController.setTimer(RPI_LED_PWR, 200, 1500);
    printf("   (Both LEDs should blink with short ON, long OFF)\n");
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 6. Restore to default triggers
    printf("Step 6: Restoring ACT and PWR LEDs to default triggers...\n");
    // For RPi, ACT is typically mmc0 (SD card activity), PWR is default-on
    result = ledController.setTrigger(RPI_LED_ACT, "mmc0\n");
    if (result < 0) {
        fprintf(stderr, "Failed to restore ACT LED trigger.\n");
    }
    result = ledController.setTrigger(RPI_LED_PWR, "default-on\n");
    if (result < 0) {
        fprintf(stderr, "Failed to restore PWR LED trigger.\n");
    }
    printf("   (ACT should reflect SD card activity, PWR should be solid ON)\n");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    printf("--- Tester Finished ---\n");

    return 0;
}