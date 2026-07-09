#include "utils.h"

std::string generateRandomString(int lenght)
{
    const std::string CHARACTERS
        = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuv"
          "wxyz0123456789";

    auto seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937 generator(static_cast<unsigned int>(seed));

    std::uniform_int_distribution<> distribution(0, CHARACTERS.size() - 1);

    std::string result = "";
    result.reserve(lenght);
    for(int i = 0; i < lenght; ++i)
    {
        result += CHARACTERS[distribution(generator)];
    }

    return result;

}

