#include "utils.hpp"
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace utils {
std::string now() {
    const std::time_t current=std::time(nullptr); std::tm local{};
#if defined(_WIN32)
    localtime_s(&local,&current);
#else
    local=*std::localtime(&current);
#endif
    std::ostringstream out; out << std::put_time(&local,"%Y-%m-%d %H:%M:%S"); return out.str();
}
std::string trim(const std::string& value) { const auto start=value.find_first_not_of(" \t\r\n"); if(start==std::string::npos)return ""; const auto end=value.find_last_not_of(" \t\r\n"); return value.substr(start,end-start+1); }
bool isNumber(const std::string& value) { return !value.empty() && std::all_of(value.begin(),value.end(),[](unsigned char c){return std::isdigit(c)!=0||c=='.'||c=='-';}); }
}  // namespace utils
