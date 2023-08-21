/**************************************** 
 * Load cell amplifier code - Thrust calculation
 * To compile: g++ -Wall -o hx711 hx711.cpp -lhx711 -llgpio
 * 
 * **************************************/

/********* Libraries ********/
#include <chrono>
#include <iostream>
#include <hx711/common.h>
#include <string>
#include <iostream>
#include <fstream>
#include <thread> // for std::this_thread::sleep_for

/******* Parameters *******/
const double weightlimit = 5.0;

/******** Functions *******/
void setGPIOValue(int pin, int value){
    std::ofstream valueFile("/sys/class/gpio/gpio" + std::to_string(pin) + "/value");
    valueFile << value;
    valueFile.close();
}

void exportGPIO(int pin) {
    std::ofstream exportFile("/sys/class/gpio/export");
    exportFile << pin;
    exportFile.close();
}

void setGPIODirection(int pin, const std::string& direction) {
    std::ofstream directionFile("/sys/class/gpio/gpio" + std::to_string(pin) + "/direction");
    directionFile << direction;
    directionFile.close();
}

  const int GPIO_PIN = 25;
  const int GPIO_PIN_1 = 28;
  
/******* Main code *******/
int main() {
  
  using namespace HX711;
  using std::chrono::seconds;
  
  exportGPIO(GPIO_PIN);
  setGPIODirection(GPIO_PIN, "out");
  setGPIOValue(GPIO_PIN, 0);

  exportGPIO(GPIO_PIN_1);
  setGPIODirection(GPIO_PIN_1, "out");
  setGPIOValue(GPIO_PIN_1, 0);

  // create an AdvancedHX711 object using GPIO pin 2 as the data pin,
  // GPIO pin 3 as the clock pin, -370 as the reference unit, -367471
  // as the offset, and indicate that the chip is operating at 80Hz
  AdvancedHX711 hx(2, 3, -44674, -68571, Rate::HZ_80);
  hx.setUnit(Mass::Unit::KG);

  // Open file dataLoadCell
  std::ofstream outputFile("dataLoadCell.csv");
  
  std::cout << "Press Enter to start the firing..." << std::endl;
  std::cin.get(); // Wait for Enter key

  // Open valve 1
  setGPIOValue(GPIO_PIN, 1);
  std::this_thread::sleep_for(std::chrono::seconds(2)); // Wait for 2 seconds

  // Open valve 2
  setGPIOValue(GPIO_PIN_1, 1);

  auto start_time = std::chrono::steady_clock::now();
  auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count();
  
  std::cout << "Firing started!" << std::endl;

  // Collect data for 7 seconds
  while (elapsed_time < 7) {
    double weight = hx.weight(1);
    std::cout << "Weight: " << weight << " kg" << std::endl;
    outputFile << weight << std::endl;

    // Check weight limit
    if (weight >= weightlimit) {
      setGPIOValue(GPIO_PIN, 0);
      setGPIOValue(GPIO_PIN_1, 0);
      std::cout << "Weight limit reached: Turning off valves" << std::endl;
      break;
    }

    // Update elapsed time
    elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count();
  }

  // Close both valves
  setGPIOValue(GPIO_PIN, 0);
  setGPIOValue(GPIO_PIN_1, 0);

  std::cout << "Firing completed!" << std::endl;
  outputFile.close();
  
  return 0;
}
