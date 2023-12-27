
#pragma once

#include <format>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

namespace nt {

using action_func_t = std::function<void()>;

struct Option {
    std::string label;
    action_func_t action;
};

using endl_t = std::ostream &(*)(std::ostream &);

/**
 * Naive implementation for a CLI
 */
class CommandLineInterface {
public:
    CommandLineInterface &title(std::string_view title) {
        title_ = title;
        return *this;
    }

    CommandLineInterface &option(Option &&opt) {
        options_.emplace_back(std::move(opt));
        return *this;
    }

    void tryToExecuteAction() const {
        std::string choiceStr;

        while (true) {
            std::cout << n_ << n_ << title_ << n_;

            for (auto i = 0; i < options_.size(); ++i) {
                std::cout << std::format("   {}. {}", i + 1, options_[i].label) << n_;
            }
            std::cout << n_ << "Enter your choice (0 to quit/reload): ";

            std::getline(std::cin, choiceStr);

            try {
                if (auto choiceInt = std::stoi(choiceStr); choiceInt > options_.size()) {
                    throw std::invalid_argument{""};
                } else {
                    if (choiceInt == 0) {
                        break;
                    }
                    options_[choiceInt - 1].action();
                }
            } catch ([[maybe_unused]] std::invalid_argument const &ex) {
                std::cerr << "\nERROR: Invalid option!\n\n";
            }
            choiceStr.clear();
            std::cin.clear();
        }
    }

    void leaveMenu() const {
        std::istringstream fakeInput{std::to_string(0)};
        std::cin.rdbuf(fakeInput.rdbuf());
    }

private:
    std::vector<Option> options_;
    std::string title_;
    endl_t n_ = std::endl;
    char tab_ = '\t';
};

} // namespace nt
