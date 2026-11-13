#include <filesystem>
#include <iostream>

void clear() {
  // CSI[2J clears screen, CSI[H moves the cursor to top-left corner
  std::cout << "\x1B[2J\x1B[H";
}

// FIXED: (SE)[Defencive]: cin >> stops at whitespace, so filesnames with spaces
// will fail. Use std::getline.
void getInput(std::string &key) {
  std::getline(std::cin, key);
  std::cin.clear();
  std::cout << std::endl;
}

/*
 * helper fucntion to get input from user for main menu.
 */
// FIXED: (SE)[Defencive]: Only invalid_argument is caught. stoi throws
// std::out_of_range on huge numbers, which is uncaught and terminates the
// program. inputs like "123abc" are accpeted as 1. On EOF, cin fails
// permanently and the menu loops forever. Catch exceptions by const&, check
// the whole string was consumed, and handle EOF.
bool getInput(int &input) {
  while (true) {
    std::cout << "Please select an option: ";

    std::string line;
    if (!std::getline(std::cin, line)) {
      return false;
    }

    // stoi trims leading whitespace, and trim the rest as well here.
    line.erase(line.find_last_not_of(" \t\r\n") + 1);

    try {
      std::size_t pos = 0;
      int value = stoi(line, &pos, 10);
      if (pos != line.size()) {
        throw std::invalid_argument("trailing characters");
      }
      input = value;
      std::cout << std::endl;
      return true;
    } catch (const std::invalid_argument &) {
      clear();
      std::cout << "You can only input integer values." << std::endl;
    } catch (const std::out_of_range &) {
      clear();
      std::cout << "Value too large." << std::endl;
    }
  }
}

/*
 * helper funciton to check if file exists.
 */
bool exists(const std::string &name) { return std::filesystem::exists(name); }
