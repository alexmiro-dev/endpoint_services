
#pragma once

#include <format>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

namespace eps {

using action_func_t = std::function<void()>;

struct Option {
    std::string label;
    action_func_t action;
};

/**
 * Naive implementation for a CLI
 */
class CommandLineInterface {
public:
    CommandLineInterface &option(Option &&opt) {
        options_.emplace_back(std::move(opt));
        return *this;
    }

    bool tryToExecuteAction(std::string_view title) const {
        std::string choiceStr;

        while (true) {
            std::cout << "\n\n" << title << "\n";

            for (auto i = 0; i < options_.size(); ++i) {
                std::cout << std::format("   {}. {}", i + 1, options_[i].label) << "\n";
            }
            std::cout << "\nEnter your choice (0 to disconnect and quit): ";

            std::getline(std::cin, choiceStr);

            try {
                if (auto choiceInt = std::stoi(choiceStr); choiceInt > options_.size()) {
                    throw std::invalid_argument{""};
                } else {
                    if (choiceInt == 0) {
                        return false;
                    }
                    options_[choiceInt - 1].action();
                    return true;
                }
            } catch ([[maybe_unused]] std::invalid_argument const &ex) {
                std::cerr << "\nERROR: Invalid option!\n\n";
            }
            choiceStr.clear();
            std::cin.clear();
        }
    }

private:
    std::vector<Option> options_;
};

} // namespace eps
